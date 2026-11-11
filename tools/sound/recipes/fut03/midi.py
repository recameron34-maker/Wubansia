"""FUT03's written music for the MIDI export (tools/sound/midi.py, MIDI v2; the schema: tools/sound/README.md, "MIDI").

- The loop (Loop_Vox, Loop_NoVox): the round 5 context loop (context5.py on context.py) lane by lane: the deep kick on
  every beat, the clap and snare, the shuffled hats, the shaker's 16ths, the congas, bongos and rims; the Rubber line
  twice; the dub stab off the kicks on G3 (its root: Gm9), its low-pass ride (CC74); Pad Warm held under all 4 bars
  (channel 11); the vox synth riff in bars 3-4; the variant's accents. The loop has no lead and no fill.
- The fill bar (the extended mix's, at every Fill cue): the kit's FillPreview (context.FILL_*): kick, hats and shaker on
  beats 1-3, the clap on beat 2, no congas, bongos or rim, beat 4 empty for the Build Fill (audit A, defect 7).
- The parts: the two rubber basslines, the keys riff (the lead), the dub stab and its Gm9 voicing, and the synth
  phrases: the rubber riff, the dub chords (Gm9 on G3, Cm9 on C4: the same file a fourth up), the keys arp and the vox
  synth riff.
- The extended mix: the house form (midi.HOUSE_FORM) until the kit engineer writes FUT03's own (TODO below).
Notes are (16th, MIDI note, length in 16ths, velocity 0-1, slide).
"""
import math

from tools.sound import channels as CH
from tools.sound.midi import Hit, Key, Note, hits, shifted
from tools.sound.recipes.fut03 import accents as A, context as C, context5 as C5, rubber as B, voxsynth as VS

SHAKER = 54                         # GM's tambourine: GM has no shaker inside 35-59, where the map keeps every drum and
                                    # perc note, as the generator's shaker lanes do
RIM = 37                            # GM's side stick
HAND = {('Conga', 55): 41, ('Conga', 60): 43, ('Conga', 62): 45, ('Bongo', 67): 47, ('Bongo', 74): 48}
KEYS_ZONES = 'G3, D4, G4, D5, G5'
CHORD_ROOT = {A.GM9: 55, A.CM9: 60}  # the Gm9 sample played on G3 is Gm9; on C4, a fourth up, it is the Cm9 voicing


def notes(rows):
    return [(s, m, ln, v, False) for s, m, ln, v in rows]


def glided(rows):
    """A rubber line, (16th, length, note, velocity, glide), as MIDI notes. A glide inside a note, (target, where,
    seconds), becomes the note up to `where` of its length, then its target slid into from it for the rest (as FUT01
    writes its dips): a mono synth set to legato glides there (rubber.py's own glide time is the synth's)."""
    out = []
    for s, ln, n, v, glide in rows:
        if glide is None:
            out.append((s, n, ln, v, False))
            continue
        target, where, _ = glide
        out += [(s, n, ln * where, v, False), (s + ln * where, target, ln * (1 - where), v, True)]
    return out


def drums(kick, clap, hats, shaker, perc):
    """Drum notes from context.py's parts: (16th, velocity) hits, the hats' (16th, kind, velocity), the perc's (16th,
    name, zone, velocity)."""
    return (hits(kick, 36) + hits(clap, 38) + [Note(s, 42 if kind == 'closed' else 46, 0.5, v) for s, kind, v in hats]
            + hits(shaker, SHAKER) + [Note(s, RIM if name == 'Rimshot' else HAND[(name, z)], 0.5, v)
                                      for s, name, z, v in perc])


LOOP_DRUMS = drums(C.KICK, C.CLAP, C.HATS, C.SHAKER, C.PERC)
PREVIEW = drums(C.FILL_KICK, C.FILL_CLAP, C.FILL_HATS, C.FILL_SHAKER, C.FILL_PERC)   # 2 bars: the groove, the fill bar
FILL_BLOCK = [n for n in LOOP_DRUMS if n.step < 48] + shifted([n for n in PREVIEW if n.step >= 16], 32)
PART = {36: 'kick', 38: 'snare', 42: 'hats', 46: 'hats'}


def stab_ride():
    """The dub stab's low-pass ride as CC74 every 60 ticks over the 4 bars, from context.stab_lane: closed at 900 Hz on
    the loop's downbeat, open to 5 kHz at its middle, a raised cosine in log frequency; on channels.cc_value's law over
    900-5000 Hz. Its Q is fixed (0.9): no CC71."""
    lo, hi = C.STAB_RIDE
    return [CH.cc_value(lo * (hi / lo) ** (0.5 - 0.5 * math.cos(2 * math.pi * i / 256)), lo, hi) for i in range(256)]


def loop_accents(variant):
    """context5's accents for a variant as hits ('end': the file ends on the loop's downbeat)."""
    return [Hit(64, name, 1, 'end') if step == 'end' else Hit(step, name, 1) for step, name in C5.ACCENTS[variant]]


PARTS = dict(
    drums=dict(
        map={36: 'KickDeep', RIM: 'Rimshot', 38: 'ClapSnare', 41: 'Conga', 42: 'ClosedHatDeep', 43: 'Conga',
             45: 'Conga', 46: 'OpenHatDeep', 47: 'Bongo', 48: 'Bongo', SHAKER: 'ShakerDeep'},
        zones={key: zone for (_, zone), key in HAND.items()},
        lanes={RIM: 'perc'},
        note=('Put the closed and open hats in one choke group: the closed hat cuts the open one. The congas and '
              'bongos are zoned samples (Conga G3 C4 D4, Bongo G4 D5): map each drum note to its zone.'),
        # TODO(FUT03 engineer): busy, turn (audit B: a conga run D4 C4 G3 on 16ths 12-15 in bar 8 of each line that
        # doesn't end on a fill; the open hat at 1.0 on the 'and' of 4 in bars 4 and 8) and variants (the bongo D5
        # pair only in lines 2 and 4, the rim on 16ths 10 and 14 in the drops)
    ),
    fill={part: [n for n in FILL_BLOCK if PART.get(n.key, 'perc') == part]
          for part in ('kick', 'snare', 'hats', 'perc')},
    bass=dict(
        items=('RubberBassClean', 'RubberBassWarm'),
        lines={k: glided(line) for k, line in B.LINES.items()},
        pick=B.PICK,
        note=('Sub Deep layers under either bass (the manifest layers it, no notes). F1 (29) plays the G1 zone pitched '
              'down. A glide inside a note is written as the note, then its target slid into from it.'),
    ),
    lead=dict(item='KeysEP', name='Keys riff', notes=notes(A.KEYS_RIFF)),
    chords=dict(items=('DubStabGm9',), name='Dub stab Gm9, off the kicks',
                trigger=[(s, 55, 1.5, v, False) for s, v in C.STAB],
                voicing=[(s, n, 1.5, v, False) for s, v in C.STAB for n in A.GM9]),
    pad=dict(on='Pad Warm', blocks={'main': [(0, 55, 64, 1.0, False)]}),
    vox=dict(item='VoxSynth', riff=notes(VS.VOX_RIFF)),
    accents=(Key('AccentGlassPingD6'), Key('AccentDubChordCm9'), Key('AccentReverseChordGm9'), Key('AccentOrganHitGm7'),
             Key('AccentNoiseSweepUp'), Key('AccentPluckChordDm7'), Key('AccentZapG4'), Key('AccentVoxSynthGm'),
             Key('AccentRubberStabG1')),
    fx=(Key('BuildFill'), Key('Riser'), Key('Impact')),
    chops=(Key('VoxChopOohAhG4'), Key('VoxChopOohAhC5'), Key('VoxChopAhD5'), Key('VoxChopStackGm7')),
    extras={'Pad Warm': 'PadWarm'},
    frames={'AccentGlassPingD6': 76021, 'AccentDubChordCm9': 109332, 'AccentReverseChordGm9': 105600,
            'AccentOrganHitGm7': 21731, 'AccentNoiseSweepUp': 45000, 'AccentPluckChordDm7': 69172,
            'AccentZapG4': 17407, 'AccentVoxSynthGm': 28594, 'AccentRubberStabG1': 18656, 'BuildFill': 90000,
            'Riser': 720000, 'Impact': 177720, 'VoxChopOohAhG4': 93859, 'VoxChopOohAhC5': 84309,
            'VoxChopAhD5': 88282, 'VoxChopStackGm7': 93951},
    cues=dict(riser='Riser', fill='BuildFill', impact='Impact', swell='AccentReverseChordGm9',
              uplift='AccentNoiseSweepUp', stab='AccentDubChordCm9'),
    loop=dict(
        drums=LOOP_DRUMS,
        lead=[],
        vox=[Note(s + C5.VOX_AT, m, ln, v) for s, m, ln, v in VS.VOX_RIFF],
        accents={v: loop_accents(v) for v in ('vox', 'novox')},
        extras={'Pad Warm': [(0, 55, 64, 1.0, False)]},
    ),
    phrases=[
        dict(name='Rubber riff', file='RubberRiff', on='bass', item='RubberBassClean',
             notes=glided(B.PHRASES['Riff']), bars=2),
        dict(name='Dub chords, Gm9 Gm9 Cm9 Gm9', file='DubChords', on='chords', item='DubStabGm9',
             notes=[(16 * b + s, CHORD_ROOT[chord], 1.5, v, False) for b, chord in enumerate(A.DUB_BARS)
                    for s, v in A.DUB_RHYTHM], bars=4),
        dict(name='Keys arp', file='KeysArp', on='lead', item='KeysEP', notes=notes(A.ARP), bars=4),
        dict(name='Vox synth riff, Gm9 to Cm9', file='VoxSynthRiff', on='vox', item='VoxSynth',
             notes=notes(VS.VOX_RIFF), bars=2),
    ],
    rides=[dict(on='Chords', cc=74, law='log', lo=round(C.STAB_RIDE[0]), hi=round(C.STAB_RIDE[1]), unit='Hz',
                what="the dub stab's low-pass: closed on the loop's downbeat, open at its middle",
                values=stab_ride())],
    recorded={'bass': 'the Rubber and Deep lines (`Loops/Line Rubber Clean.wav` and the rest), as the loop plays them',
              'vox': 'the Phrase Vox Riff (`Loops/Phrase Vox Riff.wav`), its dub throw baked in'},
    # TODO(FUT03 engineer): form=(...) (audit B, FUT03): Gm9 | Cm9 wherever the vox plays (chords blocks: the
    # trigger G3 -> C4, CHORD_ROOT; the voicing A.CM9), the pad under both breaks (C4 for the Cm9 bars: pad blocks),
    # the keys' call while the vox plays, the Rubber riff as the switch-up's bass (phrases), drop 2 on Deep with the
    # keys arp, the roll over bars 93-95 only with the new fill bar in 96 and the impact on 97.
)
