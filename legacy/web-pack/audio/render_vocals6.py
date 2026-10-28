"""Vocal wave 6 (engine v3): every sung hook re-sung (phrase-level performances, best of up to five takes judged by speech
recognition, doubles + harmony, 48 kHz bandwidth-extended synthesis, new mix) and new vocals written for the pack.

WAV sources go to out/vox_src/{vox,vox_dry}/<id>.wav (the site later packs them as 3-channel FLAC); manifest entries go to
out/manifest_v6.json (replacing items with the same id, adding new ones).
Run: python3 render_vocals6.py [section|id ...]   sections: hooks
"""
import os as _os  # noqa: E402
PACK_ROOT = _os.environ.get('PACK_ROOT') or _os.path.abspath(_os.path.join(_os.path.dirname(_os.path.abspath(__file__)), '..'))  # the legacy web-pack folder
import json, os, re, sys, time
import numpy as np
import soundfile as sf
from dsp import SR, step_seconds, peaks
import vocals3 as V3
import render_vocals2 as RV
import render_brat as RB
from vocals import VOICE_DESC

W = PACK_ROOT
SRC = f'{W}/out/vox_src'
MAN = f'{W}/out/manifest_v6.json'
KITS = json.load(open(f'{W}/out/kits_all.json'))
OCT = json.load(open(f'{W}/out/hook_octaves.json'))
OLD = {e['id']: e for f in ('manifest_vox.json', 'manifest_vox2.json', 'manifest_brat.json') for e in json.load(open(f'{W}/out/{f}'))}
DESC = {**VOICE_DESC, 'af_aoede': 'US female', 'af_river': 'US female', 'af_alloy': 'US female', 'am_liam': 'US male', 'am_puck': 'US male',
        'am_eric': 'US male', 'bf_alice': 'UK female', 'bf_emma': 'UK female', 'bm_fable': 'UK male', 'bm_george': 'UK male', 'bm_lewis': 'UK male'}
GN = {'ukg': 'UKG', 'mth': 'Minimal', 'sg': 'Speed Garage', 'ch': 'House', 'gh': 'Garage House'}
N = RV.N
LOG = []
PRE = 0.5          # seconds of pre-roll: the first onset consonant and breath come before the downbeat


def log(*a):
    s = ' '.join(str(x) for x in a)
    print(s, flush=True)
    LOG.append(s)


def write_src(sid, wet, dry):
    os.makedirs(f'{SRC}/vox', exist_ok=True)
    os.makedirs(f'{SRC}/vox_dry', exist_ok=True)
    sf.write(f'{SRC}/vox/{sid}.wav', np.clip(wet, -1, 1).astype(np.float32), SR, subtype='PCM_16')
    sf.write(f'{SRC}/vox_dry/{sid}.wav', np.clip(dry, -1, 1).astype(np.float32), SR, subtype='PCM_16')


def load_manifest():
    return json.load(open(MAN)) if os.path.exists(MAN) else []


def put(entry):
    if entry.get('new'):
        entry['wave'] = 6
    m = [e for e in load_manifest() if e['id'] != entry['id']]
    m.append(entry)
    json.dump(m, open(MAN, 'w'), indent=0)


# ================================================================= the existing sung hooks
VOICE_ALTS = {'h09': ['af_sky'], 'h12': ['af_heart', 'af_sarah'], 'h13': ['af_sky', 'af_bella']}   # tried when the first voice isn't understood


def hook_sources():
    out = []
    for sid, genre, kid, voice, explicit, harm, oct_over, title, mel in RV.HOOKS:
        out.append(dict(id=sid, genre=genre, kit=kid, voices=[voice] + VOICE_ALTS.get(sid, []), explicit=explicit, harm=harm, title=title, mel=mel, oct=OCT[sid]))
    for sid, genre, kid, voice, mode, explicit, title, opt, style, ev in RB.CHANTS:
        if mode != 'sung':
            continue
        notes = [(s, l, N(n), w) for s, l, n, w in ev]
        out.append(dict(id=sid, genre=genre, kit=kid, voices=voice if isinstance(voice, list) else [voice], explicit=explicit,
                        harm=bool(opt.get('harm')), title=title, mel=ev, oct=RV.auto_octave(notes, False)))
    return out


def sing_hook(h, seed=7):
    kit = KITS[h['kit']]
    bpm, sw = kit['bpm'], kit['swing']
    notes = [(s, l, N(n) + h['oct'], w) for s, l, n, w in h['mel']]
    n = int(4 * 16 * step_seconds(bpm) * SR)
    total = n / SR + 1.0
    best = None
    for v in h['voices']:
        log(f'  {h["id"]} voice {v}')
        sc, y, tk, heard = V3.best_lead(notes, v, bpm, sw, total, seed=seed, log=log, pre=PRE, loop_sec=n / SR)
        if best is None or sc > best[0] + 0.05:      # another voice only if clearly better understood
            best = (sc, v, y, tk, heard)
        if sc >= 0.95:
            break
    sc, voice, lead, tk, heard = best
    scale = RV.SCALE(kit['root'], kit['mode'])
    lead, dbl, har = V3.hook_parts(notes, voice, bpm, sw, total, scale, h['harm'], seed=seed, take=tk, lead=lead, pre=PRE, loop_sec=n / SR)
    wet, dry = V3.hook_mix(lead, dbl, har, bpm, n, voice in V3.MALE, pre_n=int(round(PRE * SR)))
    return dict(wet=wet, dry=dry, voice=voice, take=tk, score=sc, heard=heard, bpm=bpm, kit=kit)


def render_hooks(only=None):
    for h in hook_sources():
        if only and h['id'] not in only and 'hooks' not in only:
            continue
        t0 = time.time()
        r = sing_hook(h)
        write_src(h['id'], r['wet'], r['dry'])
        e = dict(OLD[h['id']])
        e['voice'] = DESC.get(r['voice'], r['voice'])
        kit = r['kit']
        parts = 'sung lead + 2 doubles' + (' + 3rd harmony' if h['harm'] else '')
        e['note'] = f"{parts}, sits on {h['kit']} {kit['name']}"
        e['dur'] = round(len(r['wet']) / SR, 3)
        e['peaks'] = peaks(r['wet'], 72)
        e['asr'] = r['heard']
        e['engine'] = 'v3'
        e.pop('trim', None)
        put(e)
        log(f"{h['id']} {r['voice']} take {r['take']} score {r['score']:.2f} | wet {V3.k_loudness(r['wet']):.1f} LUFS dry {V3.k_loudness(r['dry']):.1f} "
            f"| {time.time() - t0:.0f}s | heard: {r['heard']}")


# ================================================================= new sung hooks (original lyrics and melodies, written at sung pitch)
# id, kit, candidate voices, explicit, harmony, title, [(step16, len16, note, word | '~')]
NEW_HOOKS = [
    ('h19', 'GH01', ['af_heart', 'af_bella'], False, True, 'Orange light on your skin, let the night pull us in', [
        (2, 2, 'E4', 'Orange'), (4, 2, 'G4', '~'), (6, 2, 'A4', 'light'), (8, 2, 'G4', 'on'), (10, 2, 'A4', 'your'), (12, 4, 'C5', 'skin'),
        (18, 2, 'C5', 'let'), (20, 2, 'A4', 'the'), (22, 4, 'G4', 'night'), (26, 2, 'F4', 'pull'), (28, 2, 'E4', 'us'), (30, 3, 'D4', 'in'),
        (34, 2, 'E4', 'Orange'), (36, 2, 'G4', '~'), (38, 2, 'B4', 'light'), (40, 2, 'A4', 'on'), (42, 2, 'B4', 'your'), (44, 4, 'D5', 'skin'),
        (50, 2, 'D5', 'let'), (52, 2, 'C5', 'the'), (54, 3, 'B4', 'night'), (57, 2, 'A4', 'pull'), (59, 2, 'G4', 'us'), (61, 3, 'A4', 'in')]),
    ('h20', 'GH02', ['bf_emma', 'bf_isabella'], False, True, 'Missed the last train home, so we walk it slow', [
        (0, 3, 'A4', 'Missed'), (3, 3, 'A4', 'the'), (6, 2, 'C5', 'last'), (10, 2, 'A4', 'train'), (12, 4, 'F4', 'home'),
        (18, 2, 'D4', 'so'), (20, 2, 'F4', 'we'), (22, 3, 'G4', 'walk'), (25, 2, 'Bb4', 'it'), (27, 5, 'A4', 'slow'),
        (32, 3, 'A4', 'Missed'), (35, 3, 'A4', 'the'), (38, 2, 'D5', 'last'), (42, 2, 'C5', 'train'), (44, 4, 'A4', 'home'),
        (50, 2, 'E4', 'and'), (52, 2, 'G4', 'I'), (54, 3, 'A4', 'want'), (57, 2, 'C#5', 'you'), (59, 5, 'A4', 'close')]),
    ('h21', 'GH03', ['af_bella', 'af_nova'], False, True, 'Swimming in the neon light, pull me under tonight', [
        (2, 2, 'F4', 'Swimming'), (4, 2, 'Ab4', '~'), (6, 2, 'Bb4', 'in'), (8, 2, 'Ab4', 'the'), (10, 2, 'F4', 'neon'), (12, 2, 'Ab4', '~'), (14, 4, 'Db5', 'light'),
        (20, 2, 'Bb4', 'pull'), (22, 2, 'Ab4', 'me'), (24, 2, 'F4', 'under'), (26, 2, 'Ab4', '~'), (28, 1, 'Ab4', 'tonight'), (29, 3, 'Bb4', '~'),
        (34, 2, 'F4', 'Swimming'), (36, 2, 'Gb4', '~'), (38, 2, 'Bb4', 'in'), (40, 2, 'Db5', 'the'), (42, 2, 'Bb4', 'neon'), (44, 2, 'Db5', '~'), (46, 4, 'F5', 'light'),
        (52, 2, 'Eb5', 'pull'), (54, 2, 'C5', 'me'), (56, 2, 'Ab4', 'under'), (58, 2, 'C5', '~'), (60, 1, 'Bb4', 'tonight'), (61, 3, 'Ab4', '~')]),
    ('h22', 'GH04', ['am_michael', 'bm_george'], False, True, 'Meet me on the chapel steps, sing it out with every breath', [
        (0, 2, 'Bb2', 'Meet'), (2, 2, 'Eb3', 'me'), (4, 2, 'F3', 'on'), (6, 2, 'G3', 'the'), (8, 2, 'Bb3', 'chapel'), (10, 2, 'G3', '~'), (12, 4, 'F3', 'steps'),
        (18, 2, 'G3', 'sing'), (20, 2, 'F3', 'it'), (22, 2, 'Eb3', 'out'), (24, 2, 'G3', 'with'), (26, 2, 'F3', 'every'), (28, 1, 'Eb3', '~'), (29, 3, 'C3', 'breath'),
        (32, 2, 'C3', 'Meet'), (34, 2, 'Eb3', 'me'), (36, 2, 'F3', 'on'), (38, 2, 'Ab3', 'the'), (40, 2, 'C4', 'chapel'), (42, 2, 'Ab3', '~'), (44, 4, 'G3', 'steps'),
        (50, 2, 'Eb3', 'sing'), (52, 2, 'G3', 'it'), (54, 2, 'Ab3', 'out'), (56, 2, 'Bb3', 'with'), (58, 2, 'C4', 'every'), (60, 1, 'Bb3', '~'), (61, 3, 'G3', 'breath')]),
    ('h23', 'UKG07', ['af_kore', 'af_bella'], True, True, 'Lean back, take it slow, let the fucking bassline roll', [
        (0, 3, 'A4', 'Lean'), (3, 3, 'C5', 'back'), (8, 2, 'D5', 'take'), (10, 2, 'C5', 'it'), (12, 4, 'A4', 'slow'),
        (17, 1, 'F4', 'let'), (18, 2, 'A4', 'the'), (20, 2, 'C5', 'fucking'), (22, 2, 'A4', '~'), (24, 2, 'C5', 'bassline'), (26, 2, 'A4', '~'), (28, 4, 'F4', 'roll'),
        (32, 3, 'D5', 'Lean'), (35, 3, 'F5', 'back'), (40, 2, 'D5', 'take'), (42, 2, 'C5', 'it'), (44, 4, 'D5', 'slow'),
        (49, 1, 'E4', 'let'), (50, 2, 'G4', 'the'), (52, 2, 'A4', 'fucking'), (54, 2, 'G4', '~'), (56, 2, 'A4', 'bassline'), (58, 2, 'G4', '~'), (60, 4, 'A4', 'roll')]),
    ('h24', 'UKG08', ['bf_isabella', 'bf_emma'], False, True, 'Stay in bed till the evening, you and me and the feeling', [
        (2, 2, 'G4', 'Stay'), (4, 2, 'Bb4', 'in'), (6, 2, 'C5', 'bed'), (8, 2, 'Bb4', 'till'), (10, 2, 'G4', 'the'), (12, 2, 'Bb4', 'evening'), (14, 4, 'G4', '~'),
        (20, 2, 'F4', 'you'), (22, 2, 'G4', 'and'), (24, 2, 'Bb4', 'me'), (26, 1, 'G4', 'and'), (27, 1, 'F4', 'the'), (28, 2, 'G4', 'feeling'), (30, 2, 'Eb4', '~'),
        (34, 2, 'Ab4', 'Stay'), (36, 2, 'C5', 'in'), (38, 2, 'Eb5', 'bed'), (40, 2, 'C5', 'till'), (42, 2, 'Bb4', 'the'), (44, 2, 'C5', 'evening'), (46, 4, 'Bb4', '~'),
        (52, 2, 'G4', 'you'), (54, 2, 'Bb4', 'and'), (56, 2, 'D5', 'me'), (58, 1, 'C5', 'and'), (59, 1, 'Bb4', 'the'), (60, 2, 'C5', 'feeling'), (62, 2, 'G4', '~')]),
    ('h25', 'MTH05', ['bf_lily', 'af_nicole'], False, False, 'One step down, two steps in, let the dub begin', [
        (0, 2, 'D4', 'One'), (2, 2, 'F4', 'step'), (4, 4, 'E4', 'down'), (8, 2, 'D4', 'two'), (10, 2, 'F4', 'steps'), (12, 4, 'A4', 'in'),
        (18, 2, 'G4', 'let'), (20, 2, 'F4', 'the'), (22, 4, 'E4', 'dub'), (26, 1, 'D4', 'begin'), (27, 5, 'C4', '~'),
        (32, 2, 'D4', 'One'), (34, 2, 'F4', 'step'), (36, 4, 'G4', 'down'), (40, 2, 'Bb4', 'two'), (42, 2, 'A4', 'steps'), (44, 4, 'G4', 'in'),
        (50, 2, 'D4', 'let'), (52, 2, 'F4', 'the'), (54, 4, 'A4', 'dub'), (58, 1, 'G4', 'begin'), (59, 5, 'E4', '~')]),
    ('h26', 'MTH07', ['af_nicole', 'af_sky'], True, False, 'Hands on the wall, slow it down, take it all', [
        (2, 2, 'E4', 'Hands'), (4, 2, 'G4', 'on'), (6, 2, 'A4', 'the'), (8, 6, 'C5', 'wall'),
        (18, 2, 'B4', 'slow'), (20, 2, 'A4', 'it'), (22, 2, 'G4', 'down'), (26, 2, 'E4', 'take'), (28, 2, 'G4', 'it'), (30, 3, 'A4', 'all'),
        (34, 2, 'E4', 'Hands'), (36, 2, 'G4', 'on'), (38, 2, 'A4', 'the'), (40, 6, 'E5', 'wall'),
        (50, 2, 'C5', 'slow'), (52, 2, 'A4', 'it'), (54, 2, 'G4', 'down'), (58, 2, 'D4', 'take'), (60, 2, 'G4', 'it'), (62, 2, 'B4', 'all')]),
    ('h27', 'SG03', ['bm_lewis', 'bm_george'], True, False, 'Big bass weight, shake the fucking place', [
        (0, 3, 'C3', 'Big'), (3, 3, 'Eb3', 'bass'), (6, 6, 'F3', 'weight'),
        (16, 2, 'F3', 'shake'), (18, 2, 'Eb3', 'the'), (20, 2, 'C3', 'fucking'), (22, 2, 'Ab2', '~'), (24, 6, 'C3', 'place'),
        (32, 3, 'Db3', 'Big'), (35, 3, 'F3', 'bass'), (38, 6, 'Ab3', 'weight'),
        (48, 2, 'G3', 'shake'), (50, 2, 'E3', 'the'), (52, 2, 'C3', 'fucking'), (54, 2, 'Bb2', '~'), (56, 8, 'C3', 'place')]),
    ('h28', 'SG08', ['af_sarah', 'af_heart'], False, True, 'Play that organ loud, lift me off the ground', [
        (0, 2, 'G4', 'Play'), (2, 2, 'Bb4', 'that'), (4, 2, 'C5', 'organ'), (6, 2, 'Bb4', '~'), (8, 6, 'G4', 'loud'),
        (16, 2, 'Eb4', 'lift'), (18, 2, 'G4', 'me'), (20, 2, 'Ab4', 'off'), (22, 2, 'G4', 'the'), (24, 6, 'Eb4', 'ground'),
        (32, 2, 'Ab4', 'Play'), (34, 2, 'C5', 'that'), (36, 2, 'Eb5', 'organ'), (38, 2, 'C5', '~'), (40, 6, 'Ab4', 'loud'),
        (48, 2, 'F4', 'lift'), (50, 2, 'G4', 'me'), (52, 2, 'C5', 'off'), (54, 2, 'C5', 'the'), (56, 8, 'B4', 'ground')]),
    ('h29', 'CH07', ['af_nova', 'af_heart'], False, True, "Glitter on the ceiling, tell me what you're feeling", [
        (0, 2, 'F4', 'Glitter'), (2, 2, 'Ab4', '~'), (4, 2, 'Bb4', 'on'), (6, 2, 'Ab4', 'the'), (8, 4, 'Db5', 'ceiling'), (12, 4, 'Bb4', '~'),
        (16, 2, 'Ab4', 'tell'), (18, 2, 'Bb4', 'me'), (20, 2, 'Ab4', 'what'), (22, 2, 'F4', "you're"), (24, 4, 'Ab4', 'feeling'), (28, 4, 'F4', '~'),
        (32, 2, 'Gb4', 'Glitter'), (34, 2, 'Bb4', '~'), (36, 2, 'Db5', 'on'), (38, 2, 'Bb4', 'the'), (40, 4, 'F5', 'ceiling'), (44, 4, 'Db5', '~'),
        (48, 2, 'Bb4', 'tell'), (50, 2, 'C5', 'me'), (52, 2, 'Eb5', 'what'), (54, 2, 'C5', "you're"), (56, 4, 'Eb5', 'feeling'), (60, 4, 'C5', '~')]),
    ('h30', 'CH08', ['af_jessica', 'af_sarah'], False, True, 'Hands up higher, everybody, one desire', [
        (0, 4, 'C5', 'Hands'), (4, 4, 'C5', 'up'), (8, 4, 'E5', 'higher'), (12, 4, 'C5', '~'),
        (18, 2, 'A4', 'everybody'), (20, 2, 'C5', '~'), (22, 2, 'A4', '~'), (24, 2, 'G4', '~'), (26, 2, 'A4', 'one'), (28, 1, 'C5', 'desire'), (29, 3, 'D5', '~'),
        (32, 4, 'B4', 'Hands'), (36, 4, 'B4', 'up'), (40, 4, 'D5', 'higher'), (44, 4, 'B4', '~'),
        (50, 2, 'A4', 'everybody'), (52, 2, 'B4', '~'), (54, 2, 'C5', '~'), (56, 2, 'B4', '~'), (58, 2, 'A4', 'one'), (60, 1, 'G4', 'desire'), (61, 3, 'A4', '~')]),
]
GL = {'ukg': 'UKG', 'mth': 'Minimal', 'sg': 'SpeedGarage', 'ch': 'House', 'gh': 'GarageHouse'}


def slug(t):
    t = t.replace("'", '').replace('’', '')
    return ''.join(ch for ch in t.title() if ch.isalnum())


def pack_paths(sid, fam, title, kit, bpm, explicit, folder):
    key = kit['key']
    ksuf = '_' + key['label'].replace(' minor', 'min').replace(' major', 'maj').replace('#', 's') + '-' + key['camelot']
    base = f"{'VOX-E' if explicit else 'VOX'}_{sid.upper()}_{GL[fam]}_{slug(title)[:34]}{ksuf}_{bpm}bpm"
    if fam == 'gh':
        folder += '/Garage House (Disclosure-style)'
    return f'{folder}/{base}.wav', f'{folder}/Dry/{base}_DRY.wav'


def check_melody(sid, kid, mel):
    """Print notes that are neither chord tones nor in the scale, and non-chord tones on beats."""
    kit = KITS[kid]
    tl = V3.chord_timeline(kit)
    scale = set(RV.SCALE(kit['root'], kit['mode']))
    bad = []
    for s0, ln, n, w in mel:
        pcs = V3.chord_at(tl, s0)
        pc = V3.note_pc(n)
        if pc not in pcs and (pc not in scale or (s0 % 4 == 0 and ln >= 3)):
            bad.append(f'{n}@{s0}({w})')
    return bad


def render_new_hooks(only=None):
    for sid, kid, voices, explicit, harm, title, mel in NEW_HOOKS:
        if only and sid not in only and 'new_hooks' not in only:
            continue
        t0 = time.time()
        kit = KITS[kid]
        bad = check_melody(sid, kid, mel)
        if bad:
            log(f'  {sid} melody notes outside the chord: {bad}')
        h = dict(id=sid, kit=kid, voices=voices, harm=False, mel=mel, oct=0)
        bpm, sw = kit['bpm'], kit['swing']
        notes = [(s, l, N(n), w) for s, l, n, w in mel]
        n = int(4 * 16 * step_seconds(bpm) * SR)
        total = n / SR + 1.0
        best = None
        for v in voices:
            log(f'  {sid} voice {v}')
            sc, y, tk, heard = V3.best_lead(notes, v, bpm, sw, total, seed=11, log=log, pre=PRE, loop_sec=n / SR)
            if best is None or sc > best[0] + 1e-6:
                best = (sc, v, y, tk, heard)
            if sc >= 0.95:
                break
        sc, voice, lead, tk, heard = best
        scale = RV.SCALE(kit['root'], kit['mode'])
        hn = V3.harmony_chords(notes, V3.chord_timeline(kit), scale, ceiling=72 if voice in V3.MALE else 81) if harm else None
        lead, dbl, har = V3.hook_parts(notes, voice, bpm, sw, total, scale, harm, seed=11, take=tk, lead=lead, pre=PRE, loop_sec=n / SR, harm_notes=hn)
        wet, dry = V3.hook_mix(lead, dbl, har, bpm, n, voice in V3.MALE, pre_n=int(round(PRE * SR)))
        write_src(sid, wet, dry)
        fam = kit.get('fam') or kit['genre']
        pk, pkd = pack_paths(sid, fam, title, kit, bpm, explicit, 'Vocals/Sung Hooks')
        parts = 'sung lead + 2 doubles' + (' + harmony on the chords' if harm else '')
        e = {'id': sid, 'cat': 'vox', 'group': 'hooks', 'genre': kit['genre'], 'title': title, 'voice': DESC.get(voice, voice), 'style': 'sung',
             'explicit': explicit, 'bpm': bpm, 'key': kit['key']['label'], 'camelot': kit['key']['camelot'],
             'note': f"{parts}, written for {kid} {kit['name']}", 'bars': 4, 'dur': round(len(wet) / SR, 3),
             'file': f'audio/vox/{sid}.wav', 'dry': f'audio/vox_dry/{sid}.wav', 'pack': pk, 'packDry': pkd, 'peaks': peaks(wet, 72), 'stereo': True,
             'asr': heard, 'engine': 'v3', 'new': True, 'kit': kid}
        if fam == 'gh':
            e['fam'] = 'gh'
        put(e)
        log(f"{sid} {voice} take {tk} score {sc:.2f} | wet {V3.k_loudness(wet):.1f} LUFS dry {V3.k_loudness(dry):.1f} | {time.time() - t0:.0f}s | heard: {heard}")


# ================================================================= chop loops: the kit's lead line sung in syllables, 2 bars, swung like the kit
NEW_CHOPS = [
    # id, kit, voice, syllables (cycled over the notes), formant
    ('ch09', 'GH01', 'af_heart', ['Light', 'Ooh', 'Skin', 'Oh', 'Night', 'Ah', 'In', 'Ooh'], 1.06),
    ('ch10', 'GH02', 'bf_emma', ['Home', 'Oh', 'Slow', 'Yeah', 'Close', 'Oh', 'Train', 'Ooh', 'Home', 'Ah'], 1.05),
    ('ch11', 'GH03', 'af_bella', ['Light', 'Swim', 'Ooh', 'Pull', 'Ah', 'Light', 'Oh', 'Me', 'Ooh', 'Ah'], 1.06),
    ('ch12', 'GH04', 'af_nova', ['Sing', 'Oh', 'Steps', 'Yeah', 'Breath', 'Oh', 'Sing', 'Ah', 'Meet', 'Ooh'], 1.05),
    ('ch13', 'UKG07', 'af_kore', ['Lean', 'Back', 'Oh', 'Slow', 'Roll', 'Yeah', 'Lean', 'Back', 'Ah', 'Roll'], 1.07),
    ('ch14', 'UKG08', 'bf_isabella', ['Stay', 'Ooh', 'Bed', 'Oh', 'Me', 'Ah', 'Stay', 'Ooh', 'Feel', 'Yeah'], 1.05),
    ('ch15', 'SG07', 'af_sarah', ['Hey', 'Oh', 'Yeah', 'Ah', 'Hey', 'Oh', 'Yeah', 'Uh', 'Hey', 'Oh'], 1.08),
    ('ch16', 'CH07', 'af_heart', ['Oh', 'Ooh', 'Ah', 'Ooh', 'Oh', 'Ah', 'Ooh', 'Yeah'], 1.04),
]


def chop_mix(lead, bpm, loop_n, pre_n, male=False):
    L = polish(lead, male, sat=0.2)
    n = len(L)
    wet = np.zeros((n + 3 * SR, 2))
    wet[:n] += V3.to_stereo(L)
    pr = V3.plate(L, 1.2, 0.12)
    wet[:len(pr)] += pr[:len(wet)]
    dl = V3.ducked_delay(L, bpm, 0.75, 0.3, 0.17, duck_db=7.0)
    wet[:min(len(dl), len(wet))] += dl[:len(wet)]
    w = V3.finish(V3.loop_with_pre(wet, loop_n, pre_n), -13.0)
    d = V3.finish(V3.loop_with_pre(polish(lead, male, sat=0.1), loop_n, pre_n, tail=int(0.3 * SR)), -13.5)
    return w, d


def polish(x, male=False, **kw):
    return V3.polish(x, male, **kw)


def sing_chop_loop(kid, voice, syls, formant, seed=5):
    """The kit's A lead line (first 2 bars) sung one syllable per note, swung like the kit -> (wet, dry, notes, octave shift)."""
    kit = KITS[kid]
    bpm, sw = kit['bpm'], kit['swing']
    ev = [e for e in kit['parts']['leadA'] if e[0] < 3840]
    med = float(np.median([e[2] for e in ev]))
    octs = -12 if med > 74 else (12 if med < 57 else 0)
    notes = [(e[0] / 120, max(0.8, e[1] / 120), e[2] + octs, syls[i % len(syls)]) for i, e in enumerate(ev)]
    n = int(2 * 16 * step_seconds(bpm) * SR)
    S = V3.Sing(voice, speed=0.85, vib=0.0, scoop=12, drift=2, breath=0.1, fall=False, breaths=False, swell=0.8, glide=0.012,
                formant=formant, seed=seed, glottal=True)
    lead = V3.sing(notes, bpm, sw, S, n / SR + 1.0, pre=PRE, loop_sec=n / SR)
    wet, dry = chop_mix(lead, bpm, n, int(round(PRE * SR)), voice in V3.MALE)
    return wet, dry, notes, octs


def render_chops(only=None):
    for sid, kid, voice, syls, formant in NEW_CHOPS:
        if only and sid not in only and 'chops' not in only:
            continue
        t0 = time.time()
        kit = KITS[kid]
        bpm = kit['bpm']
        wet, dry, notes, octs = sing_chop_loop(kid, voice, syls, formant)
        write_src(sid, wet, dry)
        fam = kit.get('fam') or kit['genre']
        title = f"Vox Chop · {kit['name']}"
        pk, pkd = pack_paths(sid, fam, title, kit, bpm, False, 'Vocals/Chops')
        e = {'id': sid, 'cat': 'vox', 'group': 'chops', 'genre': kit['genre'], 'title': title, 'voice': DESC.get(voice, voice), 'style': 'chop',
             'explicit': False, 'bpm': bpm, 'key': kit['key']['label'], 'camelot': kit['key']['camelot'],
             'note': f"2-bar loop on the {kit['name']} lead line, swung like the kit ({' · '.join(dict.fromkeys(syls))})", 'bars': 2,
             'dur': round(len(wet) / SR, 3), 'file': f'audio/vox/{sid}.wav', 'dry': f'audio/vox_dry/{sid}.wav', 'pack': pk, 'packDry': pkd,
             'peaks': peaks(wet, 72), 'stereo': True, 'engine': 'v3', 'new': True, 'kit': kid}
        if fam == 'gh':
            e['fam'] = 'gh'
        put(e)
        log(f"{sid} {kid} {voice} {len(notes)} notes oct {octs:+d} | wet {V3.k_loudness(wet):.1f} dry {V3.k_loudness(dry):.1f} | {time.time() - t0:.0f}s")


def resing_old(only=None):
    """The first wave's chop loops and stabs, sung again by the v3 engine (same ids, words, notes, keys, tempos and file names)."""
    import render_vocals as RV1
    for sid, kid, voice, syls, formant in RV1.CHOPS:
        if only and sid not in only and 'old_chops' not in only:
            continue
        t0 = time.time()
        kit = KITS[kid]
        wet, dry, notes, octs = sing_chop_loop(kid, voice, [w.rstrip('.!') for w in syls], formant)
        write_src(sid, wet, dry)
        e = {k: v for k, v in OLD[sid].items() if k not in ('genre', 'bars')}
        e.update(note=f"2-bar loop on the {kit['name']} lead line, swung like the kit ({' · '.join(dict.fromkeys(w.rstrip('.!') for w in syls))})",
                 dur=round(len(wet) / SR, 3), peaks=peaks(wet, 72), engine='v3', voice=DESC.get(voice, voice))
        put(e)
        log(f"{sid} {kid} {voice} {len(notes)} notes oct {octs:+d} | wet {V3.k_loudness(wet):.1f} dry {V3.k_loudness(dry):.1f} | {time.time() - t0:.0f}s")
    for sid, word, voice, note, sec, formant, bpm in RV1.STABS:
        if only and sid not in only and 'old_stabs' not in only:
            continue
        t0 = time.time()
        wet, dry = sing_stab(word, voice, note, sec, formant, bpm)
        write_src(sid, wet, dry)
        e = {k: v for k, v in OLD[sid].items() if k not in ('genre',)}
        e.update(note=f'sung on {note}, dotted-8th throw', dur=round(len(wet) / SR, 3), peaks=peaks(wet, 72), engine='v3', voice=DESC.get(voice, voice))
        put(e)
        log(f"{sid} {word} {voice} {note} | dry {len(dry) / SR:.2f}s wet {len(wet) / SR:.2f}s wet {V3.k_loudness(wet):.1f} LUFS | {time.time() - t0:.0f}s")


# ================================================================= vocal pads: four voices holding the kit's chords on 'ooh' / 'aah', 4 bars
NEW_PADS = [
    # id, kit, vowel, voices (top to bottom), title
    ('p01', 'GH01', 'Ooh', ['af_heart', 'af_bella', 'af_sarah', 'af_nova'], 'Ooh choir · Sodium Glow chords'),
    ('p02', 'GH03', 'Aah', ['af_bella', 'af_nova', 'af_heart', 'af_sarah'], 'Aah choir · Neon Lido chords'),
    ('p03', 'GH04', 'Ooh', ['af_nova', 'af_heart', 'am_michael', 'bm_george'], 'Gospel ooh · Chapel Steps chords'),
    ('p04', 'UKG08', 'Aah', ['bf_emma', 'bf_isabella', 'bf_lily', 'af_heart'], 'Aah choir · Sunday Wub chords'),
]


def pad_voicings(kit, top_max=76, bottom_min=55):
    """The kit's A-part chords as 4-voice voicings (the kit's own voice-leading), shifted by octaves into a choir range."""
    tl = V3.chord_timeline(kit)
    out = []
    for st, pcs, name in tl:
        v = sorted(N(x) for x in kit['chords'][name])[:4]
        while len(v) < 4:
            v.append(v[-1] + 12)
        out.append((st, v))
    hi = max(max(v) for _, v in out)
    lo = min(min(v) for _, v in out)
    sh = 0
    while hi + sh > top_max:
        sh -= 12
    while lo + sh < bottom_min - 12 and hi + sh + 12 <= top_max:
        sh += 12
    return [(st, [m + sh for m in v]) for st, v in out]


def render_pads(only=None):
    for sid, kid, vowel, voices, title in NEW_PADS:
        if only and sid not in only and 'pads' not in only:
            continue
        t0 = time.time()
        kit = KITS[kid]
        bpm, sw = kit['bpm'], kit['swing']
        vc = pad_voicings(kit)
        n = int(4 * 16 * step_seconds(bpm) * SR)
        # two passes round the progression as one long vowel per voice; the loop is cut from the second pass, so its reverb is in
        # steady state, and its start is crossfaded from what follows its end, so the vowel runs straight through the loop point
        ch = [(st + 64 * c, v) for c in (0, 1) for st, v in vc]
        steps = [st for st, _ in ch] + [128 + vc[0][0]]
        parts = []
        for k, voice in enumerate(voices):          # part 0 = top note
            notes = []
            for j, (st, v) in enumerate(ch):
                m = v[3 - k] - (12 if voice in V3.MALE else 0)
                notes.append((st, steps[j + 1] - st, m, vowel if j == 0 else '~'))
            S = V3.Sing(voice, speed=0.8, vib=14.0, vib_rate=4.8, scoop=20, drift=4, breath=0.05, fall=False, breaths=False, swell=0.6,
                        glide=0.09, seed=31 + k, timing=0.01, tuning=4, glottal=False, air=0.35)
            parts.append(V3.sing(notes, bpm, sw, S, 2 * n / SR + 1.5, pre=PRE))
        pans = [-0.55, 0.55, -0.2, 0.2]
        lift = [polish(p, voices[k] in V3.MALE, sat=0.05) * (0.8 if k else 1.0) for k, p in enumerate(parts)]
        dry_mono = V3.board(sum(lift), [V3.pb.HighpassFilter(140), V3.pb.LowpassFilter(9500)])
        mix = sum(V3.to_stereo(x, pans[k]) for k, x in enumerate(lift))
        mix = V3.board(mix, [V3.pb.HighpassFilter(140), V3.pb.LowpassFilter(9500), V3.pb.Compressor(threshold_db=-18, ratio=2.0, attack_ms=30, release_ms=300)])
        wet = mix + V3.plate(mix.mean(axis=1), 2.6, 0.32, hp=220, lp=9000)[:len(mix)]
        a0 = int(round(PRE * SR)) + n            # start of the second pass
        xf = int(0.15 * SR)

        def cut(x):
            y = x[a0:a0 + n].copy()
            r = np.linspace(0, 1, xf)
            r = (r if y.ndim == 1 else r[:, None])
            y[:xf] = y[:xf] * r + x[a0 + n:a0 + n + xf] * (1 - r)
            return y
        w = V3.finish(cut(wet), -14.0)
        d = V3.finish(cut(dry_mono), -14.5)
        write_src(sid, w, d)
        fam = kit.get('fam') or kit['genre']
        pk, pkd = pack_paths(sid, fam, title, kit, bpm, False, 'Vocals/Pads')
        e = {'id': sid, 'cat': 'vox', 'group': 'pads', 'genre': kit['genre'], 'title': title, 'voice': 'choir (' + ', '.join(sorted(set(DESC.get(v, v) for v in voices))) + ')',
             'style': 'pad', 'explicit': False, 'bpm': bpm, 'key': kit['key']['label'], 'camelot': kit['key']['camelot'],
             'note': f"four voices on '{vowel.lower()}' holding the {kit['name']} chords ({' · '.join(nm for _, _, nm in V3.chord_timeline(kit))}), 4-bar loop",
             'bars': 4, 'dur': round(len(w) / SR, 3), 'file': f'audio/vox/{sid}.wav', 'dry': f'audio/vox_dry/{sid}.wav', 'pack': pk, 'packDry': pkd,
             'peaks': peaks(w, 72), 'stereo': True, 'engine': 'v3', 'new': True, 'kit': kid}
        if fam == 'gh':
            e['fam'] = 'gh'
        put(e)
        log(f"{sid} {kid} {vowel} {[v for _, v in vc]} | wet {V3.k_loudness(w):.1f} dry {V3.k_loudness(d):.1f} | {time.time() - t0:.0f}s")


# ================================================================= stabs: one sung syllable, a one-shot with a throw
NEW_STABS = [
    # id, word, voice, note, seconds, formant, bpm, genre
    ('s11', 'Hey!', 'af_bella', 'A4', 0.34, 1.05, 128, 'ukg'),
    ('s12', 'Yeah!', 'am_michael', 'E3', 0.45, 1.0, 132, 'ukg'),
    ('s13', 'Ooh.', 'af_heart', 'E5', 0.62, 1.04, 128, 'ch'),
    ('s14', 'Oh.', 'bf_emma', 'D5', 0.5, 1.04, 132, 'ukg'),
    ('s15', 'Ah.', 'af_nova', 'C5', 0.6, 1.03, 128, 'ch'),
    ('s16', 'Whoa!', 'bm_george', 'G3', 0.55, 1.0, 134, 'sg'),
    ('s17', 'Uh!', 'am_onyx', 'C3', 0.28, 0.97, 128, 'mth'),
    ('s18', 'Go!', 'af_sarah', 'F4', 0.3, 1.04, 136, 'sg'),
    ('s19', 'Come on!', 'bf_isabella', 'G4', 0.45, 1.03, 133, 'ukg'),
    ('s20', 'Yes!', 'af_kore', 'Bb4', 0.34, 1.05, 130, 'mth'),
    ('s21', 'Hah!', 'am_fenrir', 'D3', 0.25, 1.0, 130, 'sg'),
    ('s22', 'Hey!', 'bm_lewis', 'A3', 0.34, 1.0, 133, 'gh'),
]


def sing_stab(word, voice, note, sec, formant, bpm):
    """One sung syllable (or a two-note 'Come on') as a one-shot: dry, and wet with a plate and a dotted-8th throw."""
    st = step_seconds(bpm)
    steps = sec / st
    words = word.rstrip('!.').split()
    if len(words) == 1:
        notes = [(0, steps, N(note), words[0])]
    else:      # 'Come on': a short pickup a step below, then the note
        notes = [(0, 0.9, N(note) - 2, words[0]), (1, steps, N(note), words[1])]
    S = V3.Sing(voice, speed=0.85, vib=8.0, scoop=25, drift=1, fall=False, breaths=False, swell=1.0, formant=formant, seed=3, glottal=False)
    y = V3.sing(notes, bpm, 0.5, S, sec + 0.6, pre=PRE)
    male = voice in V3.MALE
    dry = V3.board(V3.polish(y, male, sat=0.15), [V3.pb.Compressor(threshold_db=-20, ratio=3.0, attack_ms=4, release_ms=90)])
    live = np.flatnonzero(np.abs(dry) > np.abs(dry).max() * 10 ** (-50 / 20))
    start = max(0, int(live[0]) - int(0.01 * SR)) if len(live) else 0   # from just before the first sound (the consonant before the note)
    dry = dry[start:].copy()
    fi = int(0.003 * SR)
    dry[:fi] *= np.linspace(0, 1, fi)
    k = np.flatnonzero(np.abs(dry) > np.abs(dry).max() * 10 ** (-60 / 20))
    dry = dry[:k[-1] + int(0.03 * SR)]
    f = int(0.015 * SR)
    dry[-f:] *= np.linspace(1, 0, f)
    wet = np.zeros((len(dry) + int(2.2 * SR), 2))
    wet[:len(dry)] += V3.to_stereo(dry)
    pr = V3.plate(dry, 1.6, 0.16)
    wet[:len(pr)] += pr[:len(wet)]
    dl = V3.ducked_delay(dry, bpm, 0.75, 0.38, 0.28, duck_db=4.0)
    wet[:min(len(dl), len(wet))] += dl[:len(wet)]
    k = np.flatnonzero(np.abs(wet).max(axis=1) > np.abs(wet).max() * 10 ** (-60 / 20))
    wet = wet[:k[-1] + int(0.05 * SR)]
    f = int(0.05 * SR)
    wet[-f:] *= np.linspace(1, 0, f)[:, None]
    return V3.finish(wet, -13.0), V3.finish(dry, -12.0)


def render_stabs(only=None):
    for sid, word, voice, note, sec, formant, bpm, genre in NEW_STABS:
        if only and sid not in only and 'stabs' not in only:
            continue
        t0 = time.time()
        wet, dry = sing_stab(word, voice, note, sec, formant, bpm)
        write_src(sid, wet, dry)
        title = word.strip('.!')
        base = f"VOX_{sid.upper()}_{slug(title)}_{note[:-1].replace('#', 's')}_{bpm}bpm"
        folder = 'Vocals/Stabs' + ('/Garage House (Disclosure-style)' if genre == 'gh' else '')
        e = {'id': sid, 'cat': 'vox', 'group': 'stabs', 'genre': 'ukg' if genre == 'gh' else genre, 'title': title, 'voice': DESC.get(voice, voice), 'style': 'stab',
             'explicit': False, 'bpm': bpm, 'key': note[:-1], 'note': f'sung on {note}, dotted-8th throw', 'dur': round(len(wet) / SR, 3),
             'file': f'audio/vox/{sid}.wav', 'dry': f'audio/vox_dry/{sid}.wav', 'pack': f'{folder}/{base}.wav', 'packDry': f'{folder}/Dry/{base}_DRY.wav',
             'peaks': peaks(wet, 72), 'stereo': True, 'engine': 'v3', 'new': True}
        if genre == 'gh':
            e['fam'] = 'gh'
        put(e)
        log(f"{sid} {word} {voice} {note} | dry {len(dry) / SR:.2f}s wet {len(wet) / SR:.2f}s wet {V3.k_loudness(wet):.1f} LUFS | {time.time() - t0:.0f}s")


# ================================================================= second pass on the hooks the recogniser still blurs
def logged_scores():
    """Last logged lead score per hook id (from the render logs)."""
    out = {}
    for path in (f'{W}/out/render_v6_all.out', f'{W}/out/render_v6.log', f'{W}/out/render_v7.out'):
        if not os.path.exists(path):
            continue
        for line in open(path, errors='replace'):
            m = re.match(r'^(h\d+|bh\d) \S+ take .* score (-?[\d.]+)', line)
            if m:
                out[m.group(1)] = float(m.group(2))
    return out


def improve_hooks(ids, margin=0.03):
    """Wider take search (consonant length, three speeds, glottal onsets, tinted voices, every candidate voice); a hook is
    replaced only if its new lead is understood clearly better than the one on the site."""
    prev = logged_scores()
    srcs = {h['id']: h for h in hook_sources()}
    for sid, kid, voices, ex, harm, title, mel in NEW_HOOKS:
        srcs[sid] = dict(id=sid, kit=kid, voices=voices, explicit=ex, harm=harm, title=title, mel=mel, oct=0, new=True)
    for hid in ids:
        h = srcs[hid]
        t0 = time.time()
        kit = KITS[h['kit']]
        bpm, sw = kit['bpm'], kit['swing']
        notes = [(s_, l, N(n) + h['oct'], w) for s_, l, n, w in h['mel']]
        n = int(4 * 16 * step_seconds(bpm) * SR)
        total = n / SR + 1.0
        best = None
        for v in h['voices']:
            log(f'  {hid} voice {v} (wide search; on the site: {prev.get(hid)})')
            sc, y, tk, heard = V3.best_lead(notes, v, bpm, sw, total, seed=7 if not h.get('new') else 11, log=log, pre=PRE, loop_sec=n / SR,
                                            takes=V3.WIDE_TAKES, need=0.97, retry=0.9)
            if best is None or sc > best[0] + 0.03:
                best = (sc, v, y, tk, heard)
            if sc >= 0.97:
                break
        sc, voice, lead, tk, heard = best
        if prev.get(hid) is not None and sc < prev[hid] + margin:
            log(f'{hid} kept (best new take {sc:.2f} vs {prev[hid]:.2f} on the site) | {time.time() - t0:.0f}s')
            continue
        scale = RV.SCALE(kit['root'], kit['mode'])
        hn = None
        if h.get('new') and h['harm']:
            hn = V3.harmony_chords(notes, V3.chord_timeline(kit), scale, ceiling=72 if voice in V3.MALE else 81)
        lead, dbl, har = V3.hook_parts(notes, voice, bpm, sw, total, scale, h['harm'], seed=7 if not h.get('new') else 11, take=tk, lead=lead,
                                       pre=PRE, loop_sec=n / SR, harm_notes=hn)
        wet, dry = V3.hook_mix(lead, dbl, har, bpm, n, voice in V3.MALE, pre_n=int(round(PRE * SR)))
        write_src(hid, wet, dry)
        e = next(x for x in load_manifest() if x['id'] == hid)
        e.update(voice=DESC.get(voice, voice), dur=round(len(wet) / SR, 3), peaks=peaks(wet, 72), asr=heard)
        put(e)
        log(f"{hid} {voice} take {tk} score {sc:.2f} | wet {V3.k_loudness(wet):.1f} LUFS dry {V3.k_loudness(dry):.1f} | {time.time() - t0:.0f}s | heard: {heard}")


# ================================================================= tuned / robot lines, re-made with the aligned TTS and 48 kHz air
def hardtune_f0(f0, pcs, target_center, contour=0.7):
    """The first wave's hard-tune, on a WORLD f0 track: compress the speech contour around a centre note, snap every frame to the
    key with hysteresis (held notes), 1-frame glides, creaky phrase ends dropped."""
    from scipy.ndimage import median_filter
    import vocals as V
    v = f0 > 0
    m = V.hz2m(np.where(v, f0, 1.0))
    med = np.median(m[v]) if v.any() else 60
    v = v & (m > med - 9)
    shift = float(np.round(target_center - med))
    mm = np.where(v, med + shift + np.clip((m - med) * contour, -8, 8), 0)
    mm = np.where(v, median_filter(mm, size=5), 0)
    tuned = np.zeros_like(mm)
    cur = None
    for i in range(len(mm)):
        if not v[i]:
            cur = None
            continue
        snap = V.snap_scale(np.array([mm[i]]), pcs)[0]
        if cur is not None and abs(mm[i] - cur) < 0.75:
            snap = cur
        cur = snap
        tuned[i] = snap
    g = tuned.copy()
    for i in range(1, len(tuned)):
        if v[i] and v[i - 1] and tuned[i] != tuned[i - 1]:
            g[i] = 0.5 * (tuned[i] + tuned[i - 1])
    return np.where(v, V.m2hz(np.where(v, g, 60)), 0.0)


def resing_tuned(only=None):
    import pyworld as pw
    from scipy import signal as sg
    import render_vocals as RV1
    import vocals as V
    import vocals2 as V2
    from dsp import bfilt
    for sid, text, voice, root, mode, bpm, how, explicit, kitid in RV1.TUNED:
        if only and sid not in only and 'tuned' not in only:
            continue
        t0 = time.time()
        best = None
        for sp in (0.95, 0.9, 1.0):          # best of three spoken takes
            x, toks = V3.KO().speak(text.split(), voice, sp)
            heard = V2.asr(x, V3.TTS_SR)
            sc = V3.lyric_score(text, heard)
            if best is None or sc > best[0] + 1e-6:
                best = (sc, x, sp, heard)
        sc, x, spd, heard = best
        f0, spe, ap = V3.analyze(x)
        pcs = RV1.scale(root, mode)
        v = f0 > 0
        med = np.median(V.hz2m(f0[v]))
        anchors = [n for n in range(int(med) - 7, int(med) + 8) if n % 12 in (root, (root + 7) % 12)]
        center = min(anchors, key=lambda n: abs(n - med))
        if how == 'tune':
            f0n = hardtune_f0(f0, pcs, center)
            form, apx = 1.0, ap
        else:
            tgt = center if center % 12 == root else center - 7 if (center - 7) % 12 == root else center
            f0n = np.where(v, V.m2hz(tgt), 0.0)
            form, apx = 0.97, ap * 0.6
        spw = V.warp_formants(spe, form) if abs(form - 1) > 1e-3 else spe
        kind = np.where(f0n > 0, 2, 1)
        sp2, ap2 = V3.extend_hf(spw, np.clip(apx, 0, 1), kind)
        y = pw.synthesize(np.ascontiguousarray(f0n), sp2, ap2, 2 * V3.TTS_SR, V3.FP)
        y44 = bfilt(sg.resample_poly(y, 147, 160), 'highpass', 70, 2)
        live = np.flatnonzero(np.abs(y44) > np.abs(y44).max() * 10 ** (-50 / 20))
        y44 = y44[max(0, int(live[0]) - int(0.008 * SR)):].copy()      # start on the first sound, like the first wave's files
        fi = int(0.003 * SR)
        y44[:fi] *= np.linspace(0, 1, fi)
        wet = V.style_chain(y44, 'robot' if how == 'robot' else 'tuned', bpm)
        wet, dry = RV.tail_trim(wet), RV.tail_trim(y44, -65)
        wet, dry = V3.finish(wet, -13.0), V3.finish(dry, -13.5)
        write_src(sid, wet, dry)
        e = dict(OLD[sid])
        e.pop('genre', None)
        e.update(dur=round(len(wet) / SR, 3), peaks=peaks(wet, 72), engine='v3', asr=heard, voice=DESC.get(voice, voice))
        put(e)
        log(f"{sid} {how} {voice} '{text}' take {spd} score {sc:.2f} | wet {V3.k_loudness(wet):.1f} dry {V3.k_loudness(dry):.1f} | {time.time() - t0:.0f}s | heard: {heard}")


# ================================================================= new spoken lines and ad-libs (best of three takes, the pack's spoken chains)
NEW_SPOKEN = [
    # id, genre, text, voice, style, bpm, pitch, explicit
    ('v25', 'gh', "Last train's gone. Guess we're dancing till sunrise.", 'bf_emma', 'diva', 132, 0, False),
    ('v26', 'gh', 'Welcome to the lido. No lifeguard after midnight.', 'af_heart', 'whisper', 128, 0, False),
    ('v27', 'gh', 'Sunday service on the dancefloor. Can I get an amen?', 'am_michael', 'preacher', 133, 0, False),
    ('v28', 'gh', 'Orange streetlights, slow bassline, nowhere to be.', 'bf_isabella', 'diva', 128, 0, False),
    ('v29', 'ukg', 'Pressure, pressure! Turn the bass up, selecta!', 'bm_george', 'mc', 132, 0, False),
    ('v30', 'ukg', 'Two-step, two-step, everybody move.', 'bm_lewis', 'mc', 133, 0, False),
    ('v31', 'mth', "Deep down in the basement, nobody's watching.", 'af_nicole', 'whisper', 131, 0, False),
    ('v32', 'sg', 'Strictly underground. Strictly for the heads.', 'bm_daniel', 'radio', 136, 0, False),
    ('v33', 'ch', 'Everybody, hands in the air, one more time for the organ.', 'am_fenrir', 'mc', 128, 0, False),
    ('v34', 'mth', 'Turn this shit up. Louder. Louder.', 'af_kore', 'dark', 128, -2, True),
    ('v35', 'ukg', "If you ain't sweating, what the fuck are you doing?", 'bm_george', 'mc', 132, 0, True),
    ('v36', 'sg', 'Dirty bassline, filthy little rhythm.', 'af_bella', 'dark', 130, -1, True),
]
NEW_ADLIBS = [
    # id, genre, text, voice, bpm, explicit
    ('a17', 'ukg', 'Here we go!', 'bm_george', 132, False),
    ('a18', 'ukg', 'Come on, then!', 'bm_lewis', 133, False),
    ('a19', 'gh', 'Lift it!', 'bf_emma', 128, False),
    ('a20', 'ukg', 'Oh my days!', 'bf_isabella', 132, False),
    ('a21', 'sg', 'Run it back!', 'am_michael', 134, False),
    ('a22', 'mth', 'Keep it moving.', 'af_heart', 128, False),
    ('a23', 'mth', 'Deeper.', 'af_nicole', 130, False),
    ('a24', 'sg', 'Big tune!', 'bm_george', 136, False),
    ('a25', 'mth', 'Lights off.', 'af_kore', 128, False),
    ('a26', 'ch', 'One more!', 'am_fenrir', 128, False),
    ('a27', 'ch', 'Feel that?', 'af_bella', 128, False),
    ('a28', 'gh', "Let's go!", 'af_sarah', 133, False),
    ('a29', 'ukg', 'Fucking hell!', 'bm_george', 132, True),
    ('a30', 'sg', 'Oh, shit!', 'af_bella', 134, True),
    ('a31', 'sg', "Damn, that's filthy.", 'am_onyx', 136, True),
    ('a32', 'ch', 'Work it, baby.', 'af_nicole', 128, True),
]


def spoken_save(sid, genre, text, voice, style, bpm, explicit, group, note, wet, dry, heard):
    write_src(sid, wet, dry)
    g = 'ukg' if genre == 'gh' else genre
    folder = {'adlibs': 'Vocals/Ad-libs', 'clean': 'Vocals/Clean', 'explicit': 'Vocals/Explicit'}[group]
    if genre == 'gh':
        folder += '/Garage House (Disclosure-style)'
    base = f"{'VOX-E' if explicit else 'VOX'}_{sid.upper()}_{GL[genre]}_{slug(text)[:34]}_{bpm}bpm"
    e = {'id': sid, 'cat': 'vox', 'group': group, 'genre': g, 'title': text, 'voice': DESC.get(voice, voice), 'style': style, 'explicit': explicit,
         'bpm': bpm, 'note': note, 'dur': round(len(wet) / SR, 3), 'file': f'audio/vox/{sid}.wav', 'dry': f'audio/vox_dry/{sid}.wav',
         'pack': f'{folder}/{base}.wav', 'packDry': f'{folder}/Dry/{base}_DRY.wav', 'peaks': peaks(wet, 72), 'stereo': True, 'asr': heard, 'new': True}
    if genre == 'gh':
        e['fam'] = 'gh'
    put(e)


def render_spoken(only=None):
    import vocals as V
    import vocals2 as V2
    from dsp import bfilt, board, pb
    for sid, genre, text, voice, style, bpm, pitch, explicit in NEW_SPOKEN:
        if only and sid not in only and 'spoken' not in only:
            continue
        sc, x, sp, heard = V2.best_take(text, voice)
        x44 = bfilt(V.up(x), 'highpass', 70, 2)
        dry = board(x44, [pb.PitchShift(semitones=pitch)]) if (style == 'dark' and pitch) else x44
        wet = V2.spoken_chain(x44, style, bpm, pitch, voice in V3.MALE)
        wet, dry = RV.tail_trim(wet), RV.tail_trim(V2.clean(dry, voice in V3.MALE), -65)
        wet, dry = V3.finish(wet, -13.0), V3.finish(dry, -13.0)
        note = {'mc': 'MC, delay throw', 'radio': 'pirate-radio filter', 'dark': f'pitched {pitch} st, grit + delay', 'whisper': 'whisper, wide space',
                'preacher': 'warm, close, big room', 'diva': 'clean, plate + echo'}[style] + f' · best of 3 takes ({sp:.2f}x)'
        spoken_save(sid, genre, text, voice, style, bpm, explicit, 'explicit' if explicit else 'clean', note, wet, dry, heard)
        log(f'{sid} {voice} {style} score {sc:.2f} heard: {heard}')
    for sid, genre, text, voice, bpm, explicit in NEW_ADLIBS:
        if only and sid not in only and 'adlibs' not in only:
            continue
        sc, x, sp, heard = V2.best_take(text, voice)
        x44 = bfilt(V.up(x), 'highpass', 70, 2)
        wet = RV.tail_trim(V2.spoken_chain(x44, 'adlib', bpm, 0, voice in V3.MALE))
        dry = RV.tail_trim(V2.clean(x44, voice in V3.MALE), -65)
        wet, dry = V3.finish(wet, -13.0), V3.finish(dry, -12.5)
        spoken_save(sid, genre, text, voice, 'adlib', bpm, explicit, 'adlibs', 'ad-lib, short echo', wet, dry, heard)
        log(f'{sid} {voice} adlib score {sc:.2f} heard: {heard}')


if __name__ == '__main__':
    only = sys.argv[1:] or None
    t0 = time.time()
    if only and only[0] == 'improve':          # second pass only: never re-render anything else
        improve_hooks(only[1:])
        open(f'{W}/out/render_v7.out', 'a').write('\n'.join(LOG) + '\n')
        print('done in', round(time.time() - t0), 's')
        sys.exit(0)
    if not only or 'hooks' in only or any(o in {h['id'] for h in hook_sources()} for o in only):
        render_hooks(only)
    if not only or 'new_hooks' in only or any(o in {h[0] for h in NEW_HOOKS} for o in only):
        render_new_hooks(only)
    sec = lambda name, table: not only or name in only or any(o in {r[0] for r in table} for o in only)
    if sec('chops', NEW_CHOPS):
        render_chops(only)
    import render_vocals as RV1
    if only and (any(o in ('old_chops', 'old_stabs') for o in only) or any(o in {r[0] for r in RV1.CHOPS + RV1.STABS} for o in only)):
        resing_old(only)
    if only and ('tuned' in only or any(o in {r[0] for r in RV1.TUNED} for o in only)):
        resing_tuned(only)
    if sec('pads', NEW_PADS):
        render_pads(only)
    if sec('stabs', NEW_STABS):
        render_stabs(only)
    if sec('spoken', NEW_SPOKEN) or sec('adlibs', NEW_ADLIBS):
        render_spoken(only)
    open(f'{W}/out/render_v6.log', 'a').write('\n'.join(LOG) + '\n')
    print('done in', round(time.time() - t0), 's')
