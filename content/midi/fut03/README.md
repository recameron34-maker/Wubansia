# FUT03 "Glass Current": the written loop as MIDI

128 BPM, G minor (6A), swing 53% written in. Made by `tools/sound/midi.py` from the
kit's recipes (`tools/sound/recipes/fut03/midi.py`); a test keeps these files equal to a fresh export.

- `FUT03_Bassline_Bounce_InTheLoop_2bars.mid`
- `FUT03_Bassline_Roll_2bars.mid`
- `FUT03_Chords_StabGm9_SampleTrigger_4bars.mid`
- `FUT03_Chords_StabGm9_Voicing_4bars.mid`
- `FUT03_Drums_4bars.mid`
- `FUT03_FullLoop_4bars.mid`
- `FUT03_Lead_GlassPluck_2bars.mid`

**Drums** (channel 10): a drum rack with the kit's samples on these notes (C1 = 36).

| Note | Sample |
|---|---|
| 36 (C1) | Kick Split |
| 38 (D1) | Clap |
| 42 (F#1) | Closed Hat |
| 46 (A#1) | Open Hat |
| 47 (B1) | Metal Pling G5 |
| 48 (C2) | Metal Pling D5 |
| 54 (F#2) | Shaker |

Put the closed and open hats in one choke group: the closed hat cuts the open one.

**Basslines** (2 bars; `InTheLoop` is the one the context loop plays): load Dip Bass Clean (A) or Dip Bass Warm (B) into a sampler as a
multisample, each file rooted at its note: G1, D2, G2 (MIDI 31, 38, 43). The sample names count C4 = 60, the note
Ableton and Logic call C3, so root each file by its MIDI number. A is clean, B is warm. The lines' slides glide inside one note, which a MIDI note can't carry: they play as written.

**Lead** (2 bars): Glass Pluck, rooted at D4, G4, A#4, D5, G5, A#5 (MIDI 62, 67, 70, 74, 79, 82).

**Chords:** the voicing file is for your own synth. The Stab Gm9 sample is the whole chord, so play it
with the sample trigger file: one note on C3 (MIDI 60), where Ableton's Simpler and most samplers
play a sample at its own pitch.

**Full loop** (4 bars): all of the above in one file, the bassline and lead played twice.

**Extended mix** (`extended/`): these parts arranged as a DJ-friendly extended mix, 224 bars
(7:00 at 128 BPM), as one file and one region per section, with `regions.json` and a README on
laying the regions out on a timeline.
