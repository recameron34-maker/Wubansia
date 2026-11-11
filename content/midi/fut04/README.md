# FUT04 "Furnace Code": the written music as MIDI

136 BPM, E minor (9A), swing 54% written in: every odd 16th lands 19 ticks
late. Made by `tools/sound/midi.py` from the kit's recipes (`tools/sound/recipes/fut04/midi.py`) on the channel
map in `docs/midi-map.md`; a test keeps these files equal to a fresh export.

Every file is format 1 at 960 ticks a quarter, in 4/4, with the tempo (441176 microseconds a quarter, the
integer nearest 136 BPM) and the key signature (FF 59: 1 sharp, minor) on its first track.

## The files

| File | What it plays | Channels |
|---|---|---|
| `FUT04_Accents_NoVox_4bars.mid` | the loop's accents, no vox | channel 6 |
| `FUT04_Accents_Vox_4bars.mid` | the loop's accents, with the vox synth | channel 6 |
| `FUT04_Bassline_AcidLine1_InTheLoop_2bars.mid` | the AcidLine1 line (the loop's) on Bass | channel 1 |
| `FUT04_Bassline_AcidLine2_2bars.mid` | the AcidLine2 line on Bass | channel 1 |
| `FUT04_Chords_DroneEmAdd9_4bars.mid` | the chord sample, one note per chord at its root, on Drone Em Add 9 | channel 4 |
| `FUT04_Chords_DroneEmAdd9_Voicing_4bars.mid` | the chord voicing, single notes for your own synth | channel 3 |
| `FUT04_Drums_4bars.mid` | the loop's drums and percussion, its fill bar included | channel 10 |
| `FUT04_FX_4bars.mid` | the loop's FX: its fill | channel 7 |
| `FUT04_Lead_ModBlip_2bars.mid` | the lead phrase, on Mod Blip | channel 2 |
| `FUT04_Loop_NoVox_4bars.mid` | the context loop as its club master plays it, lane by lane: no vox, the accents carry the top | channels 1, 2, 3, 4, 6, 7, 10 |
| `FUT04_Loop_Vox_4bars.mid` | the context loop as its club master plays it, lane by lane: with the vox synth | channels 1, 2, 3, 4, 5, 6, 7, 10 |
| `FUT04_Phrase_AcidStabRiff_2bars.mid` | Acid stab riff, on Accent Acid Stab E4 | channel 11 |
| `FUT04_Phrase_MetalChords_4bars.mid` | Metal chords, Em Em Am Bm, on Accent Metal Chord Em | channel 12 |
| `FUT04_Phrase_RaveStabRiff_2bars.mid` | Rave stab riff, on Accent Rave Stab Em | channel 13 |
| `FUT04_Phrase_VoxSynthRiff_2bars.mid` | Vox synth riff, on Vox Synth | channel 5 |

The extended mix: `extended/vox/` (with the vox synth) and `extended/novox/` (without), each a 224-bar mix in the
genre's form, one region file per section, `regions.json` and a README.

## Drums (channel 10)

A drum rack with the kit's samples on these notes (C1 = 36): 120-tick triggers, so play them as one-shots.

| Note | Sample | Track |
|---|---|---|
| 35 (B0) | Rumble | Rumble |
| 36 (C1) | Kick Split | Kick |
| 38 (D1) | Snare Gated | Snare |
| 42 (F#1) | Closed Hat | Hats |
| 46 (A#1) | Open Hat | Hats |
| 47 (B1) | Anvil | Perc |
| 51 (D#2) | Ride | Hats |

Put the closed and open hats in one choke group: the closed hat cuts the open one; keep the ride out of it, a short ping over a noise wash that plays over them. The rumble (35) is a 2-bar loop under the kick, gated by its note's length.

## Pitched instruments

Root each sample file at its MIDI number: the sample names count C4 = 60, the note Ableton and Logic call C3. A note
plays its zone's file repitched by the distance to its root.

| Channel | Track | Instrument | Zones: name (MIDI number) | Velocity |
|---|---|---|---|---|
| 1 | Bass | Acid Clean (A) or Acid Warm (B) | E1 (28), A1 (33), E2 (40) | layers at velocity 76, 127 (a velocity plays the nearest, the louder on a tie) |
| 2 | Lead | Mod Blip | A#4 (70), C#5 (73), E5 (76), G5 (79), A#5 (82), C#6 (85) | layers at velocity 70, 127 (a velocity plays the nearest, the louder on a tie) |
| 4 | Chords | Drone Em Add 9 | E3 (52) | one layer |
| 5 | Vox | Vox Synth | E3 (52), G3 (55), B3 (59), D4 (62), E4 (64), G4 (67), B4 (71) | layers at velocity 64 'oo', 127 'oh' (a velocity plays the nearest, the louder on a tie) |
| 11 | Acid Stab | Accent Acid Stab E4 | E4 (64) | one layer |
| 12 | Metal Chord | Accent Metal Chord Em | E4 (64) | one layer |
| 13 | Rave Stab | Accent Rave Stab Em | E4 (64) | one layer |

On channel 3, Chord voicing is the chords as single notes for your own synth; Wubansia ignores channel 3.
The squelch ticks on the kicks (velocity 57) are the voice above 800 Hz alone in the recipe: as MIDI notes they play the whole acid sample.

## One-key sounds (channels 6, 7 and 8)

Each key plays one sound at its own pitch, from 48 up. A note lasts its sample's length (or the gate the loop gives
it), so a gated pad plays the whole file; an end-aligned sound's note ends on the beat it lands on.

| Key | Channel | Sound | File (in `content/samples/fut04/`) | How it plays |
|---|---|---|---|---|
| 48 | 6 | Accent Rave Stab Em | `Music/Accent Rave Stab Em.wav` | starts on its note; one layer |
| 49 | 6 | Accent Noise Sweep E | `FX/Accent Noise Sweep E.wav` | end-aligned: ends on its beat; one layer |
| 50 | 6 | Accent Reverse Hit Em | `FX/Accent Reverse Hit Em.wav` | end-aligned: ends on its beat; one layer |
| 51 | 6 | Accent Zap E5 | `Drums/Accent Zap E5.wav` | starts on its note; one layer |
| 52 | 6 | Accent Metal Chord Em | `Music/Accent Metal Chord Em.wav` | starts on its note; one layer |
| 53 | 6 | Accent Acid Stab E4 | `Music/Accent Acid Stab E4.wav` | starts on its note; one layer |
| 54 | 6 | Accent Dist Chord Em | `Music/Accent Dist Chord Em.wav` | starts on its note; one layer |
| 55 | 6 | Accent Vox Chord Em | `Music/Accent Vox Chord Em.wav` | starts on its note; one layer |
| 48 | 7 | Stutter Fill | `FX/Stutter Fill.wav` | end-aligned: ends on its beat; one layer |
| 49 | 7 | Iron Riser | `FX/Iron Riser.wav` | end-aligned: ends on its beat; one layer |
| 50 | 7 | Accent Sub Drop E2 | `Bass/Accent Sub Drop E2.wav` | starts on its note; one layer |
| 48 | 8 | Vox Stab Oh E3 | `Vox/Vox Stab Oh E3.wav` | starts on its note; one layer |
| 49 | 8 | Vox Stab Oo B3 | `Vox/Vox Stab Oo B3.wav` | starts on its note; one layer |

## Controllers

Filter rides, every 60 ticks while they move (CC74 cutoff, CC71 resonance):
- CC74 on channel 1 (Bass): the acid's ride: its cutoff x1 over bars 1-2, opening through bar 3, x2.6 on the fill bar's third beat, closed again by the downbeat; logarithmic, 0 = 5400 Hz to 127 = 14040 Hz.
- CC71 on channel 1 (Bass): the acid's resonance: at rest over bars 1-2, rising over bar 3 to its top on the fill bar; linear, 0 = 0.9 to 127 = 0.96.

Throws: a thrown note has CC91 (Space) and CC94 (Echo) at 127 on its tick, before its note-on, and back to 0 at its note-off, or sooner at the next note-on on its track that isn't thrown, so no other note takes the throw (the GS send controllers).

## Lanes Wubansia plays from recorded phrases

Near the kit's tempo (within 2 %), Wubansia plays these lanes from the kit's recorded lines and phrases, sliced at
these files' notes; away from it, and always from MIDI in, they play the multisample above:
- **bass**: the Acid Line 1 Ride (`Loops/Acid Line 1 Ride Clean.wav` and Warm), its ride baked in; the other lines from `Loops/Acid Line 1` and `Acid Line 2`
- **lead**: the Mod Blip Phrase (`Loops/Mod Blip Phrase.wav`), its ratchets included
