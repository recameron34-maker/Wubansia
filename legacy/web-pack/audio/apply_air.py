"""Give the spoken, explicit and ad-lib vocals air above their TTS cutoff (dry always; wet unless the style is meant to be
band-limited: radio, dark). Originals are kept in out/vox_preair. Loudness is kept; peaks stay at -1 dBFS."""
import os as _os  # noqa: E402
PACK_ROOT = _os.environ.get('PACK_ROOT') or _os.path.abspath(_os.path.join(_os.path.dirname(_os.path.abspath(__file__)), '..'))  # the legacy web-pack folder
import json, os, shutil, sys
import numpy as np
import soundfile as sf
import air
import vocals3 as V3

W = PACK_ROOT
SRC = f'{W}/out/vox_src'
BAK = f'{W}/out/vox_preair'
items = {}
for f in ('manifest_vox.json', 'manifest_vox2.json', 'manifest_brat.json', 'manifest_v6.json'):
    for e in json.load(open(f'{W}/out/{f}')):
        items[e['id']] = e
todo = [e for e in items.values() if e['group'] in ('clean', 'explicit', 'adlibs')]
if sys.argv[1:]:
    todo = [e for e in todo if e['id'] in sys.argv[1:]]
report = {}
for e in sorted(todo, key=lambda e: e['id']):
    sid = e['id']
    r = {}
    for kind in ('vox_dry', 'vox'):
        path = f'{SRC}/{kind}/{sid}.wav'
        if not os.path.exists(path):
            continue
        if kind == 'vox' and e.get('style') in ('radio', 'dark'):
            r[kind] = 'kept (style)'
            continue
        os.makedirs(f'{BAK}/{kind}', exist_ok=True)
        bak = f'{BAK}/{kind}/{sid}.wav'
        if not os.path.exists(bak):
            shutil.copyfile(path, bak)
        x, sr = sf.read(bak)                      # always start from the original
        y, c, g = air.add_air(x)
        if g is None:
            shutil.copyfile(bak, path)
            r[kind] = f'kept (cutoff {c:.0f})'
            continue
        y = V3.finish(y, V3.k_loudness(x))
        sf.write(path, np.clip(y, -1, 1), sr, subtype='PCM_16')
        r[kind] = f'cutoff {c:.0f} gain {g:+.1f} dB'
    report[sid] = r
    print(sid, e.get('style'), r, flush=True)
json.dump(report, open(f'{W}/out/air_report.json', 'w'), indent=1)
print(len(report), 'vocals done')
