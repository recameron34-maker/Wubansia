"""Custom lexicon entries for bratty interjections and drawn-out vowels, plus a TTS engine that uses them."""
import os as _os  # noqa: E402
PACK_ROOT = _os.environ.get('PACK_ROOT') or _os.path.abspath(_os.path.join(_os.path.dirname(_os.path.abspath(__file__)), '..'))  # the legacy web-pack folder
import numpy as np, sherpa_onnx
import vocals as V
MD = V.MD
TEST_US = {
    'ewa': 'ˈ i u', 'ewb': 'ˈ ɪ u', 'ewc': 'ˈ i i u u', 'ewd': 'ˈ i ː u ː', 'ewe': 'ˈ i ʊ',
    'ugha': 'ˈ ʌ ɡ', 'ughb': 'ˈ ʌ ː ɡ', 'ughc': 'ˈ ʌ ʌ h',
    'mwah': 'm w ˈ ɑ', 'mwahb': 'm ˈ w ɑ ː',
    'sooa': 's ˈ O ː', 'soob': 's ˈ O O', 'hota': 'h ˈ ɑ ː t',
}
TEST_GB = {
    'ewa': 'ˈ i u', 'ewb': 'ˈ ɪ u', 'ewc': 'ˈ i i u u', 'ewd': 'ˈ i ː u ː', 'ewe': 'ˈ i ʊ',
    'ugha': 'ˈ ʌ ɡ', 'ughb': 'ˈ ʌ ː ɡ', 'mwah': 'm w ˈ ɑ', 'sooa': 's ˈ Q ː', 'soob': 's ˈ Q Q', 'hota': 'h ˈ ɒ ː t',
}


def build(extra_us, extra_gb, tag):
    for src, extra in (('lexicon-us-en.txt', extra_us), ('lexicon-gb-en.txt', extra_gb)):
        lines = open(MD + src, encoding='utf-8').read().splitlines()
        keep = [l for l in lines if l.split(' ', 1)[0] not in extra]
        keep += [f'{w} {p}' for w, p in extra.items()]
        open(f'{PACK_ROOT}/audio/lex/{tag}-{src}', 'w', encoding='utf-8').write('\n'.join(keep) + '\n')


_E = {}


def engine(voice, tag):
    key = ('gb' if voice.startswith('b') else 'us', tag)
    if key not in _E:
        lang, lex = ('en', 'lexicon-gb-en.txt') if key[0] == 'gb' else ('en-us', 'lexicon-us-en.txt')
        cfg = sherpa_onnx.OfflineTtsConfig(model=sherpa_onnx.OfflineTtsModelConfig(
            kokoro=sherpa_onnx.OfflineTtsKokoroModelConfig(model=MD + "model.onnx", voices=MD + "voices.bin", tokens=MD + "tokens.txt",
                                                           data_dir=MD + "espeak-ng-data", dict_dir=MD + "dict",
                                                           lexicon=f'{PACK_ROOT}/audio/lex/{tag}-{lex}', lang=lang),
            num_threads=2, provider="cpu"), max_num_sentences=1)
        _E[key] = sherpa_onnx.OfflineTts(cfg)
    return _E[key]


def tts(text, voice, speed=1.0, tag='brat'):
    a = engine(voice, tag).generate(text, sid=V.SPK[voice], speed=speed)
    x = np.array(a.samples, dtype=np.float64)
    assert len(x) > 0, text
    return V.trim(x, V.TTS_SR)
