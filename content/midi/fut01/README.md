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
| 36 (C1) | Kick Split |
| 38 (D1) | Snare Metal |
| 42 (F#1) | Closed Hat FM |
| 47 (B1) | Alloy Hit |

Snare Modal, Closed Hat Ring and Alloy Hit Short are the alternates.

**Basslines** (2 bars; `InTheLoop` is the one the context loop plays): load Reese Clean (A) or Reese Warm (B) into a sampler as a
multisample, each file rooted at its note (F#1, A1, C#2). A is clean, B is warm. A slide overlaps the next note
by 12 ticks, so a synth set to legato glide slides there.

**Lead** (2 bars): Chrome Pluck, rooted at C4 to C6, every 3 semitones.

**Chords:** the voicing file is for your own synth. The Air Pad F#m9 sample is the whole chord, so play it
with the sample trigger file: one note on C3 (MIDI 60), where Ableton's Simpler and most samplers
play a sample at its own pitch.

**Full loop** (4 bars): all of the above in one file, the bassline and lead played twice.
