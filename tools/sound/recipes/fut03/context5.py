"""FUT03's round 5 context loops: round 4's loop (recipes/fut03/context.py) with the text-to-speech vocals gone. Every
round 4 lane plays exactly as it did (context.lane: the drums, the perc, the bass, the stab, the pad, their faders,
pans and sends); the vocal hook's place goes to the vox synth riff, and the ear candy's to the round 4 accent synths.

- ContextLoopVoxClub / ContextLoopNoVoxClub: the wet loop, with the vox synth or without it, through the shared club
  master at the genre's settings (-8.5 LUFS-I, a PLR of at least 7 dB) and round 4's declared cap, 4.5 dB
  (context.MASTER_REDUCTION_DB, with its reason).
- ContextLoopVox / ContextLoopVoxWarm, ContextLoopNoVox / ContextLoopNoVoxWarm: dry, bass A (clean) or B (warm), the B
  loop at A's loudness so the A/B is the character alone (DRY_B_LUFS).
- ContextLoopVoxWet / ContextLoopNoVoxWet: bass B (the pick) with the kit's sends: the 1.8 s plate (ducked under the
  vox synth when it plays) and the 3/16 dub ping-pong.
The low end (the kick and the bass) is round 4's, so ContextLoopLow and ContextLoopLowWarm carry on unchanged.

The arrangement (16ths 0-63, odd 16ths on the swing):
- "vox synth": the vox riff (voxsynth.PhraseVoxRiff, its notes and echoes before they wrap) in bars 3-4, where round
  4's hook answered, its dub throw at the end of bar 4 echoing into bar 1; three accents where the ear candy was: the
  glass ping on the "and" of 4 in bar 1, the Cm9 dub chord on the "and" of 4 in bar 2 (its echoes leading into the
  riff), the reversed Gm9 swelling into bar 1.
- "no vox": the same loop with no vox lane; four accents carry the top: the glass ping and the dub chord as above, the
  Gm7 organ hit on the "and" of 4 in bar 3, where the riff was, and the noise sweep into the loop's end in place of
  the reversed chord.
Accents play their kit files at their own levels (ACCENT_TRIM_DB) on one lane, high-passed at 300 Hz with presence
(ACCENT_HP, ACCENT_EQ): ear candy under the drums.
"""
import functools

import numpy as np

from tools.sound import dsp48 as D
from tools.sound import master as MS
from tools.sound import phrase as Ph
from tools.sound import qa
from tools.sound import sends as S
from tools.sound.item import Item, Out
from tools.sound.kits import fut03 as K
from tools.sound.recipes.fut03 import context as C, voxsynth as VS

SR = D.SR
L, G = C.L, C.G
SETS = ('r5',)
VARIANTS = ('vox', 'novox')
VOX_AT = C.HOOK_AT                  # 32: the riff in bars 3-4
ACCENTS = {
    'vox': ((14, 'AccentGlassPingD6'), (30, 'AccentDubChordCm9'), ('end', 'AccentReverseChordGm9')),
    'novox': ((14, 'AccentGlassPingD6'), (30, 'AccentDubChordCm9'), (46, 'AccentOrganHitGm7'),
              ('end', 'AccentNoiseSweepUp')),
}
ACCENT_TRIM_DB = {'AccentGlassPingD6': -6.0, 'AccentDubChordCm9': -2.0, 'AccentReverseChordGm9': -3.0,
                  'AccentOrganHitGm7': -5.0, 'AccentNoiseSweepUp': -4.0}
ACCENT_HP = 300.0                   # the accent lane sits on top: out of the low mids, with presence
ACCENT_EQ = ((3500.0, 4.0, 0.7),)   # (Hz, dB, Q)
OLD = ('hook', 'candy')             # round 4's TTS lanes, gone
LANES = {**{k: v for k, v in C.LANES.items() if k not in OLD},
         'vox': ('PhraseVoxRiff', -4.0, 0.0, 0.30, 0.18),
         'accent': ('the accents (ACCENTS)', -6.0, 0.0, 0.12, 0.0)}
VOCALS = ('vox',)
VOCAL_EQ = ((330.0, -2.0, 1.0), (950.0, -4.0, 1.0), (2800.0, 5.5, 0.8))   # round 4's vocal bus EQ, deeper at 1 kHz
                                                                           # and brighter: the "aah" sits on 1 kHz
VOX_MAKEUP_DB = 2.5                 # round 4's hook's
ROOMED = ('vox', 'accent', 'perc')
KEYS = {'vox': tuple(LANES), 'novox': tuple(k for k in LANES if k != 'vox')}


# ------------------------------------------------------------------ the new lanes
def vox_lane():
    """The riff's chops and echoes before they wrap, at the riff file's gain, from bar 3: its throw echoes into
    bar 1."""
    _, _, gain_db = Ph.kit_files(K, 'PhraseVoxRiff')
    y = np.zeros((L, 2))
    Ph.place(y, VS.riff_tail() * D.undb(gain_db), G.at(VOX_AT))
    return y


def accent_lane(variant):
    """The accents' files, each on its 16th (an end-aligned swell ending on the loop's downbeat), at its trim."""
    y = np.zeros((L, 2))
    for step, name in ACCENTS[variant]:
        _, files, _ = Ph.kit_files(K, name)
        x = D.st(files[(None, 1, 1)].audio) * D.undb(ACCENT_TRIM_DB[name])
        Ph.place(y, x, (L - len(x)) % L if step == 'end' else G.at(step))
    return C.eq(Ph.circular(lambda u: D.bfilt(u, ACCENT_HP, 'highpass', order=2), y), ACCENT_EQ)


@functools.lru_cache(maxsize=None)
def lane(key, variant, char=None):
    """One lane as the loop hears it: round 4's lanes exactly (context.lane), the vox through the vocal bus's
    compressor."""
    if key in C.LANES:
        return C.lane(key, char if key == 'bass' else None)
    x = vox_lane() if key == 'vox' else accent_lane(variant)
    y = C.panned(x, LANES[key][2]) * D.undb(LANES[key][1])
    if key in VOCALS:
        g, _ = MS.glue(y, **C.VOX_COMP)
        y = g * D.undb(VOX_MAKEUP_DB)
    return y


def mix(variant, char, keys=None):
    return {k: lane(k, variant, char if k == 'bass' else None) for k in (keys or KEYS[variant])}


def mixdown(lanes, ret=None):
    """Round 4's buses (context.mixdown) with the vox synth on the vocal bus and the accents on the music bus."""
    drums, _ = MS.soft_clip(sum(lanes[k] for k in C.DRUM_BUS if k in lanes), C.drum_ceiling_db(lanes), C.DRUM_KNEE_DB,
                            4)
    room = C.pump(**C.KICK_ROOM)
    roomed = lambda k: lanes[k] * (room if k in ROOMED else 1.0)  # noqa: E731
    music = sum((roomed(k) for k in lanes if k not in C.DRUM_BUS + C.BASS_BUS + VOCALS), np.zeros((L, 2)))
    if ret is not None:
        music = music + sum(ret.values())
    vocal = sum((roomed(k) for k in VOCALS if k in lanes), np.zeros((L, 2)))
    bass = sum((lanes[k] for k in C.BASS_BUS if k in lanes), np.zeros((L, 2)))
    return C.dc_clean(drums + bass + C.eq(music, C.MUSIC_EQ) + C.eq(vocal, VOCAL_EQ))


def send_bus(lanes, send):
    j = C.SENDS[send]
    return sum(lanes[k] * LANES[k][j] for k in LANES if LANES[k][j] and k in lanes)


@functools.lru_cache(maxsize=None)
def returns(variant, char):
    """Round 4's two returns over two passes, the plate ducked under the vox synth where it plays."""
    lanes = mix(variant, char)
    two = lambda fn, x: Ph.two_pass(fn, x, C.SEAM_S)  # noqa: E731
    space = two(lambda u: S.space(u, K.space_rng('send'), **C.SPACE), D.mono(send_bus(lanes, 'space')))
    if 'vox' in lanes:
        key = lanes['vox']
        space = two(lambda u: S.duck(u, np.concatenate([key, key]), **S.DUCK)[0], space)
    echo = two(lambda u: S.echo(u, K.space_rng('echo'), C.ECHO_STEPS * K.STEP, **C.ECHO),
               D.mono(send_bus(lanes, 'echo')))
    p = C.pump(**C.RETURN_PUMP)
    return {'space': space * D.undb(S.SPACE_RETURN_DB) * p, 'echo': echo * D.undb(S.ECHO_RETURN_DB) * p}


# ------------------------------------------------------------------ the report
def lane_meta(lanes, char):
    out = {}
    for k, x in lanes.items():
        src = C.LINE[char] if k == 'bass' else LANES[k][0]
        fader = C.BASS_FADER[char] if k == 'bass' else LANES[k][1]
        out[k] = {'source': src, 'fader_db': fader, 'pan': LANES[k][2],
                  **{f'{s}_send': LANES[k][j] for s, j in C.SENDS.items()}, 'lufs_before_gain': round(qa.lufs(x), 2)}
    return out


def meta(variant, lanes, char, **extra):
    arr = {**C.ARRANGEMENT, 'variant': variant, 'accents': [[s, n, ACCENT_TRIM_DB[n]] for s, n in ACCENTS[variant]]}
    del arr['hook'], arr['candy']
    if variant == 'vox':
        arr['vox'] = {'step': VOX_AT, 'item': 'PhraseVoxRiff', 'notes': [[s + VOX_AT, D.note_name(m), ln, v]
                                                                          for s, m, ln, v in VS.VOX_RIFF]}
    buses = C.bus_meta(lanes)
    buses['vocal_bus'] = {**buses['vocal_bus'], 'lanes': [k for k in VOCALS if k in lanes], 'makeup_db': VOX_MAKEUP_DB,
                          'eq': [{'hz': f, 'db': g, 'q': q} for f, g, q in VOCAL_EQ]}
    buses['accent_lane'] = {'hp_hz': ACCENT_HP, 'eq': [{'hz': f, 'db': g, 'q': q} for f, g, q in ACCENT_EQ]}
    buses['music_bus']['lanes'] = [k for k in lanes if k not in C.DRUM_BUS + C.BASS_BUS + VOCALS]
    buses['kick_room']['lanes'] = [k for k in ROOMED if k in lanes]
    return {'grid': C.GRID, 'bass': {'character': char, 'item': C.LINE[char]}, 'arrangement': arr,
            'lanes': lane_meta(lanes, char), 'insert': C.INSERT, 'buses': buses, **extra,
            'not_modelled': "the master chain (the club loops have it), the engine's other inserts"}


# ------------------------------------------------------------------ the files
def dry(variant, char):
    lanes = mix(variant, char)
    return Out(mixdown(lanes), meta=meta(variant, lanes, char))


def wet(variant):
    char = C.BASS_PICK
    lanes = mix(variant, char)
    ret = returns(variant, char)
    m = meta(variant, lanes, char, target='the target the engine must match', bass_pick=char,
             space={**C.SPACE, 'return_db': S.SPACE_RETURN_DB, 'seed': [K.SPACE_SEED, 'send'],
                    'duck_under': [k for k in VOCALS if k in lanes], 'pump': C.RETURN_PUMP},
             echo={**C.ECHO, 'delay_steps': C.ECHO_STEPS, 'delay_s': round(C.ECHO_STEPS * K.STEP, 6),
                   'return_db': S.ECHO_RETURN_DB, 'seed': [K.SPACE_SEED, 'echo'], 'pump': C.RETURN_PUMP},
             return_lufs={k: round(qa.lufs(v), 2) for k, v in ret.items()})
    return Out(mixdown(lanes, ret), meta=m)


def club(variant):
    """The wet loop through the club master at the genre's settings with round 4's declared cap."""
    w = wet(variant)
    hits = C.club_hits()
    z, report = MS.master(w.audio, MS.settings(K, max_reduction_db=C.MASTER_REDUCTION_DB), hits=hits)
    return Out(z, top_hits=hits['kick'],
               meta={**w.meta, 'master': report, 'target': 'the target the engine must match, through its master'})


def vox_club(take):
    return club('vox')


def novox_club(take):
    return club('novox')


def vox_clean(take):
    return dry('vox', 'clean')


def vox_warm(take):
    return dry('vox', 'warm')


def novox_clean(take):
    return dry('novox', 'clean')


def novox_warm(take):
    return dry('novox', 'warm')


def vox_wet(take):
    return wet('vox')


def novox_wet(take):
    return wet('novox')


ABOUT = '4-bar context loop, 128 BPM, G minor, garage-house / deep tech house'
WHAT = {'vox': 'with the vox synth riff in bars 3-4 and three accents', 'novox': 'no vox: four accents carry the top'}
DRY_B_LUFS = {'vox': -12.38, 'novox': -12.75}   # dry B at dry A's loudness, so the A/B is the character alone: at
                                                # their own peaks B reads over A here (vox -12.14 against -12.38,
                                                # no vox -12.09 against -12.75 LUFS-I), so B comes down to A, which
                                                # keeps its peak
LOOP = dict(loop=True, hp_hz=None, sets=SETS)
CLUB = dict(loop=True, hp_hz=None, sets=SETS)
FULL_MIX = {'mud': 'off', 'why': C.FULL_MIX['why'].replace('the vocals', 'the vox synth and the accents')}
CLUB_QA = {**C.CLUB_QA, 'why': C.CLUB_QA['why'].replace('the vocals', 'the vox synth and the accents')}
ITEMS = []
for _v, _name in (('vox', 'Vox'), ('novox', 'NoVox')):
    _f = {'vox': (vox_club, vox_clean, vox_warm, vox_wet),
          'novox': (novox_club, novox_clean, novox_warm, novox_wet)}[_v]
    ITEMS += [
        Item('Loops', f'ContextLoop{_name}Club', 'loop', _f[0], tags=('wet', 'club'), qa=CLUB_QA, **CLUB,
             about=f'{ABOUT}, {WHAT[_v]}: the wet loop through the club master (-8.5 LUFS-I, a PLR of at least 7 dB, '
                   f'at most {C.MASTER_REDUCTION_DB:g} dB of peak reduction)'),
        Item('Loops', f'ContextLoop{_name}', 'loop', _f[1], tags=('dry',), qa=FULL_MIX, **LOOP,
             about=f'{ABOUT}, {WHAT[_v]}: dry, bass A (clean)'),
        Item('Loops', f'ContextLoop{_name}Warm', 'loop', _f[2], tags=('dry',), qa=FULL_MIX, level=DRY_B_LUFS[_v],
             **LOOP,
             about=f'{ABOUT}, {WHAT[_v]}: dry, bass B (warm), at dry A\'s loudness'),
        Item('Loops', f'ContextLoop{_name}Wet', 'loop', _f[3], tags=('wet',), qa=FULL_MIX, **LOOP,
             about=f'{ABOUT}, {WHAT[_v]}: wet, bass B (the pick), the 1.8 s plate and the 3/16 dub ping-pong, '
                   f'unmastered; the engine\'s target'),
    ]
