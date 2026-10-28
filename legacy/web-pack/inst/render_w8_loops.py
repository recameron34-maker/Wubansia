"""Wave 8 loops, rendered from the new kits' own parts (out/w8_parts.json, written by export_w8.js) with the new kits and
instruments, so a loop is exactly what the kit plays: drum loops and top loops, bass loops, chord and stab loops, plus
a dusty break filler and a snare build. Writes WAVs into lib/Samples/Loops/{Groove House,Tech House}/ and w8_loops.json."""
import os as _os  # noqa: E402
PACK_ROOT = _os.environ.get('PACK_ROOT') or _os.path.abspath(_os.path.join(_os.path.dirname(_os.path.abspath(__file__)), '..'))  # the legacy web-pack folder
import json, os, sys
import numpy as np
import soundfile as sf
import pedalboard as pb

sys.path.insert(0, PACK_ROOT + '/audio')
sys.path.insert(0, PACK_ROOT + '/inst')
from dsp import SR, bfilt, noise, board, sat  # noqa: E402
import render_w8 as R8  # noqa: E402
import render_inst as RI  # noqa: E402

W = PACK_ROOT
LIB = f'{W}/inst/lib'
PARTS = json.load(open(f'{W}/out/w8_parts.json'))
SHARED = f'{W}/out/plain_wav/hits'
OUT = {}
rng = np.random.default_rng(808)

# the page's drum mix: lane -> (gain, pan)
MIX = {'CB': (0.3, 0.25), 'K': (1, 0), 'SN': (0.55, 0), 'CL': (0.62, 0), 'SNP': (0.42, 0.18), 'RIM': (0.36, -0.2), 'CH': (0.26, 0.25), 'OH': (0.3, 0.3),
       'SH': (0.22, -0.3), 'TB': (0.22, 0.4), 'COH': (0.42, -0.35), 'CLO': (0.46, -0.2), 'BH': (0.38, 0.3), 'BL': (0.4, 0.15), 'CLV': (0.3, 0.35),
       'WB': (0.34, -0.4), 'RD': (0.2, 0.3), 'CR': (0.35, 0), 'LT': (0.62, -0.3), 'MT': (0.6, 0), 'HT': (0.58, 0.3)}
SHARED_LANES = {'TB': 'tamb', 'COH': 'conga_hi', 'CLO': 'conga_lo', 'BH': 'bongo_hi', 'BL': 'bongo_lo', 'CLV': 'clave', 'CB': 'cowbell', 'RD': 'ride', 'CR': 'crash',
                'LT': 'tom_lo', 'MT': 'tom_mid', 'HT': 'tom_hi', 'SNP': 'snap_gh'}
KITMAP = {'gv': {'K': 'kick_gv', 'CL': 'clap_gv', 'SN': 'snare_gv', 'RIM': 'rim_gv', 'CH': 'chat_gv', 'OH': 'ohat_gv', 'SH': 'shaker_gv', 'WB': 'perc_gv'},
          'th': {'K': 'kick_th', 'SN': 'snare_th', 'CL': 'clap_th', 'RIM': 'rim_th', 'CH': 'chat_th', 'OH': 'ohat_th', 'SH': 'shaker_th', 'WB': 'perc_th',
                 'LT': 'tom_th_lo', 'MT': 'tom_th_mid', 'HT': 'tom_th_hi'}}


def load_hits():
    meta = json.load(open(f'{W}/inst/w8_meta.json'))
    hits = {}
    for k, v in meta.items():
        if v.get('inst') or k.startswith(('gvfm', 'gvglow', 'gvfifth', 'thbounce', 'thfifth', 'thrave', 'threese')):
            continue
        x, _ = sf.read(f"{LIB}/{v['path']}", always_2d=True)
        hits[k] = x
    for lane, nm in SHARED_LANES.items():
        p = f'{SHARED}/{nm}.wav'
        if os.path.exists(p):
            hits[nm] = sf.read(p, always_2d=True)[0]
    return hits


def place(buf, x, i, gain, pan):
    x = x if x.ndim == 2 else x[:, None]
    if x.shape[1] == 1:
        l, r = np.sqrt(0.5 - 0.5 * pan) * 1.41, np.sqrt(0.5 + 0.5 * pan) * 1.41
        seg = np.concatenate([x * l, x * r], 1)
    else:
        seg = x * np.array([min(1.0, 1 - pan), min(1.0, 1 + pan)])
    n = min(len(seg), len(buf) - i)
    if n > 0:
        buf[i:i + n] += seg[:n] * gain


def drum_loop(fam, events, bars, bar_sec, hits, drop=(), keep=None, human=0.002, master=True):
    L = int(round(bars * bar_sec * SR))
    buf = np.zeros((L + 2 * SR, 2))
    evs = [e for e in sorted(events, key=lambda e: e['t']) if e['t'] < bars * bar_sec - 1e-6 and e['lane'] not in drop and (not keep or e['lane'] in keep)]
    ch_times = [e['t'] for e in evs if e['lane'] == 'CH']
    for e in evs:
        lane = e['lane']
        name = KITMAP[fam].get(lane) or SHARED_LANES.get(lane)
        if name not in hits:
            continue
        g, p = MIX[lane]
        jitter = 0 if lane in ('K', 'SN', 'CL') else rng.normal(0, human)
        i = max(0, int(round((e['t'] + jitter) * SR)))
        v = (e['v'] / 127) ** 1.6 * (1 + (rng.uniform() - 0.5) * (0.1 if lane in ('CH', 'SH', 'RIM', 'WB') else 0.03))
        x = hits[name]
        if lane == 'OH':                              # the next closed hat chokes the open one
            nxt = [t for t in ch_times + [tt + bars * bar_sec for tt in ch_times] if t > e['t'] + 1e-4]
            if nxt:
                k = int((min(nxt) - e['t']) * SR)
                if k < len(x):
                    env = np.ones(len(x))
                    env[k:] = np.exp(-np.arange(len(x) - k) / (0.006 * SR))
                    x = x * env[:, None]
        place(buf, x, i, g * v, p)
    y = buf[:L].copy()
    y[:len(buf) - L] += buf[L:]                                     # tails wrap round, so the loop repeats seamlessly
    if master:
        y = board(y, [pb.Compressor(threshold_db=-14, ratio=2.5, attack_ms=8, release_ms=90), pb.Gain(2)])
    return R8.norm(y, 0.89)


def gate_env(n, d, rel):
    """Hold for d seconds, then release with time constant rel/3 (the page sampler's release)."""
    t = np.arange(n) / SR
    return np.where(t < d, 1.0, np.exp(-(t - d) / max(1e-3, rel / 3)))


def mel_loop(events, bars, bar_sec, render_note, rel=0.1, stereo=False, tail=1.6):
    L = int(round(bars * bar_sec * SR))
    buf = np.zeros((L + int(tail * SR) + SR, 2 if stereo else 1))
    for e in events:
        if e['t'] >= bars * bar_sec - 1e-6:
            continue
        x = render_note(e)
        if x is None:
            continue
        x = x if x.ndim == 2 else x[:, None]
        n = min(len(x), int((e['d'] + rel * 2.5) * SR))
        x = x[:n] * gate_env(n, e['d'], rel)[:, None]
        if stereo and x.shape[1] == 1:
            x = np.repeat(x, 2, 1)
        i = int(round(e['t'] * SR))
        m = min(n, len(buf) - i)
        buf[i:i + m] += x[:m] * (e['v'] / 127) ** 1.35
    y = buf[:L].copy()
    y[:len(buf) - L] += buf[L:]
    return y if stereo else y[:, 0]


# ---------------------------------------------------------------- per-note renderers (cached by pitch)
_cache = {}


def cached(key, fn):
    if key not in _cache:
        _cache[key] = fn()
    return _cache[key]


def ladder_env(x, start, floor, tau, res, drive=1.2):
    return R8.ladder(x, lambda tt: floor + (start - floor) * np.exp(-tt / tau), res=res, drive=drive)


def bounce_note(e):
    """Bounce bass as the page plays it: the bright loop through a resonant ladder low-pass whose envelope opens much
    further on hard-hit notes, and an amp that falls to -10 dB."""
    x0, L = cached(('bounce', e['n']), lambda: R8.bounce_bass(e['n']))
    n = int((e['d'] + 0.35) * SR)
    x = np.tile(x0, n // len(x0) + 1)[:n] * 0.5
    vel = e['v'] / 127
    start = 380 + 4200 * vel ** 3.2
    y = ladder_env(x, start, 140, 0.085, 0.5, 1.4)
    t = np.arange(n) / SR
    amp = (0.32 + 0.68 * np.exp(-t / 0.4)) * np.clip(t / 0.003, 0, 1)
    return y * amp * 1.6


def talk_note(e):
    """Talking bass: the same loop through a high-resonance ladder whose cutoff rises like a mouth opening, then closes."""
    x0, L = cached(('bounce', e['n']), lambda: R8.bounce_bass(e['n']))
    n = int((e['d'] + 0.35) * SR)
    x = np.tile(x0, n // len(x0) + 1)[:n] * 0.5
    vel = e['v'] / 127
    peak = 700 + 1800 * vel ** 2
    def cut(tt):
        rise = 1 - np.exp(-tt / 0.045)
        fall = np.exp(-np.clip(tt - 0.06, 0, None) / 0.22)
        return 170 + (peak - 170) * rise * fall
    y = R8.ladder(x, cut, res=0.72, drive=1.3)
    t = np.arange(n) / SR
    amp = (0.45 + 0.55 * np.exp(-t / 0.5)) * np.clip(t / 0.004, 0, 1)
    return y * amp * 1.5


def reese_note(e):
    x0, L = cached(('reese', e['n']), lambda: R8.crushed_reese(e['n']))
    n = int((e['d'] + 0.3) * SR)
    t = np.arange(n) / SR
    return np.tile(x0, n // len(x0) + 1)[:n] * np.clip(t / 0.006, 0, 1) * 0.55


def oneshot(fn, tag):
    return lambda e: cached((tag, e['n']), lambda: fn(e['n']))


def organ_note(e):
    def make():
        x, (a, b) = RI.organ(e['n'])
        loop = x[a:b + 1]
        return np.concatenate([x[:a], np.tile(loop, 6)])
    x = cached(('organ', e['n']), make)
    n = int((e['d'] + 0.2) * SR)
    return x[:n] * 0.55


def fm_pluck(m, dur=0.9):
    """FM pluck after the 'freak' plucks: a sine carrier modulated by a saw at the same pitch with a falling index,
    an octave-up FM layer and a breath of noise on the attack."""
    f = RI.m2f(m)
    n = int(dur * SR)
    t = np.arange(n) / SR
    mod = R8.sawv(np.full(n, f), n, 0.0)
    x = np.sin(R8.phase_of(np.full(n, f)) + (0.35 + 2.4 * np.exp(-t / 0.07)) * mod)
    mod2 = R8.sawv(np.full(n, 2 * f), n, 0.3)
    x += 0.3 * np.sin(R8.phase_of(np.full(n, 2 * f)) + 1.6 * np.exp(-t / 0.05) * mod2) * np.exp(-t / 0.15)
    x += 0.08 * bfilt(noise(n), 'highpass', 3000) * np.exp(-t / 0.006)
    x *= np.exp(-t / 0.28) * np.clip(t / 0.0015, 0, 1)
    return R8.fade_out(bfilt(x, 'lowpass', 9000), 0.05)


def string_note(e):
    def make():
        x, L = RI.strings(e['n'])
        return x
    x0 = cached(('strings', e['n']), make)
    n = int((e['d'] + 0.6) * SR)
    x = np.tile(x0, (n // len(x0) + 1, 1))[:n]
    t = np.arange(n) / SR
    return x * np.clip(t / 0.06, 0, 1)[:, None] * 0.35


def verb_swell(dry, events, bar_sec, bars, amount=0.4):
    """The glow chords' signature: a big reverb whose return ducks to 15% while a chord sounds and swells back up in
    the gaps between the chords."""
    L = len(dry)
    gate = np.zeros(L)
    for e in events:
        i0, i1 = int(e['t'] * SR), int((e['t'] + e['d']) * SR)
        gate[max(0, i0):min(L, i1)] = 1
    # duck: 0.15 while gated, swelling back to 1 with a 0.12 s time constant after each gate closes
    g = np.ones(L)
    lvl = 1.0
    a_up = 1 - np.exp(-1 / (0.12 * SR))
    a_dn = 1 - np.exp(-1 / (0.008 * SR))
    for i in range(L):
        target = 0.15 if gate[i] else 1.0
        lvl += (target - lvl) * (a_dn if target < lvl else a_up)
        g[i] = lvl
    wet_in = np.concatenate([dry, dry])                 # two passes so the tail wraps round the loop
    wet = board(wet_in, [pb.Reverb(room_size=0.84, damping=0.5, wet_level=1.0, dry_level=0.0, width=1.0), pb.HighpassFilter(300), pb.LowpassFilter(6000)])
    wet = wet[L:2 * L]
    wet = wet * (np.sqrt(np.mean(dry ** 2)) / (np.sqrt(np.mean(wet ** 2)) + 1e-9))     # level-match, then mix in at `amount`
    return dry + amount * wet * g[:, None]


# ---------------------------------------------------------------- the loops
def melodic_loops():
    specs = [
        # id, kit, part, renderer, stereo, rel, title, note, folder
        ('bl8_gv01', 'GV01', 'bassA', oneshot(R8.fm_bass, 'fm'), False, 0.08, 'FM rubber bass', 'the GV01 bassline on the FM rubber bass', 'Bass Loops'),
        ('bl8_gv03', 'GV03', 'bassA', oneshot(R8.fm_bass, 'fm'), False, 0.06, 'FM octave bass', 'the GV03 disco octave bass on the FM rubber bass', 'Bass Loops'),
        ('bl8_th01', 'TH01', 'bassA', bounce_note, False, 0.06, 'Bounce bass', 'the TH01 bassline: deep and plucky, the filter opening on the hard-hit notes', 'Bass Loops'),
        ('bl8_th02', 'TH02', 'bassA', talk_note, False, 0.08, 'Talking bass', 'the TH02 bassline through a resonant filter that opens and closes like a mouth', 'Bass Loops'),
        ('bl8_th03', 'TH03', 'bassA', reese_note, False, 0.08, 'Crushed Reese', 'the TH03 Reese line, crushed and driven', 'Bass Loops'),
        ('bl8_th04', 'TH04', 'bassA', oneshot(R8.fifth_bass, 'fifth'), False, 0.06, 'Fifth bass', 'the TH04 line on the Moog-style fifth bass', 'Bass Loops'),
        ('ck8_gv01', 'GV01', 'chordsA', oneshot(R8.glow, 'glow'), True, 0.22, 'Glow chords', 'the GV01 chord stabs on the glow saw; the reverb ducks under each stab and swells in the gaps', 'Chord Loops'),
        ('ck8_gv03', 'GV03', 'chordsA', oneshot(R8.glow, 'glow'), True, 0.05, 'Stutter chords', 'the GV03 stuttering chords on the glow saw, reverb swelling in the gaps', 'Chord Loops'),
        ('ld8_gv01', 'GV01', 'leadA', oneshot(R8.fifth_lead, 'fifthlead'), True, 0.2, 'Fifths lead', 'the GV01 Juno-style lead, every note with its fifth', 'Chord Loops'),
        ('sk8_th03', 'TH03', 'leadA', oneshot(R8.rave_stab, 'rave'), True, 0.12, 'Rave stab riff', 'the TH03 riff on the rave stab: every note a minor chord', 'Stab Loops'),
        ('ok8_th03', 'TH03', 'chordsA', organ_note, False, 0.05, 'Organ stabs', 'the TH03 chords as M1-style organ stabs', 'Stab Loops'),
        ('pk8_th04', 'TH04', 'chordsA', oneshot(fm_pluck, 'fmpluck'), True, 0.08, 'FM pluck chords', 'the TH04 chords on an FM pluck (a saw modulating a sine)', 'Stab Loops'),
        ('st8_th02', 'TH02', 'leadA', string_note, True, 0.4, 'Dramatic strings', 'the TH02 string hook with its Phrygian turn', 'Stab Loops'),
    ]
    for sid, kid, part, fn, stereo, rel, title, note, folder in specs:
        k = PARTS[kid]
        ev = k['parts'][part]
        y = mel_loop(ev, 4, k['barSec'], fn, rel=rel, stereo=stereo)
        if sid.startswith('ck8'):
            y = verb_swell(y, ev, k['barSec'], 4)
        if sid == 'ok8_th03':
            y = board(y, [pb.Compressor(threshold_db=-18, ratio=4, attack_ms=2, release_ms=60)])
        if sid in ('ok8_th03', 'pk8_th04'):
            y = R8.room(y, 0.3, 0.14, 0.5, 0.8)
            stereo = True
        if sid == 'pk8_th04':
            y = np.stack([y[:, 0], R8.mod_delay(y[:, 1], 4.0, 1.2, 0.6)], 1)
        if sid == 'bl8_th03':
            y = 0.85 * y + 0.15 * board(y, [pb.Bitcrush(bit_depth=7)])
        y = board(y, [pb.Compressor(threshold_db=-12, ratio=2.0, attack_ms=10, release_ms=120)])
        y = R8.norm(y, 0.89)
        fam = 'Groove House' if kid.startswith('GV') else 'Tech House'
        rel_path = f"Samples/Loops/{fam}/{kid}_{title.title().replace(' ', '')}_{k['short'].replace('#', 's')}-{k['camelot']}_{k['bpm']}bpm.wav"
        R8.write_wav(f'{LIB}/{rel_path}', y)
        OUT[sid] = dict(path=rel_path, kit=kid, title=title, note=note + ', 4-bar loop', bpm=k['bpm'], bars=4, key=k['key'], camelot=k['camelot'],
                        fam=kid[:2].lower(), folder=folder, ch=1 if y.ndim == 1 else 2)
        print(sid, rel_path, f'{len(y) / SR:.2f}s', 'peak', round(float(np.abs(y).max()), 2))


def drum_loops(hits):
    specs = [
        # id, kit, variation, bars, drop, keep, title, note
        ('dl8_gv01', 'GV01', 'main', 2, (), None, 'Canal Lights groove', 'bouncy kick, reverb clap, swung vintage hats, rims and glitch ticks'),
        ('dl8_gv02', 'GV02', 'main', 2, (), None, 'Brown Cafe bongos', 'four-four with bongos, shaker and a big clap'),
        ('dl8_gv03', 'GV03', 'main', 2, (), None, 'Late Checkout disco', 'offbeat open hats, tambourine and claps'),
        ('top8_gv', 'GV01', 'var', 2, ('K', 'CL', 'SN'), None, 'Groove top loop', 'the hats, shaker, bongos and ticks without kick or clap'),
        ('dl8_th01', 'TH01', 'main', 2, (), None, 'Night Shift drop groove', 'punchy kick, hard snare, driving hats and an offbeat tom'),
        ('dl8_th02', 'TH02', 'main', 2, (), None, 'Black Marble toms', 'kick, distorted snare and rolling toms'),
        ('dl8_th03', 'TH03', 'main', 2, (), None, 'Rave Signal skippy groove', 'skippy swung hats, layered clap, ghost snares and rims'),
        ('dl8_th04', 'TH04', 'main', 2, (), None, 'Fifth Floor rims', 'thumping kick, layered clap, offbeat rims and a shaker'),
        ('top8_th', 'TH04', 'var', 2, ('K', 'CL', 'SN'), None, 'Tech top loop', 'offbeat rims, shaker and hats without kick or clap'),
    ]
    for sid, kid, var, bars, drop, keep, title, note in specs:
        k = PARTS[kid]
        fam = 'gv' if kid.startswith('GV') else 'th'
        y = drum_loop(fam, k['drums'][var], bars, k['barSec'], hits, drop=drop, keep=keep)
        folder = 'Groove House' if fam == 'gv' else 'Tech House'
        kind = 'Top Loop' if sid.startswith('top') else 'Drum Loop'
        rel_path = f"Samples/Loops/{folder}/{kid if not sid.startswith('top') else fam.upper()}_{kind.replace(' ', '')}_{title.title().replace(' ', '')}_{k['bpm']}bpm.wav"
        R8.write_wav(f'{LIB}/{rel_path}', y)
        OUT[sid] = dict(path=rel_path, kit=kid, title=f'{kind} · {title}', note=f"{note}, {round(k['swing'] * 100)}% swing", bpm=k['bpm'], bars=bars, fam=fam,
                        folder='Drum Loops' if kind == 'Drum Loop' else 'Tops & Percussion', ch=2, kind='drumloop' if kind == 'Drum Loop' else 'toploop')
        print(sid, rel_path, f'{len(y) / SR:.2f}s')


def break_filler(hits):
    """A dusty breakbeat for groove-house fills: a funky two-bar break on the groove kit, bit-crushed, sample-rate reduced,
    saturated, band-limited, with vinyl crackle on top."""
    k = PARTS['GV01']
    bar, sw = k['barSec'], k['swing']
    step = bar / 16
    def pos(s):
        base = (s // 2) * 2
        return (base + (0 if s % 2 == 0 else 2 * sw)) * step
    pat = {'K': ['9......7..9.....', '9.........9..6..'], 'SN': ['....9..5.....9..', '....9.....5..9.5'],
           'CH': ['6.5.6.5.6.5.6.5.', '6.5.6.5.6.5.6.5.'], 'OH': ['..............7.', '......6.........']}
    ev = []
    for lane, rows in pat.items():
        for b, row in enumerate(rows):
            for s, c in enumerate(row):
                if c != '.':
                    ev.append({'t': b * bar + pos(s), 'lane': lane, 'v': min(127, round(13 + int(c) * 12.7))})
    y = drum_loop('gv', ev, 2, bar, hits, master=False)
    y = board(y, [pb.Resample(target_sample_rate=14000), pb.Bitcrush(bit_depth=9), pb.Distortion(drive_db=6), pb.HighpassFilter(90), pb.LowpassFilter(7500)])
    n = len(y)
    crackle = np.zeros(n)
    idx = rng.integers(0, n, 260)
    crackle[idx] = rng.uniform(-1, 1, 260) * rng.uniform(0.2, 1, 260) ** 3
    crackle = bfilt(crackle, 'bandpass', (1500, 9000), 2) * 2.2 + 0.004 * bfilt(rng.standard_normal(n), 'lowpass', 4000)
    y = y + np.stack([crackle, np.roll(crackle, 37)], 1) * 0.35
    y = R8.norm(board(y, [pb.Compressor(threshold_db=-16, ratio=3, attack_ms=5, release_ms=80)]), 0.89)
    rel_path = f"Samples/Loops/Groove House/GV_BreakFiller_Dusty_{k['bpm']}bpm.wav"
    R8.write_wav(f'{LIB}/{rel_path}', y)
    OUT['brk8_gv'] = dict(path=rel_path, kit='GV01', title='Break filler · dusty', note='a funky two-bar break, bit-crushed and sample-rate reduced, with vinyl crackle, 60% swing',
                         bpm=k['bpm'], bars=2, fam='gv', folder='Breaks', ch=2, kind='break')
    print('brk8_gv', rel_path)


def snare_build(hits):
    """Four-bar tech-house build: the snare rolls from quarters to 32nds and climbs in pitch, the kick drops out in the
    last bar, closed hats rise through a high-pass, and the last eighth is silent."""
    k = PARTS['TH01']
    bar = k['barSec']
    L = int(4 * bar * SR)
    y = np.zeros((L + SR, 2))
    sn = hits['snare_th']
    for i in range(4):
        sub = [4, 8, 16, 32][i]
        for j in range(sub):
            t = i * bar + j * bar / sub
            if t >= 4 * bar - bar / 8:
                continue
            f = t / (4 * bar)
            rate = 1 + 0.5 * f ** 1.5
            idx = np.arange(0, len(sn) - 1, rate)
            x = np.stack([np.interp(idx, np.arange(len(sn)), sn[:, c]) for c in range(sn.shape[1])], 1)
            place(y, x, int(t * SR), 0.25 + 0.75 * f ** 1.2, 0)
        if i < 3:
            for q in range(4):
                place(y, hits['kick_th'], int((i * bar + q * bar / 4) * SR), 1.0, 0)
        for s in range(16):
            place(y, hits['chat_th'], int((i * bar + s * bar / 16) * SR), 0.18 + 0.12 * (s % 2 == 0), 0.25)
    y = y[:L]
    y = board(y, [pb.Compressor(threshold_db=-14, ratio=2.5, attack_ms=5, release_ms=80)])
    cut = int((4 * bar - bar / 8) * SR)
    y[cut:] *= np.exp(-np.arange(L - cut) / (0.004 * SR))[:, None]
    y = R8.norm(y, 0.89)
    rel_path = f"Samples/Loops/Tech House/TH_Build_SnareRoll-4bars_{k['bpm']}bpm.wav"
    R8.write_wav(f'{LIB}/{rel_path}', y)
    OUT['bld8_th'] = dict(path=rel_path, kit='TH01', title='Build · snare roll', note='four bars: the snare rolls from quarters to 32nds and climbs, the kick drops out in the last bar, the last eighth is silent',
                         bpm=k['bpm'], bars=4, fam='th', folder='Fills', ch=2, kind='fill')
    print('bld8_th', rel_path)


if __name__ == '__main__':
    what = sys.argv[1:] or ['drums', 'melodic', 'extras']
    hits = load_hits()
    if 'drums' in what:
        drum_loops(hits)
    if 'extras' in what:
        break_filler(hits); snare_build(hits)
    if 'melodic' in what:
        melodic_loops()
    prev = json.load(open(f'{W}/inst/w8_loops.json')) if os.path.exists(f'{W}/inst/w8_loops.json') else {}
    prev.update(OUT)
    json.dump(prev, open(f'{W}/inst/w8_loops.json', 'w'), indent=1)
