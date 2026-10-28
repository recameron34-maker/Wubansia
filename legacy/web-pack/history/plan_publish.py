"""Batched move of the site's vocals to FLAC, so every published version stays consistent and inside the artifact limits.
  python3 plan_publish.py <batch>   batch 1 / 2: half of the existing vocals each switch to FLAC (old WAVs removed)
                                    batch 3: the new vocals go live
Writes out/flac_live.json, rebuilds site/index.html and writes out/publish_batch<k>.json: {published path: source | null}.
"""
import json, os, subprocess, sys

W = '/home/claude/work'
OLD = [e['id'] for f in ('manifest_vox.json', 'manifest_vox2.json', 'manifest_brat.json') for e in json.load(open(f'{W}/out/{f}'))]
V6 = json.load(open(f'{W}/out/manifest_v6.json'))
NEW = [e['id'] for e in V6 if e['id'] not in set(OLD)]
VF = json.load(open(f'{W}/out/vox_flac.json'))


def batches():
    """Split the old vocals in two, balancing the number of files each publish touches (FLAC added + WAVs removed)."""
    cost = {i: 1 + 1 + (1 if os.path.exists(f'{W}/site/audio/vox_dry/{i}.wav') else 0) for i in OLD}
    a, b, ca, cb = [], [], 0, 0
    for i in OLD:
        if ca <= cb:
            a.append(i); ca += cost[i]
        else:
            b.append(i); cb += cost[i]
    return a, b, ca, cb


if __name__ == '__main__':
    k = int(sys.argv[1])
    a, b, ca, cb = batches()
    live = {1: a, 2: a + b, 3: a + b + NEW}[k]
    add = {1: a, 2: b, 3: NEW}[k]
    missing = [i for i in live if i not in VF]
    assert not missing, missing
    json.dump(sorted(live), open(f'{W}/out/flac_live.json', 'w'))
    print(subprocess.run(['python3', f'{W}/build_site.py'], capture_output=True, text=True).stdout.strip())
    files = {}
    for i in add:
        files[f'audio/voxf/{i}.ogg'] = f'audio/voxf/{i}.ogg'
        if k in (1, 2):
            files[f'audio/vox/{i}.wav'] = None
            if os.path.exists(f'{W}/site/audio/vox_dry/{i}.wav'):
                files[f'audio/vox_dry/{i}.wav'] = None
    json.dump(files, open(f'{W}/out/publish_batch{k}.json', 'w'), indent=0)
    nadd = sum(1 for v in files.values() if v)
    mb = sum(os.path.getsize(f'{W}/site/{p}') for p, v in files.items() if v) / 1e6
    print(f'batch {k}: {len(live)} vocals live as FLAC; publish adds {nadd} files ({mb:.1f} MB), removes {len(files) - nadd}; '
          f'split {len(a)}/{len(b)} old vocals ({ca}/{cb} file operations); new vocals {len(NEW)}')
