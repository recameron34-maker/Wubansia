"""Bratty delivery helpers: vocal-fry tails and uptalk on the last voiced run, done in WORLD on the tail only
and cross-faded into the untouched TTS audio so the rest of the line keeps its natural quality."""
import numpy as np
import pyworld as pw
import vocals as V

FP = V.FP
SRT = V.TTS_SR


def _last_run(f0):
    v = np.flatnonzero(f0 > 0)
    if not len(v):
        return None
    i1 = v[-1] + 1
    i0 = i1 - 1
    while i0 > 0 and f0[i0 - 1] > 0:
        i0 -= 1
    return i0, i1


def _splice(x, y, frame):
    """Original before `frame`, resynthesis after, 25 ms equal-power crossfade."""
    c = max(0, int((frame * FP / 1000) * SRT) - int(0.012 * SRT))
    w = int(0.025 * SRT)
    n = len(x)
    y = np.concatenate([y, np.zeros(max(0, n - len(y)))])[:n]
    out = x.copy()
    r = np.sin(np.linspace(0, np.pi / 2, w)) ** 2
    out[c:c + w] = x[c:c + w] * np.sqrt(1 - r) + y[c:c + w] * np.sqrt(r)
    out[c + w:] = y[c + w:]
    return out


def shape_tail(x, fry=0.0, up=0.0, dur=0.17, seed=3):
    """fry: semitones to fall into creak (0 = off); up: semitones of rise for uptalk (0 = off)."""
    if not fry and not up:
        return x
    f0, sp, ap = V.analyze(x)
    run = _last_run(f0)
    if run is None:
        return x
    i0, i1 = run
    n = int(min(dur * 1000 / FP, (i1 - i0) * 0.75))
    if n < 6:
        return x
    s = i1 - n
    f = f0.copy(); spn = sp.copy(); apn = ap.copy()
    t = np.linspace(0, 1, n)
    base = float(np.median(f0[max(i0, s - 4):s + 2]))
    if up:
        f[s:i1] = base * 2 ** (up * (3 * t ** 2 - 2 * t ** 3) / 12)
    if fry:
        rng = np.random.default_rng(seed)
        tgt = max(58.0, base * 2 ** (-fry / 12))
        cur = base * (tgt / base) ** (np.clip(t * 1.6, 0, 1) ** 0.8)
        jit = 1 + 0.22 * rng.standard_normal(n) * np.clip(t * 2 - 0.3, 0, 1)
        f[s:i1] = np.maximum(45.0, cur * jit)
        shim = 10 ** ((-4.0 * t + rng.standard_normal(n) * 1.6 * t) / 20)   # quieter, uneven pulses
        spn[s:i1] *= (shim ** 2)[:, None]
        apn[s:i1] = np.minimum(1.0, apn[s:i1] * (1 - 0.35 * t[:, None]))
    y = pw.synthesize(f, np.ascontiguousarray(spn), np.ascontiguousarray(apn), SRT, FP)
    return _splice(x, y, s)
