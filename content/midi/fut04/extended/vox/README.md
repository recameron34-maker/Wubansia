# FUT04 "Furnace Code": the extended mix, with the vox synth

The kit's written music arranged as a DJ-friendly extended mix: 224 bars, 6:35 at 136 BPM, in
E minor (9A), swing 54% written in, every section on an 8-bar line, 32 bars of drums at
each end. Made by `tools/sound/midi.py` from the kit's PARTS on the channel map in `docs/midi-map.md`; `../novox/` is
the same mix without the vox synth. A test keeps these files equal to a fresh
export.

- `FUT04_ExtendedMix_Vox_224bars.mid`: the whole mix, a track per instrument, a marker at every section start and cue points (labels: the
  notes play) at the risers, fills and vox entries.
- One region per section, `01_Intro_32bars.mid` to `07_Outro_32bars.mid`: the mix cut at its section lines, with the same
  tracks in the same order, the section's marker and its cues.
- `regions.json`: integers and strings only: the tempo, swing (permille) and length (ms), every section's start bar
  (from 1), length, kind and lanes, line by line, its cues, the tracks and the filter rides.

## The sections

| Bars | Section | Kind | Region | Lanes |
|---|---|---|---|---|
| 1-32 | Intro | intro | `01_Intro_32bars.mid` | kick, snare, hats, perc |
| 33-96 | Groove | groove | `02_Groove_64bars.mid` | kick, snare, hats, perc, bass, lead, chords, vox, accents, fx |
| 97-112 | Break | break | `03_Break_16bars.mid` | hats, bass, chords, vox, accents, fx |
| 113-128 | Build | build | `04_Build_16bars.mid` | kick, hats, perc, bass, chords, fx |
| 129-160 | Drop | drop | `05_Drop_32bars.mid` | kick, snare, hats, perc, bass, lead, chords, vox, accents, fx |
| 161-192 | Drop 2 | drop2 | `06_Drop2_32bars.mid` | kick, snare, hats, perc, bass, lead, chords, vox, accents, fx |
| 193-224 | Outro | outro | `07_Outro_32bars.mid` | kick, snare, hats, perc |

Line by line (bars of the whole mix):

**1. Intro** (bars 1-32)
- 1-8: kick
- 9-16: kick, off-beat hats; a turnaround (big) in its last bar
- 17-24: kick, snare, hats; a turnaround in its last bar
- 25-32: kick, snare, hats, perc; a turnaround (big) in its last bar

**2. Groove** (bars 33-96)
- 33-40: kick, snare, hats, perc, bass (AcidLine1), accents; the Bass filter ride held at 0; a turnaround in its last bar
- 41-48: kick, snare, hats, perc, bass (AcidLine1), chords; the Bass filter ride from 0 to 8; a turnaround (big) in its last bar
- 49-56: kick, snare, hats, perc, bass (AcidLine1), the lead's call, chords, accents; the Bass filter ride from 8 to 20; a turnaround in its last bar
- 57-64: kick, snare, hats, perc, bass (AcidLine1), lead, chords, accents, fx; the Bass filter ride from 20 to 40; the fill in its last bar
- 65-72: kick, snare, hats, perc, bass (AcidLine1), the lead's call, chords (the metal chords), accents; the Bass filter ride from 40 to 46; a turnaround in its last bar
- 73-80: kick, snare, hats, perc, bass (AcidLine1), the lead's skeleton, chords with the metal chords, accents; the Bass filter ride from 46 to 58; a turnaround (big) in its last bar
- 81-88: kick, snare, hats, perc, bass (AcidLine1), lead, chords, accents; the drums' ride on 8ths variant; the Bass filter ride from 58 to 72; a turnaround in its last bar
- 89-96: kick, snare, hats, perc, bass (AcidLine1), the lead's skeleton, chords (the metal chords), the vox's last note thrown, accents, fx; the drums' ride on 8ths variant; the Bass filter ride from 72 to 92; the fill in its last bar

**3. Break** (bars 97-112)
- 97-104: chords (the metal chords), vox (riff), accents, fx
- 105-112: off-beat hats, bass (AcidLine1), chords, accents; the Bass filter ride from 0 to 24; the hats rise from 40% to 100% velocity; a turnaround (big) in its last bar

**4. Build** (bars 113-128)
- 113-120: kick, off-beat hats, full in its second half, perc, bass (AcidLine1), chords; the Bass filter ride from 24 to 80; a turnaround in its last bar
- 121-128: kick, hats, perc, bass (AcidLine1), chords, fx; the drums' build variant; the Bass filter ride 80 at bar 0 then 127 at bar 6; the perc rolls over the line's last 2 bars up to the fill, rising hit by hit; the hats rise from 50% to 100% velocity; the fill in its last bar

**5. Drop** (bars 129-160)
- 129-136: kick, snare, hats, perc, bass (AcidLine1), the lead's skeleton, vox (loop), accents, fx; the Bass filter ride as the loop's; a turnaround in its last bar
- 137-144: kick, snare, hats, perc, bass (AcidLine1), chords, accents with the rave stab riff; the Bass filter ride as the loop's; the drums out in its last bar
- 145-152: kick, snare, hats, perc, bass (AcidLine1), chords (the metal chords), vox (riff), accents; the Bass filter ride as the loop's; a turnaround in its last bar
- 153-160: kick, snare, hats, perc, bass (AcidLine1), the lead's skeleton, chords, accents with the rave stab riff, fx; the drums' ride on 8ths variant; the Bass filter ride as the loop's; the fill in its last bar

**6. Drop 2** (bars 161-192)
- 161-168: kick, snare, hats, perc, bass (AcidLine2), chords, accents with the acid stab riff, fx; the drums' drop 2 variant; the Bass filter ride held at 0; a turnaround in its last bar
- 169-176: kick, snare, hats, perc, bass (AcidLine2), the lead's skeleton, chords, accents with the acid stab riff; the drums' drop 2 variant; the Bass filter ride from 0 to 64; a turnaround (big) in its last bar
- 177-184: kick, snare, hats, perc, bass (AcidLine2), vox (riff), accents; the drums' peak variant; the Bass filter ride 64 at bar 0 then 127 at bar 6; a turnaround in its last bar
- 185-192: kick, snare, hats, perc, bass (AcidLine2), the lead's call, vox (answer), fx; the Bass filter ride 127 at bar 0 then 116 at bar 2 then 0 at bar 8; the fill in its last bar

**7. Outro** (bars 193-224)
- 193-200: kick, snare, hats, perc; the drums' drop 2 variant; a turnaround in its last bar
- 201-208: kick, hats, perc; the drums' ride on 8ths variant; a turnaround (big) in its last bar
- 209-216: kick, hats, perc; a turnaround in its last bar
- 217-224: kick, off-beat hats

## The cues

- bar 64: Fill
- bar 96: Fill
- bar 97: Vox
- bar 121: Riser
- bar 128: Fill
- bar 129: Vox
- bar 145: Vox
- bar 160: Fill
- bar 177: Vox
- bar 185: Vox
- bar 192: Fill

A riser's note ends on its section's first beat; a fill's on the next downbeat; the vox plays from a Vox cue.

## Tracks

Drums are named as drum racks name them (C1 = 36, so C3 = 60, as Ableton and Logic count); pitched notes as the sample
names count them, C4 = 60. The two meet at MIDI 60: root each sample at its MIDI number. The kit's README (`../../`)
has every instrument's zones and every one-key sound's file.

| Track | Channel | Lane | Notes |
|---|---|---|---|
| Kick | 10 | kick | 36 Kick Split |
| Rumble | 10 | kick | 35 Rumble |
| Snare | 10 | snare | 38 Snare Gated |
| Hats | 10 | hats | 42 Closed Hat, 46 Open Hat, 51 Ride |
| Perc | 10 | perc | 47 Anvil |
| Bass | 1 | bass | 28 E1, 31 G1, 33 A1, 35 B1, 36 C2, 38 D2, 40 E2, 43 G2 |
| Lead | 2 | lead | 71 B4, 74 D5, 76 E5, 78 F#5, 79 G5, 81 A5, 83 B5, 84 C6 |
| Chords | 4 | chords | 52 E3 |
| Chord voicing | 3 | chords | 52 E3, 59 B3, 64 E4, 67 G4, 78 F#5 |
| Metal Chord | 12 | chords | 64 E4, 69 A4, 71 B4 |
| Vox | 5 | vox | 52 E3, 55 G3, 59 B3, 62 D4 |
| Accents | 6 | accents | 48 Accent Rave Stab Em, 49 Accent Noise Sweep E, 50 Accent Reverse Hit Em, 54 Accent Dist Chord Em |
| Acid Stab | 11 | accents | 62 D4, 64 E4, 66 F#4, 67 G4, 69 A4, 71 B4 |
| Rave Stab | 13 | accents | 64 E4, 69 A4, 71 B4 |
| FX | 7 | fx | 48 Stutter Fill, 49 Iron Riser, 50 Accent Sub Drop E2 |

## On a timeline

Set the project to 136 BPM in 4/4 first: every file carries the tempo, but a DAW doesn't always take it from a
dropped file.
- **The whole mix:** drop `FUT04_ExtendedMix_Vox_224bars.mid` on bar 1: a track per instrument, in the order above.
- **Your own order:** drop the region files on bar lines, each where you want its section. A region starts on its first
  downbeat and ends on its last bar line, so regions laid end to end play exactly like the whole mix. Every region has
  the same tracks; if a DAW leaves an empty one out, move the clips under it onto the tracks of the same name.
- **Ableton Live:** drag the files into the Arrangement view: a file's tracks come in as clips on consecutive MIDI
  tracks. Clips keep the notes, not their channels: route each track to its instrument (`docs/midi-map.md`).
- **Logic Pro:** File > Open on the whole-mix file brings its tempo, key and section markers with it (an import
  doesn't); File > Import > MIDI File puts a software instrument track per MIDI track at the playhead.

Not checked yet: how the mix sounds, and how each DAW opens these files. Nothing here has been played in a DAW or
listened to.
