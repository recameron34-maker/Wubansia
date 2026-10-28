"""Recompute the pack file names of the Halloween vocals from their manifest fields (no re-render)."""
import json
import render_halloween as H
SHORT = {'hv23': 'Double double toil and trouble', 'hv24': 'Double double toil and trouble whispered'}
m = json.load(open(H.MAN))
for e in m:
    if e.get('style') == 'stab':
        e['key'], e['camelot'] = 'A minor', '8A'
for e in m:
    sub = e['hsub']
    folder = H.FOLDER['explicit' if e['explicit'] and sub == 'spoken' else sub]
    keybit = ''
    if e.get('key'):
        keybit = '_' + e['key'].replace(' minor', 'min').replace(' major', 'maj').replace('#', 's') + ('-' + e['camelot'] if e.get('camelot') else '')
    bars = f"-{e['bars']}bars" if e.get('bars') else ''
    tag = 'CameronCastle' if sub in ('castle', 'supper', 'zf', 'names', 'drama') else 'Halloween'
    base = f"{'VOX-E' if e['explicit'] else 'VOX'}_{e['id'].upper()}_{tag}_{H.slug(SHORT.get(e['id'], e['title']))}{bars}{keybit}_{e['bpm']}bpm"
    e['pack'], e['packDry'] = f'{folder}/{base}.wav', f'{folder}/Dry/{H.dry_base(base, e["style"])}_DRY.wav'
    print(e['pack'])
json.dump(m, open(H.MAN, 'w'), indent=0)
