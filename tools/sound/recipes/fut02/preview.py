"""FUT02 "Orbit Relay" listening previews for round 2: loops that let a sound be heard on loop, in whole bars, in its
groove, where its master is a single file or plays without its drums (category loop, tag 'preview'). They play the
kit's files exactly as written, on the kit's grid and swing, at the context loop's faders, the way context.py plays
them; nothing here is a new sound.

- OrbitGrooveClean, OrbitGrooveWarm, RelayGrooveClean, RelayGrooveWarm: each bassline in each character over the kick
  it plays against, with the hats: the context loop's kicks and hats of bars 1-2 and the line at its bass fader, 2
  bars. The kicks are in the file (tag with_kick), so A is mud-gated and B checked under the bass-body rule in every
  gap between them: each line is checked against the kit's own kick, not only against its declared onsets.
- StarfieldPadLoop: the pad's 4-bar loop region alone, as the sampler holds it, its sweep closed at bar 1 and open at
  bar 3 (the context loop's pad lane). 4 bars.
- LaserFillPreview: a bar of the groove (kick, clap, the hats with their choke), then the laser fill over the kick on
  beats 1-3, so its dive lands on the next downbeat's kick when it loops. 2 bars.
- LaserRiserPreview: the 8-bar riser, then a bar of the groove it lands on. 9 bars.
"""
import numpy as np

from tools.sound import dsp48 as D
from tools.sound import phrase as Ph
from tools.sound.item import Item, Out
from tools.sound.kits import fut02 as K
from tools.sound.recipes.fut02 import bass, context as C

GROOVE_STEPS = 32                            # the basslines' 2 bars
FILL_STEPS = 32
RISER_STEPS = 144                            # the riser's 8 bars, then one bar of the groove
GROOVE_KICK = ((0, 1.0), (4, 1.0), (8, 1.0), (12, 1.0))   # a bar of the context loop's kicks, every beat at full
GROOVE_CLAP = ((4, 1.0), (12, 1.0))
RISER_FADER = -6.0                           # dB: its last bar comes up to the groove's level (the drums at the
                                             # context loop's faders)


def groove_bar(at):
    """One bar of the context loop's groove from 16th `at`: kicks, claps, and the closed and open hats of its bar 1."""
    kick = tuple((at + s, v) for s, v in GROOVE_KICK)
    clap = tuple((at + s, v) for s, v in GROOVE_CLAP)
    closed = tuple((at + s, v) for s, v in C.CLOSED_BAR)
    opened = tuple((at + s, v) for s, v in C.OPEN_BAR)
    return kick, clap, closed, opened


def fader(lane, x):
    """A lane at the context loop's fader and pan."""
    return C.centred(x, C.LANES[lane][2]) * D.undb(C.LANES[lane][1])


def drums(grid, kick, clap, closed, opened):
    return (fader('kick', Ph.hits_lane(K, 'KickSplit', kick, grid))
            + fader('clap', Ph.hits_lane(K, 'ElectroClap', clap, grid))
            + fader('hat', C.hat_lane(grid, closed, opened)))


def kit_file(name):
    return Ph.kit_files(K, name)[1][(None, 1, 1)].audio


def place_to_end(grid, name, at):
    """A file that ends on a downbeat (cut_end), from 16th `at`; it must end exactly on the loop's end."""
    x = kit_file(name)
    if grid.at(at) + len(x) != grid.length:
        raise ValueError(f'{name} does not end on the preview\'s downbeat')
    buf = np.zeros((grid.length, 2))
    Ph.place(buf, x, grid.at(at))
    return buf


def line_file(line, char):
    """A bassline's 2-bar loop file, in character char."""
    return kit_file(f"{line}Line{'Clean' if char == C.CLEAN else 'Warm'}")


def groove_low(line, char):
    """A bassline's low end in its groove: the context loop's kicks of bars 1-2 and the line, at the context loop's
    faders. Returns (audio, the kick onsets)."""
    G = Ph.Grid(K, GROOVE_STEPS)
    kick = tuple((s, v) for s, v in C.KICK if s < GROOVE_STEPS)
    y = fader('kick', Ph.hits_lane(K, 'KickSplit', kick, G)) + C.faded('bass', line_file(line, char), char)
    return y, tuple(G.at(s) for s, _ in kick)


def groove(line, char):
    """The line's low end and the context loop's hats of bars 1-2 (the open hats choked as there)."""
    G = Ph.Grid(K, GROOVE_STEPS)
    low, hits = groove_low(line, char)
    closed = tuple(h for h in C.HAT_CLOSED if h[0] < GROOVE_STEPS)
    opened = tuple(h for h in C.HAT_OPEN if h[0] < GROOVE_STEPS)
    y = low + fader('hat', C.hat_lane(G, closed, opened))
    meta = {'grid': {'bpm': K.BPM, 'steps': GROOVE_STEPS, 'swing': K.SWING}, 'line': line, 'bass': char,
            'kick': [list(h) for h in C.KICK if h[0] < GROOVE_STEPS],
            'faders_db': {'kick': C.LANES['kick'][1], 'bass': C.BASS_FADER[char], 'hat': C.LANES['hat'][1]}}
    return Out(y, hits=hits, meta=meta)


def orbit_groove_clean(take):
    return groove('Orbit', C.CLEAN)


def orbit_groove_warm(take):
    return groove('Orbit', C.WARM)


def relay_groove_clean(take):
    return groove('Relay', C.CLEAN)


def relay_groove_warm(take):
    return groove('Relay', C.WARM)


def pad_loop(take):
    return Out(C.pad_lane(), meta={'grid': {'bpm': K.BPM, 'steps': C.STEPS}, 'source': 'StarfieldPadDm9'})


def fill_preview(take):
    G = Ph.Grid(K, FILL_STEPS)
    kick, clap, closed, opened = groove_bar(0)
    kick += ((16, 1.0), (20, 1.0), (24, 1.0))                  # the fill bar: no kick on beat 4, where it dives
    y = drums(G, kick, clap, closed, opened) + fader('fill', place_to_end(G, 'LaserFill', 16))
    meta = {'grid': {'bpm': K.BPM, 'steps': FILL_STEPS, 'swing': K.SWING}, 'kick': list(kick), 'fill_step': 16}
    return Out(y, meta=meta)


def riser_preview(take):
    G = Ph.Grid(K, RISER_STEPS)
    y = D.undb(RISER_FADER) * place_to_end(Ph.Grid(K, 128), 'LaserRiser', 0)
    y = np.concatenate([y, np.zeros((G.length - len(y), 2))])
    y = y + drums(G, *groove_bar(128))
    meta = {'grid': {'bpm': K.BPM, 'steps': RISER_STEPS, 'swing': K.SWING}, 'riser_bars': 8, 'groove_bars': 1}
    return Out(y, meta=meta)


WITH_KICK = {'mud': 'off',
             'why': 'a listening preview: its kick is gated as KickSplit, every other part passed its own low cut'}
GROOVE_QA = {C.CLEAN: {'mud': 'gate', 'why': 'a line A with its kick in the file: gated in every gap between kicks'},
             C.WARM: {'mud': 'bass-body', 'why': f'{bass.WHY_WARM}: a line B with its kick in the file, in every gap'}}
GROOVES = (('OrbitGrooveClean', orbit_groove_clean, 'Orbit', C.CLEAN),
           ('OrbitGrooveWarm', orbit_groove_warm, 'Orbit', C.WARM),
           ('RelayGrooveClean', relay_groove_clean, 'Relay', C.CLEAN),
           ('RelayGrooveWarm', relay_groove_warm, 'Relay', C.WARM))
ITEMS = [
    Item('Loops', name, 'loop', fn, loop=True, sets=('r2',), tags=('preview', 'with_kick'), qa=GROOVE_QA[char],
         about=f"preview, 2 bars at 130 BPM: the {line} line, {'A, clean' if char == C.CLEAN else 'B, warm'}, over the "
               f"kit kick (every beat) and the hats, at the context loop's faders")
    for name, fn, line, char in GROOVES
] + [
    Item('Loops', 'StarfieldPadLoop', 'loop', pad_loop, loop=True, sets=('r2',), tags=('preview',),
         qa={'mud': 'lowcut', 'why': 'a pad loop: the low cut, like the pad'},
         about='preview: the starfield pad Dm9 held, its 4-bar loop alone (the sweep closed at bar 1, open at bar 3)'),
    Item('Loops', 'LaserFillPreview', 'loop', fill_preview, loop=True, sets=('r2',), tags=('preview',), qa=WITH_KICK,
         about='preview, 2 bars at 130 BPM: a bar of kick, clap and hats, then the laser fill landing on the downbeat'),
    Item('Loops', 'LaserRiserPreview', 'loop', riser_preview, loop=True, sets=('r2',), tags=('preview',),
         qa=WITH_KICK, about='preview, 9 bars at 130 BPM: the 8-bar laser riser, then the bar of groove it lands on'),
]
