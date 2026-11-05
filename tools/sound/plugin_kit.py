"""Export a kit's plugin manifest, content/plugin/<kit>/<KIT>_kit.json: the kit, its sample files, its instruments,
its keymap, its extras and its phrase tables, in the contract docs/plugin/manifest.md (section 2) sets out, integers
and strings only (ADR 0020).

    python3 tools/sound/plugin_kit.py --kit FUT01 [--check]       # or --kit all; --report: the multisample cycles

Where everything comes from:
- the instruments: the kit's r5 Items (kits.items(..., sets=['r5'])) as MIDI v2 plays them (tools/sound/midi.py's
  PARTS: which Item each channel and key plays), with the rules the recipes play them by (PLAY below, each value with
  the recipe it comes from);
- the files: each take's WAV (listen.plain), its frames, channels and SHA-256 read off the WAV, its loop points off
  build/<kit>-r5/qa_report.json;
- the phrase tables: tools/sound/phrase_tables.py;
- the mix channels each instrument and phrase plays on: P3's channel ids (its MIX_CHANNELS), in content/plugin/<kit>/
  <KIT>_mix.json.

build/ isn't in git and CI has no samples, so the committed manifest is the source of truth for every file's facts:
where a WAV or the report isn't here, the exporter takes the fact from the committed manifest, and --check compares
everything else. $WUBANSIA_CONTENT_DIR (a folder of <kit>/ folders) is searched for the WAVs before the checkout's
content/samples, and $WUBANSIA_BUILD_DIR for the reports before the checkout's build/.
"""
import argparse
import functools
import hashlib
import json
import math
import os
import sys
from pathlib import Path

if __package__ in (None, ''):  # run as a script: import the tools.sound package (it pins numpy's SIMD) first
    sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
    import tools.sound  # noqa: F401

from tools.sound import channels as CH  # noqa: E402
from tools.sound import denylist, kits, listen  # noqa: E402
from tools.sound import midi as Mi  # noqa: E402
from tools.sound import phrase_tables as PT  # noqa: E402

REPO = Path(__file__).resolve().parents[2]
CONTENT_VERSION = 1
SR = 48000
KITS = ('FUT01', 'FUT02', 'FUT03', 'FUT04')   # the kits the plugin plays (each has recipes/<kit>/plugin.py)
ONE_KEY = {6: 'accents', 7: 'fx', 8: 'chops'}


# ------------------------------------------------------------------ the writer (manifest.md section 1)
# plugin_common.py's, until P0 lands it: the same rules, the same bytes
iround = PT.iround          # round half up: floor(x x scale + 0.5)


def _string(s):
    out = ['"']
    for c in s:
        if c in '"\\':
            out.append('\\' + c)
        elif c == '\n':
            out.append('\\n')
        elif c == '\t':
            out.append('\\t')
        elif ord(c) < 0x20:
            out.append(f'\\u{ord(c):04x}')
        else:
            out.append(c)
    return ''.join(out + ['"'])


def _scalar(x):
    if isinstance(x, bool):
        return 'true' if x else 'false'
    if isinstance(x, int):
        return str(x)
    if isinstance(x, str):
        return _string(x)
    raise ValueError(f'not an integer, string or flag: {x!r}')


def _dump(x, ind):
    pad = '  ' * (ind + 1)
    if isinstance(x, dict):
        if not x:
            return '{}'
        body = ',\n'.join(f'{pad}{_string(k)}: {_dump(v, ind + 1)}' for k, v in x.items())
        return '{\n' + body + '\n' + '  ' * ind + '}'
    if isinstance(x, list):
        if not x:
            return '[]'
        if all(not isinstance(v, (dict, list)) for v in x):
            return '[' + ', '.join(_scalar(v) for v in x) + ']'
        return '[\n' + ',\n'.join(pad + _dump(v, ind + 1) for v in x) + '\n' + '  ' * ind + ']'
    return _scalar(x)


def dumps(obj):
    """The canonical layout: 2 spaces an indent, a member a line, arrays of scalars on one line, a final newline."""
    return _dump(obj, 0) + '\n'


def strings(obj):
    """Every string value in a manifest."""
    if isinstance(obj, dict):
        return [s for v in obj.values() for s in strings(v)]
    if isinstance(obj, list):
        return [s for v in obj for s in strings(v)]
    return [obj] if isinstance(obj, str) else []


def denied(obj):
    """(string, list) for every string a denylist (artists, slurs) catches: none may."""
    lists = {n: denylist.load(n) for n in ('artists', 'slurs')}
    return [(s, n) for s in strings(obj) for n, d in lists.items() if denylist.hits(s, d)]


# ------------------------------------------------------------------ each kit's own: tools/sound/recipes/<kit>/plugin.py
# Shared code never imports a kit (ADR 0008): every kit-specific fact is in the kit's plugin module, read through its
# RECIPES (phrase_tables.plugin_module): LANE_LABELS, KICK_AB, MIX_CHANNELS, MIX, EXTRAS, INTERP, GLIDE, BASS_ENV,
# LEAD_RELEASE, CHORD_LAW, VOX, CHOKE, LAYERS, chop_preroll(), accent_gain_mdb(), play(), phrases(), DERIVED and
# render_derived().
NO_GLIDE = ('none', 0, 0)
PAD_ENV = (2000, 20000)              # sampler.sampler_voice: play, loop while held, a 20 ms raised-cosine release


def kit_module(kit):
    mod = PT.plugin_module(kit)
    if mod is None:
        raise ValueError(f'{kit} has no plugin module (tools/sound/recipes/{kit.lower()}/plugin.py)')
    return mod


def mix_channels(kit):
    """P3's mix channels for the kit (content/plugin/<kit>/<KIT>_mix.json): {id: lane}."""
    return kit_module(kit).MIX_CHANNELS


def glide(g):
    return {'curve': g[0], 'us': g[1], 'dip_us': g[2]}


def choke(group='', role='none', fade_us=0, curve='linear'):
    return {'group': group, 'role': role, 'fade_us': fade_us, 'curve': curve}


def rules(kit, kind, item, it, key=None):
    """An instrument's playing fields (everything but id, name, lane, mix_channel and zones), from the recipe that
    plays it: the defaults of its kind, then the kit's own (its plugin module's play()). kind: 'drum', 'bass', 'lead',
    'chords', 'vox', 'accents', 'fx', 'chops', or an extra's (the module's EXTRAS: 'bass', 'lead', 'pad', 'melodic',
    'gated_stab', 'drop_glide')."""
    km = kit_module(kit)
    r = dict(mode='one_shot', align='end' if 'cut_end' in it.flags else 'start', attack_us=0, release_us=0,
             gain_mdb=0, start_frames=0, velocity_law='layer_only', interp='sinc', mono=False, glide=glide(NO_GLIDE),
             restrike_continues=False, choke=choke(), duck_key=False, pan_cycle_pm=[], rr_block_16ths=64,
             layered_with='', preroll=0)
    pitched = dict(velocity_law='pow15', interp=km.INTERP)
    if kind == 'drum':
        lane = CH.TRACKS[Mi.kit_of(kit).part_track(Mi.kit_of(kit).drum_part[key])][1]
        r['duck_key'] = lane in ('kick', 'snare')       # the kicks and claps key the ducks (mix.json)
        if key in (42, 46):                             # the closed hat chokes the open (the recipes' hat lanes)
            r['choke'] = choke('hats', 'chokes' if key == 42 else 'choked', *km.CHOKE)
    elif kind == 'bass':
        r.update(mode='sustain_loop', attack_us=km.BASS_ENV[0], release_us=km.BASS_ENV[1], mono=True,
                 glide=glide(km.GLIDE), **pitched)
    elif kind == 'lead':
        r.update(mode='gated', release_us=km.LEAD_RELEASE, **pitched)
    elif kind == 'chords':
        if it.loop:
            r.update(mode='sustain_loop', attack_us=PAD_ENV[0], release_us=PAD_ENV[1], restrike_continues=True)
        else:
            r.update(velocity_law=km.CHORD_LAW)
    elif kind == 'pad':
        r.update(mode='sustain_loop', attack_us=PAD_ENV[0], release_us=PAD_ENV[1], restrike_continues=True)
    elif kind == 'vox':
        release, law, pre = km.VOX
        r.update(mode='gated', release_us=release, velocity_law=law, preroll=pre)
    elif kind == 'chops':
        r['preroll'] = km.chop_preroll(item)
    elif kind == 'accents':
        r['gain_mdb'] = km.accent_gain_mdb(item)
    elif kind == 'melodic':                     # a one-shot played as a melody, at its accent level (the same file)
        r.update(velocity_law='pow15', gain_mdb=km.accent_gain_mdb(item))
    elif kind == 'gated_stab':                  # a stab riff: gated to its note, a 30 ms release (fut02/accents)
        r.update(mode='gated', release_us=30000, velocity_law='pow15')
    elif kind not in ('fx', 'drop_glide'):
        raise ValueError(f'{kit}: no rule for {kind} {item}')
    return km.play(kind, item, it, key, r)


# ------------------------------------------------------------------ the instruments, off the MIDI v2 PARTS
def snake(name):
    """An Item's or sound's CamelCase name as an id: 'VoxChopAhEeFs5' -> 'vox_chop_ah_ee_fs5'."""
    return '_'.join(w.lower() for w in listen.WORD.findall(name))


words = PT.words


@functools.lru_cache(maxsize=None)
def plan(kit):
    """The kit's instruments and keymap, before any file is read: (instruments, keymap). An instrument: dict(id,
    name, item, kind, lane, mix_channel, rules, zones [(root, lo, hi, note)], keys (one-key: its keys)). A keymap
    entry: (ch, lo, hi, a, b, ab)."""
    k = Mi.kit_of(kit)
    km = kit_module(kit)
    items = {it.name: it for it in kits.items(kits.load(kit), sets=['r5'])}
    mix = km.MIX
    out, keymap, by_id = [], [], {}

    def add(iid, item, kind, lane, channel, rule, zones, keys=()):
        if iid in by_id:
            raise ValueError(f'{kit}: two instruments called {iid}')
        if km.MIX_CHANNELS[channel] != lane:
            raise ValueError(f'{kit}: {iid} plays on lane {lane}, its mix channel {channel} on another')
        ins = dict(id=iid, name=words(item), item=item, kind=kind, lane=lane, mix_channel=channel, rules=rule,
                   zones=zones, keys=tuple(keys))
        by_id[iid] = ins
        out.append(ins)
        return ins

    def pitched_zones(it):
        notes = list(it.notes) or [it.root]
        return [(n, 0 if i == 0 else n, notes[i + 1] - 1 if i + 1 < len(notes) else 127, n if it.notes else None)
                for i, n in enumerate(notes)]

    def key_range(it):
        notes = list(it.notes) or [it.root]
        return max(0, notes[0] - 24), min(127, notes[-1] + 24)

    # channel 10: one instrument per drum Item, a zone on each of its notes
    drums = {}
    for note in sorted(k.drum_map):
        drums.setdefault(k.drum_map[note], []).append(note)
    ab_kick = km.KICK_AB
    for note in sorted(k.drum_map):
        name = k.drum_map[note]
        names = ab_kick if note == 36 and ab_kick else (name,)
        for n in names:
            if snake(n) in by_id:
                continue
            notes = drums.get(n, [note])
            lane = CH.TRACKS[k.part_track(k.drum_part[note])][1]
            add(snake(n), n, 'drum', lane, mix['drums'][n], rules(kit, 'drum', n, items[n], note),
                [(x, x, x, k.drum_zone.get(x)) for x in notes], notes)
        if len(names) == 2:
            keymap.append((10, note, note, snake(names[0]), snake(names[1]), 'kick'))
        else:
            keymap.append((10, note, note, snake(name), '', ''))
    # the pitched channels: 1 bass, 2 lead, 4 chords, 5 vox, 11-16 the extras
    extras = {ch: (km.EXTRAS[name], lane) for ch, name, lane in CH.extras(kit)}
    roles = {1: 'bass', 2: 'lead', 4: 'chords', 5: 'vox'}
    for ch, names in sorted(k.channel_items().items()):
        kind, lane = (roles[ch], CH.ROLE_LANE[roles[ch]]) if ch in roles else extras[ch]
        ids = []
        for n in names:
            ins = add(snake(n), n, kind, lane, mix['channels'][ch], rules(kit, kind, n, items[n]),
                      pitched_zones(items[n]))
            ids.append(ins['id'])
        lo, hi = key_range(items[names[0]])
        ab = ('bass' if lane == 'bass' else 'stab') if len(ids) == 2 else ''
        keymap.append((ch, lo, hi, ids[0], ids[1] if len(ids) == 2 else '', ab))
    # channels 6-8: one key a sound, each its own instrument on its zone
    for ch, role in ONE_KEY.items():
        for key, sound in sorted(k.keymap(role).items()):
            it = items[sound.item]
            iid = snake(sound.label)
            if iid in by_id:
                iid = f'{iid}_{role}'
            channel = mix[role] if isinstance(mix[role], str) else mix[role].get(sound.item, mix[role][None])
            add(iid, sound.label, role, CH.ROLE_LANE[role], channel, rules(kit, role, sound.item, it, sound),
                [(key, key, key, sound.zone)], (key,))
            out[-1]['item'] = sound.item
            keymap.append((ch, key, key, iid, '', ''))
    # layers: an Item under others on their multisample voices, no key of its own (FUT03's Sub Deep)
    for name, (under, gain) in km.LAYERS.items():
        it = items[name]
        lane = by_id[snake(under[0])]['lane']
        layer = add(snake(name), name, 'bass', lane, by_id[snake(under[0])]['mix_channel'],
                    dict(rules(kit, 'bass', name, it), gain_mdb=gain), pitched_zones(it))
        for u in under:
            by_id[snake(u)]['rules'] = dict(by_id[snake(u)]['rules'], layered_with=layer['id'])
    order = {iid: i for i, iid in enumerate(i['id'] for i in out)}
    keymap.sort(key=lambda e: (e[0], e[1]))
    return out, keymap, order


def item_of(kit, iid):
    """The Item an instrument plays."""
    return next(i['item'] for i in plan(kit)[0] if i['id'] == iid)


def one_key(kit, iid):
    """The keys of a one-key instrument (drums, accents, FX, chops), or () for a pitched one."""
    return next(i['keys'] for i in plan(kit)[0] if i['id'] == iid)


def zone_notes(kit, iid):
    """The Item note (zone) each of an instrument's zones plays, or None for an unzoned Item."""
    return [z[3] for z in next(i['zones'] for i in plan(kit)[0] if i['id'] == iid)]


# ------------------------------------------------------------------ the files and their facts
def sample_roots():
    env = os.environ.get('WUBANSIA_CONTENT_DIR')
    return ([Path(env)] if env else []) + [REPO / 'content' / 'samples']


def is_lfs_pointer(path):
    with open(path, 'rb') as f:
        return f.read(40).startswith(b'version https://git-lfs.github.com/spec/')


def real_wav(path):
    """The path if it is a WAV (not an LFS pointer), else None."""
    try:
        with open(path, 'rb') as f:
            return Path(path) if f.read(4) == b'RIFF' else None
    except OSError:
        return None


def find_sample(kit, rel):
    """The first real WAV of a kit's file under the sample roots, or None."""
    for root in sample_roots():
        p = real_wav(root / kit.lower() / rel)
        if p:
            return p
    return None


def qa_report(kit):
    """build/<kit>-r5/qa_report.json, or None where it isn't here."""
    env = os.environ.get('WUBANSIA_BUILD_DIR')
    for root in ([Path(env)] if env else []) + [REPO / 'build']:
        p = root / f'{kit.lower()}-r5' / 'qa_report.json'
        if p.exists():
            return json.loads(p.read_text(encoding='utf-8'))
    return None


def report_loops(report):
    """{WAV path: (loop start, loop end)} of every looped file in a round's report."""
    out = {}
    for it in report['items']:
        ms = [listen.parse(f['file']) for f in it['files']]
        lay, rrs = len({m['v'] for m in ms}) > 1, len({m['rr'] for m in ms}) > 1
        for f in it['files']:
            if 'loop' in f:
                out[f'{listen.FOLDERS[it["group"]]}/{listen.plain(f["file"], lay, rrs)}'] = tuple(f['loop'])
    return out


def take_path(it, take):
    return f'{listen.FOLDERS[it.group]}/{listen.plain(take.file, len(it.layers) > 1, it.rr > 1)}'


def wav_facts(path):
    """(frames, channels, sha256) of a WAV, 48 kHz only."""
    import soundfile as sf
    info = sf.info(path)
    if info.samplerate != SR:
        raise ValueError(f'{path} is {info.samplerate} Hz, not {SR}')
    return info.frames, info.channels, hashlib.sha256(Path(path).read_bytes()).hexdigest()


def manifest_path(kit):
    return REPO / 'content' / 'plugin' / kit.lower() / f'{kit}_kit.json'


def committed(kit):
    p = manifest_path(kit)
    return json.loads(p.read_text(encoding='utf-8')) if p.exists() else None


# ------------------------------------------------------------------ the manifest
def export(kit):
    """The kit's manifest as an object, in the contract's key order."""
    K = kits.load(kit)
    km = kit_module(kit)
    items = {it.name: it for it in kits.items(K, sets=['r5'])}
    instruments, keymap, _ = plan(kit)
    old = committed(kit)
    old_files = {f['path']: f for f in old['files']} if old else {}
    old_gain = {i['id']: i['gain_mdb'] for i in old['instruments']} if old else {}
    report = qa_report(kit)
    loops = report_loops(report) if report else None

    files, index = [], {}

    def file_of(path, looped, preroll):
        if path in index:
            f = files[index[path]]
            if f['preroll_frames'] != preroll:
                raise ValueError(f'{kit}: {path} is played with two pre-rolls')
            return index[path]
        wav = find_sample(kit, path)
        if wav is not None:
            frames, channels, sha = wav_facts(wav)
        elif path in old_files:
            o = old_files[path]
            frames, channels, sha = o['frames'], o['channels'], o['sha256']
        else:
            raise FileNotFoundError(f'{kit}: {path} is in neither the samples nor the committed manifest '
                                    f'(set WUBANSIA_CONTENT_DIR)')
        if not looped:
            loop = (0, 0)
        elif loops is not None:
            loop = loops[path]
        elif path in old_files:
            loop = (old_files[path]['loop_start'], old_files[path]['loop_end'])
        else:
            raise FileNotFoundError(f'{kit}: the loop points of {path} need build/{kit.lower()}-r5/qa_report.json '
                                    f'(set WUBANSIA_BUILD_DIR)')
        index[path] = len(files)
        files.append({'path': path, 'frames': frames, 'channels': channels, 'sha256': sha,
                      'loop_start': loop[0], 'loop_end': loop[1], 'preroll_frames': preroll})
        return index[path]

    out = []
    for ins in instruments:
        it = items[ins['item']]
        r = dict(ins['rules'])
        pre = r.pop('preroll')
        if r['gain_mdb'] is None:      # a level measured off the file (FUT04's accents), else the committed manifest
            wav = find_sample(kit, take_path(it, it.takes(kit)[0]))
            if wav is not None:
                import soundfile as sf
                from tools.sound import dsp48 as D
                r['gain_mdb'] = km.accent_gain_mdb(ins['item'], D.st(sf.read(wav, dtype='float64')[0]))
            elif ins['id'] in old_gain:
                r['gain_mdb'] = old_gain[ins['id']]
            else:
                raise FileNotFoundError(f'{kit}: {ins["id"]} needs its WAV for its level (set WUBANSIA_CONTENT_DIR)')
        zones = []
        for root, lo, hi, note in ins['zones']:
            layers = []
            for li, v in enumerate(it.layers, 1):
                rr = [file_of(take_path(it, t), r['mode'] == 'sustain_loop', pre)
                      for t in it.takes(kit) if (t.note, t.layer) == (note, li)]
                layers.append({'vel': iround(v * 127), 'rr': rr})
            zones.append({'root': root, 'lo': lo, 'hi': hi, 'layers': layers})
        out.append({'id': ins['id'], 'name': ins['name'], 'lane': ins['lane'], 'mix_channel': ins['mix_channel'],
                    **{f: r[f] for f in ('mode', 'align', 'attack_us', 'release_us', 'gain_mdb', 'start_frames',
                                         'velocity_law', 'interp', 'mono', 'glide', 'restrike_continues', 'choke',
                                         'duck_key', 'pan_cycle_pm', 'rr_block_16ths', 'layered_with')},
                    'zones': zones})
    frames = {}
    for s in PT.specs(kit):
        for p in s.stems:
            if p:
                frames[p] = files[file_of(p, False, 0)]['frames']
    labels = km.LANE_LABELS
    ab = {'kick': ('', ''), 'bass': ('', ''), 'stab': ('', '')}
    if km.KICK_AB:
        ab['kick'] = tuple(words(n) for n in km.KICK_AB)
    k = Mi.kit_of(kit)
    ab['bass'] = tuple(words(n) for n in k.channel_items()[1])
    if len(k.channel_items()[4]) == 2:
        ab['stab'] = tuple(words(n) for n in k.channel_items()[4])
    manifest = {
        'kit': {'id': kit, 'title': K.TITLE, 'direction': K.DIRECTION, 'bpm': int(K.BPM), 'key': K.KEY,
                'camelot': K.CAMELOT, 'swing_pm': iround(K.SWING * 1000), 'content_version': CONTENT_VERSION,
                'lanes': {lane: label if any(i['lane'] == lane for i in out) else ''
                          for lane, label in zip(CH.LANES, labels)},
                'ab': {s: {'a': a, 'b': b} for s, (a, b) in ab.items()}},
        'files': files,
        'instruments': out,
        'keymap': [{'ch': ch, 'keys': [lo, hi], 'a': a, 'b': b, 'ab': sw} for ch, lo, hi, a, b, sw in keymap],
        'extras': [{'ch': ch, 'name': n, 'lane': lane} for ch, n, lane in CH.extras(kit)],
        'phrases': [PT.table(s, index, frames) for s in PT.specs(kit)],
    }
    bad = denied(manifest)
    if bad:
        raise ValueError(f'{kit}: strings a denylist catches: {bad}')
    return manifest


# ------------------------------------------------------------------ the extended mixes' multisample cycles
def multisample_cycles(kit):
    """Per phrase channel, the cycles of the kit's extended mixes where a note doesn't resolve to a slice (it plays
    the multisample): {(variant, ch): [(cycle, bar, unmatched notes)]}."""
    K = kits.load(kit)
    m = committed(kit) or export(kit)
    out = {}
    for variant in ('vox', 'novox'):
        name = 'Vox' if variant == 'vox' else 'NoVox'
        path = REPO / 'content' / 'midi' / kit.lower() / 'extended' / variant / f'{kit}_ExtendedMix_{name}_224bars.mid'
        notes, ccs = PT.file_notes(K, path), PT.file_ccs(path)
        for ch in sorted({ph['ch'] for ph in m['phrases']}):
            tables = [ph for ph in m['phrases'] if ph['ch'] == ch]
            _, unmatched = PT.resolve(K, notes.get(ch, []), tables, ccs.get(ch, {}))
            cyc = tables[0]['cycle_16ths']
            by = {}
            for pos, key in unmatched:
                by.setdefault(math.floor(pos) // cyc, []).append((pos, key))
            out[(variant, ch)] = [(c, c * cyc // 16 + 1, len(v)) for c, v in sorted(by.items())]
    return out


def report(kit):
    """The multisample cycles as text lines."""
    lines = []
    for (variant, ch), cycles in multisample_cycles(kit).items():
        if cycles:
            bars = ', '.join(f'{b}' for _, b, _ in cycles)
            notes = sum(n for *_, n in cycles)
            plural = lambda n, w: f'{n} {w}' + ('s' if n != 1 else '')  # noqa: E731
            lines.append(f'{kit} {variant} ch {ch}: {plural(len(cycles), "cycle")} ({plural(notes, "note")}) from '
                         f'{plural(len(cycles), "bar")[len(str(len(cycles))) + 1:]} {bars}')
        else:
            lines.append(f'{kit} {variant} ch {ch}: every cycle resolves')
    return lines


def summary_row(kit):
    """The kit's row in content/plugin/README.md's table, off its committed manifest."""
    p = manifest_path(kit)
    m = json.loads(p.read_text(encoding='utf-8'))
    wav = sum(44 + f['frames'] * f['channels'] * 3 for f in m['files'])   # a PCM_24 WAV's header and data
    return (f"| {kit} | {p.stat().st_size:,} bytes | {len(m['instruments'])} | {len(m['keymap'])} | {len(m['files'])} "
            f"| {wav / 1e6:.1f} MB | {len(m['phrases'])} ({sum(len(x['slices']) for x in m['phrases'])}) |")


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    ap.add_argument('--kit', required=True, help='FUT01..FUT04, or all')
    ap.add_argument('--check', action='store_true', help='compare with the committed manifest; write nothing')
    ap.add_argument('--report', action='store_true', help="list the extended mixes' multisample cycles")
    a = ap.parse_args(argv)
    todo = KITS if a.kit == 'all' else (a.kit,)
    stale = 0
    for kit in todo:
        if a.report:
            print('\n'.join(report(kit)))
            continue
        text = dumps(export(kit))
        p = manifest_path(kit)
        if a.check:
            same = p.exists() and p.read_text(encoding='utf-8') == text
            print(f'{p.relative_to(REPO)}: {"up to date" if same else "STALE"}')
            stale += not same
        else:
            p.parent.mkdir(parents=True, exist_ok=True)
            p.write_text(text, encoding='utf-8', newline='\n')
            print(f'{p.relative_to(REPO)}: {len(text.encode())} bytes')
    return 1 if stale else 0


if __name__ == '__main__':
    sys.exit(main())
