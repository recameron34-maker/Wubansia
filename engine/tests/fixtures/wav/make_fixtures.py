"""Writes the WAV reader's fixtures (engine/tests/test_wav_reader.cpp) and expected.json, next to this file.

    /path/to/venv/bin/python engine/tests/fixtures/wav/make_fixtures.py

Needs numpy and soundfile (tools/sound/requirements.txt). The fixtures are a few frames each, so they're committed
(with `git add -f`: .gitignore keeps *.wav out of git). Most are written by libsndfile itself through soundfile; the
rest are built byte by byte here, for chunk layouts libsndfile doesn't write.

expected.json, integers and strings only, lists every fixture with:
- name and sha256: the test checks the bytes first, so a changed fixture fails loudly;
- result: "ok", or the error code readWav must give (format, rate, truncated);
- for "ok": frames, channels and bits, the float32 values soundfile reads (sf.read(dtype='float32')), planar
  (channel 0's frames, then channel 1's), each as its IEEE-754 bit pattern.

The script checks that soundfile's float64 read equals its float32 read for every readable fixture: for 16-bit and
24-bit PCM and float32 both are exact (s / 2^15, s / 2^23, the stored float), so "the same floats as soundfile"
holds for either dtype.
"""
import hashlib
import json
import struct
from pathlib import Path

import numpy as np
import soundfile as sf

HERE = Path(__file__).resolve().parent
RATE = 48000

I16 = np.array([-32768, -32767, -12345, -1, 0, 1, 2, 12345, 32767], dtype=np.int16)
I24 = np.array([-8388608, -8388607, -1234567, -1, 0, 1, 2, 7654321], dtype=np.int64)  # 8 frames
F32 = np.array([0.0, -0.0, 0.5, -1.0, 1.0, 1.5, -2.0, 1e-8, 1.17549435e-38, 1e-40, 0.123456789],
               dtype=np.float32)  # over-range, tiny and subnormal values pass through untouched


def stereo(a, b):
    return np.stack([a, b], axis=1)


def pcm24_input(s24):
    """libsndfile writes 24-bit PCM from int32 input as the top 24 bits: s24 << 8."""
    return (np.asarray(s24, dtype=np.int64) << 8).astype(np.int32)


def chunk(cid, payload):
    """A RIFF chunk: id, size, payload, and the pad byte an odd size needs."""
    out = cid + struct.pack('<I', len(payload)) + payload
    return out + (b'\0' if len(payload) % 2 else b'')


def riff(*chunks):
    body = b'WAVE' + b''.join(chunks)
    return b'RIFF' + struct.pack('<I', len(body)) + body


def fmt_pcm(channels, bits, rate=RATE, tag=1):
    align = channels * bits // 8
    return chunk(b'fmt ', struct.pack('<HHIIHH', tag, channels, rate, rate * align, align, bits))


GUID_TAIL = bytes.fromhex('000000001000800000aa00389b71')  # KSDATAFORMAT_SUBTYPE_*: the tag, then this


def fmt_extensible(channels, bits, subformat_tag, valid_bits=None, rate=RATE):
    align = channels * bits // 8
    mask = 0x4 if channels == 1 else 0x3
    guid = struct.pack('<H', subformat_tag) + GUID_TAIL
    ext = struct.pack('<HHI', 22, valid_bits if valid_bits is not None else bits, mask) + guid
    return chunk(b'fmt ', struct.pack('<HHIIHH', 0xFFFE, channels, rate, rate * align, align, bits) + ext)


def zero_peak_timestamp(path):
    """libsndfile's PEAK chunk (float files) holds the time it was written: zero it, so a second run writes the same
    bytes."""
    data = bytearray(path.read_bytes())
    pos = 12
    while pos + 8 <= len(data):
        size = struct.unpack_from('<I', data, pos + 4)[0]
        if data[pos:pos + 4] == b'PEAK':
            struct.pack_into('<I', data, pos + 12, 0)  # after the version
        pos += 8 + size + (size & 1)
    path.write_bytes(bytes(data))


def written(name, data, subtype, fmt='WAV', rate=RATE):
    path = HERE / name
    sf.write(path, data, rate, subtype=subtype, format=fmt)
    zero_peak_timestamp(path)
    return path


def built(name, data):
    path = HERE / name
    path.write_bytes(data)
    return path


def main():
    readable = []  # (path, what it shows)
    refused = []   # (path, code)

    # libsndfile's own files: plain fmt + data for PCM, fmt + fact + PEAK + data for float
    readable.append(written('pcm16_mono.wav', I16, 'PCM_16'))
    readable.append(written('pcm16_stereo.wav', stereo(I16, I16[::-1].copy()), 'PCM_16'))
    readable.append(written('pcm24_mono.wav', pcm24_input(I24[:7]), 'PCM_24'))  # 21 data bytes: an odd chunk
    readable.append(written('pcm24_stereo.wav', stereo(pcm24_input(I24), pcm24_input(I24[::-1])), 'PCM_24'))
    readable.append(written('float32_mono.wav', F32, 'FLOAT'))
    readable.append(written('float32_stereo.wav', stereo(F32, -F32), 'FLOAT'))
    readable.append(written('extensible_pcm24_stereo.wav', stereo(pcm24_input(I24), pcm24_input(-I24 - 1)),
                            'PCM_24', fmt='WAVEX'))
    readable.append(written('extensible_float32_mono.wav', F32, 'FLOAT', fmt='WAVEX'))

    # Built by hand: chunks before and after fmt and data, odd sizes with their pad bytes, an extensible fmt
    # with a smaller valid-bits count (20 in 24), as other writers make them.
    pcm = I16[:6].astype('<i2').tobytes()
    readable.append(built('pcm16_chunks.wav', riff(
        chunk(b'LIST', b'INFO' + chunk(b'INAM', b'abc')),  # an odd subchunk, padded
        chunk(b'JUNK', b'\0' * 4),
        fmt_pcm(1, 16),
        chunk(b'abcd', b'xyz'),  # an unknown chunk of odd size: 3 bytes and a pad byte
        chunk(b'data', pcm),
        chunk(b'cue ', b'\0' * 4))))
    s24 = (I24 >> 4) << 4  # 20 significant bits
    data24 = b''.join(struct.pack('<i', int(v))[:3] for v in s24)
    readable.append(built('extensible_pcm20in24_mono.wav', riff(fmt_extensible(1, 24, 1, valid_bits=20),
                                                               chunk(b'data', data24))))

    # Refused.
    refused.append((written('rate_44100.wav', I16, 'PCM_16', rate=44100), 'rate'))
    refused.append((written('rate_96000_float.wav', F32, 'FLOAT', rate=96000), 'rate'))
    refused.append((written('extensible_alaw.wav', I16, 'ALAW', fmt='WAVEX'), 'format'))  # non-PCM subformat
    refused.append((written('alaw.wav', I16, 'ALAW'), 'format'))
    refused.append((written('pcm8.wav', I16, 'PCM_U8'), 'format'))
    refused.append((written('pcm32.wav', I16, 'PCM_32'), 'format'))
    refused.append((written('float64.wav', F32, 'DOUBLE'), 'format'))
    refused.append((written('three_channels.wav', np.stack([I16, I16, I16], axis=1), 'PCM_16'), 'format'))
    full = (HERE / 'pcm24_stereo.wav').read_bytes()
    refused.append((built('truncated.wav', full[:-7]), 'truncated'))  # the last frame and a bit of the one before

    files = []
    for path in readable:
        y32, sr = sf.read(path, dtype='float32', always_2d=True)
        y64, _ = sf.read(path, dtype='float64', always_2d=True)
        assert sr == RATE, path
        assert np.array_equal(y64, y32.astype(np.float64)), f'{path.name}: float64 and float32 reads differ'
        bits = np.ascontiguousarray(y32.T).reshape(-1).view(np.uint32)
        files.append({'name': path.name, 'sha256': hashlib.sha256(path.read_bytes()).hexdigest(), 'result': 'ok',
                      'frames': int(y32.shape[0]), 'channels': int(y32.shape[1]),
                      'bits': [int(b) for b in bits]})
    for path, code in refused:
        files.append({'name': path.name, 'sha256': hashlib.sha256(path.read_bytes()).hexdigest(), 'result': code})

    text = json.dumps({'files': files}, indent=2) + '\n'
    (HERE / 'expected.json').write_text(text, encoding='utf-8', newline='\n')
    total = sum((HERE / f['name']).stat().st_size for f in files)
    print(f'{len(files)} fixtures, {total} bytes')


if __name__ == '__main__':
    main()
