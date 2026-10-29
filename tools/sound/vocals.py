"""The vocal chain: a Kokoro take at 24 kHz, analysed with WORLD, performed (sung or robot) and resynthesised at 48 kHz.

ADR 0005, D4: a vocal master comes only from WORLD resynthesis at 48 kHz using the features of the 24 kHz Kokoro take.
The band above the take's 12 kHz is synthetic (extend_hf), so every master made here carries the hf_synthetic flag.
The Kokoro audio is analysed and never resampled into a master; models.asr() resamples only to listen.

    perf = performance('Copper light, moving slow', K.VOICE)       # Kokoro + WORLD analysis, cached per line
    track = sing_track(perf, notes, bpm, Sing(), total, pre, rng)  # WORLD frames of a sung line, 5 ms apart
    y = synth(track)                                               # 48 kHz mono

Provenance: ported from legacy/web-pack/audio/vocals3.py (Kokoro run directly through onnxruntime so its phoneme
durations come back; align_tokens, syllables_of, fit_syllables, the time map, pitch and dynamics of sing(), extend_hf,
add_breaths, deess). Changes: WORLD synthesises at 2 x 24 kHz = 48 kHz and nothing is resampled afterwards (the legacy
resample_poly to 44.1 kHz is gone); sing() is split into frames (sing_track) and synthesis (synth), so chops can be cut
in the frame domain; the noise comes from the kit's seeded rng; polish() uses dsp48 filters instead of pedalboard.
robot_track and chop are new.
"""
import functools
import re
import sys
import types
from dataclasses import dataclass
from importlib import metadata

import numpy as np
from scipy import signal

from tools.sound import dsp48 as D
from tools.sound import models

SR = D.SR
TTS_SR = models.TTS_SR
HOP = 600                    # one Kokoro duration frame = 25 ms at 24 kHz
FP = 5.0                     # WORLD analysis frame period, ms
FPS = FP / 1000.0
VOWELS = set('AIOQWYaeiouɑɐɒæəɚɛɜɪʊʌɔᵊɨɯøœɤᵻ')   # Kokoro: A eɪ, I aɪ, O oʊ (US), Q əʊ (GB), W aʊ, Y ɔɪ
STRESS = set('ˈˌ')
PUNCT = set(';:,.!?—…')
VOICED_C = set('lmnŋɹwjvzðbdɡʒʤɾ')
UNVOICED_C = set('tkpsʃfθhʧʔ')
ON_CAP, CO_CAP = 0.16, 0.12  # longest onset and coda kept, seconds

assert SR == 2 * TTS_SR, 'WORLD resynthesises the 24 kHz take at exactly twice its rate'


def _pyworld():
    """pyworld 0.3.5's __init__ reads its own version through pkg_resources, which a fresh venv no longer has
    (setuptools isn't installed). Answer that one call from importlib.metadata rather than add setuptools."""
    if 'pyworld' in sys.modules:
        return sys.modules['pyworld']
    shim = None
    try:
        import pkg_resources  # noqa: F401
    except ImportError:
        shim = types.ModuleType('pkg_resources')
        shim.get_distribution = lambda name: types.SimpleNamespace(version=metadata.version(name))
        sys.modules['pkg_resources'] = shim
    try:
        import pyworld
    except ImportError as e:
        raise models.ModelsMissing('pyworld is not installed: '
                                   'pip install -r tools/sound/requirements-vocals.txt') from e
    finally:
        if shim is not None:
            del sys.modules['pkg_resources']
    return pyworld


# ------------------------------------------------------------------ Kokoro, run directly
class Kokoro:
    """Kokoro-82M through onnxruntime: unlike sherpa-onnx's wrapper it returns the phoneme durations the time map needs.

    The graph draws noise (RandomNormalLike and RandomUniformLike ops, its source excitation), and onnxruntime seeds
    those kernels once, when a session is built, from its global seed. So every take gets a fresh session built right
    after onnxruntime.set_seed(a seed of the line, voice and options): the same take on every run and in any order. One
    thread, so the floats don't depend on the thread pool either."""

    def __init__(self):
        d = models._model(models.KOKORO, 'model.onnx', 'voices.bin', 'tokens.txt', 'lexicon-gb-en.txt',
                          'lexicon-us-en.txt')
        self.model = (d / 'model.onnx').read_bytes()
        self.tok = {}
        for line in (d / 'tokens.txt').read_text(encoding='utf-8').split('\n'):
            if line:
                sym, i = line.rsplit(' ', 1)
                self.tok[sym if sym else ' '] = int(i)
        self.voices = np.fromfile(d / 'voices.bin', dtype=np.float32).reshape(-1, 510, 256)
        self.dir = d
        self.lex = {}

    def session(self, *key):
        import onnxruntime as ort
        ort.set_seed(D.seed('kokoro', *key) % 2 ** 31)
        opts = ort.SessionOptions()
        opts.intra_op_num_threads = 1
        opts.inter_op_num_threads = 1
        return ort.InferenceSession(self.model, sess_options=opts, providers=['CPUExecutionProvider'])

    def lexicon(self, gb):
        k = 'gb' if gb else 'us'
        if k not in self.lex:
            out = {}
            for line in (self.dir / f'lexicon-{k}-en.txt').read_text(encoding='utf-8').split('\n'):
                p = line.split(' ')
                if p[0] and p[0] not in out:
                    out[p[0]] = p[1:]
            self.lex[k] = out
        return self.lex[k]

    def word_phones(self, w, gb):
        key = w.lower().strip('.,!?;:"“”()').replace('’', "'")
        lex = self.lexicon(gb)
        for k in (key, key.replace("'", '')):
            if k in lex:
                return lex[k]
        if '-' in key:
            return [p for part in key.split('-') for p in self.word_phones(part, gb)]
        raise ValueError(f'no phonemes for {w!r} in the Kokoro lexicon: respell the word in the recipe')

    def style(self, blend, n):
        """The style vector of a voice blend {voice: weight} for a line of n tokens."""
        tot = sum(w for _, w in blend)
        return sum(self.voices[models.SPEAKERS.index(v), min(509, n - 1)] * (w / tot)
                   for v, w in blend)[None, :].astype(np.float32)

    def speak(self, words, blend, speed=1.0, glottal=False, accent=None, draw=0):
        """words: word strings, punctuation may trail a word. blend: ((voice, weight), ...). accent: the lexicon, 'gb'
        or 'us'; by default the heaviest voice's (bf_ = British). Returns (audio at 24 kHz, tokens), tokens = [{sym,
        word, t0, t1}] in seconds (word = index into words, -1 for spaces and punctuation). glottal: vowel-initial
        words after the first start with a glottal stop, as singers do, instead of linking. draw: which take, the seed
        of the graph's noise; another draw is another performance of the same line."""
        gb = max(blend, key=lambda vw: vw[1])[0].startswith('b') if accent is None else accent == 'gb'
        seq = []
        for wi, w in enumerate(words):
            if wi:
                seq.append((' ', -1))
            ph = self.word_phones(w, gb)
            first = next((p for p in ph if p not in STRESS), '')
            if glottal and wi and first in VOWELS:
                seq.append(('ʔ', wi))
            seq += [(p, wi) for p in ph]
            tail = re.findall(r'[,.!?;:]+$', w)
            if tail:
                seq.append((tail[0][0], -1))
        if not seq or seq[-1][0] not in PUNCT:
            seq.append(('.', -1))
        ids = [self.tok[s] for s, _ in seq]
        sess = self.session(' '.join(words), blend, speed, glottal, 'gb' if gb else 'us', draw)
        a, d = sess.run(None, {'tokens': np.array([[0] + ids + [0]], dtype=np.int64),
                               'style': self.style(blend, len(ids)),
                               'speed': np.array([speed], dtype=np.float32)})
        edges = np.concatenate([[0], np.cumsum(np.asarray(d, dtype=float))]) * HOP / TTS_SR
        toks = [dict(sym=s, word=wi, t0=edges[i + 1], t1=edges[i + 2]) for i, (s, wi) in enumerate(seq)]
        return np.asarray(a, dtype=np.float64), toks


@functools.lru_cache(maxsize=1)
def kokoro():
    return Kokoro()


# ------------------------------------------------------------------ WORLD analysis of a spoken take
@dataclass(frozen=True, eq=False)
class Performance:
    """A spoken take and its WORLD features, all at 24 kHz (sp and ap have 513 bins), 5 ms frames."""
    text: str
    words: tuple
    x: np.ndarray
    toks: list
    f0: np.ndarray
    sp: np.ndarray
    ap: np.ndarray


def analyze(x):
    pw = _pyworld()
    f0, t = pw.harvest(x, TTS_SR, f0_floor=60, f0_ceil=700, frame_period=FP)
    return f0, pw.cheaptrick(x, f0, t, TTS_SR), pw.d4c(x, f0, t, TTS_SR)


@functools.lru_cache(maxsize=16)
def performance(text, blend, speed=0.8, glottal=True, accent=None, draw=0):
    """Kokoro speaks the line (words split on spaces, punctuation phrases it); WORLD analyses it. blend: ((voice,
    weight), ...), e.g. kits.fut01.VOICE; accent and draw: see Kokoro.speak. Cached: a take is deterministic."""
    words = tuple(text.split(' '))
    x, toks = kokoro().speak(words, blend, speed, glottal=glottal, accent=accent, draw=draw)
    f0, sp, ap = analyze(x)
    return Performance(text, words, x, align_tokens(toks, f0, x), f0, sp, ap)


def align_tokens(toks, f0, x=None):
    """The TTS's audio runs ahead of its own duration timeline (50-115 ms, varying), so re-time the tokens from acoustic
    landmarks: voicing onsets after unvoiced sounds, voicing offsets before them, and the burst or hiss of an unvoiced
    consonant after a pause. Each landmark gives a correction; boundaries in between are corrected by interpolation."""
    voiced = f0 > 0
    n = len(voiced)
    hf = None
    if x is not None:
        y = signal.sosfiltfilt(signal.butter(4, [2500, 9000], 'bandpass', fs=TTS_SR, output='sos'), x)
        k = int(0.004 * TTS_SR)
        hf = np.sqrt(np.convolve(y ** 2, np.ones(k) / k, 'same'))[::int(FPS * TTS_SR)][:n]
    cls = ['v' if (t['sym'] in VOWELS or t['sym'] in VOICED_C) else 'u' if t['sym'] in UNVOICED_C else
           'p' if (t['sym'] in PUNCT or t['sym'] == ' ') else 's' for t in toks]
    for i in range(len(cls) - 1, -1, -1):        # stress marks belong to the vowel after them
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
            cand = [f * FPS for f in range(lo, hi) if voiced[f] and voiced[f + 1] and (f == 0 or not voiced[f - 1])]
        elif a_ == 'v' and b_ in 'up':               # voicing stops
            cand = [f * FPS for f in range(lo + 1, hi + 1) if not voiced[f] and voiced[f - 1]]
        elif a_ == 'p' and b_ == 'u' and hf is not None:   # hiss or burst after a pause
            seg = hf[lo:hi]
            if seg.max() > 0:
                cand = [(lo + int(np.argmax(seg > 0.25 * seg.max()))) * FPS]
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
        med = float(np.median([c for _, c in marks]))
        marks = [(t, c) for t, c in marks if abs(c - med) <= 0.06]
    if not marks:
        marks = [(0.0, EXP)]
    mt = np.array([t for t, _ in marks])
    mc = np.array([c for _, c in marks])
    out = [dict(t, t0=max(0.0, t['t0'] + float(np.interp(t['t0'], mt, mc))),
                t1=max(0.0, t['t1'] + float(np.interp(t['t1'], mt, mc)))) for t in toks]
    for i in range(len(out) - 1):                    # keep the tokens contiguous and in order
        a, b = out[i], out[i + 1]
        a['t1'] = b['t0'] = max(a['t0'], min(a['t1'], b['t0']) if a['t1'] > b['t0'] else b['t0'])
    return out


# ------------------------------------------------------------------ syllables
def syllables_of(toks, wi):
    """Split word wi's tokens into syllables: [{on: (t0, t1), nu: (t0, t1), co: (t0, t1), end, syms}] in source
    seconds. Onsets take one consonant between vowels (with two or more, the first goes to the coda); a following
    word-gap token joins the last coda, for legato. `end` is where the syllable's own phonemes stop: the gap can already
    hold the start of the next word."""
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
        out.append(dict(on=(tk[s]['t0'], tk[n0]['t0']), nu=(tk[n0]['t0'], tk[n1]['t1']),
                        co=(tk[n1]['t1'], tk[e - 1]['t1']), end=tk[e - 1]['t1'],
                        syms=''.join(t['sym'] for t in tk[s:e] if t['sym'] not in STRESS)))
    nxt = idx[-1] + 1
    if nxt < len(toks) and toks[nxt]['sym'] == ' ':
        out[-1]['co'] = (out[-1]['co'][0], toks[nxt]['t1'])
    return out


def fit_syllables(syls, k):
    """Exactly k sung syllables: the first group absorbs the extra syllables (its longest vowel is held, what comes
    before it is a pickup); with fewer syllables than notes the last vowel carries the extra notes."""
    if k <= 0 or not syls or len(syls) <= k:
        return syls
    g = syls[:len(syls) - k + 1]
    keep = max(range(len(g)), key=lambda i: g[i]['nu'][1] - g[i]['nu'][0])
    kv = g[keep]
    merged = dict(on=(g[0]['on'][0], kv['nu'][0]), nu=kv['nu'], co=g[-1]['co'], end=g[-1]['end'],
                  syms=''.join(x['syms'] for x in g), pickup=keep > 0)
    if keep < len(g) - 1:
        merged['tail'] = (kv['nu'][1], g[-1]['nu'][1])
    else:
        merged['co'], merged['end'] = kv['co'], kv['end']
    return [merged] + syls[len(g):]


def strong_vowels(perf, events):
    """Trim each held vowel to its strong part: spoken phrase ends fade and creak, a sung note shouldn't."""
    E = np.log(np.sum(perf.sp, axis=1) + 1e-12)
    for ev in events:
        n0, n1 = ev['nu']
        a, b = int(n0 / FPS), max(int(n0 / FPS) + 1, int(n1 / FPS))
        seg = E[a:b]
        if len(seg) >= 6:
            last = a + np.flatnonzero(seg >= seg.max() - 2.0)[-1] + 1
            ev['nu'] = (n0, min(n1, max(last, a + int(0.6 * (b - a))) * FPS))


# ------------------------------------------------------------------ WORLD frames of a performance
@dataclass
class Track:
    """WORLD frames of a performance at the output's time base. sp and ap have the take's 513 bins (extend_hf widens
    them for synthesis). kind: 0 silence, 1 onset consonant, 2 vowel, 3 coda (or breath); ev: the event (syllable) that
    owns the frame, -1 for none. events: per syllable its notes and output times (t_on, t_end) in seconds. src: the
    source time each frame was read from, -1 for silence."""
    f0: np.ndarray
    sp: np.ndarray
    ap: np.ndarray
    kind: np.ndarray
    ev: np.ndarray
    events: list
    fp: float = FP           # frame period, ms
    air: float = 1.0
    src: np.ndarray = None

    @property
    def fps(self):
        return self.fp / 1000.0


def map_frames(perf, segs, total_sec, fp=FP):
    """Lay source spans onto the output frame grid. segs: (out0, out1, src0, src1, kind, event) in seconds. Returns
    (sp, ap, kind, ev, src_voiced, src), with sp and ap interpolated between source frames (log sp) and src the
    source time of each frame (-1 where nothing maps)."""
    fps = fp / 1000.0
    nout = int(total_sec / fps) + 1
    tout = np.arange(nout) * fps
    src_t = np.full(nout, -1.0)
    kind = np.zeros(nout, dtype=int)
    ev = np.full(nout, -1)
    for o0, o1, s0, s1, k, i in segs:
        a, b = max(0, int(np.ceil(o0 / fps - 1e-9))), min(nout, int(np.ceil(o1 / fps - 1e-9)))
        if b <= a:
            continue
        f = (tout[a:b] - o0) / max(o1 - o0, 1e-9)
        src_t[a:b] = s0 + f * (s1 - s0)
        kind[a:b] = k
        ev[a:b] = i
    toks = perf.toks
    silent_src = 0.5 * toks[0]['t0'] if toks[0]['t0'] > 0.05 else 0.0
    sf_ = np.where(src_t >= 0, src_t, silent_src) / FPS
    nsrc = len(perf.f0)
    i0 = np.clip(np.floor(sf_).astype(int), 0, nsrc - 1)
    i1 = np.clip(i0 + 1, 0, nsrc - 1)
    fr = (sf_ - np.floor(sf_))[:, None]
    lsp = np.log(perf.sp + 1e-16)
    sp = np.exp(lsp[i0] * (1 - fr) + lsp[i1] * fr)
    ap = perf.ap[i0] * (1 - fr) + perf.ap[i1] * fr
    src_voiced = (perf.f0[i0] > 0) & (perf.f0[i1] > 0)
    sp[kind == 0] *= 1e-7
    return sp, ap, kind, ev, src_voiced, src_t


def release_ends(track, short=0.03, last=0.15, depth_db=100.0):
    """Fade the end of every run of sounding frames, so no syllable stops dead: the level falls depth_db x u^2 (u
    0..1) over `short` seconds, over `last` for the final run. The end of the file then decays slowly enough for the
    one-shot tail check."""
    on = track.kind > 0
    starts = np.flatnonzero(on & ~np.concatenate([[False], on[:-1]]))
    ends = np.flatnonzero(on & ~np.concatenate([on[1:], [False]]))
    for k, (s, e) in enumerate(zip(starts, ends)):
        n = min(int(round((last if k == len(ends) - 1 else short) / track.fps)), (e - s + 1) // 2)
        if n > 0:
            u = np.arange(1, n + 1) / n
            track.sp[e - n + 1:e + 1] *= (D.undb(-depth_db * u ** 2) ** 2)[:, None]
    return track


# ------------------------------------------------------------------ the sung line
def _ease(x):
    x = np.clip(x, 0, 1)
    return x * x * (3 - 2 * x)


@dataclass(frozen=True)
class Sing:
    """How one performance is sung (legacy vocals3.Sing)."""
    speed: float = 0.8        # Kokoro take speed
    vib: float = 32.0         # vibrato depth, cents
    vib_rate: float = 5.4     # Hz
    scoop: float = 55.0       # scoop into each phrase, cents
    drift: float = 5.0        # slow pitch drift, cents
    breath: float = 0.18      # breathiness of held vowels above 2.5 kHz
    timing: float = 0.0       # onset jitter, seconds (standard deviation)
    tuning: float = 0.0       # per-note tuning jitter, cents
    formant: float = 1.0      # formant shift factor
    fall: bool = True         # fall off some phrase-final notes
    consonants: float = 1.0   # consonant length factor
    swell: float = 1.5        # swell over each note, dB
    glide: float = 0.025      # legato glide time constant, seconds
    air: float = 1.0          # level of the synthetic top band
    glottal: bool = True      # glottal stops before vowel-initial words
    cboost: float = 0.0       # consonants up, dB


def group_words(notes):
    """notes: (step16, len16, midi, text) with '~' continuing the previous word -> [[word, [(s0, ln, m), ...]], ...]"""
    out = []
    for s0, ln, m, w in notes:
        if w == '~' and out:
            out[-1][1].append((s0, ln, m))
        else:
            out.append([w, [(s0, ln, m)]])
    return out


def line_text(words_):
    """Lyric words with a comma where the music rests, so the spoken take phrases like the song."""
    out = []
    for i, (w, nts) in enumerate(words_):
        end = nts[-1][0] + nts[-1][1]
        nxt = words_[i + 1][1][0][0] if i + 1 < len(words_) else None
        ww = w.rstrip('.,!?')
        out.append(ww + ('.' if nxt is None else ',' if nxt - end >= 3 else ''))
    return out


def sing_text(notes):
    return ' '.join(line_text(group_words(notes)))


def _sung_events(perf, words_, T, S, rng):
    events = []
    for wi, (_, nts) in enumerate(words_):
        syls = fit_syllables(syllables_of(perf.toks, wi), len(nts))
        if not syls:
            continue
        groups = [[n] for n in nts[:len(syls)]]
        for extra in nts[len(syls):]:          # more notes than syllables: melisma on the last vowel
            groups[-1].append(extra)
        for sy, g in zip(syls, groups):
            off = float(np.clip(rng.normal(0, S.timing), -2.2 * S.timing, 2.2 * S.timing)) if S.timing else 0.0
            nl = [(T(s0) + (off if k == 0 else 0.0), T(s0 + ln), m) for k, (s0, ln, m) in enumerate(g)]
            events.append(dict(sy, notes=nl, word=wi))
    return events


def _sung_segments(events, S):
    """The time map: onsets end on the beat, codas end where the next onset starts (or at the note end before a rest),
    vowels are stretched over the note (transitions at natural speed). Returns segs (out0, out1, src0, src1, kind,
    event)."""
    segs = []

    def on_span(ev):
        a, b = ev['on']
        return max(a, b - (0.34 if ev.get('pickup') else ON_CAP)), b

    def co_span(ev):
        a, b = ev['co']
        return a, min(b, a + CO_CAP)

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
        od_i = od * ev.get('_od_scale', 1.0)
        if od_i > 0.002:
            prev_end = segs[-1][1] if segs else -1.0
            o0 = max(t_on - od_i, prev_end)
            if t_on - o0 > 0.002:
                segs.append((o0, t_on, oa, ob, 1, i))
        n0, n1 = ev['nu']
        tail = ev.get('tail')
        vt = ve - t_on
        tl = 0.0
        if tail:
            tl = min(tail[1] - tail[0], 0.4 * vt)
            vt -= tl
        nl_ = n1 - n0
        a = min(0.035, 0.3 * nl_)
        b = min(0.045, 0.3 * nl_)
        if vt <= a + b + 0.012:
            segs.append((t_on, t_on + vt, n0, n1, 2, i))
        else:
            segs += [(t_on, t_on + a, n0, n0 + a, 2, i), (t_on + a, t_on + vt - b, n0 + a, n1 - b, 2, i),
                     (t_on + vt - b, t_on + vt, n1 - b, n1, 2, i)]
        cur = t_on + vt
        if tail:
            segs.append((cur, cur + tl, tail[0], tail[1], 2, i))
            cur += tl
        if cd > 0.002:
            segs.append((cur, cur + cd, ca, cb, 3, i))
            cur += cd
        ev['t_end'] = cur
    return segs


def _sung_pitch(events, kind, ev, S, rng, fps):
    """Each frame's MIDI pitch: the owning note, smoothed into glides, plus scoops, vibrato, falls, overshoot, drift."""
    nout = len(kind)
    tout = np.arange(nout) * fps
    allnotes = [(n[0], n[1], n[2], i) for i, e in enumerate(events) for n in e['notes']]
    target = np.zeros(nout)
    for f in range(nout):
        i = ev[f]
        if i < 0 or kind[f] == 0:
            continue
        nts = events[i]['notes']
        j = 0 if kind[f] == 1 else len(nts) - 1 if kind[f] == 3 else \
            max([q for q, n in enumerate(nts) if tout[f] >= n[0] - 0.01] or [0])
        target[f] = nts[j][2]
    mid = np.zeros(nout)
    alpha = 1 - np.exp(-fps / max(1e-3, S.glide))
    cur = None
    for f in range(nout):
        if target[f] == 0:
            cur = None
            continue
        if cur is None:
            cur = target[f]
        cur += alpha * (target[f] - cur)
        mid[f] = cur
    cents = np.zeros(nout)
    vib_phase = rng.uniform(0, 2 * np.pi)
    starts = np.flatnonzero((target > 0) & (np.concatenate([[0.0], target[:-1]]) == 0))
    for f in starts:                                # scoop into each phrase
        sc = S.scoop * (0.7 + 0.6 * rng.random())
        tt = np.arange(min(int(0.22 / fps), nout - f)) * fps
        lag = max(0.0, events[ev[f]]['notes'][0][0] - tout[f]) if ev[f] >= 0 else 0.0
        cents[f:f + len(tt)] -= sc * np.exp(-np.maximum(0, tt - lag) / 0.04)
    for q, (t_on, t_off, m, i) in enumerate(allnotes):
        a = int(max(0, t_on / fps))
        nxt_on = allnotes[q + 1][0] if q + 1 < len(allnotes) else t_off + 0.3
        b = int(min(nout, nxt_on / fps))
        if b <= a:
            continue
        tt = tout[a:b] - t_on
        dur = t_off - t_on
        if dur > 0.3 and S.vib > 0:
            depth = S.vib * (0.8 + 0.4 * rng.random()) * _ease((tt - 0.16) / 0.3)
            rate = S.vib_rate * (1 + 0.05 * np.sin(2 * np.pi * 0.6 * tt + rng.uniform(0, 6)))
            ph = vib_phase + 2 * np.pi * np.cumsum(rate) * fps
            cents[a:b] += depth * np.sin(ph)
            vib_phase = ph[-1]
        last_in_phrase = q + 1 == len(allnotes) or allnotes[q + 1][0] - t_off > 0.25
        if S.fall and last_in_phrase and dur > 0.5 and rng.random() < 0.55:
            cents[a:b] -= 150 * np.clip((tt - (dur - 0.15)) / 0.15, 0, 1) ** 2
        if q > 0 and m - allnotes[q - 1][2] > 4 and t_on - allnotes[q - 1][1] < 0.06:   # overshoot on big leaps up
            cents[a:b] += 18 * np.exp(-np.maximum(0, tt - 0.07) / 0.05) * (tt > 0.05)
        if S.tuning:
            cents[a:b] += rng.normal(0, S.tuning)
    if S.drift and nout > 40:
        nz = rng.normal(0, 1, nout // 20 + 3)
        drift = np.interp(np.arange(nout) / 20, np.arange(len(nz)), nz)
        drift = signal.filtfilt(*signal.butter(2, 0.04), drift)
        cents += S.drift * drift / (np.std(drift) + 1e-9)
    return mid, cents, allnotes


def sing_track(perf, notes, bpm, S, total_sec, pre, rng, swing=0.5):
    """Sing a line as one performance. notes: (step16, len16, midi, text), '~' continuing a word; the words must be
    perf's words. Times are pre + steps (swung like the kit); rng draws the vibrato, scoops, falls and drift. Returns a
    Track with 5 ms frames, pre + total_sec long, whose phrases still end dead: release_ends() them to sing the line,
    or chop() it."""
    st = 60.0 / bpm / 4.0

    def T(s):
        s = float(s)
        base = np.floor(s / 2) * 2
        f = s - base
        return (base + (f * 2 * swing if f <= 1 else 2 * swing + (f - 1) * (2 - 2 * swing))) * st + pre

    words_ = group_words(notes)
    if tuple(w for w, _ in words_) != tuple(w.rstrip('.,!?') for w in perf.words):
        raise ValueError(f'notes sing {[w for w, _ in words_]}, the take says {perf.words}')
    events = _sung_events(perf, words_, T, S, rng)
    strong_vowels(perf, events)
    segs = _sung_segments(events, S)
    sp, ap, kind, ev, src_voiced, src = map_frames(perf, segs, total_sec + pre)
    nout = len(kind)
    voiced = (kind == 2) | (((kind == 1) | (kind == 3)) & src_voiced)
    mid, cents, allnotes = _sung_pitch(events, kind, ev, S, rng, FPS)
    f0 = np.where(voiced & (mid > 0), D.midi_hz(mid + cents / 100), 0.0)
    tout = np.arange(nout) * FPS
    gain = np.ones(nout)                           # dynamics: a swell over each note
    for t_on, t_off, m, i in allnotes:
        a, b = int(max(0, t_on / FPS)), int(min(nout, t_off / FPS))
        if b - a >= 4:
            tt = (tout[a:b] - t_on) / max(1e-3, t_off - t_on)
            swell = S.swell * np.sin(np.pi * np.clip(tt / 0.85, 0, 1)) - 2.0 * np.clip((tt - 0.8) / 0.2, 0, 1)
            gain[a:b] *= D.undb(swell)
    if S.cboost:
        cmask = signal.filtfilt(*signal.butter(1, 0.25), ((kind == 1) | (kind == 3)).astype(float))
        gain *= D.undb(S.cboost * np.clip(cmask, 0, 1))
    gain = signal.filtfilt(*signal.butter(1, 0.1), gain)
    Eo = np.log(np.sum(sp, axis=1) + 1e-12)        # hold sustained vowels steady
    flat = np.zeros(nout)
    for t_on, t_off, m, i in allnotes:
        a, b = int(max(0, t_on / FPS)), int(min(nout, t_off / FPS))
        vv = np.arange(a, b)[kind[a:b] == 2]
        if len(vv) >= 12:
            flat[vv] = np.clip(0.7 * (np.median(Eo[vv]) - Eo[vv]), -1.5, 2.3)
    flat = signal.filtfilt(*signal.butter(1, 0.15), flat)
    sp = sp * np.exp(flat)[:, None] * (gain ** 2)[:, None]
    fb = np.linspace(0, TTS_SR / 2, ap.shape[1])
    cap = np.interp(fb, [0, 1500, 3000, 6000, 12000], [0.06, 0.1, 0.3, 0.75, 1.0])[None, :]  # sung vowels: periodic
    ap = np.where((kind == 2)[:, None], np.minimum(ap, cap), ap)
    if S.breath:
        w = np.clip((fb - 2500) / 3500, 0, 1)[None, :]
        ap = np.where((kind == 2)[:, None], 1 - (1 - ap) * (1 - S.breath * w), ap)
    if abs(S.formant - 1) > 1e-3:
        k = np.arange(sp.shape[1], dtype=float)
        sp = np.stack([np.interp(k / S.formant, k, row) for row in sp])
    for e in events:
        e['t_on'] = e['notes'][0][0]
    return Track(f0, sp, np.clip(ap, 0, 1), kind, ev, events, FP, S.air, src)


# ------------------------------------------------------------------ the robot line
def robot_track(perf, grid, bpm, f0_hz, total_sec, pre, lead, fp=1.0, gap=0.012, margin=0.005, buzz=0.04,
                formant=1.0):
    """A robot line on the 16th grid: every syllable's onset consonant ends on its step and starts at least `margin`
    after its slice point, `lead` seconds before the step (slice k is [step_k - lead, step_k+1 - lead), with silence at
    each slice point); the vowel holds a flat f0_hz (a number, or one per syllable) on a buzzing source (aperiodicity
    scaled by `buzz`), and the coda ends `gap` seconds before the next slice. grid: (step16, len16) per syllable, in
    order. Frames are fp ms apart (1 ms keeps every onset within a millisecond of the grid)."""
    st = 60.0 / bpm / 4.0
    syls = [s for wi in range(len(perf.words)) for s in syllables_of(perf.toks, wi)]
    if len(syls) != len(grid):
        raise ValueError(f'{perf.text!r} has {len(syls)} syllables ({[s["syms"] for s in syls]}), the grid {len(grid)}')
    strong_vowels(perf, syls)
    pitches = np.broadcast_to(np.asarray(f0_hz, dtype=float), (len(syls),))
    segs, events = [], []
    for i, (sy, (step, ln)) in enumerate(zip(syls, grid)):
        t_on = pre + step * st
        nxt = pre + grid[i + 1][0] * st - lead if i + 1 < len(grid) else np.inf
        t_end = min(t_on + ln * st, nxt) - gap
        oa, ob = max(sy['on'][0], sy['on'][1] - ON_CAP), sy['on'][1]
        if ob - oa > 0.002:
            segs.append((t_on - min(ob - oa, lead - margin), t_on, oa, ob, 1, i))
        ca, cb = sy['co'][0], min(sy['end'], sy['co'][0] + CO_CAP)  # never the next word's onset: clean slices
        cd = min(cb - ca, 0.4 * (t_end - t_on))
        segs.append((t_on, t_end - cd, sy['nu'][0], sy['nu'][1], 2, i))
        if cd > 0.002:
            segs.append((t_end - cd, t_end, ca, cb, 3, i))
        events.append(dict(sy, step=step, len16=ln, t_on=t_on, t_end=t_end, slice=t_on - lead,
                           notes=[(t_on, t_end, float(pitches[i]))]))
    sp, ap, kind, ev, src_voiced, src = map_frames(perf, segs, total_sec + pre, fp)
    voiced = (kind == 2) | (((kind == 1) | (kind == 3)) & src_voiced)
    f0 = np.where(voiced & (ev >= 0), pitches[np.maximum(ev, 0)], 0.0)
    ap = np.where(voiced[:, None], ap * buzz, ap)
    if abs(formant - 1) > 1e-3:
        k = np.arange(sp.shape[1], dtype=float)
        sp = np.stack([np.interp(k / formant, k, row) for row in sp])
    return release_ends(Track(f0, sp, np.clip(ap, 0, 1), kind, ev, events, fp, 1.0, src), short=0.02, last=0.1)


# ------------------------------------------------------------------ synthesis at 48 kHz
def extend_hf(sp, ap, kind, air=1.0):
    """Bandwidth extension for 48 kHz synthesis. The TTS stops at 12 kHz and its vowels carry a vocoder 'sizzle' bump
    around 10-11.5 kHz, so: vowel frames keep their envelope up to 9 kHz and continue it along their own 5 -> 8 kHz
    slope (clamped, with an extra roll-off past 14 kHz); consonant frames keep theirs up to 11 kHz and continue the
    same way. The new band is mostly breathy. Bins keep their 23.4 Hz spacing (the 24 kHz envelope is the lower half)
    and the power doubles to keep the level."""
    nb = sp.shape[1]                        # 513 bins up to 12 kHz
    df = TTS_SR / 2 / (nb - 1)
    f = np.arange(2 * nb - 1) * df          # 1025 bins up to 24 kHz
    L = 10 * np.log10(sp + 1e-20)
    band = lambda lo, hi: L[:, int(lo / df):int(hi / df)].mean(axis=1)  # noqa: E731
    la, lb = band(4000, 6500), band(7000, 9000)
    slope = np.clip((lb - la) / np.log2(8000 / 5200), -24.0, -9.0)
    ext = (lb[:, None] + slope[:, None] * np.log2(np.maximum(f, 1.0) / 8000)[None, :]
           - 24 * np.log2(np.maximum(f, 14000) / 14000)[None, :])
    ext += 10 * np.log10(max(air, 1e-3)) - np.where(kind == 2, 0.0, 3.0)[:, None]
    Lx = np.concatenate([L, np.repeat(L[:, -1:], len(f) - nb, axis=1)], axis=1)
    vow = (kind == 2)[:, None]
    x0 = np.where(vow, 9000.0, 11000.0)
    x1 = np.where(vow, 10000.0, 12000.0)
    w = np.clip((f[None, :] - x0) / (x1 - x0), 0, 1)
    sp2 = 2.0 * 10 ** ((Lx * (1 - w) + ext * w) / 10)
    ap_hi = np.maximum(ap[:, int(8000 / df):int(10000 / df)].mean(axis=1), np.where(kind == 2, 0.7, 0.9))
    apx = np.concatenate([ap, np.repeat(ap[:, -1:], len(f) - nb, axis=1)], axis=1)
    ap2 = apx * (1 - w) + ap_hi[:, None] * w
    return np.ascontiguousarray(sp2), np.ascontiguousarray(np.clip(ap2, 0, 1))


def synth(track):
    """WORLD resynthesis of a track at 48 kHz (the 24 kHz features widened by extend_hf), float64 mono."""
    pw = _pyworld()
    sp, ap = extend_hf(track.sp, track.ap, track.kind, track.air)
    return pw.synthesize(np.ascontiguousarray(track.f0, dtype=np.float64), sp, ap, SR, track.fp)


def breaths(y, events, rng, level=0.007):
    """A quiet inhale before each phrase that starts after a gap: band-passed noise synthesised at 48 kHz."""
    out = y.copy()
    prev_end = -9.0
    peak = np.max(np.abs(y)) + 1e-9
    for e in events:
        start = e['t_on'] - 0.12
        if start - prev_end > 0.45:
            n = int(0.22 * SR)
            a = int((start - 0.24) * SR)
            if a > 0 and a + n < len(out):
                nz = D.bfilt(D.bfilt(rng.normal(0, 1, n), (1100, 4800), 'bandpass'), 6000, 'lowpass')
                out[a:a + n] += nz / (np.std(nz) + 1e-9) * np.sin(np.linspace(0, np.pi, n)) ** 2.2 * peak * level
        prev_end = e['notes'][-1][1]
    return out


# ------------------------------------------------------------------ chops, in the frame domain
def chop(track, first, last, pre=0.010, release=0.04, tail=0.24, tail_db=-6.0, floor_db=-100.0):
    """Cut syllables first..last (event indexes) out of a track in the WORLD frame domain, so the cut is the phrase's
    own audio with no edge to click: `pre` seconds of silence, the onset consonant whole, the vowel and coda, then a
    `release` that turns the last frames into pure breath (so voicing never stops dead) and down 6 dB, and a breath
    tail that carries on whatever sounded
    last (a breathy vowel, or the aspiration after a t): the last frames' spectral shape on pure noise, from tail_db
    under them falling linearly in dB to floor_db over `tail` seconds. Cut from a track whose phrase ends are not yet
    released. The last coda stops at the syllable's own end, before the word gap that may hold the next onset.
    Returns (track, junction seconds): where the tail starts."""
    fps = track.fps
    own = (track.ev >= first) & (track.ev <= last) & (track.kind > 0)
    own &= ~((track.ev == last) & (track.kind == 3) & (track.src > track.events[last]['end'] + 0.01))  # next word
    idx = np.flatnonzero(own)
    a = max(0, idx[0] - int(round(pre / fps)))
    b = idx[-1] + 1
    keep = slice(a, b)
    f0 = np.where(own[keep], track.f0[keep], 0.0)
    sp = np.where(own[keep][:, None], track.sp[keep], track.sp[keep] * 1e-7)
    ap = track.ap[keep].copy()
    kind = np.where(own[keep], track.kind[keep], 0)
    n_rel = min(int(round(release / fps)), len(f0) // 3)
    if n_rel:
        u = np.linspace(0, 1, n_rel)
        sp[-n_rel:] *= D.undb(-6.0 * u)[:, None] ** 2
        ap[-n_rel:] = 1 - (1 - ap[-n_rel:]) * (1 - u)[:, None]
    shape = sp[-3:].mean(0)
    n_tail = int(round(tail / fps))
    g = D.undb(np.linspace(tail_db, floor_db, n_tail))
    out = Track(np.concatenate([f0, np.zeros(n_tail)]), np.concatenate([sp, shape[None, :] * (g ** 2)[:, None]]),
                np.concatenate([ap, np.ones((n_tail, ap.shape[1]))]), np.concatenate([kind, np.full(n_tail, 3)]),
                np.concatenate([np.where(own[keep], track.ev[keep], -1), np.full(n_tail, last)]),
                [dict(e, t_on=e['t_on'] - a * fps) for e in track.events[first:last + 1]], track.fp, track.air,
                np.concatenate([np.where(own[keep], track.src[keep], -1.0), np.full(n_tail, -1.0)]))
    return out, len(f0) * fps


# ------------------------------------------------------------------ mixing (numpy and dsp48 only)
def shelf(x, fc, gain_db, kind='high'):
    """RBJ cookbook shelving EQ, slope S = 1."""
    A = 10 ** (gain_db / 40)
    w0 = 2 * np.pi * fc / SR
    al = np.sin(w0) / 2 * np.sqrt(2)
    c = np.cos(w0)
    s = 1 if kind == 'high' else -1
    b = np.array([A * ((A + 1) + s * (A - 1) * c + 2 * np.sqrt(A) * al), -2 * s * A * ((A - 1) + s * (A + 1) * c),
                  A * ((A + 1) + s * (A - 1) * c - 2 * np.sqrt(A) * al)])
    a = np.array([(A + 1) - s * (A - 1) * c + 2 * np.sqrt(A) * al, 2 * s * ((A - 1) - s * (A + 1) * c),
                  (A + 1) - s * (A - 1) * c - 2 * np.sqrt(A) * al])
    return signal.lfilter(b / a[0], a / a[0], x, axis=0)


def _smooth(v, ms_):
    k = max(1, int(ms_ / 1000 * SR))
    return np.convolve(v, np.ones(k) / k, 'same')


def deess(x, f_lo=5200, f_hi=11000, thr_db=-28, ratio=4.0, rel_db=-9.0, ref=None):
    """Split-band de-esser: the band is turned down only where it is both loud (above thr_db re `ref`, default the
    peak) and dominant (within rel_db of the full signal), so bright vowels pass and sibilants are tamed. Mono."""
    ref = np.max(np.abs(x)) if ref is None else ref
    band = D.bfilt(x, (f_lo, f_hi), 'bandpass')
    env = np.sqrt(_smooth(band ** 2, 4)) + 1e-9
    full = np.sqrt(_smooth(x ** 2, 4)) + 1e-9
    thr = np.maximum(ref * D.undb(thr_db), full * D.undb(rel_db)) + 1e-9
    g = _smooth(np.where(env > thr, (thr / env) ** (1 - 1 / ratio), 1.0), 4)
    return x - band + band * g


def compress(x, thr_db, ratio, attack, release, rms=0.01, block=16, ref=None):
    """Feed-forward compressor on a mono signal: RMS level (one-pole on the power, `rms` seconds, long enough that the
    gain never follows single pitch cycles), attack and release on the level in dB (seconds); the threshold is re
    `ref`, by default the signal's peak, so a take compresses the same at any level."""
    ref = (np.max(np.abs(x)) if ref is None else ref) + 1e-12
    nb = -(-len(x) // block)
    p = (np.concatenate([x, np.zeros(nb * block - len(x))]).reshape(nb, block) ** 2).mean(1)
    a_rms = np.exp(-block / (rms * SR))
    p = signal.lfilter([1 - a_rms], [1, -a_rms], p)
    lvl_in = 10 * np.log10(p / ref ** 2 + 1e-20)
    a_att, a_rel = np.exp(-block / (attack * SR)), np.exp(-block / (release * SR))
    lvl = np.empty(nb)
    s = -120.0
    for i, v in enumerate(lvl_in.tolist()):
        a = a_att if v > s else a_rel
        s = a * s + (1 - a) * v
        lvl[i] = s
    g = -np.maximum(lvl - thr_db, 0.0) * (1 - 1 / ratio)
    centres = (np.arange(nb) + 0.5) * block - 0.5
    return x * D.undb(np.interp(np.arange(len(x)), centres, g))


def polish(x, hp=100.0, sat=0.2, bright=4.0, air=1.5, levels=None, record=None):
    """One dry vocal (legacy vocals3.polish on dsp48 filters): a low cut at `hp` (nothing under ~100 Hz competes with
    the kick and sub), mud and harshness cuts, de-ess, fast then slow compression, a little 4x oversampled saturation,
    presence, brightness and air, a last de-ess. Mono in, mono out.

    The dynamic stages work re a reference level, by default the peak in front of each. levels: those references as
    polish_levels() measured them on another take, the whole phrase: a chop of it is then processed exactly as it was
    inside the phrase, instead of against its own peak (which, for a chop that opens on an s, is the s)."""
    queue = None if levels is None else list(levels)

    def ref(y):
        r = float(np.max(np.abs(y))) if queue is None else queue.pop(0)
        if record is not None:
            record.append(r)
        return r

    y = D.bfilt(x, hp, 'highpass', order=4)
    y = D.peaking(D.peaking(y, 300, -2.5, 1.0), 4800, -1.5, 2.0)
    y = deess(y, ref=ref(y))
    y = compress(y, -22, 3.5, 0.002, 0.06, ref=ref(y))
    y = compress(y, -16, 2.0, 0.025, 0.2, ref=ref(y))
    if sat:
        pk = ref(y) + 1e-12
        y = (1 - sat) * y + sat * pk * D.oversampled(lambda u: D.tanh_adaa(u, 2.2), y / pk)
    y = shelf(y, 180, 0.5, 'low')
    y = D.peaking(D.peaking(y, 3000, 2.0, 0.8), 8000, bright, 0.7)
    y = shelf(y, 12000, air, 'high')
    return deess(y, 6000, 12000, thr_db=-30, ratio=3.0, ref=ref(y))


def polish_levels(x, **kw):
    """The reference levels polish(x, **kw) works against, to polish pieces of x the same way."""
    out = []
    polish(x, record=out, **kw)
    return tuple(out)
