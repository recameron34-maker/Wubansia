"""Reference vectors for the engine's mix and effects (P8, ADR 0021): what engine/tests/test_fx.cpp and
test_master.cpp hold each C++ block to, at 48 kHz, and the two tap tables engine/fx/generated/ compiles in.

    python3 tools/sound/export_engine_vectors.py            # writes engine/tests/vectors/fx/ and the headers
    python3 tools/sound/export_engine_vectors.py --check    # regenerates in memory; fails on any difference

Every vector comes from the Python the C++ ports: dsp48 (filters, os_fir(4), tanh_adaa, svf_tpt, mono_below),
master.py (soft_clip, the glue's detector and envelopes at a fixed threshold, the two-sided limiter), sends.py (space,
echo, resonator, duck), phrase.pump_db, and rt_ref.py's real-time stages (the half-band lane clip and drive, the
look-back limiter). The loop-based references (master.py's and sends.duck's) run on the signal followed by enough
silence that going round the loop equals starting from rest, which is how the engine starts.

Inputs are generated, not stored: music() is a splitmix64 stream turned into brown noise and noise bursts with only
+, -, * and /, so the C++ test's copy (engine/tests/fx_vectors.h) makes the same float32 samples bit for bit. The
outputs are stored as raw little-endian float32 (stereo interleaved L, R), some decimated (every k-th sample) to
keep the fixtures small, and pinned by vectors.sha256 (sha256sum's format), which the C++ test checks first.

rt_ref.py (P3) is the reference engine_ref.py builds the whole mix from. This script keeps its own copies of the
two rt_ref stages it needs (the half-band cascade, the look-back limiter), so the vectors don't depend on whether
rt_ref is importable; when it is, --check also proves the copies equal it.
"""
import argparse
import hashlib
import math
import pathlib
import sys

import numpy as np
from scipy import ndimage, signal

from tools.sound import dsp48 as D
from tools.sound import master as MS
from tools.sound import phrase as Ph
from tools.sound import sends as S

try:
    from tools.sound import rt_ref as R
except ImportError:          # rt_ref.py is P3's; the copies below stand in until both branches meet
    R = None

SR = D.SR
ROOT = pathlib.Path(__file__).resolve().parents[2]
OUT = ROOT / 'engine' / 'tests' / 'vectors' / 'fx'
GENERATED = ROOT / 'engine' / 'fx' / 'generated'
MASK = (1 << 64) - 1


# ------------------------------------------------------------------ inputs (engine/tests/fx_vectors.h has the C++ twin)
class SplitMix64:
    """splitmix64: the same 64-bit stream in Python and C++."""

    def __init__(self, seed):
        self.s = seed & MASK

    def next(self):
        self.s = (self.s + 0x9E3779B97F4A7C15) & MASK
        z = self.s
        z = ((z ^ (z >> 30)) * 0xBF58476D1CE4E5B9) & MASK
        z = ((z ^ (z >> 27)) * 0x94D049BB133111EB) & MASK
        return z ^ (z >> 31)

    def uniform(self):
        """[0, 1) with 53 bits."""
        return (self.next() >> 11) * (1.0 / (1 << 53))


def music(n, seed, bursts=(), peak=0.5, brown=1.0, channels=2, silent=0):
    """A test signal, float32 (n, channels): brown noise (a leaky sum of white noise, strong under 40 Hz) times
    `brown`, plus noise bursts (start, length, amplitude) that fall linearly to 0, scaled so its peak is `peak`. The
    first `silent` samples are 0. The random numbers are drawn sample by sample, channel by channel, the brown noise's
    first, then each burst's in turn."""
    rng = SplitMix64(seed)
    x = np.zeros((n, channels))
    b = [0.0] * channels
    for i in range(n):
        for c in range(channels):
            w = 2.0 * rng.uniform() - 1.0
            b[c] = 0.995 * b[c] + 0.05 * w
            x[i, c] = brown * b[c]
    for start, length, amp in bursts:
        for i in range(start, min(n, start + length)):
            e = amp * (1.0 - (i - start) / length)
            for c in range(channels):
                x[i, c] += e * (2.0 * rng.uniform() - 1.0)
    x[:silent] = 0.0
    scale = peak / float(np.abs(x).max())
    return (x * scale).astype(np.float32)


def f64(x):
    return np.asarray(x, dtype=np.float64)


# the shared inputs (the C++ test builds the same)
def input_a():
    """1024 stereo samples, -6 dBFS: brown noise and three bursts."""
    return music(1024, 1, ((0, 300, 0.8), (400, 200, 1.0), (700, 300, 0.6)), peak=0.5)


def input_b():
    """2048 stereo samples, -6 dBFS (the SVF's)."""
    return music(2048, 2, ((100, 500, 1.0), (1200, 400, 0.7)), peak=0.5)


# ------------------------------------------------------------------ the references the engine has no Python twin of
def rbj(kind, fc, gain_db, q, fs=SR):
    """RBJ cookbook biquads (b, a), a[0] = 1: 'lowpass', 'highpass' (gain unused), 'peak' (dsp48.peaking's),
    'low_shelf', 'high_shelf' (the shelf slope set by Q)."""
    A = 10.0 ** (gain_db / 40.0)
    w0 = 2.0 * math.pi * fc / fs
    cw, al = math.cos(w0), math.sin(w0) / (2.0 * q)
    sa = 2.0 * math.sqrt(A) * al
    if kind == 'lowpass':
        b, a = [(1 - cw) / 2, 1 - cw, (1 - cw) / 2], [1 + al, -2 * cw, 1 - al]
    elif kind == 'highpass':
        b, a = [(1 + cw) / 2, -(1 + cw), (1 + cw) / 2], [1 + al, -2 * cw, 1 - al]
    elif kind == 'peak':
        b, a = [1 + al * A, -2 * cw, 1 - al * A], [1 + al / A, -2 * cw, 1 - al / A]
    elif kind == 'low_shelf':
        b = [A * ((A + 1) - (A - 1) * cw + sa), 2 * A * ((A - 1) - (A + 1) * cw), A * ((A + 1) - (A - 1) * cw - sa)]
        a = [(A + 1) + (A - 1) * cw + sa, -2 * ((A - 1) + (A + 1) * cw), (A + 1) + (A - 1) * cw - sa]
    elif kind == 'high_shelf':
        b = [A * ((A + 1) + (A - 1) * cw + sa), -2 * A * ((A - 1) + (A + 1) * cw), A * ((A + 1) + (A - 1) * cw - sa)]
        a = [(A + 1) - (A - 1) * cw + sa, 2 * ((A - 1) - (A + 1) * cw), (A + 1) - (A - 1) * cw - sa]
    else:
        raise ValueError(kind)
    b, a = np.array(b), np.array(a)
    return b / a[0], a / a[0]


def band_split(x, lo_hz, hi_hz):
    """(rest, band): rt_ref.band_split, LR4 crossovers; rest + band is an all-pass of x."""
    if not lo_hz and not hi_hz:
        return np.zeros_like(x), x
    if not hi_hz:
        return D.lr4(x, lo_hz, 'lowpass'), D.lr4(x, lo_hz, 'highpass')
    if not lo_hz:
        return D.lr4(x, hi_hz, 'highpass'), D.lr4(x, hi_hz, 'lowpass')
    low, up = D.lr4(x, lo_hz, 'lowpass'), D.lr4(x, lo_hz, 'highpass')
    band, top = D.lr4(up, hi_hz, 'lowpass'), D.lr4(up, hi_hz, 'highpass')
    return D.lr4(low, hi_hz, 'lowpass') + D.lr4(low, hi_hz, 'highpass') + top, band


def band_part(x, lo_hz, hi_hz):
    """rt_ref.band_part: the LR4 band a duck works in, an LR4 high-pass at lo_hz then a low-pass at hi_hz (each only
    if set)."""
    x = f64(x)
    if lo_hz:
        x = D.lr4(x, lo_hz, 'highpass')
    if hi_hz:
        x = D.lr4(x, hi_hz, 'lowpass')
    return x


def band_duck(x, gain_db, lo_hz, hi_hz):
    """rt_ref.band_duck: a duck's gain (dB per sample) on its band, added to the signal, x + (g - 1) band (x), so at
    rest the signal passes untouched; the whole band is x g."""
    x = f64(x)
    g = D.undb(gain_db)
    g = g[:, None] if x.ndim == 2 else g
    if not lo_hz and not hi_hz:
        return x * g
    return x + (g - 1.0) * band_part(x, lo_hz, hi_hz)


def widen(x, fc, amount, order=4):
    """The width insert (context4.widen): the side over fc (a Butterworth high-pass) scaled by amount."""
    m, s = 0.5 * (x[:, 0] + x[:, 1]), 0.5 * (x[:, 0] - x[:, 1])
    s = s + (amount - 1.0) * D.bfilt(s, fc, 'highpass', order=order)
    return np.stack([m + s, m - s], 1)


# rt_ref's half-band cascade (P3 holds the original; --check proves these equal it)
def halfband(N, beta):            # N = 4k + 3
    c = (N - 1) // 2
    n = np.arange(N) - c
    h = np.zeros(N)
    w = np.kaiser(N, beta)
    odd = n % 2 != 0
    h[odd] = np.sin(np.pi * n[odd] / 2) / (np.pi * n[odd]) * w[odd]
    h[odd] *= 0.5 / h[odd].sum()
    h[c] = 0.5
    return h


HB1, HB2 = halfband(47, 7.0), halfband(15, 7.0)


def _up2(x, h):
    c = (len(h) - 1) // 2
    y = np.zeros((2 * len(x),) + x.shape[1:])
    y[0::2] = x
    return signal.lfilter(2 * h, [1.0], np.concatenate([y, np.zeros((c,) + y.shape[1:])]), axis=0)[c:c + len(y)]


def _down2(y, h):
    c = (len(h) - 1) // 2
    return signal.lfilter(h, [1.0], np.concatenate([y, np.zeros((c,) + y.shape[1:])]), axis=0)[c:c + len(y)][0::2]


def hb_up4(x):
    return _up2(_up2(f64(x), HB1), HB2)


def hb_down4(u):
    return _down2(_down2(u, HB2), HB1)


def clip_curves(ceiling_db, knee_db=1.5):
    """master.soft_clip's excess over the knee and its antiderivative."""
    C = float(D.undb(ceiling_db))
    T = C * float(D.undb(-knee_db))
    k = C - T

    def excess(u):
        a = np.abs(u)
        return np.where(a <= T, 0.0, np.sign(u) * (T + k * np.tanh((a - T) / k)) - u)

    def excess_int(u):
        a = np.abs(u)
        v = np.maximum((a - T) / k, 0.0)
        logcosh = v + np.log1p(np.exp(-2 * v)) - math.log(2.0)
        return np.where(a <= T, 0.0, T * a + k * k * logcosh - 0.5 * a * a - 0.5 * T * T)

    return excess, excess_int


def lane_soft_clip(x, ceiling_db, knee_db=1.5):
    excess, excess_int = clip_curves(ceiling_db, knee_db)
    u = hb_up4(x)
    return hb_down4(u + D._adaa(u, excess, excess_int))


def lane_drive(x, drive, bias=0.0, output_db=0.0):
    return hb_down4(D.tanh_adaa(hb_up4(x), drive, bias)) * float(D.undb(output_db))


# rt_ref's limiter on a stream (P3's; --check proves these equal it)
def _sliding_min(a, before, after, fill=1.0):
    size = before + after + 1
    pad = np.concatenate([np.full(before, fill), a, np.full(after, fill)])
    m = ndimage.minimum_filter1d(pad, size=size, mode='nearest')
    return m[size // 2: size // 2 + len(a)]


def _mean_back(a, w, fill=1.0):
    c = np.concatenate([[0.0], np.cumsum(np.concatenate([np.full(w, fill), a]))])
    k = np.arange(len(a))
    return (c[k + w + 1] - c[k]) / (w + 1)


def _release(r2, release_s):
    out = np.empty(len(r2))
    a = math.exp(-1.0 / (release_s * SR))
    r = 0.0
    for i, v in enumerate(r2.tolist()):
        r = max(v, r * a)
        out[i] = r
    return np.where(out < 1e-9, 0.0, out)


TP_REACH = MS.TP_REACH            # 24: os_fir(4)'s reach each way at 1x


def limit_db(x, ceiling_db, lookahead_s=0.005, hold_s=0.060, release_s=0.080, hold='lookback'):
    """The true-peak limiter's gain reduction (dB) on a stream: master.limit_gain_db's need, hold, ramp and release,
    the hold reaching both ways ('two_sided', master.limit) or looking back ('lookback'), whose minimum then runs from
    twice the hold back to the lookahead plus the true-peak FIR's 24 samples ahead: the gain is flat across the FIR
    round each peak, and the window is as long as the two-sided one, so a held low note doesn't step it."""
    x = D.st(f64(x))
    n = len(x)
    c = float(D.undb(ceiling_db))
    p = np.abs(D.up(x, 4)).max(1)
    pp = np.concatenate([np.zeros(4), p, np.zeros(4)])
    idx = 4 * np.arange(n) + 4
    pk = np.max(np.stack([pp[idx + d] for d in range(-4, 5)]), 0)
    need = np.minimum(1.0, c / (pk + 1e-30))
    w = 2 * int(round(lookahead_s * SR / 2))
    h = int(round(hold_s * SR))
    # ADR 0021: looking back, the window reaches the TP FIR's 24 past the lookahead, and twice the hold back
    before, after = (h, w + h) if hold == 'two_sided' else (2 * h, w + TP_REACH)
    g = _mean_back(_sliding_min(need, before, after), w)
    return _release(-20 * np.log10(np.minimum(g, 1.0)), release_s)


def padded(fn, x, pad=128):
    """fn on x with `pad` samples of silence either side, x's span of the result. rt_ref's centred stages each cut their
    output to the input's span, which drops what an upsampler rings before the first sample and after the last; a
    stream (and the engine) keeps it, so a stage's reference runs on silence either side."""
    z = np.zeros((pad,) + x.shape[1:])
    return fn(np.concatenate([z, x, z]))[pad:pad + len(x)]


# ------------------------------------------------------------------ the master.py references, from rest
def glue_reduction_db(x, threshold_db, ratio, knee_db, attack_s, release_s):
    """master.glue's gain reduction per sample at a fixed threshold (its _detector_db, _reduction_db and _per_sample
    round a loop that ends in 30 s of silence, so it starts from rest)."""
    xx = np.concatenate([D.st(f64(x)), np.zeros((30 * SR, 2))])
    level, lens = MS._detector_db(xx)
    r = MS._reduction_db(level, lens, threshold_db, ratio, knee_db, attack_s, release_s, len(xx))
    return MS._per_sample(r, len(xx))[:len(x)]


def master_limit_db(x, ceiling_db):
    """master.limit_gain_db (two-sided) round a loop with a second of silence either side: from rest."""
    xx = np.concatenate([np.zeros((SR, 2)), D.st(f64(x)), np.zeros((SR, 2))])
    return MS.limit_gain_db(xx, ceiling_db)[SR:SR + len(x)]


def master_soft_clip(x, ceiling_db, knee_db):
    """master.soft_clip (4x, os_fir(4)) round a loop padded with silence either side: a stream."""
    pad = 2 * MS.CLIP_PAD
    xx = np.concatenate([np.zeros((pad, 2)), D.st(f64(x)), np.zeros((pad, 2))])
    return MS.soft_clip(xx, ceiling_db, knee_db, 4)[0][pad:pad + len(x)]


# ------------------------------------------------------------------ the vectors
class FakeRng:
    """Stands in for a numpy Generator where a reference draws values the manifest stores (the echo's phases)."""

    def __init__(self, values):
        self.values = np.asarray(values, dtype=float)

    def uniform(self, lo, hi, n):
        assert n == len(self.values)
        return self.values.copy()


# the parameters below are mirrored in engine/tests/test_fx.cpp and test_master.cpp
SPACE = dict(t60=2.6, size=0.45, damp_hz=8500.0, predelay=0.002, send_hp=500.0, send_order=4, low_hz=500.0,
             t60_low=0.4, shimmer=0.06, shimmer_hp=300.0, shimmer_lp=7000.0, window=1024, mono_hz=150.0)
ECHO_DELAY_S = 0.03125           # 3 16ths at 1440 BPM: 1500 samples
ECHO = dict(feedback=0.38, lp_hz=2600.0, hp_hz=300.0, drive=1.5, wow=0.0004, wow_hz=0.5, flutter=0.00015,
            flutter_hz=4.7)
ECHO_PHASES = (1.234, 5.678)
RESONATOR_MHZ = (739989, 1108731, 1318510, 1661219, 1760000)
GLUE = dict(threshold_db=-20.0, ratio=2.0, knee_db=6.0, attack_s=0.030, release_s=(0.15, 1.5))
DUCK = dict(depth_db=6.0, range_db=18.0, attack=0.010, release=0.300)
DUCK_TOP_DB = -12.0
PUMP = dict(depth_db=6.0, ramp=0.002, hold=0.060, release=0.080)
PUMP_ONSETS = (1000, 9000, 9500, 17000)
LIFT = dict(depth_db=-3.0, ramp=0.0, hold=0.012, release=0.013)
LIFT_ONSETS = (500, 3000)
BAND_DUCK_ONSETS = (100, 600)
LIMIT_CEILING_DB = -1.02


def band_duck_gain(n):
    """The band duck vector's gain (dB): pump_db's shape at BAND_DUCK_ONSETS on a stream (a loop long enough not to
    wrap), its first n samples."""
    return Ph.pump_db(BAND_DUCK_ONSETS, SR, **PUMP)[:n]


def space_draws(seed_key, size):
    """The delays and input signs sends.space draws from D.rng(*seed_key), in its order."""
    r = D.rng(*seed_key)
    d = np.round(np.array(S.FDN_DELAYS) * size * (1 + 0.03 * r.uniform(-1, 1, 8))).astype(int)
    return d, r.choice([-1, 1], 8)


def vectors():
    """{name: float32 array}, in a fixed order. Stereo arrays are (n, 2) and stored interleaved."""
    with np.errstate(divide='ignore', invalid='ignore'):     # pump_db's ramp 0 divides by it where it isn't used
        return _vectors()


def _vectors():
    a = f64(input_a())
    a0 = a[:, 0]
    v = {}
    # Butterworth (dsp48.bfilt: scipy's butter, sosfilt), mono, 1024 samples of input A's left channel
    v['bw_hp4_25'] = D.bfilt(a0, 25.0, 'highpass', order=4)
    v['bw_hp2_5'] = D.bfilt(a0, 5.0, 'highpass', order=2)              # the master's causal DC blocker
    v['bw_lp2_2600'] = D.bfilt(a0, 2600.0, 'lowpass', order=2)
    v['bw_hp8_150'] = D.bfilt(a0, 150.0, 'highpass', order=8)
    v['bw_hp3_450'] = D.bfilt(a0, 450.0, 'highpass', order=3)
    v['bw_lp1_9000'] = D.bfilt(a0, 9000.0, 'lowpass', order=1)
    v['bw_bp4_300_7000'] = D.bfilt(a0, (300.0, 7000.0), 'bandpass', order=4)
    # RBJ cookbook biquads (dsp48.peaking's peak; the rest from the cookbook), mono
    v['rbj_peak_1000'] = D.peaking(a0, 1000.0, -3.0, 1.2)
    v['rbj_peak_16000'] = D.peaking(a0, 16000.0, -2.5, 0.8)
    for name, kind, fc, g, q in (('rbj_lowshelf_200', 'low_shelf', 200.0, 4.0, 0.707),
                                 ('rbj_highshelf_8000', 'high_shelf', 8000.0, -3.0, 0.707),
                                 ('rbj_lp_1200', 'lowpass', 1200.0, 0.0, 2.0),
                                 ('rbj_hp_300', 'highpass', 300.0, 0.0, 0.9)):
        b, aa = rbj(kind, fc, g, q)
        v[name] = signal.lfilter(b, aa, a0)
    # LR4 and the insert band split, mono
    v['lr4_lo_1200'] = D.lr4(a0, 1200.0, 'lowpass')
    v['lr4_hi_1200'] = D.lr4(a0, 1200.0, 'highpass')
    rest, band = band_split(a0, 1200.0, 6500.0)
    v['band_rest_1200_6500'] = rest
    v['band_band_1200_6500'] = band
    # stereo: mono_below, width
    v['mono_below_150'] = D.mono_below(a, 150.0, 8)
    v['width_300_1600'] = widen(a, 300.0, 1.6, 4)
    # the TPT SVF with a per-sample cutoff: 100 Hz to 10 kHz exponentially over 2048 samples, Q 2, input B's left
    b0 = f64(input_b())[:, 0]
    n = len(b0)
    fc = 100.0 * 100.0 ** (np.arange(n) / (n - 1))
    lp, bp, hp = D.svf_tpt(b0, fc, q=2.0)
    v['svf_lp'], v['svf_bp'], v['svf_hp'] = lp, bp, hp
    # os_fir(4): up 256 samples of A's left to 1024; down 1024 samples of A's right (as a 4x stream) to 256
    v['os_up'] = D.up(a0[:256], 4)
    v['os_down'] = D.down(a[:, 1], 4)
    # nonlinear stages: master.soft_clip (os_fir(4)), tanh_adaa at 1x, the half-band lane clip and drive
    v['soft_clip_master'] = master_soft_clip(a * 3.0, -1.0, 1.5)
    v['tanh_adaa'] = D.tanh_adaa(a0 * 2.0, 2.5, 0.2)
    v['lane_clip'] = padded(lambda u: lane_soft_clip(u, -1.0, 1.5), a * 3.0)
    v['lane_drive'] = padded(lambda u: lane_drive(u, 2.5, 0.2, -1.5), a0 * 1.5)
    # the glue at a fixed threshold: 1 s of input C; its reduction (dB) at 16 i + 8, and 512 stereo samples of output
    c = f64(music(SR, 3, ((2400, 2400, 2.0), (12000, 6000, 1.5), (30000, 1200, 2.5)), peak=0.9))
    gr = glue_reduction_db(c, **GLUE)
    v['glue_gr'] = gr[8::16]
    v['glue_out'] = (c * D.undb(-gr)[:, None])[24000:24512]
    # the limiter, two-sided (master.limit_gain_db) and looking back (rt_ref.limit_db): 0.5 s of input D peaking
    # at +6 dBFS, from silence (master.py's loop, rt_ref's stream after a second of it: either way the ramp before
    # the first sample already sees what follows); the reduction (dB) every 16th sample, 512 stereo samples of output
    d = f64(music(SR // 2, 4, ((1000, 800, 3.0), (6000, 300, 4.0), (15000, 3000, 2.0)), peak=2.0))
    r2 = master_limit_db(d, LIMIT_CEILING_DB)
    rl = padded(lambda u: limit_db(u, LIMIT_CEILING_DB, hold='lookback'), d, SR)
    v['limit2_gr'] = r2[::16]
    v['limit2_out'] = (d * D.undb(-r2)[:, None])[5800:6312]
    v['limitlb_gr'] = rl[::16]
    v['limitlb_out'] = (d * D.undb(-rl)[:, None])[5800:6312]
    # the level duck (sends.duck, its top the key's loudest block, set to DUCK_TOP_DB): the gain (dB) every 16th
    # sample of 0.6 s; the engine's is the same 96 samples later (rt_ref.LEVEL_LAG)
    key = level_key()
    _, g = S.duck(np.ones(len(key)), key, **DUCK)
    v['duck_level_db'] = g[::16]
    # phrase.pump_db, every 8th sample of 0.5 s; and a lift, every 4th of 6000 samples. pump_db takes the minimum
    # with 0 dB, so it can't lift: a lift is -depth times its envelope (rt_ref.onset_db), the envelope being pump_db
    # at a depth of -1 dB (the largest of the overlapping shapes)
    v['pump_db'] = Ph.pump_db(PUMP_ONSETS, SR // 2, **PUMP)[::8]
    # a duck on a band (rt_ref.band_duck): input A under pump_db's shape at onsets 100 and 600, the band 120-2500 Hz
    v['band_duck_120_2500'] = band_duck(a, band_duck_gain(len(a)), 120.0, 2500.0)
    envelope = -Ph.pump_db(LIFT_ONSETS, 6000, 1.0, LIFT['ramp'], LIFT['hold'], LIFT['release'])
    v['pump_lift_db'] = (-LIFT['depth_db'] * envelope)[::4]
    # the returns, on a mono burst: Space (8192 samples, every 4th, stereo), Echo (7200, every 4th, stereo),
    # Resonator (4096, every 2nd, stereo)
    e = f64(music(8192, 7, ((0, 400, 1.0),), peak=0.5, brown=0.0, channels=1))[:, 0]
    v['space'] = S.space(e, D.rng('P8', 'space'), **SPACE)[::4]
    f = f64(music(7200, 8, ((0, 300, 1.0),), peak=0.5, brown=0.0, channels=1))[:, 0]
    v['echo'] = S.echo(f, FakeRng(ECHO_PHASES), ECHO_DELAY_S, **ECHO)[::4]
    r = np.concatenate([a0, np.zeros(3072)])
    v['resonator'] = S.resonator(r, [m / 1000.0 for m in RESONATOR_MHZ], tau=0.4, send_hp=600.0)[::2]
    return {k: np.asarray(x, dtype=np.float64).astype(np.float32) for k, x in v.items()}


def level_key():
    """The level duck's key: 0.6 s of input E, its first 3 blocks silent (so sends.duck starts from silence, as the
    engine does), scaled so its loudest 64-sample block is exactly DUCK_TOP_DB."""
    k = f64(music(28800, 6, ((3000, 2000, 2.0), (14000, 4000, 1.0)), peak=0.5, silent=192))
    m = D.mono(k)
    nb = -(-len(m) // 64)
    p = np.concatenate([m ** 2, np.zeros(nb * 64 - len(m))]).reshape(nb, 64).mean(1)
    top = 10 * np.log10(p.max() + 1e-20)
    return k * 10.0 ** ((DUCK_TOP_DB - top) / 20.0)


# ------------------------------------------------------------------ the generated headers
def header(about, tables):
    """A C++ header holding tap tables: (identifier, taps, comment) each, every tap as its exact double (repr)."""
    lines = ['#pragma once', '', '// GENERATED by tools/sound/export_engine_vectors.py: do not edit.']
    lines += [f'// {line}' for line in about]
    lines += ['', '#include <array>', '', 'namespace wubansia::engine::fx', '{']
    for i, (ident, taps, comment) in enumerate(tables):
        if i:
            lines.append('')
        lines.append(f'    // {comment}')
        lines.append(f'    inline constexpr std::array<double, {len(taps)}> {ident} {{')
        lines += [f'        {float(t)!r},' for t in taps]
        lines.append('    };')
    lines.append('}')
    return '\n'.join(lines) + '\n'


def headers():
    return {
        'os_fir4.h': header(("dsp48.os_fir(4): scipy.signal.firwin(193, 24000, width=8000, fs=192000), the taps of the "
                             "club master's",
                             "4x oversampling and of the limiter's true-peak detector. Each tap is the exact double "
                             "(Python's repr)."),
                            [('kOsFir4', D.os_fir(4), '193 taps, symmetric, centre 96, sum 1')]),
        'halfband.h': header(("rt_ref.py's half-band cascade (halfband (N, beta), ADR 0021): the 4x oversampling of "
                              "the clip and drive",
                              "inserts. Each tap is the exact double (Python's repr); the even taps but the centre "
                              "are 0."),
                             [('kHalfband1', HB1, 'HB1 = halfband (47, 7.0): 1x <-> 2x, centre 23'),
                              ('kHalfband2', HB2, 'HB2 = halfband (15, 7.0): 2x <-> 4x, centre 7')]),
    }


# ------------------------------------------------------------------ rt_ref cross-check
def cross_check():
    """When rt_ref.py is importable: the copies here equal it."""
    if R is None:
        return 'rt_ref.py not importable here: the local copies were not cross-checked'
    worst = []
    for mine, theirs in ((HB1, R.HB1), (HB2, R.HB2)):
        worst.append(float(np.max(np.abs(mine - theirs))))
    a = f64(input_a())
    worst.append(float(np.max(np.abs(lane_soft_clip(a * 3.0, -1.0, 1.5) - R.lane_soft_clip(a * 3.0, -1.0, 1.5)))))
    worst.append(float(np.max(np.abs(lane_drive(a[:, 0] * 1.5, 2.5, 0.2, -1.5)
                                     - R.lane_drive(a[:, 0] * 1.5, 2.5, 0.2, -1.5)))))
    d = f64(music(SR // 2, 4, ((1000, 800, 3.0), (6000, 300, 4.0), (15000, 3000, 2.0)), peak=2.0))
    for hold in ('lookback', 'two_sided'):
        worst.append(float(np.max(np.abs(limit_db(d, LIMIT_CEILING_DB, hold=hold)
                                         - R.limit_db(d, LIMIT_CEILING_DB, hold=hold)))))
    if hasattr(R, 'band_duck'):
        g = band_duck_gain(len(a))
        worst.append(float(np.max(np.abs(band_duck(a, g, 120.0, 2500.0) - R.band_duck(a, g, 120.0, 2500.0)))))
    if max(worst) > 1e-9:
        raise SystemExit(f'export_engine_vectors: the local copies differ from rt_ref.py by {max(worst):.3g}')
    return f'rt_ref.py cross-checked: the copies agree within {max(worst):.3g}'


# ------------------------------------------------------------------ main
def files():
    out = {}
    for name, x in vectors().items():
        out[OUT / f'{name}.f32'] = x.astype('<f4').tobytes()
    for name, text in headers().items():
        out[GENERATED / name] = text.encode('utf-8')
    manifest = ''.join(f'{hashlib.sha256(data).hexdigest()}  {path.name}\n'
                       for path, data in sorted(out.items()) if path.suffix == '.f32')
    out[OUT / 'vectors.sha256'] = manifest.encode('utf-8')
    return out


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--check', action='store_true', help='regenerate in memory and compare with the files')
    args = ap.parse_args(argv)
    out = files()
    print(cross_check())
    if args.check:
        bad = [str(p.relative_to(ROOT)) for p, data in out.items() if not p.exists() or p.read_bytes() != data]
        extra = sorted(str(p.relative_to(ROOT)) for p in OUT.glob('*') if p not in out)
        if bad or extra:
            print('export_engine_vectors: out of date:', ', '.join(bad + extra), file=sys.stderr)
            return 1
        print(f'export_engine_vectors: {len(out)} files up to date')
        return 0
    OUT.mkdir(parents=True, exist_ok=True)
    GENERATED.mkdir(parents=True, exist_ok=True)
    for p in OUT.glob('*.f32'):
        if p not in out:
            p.unlink()
    for p, data in out.items():
        p.write_bytes(data)
    total = sum(len(d) for p, d in out.items() if p.suffix == '.f32')
    print(f'export_engine_vectors: wrote {len(out)} files, vectors {total / 1024:.0f} KiB')
    return 0


if __name__ == '__main__':
    sys.exit(main())
