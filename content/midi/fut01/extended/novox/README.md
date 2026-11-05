# FUT01 "Alloy Signal": the extended mix, no vox

The kit's written music arranged as a DJ-friendly extended mix: 224 bars, 6:41 at 134 BPM, in
F# minor (11A), swing 59% written in, every section on an 8-bar line, 32 bars of drums at
each end. Made by `tools/sound/midi.py` from the kit's PARTS on the channel map in `docs/midi-map.md`; `../vox/` is
the same mix with the vox synth. A test keeps these files equal to a fresh
export.

- `FUT01_ExtendedMix_NoVox_224bars.mid`: the whole mix, a track per instrument, a marker at every section start and cue points (labels: the
  notes play) at the risers, fills and vox entries.
- One region per section, `01_Intro_32bars.mid` to `09_Outro_32bars.mid`: the mix cut at its section lines, with the same
  tracks in the same order, the section's marker and its cues.
- `regions.json`: integers and strings only: the tempo, swing (permille) and length (ms), every section's start bar
  (from 1), length, kind and lanes, line by line, its cues, the tracks and the filter rides.

## The sections

| Bars | Section | Kind | Region | Lanes |
|---|---|---|---|---|
| 1-32 | Intro | intro | `01_Intro_32bars.mid` | kick, snare, hats, perc |
| 33-64 | Groove | groove | `02_Groove_32bars.mid` | kick, snare, hats, perc, bass, lead, chords, accents, fx |
| 65-80 | Break | break | `03_Break_16bars.mid` | hats, lead, chords, accents, fx |
| 81-88 | Build | build | `04_Build_8bars.mid` | kick, snare, hats, perc, chords, fx |
| 89-120 | Drop | drop | `05_Drop_32bars.mid` | kick, snare, hats, perc, bass, lead, chords, accents, fx |
| 121-136 | Switch-up | switch | `06_SwitchUp_16bars.mid` | kick, snare, hats, perc, bass, chords, accents, fx |
| 137-152 | Break 2 | break | `07_Break2_16bars.mid` | hats, lead, chords, accents, fx |
| 153-192 | Drop 2 | drop2 | `08_Drop2_40bars.mid` | kick, snare, hats, perc, bass, lead, chords, accents, fx |
| 193-224 | Outro | outro | `09_Outro_32bars.mid` | kick, snare, hats, perc |

Line by line (bars of the whole mix):

**1. Intro** (bars 1-32)
- 1-8: kick, off-beat hats, perc; a turnaround (thin) in its last bar
- 9-16: kick, hats, perc; a turnaround (gap) in its last bar
- 17-24: kick, clap, hats, perc; a turnaround in its last bar
- 25-32: kick, clap, busy hats, perc; a turnaround (flam) in its last bar

**2. Groove** (bars 33-64)
- 33-40: kick, clap, hats, perc, bass (Low); a turnaround in its last bar
- 41-48: kick, clap, hats, perc, bass (Low), chords, accents, fx; the fill in its last bar
- 49-56: kick, clap, hats, perc, bass (Low), the lead's call, chords, accents; the drums' alloy variant; a turnaround (flam) in its last bar
- 57-64: kick, clap, busy hats, perc, bass (Low), the lead's call, chords, accents, fx; the fill in its last bar

**3. Break** (bars 65-80)
- 65-72: pad, accents
- 73-80: off-beat hats, the lead's call, chords, pad, accents, fx; the hats rise from 40% to 100% velocity; the fill in its last bar

**4. Build** (bars 81-88)
- 81-88: kick, clap, off-beat hats, perc, chords, fx; the drums' tamb variant; the clap rolls over the line's last 2 bars up to the fill, rising hit by hit; the Chords rise from 55% to 100% velocity; the fill (Glitch Fill Lift) in its last bar, the drums out from its start

**5. Drop** (bars 89-120)
- 89-96: kick, clap, hats, perc, bass (Bounce), chords, accents, fx; a turnaround in its last bar
- 97-104: kick, clap, hats, perc, bass (Bounce), the lead's call, chords, accents, fx; the fill in its last bar
- 105-112: kick, clap, busy hats, perc, bass (Bounce), lead, chords, accents; the drums' rims variant; a turnaround (gap) in its last bar
- 113-120: kick, clap, hats, perc, bass (Bounce), the lead's call, chords, accents; the drums' alloy variant; the drums out in its last bar

**6. Switch-up** (bars 121-136)
- 121-128: kick, clap, hats, perc, bass (Speed), accents, fx; the drums' 4x4 variant; a turnaround (4x4) in its last bar
- 129-136: kick, clap, busy hats, perc, bass (Speed), chords, accents, fx; the drums' 4x4 variant; the fill in its last bar

**7. Break 2** (bars 137-152)
- 137-144: lead (the organ riff), pad (slow), accents
- 145-152: off-beat hats, lead (the organ riff), chords (slow), pad (slow), fx; the hats rise from 40% to 100% velocity; the fill in its last bar

**8. Drop 2** (bars 153-192)
- 153-160: kick, clap, busy hats, perc, bass (Bounce), lead (the organ riff), chords, accents, fx; the drums' alloy2 variant; a turnaround in its last bar
- 161-168: kick, clap, busy hats, perc, bass (the warp riff), the lead's call, chords, accents, fx; the fill in its last bar
- 169-176: kick, clap, hats, perc, bass (Bounce), lead (the organ riff), chords (the e-piano stab rhythm), accents; the drums' rims variant; a turnaround (flam) in its last bar
- 177-184: kick, clap, busy hats, perc, bass (Bounce), the lead's answer, chords, accents, fx; the fill in its last bar
- 185-192: kick, clap, hats, perc, bass (Low); a turnaround (gap) in its last bar

**9. Outro** (bars 193-224)
- 193-200: kick, clap, busy hats, perc; the drums' rims variant; a turnaround (gap) in its last bar
- 201-208: kick, clap, hats, perc; the drums' alloy variant; a turnaround (flam) in its last bar
- 209-216: kick, hats, perc; the drums' tamb8 variant; a turnaround in its last bar
- 217-224: kick, hats, perc; the drums' tamb variant; a turnaround (gap) in its last bar

## The cues

- bar 48: Fill
- bar 57: Vox
- bar 64: Fill
- bar 65: Vox
- bar 80: Fill
- bar 81: Riser
- bar 88: Fill
- bar 89: Vox
- bar 104: Fill
- bar 113: Vox
- bar 136: Fill
- bar 145: Riser
- bar 152: Fill
- bar 153: Vox
- bar 168: Fill
- bar 177: Vox
- bar 184: Fill

A riser's note ends on its section's first beat; a fill's on the next downbeat; the vox plays from a Vox cue.

## Tracks

Drums are named as drum racks name them (C1 = 36, so C3 = 60, as Ableton and Logic count); pitched notes as the sample
names count them, C4 = 60. The two meet at MIDI 60: root each sample at its MIDI number. The kit's README (`../../`)
has every instrument's zones and every one-key sound's file.

| Track | Channel | Lane | Notes |
|---|---|---|---|
| Kick | 10 | kick | 36 Kick Garage |
| Clap | 10 | snare | 38 Clap Garage |
| Hats | 10 | hats | 42 Hat Closed, 46 Hat Open |
| Perc | 10 | perc | 37 Rim Shot, 47 Alloy Hit, 54 Tambourine |
| Bass | 1 | bass | 30 F#1, 33 A1, 35 B1, 37 C#2, 38 D2, 40 E2, 42 F#2 |
| Organ Bass | 11 | bass | 30 F#1, 32 G#1, 33 A1, 35 B1, 37 C#2 |
| Lead | 2 | lead | 66 F#4, 69 A4, 71 B4, 73 C#5, 76 E5, 78 F#5, 80 G#5 |
| Organ Lead | 12 | lead | 66 F#4, 68 G#4, 69 A4, 71 B4, 73 C#5, 76 E5, 78 F#5 |
| Chords | 4 | chords | 54 F#3, 59 B3 |
| Chord voicing | 3 | chords | 54 F#3, 59 B3, 61 C#4, 64 E4, 66 F#4, 68 G#4, 69 A4, 73 C#5, 74 D5 |
| Air Pad | 13 | chords | 54 F#3, 59 B3 |
| Accents | 6 | accents | 48 Reverse Stab, 49 Noise Uplifter, 50 Organ Hit, 51 Warp Stab, 52 Rave Stab, 53 Chord Sweep, 54 Vox Synth Stab, 55 Alloy Bell |
| FX | 7 | fx | 48 Glitch Fill Lift, 49 Glitch Fill Lift, from its 16th 4, 50 Shift Riser, 51 Sub Drop |

## On a timeline

Set the project to 134 BPM in 4/4 first: every file carries the tempo, but a DAW doesn't always take it from a
dropped file.
- **The whole mix:** drop `FUT01_ExtendedMix_NoVox_224bars.mid` on bar 1: a track per instrument, in the order above.
- **Your own order:** drop the region files on bar lines, each where you want its section. A region starts on its first
  downbeat and ends on its last bar line, so regions laid end to end play like the whole mix, except that a note held
  over a section line there (a vox throw into a break, a long one-shot) stops on the line. Every region has the same
  tracks; if a DAW leaves an empty one out, move the clips under it onto the tracks of the same name.
- **Ableton Live:** drag the files into the Arrangement view: a file's tracks come in as clips on consecutive MIDI
  tracks. Clips keep the notes, not their channels: route each track to its instrument (`docs/midi-map.md`).
- **Logic Pro:** File > Open on the whole-mix file brings its tempo, key and section markers with it (an import
  doesn't); File > Import > MIDI File puts a software instrument track per MIDI track at the playhead.

Not checked yet: how the mix sounds, and how each DAW opens these files. Nothing here has been played in a DAW or
listened to.
