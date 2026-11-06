# FUT03 "Glass Current": the written loop as MIDI

128 BPM, G minor (6A), swing 56% written in. Made by `tools/sound/midi.py` from the
kit's recipes (`tools/sound/recipes/fut03/midi.py`); a test keeps these files equal to a fresh export.

- `FUT03_Bassline_Deep_2bars.mid`
- `FUT03_Bassline_Rubber_InTheLoop_2bars.mid`
- `FUT03_Chords_DubStabGm9_SampleTrigger_4bars.mid`
- `FUT03_Chords_DubStabGm9_Voicing_4bars.mid`
- `FUT03_Drums_4bars.mid`
- `FUT03_FullLoop_4bars.mid`
- `FUT03_Lead_KeysEP_2bars.mid`
- `FUT03_Phrase_DubStabGm9_4bars.mid`
- `FUT03_Phrase_KeysEP_4bars.mid`
- `FUT03_Phrase_RubberBassClean_2bars.mid`
- `FUT03_Phrase_VoxSynth_2bars.mid`

**Drums** (channel 10): a drum rack with the kit's samples on these notes (C1 = 36).

| Note | Sample |
|---|---|
| 36 (C1) | Kick Deep |
| 37 (C#1) | Rimshot |
| 38 (D1) | Clap Snare |
| 41 (F1) | Conga G3 |
| 42 (F#1) | Closed Hat Deep |
| 43 (G1) | Conga C4 |
| 45 (A1) | Conga D4 |
| 46 (A#1) | Open Hat Deep |
| 47 (B1) | Bongo G4 |
| 48 (C2) | Bongo D5 |
| 54 (F#2) | Shaker Deep |

Put the closed and open hats in one choke group: the closed hat cuts the open one. The congas and bongos are zoned samples (Conga G3 C4 D4, Bongo G4 D5): map each drum note to its zone.

**Basslines** (2 bars; `InTheLoop` is the one the context loop plays): load Rubber Bass Clean (A) or Rubber Bass Warm (B), with Sub Deep under either into a sampler as a
multisample, each file rooted at its note: G1, D2, G2 (MIDI 31, 38, 43). The sample names count C4 = 60, the note
Ableton and Logic call C3, so root each file by its MIDI number. A is clean, B is warm. Where a line glides into a note, the note before is held 12 ticks into it, so a synth set to legato glide slides there. 2 more slides glide inside one note, which a MIDI note can't carry: they play as written.

**Lead** (2 bars): Keys EP, rooted at G3, D4, G4, D5, G5 (MIDI 55, 62, 67, 74, 79).

**Chords:** the voicing file is for your own synth. The Dub Stab Gm9 sample is the whole chord, so play it
with the sample trigger file: one note on C3 (MIDI 60), where Ableton's Simpler and most samplers
play a sample at its own pitch.

**Full loop** (4 bars): all of the above in one file, the bassline and lead played twice.

**Synth phrases** (channel 4): the kit's synth riffs as MIDI, each on its own sample:

- `FUT03_Phrase_RubberBassClean_2bars.mid`: Rubber riff (2 bars), on Rubber Bass Clean, rooted at G1, D2, G2 (MIDI 31, 38, 43)
- `FUT03_Phrase_DubStabGm9_4bars.mid`: Dub chords, Gm9 Gm9 Cm9 Gm9 (4 bars), on Dub Stab Gm9, rooted at G3 (C4 plays it a fourth up: Cm9) (MIDI 55, 60)
- `FUT03_Phrase_KeysEP_4bars.mid`: Keys arp (4 bars), on Keys EP, rooted at G3, D4, G4, D5, G5 (MIDI 55, 62, 67, 74, 79)
- `FUT03_Phrase_VoxSynth_2bars.mid`: Vox synth riff, Gm9 to Cm9 (2 bars), on Vox Synth, rooted at G3, D4, G4, D5 (MIDI 55, 62, 67, 74)

**Extended mix** (`extended/`): these parts arranged as a DJ-friendly extended mix, 224 bars
(7:00 at 128 BPM), as one file and one region per section, with `regions.json` and a README on
laying the regions out on a timeline.
