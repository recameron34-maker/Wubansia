# FUT02 "Orbit Relay": the written loop as MIDI

130 BPM, D minor (7A), swing 52% written in. Made by `tools/sound/midi.py` from the
kit's recipes (`tools/sound/recipes/fut02/midi.py`); a test keeps these files equal to a fresh export.

- `FUT02_Bassline_Orbit_InTheLoop_2bars.mid`
- `FUT02_Bassline_Relay_2bars.mid`
- `FUT02_Chords_StarfieldPadDm9_SampleTrigger_4bars.mid`
- `FUT02_Chords_StarfieldPadDm9_Voicing_4bars.mid`
- `FUT02_Drums_4bars.mid`
- `FUT02_FullLoop_4bars.mid`
- `FUT02_Lead_ChromeArp_2bars.mid`
- `FUT02_Phrase_GlassPing_4bars.mid`
- `FUT02_Phrase_LaserChordStab_2bars.mid`
- `FUT02_Phrase_VoxSynth_2bars.mid`
- `FUT02_Phrase_VoxSynth_4bars.mid`

**Drums** (channel 10): a drum rack with the kit's samples on these notes (C1 = 36).

| Note | Sample |
|---|---|
| 36 (C1) | Kick Split |
| 38 (D1) | Electro Clap |
| 42 (F#1) | Closed Hat |
| 46 (A#1) | Open Hat |
| 47 (B1) | Zap |

Put the closed and open hats in one choke group: the closed hat cuts the open one.

**Basslines** (2 bars; `InTheLoop` is the one the context loop plays): load Roll Bass Clean (A) or Roll Bass Warm (B) into a sampler as a
multisample, each file rooted at its note: D1, A1, D2 (MIDI 26, 33, 38). The sample names count C4 = 60, the note
Ableton and Logic call C3, so root each file by its MIDI number. A is clean, B is warm. Where a line glides into a note, the note before is held 12 ticks into it, so a synth set to legato glide slides there.

**Lead** (2 bars): Chrome Arp, rooted at D4, G4, C5, F5, A#5 (MIDI 62, 67, 72, 77, 82).

**Chords:** the voicing file is for your own synth. The Starfield Pad Dm9 sample is the whole chord, so play it
with the sample trigger file: one note on C3 (MIDI 60), where Ableton's Simpler and most samplers
play a sample at its own pitch.

**Full loop** (4 bars): all of the above in one file, the bassline and lead played twice.

**Synth phrases** (channel 4): the kit's synth riffs as MIDI, each on its own sample:

- `FUT02_Phrase_VoxSynth_2bars.mid`: Vox synth riff, the velocity picks the vowel (soft oh, medium ah, hard ee) (2 bars), on Vox Synth, rooted at D4, F4, A4, C5, F5, A5 (MIDI 62, 65, 69, 72, 77, 81)
- `FUT02_Phrase_LaserChordStab_2bars.mid`: Laser stab riff, Dm9 and B-flat maj9 (2 bars), on Laser Chord Stab, rooted at A#3, D4 (MIDI 58, 62)
- `FUT02_Phrase_GlassPing_4bars.mid`: Glass melody (4 bars), on Glass Ping, rooted at D6 (MIDI 86)
- `FUT02_Phrase_VoxSynth_4bars.mid`: Vox choir chords, the velocity picks the vowel (4 bars), on Vox Synth, rooted at D4, F4, A4, C5, F5, A5 (MIDI 62, 65, 69, 72, 77, 81)

**Extended mix** (`extended/`): these parts arranged as a DJ-friendly extended mix, 224 bars
(6:54 at 130 BPM), as one file and one region per section, with `regions.json` and a README on
laying the regions out on a timeline.
