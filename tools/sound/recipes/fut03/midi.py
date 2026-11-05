"""FUT03's written loop for the MIDI export (tools/sound/midi.py): the context loop's drums (round 3: with the shaker's
16ths) and stab hits (round 3: voiced from D3), the two basslines and the pluck phrase. Notes are (16th, MIDI note,
length in 16ths, velocity 0-1, slide)."""
from tools.sound.midi import hits
from tools.sound.recipes.fut03 import bass as B, context as C, lead as L, pad as P

PLING = {79: 47, 74: 48}            # the pling's two zones in the loop, G5 and D5, on two drum notes
SHAKER = 54                         # GM's tambourine: GM has no shaker inside 35-59, where the plan keeps every
                                    # drum and perc note (plan section 2.6), as the generator's shaker lanes do

PARTS = dict(
    drums=(hits(C.KICK, 36) + hits(C.CLAP, 38)
           + [(s, 42 if kind == 'closed' else 46, 0.5, v, False) for s, kind, v in C.HATS]
           + hits(C.SHAKER, SHAKER)
           + [(s, PLING[n], 0.5, v, False) for s, n, v in C.PERC]),
    drum_map={36: 'Kick Split', 38: 'Clap', 42: 'Closed Hat', 46: 'Open Hat', 47: 'Metal Pling G5', 48: 'Metal Pling D5',
              SHAKER: 'Shaker'},
    drum_note='Put the closed and open hats in one choke group: the closed hat cuts the open one.',
    lines={k: [(s, n, ln, v, bool(sl)) for s, ln, n, v, sl in line] for k, line in B.LINES.items()},
    pick=B.PICK,
    bass_samples='Dip Bass Clean (A) or Dip Bass Warm (B)',
    bass_zones='G1, D2, G2',
    lead=dict(name='Glass pluck phrase', sample='Glass Pluck', zones='D4, G4, A#4, D5, G5, A#5',
              notes=[(s, n, ln, v, False) for s, n, ln, v in L.PHRASE]),
    chords=dict(name='Supersaw stab Gm9, pumped hits', sample='Stab Gm9',
                notes=[(s, n, 1.5, v, False) for s, v in C.STAB for n in P.STAB_CHORD]),
)
