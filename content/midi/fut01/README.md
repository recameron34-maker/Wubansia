# FUT01 "Alloy Signal": the written loop as MIDI

134 BPM, F# minor (11A), swing 57% written in. Made by `tools/sound/midi.py` from the
kit's recipes (`tools/sound/recipes/fut01/midi.py`); a test keeps these files equal to a fresh export.

- `FUT01_Bassline_Bounce_2bars.mid`
- `FUT01_Bassline_Deep_2bars.mid`
- `FUT01_Bassline_Walk_InTheLoop_2bars.mid`
- `FUT01_Chords_AirPadFsm9_SampleTrigger_4bars.mid`
- `FUT01_Chords_AirPadFsm9_Voicing_4bars.mid`
- `FUT01_Drums_4bars.mid`
- `FUT01_FullLoop_4bars.mid`
- `FUT01_Lead_ChromePluck_2bars.mid`

**Drums** (channel 10): a drum rack with the kit's samples on these notes (C1 = 36).

| Note | Sample |
|---|---|
| 36 (C1) | Kick Punch |
| 38 (D1) | Snare Clap |
| 42 (F#1) | Closed Hat FM |
| 47 (B1) | Alloy Hit |
| 54 (F#2) | Shaker |

Kick Split (kick A, round 1's pick), Snare Metal, Snare Modal, Closed Hat Ring and Alloy Hit Short are the alternates.

**Basslines** (2 bars; `InTheLoop` is the one the context loop plays): load Reese Clean (A) or Reese Warm (B) into a sampler as a
multisample, each file rooted at its note: F#1, A1, C#2 (MIDI 30, 33, 37). The sample names count C4 = 60, the note
Ableton and Logic call C3, so root each file by its MIDI number. A is clean, B is warm. Where a line glides into a note, the note before is held 12 ticks into it, so a synth set to legato glide slides there.

**Lead** (2 bars): Chrome Pluck, rooted at C4 to C6, every 3 semitones (MIDI 60 to 84).

**Chords:** the voicing file is for your own synth. The Air Pad F#m9 sample is the whole chord, so play it
with the sample trigger file: one note on C3 (MIDI 60), where Ableton's Simpler and most samplers
play a sample at its own pitch.

**Full loop** (4 bars): all of the above in one file, the bassline and lead played twice.

**Extended mix** (`extended/`): these parts arranged as a DJ-friendly extended mix, 224 bars
(6:41 at 134 BPM), as one file and one region per section, with `regions.json` and a README on
laying the regions out on a timeline.
