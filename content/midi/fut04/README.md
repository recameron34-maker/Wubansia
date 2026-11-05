# FUT04 "Furnace Code": the written loop as MIDI

136 BPM, E minor (9A), swing 54% written in. Made by `tools/sound/midi.py` from the
kit's recipes (`tools/sound/recipes/fut04/midi.py`); a test keeps these files equal to a fresh export.

- `FUT04_Bassline_AcidLine1_InTheLoop_2bars.mid`
- `FUT04_Bassline_AcidLine2_2bars.mid`
- `FUT04_Chords_DroneEmAdd9_SampleTrigger_4bars.mid`
- `FUT04_Chords_DroneEmAdd9_Voicing_4bars.mid`
- `FUT04_Drums_4bars.mid`
- `FUT04_FullLoop_4bars.mid`
- `FUT04_Lead_ModBlip_2bars.mid`

**Drums** (channel 10): a drum rack with the kit's samples on these notes (C1 = 36).

| Note | Sample |
|---|---|
| 36 (C1) | Kick Split |
| 38 (D1) | Snare Gated |
| 42 (F#1) | Closed Hat |
| 46 (A#1) | Open Hat |
| 47 (B1) | Anvil |
| 51 (D#2) | Ride |

Put the closed and open hats in one choke group: the closed hat cuts the open one; keep the ride out of it, a short ping over a noise wash that plays over them. The rumble is a 2-bar loop (Bass/Rumble.wav) that sits under all 4 bars.

**Basslines** (2 bars; `InTheLoop` is the one the context loop plays): load Acid Clean (A) or Acid Warm (B) into a sampler as a
multisample, each file rooted at its note: E1, A1, E2 (MIDI 28, 33, 40). The sample names count C4 = 60, the note
Ableton and Logic call C3, so root each file by its MIDI number. A is clean, B is warm. Where a line glides into a note, the note before is held 12 ticks into it, so a synth set to legato glide slides there.

**Lead** (2 bars): Mod Blip, rooted at A#4, C#5, E5, G5, A#5, C#6 (MIDI 70, 73, 76, 79, 82, 85).

**Chords:** the voicing file is for your own synth. The Drone Em Add 9 sample is the whole chord, so play it
with the sample trigger file: one note on C3 (MIDI 60), where Ableton's Simpler and most samplers
play a sample at its own pitch.

**Full loop** (4 bars): all of the above in one file, the bassline and lead played twice.

**Extended mix** (`extended/`): these parts arranged as a DJ-friendly extended mix, 224 bars
(6:35 at 136 BPM), as one file and one region per section, with `regions.json` and a README on
laying the regions out on a timeline.
