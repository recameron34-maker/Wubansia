"""FUT04 round 5 context loops (set r5): round 3's context loop (context.py) with the vox synth riff in place of the
text-to-speech vocals, and the same loop instrumental, both featuring the round 5 synth accents (accents.py).

Every lane but the vocals is round 3's, as context.py mixes it (context.mixed: the same files, faders, pans, the bass's
sidechain under the kicks), through the same buses (the drum bus's grit and clip, the pump, the music bus's EQ), the
same three sends at the kit's settings (Space, the dark 5 s hall; Echo, the dub ping-pong; the resonator on the iron
ratios) and, for the club loops, the same club master at the techno target and the kit's 6 dB cap (context.MASTER).
What changes:
- the vox lane (with the vox synth): the riff (voxsynth.RIFF, 2 bars) on VoxSynth's files in bars 1-2, its bar 1 again
  in bar 3 with its throw moved to the "and" of 4 (D4 on step 46, thrown into the fill bar, as round 3 threw its last
  chop), nothing in the fill bar itself; its fader at VOX_LUFS. Each throw (steps 30 and 46) takes the full Space and
  Echo send (context.THROW). As round 3's vocals did, it keys the bass's and the fill's top bands (context.DIPS) and
  the Space return's duck;
- the accent lane: the accents placed as a record places its ear candy (ACCENTS), each set to its own loudness
  (ACCENT_LUFS: at its hit 4-9 LU under the drums), sent to the hall and the echo (ACCENT_SENDS):
  - with the vox synth: the dark rave stab on the "and" of 4 in bar 1 (step 14, where the riff rests), the industrial
    noise sweep through the fill bar and the reverse hit swelling over its last 3 beats, both ending on the loop's
    downbeat (into bar 1);
  - without it ("no vox"): the rave stab on step 14, the zap on bar 2's "and" of 4 and the metal chord hit on bar 3's
    (steps 30 and 46, the vox's two throws, thrown the same way), and the reverse hit into bar 1: the accents carry the
    top.

The items (tags as round 3's): ContextLoopClubVox and ContextLoopClubNoVox, the wet loops through the club master;
ContextLoopWetVox and ContextLoopWetNoVox, the wet loops (bass B, warm, round 3's pick for the wet loop); ContextLoopVox
and ContextLoopVoxWarm, the dry loop with the vox synth on bass A and B. The low-end loops are round 3's
(ContextLoopLow, ContextLoopLowWarm: the kick, bass and rumble lanes, which round 5 leaves as they were). Nothing here
needs the vocal models.
"""
import functools

import numpy as np

from tools.sound import dsp48 as D
from tools.sound import master as MS
from tools.sound import phrase as Ph
from tools.sound import qa
from tools.sound import sends as S
from tools.sound.item import Item, Out
from tools.sound.kits import fut04 as K
from tools.sound.recipes.fut04 import context as C, voxsynth as VS

SR = D.SR
L, G = C.L, C.G
VOX, NOVOX = 'vox', 'novox'
BASE = ('kick', 'bass', 'rumble', 'snare', 'hat', 'ride', 'perc', 'pad', 'lead', 'fill')   # round 3's lanes, as mixed

# ------------------------------------------------------------------ the vox lane
VOX_NOTES = (tuple(VS.RIFF) + tuple((s + 32, m, ln, v) for s, m, ln, v in VS.RIFF if s < 16)
             + ((46, 62, 2.0, 1.0),))     # bars 1-2 the riff; bar 3 its first bar and D4 on the "and" of 4, thrown
VOX_THROWS = (VS.THROW_STEP, 46)
VOX_LUFS = -21.0                          # the vox lane over the loop (round 3: the chops -19.5, the robot -20)
VOX_SENDS = (0.35, 0.25, 0.10)            # Space, Echo, resonator; a throw: context.THROW (full Space and Echo)

# ------------------------------------------------------------------ the accent lane
# (16th, or 'end' to end on the loop's downbeat, item)
ACCENTS = {
    VOX: ((14, 'AccentRaveStabEm'), ('end', 'AccentNoiseSweepE'), ('end', 'AccentReverseHitEm')),
    NOVOX: ((14, 'AccentRaveStabEm'), (30, 'AccentZapE5'), (46, 'AccentMetalChordEm'), ('end', 'AccentReverseHitEm')),
}
ACCENT_THROWS = {VOX: (), NOVOX: (30, 46)}   # thrown as the vox's throws, with the full Space and Echo send
ACCENT_LUFS = {'AccentRaveStabEm': -22.0, 'AccentZapE5': -24.0, 'AccentMetalChordEm': -26.5,
               'AccentNoiseSweepE': -27.0, 'AccentReverseHitEm': -25.5}   # each file's own LUFS-I in the loop: at its
                                          # hit, 4-9 LU under the drum bus (over 500 ms), its peak 3 dB or more under
                                          # the kick's
ACCENT_SENDS = (0.30, 0.20, 0.0)

LANES5 = {**{k: C.LANES[k] for k in BASE}, 'vox': ('VoxSynth, the riff', 0.0, *VOX_SENDS),
          'accent': ('the accents', 0.0, *ACCENT_SENDS)}
MUSIC_BUS = ('pad', 'lead', 'vox', 'accent')   # and the returns, in the wet loops


def accent_lane(mode):
    """The accents on the grid, each at its own loudness: (the lane, the thrown hits alone)."""
    buf, thrown = np.zeros((L, 2)), np.zeros((L, 2))
    for at, name in ACCENTS[mode]:
        x = C.kit_file(name).audio
        g = D.undb(ACCENT_LUFS[name] - qa.lufs(x))
        pos = L - len(x) if at == 'end' else G.at(at)
        Ph.place(thrown if at in ACCENT_THROWS[mode] else buf, x * g, pos)
    return buf + thrown, thrown


@functools.lru_cache(maxsize=None)
def vox_lane():
    """The vox lane after its fader: (the lane, the thrown stabs alone)."""
    dry, thrown = VS.riff_parts(G, VOX_NOTES, VOX_THROWS)
    g = D.undb(VOX_LUFS - qa.lufs(dry + thrown))
    return (dry + thrown) * g, thrown * g


def lane_keys(mode):
    """The loop's lanes: round 3's (BASE), the vox lane with the vox synth only, and the accents."""
    return BASE + (('vox',) if mode == VOX else ()) + ('accent',)


@functools.lru_cache(maxsize=None)
def lanes(mode, char):
    """Every lane as the loop hears it, and the thrown parts (vox, accents): round 3's lanes (context.mixed), the vox
    lane keying the bass's and fill's dips (context.DIPS) as the vocals did, and the accents."""
    mix = {k: C.mixed(k, char) for k in BASE}
    vox_throw = np.zeros((L, 2))
    if mode == VOX:
        mix['vox'], vox_throw = vox_lane()
        for k, hz in C.DIPS.items():
            mix[k] = C.dipped(mix[k], mix['vox'], hz)
    mix['accent'], acc_throw = accent_lane(mode)
    if tuple(mix) != lane_keys(mode):
        raise ValueError(f'the {mode} loop mixes {tuple(mix)}, not {lane_keys(mode)}')
    return mix, vox_throw, acc_throw


def mixdown(mix, ret=None):
    """The lanes through round 3's buses (context.drum_bus, the pump, context.bass_bus, context.music_eq), plus the
    returns when given."""
    drums = C.drum_bus(mix)
    duck = lambda k: C.pump(C.PUMP) if C.PUMP and k in C.PUMPED else 1.0  # noqa: E731
    music = sum(mix[k] * duck(k) for k in MUSIC_BUS if k in mix)
    if ret is not None:
        music = music + sum(ret.values()) * (C.pump(C.RETURN_PUMP) if C.RETURN_PUMP else 1.0)
    return drums + C.bass_bus(mix) + mix['rumble'] + C.music_eq(music)


def send_bus(mix, throws, send):
    """What a send hears: every lane at its send level, the thrown parts at the throw's (context.THROW) in place of
    their lane's."""
    j = C.SENDS[send]
    bus = sum(mix[k] * LANES5[k][j] for k in mix if LANES5[k][j])
    for part, lane in throws:
        if j < 4:
            bus = bus + part * (C.THROW[j - 2] - LANES5[lane][j])
    return bus


@functools.lru_cache(maxsize=None)
def returns(mode, char):
    """The three returns at the kit's settings (context.SPACE, ECHO, the resonator), each over two passes; the Space
    return ducks under the vox lane (round 3: under the vocals), if there is one."""
    mix, vox_throw, acc_throw = lanes(mode, char)
    throws = ((vox_throw, 'vox'), (acc_throw, 'accent'))
    two_pass = lambda fn, x: Ph.two_pass(fn, x, C.SEAM_S)  # noqa: E731
    space = two_pass(lambda u: S.space(u, K.space_rng('send'), **C.SPACE), D.mono(send_bus(mix, throws, 'space')))
    if mode == VOX:
        key = np.concatenate([mix['vox'], mix['vox']])
        space = two_pass(lambda u: S.duck(u, key, **S.DUCK)[0], space)
    delay = S.ECHO_STEPS * K.STEP
    echo = two_pass(lambda u: S.echo(u, K.space_rng('echo'), delay, **C.ECHO), D.mono(send_bus(mix, throws, 'echo')))
    res = two_pass(lambda u: S.resonator(u, C.resonator_freqs(), **S.RESONATOR),
                   D.mono(send_bus(mix, throws, 'resonator')))
    return {'space': space * D.undb(S.SPACE_RETURN_DB), 'echo': echo * D.undb(S.ECHO_RETURN_DB),
            'resonator': res * D.undb(S.RESONATOR_RETURN_DB)}


def meta(mode, char, mix):
    lanes_meta = {}
    for k in mix:
        lanes_meta[k] = {'source': C.line_name(char) if k == 'bass' else LANES5[k][0], 'pan': LANES5[k][1],
                         **{f'{s}_send': LANES5[k][j] for s, j in C.SENDS.items()},
                         'lufs': round(qa.lufs(mix[k]), 2)}
    out = {'grid': C.GRID, 'bass': char, 'mode': mode, 'lanes': lanes_meta,
           'accents': [{'at': at, 'item': name, 'lufs': ACCENT_LUFS[name], 'thrown': at in ACCENT_THROWS[mode]}
                       for at, name in ACCENTS[mode]],
           'round3': 'every lane but the vocals as context.py mixes it, through its buses (context.bus_meta)'}
    if mode == VOX:
        out['vox'] = {'item': 'VoxSynth', 'lufs': VOX_LUFS, 'throws': list(VOX_THROWS),
                      'notes': [[s, D.note_name(m), ln, v] for s, m, ln, v in VOX_NOTES]}
    return out


def dry(mode, char):
    mix, _, _ = lanes(mode, char)
    return Out(mixdown(mix), meta=meta(mode, char, mix))


@functools.lru_cache(maxsize=None)
def wet(mode):
    char = C.WET_BASS
    mix, _, _ = lanes(mode, char)
    ret = returns(mode, char)
    m = {**meta(mode, char, mix), 'space': C.SPACE, 'echo': C.ECHO,
         'return_lufs': {k: round(qa.lufs(v), 2) for k, v in ret.items()}}
    return mixdown(mix, ret), m


def club(mode):
    y, m = wet(mode)
    hits = C.club_hits()
    z, report = MS.master(y, MS.settings(K, **C.MASTER), hits=hits)
    return Out(z, top_hits=hits['kick'], meta={**m, 'target': 'the club master the engine must match (S1e)',
                                               'master': report})


def club_vox(take):
    return club(VOX)


def club_novox(take):
    return club(NOVOX)


def wet_vox(take):
    y, m = wet(VOX)
    return Out(y, meta=m)


def wet_novox(take):
    y, m = wet(NOVOX)
    return Out(y, meta=m)


def dry_vox(take):
    return dry(VOX, C.CLEAN)


def dry_vox_warm(take):
    return dry(VOX, C.WARM)


R5 = ('r5',)
ABOUT = '4-bar context loop, 136 BPM, E minor, bassline AcidLine1 (the rolling hook), round 5'
FULL_MIX = {'mud': 'off',
            'why': 'the full mix: the vox synth, snare, drone and anvil belong in 150-400 Hz; its low end is checked '
                   'as ContextLoopLow (the gate) and ContextLoopLowWarm (bass-body), and every other lane passed its '
                   'own low cut'}
CLUB_QA = {**C.CLUB_QA,
           'why': FULL_MIX['why'] + ". The 125 and 250 Hz windows are exempt, every other band stays gated: round 3's "
                  "reason (context.CLUB_QA), the same low end and buses. The master's cap is "
                  f"{C.MASTER['max_reduction_db']:g} dB, over the standard's 4, for your decision by ear, and the "
                  "crest the master may take from the kicks and snares is declared at 2.4 dB, as round 3's"}
ITEMS = [
    Item('Loops', 'ContextLoopClubVox', 'loop', club_vox, loop=True, hp_hz=None, sets=R5,
         tags=('wet', 'club'), qa=CLUB_QA,
         about=f'{ABOUT}, with the vox synth riff and the accents, wet, through the club master at the techno target '
               '(-8 LUFS)'),
    Item('Loops', 'ContextLoopClubNoVox', 'loop', club_novox, loop=True, hp_hz=None, sets=R5,
         tags=('wet', 'club'), qa=CLUB_QA,
         about=f'{ABOUT}, instrumental, the accents carrying the top, wet, through the club master at the techno '
               'target (-8 LUFS)'),
    Item('Loops', 'ContextLoopWetVox', 'loop', wet_vox, loop=True, level=C.LOOP_LUFS, sets=R5,
         tags=('wet',), qa=FULL_MIX,
         about=f'{ABOUT}, with the vox synth riff and the accents, wet, bass B (warm), unmastered'),
    Item('Loops', 'ContextLoopWetNoVox', 'loop', wet_novox, loop=True, level=C.LOOP_LUFS, sets=R5,
         tags=('wet',), qa=FULL_MIX,
         about=f'{ABOUT}, instrumental, the accents carrying the top, wet, bass B (warm), unmastered'),
    Item('Loops', 'ContextLoopVox', 'loop', dry_vox, loop=True, level=C.LOOP_LUFS, sets=R5, tags=('dry',),
         qa=FULL_MIX, about=f'{ABOUT}, with the vox synth riff and the accents, dry, bass A (clean)'),
    Item('Loops', 'ContextLoopVoxWarm', 'loop', dry_vox_warm, loop=True, level=C.LOOP_LUFS, sets=R5,
         tags=('dry',), qa=FULL_MIX, about=f'{ABOUT}, with the vox synth riff and the accents, dry, bass B (warm)'),
]
