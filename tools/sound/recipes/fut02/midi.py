"""FUT02's written loop for the MIDI export (tools/sound/midi.py): the context loop's drums, the two basslines, the
arp phrase and the pad. Notes are (16th, MIDI note, length in 16ths, velocity 0-1, slide)."""
from tools.sound.midi import hits, per_bar
from tools.sound.recipes.fut02 import bass as B, context as C, lead as L, pad as P

PARTS = dict(
    drums=(hits(C.KICK, 36) + hits(C.CLAP, 38) + per_bar(C.OPEN_BAR, 46)
           + per_bar(C.CLOSED_BAR, 42, extra=C.CLOSED_VARY, drop=C.CLOSED_DROP) + hits(C.ZAP, 47)),
    drum_map={36: 'Kick Split', 38: 'Electro Clap', 42: 'Closed Hat', 46: 'Open Hat', 47: 'Zap'},
    drum_note='Put the closed and open hats in one choke group: the closed hat cuts the open one.',
    lines={k: [(s, n, ln, v, bool(sl)) for s, n, ln, v, sl in line] for k, line in B.LINES.items()},
    pick=B.PICK,
    bass_samples='Roll Bass Clean (A) or Roll Bass Warm (B)',
    bass_zones='D1, A1, D2',
    lead=dict(name='Chrome arp phrase', sample='Chrome Arp', zones='D4, G4, C5, F5, A#5',
              notes=[(s, n, ln, v, False) for s, n, ln, v in L.phrase_notes()]),
    chords=dict(name='Starfield pad Dm9, held', sample='Starfield Pad Dm9',
                notes=[(0, n, 64, 0.7, False) for n in P.CHORD]),
)
