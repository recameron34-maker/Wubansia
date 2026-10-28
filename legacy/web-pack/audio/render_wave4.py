"""Wave 4 for the web pack: the Disclosure-style garage-house set (drum hits, four 128 BPM grooves, the chord-pluck
and glide-bass multisamples), packed into two sprites with a manifest in the wave-3 format."""
import os as _os  # noqa: E402
PACK_ROOT = _os.environ.get('PACK_ROOT') or _os.path.abspath(_os.path.join(_os.path.dirname(_os.path.abspath(__file__)), '..'))  # the legacy web-pack folder
import json, os, re, struct, sys
import numpy as np
import soundfile as sf

sys.path.insert(0, PACK_ROOT + '/audio')
from dsp import peaks  # noqa: E402

WORK = PACK_ROOT
LIB = f'{WORK}/inst/lib'
SPR = f'{WORK}/site/audio/s4'
STAGE = f'{WORK}/out/wave4'
GH = json.load(open(f'{WORK}/inst/gh_meta.json'))
SR = 44100
BASE = 'UK Garage/Garage House (Disclosure-style)'
NAMES = ['C', 'C#', 'D', 'Eb', 'E', 'F', 'F#', 'G', 'Ab', 'A', 'Bb', 'B']


def nn(m):
    return NAMES[m % 12] + str(m // 12 - 1)


def wav_bytes(pcm, ch):
    data = pcm.tobytes()
    return (b'RIFF' + struct.pack('<I', 36 + len(data)) + b'WAVE' + b'fmt ' + struct.pack('<IHHIIHH', 16, 1, ch, SR, SR * 2 * ch, 2 * ch, 16)
            + b'data' + struct.pack('<I', len(data)) + data)


def to_int16(x):
    return np.round(np.clip(x, -1, 1) * 32767).astype('<i2')


HITS = [('kick_gh', 'Kick Garage House Round', 'Kick-GarageHouse-Round_F', 'F', 'round, tuned deep-house kick with a short sub'),
        ('clap_gh', 'Clap Garage House Plate', 'Clap-GarageHouse-Plate', None, 'four-layer clap into a short plate'),
        ('snare_gh', 'Snare Garage House Crisp', 'Snare-GarageHouse-Crisp', None, 'tight, bright snare'),
        ('rim_gh', 'Rim Garage House', 'Rim-GarageHouse', None, 'wooden rimshot'),
        ('chat_gh', 'Closed Hat Garage House', 'ClosedHat-GarageHouse', None, 'crisp closed hat'),
        ('ohat_gh', 'Open Hat Garage House', 'OpenHat-GarageHouse', None, 'open hat, 0.6 s'),
        ('shaker_gh', 'Shaker Garage House', 'Shaker-GarageHouse', None, 'shaker for 16th grooves'),
        ('snap_gh', 'Snap Garage House', 'Snap-GarageHouse', None, 'finger snap with a little plate'),
        ('tamb_gh', 'Tambourine Garage House', 'Tambourine-GarageHouse', None, 'tambourine hit')]
LOOP_NOTES = {'gh_loop1': 'kick on 1 and the "and" of 3, clap backbeat, swung hats and rims',
              'gh_loop2': 'four-to-the-floor with offbeat open hats and a swung shaker',
              'gh_loop3': 'snaps on the backbeat, syncopated rims, 16th shaker',
              'gh_loop4': 'skippy 2-step kick, snare ghosts and flammed hats'}


def main():
    os.makedirs(SPR, exist_ok=True)
    stereo, mono, manifest = [], [], []
    for key, title, fname, k, note in HITS:
        x, _ = sf.read(f"{LIB}/{GH[key]['path']}")
        x = np.stack([x, x], 1) if x.ndim == 1 else x
        stereo.append(dict(id=key, cat='hits', title=title, note=note, key=k, x=x, pack=f'{BASE}/Drum Hits/GH_{fname}.wav'))
    for key in ['gh_loop1', 'gh_loop2', 'gh_loop3', 'gh_loop4']:
        v = GH[key]
        x, _ = sf.read(f"{LIB}/{v['path']}")
        stereo.append(dict(id=key, cat='loops', grp='dl-gh', kind='drumloop', title='Drum Loop · ' + v['title'], bpm=128, bars=2,
                           note=LOOP_NOTES[key] + ', 58% swing', x=x, pack=f"{BASE}/Drum Loops/{os.path.basename(v['path'])}"))
    for prefix, sset, grp, cat, label, note in [('pluck', 'gh_pluck', 'ghkeys', 'keys', 'Chord Pluck', 'detuned-saw pluck, 1.35 s'),
                                                ('gbass', 'gh_bass', 'ghbass', 'bass', 'Glide Bass', 'sine and filtered square, loops cleanly')]:
        for k, v in sorted(GH.items(), key=lambda kv: kv[1].get('root') or 0):
            if not re.match(r'^%s_\d+$' % prefix, k):
                continue
            x, _ = sf.read(f"{LIB}/{v['path']}")
            m = v['root']
            mono.append(dict(id=f'{prefix}4_{nn(m).replace("#", "s")}', cat=cat, grp=grp, set=sset, noteName=nn(m), key=NAMES[m % 12],
                             title=f'{label} {nn(m)}', note=note, x=x, pack=f"{BASE}/{label}/GH_{label.replace(' ', '')}_{nn(m).replace('#', 's')}.wav"))
    GAP = 512
    for name, mem, ch in [('w4_gh_stereo', stereo, 2), ('w4_gh_mono', mono, 1)]:
        chunks, off = [], 0
        for it in mem:
            pcm = to_int16(it['x'])
            chunks += [pcm, np.zeros((GAP,) + pcm.shape[1:], dtype='<i2')]
            e = dict(id=it['id'], cat=it['cat'], grp=it.get('grp'), title=it['title'], bpm=it.get('bpm'), note=it['note'], bars=it.get('bars'),
                     dur=round(len(pcm) / SR, 3), file=f'sprite:audio/s4/{name}.wav?o={off}&n={len(pcm)}&c={ch}', pack=it['pack'],
                     peaks=peaks(it['x'], 72), stereo=ch == 2, genre='ukg', kind=it.get('kind'), wave=4, key=it.get('key'), set=it.get('set'),
                     noteName=it.get('noteName'))
            manifest.append({k: v for k, v in e.items() if v is not None})
            os.makedirs(os.path.dirname(f"{STAGE}/{it['pack']}"), exist_ok=True)
            open(f"{STAGE}/{it['pack']}", 'wb').write(wav_bytes(pcm, ch))
            off += len(pcm) + GAP
        allpcm = np.concatenate(chunks)
        open(f'{SPR}/{name}.wav', 'wb').write(wav_bytes(allpcm, ch))
        print(name, len(mem), 'items', round(os.path.getsize(f'{SPR}/{name}.wav') / 1e6, 2), 'MB')
    json.dump(manifest, open(f'{WORK}/out/manifest_wave4.json', 'w'))
    print(len(manifest), 'manifest items')


if __name__ == '__main__':
    main()
