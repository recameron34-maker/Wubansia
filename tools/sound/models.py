"""The build-time models, Kokoro-82M (TTS) and Whisper base.en (ASR), run through sherpa-onnx.

They are not in git and not pip packages: download them once and point WUBANSIA_MODELS_DIR at the folder that holds
kokoro-multi-lang-v1_0/ and sherpa-onnx-whisper-base.en/ (README.md). sherpa-onnx comes from requirements-vocals.txt.

Kokoro renders at 24 kHz. Masters never resample that audio (ADR 0005, D4): the vocal chain resynthesises it at
48 kHz with WORLD. asr() resamples to 16 kHz only to listen to a file, never to make one.

Provenance: the sherpa-onnx calls follow legacy/web-pack/audio/vocals.py (engine, tts) and vocals2.py (asr).
"""
import os
from pathlib import Path

import numpy as np
from scipy import signal

ENV = 'WUBANSIA_MODELS_DIR'
KOKORO = 'kokoro-multi-lang-v1_0'
WHISPER = 'sherpa-onnx-whisper-base.en'
TTS_SR = 24000
# Kokoro v1.0 speaker ids, in voices.bin order
SPEAKERS = ('af_alloy af_aoede af_bella af_heart af_jessica af_kore af_nicole af_nova af_river af_sarah af_sky '
            'am_adam am_echo am_eric am_fenrir am_liam am_michael am_onyx am_puck am_santa '
            'bf_alice bf_emma bf_isabella bf_lily bm_daniel bm_fable bm_george bm_lewis').split()
_TTS = {}
_ASR = []


class ModelsMissing(RuntimeError):
    pass


def models_dir():
    """The models folder from WUBANSIA_MODELS_DIR, checked."""
    value = os.environ.get(ENV)
    if not value:
        raise ModelsMissing(f'{ENV} is not set. Point it at the folder that holds {KOKORO}/ and {WHISPER}/ '
                            '(see tools/sound/README.md, "Models").')
    path = Path(value).expanduser()
    if not path.is_dir():
        raise ModelsMissing(f'{ENV}={value} is not a folder.')
    return path


def _model(sub, *files):
    d = models_dir() / sub
    missing = [f for f in files if not (d / f).exists()]
    if missing:
        raise ModelsMissing(f'{d} lacks {", ".join(missing)}: download {sub} into {models_dir()} (README.md).')
    return d


def _sherpa():
    try:
        import sherpa_onnx
    except ImportError as e:
        raise ModelsMissing('sherpa-onnx is not installed: pip install -r tools/sound/requirements-vocals.txt') from e
    return sherpa_onnx


def _tts_engine(voice):
    key = 'gb' if voice.startswith('b') else 'us'
    if key not in _TTS:
        so = _sherpa()
        d = _model(KOKORO, 'model.onnx', 'voices.bin', 'tokens.txt', 'espeak-ng-data', 'dict')
        lang, lex = ('en', 'lexicon-gb-en.txt') if key == 'gb' else ('en-us', 'lexicon-us-en.txt')
        kokoro = so.OfflineTtsKokoroModelConfig(model=str(d / 'model.onnx'), voices=str(d / 'voices.bin'),
                                                tokens=str(d / 'tokens.txt'), data_dir=str(d / 'espeak-ng-data'),
                                                dict_dir=str(d / 'dict'), lexicon=str(d / lex), lang=lang)
        cfg = so.OfflineTtsConfig(model=so.OfflineTtsModelConfig(kokoro=kokoro, num_threads=2, provider='cpu'),
                                  max_num_sentences=1)
        _TTS[key] = so.OfflineTts(cfg)
    return _TTS[key]


def tts(text, voice, speed=1.0):
    """One Kokoro take at 24 kHz (float64). voice: a name from SPEAKERS."""
    if voice not in SPEAKERS:
        raise ValueError(f'unknown Kokoro voice {voice!r}')
    audio = _tts_engine(voice).generate(text, sid=SPEAKERS.index(voice), speed=speed)
    x = np.array(audio.samples, dtype=np.float64)
    if not len(x):
        raise RuntimeError(f'Kokoro returned no audio for {text!r}')
    return x


def asr(x, sr):
    """Whisper base.en transcript of mono audio at sample rate sr."""
    if not _ASR:
        so = _sherpa()
        d = _model(WHISPER, 'base.en-encoder.int8.onnx', 'base.en-decoder.int8.onnx', 'base.en-tokens.txt')
        _ASR.append(so.OfflineRecognizer.from_whisper(encoder=str(d / 'base.en-encoder.int8.onnx'),
                                                      decoder=str(d / 'base.en-decoder.int8.onnx'),
                                                      tokens=str(d / 'base.en-tokens.txt'), language='en',
                                                      task='transcribe', num_threads=2))
    y = np.asarray(x, dtype=np.float64)
    y = signal.resample_poly(y, 16000, sr) if sr != 16000 else y
    stream = _ASR[0].create_stream()
    stream.accept_waveform(16000, y.astype(np.float32))
    _ASR[0].decode_stream(stream)
    return stream.result.text.strip()
