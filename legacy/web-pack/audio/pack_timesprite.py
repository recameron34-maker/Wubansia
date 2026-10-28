"""Two space savers for the site, both read by the page as 'flac:<path>?ch=<channels>&n=<frames>&o=<first frame>':

  sprites   the wave-3/4 WAV sprites (site/audio/s3, s4) become Ogg FLAC sprites with the same frame layout; the WAVs move to
            out/sprites_wav/ (kept for reference and tests). build_site.py rewrites 'sprite:' URLs to the FLAC ones.
  stabs     sounds laid end to end in one 3-channel Ogg FLAC (wet L, wet R, dry), each at its own offset, with a short gap;
            out/vox_flac.json gets an entry per sound.

Usage: python3 pack_timesprite.py sprites
       python3 pack_timesprite.py stabs <file-name> id [id ...]
"""
import os as _os  # noqa: E402
PACK_ROOT = _os.environ.get('PACK_ROOT') or _os.path.abspath(_os.path.join(_os.path.dirname(_os.path.abspath(__file__)), '..'))  # the legacy web-pack folder
import glob, json, os, shutil, subprocess, sys
import numpy as np
import soundfile as sf

W = PACK_ROOT
SITE = f'{W}/site'
SRC = f'{W}/out/vox_src'
NATIVE = f'{W}/out/vox_flac_native'
MAP = f'{W}/out/vox_flac.json'
KEEP = f'{W}/out/sprites_wav'


def to_ogg(x, name, out):
    nat = f'{NATIVE}/{name}.flac'
    sf.write(nat, x, 44100, subtype='PCM_16', format='FLAC', compression_level=1.0)
    back = sf.read(nat, dtype='int16', always_2d=True)[0]
    assert back.shape == x.shape and np.array_equal(back, x), name
    subprocess.run(['ffmpeg', '-hide_banner', '-loglevel', 'error', '-y', '-i', nat, '-c:a', 'copy', '-map_metadata', '-1', out], check=True)
    return os.path.getsize(out)


def sprites():
    os.makedirs(KEEP, exist_ok=True)
    os.makedirs(NATIVE, exist_ok=True)
    for p in sorted(glob.glob(f'{SITE}/audio/s3/*.wav') + glob.glob(f'{SITE}/audio/s4/*.wav')):
        x, sr = sf.read(p, dtype='int16', always_2d=True)
        assert sr == 44100
        name = os.path.basename(p)[:-4]
        size = to_ogg(x, name, p[:-4] + '.ogg')
        shutil.move(p, f'{KEEP}/{os.path.basename(p)}')
        print(f'{name}: {x.shape[1]} ch, {len(x)} frames, {os.path.getsize(KEEP + "/" + name + ".wav") / 1e6:.2f} -> {size / 1e6:.2f} MB')


def stabs(name, ids, gap=0.05):
    parts, n = [], 0
    for sid in ids:
        w, sr = sf.read(f'{SRC}/vox/{sid}.wav', dtype='int16', always_2d=True)
        d = sf.read(f'{SRC}/vox_dry/{sid}.wav', dtype='int16')[0]
        assert sr == 44100 and w.shape[1] == 2 and d.ndim == 1, sid
        parts.append((sid, w, d, n))
        n += max(len(w), len(d)) + int(gap * SR_)
    x = np.zeros((n, 3), np.int16)
    for sid, w, d, o in parts:
        x[o:o + len(w), :2] = w
        x[o:o + len(d), 2] = d
    size = to_ogg(x, name, f'{SITE}/audio/voxf/{name}.ogg')
    m = json.load(open(MAP))
    for sid, w, d, o in parts:
        m[sid] = {'file': f'flac:audio/voxf/{name}.ogg?ch=0,1&n={len(w)}&o={o}', 'dry': f'flac:audio/voxf/{name}.ogg?ch=2&n={len(d)}&o={o}',
                  'bytes': round(size * max(len(w), len(d)) / n), 'wav_bytes': os.path.getsize(f'{SRC}/vox/{sid}.wav') + os.path.getsize(f'{SRC}/vox_dry/{sid}.wav'),
                  'sprite': name}
    json.dump(m, open(MAP, 'w'), indent=0, sort_keys=True)
    print(f'{name}.ogg: {len(ids)} sounds, {n / SR_:.1f} s, {size / 1e6:.2f} MB')


def plain(limit=5_000_000):
    """The pack's remaining plain WAVs (bass, keys, fx, fills, hits, inst, loops) laid end to end in Ogg FLAC time sprites,
    by folder and channel count, about `limit` bytes of WAV per sprite (so a first decode stays short). Writes
    out/plain_flac.json (old URL -> 'flac:' URL) for build_site.py; the WAVs move to out/plain_wav/<folder>/."""
    out_map = {}
    os.makedirs(f'{SITE}/audio/s0', exist_ok=True)
    for folder in ['hits', 'inst', 'bass', 'keys', 'fx', 'fills', 'loops']:
        files = sorted(glob.glob(f'{SITE}/audio/{folder}/*.wav'))
        by_ch = {}
        for p in files:
            info = sf.info(p)
            assert info.samplerate == 44100 and info.subtype == 'PCM_16', p
            by_ch.setdefault(info.channels, []).append(p)
        for ch, group in sorted(by_ch.items()):
            chunks, cur, size = [], [], 0
            for p in group:
                b = os.path.getsize(p)
                if cur and size + b > limit:
                    chunks.append(cur); cur, size = [], 0
                cur.append(p); size += b
            if cur:
                chunks.append(cur)
            for k, chunk in enumerate(chunks):
                name = f"{folder}_{'mono' if ch == 1 else 'stereo'}" + (f'{k + 1}' if len(chunks) > 1 else '')
                parts, n = [], 0
                for p in chunk:
                    x = sf.read(p, dtype='int16', always_2d=True)[0]
                    parts.append((p, x, n))
                    n += len(x)
                y = np.zeros((n, ch), np.int16)
                for p, x, o in parts:
                    y[o:o + len(x)] = x
                size_ogg = to_ogg(y, 's0_' + name, f'{SITE}/audio/s0/{name}.ogg')
                for p, x, o in parts:
                    rel = os.path.relpath(p, SITE)
                    out_map[rel] = f"flac:audio/s0/{name}.ogg?ch={'0,1' if ch == 2 else '0'}&n={len(x)}&o={o}"
                print(f'{name}.ogg: {len(chunk)} files, {sum(os.path.getsize(p) for p in chunk) / 1e6:.2f} MB WAV -> {size_ogg / 1e6:.2f} MB')
    json.dump(out_map, open(f'{W}/out/plain_flac.json', 'w'), indent=0, sort_keys=True)
    for rel in out_map:
        dst = f'{W}/out/plain_wav/' + rel[len('audio/'):]
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        shutil.move(f'{SITE}/{rel}', dst)
    for folder in ['hits', 'inst', 'bass', 'keys', 'fx', 'fills', 'loops']:
        d = f'{SITE}/audio/{folder}'
        if os.path.isdir(d) and not os.listdir(d):
            os.rmdir(d)
    print(len(out_map), 'files packed')


SR_ = 44100
if __name__ == '__main__':
    if sys.argv[1] == 'sprites':
        sprites()
    elif sys.argv[1] == 'stabs':
        stabs(sys.argv[2], sys.argv[3:])
    elif sys.argv[1] == 'plain':
        plain()
