"""FUT03's written loop for the MIDI export (tools/sound/midi.py), round 4: the context loop's drums and perc (the deep
kick, the clap and snare, the shuffled hats, the shaker's 16ths, the congas, bongos and rims), the two rubber basslines,
the keys riff as the lead, the dub stab's Gm9 hits as the chords, and the synth phrases (recipes/fut03/accents.py).
Notes are (16th, MIDI note, length in 16ths, velocity 0-1, slide)."""
from tools.sound.midi import hits
from tools.sound.recipes.fut03 import accents as A, context as C, rubber as B

SHAKER = 54                         # GM's tambourine: GM has no shaker inside 35-59, where the plan keeps every
                                    # drum and perc note (plan section 2.6), as the generator's shaker lanes do
RIM = 37                            # GM's side stick
HAND = {('Conga', 55): 41, ('Conga', 60): 43, ('Conga', 62): 45, ('Bongo', 67): 47, ('Bongo', 74): 48}
HAND_NAMES = {41: 'Conga G3', 43: 'Conga C4', 45: 'Conga D4', 47: 'Bongo G4', 48: 'Bongo D5'}
KEYS_ZONES = 'G3, D4, G4, D5, G5'
CHORD_ROOT = {A.GM9: 55, A.CM9: 60}  # the Gm9 sample played on G3 is Gm9; on C4, a fourth up, it is the Cm9 voicing


def notes(rows):
    return [(s, m, ln, v, False) for s, m, ln, v in rows]


PARTS = dict(
    drums=(hits(C.KICK, 36) + hits(C.CLAP, 38)
           + [(s, 42 if kind == 'closed' else 46, 0.5, v, False) for s, kind, v in C.HATS]
           + hits(C.SHAKER, SHAKER)
           + [(s, RIM if name == 'Rimshot' else HAND[(name, z)], 0.5, v, False) for s, name, z, v in C.PERC]),
    drum_map={36: 'Kick Deep', 38: 'Clap Snare', 42: 'Closed Hat Deep', 46: 'Open Hat Deep', SHAKER: 'Shaker Deep',
              RIM: 'Rimshot', **HAND_NAMES},
    drum_lanes={RIM: 'perc'},
    drum_note=('Put the closed and open hats in one choke group: the closed hat cuts the open one. The congas and bongos '
               'are zoned samples (Conga G3 C4 D4, Bongo G4 D5): map each drum note to its zone.'),
    lines={k: [(s, n, ln, v, bool(sl)) for s, ln, n, v, sl in line] for k, line in B.LINES.items()},
    pick=B.PICK,
    bass_samples='Rubber Bass Clean (A) or Rubber Bass Warm (B), with Sub Deep under either',
    bass_zones='G1, D2, G2',
    lead=dict(name='Keys riff', sample='Keys EP', zones=KEYS_ZONES, notes=notes(A.KEYS_RIFF)),
    chords=dict(name='Dub stab Gm9, off the kicks', sample='Dub Stab Gm9',
                notes=[(s, n, 1.5, v, False) for s, v in C.STAB for n in A.GM9]),
    phrases=[
        dict(name='Rubber riff', sample='Rubber Bass Clean', zones='G1, D2, G2',
             notes=[(s, n, ln, v, bool(sl)) for s, ln, n, v, sl in B.PHRASES['Riff']], bars=2),
        dict(name='Dub chords, Gm9 Gm9 Cm9 Gm9', sample='Dub Stab Gm9', zones='G3 (C4 plays it a fourth up: Cm9)',
             notes=[(16 * b + s, CHORD_ROOT[chord], 1.5, v, False) for b, chord in enumerate(A.DUB_BARS)
                    for s, v in A.DUB_RHYTHM], bars=4),
        dict(name='Keys arp', sample='Keys EP', zones=KEYS_ZONES, notes=notes(A.ARP), bars=4),
    ],
)
