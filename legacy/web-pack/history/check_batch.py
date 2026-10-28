"""Check that a planned publish leaves the artifact consistent: every file the new index.html references exists afterwards."""
import json, os, re, sys
W = '/home/claude/work'
k = int(sys.argv[1])
prev = set(json.load(open(f'{W}/out/artifact_files.json')))       # what the artifact holds now
plan = json.load(open(f'{W}/out/publish_batch{k}.json'))
after = (prev - {p for p, v in plan.items() if v is None}) | {p for p, v in plan.items() if v}
h = open(f'{W}/site/index.html').read()
man = json.loads('{' + re.search(r'"items":\[.*?\],"bytes":\d+\}', h, re.S).group(0))
refs = set()
for it in man['items']:
    for key in ('file', 'dry'):
        u = it.get(key)
        if not u:
            continue
        m = re.match(r'^(?:sprite|flac):([^?]+)\?', u)
        refs.add(m.group(1) if m else u)
missing = sorted(refs - after)
size = sum(os.path.getsize(f'{W}/site/{p}') for p in after if p != 'index.html') + os.path.getsize(f'{W}/site/index.html')
print(f'batch {k}: artifact files after publish {len(after)} (+index.html), {size / 1e6:.1f} MB; referenced {len(refs)}; missing {len(missing)} {missing[:5]}')
unused = sorted(p for p in after - refs if p.startswith('audio/'))
print('  files present but unreferenced:', len(unused), unused[:5])
if not missing:
    json.dump(sorted(after), open(f'{W}/out/artifact_files_after{k}.json', 'w'))
