"""Brat vocals: attitude prosody (WORLD), breaths, brat FX chains, and talk-sung chants placed on the grid."""
import numpy as np
import pyworld as pw
import pedalboard as pb
from dsp import SR, bfilt, board, reverb, pingpong, to_stereo, place, warp16, step_seconds
import vocals as V
import vocals2 as V2

TTS_SR = V.TTS_SR
FP = V.FP
RNG = np.random.default_rng(23)


def analyze_refined(x, sr=TTS_SR):
    """WORLD analysis with a stricter voicing decision. Harvest happily calls the noise of an 'f' or 's' voiced, and
    resynthesis then turns 'face' into 'vase'; frames without real periodicity at the found pitch are made unvoiced."""
    x = np.ascontiguousarray(x, dtype=np.float64)
    f0, t = pw.harvest(x, sr, f0_floor=65, f0_ceil=750, frame_period=FP)
    f0r = f0.copy()
    for i in np.flatnonzero(f0 > 0):
        c = int(t[i] * sr); P = sr / f0[i]
        w = int(max(2.2 * P, 0.018 * sr))
        a = c - w // 2; b = a + w
        hi = int(P * 1.12) + 1
        if a < 0 or b + hi >= len(x):
            continue
        seg = x[a:b]; e1 = np.dot(seg, seg)
        if e1 < 1e-10:
            f0r[i] = 0; continue
        best = 0.0
        for lag in range(int(P * 0.9), hi):
            s2 = x[a + lag:b + lag]
            best = max(best, np.dot(seg, s2) / (np.sqrt(e1 * np.dot(s2, s2)) + 1e-12))
        if best < 0.5:
            f0r[i] = 0
    # drop voiced specks shorter than 3 frames
    for a, b in runs(f0r > 0):
        if b - a < 3:
            f0r[a:b] = 0
    sp = pw.cheaptrick(x, f0r, t, sr)
    ap = pw.d4c(x, f0r, t, sr)
    return f0r, sp, ap


def runs(mask):
    """Start/end indices of True runs."""
    d = np.diff(np.concatenate([[0], mask.astype(int), [0]]))
    return list(zip(np.flatnonzero(d == 1), np.flatnonzero(d == -1)))


def attitude(x, lift=0.0, g=1.0, formant=1.0, uptalk=0.0, fry=0.0, sr=TTS_SR):
    """Re-voice a TTS line with more attitude: lift the pitch, exaggerate the contour (g), optional
    valley-girl rise on the last phrase (uptalk, semitones) and a creaky fry on the final syllable (fry, seconds)."""
    if lift == 0 and g == 1 and formant == 1 and not uptalk and not fry:
        return x
    f0, sp, ap = V.analyze(x, sr)
    v = f0 > 0
    if not v.any():
        return x
    m = V.hz2m(np.where(v, f0, 1.0))
    med = float(np.median(m[v]))
    v = v & (m > med - 9)                       # leave TTS creak alone rather than re-pitching it
    mm = np.where(v, med + lift + g * (m - med), 0.0)
    R = [r for r in runs(v) if r[1] - r[0] >= 4]
    sp = sp.copy(); ap = ap.copy()
    if uptalk and R:
        a, b = R[-1]
        s = a + int(0.35 * (b - a))
        mm[s:b] += uptalk * np.linspace(0, 1, b - s) ** 1.4
    if fry and R:
        a, b = R[-1]
        k = min(b - a, int(fry * 1000 / FP))
        s = b - k
        r = np.linspace(0, 1, k) ** 0.8
        target = med + lift - 15.0
        mm[s:b] = mm[s:b] * (1 - r) + target * r + RNG.normal(0, 1.3, k) * r
        ap[s:b] = np.minimum(0.999, ap[s:b] + 0.22 * r[:, None])
        sp[s:b] *= (1 - 0.55 * r)[:, None]
    f0n = np.where(v, V.m2hz(mm), 0.0)
    y = pw.synthesize(np.ascontiguousarray(f0n), np.ascontiguousarray(V.warp_formants(sp, formant)), np.ascontiguousarray(ap), sr, FP)
    return y[:len(x) + int(0.02 * sr)]


def breath(level_db=-30, dur=0.22, sr=SR):
    """A soft inhale: band-passed noise with a swell, to sit before a line."""
    n = int(dur * sr)
    t = np.linspace(0, 1, n)
    env = np.sin(np.pi * t ** 0.7) ** 2
    nz = RNG.standard_normal(n)
    nz = bfilt(nz, 'bandpass', (900, 6500), 2) + 0.4 * bfilt(nz, 'bandpass', (2500, 4200), 2)
    nz = nz / (np.abs(nz).max() + 1e-9)
    return nz * env * 10 ** (level_db / 20)


def with_breath(x44, level_db=-30, gap=0.04):
    b = breath(level_db)
    return np.concatenate([b, np.zeros(int(gap * SR)), x44])


def air(y, amt_db=-17):
    """Exciter: the TTS stops at 12 kHz, so square the 5.5-11 kHz band (clean 2nd harmonics, no aliasing) and add it back above 11.5 kHz."""
    band = bfilt(y, 'bandpass', (5500, 11000), 2)
    h = bfilt(band ** 2, 'highpass', 11500, 4)
    pk = np.abs(h).max()
    if pk < 1e-12:
        return y
    return y + h * (np.abs(y).max() * 10 ** (amt_db / 20) / pk)


def prepend_breath(wet, rel_db=-30, gap=0.035):
    b = breath(0.0)
    b = b / (np.abs(b).max() + 1e-9) * np.abs(wet).max() * 10 ** (rel_db / 20)
    b = to_stereo(b) if wet.ndim == 2 else b
    z = np.zeros((int(gap * SR), 2)) if wet.ndim == 2 else np.zeros(int(gap * SR))
    return np.concatenate([b, z, wet])


def brat_chain(x, style, bpm):
    """x: 44.1 kHz mono after cleanup-free TTS. Returns stereo."""
    y = V2.clean(x)
    if style != 'bratphone':
        y = air(y)
    if style == 'brat':           # close and glossy: presence, a little nasal honk, short room, delay throw on the last word
        y = board(y, [pb.PeakFilter(1350, 1.5, 1.1)])
        y = V.parallel_dist(y, 8, 0.14)
        y = V.throw(y, bpm, 0.3, 0.3, 0.26, 5200)
        return reverb(y, 0.22, 0.07, 0.4, 1.0, 1.0)
    if style == 'bratphone':      # FaceTime / voice-note speaker
        y = bfilt(bfilt(y, 'highpass', 420, 4), 'lowpass', 3600, 4)
        y = board(y, [pb.PeakFilter(1800, 4.0, 1.4), pb.Distortion(drive_db=10),
                      pb.Compressor(threshold_db=-20, ratio=6, attack_ms=1, release_ms=60), pb.Gain(-5)])
        y = V.throw(y, bpm, 0.3, 0.35, 0.3, 3200)
        return reverb(y, 0.25, 0.06, 0.5, 0.8, 1.0)
    if style == 'bratpitch':      # hyperpop: pitched up before this chain, chorus + bright throw
        y = board(y, [pb.HighShelfFilter(11000, 2.5)])
        y = board(to_stereo(y), [pb.Chorus(rate_hz=0.6, depth=0.12, mix=0.2, centre_delay_ms=7)])
        y = V.throw(y, bpm, 0.35, 0.36, 0.3, 6000)
        return reverb(y, 0.3, 0.09, 0.4, 1.0, 1.0)
    if style == 'bratwhisper':    # close ASMR whisper, tight space
        y = board(y, [pb.HighpassFilter(170), pb.Compressor(threshold_db=-30, ratio=5, attack_ms=2, release_ms=70), pb.HighShelfFilter(8000, 3.5)])
        y = board(to_stereo(y), [pb.Chorus(rate_hz=0.7, depth=0.12, mix=0.15, centre_delay_ms=8)])
        y = V.throw(y, bpm, 0.35, 0.35, 0.25, 6000, dotted=False)
        return reverb(y, 0.35, 0.1, 0.4, 1.0, 1.0)
    if style == 'bratadlib':
        y = board(y, [pb.HighShelfFilter(12000, 2.0)])
        return V2.space(y, bpm, 0.12, 0.2, 0.4)
    raise ValueError(style)


# ---------------------------------------------------------------- talk-sung chants
_W = {}


_HARVEST = V.analyze      # talk chants keep plain harvest voicing (the stricter pass can swallow a 'b' burst)


def word_take(text, voice, speed=1.0):
    k = (text, voice, speed)
    if k not in _W:
        x = V.tts(text, voice, speed)
        f0, sp, ap = _HARVEST(x)
        _W[k] = (x, f0, sp, ap)
    return _W[k]


def _lead(f0):
    v = f0 > 0
    if not v.any():
        return 0.0
    m = V.hz2m(np.where(v, f0, 1.0)); med = float(np.median(m[v]))
    return np.flatnonzero(v & (m > med - 9))[0] * FP / 1000.0


def talk_word(text, voice, midi, contour=0.35, formant=1.0, speed=1.0, cents=0.0, max_dur=None):
    """Speak a word/short phrase, pulled toward `midi` (contour keeps some speech melody), time-squeezed (max 28%)
    if it would run into the next word. Returns (24k audio, vowel lead-in s)."""
    x, f0, sp, ap = word_take(text, voice, speed)
    v = f0 > 0
    if not v.any():
        return x, 0.0
    m = V.hz2m(np.where(v, f0, 1.0))
    med = float(np.median(m[v]))
    v2 = v & (m > med - 9)
    mm = np.where(v2, midi + cents / 100.0 + contour * np.clip(m - med, -7, 7), 0.0)
    n = len(f0)
    r = 1.0 if not max_dur else min(1.0, max(0.72, max_dur / (n * FP / 1000.0)))
    if r < 1.0:   # squeeze only after the onset consonant + first 30 ms of the vowel, so bursts like the 'b' in 'broke' survive
        k_on = min(n - 2, int(np.flatnonzero(v2)[0]) + 6)
        n2 = max(k_on + 4, int(round(n * r)))
        idx = np.concatenate([np.arange(k_on, dtype=float), np.linspace(k_on, n - 1, n2 - k_on)])
        a = np.floor(idx).astype(int); b = np.minimum(a + 1, n - 1); fr = (idx - a)[:, None]
        near = np.round(idx).astype(int)
        v2 = v2[near]
        ma, mb = mm[a], mm[b]
        ma, mb = np.where(ma > 0, ma, mb), np.where(mb > 0, mb, ma)   # never interpolate toward an unvoiced 0
        mm = np.where(v2, ma * (1 - fr[:, 0]) + mb * fr[:, 0], 0.0)
        sp = sp[a] * (1 - fr) + sp[b] * fr
        ap = ap[a] * (1 - fr) + ap[b] * fr
    y = pw.synthesize(np.ascontiguousarray(np.where(v2, V.m2hz(np.maximum(mm, 1)), 0.0)), np.ascontiguousarray(V.warp_formants(sp, formant)),
                      np.ascontiguousarray(ap), TTS_SR, FP)
    r_ = min(len(y) // 4, int(0.01 * TTS_SR))
    y[-r_:] *= np.linspace(1, 0, r_)
    lead = np.flatnonzero(v2)[0] * FP / 1000.0
    return y, lead


def chant_line(events, voice, bpm, swing, bars, contour=0.35, formant=1.0, cents=0.0, delay=0.0, speed=1.0):
    """events: (step16, midi, text). Each text lands with its first vowel on the step (swung like the kit) and is squeezed
    if it would run into the next one (the last wraps onto the first). 44.1k mono."""
    st = step_seconds(bpm)
    T = lambda s: float(warp16(np.array(float(s)), swing)) * st
    loop = bars * 16 * st
    L = int(loop * TTS_SR)
    buf = np.zeros(L + 3 * TTS_SR)
    ev = sorted(events)
    leads = [_lead(word_take(w, voice, speed)[1]) for _, _, w in ev]
    starts = [T(s) - ld for (s, _, _), ld in zip(ev, leads)]
    for i, (s0, m, w) in enumerate(ev):
        nxt = starts[i + 1] if i + 1 < len(ev) else loop + starts[0]
        y, lead = talk_word(w, voice, m, contour, formant, speed, cents, max_dur=nxt - starts[i] - 0.02)
        at = int((T(s0) - lead + delay) * TTS_SR)
        if at < 0:
            y = y[-at:]; at = 0
        place(buf, y, at)
    return V.up(buf)


def chant_mix(lead, doubles, bpm, loop_len, style='brat', octave_up=None):
    """Lead centre, doubles wide (and an optional octave-up layer), brat colour, loop-wrapped tail."""
    n = max(len(lead), *(len(d) for d in doubles), len(octave_up) if octave_up is not None else 0)
    pad = lambda a: np.concatenate([a, np.zeros(n - len(a))])
    mix = to_stereo(pad(V2.clean(lead)))
    for d, p in zip(doubles, [-0.7, 0.7]):
        mix += to_stereo(pad(V2.clean(d)) * 10 ** (-7 / 20), p)
    if octave_up is not None:
        mix += to_stereo(pad(V2.clean(octave_up)) * 10 ** (-13 / 20), 0.15)
    mix = board(mix, [pb.Compressor(threshold_db=-14, ratio=2.5, attack_ms=6, release_ms=110)])
    mix = np.stack([air(mix[:, 0]), air(mix[:, 1])], axis=1)
    if style == 'bratpitch':
        mix = board(mix, [pb.Chorus(rate_hz=0.6, depth=0.1, mix=0.18, centre_delay_ms=7)])
    out = V2.space(mix, bpm, 0.13, 0.16, 0.45)
    loop = out[:loop_len].copy()
    tail = out[loop_len:2 * loop_len]
    loop[:len(tail)] += tail[:len(loop)]
    return loop
