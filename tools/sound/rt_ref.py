"""Real-time references: the club master's and the mixer's stages as the engine runs them live (P8, ADR 0021 draft),
in numpy. master.py, sends.py and phrase.py hold the offline versions the round 5 loops were rendered with, run round
the loop; these are their streaming equivalents, and engine_ref.py builds the whole mix from them. The C++ blocks are
tested against vectors from these functions (export_engine_vectors.py).

Every function takes float64 audio at 48 kHz, (n,) or (n, 2), as a stream: it starts at sample 0 with silence before
it, and nothing wraps round a loop. Results are in aligned time: a stage with latency d (LATENCY) is causal when its
output is delayed by d samples, y_causal[k + d] = y_aligned[k], and the engine's graph delays the shorter paths to
match. A stage with latency 0 is causal as it stands.

- band_split: the LR4 band an insert works in, and the rest (an all-pass of the input when added back).
- onset_db: phrase.pump_db's envelope on a stream of key onsets: the deepest duck wins, a negative depth lifts.
- level_duck_db: sends.duck with a fixed top (the key's loudest block in the reference loop), its gain 1.5 blocks
  later than sends.duck's centred interpolation, which needs the next block.
- glue_db: master.glue at a fixed threshold (no solve): the same blocks, detector and envelopes, latency 23.
- soft_clip: master.soft_clip at 4x on os_fir(4)'s taps, latency 48 (24 each way): the club master's clip.
- lane_soft_clip, lane_drive: master.soft_clip's curve and dsp48.tanh_adaa at 4x on a half-band cascade (halfband,
  HB1 for 1x<->2x, HB2 for 2x<->4x; hb_up4, hb_down4), latency 27: every clip and drive insert (channels, groups,
  returns, bus EQ, parallel chains). P8 measured aliases at -94 dB, and -69 dB under 16 kHz against
  master.soft_clip on drum material, at about a fifth of os_fir(4)'s cost.
- limit_db: master.limit's gain on a stream, its 60 ms hold two-sided (master.py's, latency 3145) or looking back
  only (the real-time master's, latency 265).
- dc_block: the real-time master's causal DC blocker, in place of master.dc_clean's zero-phase one.
"""
import math

import numpy as np
from scipy import ndimage, signal

from tools.sound import dsp48 as D
from tools.sound import master as MS

SR = D.SR
OS_REACH = (len(D.os_fir(4)) - 1) // 8          # samples os_fir(4) reaches each way at 1x: 24
LATENCY = {
    'soft_clip': 2 * OS_REACH,                    # up and down through os_fir(4): the club master's clip
    'drive': 2 * OS_REACH,
    'lane_clip': 27,                              # the half-band cascade: (23 / 2 + 7 / 4) x 2 = 26.5, delayed 27
    'lane_drive': 27,
    'glue': MS.BLOCK + MS.BLOCK // 2 - 1,         # the gain between two block centres needs the next block: 23
    'limit_lookback': None,                       # limit_latency(): set below from the settings
}
LEVEL_BLOCK = 64                                  # sends.duck's RMS block at 48 kHz
LEVEL_LAG = LEVEL_BLOCK + LEVEL_BLOCK // 2        # level_duck_db's gain lags sends.duck's by this much: 96 samples
DC_ORDER = 2                                      # the causal DC blocker's order (Butterworth high-pass)
SILENCE_DB = -200.0                               # 10 log10(1e-20): the level of silence, as sends.duck reads it


def limit_latency(lookahead_s=0.005, hold_s=0.060, hold='lookback'):
    """How far ahead the limiter's gain looks, samples: the lookahead's ramp, the hold if it reaches forward
    (two-sided, master.limit_latency()), the sample either side the peak detector reads and the 4x FIR's reach. At
    the defaults: 265 looking back, 3145 two-sided."""
    w = 2 * int(round(lookahead_s * SR / 2))
    h = int(round(hold_s * SR)) if hold == 'two_sided' else 0
    return w + h + 1 + OS_REACH


LATENCY['limit_lookback'] = limit_latency()


# ------------------------------------------------------------------ bands
def band_split(x, lo_hz, hi_hz):
    """(rest, band): the band an insert works in, split off with LR4 crossovers (dsp48.lr4), and what passes it by.
    {lo, 0}: everything over lo; {0, hi}: everything under hi; {lo, hi}: the band between, the part under lo going
    through the upper crossover's all-pass so rest + band is an all-pass of x (flat). {0, 0}: the whole signal is the
    band. Causal."""
    x = np.asarray(x, dtype=float)
    if not lo_hz and not hi_hz:
        return np.zeros_like(x), x
    if not hi_hz:
        return D.lr4(x, lo_hz, 'lowpass'), D.lr4(x, lo_hz, 'highpass')
    if not lo_hz:
        return D.lr4(x, hi_hz, 'highpass'), D.lr4(x, hi_hz, 'lowpass')
    if not lo_hz < hi_hz:
        raise ValueError(f'band {lo_hz}-{hi_hz} Hz: lo must be under hi')
    low, up = D.lr4(x, lo_hz, 'lowpass'), D.lr4(x, lo_hz, 'highpass')
    band, top = D.lr4(up, hi_hz, 'lowpass'), D.lr4(up, hi_hz, 'highpass')
    rest = D.lr4(low, hi_hz, 'lowpass') + D.lr4(low, hi_hz, 'highpass') + top
    return rest, band


def in_band(fn, x, lo_hz, hi_hz):
    """fn on the band (band_split), the rest added back untouched."""
    rest, band = band_split(x, lo_hz, hi_hz)
    return rest + fn(band)


# ------------------------------------------------------------------ ducks
def onset_envelope(onsets, n, ramp_s, hold_s, release_s):
    """phrase.pump_db's shape, 0..1 per sample over a stream of n samples (no wrap): at each onset (a sample index,
    already moved earlier by any pre-roll) up over ramp_s (linear; 0: at once), held hold_s, then down over release_s
    on a raised cosine; overlapping onsets take the larger value. Onsets before the stream count from where they
    are."""
    e = np.zeros(n)
    r, h, rel = ramp_s * SR, hold_s * SR, release_s * SR
    span = int(math.ceil(r + h + rel)) + 1
    for p in sorted(int(o) for o in onsets):
        a, b = max(p, 0), min(n, p + span)
        if a >= b:
            continue
        tau = np.arange(a, b) - p                  # samples since the onset
        u = np.clip((tau - r - h) / rel, 0.0, 1.0)
        down = np.where(u < 1.0, 0.5 + 0.5 * np.cos(np.pi * u), 0.0)
        ee = np.where(tau < r, tau / r, down) if r > 0 else down
        e[a:b] = np.maximum(e[a:b], ee)
    return e


def onset_db(onsets, n, depth_db, ramp_s, hold_s, release_s):
    """An onset duck's gain in dB per sample (phrase.pump_db on a stream): down depth_db at full envelope; a
    negative depth lifts. Overlapping ducks take the deepest (the highest lift)."""
    return -depth_db * onset_envelope(onsets, n, ramp_s, hold_s, release_s)


def level_duck_db(key, depth_db, range_db, top_db, attack_s, release_s, block=LEVEL_BLOCK):
    """sends.duck, causal, with a fixed top: the key's level in RMS blocks of `block` samples, smoothed in dB (attack
    when rising, release when falling, per block, from silence), mapped from (top_db - range_db)..top_db onto
    0..depth_db of reduction. sends.duck reads its top from the whole key and interpolates its gain between block
    centres, which needs the next block; here the top is fixed (the reference loop's, exported) and the gain follows
    LEVEL_LAG samples later, from finished blocks only. Returns the gain in dB per sample (<= 0)."""
    k = D.mono(np.asarray(key, dtype=float))
    n = len(k)
    nb = -(-n // block)
    p = np.concatenate([k ** 2, np.zeros(nb * block - n)]).reshape(nb, block).mean(1)
    lv = 10 * np.log10(p + 1e-20)
    a_att, a_rel = math.exp(-block / (attack_s * SR)), math.exp(-block / (release_s * SR))
    s, sm = SILENCE_DB, np.empty(nb)
    for i, v in enumerate(lv.tolist()):
        c = a_att if v > s else a_rel
        s = c * s + (1 - c) * v
        sm[i] = s
    gr = -depth_db * np.clip((sm - (top_db - range_db)) / range_db, 0.0, 1.0)
    lag = block + block // 2
    return np.interp(np.arange(n) - lag, (np.arange(nb) + 0.5) * block - 0.5, gr, left=0.0)


def loudest_block_db(key, block=LEVEL_BLOCK):
    """The top a level duck exports: the key's loudest RMS block, dB, as sends.duck reads it over one loop."""
    k = D.mono(np.asarray(key, dtype=float))
    nb = -(-len(k) // block)
    p = np.concatenate([k ** 2, np.zeros(nb * block - len(k))]).reshape(nb, block).mean(1)
    return float(10 * np.log10(p.max() + 1e-20))


# ------------------------------------------------------------------ the glue (master.glue at a fixed threshold)
def glue_db(x, threshold_db, ratio=2.0, knee_db=6.0, attack_s=0.030, release_s=(0.15, 1.5)):
    """master.glue's gain reduction (dB, >= 0) per sample at a fixed threshold, on a stream: stereo-linked block power
    (MS.BLOCK samples), RMS-smoothed (MS.GLUE_RMS_S), the soft-knee curve, a fast envelope (attack, release[0]) and a
    slow one following it (MS.SLOW_ATTACK_S, release[1]), the larger applied, interpolated between block centres.
    Every envelope starts from rest. Latency LATENCY['glue']: the gain at a sample needs the block after it."""
    x = D.st(np.asarray(x, dtype=float))
    n = len(x)
    starts = np.arange(0, n, MS.BLOCK)
    lens = np.diff(np.append(starts, n))
    p = np.add.reduceat(np.mean(x ** 2, 1), starts) / lens
    a = math.exp(-MS.BLOCK / (MS.GLUE_RMS_S * SR))
    level = 10 * np.log10(signal.lfilter([1 - a], [1, -a], p) + 1e-20)
    over = level - threshold_db
    knee = knee_db
    tgt = (1 - 1 / ratio) * np.where(over <= -knee / 2, 0.0, np.where(
        over >= knee / 2, over, (over + knee / 2) ** 2 / (2 * knee) if knee > 0 else over))
    coef = lambda tau: np.exp(-lens / (tau * SR)).tolist()  # noqa: E731
    aa, rf, sa, rs = coef(attack_s), coef(release_s[0]), coef(MS.SLOW_ATTACK_S), coef(release_s[1])
    f = s = 0.0
    out = np.empty(len(tgt))
    for i, ti in enumerate(tgt.tolist()):
        f = ti + (aa[i] if ti > f else rf[i]) * (f - ti)
        s = f + (sa[i] if f > s else rs[i]) * (s - f)
        out[i] = f if f > s else s
    centres = starts + 0.5 * lens - 0.5
    return np.interp(np.arange(n), centres, out)


def glue(x, threshold_db, ratio=2.0, knee_db=6.0, attack_s=0.030, release_s=(0.15, 1.5), makeup_db=0.0):
    """The glue (or a comp insert) applied: x times its gain, then the makeup."""
    x = D.st(np.asarray(x, dtype=float))
    g = glue_db(x, threshold_db, ratio, knee_db, attack_s, release_s)
    return x * D.undb(makeup_db - g)[:, None]


# ------------------------------------------------------------------ nonlinear stages at 4x
def clip_curves(ceiling_db, knee_db=1.5):
    """master.soft_clip's curve as (excess, its antiderivative): linear up to knee_db under the ceiling (dBFS), then
    a tanh toward it; first-order ADAA runs on the excess only."""
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


def soft_clip(x, ceiling_db, knee_db=1.5):
    """master.soft_clip on a stream (no wrap): os_fir(4) up, first-order ADAA on the excess over the knee, down: the
    club master's clip. Latency LATENCY['soft_clip']."""
    excess, excess_int = clip_curves(ceiling_db, knee_db)
    uu = D.up(np.asarray(x, dtype=float), 4)
    return D.down(uu + D._adaa(uu, excess, excess_int), 4)


def drive(x, drive_amount, bias=0.0):
    """dsp48.tanh_adaa at 4x on os_fir(4): (tanh(d x + b) - tanh(b)) / tanh(d). Latency LATENCY['drive']."""
    return D.oversampled(lambda u: D.tanh_adaa(u, drive_amount, bias), np.asarray(x, dtype=float), 4)


# the lanes' 4x oversampling: a half-band cascade (P8's, ADR 0021 draft). Names as P8's vector export imports them.
def halfband(N, beta):            # N = 4k + 3
    """A half-band low-pass of N taps (N = 4k + 3): a Kaiser-windowed sinc at a quarter of the rate, its even taps
    zero but the centre, 0.5; the odd taps scaled to sum to 0.5 (unity at DC)."""
    c = (N - 1) // 2
    n = np.arange(N) - c
    h = np.zeros(N)
    w = np.kaiser(N, beta)
    odd = n % 2 != 0
    h[odd] = np.sin(np.pi * n[odd] / 2) / (np.pi * n[odd]) * w[odd]
    h[odd] *= 0.5 / h[odd].sum()
    h[c] = 0.5
    return h


HB1, HB2 = halfband(47, 7.0), halfband(15, 7.0)      # 1x<->2x, 2x<->4x


def _up2(x, h):    # zero-stuff, 2h, centred
    c = (len(h) - 1) // 2
    y = np.zeros((2 * len(x),) + x.shape[1:])
    y[0::2] = x
    return signal.lfilter(2 * h, [1.0], np.concatenate([y, np.zeros((c,) + y.shape[1:])]), axis=0)[c:c + len(y)]


def _down2(y, h):  # h, centred, keep the even samples
    c = (len(h) - 1) // 2
    return signal.lfilter(h, [1.0], np.concatenate([y, np.zeros((c,) + y.shape[1:])]), axis=0)[c:c + len(y)][0::2]


def hb_up4(x):
    """1x to 4x through the half-band cascade (centred: aligned time)."""
    return _up2(_up2(np.asarray(x, dtype=float), HB1), HB2)


def hb_down4(u):
    """4x to 1x through the half-band cascade (centred)."""
    return _down2(_down2(u, HB2), HB1)


def lane_soft_clip(x, ceiling_db, knee_db=1.5):
    """A clip insert: master.soft_clip's curve and ADAA at 4x on the half-band cascade, at an absolute ceiling
    (dBFS). Latency LATENCY['lane_clip']."""
    excess, excess_int = clip_curves(ceiling_db, knee_db)
    u = hb_up4(x)
    return hb_down4(u + D._adaa(u, excess, excess_int))


def lane_drive(x, drive_amount, bias=0.0, output_db=0.0):
    """A drive insert: dsp48.tanh_adaa at 4x on the half-band cascade, then output_db. Latency
    LATENCY['lane_drive']."""
    return hb_down4(D.tanh_adaa(hb_up4(x), drive_amount, bias)) * float(D.undb(output_db))


# ------------------------------------------------------------------ the limiter
def sliding_min(a, before, after, fill=1.0):
    """min of a[k - before .. k + after] per k (fill outside the stream), by ndimage's van Herk filter."""
    a = np.asarray(a, dtype=float)
    size = before + after + 1
    pad = np.concatenate([np.full(before, fill), a, np.full(after, fill)])
    m = ndimage.minimum_filter1d(pad, size=size, mode='nearest')
    # minimum_filter1d's window for output i is pad[i - size // 2 .. i - size // 2 + size - 1]: the one that starts
    # at pad[k] (a[k - before]) is output k + size // 2
    return m[size // 2: size // 2 + len(a)]


def _mean_back(a, w, fill=1.0):
    """mean of a[k - w .. k] per k; before the stream the value is `fill`."""
    c = np.concatenate([[0.0], np.cumsum(np.concatenate([np.full(w, fill), a]))])
    k = np.arange(len(a))
    return (c[k + w + 1] - c[k]) / (w + 1)


def _release(r2, release_s, chunk=65536):
    """r[k] = max(r2[k], r[k - 1] e^la) from r[-1] = 0: master.limit_gain_db's exponential release in dB, without
    the wrap, in chunks so the exponents stay in range."""
    la = -1.0 / (release_s * SR)
    out = np.empty(len(r2))
    last = 0.0
    for s in range(0, len(r2), chunk):
        rr = np.asarray(r2[s:s + chunk], dtype=float)
        k = np.arange(len(rr))
        seed = np.log(max(last, 1e-300)) - (-1) * la     # the carried value enters at k = -1
        with np.errstate(divide='ignore'):
            acc = np.maximum.accumulate(np.concatenate([[seed], np.log(np.maximum(rr, 1e-300)) - k * la]))[1:]
        r = np.maximum(np.exp(k * la + acc), rr)
        out[s:s + len(rr)] = r
        last = float(r[-1])
    return np.where(out < 1e-9, 0.0, out)


def limit_db(x, ceiling_db, lookahead_s=0.005, hold_s=0.060, release_s=0.080, hold='lookback'):
    """The true-peak limiter's gain reduction (dB, >= 0) per sample on a stream, stereo-linked: the gain each sample
    needs from the 4x peak within a sample of it (os_fir(4)), the lowest need held from hold_s before to lookahead_s
    after (hold='lookback', the real-time master) or to lookahead_s + hold_s after (hold='two_sided', master.limit),
    ramped down over the lookahead by a moving average and released exponentially in dB. Latency
    limit_latency(lookahead_s, hold_s, hold)."""
    x = D.st(np.asarray(x, dtype=float))
    n = len(x)
    c = float(D.undb(ceiling_db))
    p = np.abs(D.up(x, 4)).max(1)
    pp = np.concatenate([np.zeros(4), p, np.zeros(4)])
    idx = 4 * np.arange(n) + 4
    pk = np.max(np.stack([pp[idx + d] for d in range(-4, 5)]), 0)
    need = np.minimum(1.0, c / (pk + 1e-30))
    w = 2 * int(round(lookahead_s * SR / 2))
    h = int(round(hold_s * SR))
    if hold not in ('lookback', 'two_sided'):
        raise ValueError(f"hold is 'lookback' or 'two_sided', not {hold!r}")
    after = w + h if hold == 'two_sided' else w
    m = sliding_min(need, h, after)
    g = _mean_back(m, w)
    r2 = -20 * np.log10(np.minimum(g, 1.0))
    return _release(r2, release_s)


def limit(x, ceiling_db, lookahead_s=0.005, hold_s=0.060, release_s=0.080, hold='lookback'):
    """The limiter applied: x times its gain (aligned)."""
    x = D.st(np.asarray(x, dtype=float))
    r = limit_db(x, ceiling_db, lookahead_s, hold_s, release_s, hold)
    return x * D.undb(-r)[:, None]


# ------------------------------------------------------------------ DC
def dc_block(x, hz=5.0, order=DC_ORDER):
    """The real-time master's DC blocker: a causal Butterworth high-pass (master.dc_clean's is zero-phase)."""
    return D.bfilt(np.asarray(x, dtype=float), hz, 'highpass', order=order)
