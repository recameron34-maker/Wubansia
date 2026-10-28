import os as _os  # noqa: E402
PACK_ROOT = _os.environ.get('PACK_ROOT') or _os.path.abspath(_os.path.dirname(_os.path.abspath(__file__)))  # the legacy web-pack folder
import json, os, re
W = PACK_ROOT
fx = json.load(open(f'{W}/out/manifest_fx.json'))
vox = json.load(open(f'{W}/out/manifest_vox.json'))
more = json.load(open(f'{W}/out/manifest_more.json'))
vox2 = json.load(open(f'{W}/out/manifest_vox2.json')) if os.path.exists(f'{W}/out/manifest_vox2.json') else []
brat = json.load(open(f'{W}/out/manifest_brat.json')) if os.path.exists(f'{W}/out/manifest_brat.json') else []
wave3 = json.load(open(f'{W}/out/manifest_wave3.json')) if os.path.exists(f'{W}/out/manifest_wave3.json') else []
wave4 = json.load(open(f'{W}/out/manifest_wave4.json')) if os.path.exists(f'{W}/out/manifest_wave4.json') else []

# genre tags for the first batch
UKG_HITS = {'kick_ukg', 'kick_ukg_boom', 'clap_ukg', 'snare_ukg', 'rim_ukg', 'chat_ukg', 'ohat_ukg'}
MTH_HITS = {'kick_tech', 'kick_tech_round', 'clap_tech', 'snare_tech', 'rim_tech', 'chat_tech', 'ohat_tech'}
OLD_VOX = {
    **{k: 'ukg' for k in ['c01', 'c02', 'c03', 'c04', 'c05', 'c06', 'c07', 'c08', 'c17', 'c18', 'c19', 'e03', 'e06', 'e07', 'e14', 'e15', 'e17', 'e18',
                          't01', 't03', 't04', 't07', 't08', 't11', 'ch01', 'ch02', 'ch03', 'ch04', 'ch05', 'ch06']},
    **{k: 'mth' for k in ['c09', 'c10', 'c11', 'c12', 'c13', 'c14', 'c15', 'c16', 'e01', 'e02', 'e04', 'e05', 'e08', 'e09', 'e10', 'e11', 'e12', 'e13', 'e16',
                          't02', 't05', 't06', 't09', 't10', 't12', 'ch07', 'ch08']},
}
for it in fx:
    if it['cat'] == 'hits':
        it['genre'] = 'ukg' if it['id'] in UKG_HITS else 'mth' if it['id'] in MTH_HITS else 'all'
    else:
        it['genre'] = 'ukg' if it['id'].startswith('ukg') else 'mth'
for it in vox:
    it['genre'] = OLD_VOX.get(it['id'], 'all')
for it in more:
    it.setdefault('genre', 'all')

# chop loops are exactly two bars: mark them so the page loops them and the Fits filter checks their tempo
for it in vox:
    if it.get('group') == 'chops' and it.get('bpm') and abs(it['dur'] - 2 * 240 / it['bpm']) < 0.01:
        it['bars'] = 2
items = fx + more + vox + vox2 + brat + wave3 + wave4

# wave 6: vocals move to one FLAC per vocal (wet L/R + dry). Only ids in the live set switch over (so a site version can be
# published part-converted and stay consistent); re-sung items (manifest_v6) and new vocals go live with their FLAC.
v6 = json.load(open(f'{W}/out/manifest_v6.json')) if os.path.exists(f'{W}/out/manifest_v6.json') else []
vf = json.load(open(f'{W}/out/vox_flac.json')) if os.path.exists(f'{W}/out/vox_flac.json') else {}
live = set(json.load(open(f'{W}/out/flac_live.json'))) if os.path.exists(f'{W}/out/flac_live.json') else set(vf)
assert live <= set(vf), sorted(live - set(vf))
V6 = {e['id']: e for e in v6}
def flacify(it):
    f = vf[it['id']]
    it['file'] = f['file']
    if f.get('dry'):
        it['dry'] = f['dry']
    else:
        it.pop('dry', None)
    return it
known = set()
for i, it in enumerate(items):
    known.add(it['id'])
    if it['id'] in live:
        new = dict(V6[it['id']]) if it['id'] in V6 else it
        for k in ('genre', 'bars'):
            if k in it and k not in new:
                new[k] = it[k]
        items[i] = flacify(new)
items += [flacify(dict(e)) for e in v6 if e['id'] not in known and e['id'] in live]
known |= {e['id'] for e in v6}
# Halloween wave (wave 7): vocals and synthesised FX, all packed as FLAC like the vocals (FX without a dry channel); wave 8 vocal shots
for f in ('manifest_halloween.json', 'manifest_halloween_fx.json', 'manifest_w8_vox.json'):
    if os.path.exists(f'{W}/out/{f}'):
        items += [flacify(dict(e)) for e in json.load(open(f'{W}/out/{f}')) if e['id'] in live and e['id'] not in known]
# wave 8 (groove house + tech house): hits, instruments, loops and FX, already in FLAC sprites under audio/s8/
if os.path.exists(f'{W}/out/manifest_wave8.json'):
    items += [e for e in json.load(open(f'{W}/out/manifest_wave8.json')) if e['id'] not in known]
# the wave-3/4 WAV sprites are stored as Ogg FLAC sprites with the same frame layout (pack_timesprite.py sprites): point the slices there
def unsprite(url):
    m = re.match(r'^sprite:([^?]+)\.wav\?o=(\d+)&n=(\d+)&c=(\d)$', url or '')
    if not m or not os.path.exists(f'{W}/site/{m.group(1)}.ogg'):
        return url
    return f"flac:{m.group(1)}.ogg?ch={'0,1' if m.group(4) == '2' else '0'}&n={m.group(3)}&o={m.group(2)}"
# the plain WAVs (bass, keys, fx, fills, hits, inst, loops) live in Ogg FLAC time sprites too (pack_timesprite.py plain)
PLAIN = json.load(open(f'{W}/out/plain_flac.json')) if os.path.exists(f'{W}/out/plain_flac.json') else {}
for it in items:
    for k in ('file', 'dry'):
        if it.get(k):
            it[k] = PLAIN.get(it[k], unsprite(it[k]))
            assert it[k].startswith('flac:') or os.path.exists(f'{W}/site/' + it[k]), (it['id'], it[k])
seen = set()
for it in items:
    assert it['id'] not in seen, it['id']
    seen.add(it['id'])
    for k in list(it):
        if it[k] is None or k in ('asr', 'engine', 'new', 'kit'):
            del it[k]
total = 0
for it in items:
    if it.get('pack'):
        for key in ('file', 'dry'):
            url = it.get(key)
            if not url:
                continue
            assert not url.startswith('sprite:'), (it['id'], url)   # the page only reads FLAC sprites now
            f = re.match(r'^flac:([^?]+)\?ch=([\d,]+)&n=(\d+)(?:&o=\d+)?$', url)
            if f:   # channels (and a time slice) of a FLAC, saved as a 16-bit WAV
                assert os.path.exists(f'{W}/site/' + f.group(1)), f.group(1)
                total += 44 + int(f.group(3)) * 2 * len(f.group(2).split(','))
            else:
                total += os.path.getsize(f'{W}/site/' + url)
man = json.dumps({'items': items, 'bytes': total}, separators=(',', ':'))
import sys
sys.path.insert(0, f'{W}/ui')
import themes
src = open(f'{W}/ui/app.src.html').read().replace('/*__THEMES__*/', themes.emit_css())
core = open(f'{W}/core.js').read()
app = open(f'{W}/flac.js').read() + '\n' + open(f'{W}/app.js').read().replace('/*__LAB__*/', open(f'{W}/ui/lab.js').read())
assert '</script' not in core and '</script' not in app
html = src.replace('/*__CORE__*/', core).replace('/*__MANIFEST__*/', man).replace('/*__APP__*/', app)
open(f'{W}/site/index.html', 'w').write(html)
print('index.html', len(html) // 1024, 'KB;', len(items), 'audio items; pack audio', round(total / 1e6, 1), 'MB')
