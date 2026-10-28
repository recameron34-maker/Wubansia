"""Put vocal masters (and their manifest_v6 voice/dur/peaks) back to what the site serves, decoding the site's Ogg FLAC
losslessly. Usage: python3 restore_from_site.py id [id ...]"""
import os as _os  # noqa: E402
PACK_ROOT = _os.environ.get('PACK_ROOT') or _os.path.abspath(_os.path.join(_os.path.dirname(_os.path.abspath(__file__)), '..'))  # the legacy web-pack folder
import json, re, subprocess, sys
import numpy as np
import soundfile as sf

W = PACK_ROOT
VF = json.load(open(f'{W}/out/vox_flac.json'))
html = open(f'{W}/site/index.html').read()
site = {e['id']: e for e in json.loads('{' + re.search(r'"items":\[.*?\],"bytes":\d+\}', html, re.S).group(0))['items']}
man = json.load(open(f'{W}/out/manifest_v6.json'))
for sid in sys.argv[1:]:
    e = VF[sid]
    path = re.match(r'^flac:([^?]+)\?', e['file']).group(1)
    nw = int(re.search(r'n=(\d+)', e['file']).group(1))
    raw = subprocess.run(['ffmpeg', '-hide_banner', '-loglevel', 'error', '-i', f'{W}/site/{path}', '-f', 's16le', '-acodec', 'pcm_s16le', '-'],
                         capture_output=True, check=True).stdout
    ch = 3 if e.get('dry') else 2
    x = np.frombuffer(raw, dtype='<i2').reshape(-1, ch)
    sf.write(f'{W}/out/vox_src/vox/{sid}.wav', x[:nw, :2], 44100, subtype='PCM_16')
    if e.get('dry'):
        nd = int(re.search(r'n=(\d+)', e['dry']).group(1))
        sf.write(f'{W}/out/vox_src/vox_dry/{sid}.wav', x[:nd, 2], 44100, subtype='PCM_16')
    s = site[sid]
    for m in man:
        if m['id'] == sid:
            for k in ('voice', 'dur', 'peaks', 'title', 'note'):
                if k in s:
                    m[k] = s[k]
    print(sid, 'restored', x.shape)
json.dump(man, open(f'{W}/out/manifest_v6.json', 'w'), indent=0)
