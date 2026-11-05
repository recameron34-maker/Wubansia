"""FUT04's written loop for the MIDI export (tools/sound/midi.py): the round 3 context loop's drums (the ride on the
off-beats among them), the two acid lines (with their squelch ticks on the kick), the blip sequence and the drone.
Notes are (16th, MIDI note, length in 16ths, velocity 0-1, slide). The sequence's ratchets play as single notes here.
The context loop's 4-bar cutoff and resonance ride on the acid (bass.RIDES) is knob automation, not notes: the lines
are written as they play in bars 1 and 2."""
from tools.sound.midi import hits
from tools.sound.recipes.fut04 import bass as B, context as C, lead as L, pad as P


def line_notes(line):
    """A line's notes and its squelch ticks, in step order."""
    ticks = [(s, n, ln, v, False) for s, n, ln, v in line.ticks]
    return sorted([(s, n, ln, v, bool(sl)) for s, n, ln, v, sl in line.notes] + ticks)


PARTS = dict(
    drums=(hits(C.KICK, 36) + hits(C.SNARE, 38) + hits(C.HAT_CLOSED, 42) + hits(C.HAT_OPEN, 46)
           + hits(C.ANVIL, 47) + hits(C.RIDE, 51)),
    drum_map={36: 'Kick Split', 38: 'Snare Gated', 42: 'Closed Hat', 46: 'Open Hat', 47: 'Anvil', 51: 'Ride'},
    drum_note='Put the closed and open hats in one choke group: the closed hat cuts the open one; keep the ride out '
              'of it, a short ping over a noise wash that plays over them. The rumble is a 2-bar loop '
              '(Bass/Rumble.wav) that sits under all 4 bars.',
    lines={line.name: line_notes(line) for line in B.LINES},
    pick=B.PICK,
    bass_samples='Acid Clean (A) or Acid Warm (B)',
    bass_zones='E1, A1, E2',
    lead=dict(name='Modular blip sequence', sample='Mod Blip', zones='A#4, C#5, E5, G5, A#5, C#6',
              notes=[(s, n, ln, v, False) for s, n, ln, v, _ in L.PHRASE]),
    chords=dict(name='Drone Em(add9), held', sample='Drone Em Add 9',
                notes=[(0, n, 64, 0.7, False) for n in P.CHORD]),
)
