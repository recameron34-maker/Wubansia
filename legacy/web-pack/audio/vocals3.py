"""Vocal engine v3.

Whole phrases are spoken by Kokoro (run directly through onnxruntime so the model's own phoneme durations come back),
then sung through WORLD: every syllable's consonants keep close to their natural length and land just before the beat,
the vowel is stretched over the note (transitions at natural speed, steady part stretched), and the pitch is a sung
contour (scoops into phrases, legato glides, delayed vibrato with drifting rate and depth, small pitch drift, falls at
phrase ends). Doubles are separate performances (a blended voice, a different take speed, humanised timing and tuning),
harmonies follow the scale, and the mix uses a plate-style convolution reverb, a delay that ducks under the voice,
de-essing, gentle saturation and loudness matching.
"""
import os as _os  # noqa: E402
PACK_ROOT = _os.environ.get('PACK_ROOT') or _os.path.abspath(_os.path.join(_os.path.dirname(_os.path.abspath(__file__)), '..'))  # the legacy web-pack folder
MODELS_DIR = _os.environ.get('MODELS_DIR') or _os.path.join(PACK_ROOT, 'models')  # Kokoro TTS + Whisper ONNX models (see README)
import functools, re
import numpy as np
import onnxruntime as ort
import pyworld as pw
import pedalboard as pb
from scipy import signal
from dsp import SR, bfilt, board, to_stereo, warp16, step_seconds

MD = MODELS_DIR + '/kokoro-multi-lang-v1_0/'
TTS_SR = 24000
HOP = 600                    # one Kokoro duration frame = 25 ms
FP = 5.0                     # WORLD frame period, ms
FPS = FP / 1000.0
NAMES = ('af_alloy,af_aoede,af_bella,af_heart,af_jessica,af_kore,af_nicole,af_nova,af_river,af_sarah,af_sky,am_adam,am_echo,am_eric,am_fenrir,'
         'am_liam,am_michael,am_onyx,am_puck,am_santa,bf_alice,bf_emma,bf_isabella,bf_lily,bm_daniel,bm_fable,bm_george,bm_lewis').split(',')
SPK = {n: i for i, n in enumerate(NAMES)}
VOWELS = set('AIOQWYaeiouɑɐɒæəɚɛɜɪʊʌɔᵊɨɯøœɤᵻ')   # Kokoro: A eɪ, I aɪ, O oʊ (US), Q əʊ (GB), W aʊ, Y ɔɪ
STRESS = set('ˈˌ')
PUNCT = set(';:,.!?—…')
EXTRA_LEX = {   # words the lexicon doesn't have
    'selecta': 's ɪ l ˈ ɛ k t ə', 'vip': 'v ˌ i ˌ I p ˈ i', 'eew': 'ˈ i u', 'dancefloor': 'd ˈ æ n s f l ˌ ɔ ɹ', 'mmm': 'm m m',
    'mm': 'm m', 'ooh': 'ˈ u ː', 'aah': 'ˈ ɑ ː', 'ahh': 'ˈ ɑ ː', 'oooh': 'ˈ u ː', 'whoa': 'w ˈ O', 'ayy': 'ˈ A', 'uh': 'ˈ ʌ',
    'yeah': 'j ˈ ɛ ə', 'na': 'n ˈ ɑ', 'la': 'l ˈ ɑ', 'da': 'd ˈ ɑ', 'doo': 'd ˈ u', 'hoo': 'h ˈ u', 'ha': 'h ˈ ɑ'}


# ================================================================= Kokoro, run directly
class Kokoro:
    def __init__(self):
        self.sess = ort.InferenceSession(MD + 'model.onnx', providers=['CPUExecutionProvider'])
        self.tok = {}
        for line in open(MD + 'tokens.txt', encoding='utf-8'):
            line = line.rstrip('\n')
            if not line:
                continue
            sym, i = line.rsplit(' ', 1)
            self.tok[sym if sym else ' '] = int(i)
        self.voices = np.fromfile(MD + 'voices.bin', dtype=np.float32).reshape(-1, 510, 256)
        self.lex = {}

    def lexicon(self, gb):
        k = 'gb' if gb else 'us'
        if k not in self.lex:
            d = {}
            for line in open(MD + f'lexicon-{k}-en.txt', encoding='utf-8'):
                p = line.rstrip('\n').split(' ')
                if p[0] not in d:
                    d[p[0]] = p[1:]
            self.lex[k] = d
        return self.lex[k]

    def word_phones(self, w, gb):
        key = w.lower().strip(".,!?;:\"“”()").replace('’', "'")
        if key in EXTRA_LEX:
            return EXTRA_LEX[key].split()
        lex = self.lexicon(gb)
        if key in lex:
            return lex[key]
        if key.replace("'", '') in lex:
            return lex[key.replace("'", '')]
        if '-' in key:
            out = []
            for part in key.split('-'):
                out += self.word_phones(part, gb)
            return out
        raise KeyError(f'no phonemes for {w!r}')

    def style(self, blend, n):
        """blend: voice name or {voice: weight}"""
        if isinstance(blend, str):
            blend = {blend: 1.0}
        tot = sum(blend.values())
        return sum(self.voices[SPK[v], min(509, n - 1)] * (wt / tot) for v, wt in blend.items())[None, :].astype(np.float32)

    def speak(self, words, blend, speed=1.0, gb=None, glottal=False):
        """words: list of word strings; punctuation may trail a word (',' '.' '!' '?'). Returns (audio 24 kHz, tokens) where
        tokens = [{'sym', 'word', 't0', 't1'}] with times in seconds (word = index into words, -1 for spaces/punctuation/pads).
        glottal: start vowel-initial words (after the first) with a glottal stop, as singers do, instead of linking them."""
        if gb is None:
            gb = (blend if isinstance(blend, str) else max(blend, key=blend.get)).startswith('b')
        seq = []
        for wi, w in enumerate(words):
            if wi:
                seq.append((' ', -1))
            ph = self.word_phones(w, gb)
            first = next((p for p in ph if p not in STRESS), '')
            if glottal and wi and first in VOWELS:
                seq.append(('ʔ', wi))
            for p in ph:
                seq.append((p, wi))
            tail = re.findall(r'[,.!?;:]+$', w)
            if tail:
                for ch in tail[0][:1]:
                    seq.append((ch, -1))
        if not seq or seq[-1][0] not in PUNCT:
            seq.append(('.', -1))
        ids = [self.tok[s] for s, _ in seq]
        a, d = self.sess.run(None, {'tokens': np.array([[0] + ids + [0]], dtype=np.int64), 'style': self.style(blend, len(ids)),
                                    'speed': np.array([speed], dtype=np.float32)})
        d = np.asarray(d, dtype=float)
        edges = np.concatenate([[0], np.cumsum(d)]) * HOP / TTS_SR
        toks = [dict(sym=s, word=wi, t0=edges[i + 1], t1=edges[i + 2]) for i, (s, wi) in enumerate(seq)]
        return np.asarray(a, dtype=np.float64), toks


@functools.lru_cache(maxsize=1)
def KO():
    return Kokoro()


# ================================================================= WORLD analysis
def analyze(x):
    f0, t = pw.harvest(x, TTS_SR, f0_floor=60, f0_ceil=700, frame_period=FP)
    sp = pw.cheaptrick(x, f0, t, TTS_SR)
    ap = pw.d4c(x, f0, t, TTS_SR)
    return f0, sp, ap


@functools.lru_cache(maxsize=64)
def performance(text_key, blend_key, speed, glottal=True, draw=0):
    """Spoken take of a line + its WORLD analysis (cached per line, voice blend, speed and glottal onsets; draw is only part of the
    cache key: the TTS is deterministic)."""
    words = text_key.split(' ')
    blend = dict(blend_key)
    x, toks = KO().speak(words, blend, speed, glottal=glottal)
    f0, sp, ap = analyze(x)
    toks = align_tokens(toks, f0, x)
    return x, toks, f0, sp, ap


VOICED_C = set('lmnŋɹwjvzðbdɡʒʤɾ')
UNVOICED_C = set('tkpsʃfθhʧʔ')


def align_tokens(toks, f0, x=None):
    """The TTS's audio runs ahead of its own duration timeline (50-115 ms, varying), so re-time the tokens from acoustic
    landmarks: voicing onsets after unvoiced sounds, voicing offsets before them, and the burst/hiss onset of an unvoiced
    consonant after a pause. Each landmark gives a correction; boundaries in between are corrected by interpolation."""
    voiced = f0 > 0
    n = len(voiced)
    hf = None
    if x is not None:
        y = signal.sosfiltfilt(signal.butter(4, [2500, 9000], 'bandpass', fs=TTS_SR, output='sos'), x)
        k = int(0.004 * TTS_SR)
        env = np.sqrt(np.convolve(y ** 2, np.ones(k) / k, 'same'))
        hf = env[::int(FPS * TTS_SR)][:n]
    cls = []
    for t in toks:
        sym = t['sym']
        cls.append('v' if (sym in VOWELS or sym in VOICED_C) else 'u' if sym in UNVOICED_C else 'p' if (sym in PUNCT or sym == ' ') else 's')
    # stress marks belong to the vowel after them
    for i in range(len(cls) - 1, -1, -1):
        if cls[i] == 's':
            cls[i] = cls[i + 1] if i + 1 < len(cls) else 'p'
    marks = []
    EXP = -0.075
    for i in range(1, len(toks)):
        a_, b_ = cls[i - 1], cls[i]
        if a_ == b_:
            continue
        t = toks[i]['t0']
        lo, hi = max(0, int((t - 0.2) / FPS)), min(n - 1, int((t + 0.06) / FPS))
        if hi - lo < 4:
            continue
        cand = None
        if b_ == 'v' and a_ in 'up':                 # voicing starts
            on = [f for f in range(lo, hi) if voiced[f] and voiced[f + 1] and (f == 0 or not voiced[f - 1])]
            cand = [f * FPS for f in on]
        elif a_ == 'v' and b_ in 'up':               # voicing stops
            off = [f for f in range(lo + 1, hi + 1) if not voiced[f] and voiced[f - 1]]
            cand = [f * FPS for f in off]
        elif a_ == 'p' and b_ == 'u' and hf is not None:   # hiss / burst after a pause
            seg = hf[lo:hi]
            if seg.max() > 0:
                f = lo + int(np.argmax(seg > 0.25 * seg.max()))
                cand = [f * FPS]
        if cand:
            best = min(cand, key=lambda c: abs(c - (t + EXP)))
            marks.append((t, best - t))
    if x is not None and toks:                       # where the utterance starts and stops in the audio
        k = int(0.01 * TTS_SR)
        e = np.sqrt(np.convolve(x ** 2, np.ones(k) / k, 'same'))
        live = np.flatnonzero(e > e.max() * 10 ** (-38 / 20))
        if len(live):
            first = next((t for t in toks if t['word'] >= 0), None)
            last = next((t for t in reversed(toks) if t['word'] >= 0), None)
            if first is not None:
                marks.append((first['t0'], live[0] / TTS_SR - first['t0']))
            if last is not None:
                marks.append((last['t1'], live[-1] / TTS_SR - last['t1']))
    marks.sort()
    if marks:
        cs = np.array([c for _, c in marks])
        med = float(np.median(cs))
        marks = [(t, c) for t, c in marks if abs(c - med) <= 0.06]
    if not marks:
        marks = [(0.0, EXP)]
    mt = np.array([t for t, _ in marks])
    mc = np.array([c for _, c in marks])
    corr = lambda t: float(np.interp(t, mt, mc))
    out = []
    for t in toks:
        out.append(dict(t, t0=max(0.0, t['t0'] + corr(t['t0'])), t1=max(0.0, t['t1'] + corr(t['t1'])), lag=round(corr(t['t0']), 3)))
    for i in range(len(out) - 1):                 # keep the tokens contiguous and in order
        out[i]['t1'] = out[i + 1]['t0'] = max(out[i]['t0'], min(out[i]['t1'], out[i + 1]['t0']) if out[i]['t1'] > out[i + 1]['t0'] else out[i + 1]['t0'])
    return out


# ================================================================= syllables
def syllables_of(toks, wi):
    """Split word wi's tokens into syllables: [{'on': (t0, t1), 'nu': (t0, t1), 'co': (t0, t1), 'syms': str}] (source seconds).
    Onsets take one consonant between vowels (two or more: the first goes to the coda); a following word-gap token joins the last coda."""
    idx = [i for i, t in enumerate(toks) if t['word'] == wi]
    if not idx:
        return []
    tk = [toks[i] for i in idx]
    nuc = [i for i, t in enumerate(tk) if t['sym'] in VOWELS]
    if not nuc:                     # 'mmm': the longest token is the nucleus
        nuc = [int(np.argmax([t['t1'] - t['t0'] for t in tk]))]
    starts = [0]
    for a, b in zip(nuc, nuc[1:]):
        cons = [i for i in range(a + 1, b) if tk[i]['sym'] not in STRESS and tk[i]['sym'] != 'ː']
        starts.append(b if not cons else cons[0] if len(cons) == 1 else cons[1])
    ends = starts[1:] + [len(tk)]
    out = []
    for s, n, e in zip(starts, nuc, ends):
        n0 = n
        while n0 - 1 >= s and tk[n0 - 1]['sym'] in STRESS:
            n0 -= 1
        n1 = n
        while n1 + 1 < e and tk[n1 + 1]['sym'] == 'ː':
            n1 += 1
        out.append(dict(on=(tk[s]['t0'], tk[n0]['t0']), nu=(tk[n0]['t0'], tk[n1]['t1']), co=(tk[n1]['t1'], tk[e - 1]['t1']),
                        syms=''.join(t['sym'] for t in tk[s:e] if t['sym'] not in STRESS)))
    nxt = idx[-1] + 1
    if nxt < len(toks) and toks[nxt]['sym'] == ' ':
        out[-1]['co'] = (out[-1]['co'][0], toks[nxt]['t1'])
    return out


def fit_syllables(syls, k):
    """Exactly k sung syllables: the first group absorbs the extra syllables (its longest vowel is the one held, what comes before it
    is sung as a pickup, what comes after at natural speed); with fewer syllables than notes the last vowel carries the extra notes."""
    if k <= 0 or not syls or len(syls) <= k:
        return syls
    g = syls[:len(syls) - k + 1]
    keep = max(range(len(g)), key=lambda i: g[i]['nu'][1] - g[i]['nu'][0])
    kv = g[keep]
    merged = dict(on=(g[0]['on'][0], kv['nu'][0]), nu=kv['nu'], co=g[-1]['co'], syms=''.join(x['syms'] for x in g), pickup=keep > 0)
    if keep < len(g) - 1:
        merged['tail'] = (kv['nu'][1], g[-1]['nu'][1])
        merged['co'] = g[-1]['co']
    else:
        merged['co'] = kv['co']
    return [merged] + syls[len(g):]


# ================================================================= the sung line
def _ease(x):
    x = np.clip(x, 0, 1)
    return x * x * (3 - 2 * x)


class Sing:
    """Parameters of one performance (lead, double or harmony)."""

    def __init__(self, blend, speed=0.8, vib=32.0, vib_rate=5.4, scoop=55.0, drift=5.0, breath=0.18, timing=0.0, tuning=0.0,
                 formant=1.0, fall=True, seed=0, consonants=1.0, swell=1.5, breaths=True, glide=0.025, air=1.0, glottal=True, draw=0, cboost=0.0):
        self.blend, self.speed, self.vib, self.vib_rate, self.scoop, self.drift = blend, speed, vib, vib_rate, scoop, drift
        self.breath, self.timing, self.tuning, self.formant, self.fall, self.seed = breath, timing, tuning, formant, fall, seed
        self.consonants, self.swell, self.breaths, self.glide, self.air, self.glottal, self.draw, self.cboost = consonants, swell, breaths, glide, air, glottal, draw, cboost


def group_words(notes):
    """notes: (step16, len16, midi, text) with '~' continuing the previous word -> [[word, [(s0, ln, m), ...]], ...]"""
    words_ = []
    for s0, ln, m, w in notes:
        if w == '~' and words_:
            words_[-1][1].append((s0, ln, m))
        else:
            words_.append([w, [(s0, ln, m)]])
    return words_


def line_text(words_):
    """Lyric words with a comma where the music rests (so the spoken take phrases like the song)."""
    out = []
    for i, (w, nts) in enumerate(words_):
        end = nts[-1][0] + nts[-1][1]
        nxt = words_[i + 1][1][0][0] if i + 1 < len(words_) else None
        ww = w.rstrip('.,!?')
        if nxt is None:
            ww += '.'
        elif nxt - end >= 3:
            ww += ','
        out.append(ww)
    return out


ON_CAP, CO_CAP = 0.16, 0.12


def sing(notes, bpm, swing, S, total_sec, pitch_shift=0, pre=0.0, loop_sec=None):
    """Sing a line (notes in 16th steps, swung like the kit) as one performance. Returns 44.1 kHz mono, pre + total_sec long:
    with a pre-roll the first onset consonant and breath, which come before the downbeat, are kept (a loop wraps them to its end)."""
    rng = np.random.default_rng(S.seed)
    st = step_seconds(bpm)
    T = lambda s: float(warp16(np.array(float(s)), swing)) * st + pre
    total_sec = total_sec + pre
    words_ = group_words(notes)
    text = line_text(words_)
    blend_key = tuple(sorted((S.blend if isinstance(S.blend, dict) else {S.blend: 1.0}).items()))
    x, toks, f0s, sps, aps = performance(' '.join(text), blend_key, S.speed, S.glottal, S.draw)
    nsrc = len(f0s)
    # ---------- syllables and their notes ----------
    events = []
    for wi, (w, nts) in enumerate(words_):
        syls = fit_syllables(syllables_of(toks, wi), len(nts))
        if not syls:
            continue
        groups = [[n] for n in nts[:len(syls)]]
        for extra in nts[len(syls):]:          # more notes than syllables: melisma on the last vowel
            groups[-1].append(extra)
        for sy, g in zip(syls, groups):
            off = float(np.clip(rng.normal(0, S.timing), -2.2 * S.timing, 2.2 * S.timing)) if S.timing else 0.0
            nl = [(T(s0) + (off if k == 0 else 0.0), T(s0 + ln), m + pitch_shift) for k, (s0, ln, m) in enumerate(g)]
            events.append(dict(sy, notes=nl, word=wi))
    # trim each held vowel to its strong part (spoken phrase ends fade and creak; a sung note shouldn't)
    E = np.log(np.sum(sps, axis=1) + 1e-12)
    for ev in events:
        n0, n1 = ev['nu']
        a_, b_ = int(n0 / FPS), max(int(n0 / FPS) + 1, int(n1 / FPS))
        seg = E[a_:b_]
        if len(seg) >= 6:
            strong = np.flatnonzero(seg >= seg.max() - 2.0)
            last = a_ + strong[-1] + 1
            keep = max(last, a_ + int(0.6 * (b_ - a_)))
            ev['nu'] = (n0, min(n1, keep * FPS))
    # ---------- time map: onsets end on the beat, codas end where the next onset starts (or at the note end before a rest) ----------
    segs = []   # (out0, out1, src0, src1, kind) kind: 1 onset, 2 vowel, 3 coda
    def on_span(ev):
        a, b = ev['on']
        cap = 0.34 if ev.get('pickup') else ON_CAP
        return (max(a, b - cap), b)
    def co_span(ev):
        a, b = ev['co']
        return (a, min(b, a + CO_CAP))
    for i, ev in enumerate(events):
        t_on, t_end = ev['notes'][0][0], ev['notes'][-1][1]
        nxt = events[i + 1] if i + 1 < len(events) else None
        oa, ob = on_span(ev)
        ca, cb = co_span(ev)
        od, cd = (ob - oa) * S.consonants, (cb - ca) * S.consonants
        legato = nxt is not None and nxt['notes'][0][0] - t_end < 0.03
        nod = 0.0
        if legato:
            na, nb = on_span(nxt)
            nod = (nb - na) * S.consonants
            ve = nxt['notes'][0][0] - nod - cd
        else:
            ve = t_end - 0.015 - 0.6 * cd
        min_v = max(0.045, 0.45 * (t_end - t_on))
        if ve - t_on < min_v:                     # fast notes: squeeze the consonants between the vowels
            limit_t = nxt['notes'][0][0] if legato else t_end
            room = max(0.0, limit_t - t_on - min_v)
            need = cd + nod
            f = max(0.3, min(1.0, room / need)) if need > 0 else 1.0
            cd *= f
            if legato:
                nxt['_od_scale'] = f
            ve = max(t_on + 0.03, limit_t - cd - nod * f)
        # onset
        od_i = od * ev.get('_od_scale', 1.0)
        if od_i > 0.002:
            prev_end = segs[-1][1] if segs else -1.0
            o0 = max(t_on - od_i, prev_end)
            if t_on - o0 > 0.002:
                segs.append((o0, t_on, oa, ob, 1, i))
        # vowel (+ tail of merged syllables)
        n0, n1 = ev['nu']
        tail = ev.get('tail')
        vt = ve - t_on
        if tail:
            tl = min(tail[1] - tail[0], 0.4 * vt)
            vt -= tl
        nl_ = n1 - n0
        a = min(0.035, 0.3 * nl_)
        b = min(0.045, 0.3 * nl_)
        if vt <= a + b + 0.012:
            segs.append((t_on, t_on + vt, n0, n1, 2, i))
        else:
            segs += [(t_on, t_on + a, n0, n0 + a, 2, i), (t_on + a, t_on + vt - b, n0 + a, n1 - b, 2, i), (t_on + vt - b, t_on + vt, n1 - b, n1, 2, i)]
        cur = t_on + vt
        if tail:
            segs.append((cur, cur + tl, tail[0], tail[1], 2, i))
            cur += tl
        if cd > 0.002:
            segs.append((cur, cur + cd, ca, cb, 3, i))
        ev['ve'] = cur
    # ---------- frame grids ----------
    nout = int(total_sec / FPS) + 1
    tout = np.arange(nout) * FPS
    src_t = np.full(nout, -1.0)
    kind = np.zeros(nout, dtype=int)
    evix = np.full(nout, -1)
    for o0, o1, s0, s1, k, i in segs:
        a, b = max(0, int(np.ceil(o0 / FPS))), min(nout, int(np.ceil(o1 / FPS)))
        if b <= a:
            continue
        f = (tout[a:b] - o0) / max(o1 - o0, 1e-9)
        src_t[a:b] = s0 + f * (s1 - s0)
        kind[a:b] = k
        evix[a:b] = i
    silent_src = 0.5 * toks[0]['t0'] if toks[0]['t0'] > 0.05 else 0.0
    sf_ = np.where(src_t >= 0, src_t, silent_src) / FPS
    i0 = np.clip(np.floor(sf_).astype(int), 0, nsrc - 1)
    i1 = np.clip(i0 + 1, 0, nsrc - 1)
    fr = (sf_ - np.floor(sf_))[:, None]
    lsp = np.log(sps + 1e-16)
    sp = np.exp(lsp[i0] * (1 - fr) + lsp[i1] * fr)
    ap = aps[i0] * (1 - fr) + aps[i1] * fr
    src_voiced = (f0s[i0] > 0) & (f0s[i1] > 0)
    voiced = (kind == 2) | (((kind == 1) | (kind == 3)) & src_voiced)
    sp[kind == 0] *= 1e-7
    # ---------- pitch: the owning note of every frame, smoothed into glides ----------
    allnotes = []
    for i, ev in enumerate(events):
        for n in ev['notes']:
            allnotes.append((n[0], n[1], n[2], i))
    target = np.zeros(nout)
    owner = np.full(nout, -1)
    for f in range(nout):
        i = evix[f]
        if i < 0 or kind[f] == 0:
            continue
        nts = events[i]['notes']
        if kind[f] == 1:
            j = 0
        elif kind[f] == 3:
            j = len(nts) - 1
        else:
            j = 0
            for q, n in enumerate(nts):
                if tout[f] >= n[0] - 0.01:
                    j = q
        target[f] = nts[j][2]
        owner[f] = i * 100 + j
    mid = np.zeros(nout)
    alpha = 1 - np.exp(-FPS / max(1e-3, S.glide))
    cur = None
    for f in range(nout):
        if target[f] == 0:
            cur = None
            continue
        if cur is None or (f > 0 and target[f - 1] == 0):
            cur = target[f]
        cur += alpha * (target[f] - cur)
        mid[f] = cur
    cents = np.zeros(nout)
    vib_phase = rng.uniform(0, 2 * np.pi)
    phrase_start = np.zeros(nout, dtype=bool)
    phrase_start[1:] = (target[1:] > 0) & (target[:-1] == 0)
    phrase_start[0] = target[0] > 0
    for f in np.flatnonzero(phrase_start):        # scoop into each phrase
        sc = S.scoop * (0.7 + 0.6 * rng.random())
        n = int(0.22 / FPS)
        tt = np.arange(min(n, nout - f)) * FPS
        on_t = None
        i = evix[f]
        if i >= 0:
            on_t = events[i]['notes'][0][0]
        lag = max(0.0, (on_t - tout[f]) if on_t is not None else 0.0)
        cents[f:f + len(tt)] -= sc * np.exp(-np.maximum(0, tt - lag) / 0.04)
    for q, (t_on, t_off, m, i) in enumerate(allnotes):
        a = int(max(0, t_on / FPS))
        nxt_on = allnotes[q + 1][0] if q + 1 < len(allnotes) else t_off + 0.3
        b = int(min(nout, nxt_on / FPS))
        if b <= a:
            continue
        tt = tout[a:b] - t_on
        dur = t_off - t_on
        if dur > 0.3 and S.vib > 0:
            depth = S.vib * (0.8 + 0.4 * rng.random()) * _ease((tt - 0.16) / 0.3)
            rate = S.vib_rate * (1 + 0.05 * np.sin(2 * np.pi * 0.6 * tt + rng.uniform(0, 6)))
            ph = vib_phase + 2 * np.pi * np.cumsum(rate) * FPS
            cents[a:b] += depth * np.sin(ph)
            vib_phase = ph[-1]
        last_in_phrase = (q + 1 == len(allnotes)) or (allnotes[q + 1][0] - t_off > 0.25)
        if S.fall and last_in_phrase and dur > 0.5 and rng.random() < 0.55:
            fl = np.clip((tt - (dur - 0.15)) / 0.15, 0, 1)
            cents[a:b] -= 150 * fl ** 2
        if q > 0 and m - allnotes[q - 1][2] > 4 and t_on - allnotes[q - 1][1] < 0.06:   # overshoot on big leaps up
            cents[a:b] += 18 * np.exp(-np.maximum(0, tt - 0.07) / 0.05) * (tt > 0.05)
        if S.tuning:
            cents[a:b] += rng.normal(0, S.tuning)
    if S.drift and nout > 40:
        nz = rng.normal(0, 1, nout // 20 + 3)
        drift = np.interp(np.arange(nout) / 20, np.arange(len(nz)), nz)
        drift = signal.filtfilt(*signal.butter(2, 0.04), drift)
        cents += S.drift * drift / (np.std(drift) + 1e-9)
    f0 = np.where(voiced & (mid > 0), 440.0 * 2 ** ((mid + cents / 100 - 69) / 12), 0.0)
    # ---------- dynamics, breathiness, formant ----------
    gain = np.ones(nout)
    for q, (t_on, t_off, m, i) in enumerate(allnotes):
        a, b = int(max(0, t_on / FPS)), int(min(nout, t_off / FPS))
        if b - a < 4:
            continue
        tt = (tout[a:b] - t_on) / max(1e-3, t_off - t_on)
        sw = S.swell * np.sin(np.pi * np.clip(tt / 0.85, 0, 1)) - 2.0 * np.clip((tt - 0.8) / 0.2, 0, 1)
        gain[a:b] *= 10 ** (sw / 20)
    if S.cboost:                                   # consonants up (intelligibility), eased in and out
        cmask = signal.filtfilt(*signal.butter(1, 0.25), ((kind == 1) | (kind == 3)).astype(float))
        gain *= 10 ** (S.cboost * np.clip(cmask, 0, 1) / 20)
    gain = signal.filtfilt(*signal.butter(1, 0.1), gain)
    # hold sustained vowels steady: pull each vowel frame's energy most of the way to the note's median
    Eo = np.log(np.sum(sp, axis=1) + 1e-12)
    flat = np.zeros(nout)
    for q, (t_on, t_off, m, i) in enumerate(allnotes):
        a, b = int(max(0, t_on / FPS)), int(min(nout, t_off / FPS))
        vv = np.arange(a, b)[kind[a:b] == 2]
        if len(vv) >= 12:
            ref = np.median(Eo[vv])
            flat[vv] = np.clip(0.7 * (ref - Eo[vv]), -1.5, 2.3)
    flat = signal.filtfilt(*signal.butter(1, 0.15), flat)
    sp *= np.exp(flat)[:, None]
    sp *= (gain ** 2)[:, None]
    fb = np.linspace(0, TTS_SR / 2, ap.shape[1])
    cap = np.interp(fb, [0, 1500, 3000, 6000, 12000], [0.06, 0.1, 0.3, 0.75, 1.0])[None, :]   # sung vowels stay periodic
    ap = np.where((kind == 2)[:, None], np.minimum(ap, cap), ap)
    if S.breath:
        w = np.clip((fb - 2500) / 3500, 0, 1)[None, :]
        ap = np.where((kind == 2)[:, None], 1 - (1 - ap) * (1 - S.breath * w), ap)
    if abs(S.formant - 1) > 1e-3:
        k = np.arange(sp.shape[1], dtype=float)
        sp = np.stack([np.interp(k / S.formant, k, row) for row in sp])
    sp, ap = extend_hf(sp, np.clip(ap, 0, 1), kind, S.air)
    y = pw.synthesize(np.ascontiguousarray(f0), np.ascontiguousarray(sp), np.ascontiguousarray(ap), 2 * TTS_SR, FP)
    y = signal.resample_poly(y, 147, 160)
    n = int(round(total_sec * SR))
    y = np.concatenate([y, np.zeros(max(0, n - len(y)))])[:n]
    if S.breaths:
        y = add_breaths(y, events, rng, loop_sec)
    return y


def extend_hf(sp, ap, kind, air=1.0):
    """Bandwidth extension for 48 kHz synthesis. The TTS stops at 12 kHz and its vowels carry a vocoder 'sizzle' bump around
    10-11.5 kHz, so: vowel frames keep their envelope up to 9 kHz and continue it along their own 5 -> 8 kHz slope (clamped, with an
    extra roll-off past 15 kHz); consonant frames keep theirs up to 11 kHz and continue the same way. The new band is mostly
    breathy. Bins keep their 23.4 Hz spacing (the 24 kHz envelope is the lower half) and power doubles to keep the level."""
    nb = sp.shape[1]                        # 513 bins up to 12 kHz
    df = TTS_SR / 2 / (nb - 1)
    f = np.arange(2 * nb - 1) * df          # 1025 bins up to 24 kHz
    L = 10 * np.log10(sp + 1e-20)
    band = lambda lo, hi: L[:, int(lo / df):int(hi / df)].mean(axis=1)
    la, lb = band(4000, 6500), band(7000, 9000)
    slope = np.clip((lb - la) / np.log2(8000 / 5200), -24.0, -9.0)
    ext = lb[:, None] + slope[:, None] * np.log2(np.maximum(f, 1.0) / 8000)[None, :] - 24 * np.log2(np.maximum(f, 14000) / 14000)[None, :]
    ext += 10 * np.log10(max(air, 1e-3)) - np.where(kind == 2, 0.0, 3.0)[:, None]
    Lx = np.concatenate([L, np.repeat(L[:, -1:], len(f) - nb, axis=1)], axis=1)
    vow = (kind == 2)[:, None]
    x0 = np.where(vow, 9000.0, 11000.0)
    x1 = np.where(vow, 10000.0, 12000.0)
    w = np.clip((f[None, :] - x0) / (x1 - x0), 0, 1)
    Lfull = Lx * (1 - w) + ext * w
    sp2 = 2.0 * 10 ** (Lfull / 10)
    ap_hi = np.maximum(ap[:, int(8000 / df):int(10000 / df)].mean(axis=1), np.where(kind == 2, 0.7, 0.9))
    apx = np.concatenate([ap, np.repeat(ap[:, -1:], len(f) - nb, axis=1)], axis=1)
    ap2 = apx * (1 - w) + ap_hi[:, None] * w
    return np.ascontiguousarray(sp2), np.ascontiguousarray(np.clip(ap2, 0, 1))


def add_breaths(y, events, rng, loop_sec=None):
    """A quiet inhale before each phrase that starts after a gap (in a loop, the gap before the first phrase is the one after the last)."""
    out = y.copy()
    prev_end = events[-1]['notes'][-1][1] - loop_sec if (loop_sec and events) else -9
    peak = np.max(np.abs(y)) + 1e-9
    for ev in events:
        t_on = ev['notes'][0][0]
        start = t_on - 0.12
        if start - prev_end > 0.45:
            L = int(0.22 * SR)
            a = int((start - 0.24) * SR)
            if a > 0 and a + L < len(out):
                nz = rng.normal(0, 1, L)
                nz = bfilt(bfilt(nz, 'bandpass', (1100, 4800), 2), 'lowpass', 6000, 2)
                env = np.sin(np.linspace(0, np.pi, L)) ** 2.2
                nz = nz / (np.std(nz) + 1e-9) * env * peak * 0.007
                out[a:a + L] += nz
        prev_end = ev['notes'][-1][1]
    return out


# ================================================================= mixing
def deess(x, f_lo=5200, f_hi=11000, thr_db=-28, ratio=4.0, rel_db=-9.0):
    """Split-band de-esser. The band is turned down only where it is both loud (above thr_db re the peak) and dominant
    (within rel_db of the full signal), so bright vowels pass and sibilants are tamed."""
    band = bfilt(x, 'bandpass', (f_lo, f_hi), 2)
    bm = band if band.ndim == 1 else band.mean(axis=1)
    mono = x if x.ndim == 1 else x.mean(axis=1)
    k = int(0.004 * SR)
    env = np.sqrt(np.convolve(bm ** 2, np.ones(k) / k, 'same')) + 1e-9
    full = np.sqrt(np.convolve(mono ** 2, np.ones(k) / k, 'same')) + 1e-9
    thr = np.maximum(np.max(np.abs(mono)) * 10 ** (thr_db / 20), full * 10 ** (rel_db / 20)) + 1e-9
    g = np.where(env > thr, (thr / env) ** (1 - 1 / ratio), 1.0)
    g = np.convolve(g, np.ones(k) / k, 'same')
    return x - band + band * (g if band.ndim == 1 else g[:, None])


def polish(x, male=False, air=1.5, sat=0.25, bright=4.0):
    """One vocal: low cut, mud and harshness cuts, de-ess, fast + slow compression, warmth, presence, brightness and air."""
    y = board(x, [pb.HighpassFilter(80 if male else 110), pb.PeakFilter(300, -2.5, 1.0), pb.PeakFilter(4200 if male else 4800, -1.5, 2.0)])
    y = deess(y)
    y = board(y, [pb.Compressor(threshold_db=-22, ratio=3.5, attack_ms=2, release_ms=60),
                  pb.Compressor(threshold_db=-16, ratio=2.0, attack_ms=25, release_ms=200)])
    if sat:
        d = np.tanh(2.2 * y / (np.max(np.abs(y)) + 1e-9)) * (np.max(np.abs(y)) + 1e-9) / np.tanh(2.2)
        y = y * (1 - sat) + d * sat
    y = board(y, [pb.LowShelfFilter(180, 1.0 if male else 0.5), pb.PeakFilter(3000, 2.0, 0.8),
                  pb.PeakFilter(8000, bright - (0.5 if male else 0.0), 0.7), pb.HighShelfFilter(12000, air)])
    return deess(y, 6000, 12000, thr_db=-30, ratio=3.0)


def plate_ir(sec=1.9, predelay=0.022, seed=3):
    """Dense stereo plate-style impulse response: exponential decay, highs dying faster than lows."""
    rng = np.random.default_rng(seed)
    n = int(sec * SR)
    t = np.arange(n) / SR
    out = np.zeros((n, 2))
    bands = [(0, 400, 1.0), (400, 1800, 1.0), (1800, 5000, 0.75), (5000, 11000, 0.45)]
    for c in range(2):
        acc = np.zeros(n)
        for lo, hi, rt in bands:
            nz = rng.normal(0, 1, n)
            nz = bfilt(nz, 'bandpass', (max(lo, 40), hi), 2) if lo > 0 else bfilt(nz, 'lowpass', hi, 2)
            acc += nz * np.exp(-6.9 * t / (sec * rt))
        out[:, c] = acc
    out *= np.clip(t / 0.004, 0, 1)[:, None]            # soft start
    out = np.concatenate([np.zeros((int(predelay * SR), 2)), out])
    return out / np.sqrt(np.sum(out ** 2) / 2)


_IR = {}


def plate(x, sec=1.9, wet=0.18, hp=280, lp=8500):
    if sec not in _IR:
        _IR[sec] = plate_ir(sec)
    ir = _IR[sec]
    x2 = to_stereo(x)
    w = np.stack([signal.fftconvolve(x2[:, c], ir[:, c]) for c in range(2)], 1)
    w = bfilt(bfilt(w, 'highpass', hp, 2), 'lowpass', lp, 2)
    return w * wet


def ducked_delay(x, bpm, beats=0.75, fb=0.38, wet=0.2, duck_db=9.0, lp=4200, hp=350, taps=8):
    """Ping-pong delay whose return dips while the dry voice sings."""
    x2 = to_stereo(x)
    mono = x2.mean(axis=1)
    d = int(beats * 60 / bpm * SR)
    n = len(mono)
    total = n + d * taps
    out = np.zeros((total, 2))
    tap = mono.copy()
    for k in range(1, taps + 1):
        tap = bfilt(bfilt(tap, 'lowpass', lp, 1), 'highpass', hp, 1) * (fb if k > 1 else 1.0)
        out[k * d:k * d + n, (k - 1) % 2] += tap
    k = int(0.02 * SR)
    env = np.sqrt(np.convolve(np.concatenate([mono, np.zeros(total - n)]) ** 2, np.ones(k) / k, 'same'))
    env = env / (np.max(env) + 1e-9)
    rel = signal.lfilter([1 - 0.9995], [1, -0.9995], env)      # slow release so the echo blooms after each phrase
    duck = 10 ** (-duck_db * np.clip(np.maximum(env, rel) * 3, 0, 1) / 20)
    return out * wet * duck[:, None]


def k_loudness(x):
    """Integrated loudness (BS.1770-style K-weighting, 400 ms blocks, absolute + relative gating), in LUFS."""
    x2 = to_stereo(x)
    b1, a1 = signal.iirfilter(2, 1500 / (SR / 2), btype='highpass', ftype='butter')
    y = signal.lfilter(*signal.butter(2, 38 / (SR / 2), 'highpass'), x2, axis=0)
    shelf = board(y, [pb.HighShelfFilter(1500, 4.0, 0.7)])
    blk, hop = int(0.4 * SR), int(0.1 * SR)
    ms = []
    for s in range(0, max(1, len(shelf) - blk + 1), hop):
        seg = shelf[s:s + blk]
        ms.append(np.mean(np.sum(seg ** 2, axis=1)))
    ms = np.array(ms) + 1e-12
    lk = -0.691 + 10 * np.log10(ms)
    g1 = ms[lk > -70]
    if not len(g1):
        return -70.0
    rel = -0.691 + 10 * np.log10(np.mean(g1)) - 10
    g2 = ms[(lk > -70) & (lk > rel)]
    return float(-0.691 + 10 * np.log10(np.mean(g2 if len(g2) else g1)))


def limit(x, ceiling_db=-1.0, look=0.004, release=0.06):
    """Look-ahead peak limiter (sample-accurate gain, smoothed release)."""
    c = 10 ** (ceiling_db / 20)
    x2 = x if x.ndim == 2 else x[:, None]
    pk = np.max(np.abs(x2), axis=1)
    la = int(look * SR)
    need = np.minimum(1.0, c / np.maximum(pk, 1e-12))
    # minimum over the look-ahead window, then a release filter that only rises slowly
    from scipy.ndimage import minimum_filter1d
    g = minimum_filter1d(need, size=2 * la + 1, origin=0)
    a = np.exp(-1 / (release * SR))
    out = np.empty_like(g)
    cur = 1.0
    for i in range(len(g)):            # attack is instant (g already looks ahead), release is smooth
        cur = g[i] if g[i] < cur else cur * a + g[i] * (1 - a)
        out[i] = cur
    y = x2 * out[:, None]
    return y if x.ndim == 2 else y[:, 0]


def finish(x, lufs=-14.0, ceiling=-1.0, max_push=6.0):
    """Match loudness, then limit peaks; if limiting leaves it quiet (spiky material), push into the limiter a little more
    (at most max_push dB beyond the plain match)."""
    g = 10 ** ((lufs - k_loudness(x)) / 20)
    g0 = g
    y = limit(x * g, ceiling)
    for _ in range(3):
        L = k_loudness(y)
        if abs(L - lufs) < 0.3:
            break
        g = min(g * 10 ** ((lufs - L) / 20), g0 * 10 ** (max_push / 20))
        y = limit(x * g, ceiling)
    return y


def wrap_loop(x, n):
    """Fold everything after n samples back onto the start so the loop repeats without losing tails."""
    loop = x[:n].copy()
    rest = x[n:]
    k = 0
    while len(rest) and k < 4:
        m = min(n, len(rest))
        loop[:m] += rest[:m]
        rest = rest[m:]
        k += 1
    return loop


# ================================================================= hooks: lead + doubles + harmony, mixed
FEMALE_ALTS = {'af_bella': ['af_heart', 'af_sarah'], 'af_heart': ['af_bella', 'af_nova'], 'af_nicole': ['af_sky', 'af_heart'],
               'af_sarah': ['af_heart', 'af_bella'], 'af_nova': ['af_heart', 'af_sarah'], 'af_jessica': ['af_heart', 'af_nova'],
               'af_kore': ['af_bella', 'af_sarah'], 'af_sky': ['af_nicole', 'af_heart'], 'af_aoede': ['af_heart', 'af_bella'],
               'af_river': ['af_bella', 'af_heart'], 'af_alloy': ['af_heart', 'af_sarah'],
               'bf_emma': ['bf_isabella', 'bf_lily'], 'bf_isabella': ['bf_emma', 'bf_alice'], 'bf_lily': ['bf_emma', 'bf_alice'],
               'bf_alice': ['bf_emma', 'bf_isabella'],
               'am_michael': ['am_fenrir', 'am_onyx'], 'am_fenrir': ['am_michael', 'am_puck'], 'am_puck': ['am_fenrir', 'am_adam'],
               'am_onyx': ['am_michael', 'am_fenrir'], 'am_adam': ['am_puck', 'am_echo'], 'am_echo': ['am_adam', 'am_liam'],
               'am_liam': ['am_echo', 'am_puck'], 'am_eric': ['am_adam', 'am_liam'],
               'bm_george': ['bm_lewis', 'bm_daniel'], 'bm_lewis': ['bm_george', 'bm_fable'], 'bm_daniel': ['bm_george', 'bm_lewis'],
               'bm_fable': ['bm_lewis', 'bm_george']}
MALE = {'am_michael', 'am_fenrir', 'am_puck', 'am_onyx', 'am_adam', 'am_echo', 'am_liam', 'am_eric', 'am_santa', 'bm_george', 'bm_lewis', 'bm_daniel', 'bm_fable'}


def blend_of(voice, alt=None, w=0.22):
    return {voice: 1.0} if alt is None else {voice: 1 - w, alt: w}


def harmony_notes(notes, scale_pcs, up=2):
    """Diatonic third (up=2 scale steps) above each note."""
    out = []
    for s0, ln, m, w in notes:
        cands = [n for n in range(m + 1, m + 6) if n % 12 in scale_pcs]
        out.append((s0, ln, cands[min(up - 1, len(cands) - 1)], w))
    return out


PCS = {'C': 0, 'D': 2, 'E': 4, 'F': 5, 'G': 7, 'A': 9, 'B': 11}


def note_pc(name):
    """'Bb3' / 'C#4' / 'Eb' -> pitch class"""
    pc = PCS[name[0]]
    for ch in name[1:]:
        if ch == '#':
            pc += 1
        elif ch == 'b':
            pc -= 1
    return pc % 12


def chord_timeline(kit):
    """[(step, pitch classes)] from a kit's A chord part (the voicing's notes plus the chord root)."""
    out = []
    for e in kit['parts']['chordsA']:
        step, name = e[0] // 120, e[5]
        if not name or (out and out[-1][2] == name):
            continue
        root = re.match(r'^[A-G][b#]?', name).group(0)
        pcs = {note_pc(n) for n in kit['chords'].get(name, [])} | {note_pc(root)}
        out.append((step, pcs, name))
    return out


def chord_at(timeline, step, loop=64):
    """Chord pitch classes sounding at a step (the last change at or before it, wrapping round the loop)."""
    cur = timeline[-1][1]
    for st, pcs, _ in timeline:
        if st <= step % loop:
            cur = pcs
    return cur


def harmony_chords(notes, timeline, scale_pcs, ceiling=81):
    """A backing line a third (or a fourth) above, on chord tones where it can: thirds that are chord tones first, then a
    fourth that is, then the diatonic third."""
    out = []
    for s0, ln, m, w in notes:
        pcs = chord_at(timeline, s0)
        pick = next((c for c in (m + 3, m + 4) if c % 12 in pcs and c % 12 in scale_pcs), None)
        if pick is None:
            pick = next((c for c in (m + 3, m + 4) if c % 12 in pcs), None)
        if pick is None and (m + 5) % 12 in pcs:
            pick = m + 5
        if pick is None:
            cands = [n for n in range(m + 3, m + 5) if n % 12 in scale_pcs]
            pick = cands[0] if cands else m + 3
        while pick > ceiling:
            pick -= 12
        out.append((s0, ln, pick, w))
    return out


INTERJ = {'ooh', 'oooh', 'oh', 'ah', 'aah', 'ahh', 'yeah', 'hey', 'woo', 'mmm', 'mm', 'uh', 'whoa', 'na', 'la', 'da', 'doo'}
SAME = {'fuckin': 'fucking', 'gonna': 'going', 'wanna': 'want', 'til': 'till', 'colour': 'color', 'colours': 'colors', 'tonite': 'tonight'}


CENSORED = [(r'\bf\*+ck', 'fuck'), (r'\bf\*+king', 'fucking'), (r'\bf\*+kin', 'fucking'), (r'\bsh\*+t', 'shit'), (r'\bb\*+ch', 'bitch'), (r'\ba\*+', 'ass')]


def lyric_score(want_text, heard):
    """Share of the sung words heard, in order, minus a little for words that don't belong."""
    import difflib
    import vocals2 as V2
    norm = lambda w: SAME.get(V2.HOMO.get(w, w), V2.HOMO.get(w, w))
    for pat, rep in CENSORED:
        heard = re.sub(pat, rep, heard, flags=re.I)
    want = [norm(w) for w in V2.words(want_text) if w not in INTERJ]
    got = [norm(w) for w in V2.words(heard) if w not in INTERJ]
    if not want:
        return 0.0
    hit = sum(b.size for b in difflib.SequenceMatcher(a=want, b=got, autojunk=False).get_matching_blocks())
    return hit / len(want) - 0.2 * max(0, len(got) - hit) / max(1, len(got))


def phrases_of(notes, split=32):
    """The line as ASR-sized phrases: halves at the bar-3 line when both halves have words."""
    a = [n for n in notes if n[0] < split]
    b = [n for n in notes if n[0] >= split]
    return [a, b] if a and b and a[0][3] != '~' and b[0][3] != '~' else [notes]


def judge(y, notes, bpm, swing, male=False, pre=0.0):
    """ASR score of a sung lead, phrase by phrase (polished first, as it will be heard)."""
    import vocals2 as V2
    st = step_seconds(bpm)
    T = lambda s_: float(warp16(np.array(float(s_)), swing)) * st + pre
    z = polish(y, male)
    scores, heard = [], []
    phs = phrases_of(notes)
    pad = np.zeros(int(0.3 * SR))
    for k, ph in enumerate(phs):
        a = max(0, int((T(ph[0][0]) - 0.2) * SR))
        end = T(ph[-1][0] + ph[-1][1]) + 0.35
        if k + 1 < len(phs):
            end = min(end, T(phs[k + 1][0][0]) - 0.12)
        b = min(len(z), int(end * SR))
        h = V2.asr(np.concatenate([pad, z[a:b], pad]), SR)
        scores.append(lyric_score(' '.join(n[3] for n in ph if n[3] != '~'), h))
        heard.append(h)
    return float(np.mean(scores)), ' | '.join(heard)


TAKES = [dict(glottal=True, speed=0.8), dict(glottal=False, speed=0.8), dict(glottal=True, speed=0.72), dict(glottal=False, speed=0.72),
         dict(glottal=True, speed=0.88)]


WIDE_TAKES = [dict(glottal=g, speed=sp, consonants=c) for c in (1.0, 1.2) for sp in (0.8, 0.72, 0.88) for g in (True, False)]


def best_lead(notes, voice, bpm, swing, total_sec, seed=0, vib=None, need=0.95, log=None, pre=0.0, loop_sec=None, retry=0.75, takes=None):
    """Sing the lead in up to five takes (glottal onsets on/off, three take speeds) and keep the one the recogniser understands best;
    if none reaches `retry`, five more with the voice tinted by its nearest neighbour (the TTS is deterministic, so a new take
    needs a different input)."""
    male = voice in MALE
    vb = (24.0 if male else 32.0) if vib is None else vib
    alt = FEMALE_ALTS.get(voice, [voice])[0]
    best = None
    for tint in (0.0, 0.12):
        for k, tk in enumerate(takes or TAKES):
            S = Sing(blend_of(voice, alt, tint) if tint else voice, seed=seed, vib=vb, **tk)
            y = sing(notes, bpm, swing, S, total_sec, pre=pre, loop_sec=loop_sec)
            sc, heard = judge(y, notes, bpm, swing, male, pre)
            if log:
                log(f'    take {tint}/{k} {tk} score {sc:.2f} heard: {heard}')
            if best is None or sc > best[0] + 1e-6:
                best = (sc, y, dict(tk, tint=tint), heard)
            if sc >= need:
                return best
        if best[0] >= retry:
            break
    return best


def hook_parts(notes, voice, bpm, swing, total_sec, scale_pcs=None, harm=True, seed=0, vib=None, take=None, lead=None, pre=0.0, loop_sec=None,
               harm_notes=None):
    """Lead (given, or sung with take = {'glottal', 'speed'}) + two doubles (other blended voices and take speeds, looser vibrato,
    humanised timing and tuning, small formant offsets) + an optional diatonic third above."""
    male = voice in MALE
    alts = FEMALE_ALTS.get(voice, [voice, voice])
    vb = (24.0 if male else 32.0) if vib is None else vib
    tk = take or dict(glottal=True, speed=0.8)
    sp, gl, cons = tk['speed'], tk['glottal'], tk.get('consonants', 1.0)
    if lead is None:
        lead = sing(notes, bpm, swing, Sing(blend_of(voice, alts[0], tk['tint']) if tk.get('tint') else voice, seed=seed, vib=vb, speed=sp, glottal=gl, consonants=cons),
                    total_sec, pre=pre, loop_sec=loop_sec)
    d1 = sing(notes, bpm, swing, Sing(blend_of(voice, alts[0]), speed=round(sp * 1.075, 3), vib=vb * 0.45, timing=0.008, tuning=5,
                                     formant=1.012, seed=seed + 11, breaths=False, scoop=40, fall=False, glottal=gl, consonants=cons), total_sec, pre=pre)
    d2 = sing(notes, bpm, swing, Sing(blend_of(voice, alts[1]), speed=round(sp * 0.925, 3), vib=vb * 0.55, timing=0.009, tuning=5,
                                     formant=0.988, seed=seed + 23, breaths=False, scoop=45, fall=False, glottal=gl, consonants=cons), total_sec, pre=pre)
    har = None
    if harm and (harm_notes is not None or scale_pcs is not None):
        har = sing(harm_notes if harm_notes is not None else harmony_notes(notes, scale_pcs), bpm, swing, Sing(blend_of(voice, alts[0], 0.15), speed=sp, vib=vb * 0.7, timing=0.006,
                                                                    tuning=4, seed=seed + 37, breaths=False, fall=False, glottal=gl, consonants=cons), total_sec, pre=pre)
    return lead, [d1, d2], har


def hook_mix(lead, doubles, harmony, bpm, loop_n, male=False, verb=0.17, dly=0.2, wet_lufs=-13.0, dry_lufs=-13.5, pre_n=0):
    """Returns (wet stereo loop, dry mono lead) for a 4-bar hook. Parts may carry pre_n samples of pre-roll (the first onset and
    breath): the loop starts after it, everything past the loop end folds back onto the start and the pre-roll onto the end."""
    L = polish(lead, male)
    mix = to_stereo(L)
    for d, p in zip(doubles, (-0.7, 0.7)):
        dd = board(polish(d, male, air=2.0, sat=0.15), [pb.HighpassFilter(170), pb.Compressor(threshold_db=-24, ratio=4, attack_ms=3, release_ms=80)])
        mix += to_stereo(dd * 10 ** (-7.5 / 20), p)
    if harmony is not None:
        hh = board(polish(harmony, male, air=2.0, sat=0.15), [pb.HighpassFilter(190)])
        mix += to_stereo(hh * 10 ** (-9.5 / 20), -0.25)
    mix = board(mix, [pb.Compressor(threshold_db=-16, ratio=2.0, attack_ms=12, release_ms=140)])
    n = len(mix)
    wet = np.zeros((n + 3 * SR, 2))
    wet[:n] += mix
    pr = plate(L, 1.8, verb)
    wet[:len(pr)] += pr[:len(wet)]
    dl = ducked_delay(mix.mean(axis=1), bpm, 0.75, 0.36, dly)
    wet[:min(len(dl), len(wet))] += dl[:len(wet)]
    loop = loop_with_pre(wet, loop_n, pre_n)
    wet_out = finish(loop, wet_lufs)
    dry = finish(loop_with_pre(polish(lead, male, sat=0.1), loop_n, pre_n, tail=int(0.4 * SR)), dry_lufs)
    return wet_out, dry


def loop_with_pre(x, n, pre_n, tail=None):
    """x = pre-roll + loop + tail: wrap the tail (all of it, or the first `tail` samples) onto the start and the pre-roll onto the end."""
    body = x[pre_n:] if tail is None else x[pre_n:pre_n + n + tail]
    loop = wrap_loop(body, n)
    if pre_n:
        loop[n - pre_n:] += x[:pre_n]
    return loop
