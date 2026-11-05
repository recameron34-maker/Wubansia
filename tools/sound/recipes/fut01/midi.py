"""FUT01's written loop for the MIDI export (tools/sound/midi.py): the round 4 context loop's drums (context4.py: the
2-step kick, the garage clap, the swung closed hats, the open hats, the tambourine on every 16th, the rim ghosts and
the alloy hit), the three warp-bass lines (the 2-step bounce, the organ bass's low line and the 4x4 speed garage
line), the chrome pluck phrase, the organ stabs, and the four synth phrases (riffs.py). Notes are (16th, MIDI note,
length in 16ths, velocity 0-1, slide); a pitch dip is written as two slides, down and back."""
from tools.sound.midi import hits
from tools.sound.recipes.fut01 import context4 as C, keys as Ky, music as M, riffs as R

TAMBOURINE = 54                     # GM's tambourine
RIM = 37                            # GM's side stick
LINE_NAMES = {'garage': 'Bounce', 'organ': 'Low', 'speed': 'Speed'}


def line(notes):
    """A context4 line, (16th, length, note, velocity, slide), as MIDI notes: a slide or a dip is a slide."""
    return [(s, n, ln, v, bool(sl)) for s, ln, n, v, sl in notes]


PARTS = dict(
    drums=(hits(C.KICK, 36) + hits(C.CLAP, 38) + hits(C.HAT_C, 42) + hits(C.HAT_O, 46) + hits(C.TAMB, TAMBOURINE)
           + hits(C.RIM, RIM) + hits(C.PERC, 47)),
    drum_map={36: 'Kick Garage', RIM: 'Rim Shot', 38: 'Clap Garage', 42: 'Hat Closed', 46: 'Hat Open',
              47: 'Alloy Hit', TAMBOURINE: 'Tambourine'},
    drum_lanes={RIM: 'perc'},
    drum_note=("Kick Split (kick A, round 1's pick) and the Shaker (for the tambourine) are the alternates. Choke Hat "
               'Open with Hat Closed (one choke group), as the loop plays them.'),
    lines={LINE_NAMES['garage']: line(C.WARP_LINE), LINE_NAMES['organ']: line(C.ORGAN_LINE),
           LINE_NAMES['speed']: line(C.SPEED_LINE)},
    pick=LINE_NAMES['garage'],
    switch_line=LINE_NAMES['organ'],
    drop2_line=LINE_NAMES['organ'],
    bass_samples='Warp Clean (A) or Warp Warm (B), or Organ Bass for the Low line',
    bass_zones='F#1, A1, C#2',
    lead=dict(name='Chrome pluck phrase', sample='Chrome Pluck', zones='C4 to C6, every 3 semitones',
              notes=[(s, n, ln, v, False) for s, n, ln, v in M.PHRASE]),
    chords=dict(name='Organ stabs, F#m9 and Bm9', sample='Organ Stab',
                notes=[(s, z + d, 1, v, False) for s, z, v in C.STABS for d in Ky.SHAPE],
                trigger=[(s, z, 1, v, False) for s, z, v in C.STABS], zones='F#3, B3'),   # a file per chord
    phrases=[
        dict(name='Organ riff', sample='Organ Lead', zones='C4 to C6, every 3 semitones', notes=list(R.ORGAN_RIFF),
             bars=4),
        dict(name='Warp riff', sample='Warp Clean', zones='F#1, A1, C#2', notes=line(R.WARP_RIFF), bars=2),
        dict(name='Pluck riff', sample='Chrome Pluck', zones='C4 to C6, every 3 semitones', notes=list(R.PLUCK_RIFF),
             bars=4),
        dict(name='E-piano stab rhythm', sample='E Piano Stab', zones='F#3, B3', notes=R.stab_notes(), bars=2),
    ],
)
