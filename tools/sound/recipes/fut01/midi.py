"""FUT01's written loop for the MIDI export (tools/sound/midi.py): the context loop's drums, the three basslines,
the pluck phrase and the pad. Notes are (16th, MIDI note, length in 16ths, velocity 0-1, slide)."""
from tools.sound.midi import hits, per_bar
from tools.sound.recipes.fut01 import basslines as B, context as C, music as M

LINE_NAMES = {'A': 'Bounce', 'B': 'Walk', 'C': 'Deep'}

PARTS = dict(
    drums=hits(C.KICK, 36) + hits(C.SNARE, 38) + per_bar(C.HAT_BAR, 42) + hits(C.PERC, 47),
    drum_map={36: 'Kick Split', 38: 'Snare Metal', 42: 'Closed Hat FM', 47: 'Alloy Hit'},
    drum_note='Snare Modal, Closed Hat Ring and Alloy Hit Short are the alternates.',
    lines={LINE_NAMES[k]: [(s, n, ln, v, bool(sl)) for s, ln, n, v, sl in line] for k, line in B.LINES.items()},
    pick=LINE_NAMES[B.PICK],
    bass_samples='Reese Clean (A) or Reese Warm (B)',
    bass_zones='F#1, A1, C#2',
    lead=dict(name='Chrome pluck phrase', sample='Chrome Pluck', zones='C4 to C6, every 3 semitones',
              notes=[(s, n, ln, v, False) for s, n, ln, v in M.PHRASE]),
    chords=dict(name='Air pad F#m9, held', sample='Air Pad F#m9',
                notes=[(0, n, 64, 0.7, False) for n in M.PAD_CHORD]),
)
