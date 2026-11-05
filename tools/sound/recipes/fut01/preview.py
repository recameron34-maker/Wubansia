"""FUT01 previews for listening rounds 2 and 3: a few files made only to hear a sound the way it is used, each on the
kit's own files exactly as written (phrase.kit_files) and on the kit's grid. They are not kit sounds (tag preview).

- DrumGrooveSplit, DrumGroovePunch (round 3): the kick A/B in a groove. Each plays bars 1 and 2 of the context loop's
  drums (the kick, snare C, hat A and the shaker, at the loop's balance) with kick A (Split) or kick B (Punch), each
  kick lane through the loop's own kick clip (context.kick_clip, KICK_CLIP_DB under its peak: their files peak at the
  same level, so both take the same clip) and both files at one loudness (DRUM_LUFS), so the A/B is the kick alone,
  as the loops play it: Punch's shorter sub and its 100-150 Hz knock.
- DrumGrooveSwing61 (round 3): the standard's optional shuffle A/B. DrumGroovePunch with every odd 16th on swing 0.61
  instead of the kit's 0.57 (24.6 ms late instead of 15.7), at the same loudness. The drums alone: the vocals, the
  pluck phrase and the basslines are rendered on the kit's swing, so a heavier pick would move the kit's DNA
  (K.SWING) and re-render them.

- HatPatternFM, HatPatternRing: the two closed hats are 0.12 and 0.45 s, too short to judge one hit at a time, so each
  plays the context loop's hat part for 2 bars (off-beats with ghosts, the extra ghost in bar 2), on the kit's swing,
  each velocity on its nearest layer and the round robins in turn: the same part on both, at the same loudness
  (HAT_LUFS), so the A/B is the hat alone.
- AirPadHold: the pad held for 4 bars as the sampler plays it (its 4 s loop running), crossfaded over the seam as in the
  context loop, so it loops in whole bars without its 1.5 s attack.
- ShiftRiserDrop: the 8-bar riser as written, then where it lands: the picked bassline (B, the Walk, in the wet
  loop's character) for 2 bars and the next downbeat, fading out. The riser is end-aligned (cut_end), so its last
  sample is the downbeat the drop starts on; it is turned down so its last bar sits 2 LU under the drop.
"""
import numpy as np

from tools.sound import dsp48 as D
from tools.sound import phrase as Ph
from tools.sound import qa
from tools.sound.item import Item, Out
from tools.sound.kits import fut01 as K
from tools.sound.recipes.fut01 import basslines as B
from tools.sound.recipes.fut01 import context as C

SR = D.SR
G2 = Ph.Grid(K, 32)                 # 2 bars of 16ths on the kit's swing
HATS = tuple((s, v) for s, v in C.HAT if s < 32)   # the context loop's hat part, bars 1 and 2
DROP_BASS = f'Bassline{B.PICK}{C.WET_BASS.capitalize()}'   # the drop: the picked line in the wet loop's character
DROP_END = (0.20, 600.0, 0.25)      # the next downbeat after the 2 bars: held 0.20 s, then down 600 dB per second (an
                                    # exponential fade the tail check reads as a decay), rendered for 0.25 s more
HAT_LUFS = -25.0                    # both hat patterns at one loudness, so the A/B is by sound: hat A's natural level
                                    # with its peak at the ceiling (-24.9)
BAR = int(round(16 * K.STEP * SR))  # one bar, samples
RISER_UNDER_DROP = 2.0              # the riser's last bar this many LU under the drop's 2 bars (LUFS-I), so it lands on
                                    # a drop
DRUM_LUFS = -16.5                   # the drum grooves at one loudness, each under the ceiling (through the kick clip,
                                    # A, Split, peaks 0.5 dB higher for its loudness)
GROOVE_LANES = ('snare', 'hat', 'shaker')   # the context loop's drums besides the kick, at its faders and pans
HEAVY_SWING = 0.61                  # the heavier shuffle to A/B (the standard: 0.60-0.62 against the kit's 0.57)


def hat_pattern(name):
    return Out(Ph.hits_lane(K, name, HATS, G2), meta={'steps': 32, 'bpm': K.BPM, 'swing': K.SWING,
                                                      'hits': [[s, v] for s, v in HATS]})


def hat_pattern_fm(take):
    return hat_pattern('ClosedHatFM')


def hat_pattern_ring(take):
    return hat_pattern('ClosedHatRing')


def groove_grid(swing):
    """The 2-bar grid with its odd 16ths on `swing` instead of the kit's."""
    g = Ph.Grid(K, 32)
    g.swing = swing
    return g


def drum_groove(kick, grid=G2):
    """Bars 1 and 2 of the context loop's drums with this kick on the grid (the kit's swing unless given), its lane
    through the loop's kick clip (context.kick_clip); the other drums at the loop's faders and pans, each against the
    kick's fader there (kick B's)."""
    two = lambda part: tuple((s, v) for s, v in part if s < 32)
    buf = C.kick_clip(Ph.hits_lane(K, kick, two(C.KICK), grid))
    crests = C.kick_crests(buf, C.full_kicks(grid))
    parts = {'snare': C.SNARE, 'hat': C.HAT, 'shaker': C.SHAKER}
    for k in GROOVE_LANES:
        lane = Ph.hits_lane(K, C.LANES[k][0], two(parts[k]), grid)
        buf = buf + Ph.pan(lane, C.LANES[k][2]) * D.undb(C.LANES[k][1] - C.LANES['kick'][1])
    return Out(buf, meta={'steps': 32, 'bpm': K.BPM, 'swing': grid.swing, 'kick': kick, 'kick_clip_db': C.KICK_CLIP_DB,
                          'kick_crest_db_full_kicks': crests, 'lanes': {k: C.LANES[k][0] for k in GROOVE_LANES}})


def drum_groove_split(take):
    return drum_groove('KickSplit')


def drum_groove_punch(take):
    return drum_groove('KickPunch')


def drum_groove_swing61(take):
    return drum_groove('KickPunch', groove_grid(HEAVY_SWING))


def pad_hold(take):
    return Out(C.pad_lane(), meta={'bars': 4, 'bpm': K.BPM, 'samples': C.L, 'seam_hold_s': C.SEAM_HOLD})


def riser_drop(take):
    """The riser, then the drop: the bassline loop (kick A, hat A, the picked line) once, then its first downbeat
    again, faded, so it ends on the downbeat after the 2 bars. The riser is set so its last bar sits RISER_UNDER_DROP
    under the drop (both files are at their own gains: as written the riser's last bar is 4.6 LU over the bassline
    loop, which then sounds like a breakdown). The join is logged as a splice."""
    riser = Ph.kit_files(K, 'ShiftRiser')[1][(None, 1, 1)].audio
    loop = Ph.kit_files(K, DROP_BASS)[1][(None, 1, 1)].audio
    gain_db = qa.lufs(loop) - RISER_UNDER_DROP - qa.lufs(riser[-BAR:])
    hold, rate, more = DROP_END
    end = int((hold + more) * SR)
    fade = D.undb(-rate * np.maximum(np.arange(end) / SR - hold, 0.0))
    last = loop[:end] * fade[:, None]
    meta = {'riser_samples': len(riser), 'drop': DROP_BASS, 'drop_at': len(riser), 'drop_bars': 2, 'bpm': K.BPM,
            'riser_gain_db': round(float(gain_db), 2), 'riser_last_bar_under_drop_lu': RISER_UNDER_DROP}
    return Out(np.concatenate([riser * D.undb(gain_db), loop, last]), splices=(len(riser),), meta=meta)


LOWCUT = {'mud': 'lowcut', 'why': 'a preview of a sound under the low cut: the low cut, like the sound'}
DRUMS = {'mud': 'off', 'why': 'a preview: the drums as the loop plays them; the kicks pass the mud gate and the snare, '
                              'hat and shaker the low cut, each on its own'}
ITEMS = [
    Item('Loops', 'DrumGrooveSplit', 'loop', drum_groove_split, loop=True, level=DRUM_LUFS, sets=('r3',),
         tags=('preview', 'with_kick'), qa=DRUMS,
         about="preview: kick A (Split) in bars 1-2 of the context loop's drums (snare C, hat A, shaker), 134 BPM"),
    Item('Loops', 'DrumGroovePunch', 'loop', drum_groove_punch, loop=True, level=DRUM_LUFS, sets=('r3',),
         tags=('preview', 'with_kick'), qa=DRUMS,
         about="preview: kick B (Punch) in bars 1-2 of the context loop's drums (snare C, hat A, shaker), 134 BPM"),
    Item('Loops', 'DrumGrooveSwing61', 'loop', drum_groove_swing61, loop=True, level=DRUM_LUFS, sets=('r3',),
         tags=('preview', 'with_kick'), qa=DRUMS,
         about="preview: DrumGroovePunch's drums with every odd 16th on swing 0.61 instead of the kit's 0.57, 134 BPM"),
    Item('Loops', 'HatPatternFM', 'loop', hat_pattern_fm, loop=True, level=HAT_LUFS, sets=('r2', 'r3'),
         tags=('preview',), qa=LOWCUT,
         about="preview: closed hat A (FM) on the context loop's hat part, 2 bars, 134 BPM, swing 0.57"),
    Item('Loops', 'HatPatternRing', 'loop', hat_pattern_ring, loop=True, level=HAT_LUFS, sets=('r2', 'r3'),
         tags=('preview',), qa=LOWCUT,
         about="preview: closed hat B (Ring) on the context loop's hat part, 2 bars, 134 BPM, swing 0.57"),
    Item('Loops', 'AirPadHold', 'loop', pad_hold, loop=True, sets=('r2', 'r3'), tags=('preview',), qa=LOWCUT,
         about='preview: the air pad (F#m9) held for 4 bars at 134 BPM, looping over its seam'),
    Item('FX', 'ShiftRiserDrop', 'loop', riser_drop, sets=('r2', 'r3'), tags=('preview',),
         qa={'mud': 'off', 'why': 'a preview: the riser into the drop; the riser and the bassline loop pass their own '
                                  'low-end rules'},
         about="preview: the 8-bar riser landing on the drop, bassline B (the Walk) with kick A and hat A for 2 bars"),
]
