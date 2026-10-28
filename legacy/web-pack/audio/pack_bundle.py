"""Pack several sounds into one Ogg FLAC (up to 8 channels: each sound's wet L/R, then its dry if it has one), so the site
stays under the artifact host's file limit. Each sound keeps its own entry in out/vox_flac.json, pointing at its channels of
the shared file: 'flac:audio/voxf/<bundle>.ogg?ch=<a,b>&n=<frames>' (and '?ch=<c>&n=<frames>' for the dry). The page decodes
the whole file once and cuts each sound out of it. Shorter sounds are zero-padded to the longest.
Usage: python3 pack_bundle.py <bundle-name> id [id ...]
"""
import os as _os  # noqa: E402
PACK_ROOT = _os.environ.get('PACK_ROOT') or _os.path.abspath(_os.path.join(_os.path.dirname(_os.path.abspath(__file__)), '..'))  # the legacy web-pack folder
import json, os, subprocess, sys
import numpy as np
import soundfile as sf

W = PACK_ROOT
SRC = f'{W}/out/vox_src'
DST = f'{W}/site/audio/voxf'
NATIVE = f'{W}/out/vox_flac_native'
MAP = f'{W}/out/vox_flac.json'


def bundle(name, ids):
    parts, ch = [], 0
    for sid in ids:
        w, sr = sf.read(f'{SRC}/vox/{sid}.wav', dtype='int16', always_2d=True)
        assert sr == 44100 and w.shape[1] == 2, (sid, sr, w.shape)
        dp = f'{SRC}/vox_dry/{sid}.wav'
        d = sf.read(dp, dtype='int16')[0] if os.path.exists(dp) else None
        parts.append((sid, w, d, ch))
        ch += 2 + (d is not None)
    assert ch <= 8, f'{name}: {ch} channels (FLAC holds at most 8)'
    n = max(max(len(w), len(d) if d is not None else 0) for _, w, d, _ in parts)
    x = np.zeros((n, ch), np.int16)
    for sid, w, d, c0 in parts:
        x[:len(w), c0:c0 + 2] = w
        if d is not None:
            x[:len(d), c0 + 2] = d
    nat, out = f'{NATIVE}/{name}.flac', f'{DST}/{name}.ogg'
    sf.write(nat, x, 44100, subtype='PCM_16', format='FLAC', compression_level=1.0)
    back = sf.read(nat, dtype='int16', always_2d=True)[0]
    assert back.shape == x.shape and np.array_equal(back, x), name
    subprocess.run(['ffmpeg', '-hide_banner', '-loglevel', 'error', '-y', '-i', nat, '-c:a', 'copy', '-map_metadata', '-1', out], check=True)
    size = os.path.getsize(out)
    entries = {}
    for sid, w, d, c0 in parts:
        e = {'file': f'flac:audio/voxf/{name}.ogg?ch={c0},{c0 + 1}&n={len(w)}', 'bytes': round(size * (2 + (d is not None)) / ch),
             'wav_bytes': os.path.getsize(f'{SRC}/vox/{sid}.wav') + (os.path.getsize(f'{SRC}/vox_dry/{sid}.wav') if d is not None else 0), 'bundle': name}
        if d is not None:
            e['dry'] = f'flac:audio/voxf/{name}.ogg?ch={c0 + 2}&n={len(d)}'
        entries[sid] = e
    return entries, size, ch


if __name__ == '__main__':
    name, ids = sys.argv[1], sys.argv[2:]
    m = json.load(open(MAP))
    entries, size, ch = bundle(name, ids)
    m.update(entries)
    json.dump(m, open(MAP, 'w'), indent=0, sort_keys=True)
    print(f'{name}.ogg: {len(ids)} sounds, {ch} channels, {size / 1e6:.2f} MB')
