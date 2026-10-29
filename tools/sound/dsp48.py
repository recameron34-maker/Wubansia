"""48 kHz DSP helpers for the sound recipes.

Everything renders at SR = 48000 in float64, mono as (n,) and stereo as (n, 2). Nonlinear stages run oversampled with
first-order ADAA. Noise always comes from rng(kit, sound, layer, rr, ...), never from a global generator, so a sound
renders the same bytes whatever else ran first.

Provenance: the helpers from `seed` to `mono_below` are ported, cleaned up, from the S1 research prototype (`fut.py` and
parts of `recipes.py`, written during planning on 2026-10-29). freq_shift, ring_mod, updown3 and resample_pass are the
new tools of plan section 2.2.
"""
import hashlib

import numpy as np
from scipy import fft as sp_fft
from scipy import signal
from scipy.linalg import hadamard

SR = 48000
TP_TARGET = -1.02  # render target, dBTP: 20 mdB under the QA ceiling of -1.0, for the 24-bit rounding


# ------------------------------------------------------------------ determinism
def seed(*key):
    """64-bit seed from a key: SHA-256 of its parts joined by U+001F. Python's hash() is salted per process: never use it."""
    text = '\x1f'.join(str(k) for k in key)
    return int.from_bytes(hashlib.sha256(text.encode('utf-8')).digest()[:8], 'little')


def rng(*key):
    """rng(kit, sound, layer, rr, *extra): the same key gives the same stream on every run (numpy is pinned)."""
    return np.random.Generator(np.random.PCG64(seed(*key)))


# ------------------------------------------------------------------ units
NOTE_NAMES = ('C', 'Cs', 'D', 'Ds', 'E', 'F', 'Fs', 'G', 'Gs', 'A', 'As', 'B')


def midi_hz(m):
    return 440.0 * 2.0 ** ((np.asarray(m, dtype=float) - 69.0) / 12.0)


def note_name(m):
    """MIDI note as a file-safe name, with C4 = 60: 30 -> 'Fs1'."""
    return f'{NOTE_NAMES[int(m) % 12]}{int(m) // 12 - 1}'


def cents(f, ref):
    return 1200.0 * np.log2(f / ref)


def db(a):
    return 20.0 * np.log10(np.abs(a) + 1e-12)


def undb(d):
    return 10.0 ** (np.asarray(d, dtype=float) / 20.0)


def st(x):
    """Mono to stereo (n, 2); stereo passes through."""
    x = np.asarray(x, dtype=float)
    return np.stack([x, x], 1) if x.ndim == 1 else x


def mono(x):
    x = np.asarray(x, dtype=float)
    return x if x.ndim == 1 else x.mean(1)


# ------------------------------------------------------------------ oversampling
_OS = {}


def os_fir(k):
    """Anti-imaging / anti-alias FIR for k x oversampling: flat to 20 kHz, about 100 dB down from 28 kHz (what folds back
    lands above 20 kHz)."""
    if k not in _OS:
        _OS[k] = signal.firwin(48 * k + 1, 24000.0, width=8000.0, window=('kaiser', 10.0), fs=SR * k)
    return _OS[k]


def up(x, k):
    return x if k == 1 else signal.resample_poly(x, k, 1, axis=0, window=os_fir(k))


def down(x, k):
    return x if k == 1 else signal.resample_poly(x, 1, k, axis=0, window=os_fir(k))


def oversampled(fn, x, k=4):
    return down(fn(up(x, k)), k)


def true_peak_db(x, k=4):
    """Peak of the k x oversampled signal, dBTP."""
    return float(db(np.abs(up(np.asarray(x, dtype=float), k)).max()))


def to_ceiling(x, tp_db=TP_TARGET):
    """Scale so the true peak sits at tp_db. Only for single sounds: an instrument shares one gain (render.py)."""
    return x * undb(tp_db - true_peak_db(x))


# ------------------------------------------------------------------ ADAA nonlinearities (first order, vectorised)
def _logcosh(u):
    a = np.abs(u)
    return a + np.log1p(np.exp(-2.0 * a)) - np.log(2.0)


def _adaa(u, f, F, eps=1e-6):
    u1 = np.concatenate([u[:1] * 0, u[:-1]], axis=0)
    d = u - u1
    big = np.abs(d) > eps
    safe = np.where(big, d, 1.0)
    return np.where(big, (F(u) - F(u1)) / safe, f(0.5 * (u + u1)))


def tanh_adaa(x, drive=1.0, bias=0.0):
    """Soft clip, unity at full scale. bias adds even harmonics; the static offset of the bias is removed, but a biased
    curve still makes DC from the signal itself: high-pass afterwards (dc_block)."""
    u = drive * x
    tb = np.tanh(bias)
    y = _adaa(u, lambda v: np.tanh(v + bias) - tb, lambda v: _logcosh(v + bias) - v * tb)
    return y / np.tanh(max(drive, 1e-9))


def hardclip_adaa(x, drive=1.0):
    u = drive * x
    F = lambda v: np.where(np.abs(v) <= 1, 0.5 * v * v, np.abs(v) - 0.5)
    return _adaa(u, lambda v: np.clip(v, -1, 1), F)


def sinefold_adaa(x, fold=1.0):
    """Sine wavefolder y = sin(g x), g = pi/2 * fold."""
    g = 0.5 * np.pi * max(fold, 1e-6)
    return _adaa(x, lambda v: np.sin(g * v), lambda v: -np.cos(g * v) / g)


# ------------------------------------------------------------------ oscillators
def phase(freq, n):
    """Running phase in cycles for a frequency (Hz, a number or one value per sample), starting at 0."""
    f = np.broadcast_to(np.asarray(freq, dtype=float), (n,))
    return np.concatenate([[0.0], np.cumsum(f[:-1] / SR)])


def polyblep_saw(freq, n, ph0=0.0, k=1):
    """PolyBLEP saw rendered at k * SR: pass freq at the output rate; returns k * n samples."""
    f = np.repeat(np.broadcast_to(np.asarray(freq, dtype=float), (n,)), k)
    dt = np.clip(f / (SR * k), 1e-9, 0.5)
    p = (np.concatenate([[0.0], np.cumsum(dt[:-1])]) + ph0) % 1.0
    y = 2.0 * p - 1.0
    m1 = p < dt
    t = p[m1] / dt[m1]
    y[m1] -= t + t - t * t - 1.0
    m2 = p > 1.0 - dt
    t = (p[m2] - 1.0) / dt[m2]
    y[m2] -= t * t + t + t + 1.0
    return y


def additive_saw(freq, n, ph0=0.0, fmax=20000.0):
    """Exactly band-limited saw: harmonics up to fmax at the highest instantaneous pitch. Slow, but clean."""
    f = np.broadcast_to(np.asarray(freq, dtype=float), (n,))
    ph = 2 * np.pi * (phase(f, n) + ph0)
    y = np.zeros(n)
    for h in range(1, int(fmax // f.max()) + 1):
        y += np.sin(h * ph) / h
    return y * (2 / np.pi)


# Szabo (2010) JP-8000 supersaw fit, as transcribed in gist audionerd/fe50790b7601cba65ddd855caffb05ad
SZ_OFF = np.array([-0.11002313, -0.06288439, -0.01952356, 0.0, 0.01991221, 0.06216538, 0.10745242])
_SZ_POLY = [10028.7312891634, -50818.8652045924, 111363.4808729368, -138150.6761080548, 106649.6679158292,
            -53046.9642751875, 17019.9518580080, -3425.0836591318, 404.2703938388, -24.1878824391, 0.6717417634,
            0.0030115596]


def sz_detune(x):
    return float(np.polyval(_SZ_POLY, x))


def sz_mix(x):
    """(centre gain, side gain) for a mix amount 0..1."""
    return -0.55366 * x + 0.99785, -0.73764 * x * x + 1.2841 * x + 0.044372


def supersaw(f, n, r, detune=0.35, mix=0.6, additive=True):
    """Seven detuned saws (Szabo detune and mix curves) spread across the stereo field, high-passed at the fundamental."""
    dfac = sz_detune(detune)
    cg, sg = sz_mix(mix)
    out = np.zeros((n, 2))
    pans = np.array([-1, -0.66, -0.33, 0, 0.33, 0.66, 1.0]) * 0.9
    for off, p in zip(SZ_OFF, pans):
        fi = np.asarray(f, dtype=float) * (1 + dfac * off)
        osc = additive_saw(fi, n, r.uniform()) if additive else polyblep_saw(fi, n, r.uniform())
        g = cg if off == 0 else sg
        out[:, 0] += g * osc * np.cos((p + 1) * np.pi / 4)
        out[:, 1] += g * osc * np.sin((p + 1) * np.pi / 4)
    return bfilt(out, float(np.min(f)), 'highpass', order=1) / 3


# ------------------------------------------------------------------ filters
def bfilt(x, fc, kind, order=2, zero_phase=False):
    """Butterworth along axis 0. kind: 'lowpass', 'highpass', 'bandpass' (fc = (lo, hi)) or 'bandstop'. zero_phase runs
    it forward and back over a copy padded with 4 periods of silence at the lowest cutoff (scipy's own odd-extension
    padding would invent a transient at the edges)."""
    sos = signal.butter(order, fc, kind, fs=SR, output='sos')
    if not zero_phase:
        return signal.sosfilt(sos, x, axis=0)
    x = np.asarray(x, dtype=float)
    pad = int(4 * SR / np.min(fc))
    z = np.zeros((pad,) + x.shape[1:])
    return signal.sosfiltfilt(sos, np.concatenate([z, x, z]), axis=0, padtype=None)[pad:pad + len(x)]


def lr4(x, fc, kind):
    """Linkwitz-Riley 4th order (Butterworth squared): low + high sum to an all-pass, so bands recombine flat."""
    sos = signal.butter(2, fc, kind, fs=SR, output='sos')
    return signal.sosfilt(sos, signal.sosfilt(sos, x, axis=0), axis=0)


def peaking(x, fc, gain_db, q):
    """RBJ cookbook peaking EQ."""
    A = 10 ** (gain_db / 40)
    w0 = 2 * np.pi * fc / SR
    al = np.sin(w0) / (2 * q)
    b = np.array([1 + al * A, -2 * np.cos(w0), 1 - al * A])
    a = np.array([1 + al / A, -2 * np.cos(w0), 1 - al / A])
    return signal.lfilter(b / a[0], a / a[0], x, axis=0)


def dc_block(x, fc=20.0, order=4):
    """High-pass for the DC and sub-sonic drift that asymmetric drive leaves behind. Use it before finish()."""
    return bfilt(x, fc, 'highpass', order=order)


def svf_tpt(x, fc, q=0.707, fs=SR):
    """Zavalishin TPT state-variable filter with a per-sample cutoff, mono. Returns (lp, bp, hp). A Python loop: about
    1 s per 10 s of audio."""
    n = len(x)
    fc = np.broadcast_to(np.asarray(fc, dtype=float), (n,))
    g = np.tan(np.pi * np.clip(fc, 10, fs * 0.49) / fs)
    k = 1.0 / q
    a1 = 1.0 / (1.0 + g * (g + k))
    lp = np.empty(n)
    bp = np.empty(n)
    hp = np.empty(n)
    s1 = s2 = 0.0
    for i in range(n):
        h = (x[i] - (k + g[i]) * s1 - s2) * a1[i]
        v1 = g[i] * h
        b = v1 + s1
        s1 = b + v1
        v2 = g[i] * b
        lo = v2 + s2
        s2 = lo + v2
        lp[i] = lo
        bp[i] = b
        hp[i] = h
    return lp, bp, hp


def ladder_tanh(x, fc, res, fs, drive=1.0):
    """Huovilainen-style 4-pole ladder with a tanh per stage (one step, no ZDF), mono. Run it at >= 4 x. res 0..1.1
    (self-oscillation near 1). A Python loop."""
    n = len(x)
    fc = np.broadcast_to(np.asarray(fc, dtype=float), (n,))
    g = 1.0 - np.exp(-2 * np.pi * np.clip(fc, 10, fs * 0.45) / fs)
    s = [0.0, 0.0, 0.0, 0.0]
    y = np.empty(n)
    th = np.tanh
    r4 = 4.0 * res
    for i in range(n):
        gi = g[i]
        u = th(drive * x[i] - r4 * s[3])
        s[0] += gi * (u - th(s[0]))
        s[1] += gi * (th(s[0]) - th(s[1]))
        s[2] += gi * (th(s[1]) - th(s[2]))
        s[3] += gi * (th(s[2]) - th(s[3]))
        y[i] = s[3]
    return y


# male averages, Peterson & Barney (1952), via the Stanford/Princeton tables
VOWELS = {'ee': (270, 2290, 3010), 'eh': (530, 1840, 2480), 'ae': (660, 1720, 2410), 'ah': (730, 1090, 2440),
          'oo': (300, 870, 2240)}


def formant_stft(x, vowel_track, q=6.0, nper=1024, hop=128):
    """Time-varying vowel filter in the STFT domain, mono: each frame is weighted by a 3-formant envelope.
    vowel_track: (frames, 3) formant frequencies in Hz, stretched over the sound."""
    f, _, X = signal.stft(x, SR, nperseg=nper, noverlap=nper - hop, boundary='even')
    vt = np.asarray(vowel_track, dtype=float)
    idx = np.clip(np.round(np.linspace(0, len(vt) - 1, X.shape[-1])).astype(int), 0, len(vt) - 1)
    F = vt[idx]
    lf = np.log2(np.maximum(f, 20.0))[:, None]
    env = np.zeros((len(f), X.shape[-1]))
    for j, a in enumerate((1.0, 0.7, 0.35)):
        env += a * np.exp(-0.5 * ((lf - np.log2(F[:, j])[None, :]) * q) ** 2)
    _, y = signal.istft(X * (0.15 + env), SR, nperseg=nper, noverlap=nper - hop, boundary=True)
    return y[:len(x)]


# ------------------------------------------------------------------ resonators, feedback structures (block-vectorised)
def modal_ir(f0, ratios, decays, amps, n, r, fmax=20000.0):
    """Sum of damped sines at f0 * ratios (modes above fmax are left out), random start phases from r."""
    t = np.arange(n) / SR
    y = np.zeros(n)
    for rt, d, a in zip(ratios, decays, amps):
        f = f0 * rt
        if f < fmax:
            y += a * np.exp(-t / d) * np.sin(2 * np.pi * f * t + r.uniform(0, 2 * np.pi))
    return y


def karplus(excite, f0, n, decay_s=1.2, S=0.5, disp=0.0):
    """Extended Karplus-Strong (Jaffe & Smith 1983), one delay length per block (exact: the loop reads only samples at
    least N back). Loop = brightness FIR (1 - S) + S z^-1, a dispersion all-pass (disp != 0 bends the partials: metallic)
    and a tuning all-pass for the fractional delay."""
    period = SR / f0
    dd = (1 - disp) / (1 + disp) if disp else 0.0  # DC phase delay of the dispersion all-pass
    N = int(np.floor(period - S - dd - 0.1))
    frac = period - N - S - dd
    eta = (1 - frac) / (1 + frac)
    g = 10 ** (-3 / (decay_s * f0))  # -60 dB after decay_s
    B = np.polymul([1 - S, S], [eta, 1.0])
    A = np.array([1.0, eta])
    if disp:
        B = np.polymul(B, [disp, 1.0])
        A = np.polymul(A, [1.0, disp])
    y = np.zeros(n + N)
    x = np.zeros(n + N)
    m = min(len(excite), n)
    x[N:N + m] = excite[:m]
    zi = np.zeros(max(len(A), len(B)) - 1)
    for s in range(N, n + N, N):
        e = min(s + N, n + N)
        v, zi = signal.lfilter(B, A, y[s - N:e - N], zi=zi)
        y[s:e] = x[s:e] + g * v
    return y[N:]


def fdn_reverb(x, r, t60=3.5, size=1.0, damp_hz=7000.0, n_tail=0):
    """8-line FDN, Hadamard matrix, one-pole damping per line, computed in blocks of the shortest delay (exact,
    vectorised). Mono in, stereo out (decorrelated output signs). r sets the delay jitter and input signs: pass the kit's
    space seed, rng('FUT01/space')."""
    base = np.array([1153, 1327, 1523, 1741, 1999, 2297, 2593, 2953]) * size
    d = np.round(base * (1 + 0.03 * r.uniform(-1, 1, 8))).astype(int)
    Hm = hadamard(8) / np.sqrt(8)
    g = 10 ** (-3 * d / (t60 * SR))  # per-line gain for the T60
    a = np.exp(-2 * np.pi * damp_hz / SR)
    n = len(x) + n_tail
    xin = np.zeros(n)
    xin[:len(x)] = x
    D = d.max()
    lines = np.zeros((8, n + D))
    zi = np.zeros((8, 1))
    B = int(d.min())
    inj = r.choice([-1, 1], 8) / np.sqrt(8)
    for s in range(0, n, B):
        e = min(s + B, n)
        taps = np.stack([lines[i, D + s - d[i]:D + e - d[i]] for i in range(8)])
        taps, zi = signal.lfilter([1 - a], [1, -a], taps, axis=1, zi=zi)
        lines[:, D + s:D + e] = Hm @ (taps * g[:, None]) + inj[:, None] * xin[s:e][None, :]
    out = lines[:, D:]
    sl = np.array([1, -1, 1, 1, -1, 1, -1, -1])
    sr = np.array([1, 1, -1, 1, 1, -1, -1, 1])
    return np.stack([sl @ out, sr @ out], 1) / np.sqrt(8)


# ------------------------------------------------------------------ envelopes and edges
def env_exp(n, tau, attack=0.0, fs=SR):
    e = np.exp(-np.arange(n) / fs / tau)
    if attack > 0:
        a = max(1, int(attack * fs))
        e[:a] *= 0.5 - 0.5 * np.cos(np.pi * np.arange(a) / a)
    return e


def taper(n, t0, length):
    """1 until t0 seconds, then a raised-cosine fall to exactly 0 over `length` seconds."""
    u = np.clip((np.arange(n) / SR - t0) / length, 0.0, 1.0)
    return 0.5 + 0.5 * np.cos(np.pi * u)


def fade(x, fin=0.0, fout=0.005):
    """Raised-cosine fade-in and fade-out, in seconds."""
    x = np.array(x, dtype=float)
    a, b = int(fin * SR), int(fout * SR)
    w = lambda m: 0.5 - 0.5 * np.cos(np.pi * np.arange(m) / m)
    if a > 0:
        x[:a] *= w(a)[:, None] if x.ndim == 2 else w(a)
    if b > 0:
        x[-b:] *= w(b)[::-1][:, None] if x.ndim == 2 else w(b)[::-1]
    return x


def trim_tail(x, floor_db=-90.0, keep=0.005):
    """Cut where the sound stays under floor_db re its own peak, keeping `keep` seconds after the last louder sample."""
    env = np.abs(x) if x.ndim == 1 else np.abs(x).max(1)
    above = np.flatnonzero(env > env.max() * undb(floor_db))
    return x[:min(len(x), above[-1] + 1 + int(keep * SR))] if len(above) else x


def finish(x, oneshot=True, fade_in=0.00025, fade_out=0.005, floor_db=-90.0):
    """Master edges. One-shots: trim at floor_db re peak, then fade_out. Every sound: a 0.25 ms fade-in, so sample 0 is
    silent. Render long: if the sound is still loud at the end, the fade cuts it and the QA tail check fails."""
    x = np.asarray(x, dtype=float)
    if oneshot:
        return fade(trim_tail(x, floor_db), fade_in, fade_out)
    return fade(x, fade_in, 0.0)


# ------------------------------------------------------------------ stereo
def ms(L, R):
    return 0.5 * (L + R), 0.5 * (L - R)


def mono_below(x, fc=150.0, order=8):
    """Keep everything under fc in the centre: the side channel is high-passed at fc (Butterworth, 8th order: an LR4
    leaves decorrelated content an octave under fc only 18 dB down, short of the QA's -20 dB)."""
    M, S = ms(x[:, 0], x[:, 1])
    S = bfilt(S, fc, 'highpass', order=order)
    return np.stack([M + S, M - S], 1)


# ------------------------------------------------------------------ new tools (plan section 2.2)
def freq_shift(x, shift_hz, pad=4800):
    """Single-sideband frequency shifter: every component moves by shift_hz (a number, or one value per sample for
    barber-pole sweeps), so harmonic sounds turn inharmonic. The analytic signal comes from scipy.signal.hilbert (an
    FFT) of a zero-padded copy, which keeps the circular wrap out of the sound. Components pushed below 0 Hz fold back."""
    x = np.asarray(x, dtype=float)
    n = len(x)
    m = sp_fft.next_fast_len(n + 2 * pad)
    xp = np.zeros((m,) + x.shape[1:])
    xp[pad:pad + n] = x
    a = signal.hilbert(xp, axis=0)[pad:pad + n]
    rot = np.exp(2j * np.pi * phase(shift_hz, n))
    return np.real(a * (rot[:, None] if x.ndim == 2 else rot))


def ring_mod(x, f0, ratios, amps=None, mix=1.0, k=2, r=None):
    """Ring modulator with a bank of sine carriers at f0 * ratios (pass the kit's alloy ratios). Each input partial f
    becomes |f - f0 r| and f + f0 r, and f itself disappears. amps weight the carriers (default 1 / (1 + i)), normalised
    to sum to 1; r gives random carrier start phases (default 0). Runs at k x so sum tones don't fold back."""
    x = np.asarray(x, dtype=float)
    n = len(x)
    ratios = np.asarray(ratios, dtype=float)
    amps = 1.0 / (1.0 + np.arange(len(ratios))) if amps is None else np.asarray(amps, dtype=float)
    amps = amps / amps.sum()
    ph0 = r.uniform(0, 2 * np.pi, len(ratios)) if r is not None else np.zeros(len(ratios))
    fk = np.repeat(np.broadcast_to(np.asarray(f0, dtype=float), (n,)), k)
    base = 2 * np.pi * np.concatenate([[0.0], np.cumsum(fk[:-1] / (SR * k))])
    car = sum(a * np.sin(rt * base + p) for rt, a, p in zip(ratios, amps, ph0))
    u = up(x, k)
    wet = down(u * (car[:, None] if x.ndim == 2 else car), k)
    return (1 - mix) * x + mix * wet


def _bands3(v):
    return tuple(v) if np.ndim(v) else (v, v, v)


def _band_gain_db(b, down_db, down_ratio, up_db, up_ratio, max_up_db, floor_db, attack, release, rms, block):
    """Gain in dB per block for one band: stereo-linked RMS (one-pole on the power, rms seconds), then attack/release
    on the level in dB, then the static curve."""
    p2 = b ** 2 if b.ndim == 1 else (b ** 2).mean(1)
    nb = -(-len(p2) // block)
    p = np.concatenate([p2, np.zeros(nb * block - len(p2))]).reshape(nb, block).mean(1)
    a_rms = np.exp(-block / (rms * SR))
    p = signal.lfilter([1 - a_rms], [1, -a_rms], p)
    a_att = np.exp(-block / (attack * SR))
    a_rel = np.exp(-block / (release * SR))
    lvl = np.empty(nb)
    s = -120.0
    for i, v in enumerate((10 * np.log10(p + 1e-20)).tolist()):
        a = a_att if v > s else a_rel
        s = a * s + (1 - a) * v
        lvl[i] = s
    over = np.maximum(lvl - down_db, 0.0)
    under = np.maximum(up_db - lvl, 0.0)
    lift = np.minimum(under * (1 - 1 / up_ratio), max_up_db)
    lift *= np.clip((lvl - (floor_db - 12.0)) / 12.0, 0.0, 1.0)  # no lift for silence and far tails
    return lift - over * (1 - 1 / down_ratio)


def updown3(x, xover=(120.0, 2500.0), down_db=-20.0, down_ratio=4.0, up_db=-36.0, up_ratio=2.0, max_up_db=12.0,
            floor_db=-66.0, attack=0.003, release=0.12, rms=0.02, makeup_db=0.0, mix=1.0, block=16):
    """3-band upward + downward compressor, the 'hyper-real' stage on the reese, stabs and snare. LR4 bands (the low band
    gets the same all-pass at the upper crossover, so neutral settings sum flat). Per band: a stereo-linked RMS detector
    on 16-sample blocks, attack/release on its level in dB (seconds), downward compression above down_db, upward
    compression below up_db (at most max_up_db, faded out under floor_db so silence and tails aren't lifted). Levels
    are dBFS; per-band settings take a number or a 3-tuple (low, mid, high). Gains are interpolated per sample."""
    x = np.asarray(x, dtype=float)
    f1, f2 = xover
    lo = lr4(x, f1, 'lowpass')
    rest = lr4(x, f1, 'highpass')
    lo = lr4(lo, f2, 'lowpass') + lr4(lo, f2, 'highpass')
    bands = (lo, lr4(rest, f2, 'lowpass'), lr4(rest, f2, 'highpass'))
    centres = (np.arange(-(-len(x) // block)) + 0.5) * block - 0.5
    pos = np.arange(len(x))
    wet = np.zeros_like(x)
    for b, dd, dr, ud, ur, mk in zip(bands, _bands3(down_db), _bands3(down_ratio), _bands3(up_db), _bands3(up_ratio),
                                     _bands3(makeup_db)):
        g = _band_gain_db(b, dd, dr, ud, ur, max_up_db, floor_db, attack, release, rms, block) + mk
        gs = undb(np.interp(pos, centres, g))
        wet += b * (gs[:, None] if x.ndim == 2 else gs)
    return (1 - mix) * sum(bands) + mix * wet


def bounce(x, peak_db=-6.0, hp_hz=20.0):
    """What printing a pass to audio does to the level: high-pass the DC and sub-sonic drift, set the peak to peak_db."""
    y = bfilt(x, hp_hz, 'highpass') if hp_hz else np.asarray(x, dtype=float)
    pk = np.abs(y).max()
    return y * (undb(peak_db) / pk) if pk > 0 else y


def resample_pass(x, stages, passes=1, key=('resample',), peak_db=-6.0, hp_hz=20.0):
    """Resampling, the way a producer does it: render, process, bounce, process again (growl -> reese -> multiband ->
    comb/formant, then the whole chain again). Each stage is stage(x, r) and returns audio; after every stage the result
    is bounced (bounce()). r = rng(*key, pass, stage) gives each stage its own deterministic noise."""
    y = np.asarray(x, dtype=float)
    for p in range(passes):
        for i, stage in enumerate(stages):
            y = bounce(np.asarray(stage(y, rng(*key, p, i)), dtype=float), peak_db, hp_hz)
    return y
