"""Wave 3 sample expansion: drum, top, percussion and break loops; subs, synth basses and bass loops; garage keys,
dub chords, house brass/orchestra/choir/strings; airhorns, risers and textures. Renders each item, checks it,
writes the individual 16-bit WAVs (staging, for the delivery zip) and packs them into a few sprite WAVs that the
page slices per sample, plus out/manifest_wave3.json."""
import os as _os  # noqa: E402
PACK_ROOT = _os.environ.get('PACK_ROOT') or _os.path.abspath(_os.path.join(_os.path.dirname(_os.path.abspath(__file__)), '..'))  # the legacy web-pack folder
import json, os, re, sys, time, struct
import numpy as np
from scipy import signal
from dsp import *
import fx as F
import reese as R
import keys as Kz
import wave3 as W

WORK = PACK_ROOT
STAGE = f'{WORK}/out/wave3'
SPRITES = f'{WORK}/site/audio/s3'
K = json.load(open(f'{WORK}/out/kits.json'))
LAY = json.load(open(f'{WORK}/out/ext_layers.json'))
PCN = {'C': 0, 'C#': 1, 'Db': 1, 'D': 2, 'D#': 3, 'Eb': 3, 'E': 4, 'F': 5, 'F#': 6, 'Gb': 6, 'G': 7, 'G#': 8, 'Ab': 8, 'A': 9, 'A#': 10, 'Bb': 10, 'B': 11}
NAMES = ['C', 'C#', 'D', 'Eb', 'E', 'F', 'F#', 'G', 'Ab', 'A', 'Bb', 'B']
CAM_MIN = {0: '5A', 1: '12A', 2: '7A', 3: '2A', 4: '9A', 5: '4A', 6: '11A', 7: '6A', 8: '1A', 9: '8A', 10: '3A', 11: '10A'}
CAM_MAJ = {0: '8B', 1: '3B', 2: '10B', 3: '5B', 4: '12B', 5: '7B', 6: '2B', 7: '9B', 8: '4B', 9: '11B', 10: '6B', 11: '1B'}
GF = {'ukg': 'UK Garage', 'mth': 'Minimal Tech House', 'sg': 'Speed Garage', 'ch': 'Classic House', 'all': 'Classic House'}
GP = {'ukg': 'UKG', 'mth': 'MTH', 'sg': 'SG', 'ch': 'CH', 'all': 'ALL'}


def nn(name):
    m = re.match(r'^([A-G][#b]?)(-?\d)$', name)
    return 12 * (int(m.group(2)) + 1) + PCN[m.group(1)]


def nname(m):
    return NAMES[m % 12] + str(m // 12 - 1)


def key_of(label):   # 'D minor' -> (label, camelot, short)
    pc, mode = label.split()
    c = (CAM_MIN if mode == 'minor' else CAM_MAJ)[PCN[pc]]
    return label, c, pc.replace('#', 's') + ('min' if mode == 'minor' else 'maj')


def bar_s(bpm):
    return 4 * 60 / bpm


def kit_notes(kid, part, bars=4):
    bpm = K[kid]['bpm']
    sec = lambda t: t / 480 * 60 / bpm
    return [(sec(t), sec(d), n, v, bool(l), ch) for t, d, n, v, l, ch in K[kid]['parts'][part] if t < bars * 1920]


def layer_notes(kid, trk, bars=4):
    bpm = K[kid]['bpm']
    sec = lambda t: t / 480 * 60 / bpm
    return [(sec(t), sec(d), n, v, bool(l)) for t, d, n, v, l, _ in LAY[kid][trk] if t < bars * 1920]


def chord_notes(kid, ch, up=0, root_oct=None):
    ms = [nn(x) + up for x in K[kid]['chords'][ch]]
    if root_oct is not None:
        r = PCN[re.match(r'^([A-G][#b]?)', ch).group(1)]
        ms = [12 * (root_oct + 1) + r] + ms
    return ms


def place_notes(L, events, fn, tail=1.0, stereo=False):
    buf = np.zeros((L + n_samples(tail), 2)) if stereo else np.zeros(L + n_samples(tail))
    for ev in events:
        x = fn(*ev)
        if x is None:
            continue
        if stereo and x.ndim == 1:
            x = to_stereo(x)
        place(buf, x, n_samples(ev[0]))
    return buf


ITEMS = []


def item(sid, cat, grp, genre, title, fn, bpm=None, key=None, bars=None, note=None, pack=None, mono=False, sset=None, noteName=None, kind=None, keyname=None):
    ITEMS.append(dict(id=sid, cat=cat, grp=grp, genre=genre, title=title, fn=fn, bpm=bpm, key=key, bars=bars, note=note, pack=pack, mono=mono, set=sset, noteName=noteName, kind=kind, keyname=keyname))


def loop_len(bpm, bars):
    return n_samples(bars * bar_s(bpm))


# ======================================================================= drums
KIT_SOUND = {'ukg': 'ukg', 'mth': 'tech', 'sg': 'sg', 'ch': 'house'}
for kid, var in [('UKG01', 'main'), ('UKG05', 'var'), ('UKG07', 'main'), ('UKG08', 'var'), ('MTH01', 'main'), ('MTH04', 'var'), ('MTH07', 'main'), ('MTH08', 'main'),
                 ('SG07', 'main'), ('SG08', 'var'), ('CH07', 'main'), ('CH08', 'var')]:
    k = K[kid]
    g = k['genre']

    def fn(k=k, var=var, g=g):
        x = W.render_pattern(k['drums'][var], 2, k['bpm'], k['swing'], KIT_SOUND[g])
        return W.glue(x, -16, 2.5, 2.0)
    item(f'dl3_{kid.lower()}', 'loops', 'dl-' + g, g, f'Drum Loop · {k["name"]}', fn, k['bpm'], None, 2,
         f'{kid} {"main" if var == "main" else "variation"} groove, {round(k["swing"] * 100)}% swing',
         f'{GF[g]}/Drum Loops/{kid}_DrumLoop{"" if var == "main" else "-Var"}_{k["bpm"]}bpm', kind='drumloop')

TOPS = [
    ('top3_ukg_shuffle', 'ukg', 'Shuffle Top Loop', 134, 0.60, 'ukg', {'CH': ['5364536453645364', '536453645364536.'], 'OH': ['..............6.', '..............7.'], 'SH': ['3243324332433243'], 'TB': ['....5.......5...'], 'RIM': ['...4......4.....', '...4..3...4...3.']}, 'hats, shaker, tamb and rims, no kick', 'UKG_TopLoop_Shuffle'),
    ('top3_ukg_rimtamb', 'ukg', 'Rim & Tamb Top', 132, 0.58, 'ukg', {'RIM': ['...5..4...5..4..', '..5...4..5....4.'], 'TB': ['..5...5...5...5.'], 'SH': ['2.3.2.3.2.3.2.3.'], 'CH': ['4.4.4.4.4.4.4.4.']}, 'syncopated rims over tambourine', 'UKG_TopLoop_RimTamb'),
    ('top3_mth_skippy', 'mth', 'Skippy Top Loop', 128, 0.56, 'tech', {'CH': ['.5.4.5.4.5.4.5.4', '.5.4.5.4.5.4..54'], 'RIM': ['..4..3....4..3..', '..4..3...4..3.3.'], 'WB': ['.....3.......3..', '...3.....3......'], 'SH': ['4232.232.2324232']}, 'offbeat hats that skip, rims and clicks', 'MTH_TopLoop_Skippy'),
    ('top3_mth_ride', 'mth', 'Ride & Shaker Top', 128, 0.55, 'tech', {'RD': ['5.6.5.6.5.6.5.6.'], 'SH': ['4343434343434343'], 'OH': ['..4...4...4...4.'], 'CLV': ['3..3..3...3.....', '..3..3....3.3...']}, 'ride, 16th shaker, quiet open hats, claves', 'MTH_TopLoop_RideShaker'),
    ('top3_sg_top', 'sg', 'Speed Garage Top Loop', 134, 0.57, 'sg', {'CH': ['5464546454645464'], 'OH': ['..8...8...8...8.'], 'TB': ['....5.......5...'], 'SH': ['3343334333433343']}, 'shuffled 16ths with offbeat open hats', 'SG_TopLoop'),
    ('top3_ch_shaker', 'ch', 'Shaker & Tamb Top', 128, 0.52, 'house', {'SH': ['4545454545454545', '4545454545454554'], 'TB': ['..6...6...6...6.'], 'CH': ['3.3.3.3.3.3.3.3.']}, '16th shaker, offbeat tambourine', 'CH_TopLoop_ShakerTamb'),
    ('top3_ch_offbeat', 'ch', 'Offbeat Hat & Ride Top', 128, 0.53, 'house', {'OH': ['..8...8...8...8.'], 'RD': ['6.5.6.5.6.5.6.5.'], 'CH': ['4545454545454545']}, 'open hats on the offbeat, ride eighths', 'CH_TopLoop_OffbeatRide'),
]
for sid, g, title, bpm, sw, kit, pat, note, pk in TOPS:
    item(sid, 'loops', 'tops', g, title, lambda pat=pat, bpm=bpm, sw=sw, kit=kit: W.glue(W.render_pattern(pat, 2, bpm, sw, kit), -18, 2.0, 1.5),
         bpm, None, 2, note, f'{GF[g]}/Tops & Percussion/{pk}_{bpm}bpm', kind='top')

item('top3_sg_2step', 'loops', 'dl-sg', 'sg', 'Speed Garage 2-Step Loop', lambda: W.glue(W.render_pattern(
    {'K': ['9.........9.....', '9......6..9.....'], 'CL': ['....9.......9...'], 'SN': ['....6..3....6..4', '....6.......6.45'], 'CH': ['5464546454645464'], 'OH': ['..............7.'], 'RIM': ['...5......5..4..']},
    2, 134, 0.57, 'sg'), -16, 2.5, 2.0), 134, None, 2, 'the four-to-the-floor flipped to 2-step', 'Speed Garage/Drum Loops/SG_DrumLoop_2Step_134bpm', kind='drumloop')


def beads():   # polymetric percussion: cycles of 3, 5, 7, 6 and 9 sixteenths drifting against the bar
    ev = []
    for s in range(32):
        if s % 3 == 0: ev.append((s, 'bongo_hi', 0.42 if s % 9 == 0 else 0.3, 0.35, [0, -2, 3][(s // 3) % 3]))
        if s % 5 == 0: ev.append((s, 'clave', 0.26, -0.3, 0))
        if s % 7 == 3: ev.append((s, 'click', 0.3, -0.5, 2 * ((s // 7) % 2)))
        if s % 6 == 4: ev.append((s, 'bongo_lo', 0.34, 0.1, 0))
        if s % 9 == 5: ev.append((s, 'conga_hi', 0.32, -0.15, -3))
    return W.glue(W.render_perc(ev, 2, 128, 0.56), -20, 2.0, 1.0)


def bongos():
    ev = []
    for b in range(2):
        o = 16 * b
        for p in [2, 5, 7, 10, 13]: ev.append((o + p, 'bongo_hi', 0.4 if p in (2, 10) else 0.28, 0.35, [0, 2, 0, -1, 0][[2, 5, 7, 10, 13].index(p)]))
        for p in [0, 6, 11] if b == 0 else [0, 6, 9, 14]: ev.append((o + p, 'bongo_lo', 0.4, 0.1, 0))
        for p in [7, 15]: ev.append((o + p, 'conga_hi', 0.3, -0.25, -2))
        for p in [3, 8, 12]: ev.append((o + p, 'bongo_hi', 0.12, 0.4, 5))   # finger ghosts
    return W.glue(W.render_perc(ev, 2, 128, 0.56), -20, 2.0, 1.0)


def congas():
    ev = []
    for b in range(2):
        o = 16 * b
        for p in [0, 6, 10]: ev.append((o + p, 'conga_lo', 0.45, -0.2, 0))
        for p in [3, 6, 11, 13] if b == 0 else [3, 7, 11, 14]: ev.append((o + p, 'conga_hi', 0.4 if p in (3, 11) else 0.28, -0.35, 0))
        for p in [2, 6, 10, 14]: ev.append((o + p, 'bongo_hi', 0.26, 0.35, 0))
        for p in ([0, 6, 12] if b == 0 else [4, 8]): ev.append((o + p, 'clave', 0.3, 0.3, 0))   # 3-2 son clave
        for p in [4, 12]: ev.append((o + p, 'cowbell', 0.12, 0.25, 0))
    return W.glue(W.render_perc(ev, 2, 128, 0.52), -20, 2.0, 1.0)


item('perc3_mth_beads', 'loops', 'tops', 'mth', 'Polymetric Beads Perc', beads, 128, None, 2, 'bongo, clave and click cycles of 3, 5, 7, 6 and 9 steps', 'Minimal Tech House/Tops & Percussion/MTH_PercLoop_PolymetricBeads_128bpm', kind='perc')
item('perc3_mth_bongo', 'loops', 'tops', 'mth', 'Rominimal Bongo Loop', bongos, 128, None, 2, 'bongos with finger ghosts and pitch moves', 'Minimal Tech House/Tops & Percussion/MTH_PercLoop_RominimalBongos_128bpm', kind='perc')
item('perc3_ch_congas', 'loops', 'tops', 'ch', 'Conga & Bongo Loop', congas, 128, None, 2, 'house congas, bongos, a 3-2 son clave and cowbell', 'Classic House/Tops & Percussion/CH_PercLoop_CongaBongo_128bpm', kind='perc')

BREAKS = [
    ('brk3_garage', 'Garage Break 132', 132, 0.56, 2, {'K': ['9.........8.....', '9.....7...8.....'], 'S': ['....9.......9...', '....9.......9..6'], 'G': ['.......3.3....3.', '..3....3......3.'],
                                                   'H': ['5.4.5.4.5.4.5.4.', '5.4.5.4.5.4.5...'], 'O': ['..............6.', '..............7.']}, 'a roomy, squashed break for layering under 2-step', 'UKG_Break_Garage_132bpm', True),
    ('brk3_slice', 'Slice-Ready Break 132', 132, 0.5, 1, {'K': ['9.7.....8.7.....'], 'S': ['....9.......9...'], 'G': ['.....3.3......34'], 'H': ['54545454545454.5']},
     'one straight bar: cut it into 16 equal slices and it lines up with the Break Slices track in UKG07', 'UKG_Break_SliceReady-1bar_132bpm', False),
    ('brk3_jungle', 'Jungle Edit Break 136', 136, 0.52, 2, {'K': ['9.....7...8.....', '9.8.......8.....'], 'S': ['....9.......9...', '....9..7....9999'], 'G': ['.3.....3.3...3..', '.....3....3.....'],
                                                       'R': ['5.5.5.5.5.5.5.5.', '5.5.5.5.5.5.....'], 'H': ['..4...4...4...4.', '..4...4.........']}, 'ride-led break with a snare roll, for jungle-garage edits', 'UKG_Break_JungleEdit_136bpm', True),
]
for sid, title, bpm, sw, bars, lanes, note, pk, human in BREAKS:
    item(sid, 'loops', 'breaks', 'ukg', title, lambda lanes=lanes, bars=bars, bpm=bpm, sw=sw, human=human: W.render_break(lanes, bars, bpm, sw, 0.004 if human else 0.0),
         bpm, None, bars, note, f'UK Garage/Breaks/{pk}', kind='break')

# ======================================================================= basses
for m in range(24, 36):
    item(f'sub3_{nname(m).replace("#", "s")}', 'bass', 'sub', 'ukg', f'2-Step Sub {nname(m)}', lambda m=m: W.sub808(m), None, None, None, '808-style sine sub, 1.6 s tail',
         f'UK Garage/Bass/2-Step Sub/UKG_2StepSub_{nname(m)}', mono=True, sset='sub_2step', noteName=nname(m), keyname=NAMES[m % 12])

for m in [29, 33, 36, 38]:
    item(f'rub3_{nname(m).replace("#", "s")}', 'bass', 'synthbass', 'mth', f'Rubber Bass {nname(m)}', lambda m=m: W.rubber(m), None, None, None, 'round minimal bass with a filter snap',
         f'Minimal Tech House/Bass/One-Shots/MTH_RubberBass_{nname(m)}', mono=True, keyname=NAMES[m % 12])
for m in [48, 53, 55]:
    item(f'donk3_{nname(m).replace("#", "s")}', 'bass', 'synthbass', 'sg', f'Donk {nname(m)}', lambda m=m: W.donk(m), None, None, None, 'bassline donk, FM blip into pitch',
         f'Speed Garage/Bass/Donks/SG_Donk_{nname(m)}', mono=True, keyname=NAMES[m % 12])
for m in [54, 48]:
    item(f'hoov3_{nname(m).replace("#", "s")}', 'bass', 'synthbass', 'sg', f'Hoover Stab {nname(m)}', lambda m=m: W.hoover(m), None, None, None, 'PWM saws with a swoop up into pitch',
         f'Speed Garage/Synths/SG_HooverStab_{nname(m)}', keyname=NAMES[m % 12])


def fold_sub(m):
    s = m
    while s > 37: s -= 12
    while s < 26: s += 12
    return s


def bl_ukg07():
    kid = 'UKG07'; bpm = K[kid]['bpm']; L = loop_len(bpm, 4)
    ns = [(a, b, c, d, e) for a, b, c, d, e, _ in kit_notes(kid, 'bassA')]
    reese = R.render(ns, 4 * bar_s(bpm), 'classic', bpm)
    sub = place_notes(L, [(a, b, fold_sub(c), d) for a, b, c, d, e, _ in kit_notes(kid, 'bassA')], lambda a, b, c, d: W.sub_note(c, b, b * 0.95, 0, d / 127) * 0.55)
    x = trim_to(reese, L + n_samples(1.0)) * 0.8 + to_stereo(bfilt(sub, 'highpass', 22, 2))
    return W.wrap_loop(x, L)


def bl_ukg08():
    kid = 'UKG08'; bpm = K[kid]['bpm']; L = loop_len(bpm, 4)
    ev = [(a, b, c, d) for a, b, c, d, e, _ in kit_notes(kid, 'bassA')]
    return W.wrap_loop(bfilt(place_notes(L, ev, lambda a, b, c, d: W.sub_note(c, b, b * 0.97, 4.0 if b > 0.3 else 0.0, d / 127)), 'highpass', 22, 2), L)


def zoom(x, bpm, bars):
    n = len(x); t = np.arange(n) / SR
    ph = (t / (bars * bar_s(bpm))) % 1.0
    shape = np.where(ph < 0.5, (ph / 0.5) ** 1.5, (1 - (ph - 0.5) / 0.5) ** 0.7)   # zoom open over two bars, retract over two
    return svf(x, 170 * 2 ** (4.2 * shape), 1.9, 'lp')


def bl_mth07():
    kid = 'MTH07'; bpm = K[kid]['bpm']; L = loop_len(bpm, 4)
    ev = [(a, b, c, d) for a, b, c, d, e, _ in kit_notes(kid, 'bassA')]
    x = place_notes(L, ev, lambda a, b, c, d: W.rubber(c, b + 0.25, b, d / 127))
    return W.wrap_loop(zoom(x, bpm, 4) * 1.3, L)


def bl_mth08():
    kid = 'MTH08'; bpm = K[kid]['bpm']; L = loop_len(bpm, 4)
    ev = [(a, b, c, d) for a, b, c, d, e, _ in kit_notes(kid, 'bassA')]
    return W.wrap_loop(place_notes(L, ev, lambda a, b, c, d: W.rubber(c, b + 0.2, b, d / 127, bounce=2.0)), L)


def bl_sg07():
    kid = 'SG07'; bpm = K[kid]['bpm']; L = loop_len(bpm, 4)
    ns = [(a, b, c, d, e) for a, b, c, d, e, _ in kit_notes(kid, 'bassA')]
    x = R.render(ns, 4 * bar_s(bpm), 'classic', bpm, dive=(3 * bar_s(bpm) + 3 * 60 / bpm, -12, 0.42))
    return W.wrap_loop(trim_to(x, L + n_samples(1.0)), L)


def wub(x, bpm, bars):
    n = len(x); t = np.arange(n) / SR
    beat = 60 / bpm
    rate = np.where((t // (2 * bar_s(bpm))) % 2 == 0, 2 / beat, 3 / beat)   # 1/8 wobble for two bars, 1/8 triplets for two
    ph = np.cumsum(rate / SR)
    lfo = 0.5 - 0.5 * np.cos(2 * np.pi * ph)
    return svf(x, 260 * 2 ** (3.3 * lfo), 2.2, 'lp')


def bl_sg08():
    kid = 'SG08'; bpm = K[kid]['bpm']; L = loop_len(bpm, 4)
    ev = [(a, b, c, d) for a, b, c, d, e, _ in kit_notes(kid, 'bassA')]
    x = place_notes(L, ev, lambda a, b, c, d: Kz.organ_bass_note(c, b + 0.02, b * 0.92, d))
    return W.wrap_loop(wub(x, bpm, 4) * 1.4, L)


def bl_ch07():
    kid = 'CH07'; bpm = K[kid]['bpm']; L = loop_len(bpm, 4)
    ev = [(a, b, c, d) for a, b, c, d, e, _ in kit_notes(kid, 'bassA')]
    x = place_notes(L, ev, lambda a, b, c, d: F.pluck_bass(c, b + 0.03, b * 0.85) * (0.6 + 0.4 * d / 127))
    return W.wrap_loop(board(x, [pb.Compressor(threshold_db=-14, ratio=3, attack_ms=5, release_ms=90)]), L)


def bl_ch08():
    kid = 'CH08'; bpm = K[kid]['bpm']; L = loop_len(bpm, 4)
    ev = [(a, b, c, d) for a, b, c, d, e, _ in kit_notes(kid, 'bassA')]
    return W.wrap_loop(place_notes(L, ev, lambda a, b, c, d: Kz.organ_bass_note(c, b + 0.02, b * 0.92, d)), L)


def steps_from(events, bpm, bars):
    st = [None] * (16 * bars)
    for a, b, c, d, e in events:
        i = int(round(a / step_seconds(bpm)))
        if 0 <= i < len(st):
            st[i] = (c, d >= 110, e)
    return st


def acid_mth08():
    kid = 'MTH08'; bpm = K[kid]['bpm']; L = loop_len(bpm, 4)
    x = W.acid303(steps_from(layer_notes(kid, 'acid'), bpm, 4), bpm, K[kid]['swing'], cut=420, reso=7.5)
    return W.wrap_loop(x, L)


def acid_pattern(text, root):
    """'C2* . C3~ Eb2 ...' 16ths: note, * accent, ~ slide into the next, '.' rest."""
    out = []
    for tok in text.split():
        if tok == '.':
            out.append(None); continue
        acc, slide = '*' in tok, '~' in tok
        out.append((nn(tok.strip('*~')), acc, slide))
    return out


ACID_F = acid_pattern('F2* . F3~ Ab2 . F2 C3* . F2 . Eb3~ F3 . Ab2* F2 . '
                      'F2* . F3 . Ab2~ Bb2 . F2 C3* . F2~ Ab2 . Eb3* F3 .', 41)
ACID_A = acid_pattern('A1* A1 A2~ C3 . A1 E2* . A1 G2~ A2 . C3* . A1 . '
                      'A1* . A2 A1 C3~ D3 . A1 E2* . A1 . G2~ A2 E2* .', 33)
item('acid3_mth08', 'bass', 'bassloops', 'mth', 'Acid Loop · Black Card Bounce', acid_mth08, 128, 'C minor', 4, '303 line from the drop of MTH08, dry', 'Minimal Tech House/Bass/Acid/MTH08_AcidLoop_Cmin-5A_128bpm', mono=True, kind='acid')
item('acid3_f130', 'bass', 'bassloops', 'mth', 'Acid Squelch 130', lambda: W.wrap_loop(W.acid303(ACID_F, 130, 0.54, cut=360, reso=8.0, decay=0.2), loop_len(130, 2)), 130, 'F minor', 2,
     'resonant saw 303, accents and slides, dry', 'Minimal Tech House/Bass/Acid/MTH_AcidLoop_Squelch_Fmin-4A_130bpm', mono=True, kind='acid')
item('acid3_ch_a128', 'bass', 'bassloops', 'ch', 'Acid House Loop 128', lambda: W.wrap_loop(W.acid303(ACID_A, 128, 0.52, wave='square', cut=300, reso=6.5, decay=0.28, dist=3.2), loop_len(128, 2)), 128, 'A minor', 2,
     'square-wave 303 in the Chicago style, dry', 'Classic House/Bass/Acid/CH_AcidHouseLoop_Amin-8A_128bpm', mono=True, kind='acid')

BL = [('bl3_ukg07', 'ukg', 'Reese + Sub Loop · Halftime Pressure', bl_ukg07, 'UKG07', 'Reese on the offbeats with a sine sub doubling it', 'UK Garage/Bass/Loops/UKG07_ReeseSubLoop', False),
      ('bl3_ukg08', 'ukg', 'Bubbling Sub Loop · Sunday Wub', bl_ukg08, 'UKG08', 'sine sub that dips in pitch at the end of held notes', 'UK Garage/Bass/Loops/UKG08_BubblingSubLoop', True),
      ('bl3_mth07', 'mth', 'Zoom Bass Loop · Pattering Beads', bl_mth07, 'MTH07', 'rubber bass, filter zooms open over two bars and retracts', 'Minimal Tech House/Bass/Loops/MTH07_ZoomBassLoop', True),
      ('bl3_mth08', 'mth', 'Bounce Bass Loop · Black Card Bounce', bl_mth08, 'MTH08', 'every note drops two semitones into pitch', 'Minimal Tech House/Bass/Loops/MTH08_BounceBassLoop', True),
      ('bl3_sg07', 'sg', 'Reese Dive Loop · Siren Pressure', bl_sg07, 'SG07', 'Reese bassline with an octave dive on the last beat', 'Speed Garage/Bass/Loops/SG07_ReeseDiveLoop', False),
      ('bl3_sg08', 'sg', 'Organ Wub Loop · Organ Wub', bl_sg08, 'SG08', 'M1 organ bass through a 1/8 then triplet filter wobble', 'Speed Garage/Bass/Loops/SG08_OrganWubLoop', True),
      ('bl3_ch07', 'ch', 'Disco Octave Bass · Disco Diamonds', bl_ch07, 'CH07', 'plucked octave disco bass', 'Classic House/Bass/Loops/CH07_DiscoOctaveBassLoop', True),
      ('bl3_ch08', 'ch', 'Organ Bass Loop · Hands Up Higher', bl_ch08, 'CH08', 'M1 organ bass from CH08', 'Classic House/Bass/Loops/CH08_OrganBassLoop', True)]
for sid, g, title, fn, kid, note, pk, mono in BL:
    lab, cam, short = key_of(K[kid]['key']['label'])
    item(sid, 'bass', 'bassloops', g, title, fn, K[kid]['bpm'], lab, 4, note, f'{pk}_{short}-{cam}_{K[kid]["bpm"]}bpm', mono=mono, kind='bassloop')


def donk_loop():
    kid = 'SG08'; bpm = K[kid]['bpm']; L = loop_len(bpm, 2)
    return W.wrap_loop(place_notes(L, [(a, b, c, d) for a, b, c, d, e in layer_notes(kid, 'donk', 2)], lambda a, b, c, d: W.donk(c) * (0.6 + 0.4 * d / 127)), L)


item('donk3_loop_sg08', 'bass', 'bassloops', 'sg', 'Donk Loop · Organ Wub', donk_loop, 136, 'C minor', 2, 'offbeat donks on the root, the octave on the last one',
     'Speed Garage/Bass/Donks/SG08_DonkLoop_Cmin-5A_136bpm', mono=True, kind='donk')

# ======================================================================= keys and stabs
for kid, ch in [('UKG07', 'Dm9'), ('UKG07', 'Bbmaj7'), ('UKG08', 'Ebmaj9'), ('UKG08', 'Cm9')]:
    lab, cam, short = key_of(K[kid]['key']['label'])
    item(f'org3_{ch.lower()}', 'keys', 'garagekeys', 'ukg', f'Garage Organ Stab {ch}', lambda kid=kid, ch=ch: W.organ_stab(chord_notes(kid, ch, 12, 3)), None, lab, None,
         f'M1-style organ stab, {ch} from {K[kid]["name"]}', f'UK Garage/Keys/Organ Stabs/UKG_OrganStab_{ch}_{short}-{cam}', kind='organ')
for ch, ro in [('Ebmaj9', 2), ('Cm9', 2), ('Fm11', 2)]:
    lab, cam, short = key_of(K['UKG08']['key']['label'])
    item(f'rho3_{ch.lower()}', 'keys', 'garagekeys', 'ukg', f'Rhodes Chord {ch}', lambda ch=ch, ro=ro: W.rhodes_post(sum(W.rhodes_note(m, 1.9, 92 if m > 50 else 84) for m in chord_notes('UKG08', ch, 0, ro)) / 2.6, 133),
         None, lab, None, f'FM electric piano with tremolo, {ch}', f'UK Garage/Keys/Rhodes/UKG_Rhodes_{ch}_{short}-{cam}', kind='rhodes')


def rhodes_loop():
    kid = 'UKG08'; bpm = K[kid]['bpm']; L = loop_len(bpm, 4)
    groups = {}
    for a, b, c, d, e, ch in kit_notes(kid, 'chordsA'):
        groups.setdefault((round(a, 4), round(b, 4), ch, d), []).append(c)
    buf = np.zeros(L + n_samples(2.0))
    for (a, b, ch, d), ms in groups.items():
        r = PCN[re.match(r'^([A-G][#b]?)', ch).group(1)]
        for m in ms + [36 + r if 36 + r < 46 else 24 + r]:
            place(buf, W.rhodes_note(m, b, d), n_samples(a), 1 / 2.6)
    return W.wrap_loop(W.rhodes_post(buf, bpm), L)


def bell_loop():
    kid = 'UKG08'; bpm = K[kid]['bpm']; L = loop_len(bpm, 4)
    buf = place_notes(L, [(a, b, c, d) for a, b, c, d, e, _ in kit_notes(kid, 'leadA')], lambda a, b, c, d: W.fm_bell(c, 1.2, d) * 0.5, tail=2.0)
    y = pingpong(buf, 3 * step_seconds(bpm), 0.42, 0.32, 3800, 8)
    return W.wrap_loop(reverb(y, 0.5, 0.16, 0.5, 1.0, 1.0), L)


lab, cam, short = key_of('Eb major')
item('rho3_loop_ukg08', 'keys', 'garagekeys', 'ukg', 'Rhodes Loop · Sunday Wub', rhodes_loop, 133, lab, 4, 'UKG08 chords A on Rhodes, tremolo in 8ths', f'UK Garage/Keys/Loops/UKG08_RhodesLoop_{short}-{cam}_133bpm', kind='rhodes')
item('bell3_loop_ukg08', 'keys', 'garagekeys', 'ukg', 'Bell Loop · Sunday Wub', bell_loop, 133, lab, 4, 'UKG08 melody A on FM bells with ping-pong echo', f'UK Garage/Keys/Loops/UKG08_BellLoop_{short}-{cam}_133bpm', kind='bell')

for kid, ch, ro in [('MTH07', 'Am11', 2), ('MTH07', 'Fmaj9', 2), ('MTH08', 'Cm9', 3), ('MTH08', 'Abmaj9', 2)]:
    lab, cam, short = key_of(K[kid]['key']['label'])
    item(f'dub3_{ch.lower()}', 'keys', 'dubchords', 'mth', f'Dub Chord {ch}', lambda kid=kid, ch=ch, ro=ro: W.dub_chord(chord_notes(kid, ch, 0, ro), 128), 128, lab, None,
         f'filtered stab into a 3/16 ping-pong delay and a big room, {ch}', f'Minimal Tech House/Keys/Dub Chords/MTH_DubChord_{ch}_{short}-{cam}_128bpm', kind='dubchord')


def bleep_loop():
    kid = 'MTH07'; bpm = K[kid]['bpm']; L = loop_len(bpm, 2)
    buf = place_notes(L, [(a, b, c, d) for a, b, c, d, e, _ in kit_notes(kid, 'leadA', 2)], lambda a, b, c, d: W.bleep(c, 0.16, d) * 0.5, tail=1.0)
    y = pingpong(buf, 3 * step_seconds(bpm), 0.45, 0.4, 3000, 8)
    return W.wrap_loop(reverb(y, 0.45, 0.14, 0.5, 1.0, 1.0), L)


lab, cam, short = key_of('A minor')
item('bleep3_loop_mth07', 'keys', 'dubchords', 'mth', 'Bleep Loop · Pattering Beads', bleep_loop, 128, lab, 2, 'MTH07 melody A on sine bleeps with dub echo', f'Minimal Tech House/Keys/Loops/MTH07_BleepLoop_{short}-{cam}_128bpm', kind='bleep')

for ch, up, fall, title in [('Bbm11', 0, 0, 'Brass Stab Bbm11'), ('Gbmaj9', 12, 0, 'Brass Stab Gbmaj9'), ('Ab6', 0, 0, 'Brass Stab Ab6'), ('Ebm9', 12, 5, 'Brass Fall Ebm9')]:
    lab, cam, short = key_of('Bb minor')
    item(f'brass3_{ch.lower()}' + ('_fall' if fall else ''), 'keys', 'housestabs', 'ch', title, lambda ch=ch, up=up, fall=fall: W.brass(chord_notes('CH07', ch, up), 1.25 if fall else 0.9, 0.5 if fall else 0.22, fall), None, lab, None,
         ('falls five semitones off the end, ' if fall else 'disco section stab, ') + ch, f'Classic House/Brass/CH_Brass{"Fall" if fall else "Stab"}_{ch}_{short}-{cam}', kind='brass')
for name, ms, root in [('Am', [57, 60, 64], 45), ('F', [53, 57, 60], 41), ('G', [55, 59, 62], 43)]:
    lab, cam, short = key_of('A minor')
    item(f'orch3_{name.lower()}', 'keys', 'housestabs', 'ch', f'Orchestra Hit {name}', lambda ms=ms, root=root: W.orch_hit(ms, root), None, lab, None,
         f'stacked orchestral slam on {name} with a timpani thump', f'Classic House/Orchestra Hits/CH_OrchHit_{name}_{short}-{cam}', kind='orch')
for ch, dur, gate, title, vowel in [('Fmaj9', 2.4, 1.9, 'Choir Aah Fmaj9', 'a'), ('Am9', 2.4, 1.9, 'Choir Aah Am9', 'a'), ('Dm9', 1.3, 0.25, 'Choir Stab Dm9', 'o')]:
    lab, cam, short = key_of('A minor')
    item(f'choir3_{ch.lower()}' + ('_stab' if dur < 2 else ''), 'keys', 'housestabs', 'ch', title, lambda ch=ch, dur=dur, gate=gate, vowel=vowel: W.choir(chord_notes('CH08', ch, 0, 3), dur, gate, vowel, 0.18 if dur > 2 else 0.02),
         None, lab, None, ('gospel choir, ' if dur > 2 else "short 'oh' stab, ") + ch, f'Classic House/Choir/CH_Choir{"Aah" if dur > 2 else "Stab"}_{ch}_{short}-{cam}', kind='choir')


def string_run(scale, chord, bpm):
    st = step_seconds(bpm) / 2   # 32nds
    n = n_samples(2.4)
    buf = np.zeros((n, 2))
    for i, m in enumerate(scale):
        place(buf, W.string_voice([m], st * 1.6, st * 1.1, 0.006, 5200) * (0.55 + 0.06 * i), n_samples(i * st))
    t0 = len(scale) * st
    place(buf, W.string_voice(chord, 1.3, 1.1, 0.03, 4800), n_samples(t0))
    y = board(buf, [pb.HighpassFilter(140), pb.Chorus(rate_hz=0.6, depth=0.25, mix=0.35, centre_delay_ms=9)])
    return reverb(y, 0.65, 0.24, 0.5, 1.0, 1.0)[:n]


lab, cam, short = key_of('Bb minor')
item('run3_bbm', 'keys', 'strings3', 'ch', 'Disco String Run Bb minor', lambda: string_run([nn(x) for x in ['F4', 'Gb4', 'Ab4', 'Bb4', 'C5', 'Db5', 'Eb5']], [nn(x) for x in ['Db5', 'F5', 'Ab5', 'Eb5']], 128), 128, lab, None,
     'a 32nd-note scale run into a held Bbm11', f'Classic House/Strings/CH_StringRun_{short}-{cam}_128bpm', kind='strings')
lab, cam, short = key_of('A minor')
item('run3_am', 'keys', 'strings3', 'ch', 'Disco String Run A minor', lambda: string_run([nn(x) for x in ['E4', 'F4', 'G4', 'A4', 'B4', 'C5', 'D5']], [nn(x) for x in ['C5', 'E5', 'G5', 'B5']], 128), 128, lab, None,
     'a 32nd-note scale run into a held Am9', f'Classic House/Strings/CH_StringRun_{short}-{cam}_128bpm', kind='strings')
for kid, ch, ro in [('SG07', 'F#m11', 2), ('SG08', 'Cm7', 3)]:
    lab, cam, short = key_of(K[kid]['key']['label'])
    item(f'sstab3_{ch.lower().replace("#", "s")}', 'keys', 'strings3', 'sg', f'Dark String Stab {ch}',
         lambda kid=kid, ch=ch, ro=ro: reverb(board(W.string_voice(chord_notes(kid, ch, 0, ro), 1.0, 0.32, 0.008, 2600, decay=0.22), [pb.HighpassFilter(90)]), 0.7, 0.26, 0.5, 1.0, 1.0)[:n_samples(1.4)],
         None, lab, None, f'short, low string stab with a long room, {ch}', f'Speed Garage/Strings/SG_DarkStringStab_{ch.replace("#", "s")}_{short}-{cam}', kind='strings')

# ======================================================================= FX
item('fx3_airhorn', 'fx', 'fx3', 'ukg', 'Airhorn', lambda: W.airhorn(415.3), None, None, None, 'two horns beating, three blasts (Ab)', 'UK Garage/FX/UKG_FX_Airhorn_Ab', kind='airhorn', keyname='Ab')
item('fx3_airhorn_low', 'fx', 'fx3', 'sg', 'Airhorn Low', lambda: W.airhorn(311.1, ((0.0, 0.16), (0.24, 0.16), (0.48, 0.9)), 2.1), None, None, None, 'pitched down, slower blasts (Eb)', 'Speed Garage/FX/SG_FX_AirhornLow_Eb', kind='airhorn', keyname='Eb')


def dub_airhorn():
    bpm = 134
    x = W.airhorn(370.0, ((0.0, 0.5),), 0.7)
    y = pingpong(x, 3 * step_seconds(bpm), 0.62, 0.85, 2800, 10)
    y = reverb(y, 0.6, 0.2, 0.5, 1.0, 1.0)
    return fade(trim_to(y, loop_len(bpm, 2)), 0.002, 0.3)


item('fx3_dubhorn', 'fx', 'fx3', 'sg', 'Dub Airhorn', dub_airhorn, 134, None, 2, 'one blast into 3/16 echoes that darken (F#)', 'Speed Garage/FX/SG_FX_DubAirhorn_Fs_134bpm', kind='airhorn', keyname='F#')
item('fx3_bomb', 'fx', 'fx3', 'sg', 'Bomb Drop', lambda: W.bomb_drop(134), 134, None, 2, 'a bar and a half of falling whistle, the hit lands on bar 2', 'Speed Garage/FX/SG_FX_BombDrop-2bars_134bpm', kind='bomb')


def hoover_riser():
    bpm = 134; n = loop_len(bpm, 4); t = np.arange(n) / SR
    rate = (bpm / 60) * (1 * 2 ** (3 * (t / t[-1]) ** 1.4))   # tremolo from quarters to 32nds
    trem = 0.55 + 0.45 * np.cos(2 * np.pi * np.cumsum(rate / SR))
    x = W.hoover(54, n / SR, n / SR, 0.0, (12, 1.6), trem * np.minimum(1, 0.15 + t / t[-1]))
    x = trim_to(x, n) + F.noise_riser(4, bpm) * 0.2
    return fade(x, 0.01, 0.004)


def acid_riser():
    bpm = 128
    a = W.acid303([(36, i % 4 == 0, False) for i in range(48)], bpm, 0.5, cut=200, reso=9.0, decay=0.15, cutmul=np.linspace(1, 9, 48) ** 1.1)
    b = W.acid303([(36 + (12 if i % 8 >= 4 else 0), True, False) for i in range(32)], bpm * 2, 0.5, cut=1600, reso=9.5, decay=0.1, cutmul=np.linspace(1, 3, 32))
    L3, L1 = loop_len(bpm, 3), loop_len(bpm, 1)
    x = np.concatenate([trim_to(a, L3), trim_to(b, L1)])
    st = to_stereo(x * np.linspace(0.6, 1.0, len(x)))
    st = st + F.noise_riser(4, bpm) * 0.15
    return fade(st, 0.005, 0.004)


def string_riser():
    bpm = 128; n = loop_len(bpm, 4)
    ch = [nn(x) for x in ['Bb3', 'Db4', 'F4', 'Ab4', 'Db5', 'F5']]
    s = W.string_voice(ch, n / SR, n / SR, n / SR * 0.95, 4500)[:n]
    s = s * np.linspace(0.05, 1, n)[:, None] ** 1.2
    s = reverb(s, 0.7, 0.25, 0.5, 1.0, 1.0)[:n] + F.noise_riser(4, bpm) * 0.3
    return fade(s, 0.01, 0.004)


item('fx3_hooverriser', 'fx', 'fx3', 'sg', 'Hoover Riser', hoover_riser, 134, 'F# minor', 4, 'hoover climbing an octave, tremolo speeding to 32nds', 'Speed Garage/FX/SG_FX_HooverRiser-4bars_Fsmin-11A_134bpm', kind='riser')
item('fx3_acidriser', 'fx', 'fx3', 'mth', 'Acid Riser', acid_riser, 128, 'C minor', 4, '303 on C with the filter opening, 32nds in the last bar', 'Minimal Tech House/FX/MTH_FX_AcidRiser-4bars_Cmin-5A_128bpm', kind='riser')
item('fx3_stringriser', 'fx', 'fx3', 'ch', 'String Swell Riser', string_riser, 128, 'Bb minor', 4, 'a four-bar string crescendo with noise', 'Classic House/FX/CH_FX_StringSwellRiser-4bars_Bbmin-3A_128bpm', kind='riser')


def click_texture():
    bpm = 128; L = loop_len(bpm, 2); n = L + n_samples(0.5)
    x = np.zeros((n, 2))
    for i in range(int(2 * 4 * 12)):
        p = W.RNG3.integers(0, L)
        h = W.perc_hit(['click', 'clave', 'rim_tech'][i % 3], W.RNG3.uniform(-3, 7)) * W.RNG3.uniform(0.05, 0.22)
        place(x, to_stereo(h, W.RNG3.uniform(-0.9, 0.9)), p)
    st = step_seconds(bpm)
    for b in range(2):
        t0 = n_samples((b * 16 + 14) * st)
        for k in range(8):
            place(x, to_stereo(W.perc_hit('rim_tech') * (0.08 + 0.03 * k), 0.4 * (-1) ** k), t0 + n_samples(k * st / 4))
    x = x + to_stereo(F.crackle(n, 30, 0.05))
    return W.wrap_loop(reverb(x, 0.3, 0.12, 0.5, 1.0, 1.0), L)


item('fx3_clicks', 'fx', 'fx3', 'mth', 'Micro Click Texture', click_texture, 128, None, 2, 'scattered clicks and pops with a ratchet at each bar end', 'Minimal Tech House/FX/MTH_FX_MicroClickTexture_128bpm', kind='texture')
item('fx3_crackle', 'fx', 'fx3', 'all', 'Vinyl Crackle Bed', lambda: W.crackle_bed(128, 2), 128, None, 2, 'crackle, pops, hiss and rumble; loops cleanly', 'Textures/ALL_VinylCrackleBed_128bpm', kind='texture')
item('fx3_whoosh_up', 'fx', 'fx3', 'all', 'Noise Whoosh Up', lambda: W.whoosh(128, 1, True), 128, None, 1, 'one-bar filtered noise sweep up, panning', 'Textures/ALL_NoiseWhooshUp-1bar_128bpm', kind='whoosh')
item('fx3_whoosh_down', 'fx', 'fx3', 'all', 'Noise Whoosh Down', lambda: W.whoosh(128, 1, False), 128, None, 1, 'one-bar filtered noise sweep down, panning', 'Textures/ALL_NoiseWhooshDown-1bar_128bpm', kind='whoosh')


# ======================================================================= render, check, stage, sprite
def sprite_for(it, mono):
    """Ten sprite files: stereo loops and stereo sounds per genre, and two mono bass sprites."""
    if mono:
        return 'w3_bass_loops' if it['bars'] else 'w3_bass_shots'
    g = it['genre'] if it['genre'] != 'all' else 'ch'
    return f'w3_{g}_loops' if it['cat'] == 'loops' else f'w3_{g}_sounds'


def to_int16(x):
    x = np.clip(x, -1, 1)
    return np.round(x * 32767).astype('<i2')


def wav_bytes(pcm, ch):
    data = pcm.tobytes()
    hdr = b'RIFF' + struct.pack('<I', 36 + len(data)) + b'WAVE' + b'fmt ' + struct.pack('<IHHIIHH', 16, 1, ch, SR, SR * 2 * ch, 2 * ch, 16) + b'data' + struct.pack('<I', len(data))
    return hdr + data


def f0_estimate(x, lo=25, hi=1200):
    """Autocorrelation pitch of 0.4 s after the attack: the first strong peak (avoids octave errors), refined."""
    m = x if x.ndim == 1 else x.mean(axis=1)
    a = n_samples(0.12)
    seg = m[a:a + n_samples(0.4)]
    seg = seg - seg.mean()
    if len(seg) < 4096 or np.max(np.abs(seg)) < 1e-4:
        return None
    ac = signal.fftconvolve(seg, seg[::-1], 'full')[len(seg) - 1:]
    ac = ac / (ac[0] + 1e-12)
    lmin, lmax = int(SR / hi), min(len(ac) - 2, int(SR / lo))
    reg = ac[lmin:lmax]
    pk = int(np.argmax(reg))
    thr = 0.88 * reg[pk]
    for i in range(1, len(reg) - 1):
        if reg[i] >= thr and reg[i] >= reg[i - 1] and reg[i] >= reg[i + 1]:
            pk = i
            break
    lag = lmin + pk
    y0, y1, y2 = ac[lag - 1], ac[lag], ac[lag + 1]
    d = y0 - 2 * y1 + y2
    off = 0.5 * (y0 - y2) / d if abs(d) > 1e-12 else 0.0
    return SR / (lag + float(np.clip(off, -0.5, 0.5)))


def main(only=None):
    t0 = time.time()
    os.makedirs(SPRITES, exist_ok=True)
    rendered, problems = [], []
    for it in ITEMS:
        if only and not re.search(only, it['id']):
            continue
        x = it['fn']()
        x = np.nan_to_num(np.asarray(x, dtype=float))
        if it['mono'] and x.ndim == 2:
            x = x.mean(axis=1)
        if not it['mono'] and x.ndim == 1:
            x = to_stereo(x)
        if it['bars']:   # loops are exactly their bar length
            x = trim_to(x, loop_len(it['bpm'], it['bars']))
        x = x - (x.mean(axis=0) if not it['bars'] else 0)
        x = normalize(x, -1.0)
        if not it['bars']:
            x = fade(x, 0.0005, 0.02)
        pcm = to_int16(x)
        ch = 1 if pcm.ndim == 1 else 2
        mm = x if x.ndim == 1 else x.mean(axis=1)
        rms = 20 * np.log10(np.sqrt(np.mean(mm ** 2)) + 1e-12)
        dc = float(np.mean(mm))
        info = f'{it["id"]:22s} {len(pcm) / SR:5.2f}s {"mono  " if ch == 1 else "stereo"} rms {rms:6.1f} dB dc {dc:+.4f}'
        if it['bars']:
            exp = loop_len(it['bpm'], it['bars'])
            if len(pcm) != exp: problems.append(it['id'] + ' loop length')
            edge = float(np.max(np.abs(mm[:32]))), float(np.max(np.abs(mm[-32:])))
            info += f' | {it["bars"]} bars @ {it["bpm"]}'
        if it['keyname'] and it['kind'] not in ('airhorn',):
            f0 = f0_estimate(x)
            target = it['noteName'] or it['title'].split()[-1]
            if f0 and re.match(r'^[A-G][#b]?-?\d$', target):
                cents = 1200 * np.log2(f0 / W.m2f(nn(target)))
                cents = (cents + 600) % 1200 - 600   # octave-agnostic
                info += f' | f0 {f0:7.2f} Hz ({cents:+5.0f} c vs {target})'
                if abs(cents) > 25: problems.append(f'{it["id"]} tuning {cents:+.0f} c')
        if abs(dc) > 0.01: problems.append(it['id'] + f' DC {dc:+.3f}')
        if rms < -40: problems.append(it['id'] + f' quiet {rms:.1f}')
        print(info, flush=True)
        os.makedirs(f'{STAGE}/{os.path.dirname(it["pack"])}', exist_ok=True)
        open(f'{STAGE}/{it["pack"]}.wav', 'wb').write(wav_bytes(pcm, ch))
        rendered.append((it, pcm, ch, x))
    if only:   # partial runs only stage the WAVs; the sprites and manifest come from full runs
        print('staged', len(rendered), 'items;', 'PROBLEMS' if problems else 'no problems', problems)
        return
    # sprites
    groups = {}
    for it, pcm, ch, x in rendered:
        groups.setdefault(sprite_for(it, ch == 1), []).append((it, pcm, ch, x))
    GAP = 512
    manifest = []
    for name, mem in groups.items():
        ch = mem[0][2]
        chunks, off = [], 0
        for it, pcm, c, x in mem:
            assert c == ch
            chunks.append(pcm)
            chunks.append(np.zeros((GAP,) + pcm.shape[1:], dtype='<i2'))
            e = {'id': it['id'], 'cat': it['cat'], 'grp': it['grp'], 'title': it['title'], 'bpm': it['bpm'], 'note': it['note'], 'bars': it['bars'],
                 'dur': round(len(pcm) / SR, 3), 'file': f'sprite:audio/s3/{name}.wav?o={off}&n={len(pcm)}&c={ch}', 'pack': it['pack'] + '.wav',
                 'peaks': peaks(x, 72), 'stereo': ch == 2, 'genre': it['genre'], 'kind': it['kind'], 'wave': 3}
            if it['key']:
                lab, cam, _ = key_of(it['key'])
                e['key'] = lab; e['camelot'] = cam
            elif it['keyname']:
                e['key'] = it['keyname']
            if it['set']:
                e['set'] = it['set']; e['noteName'] = it['noteName']
            manifest.append({k: v for k, v in e.items() if v is not None})
            off += len(pcm) + GAP
        allpcm = np.concatenate(chunks)
        open(f'{SPRITES}/{name}.wav', 'wb').write(wav_bytes(allpcm, ch))
        print(f'sprite {name}: {len(mem)} items, {len(allpcm) / SR:6.1f} s, {os.path.getsize(f"{SPRITES}/{name}.wav") / 1e6:5.2f} MB', flush=True)
    json.dump(manifest, open(f'{WORK}/out/manifest_wave3.json', 'w'))
    total = sum(os.path.getsize(f'{SPRITES}/{f}') for f in os.listdir(SPRITES))
    print(f'{len(manifest)} items, sprites {total / 1e6:.1f} MB, {round(time.time() - t0)} s')
    print('PROBLEMS' if problems else 'no problems', problems)


if __name__ == '__main__':
    main(sys.argv[1] if len(sys.argv) > 1 else None)
