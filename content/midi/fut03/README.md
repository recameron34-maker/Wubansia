# FUT03 "Glass Current": the written music as MIDI

128 BPM, G minor (6A), swing 56% written in: every odd 16th lands 29 ticks
late. Made by `tools/sound/midi.py` from the kit's recipes (`tools/sound/recipes/fut03/midi.py`) on the channel
map in `docs/midi-map.md`; a test keeps these files equal to a fresh export.

Every file is format 1 at 960 ticks a quarter, in 4/4, with the tempo (468750 microseconds a quarter, the
integer nearest 128 BPM) and the key signature (FF 59: 2 flats, minor) on its first track.

## The files

| File | What it plays | Channels |
|---|---|---|
| `FUT03_Accents_NoVox_4bars.mid` | the loop's accents, no vox | channel 6 |
| `FUT03_Accents_Vox_4bars.mid` | the loop's accents, with the vox synth | channel 6 |
| `FUT03_Bassline_Deep_2bars.mid` | the Deep line on Bass | channel 1 |
| `FUT03_Bassline_Rubber_InTheLoop_2bars.mid` | the Rubber line (the loop's) on Bass | channel 1 |
| `FUT03_Chords_DubStabGm9_4bars.mid` | the chord sample, one note per chord at its root, on Dub Stab Gm9 | channel 4 |
| `FUT03_Chords_DubStabGm9_Voicing_4bars.mid` | the chord voicing, single notes for your own synth | channel 3 |
| `FUT03_Drums_4bars.mid` | the loop's drums and percussion, its fill bar included | channel 10 |
| `FUT03_Lead_KeysEP_2bars.mid` | the lead phrase, on Keys EP | channel 2 |
| `FUT03_Loop_NoVox_4bars.mid` | the context loop as its club master plays it, lane by lane: no vox, the accents carry the top | channels 1, 3, 4, 6, 10, 11 |
| `FUT03_Loop_Vox_4bars.mid` | the context loop as its club master plays it, lane by lane: with the vox synth | channels 1, 3, 4, 5, 6, 10, 11 |
| `FUT03_Phrase_DubChords_4bars.mid` | Dub chords, Gm9 Gm9 Cm9 Gm9, on Dub Stab Gm9 | channel 4 |
| `FUT03_Phrase_KeysArp_4bars.mid` | Keys arp, on Keys EP | channel 2 |
| `FUT03_Phrase_RubberRiff_2bars.mid` | Rubber riff, on Rubber Bass Clean | channel 1 |
| `FUT03_Phrase_VoxSynthRiff_2bars.mid` | Vox synth riff, Gm9 to Cm9, on Vox Synth | channel 5 |

The extended mix: `extended/vox/` (with the vox synth) and `extended/novox/` (without), each a 224-bar mix in the
genre's form, one region file per section, `regions.json` and a README.

## Drums (channel 10)

A drum rack with the kit's samples on these notes (C1 = 36): 120-tick triggers, so play them as one-shots.

| Note | Sample | Track |
|---|---|---|
| 36 (C1) | Kick Deep | Kick |
| 37 (C#1) | Rimshot | Perc |
| 38 (D1) | Clap Snare | Clap |
| 41 (F1) | Conga | Perc |
| 42 (F#1) | Closed Hat Deep | Hats |
| 43 (G1) | Conga | Perc |
| 45 (A1) | Conga | Perc |
| 46 (A#1) | Open Hat Deep | Hats |
| 47 (B1) | Bongo | Perc |
| 48 (C2) | Bongo | Perc |
| 54 (F#2) | Shaker Deep | Perc |

Put the closed and open hats in one choke group: the closed hat cuts the open one. The congas and bongos are zoned samples (Conga G3 C4 D4, Bongo G4 D5): map each drum note to its zone.

## Pitched instruments

Root each sample file at its MIDI number: the sample names count C4 = 60, the note Ableton and Logic call C3. A note
plays its zone's file repitched by the distance to its root.

| Channel | Track | Instrument | Zones: name (MIDI number) | Velocity |
|---|---|---|---|---|
| 1 | Bass | Rubber Bass Clean (A) or Rubber Bass Warm (B) | G1 (31), D2 (38), G2 (43) | layers at velocity 76, 127 (a velocity plays the nearest, the louder on a tie) |
| 2 | Lead | Keys EP | G3 (55), D4 (62), G4 (67), D5 (74), G5 (79) | layers at velocity 64, 127 (a velocity plays the nearest, the louder on a tie) |
| 4 | Chords | Dub Stab Gm9 | G3 (55) | layers at velocity 64, 95, 127 (a velocity plays the nearest, the louder on a tie) |
| 5 | Vox | Vox Synth | G3 (55), D4 (62), G4 (67), D5 (74) | layers at velocity 76, 127 (a velocity plays the nearest, the louder on a tie) |
| 11 | Pad Warm | Pad Warm | G3 (55) | one layer |

On channel 3, Chord voicing is the chords as single notes for your own synth; Wubansia ignores channel 3.
Sub Deep layers under either bass (the manifest layers it, no notes). F1 (29) plays the G1 zone pitched down. A glide inside a note is written as the note, then its target slid into from it.

## One-key sounds (channels 6, 7 and 8)

Each key plays one sound at its own pitch, from 48 up. A note lasts its sample's length (or the gate the loop gives
it), so a gated pad plays the whole file; an end-aligned sound's note ends on the beat it lands on.

| Key | Channel | Sound | File (in `content/samples/fut03/`) | How it plays |
|---|---|---|---|---|
| 48 | 6 | Accent Glass Ping D6 | `Drums/Accent Glass Ping D6.wav` | starts on its note; one layer |
| 49 | 6 | Accent Dub Chord Cm9 | `Music/Accent Dub Chord Cm9.wav` | starts on its note; one layer |
| 50 | 6 | Accent Reverse Chord Gm9 | `FX/Accent Reverse Chord Gm9.wav` | end-aligned: ends on its beat; one layer |
| 51 | 6 | Accent Organ Hit Gm7 | `Music/Accent Organ Hit Gm7.wav` | starts on its note; one layer |
| 52 | 6 | Accent Noise Sweep Up | `FX/Accent Noise Sweep Up.wav` | end-aligned: ends on its beat; one layer |
| 53 | 6 | Accent Pluck Chord Dm7 | `Music/Accent Pluck Chord Dm7.wav` | starts on its note; one layer |
| 54 | 6 | Accent Zap G4 | `Drums/Accent Zap G4.wav` | starts on its note; one layer |
| 55 | 6 | Accent Vox Synth Gm | `Music/Accent Vox Synth Gm.wav` | starts on its note; one layer |
| 56 | 6 | Accent Rubber Stab G1 | `Bass/Accent Rubber Stab G1.wav` | starts on its note; one layer |
| 48 | 7 | Build Fill | `FX/Build Fill.wav` | end-aligned: ends on its beat; one layer |
| 49 | 7 | Riser | `FX/Riser.wav` | end-aligned: ends on its beat; one layer |
| 50 | 7 | Impact | `FX/Impact.wav` | starts on its note; one layer |
| 48 | 8 | Vox Chop Ooh Ah G4 | `Vox/Vox Chop Ooh Ah G4.wav` | starts on its note; one layer |
| 49 | 8 | Vox Chop Ooh Ah C5 | `Vox/Vox Chop Ooh Ah C5.wav` | starts on its note; one layer |
| 50 | 8 | Vox Chop Ah D5 | `Vox/Vox Chop Ah D5.wav` | starts on its note; one layer |
| 51 | 8 | Vox Chop Stack Gm7 | `Vox/Vox Chop Stack Gm7.wav` | starts on its note; one layer |

## Controllers

Filter rides, every 60 ticks while they move (CC74 cutoff, CC71 resonance):
- CC74 on channel 4 (Chords): the dub stab's low-pass: closed on the loop's downbeat, open at its middle; logarithmic, 0 = 900 Hz to 127 = 5000 Hz.

Throws: a thrown note has CC91 (Space) and CC94 (Echo) at 127 on its tick, before its note-on, and back to 0 at its note-off (the GS send controllers).

## Lanes Wubansia plays from recorded phrases

Near the kit's tempo (within 2 %), Wubansia plays these lanes from the kit's recorded lines and phrases, sliced at
these files' notes; away from it, and always from MIDI in, they play the multisample above:
- **bass**: the Rubber and Deep lines (`Loops/Line Rubber Clean.wav` and the rest), as the loop plays them
- **vox**: the Phrase Vox Riff (`Loops/Phrase Vox Riff.wav`), its dub throw baked in
