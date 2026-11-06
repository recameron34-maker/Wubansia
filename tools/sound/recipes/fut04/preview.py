"""FUT04 "Furnace Code" listening previews for rounds 2 and 3: loops that let a sound be heard on loop, in whole bars,
where its master is a single file that isn't one, or a sound made to sit under the kick (category loop, tag 'preview').
They play the kit's files exactly as written, on the kit's grid and swing, at the context loop's faders and through its
drum bus (round 3), the way context.py plays them; nothing here is a new sound.

- DroneLoop: the drone's 4-bar loop region alone, as the sampler holds it, its filter darkest at bar 1 and most open at
  bar 3 (the context loop's pad lane). 4 bars.
- RumbleUnderKick: the rumble under the kick it is made for, four on the floor at full velocity, both at the context
  loop's faders (the kick out of the drum bus), so its swell after each kick is heard against the hit. 2 bars,
  mud-gated in every gap (with_kick).
- StutterFillPreview: a bar of the groove (kick, snare, the hats with their choke and, round 3, the ride), then the
  stutter fill over the kick on beats 1-3 and the snare on 2 and 4, as in the context loop's bar 4, so its 64th roll
  lands on the next downbeat's kick when it loops. 2 bars.
- IronRiserPreview: the 8-bar riser, then the bar it lands on: the groove and the first bar of the picked acid line
  (AcidLine1, in the wet loop's character, its sub sidechained to the kicks as in the context loop). 9 bars.
"""
import numpy as np

from tools.sound import dsp48 as D
from tools.sound import phrase as Ph
from tools.sound.item import Item, Out
from tools.sound.kits import fut04 as K
from tools.sound.recipes.fut04 import bass
from tools.sound.recipes.fut04 import context as C

FILL_STEPS = 32
RISER_STEPS = 144                         # the riser's 8 bars, then one bar of the groove
RUMBLE_STEPS = bass.LOOP_STEPS            # the rumble's 2 bars
GROOVE_KICK = tuple((s, 1.0) for s in (0, 4, 8, 12))
GROOVE_SNARE = ((4, 1.0), (12, 0.8))
RISER_FADER = -4.0                        # dB: its last bar comes up to about the groove bar's loudness (the drums
                                          # and bass at the context loop's faders)


def bar_hats(at, upto=16):
    """The context loop's bar 1 hats (closed, open) and ride, moved to 16th `at`, up to `upto` 16ths into the bar."""
    closed = tuple((at + s, v) for s, v in C.HAT_CLOSED if s < upto)
    opened = tuple((at + s, v) for s, v in C.HAT_OPEN if s < upto)
    ride = tuple((at + s, v) for s, v in C.RIDE if s < upto)
    return closed, opened, ride


def groove_bar(at):
    """One bar of the context loop's groove from 16th `at`: kicks, snares, and the closed and open hats and the ride of
    its bar 1."""
    kick = tuple((at + s, v) for s, v in GROOVE_KICK)
    snare = tuple((at + s, v) for s, v in GROOVE_SNARE)
    return (kick, snare) + bar_hats(at)


def fader(lane, x):
    """A lane at the context loop's fader and pan."""
    return C.centred(x, C.LANES[lane][1]) * D.undb(C.fader_db(lane))


def drum_lanes(grid, kick, snare, closed, opened, ride):
    """The groove's drum lanes at the context loop's faders, by lane."""
    return {'kick': fader('kick', Ph.hits_lane(K, 'KickSplit', kick, grid)),
            'snare': fader('snare', Ph.hits_lane(K, 'SnareGated', snare, grid)),
            'hat': fader('hat', C.hat_lane(grid, closed, opened)),
            'ride': fader('ride', Ph.hits_lane(K, 'Ride', ride, grid))}


def drums(grid, kick, snare, closed, opened, ride, extra=None):
    """The groove's drums (and any extra drum-bus lanes, such as the fill) through the context loop's drum bus."""
    mix = {**drum_lanes(grid, kick, snare, closed, opened, ride), **(extra or {})}
    return C.drum_bus(mix, tuple(mix))


def place_to_end(grid, name, at):
    """A file that ends on a downbeat (cut_end), from 16th `at`; it must end exactly on the loop's end."""
    x = C.kit_file(name).audio
    if grid.at(at) + len(x) != grid.length:
        raise ValueError(f'{name} does not end on the preview\'s downbeat')
    buf = np.zeros((grid.length, 2))
    Ph.place(buf, x, grid.at(at))
    return buf


def grid_meta(steps):
    return {'bpm': K.BPM, 'steps': steps, 'swing': K.SWING}


def drone_loop(take):
    return Out(C.pad_lane(), meta={'grid': grid_meta(C.STEPS), 'source': 'DroneEmAdd9',
                                   'fader_db': 'none: the file as written'})


def rumble_under_kick(take):
    G = Ph.Grid(K, RUMBLE_STEPS)
    kick = tuple((s, 1.0) for s in range(0, RUMBLE_STEPS, 4))
    rumble = C.kit_file('Rumble').audio
    if len(rumble) != G.length:
        raise ValueError('the rumble is not 2 bars')
    k = fader('kick', Ph.hits_lane(K, 'KickSplit', kick, G))
    y = C.drum_bus({'kick': k}, ('kick',)) + fader('rumble', rumble)
    meta = {'grid': grid_meta(RUMBLE_STEPS), 'kick': [list(h) for h in kick], 'drum_bus': 'the kick alone through it',
            'fader_db': {k: round(C.fader_db(k), 2) for k in ('kick', 'rumble')}}
    return Out(y, hits=tuple(G.at(s) for s, _ in kick), meta=meta)


def fill_preview(take):
    G = Ph.Grid(K, FILL_STEPS)
    kick, snare, closed, opened, ride = groove_bar(0)
    kick += ((16, 1.0), (20, 1.0), (24, 1.0))                  # the fill bar: no kick on beat 4, under the 64th roll
    snare += ((20, 1.0), (28, 0.8))
    more_closed, more_open, more_ride = bar_hats(16, upto=4)    # the fill bar keeps its first beat of hats
    y = drums(G, kick, snare, closed + more_closed, opened + more_open, ride + more_ride,
              {'fill': fader('fill', place_to_end(G, 'StutterFill', 16))})
    meta = {'grid': grid_meta(FILL_STEPS), 'kick': [list(h) for h in kick], 'snare': [list(h) for h in snare],
            'ride': [list(h) for h in ride + more_ride], 'fill_step': 16, 'drum_bus': 'the context loop\'s'}
    return Out(y, meta=meta)


def riser_preview(take):
    G = Ph.Grid(K, RISER_STEPS)
    y = D.undb(RISER_FADER) * place_to_end(Ph.Grid(K, 128), 'IronRiser', 0)
    y = np.concatenate([y, np.zeros((G.length - len(y), 2))])
    y = y + drums(G, *groove_bar(128))
    line = D.st(C.kit_file(C.line_name(C.WET_BASS)).audio)[:G.length - G.at(128)]   # the line's first bar
    land = np.zeros((G.length, 2))
    land[G.at(128):] = line
    kicks = [G.at(s) for s, _ in groove_bar(128)[0]]
    y = y + C.sidechained(fader('bass', land), kicks)   # the wet loop's character (fader_db's default), its sub ducked
                                                        # under the landing bar's kicks, as the context loop ducks it
    meta = {'grid': grid_meta(RISER_STEPS), 'riser_bars': 8, 'groove_bars': 1, 'riser_fader_db': RISER_FADER,
            'bass': C.line_name(C.WET_BASS), 'bass_sidechain': C.BASS_SIDECHAIN}
    return Out(y, meta=meta)


R23 = ('r2', 'r3', 'r5')            # round 5 keeps them as they are
PREVIEW_MIX = 'a listening preview: its kick is gated as KickSplit, '
FILL_PREVIEW_QA = {'mud': 'off', 'why': PREVIEW_MIX + 'every other part passed its own low cut'}
RISER_PREVIEW_QA = {'mud': 'off', 'why': PREVIEW_MIX + 'its bass as its own line (AcidLine1Warm), every other part '
                                                       'passed its own low cut'}
ITEMS = [
    Item('Loops', 'DroneLoop', 'loop', drone_loop, loop=True, sets=R23, tags=('preview',),
         qa={'mud': 'lowcut', 'why': 'a pad loop: the low cut, like the drone'},
         about='preview: the Em(add9) drone held, its 4-bar loop alone (the filter darkest at bar 1, most open at '
               'bar 3)'),
    Item('Loops', 'RumbleUnderKick', 'loop', rumble_under_kick, loop=True, sets=R23, tags=('preview', 'with_kick'),
         qa={'mud': 'gate', 'why': 'the rumble under its kick, as in the context loop: gated in every gap'},
         about='preview, 2 bars at 136 BPM: the kick-fed rumble under the kick it is made for, at the context loop\'s '
               'faders'),
    Item('Loops', 'StutterFillPreview', 'loop', fill_preview, loop=True, sets=R23, tags=('preview',),
         qa=FILL_PREVIEW_QA,
         about='preview, 2 bars at 136 BPM: a bar of kick, snare, hats and ride, then the stutter fill landing on the '
               'downbeat'),
    Item('Loops', 'IronRiserPreview', 'loop', riser_preview, loop=True, sets=R23, tags=('preview',),
         qa=RISER_PREVIEW_QA,
         about='preview, 9 bars at 136 BPM: the 8-bar iron riser, then the bar of groove and acid line it lands on'),
]
