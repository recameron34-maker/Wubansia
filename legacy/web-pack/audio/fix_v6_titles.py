"""Bring manifest_v6 titles (and the pack file names made from them) in line with render_vocals6's tables."""
import json
import render_vocals6 as R

M = R.MAN
man = json.load(open(M))
want = {sid: (kid, ex, title) for sid, kid, voices, ex, harm, title, mel in R.NEW_HOOKS}
n = 0
for e in man:
    if e['id'] in want and e['title'] != want[e['id']][2]:
        kid, ex, title = want[e['id']]
        kit = R.KITS[kid]
        fam = kit.get('fam') or kit['genre']
        e['title'] = title
        e['pack'], e['packDry'] = R.pack_paths(e['id'], fam, title, kit, kit['bpm'], ex, 'Vocals/Sung Hooks')
        n += 1
        print(e['id'], '->', title, '|', e['pack'])
json.dump(man, open(M, 'w'), indent=0)
print(n, 'titles fixed')
