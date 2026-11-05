# FUT02 "Orbit Relay": the written music as MIDI

130 BPM, D minor (7A), swing 52% written in: every odd 16th lands 10 ticks
late. Made by `tools/sound/midi.py` from the kit's recipes (`tools/sound/recipes/fut02/midi.py`) on the channel
map in `docs/midi-map.md`; a test keeps these files equal to a fresh export.

Every file is format 1 at 960 ticks a quarter, in 4/4, with the tempo (461538 microseconds a quarter, the
integer nearest 130 BPM) and the key signature (FF 59: 1 flat, minor) on its first track.

## The files

| File | What it plays | Channels |
|---|---|---|
| `FUT02_Accents_NoVox_4bars.mid` | the loop's accents, no vox | channel 6 |
| `FUT02_Accents_Vox_4bars.mid` | the loop's accents, with the vox synth | channel 6 |
| `FUT02_Bassline_Orbit_InTheLoop_2bars.mid` | the Orbit line (the loop's) on Bass | channel 1 |
| `FUT02_Bassline_Relay_2bars.mid` | the Relay line on Bass | channel 1 |
| `FUT02_Chords_StarfieldPadDm9_4bars.mid` | the chord sample, one note per chord at its root, on Starfield Pad Dm9 | channel 4 |
| `FUT02_Chords_StarfieldPadDm9_Voicing_4bars.mid` | the chord voicing, single notes for your own synth | channel 3 |
| `FUT02_Drums_4bars.mid` | the loop's drums and percussion, its fill bar included | channel 10 |
| `FUT02_FX_4bars.mid` | the loop's FX: its fill | channel 7 |
| `FUT02_Lead_ChromeArp_2bars.mid` | the lead phrase, on Chrome Arp | channel 2 |
| `FUT02_Loop_NoVox_4bars.mid` | the context loop as its club master plays it, lane by lane: no vox, the accents carry the top | channels 1, 2, 3, 4, 6, 7, 10, 11 |
| `FUT02_Loop_Vox_4bars.mid` | the context loop as its club master plays it, lane by lane: with the vox synth | channels 1, 2, 3, 4, 5, 6, 7, 10, 11 |
| `FUT02_Phrase_GlassMelody_4bars.mid` | Glass melody, on Glass Ping | channel 13 |
| `FUT02_Phrase_LaserStabRiff_2bars.mid` | Laser stab riff, Dm9 and B-flat maj9, on Laser Chord Stab | channel 12 |
| `FUT02_Phrase_VoxChoirChords_4bars.mid` | Vox choir chords, the velocity picks the vowel, on Vox Synth | channel 5 |
| `FUT02_Phrase_VoxSynthRiff_2bars.mid` | Vox synth riff, the velocity picks the vowel (soft oh, medium ah, hard ee), on Vox Synth | channel 5 |

The extended mix: `extended/vox/` (with the vox synth) and `extended/novox/` (without), each a 224-bar mix in the kit's
own form, one region file per section, `regions.json` and a README.

## Drums (channel 10)

A drum rack with the kit's samples on these notes (C1 = 36): 120-tick triggers, so play them as one-shots.

| Note | Sample | Track |
|---|---|---|
| 36 (C1) | Kick Split | Kick |
| 38 (D1) | Electro Clap | Clap |
| 42 (F#1) | Closed Hat | Hats |
| 46 (A#1) | Open Hat | Hats |
| 47 (B1) | Zap | Perc |

Put the closed and open hats in one choke group: the closed hat cuts the open one.

## Pitched instruments

Root each sample file at its MIDI number: the sample names count C4 = 60, the note Ableton and Logic call C3. A note
plays its zone's file repitched by the distance to its root.

| Channel | Track | Instrument | Zones: name (MIDI number) | Velocity |
|---|---|---|---|---|
| 1 | Bass | Roll Bass Clean (A) or Roll Bass Warm (B) | D1 (26), A1 (33), D2 (38) | layers at velocity 76, 127 (a velocity plays the nearest, the louder on a tie) |
| 2 | Lead | Chrome Arp | D4 (62), G4 (67), C5 (72), F5 (77), A#5 (82) | layers at velocity 70, 127 (a velocity plays the nearest, the louder on a tie) |
| 4 | Chords | Starfield Pad Dm9 | D3 (50) | one layer |
| 5 | Vox | Vox Synth | D4 (62), F4 (65), A4 (69), C5 (72), F5 (77), A5 (81) | layers at velocity 64 'oh', 95 'ah', 127 'ee' (a velocity plays the nearest, the louder on a tie) |
| 11 | Drop Glide | Drop Glide Clean (A) or Drop Glide Warm (B) | D1 (26) | layers at velocity 89, 127 (a velocity plays the nearest, the louder on a tie) |
| 12 | Laser Chord Stab | Laser Chord Stab | A#3 (58), D4 (62) | layers at velocity 76, 127 (a velocity plays the nearest, the louder on a tie) |
| 13 | Glass Ping | Glass Ping | D6 (86) | layers at velocity 76, 127 (a velocity plays the nearest, the louder on a tie) |

On channel 3, Chord voicing is the chords as single notes for your own synth; Wubansia ignores channel 3.
The drop glide (channel 11) follows the bass A/B switch: Drop Glide Clean (A) or Warm (B), rooted at D1 (26).

## One-key sounds (channels 6, 7 and 8)

Each key plays one sound at its own pitch, from 48 up. A note lasts its sample's length (or the gate the loop gives
it), so a gated pad plays the whole file; an end-aligned sound's note ends on the beat it lands on.

| Key | Channel | Sound | File (in `content/samples/fut02/`) | How it plays |
|---|---|---|---|---|
| 48 | 6 | Laser Chord Stab D4 | `Music/Laser Chord Stab D4 v2.wav` | starts on its note; layers at velocity 76, 127 (a velocity plays the nearest, the louder on a tie) |
| 49 | 6 | Laser Chord Stab A#3 | `Music/Laser Chord Stab A#3 v2.wav` | starts on its note; layers at velocity 76, 127 (a velocity plays the nearest, the louder on a tie) |
| 50 | 6 | Glass Ping | `Drums/Glass Ping v2 rr1.wav` | starts on its note; layers at velocity 76, 127 (a velocity plays the nearest, the louder on a tie) |
| 51 | 6 | Starfield Sweep | `FX/Starfield Sweep.wav` | end-aligned: ends on its beat; one layer |
| 52 | 6 | Reverse Chord | `FX/Reverse Chord v2.wav` | end-aligned: ends on its beat; layers at velocity 76, 127 (a velocity plays the nearest, the louder on a tie) |
| 53 | 6 | Arp Stab | `Music/Arp Stab v2.wav` | starts on its note; layers at velocity 76, 127 (a velocity plays the nearest, the louder on a tie) |
| 54 | 6 | Noise Uplifter | `FX/Noise Uplifter.wav` | end-aligned: ends on its beat; one layer |
| 55 | 6 | Resonant Zap | `Drums/Resonant Zap v2.wav` | starts on its note; layers at velocity 76, 127 (a velocity plays the nearest, the louder on a tie) |
| 56 | 6 | Vox Synth Stab | `Music/Vox Synth Stab v2.wav` | starts on its note; layers at velocity 76, 127 (a velocity plays the nearest, the louder on a tie) |
| 48 | 7 | Laser Fill | `FX/Laser Fill.wav` | end-aligned: ends on its beat; one layer |
| 49 | 7 | Laser Riser | `FX/Laser Riser.wav` | end-aligned: ends on its beat; one layer |
| 50 | 7 | Sub Drop | `Bass/Sub Drop.wav` | starts on its note; one layer |
| 48 | 8 | Vox Chop Ah D5 | `Vox/Vox Chop Ah D5.wav` | starts on its note; one layer |
| 49 | 8 | Vox Chop Ee F5 | `Vox/Vox Chop Ee F5.wav` | starts on its note; one layer |
| 50 | 8 | Vox Chop Oh A4 | `Vox/Vox Chop Oh A4.wav` | starts on its note; one layer |
| 51 | 8 | Vox Chop Ee Ah C5 | `Vox/Vox Chop Ee Ah C5.wav` | starts on its note; one layer |
| 52 | 8 | Vox Chop Ah Oh A5 | `Vox/Vox Chop Ah Oh A5.wav` | starts on its note; one layer |

## Controllers

Filter rides, every 60 ticks while they move (CC74 cutoff, CC71 resonance):
- CC74 on channel 2 (Lead): the arp's filter ride: open from bar 1 to 16th 46, closed again by the loop's end; logarithmic, 0 = 1600 Hz to 127 = 16000 Hz.

Throws: a thrown note has CC91 (Space) and CC94 (Echo) at 127 on its tick, before its note-on, and back to 0 at its note-off, or sooner at the next note-on on its track that isn't thrown, so no other note takes the throw (the GS send controllers).

## Lanes Wubansia plays from recorded phrases

Near the kit's tempo (within 2 %), Wubansia plays these lanes from the kit's recorded lines and phrases, sliced at
these files' notes; away from it, and always from MIDI in, they play the multisample above:
- **bass**: the Orbit and Relay lines (`Loops/Orbit Line Clean.wav` and the rest), as the loop plays them
- **lead**: the Chrome Arp Phrase (`Loops/Chrome Arp Phrase.wav`); its filter ride follows CC74
- **vox**: the vox synth riff, as the loop sings it (a stem derived from the recipe, if approved)
