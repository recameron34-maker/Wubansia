"""Vocal generation: TTS voices, hard-tune / robot / sung chops via WORLD, and per-style FX chains."""
import os as _os  # noqa: E402
PACK_ROOT = _os.environ.get('PACK_ROOT') or _os.path.abspath(_os.path.join(_os.path.dirname(_os.path.abspath(__file__)), '..'))  # the legacy web-pack folder
MODELS_DIR = _os.environ.get('MODELS_DIR') or _os.path.join(PACK_ROOT, 'models')  # Kokoro TTS + Whisper ONNX models (see README)
import json, os, re, sys, time
import numpy as np
import pyworld as pw
from scipy import signal
from scipy.ndimage import median_filter
import sherpa_onnx
from dsp import *

MD = MODELS_DIR + "/kokoro-multi-lang-v1_0/"
SPK = {n: i for i, n in enumerate(
    "af_alloy,af_aoede,af_bella,af_heart,af_jessica,af_kore,af_nicole,af_nova,af_river,af_sarah,af_sky,am_adam,am_echo,am_eric,am_fenrir,am_liam,am_michael,am_onyx,am_puck,am_santa,bf_alice,bf_emma,bf_isabella,bf_lily,bm_daniel,bm_fable,bm_george,bm_lewis".split(','))}
VOICE_DESC = {
    'bm_george': 'UK male MC', 'bm_lewis': 'UK male MC', 'bm_daniel': 'UK male', 'bm_fable': 'UK male',
    'bf_emma': 'UK female', 'bf_isabella': 'UK female', 'bf_lily': 'UK female',
    'am_michael': 'US male, deep', 'am_onyx': 'US male, deep', 'am_fenrir': 'US male', 'am_adam': 'US male', 'am_echo': 'US male',
    'af_bella': 'US female', 'af_heart': 'US female', 'af_nicole': 'US female, whisper', 'af_nova': 'US female',
    'af_sarah': 'US female', 'af_jessica': 'US female', 'af_kore': 'US female', 'af_sky': 'US female'}
_ENG = {}
TTS_SR = 24000


def engine(voice):
    key = 'gb' if voice.startswith('b') else 'us'
    if key not in _ENG:
        lang, lex = ('en', 'lexicon-gb-en.txt') if key == 'gb' else ('en-us', 'lexicon-us-en.txt')
        cfg = sherpa_onnx.OfflineTtsConfig(model=sherpa_onnx.OfflineTtsModelConfig(
            kokoro=sherpa_onnx.OfflineTtsKokoroModelConfig(model=MD + "model.onnx", voices=MD + "voices.bin", tokens=MD + "tokens.txt",
                                                           data_dir=MD + "espeak-ng-data", dict_dir=MD + "dict", lexicon=MD + lex, lang=lang),
            num_threads=2, provider="cpu"), max_num_sentences=1)
        _ENG[key] = sherpa_onnx.OfflineTts(cfg)
    return _ENG[key]


def tts(text, voice, speed=1.0):
    a = engine(voice).generate(text, sid=SPK[voice], speed=speed)
    x = np.array(a.samples, dtype=np.float64)
    assert len(x) > 0, text
    return trim(x, TTS_SR)


def trim(x, sr, thresh_db=-40, pre=0.006, post=0.06):
    env = np.convolve(np.abs(x), np.ones(int(0.005 * sr)) / int(0.005 * sr), 'same')
    thr = env.max() * 10 ** (thresh_db / 20)
    idx = np.where(env > thr)[0]
    a = max(0, idx[0] - int(pre * sr))
    b = min(len(x), idx[-1] + int(post * sr))
    y = x[a:b].copy()
    f = int(0.004 * sr)
    y[:f] *= np.linspace(0, 1, f)
    y[-f * 4:] *= np.linspace(1, 0, f * 4)
    return y


def up(x):  # 24k -> 44.1k
    return signal.resample_poly(x, 147, 80)


# ---------------- WORLD processing ----------------
FP = 5.0


def analyze(x, sr=TTS_SR):
    f0, t = pw.harvest(x, sr, f0_floor=65, f0_ceil=750, frame_period=FP)
    sp = pw.cheaptrick(x, f0, t, sr)
    ap = pw.d4c(x, f0, t, sr)
    return f0, sp, ap


def warp_formants(sp, alpha):
    if abs(alpha - 1) < 1e-3:
        return sp
    k = np.arange(sp.shape[1], dtype=float)
    src = k / alpha
    return np.stack([np.interp(src, k, fr) for fr in sp])


def hz2m(f):
    return 69 + 12 * np.log2(np.maximum(f, 1e-6) / 440.0)


def m2hz(m):
    return 440.0 * 2 ** ((np.asarray(m) - 69) / 12)


def snap_scale(m, pcs):
    base = np.round(m)
    best = None
    for d in (0, -1, 1, -2, 2):
        c = base + d
        ok = np.isin(np.mod(c, 12), pcs)
        dist = np.where(ok, np.abs(c - m), 99)
        if best is None:
            best, bd = c.copy(), dist
        else:
            better = dist < bd
            best[better] = c[better]
            bd = np.minimum(bd, dist)
    return best


def hardtune(x, pcs, target_center=None, formant=1.0, contour=0.7):
    """Hard pitch correction: compress the speech contour, snap every frame to the key, hold notes, 1-frame glides."""
    f0, sp, ap = analyze(x)
    v = f0 > 0
    m = hz2m(f0)
    med = np.median(m[v]) if v.any() else 60
    v = v & (m > med - 9)  # drop creaky phrase-end fry instead of tuning it
    shift = float(np.round(target_center - med)) if target_center is not None else 0.0
    mm = np.where(v, med + shift + np.clip((m - med) * contour, -8, 8), 0)
    mm = np.where(v, median_filter(mm, size=5), 0)
    tuned = np.zeros_like(mm)
    cur = None
    for i in range(len(mm)):
        if not v[i]:
            cur = None
            continue
        s = snap_scale(np.array([mm[i]]), pcs)[0]
        if cur is not None and abs(mm[i] - cur) < 0.75:  # hysteresis: hold the note
            s = cur
        cur = s
        tuned[i] = s
    g = tuned.copy()
    for i in range(1, len(tuned)):
        if v[i] and v[i - 1] and tuned[i] != tuned[i - 1]:
            g[i] = 0.5 * (tuned[i] + tuned[i - 1])
    f0n = np.where(v, m2hz(g), 0)
    y = pw.synthesize(f0n, warp_formants(sp, formant), ap, TTS_SR, FP)
    return y, int(round(np.median(tuned[v]))) if v.any() else None


def robot(x, midi_note, formant=1.0):
    f0, sp, ap = analyze(x)
    v = f0 > 0
    f0n = np.where(v, m2hz(midi_note), 0)
    ap2 = ap * 0.6
    return pw.synthesize(f0n, warp_formants(sp, formant), ap2, TTS_SR, FP)


_SYL = {}


def syllable(word, voice):
    k = (word, voice)
    if k not in _SYL:
        x = tts(word, voice)
        _SYL[k] = (x, analyze(x))
    return _SYL[k]


def chop(word, voice, midi_note, dur, formant=1.12, vib=10, attack_ms=35):
    """Sing one syllable on midi_note for dur seconds (stretching the vowel)."""
    x, (f0, sp, ap) = syllable(word, voice)
    v = np.where(f0 > 0)[0]
    a0 = max(0, v[0] - 4)
    a1 = v[-1] + 1
    f0, sp, ap = f0[a0:a1], sp[a0:a1], ap[a0:a1]
    total = max(4, int(round(dur * 1000 / FP)))
    on = min(int(attack_ms / FP), len(f0) - 2, total - 2)
    body_len = total - on
    src = np.linspace(on, len(f0) - 1, body_len)
    idx = np.concatenate([np.arange(on, dtype=float), src])
    i0 = np.floor(idx).astype(int)
    i1 = np.minimum(i0 + 1, len(f0) - 1)
    fr = (idx - i0)[:, None]
    spn = sp[i0] * (1 - fr) + sp[i1] * fr
    apn = ap[i0] * (1 - fr) + ap[i1] * fr
    voiced = (f0[i0] > 0) | (np.arange(total) >= on)
    t = np.arange(total) * FP / 1000
    vib_amt = vib * np.clip((t - 0.15) / 0.2, 0, 1)
    f0n = m2hz(midi_note + vib_amt / 100 * np.sin(2 * np.pi * 5.4 * t))
    f0n = np.where(voiced, f0n, 0)
    y = pw.synthesize(f0n, warp_formants(np.ascontiguousarray(spn), formant), np.ascontiguousarray(apn), TTS_SR, FP)
    r = min(len(y) // 3, int(0.025 * TTS_SR))
    y[-r:] *= np.linspace(1, 0, r)
    return y


# ---------------- FX chains ----------------
def parallel_dist(x, drive, mix):
    d = board(x, [pb.Distortion(drive_db=drive), pb.Gain(-drive * 0.6)])
    return x * (1 - mix) + d * mix


def throw(x, bpm, frac=0.35, fb=0.38, mix=0.4, lp=3800, dotted=True):
    """Delay throw on the last part of a phrase."""
    st = step_seconds(bpm)
    n = len(x)
    cut = int(n * (1 - frac))
    tail = np.zeros_like(x)
    tail[cut:] = x[cut:]
    echoes = pingpong(tail, (3 if dotted else 2) * st, fb, mix, lp, 7)
    out = np.zeros((len(echoes), 2))
    out[:n] += to_stereo(x) if x.ndim == 1 else x
    out += echoes - np.concatenate([to_stereo(tail) * (1 - mix * 0.5), np.zeros((len(echoes) - n, 2))])
    return out


def style_chain(x, style, bpm, pitch=0):
    if style == 'mc':
        y = board(x, [pb.HighpassFilter(110), pb.Compressor(threshold_db=-20, ratio=4, attack_ms=2, release_ms=60),
                      pb.PeakFilter(3000, 3, 0.8), pb.HighShelfFilter(9000, 2.0)])
        y = parallel_dist(y, 10, 0.25)
        y = throw(y, bpm, 0.3, 0.35, 0.35, 4200)
        y = reverb(y, 0.25, 0.08, 0.5, 0.8, 1.0)
    elif style == 'radio':
        y = bfilt(bfilt(x, 'highpass', 380, 4), 'lowpass', 3200, 4)
        y = board(y, [pb.Distortion(drive_db=14), pb.Compressor(threshold_db=-18, ratio=6, attack_ms=1, release_ms=50), pb.Gain(-6)])
        y = throw(y, bpm, 0.3, 0.4, 0.35, 3000)
        y = reverb(y, 0.2, 0.06, 0.6, 0.6, 1.0)
    elif style == 'dark':
        y = board(x, [pb.PitchShift(semitones=pitch)]) if pitch else x
        y = board(y, [pb.HighpassFilter(60), pb.LowShelfFilter(160, 2.5), pb.Compressor(threshold_db=-22, ratio=5, attack_ms=3, release_ms=80),
                      pb.LowpassFilter(11000)])
        y = parallel_dist(y, 12, 0.3)
        y = throw(y, bpm, 0.3, 0.42, 0.32, 3200)
        y = reverb(y, 0.4, 0.1, 0.5, 1.0, 1.0)
    elif style == 'whisper':
        y = board(x, [pb.HighpassFilter(160), pb.Compressor(threshold_db=-28, ratio=6, attack_ms=2, release_ms=70),
                      pb.HighShelfFilter(7500, 4.0)])
        y = to_stereo(y)
        y = board(y, [pb.Chorus(rate_hz=0.8, depth=0.15, mix=0.25, centre_delay_ms=8)])
        y = throw(y, bpm, 0.4, 0.4, 0.3, 5000, dotted=False)
        y = reverb(y, 0.6, 0.2, 0.4, 1.0, 1.0)
    elif style in ('diva', 'tuned'):
        y = board(x, [pb.HighpassFilter(120), pb.Compressor(threshold_db=-18, ratio=3, attack_ms=3, release_ms=80),
                      pb.PeakFilter(2500, 2.0, 0.9), pb.HighShelfFilter(10000, 3.0)])
        y = to_stereo(y)
        if style == 'tuned':
            y = board(y, [pb.Chorus(rate_hz=0.5, depth=0.12, mix=0.2, centre_delay_ms=7)])
        y = throw(y, bpm, 0.35, 0.38, 0.3, 4500)
        y = reverb(y, 0.45, 0.15, 0.45, 1.0, 1.0)
    elif style == 'robot':
        y = board(x, [pb.HighpassFilter(140), pb.Bitcrush(bit_depth=10), pb.Compressor(threshold_db=-20, ratio=6),
                      pb.PeakFilter(1800, 4, 1.0)])
        y = throw(y, bpm, 0.35, 0.45, 0.35, 3500)
        y = reverb(y, 0.35, 0.1, 0.5, 1.0, 1.0)
    elif style == 'chop':
        y = board(x, [pb.HighpassFilter(150), pb.Compressor(threshold_db=-18, ratio=3), pb.HighShelfFilter(9000, 2.5)])
        y = to_stereo(y)
        y = board(y, [pb.Chorus(rate_hz=0.6, depth=0.1, mix=0.18, centre_delay_ms=7)])
        y = reverb(y, 0.5, 0.16, 0.5, 1.0, 1.0)
    else:
        raise ValueError(style)
    return to_stereo(y) if y.ndim == 1 else y


def stutter(x44, bpm, reps=3, chunk=0.07):
    st = step_seconds(bpm)
    c = x44[:int(chunk * SR)].copy()
    f = int(0.004 * SR)
    c[-f:] *= np.linspace(1, 0, f)
    out = np.zeros(int(reps * st * SR) + len(x44))
    for r in range(reps):
        place(out, c, int(r * st * SR), 0.9)
    place(out, x44, int(reps * st * SR), 1.0)
    return out
