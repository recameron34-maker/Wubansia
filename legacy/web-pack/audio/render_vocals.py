"""Render the vocal set: clean + explicit spoken hooks, hard-tuned hooks, robot hooks, sung chop loops, vocal stabs."""
import os as _os  # noqa: E402
PACK_ROOT = _os.environ.get('PACK_ROOT') or _os.path.abspath(_os.path.join(_os.path.dirname(_os.path.abspath(__file__)), '..'))  # the legacy web-pack folder
import json, os, sys, time
import numpy as np
from dsp import *
import vocals as V

SITE = PACK_ROOT + '/site/audio'
LEADS = json.load(open(PACK_ROOT + '/out/leads.json'))
OUT = []
CAM_MIN = {0: '5A', 1: '12A', 2: '7A', 3: '2A', 4: '9A', 5: '4A', 6: '11A', 7: '6A', 8: '1A', 9: '8A', 10: '3A', 11: '10A'}
CAM_MAJ = {0: '8B', 1: '3B', 2: '10B', 3: '5B', 4: '12B', 5: '7B', 6: '2B', 7: '9B', 8: '4B', 9: '11B', 10: '6B', 11: '1B'}
NAMES = {0: 'C', 1: 'C#', 2: 'D', 3: 'Eb', 4: 'E', 5: 'F', 6: 'F#', 7: 'G', 8: 'Ab', 9: 'A', 10: 'Bb', 11: 'B'}
PCN = {'C': 0, 'C#': 1, 'Db': 1, 'D': 2, 'Eb': 3, 'D#': 3, 'E': 4, 'F': 5, 'F#': 6, 'G': 7, 'Ab': 8, 'G#': 8, 'A': 9, 'Bb': 10, 'B': 11}


def key_of(root, mode):
    return NAMES[root] + (' major' if mode == 'major' else ' minor'), (CAM_MAJ if mode == 'major' else CAM_MIN)[root]


def scale(root, mode):
    iv = [0, 2, 4, 5, 7, 9, 11] if mode == 'major' else [0, 2, 3, 5, 7, 8, 10]
    return [(root + i) % 12 for i in iv]


def tail_trim(x, db=-58):
    m = np.abs(x).max(axis=1) if x.ndim == 2 else np.abs(x)
    thr = m.max() * 10 ** (db / 20)
    idx = np.where(m > thr)[0]
    end = min(len(x), idx[-1] + int(0.03 * SR))
    y = x[:end].copy()
    f = min(len(y) // 4, int(0.04 * SR))
    y[-f:] *= (np.linspace(1, 0, f)[:, None] if y.ndim == 2 else np.linspace(1, 0, f))
    return y


def slug(s):
    return ''.join(ch for ch in s.title() if ch.isalnum())


def save(group, sid, text, wet, dry, bpm, keylabel=None, cam=None, voice=None, style=None, explicit=False, note=None, pack_dir='Vocals'):
    os.makedirs(f'{SITE}/vox', exist_ok=True)
    os.makedirs(f'{SITE}/vox_dry', exist_ok=True)
    wet = write(f'{SITE}/vox/{sid}.wav', tail_trim(wet), -1.0)
    dry = write(f'{SITE}/vox_dry/{sid}.wav', tail_trim(dry, -65), -1.0)
    ksuf = ('_' + keylabel.replace(' minor', 'min').replace(' major', 'maj') + ('-' + cam if cam else '')) if keylabel else ''
    base = f"{'VOX-E' if explicit else 'VOX'}_{sid.upper()}_{slug(text)[:34]}{ksuf}_{bpm}bpm"
    OUT.append({'id': sid, 'cat': 'vox', 'group': group, 'title': text, 'voice': V.VOICE_DESC.get(voice, voice), 'style': style,
                'explicit': explicit, 'bpm': bpm, 'key': keylabel, 'camelot': cam, 'note': note,
                'dur': round(len(wet) / SR, 3), 'file': f'audio/vox/{sid}.wav', 'dry': f'audio/vox_dry/{sid}.wav',
                'pack': f'{pack_dir}/{base}.wav', 'packDry': f'{pack_dir}/Dry/{base}_DRY.wav', 'peaks': peaks(wet, 72), 'stereo': True})
    print(f'  {sid:6s} {len(wet)/SR:5.2f}s {bpm} {keylabel or "spoken":10s} {text}', flush=True)


# --------------------------------------------------------------------------
SPOKEN = [
    # id, text, voice, style, bpm, pitch(semitones for dark), speed, explicit, stutter
    ('c01', 'Rewind!', 'bm_george', 'mc', 134, 0, 1.0, False, False),
    ('c02', 'Rewind!', 'bm_george', 'mc', 134, 0, 1.0, False, True),
    ('c03', 'Big up all the ravers.', 'bm_lewis', 'mc', 132, 0, 1.05, False, False),
    ('c04', 'Bring it back, selecta.', 'bm_daniel', 'radio', 136, 0, 1.0, False, False),
    ('c05', 'Back to the garage.', 'bm_george', 'mc', 133, 0, 1.0, False, False),
    ('c06', 'Lock it down.', 'bm_fable', 'dark', 130, -2, 0.95, False, False),
    ('c07', 'Pull up, pull up!', 'bm_george', 'mc', 138, 0, 1.05, False, False),
    ('c08', 'Strictly underground.', 'bm_daniel', 'dark', 131, -3, 0.95, False, False),
    ('c09', 'Feel the bass.', 'af_nicole', 'whisper', 128, 0, 0.9, False, False),
    ('c10', 'Let the groove take control.', 'am_michael', 'dark', 129, -3, 0.95, False, False),
    ('c11', 'Turn it up.', 'am_fenrir', 'dark', 128, -2, 1.0, False, False),
    ('c12', 'Lose yourself.', 'af_bella', 'diva', 130, 0, 0.95, False, False),
    ('c13', 'Move your body.', 'am_onyx', 'dark', 128, -4, 0.95, False, False),
    ('c14', "We don't stop.", 'af_nova', 'diva', 132, 0, 1.0, False, False),
    ('c15', 'Deeper.', 'af_nicole', 'whisper', 129, 0, 0.85, False, False),
    ('c16', 'Hands in the air.', 'am_adam', 'mc', 130, 0, 1.05, False, False),
    ('c17', 'London, make some noise!', 'bm_george', 'mc', 134, 0, 1.05, False, False),
    ('c18', 'Proper vibes.', 'bf_emma', 'diva', 133, 0, 1.0, False, False),
    ('c19', 'Body on the dancefloor.', 'bf_isabella', 'diva', 136, 0, 1.0, False, False),
    ('e01', 'Move your fucking body.', 'am_onyx', 'dark', 128, -4, 0.95, True, False),
    ('e02', 'Shake that ass.', 'af_bella', 'diva', 130, 0, 0.95, True, False),
    ('e03', 'Fuck it up!', 'bm_lewis', 'mc', 134, 0, 1.0, True, False),
    ('e04', 'This shit is filthy.', 'am_michael', 'dark', 129, -3, 0.95, True, False),
    ('e05', 'Make it nasty.', 'af_nicole', 'whisper', 128, 0, 0.9, True, False),
    ('e06', 'Turn this shit up.', 'bm_daniel', 'radio', 136, 0, 1.0, True, False),
    ('e07', 'Too fucking loud? Good.', 'bm_george', 'mc', 132, 0, 1.0, True, False),
    ('e08', "This bass is so heavy, it's fucking rude.", 'am_adam', 'dark', 130, -2, 1.0, True, False),
    ('e09', 'Lose your fucking mind.', 'am_echo', 'dark', 131, -3, 0.95, True, False),
    ('e10', "Damn, that's hot.", 'af_sarah', 'diva', 133, 0, 0.95, True, False),
    ('e11', 'I like it when you move like that.', 'af_nicole', 'whisper', 128, 0, 0.9, True, False),
    ('e12', 'Drop it low, make it dirty.', 'af_jessica', 'diva', 132, 0, 1.0, True, False),
    ('e13', "Let's get fucked up.", 'am_fenrir', 'dark', 130, -3, 1.0, True, True),
    ('e14', 'Fuck the curfew.', 'bm_lewis', 'mc', 138, 0, 1.0, True, False),
    ('e15', 'Rave till sunrise, motherfuckers.', 'bm_fable', 'mc', 134, 0, 1.0, True, False),
    ('e16', 'Sweaty, dirty, nasty.', 'af_kore', 'whisper', 129, 0, 0.9, True, False),
    ('e17', 'Who the fuck is on the decks?', 'bm_george', 'radio', 136, 0, 1.0, True, False),
    ('e18', 'Bad boy shit.', 'bm_daniel', 'dark', 138, -2, 1.0, True, False),
]

TUNED = [
    # id, text, voice, root, mode, bpm, mode('tune'|'robot'), explicit, kit
    ('t01', 'Lose yourself.', 'af_bella', 5, 'minor', 134, 'tune', False, 'UKG01'),
    ('t02', 'Feel the bass.', 'af_heart', 9, 'minor', 128, 'tune', False, 'MTH01'),
    ('t03', 'Bring it back.', 'bf_emma', 2, 'minor', 132, 'tune', False, 'UKG02'),
    ('t04', 'All night long.', 'bf_lily', 3, 'major', 133, 'tune', False, 'UKG05'),
    ('t05', "Don't stop.", 'af_nova', 11, 'minor', 132, 'tune', False, 'MTH06'),
    ('t06', 'Turn it up.', 'am_michael', 2, 'minor', 131, 'tune', False, 'MTH05'),
    ('t07', 'Tonight.', 'bf_isabella', 9, 'minor', 136, 'tune', False, 'UKG04'),
    ('t08', 'Lock it down.', 'bm_george', 7, 'minor', 130, 'robot', False, 'UKG03'),
    ('t09', 'Move your fucking body.', 'am_onyx', 7, 'minor', 129, 'tune', True, 'MTH02'),
    ('t10', 'Shake that ass.', 'af_bella', 0, 'minor', 128, 'tune', True, 'MTH04'),
    ('t11', 'Make it nasty.', 'af_nicole', 4, 'minor', 138, 'tune', True, 'UKG06'),
    ('t12', 'This shit is filthy.', 'am_michael', 5, 'minor', 130, 'robot', True, 'MTH03'),
]

CHOPS = [
    # id, kit, voice, syllables, formant
    ('ch01', 'UKG01', 'af_heart', ['Ooh.', 'Yeah.', 'Oh.', 'Ah.'], 1.12),
    ('ch02', 'UKG02', 'bf_emma', ['Ah.', 'Ooh.', 'Yeah.'], 1.1),
    ('ch03', 'UKG03', 'af_bella', ['Hey!', 'Oh.', 'Yeah.', 'Ooh.'], 1.1),
    ('ch04', 'UKG04', 'bf_isabella', ['Yeah.', 'Oh.', 'Ooh.', 'Ah.'], 1.14),
    ('ch05', 'UKG05', 'af_heart', ['Ooh.', 'Ah.', 'Oh.'], 1.08),
    ('ch06', 'UKG06', 'bf_lily', ['Ooh.', 'Ah.'], 1.04),
    ('ch07', 'MTH02', 'af_nicole', ['Oh.', 'Ah.', 'Hey!'], 1.06),
    ('ch08', 'MTH05', 'af_sky', ['Ah.', 'Ooh.', 'Oh.'], 1.06),
]

STABS = [
    # id, word, voice, note, dur, formant, bpm
    ('s01', 'Hey!', 'af_bella', 'F4', 0.42, 1.06, 134),
    ('s02', 'Yeah!', 'bm_lewis', 'Ab3', 0.45, 1.0, 132),
    ('s03', 'Oh.', 'af_heart', 'C5', 0.5, 1.1, 130),
    ('s04', 'Uh!', 'am_onyx', 'G2', 0.3, 0.92, 128),
    ('s05', 'Woo!', 'af_nova', 'A4', 0.5, 1.08, 136),
    ('s06', 'Ooh.', 'bf_emma', 'D5', 0.6, 1.1, 132),
    ('s07', 'Ah.', 'af_sky', 'E4', 0.55, 1.05, 138),
    ('s08', 'Huh!', 'am_michael', 'B2', 0.3, 0.95, 132),
    ('s09', 'Yeah.', 'af_bella', 'Eb4', 0.5, 1.06, 133),
    ('s10', 'Come on!', 'bm_george', 'C3', 0.55, 1.0, 128),
]


def main():
    t0 = time.time()
    only = sys.argv[1:] or None
    # ---- spoken ----
    print('spoken')
    for sid, text, voice, style, bpm, pitch, speed, explicit, stut in SPOKEN:
        if only and sid not in only and 'spoken' not in only:
            continue
        x = V.up(V.tts(text, voice, speed))
        x = bfilt(x, 'highpass', 70, 2)
        dry = board(x, [pb.PitchShift(semitones=pitch)]) if (style == 'dark' and pitch) else x
        src = x
        if stut:
            src = V.stutter(x, bpm, 3, 0.075)
            dry = V.stutter(dry, bpm, 3, 0.075)
        wet = V.style_chain(src, style, bpm, pitch)
        title = text if not stut else ('R-r-r-rewind!' if sid == 'c02' else "L-l-l-let's get fucked up.")
        note = {'mc': 'MC shout, delay throw', 'radio': 'pirate-radio filter', 'dark': f'pitched {pitch} st, grit + delay', 'whisper': 'whisper, wide reverb', 'diva': 'clean hook, plate + echo'}[style]
        if stut:
            note += ', 1/16 stutter'
        save('explicit' if explicit else 'clean', sid, title, wet, dry, bpm, None, None, voice, style, explicit, note,
             'Vocals/Explicit' if explicit else 'Vocals/Clean')
    # ---- tuned / robot ----
    print('tuned')
    for sid, text, voice, root, mode, bpm, how, explicit, kit in TUNED:
        if only and sid not in only and 'tuned' not in only:
            continue
        x = V.tts(text, voice, 0.95)
        pcs = scale(root, mode)
        f0, sp, ap = V.analyze(x)
        v = f0 > 0
        med = np.median(V.hz2m(f0[v]))
        anchors = [n for n in range(int(med) - 7, int(med) + 8) if n % 12 in (root, (root + 7) % 12)]
        center = min(anchors, key=lambda n: abs(n - med))
        if how == 'tune':
            y, _ = V.hardtune(x, pcs, target_center=center, formant=1.0)
            note = 'hard-tuned to key'
        else:
            tgt = center if center % 12 == root else center - 7 if (center - 7) % 12 == root else center
            y = V.robot(x, tgt, 0.97)
            note = f'robot monotone on {NAMES[root]}'
        y44 = bfilt(V.up(y), 'highpass', 70, 2)
        wet = V.style_chain(y44, 'robot' if how == 'robot' else 'tuned', bpm)
        kl, cam = key_of(root, mode)
        save('tuned', sid, text, wet, y44, bpm, kl, cam, voice, how, explicit, note + f' · fits {kit}',
             'Vocals/Tuned & Robot')
    # ---- chop loops ----
    print('chops')
    for sid, kit, voice, syls, formant in CHOPS:
        if only and sid not in only and 'chops' not in only:
            continue
        L = LEADS[kit]
        bpm = L['bpm']
        notes = [e['n'] for e in L['lead']]
        med = np.median(notes)
        octs = -12 if med > 74 else (12 if med < 57 else 0)
        sec = lambda ticks: ticks / 480 * 60 / bpm
        total = int(round(sec(2 * 1920) * SR))
        buf = np.zeros(total + SR * 2)
        for i, e in enumerate(L['lead']):
            d = sec(e['d'])
            word = syls[i % len(syls)]
            c = V.up(V.chop(word, voice, e['n'] + octs, max(0.09, d * 0.96), formant))
            place(buf, c, int(sec(e['t']) * SR), 0.9)
        dry = buf[:total].copy()
        wet = V.style_chain(bfilt(buf, 'highpass', 90, 2), 'chop', bpm)
        # loop-friendly: wrap the reverb tail back onto the start
        loop = wet[:total].copy()
        tail = wet[total:total + total]
        loop[:len(tail)] += tail[:len(loop)]
        kl, cam = key_of(L['root'], L['mode'])
        save('chops', sid, f"Vox Chop · {L['name']}", loop, dry, bpm, kl, cam, voice, 'chop', False,
             f"2-bar loop on the {L['name']} melody", 'Vocals/Chops')
    # ---- stabs ----
    print('stabs')
    for sid, word, voice, note, dur, formant, bpm in STABS:
        if only and sid not in only and 'stabs' not in only:
            continue
        m = 12 * (int(note[-1]) + 1) + PCN[note[:-1]]
        c = V.up(V.chop(word, voice, m, dur, formant, vib=6, attack_ms=40))
        dry = bfilt(c, 'highpass', 70, 2)
        wet = V.style_chain(dry, 'chop', bpm)
        wet = V.throw(wet.mean(axis=1), bpm, 0.5, 0.35, 0.28, 4200)
        wet = reverb(wet, 0.45, 0.14, 0.5, 1.0, 1.0)
        save('stabs', sid, word.strip('.!'), wet, dry, bpm, note[:-1], None, voice, 'stab', False, f'sung on {note}',
             'Vocals/Stabs')
    with open(PACK_ROOT + '/out/manifest_vox.json' if not only else PACK_ROOT + '/out/manifest_vox_partial.json', 'w') as f:
        json.dump(OUT, f)
    print('done', len(OUT), 'vocals in', round(time.time() - t0, 1), 's')


if __name__ == '__main__':
    main()
