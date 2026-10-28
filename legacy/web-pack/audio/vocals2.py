"""Vocal engine v2: sung hooks from lyric syllables (WORLD), doubles + harmony, cleaner chains, ASR-picked takes."""
import os as _os  # noqa: E402
PACK_ROOT = _os.environ.get('PACK_ROOT') or _os.path.abspath(_os.path.join(_os.path.dirname(_os.path.abspath(__file__)), '..'))  # the legacy web-pack folder
MODELS_DIR = _os.environ.get('MODELS_DIR') or _os.path.join(PACK_ROOT, 'models')  # Kokoro TTS + Whisper ONNX models (see README)
import re
import numpy as np
import pyworld as pw
import pedalboard as pb
from scipy import signal
from dsp import SR, bfilt, board, reverb, pingpong, to_stereo, place, warp16, step_seconds
import vocals as V

TTS_SR = V.TTS_SR
FP = V.FP

# ---------------------------------------------------------------- ASR (take picker)
_REC = None


def asr(x, sr):
    global _REC
    import sherpa_onnx
    if _REC is None:
        D = MODELS_DIR + '/sherpa-onnx-whisper-base.en/'
        _REC = sherpa_onnx.OfflineRecognizer.from_whisper(encoder=D + 'base.en-encoder.int8.onnx', decoder=D + 'base.en-decoder.int8.onnx',
                                                         tokens=D + 'base.en-tokens.txt', language='en', task='transcribe', num_threads=2)
    y = signal.resample_poly(x, 16000, sr).astype(np.float32) if sr != 16000 else x.astype(np.float32)
    s = _REC.create_stream(); s.accept_waveform(16000, y); _REC.decode_stream(s)
    return s.result.text.strip()


def words(s):
    return re.sub(r"[^a-z' ]", ' ', s.lower().replace('-', ' ')).replace("'", '').split()


HOMO = {'base': 'bass', 'bassline': 'bass', 'line': 'line', 'selector': 'selecta', 'dump': 'dub'}


def score(text, heard):
    """Share of wanted words heard (repeats are fine for loops), minus a penalty for words that don't belong."""
    want = set(words(text))
    got = [HOMO.get(w, w) for w in words(heard)]
    if not want:
        return 0.0
    hit = sum(1 for w in want if w in got) / len(want)
    noise = sum(1 for g in got if g not in want) / max(1, len(got))
    return hit - 0.3 * noise


def best_take(text, voice, speeds=(1.0, 0.92, 1.08)):
    best = None
    for sp in speeds:
        x = V.tts(text, voice, sp)
        heard = asr(x, TTS_SR)
        sc = score(text, heard) + (0.02 if sp == 1.0 else 0)
        if best is None or sc > best[0]:
            best = (sc, x, sp, heard)
    return best


# ---------------------------------------------------------------- singing
_SYL = {}


def syl(word, voice):
    k = (word, voice)
    if k not in _SYL:
        txt = word if word[-1] in '.!?' else word + '.'
        x = V.tts(txt, voice, 0.95)
        f0, sp, ap = V.analyze(x)
        _SYL[k] = (x, f0, sp, ap)
    return _SYL[k]


def sing_syllable(word, voice, midi, dur, prev=None, vib=18.0, scoop=30.0, cents=0.0):
    """Returns (24 kHz audio, lead-in seconds before the vowel onset)."""
    x, f0, sp, ap = syl(word, voice)
    v = np.flatnonzero(f0 > 0)
    if len(v) == 0:
        return x, 0.0
    i0, i1 = v[0], v[-1] + 1
    on_end = min(i0 + 2, i1)
    co_start = max(on_end + 1, i1 - 6)
    tail_end = min(len(f0), i1 + 14)          # keep the final consonant (t, s, k...) but not the silence after it
    n_on, n_co = on_end, tail_end - co_start
    total = max(8, int(round(dur * 1000 / FP)))
    n_nu = max(4, total - n_on - n_co)
    nu = np.linspace(on_end, co_start - 1, n_nu) if co_start - 1 > on_end else np.full(n_nu, float(on_end))
    idx = np.concatenate([np.arange(n_on, dtype=float), nu, np.arange(co_start, tail_end, dtype=float)])
    a = np.floor(idx).astype(int); b = np.minimum(a + 1, len(f0) - 1); fr = (idx - a)[:, None]
    spn = np.ascontiguousarray(sp[a] * (1 - fr) + sp[b] * fr)
    apn = np.ascontiguousarray(ap[a] * (1 - fr) + ap[b] * fr)
    voiced = f0[np.round(idx).astype(int)] > 0
    voiced[n_on:n_on + n_nu] = True
    t = np.arange(len(idx)) * FP / 1000.0
    tv = np.maximum(0, t - i0 * FP / 1000.0)
    curve = np.full(len(idx), float(midi) + cents / 100.0)
    if prev is not None:   # legato glide from the previous note
        g = np.clip(tv / 0.045, 0, 1)
        curve = prev + (curve - prev) * (1 - (1 - g) ** 2)
    else:                  # small scoop up into the note
        curve -= (scoop / 100.0) * np.exp(-tv / 0.035)
    vib_amt = vib / 100.0 * np.clip((tv - 0.18) / 0.15, 0, 1)
    curve += vib_amt * np.sin(2 * np.pi * 5.3 * tv + np.random.uniform(0, 1))
    f0n = np.where(voiced, V.m2hz(curve), 0.0)
    y = pw.synthesize(f0n, spn, apn, TTS_SR, FP)
    r = min(len(y) // 4, int(0.012 * TTS_SR))
    y[-r:] *= np.linspace(1, 0, r)
    return y, i0 * FP / 1000.0


def sing_line(notes, voice, bpm, swing, bars, cents=0.0, delay=0.0, vib=18.0, octave=0):
    """notes: (step16, len16, midi, syllable). Swung like the kit. Returns 44.1 kHz mono, exact loop length + tail."""
    st = step_seconds(bpm)
    L = int(bars * 16 * st * TTS_SR)
    buf = np.zeros(L + 3 * TTS_SR)
    prev_end, prev_m = -1, None
    for i, (s0, ln, m, w) in enumerate(notes):
        t0 = float(warp16(np.array(float(s0)), swing)) * st
        t1 = float(warp16(np.array(float(s0 + ln)), swing)) * st
        leg = prev_m is not None and abs(s0 - prev_end) < 0.01
        y, lead = sing_syllable(w, voice, m + octave, max(0.08, t1 - t0 - 0.025), prev_m + octave if leg else None, vib, cents=cents)
        at = int((t0 - lead + delay) * TTS_SR)
        if at < 0:
            y = y[-at:]; at = 0
        place(buf, y, at)
        prev_end, prev_m = s0 + ln, m
    return V.up(buf)


def harmony_notes(notes, scale_pcs, up=2):
    """Diatonic third (up=2 scale steps) above each note."""
    out = []
    for s0, ln, m, w in notes:
        cands = [n for n in range(m + 1, m + 6) if n % 12 in scale_pcs]
        out.append((s0, ln, cands[min(up - 1, len(cands) - 1)], w))
    return out


# ---------------------------------------------------------------- processing
def deess(x, f_lo=5500, f_hi=10500, thr_db=-30, ratio=4.0):
    mono = x if x.ndim == 1 else x.mean(axis=1)
    band = bfilt(x, 'bandpass', (f_lo, f_hi), 2)
    bm = band if band.ndim == 1 else band.mean(axis=1)
    k = int(0.004 * SR)
    env = np.sqrt(np.convolve(bm ** 2, np.ones(k) / k, 'same')) + 1e-9
    thr = np.max(np.abs(mono)) * 10 ** (thr_db / 20) + 1e-9
    g = np.where(env > thr, (thr / env) ** (1 - 1 / ratio), 1.0)
    g = np.convolve(g, np.ones(k) / k, 'same')
    return x - band + band * (g if band.ndim == 1 else g[:, None])


def clean(x, male=False):
    """Shared vocal cleanup: HPF, mud cut, de-ess, two-stage compression, presence, air."""
    y = board(x, [pb.HighpassFilter(85 if male else 115), pb.PeakFilter(320, -2.5, 1.0)])
    y = deess(y)
    y = board(y, [pb.Compressor(threshold_db=-20, ratio=4, attack_ms=1.5, release_ms=50),
                  pb.Compressor(threshold_db=-16, ratio=2.5, attack_ms=20, release_ms=180),
                  pb.PeakFilter(3200, 2.5, 0.8), pb.HighShelfFilter(10000, 3.0)])
    return y


def space(x, bpm, verb=0.16, dly=0.18, size=0.5, dotted=True):
    """Pre-delayed plate-ish reverb (return high-passed) + filtered ping-pong delay."""
    x = to_stereo(x)
    pre = np.concatenate([np.zeros((int(0.025 * SR), 2)), x])
    wet = board(pre, [pb.Reverb(room_size=size, damping=0.45, wet_level=1.0, dry_level=0.0, width=1.0), pb.HighpassFilter(320)])
    st = step_seconds(bpm)
    d = pingpong(x, (3 if dotted else 2) * st, 0.36, 1.0, 4200, 7)
    n = max(len(wet), len(d))
    out = np.zeros((n, 2))
    out[:len(x)] += x
    out[:len(wet)] += wet * verb
    out[:len(d)] += (d[:len(d)] - np.concatenate([x * 0.5, np.zeros((len(d) - len(x), 2))])) * dly
    return out


def hook_mix(lead, doubles, harmony, bpm, loop_len):
    """Lead centre, doubles wide, harmony slightly left of centre; glue, space, wrap the tail for a seamless loop."""
    n = max(len(lead), *(len(d) for d in doubles), len(harmony) if harmony is not None else 0)
    pad = lambda a: np.concatenate([a, np.zeros(n - len(a))])
    lead = pad(clean(lead))
    mix = to_stereo(lead)
    pans = [-0.65, 0.65]
    for d, p in zip(doubles, pans):
        mix += to_stereo(pad(clean(d)) * 10 ** (-8 / 20), p)
    if harmony is not None:
        mix += to_stereo(pad(clean(harmony)) * 10 ** (-9 / 20), -0.2)
    mix = board(mix, [pb.Compressor(threshold_db=-14, ratio=2, attack_ms=10, release_ms=120)])
    out = space(mix, bpm, 0.18, 0.17, 0.55)
    loop = out[:loop_len].copy()
    tail = out[loop_len:2 * loop_len]
    loop[:len(tail)] += tail[:len(loop)]
    return loop


def spoken_chain(x, style, bpm, pitch=0, male=False):
    """v2 spoken chain: cleanup first, then the style colour."""
    if style == 'dark' and pitch:
        x = board(x, [pb.PitchShift(semitones=pitch)])
    y = clean(x, male)
    if style == 'mc':
        y = V.parallel_dist(y, 9, 0.22)
        y = V.throw(y, bpm, 0.3, 0.35, 0.33, 4200)
        return reverb(y, 0.25, 0.07, 0.5, 0.8, 1.0)
    if style == 'radio':
        y = bfilt(bfilt(y, 'highpass', 380, 4), 'lowpass', 3300, 4)
        y = board(y, [pb.Distortion(drive_db=12), pb.Compressor(threshold_db=-18, ratio=6, attack_ms=1, release_ms=50), pb.Gain(-6)])
        y = V.throw(y, bpm, 0.3, 0.4, 0.35, 3000)
        return reverb(y, 0.2, 0.06, 0.6, 0.6, 1.0)
    if style == 'dark':
        y = board(y, [pb.LowShelfFilter(160, 2.5), pb.LowpassFilter(11500)])
        y = V.parallel_dist(y, 11, 0.28)
        y = V.throw(y, bpm, 0.3, 0.42, 0.3, 3200)
        return reverb(y, 0.4, 0.1, 0.5, 1.0, 1.0)
    if style == 'whisper':
        y = board(y, [pb.HighShelfFilter(7500, 3.0)])
        y = board(to_stereo(y), [pb.Chorus(rate_hz=0.8, depth=0.15, mix=0.22, centre_delay_ms=8)])
        y = V.throw(y, bpm, 0.4, 0.4, 0.28, 5000, dotted=False)
        return reverb(y, 0.6, 0.18, 0.4, 1.0, 1.0)
    if style == 'preacher':  # warm, close, big room behind
        y = board(y, [pb.LowShelfFilter(180, 2.0), pb.PeakFilter(2500, 1.5, 0.8)])
        y = V.parallel_dist(y, 6, 0.15)
        return space(y, bpm, 0.24, 0.12, 0.75)
    if style == 'diva':
        y = board(to_stereo(y), [pb.Chorus(rate_hz=0.5, depth=0.1, mix=0.15, centre_delay_ms=7)])
        return space(y, bpm, 0.16, 0.18, 0.5)
    if style == 'adlib':
        return space(y, bpm, 0.14, 0.22, 0.45)
    raise ValueError(style)


# ---------------------------------------------------------------- whole-word singing (multi-syllable words keep their coarticulation)
_WORD = {}


def word_data(word, voice):
    k = (word, voice)
    if k not in _WORD:
        txt = word if word[-1] in '.!?' else word + '.'
        x = V.tts(txt, voice, 0.95)
        f0, sp, ap = V.analyze(x)
        en = np.log(np.sum(sp[:, 8:200], axis=1) + 1e-12)
        _WORD[k] = (x, f0, sp, ap, en)
    return _WORD[k]


def _splits(f0, en, k):
    v = np.flatnonzero(f0 > 0)
    if k <= 1 or len(v) < 4:
        return []
    i0, i1 = v[0], v[-1] + 1
    span = i1 - i0
    pts = []
    for j in range(1, k):
        c = i0 + span * j / k
        lo, hi = int(max(i0 + 2, c - span * 0.2)), int(min(i1 - 2, c + span * 0.2))
        if hi <= lo:
            pts.append(int(c)); continue
        pts.append(lo + int(np.argmin(en[lo:hi])))
    return pts


def sing_word(word, voice, notes, prev=None, vib=18.0, cents=0.0, scoop=30.0):
    """notes: [(t_start, t_end, midi)] absolute seconds, one per syllable.
    Returns (24 kHz audio, absolute start time to place it)."""
    x, f0, sp, ap, en = word_data(word, voice)
    k = len(notes)
    bounds = [0] + _splits(f0, en, k) + [len(f0)]
    if len(bounds) != k + 1:
        bounds = list(np.linspace(0, len(f0), k + 1).astype(int))
    segs = []
    for j in range(k):
        a, b = bounds[j], bounds[j + 1]
        fv = np.flatnonzero(f0[a:b] > 0) + a
        if len(fv) == 0:
            segs.append((a, a, a, b, b)); continue
        i0, i1 = fv[0], fv[-1] + 1
        on_end = min(i0 + 2, i1)
        co_start = max(on_end + 1, i1 - 5)
        tail_end = b if j < k - 1 else min(len(f0), i1 + 14)
        segs.append((a, i0, on_end, co_start, tail_end))
    fp = FP / 1000.0
    lead = [(s[2] - s[0]) * fp for s in segs]           # time from segment start to vowel (after 2 voiced frames)
    begin = [notes[j][0] - lead[j] for j in range(k)]
    idx_all, curve_all, vm_all = [], [], []
    last_m = prev
    for j, (a, i0, on_end, co_start, tail_end) in enumerate(segs):
        m = notes[j][2] + cents / 100.0
        if j < k - 1:
            budget = int(round((begin[j + 1] - begin[j]) / fp))
        else:
            budget = int(round((notes[j][1] - notes[j][0] - 0.02) / fp)) + (on_end - a)
        n_on, n_co = on_end - a, tail_end - co_start
        n_nu = max(3, budget - n_on - n_co)
        nu = np.linspace(on_end, max(on_end, co_start - 1), n_nu)
        idx = np.concatenate([np.arange(a, on_end, dtype=float), nu, np.arange(co_start, tail_end, dtype=float)])
        vm = f0[np.minimum(len(f0) - 1, np.round(idx).astype(int))] > 0
        vm[n_on:n_on + n_nu] = True
        t = np.arange(len(idx)) * fp
        tv = np.maximum(0, t - (i0 - a) * fp)
        cur = np.full(len(idx), m)
        if last_m is not None:
            g = np.clip(tv / 0.045, 0, 1)
            cur = last_m + (cur - last_m) * (1 - (1 - g) ** 2)
        else:
            cur -= (scoop / 100.0) * np.exp(-tv / 0.035)
        va = vib / 100.0 * np.clip((tv - 0.18) / 0.15, 0, 1)
        cur += va * np.sin(2 * np.pi * 5.3 * tv + np.random.uniform(0, 1))
        idx_all.append(idx); curve_all.append(cur); vm_all.append(vm)
        last_m = m
    idx = np.concatenate(idx_all); curve = np.concatenate(curve_all); vm = np.concatenate(vm_all)
    a_ = np.floor(idx).astype(int); b_ = np.minimum(a_ + 1, len(f0) - 1); fr = (idx - a_)[:, None]
    spn = np.ascontiguousarray(sp[a_] * (1 - fr) + sp[b_] * fr)
    apn = np.ascontiguousarray(ap[a_] * (1 - fr) + ap[b_] * fr)
    y = pw.synthesize(np.where(vm, V.m2hz(curve), 0.0), spn, apn, TTS_SR, FP)
    r = min(len(y) // 4, int(0.012 * TTS_SR))
    y[-r:] *= np.linspace(1, 0, r)
    return y, begin[0]


def sing_line2(notes, voice, bpm, swing, bars, cents=0.0, delay=0.0, vib=18.0, octave=0):
    """notes: (step16, len16, midi, text); text '~' continues the previous word on a new syllable."""
    st = step_seconds(bpm)
    L = int(bars * 16 * st * TTS_SR)
    buf = np.zeros(L + 3 * TTS_SR)
    T = lambda s: float(warp16(np.array(float(s)), swing)) * st
    words_ = []
    for s0, ln, m, w in notes:
        if w == '~' and words_:
            words_[-1][1].append((s0, ln, m))
        else:
            words_.append([w, [(s0, ln, m)]])
    prev_end, prev_m = None, None
    for w, syls in words_:
        nts = [(T(s0), T(s0 + ln), m + octave) for s0, ln, m in syls]
        leg = prev_end is not None and abs(syls[0][0] - prev_end) < 0.01
        y, at = sing_word(w, voice, nts, prev_m if leg else None, vib, cents)
        at = int((at + delay) * TTS_SR)
        if at < 0:
            y = y[-at:]; at = 0
        place(buf, y, at)
        prev_end = syls[-1][0] + syls[-1][1]; prev_m = syls[-1][2] + octave
    return V.up(buf)
