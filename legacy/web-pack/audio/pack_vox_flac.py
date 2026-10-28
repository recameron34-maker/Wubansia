"""Pack vocals for the site: each vocal's wet (stereo) and dry (mono) WAVs from out/vox_src become one FLAC (channels: wet L,
wet R, dry; the shorter part zero-padded), muxed into an Ogg container (the artifact host serves .ogg, not .flac) in
site/audio/voxf/<id>.ogg. Writes out/vox_flac.json: {id: {file, dry, bytes, wav_bytes}} with 'flac:<path>?ch=0,1&n=<frames>'
URLs. Only re-encodes when a source is newer than its file.
Usage: python3 pack_vox_flac.py id [id ...] | all
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


def pack(sid):
    wp, dp = f'{SRC}/vox/{sid}.wav', f'{SRC}/vox_dry/{sid}.wav'
    has_dry = os.path.exists(dp)
    nat, out = f'{NATIVE}/{sid}.flac', f'{DST}/{sid}.ogg'
    w, sr = sf.read(wp, dtype='int16', always_2d=True)
    assert sr == 44100 and w.shape[1] == 2, (sid, sr, w.shape)
    d = sf.read(dp, dtype='int16')[0] if has_dry else None
    if d is not None:
        assert d.ndim == 1, sid
    n = max(len(w), len(d) if d is not None else 0)
    x = np.zeros((n, 3 if has_dry else 2), np.int16)
    x[:len(w), :2] = w
    if has_dry:
        x[:len(d), 2] = d
    newest = max(os.path.getmtime(wp), os.path.getmtime(dp) if has_dry else 0)
    if not (os.path.exists(out) and os.path.getmtime(out) >= newest):
        sf.write(nat, x, 44100, subtype='PCM_16', format='FLAC', compression_level=1.0)
        back = sf.read(nat, dtype='int16', always_2d=True)[0]
        assert back.shape == x.shape and np.array_equal(back, x), sid
        subprocess.run(['ffmpeg', '-hide_banner', '-loglevel', 'error', '-y', '-i', nat, '-c:a', 'copy', '-map_metadata', '-1', out], check=True)
    e = {'file': f'flac:audio/voxf/{sid}.ogg?ch=0,1&n={len(w)}', 'bytes': os.path.getsize(out),
         'wav_bytes': os.path.getsize(wp) + (os.path.getsize(dp) if has_dry else 0)}
    if has_dry:
        e['dry'] = f'flac:audio/voxf/{sid}.ogg?ch=2&n={len(d)}'
    return e


if __name__ == '__main__':
    os.makedirs(DST, exist_ok=True)
    os.makedirs(NATIVE, exist_ok=True)
    m = json.load(open(MAP)) if os.path.exists(MAP) else {}
    ids = sys.argv[1:]
    if ids == ['all']:
        ids = sorted(f[:-4] for f in os.listdir(f'{SRC}/vox') if f.endswith('.wav'))
    for sid in ids:
        m[sid] = pack(sid)
    json.dump(m, open(MAP, 'w'), indent=0, sort_keys=True)
    tot = sum(v['bytes'] for k, v in m.items() if k in ids)
    wav = sum(v['wav_bytes'] for k, v in m.items() if k in ids)
    print(len(ids), 'vocals packed:', round(tot / 1e6, 1), 'MB FLAC (from', round(wav / 1e6, 1), 'MB WAV)')
