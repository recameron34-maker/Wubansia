"""Structural checks for the generated presets (what Decent Sampler's own Validate Preset checks, plus index and range checks)."""
import os as _os  # noqa: E402
PACK_ROOT = _os.environ.get('PACK_ROOT') or _os.path.abspath(_os.path.join(_os.path.dirname(_os.path.abspath(__file__)), '..'))  # the legacy web-pack folder
import os, sys, glob
import xml.etree.ElementTree as ET
import soundfile as sf

LIB = PACK_ROOT + '/inst/lib'
errors, warnings = [], []
_info = {}


def info(p):
    if p not in _info:
        i = sf.info(p)
        _info[p] = (i.frames, i.channels, i.samplerate)
    return _info[p]


def exact_exists(base, rel):
    """Case-sensitive existence check, component by component."""
    cur = base
    for part in rel.split('/'):
        if part not in os.listdir(cur):
            return False
        cur = os.path.join(cur, part)
    return True


for f in sorted(glob.glob(f'{LIB}/*.dspreset')):
    name = os.path.basename(f)
    E = lambda m: errors.append(f'{name}: {m}')
    try:
        root = ET.parse(f).getroot()
    except ET.ParseError as e:
        E(f'XML: {e}')
        continue
    ui = root.find('ui')
    if not exact_exists(LIB, ui.get('bgImage')):
        E('missing bgImage')
    tab = list(ui.find('tab'))
    groups = root.find('groups').findall('group')
    geffects = root.find('effects')
    neff = len(geffects) if geffects is not None else 0
    mods = root.find('modulators')
    nmods = len(mods) if mods is not None else 0
    # samples
    nz = 0
    for gi, g in enumerate(groups):
        gfx = g.find('effects')
        for s in g.findall('sample'):
            nz += 1
            rel = s.get('path')
            if not exact_exists(LIB, rel):
                E(f'missing sample {rel}')
                continue
            fr, ch, sr = info(os.path.join(LIB, rel))
            if sr != 44100:
                E(f'{rel} sr {sr}')
            r, lo, hi = int(s.get('rootNote')), int(s.get('loNote', 0)), int(s.get('hiNote', 127))
            if not (0 <= lo <= hi <= 127 and 0 <= r <= 127):
                E(f'bad note range {rel} {lo}-{hi} root {r}')
            st, en = int(s.get('start', 0)), int(s.get('end', fr - 1))
            if not (0 <= st < en <= fr - 1):
                E(f'bad start/end {rel} {st}-{en} of {fr}')
            if s.get('loopEnabled') == 'true':
                ls, le = int(s.get('loopStart')), int(s.get('loopEnd'))
                if not (0 <= ls < le <= fr - 1):
                    E(f'bad loop {rel} {ls}-{le} of {fr}')
        if g.find('oscillator') is not None:
            nz += 1
    if nz == 0:
        E('no zones')
    # bindings
    def check_binding(b, where):
        t, lvl, par = b.get('type'), b.get('level'), b.get('parameter')
        pos = b.get('position') or b.get('groupIndex') or b.get('effectIndex') or b.get('modulatorIndex') or b.get('controlIndex')
        if lvl == 'group' and t in ('amp', 'general'):
            if int(pos) >= len(groups):
                E(f'{where}: group {pos} out of range ({len(groups)})')
        if t == 'effect' and lvl == 'instrument':
            if int(b.get('position', b.get('effectIndex', 0))) >= neff:
                E(f'{where}: effect {pos} out of range ({neff})')
        if t == 'effect' and lvl == 'group':
            gi, ei = int(b.get('groupIndex')), int(b.get('effectIndex'))
            ge = groups[gi].find('effects') if gi < len(groups) else None
            if ge is None or ei >= len(ge):
                E(f'{where}: group effect {gi}/{ei} missing')
        if t == 'modulator':
            if int(pos) >= nmods:
                E(f'{where}: modulator {pos} out of range ({nmods})')
        if t == 'control' and lvl == 'ui':
            p = int(pos)
            if p >= len(tab) or tab[p].tag not in ('labeled-knob', 'control'):
                E(f'{where}: ui control {p} is {tab[p].tag if p < len(tab) else "missing"}')
        if lvl == 'tag' and not b.get('identifier'):
            E(f'{where}: tag binding without identifier')
    for i, e in enumerate(tab):
        for b in e.iter('binding'):
            check_binding(b, f'ui[{i}] {e.get("label") or e.tag}')
        if e.tag == 'menu':
            opts = e.findall('option')
            if not 1 <= int(e.get('value', 1)) <= len(opts):
                E('menu value out of range')
        if e.tag == 'labeled-knob':
            v, lo, hi = float(e.get('value')), float(e.get('minValue')), float(e.get('maxValue'))
            if not lo <= v <= hi:
                E(f'knob {e.get("label")} value {v} outside {lo}-{hi}')
            if e.get('valueType') == 'musical_time' and not (0 <= lo and hi <= 20):
                warnings.append(f'{name}: musical_time knob range {lo}-{hi}')
    if mods is not None:
        for m in mods:
            for b in m.iter('binding'):
                check_binding(b, 'modulator ' + m.tag)
    midi = root.find('midi')
    if midi is not None:
        for cc in midi:
            for b in cc.iter('binding'):
                check_binding(b, 'cc ' + cc.get('number'))
    # menu-enabled groups agree with the default option
    print(f'{name:45s} groups {len(groups):2d}  zones {nz:3d}  effects {neff}  mods {nmods}  ui {len(tab)}')

# library info
li = ET.parse(f'{LIB}/DSLibraryInfo.xml').getroot()
for p in li.iter('preset'):
    if not exact_exists(LIB, p.get('file')):
        errors.append('DSLibraryInfo: missing ' + p.get('file'))
print('\nERRORS', len(errors)); [print(' ', e) for e in errors]
print('WARNINGS', len(warnings)); [print(' ', w) for w in warnings]
sys.exit(1 if errors else 0)
