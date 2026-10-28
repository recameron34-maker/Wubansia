"""Build the 'UKG & Minimal Pack Instruments' library: Decent Sampler presets + SFZ versions, skins, README."""
import os as _os  # noqa: E402
PACK_ROOT = _os.environ.get('PACK_ROOT') or _os.path.abspath(_os.path.join(_os.path.dirname(_os.path.abspath(__file__)), '..'))  # the legacy web-pack folder
import json, os, shutil, re, sys, math
import soundfile as sf
import numpy as np
sys.path.insert(0, os.path.dirname(__file__))
from dslib import el, binding, write_preset, skin, cover, UI, knob_row, argb, C, db, fmt, sfz_line, CUTOFF_TABLE, TONE_TABLE, LOWCUT_TABLE  # noqa: E402

W = PACK_ROOT
LIB = f'{W}/inst/lib'
SAMP = f'{LIB}/Samples'
MINV = '1.37.0'
NAMES = ['C', 'C#', 'D', 'Eb', 'E', 'F', 'F#', 'G', 'Ab', 'A', 'Bb', 'B']
FNAMES = ['C', 'Cs', 'D', 'Eb', 'E', 'F', 'Fs', 'G', 'Ab', 'A', 'Bb', 'B']


def nname(m):
    return NAMES[m % 12] + str(m // 12 - 1)


def nnum(name):
    m = re.match(r'^([A-G])(#|s|b)?(-?\d)$', name)
    pc = {'C': 0, 'D': 2, 'E': 4, 'F': 5, 'G': 7, 'A': 9, 'B': 11}[m.group(1)] + {'#': 1, 's': 1, 'b': -1, None: 0}[m.group(2)]
    return pc + 12 * (int(m.group(3)) + 1)


ITEMS = {}
for f in ['manifest_fx', 'manifest_more', 'manifest_vox', 'manifest_vox2', 'manifest_brat', 'manifest_wave3', 'manifest_v6']:
    for it in json.load(open(f'{W}/out/{f}.json')):
        ITEMS[it['id']] = {**ITEMS.get(it['id'], {}), **it}   # wave 6 re-sung vocals replace the old entries
INST = json.load(open(f'{W}/inst/inst_meta.json'))
INFO = {}   # library-relative path -> (frames, channels)


def src_of(it):
    if it['file'].startswith('sprite:'):
        return f"{W}/out/wave3/{it['pack']}"
    if it.get('cat') == 'vox':          # the site serves vocals as FLAC; the masters are WAVs
        return f"{W}/out/vox_src/vox/{it['id']}.wav"
    return f"{W}/site/{it['file']}"


def take(item_id, cat, name=None, dry=False):
    """Copy one pack sample into the library; returns its library-relative path."""
    it = ITEMS[item_id]
    src = (f"{W}/out/vox_src/vox_dry/{it['id']}.wav" if it.get('cat') == 'vox' else f"{W}/site/{it['dry']}") if dry else src_of(it)
    base = name or os.path.basename(it['packDry'] if dry else it['pack'])
    base = base.replace('Hit_', '')
    rel = f'Samples/{cat}/{base}'
    dst = f'{LIB}/{rel}'
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    if not os.path.exists(dst) or os.path.getmtime(dst) < os.path.getmtime(src) or os.path.getsize(dst) != os.path.getsize(src):
        shutil.copyfile(src, dst)
    i = sf.info(dst)
    assert i.samplerate == 44100, dst
    INFO[rel] = (i.frames, i.channels)
    return rel


def frames(rel):
    if rel not in INFO:
        i = sf.info(f'{LIB}/{rel}')
        INFO[rel] = (i.frames, i.channels)
    return INFO[rel][0]


PRESETS = []   # (category, file, title)
SFZS = []


# ================================================================= shared bits
def preset_root(ui, groups_el, effects=None, modulators=None, midi=None, tags=None, keyboard=None, bg=None, width=812, height=375, limiter=None):
    tab = el('tab', dict(name='main'), ui.items)
    ui_el = el('ui', dict(width=width, height=height, bgImage=bg, bgColor=argb(C['chassis'])), tab, keyboard)
    return el('DecentSampler', dict(minVersion=MINV, limiterStyle=limiter), ui_el, groups_el, effects, modulators, midi, tags)


def kb(colors):
    """Decent Sampler colours only white keys, and any colours turn off its own highlighting of mapped keys,
    so the presets leave the keyboard to the default highlighting."""
    return None


def fx_lowpass(freq=22000, res=0.7, tags=None):
    return el('effect', dict(type='lowpass', frequency=freq, resonance=res, tags=tags))


def fx_highpass(freq=20, res=0.7):
    return el('effect', dict(type='highpass', frequency=freq, resonance=res))


def fx_reverb(wet=0.1, room=0.5, damp=0.45):
    return el('effect', dict(type='reverb', roomSize=room, damping=damp, wetLevel=wet))


def fx_delay(wet=0.0, fb=0.35, index=12, cutoff=5000, offset=0.012):
    return el('effect', dict(type='delay', delayTimeFormat='musical_time', delayTime=index, feedback=fb, stereoOffset=offset, wetLevel=wet, feedbackCutoff=cutoff))


def fx_chorus(mix=0.0, depth=0.25, rate=0.6):
    return el('effect', dict(type='chorus', mix=mix, modDepth=depth, modRate=rate))


def b_fx(i, param, **kw):
    return binding(type='effect', level='instrument', position=i, parameter=param, **kw)


def b_gfx(g, i, param, **kw):
    return binding(type='effect', level='group', groupIndex=g, effectIndex=i, parameter=param, **kw)


def b_amp(param, **kw):
    return binding(type='amp', level='instrument', position=0, parameter=param, **kw)


def b_group(g, param, typ='amp', **kw):
    return binding(type=typ, level='group', position=g, parameter=param, **kw)


def b_enable(g, on):
    return binding(type='general', level='group', position=g, parameter='ENABLED', translation='fixed_value', translationValue='true' if on else 'false')


def tone_knob(ui, x, y, fx_index, value=1.0, label='Tone'):
    return ui.knob(x, y, label, 0, 1, value, [b_fx(fx_index, 'FX_FILTER_FREQUENCY', translation='table', translationTable=TONE_TABLE)],
                   tip='Low-pass filter. CC74 moves it.')


def lowcut_knob(ui, x, y, fx_index, value=0.0):
    return ui.knob(x, y, 'Low Cut', 0, 1, value, [b_fx(fx_index, 'FX_FILTER_FREQUENCY', translation='table', translationTable=LOWCUT_TABLE)],
                   tip='High-pass filter, for thinning a layer.')


def room_knob(ui, x, y, fx_index, value=0.1):
    return ui.knob(x, y, 'Room', 0, 1, value, [b_fx(fx_index, 'FX_REVERB_WET_LEVEL')], tip='Reverb level. CC91 moves it.')


def cc_map(pairs):
    """pairs: [(cc number, control position)]"""
    return el('midi', None, [el('cc', dict(number=n), binding(level='ui', type='control', position=p, parameter='VALUE', translation='linear',
                                                                translationOutputMin=lo, translationOutputMax=hi)) for n, p, lo, hi in pairs])


def sections_for(xs, y, h, labels, w=78, pad=10):
    """Panels around groups of knobs: labels = [(label, first index, last index)]. Neighbouring panels keep a gap."""
    out = []
    for lab, a, b in labels:
        left = pad if a == 0 else min(pad, (xs[a] - xs[a - 1] - w) / 2 - 3)
        right = pad if b == len(xs) - 1 else min(pad, (xs[b + 1] - xs[b] - w) / 2 - 3)
        x0 = xs[a] - left
        x1 = xs[b] + w + right
        out.append((x0, y - 22, x1 - x0, h + 30, lab))
    return out


def common(groups, **attrs):
    """Attributes documented at <group> level (velocity tracking, key tracking, one-shot), applied to every group that hasn't set its own."""
    for g in groups:
        for k, v in attrs.items():
            if g.get(k) is None:
                g.set(k, fmt(v))
    return groups


def table_eval(table, v):
    pts = [tuple(map(float, p.split(','))) for p in table.split(';')]
    for (x0, y0), (x1, y1) in zip(pts, pts[1:]):
        if v <= x1:
            return y0 + (y1 - y0) * (v - x0) / (x1 - x0) if x1 > x0 else y1
    return pts[-1][1]


FX_ATTR = dict(FX_FILTER_FREQUENCY='frequency', FX_FILTER_RESONANCE='resonance', FX_REVERB_WET_LEVEL='wetLevel', FX_MIX='mix', FX_DRIVE='drive',
               FX_OUTPUT_LEVEL='outputLevel', FX_FEEDBACK='feedback', FX_DELAY_TIME='delayTime', FX_WET_LEVEL='wetLevel')


def sync_effects(root):
    """Knob bindings fire on load anyway; this keeps each effect's own attributes equal to what its knob will send."""
    groups = root.find('groups').findall('group')
    gfx = root.find('effects')
    for knob in root.find('ui').find('tab'):
        if knob.tag != 'labeled-knob':
            continue
        v = float(knob.get('value'))
        for b in knob.findall('binding'):
            if b.get('type') != 'effect' or b.get('parameter') not in FX_ATTR:
                continue
            if b.get('level') == 'group':
                eff = groups[int(b.get('groupIndex'))].find('effects')[int(b.get('effectIndex'))]
            else:
                eff = gfx[int(b.get('position'))]
            t = b.get('translation', 'linear')
            if t == 'table':
                out = table_eval(b.get('translationTable'), v)
            elif b.get('translationOutputMin') is not None:
                lo, hi = float(knob.get('minValue')), float(knob.get('maxValue'))
                out = float(b.get('translationOutputMin')) + (v - lo) / (hi - lo) * (float(b.get('translationOutputMax')) - float(b.get('translationOutputMin')))
            else:
                out = v
            eff.set(FX_ATTR[b.get('parameter')], fmt(round(out, 4)))


def save_preset(cat, title, fname, root, skin_args):
    sync_effects(root)
    bg = f'Images/{fname}.png'
    root.find('ui').set('bgImage', bg)
    skin(f'{LIB}/{bg}', **skin_args)
    write_preset(f'{LIB}/{fname}.dspreset', root)
    PRESETS.append((cat, fname, title))


def write_sfz(cat, fname, header, body, control=None):
    path = f'{LIB}/SFZ/{cat}/{fname}.sfz'
    os.makedirs(os.path.dirname(path), exist_ok=True)
    txt = '// ' + '\n// '.join(header) + '\n\n<control>\ndefault_path=../../\n' + (control + '\n' if control else '') + '\n' + body.strip() + '\n'
    with open(path, 'w') as fh:
        fh.write(txt)
    SFZS.append((cat, fname))


# ================================================================= drums
LANE_HIT = {'ukg': dict(K='kick_ukg', SN='snare_ukg', CL='clap_ukg', RIM='rim_ukg', CH='chat_ukg', OH='ohat_ukg'),
            'mth': dict(K='kick_tech', SN='snare_tech', CL='clap_tech', RIM='rim_tech', CH='chat_tech', OH='ohat_tech'),
            'sg': dict(K='kick_sg', SN='snare_ukg', CL='clap_ukg', RIM='rim_ukg', CH='chat_ukg', OH='ohat_ukg'),
            'ch': dict(K='kick_house', SN='snare_house', CL='clap_house', RIM='rim_tech', CH='chat_house', OH='ohat_house')}
SHARED = dict(SH='shaker', TB='tamb', COH='conga_hi', CLO='conga_lo', BH='bongo_hi', BL='bongo_lo', CLV='clave', WB='click', RD='ride', CR='crash',
              LT='tom_lo', MT='tom_mid', HT='tom_hi', CB='cowbell')
MIX = dict(CB=(0.3, 0.25), K=(1, 0), SN=(0.55, 0), CL=(0.62, 0), RIM=(0.36, -0.2), CH=(0.26, 0.25), OH=(0.3, 0.3), SH=(0.22, -0.3), TB=(0.22, 0.4),
           COH=(0.42, -0.35), CLO=(0.46, -0.2), BH=(0.38, 0.3), BL=(0.4, 0.15), CLV=(0.3, 0.35), WB=(0.34, -0.4), RD=(0.2, 0.3), CR=(0.35, 0),
           LT=(0.62, -0.3), MT=(0.6, 0), HT=(0.58, 0.3), SNAP=(0.5, 0.12))
NOTE = dict(K=36, RIM=37, SN=38, CL=39, CH=42, OH=46, LT=45, MT=47, HT=50, CR=49, RD=51, TB=54, CB=56, BH=60, BL=61, COH=63, CLO=64, SH=70, CLV=75, WB=76)
LABEL = dict(K='Kick', RIM='Rim', SN='Snare', CL='Clap', CH='Closed hat', OH='Open hat', LT='Low tom', MT='Mid tom', HT='High tom', CR='Crash', RD='Ride',
             TB='Tambourine', CB='Cowbell', BH='Bongo hi', BL='Bongo lo', COH='Conga hi', CLO='Conga lo', SH='Shaker', CLV='Clave', WB='Click', SNAP='Snap')
FAMILY = dict(K='kick', RIM='snare', SN='snare', CL='snare', CH='hats', OH='hats', RD='hats', CR='hats', LT='perc', MT='perc', HT='perc',
              TB='perc', CB='perc', BH='perc', BL='perc', COH='perc', CLO='perc', SH='perc', CLV='perc', WB='perc', SNAP='snare')
KICKS = [('kick_ukg', 'UKG punch'), ('kick_ukg_boom', 'UKG boom'), ('kick_tech', 'Tech'), ('kick_tech_round', 'Tech round'), ('kick_sg', 'Speed garage'),
         ('kick_house', 'House 909')]
GENRE_TITLE = dict(ukg='UK Garage', mth='Minimal Tech', sg='Speed Garage', ch='Classic House')
GENRE_LONG = dict(ukg='UK garage', mth='minimal tech house', sg='speed garage', ch='classic house')
FAM_COL = dict(kick=C['accent'], snare='#e6a25a', hats='#d9c46a', perc='#6db7da')
VEL_K = 1.0


GH = json.load(open(f'{W}/inst/gh_meta.json'))


def gh(key):
    v = GH[key]
    INFO[v['path']] = (v['frames'], v['ch'])
    return v['path']


def drum_kit(g, gh_kit=False):
    """g: genre key (ukg/mth/sg/ch). gh_kit: the garage-house (Disclosure-style) kit built from the new one-shots."""
    if gh_kit:
        rels = {l: take(i, 'Drums') for l, i in SHARED.items()}
        rels.update(K=gh('kick_gh'), SN=gh('snare_gh'), CL=gh('clap_gh'), RIM=gh('rim_gh'), CH=gh('chat_gh'), OH=gh('ohat_gh'), SH=gh('shaker_gh'), TB=gh('tamb_gh'),
                    SNAP=gh('snap_gh'))
        kicks = [(gh('kick_gh'), 'Garage house')] + [(take(k, 'Drums'), kl) for k, kl in KICKS]
        kick_default = 0
        title, gkey, color_genre = 'Garage House Kit', 'gh', 'gh'
        cat = 'Garage House'
        lcd1, lcd2 = 'GARAGE HOUSE KIT', 'GM MAP · 21 PADS · 7 KICKS'
        sub = 'Disclosure-style garage house drums: a round tuned kick, plate clap, crisp swung hats, snap and shaker. General MIDI mapped.'
    else:
        hits = dict(SHARED, **LANE_HIT[g])
        rels = {l: take(i, 'Drums') for l, i in hits.items()}
        rels['SNAP'] = gh('snap_gh')   # the garage-house snap on 40, so the GH kits' MIDI plays complete on every kit
        kicks = [(take(k, 'Drums'), kl) for k, kl in KICKS]
        kick_default = [k for k, _ in KICKS].index(hits['K'])
        title, gkey, color_genre = GENRE_TITLE[g] + ' Kit', g, g
        cat = 'Drums'
        lcd1 = {'ukg': 'UKG DRUM KIT', 'mth': 'MINIMAL DRUM KIT', 'sg': 'SPEED GARAGE KIT', 'ch': 'HOUSE DRUM KIT'}[g]
        lcd2 = 'GM MAP · 21 PADS · 6 KICKS'
        sub = f'The {GENRE_LONG[g]} drum kit, mapped to General MIDI so any drum MIDI from the pack plays it as written.'
    groups = []
    for i, (rel, kl) in enumerate(kicks):
        z = el('sample', dict(path=rel, rootNote=36, loNote=36, hiNote=36))
        groups.append(el('group', dict(name='Kick ' + kl, enabled=i == kick_default, tags='kick', volume=db(MIX['K'][0]), pan=0), z))
    lanes = [l for l in NOTE if l != 'K'] + ['SNAP']
    for l in lanes:
        n = NOTE.get(l, 40)
        z = el('sample', dict(path=rels[l], rootNote=n, loNote=n, hiNote=n))
        mix = MIX.get(l, (0.5, 0.1))
        a = dict(name=LABEL.get(l, 'Snap'), tags=FAMILY.get(l, 'snare') + (',chat' if l == 'CH' else ',ohat' if l == 'OH' else ''), volume=db(mix[0]), pan=int(mix[1] * 100))
        if l == 'OH':
            a.update(silencedByTags='chat,ohat', silencingDecay=0.02)
        groups.append(el('group', a, z))
    common(groups, ampVelTrack=VEL_K, pitchKeyTrack=0, ampEnvEnabled=False)
    groups_el = el('groups', dict(volume=db(1.6), tuningIgnoresKeyTrack=True, attack=0.0, release=0.05), groups)
    effects = el('effects', None, fx_lowpass(22000, 0.7), fx_reverb(0.1 if gh_kit else 0.08, 0.32, 0.5))
    ui = UI(color_genre)
    xs = knob_row(7, 40, 772, w=78)
    y = 162
    opts = [(kl, [b_enable(j, j == i) for j in range(len(kicks))]) for i, (_, kl) in enumerate(kicks)]
    ui.menu(126, 112, 150, 26, opts, value=kick_default + 1, name='Kick')
    tv = lambda tag: binding(type='amp', level='tag', identifier=tag, parameter='TAG_VOLUME')
    k = [ui.knob(xs[0], y, 'Kick', 0, 2, 1.0, [tv('kick')]),
         ui.knob(xs[1], y, 'Snare', 0, 2, 1.0, [tv('snare')], tip='Snare, clap, rim and snap'),
         ui.knob(xs[2], y, 'Hats', 0, 2, 1.0, [tv('hats')], tip='Hats, ride and crash'),
         ui.knob(xs[3], y, 'Perc', 0, 2, 1.0, [tv('perc')], tip='Toms, congas, bongos, shaker, tambourine, clave, cowbell'),
         ui.knob(xs[4], y, 'Pitch', -12, 12, 0.0, [b_amp('GLOBAL_TUNING')], tip='Tune the whole kit in semitones'),
         tone_knob(ui, xs[5], y, 0),
         room_knob(ui, xs[6], y, 1, 0.1 if gh_kit else 0.08)]
    midi = cc_map([(74, k[5], 0, 1), (91, k[6], 0, 1)])
    colors = [(36, 36, FAM_COL['kick'])] + [(NOTE.get(l, 40), NOTE.get(l, 40), FAM_COL[FAMILY.get(l, 'snare')]) for l in lanes]
    root = preset_root(ui, groups_el, effects, None, midi, None, kb(colors))
    legend = ['36 Kick   37 Rim   38 Snare   39 Clap   40 Snap   42 Closed hat   46 Open hat   49 Crash   51 Ride   45 / 47 / 50 Toms',
              '54 Tambourine   56 Cowbell   60 / 61 Bongos   63 / 64 Congas   70 Shaker   75 Clave   76 Click   (General MIDI, like every drum MIDI in the pack)']
    save_preset(cat, title, 'Garage House - Drum Kit' if gh_kit else f'Drums - {title}', root, dict(
        title=title, subtitle=sub, lcd1=lcd1, lcd2=lcd2, genre=color_genre,
        sections=sections_for(xs, y, 92, [('Levels', 0, 3), ('Tone', 4, 6)]), menus=[(126, 112, 150, 26, 'Kick')], legend=legend))
    lines = []
    for l in ['K'] + lanes:
        rel = kicks[kick_default][0] if l == 'K' else rels[l]
        mix = MIX.get(l, (0.5, 0.1))
        gain = 20 * math.log10(mix[0])
        extra = {}
        if l == 'CH':
            extra = dict(group=1)
        if l == 'OH':
            extra = dict(group=2, off_by=1, off_mode='normal')
        n = NOTE.get(l, 40)
        lines.append(sfz_line(key=n, pitch_keycenter=n, pitch_keytrack=0, loop_mode='one_shot', volume=round(gain, 1), pan=int(mix[1] * 100),
                              **extra, sample=rels.get(l, rel) if l != 'K' else rel) + '   // ' + LABEL.get(l, 'Snap'))
    write_sfz(cat if gh_kit else 'Drums', 'Drum Kit' if gh_kit else title, [f'{title}: drums on General MIDI notes (36 kick, 38 snare, 42/46 hats...).',
                                                  'From the UKG & Minimal Pack. Closed hat chokes the open hat.'],
              '<global> amp_veltrack=100 ampeg_release=0.05\n<region> ' + '\n<region> '.join(lines))


# ================================================================= break slicer
def zero_cross(x, i, win=90):
    lo, hi = max(1, i - win), min(len(x) - 2, i + win)
    seg = np.abs(x[lo:hi]).sum(1) if x.ndim == 2 else np.abs(x[lo:hi])
    return lo + int(np.argmin(seg))


BREAKS = [('brk3_slice', 'Slice-Ready 132 (UKG07)'), ('brk3_garage', 'Garage Break 132'), ('brk3_jungle', 'Jungle Edit 136')]


def break_slicer():
    groups, slice_data = [], []
    for bi, (bid, bl) in enumerate(BREAKS):
        it = ITEMS[bid]
        rel = take(bid, 'Breaks')
        x, _ = sf.read(f'{LIB}/{rel}')
        n = len(x)
        nsl = 16 * it['bars']
        bounds = [0] + [zero_cross(x, round(k * n / nsl)) for k in range(1, nsl)] + [n]
        zs = [el('sample', dict(path=rel, rootNote=36 + k, loNote=36 + k, hiNote=36 + k, start=bounds[k], end=bounds[k + 1] - 1)) for k in range(nsl)]
        slice_data.append((rel, bounds, it))
        groups.append(el('group', dict(name=bl + ' slices', enabled=bi == 0, tags='slice', silencedByTags='slice', silencingDecay=0.006, ampEnvEnabled=False,
                                       pitchKeyTrack=0), zs))
        groups.append(el('group', dict(name=bl + ' loop', enabled=bi == 0, sampleMode='slice_stretch', beats=4 * it['bars'], sliceGrid='1/16',
                                       sliceLoopMode='forward', attack=0.001, release=0.03),
                         el('sample', dict(path=rel, rootNote=72, loNote=72, hiNote=72))))
    common(groups, ampVelTrack=0.85)
    groups_el = el('groups', dict(volume=db(1.5), tuningIgnoresKeyTrack=True), groups)
    effects = el('effects', None, fx_highpass(20), fx_lowpass(22000), fx_reverb(0.06, 0.3, 0.5))
    ui = UI('ukg')
    opts = [(bl, [b_enable(2 * j + s, j == i) for j in range(len(BREAKS)) for s in (0, 1)]) for i, (_, bl) in enumerate(BREAKS)]
    ui.menu(126, 112, 220, 26, opts, value=1, name='Break')
    xs = knob_row(4, 160, 652)
    y = 162
    k = [ui.knob(xs[0], y, 'Pitch', -12, 12, 0.0, [b_amp('GLOBAL_TUNING')], tip='Retune every slice in semitones'),
         lowcut_knob(ui, xs[1], y, 0), tone_knob(ui, xs[2], y, 1), room_knob(ui, xs[3], y, 2, 0.06)]
    midi = cc_map([(74, k[2], 0, 1), (91, k[3], 0, 1)])
    colors = [(36, 51, C['ukg']), (52, 67, '#8e66a8'), (72, 72, C['accent'])]
    root = preset_root(ui, groups_el, effects, None, midi, None, kb(colors))
    save_preset('Drums', 'Break Slicer', 'Drums - Break Slicer', root, dict(
        title='Break Slicer', subtitle='Each 16th of the break on its own key, cut at zero crossings so slices start and stop cleanly.',
        lcd1='BREAK SLICER', lcd2='SLICES 36-67 · LOOP 72', genre='ukg', sections=sections_for(xs, y, 92, [('Shape', 0, 3)]),
        menus=[(126, 112, 220, 26, 'Break')],
        legend=['Notes 36-51: slices 1-16 of bar 1 (the order UKG07\'s Break Slices track expects).   Notes 52-67: bar 2 of the two-bar breaks.',
                'Note 72: the whole break, following your song\'s tempo while held. A new slice cuts the last one, like an MPC in mono.']))
    for (rel, bounds, it), (_, bl) in zip(slice_data, BREAKS):
        lines = [sfz_line(key=36 + k, pitch_keycenter=36 + k, pitch_keytrack=0, offset=bounds[k], end=bounds[k + 1] - 1, loop_mode='one_shot', group=1, off_by=1,
                          off_mode='fast', sample=rel) for k in range(len(bounds) - 1)]
        lines.append(sfz_line(key=72, pitch_keycenter=72, pitch_keytrack=0, loop_mode='loop_continuous', loop_start=0, loop_end=frames(rel) - 1, sample=rel)
                     + '   // whole break at its own tempo (' + str(it['bpm']) + ' BPM)')
        write_sfz('Drums', 'Break Slicer - ' + bl.split(' (')[0], [f'{bl}: 16 slices per bar from note 36; note 72 loops the whole break.'],
                  '<global> ampeg_attack=0.001 ampeg_release=0.01 amp_veltrack=85\n<region> ' + '\n<region> '.join(lines))


# ================================================================= tempo-synced loop player
def loop_player():
    plan = [('ukg', 'UK garage', ['dl3_ukg01', 'dl3_ukg05', 'dl3_ukg07', 'dl3_ukg08', 'top3_ukg_shuffle', 'top3_ukg_rimtamb', 'brk3_garage', 'brk3_slice', 'brk3_jungle']),
            ('mth', 'Minimal', ['dl3_mth01', 'dl3_mth04', 'dl3_mth07', 'dl3_mth08', 'top3_mth_skippy', 'top3_mth_ride', 'perc3_mth_beads', 'perc3_mth_bongo']),
            ('sg', 'Speed garage', ['drums_sg01', 'drums_sg06', 'dl3_sg07', 'dl3_sg08', 'top3_sg_2step', 'top3_sg_top']),
            ('ch', 'House', ['drums_ch01', 'drums_ch02', 'drums_ch03', 'drums_ch04', 'drums_ch05', 'drums_ch06', 'dl3_ch07', 'dl3_ch08', 'top3_ch_shaker',
                             'top3_ch_offbeat', 'perc3_ch_congas']),
            ('fills', 'Fills', [f'ukg_fill_0{i}' for i in range(1, 9)] + [f'mth_fill_0{i}' for i in range(1, 9)])]
    plan.append(('gh', 'Garage hse', ['gh_loop1', 'gh_loop2', 'gh_loop3', 'gh_loop4']))
    groups, mapping, colors = [], [], []
    base = {'ukg': 36, 'mth': 48, 'sg': 60, 'ch': 72, 'fills': 84, 'gh': 24}
    gcol = {'ukg': C['ukg'], 'mth': C['mth'], 'sg': C['sg'], 'ch': C['ch'], 'fills': '#d9c46a', 'gh': C['gh']}
    for g, gl, ids in plan:
        zs = []
        for i, iid in enumerate(ids):
            if g == 'gh':
                it = dict(GH[iid])
                rel = gh(iid)
            else:
                it = ITEMS[iid]
                cat = 'Loops/Fills' if g == 'fills' else 'Loops/' + {'ukg': 'UK Garage', 'mth': 'Minimal', 'sg': 'Speed Garage', 'ch': 'House'}[g]
                rel = take(iid, cat)
            key = base[g] + i
            assert it['bars'] and it['bpm'], iid
            zs.append(el('sample', dict(path=rel, rootNote=key, loNote=key, hiNote=key, beats=4 * it['bars'], tags=f'k{key}', silencedByTags=f'k{key}')))
            mapping.append((key, it['title'], it['bpm'], it['bars'], rel))
        groups.append(el('group', dict(name=gl, sampleMode='slice_stretch', sliceGrid='1/16', sliceLoopMode='forward', sliceEnvelope=100), zs))
        colors.append((base[g], base[g] + len(ids) - 1, gcol[g]))
    common(groups, ampVelTrack=0.6)
    groups_el = el('groups', dict(volume=db(1.4), attack=0.0, decay=0, sustain=1, release=0.04), groups)
    effects = el('effects', None, fx_highpass(20), fx_lowpass(22000), fx_reverb(0.0, 0.35, 0.5))
    ui = UI('all')
    gaps = [('Gaps: loop', 'forward'), ('Gaps: silent', 'off'), ('Gaps: back & forth', 'back_and_forth')]
    ui.menu(126, 112, 190, 26, [(n, [binding(type='general', level='group', position=gi, parameter='SLICE_LOOP_MODE', translation='fixed_value', translationValue=v)
                                     for gi in range(len(plan))]) for n, v in gaps], value=1, name='Gaps')
    xs = knob_row(10, 26, 786, w=70)
    y = 162
    k = [ui.knob(xs[i], y, lab, 0, 1.5, 1.0, [b_group(i, 'AMP_VOLUME')], w=70) for i, (_, lab, _) in enumerate(plan)]
    k.append(ui.knob(xs[6], y, 'Slice Env', 0, 100, 100, [binding(type='general', level='group', position=gi, parameter='SLICE_ENVELOPE') for gi in range(len(plan))],
                     w=70, tip='Below 100, each slice fades sooner: tighter and choppier'))
    k.append(ui.knob(xs[7], y, 'Low Cut', 0, 1, 0.0, [b_fx(0, 'FX_FILTER_FREQUENCY', translation='table', translationTable=LOWCUT_TABLE)], w=70))
    k.append(ui.knob(xs[8], y, 'Tone', 0, 1, 1.0, [b_fx(1, 'FX_FILTER_FREQUENCY', translation='table', translationTable=TONE_TABLE)], w=70))
    k.append(ui.knob(xs[9], y, 'Room', 0, 1, 0.0, [b_fx(2, 'FX_REVERB_WET_LEVEL')], w=70))
    midi = cc_map([(74, k[8], 0, 1), (91, k[9], 0, 1)])
    root = preset_root(ui, groups_el, effects, None, midi, None, kb(colors))
    save_preset('Loops', 'Loop Player', 'Loops - Loop Player (Tempo Sync)', root, dict(
        title='Loop Player', subtitle='Drum loops, tops, breaks and fills that follow your song\'s tempo. Hold a key; start on the beat.',
        lcd1='LOOP PLAYER', lcd2=f'{len(mapping)} LOOPS & FILLS · SYNC', genre='all', menus=[(126, 112, 190, 26, 'Gaps')],
        sections=sections_for(xs, y, 92, [('Levels', 0, 5), ('Shape', 6, 9)], w=70),
        legend=['24-27 Garage house (Disclosure-style)   36-44 UK garage   48-55 Minimal   60-65 Speed garage   72-82 House   84-99 Fills',
                'Every loop is sliced every 16th and re-timed to the host tempo, so pitch stays put. Notes above and below are not mapped.']))
    # SFZ: plain triggers at the loops' own tempo (SFZ players can't re-time)
    lines = [sfz_line(key=k_, pitch_keycenter=k_, pitch_keytrack=0, loop_mode='loop_continuous', loop_start=0, loop_end=frames(rel) - 1, sample=rel)
             + f'   // {t} ({bpm} BPM, {bars} bar{"s" if bars > 1 else ""})' for k_, t, bpm, bars, rel in mapping]
    write_sfz('Loops', 'Loop Player', ['Every loop on its own key (36 up). SFZ players can\'t follow the tempo, so loops play at the BPM shown.'],
              '<global> ampeg_release=0.04 amp_veltrack=60\n<region> ' + '\n<region> '.join(lines))
    return mapping


# ================================================================= FX hits
FX_COMMON = [(36, 'mth_fx_impact', 'Impact'), (41, 'ukg_fx_subdrop', 'Sub drop'), (43, 'ukg_fx_rewind', 'Rewind spin-back'), (44, 'mth_fx_tapestop', 'Tape stop')]
FX_FLAVOUR = [('UK garage', 'ukg_fx_revcrash', 'mth_fx_downsweep'), ('Minimal / house', 'mth_fx_revcrash', 'mth_fx_downsweep'),
              ('Speed garage', 'mth_fx_revcrash', 'sg_fx_reesedown')]
FX_EXTRA = ['ukg_fx_riser', 'mth_fx_riser', 'mth_fx_pitchriser', 'sg_fx_wobbleriser', 'fx3_hooverriser', 'fx3_acidriser', 'fx3_stringriser', 'fx3_whoosh_up',
            'fx3_whoosh_down', 'ukg_fx_revcrash', 'mth_fx_revcrash', 'mth_fx_downsweep', 'sg_fx_reesedown', 'fx3_bomb', 'fx3_airhorn', 'fx3_airhorn_low',
            'fx3_dubhorn', 'ukg_fx_siren', 'sg_fx_siren', 'ukg_fx_laser', 'ukg_fx_organecho', 'mth_fx_dubecho', 'mth_fx_glitch', 'ukg_fx_vinylstop',
            'fx3_crackle', 'fx3_clicks']


def fx_hits():
    groups = []
    zs = [el('sample', dict(path=take(i, 'FX'), rootNote=n, loNote=n, hiNote=n)) for n, i, _ in FX_COMMON]
    groups.append(el('group', dict(name='Fixed FX hits'), zs))
    for fi, (fl, rev, down) in enumerate(FX_FLAVOUR):
        groups.append(el('group', dict(name='Flavour ' + fl, enabled=fi == 0),
                         el('sample', dict(path=take(rev, 'FX'), rootNote=38, loNote=38, hiNote=38)),
                         el('sample', dict(path=take(down, 'FX'), rootNote=40, loNote=40, hiNote=40))))
    extra = []
    for i, iid in enumerate(FX_EXTRA):
        n = 48 + i
        extra.append((n, iid))
    groups.append(el('group', dict(name='FX keyboard'), [el('sample', dict(path=take(iid, 'FX'), rootNote=n, loNote=n, hiNote=n)) for n, iid in extra]))
    common(groups, ampEnvEnabled=False, pitchKeyTrack=0, ampVelTrack=0.7)
    groups_el = el('groups', dict(volume=db(1.3), tuningIgnoresKeyTrack=True), groups)
    effects = el('effects', None, fx_highpass(20), fx_lowpass(22000), fx_delay(0.0, 0.4, 12, 4500), fx_reverb(0.0, 0.6, 0.4))
    ui = UI('all')
    # flavour menu: 4 options (house shares the minimal set)
    fl_opts = []
    for name, gi in [('UK garage', 0), ('Minimal', 1), ('Speed garage', 2), ('Classic house', 1)]:
        fl_opts.append((name, [b_enable(1 + j, j == gi) for j in range(len(FX_FLAVOUR))]))
    ui.menu(126, 112, 170, 26, fl_opts, value=1, name='Flavour')
    xs = knob_row(7, 40, 772)
    y = 162
    k = [ui.knob(xs[0], y, 'Pitch', -12, 12, 0.0, [b_amp('GLOBAL_TUNING')]), lowcut_knob(ui, xs[1], y, 0), tone_knob(ui, xs[2], y, 1),
         ui.knob(xs[3], y, 'Delay', 0, 1, 0.0, [b_fx(2, 'FX_WET_LEVEL')], tip='Tempo-synced echo'),
         ui.knob(xs[4], y, 'Time', 0, 20, 12, [b_fx(2, 'FX_DELAY_TIME')], vtype='musical_time', tip='Echo time, synced to your song (default dotted 1/8)'),
         ui.knob(xs[5], y, 'Feedback', 0, 0.9, 0.4, [b_fx(2, 'FX_FEEDBACK')]),
         room_knob(ui, xs[6], y, 3, 0.0)]
    midi = cc_map([(74, k[2], 0, 1), (91, k[6], 0, 1)])
    colors = [(36, 44, C['accent'])] + [(48, 48 + len(extra) - 1, '#d9c46a')]
    root = preset_root(ui, groups_el, effects, None, midi, None, kb(colors))
    names = {iid: ITEMS[iid]['title'] for _, iid in extra}
    save_preset('FX', 'FX Hits', 'FX - FX Hits', root, dict(
        title='FX Hits', subtitle='The pack\'s risers, impacts and transitions. Notes 36-44 match the FX Hits track in every extended mix.',
        lcd1='FX HITS', lcd2=f'36-44 MIX FX · 48-{48 + len(extra) - 1} FX KEYS', genre='all', menus=[(126, 112, 170, 26, 'Flavour')],
        sections=sections_for(xs, y, 92, [('Pitch & tone', 0, 2), ('Echo & room', 3, 6)]),
        legend=['36 Impact   38 Reverse crash (one bar, peaks on the drop)   40 Downlifter   41 Sub drop   43 Rewind   44 Tape stop',
                '48 up: risers, whooshes, sweeps, bomb drop, airhorns, sirens, lasers, echoes, glitch, vinyl stop, crackle, clicks. All play to the end.']))
    lines = [sfz_line(key=n, pitch_keycenter=n, pitch_keytrack=0, loop_mode='one_shot', sample=take(i, 'FX')) + '   // ' + lab for n, i, lab in FX_COMMON]
    lines += [sfz_line(key=38, pitch_keycenter=38, pitch_keytrack=0, loop_mode='one_shot', sample=take('ukg_fx_revcrash', 'FX')) + '   // Reverse crash',
              sfz_line(key=40, pitch_keycenter=40, pitch_keytrack=0, loop_mode='one_shot', sample=take('mth_fx_downsweep', 'FX')) + '   // Downlifter']
    lines += [sfz_line(key=n, pitch_keycenter=n, pitch_keytrack=0, loop_mode='one_shot', sample=take(iid, 'FX')) + '   // ' + names[iid] for n, iid in extra]
    write_sfz('FX', 'FX Hits', ['Notes 36-44 match the FX Hits track in the extended mixes; 48 up is a keyboard of the pack\'s FX.'],
              '<global> amp_veltrack=70\n<region> ' + '\n<region> '.join(lines))
    return extra


# ================================================================= shouts and ad-libs
def kit_shouts():
    import subprocess
    out = subprocess.run(['node', '-e', "const P=require(" + json.dumps(PACK_ROOT + '/core.js') + ");console.log(JSON.stringify(P.KITS.filter(k=>k.ext).map(k=>[k.id,P.buildExtended(k,k.root).shouts])))"],
                         capture_output=True, text=True, check=True).stdout
    return [(k, s) for k, s in json.loads(out) if s]


def shouts():
    sets = [('All ad-libs', [f'a{i:02d}' for i in range(1, 17)] + [f'ba{i:02d}' for i in range(1, 13)])]
    for kid, lst in kit_shouts():
        sets.append((kid + ' shouts', lst))
    groups = []
    for si, (sl, ids) in enumerate(sets):
        zs = [el('sample', dict(path=take(iid, 'Vocals/Shouts'), rootNote=48 + j, loNote=48 + j, hiNote=48 + j)) for j, iid in enumerate(ids)]
        groups.append(el('group', dict(name=sl, enabled=si == 0), zs))
    common(groups, ampEnvEnabled=False, pitchKeyTrack=0, ampVelTrack=0.6)
    groups_el = el('groups', dict(volume=db(1.2), tuningIgnoresKeyTrack=True), groups)
    effects = el('effects', None, fx_highpass(20), fx_lowpass(22000), fx_delay(0.0, 0.35, 12, 4000), fx_reverb(0.05, 0.4, 0.5))
    ui = UI('all')
    ui.menu(126, 112, 170, 26, [(sl, [b_enable(j, j == i) for j in range(len(sets))]) for i, (sl, _) in enumerate(sets)], value=1, name='Set')
    xs = knob_row(6, 70, 742)
    y = 162
    k = [ui.knob(xs[0], y, 'Pitch', -12, 12, 0.0, [b_amp('GLOBAL_TUNING')]), lowcut_knob(ui, xs[1], y, 0), tone_knob(ui, xs[2], y, 1),
         ui.knob(xs[3], y, 'Delay', 0, 1, 0.0, [b_fx(2, 'FX_WET_LEVEL')]),
         ui.knob(xs[4], y, 'Time', 0, 20, 12, [b_fx(2, 'FX_DELAY_TIME')], vtype='musical_time'),
         room_knob(ui, xs[5], y, 3, 0.05)]
    midi = cc_map([(74, k[2], 0, 1), (91, k[5], 0, 1)])
    root = preset_root(ui, groups_el, effects, None, midi, None, kb([(48, 48 + len(sets[0][1]) - 1, C['accent'])]))
    titles = {iid: ITEMS[iid]['title'] for iid in sets[0][1]}
    save_preset('Vocals', 'Shouts & Ad-libs', 'Vocals - Shouts & Ad-libs', root, dict(
        title='Shouts & Ad-libs', subtitle='MC shouts and brat ad-libs, one per key from note 48. Pick a kit to match its Shouts track.',
        lcd1='SHOUTS & AD-LIBS', lcd2=f'{len(sets[0][1])} VOCALS · FROM 48', genre='all', menus=[(126, 112, 170, 26, 'Set')],
        sections=sections_for(xs, y, 92, [('Pitch & tone', 0, 2), ('Echo & room', 3, 5)]),
        legend=['All ad-libs: 48-63 MC and US/UK ad-libs (Wheel it up!, Come again!, Oi oi!...), 64-75 brat ad-libs (Eew, Obviously, Whatever...).',
                'MTH08 / SG07 / SG08: the exact layout of that extended mix\'s Shouts track, so the MIDI plays the right shout.']))
    for sl, ids in sets:
        lines = [sfz_line(key=48 + j, pitch_keycenter=48 + j, pitch_keytrack=0, loop_mode='one_shot', sample=take(iid, 'Vocals/Shouts')) + '   // ' + ITEMS[iid]['title']
                 for j, iid in enumerate(ids)]
        write_sfz('Vocals', 'Shouts - ' + sl, [sl + ': one shout per key from note 48.'], '<global> amp_veltrack=60\n<region> ' + '\n<region> '.join(lines))
    return sets


# ================================================================= vocal chops (pitched)
CHOP_DEFAULT = 's06'   # "Ooh" on D5 sits in the middle of the extended mixes' Vocal Chops range (67-86)


def vocal_chops():
    stabs = [f's{i:02d}' for i in range(1, 23)]   # s01-s10 from the first wave, s11-s22 sung by the wave-6 engine
    groups = []
    info = []
    for si, sid in enumerate(stabs):
        it = ITEMS[sid]
        sung = re.search(r'sung on ([A-G][b#]?\d)', it['note']).group(1)
        root = nnum(sung)
        rel = take(sid, 'Vocals/Chops', dry=True)
        groups.append(el('group', dict(name=f"{it['title']} ({sung})", enabled=sid == CHOP_DEFAULT),
                         el('sample', dict(path=rel, rootNote=root, loNote=24, hiNote=108))))
        info.append((sid, it['title'], sung, root, rel, it['voice']))
    common(groups, ampVelTrack=0.7)
    groups_el = el('groups', dict(volume=db(1.3), attack=0.002, decay=0, sustain=1, release=0.12), groups)
    effects = el('effects', None, fx_highpass(140), fx_lowpass(22000), fx_delay(0.22, 0.35, 12, 4200), fx_reverb(0.14, 0.45, 0.5))
    ui = UI('all')
    ui.menu(126, 112, 220, 26, [(f'{t} · {s} · {v}', [b_enable(j, j == i) for j in range(len(stabs))]) for i, (_, t, s, _, _, v) in enumerate(info)],
            value=stabs.index(CHOP_DEFAULT) + 1, name='Chop')
    xs = knob_row(8, 30, 782)
    y = 162
    k = [ui.knob(xs[0], y, 'Attack', 0, 1, 0.002, [b_amp('ENV_ATTACK')]),
         ui.knob(xs[1], y, 'Release', 0, 3, 0.12, [b_amp('ENV_RELEASE')]),
         lowcut_knob(ui, xs[2], y, 0, 0.45), tone_knob(ui, xs[3], y, 1),
         ui.knob(xs[4], y, 'Delay', 0, 1, 0.22, [b_fx(2, 'FX_WET_LEVEL')]),
         ui.knob(xs[5], y, 'Time', 0, 20, 12, [b_fx(2, 'FX_DELAY_TIME')], vtype='musical_time'),
         ui.knob(xs[6], y, 'Feedback', 0, 0.9, 0.35, [b_fx(2, 'FX_FEEDBACK')]),
         room_knob(ui, xs[7], y, 3, 0.14)]
    midi = cc_map([(74, k[3], 0, 1), (91, k[7], 0, 1)])
    root = preset_root(ui, groups_el, effects, None, midi, None, None)
    save_preset('Vocals', 'Vocal Chops', 'Vocals - Vocal Chops', root, dict(
        title='Vocal Chops', subtitle=f'{len(stabs)} sung syllables, playable across the keyboard. Each is mapped at the note it was sung on.',
        lcd1='VOCAL CHOPS', lcd2=f'{len(stabs)} SYLLABLES · DRY + FX', genre='all', menus=[(126, 112, 220, 26, 'Chop')],
        sections=sections_for(xs, y, 92, [('Envelope', 0, 1), ('Tone', 2, 3), ('Echo & room', 4, 7)]),
        legend=['Play the Vocal Chops track from any extended mix through this, or write your own chop riffs. The samples are dry; echo and room are live.',
                'Time is tempo-synced (dotted 1/8 to start). Lower notes drop the formants, higher ones chipmunk, as a sampler does.']))
    for sid, t, s, rootn, rel, v in info:
        write_sfz('Vocals', f'Vocal Chop - {t} ({s})', [f'"{t}" sung on {s} by {v}, mapped across the keyboard.'],
                  '<global> ampeg_attack=0.002 ampeg_release=0.12 amp_veltrack=70\n<region> ' + sfz_line(lokey=24, hikey=108, pitch_keycenter=rootn, sample=rel))
    return info


# ================================================================= melodic multisample helper
def spread(roots, lo=21, hi=108):
    """Key ranges for a sorted list of root notes: each zone reaches halfway to its neighbours."""
    out = []
    for i, r in enumerate(roots):
        a = lo if i == 0 else (roots[i - 1] + r) // 2 + 1
        b = hi if i == len(roots) - 1 else (r + roots[i + 1]) // 2
        out.append((r, a, b))
    return out


def zones_for(entries, lo=21, hi=108, vel=None, loop=False):
    """entries: [(root, rel, loop tuple or None)]"""
    zs, sfz = [], []
    roots = [e[0] for e in entries]
    for (r, a, b), (_, rel, lp) in zip(spread(roots, lo, hi), entries):
        att = dict(path=rel, rootNote=r, loNote=a, hiNote=b)
        so = dict(lokey=a, hikey=b, pitch_keycenter=r)
        if vel:
            att.update(loVel=vel[0], hiVel=vel[1])
            so.update(lovel=vel[0], hivel=vel[1])
        if lp:
            att.update(loopStart=lp[0], loopEnd=lp[1], loopEnabled=True)
            so.update(loop_mode='loop_continuous', loop_start=lp[0], loop_end=lp[1])
        zs.append(el('sample', att))
        sfz.append(sfz_line(**so, sample=rel))
    return zs, sfz


def inst_entries(prefix, suffix=''):
    out = []
    for k, v in INST.items():
        m = re.match(r'^%s_(\d+)%s$' % (prefix, suffix), k)
        if m:
            INFO[v['path']] = (v['frames'], v['ch'])
            out.append((v['root'], v['path'], tuple(v['loop']) if v['loop'] else None))
    return sorted(out)


# ================================================================= Reese (samples + sine sub, mono with glide, wobble)
def reese():
    ent = inst_entries('reese')
    zs, sfzl = zones_for(ent, 12, 72)
    g0 = el('group', dict(name='Reese', tags='reese', volume=db(1.0)), zs, el('effects', None, el('effect', dict(type='wave_shaper', drive=1.0, outputLevel=1.0))))
    g1 = el('group', dict(name='Sub', volume=db(0.35)), el('oscillator', dict(waveform='sine', polyphony=1)))
    common([g0, g1], ampVelTrack=0.4)
    groups_el = el('groups', dict(volume=db(1.1), attack=0.003, decay=0, sustain=1, release=0.08, glideMode='legato', glideTime=0.06), g0, g1)
    effects = el('effects', None, fx_lowpass(4600, 1.2), fx_chorus(0.0, 0.3, 0.45), fx_reverb(0.0, 0.4, 0.5))
    mods = el('modulators', None, el('lfo', dict(shape='sine', frequencyFormat='musical_time', frequency=10, modAmount=0.0, scope='global'),
                                     b_fx(0, 'FX_FILTER_FREQUENCY', modBehavior='add', translation='linear', translationOutputMin=0, translationOutputMax=2600)))
    tags = el('tags', None, el('tag', dict(name='reese', polyphony=1)))
    ui = UI('sg')
    xs = knob_row(9, 26, 786, w=74)
    y = 150   # Cutoff starts around 2.9 kHz so the upward-only wobble has room to open it
    DRIVE_D = '0,1;0.25,2.5;0.5,6;0.75,14;1.0001,30'
    DRIVE_O = '0,1;0.25,0.62;0.5,0.42;0.75,0.33;1.0001,0.28'
    k = [ui.knob(xs[0], y, 'Cutoff', 0, 1, 0.7, [b_fx(0, 'FX_FILTER_FREQUENCY', translation='table', translationTable=CUTOFF_TABLE)], w=74, tip='CC74'),
         ui.knob(xs[1], y, 'Reso', 0.3, 4.5, 1.2, [b_fx(0, 'FX_FILTER_RESONANCE')], w=74, tip='CC71'),
         ui.knob(xs[2], y, 'Drive', 0, 1, 0.2, [b_gfx(0, 0, 'FX_DRIVE', translation='table', translationTable=DRIVE_D),
                                               b_gfx(0, 0, 'FX_OUTPUT_LEVEL', translation='table', translationTable=DRIVE_O)], w=74),
         ui.knob(xs[3], y, 'Wobble', 0, 1, 0.0, [binding(type='modulator', level='instrument', position=0, parameter='MOD_AMOUNT')], w=74,
                 tip='Filter LFO depth. CC1 (mod wheel) moves it, like the speed garage bass MIDI does.'),
         ui.knob(xs[4], y, 'Rate', 0, 20, 10, [binding(type='modulator', level='instrument', position=0, parameter='FREQUENCY')], vtype='musical_time', w=74,
                 tip='Wobble speed, synced to your song'),
         ui.knob(xs[5], y, 'Sub', 0, 1, 0.35, [b_group(1, 'AMP_VOLUME')], w=74, tip='A clean sine on the same note, under the Reese'),
         ui.knob(xs[6], y, 'Glide', 0, 0.5, 0.06, [b_amp('GLIDE_TIME')], w=74, tip='Slide time between overlapping notes'),
         ui.knob(xs[7], y, 'Release', 0, 2, 0.08, [b_amp('ENV_RELEASE')], w=74),
         ui.knob(xs[8], y, 'Width', 0, 1, 0.0, [b_fx(1, 'FX_MIX')], w=74, tip='Chorus on the whole bass')]
    midi = cc_map([(74, k[0], 0, 1), (1, k[3], 0, 1), (71, k[1], 0.3, 4.5)])
    root = preset_root(ui, groups_el, effects, mods, midi, tags, None)
    save_preset('Bass', 'Reese Bass', 'Bass - Reese Bass', root, dict(
        title='Reese Bass', subtitle='Detuned-saw Reese for speed garage, multisampled C1-C4 with seamless loops. Mono, with glide and a synced wobble.',
        lcd1='REESE BASS', lcd2='MONO · GLIDE · WOBBLE', genre='sg', sections=sections_for(xs, y, 92, [('Filter', 0, 2), ('Wobble', 3, 4), ('Voice', 5, 8)], w=74),
        legend=['Plays the Bass and Reese tracks from any speed garage MIDI. CC74 opens the filter in builds, CC1 adds wobble (the MIDI already writes both).',
                'Overlap notes to slide between them. The sub is a pure sine following the same note.']))
    write_sfz('Bass', 'Reese Bass', ['Reese bass, 13 looped samples C1-C4. Mono legato with 60 ms glide.'],
              '<global> ampeg_attack=0.003 ampeg_release=0.08 amp_veltrack=40\n'
              '<group> group=1 off_by=1 off_mode=fast fil_type=lpf_2p cutoff=150 resonance=2 cutoff_oncc74=6800\n<region> ' + '\n<region> '.join(sfzl)
              + '\n<group> group=2 off_by=2 off_mode=fast volume=-9\n<region> lokey=12 hikey=72 pitch_keycenter=60 sample=*sine',
              control='set_cc74=110\nlabel_cc74=Cutoff')


# ================================================================= 2-step sub (synth + 808 samples)
def sub_bass():
    subs = [(nnum(ITEMS[f'sub3_{n}1']['noteName']), take(f'sub3_{n}1', 'Bass/Sub808'), None) for n in FNAMES]
    subs.sort()
    zs, sfzl = zones_for(subs, 12, 60)
    gsyn = el('group', dict(name='Sine synth', tags='sub', volume=db(0.5)), el('oscillator', dict(waveform='sine', polyphony=1)),
              el('effects', None, el('effect', dict(type='wave_shaper', drive=1.0, outputLevel=1.0))))
    gsmp = el('group', dict(name='808 samples', tags='sub', enabled=False, volume=db(1.0)), zs)
    gclk = el('group', dict(name='Click', volume=db(0.12), attack=0, decay=0.012, sustain=0, release=0.01, ampVelTrack=1.0),
              el('oscillator', dict(waveform='noise')), el('effects', None, el('effect', dict(type='bandpass', frequency=3200, resonance=0.8))))
    common([gsyn, gsmp, gclk], ampVelTrack=0.35)
    groups_el = el('groups', dict(volume=db(1.0), attack=0.002, decay=0.9, sustain=0.55, release=0.12, glideMode='legato', glideTime=0.03),
                   gsyn, gsmp, gclk)
    effects = el('effects', None, fx_lowpass(3000, 0.7))
    mods = el('modulators', None, el('envelope', dict(attack=0, decay=0.07, sustain=0, release=0.01, modAmount=0.5, scope='voice'),
                                     b_group(0, 'GROUP_TUNING', modBehavior='add', translation='linear', translationOutputMin=0, translationOutputMax=12)))
    tags = el('tags', None, el('tag', dict(name='sub', polyphony=1)))
    ui = UI('ukg')
    src_opts = [('Sine synth', [b_enable(0, True), b_enable(1, False)]), ('808 samples', [b_enable(0, False), b_enable(1, True)])]
    ui.menu(126, 112, 170, 26, src_opts, value=1, name='Source')
    xs = knob_row(8, 30, 782)
    y = 162
    DRIVE_D = '0,1;0.25,2;0.5,4;0.75,8;1.0001,16'
    DRIVE_O = '0,1;0.25,0.7;0.5,0.5;0.75,0.42;1.0001,0.38'
    k = [ui.knob(xs[0], y, 'Decay', 0.05, 4, 0.9, [b_amp('ENV_DECAY')]),
         ui.knob(xs[1], y, 'Sustain', 0, 1, 0.55, [b_amp('ENV_SUSTAIN')]),
         ui.knob(xs[2], y, 'Dip', 0, 1, 0.5, [binding(type='modulator', level='instrument', position=0, parameter='MOD_AMOUNT')],
                 tip='Pitch drops onto the note at the start, up to an octave'),
         ui.knob(xs[3], y, 'Drive', 0, 1, 0.15, [b_gfx(0, 0, 'FX_DRIVE', translation='table', translationTable=DRIVE_D),
                                                b_gfx(0, 0, 'FX_OUTPUT_LEVEL', translation='table', translationTable=DRIVE_O)], tip='Harmonics so the sub reads on small speakers'),
         ui.knob(xs[4], y, 'Click', 0, 0.5, 0.12, [b_group(2, 'AMP_VOLUME')]),
         tone_knob(ui, xs[5], y, 0, 0.62),
         ui.knob(xs[6], y, 'Glide', 0, 0.5, 0.03, [b_amp('GLIDE_TIME')]),
         ui.knob(xs[7], y, 'Release', 0, 2, 0.12, [b_amp('ENV_RELEASE')])]
    midi = cc_map([(74, k[5], 0, 1)])
    root = preset_root(ui, groups_el, effects, mods, midi, tags, None)
    save_preset('Bass', '2-Step Sub', 'Bass - 2-Step Sub', root, dict(
        title='2-Step Sub', subtitle='A garage sine sub with a pitch dip, drive for small speakers and a click. Or switch to the pack\'s 808 subs.',
        lcd1='2-STEP SUB', lcd2='MONO · DIP · DRIVE', genre='ukg', menus=[(126, 112, 170, 26, 'Source')],
        sections=sections_for(xs, y, 92, [('Envelope', 0, 2), ('Colour', 3, 5), ('Voice', 6, 7)]),
        legend=['Plays the Bass and Sub tracks from the UK garage and house MIDI. Decay and Sustain set how long notes ring; Sustain 1 holds them like an organ.',
                'Dip drops the pitch onto each note (808 style). Overlapping notes slide by the Glide time.']))
    write_sfz('Bass', '2-Step Sub', ['2-step 808 sub: the pack\'s 12 sub samples C1-B1, stretched across the bass range.'],
              '<global> ampeg_attack=0.002 ampeg_decay=0.9 ampeg_sustain=55 ampeg_release=0.12 polyphony=1\n<region> ' + '\n<region> '.join(sfzl))


# ================================================================= acid (303-style synth)
def acid():
    g0 = el('group', dict(name='Acid', tags='acid', volume=db(0.42)), el('oscillator', dict(waveform='saw', polyphony=1)),
            el('effects', None, fx_lowpass(420, 3.2), el('effect', dict(type='wave_shaper', drive=2.5, outputLevel=0.62))))
    common([g0], ampVelTrack=0.3)
    groups_el = el('groups', dict(volume=db(1.0), attack=0.002, decay=0, sustain=1, release=0.03, glideMode='legato', glideTime=0.06), g0)
    effects = el('effects', None, fx_delay(0.0, 0.42, 12, 3200), fx_reverb(0.0, 0.4, 0.5))
    mods = el('modulators', None,
              el('envelope', dict(attack=0.002, decay=0.22, sustain=0, release=0.05, modAmount=0.6, scope='voice'),
                 b_gfx(0, 0, 'FX_FILTER_FREQUENCY', modBehavior='add', translation='linear', translationOutputMin=0, translationOutputMax=4200)),
              el('midiVelocity', dict(modAmount=0.4, scope='voice'),
                 b_gfx(0, 0, 'FX_FILTER_FREQUENCY', modBehavior='add', translation='linear', translationOutputMin=0, translationOutputMax=1800)))
    tags = el('tags', None, el('tag', dict(name='acid', polyphony=1)))
    ui = UI('mth')
    ui.menu(126, 112, 150, 26, [('Saw', [binding(type='general', level='group', position=0, parameter='OSCILLATOR_WAVEFORM', translation='fixed_value', translationValue='saw')]),
                                ('Square', [binding(type='general', level='group', position=0, parameter='OSCILLATOR_WAVEFORM', translation='fixed_value', translationValue='square')])],
            value=1, name='Wave')
    xs = knob_row(8, 30, 782)
    y = 162
    k = [ui.knob(xs[0], y, 'Cutoff', 0, 1, 0.38, [b_gfx(0, 0, 'FX_FILTER_FREQUENCY', translation='table', translationTable=CUTOFF_TABLE)], tip='CC74'),
         ui.knob(xs[1], y, 'Reso', 0.5, 5, 3.2, [b_gfx(0, 0, 'FX_FILTER_RESONANCE')], tip='CC71, which the acid sections ramp'),
         ui.knob(xs[2], y, 'Env Mod', 0, 1, 0.6, [binding(type='modulator', level='instrument', position=0, parameter='MOD_AMOUNT')]),
         ui.knob(xs[3], y, 'Decay', 0.03, 1.5, 0.22, [binding(type='modulator', level='instrument', position=0, parameter='ENV_DECAY')]),
         ui.knob(xs[4], y, 'Accent', 0, 1, 0.4, [binding(type='modulator', level='instrument', position=1, parameter='MOD_AMOUNT')],
                 tip='How much harder notes open the filter'),
         ui.knob(xs[5], y, 'Drive', 1, 12, 2.5, [b_gfx(0, 1, 'FX_DRIVE')]),
         ui.knob(xs[6], y, 'Slide', 0, 0.3, 0.06, [b_amp('GLIDE_TIME')], tip='Overlap two notes to slide, as on a 303'),
         ui.knob(xs[7], y, 'Delay', 0, 1, 0.0, [b_fx(0, 'FX_WET_LEVEL')], tip='Dotted 1/8 echo')]
    midi = cc_map([(74, k[0], 0, 1), (71, k[1], 0.5, 5)])
    root = preset_root(ui, groups_el, effects, mods, midi, tags, None)
    save_preset('Bass', 'Acid Bass', 'Bass - Acid Bass', root, dict(
        title='Acid Bass', subtitle='A 303-style mono synth: resonant filter with its own envelope, accent from velocity, and slides on overlapping notes.',
        lcd1='ACID BASS', lcd2='303 STYLE · SLIDE · ACCENT', genre='mth', menus=[(126, 112, 150, 26, 'Wave')],
        sections=sections_for(xs, y, 92, [('Filter', 0, 1), ('Envelope', 2, 4), ('Voice', 5, 7)]),
        legend=['Plays the Acid 303 tracks in the new songs (MTH07, MTH08 and others). Their CC71 ramps open the resonance in the acid sections.',
                'Overlapping notes slide; velocity accents. Turn Cutoff down and Env Mod up for the classic squelch.']))


# ================================================================= keys
def melodic(cat, title, fname, genre, groups, groups_attrs, knobs_spec, effects_list, sub, lcd1, lcd2, legend, mods=None, tags=None, menu=None, ccs=None,
            sections=None, sfz=None):
    """Generic melodic preset: knobs_spec = list of callables (ui, x, y) -> position."""
    ga = dict(groups_attrs)
    common(groups, ampVelTrack=ga.pop('ampVelTrack', 1.0))
    groups_el = el('groups', ga, groups)
    effects = el('effects', None, effects_list)
    ui = UI(genre)
    menus = []
    if menu:
        name, opts, w = menu[:3]
        ui.menu(126, 112, w, 26, opts, value=menu[3] if len(menu) > 3 else 1, name=name)
        menus = [(126, 112, w, 26, name)]
    n = len(knobs_spec)
    xs = knob_row(n, 30 if n > 6 else 70, 782 if n > 6 else 742)
    y = 162 if menu else 150
    pos = [f(ui, xs[i], y) for i, (lab, f) in enumerate(knobs_spec)]
    named = {lab: p for (lab, _), p in zip(knobs_spec, pos)}
    midi = cc_map([(cc, named[lab], lo, hi) for cc, lab, lo, hi in (ccs or []) if lab in named]) if ccs else None
    root = preset_root(ui, groups_el, effects, mods, midi, tags, None)
    sec = sections_for(xs, y, 92, [(lab, a, b) for lab, a, b in sections]) if sections else []
    save_preset(cat, title, fname, root, dict(title=title, subtitle=sub, lcd1=lcd1, lcd2=lcd2, genre=genre, sections=sec, menus=menus, legend=legend))
    if sfz:
        write_sfz(cat, fname.split(' - ', 1)[1], sfz[0], sfz[1])


def k_attack(maxv=2.0, v=0.002):
    return ('Attack', lambda ui, x, y: ui.knob(x, y, 'Attack', 0, maxv, v, [b_amp('ENV_ATTACK')]))


def k_release(maxv=4.0, v=0.3):
    return ('Release', lambda ui, x, y: ui.knob(x, y, 'Release', 0, maxv, v, [b_amp('ENV_RELEASE')]))


def k_tone(i, v=1.0):
    return ('Tone', lambda ui, x, y: tone_knob(ui, x, y, i, v))


def k_lowcut(i, v=0.0):
    return ('Low Cut', lambda ui, x, y: lowcut_knob(ui, x, y, i, v))


def k_room(i, v=0.12):
    return ('Room', lambda ui, x, y: room_knob(ui, x, y, i, v))


def k_chorus(i, v=0.0):
    return ('Chorus', lambda ui, x, y: ui.knob(x, y, 'Chorus', 0, 1, v, [b_fx(i, 'FX_MIX')]))


def k_delay(i, v=0.0):
    return ('Delay', lambda ui, x, y: ui.knob(x, y, 'Delay', 0, 1, v, [b_fx(i, 'FX_WET_LEVEL')], tip='Tempo-synced echo (dotted 1/8)'))


def k_time(i, v=12):
    return ('Time', lambda ui, x, y: ui.knob(x, y, 'Time', 0, 20, v, [b_fx(i, 'FX_DELAY_TIME')], vtype='musical_time'))


def k_feedback(i, v=0.4):
    return ('Feedback', lambda ui, x, y: ui.knob(x, y, 'Feedback', 0, 0.92, v, [b_fx(i, 'FX_FEEDBACK')]))


STD_CC = [(74, 'Tone', 0, 1), (91, 'Room', 0, 1)]


def organ_bass():
    ent = []
    for n in ['C', 'Cs', 'D', 'Eb', 'E', 'F', 'Fs', 'G', 'Ab', 'A', 'Bb', 'B']:
        for o in (1, 2):
            iid = f'organ_{n}{o}'
            ent.append((nnum(n + str(o)), take(iid, 'Bass/OrganBass'), None))
    ent.sort()
    zs, sfzl = zones_for(ent, 12, 72)
    g = el('group', dict(name='Organ bass', tags='bass'), zs, el('effects', None, el('effect', dict(type='wave_shaper', drive=1.0, outputLevel=1.0))))
    DRIVE_D = '0,1;0.5,4;1.0001,12'
    DRIVE_O = '0,1;0.5,0.5;1.0001,0.36'
    voice = ('Voice', [('Poly', [binding(type='general', level='tag', identifier='bass', parameter='TAG_POLYPHONY', translation='fixed_value', translationValue=16)]),
                       ('Mono', [binding(type='general', level='tag', identifier='bass', parameter='TAG_POLYPHONY', translation='fixed_value', translationValue=1)])], 130)
    melodic('Bass', 'Organ Bass', 'Bass - Organ Bass', 'ch', [g],
            dict(volume=db(1.3), attack=0.002, decay=0, sustain=1, release=0.06, ampVelTrack=0.5, glideMode='legato', glideTime=0.0),
            [('Tone', lambda ui, x, y: ui.knob(x, y, 'Tone', 0, 1, 0.85, [b_fx(0, 'FX_FILTER_FREQUENCY', translation='table', translationTable=CUTOFF_TABLE)])),
             ('Reso', lambda ui, x, y: ui.knob(x, y, 'Reso', 0.3, 4, 0.8, [b_fx(0, 'FX_FILTER_RESONANCE')])),
             ('Drive', lambda ui, x, y: ui.knob(x, y, 'Drive', 0, 1, 0.0, [b_gfx(0, 0, 'FX_DRIVE', translation='table', translationTable=DRIVE_D),
                                                                       b_gfx(0, 0, 'FX_OUTPUT_LEVEL', translation='table', translationTable=DRIVE_O)])),
             ('Glide', lambda ui, x, y: ui.knob(x, y, 'Glide', 0, 0.4, 0.0, [b_amp('GLIDE_TIME')], tip='Slides between overlapping notes in Mono')),
             k_release(2.0, 0.06), k_room(1, 0.0)],
            [fx_lowpass(9000, 0.8), fx_reverb(0.0, 0.35, 0.5)],
            'The M1-style organ bass from the classic house kits: 24 notes C1-B2, each sampled. Mono mode adds glide.',
            'ORGAN BASS', '24 NOTES · C1-B2', ['Plays the Bass track of any classic house MIDI (CH01-CH08). Tone follows CC74 in the builds.',
                                               'Switch Voice to Mono and raise Glide for sliding basslines.'],
            tags=el('tags', None, el('tag', dict(name='bass', polyphony=16))), menu=voice, ccs=[(74, 'Tone', 0, 1), (91, 'Room', 0, 1)],
            sections=[('Filter', 0, 2), ('Voice', 3, 5)],
            sfz=(['Organ bass, 24 samples C1-B2.'], '<global> ampeg_attack=0.002 ampeg_release=0.06 amp_veltrack=50\n<region> ' + '\n<region> '.join(sfzl)))


def house_piano():
    ent = sorted((m, take(f'piano_{m}', 'Keys/Piano'), None) for m in range(36, 85, 3))
    zs, sfzl = zones_for(ent)
    melodic('Keys', 'House Piano', 'Keys - House Piano', 'ch', [el('group', dict(name='Piano'), zs)],
            dict(volume=db(1.6), attack=0.002, decay=0, sustain=1, release=0.25, ampVelTrack=0.75),
            [k_attack(1.0), k_release(3.0, 0.25), k_tone(0), k_chorus(1, 0.0), k_delay(2, 0.0), k_time(2), k_room(3, 0.12)],
            [fx_lowpass(22000), fx_chorus(0.0, 0.25, 0.5), fx_delay(0.0, 0.35, 12, 4800), fx_reverb(0.12, 0.45, 0.5)],
            'The bright house piano from the classic house kits, sampled every minor third C2-C6.',
            'HOUSE PIANO', '17 ZONES · C2-C6', ['Plays the Chords and Piano Fills tracks of the classic house MIDI. Add Chorus for the M1 piano shimmer.',
                                                 'Time is tempo-synced; Delay sets the echo level.'],
            ccs=STD_CC, sections=[('Envelope', 0, 1), ('Tone', 2, 3), ('Echo & room', 4, 6)],
            sfz=(['House piano, 17 samples C2-C6.'], '<global> volume=-6 ampeg_release=0.25 amp_veltrack=75\n<region> ' + '\n<region> '.join(sfzl)))


def garage_organ():
    ent = inst_entries('organ')
    zs, sfzl = zones_for(ent, loop=True)
    melodic('Keys', 'Garage Organ', 'Keys - Garage Organ', 'ukg', [el('group', dict(name='Organ'), zs)],
            dict(volume=db(1.0), attack=0.002, decay=0, sustain=1, release=0.08, ampVelTrack=0.5),
            [k_attack(1.0), k_release(2.0, 0.08), k_tone(0), k_chorus(1, 0.45), k_delay(2, 0.0), k_time(2), k_feedback(2, 0.35), k_room(3, 0.1)],
            [fx_lowpass(22000), fx_chorus(0.45, 0.16, 6.2), fx_delay(0.0, 0.35, 12, 4000), fx_reverb(0.1, 0.35, 0.5)],
            'M1-style drawbar organ for 2-step stabs: a percussive third harmonic and key click, then a sustain that loops cleanly.',
            'GARAGE ORGAN', '17 ZONES · LOOPED', ['Plays the Chords and Stabs tracks of the UK garage MIDI (the organ stab kits). Chorus is the fast Leslie shimmer.',
                                                   'Short notes give the classic stab; hold them and the organ sustains.'],
            ccs=STD_CC, sections=[('Envelope', 0, 1), ('Tone', 2, 3), ('Echo & room', 4, 7)],
            sfz=(['Garage organ, 17 looped samples C2-C6.'], '<global> volume=-6 ampeg_release=0.08 amp_veltrack=50\n<region> ' + '\n<region> '.join(sfzl)))


def rhodes():
    soft = inst_entries('rhodes', '_soft')
    hard = inst_entries('rhodes', '_hard')
    zs1, s1 = zones_for(soft, vel=(1, 84))
    zs2, s2 = zones_for(hard, vel=(85, 127))
    mods = el('modulators', None, el('lfo', dict(shape='sine', frequencyFormat='musical_time', frequency=10, modAmount=0.35, scope='global'),
                                     [binding(type='amp', level='group', position=g, parameter='PAN', modBehavior='modulate', translation='linear',
                                              translationOutputMin=-70, translationOutputMax=70) for g in (0, 1)]))
    melodic('Keys', 'Garage Rhodes', 'Keys - Garage Rhodes', 'ukg', [el('group', dict(name='Soft'), zs1), el('group', dict(name='Hard'), zs2)],
            dict(volume=db(1.7), attack=0.002, decay=0, sustain=1, release=0.35, ampVelTrack=0.6),
            [('Tremolo', lambda ui, x, y: ui.knob(x, y, 'Tremolo', 0, 1, 0.35, [binding(type='modulator', level='instrument', position=0, parameter='MOD_AMOUNT')],
                                                  tip='Suitcase auto-pan depth')),
             ('Rate', lambda ui, x, y: ui.knob(x, y, 'Rate', 0, 20, 10, [binding(type='modulator', level='instrument', position=0, parameter='FREQUENCY')],
                                               vtype='musical_time', tip='Auto-pan speed, synced to your song')),
             k_tone(0), k_chorus(1, 0.2), k_delay(2, 0.0), k_time(2), k_release(3.0, 0.35), k_room(3, 0.12)],
            [fx_lowpass(22000), fx_chorus(0.2, 0.18, 0.7), fx_delay(0.0, 0.35, 12, 4200), fx_reverb(0.12, 0.45, 0.5)],
            'An FM electric piano for garage chords: two velocity layers (soft and bell-bright), with a tempo-synced suitcase auto-pan.',
            'GARAGE RHODES', '2 LAYERS · 34 SAMPLES', ['Plays the Chords and Melody tracks of the UK garage MIDI (UKG02 Silk Rhodes and others).',
                                                        'Velocity above 84 switches to the bright layer. Tremolo and Rate set the stereo auto-pan.'],
            mods=mods, ccs=STD_CC, sections=[('Tremolo', 0, 1), ('Tone', 2, 3), ('Echo', 4, 5), ('Output', 6, 7)],
            sfz=(['Garage Rhodes, soft (vel 1-84) and hard (85-127) layers, C2-C6.'],
                 '<global> ampeg_release=0.35 amp_veltrack=60\n<region> ' + '\n<region> '.join(s1 + s2)))


def strings_preset():
    ent = inst_entries('strings')
    zs, sfzl = zones_for(ent, loop=True)
    melodic('Keys', 'Strings', 'Keys - Strings', 'ch', [el('group', dict(name='Strings'), zs)],
            dict(volume=db(1.5), attack=0.25, decay=0, sustain=1, release=0.9, ampVelTrack=0.4),
            [k_attack(3.0, 0.25), k_release(5.0, 0.9), k_lowcut(0, 0.0), k_tone(1, 1.0), k_chorus(2, 0.25), k_room(3, 0.2)],
            [fx_highpass(20), fx_lowpass(22000), fx_chorus(0.25, 0.22, 0.4), fx_reverb(0.2, 0.6, 0.45)],
            'A wide string ensemble for house pads and swells: three detuned voices a side with vibrato, looped so chords sustain forever.',
            'STRINGS', '17 ZONES · STEREO LOOPS', ['Plays the Pad and Strings tracks of any song. CC11 swells in the MIDI ride the volume; CC74 is the Tone knob.',
                                                    'Raise Attack for slow swells, drop it for stabs.'],
            ccs=STD_CC, sections=[('Envelope', 0, 1), ('Tone', 2, 3), ('Space', 4, 5)],
            sfz=(['String ensemble, 17 looped stereo samples C2-C6.'], '<global> ampeg_attack=0.25 ampeg_release=0.9 amp_veltrack=40\n<region> ' + '\n<region> '.join(sfzl)))


def dub_chords():
    chords = [('m7', 'Minor 7'), ('m9', 'Minor 9'), ('m11', 'Minor 11 (fourths)'), ('maj9', 'Major 9')]
    groups, sfz_parts = [], []
    for i, (c, lab) in enumerate(chords):
        v = INST[f'dub_{c}']
        INFO[v['path']] = (v['frames'], v['ch'])
        groups.append(el('group', dict(name=lab, enabled=i == 0), el('sample', dict(path=v['path'], rootNote=57, loNote=33, hiNote=84))))
        sfz_parts.append((lab, v['path']))
    opts = [(lab, [b_enable(j, j == i) for j in range(len(chords))]) for i, (_, lab) in enumerate(chords)]
    melodic('Keys', 'Dub Chords', 'Keys - Dub Chords', 'mth', groups,
            dict(volume=db(1.5), attack=0.002, decay=0.35, sustain=0.0, release=0.12, ampVelTrack=0.5),
            [('Length', lambda ui, x, y: ui.knob(x, y, 'Length', 0.05, 1.5, 0.35, [b_amp('ENV_DECAY')], tip='How long each stab rings')),
             k_tone(1, 0.75), k_delay(2, 0.45), k_time(2, 12), k_feedback(2, 0.62),
             ('Room', lambda ui, x, y: room_knob(ui, x, y, 3, 0.3))],
            [fx_highpass(160), fx_lowpass(22000), fx_delay(0.45, 0.62, 12, 2600, 0.018), fx_reverb(0.3, 0.82, 0.55)],
            'One chord on every key, Basic Channel style: a dark filtered stab into a darkening, tempo-synced echo and a big room.',
            'DUB CHORDS', 'CHORD MEMORY · ROOT A3', ['Every key plays the whole chord from that root (A3 = note 57). Plays the Stabs track of the minimal MIDI.',
                                                     'CC91 in the MIDI throws the room; Feedback past 0.8 runs away like a tape echo.'],
            menu=('Chord', opts, 190), ccs=STD_CC, sections=[('Stab', 0, 1), ('Echo', 2, 4), ('Room', 5, 5)],
            sfz=(['Dub chord stabs rooted on A3 (57): minor 7 (default).'], '<global> ampeg_decay=0.35 ampeg_sustain=0 ampeg_release=0.12\n<region> '
                 + sfz_line(lokey=33, hikey=84, pitch_keycenter=57, sample=sfz_parts[0][1])))


# ================================================================= garage house (Disclosure-style) melodic set
def gh_entries(prefix):
    out = []
    for k, v in GH.items():
        m = re.match(r'^%s_(\d+)$' % prefix, k)
        if m:
            INFO[v['path']] = (v['frames'], v['ch'])
            out.append((v['root'], v['path'], tuple(v['loop']) if v['loop'] else None))
    return sorted(out)


VOICE_MENU = lambda tag, default_poly=True: ('Voice', [('Poly', [binding(type='general', level='tag', identifier=tag, parameter='TAG_POLYPHONY', translation='fixed_value', translationValue=16)]),
                                                        ('Mono', [binding(type='general', level='tag', identifier=tag, parameter='TAG_POLYPHONY', translation='fixed_value', translationValue=1)])], 130)


def gh_pluck():
    zs, sfzl = zones_for(gh_entries('pluck'))
    melodic('Garage House', 'Chord Pluck', 'Garage House - Chord Pluck', 'gh', [el('group', dict(name='Pluck'), zs)],
            dict(volume=db(1.5), attack=0.0, decay=0, sustain=1, release=0.18, ampVelTrack=0.6),
            [('Pump', lambda ui, x, y: ui.knob(x, y, 'Pump', 0, 0.25, 0.0, [b_amp('ENV_ATTACK')], tip='Slower attack: the sidechained swell of garage-house chords')),
             k_release(2.0, 0.18), k_tone(0, 1.0), k_chorus(1, 0.3), k_delay(2, 0.18), k_time(2, 12), k_room(3, 0.14)],
            [fx_lowpass(22000), fx_chorus(0.3, 0.25, 0.55), fx_delay(0.18, 0.35, 12, 4200), fx_reverb(0.14, 0.45, 0.5)],
            'Disclosure-style chord pluck: two detuned saws and a square through a snappy filter. Made for offbeat minor 7th and 9th chords.',
            'CHORD PLUCK', 'GARAGE HOUSE · 17 ZONES', ['Play it on offbeats with a minor 9th voicing; raise Pump to fake the sidechain swell on every chord.',
                                                         'Plays any Chords or Stabs track in the pack. Chorus widens it; the echo is tempo-synced.'],
            ccs=STD_CC, sections=[('Envelope', 0, 1), ('Tone', 2, 3), ('Echo & room', 4, 6)],
            sfz=(['Garage-house chord pluck (Disclosure-style), 17 samples C2-C6.'], '<global> volume=-6 ampeg_release=0.18 amp_veltrack=60\n<region> ' + '\n<region> '.join(sfzl)))


def gh_bass():
    zs, sfzl = zones_for(gh_entries('gbass'), 12, 72)
    g = el('group', dict(name='Glide bass', tags='gbass'), zs,
           el('effects', None, fx_lowpass(900, 1.1), el('effect', dict(type='wave_shaper', drive=1.0, outputLevel=1.0))))
    mods = el('modulators', None, el('envelope', dict(attack=0.002, decay=0.16, sustain=0, release=0.05, modAmount=0.5, scope='voice'),
                                     b_gfx(0, 0, 'FX_FILTER_FREQUENCY', modBehavior='add', translation='linear', translationOutputMin=0, translationOutputMax=2600)))
    DRIVE_D = '0,1;0.5,4;1.0001,12'
    DRIVE_O = '0,1;0.5,0.5;1.0001,0.36'
    melodic('Garage House', 'Glide Bass', 'Garage House - Glide Bass', 'gh', [g],
            dict(volume=db(1.2), attack=0.003, decay=0, sustain=1, release=0.07, ampVelTrack=0.4, glideMode='legato', glideTime=0.08),
            [('Tone', lambda ui, x, y: ui.knob(x, y, 'Tone', 0, 1, 0.55, [b_gfx(0, 0, 'FX_FILTER_FREQUENCY', translation='table', translationTable=CUTOFF_TABLE)], tip='CC74')),
             ('Pluck', lambda ui, x, y: ui.knob(x, y, 'Pluck', 0, 1, 0.5, [binding(type='modulator', level='instrument', position=0, parameter='MOD_AMOUNT')],
                                                tip='How far the filter opens at the start of each note')),
             ('Decay', lambda ui, x, y: ui.knob(x, y, 'Decay', 0.03, 1.2, 0.16, [binding(type='modulator', level='instrument', position=0, parameter='ENV_DECAY')])),
             ('Drive', lambda ui, x, y: ui.knob(x, y, 'Drive', 0, 1, 0.1, [b_gfx(0, 1, 'FX_DRIVE', translation='table', translationTable=DRIVE_D),
                                                                      b_gfx(0, 1, 'FX_OUTPUT_LEVEL', translation='table', translationTable=DRIVE_O)])),
             ('Glide', lambda ui, x, y: ui.knob(x, y, 'Glide', 0, 0.5, 0.08, [b_amp('GLIDE_TIME')], tip='Overlap notes to slide (Mono)')),
             k_release(1.5, 0.07)],
            [fx_reverb(0.0, 0.3, 0.5)],
            'Disclosure-style deep bass: a round sine with a filtered square on top, a plucky filter, and slides between overlapping notes.',
            'GLIDE BASS', 'MONO · GLIDE · PLUCK', ['Starts in Mono so overlapping notes slide, like the bouncy basslines on early garage-house records.',
                                                    'Plays any Bass or Sub track in the pack. Tone follows CC74.'],
            mods=mods, tags=el('tags', None, el('tag', dict(name='gbass', polyphony=1))), menu=VOICE_MENU('gbass') + (2,),
            ccs=[(74, 'Tone', 0, 1)], sections=[('Filter', 0, 3), ('Voice', 4, 5)],
            sfz=(['Garage-house glide bass (Disclosure-style), 13 looped samples C1-C4. Mono.'],
                 '<global> ampeg_attack=0.003 ampeg_release=0.07 polyphony=1\n<region> ' + '\n<region> '.join(sfzl)))


def gh_stab():
    zs, sfzl = zones_for(gh_entries('sqstab'), 24, 96)
    melodic('Garage House', 'Square Stab', 'Garage House - Square Stab', 'gh', [el('group', dict(name='Square stab', tags='stab'), zs)],
            dict(volume=db(1.0), attack=0.002, decay=0.32, sustain=0.55, release=0.12, ampVelTrack=0.5, glideMode='legato', glideTime=0.05),
            [k_attack(1.0), ('Decay', lambda ui, x, y: ui.knob(x, y, 'Decay', 0.02, 2, 0.32, [b_amp('ENV_DECAY')])),
             ('Sustain', lambda ui, x, y: ui.knob(x, y, 'Sustain', 0, 1, 0.55, [b_amp('ENV_SUSTAIN')])),
             ('Tone', lambda ui, x, y: ui.knob(x, y, 'Tone', 0, 1, 0.82, [b_fx(0, 'FX_FILTER_FREQUENCY', translation='table', translationTable=CUTOFF_TABLE)], tip='CC74')),
             ('Glide', lambda ui, x, y: ui.knob(x, y, 'Glide', 0, 0.4, 0.05, [b_amp('GLIDE_TIME')], tip='Slides in Mono')),
             k_chorus(1, 0.2), k_delay(2, 0.15), k_room(3, 0.1)],
            [fx_lowpass(5000, 1.4), fx_chorus(0.2, 0.25, 0.5), fx_delay(0.15, 0.35, 12, 4000), fx_reverb(0.1, 0.4, 0.5)],
            'A hollow detuned-square stab for riffs and hooks, the kind that carried 2010s garage-house singles. Mono mode slides between notes.',
            'SQUARE STAB', 'DETUNED SQUARES · LOOPED', ['Short notes stab, held notes sustain. Switch Voice to Mono for sliding riffs.',
                                                         'Plays Melody, Counter-melody and Stabs tracks. Tone follows CC74.'],
            tags=el('tags', None, el('tag', dict(name='stab', polyphony=16))), menu=VOICE_MENU('stab'), ccs=STD_CC,
            sections=[('Envelope', 0, 2), ('Tone', 3, 4), ('Space', 5, 7)],
            sfz=(['Detuned square stab (Disclosure-style), 13 looped samples C2-C5.'],
                 '<global> ampeg_decay=0.32 ampeg_sustain=55 ampeg_release=0.12\n<region> ' + '\n<region> '.join(sfzl)))


def gh_pad():
    zs, sfzl = zones_for(gh_entries('dpad'))
    melodic('Garage House', 'Deep Pad', 'Garage House - Deep Pad', 'gh', [el('group', dict(name='Pad'), zs)],
            dict(volume=db(1.5), attack=0.4, decay=0, sustain=1, release=1.2, ampVelTrack=0.3),
            [k_attack(4.0, 0.4), k_release(6.0, 1.2), k_lowcut(0, 0.2), k_tone(1, 0.85), k_chorus(2, 0.35), k_room(3, 0.25)],
            [fx_highpass(20), fx_lowpass(22000), fx_chorus(0.35, 0.25, 0.35), fx_reverb(0.25, 0.7, 0.5)],
            'A warm, dark saw pad for the lush breakdowns of garage house: detuned saws with slow vibrato and a sine an octave down.',
            'DEEP PAD', '17 ZONES · STEREO LOOPS', ['Hold minor 9th chords through a breakdown. CC11 swells and CC74 from the pack\'s Pad tracks ride it.',
                                                     'Low Cut keeps it out of the bassline\'s way.'],
            ccs=STD_CC, sections=[('Envelope', 0, 1), ('Tone', 2, 3), ('Space', 4, 5)],
            sfz=(['Deep garage-house pad (Disclosure-style), 17 looped stereo samples C2-C6.'],
                 '<global> ampeg_attack=0.4 ampeg_release=1.2 amp_veltrack=30\n<region> ' + '\n<region> '.join(sfzl)))


# ================================================================= wave 5: house stabs, vocal mosaic, hoover, donk
W5 = json.load(open(f'{W}/inst/w5_meta.json'))


def w5(key):
    v = W5[key]
    INFO[v['path']] = (v['frames'], v['ch'])
    return v


def w5_entries(prefix):
    out = []
    for k, v in W5.items():
        if re.match(r'^%s_(\d+)$' % prefix, k):
            w5(k)
            out.append((v['root'], v['path'], tuple(v['loop']) if v['loop'] else None))
    return sorted(out)


HOUSE_STABS = [('brass3_bbm11', 'Brass stab Bbm11', 58), ('brass3_gbmaj9', 'Brass stab Gbmaj9', 66), ('brass3_ab6', 'Brass stab Ab6', 56),
               ('brass3_ebm9_fall', 'Brass fall Ebm9', 63), ('orch3_am', 'Orchestra hit Am', 57), ('orch3_f', 'Orchestra hit F', 53),
               ('orch3_g', 'Orchestra hit G', 55), ('choir3_fmaj9', 'Choir aah Fmaj9', 53), ('choir3_am9', 'Choir aah Am9', 57),
               ('choir3_dm9_stab', 'Choir stab Dm9', 50)]


def house_stabs():
    groups, info = [], []
    for i, (iid, lab, rootn) in enumerate(HOUSE_STABS):
        rel = take(iid, 'Keys/HouseStabs')
        groups.append(el('group', dict(name=lab, enabled=i == 0), el('sample', dict(path=rel, rootNote=rootn, loNote=24, hiNote=96))))
        info.append((lab, rootn, rel))
    opts = [(lab, [b_enable(j, j == i) for j in range(len(HOUSE_STABS))]) for i, (_, lab, _) in enumerate(HOUSE_STABS)]
    melodic('Keys', 'House Stabs', 'Keys - House Stabs', 'ch', groups,
            dict(volume=db(1.3), attack=0.001, decay=0, sustain=1, release=0.25, ampVelTrack=0.6),
            [k_attack(1.0, 0.001), k_release(3.0, 0.25), k_lowcut(0, 0.0), k_tone(1, 1.0), k_delay(2, 0.0), k_time(2), k_room(3, 0.12)],
            [fx_highpass(20), fx_lowpass(22000), fx_delay(0.0, 0.35, 12, 4200), fx_reverb(0.12, 0.5, 0.5)],
            'Chord memory for house and garage: brass stabs and a brass fall, orchestra hits and a gospel choir, a whole chord on every key.',
            'HOUSE STABS', 'CHORD MEMORY · 10 STABS', ['Each key plays the whole chord from that root: each stab sits at its own pitch on its root (Bbm11 on note 58, Am on 57; see README).',
                                                        'Plays the Brass, Orchestra Hits and Choir tracks of the house MIDI too. Low Cut thins a stab under a vocal.'],
            menu=('Stab', opts, 200), ccs=STD_CC, sections=[('Envelope', 0, 1), ('Tone', 2, 3), ('Echo & room', 4, 6)])
    for lab, rootn, rel in info:
        write_sfz('Keys', 'House Stab - ' + lab, [f'{lab}: the whole chord on every key, rooted on {nname(rootn)} ({rootn}).'],
                  '<global> ampeg_attack=0.001 ampeg_release=0.25 amp_veltrack=60\n<region> ' + sfz_line(lokey=24, hikey=96, pitch_keycenter=rootn, sample=rel))
    return info


MOSAIC = ['h19', 'h20', 'h21', 'h22', 'h02', 'h03', 'h06', 'h14', 'h16', 'bh3', 'bh5', 'h11', 'h12', 'h24', 'h29', 'h30']


def vocal_mosaic():
    mo = W5['_mosaic']
    groups, info = [], []
    for hi, hid in enumerate(MOSAIC):
        h = mo[hid]
        zs = []
        for k, c in enumerate(h['chops']):
            v = w5(c['key'])
            zs.append(el('sample', dict(path=v['path'], rootNote=48 + k, loNote=48 + k, hiNote=48 + k)))
        groups.append(el('group', dict(name=h['title'] + ' (chops)', enabled=hi == 0, tags='chop', silencedByTags='chop', silencingDecay=0.015, pitchKeyTrack=0), zs))
        lv = w5(f'mosaic_{hid}_loop')
        groups.append(el('group', dict(name=h['title'] + ' (whole hook)', enabled=hi == 0, sampleMode='slice_stretch', beats=16, sliceGrid='1/16',
                                       sliceLoopMode='forward', attack=0.001, release=0.03, pitchKeyTrack=0),
                         el('sample', dict(path=lv['path'], rootNote=72, loNote=72, hiNote=72))))
        info.append((hid, h, lv))
    common(groups, ampVelTrack=0.6)
    groups_el = el('groups', dict(volume=db(1.25), attack=0.002, decay=0, sustain=1, release=0.06, tuningIgnoresKeyTrack=True), groups)
    effects = el('effects', None, fx_highpass(120), fx_lowpass(22000), fx_delay(0.18, 0.35, 12, 4200), fx_reverb(0.12, 0.45, 0.5))
    ui = UI('gh')
    short = lambda t: t if len(t) <= 34 else t.split(',')[0]
    opts = [(f'{hid.upper()} · {short(h["title"])}', [b_enable(2 * j + s_, j == i) for j in range(len(MOSAIC)) for s_ in (0, 1)]) for i, (hid, h, _) in enumerate(info)]
    ui.menu(126, 112, 300, 26, opts, value=1, name='Hook')
    xs = knob_row(8, 30, 782)
    y = 162
    k = [ui.knob(xs[0], y, 'Pitch', -12, 12, 0.0, [b_amp('GLOBAL_TUNING')], tip='Retune every chop in semitones'),
         ui.knob(xs[1], y, 'Attack', 0, 0.5, 0.002, [b_amp('ENV_ATTACK')]),
         ui.knob(xs[2], y, 'Release', 0, 2, 0.06, [b_amp('ENV_RELEASE')]),
         lowcut_knob(ui, xs[3], y, 0, 0.35), tone_knob(ui, xs[4], y, 1),
         ui.knob(xs[5], y, 'Delay', 0, 1, 0.18, [b_fx(2, 'FX_WET_LEVEL')], tip='Tempo-synced echo'),
         ui.knob(xs[6], y, 'Time', 0, 20, 12, [b_fx(2, 'FX_DELAY_TIME')], vtype='musical_time'),
         room_knob(ui, xs[7], y, 3, 0.12)]
    midi = cc_map([(74, k[4], 0, 1), (91, k[7], 0, 1)])
    root = preset_root(ui, groups_el, effects, None, midi, None, None)
    save_preset('Vocals', 'Vocal Mosaic', 'Vocals - Vocal Mosaic', root, dict(
        title='Vocal Mosaic', subtitle='The pack\'s sung hooks cut into syllables, one per key, for Disclosure and Todd Edwards-style chop riffs.',
        lcd1='VOCAL MOSAIC', lcd2=f'{len(MOSAIC)} HOOKS · CHOPS FROM 48', genre='gh', menus=[(126, 112, 300, 26, 'Hook')],
        sections=sections_for(xs, y, 92, [('Pitch & shape', 0, 2), ('Tone', 3, 4), ('Echo & room', 5, 7)]),
        legend=['48 up: the hook\'s sung notes in order, one chop per key (Or-, -ange, light, on...). 72: the whole hook, following your tempo.',
                'A new chop cuts the last one, MPC-style. Chops play at the pitch they were sung; Pitch retunes them all. Dry samples, live echo.']))
    for hid, h, lv in info:
        lines = [sfz_line(key=48 + j, pitch_keycenter=48 + j, pitch_keytrack=0, loop_mode='one_shot', group=1, off_by=1, off_mode='fast', sample=W5[c['key']]['path'])
                 + '   // ' + c['word'] for j, c in enumerate(h['chops'])]
        lines.append(sfz_line(key=72, pitch_keycenter=72, pitch_keytrack=0, loop_mode='loop_continuous', loop_start=0, loop_end=lv['frames'] - 1, sample=lv['path'])
                     + f'   // the whole hook at {h["bpm"]} BPM')
        write_sfz('Vocals', f'Vocal Mosaic - {hid.upper()}', [f'"{h["title"]}": one chop per key from note 48; note 72 loops the whole hook at {h["bpm"]} BPM.'],
                  '<global> ampeg_attack=0.002 ampeg_release=0.06 amp_veltrack=60\n<region> ' + '\n<region> '.join(lines))
    return [(hid, h['title'], h['bpm'], [c['word'] for c in h['chops']]) for hid, h, _ in info]


def hoover_preset():
    zs, sfzl = zones_for(w5_entries('hoover'), 12, 96)
    g = el('group', dict(name='Hoover', tags='hoover'), zs)
    mods = el('modulators', None,
              el('envelope', dict(attack=0.0, decay=0.16, sustain=0, release=0.05, modAmount=0.55, scope='voice'),
                 b_group(0, 'GROUP_TUNING', modBehavior='add', translation='table', translationTable='0,0;1.0001,-8')))
    melodic('Bass', 'Hoover', 'Bass - Hoover', 'sg', [g],
            dict(volume=db(0.95), attack=0.004, decay=0, sustain=1, release=0.15, ampVelTrack=0.4, glideMode='legato', glideTime=0.12),
            [('Swoop', lambda ui, x, y: ui.knob(x, y, 'Swoop', 0, 1, 0.55, [binding(type='modulator', level='instrument', position=0, parameter='MOD_AMOUNT')],
                                               tip='How far below the note each one starts, scooping up into pitch (up to 8 semitones)')),
             ('Swoop Time', lambda ui, x, y: ui.knob(x, y, 'Swoop Time', 0.02, 0.8, 0.16, [binding(type='modulator', level='instrument', position=0, parameter='ENV_DECAY')])),
             ('Tone', lambda ui, x, y: ui.knob(x, y, 'Tone', 0, 1, 0.9, [b_fx(0, 'FX_FILTER_FREQUENCY', translation='table', translationTable=CUTOFF_TABLE)], tip='CC74')),
             ('Glide', lambda ui, x, y: ui.knob(x, y, 'Glide', 0, 0.6, 0.12, [b_amp('GLIDE_TIME')], tip='Slides between overlapping notes in Mono')),
             k_chorus(1, 0.35), k_release(2.0, 0.15), k_room(2, 0.08)],
            [fx_lowpass(9000, 0.9), fx_chorus(0.35, 0.3, 0.6), fx_reverb(0.08, 0.5, 0.5)],
            'A rave hoover: detuned saws, a pulse whose width sweeps, and the octave below, scooping up into every note. Mono with glide, or poly.',
            'HOOVER', 'SWOOP · GLIDE · 17 LOOPS', ['Plays the Hoover tracks in SG07 and MTH08 (their pitch-bend dives need a bend range of 12, if your host lets you set one).',
                                                  'Swoop sets how far below each note starts; overlap notes in Mono to slide.'],
            mods=mods, tags=el('tags', None, el('tag', dict(name='hoover', polyphony=1))), menu=VOICE_MENU('hoover') + (2,),
            ccs=[(74, 'Tone', 0, 1), (91, 'Room', 0, 1)], sections=[('Swoop', 0, 1), ('Tone', 2, 3), ('Space', 4, 6)],
            sfz=(['Rave hoover, 17 looped stereo samples C1-C5. Mono with glide.'],
                 '<global> ampeg_attack=0.004 ampeg_release=0.15 amp_veltrack=40 polyphony=1 pitcheg_depth=-450 pitcheg_decay=0.16 pitcheg_sustain=0\n<region> ' + '\n<region> '.join(sfzl)))


def donk_preset():
    zs, sfzl = zones_for(w5_entries('donk'), 12, 84)
    g = el('group', dict(name='Donk', tags='donk'), zs)
    melodic('Bass', 'Donk', 'Bass - Donk', 'sg', [g],
            dict(volume=db(1.2), attack=0.0, decay=0.3, sustain=0.0, release=0.05, ampVelTrack=0.6),
            [('Length', lambda ui, x, y: ui.knob(x, y, 'Length', 0.03, 1.0, 0.3, [b_amp('ENV_DECAY')], tip='How long each donk rings')),
             ('Tone', lambda ui, x, y: ui.knob(x, y, 'Tone', 0, 1, 1.0, [b_fx(0, 'FX_FILTER_FREQUENCY', translation='table', translationTable=TONE_TABLE)], tip='CC74')),
             k_delay(1, 0.0), k_time(1, 12), k_feedback(1, 0.3), k_room(2, 0.05)],
            [fx_lowpass(22000), fx_delay(0.0, 0.3, 12, 3200), fx_reverb(0.05, 0.3, 0.5)],
            'The bassline donk: a hollow sine that drops into pitch with a square bite and a woody knock. Made for offbeat speed garage and bassline riffs.',
            'DONK', '13 ZONES · C1-C4', ['Plays the Donk track in SG08: offbeat roots with the octave on the last one. Try it on any speed garage bassline.',
                                         'Short and punchy by default; raise Length for a rounder, ringing donk.'],
            ccs=[(74, 'Tone', 0, 1), (91, 'Room', 0, 1)], sections=[('Shape', 0, 1), ('Echo & room', 2, 5)],
            sfz=(['Bassline donk, 13 samples C1-C4.'], '<global> ampeg_decay=0.3 ampeg_sustain=0 ampeg_release=0.05 amp_veltrack=60\n<region> ' + '\n<region> '.join(sfzl)))


# ================================================================= library info, README, main
MENU_ORDER = ['Drums', 'Loops', 'Bass', 'Keys', 'Garage House', 'Vocals', 'FX']


def library_info():
    menus = []
    for cat in MENU_ORDER:
        ps = [el('preset', dict(file=f + '.dspreset')) for c, f, t in PRESETS if c == cat]
        if ps:
            menus.append(el('menu', dict(name=cat), ps))
    root = el('DecentSamplerLibraryInfo', dict(name='UKG & Minimal Pack Instruments', version='1.3.0', coverArt='CoverArt.png'), el('presetMenu', None, menus))
    write_preset(f'{LIB}/DSLibraryInfo.xml', root)
    cover(f'{LIB}/CoverArt.png', 'UKG & MINIMAL', 'Decent Sampler instruments')


def main():
    if os.path.exists(LIB):
        for d in os.listdir(LIB):
            if d != 'Samples':
                p = os.path.join(LIB, d)
                shutil.rmtree(p) if os.path.isdir(p) else os.remove(p)
    for g in ['ukg', 'mth', 'sg', 'ch']:
        drum_kit(g)
    drum_kit('ukg', gh_kit=True)
    break_slicer()
    loops = loop_player()
    fx = fx_hits()
    sh = shouts()
    vc = vocal_chops()
    reese()
    sub_bass()
    acid()
    organ_bass()
    house_piano()
    garage_organ()
    rhodes()
    strings_preset()
    dub_chords()
    gh_pluck()
    gh_bass()
    gh_stab()
    gh_pad()
    stabs = house_stabs()
    mosaic = vocal_mosaic()
    hoover_preset()
    donk_preset()
    library_info()
    json.dump(dict(presets=PRESETS, sfz=SFZS, loops=loops, fx=fx, stabs=stabs, mosaic=mosaic), open(f'{W}/inst/build_report.json', 'w'), indent=1)
    print(len(PRESETS), 'presets;', len(SFZS), 'sfz files')


if __name__ == '__main__':
    main()
