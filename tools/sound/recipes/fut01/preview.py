"""FUT01 previews for listening round 2: a few files made only to hear a sound the way it is used, each on the kit's
own files exactly as written (phrase.kit_files) and on the kit's grid. They are not kit sounds (tag preview).

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


def hat_pattern(name):
    return Out(Ph.hits_lane(K, name, HATS, G2), meta={'steps': 32, 'bpm': K.BPM, 'swing': K.SWING,
                                                      'hits': [[s, v] for s, v in HATS]})


def hat_pattern_fm(take):
    return hat_pattern('ClosedHatFM')


def hat_pattern_ring(take):
    return hat_pattern('ClosedHatRing')


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
ITEMS = [
    Item('Loops', 'HatPatternFM', 'loop', hat_pattern_fm, loop=True, level=HAT_LUFS, sets=('r2',), tags=('preview',),
         qa=LOWCUT, about="preview: closed hat A (FM) on the context loop's hat part, 2 bars, 134 BPM, swing 0.57"),
    Item('Loops', 'HatPatternRing', 'loop', hat_pattern_ring, loop=True, level=HAT_LUFS, sets=('r2',),
         tags=('preview',), qa=LOWCUT,
         about="preview: closed hat B (Ring) on the context loop's hat part, 2 bars, 134 BPM, swing 0.57"),
    Item('Loops', 'AirPadHold', 'loop', pad_hold, loop=True, sets=('r2',), tags=('preview',), qa=LOWCUT,
         about='preview: the air pad (F#m9) held for 4 bars at 134 BPM, looping over its seam'),
    Item('FX', 'ShiftRiserDrop', 'loop', riser_drop, sets=('r2',), tags=('preview',),
         qa={'mud': 'off', 'why': 'a preview: the riser into the drop; the riser and the bassline loop pass their own '
                                  'low-end rules'},
         about="preview: the 8-bar riser landing on the drop, bassline B (the Walk) with kick A and hat A for 2 bars"),
]
