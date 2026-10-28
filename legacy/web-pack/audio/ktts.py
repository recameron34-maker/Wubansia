"""Direct Kokoro v1.0 ONNX driver with our own phonemes (misaki lexicon + custom entries).
Lets us shape pronunciations the stock front-end can't: 'Ew', 'Ugh', drawn-out vowels."""
import re
import numpy as np
import onnxruntime as ort
import vocals as V

MD = V.MD
_TOK = {}
for line in open(MD + 'tokens.txt', encoding='utf-8'):
    line = line.rstrip('\n')
    if not line:
        continue
    sym, idx = line.rsplit(' ', 1)
    _TOK[sym if sym else ' '] = int(idx)
_TOK[' '] = 16
_VOX = np.fromfile(MD + 'voices.bin', dtype=np.float32).reshape(54, 510, 256)
_SESS = None
_LEX = {}


def lex(gb):
    k = 'gb' if gb else 'us'
    if k not in _LEX:
        d = {}
        for line in open(MD + f'lexicon-{k}-en.txt', encoding='utf-8'):
            w, _, p = line.rstrip('\n').partition(' ')
            if w and w not in d:
                d[w] = p.replace(' ', '')
        _LEX[k] = d
    return _LEX[k]


CUSTOM = {}   # word -> (us, gb) phoneme strings


def word_ph(w, gb):
    lw = w.lower()
    if lw in CUSTOM:
        return CUSTOM[lw][1 if gb else 0]
    L = lex(gb)
    if lw in L:
        return L[lw]
    if lw.endswith("'s") and lw[:-2] in L:          # possessive / 'is' contraction
        base = L[lw[:-2]]
        last = base.rstrip('ˈˌː')[-1]
        return base + ('ɪz' if last in 'szʃʒʧʤ' else 's' if last in 'ptkfθ' else 'z')
    raise KeyError(f'no phonemes for {w!r}')


def phonemize(text, gb=False):
    """Words -> lexicon phonemes; punctuation kept; '{...}' passes raw phonemes through."""
    out = ''
    for m in re.finditer(r"\{[^}]*\}|[A-Za-z][A-Za-z'’-]*|[.,!?;:…—]", text):
        t = m.group(0)
        if t.startswith('{'):
            ph = t[1:-1]
            out += (' ' if out and out[-1] not in ' ' else '') + ph
        elif re.match(r'[.,!?;:…—]', t):
            out = out.rstrip(' ') + t + ' '
        else:
            parts = [p for p in re.split(r'-', t.replace('’', "'")) if p]
            ph = ' '.join(word_ph(p, gb) for p in parts)
            out += (' ' if out and out[-1] != ' ' else '') + ph
    return out.strip()


def synth(phonemes, voice, speed=1.0):
    global _SESS
    if _SESS is None:
        so = ort.SessionOptions(); so.intra_op_num_threads = 4
        _SESS = ort.InferenceSession(MD + 'model.onnx', so, providers=['CPUExecutionProvider'])
    ids = [_TOK[c] for c in phonemes if c in _TOK]
    missing = [c for c in phonemes if c not in _TOK]
    assert not missing, missing
    assert len(ids) < 509
    style = _VOX[V.SPK[voice], len(ids)][None, :]
    audio = _SESS.run(None, {'tokens': np.array([[0] + ids + [0]], dtype=np.int64), 'style': style.astype(np.float32),
                             'speed': np.array([speed], dtype=np.float32)})[0]
    return V.trim(np.asarray(audio, dtype=np.float64), V.TTS_SR)


def tts(text, voice, speed=1.0):
    return synth(phonemize(text, voice.startswith('b')), voice, speed)
