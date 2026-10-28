"""Vocal push across all four genres: sung hooks (lead + 2 doubles + harmony), spoken lines (best of 3 takes), ad-libs."""
import os as _os  # noqa: E402
PACK_ROOT = _os.environ.get('PACK_ROOT') or _os.path.abspath(_os.path.join(_os.path.dirname(_os.path.abspath(__file__)), '..'))  # the legacy web-pack folder
import json, os, sys, time
import numpy as np
from dsp import *
import vocals as V
import vocals2 as V2

SITE = PACK_ROOT + '/site/audio'
K = json.load(open(PACK_ROOT + '/out/kits.json'))
OUT = []
PCN = {'C': 0, 'C#': 1, 'Db': 1, 'D': 2, 'D#': 3, 'Eb': 3, 'E': 4, 'F': 5, 'F#': 6, 'Gb': 6, 'G': 7, 'G#': 8, 'Ab': 8, 'A': 9, 'A#': 10, 'Bb': 10, 'B': 11}
GN = {'ukg': 'UKG', 'mth': 'Minimal', 'sg': 'Speed Garage', 'ch': 'House'}
N = lambda s: 12 * (int(s[-1]) + 1) + PCN[s[:-1]]
SCALE = lambda root, mode: [(root + i) % 12 for i in ([0, 2, 4, 5, 7, 9, 11] if mode == 'major' else [0, 2, 3, 5, 7, 8, 10])]
MALE = {'am_michael', 'am_fenrir', 'am_puck', 'bm_george', 'bm_fable'}


def slug(s):
    s = s.replace("'", '').replace('’', '')
    return ''.join(ch for ch in s.title() if ch.isalnum())


def tail_trim(x, db=-60):
    m = np.abs(x).max(axis=1) if x.ndim == 2 else np.abs(x)
    idx = np.flatnonzero(m > m.max() * 10 ** (db / 20))
    y = x[:min(len(x), idx[-1] + int(0.03 * SR))].copy()
    f = min(len(y) // 4, int(0.04 * SR))
    y[-f:] *= (np.linspace(1, 0, f)[:, None] if y.ndim == 2 else np.linspace(1, 0, f))
    return y


def save(group, sid, title, wet, dry, bpm, genre, keylabel=None, cam=None, voice=None, style=None, explicit=False, note=None, folder='Vocals', trim=True, bars=None):
    os.makedirs(f'{SITE}/vox', exist_ok=True); os.makedirs(f'{SITE}/vox_dry', exist_ok=True)
    wet = write(f'{SITE}/vox/{sid}.wav', tail_trim(wet) if trim else wet, -1.0)
    dry = write(f'{SITE}/vox_dry/{sid}.wav', tail_trim(dry, -65) if trim else dry, -1.0)
    ksuf = ('_' + keylabel.replace(' minor', 'min').replace(' major', 'maj').replace('#', 's') + ('-' + cam if cam else '')) if keylabel else ''
    base = f"{'VOX-E' if explicit else 'VOX'}_{sid.upper()}_{GN[genre].replace(' ', '')}_{slug(title)[:34]}{ksuf}_{bpm}bpm"
    e = {'id': sid, 'cat': 'vox', 'group': group, 'genre': genre, 'title': title, 'voice': V.VOICE_DESC.get(voice, voice), 'style': style,
         'explicit': explicit, 'bpm': bpm, 'key': keylabel, 'camelot': cam, 'note': note, 'bars': bars,
         'dur': round(len(wet) / SR, 3), 'file': f'audio/vox/{sid}.wav', 'dry': f'audio/vox_dry/{sid}.wav',
         'pack': f'{folder}/{base}.wav', 'packDry': f'{folder}/Dry/{base}_DRY.wav', 'peaks': peaks(wet, 72), 'stereo': True}
    OUT.append({k: v for k, v in e.items() if v is not None})
    print(f'  {sid:5s} {len(wet)/SR:5.2f}s {bpm} {keylabel or "spoken":10s} [{GN[genre]}] {title}', flush=True)


# ------------------------------------------------------------------ sung hooks
HOOKS = [
    ('h01', 'ukg', 'UKG01', 'af_heart', False, True, None, "Baby, don't you know, I've been waiting all night", [
        (0, 2, 'C5', 'Baby'), (2, 2, 'Ab4', '~'), (4, 2, 'C5', "don't"), (6, 2, 'Eb5', 'you'), (8, 6, 'C5', 'know'),
        (18, 2, 'Ab4', "I've"), (20, 2, 'Bb4', 'been'), (22, 2, 'C5', 'waiting'), (24, 2, 'Ab4', '~'), (26, 2, 'F4', 'all'), (28, 4, 'Ab4', 'night'),
        (32, 2, 'C5', 'Baby'), (34, 2, 'Ab4', '~'), (36, 2, 'C5', "don't"), (38, 2, 'Db5', 'you'), (40, 6, 'C5', 'know'),
        (50, 2, 'G4', "I've"), (52, 2, 'Ab4', 'been'), (54, 2, 'Bb4', 'waiting'), (56, 2, 'G4', '~'), (58, 2, 'F4', 'all'), (60, 4, 'G4', 'night')]),
    ('h02', 'ukg', 'UKG02', 'bf_emma', False, True, None, "Hold me closer, say you'll stay", [
        (0, 4, 'A4', 'Hold'), (4, 2, 'C5', 'me'), (6, 3, 'D5', 'closer'), (9, 3, 'C5', '~'),
        (18, 2, 'D5', 'say'), (20, 2, 'F5', "you'll"), (22, 8, 'D5', 'stay'),
        (32, 4, 'Bb4', 'Hold'), (36, 2, 'D5', 'me'), (38, 3, 'F5', 'closer'), (41, 3, 'D5', '~'),
        (50, 2, 'C#5', 'say'), (52, 2, 'E5', "you'll"), (54, 8, 'A4', 'stay')]),
    ('h03', 'ukg', 'UKG05', 'af_bella', False, True, None, 'Sunday morning, feeling your love', [
        (0, 3, 'Bb4', 'Sunday'), (3, 3, 'G4', '~'), (6, 4, 'Bb4', 'morning'), (10, 4, 'C5', '~'),
        (16, 3, 'Eb5', 'feeling'), (19, 3, 'D5', '~'), (22, 2, 'C5', 'your'), (24, 8, 'Bb4', 'love'),
        (32, 3, 'C5', 'Sunday'), (35, 3, 'Ab4', '~'), (38, 4, 'C5', 'morning'), (42, 4, 'Eb5', '~'),
        (48, 3, 'F5', 'feeling'), (51, 3, 'Eb5', '~'), (54, 2, 'D5', 'your'), (56, 8, 'Bb4', 'love')]),
    ('h04', 'ukg', 'UKG04', 'af_heart', False, True, None, 'Lights down low, we can take it slow', [
        (0, 2, 'E5', 'Lights'), (3, 2, 'D5', 'down'), (6, 6, 'C5', 'low'),
        (16, 2, 'A4', 'we'), (18, 2, 'C5', 'can'), (20, 2, 'E5', 'take'), (22, 2, 'D5', 'it'), (24, 8, 'C5', 'slow'),
        (32, 2, 'F5', 'Lights'), (35, 2, 'E5', 'down'), (38, 6, 'D5', 'low'),
        (48, 2, 'B4', 'we'), (50, 2, 'D5', 'can'), (52, 2, 'E5', 'take'), (54, 2, 'D5', 'it'), (56, 8, 'B4', 'slow')]),
    ('h05', 'ukg', 'UKG06', 'af_bella', True, True, None, 'You know I want you, fuck what they say', [
        (0, 2, 'B4', 'You'), (2, 4, 'D5', 'know'), (6, 2, 'E5', 'I'), (8, 3, 'D5', 'want'), (11, 5, 'B4', 'you'),
        (18, 2, 'E5', 'fuck'), (20, 2, 'D5', 'what'), (22, 2, 'B4', 'they'), (24, 8, 'G4', 'say'),
        (32, 2, 'C5', 'You'), (34, 4, 'E5', 'know'), (38, 2, 'G5', 'I'), (40, 3, 'E5', 'want'), (43, 5, 'C5', 'you'),
        (50, 2, 'D#5', 'fuck'), (52, 2, 'F#5', 'what'), (54, 2, 'D#5', 'they'), (56, 8, 'B4', 'say')]),
    ('h06', 'mth', 'MTH01', 'af_nicole', False, False, None, 'Deeper, deeper, into the night', [
        (2, 2, 'A4', 'Deeper'), (4, 4, 'E4', '~'), (10, 2, 'A4', 'deeper'), (12, 4, 'E4', '~'),
        (18, 2, 'G4', 'into'), (20, 2, 'A4', '~'), (22, 2, 'C5', 'the'), (24, 8, 'A4', 'night'),
        (34, 2, 'A4', 'Deeper'), (36, 4, 'E4', '~'), (42, 2, 'A4', 'deeper'), (44, 4, 'E4', '~'),
        (50, 2, 'G4', 'into'), (52, 2, 'E4', '~'), (54, 2, 'D4', 'the'), (56, 8, 'E4', 'night')]),
    ('h07', 'mth', 'MTH02', 'am_michael', False, False, None, 'Lose control, let it go', [
        (0, 4, 'G2', 'Lose'), (4, 2, 'Bb2', 'control'), (6, 6, 'D3', '~'),
        (18, 2, 'C3', 'let'), (20, 2, 'Bb2', 'it'), (22, 10, 'G2', 'go'),
        (32, 4, 'G2', 'Lose'), (36, 2, 'Bb2', 'control'), (38, 6, 'F3', '~'),
        (50, 2, 'Eb3', 'let'), (52, 2, 'D3', 'it'), (54, 10, 'Bb2', 'go')]),
    ('h08', 'mth', 'MTH06', 'af_bella', False, True, None, "Don't stop the groove", [
        (0, 2, 'F#4', "Don't"), (2, 6, 'A4', 'stop'), (10, 2, 'F#4', 'the'), (12, 12, 'B4', 'groove'),
        (32, 2, 'F#4', "Don't"), (34, 6, 'A4', 'stop'), (42, 2, 'F#4', 'the'), (44, 12, 'D5', 'groove')]),
    ('h09', 'mth', 'MTH03', 'af_nicole', True, False, None, 'Fuck it up, let it go', [
        (0, 2, 'C5', 'Fuck'), (2, 2, 'Ab4', 'it'), (4, 4, 'F4', 'up'),
        (18, 2, 'G4', 'let'), (20, 2, 'Ab4', 'it'), (22, 10, 'F4', 'go'),
        (32, 2, 'C5', 'Fuck'), (34, 2, 'Eb5', 'it'), (36, 4, 'C5', 'up'),
        (50, 2, 'Db5', 'let'), (52, 2, 'C5', 'it'), (54, 10, 'Bb4', 'go')]),
    ('h10', 'sg', 'SG01', 'af_heart', False, True, None, 'I need your love tonight', [
        (2, 2, 'D5', 'I'), (4, 4, 'F5', 'need'), (8, 2, 'D5', 'your'), (10, 6, 'C5', 'love'),
        (20, 2, 'Bb4', 'tonight'), (22, 10, 'G4', '~'),
        (34, 2, 'D5', 'I'), (36, 4, 'G5', 'need'), (40, 2, 'F5', 'your'), (42, 6, 'D5', 'love'),
        (52, 2, 'C5', 'tonight'), (54, 10, 'A4', '~')]),
    ('h11', 'sg', 'SG02', 'bm_george', False, False, None, 'Feel the pressure when the bass drop', [
        (0, 2, 'C#3', 'Feel'), (2, 2, 'C#3', 'the'), (4, 2, 'E3', 'pressure'), (6, 4, 'C#3', '~'),
        (16, 2, 'A2', 'when'), (18, 2, 'B2', 'the'), (20, 4, 'C#3', 'bass'), (24, 6, 'F#2', 'drop'),
        (32, 2, 'D3', 'Feel'), (34, 2, 'D3', 'the'), (36, 2, 'F#3', 'pressure'), (38, 4, 'D3', '~'),
        (48, 2, 'B2', 'when'), (50, 2, 'C#3', 'the'), (52, 4, 'F#3', 'bass'), (56, 6, 'C#3', 'drop')]),
    ('h12', 'sg', 'SG06', 'af_bella', False, True, None, 'All the girls say ooh', [
        (0, 2, 'G4', 'All'), (2, 2, 'Bb4', 'the'), (4, 4, 'C5', 'girls'), (8, 4, 'Eb5', 'say'), (12, 12, 'Eb5', 'ooh'),
        (32, 2, 'Ab4', 'All'), (34, 2, 'C5', 'the'), (36, 4, 'Eb5', 'girls'), (40, 4, 'G5', 'say'), (44, 12, 'F5', 'ooh')]),
    ('h13', 'sg', 'SG05', 'af_nicole', True, False, None, 'Bass so heavy, shake that ass', [
        (0, 4, 'B4', 'Bass'), (4, 2, 'D5', 'so'), (6, 2, 'E5', 'heavy'), (8, 6, 'D5', '~'),
        (18, 2, 'E5', 'shake'), (20, 2, 'D5', 'that'), (22, 10, 'B4', 'ass'),
        (32, 4, 'C5', 'Bass'), (36, 2, 'E5', 'so'), (38, 2, 'G5', 'heavy'), (40, 6, 'E5', '~'),
        (50, 2, 'F#5', 'shake'), (52, 2, 'E5', 'that'), (54, 10, 'B4', 'ass')]),
    ('h14', 'ch', 'CH01', 'af_heart', False, True, 0, 'Lift me higher, higher', [
        (0, 4, 'E5', 'Lift'), (4, 2, 'C5', 'me'), (6, 4, 'E5', 'higher'), (10, 6, 'A4', '~'),
        (16, 6, 'F5', 'higher'), (22, 10, 'D5', '~'),
        (32, 4, 'D5', 'Lift'), (36, 2, 'B4', 'me'), (38, 4, 'D5', 'higher'), (42, 6, 'G4', '~'),
        (48, 6, 'E5', 'higher'), (54, 10, 'C5', '~')]),
    ('h15', 'ch', 'CH06', 'bf_emma', False, True, None, 'Feel the music in your soul', [
        (0, 4, 'C5', 'Feel'), (4, 2, 'Ab4', 'the'), (6, 2, 'C5', 'music'), (8, 8, 'Eb5', '~'),
        (18, 2, 'C5', 'in'), (20, 2, 'Bb4', 'your'), (22, 10, 'G4', 'soul'),
        (32, 4, 'Db5', 'Feel'), (36, 2, 'Bb4', 'the'), (38, 2, 'Db5', 'music'), (40, 8, 'F5', '~'),
        (50, 2, 'Eb5', 'in'), (52, 2, 'Db5', 'your'), (54, 10, 'Bb4', 'soul')]),
    ('h16', 'ch', 'CH05', 'af_bella', False, True, None, 'You got me dancing all night long', [
        (0, 2, 'F4', 'You'), (2, 2, 'Ab4', 'got'), (4, 2, 'Bb4', 'me'), (6, 4, 'Db5', 'dancing'), (10, 6, 'C5', '~'),
        (18, 2, 'Bb4', 'all'), (20, 4, 'Db5', 'night'), (24, 8, 'F5', 'long'),
        (32, 2, 'Gb4', 'You'), (34, 2, 'Bb4', 'got'), (36, 2, 'Db5', 'me'), (38, 4, 'Eb5', 'dancing'), (42, 6, 'Db5', '~'),
        (50, 2, 'C5', 'all'), (52, 4, 'Eb5', 'night'), (56, 8, 'F5', 'long')]),
    ('h17', 'ch', 'CH02', 'am_fenrir', False, False, None, 'Let the music take control', [
        (0, 2, 'A2', 'Let'), (2, 2, 'C3', 'the'), (4, 2, 'D3', 'music'), (6, 6, 'F3', '~'),
        (16, 4, 'D3', 'take'), (20, 2, 'C3', 'control'), (22, 10, 'Bb2', '~'),
        (32, 2, 'A2', 'Let'), (34, 2, 'C3', 'the'), (36, 2, 'D3', 'music'), (38, 6, 'F3', '~'),
        (48, 4, 'E3', 'take'), (52, 2, 'D3', 'control'), (54, 10, 'A2', '~')]),
    ('h18', 'ch', 'CH03', 'af_bella', True, True, None, 'Work that body, make that ass bounce', [
        (0, 2, 'G4', 'Work'), (2, 2, 'Bb4', 'that'), (4, 2, 'D5', 'body'), (6, 4, 'C5', '~'),
        (16, 2, 'Bb4', 'make'), (18, 2, 'G4', 'that'), (20, 4, 'Bb4', 'ass'), (24, 6, 'G4', 'bounce'),
        (32, 2, 'G4', 'Work'), (34, 2, 'Bb4', 'that'), (36, 2, 'F5', 'body'), (38, 4, 'D5', '~'),
        (48, 2, 'Eb5', 'make'), (50, 2, 'C5', 'that'), (52, 4, 'Eb5', 'ass'), (56, 6, 'C5', 'bounce')]),
]

# ------------------------------------------------------------------ spoken lines and ad-libs
SPOKEN = [
    # id, genre, text, voice, style, bpm, pitch, explicit
    ('v01', 'ukg', 'Garage music, all night long!', 'bm_george', 'mc', 134, 0, False),
    ('v02', 'ukg', 'Big tune, selecta, bring it back!', 'bm_fable', 'mc', 132, 0, False),
    ('v03', 'ukg', 'Inside the dance, everybody!', 'bm_george', 'radio', 136, 0, False),
    ('v04', 'ukg', 'Two-step, keep it moving.', 'bf_emma', 'diva', 133, 0, False),
    ('v05', 'ukg', 'This tune is fucking wicked!', 'bm_fable', 'mc', 134, 0, True),
    ('v06', 'ukg', 'Fuck the haters, turn it up!', 'bm_george', 'mc', 138, 0, True),
    ('v07', 'mth', 'Close your eyes.', 'af_nicole', 'whisper', 128, 0, False),
    ('v08', 'mth', 'We are the night.', 'am_michael', 'dark', 129, -2, False),
    ('v09', 'mth', 'Follow the rhythm.', 'am_fenrir', 'dark', 130, -2, False),
    ('v10', 'mth', 'Deeper still.', 'af_nicole', 'whisper', 131, 0, False),
    ('v11', 'mth', 'This is so fucking deep.', 'am_michael', 'dark', 128, -2, True),
    ('v12', 'mth', "Keep going, don't you fucking stop.", 'af_nicole', 'whisper', 132, 0, True),
    ('v13', 'sg', 'Speed garage, massive!', 'bm_george', 'mc', 132, 0, False),
    ('v14', 'sg', 'Bass in your face!', 'bm_fable', 'mc', 134, 0, False),
    ('v15', 'sg', 'Dubplate pressure!', 'bm_george', 'radio', 133, 0, False),
    ('v16', 'sg', 'Rewind and come again!', 'bm_fable', 'mc', 136, 0, False),
    ('v17', 'sg', 'This bassline is fucking wicked!', 'bm_george', 'mc', 132, 0, True),
    ('v18', 'sg', 'Shut up and dance, motherfuckers!', 'bm_fable', 'mc', 135, 0, True),
    ('v19', 'ch', 'Welcome to the house.', 'am_fenrir', 'preacher', 128, 0, False),
    ('v20', 'ch', 'Brothers and sisters, let the music play.', 'am_michael', 'preacher', 130, 0, False),
    ('v21', 'ch', 'Somebody scream!', 'af_heart', 'diva', 129, 0, False),
    ('v22', 'ch', 'This is house music.', 'am_puck', 'mc', 128, 0, False),
    ('v23', 'ch', 'Fuck yeah, house music!', 'am_fenrir', 'mc', 130, 0, True),
    ('v24', 'ch', 'Shake your sexy ass.', 'af_bella', 'diva', 128, 0, True),
]
ADLIBS = [
    ('a01', 'ukg', 'Wheel it up!', 'bm_george', 134), ('a02', 'ukg', 'Come again!', 'bm_fable', 132),
    ('a03', 'ukg', 'Oi oi!', 'bm_george', 136), ('a04', 'ukg', 'Yeah, yeah!', 'bf_emma', 133),
    ('a05', 'mth', 'Mmm.', 'af_nicole', 128), ('a06', 'mth', 'Yes.', 'am_michael', 129),
    ('a07', 'mth', 'Uh.', 'af_bella', 130), ('a08', 'mth', 'Hey.', 'am_puck', 132),
    ('a09', 'sg', 'Bo!', 'bm_george', 132), ('a10', 'sg', 'Wicked!', 'bm_fable', 134),
    ('a11', 'sg', 'Big tune!', 'bm_george', 135), ('a12', 'sg', 'Pull up!', 'bm_fable', 136),
    ('a13', 'ch', 'Oh yes!', 'af_heart', 128), ('a14', 'ch', 'Jack it up!', 'am_puck', 130),
    ('a15', 'ch', 'Come on!', 'af_bella', 129), ('a16', 'ch', 'Woo!', 'af_heart', 132),
]


def auto_octave(notes, male):
    med = float(np.median([n for _, _, n, _ in notes]))
    lo, hi = (43, 56) if male else (60, 72)
    o = 0
    while med + o > hi:
        o -= 12
    while med + o < lo:
        o += 12
    return o


def render_hooks(only=None):
    for sid, genre, kid, voice, explicit, harm, oct_over, title, mel in HOOKS:
        if only and sid not in only:
            continue
        t0 = time.time()
        kit = K[kid]; bpm, sw = kit['bpm'], kit['swing']
        notes = [(s, l, N(n), w) for s, l, n, w in mel]
        octv = auto_octave(notes, voice in MALE) if oct_over is None else oct_over
        notes = [(s, l, m + octv, w) for s, l, m, w in notes]
        bars = 4
        L = int(bars * 16 * step_seconds(bpm) * SR)
        lead = V2.sing_line2(notes, voice, bpm, sw, bars, vib=16 if voice in MALE else 18)
        d1 = V2.sing_line2(notes, voice, bpm, sw, bars, cents=7, delay=0.014, vib=13)
        d2 = V2.sing_line2(notes, voice, bpm, sw, bars, cents=-7, delay=0.023, vib=21)
        har = V2.sing_line2(V2.harmony_notes(notes, SCALE(kit['root'], kit['mode'])), voice, bpm, sw, bars, vib=14) if harm else None
        wet = V2.hook_mix(lead, [d1, d2], har, bpm, L)
        dry = V2.clean(lead[:L], voice in MALE)
        heard = V2.asr(lead[:L], SR)
        kl, cam = kit['key']['label'], kit['key']['camelot']
        note = f"sung lead + 2 doubles{' + 3rd harmony' if harm else ''}, sits on {kid} {kit['name']}"
        save('hooks', sid, title, wet, dry, bpm, genre, kl, cam, voice, 'sung', explicit, note, 'Vocals/Sung Hooks', trim=False, bars=4)
        OUT[-1]['asr'] = heard
        print(f'        heard: {heard}   ({time.time() - t0:.0f}s, octave {octv:+d})', flush=True)


def render_spoken(only=None):
    for sid, genre, text, voice, style, bpm, pitch, explicit in SPOKEN:
        if only and sid not in only:
            continue
        sc, x, sp, heard = V2.best_take(text, voice)
        x44 = bfilt(V.up(x), 'highpass', 70, 2)
        dry = board(x44, [pb.PitchShift(semitones=pitch)]) if (style == 'dark' and pitch) else x44
        wet = V2.spoken_chain(x44, style, bpm, pitch, voice in MALE)
        note = {'mc': 'MC, delay throw', 'radio': 'pirate-radio filter', 'dark': f'pitched {pitch} st, grit + delay', 'whisper': 'whisper, wide space',
                'preacher': 'warm, close, big room', 'diva': 'clean, plate + echo'}[style] + f' · best of 3 takes ({sp:.2f}x)'
        save('explicit' if explicit else 'clean', sid, text, wet, V2.clean(dry, voice in MALE), bpm, genre, None, None, voice, style, explicit, note,
             'Vocals/Explicit' if explicit else 'Vocals/Clean')
        OUT[-1]['asr'] = heard


def render_adlibs(only=None):
    for sid, genre, text, voice, bpm in ADLIBS:
        if only and sid not in only:
            continue
        sc, x, sp, heard = V2.best_take(text, voice)
        x44 = bfilt(V.up(x), 'highpass', 70, 2)
        wet = V2.spoken_chain(x44, 'adlib', bpm, 0, voice in MALE)
        save('adlibs', sid, text, wet, V2.clean(x44, voice in MALE), bpm, genre, None, None, voice, 'adlib', False, 'ad-lib, short echo', 'Vocals/Ad-libs')
        OUT[-1]['asr'] = heard


if __name__ == '__main__':
    only = sys.argv[1:] or None
    t0 = time.time()
    print('hooks'); render_hooks(only)
    print('spoken'); render_spoken(only)
    print('adlibs'); render_adlibs(only)
    path = PACK_ROOT + '/out/manifest_vox2.json' if not only else PACK_ROOT + '/out/manifest_vox2_partial.json'
    json.dump(OUT, open(path, 'w'))
    print('done', len(OUT), 'in', round(time.time() - t0), 's')
