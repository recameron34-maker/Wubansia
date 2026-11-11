# FUT01 "Alloy Signal": the written music as MIDI

134 BPM, F# minor (11A), swing 59% written in: every odd 16th lands 43 ticks
late. Made by `tools/sound/midi.py` from the kit's recipes (`tools/sound/recipes/fut01/midi.py`) on the channel
map in `docs/midi-map.md`; a test keeps these files equal to a fresh export.

Every file is format 1 at 960 ticks a quarter, in 4/4, with the tempo (447761 microseconds a quarter, the
integer nearest 134 BPM) and the key signature (FF 59: 3 sharps, minor) on its first track.

## The files

| File | What it plays | Channels |
|---|---|---|
| `FUT01_Accents_NoVox_4bars.mid` | the loop's accents, no vox | channel 6 |
| `FUT01_Accents_Vox_4bars.mid` | the loop's accents, with the vox synth | channel 6 |
| `FUT01_Bassline_Bounce_InTheLoop_2bars.mid` | the Bounce line (the loop's) on Bass | channel 1 |
| `FUT01_Bassline_Low_2bars.mid` | the Low line on Organ Bass | channel 11 |
| `FUT01_Bassline_Speed_2bars.mid` | the Speed line on Bass | channel 1 |
| `FUT01_Chords_OrganStab_4bars.mid` | the chord sample, one note per chord at its root, on Organ Stab or E Piano Stab | channel 4 |
| `FUT01_Chords_OrganStab_Voicing_4bars.mid` | the chord voicing, single notes for your own synth | channel 3 |
| `FUT01_Drums_4bars.mid` | the loop's drums and percussion, its fill bar included | channel 10 |
| `FUT01_FX_4bars.mid` | the loop's FX: its fill | channel 7 |
| `FUT01_Lead_ChromePluck_2bars.mid` | the lead phrase, on Chrome Pluck | channel 2 |
| `FUT01_Loop_NoVox_4bars.mid` | the context loop as its club master plays it, lane by lane: no vox, the accents carry the top | channels 1, 2, 3, 4, 6, 7, 10 |
| `FUT01_Loop_Vox_4bars.mid` | the context loop as its club master plays it, lane by lane: with the vox synth | channels 1, 2, 3, 4, 5, 6, 7, 10 |
| `FUT01_Phrase_EPianoStabRhythm_2bars.mid` | E-piano stab rhythm, on E Piano Stab | channel 4 |
| `FUT01_Phrase_OrganRiff_4bars.mid` | Organ riff, on Organ Lead | channel 12 |
| `FUT01_Phrase_PluckRiff_4bars.mid` | Pluck riff, on Chrome Pluck | channel 2 |
| `FUT01_Phrase_VoxSynthRiff_2bars.mid` | Vox synth riff, on Vox Synth | channel 5 |
| `FUT01_Phrase_WarpRiff_2bars.mid` | Warp riff, on Warp Clean | channel 1 |

The extended mix: `extended/vox/` (with the vox synth) and `extended/novox/` (without), each a 224-bar mix in the
genre's form, one region file per section, `regions.json` and a README.

## Drums (channel 10)

A drum rack with the kit's samples on these notes (C1 = 36): 120-tick triggers, so play them as one-shots.

| Note | Sample | Track |
|---|---|---|
| 36 (C1) | Kick Garage | Kick |
| 37 (C#1) | Rim Shot | Perc |
| 38 (D1) | Clap Garage | Clap |
| 42 (F#1) | Hat Closed | Hats |
| 46 (A#1) | Hat Open | Hats |
| 47 (B1) | Alloy Hit | Perc |
| 54 (F#2) | Tambourine | Perc |

Kick Split (kick A, round 1's pick) and the Shaker (for the tambourine) are the alternates. Choke Hat Open with Hat Closed (one choke group), as the loop plays them.

## Pitched instruments

Root each sample file at its MIDI number: the sample names count C4 = 60, the note Ableton and Logic call C3. A note
plays its zone's file repitched by the distance to its root.

| Channel | Track | Instrument | Zones: name (MIDI number) | Velocity |
|---|---|---|---|---|
| 1 | Bass | Warp Clean (A) or Warp Warm (B) | F#1 (30), A1 (33), C#2 (37) | layers at velocity 76, 127 (a velocity plays the nearest, the louder on a tie) |
| 2 | Lead | Chrome Pluck | C4 (60), D#4 (63), F#4 (66), A4 (69), C5 (72), D#5 (75), F#5 (78), A5 (81), C6 (84) | layers at velocity 70, 127 (a velocity plays the nearest, the louder on a tie) |
| 4 | Chords | Organ Stab (A) or E Piano Stab (B) | F#3 (54), B3 (59) | layers at velocity 76, 127 (a velocity plays the nearest, the louder on a tie) |
| 5 | Vox | Vox Synth | A4 (69), C#5 (73), F#5 (78) | layers at velocity 76, 127 (a velocity plays the nearest, the louder on a tie) |
| 11 | Organ Bass | Organ Bass | F#1 (30), A1 (33), C#2 (37) | layers at velocity 76, 127 (a velocity plays the nearest, the louder on a tie) |
| 12 | Organ Lead | Organ Lead | C4 (60), D#4 (63), F#4 (66), A4 (69), C5 (72), D#5 (75), F#5 (78), A5 (81), C6 (84) | layers at velocity 76, 127 (a velocity plays the nearest, the louder on a tie) |
| 13 | Air Pad | Air Pad F#m9 | F#3 (54) | one layer |

On channel 3, Chord voicing is the chords as single notes for your own synth; Wubansia ignores channel 3.
The Low line plays the Organ Bass on channel 11, rooted as the warp bass (F#1, A1, C#2).

## One-key sounds (channels 6, 7 and 8)

Each key plays one sound at its own pitch, from 48 up. A note lasts its sample's length (or the gate the loop gives
it), so a gated pad plays the whole file; an end-aligned sound's note ends on the beat it lands on.

| Key | Channel | Sound | File (in `content/samples/fut01/`) | How it plays |
|---|---|---|---|---|
| 48 | 6 | Reverse Stab | `FX/Reverse Stab v2.wav` | end-aligned: ends on its beat; layers at velocity 76, 127 (a velocity plays the nearest, the louder on a tie) |
| 49 | 6 | Noise Uplifter | `FX/Noise Uplifter.wav` | end-aligned: ends on its beat; one layer |
| 50 | 6 | Organ Hit | `Music/Organ Hit v2.wav` | starts on its note; layers at velocity 76, 127 (a velocity plays the nearest, the louder on a tie) |
| 51 | 6 | Warp Stab | `Music/Warp Stab v2.wav` | starts on its note; layers at velocity 76, 127 (a velocity plays the nearest, the louder on a tie) |
| 52 | 6 | Rave Stab | `Music/Rave Stab v2.wav` | starts on its note; layers at velocity 76, 127 (a velocity plays the nearest, the louder on a tie) |
| 53 | 6 | Chord Sweep | `Music/Chord Sweep v2.wav` | starts on its note; layers at velocity 76, 127 (a velocity plays the nearest, the louder on a tie) |
| 54 | 6 | Vox Synth Stab | `Music/Vox Synth Stab v2.wav` | starts on its note; layers at velocity 76, 127 (a velocity plays the nearest, the louder on a tie) |
| 55 | 6 | Alloy Bell | `Drums/Alloy Bell v4 rr1.wav` | starts on its note; layers at velocity 32, 64, 95, 127 (a velocity plays the nearest, the louder on a tie) |
| 48 | 7 | Glitch Fill Lift | `FX/Glitch Fill Lift.wav` | end-aligned: ends on its beat; one layer |
| 49 | 7 | Glitch Fill Lift, from its 16th 4 | `FX/Glitch Fill Lift.wav` | end-aligned: ends on its beat; one layer |
| 50 | 7 | Shift Riser | `FX/Shift Riser.wav` | end-aligned: ends on its beat; one layer |
| 51 | 7 | Sub Drop | `Bass/Sub Drop.wav` | starts on its note; one layer |
| 48 | 8 | Vox Chop Ah C#5 | `Vox/Vox Chop Ah C#5.wav` | starts on its note; one layer |
| 49 | 8 | Vox Chop Ee E5 | `Vox/Vox Chop Ee E5.wav` | starts on its note; one layer |
| 50 | 8 | Vox Chop Ah Ee F#5 | `Vox/Vox Chop Ah Ee F#5.wav` | starts on its note; one layer |
| 51 | 8 | Vox Chop Oh A4 | `Vox/Vox Chop Oh A4.wav` | starts on its note; one layer |
| 52 | 8 | Vox Chop Ah Oh B4 | `Vox/Vox Chop Ah Oh B4.wav` | starts on its note; one layer |

## Controllers

Filter rides, every 60 ticks while they move (CC74 cutoff, CC71 resonance):
- CC74 on channel 1 (Bass): the warp bass's wobble: the lane filter over 450 Hz opening once an 8th, deeper in bars 2 and 4; logarithmic, 0 = 650 Hz to 127 = 2210 Hz.

Throws: a thrown note has CC91 (Space) and CC94 (Echo) at 127 on its tick, before its note-on, and back to 0 at its note-off, or sooner at the next note-on on its track that isn't thrown, so no other note takes the throw; every track on a channel that throws anywhere in these files starts with both at 0 on tick 0 (the GS send controllers).

## Lanes Wubansia plays from recorded phrases

Near the kit's tempo (within 2 %), Wubansia plays these lanes from the kit's recorded lines and phrases, sliced at
these files' notes; away from it, and always from MIDI in, they play the multisample above:
- **lead**: the Chrome Pluck Phrase (`Loops/Chrome Pluck Phrase.wav`), the phrase as the loop plays it
- **vox**: the Vox Synth Riff (`Loops/Vox Synth Riff.wav`), its throw baked in
