"""Wave 8 for the web pack: groove house (Chris Stussy-style) and tech house (Chris Lake & Riordan-style). Drum hits,
multisampled instruments, loops and FX from inst/ (render_w8*.py), packed into Ogg FLAC time sprites under site/audio/s8/,
with a manifest in the pack's format (out/manifest_wave8.json) and WAV copies of every file under out/wave8/ for tests."""
import os as _os  # noqa: E402
PACK_ROOT = _os.environ.get('PACK_ROOT') or _os.path.abspath(_os.path.join(_os.path.dirname(_os.path.abspath(__file__)), '..'))  # the legacy web-pack folder
import json, os, re, struct, subprocess, sys
import numpy as np
import soundfile as sf

sys.path.insert(0, PACK_ROOT + '/audio')
from dsp import peaks  # noqa: E402

W = PACK_ROOT
LIB = f'{W}/inst/lib'
SPR = f'{W}/site/audio/s8'
STAGE = f'{W}/out/wave8'
NATIVE = f'{W}/out/vox_flac_native'
SR = 44100
META = json.load(open(f'{W}/inst/w8_meta.json'))
LOOPS = json.load(open(f'{W}/inst/w8_loops.json'))
FXM = json.load(open(f'{W}/inst/w8_fx.json'))
BASE = {'gv': 'Groove House (Chris Stussy-style)', 'th': 'Tech House (Chris Lake & Riordan-style)'}
GENRE = {'gv': 'ch', 'th': 'mth'}
NAMES = ['C', 'C#', 'D', 'Eb', 'E', 'F', 'F#', 'G', 'Ab', 'A', 'Bb', 'B']
INST = {   # meta prefix: (family, cat, set id, label, folder, note)
    'gvfm': ('gv', 'bass', 'gv_fm', 'FM Rubber Bass', 'FM Rubber Bass', 'two-operator FM bass: a fast-falling index for the pluck, a 25-cent drop at the start, warm drive'),
    'gvglow': ('gv', 'keys', 'gv_glow', 'Glow Saw', 'Glow Saw', 'two saws three cents apart, a breath of noise, a closing ladder filter, a long release and a slow stereo phaser'),
    'gvfifth': ('gv', 'keys', 'gv_fifth', 'Fifths Lead', 'Fifths Lead', 'Juno-style lead: every key plays the note and its fifth, PWM, slow detune drift, BBD chorus'),
    'thbounce': ('th', 'bass', 'th_bounce', 'Bounce Bass', 'Bounce Bass', 'bright, driven saw + square + sub that loops cleanly; the player adds a resonant filter envelope that opens on hard-hit notes'),
    'thfifth': ('th', 'bass', 'th_fifth', 'Fifth Bass', 'Fifth Bass', 'Moog-style: saws on the note and its fifth over a square an octave down, a resonant ladder pluck, a touch of bit-crush'),
    'thrave': ('th', 'keys', 'th_rave', 'Rave Stab', 'Rave Stab', 'chord memory: every key plays a minor chord with the octave, detuned saws, a snapping filter and a small room'),
    'threese': ('th', 'bass', 'th_reese', 'Crushed Reese', 'Crushed Reese', 'two saws a few cents apart, a sine a fifth up, driven, band-pass overdrive and 6-bit grit; loops cleanly'),
}
HIT_NOTE = {k: v.get('note') for k, v in META.items()}


def nn(m):
    return NAMES[m % 12] + str(m // 12 - 1)


def to_int16(x):
    return np.round(np.clip(x, -1, 1) * 32767).astype('<i2')


def wav_bytes(pcm, ch):
    data = pcm.tobytes()
    return (b'RIFF' + struct.pack('<I', 36 + len(data)) + b'WAVE' + b'fmt ' + struct.pack('<IHHIIHH', 16, 1, ch, SR, SR * 2 * ch, 2 * ch, 16)
            + b'data' + struct.pack('<I', len(data)) + data)


def read(path, ch):
    x, sr = sf.read(f'{LIB}/{path}', always_2d=True)
    assert sr == SR, path
    if ch == 2 and x.shape[1] == 1:
        x = np.repeat(x, 2, 1)
    if ch == 1 and x.shape[1] == 2:
        x = x.mean(1, keepdims=True)
    return x


def main():
    groups = {}   # sprite name -> (channels, [items])

    def add(sprite, ch, it):
        groups.setdefault(sprite, (ch, []))[1].append(it)

    # drum hits, stored stereo (like the garage-house kit)
    for k, v in META.items():
        if v.get('inst'):
            continue
        fam = 'gv' if k.endswith('_gv') else 'th'
        add(f'{fam}_hits', 2, dict(id=k, cat='hits', title=v['title'], note=v.get('note'), key=v.get('key'), fam=fam, x=read(v['path'], 2),
                                  pack=f"{BASE[fam]}/Drum Hits/{os.path.basename(v['path'])}"))
    # multisampled instruments, one sprite per instrument
    for k, v in sorted(META.items(), key=lambda kv: (kv[1].get('inst') or '', kv[1].get('root') or 0)):
        if not v.get('inst'):
            continue
        fam, cat, sset, label, folder, note = INST[v['inst']]
        m = v['root']
        ch = v['ch']
        add(sset, ch, dict(id=f"{v['inst']}_{nn(m).replace('#', 's')}", cat=cat, grp=v['inst'], set=sset, noteName=nn(m), key=NAMES[m % 12], fam=fam,
                           title=f'{label} {nn(m)}', note=note, x=read(v['path'], ch), pack=f"{BASE[fam]}/{folder}/{os.path.basename(v['path'])}"))
    # loops
    for k, v in LOOPS.items():
        fam = v['fam']
        ch = v['ch']
        kind = v.get('kind')
        if kind in ('drumloop', 'toploop', 'break'):
            cat, grp = 'loops', {'drumloop': f'dl-{fam}', 'toploop': 'tops', 'break': 'breaks'}[kind]
            sprite = f'{fam}_drumloops'
        elif kind == 'fill':
            cat, grp, sprite = 'fills', f'{fam}-fills', f'{fam}_drumloops'
        else:
            cat = 'bass' if v['folder'] == 'Bass Loops' else 'keys'
            grp = f'{fam}bassloops' if cat == 'bass' else f'{fam}keyloops'
            sprite = f'{fam}_bassloops' if cat == 'bass' else f'{fam}_keyloops'
        ch = 2 if sprite.endswith(('drumloops', 'keyloops')) else 1
        it = dict(id=k, cat=cat, grp=grp, kind=kind, title=v['title'], note=v['note'], bpm=v['bpm'], bars=v['bars'], key=v.get('key'), camelot=v.get('camelot'),
                  fam=fam, kit=v.get('kit'), x=read(v['path'], ch), pack=f"{BASE[fam]}/{v['folder']}/{os.path.basename(v['path'])}")
        add(sprite, ch, it)
    # FX
    for k, v in FXM.items():
        fam = v['fam']
        add('w8_fx', 2, dict(id=k, cat='fx', grp=f'{fam}-fx', title=v['title'], note=v['note'], bpm=v.get('bpm'), bars=v.get('bars'), fam=fam,
                             x=read(v['path'], 2), pack=f"{BASE[fam]}/FX/{os.path.basename(v['path'])}"))

    os.makedirs(SPR, exist_ok=True)
    os.makedirs(NATIVE, exist_ok=True)
    manifest = []
    for name, (ch, mem) in groups.items():
        parts, off = [], 0
        for it in mem:
            pcm = to_int16(it['x'])
            assert pcm.shape[1] == ch, (it['id'], pcm.shape, ch)
            parts.append((it, pcm, off))
            off += len(pcm)
        allpcm = np.concatenate([p for _, p, _ in parts])
        nat, out = f'{NATIVE}/s8_{name}.flac', f'{SPR}/{name}.ogg'
        sf.write(nat, allpcm, SR, subtype='PCM_16', format='FLAC', compression_level=1.0)
        back = sf.read(nat, dtype='int16', always_2d=True)[0]
        assert back.shape == allpcm.shape and np.array_equal(back, allpcm), name
        subprocess.run(['ffmpeg', '-hide_banner', '-loglevel', 'error', '-y', '-i', nat, '-c:a', 'copy', '-map_metadata', '-1', out], check=True)
        for it, pcm, o in parts:
            e = dict(id=it['id'], cat=it['cat'], grp=it.get('grp'), title=it['title'], bpm=it.get('bpm'), note=it.get('note'), bars=it.get('bars'),
                     dur=round(len(pcm) / SR, 3), file=f"flac:audio/s8/{name}.ogg?ch={'0,1' if ch == 2 else '0'}&n={len(pcm)}&o={o}", pack=it['pack'],
                     peaks=peaks(it['x'] if ch == 2 else it['x'][:, 0], 72), stereo=ch == 2, genre=GENRE[it['fam']], fam=it['fam'], kind=it.get('kind'), wave=8,
                     key=it.get('key'), camelot=it.get('camelot'), set=it.get('set'), noteName=it.get('noteName'), kit=it.get('kit'))
            manifest.append({k: v for k, v in e.items() if v is not None})
            dst = f"{STAGE}/{it['pack']}"
            os.makedirs(os.path.dirname(dst), exist_ok=True)
            open(dst, 'wb').write(wav_bytes(pcm, ch))
        print(f'{name}.ogg: {len(mem)} items, {ch} ch, {len(allpcm) / SR:.1f} s, {os.path.getsize(out) / 1e6:.2f} MB')
    json.dump(manifest, open(f'{W}/out/manifest_wave8.json', 'w'))
    print(len(manifest), 'manifest items')


if __name__ == '__main__':
    main()
