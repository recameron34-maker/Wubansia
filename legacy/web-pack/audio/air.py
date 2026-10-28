"""Air for band-limited vocals: the TTS behind the older spoken vocals stops at 10-12 kHz. WORLD analyses the vocal, each frame's
spectral envelope is continued above the cutoff along its own upper slope (with an extra roll-off past 14 kHz), and only that new
band is synthesised (mostly breathy, harmonic where the voice is voiced) and added back. The original audio below the cutoff is
untouched."""
import numpy as np
import pyworld as pw
from scipy import signal

SR = 44100
FP = 5.0


def cutoff(x, sr=SR):
    """Where the spectrum falls away: the lowest frequency above 6 kHz from which the long-term spectrum stays 30 dB under its
    4-8 kHz level (ignoring anything above 18 kHz)."""
    f, P = signal.welch(x, sr, nperseg=4096)
    L = 10 * np.log10(np.convolve(P, np.ones(5) / 5, 'same') + 1e-20)
    ref = np.median(L[(f >= 4000) & (f < 8000)])
    idx = np.flatnonzero((f >= 6000) & (f < 18000))
    below = L[idx] < ref - 30
    for k in range(len(idx)):
        if below[k:].all():
            return float(f[idx[k]])
    return float(f[idx[-1]])


def _band(x, sr, c, f0, t, gain_db):
    sp = pw.cheaptrick(x, f0, t, sr)
    ap = pw.d4c(x, f0, t, sr)
    nb = sp.shape[1]
    f = np.arange(nb) * (sr / 2) / (nb - 1)
    L = 10 * np.log10(sp + 1e-20)
    band = lambda lo, hi: L[:, (f >= lo) & (f < hi)].mean(axis=1)
    la, lb = band(0.45 * c, 0.7 * c), band(0.72 * c, 0.92 * c)
    slope = np.clip((lb - la) / np.log2(0.82 / 0.575), -24.0, -8.0)
    ext = lb[:, None] + slope[:, None] * np.log2(np.maximum(f, 1.0) / (0.82 * c))[None, :] - 24 * np.log2(np.maximum(f, 14000) / 14000)[None, :] + gain_db
    on = f >= 0.8 * c
    sp2 = np.where(on[None, :], 10 ** (ext / 10), 1e-12 * sp)
    aph = np.maximum(ap[:, (f >= 0.7 * c) & (f < 0.92 * c)].mean(axis=1), np.where(f0 > 0, 0.6, 0.9))
    ap2 = np.where(on[None, :], aph[:, None], ap)
    y = pw.synthesize(np.ascontiguousarray(f0), np.ascontiguousarray(sp2), np.ascontiguousarray(np.clip(ap2, 0, 1)), sr, FP)
    y = signal.sosfiltfilt(signal.butter(4, XO * c, 'highpass', fs=sr, output='sos'), y)
    return y[:len(x)] if len(y) >= len(x) else np.concatenate([y, np.zeros(len(x) - len(y))])


XO = 0.9     # crossover, as a fraction of the cutoff: the original's own roll-off above it is replaced by the new band


def _lowpass(x, sr, fc):
    return signal.sosfiltfilt(signal.butter(4, fc, 'lowpass', fs=sr, output='sos'), x, axis=0)


def _energy(x, sr, lo, hi):
    m = x if x.ndim == 1 else x.mean(axis=1)
    f, P = signal.welch(m, sr, nperseg=4096)
    return float(P[(f >= lo) & (f < hi)].sum())


def add_air(x, sr=SR, target=-25.0, c=None, lo_db=-9.0, hi_db=3.0):
    """x: mono (n,) or stereo (n, 2) float. Adds a synthesised band above the cutoff, scaled so the 12-18 kHz energy sits `target`
    dB under the 1-4 kHz energy (within lo_db..hi_db of the band's natural level). Returns (y, cutoff, gain dB or None if skipped)."""
    mono = x if x.ndim == 1 else x.mean(axis=1)
    c = cutoff(mono, sr) if c is None else c
    mid, top = _energy(x, sr, 1000, 4000), _energy(x, sr, 12000, 18000)
    want = mid * 10 ** (target / 10)
    if c > 15000 or top >= want:          # already has air
        return x, c, None
    xs = np.ascontiguousarray(mono.astype(np.float64))
    f0, t = pw.dio(xs, sr, f0_floor=60, f0_ceil=800, frame_period=FP)
    f0 = pw.stonemask(xs, f0, t, sr)
    if x.ndim == 1:
        band = _band(xs, sr, c, f0, t, 0.0)
    else:
        band = np.stack([_band(np.ascontiguousarray(x[:, ch].astype(np.float64)), sr, c, f0, t, 0.0) for ch in range(x.shape[1])], axis=1)
    base = _lowpass(x, sr, XO * c)           # zero-phase Butterworth pair: the two halves cross over flat
    top0 = _energy(base, sr, 12000, 18000)
    eb = _energy(band, sr, 12000, 18000) + 1e-30
    g = np.sqrt(max(want - top0, 0.0) / eb)
    g_db = float(np.clip(20 * np.log10(max(g, 1e-9)), lo_db, hi_db))
    return base + band * 10 ** (g_db / 20), c, g_db
