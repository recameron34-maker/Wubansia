# FUT01 "Alloy Signal": the written loop as MIDI

134 BPM, F# minor (11A), swing 59% written in. Made by `tools/sound/midi.py` from the
kit's recipes (`tools/sound/recipes/fut01/midi.py`); a test keeps these files equal to a fresh export.

- `FUT01_Bassline_Bounce_InTheLoop_2bars.mid`
- `FUT01_Bassline_Low_2bars.mid`
- `FUT01_Bassline_Speed_2bars.mid`
- `FUT01_Chords_OrganStab_SampleTrigger_4bars.mid`
- `FUT01_Chords_OrganStab_Voicing_4bars.mid`
- `FUT01_Drums_4bars.mid`
- `FUT01_FullLoop_4bars.mid`
- `FUT01_Lead_ChromePluck_2bars.mid`
- `FUT01_Phrase_ChromePluck_4bars.mid`
- `FUT01_Phrase_EPianoStab_2bars.mid`
- `FUT01_Phrase_OrganLead_4bars.mid`
- `FUT01_Phrase_WarpClean_2bars.mid`

**Drums** (channel 10): a drum rack with the kit's samples on these notes (C1 = 36).

| Note | Sample |
|---|---|
| 36 (C1) | Kick Garage |
| 37 (C#1) | Rim Shot |
| 38 (D1) | Clap Garage |
| 42 (F#1) | Hat Closed |
| 46 (A#1) | Hat Open |
| 47 (B1) | Alloy Hit |
| 54 (F#2) | Tambourine |

Kick Split (kick A, round 1's pick) and the Shaker (for the tambourine) are the alternates. Choke Hat Open with Hat Closed (one choke group), as the loop plays them.

**Basslines** (2 bars; `InTheLoop` is the one the context loop plays): load Warp Clean (A) or Warp Warm (B), or Organ Bass for the Low line into a sampler as a
multisample, each file rooted at its note: F#1, A1, C#2 (MIDI 30, 33, 37). The sample names count C4 = 60, the note
Ableton and Logic call C3, so root each file by its MIDI number. A is clean, B is warm. Where a line glides into a note, the note before is held 12 ticks into it, so a synth set to legato glide slides there.

**Lead** (2 bars): Chrome Pluck, rooted at C4 to C6, every 3 semitones (MIDI 60 to 84).

**Chords:** the voicing file is for your own synth. The Organ Stab sample is one file per chord, each the whole chord: load them into a sampler as a
multisample, each file rooted at its note: F#3, B3 (MIDI 54, 59). The sample trigger file plays one note per stab, on its chord's file.

**Full loop** (4 bars): all of the above in one file, the bassline and lead played twice.

**Synth phrases** (channel 4): the kit's synth riffs as MIDI, each on its own sample:

- `FUT01_Phrase_OrganLead_4bars.mid`: Organ riff (4 bars), on Organ Lead, rooted at C4 to C6, every 3 semitones (MIDI 60 to 84)
- `FUT01_Phrase_WarpClean_2bars.mid`: Warp riff (2 bars), on Warp Clean, rooted at F#1, A1, C#2 (MIDI 30, 33, 37)
- `FUT01_Phrase_ChromePluck_4bars.mid`: Pluck riff (4 bars), on Chrome Pluck, rooted at C4 to C6, every 3 semitones (MIDI 60 to 84)
- `FUT01_Phrase_EPianoStab_2bars.mid`: E-piano stab rhythm (2 bars), on E Piano Stab, rooted at F#3, B3 (MIDI 54, 59)

**Extended mix** (`extended/`): these parts arranged as a DJ-friendly extended mix, 224 bars
(6:41 at 134 BPM), as one file and one region per section, with `regions.json` and a README on
laying the regions out on a timeline.
