"""Halloween wave (sound effects), synthesised from scratch: thunder, a creaking door, a tuned church bell, a pipe-organ stab, a
wolf howl, a horror string stinger, chains, bats and a ghost whoosh (one-shots), and four loops in A minor at 128 BPM
(theremin, music box, ghost choir; heartbeat and cauldron bubbles are unpitched).

Stereo WAV sources go to out/vox_src/vox/<id>.wav (packed to Ogg FLAC by pack_vox_flac.py, no dry channel); manifest entries go
to out/manifest_halloween_fx.json. Loops are rendered three times round and cut from the middle pass, with the end crossfaded
into what precedes the loop start, so reverb tails carry across the loop point and the loop repeats without a click.
Run: python3 render_halloween_fx.py [id ...]
"""
import os as _os  # noqa: E402
PACK_ROOT = _os.environ.get('PACK_ROOT') or _os.path.abspath(_os.path.join(_os.path.dirname(_os.path.abspath(__file__)), '..'))  # the legacy web-pack folder
import json, os, sys, time
import numpy as np
import soundfile as sf
from scipy import signal as sg
from scipy.signal import fftconvolve
from dsp import SR, peaks, note_hz, step_seconds, bfilt, svf, saw, board, pb
import vocals3 as V3

W = PACK_ROOT
MAN = f'{W}/out/manifest_halloween_fx.json'
SRC = f'{W}/out/vox_src/vox'
LOG = []
BPM = 128
BAR = 16 * step_seconds(BPM)          # 1.875 s


def log(*a):
    s = ' '.join(str(x) for x in a)
    print(s, flush=True)
    LOG.append(s)


def put(entry):
    m = json.load(open(MAN)) if os.path.exists(MAN) else []
    m = [e for e in m if e['id'] != entry['id']] + [entry]
    m.sort(key=lambda e: e['id'])
    json.dump(m, open(MAN, 'w'), indent=0)


# ================================================================= building blocks
def t_of(n):
    return np.arange(n) / SR


def pan2(x, p):
    """Equal-power pan of mono x to stereo, p in -1..1."""
    p = np.broadcast_to(np.asarray(p, float), x.shape)
    a = (p + 1) * np.pi / 4
    return np.stack([x * np.cos(a), x * np.sin(a)], axis=1) * np.sqrt(2)


def hall_ir(sec, predelay=0.02, seed=3, dark=0.5, lo=150, bright=7000):
    rng = np.random.default_rng(seed)
    n = int(sec * SR)
    t = t_of(n)
    ir = rng.standard_normal((n, 2)) * np.exp(-6.9 * t / sec)[:, None]
    lpd = sg.lfilter(*sg.butter(2, 1800 / (SR / 2)), ir, axis=0)
    mix = np.clip(t / sec * (1 + dark), 0, 1)[:, None]
    ir = ir * (1 - mix) + lpd * mix
    ir = sg.lfilter(*sg.butter(2, [lo / (SR / 2), bright / (SR / 2)], 'band'), ir, axis=0)
    ir = np.concatenate([np.zeros((int(predelay * SR), 2)), ir])
    return ir / np.sqrt(np.sum(ir ** 2) / 2)


def verb(x, ir, wet, tail=True):
    """x mono or stereo -> stereo with the impulse added at `wet` (output grows by the impulse length)."""
    st = x if x.ndim == 2 else pan2(x, 0.0)
    n = len(st) + (len(ir) if tail else 0)
    out = np.zeros((n, 2))
    out[:len(st)] += st
    m = st.mean(axis=1)
    for c in (0, 1):
        y = fftconvolve(m, ir[:, c])[:n]
        out[:len(y), c] += y * wet
    return out


def peakn(x, p=1.0):
    return x / (np.abs(x).max() + 1e-12) * p


def tail_trim(x, db=-62, fade=0.05):
    m = np.abs(x).max(axis=1) if x.ndim == 2 else np.abs(x)
    k = np.flatnonzero(m > m.max() * 10 ** (db / 20))
    y = x[:k[-1] + int(0.02 * SR)].copy()
    f = min(len(y) // 4, int(fade * SR))
    y[-f:] *= np.linspace(1, 0, f)[:, None] if y.ndim == 2 else np.linspace(1, 0, f)
    return y


def fade_in(x, sec=0.002):
    f = int(sec * SR)
    x = x.copy()
    x[:f] *= np.linspace(0, 1, f)[:, None] if x.ndim == 2 else np.linspace(0, 1, f)
    return x


def cut_loop(x, L, xf=0.08):
    """x holds three passes of an L-sample loop; returns the middle pass with its end crossfaded into what precedes its start."""
    y = x[L:2 * L].copy()
    k = int(xf * SR)
    r = np.linspace(0, 1, k)
    fo, fi = np.cos(r * np.pi / 2), np.sin(r * np.pi / 2)
    if y.ndim == 2:
        fo, fi = fo[:, None], fi[:, None]
    y[-k:] = y[-k:] * fo + x[L - k:L] * fi
    return y


def smooth_steps(times, values, n, tau):
    """A stepped control curve (value v from time t on) smoothed by a one-pole of time constant tau (portamento)."""
    raw = np.zeros(n)
    order = np.argsort(times)
    for i in order:
        raw[int(times[i] * SR):] = values[i]
    a = np.exp(-1 / (tau * SR))
    return sg.lfilter([1 - a], [1, -a], raw, zi=[raw[0] * a])[0]


def periodic_lfo(n, L, rate, seed=0, depth=1.0):
    """A sine whose rate is rounded so a whole number of cycles fits the loop length L (so it repeats with the loop)."""
    cyc = max(1, round(rate * L / SR))
    ph = np.random.default_rng(seed).uniform(0, 2 * np.pi)
    return depth * np.sin(2 * np.pi * cyc * np.arange(n) / L + ph)


def finish(x, lufs):
    return V3.finish(x, lufs)


def save(fid, x, title, note, kind, pack, **kw):
    os.makedirs(SRC, exist_ok=True)
    x = np.clip(x, -1, 1)
    sf.write(f'{SRC}/{fid}.wav', x.astype(np.float32), SR, subtype='PCM_16')
    e = {'id': fid, 'cat': 'fx', 'grp': 'halloween', 'genre': 'all', 'title': title, 'note': note, 'dur': round(len(x) / SR, 3),
         'file': f'audio/fx/{fid}.wav', 'pack': pack, 'peaks': peaks(x, 72), 'stereo': True, 'kind': kind, 'wave': 7}
    e.update(kw)
    put(e)
    log(f'{fid} {title}: {len(x) / SR:.2f}s, {V3.k_loudness(x):.1f} LUFS, peak {20 * np.log10(np.abs(x).max() + 1e-9):.1f} dB')


ONE = 'Halloween/FX/One-shots/HW_FX_'
LOOP = 'Halloween/FX/Loops/HW_FX_'


# ================================================================= one-shots
def thunder():
    rng = np.random.default_rng(11)
    n = int(7.0 * SR)
    t = t_of(n)
    crack = np.zeros(n)                       # the near crack: a dense spray of tiny bursts, thinning out
    tt = 0.0
    while tt < 0.7:
        i, L = int(tt * SR), int(rng.uniform(0.0015, 0.011) * SR)
        b = rng.standard_normal(L) * np.exp(-np.arange(L) / (L / 3 + 1))
        crack[i:i + L] += b * rng.uniform(0.3, 1.0) * np.exp(-tt / 0.2)
        tt += rng.exponential(0.0035 + tt * 0.05)
    crack = bfilt(crack, 'highpass', 450, 2)
    bang = bfilt(rng.standard_normal(n), 'lowpass', 1400, 2) * np.exp(-t / 0.1)
    drop = np.sin(2 * np.pi * np.cumsum(30 + 60 * np.exp(-t / 0.35)) / SR) * np.exp(-t / 0.5)     # shock-wave thump, 90 -> 30 Hz
    rolls = sum(a * np.exp(-0.5 * ((t - c) / w) ** 2) for a, c, w in [(1.0, 0.3, 0.22), (0.85, 1.0, 0.38), (0.7, 1.9, 0.5), (0.5, 3.0, 0.7), (0.32, 4.4, 0.9)])
    rolls *= np.clip(t / 0.04, 0, 1)
    flutter = 1 + 0.35 * peakn(bfilt(rng.standard_normal(n), 'lowpass', 9, 2))
    cutoff = 1500 * np.exp(-t / 0.8) + 180 * np.exp(-t / 3.5) + 70
    out = np.zeros((n, 2))
    for c, sd in enumerate((21, 22)):         # rumble: brown noise per side, rolling, a low-pass closing as it travels
        r2 = np.random.default_rng(sd)
        br = bfilt(np.cumsum(r2.standard_normal(n)), 'highpass', 22, 2)
        out[:, c] = peakn(svf(peakn(br) * rolls * flutter, cutoff, q=0.55)) * 0.85
    out += pan2(peakn(crack) * 0.75, -0.25) + pan2(peakn(bang) * 0.6 + peakn(drop) * 0.7, 0.0) / np.sqrt(2)
    out = verb(out, hall_ir(4.0, 0.06, seed=4, dark=0.8, lo=60, bright=5000), 0.25)
    return finish(fade_in(tail_trim(out)), -12.0)


def door():
    rng = np.random.default_rng(5)
    dur = 2.9
    n = int(dur * SR)
    t = t_of(n)
    # stick-slip rate (impulses per second) as the door swings: a slow groan, a hinge squeal, settling back down
    knots = [(0.0, 22), (0.35, 45), (0.8, 95), (1.05, 160), (1.2, 330), (1.45, 520), (1.6, 380), (1.75, 150), (2.2, 70), (2.7, 30), (2.9, 25)]
    rate = np.interp(t, [k[0] for k in knots], [k[1] for k in knots]) * (1 + 0.06 * peakn(bfilt(rng.standard_normal(n), 'lowpass', 6, 2)))
    amp = np.interp(t, [0, 0.12, 0.6, 1.3, 1.6, 2.3, 2.9], [0, 0.6, 0.9, 1.0, 0.8, 0.5, 0.0])
    imp = np.zeros(n)
    ph = 0.0
    for i in range(n):
        ph += rate[i] / SR
        if ph >= 1.0:
            ph -= 1.0 + rng.normal(0, 0.07)
            imp[i] = amp[i] * rng.uniform(0.55, 1.0)
    wood = sum(g * sg.lfilter(*sg.iirpeak(f / (SR / 2), q), imp) for f, q, g in
               [(190, 10, 1.0), (430, 14, 0.9), (780, 18, 0.8), (1150, 20, 0.6), (1620, 24, 0.45), (2300, 26, 0.3), (3150, 30, 0.2)])
    metal = sum(g * sg.lfilter(*sg.iirpeak(f / (SR / 2), q), imp) for f, q, g in [(2750, 70, 0.35), (4100, 80, 0.25), (5700, 90, 0.15)])
    rub = bfilt(rng.standard_normal(n), 'bandpass', (300, 2400), 2) * amp * 0.05 * np.clip(rate / 200, 0, 1)
    x = peakn(wood) + peakn(metal) * 0.35 + rub
    x = bfilt(x, 'highpass', 90, 2)
    out = verb(pan2(peakn(x), np.linspace(-0.2, 0.25, n)), hall_ir(1.4, 0.012, seed=6, dark=0.4, lo=120, bright=6000), 0.2)
    return finish(fade_in(tail_trim(out)), -15.0)


def bell(prime=220.0):
    """A church bell struck on A3: hum, prime, minor-third tierce, quint, nominal and upper partials. Each partial has a weaker
    doublet a fraction of a hertz away (the slow warble of a real bell), offset differently left and right."""
    rng = np.random.default_rng(3)
    n = int(9.0 * SR)
    t = t_of(n)
    parts = [(0.5, 0.75, 6.0), (1.0, 0.8, 5.0), (1.189, 0.6, 4.0), (1.498, 0.35, 2.8), (2.0, 1.0, 3.2), (2.52, 0.3, 2.0), (2.67, 0.25, 1.8),
             (3.0, 0.32, 1.5), (4.0, 0.22, 1.1), (5.04, 0.12, 0.7), (5.33, 0.1, 0.6), (6.0, 0.08, 0.5), (8.0, 0.05, 0.35)]
    out = np.zeros((n, 2))
    for r, a, tau in parts:
        f = prime * r
        ph = rng.uniform(0, 6.28)
        main = np.sin(2 * np.pi * f * t + ph) * np.exp(-t / tau)
        for c in (0, 1):
            d = f * rng.uniform(0.0004, 0.0011) * (1 if c else -1)
            out[:, c] += a * (main + 0.28 * np.sin(2 * np.pi * (f + d) * t + rng.uniform(0, 6.28)) * np.exp(-t / tau))
    strike = bfilt(rng.standard_normal(n), 'highpass', 1800, 2) * np.exp(-t / 0.012) * 0.6
    clang = sum(np.sin(2 * np.pi * f * t) * np.exp(-t / 0.06) for f in (3170, 4410, 5930, 7020)) * 0.12
    out += pan2(strike + clang, 0.0) / np.sqrt(2)
    out *= (np.clip(t / 0.0015, 0, 1) * np.clip((n / SR - t) / 2.5, 0, 1))[:, None]      # struck; faded well before the end
    out = verb(out, hall_ir(4.5, 0.04, seed=9, dark=0.6, lo=90, bright=6500), 0.3)
    out = out[:int(9.5 * SR)]
    k = int(1.0 * SR)
    out[-k:] *= np.linspace(1, 0, k)[:, None] ** 2
    return finish(fade_in(tail_trim(out)), -13.0)


def organ():
    """Pipe organ, A minor with the octaves doubled (A2 E3 A3 C4 E4 A4), principal ranks 16'-2' and a mixture, tremulant,
    chiff on the attack, held 1.5 s into a cathedral."""
    rng = np.random.default_rng(8)
    hold = 1.5
    n = int((hold + 0.4) * SR)
    t = t_of(n)
    notes = ['A2', 'E3', 'A3', 'C4', 'E4', 'A4']
    ranks = [(0.5, 0.55), (1.0, 1.0), (2.0, 0.5), (3.0, 0.22), (4.0, 0.22), (6.0, 0.07), (8.0, 0.06)]
    trem_a = 1 + 0.07 * np.sin(2 * np.pi * 6.1 * t)
    trem_p = 2 ** (4 * np.sin(2 * np.pi * 6.1 * t + 0.6) / 1200)
    env = np.clip(t / 0.05, 0, 1) * np.clip((hold + 0.12 - t) / 0.12, 0, 1)
    out = np.zeros((n, 2))
    for k, nm in enumerate(notes):
        f0 = note_hz(nm)
        x = np.zeros(n)
        for r, a in ranks:
            if r == 0.5 and k > 1:
                continue
            f = f0 * r * 2 ** (rng.normal(0, 1.5) / 1200)
            if f > 9000:
                continue
            ph = 2 * np.pi * np.cumsum(f * trem_p) / SR + rng.uniform(0, 6.28)
            x += a * (np.sin(ph) + 0.3 * np.sin(2 * ph) + 0.12 * np.sin(3 * ph) + 0.05 * np.sin(4 * ph))
        chiff = bfilt(rng.standard_normal(n), 'bandpass', (f0 * 3, min(f0 * 8, 12000)), 2) * np.exp(-t / 0.025) * 0.35
        out += pan2((x / 3 + chiff) * env * trem_a, (k / (len(notes) - 1) - 0.5) * 0.8)
    wind = bfilt(rng.standard_normal(n), 'lowpass', 900, 2) * env * 0.012
    out += pan2(wind, 0.0)
    out = verb(out, hall_ir(5.0, 0.05, seed=12, dark=0.55, lo=70, bright=7000), 0.42)
    return finish(fade_in(tail_trim(out)), -12.0)


def formant_bank(x, formants):
    return sum(g * sg.lfilter(*sg.iirpeak(f / (SR / 2), f / bw), x) for f, bw, g in formants)


def howl_voice(n, t, f_knots, a_knots, seed, vib=18):
    rng = np.random.default_rng(seed)
    f0 = np.interp(t, *zip(*f_knots))
    f0 *= 2 ** ((vib * np.sin(2 * np.pi * 5.2 * t + rng.uniform(0, 6)) * np.clip((t - 0.6) / 0.5, 0, 1) +
                 6 * peakn(bfilt(rng.standard_normal(n), 'lowpass', 3, 2))) / 1200)
    amp = np.interp(t, *zip(*a_knots))
    src = saw(f0) * 0.45 + np.sin(2 * np.pi * np.cumsum(f0) / SR) * 0.8
    src = bfilt(src, 'lowpass', 3500, 2) + rng.standard_normal(n) * 0.05
    vow = np.interp(t, [0, 0.9, 2.4, 3.6], [0, 1, 1, 0.2])     # 'oo' -> 'ah' -> 'oo'
    oo = formant_bank(src, [(360, 90, 1.0), (820, 120, 0.5), (2600, 200, 0.08)])
    ah = formant_bank(src, [(720, 110, 1.0), (1180, 130, 0.6), (2650, 220, 0.12)])
    return (oo * (1 - vow) + ah * vow) * amp


def howl():
    n = int(5.2 * SR)
    t = t_of(n)
    lead = howl_voice(n, t, [(0, 300), (0.15, 340), (0.9, 640), (2.5, 665), (3.4, 520), (3.9, 430), (5.2, 420)],
                      [(0, 0), (0.25, 0.9), (2.5, 1.0), (3.5, 0.55), (3.95, 0.0), (5.2, 0)], 1)
    far = howl_voice(n, t, [(0, 400), (1.6, 400), (1.75, 450), (2.4, 700), (3.8, 690), (4.6, 560), (5.0, 500), (5.2, 500)],
                     [(0, 0), (1.6, 0), (1.9, 0.5), (3.8, 0.55), (4.7, 0.2), (5.0, 0), (5.2, 0)], 2, vib=24)
    far = bfilt(far, 'lowpass', 2500, 2)
    out = pan2(peakn(lead), -0.25) + pan2(peakn(far) * 0.35, 0.7)
    for d, g, p in ((0.31, 0.22, 0.6), (0.57, 0.12, -0.6)):        # echoes off the hills
        k = int(d * SR)
        e = np.zeros(n)
        e[k:] = bfilt(peakn(lead), 'lowpass', 1800, 2)[:n - k]
        out += pan2(e * g, p)
    out = verb(out, hall_ir(3.0, 0.03, seed=14, dark=0.6, lo=150, bright=6000), 0.22)
    return finish(fade_in(tail_trim(out)), -14.0)


def stinger():
    """A screeching cluster of violins round A5-C6 (sul ponticello, wide vibrato, bow tremolo) swelling for 1.4 s over a low
    timpani and brass hit, then cut into a hall."""
    rng = np.random.default_rng(13)
    n = int(2.2 * SR)
    t = t_of(n)
    hold = 1.45
    strings = np.zeros((n, 2))
    pitches = [81, 82, 83, 84, 81.3, 82.6, 83.4, 84.2, 93, 94, 95.5]
    for k, m in enumerate(pitches):
        f = 440 * 2 ** ((m - 69) / 12)
        vib = 2 ** ((rng.uniform(25, 45) * np.sin(2 * np.pi * rng.uniform(5.8, 7.2) * t + rng.uniform(0, 6))) / 1200)
        bend = 2 ** (rng.uniform(-30, 30) * t / hold / 1200)
        x = saw(f * vib * bend)
        trem = 1 - 0.45 * (0.5 + 0.5 * np.sin(2 * np.pi * rng.uniform(12, 16) * t + rng.uniform(0, 6)))
        env = np.clip(t / 0.04, 0, 1) * (0.35 + 0.65 * np.clip(t / hold, 0, 1) ** 0.7) * np.clip((hold + 0.06 - t) / 0.06, 0, 1)
        strings += pan2(x * trem * env * (0.6 if m > 90 else 1.0), rng.uniform(-0.8, 0.8))
    strings = bfilt(strings, 'highpass', 1000, 2)
    strings = board(strings, [pb.PeakFilter(3500, 6.0, 0.8), pb.LowpassFilter(9500)])
    timp = np.sin(2 * np.pi * np.cumsum(55 + 25 * np.exp(-t / 0.05)) / SR) * np.exp(-t / 0.9)
    thump = bfilt(rng.standard_normal(n), 'lowpass', 400, 2) * np.exp(-t / 0.05)
    brass = sum(saw(np.full(n, 440 * 2 ** ((m - 69) / 12))) for m in (33, 34, 45)) * np.exp(-t / 0.7) * np.clip(t / 0.02, 0, 1)
    brass = bfilt(brass, 'lowpass', 420, 2)
    low = peakn(timp) * 0.9 + peakn(thump) * 0.5 + peakn(brass) * 0.45
    out = peakn(strings) * 0.8 + pan2(peakn(low) * 0.9, 0.0) / np.sqrt(2)
    out = verb(out, hall_ir(3.6, 0.03, seed=15, dark=0.5, lo=90, bright=8000), 0.32)
    return finish(fade_in(tail_trim(out)), -12.0)


def clink(n_len, base, rng, heavy=False):
    L = int(n_len * SR)
    t = t_of(L)
    ratios = [1.0, 2.756, 5.404, 8.933]
    x = np.zeros(L)
    for k, r in enumerate(ratios):
        f = base * r * rng.uniform(0.97, 1.03)
        if f > 15000:
            continue
        tau = (0.14 if heavy else 0.07) / (1 + k * 1.3)
        x += np.sin(2 * np.pi * f * t + rng.uniform(0, 6)) * np.exp(-t / tau) / (1 + k * 0.6)
    x += rng.standard_normal(L) * np.exp(-t / 0.0015) * 0.4
    return x


def chains():
    rng = np.random.default_rng(17)
    n = int(3.2 * SR)
    out = np.zeros((n, 2))
    for c0, c1, peak in ((0.0, 0.75, 1.0), (0.95, 1.6, 0.7), (1.85, 2.7, 0.9)):     # three shakes and drags
        tt = c0
        while tt < c1:
            q = (tt - c0) / (c1 - c0)
            heavy = rng.random() < 0.15
            x = clink(0.35 if heavy else 0.2, rng.uniform(500, 1100) if heavy else rng.uniform(1700, 3400), rng, heavy)
            g = peak * np.sin(np.pi * q) ** 0.5 * rng.uniform(0.35, 1.0) * (1.4 if heavy else 1.0)
            i = int(tt * SR)
            k = min(len(x), n - i)
            out[i:i + k] += pan2(x[:k] * g, rng.uniform(-0.6, 0.6))
            tt += rng.exponential(1 / 38)
    drag = bfilt(rng.standard_normal(n), 'bandpass', (1500, 6000), 2) * 0.02
    out += pan2(drag * np.interp(t_of(n), [0, 0.3, 2.7, 3.2], [0, 1, 1, 0]), 0.0)
    out = bfilt(out, 'highpass', 200, 2)
    out = verb(out, hall_ir(1.8, 0.015, seed=18, dark=0.4, lo=200, bright=9000), 0.25)
    return finish(fade_in(tail_trim(out)), -15.0)


def bats():
    rng = np.random.default_rng(19)
    n = int(3.0 * SR)
    t = t_of(n)
    out = np.zeros((n, 2))
    for b in range(7):
        c = rng.uniform(0.5, 2.3)
        w = rng.uniform(0.35, 0.6)
        side = rng.choice([-1, 1])
        rate = rng.uniform(10.5, 14.5)
        env = np.exp(-0.5 * ((t - c) / w) ** 2)
        pan = np.clip(side * (t - c) / (1.6 * w), -0.95, 0.95)
        flaps = np.zeros(n)
        tt = c - 2.5 * w
        while tt < c + 2.5 * w:
            i = int(tt * SR)
            if 0 <= i < n - 2000:
                L = int(0.028 * SR)
                burst = rng.standard_normal(L) * np.sin(np.pi * np.arange(L) / L) ** 2
                flaps[i:i + L] += burst * rng.uniform(0.6, 1.0)
            tt += 1 / rate * rng.uniform(0.92, 1.08)
        flaps = bfilt(flaps, 'bandpass', (250, 2200), 2)
        squeaks = np.zeros(n)
        for _ in range(rng.integers(3, 7)):
            st = c + rng.uniform(-1.5, 1.5) * w
            i = int(st * SR)
            L = int(rng.uniform(0.018, 0.035) * SR)
            if not 0 <= i < n - L:
                continue
            tl = t_of(L)
            f = np.linspace(rng.uniform(8500, 10500), rng.uniform(5500, 7000), L)
            sq = np.sin(2 * np.pi * np.cumsum(f) / SR) * np.clip(np.sin(np.pi * tl / tl[-1]), 0, 1) ** 1.5
            squeaks[i:i + L] += sq * rng.uniform(0.2, 0.45)
        out += pan2((flaps * 0.9 + squeaks) * env, pan)
    out = verb(out, hall_ir(1.6, 0.02, seed=20, dark=0.5, lo=200, bright=11000), 0.2)
    return finish(fade_in(tail_trim(out)), -16.0)


def whoosh():
    rng = np.random.default_rng(23)
    n = int(3.6 * SR)
    t = t_of(n)
    swell = np.interp(t, [0, 1.1, 1.5, 3.0, 3.6], [0, 1.0, 0.9, 0.15, 0])
    centre = np.interp(t, [0, 1.2, 3.6], [280, 2300, 500])
    air = svf(peakn(rng.standard_normal(n)), centre, q=3.0, mode='bp')
    f0 = np.interp(t, [0, 1.3, 3.6], [560, 520, 360]) * 2 ** (30 * np.sin(2 * np.pi * 4.6 * t) / 1200)
    ph = 2 * np.pi * np.cumsum(f0) / SR
    moan = np.sin(ph) + 0.25 * np.sin(3 * ph) + 0.1 * np.sin(5 * ph)
    moan = formant_bank(moan, [(400, 80, 1.0), (850, 120, 0.4)]) * np.interp(t, [0, 0.5, 1.4, 2.8, 3.4], [0, 0.2, 1.0, 0.3, 0])
    mono = peakn(air) * swell * 0.8 + peakn(moan) * 0.55
    rev_ir = hall_ir(1.8, 0.0, seed=24, dark=0.5)[:, 0]
    pre = fftconvolve(mono[::-1], rev_ir)[:n][::-1]          # reverse reverb: the ghost breathes in before it passes
    out = pan2(mono + peakn(pre) * 0.35, np.interp(t, [0, 3.6], [-0.9, 0.9]))
    out = verb(out, hall_ir(2.8, 0.02, seed=25, dark=0.5, lo=150, bright=8000), 0.25)
    return finish(fade_in(tail_trim(out), 0.02), -15.0)


# ================================================================= loops (A minor, 128 BPM)
def three_pass_events(events, L, synth, seed=0):
    """Render events [(t_sec, ...)] three times round into a buffer of 3 loops (plus 2 s of tail)."""
    out = np.zeros((3 * L + 2 * SR, 2))
    for p in range(3):
        rng = np.random.default_rng(seed)        # identical randomness each pass, so the passes match
        for ev in events:
            x = synth(ev, rng)
            i = int(round(ev[0] * SR)) + p * L
            if i < 0:
                x, i = x[-i:], 0
            k = min(len(x), len(out) - i)
            out[i:i + k] += x[:k]
    return out


def theremin():
    L = int(4 * BAR * SR)
    st = step_seconds(BPM)
    notes = [(0, 6, 'A4'), (6, 10, 'E5'), (16, 6, 'D5'), (22, 4, 'C5'), (26, 6, 'E5'), (32, 8, 'A5'), (40, 4, 'G5'), (44, 4, 'E5'),
             (48, 4, 'F5'), (52, 6, 'E5'), (58, 6, 'C5')]
    n = 3 * L + 2 * SR
    times, freqs, starts = [], [], []
    for p in range(4):
        for s, ln, nm in notes:
            times.append(p * L / SR + s * st)
            freqs.append(np.log2(note_hz(nm)))
            starts.append(p * L / SR + s * st)
    lf = smooth_steps(np.array(times), np.array(freqs), n, 0.045)
    t = t_of(n)
    since = np.zeros(n)                       # seconds since the current note began (vibrato fades in, volume swells)
    for s0 in sorted(starts):
        i = int(s0 * SR)
        since[i:] = t[i:] - s0
    vib = 26 * np.clip((since - 0.12) / 0.25, 0, 1) * np.sin(2 * np.pi * 6.2 * t)
    f = 2 ** lf * 2 ** (vib / 1200)
    ph = 2 * np.pi * np.cumsum(f) / SR
    tone = np.sin(ph) + 0.16 * np.sin(2 * ph + 0.4) + 0.06 * np.sin(3 * ph)
    vol = 0.72 + 0.28 * np.clip(since / 0.09, 0, 1) * np.exp(-since / 3.0)
    vol *= 1 - 0.25 * np.exp(-since / 0.03)
    a = np.exp(-1 / (0.018 * SR))                  # the volume hand moves smoothly: no steps at the note changes
    vol = sg.lfilter([1 - a], [1, -a], vol, zi=[vol[0] * a])[0]
    x = tone * vol
    out = pan2(x, 0.0) / np.sqrt(2)
    d = int(3 * st * SR)
    echo = np.zeros(n)
    echo[d:] = bfilt(x, 'lowpass', 2500, 2)[:n - d]
    out += pan2(echo * 0.22, 0.55)
    out = verb(out, hall_ir(2.4, 0.02, seed=31, dark=0.5, lo=200, bright=7000), 0.28, tail=False)
    out = finish(out[:3 * L + int(0.5 * SR)], -15.0)
    return cut_loop(out, L)


def tine(f, rng, dur=2.5):
    n = int(dur * SR)
    t = t_of(n)
    det = 2 ** (rng.normal(0, 3) / 1200)
    tau1 = np.clip(2.6 - np.log2(f / 500), 0.6, 2.4)
    x = (np.sin(2 * np.pi * f * det * t) * np.exp(-t / tau1) + 0.32 * np.sin(2 * np.pi * f * 6.27 * t) * np.exp(-t / 0.18)
         + 0.1 * np.sin(2 * np.pi * min(f * 17.55, 19000) * t) * np.exp(-t / 0.05))
    x += bfilt(rng.standard_normal(n), 'highpass', 3000, 2) * np.exp(-t / 0.0015) * 0.25
    return x * np.clip(t / 0.0008, 0, 1)


def musicbox():
    L = int(4 * BAR * SR)
    st = step_seconds(BPM)
    mel = [(0, 'E5'), (2, 'A5'), (4, 'C6'), (6, 'E6'), (10, 'D6'), (12, 'C6'), (14, 'B5'),
           (16, 'A5'), (20, 'E5'), (22, 'G5'), (24, 'A5'),
           (32, 'E5'), (34, 'A5'), (36, 'C6'), (38, 'E6'), (42, 'F6'), (44, 'E6'), (46, 'D6'),
           (48, 'C6'), (52, 'B5'), (54, 'G5'), (56, 'A5')]
    bass = [(0, 'A4'), (16, 'E4'), (24, 'A4'), (32, 'A4'), (48, 'E4'), (56, 'A4')]
    ev = [(s * st, nm, 0.0, 1.0) for s, nm in mel] + [(s * st, nm, 0.0, 0.55) for s, nm in bass]
    ev = [(t0 + 0.006 * np.sin(k * 1.7), nm, 0, g) for k, (t0, nm, _, g) in enumerate(ev)]     # a mechanism that isn't quite even
    out = three_pass_events(ev, L, lambda e, rng: pan2(tine(note_hz(e[1]), rng) * e[3], 0.25 * np.sin(note_hz(e[1]) / 90)), seed=4)
    n = len(out)
    wow = 2 ** (9 * np.sin(2 * np.pi * np.arange(n) / L * 3) / 1200)        # tape wow, three cycles a loop
    idx = np.clip(np.cumsum(wow) - 1, 0, n - 1)
    out = np.stack([np.interp(idx, np.arange(n), out[:, c]) for c in (0, 1)], axis=1)
    out = board(out, [pb.LowpassFilter(7500), pb.HighpassFilter(250)])
    out = verb(out, hall_ir(1.8, 0.015, seed=33, dark=0.5, lo=200, bright=8000), 0.25, tail=False)
    out = finish(out[:3 * L + int(0.5 * SR)], -16.0)
    return cut_loop(out, L, 0.03)


def heartbeat():
    L = int(2 * BAR * SR)
    beat = 4 * step_seconds(BPM)

    def thump(f_hi, f_lo, tau, g, rng):
        n = int(0.35 * SR)
        t = t_of(n)
        x = np.sin(2 * np.pi * np.cumsum(f_lo + (f_hi - f_lo) * np.exp(-t / 0.03)) / SR) * np.exp(-t / tau)
        x += bfilt(rng.standard_normal(n), 'lowpass', 220, 2) * np.exp(-t / 0.02) * 0.3
        return x * np.clip(t / 0.004, 0, 1) * g

    ev = []
    for b in range(8):
        ev.append((b * beat, 'lub'))
        ev.append((b * beat + 0.135, 'dub'))
    out = three_pass_events(ev, L, lambda e, rng: pan2(thump(78, 44, 0.07, 1.0, rng) if e[1] == 'lub' else thump(92, 52, 0.05, 0.7, rng), 0.0), seed=6)
    out = np.tanh(2.2 * out / np.abs(out).max()) * 0.8          # saturation: harmonics so it reads on small speakers too
    out = board(out, [pb.LowpassFilter(900), pb.HighpassFilter(28)])
    out = verb(out, hall_ir(0.8, 0.005, seed=35, dark=0.6, lo=60, bright=2000), 0.12, tail=False)
    out = finish(out[:3 * L + int(0.5 * SR)], -13.0)
    return cut_loop(out, L, 0.02)


def cauldron():
    L = int(2 * BAR * SR)
    rng = np.random.default_rng(37)
    ev = []
    tt = 0.0
    while tt < L / SR:
        big = rng.random() < 0.2
        f0 = rng.uniform(140, 260) if big else rng.uniform(320, 900)
        ev.append((tt, f0, big, rng.uniform(-0.6, 0.6), rng.uniform(0.5, 1.0)))
        tt += rng.exponential(1 / 13)

    def bubble(e, r):
        t0, f0, big, p, g = e
        tau = 0.11 if big else rng_tau(f0)
        n = int(tau * 6 * SR)
        t = t_of(n)
        f = f0 * (1 + 0.9 * t / (tau * 2))                  # the pitch rises as the bubble rises
        x = np.sin(2 * np.pi * np.cumsum(f) / SR) * np.exp(-t / tau) * np.clip(t / 0.002, 0, 1)
        if big:
            k = int(tau * 3.5 * SR)
            pop = bfilt(r.standard_normal(n), 'bandpass', (600, 3000), 2) * np.exp(-np.maximum(t - t[k], 0) / 0.006) * (t >= t[k]) * 0.4
            x += pop
        return pan2(x * g * (1.3 if big else 0.8), p)

    def rng_tau(f0):
        return 0.02 + 6.0 / f0

    out = three_pass_events(ev, L, bubble, seed=38)
    n = len(out)
    simmer = np.zeros((n, 2))
    for c, sd in enumerate((41, 42)):
        r2 = np.random.default_rng(sd)
        brn = bfilt(np.cumsum(r2.standard_normal(n)), 'highpass', 30, 2)
        simmer[:, c] = bfilt(peakn(brn), 'lowpass', 220, 2) * 0.5 + bfilt(r2.standard_normal(n), 'bandpass', (2500, 7000), 2) * 0.012
    out = peakn(out) + peakn(simmer) * 0.22
    out = verb(out, hall_ir(1.3, 0.01, seed=43, dark=0.5, lo=120, bright=7000), 0.16, tail=False)
    out = finish(out[:3 * L + int(0.5 * SR)], -18.0)
    return cut_loop(out, L, 0.12)


def choir():
    """Ghost choir on 'oo': four parts, three singers each (detuned, their own vibrato), A3 and E4 held under a soprano that
    leans on the flat sixth (E5 | F5 | E5 | D5) and an alto (C5 | C5 | B4 | B4); breathes with the bar, long hall."""
    L = int(4 * BAR * SR)
    n = 3 * L + 2 * SR
    t = t_of(n)
    parts = [(['A3'] * 4, 0.0, 1.0), (['E4'] * 4, -0.35, 0.8), (['C5', 'C5', 'B4', 'B4'], 0.35, 0.7), (['E5', 'F5', 'E5', 'D5'], 0.0, 0.75)]
    out = np.zeros((n, 2))
    breath = 0.8 + 0.2 * (0.5 + 0.5 * np.sin(2 * np.pi * np.arange(n) / (BAR * SR) - np.pi / 2))
    k = 0
    for seq, pan, g in parts:
        times = [p * L / SR + b * BAR for p in range(4) for b in range(4)]
        vals = [np.log2(note_hz(seq[b])) for p in range(4) for b in range(4)]
        lf = smooth_steps(np.array(times), np.array(vals), n, 0.08)
        for s in range(3):
            k += 1
            det = (s - 1) * 7 + periodic_lfo(n, L, 0.11, seed=k, depth=4)
            vib = periodic_lfo(n, L, 5.0 + 0.3 * s, seed=100 + k, depth=14)
            f = 2 ** lf * 2 ** ((det + vib) / 1200)
            src = saw(f)
            src = sg.lfilter([0.15], [1, -0.85], src)            # soft glottal tilt
            src += np.random.default_rng(200 + k).standard_normal(n) * 0.015
            v = formant_bank(src, [(310, 60, 1.0), (870, 90, 0.45), (2250, 120, 0.12), (3300, 160, 0.05)])
            out += pan2(peakn(v) * g * breath, np.clip(pan + (s - 1) * 0.25, -0.9, 0.9))
    out = board(out, [pb.HighpassFilter(110), pb.Chorus(rate_hz=0.4, depth=0.25, mix=0.3, centre_delay_ms=12)])
    out = verb(out, hall_ir(4.5, 0.04, seed=45, dark=0.55, lo=150, bright=7000), 0.45, tail=False)
    out = finish(out[:3 * L + int(0.5 * SR)], -16.0)
    return cut_loop(out, L, 0.15)


# ================================================================= Murder at Cameron Castle
def gong(f0=220.0):
    """A brass dinner gong on A3, struck three times on the beat (a summons): inharmonic partials, the upper ones blooming a
    moment after each strike, soft felt mallet, a castle hall."""
    rng = np.random.default_rng(47)
    beat = 60 / BPM
    n = int(9.0 * SR)
    t = t_of(n)
    parts = [(1.0, 1.0, 5.5, 0.0), (1.52, 0.55, 3.6, 0.06), (2.0, 0.5, 3.0, 0.1), (2.31, 0.3, 2.2, 0.14), (2.7, 0.28, 1.8, 0.2), (3.0, 0.22, 1.6, 0.2),
             (3.48, 0.18, 1.3, 0.25), (4.1, 0.14, 1.0, 0.3), (4.65, 0.1, 0.8, 0.3), (5.3, 0.08, 0.6, 0.35), (6.0, 0.06, 0.5, 0.35)]
    out = np.zeros((n, 2))
    for at, g in ((0.0, 0.55), (2 * beat, 0.75), (4 * beat, 1.0)):
        i = int(at * SR)
        tl = t[:n - i]
        ring = np.zeros((n - i, 2))
        for r, a, tau, bloom in parts:
            env = np.exp(-tl / tau) * (1 - np.exp(-tl / bloom) if bloom else 1.0)
            for c in (0, 1):
                f = f0 * r * (1 + rng.uniform(-0.0006, 0.0006))
                ring[:, c] += a * env * (np.sin(2 * np.pi * f * tl + rng.uniform(0, 6.28)) + 0.25 * np.sin(2 * np.pi * f * (1.0012 if c else 0.9988) * tl))
        mallet = bfilt(rng.standard_normal(n - i), 'lowpass', 900, 2) * np.exp(-tl / 0.015) * 0.6
        ring += pan2(mallet, 0.0) / np.sqrt(2)
        out[i:] += ring * g * np.clip(tl / 0.002, 0, 1)[:, None]
    out *= np.clip((n / SR - t) / 2.0, 0, 1)[:, None]
    out = verb(out, hall_ir(3.6, 0.035, seed=48, dark=0.55, lo=90, bright=7000), 0.26)[:int(8.6 * SR)]
    k = int(1.2 * SR)
    out[-k:] *= np.linspace(1, 0, k)[:, None] ** 2
    return finish(fade_in(tail_trim(out)), -14.0)


def brass(freqs, t, attack=0.012, cut_hi=2600.0, cut_lo=500.0, tau_f=0.25, tau_a=0.4, detune=5.0):
    n = len(t)
    x = np.zeros(n)
    for f in freqs:
        for d in (-detune, 0.0, detune):
            x += saw(np.full(n, f * 2 ** (d / 1200)))
    rise = np.clip(t / attack, 0, 1)
    cutoff = cut_lo + (cut_hi - cut_lo) * np.exp(-t / tau_f) * rise
    return svf(x / (3 * len(freqs)), cutoff, q=0.9) * rise * np.exp(-t / tau_a)


def timp(f, t, rng, g=1.0):
    n = len(t)
    pitch = f * (1 + 0.25 * np.exp(-t / 0.04))
    x = np.sin(2 * np.pi * np.cumsum(pitch) / SR) + 0.35 * np.sin(2 * np.pi * np.cumsum(pitch * 1.5) / SR) * np.exp(-t / 0.3)
    x = x * np.exp(-t / 0.8) + bfilt(rng.standard_normal(n), 'lowpass', 500, 2) * np.exp(-t / 0.03) * 0.5
    return x * g * np.clip(t / 0.002, 0, 1)


def reveal():
    """The whodunnit sting: two short low-brass and timpani hits on A, then a held A diminished chord with tremolo strings,
    a timpani roll and a cymbal swell, into a hall."""
    rng = np.random.default_rng(51)
    beat = 60 / BPM
    n = int(6.5 * SR)
    out = np.zeros((n, 2))

    def place(x, at, pan=0.0, g=1.0):
        i = int(at * SR)
        k = min(len(x), n - i)
        out[i:i + k] += pan2(x[:k] * g, pan) / np.sqrt(2)

    for at in (0.0, beat):
        tl = t_of(int(0.9 * SR))
        place(peakn(brass([55.0, 82.41, 110.0], tl, tau_a=0.3)), at, 0.0, 0.85)
        place(peakn(timp(55.0, tl, rng)), at, 0.0, 0.9)
    at = 2 * beat
    tl = t_of(int(4.4 * SR))
    tail = np.clip((tl[-1] - tl) / 1.2, 0, 1) ** 2          # everything in the last hit dies away before its buffer ends
    held = brass([55.0, 110.0, 130.81, 155.56], tl, tau_f=0.7, tau_a=1.7, cut_hi=3400.0, cut_lo=800.0) * tail
    place(peakn(held), at, 0.0, 1.0)
    strings = sum(saw(np.full(len(tl), f * 2 ** (rng.uniform(-9, 9) / 1200))) for f in (440.0, 523.25, 622.25) for _ in range(3))
    trem = 1 - 0.5 * (0.5 + 0.5 * np.sin(2 * np.pi * 13 * tl))
    strings = bfilt(strings * trem, 'bandpass', (500, 6500), 2) * np.clip(tl / 0.03, 0, 1) * np.exp(-tl / 1.5) * tail
    place(peakn(strings), at, -0.35, 0.4)
    place(peakn(strings[::-1][::-1]), at + 0.012, 0.35, 0.3)
    roll = np.zeros(len(tl))
    for k in range(int(1.6 * 18)):                     # a timpani roll fading away, 18 strokes a second
        i = int(k / 18 * SR)
        st = timp(55.0, t_of(len(tl) - i), rng, g=0.5 * np.exp(-k / 18 / 0.7) * rng.uniform(0.8, 1.0))
        roll[i:] += st
    place(peakn(roll * tail), at, 0.1, 0.55)
    cym = bfilt(rng.standard_normal(len(tl)), 'highpass', 4500, 2) * np.exp(-tl / 1.3) * np.clip(tl / 0.01, 0, 1) * tail
    place(cym, at, -0.2, 0.18)
    out = verb(out, hall_ir(3.8, 0.03, seed=52, dark=0.5, lo=80, bright=8000), 0.3)
    return finish(fade_in(tail_trim(out)), -12.0)


def camera_flash():
    """Flash McGillicuddy's camera: the flash charging up (a whine rising to 9 kHz), the shutter (two curtain clicks and a clack),
    the flash firing (a pop and a low thump), then the film wound on (a ratchet). Small room."""
    rng = np.random.default_rng(61)
    n = int(2.4 * SR)
    t = t_of(n)
    out = np.zeros(n)
    k0, k1 = 0.0, 0.95
    m = (t >= k0) & (t < k1)
    q = (t[m] - k0) / (k1 - k0)
    f = 1800 * (9000 / 1800) ** (q ** 0.8)
    whine = np.sin(2 * np.pi * np.cumsum(f) / SR) * (0.15 + 0.2 * q) * np.clip(q / 0.08, 0, 1) * (1 + 0.15 * np.sin(2 * np.pi * 37 * t[m]))
    whine *= np.clip((k1 - t[m]) / 0.012, 0, 1)            # charged: the whine stops (without a click)
    out[m] += whine

    def click(at, f_lo, f_hi, dur, g):
        i, L = int(at * SR), int(dur * SR)
        b = bfilt(rng.standard_normal(L), 'bandpass', (f_lo, f_hi), 2) * np.exp(-np.arange(L) / (L / 4))
        out[i:i + L] += b * g

    click(1.0, 2000, 7000, 0.004, 0.9)                    # first curtain
    click(1.004, 900, 1800, 0.03, 0.6)                    # the mirror's clack
    click(1.04, 2500, 8000, 0.004, 0.7)                   # second curtain
    i = int(1.0 * SR)
    L = int(0.3 * SR)
    tl = t_of(L)
    pop = rng.standard_normal(L) * np.exp(-tl / 0.012) * 0.8 + np.sin(2 * np.pi * np.cumsum(50 + 60 * np.exp(-tl / 0.03)) / SR) * np.exp(-tl / 0.07) * 0.9
    out[i:i + L] += pop
    for k in range(9):                                    # winding on: a ratchet, a little uneven
        click(1.32 + k * 0.045 + rng.uniform(-0.004, 0.004), 1500, 6000, 0.006, 0.35 + 0.1 * (k % 2))
    out = verb(pan2(out, 0.1), hall_ir(0.9, 0.008, seed=62, dark=0.4, lo=200, bright=9000), 0.14)
    return finish(fade_in(tail_trim(out)), -16.0)


CASTLE = 'Halloween/Murder at Cameron Castle/HW_FX_'
KEY = dict(key='A minor', camelot='8A')
FX = [
    ('mc_fx_gong', gong, 'Dinner Gong', 'a brass gong on A struck three times on the beat, castle hall: dinner is served', 'gong',
     CASTLE + 'DinnerGong_A.wav', dict(key='A', castle=True)),
    ('mc_fx_reveal', reveal, 'Murder Reveal Sting', 'dun, dun, DUN: low brass and timpani, then a held A diminished chord with tremolo strings', 'sting',
     CASTLE + 'MurderRevealSting_A.wav', dict(key='A', castle=True)),
    ('mc_fx_flash', camera_flash, 'Camera Flash', "Flash McGillicuddy's camera: the flash charging, the shutter, the pop, the film wound on", 'camera',
     CASTLE + 'CameraFlash.wav', dict(castle=True, zf=True)),
    ('hw_fx_thunder', thunder, 'Thunderclap', 'a near crack, a thump and a rumble rolling away for six seconds', 'thunder', ONE + 'Thunderclap.wav', {}),
    ('hw_fx_bell', bell, 'Midnight Bell', 'church bell struck on A: hum, minor-third tierce and nominal, slow beating, stone hall', 'bell',
     ONE + 'MidnightBell_A.wav', dict(key='A')),
    ('hw_fx_organ', organ, 'Haunted Organ Stab', 'pipe organ, A minor in octaves, 16-foot to mixture, tremulant, cathedral tail', 'organ',
     ONE + 'HauntedOrganStab_Amin-8A.wav', dict(KEY)),
    ('hw_fx_door', door, 'Creaking Door', 'slow groan, a hinge squeal, settling back; small old room', 'door', ONE + 'CreakingDoor.wav', {}),
    ('hw_fx_howl', howl, 'Wolf Howl', "a howl rising to a held cry, a second wolf answering far off, echoes off the hills", 'howl',
     ONE + 'WolfHowl.wav', {}),
    ('hw_fx_stinger', stinger, 'Horror Stinger', 'screeching violin cluster swelling over a timpani and low brass hit', 'stinger',
     ONE + 'HorrorStinger.wav', {}),
    ('hw_fx_chains', chains, 'Rattling Chains', 'three shakes and drags of iron links, dungeon room', 'chains', ONE + 'RattlingChains.wav', {}),
    ('hw_fx_bats', bats, 'Bat Swarm', 'seven bats flapping and squeaking past, left and right', 'bats', ONE + 'BatSwarm.wav', {}),
    ('hw_fx_whoosh', whoosh, 'Ghost Whoosh', 'a moaning ghost sweeps left to right, breathing in on a reversed reverb', 'whoosh',
     ONE + 'GhostWhoosh.wav', {}),
    ('hw_fx_theremin', theremin, 'Theremin Melody', 'sine theremin with portamento and delayed vibrato, slap echo, 4-bar loop', 'theremin',
     LOOP + 'ThereminMelody-4bars_Amin-8A_128bpm.wav', dict(KEY, bpm=128, bars=4, loop=True)),
    ('hw_fx_musicbox', musicbox, 'Music Box Lullaby', 'comb tines with tape wow, a slightly uneven mechanism, 4-bar loop', 'musicbox',
     LOOP + 'MusicBoxLullaby-4bars_Amin-8A_128bpm.wav', dict(KEY, bpm=128, bars=4, loop=True)),
    ('hw_fx_choir', choir, 'Ghost Choir', "four-part 'oo' choir, three voices a part, leaning on the flat sixth, long hall, 4-bar loop", 'choir',
     LOOP + 'GhostChoir-4bars_Amin-8A_128bpm.wav', dict(KEY, bpm=128, bars=4, loop=True)),
    ('hw_fx_heartbeat', heartbeat, 'Racing Heartbeat', 'lub-dub on every beat at 128, saturated so it reads on small speakers, 2-bar loop', 'heartbeat',
     LOOP + 'RacingHeartbeat-2bars_128bpm.wav', dict(bpm=128, bars=2, loop=True)),
    ('hw_fx_cauldron', cauldron, 'Cauldron Bubbles', 'bubbles rising through a thick brew over a low simmer, 2-bar loop', 'cauldron',
     LOOP + 'CauldronBubbles-2bars_128bpm.wav', dict(bpm=128, bars=2, loop=True)),
]


if __name__ == '__main__':
    only = set(sys.argv[1:])
    t0 = time.time()
    for fid, fn, title, note, kind, pack, kw in FX:
        if only and fid not in only:
            continue
        t1 = time.time()
        x = fn()
        save(fid, x, title, note, kind, pack, **kw)
        log(f'   ({time.time() - t1:.0f}s)')
    open(f'{W}/out/render_halloween_fx.log', 'a').write('\n'.join(LOG) + '\n')
    print('done in', round(time.time() - t0), 's')
