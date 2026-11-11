# FUT01 "Alloy Signal": the extended mix, no vox

The kit's written music arranged as a DJ-friendly extended mix: 224 bars, 6:41 at 134 BPM, in
F# minor (11A), swing 59% written in, every section on an 8-bar line, 32 bars of drums at
each end. Made by `tools/sound/midi.py` from the kit's PARTS on the channel map in `docs/midi-map.md`; `../vox/` is
the same mix with the vox synth. A test keeps these files equal to a fresh
export.

- `FUT01_ExtendedMix_NoVox_224bars.mid`: the whole mix, a track per instrument, a marker at every section start and cue points (labels: the
  notes play) at the risers and fills.
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
| 81-96 | Build | build | `04_Build_16bars.mid` | kick, snare, hats, perc, chords, fx |
| 97-128 | Drop | drop | `05_Drop_32bars.mid` | kick, snare, hats, perc, bass, lead, chords, accents, fx |
| 129-144 | Switch-up | switch | `06_SwitchUp_16bars.mid` | kick, snare, hats, perc, bass, chords, accents, fx |
| 145-160 | Break 2 | break | `07_Break2_16bars.mid` | hats, lead, chords, accents, fx |
| 161-192 | Drop 2 | drop2 | `08_Drop2_32bars.mid` | kick, snare, hats, perc, bass, lead, chords, accents, fx |
| 193-224 | Outro | outro | `09_Outro_32bars.mid` | kick, snare, hats, perc |

Line by line (bars of the whole mix):

**1. Intro** (bars 1-32)
- 1-8: kick, hats, perc
- 9-16: kick, off-beat hats, perc; a turnaround in its last bar
- 17-24: kick, clap, hats, perc
- 25-32: kick, clap, hats, perc; a turnaround in its last bar

**2. Groove** (bars 33-64)
- 33-40: kick, clap, hats, perc, bass (Bounce); a turnaround in its last bar
- 41-48: kick, clap, hats, perc, bass (Bounce), chords, accents, fx; the fill in its last bar
- 49-56: kick, clap, hats, perc, bass (Bounce), the lead's call, chords, accents; a turnaround in its last bar
- 57-64: kick, clap, hats, perc, bass (Bounce), the lead's call, chords, accents, fx; the fill in its last bar

**3. Break** (bars 65-80)
- 65-72: the lead's call, chords
- 73-80: off-beat hats, lead, chords, accents, fx; the hats rise from 40% to 100% velocity; the fill in its last bar

**4. Build** (bars 81-96)
- 81-88: kick, clap, off-beat hats, perc, chords; a turnaround in its last bar
- 89-96: kick, clap, hats, perc, fx; the clap rolls over bars 95-96 up to the fill, rising hit by hit; the fill in its last bar

**5. Drop** (bars 97-128)
- 97-104: kick, clap, hats, perc, bass (Bounce), lead, chords, accents, fx; a turnaround in its last bar
- 105-112: kick, clap, hats, perc, bass (Bounce), lead, chords, accents, fx; the fill in its last bar
- 113-120: kick, clap, busy hats, perc, bass (Bounce), lead, chords, accents; a turnaround in its last bar
- 121-128: kick, clap, hats, perc, bass (Bounce), lead, chords, accents; the drums out in its last bar

**6. Switch-up** (bars 129-144)
- 129-136: kick, clap, busy hats, perc, bass (Low), accents; a turnaround in its last bar
- 137-144: kick, clap, busy hats, perc, bass (Low), chords, accents, fx; the fill in its last bar

**7. Break 2** (bars 145-160)
- 145-152: the lead's call, chords
- 153-160: off-beat hats, lead, chords, accents, fx; the hats rise from 40% to 100% velocity; the fill in its last bar

**8. Drop 2** (bars 161-192)
- 161-168: kick, clap, busy hats, perc, bass (Bounce), lead, chords, accents, fx; a turnaround in its last bar
- 169-176: kick, clap, busy hats, perc, bass (Bounce), lead, chords, accents, fx; the fill in its last bar
- 177-184: kick, clap, busy hats, perc, bass (Bounce), lead, chords, accents; a turnaround in its last bar
- 185-192: kick, clap, hats, perc, bass (Bounce), chords, accents, fx; the fill in its last bar

**9. Outro** (bars 193-224)
- 193-200: kick, clap, hats, perc
- 201-208: kick, clap, hats, perc; a turnaround in its last bar
- 209-216: kick, off-beat hats, perc
- 217-224: kick, hats, perc

## The cues

- bar 48: Fill
- bar 64: Fill
- bar 80: Fill
- bar 89: Riser
- bar 96: Fill
- bar 112: Fill
- bar 144: Fill
- bar 153: Riser
- bar 160: Fill
- bar 176: Fill
- bar 192: Fill

A riser's note ends on its section's first beat; a fill's on the next downbeat.

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
| Bass | 1 | bass | 30 F#1, 33 A1, 35 B1, 37 C#2, 38 D2, 42 F#2 |
| Organ Bass | 11 | bass | 30 F#1, 32 G#1, 33 A1, 35 B1, 37 C#2 |
| Lead | 2 | lead | 66 F#4, 69 A4, 71 B4, 73 C#5, 76 E5, 78 F#5, 80 G#5 |
| Chords | 4 | chords | 54 F#3, 59 B3 |
| Chord voicing | 3 | chords | 54 F#3, 59 B3, 61 C#4, 64 E4, 66 F#4, 68 G#4, 69 A4, 73 C#5, 74 D5 |
| Accents | 6 | accents | 48 Reverse Stab, 49 Noise Uplifter, 50 Organ Hit, 51 Warp Stab, 55 Alloy Bell |
| FX | 7 | fx | 49 Glitch Fill Lift, from its 16th 4, 50 Shift Riser, 51 Sub Drop |

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
