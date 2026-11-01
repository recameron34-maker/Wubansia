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
multisample, each file rooted at its note (D1, A1, D2). A is clean, B is warm. A slide overlaps the next note
by 12 ticks, so a synth set to legato glide slides there.

**Lead** (2 bars): Chrome Arp, rooted at D4, G4, C5, F5, A#5.

**Chords:** the voicing file is for your own synth. The Starfield Pad Dm9 sample is the whole chord, so play it
with the sample trigger file: one note on C3 (MIDI 60), where Ableton's Simpler and most samplers
play a sample at its own pitch.

**Full loop** (4 bars): all of the above in one file, the bassline and lead played twice.
