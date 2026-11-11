# FUT03 "Glass Current": the extended mix, with the vox synth

The kit's written music arranged as a DJ-friendly extended mix: 224 bars, 7:00 at 128 BPM, in
G minor (6A), swing 56% written in, every section on an 8-bar line, 32 bars of drums at
each end. Made by `tools/sound/midi.py` from the kit's PARTS on the channel map in `docs/midi-map.md`; `../novox/` is
the same mix without the vox synth. A test keeps these files equal to a fresh
export.

- `FUT03_ExtendedMix_Vox_224bars.mid`: the whole mix, a track per instrument, a marker at every section start and cue points (labels: the
  notes play) at the risers, fills and vox entries.
- One region per section, `01_Intro_32bars.mid` to `09_Outro_32bars.mid`: the mix cut at its section lines, with the same
  tracks in the same order, the section's marker and its cues.
- `regions.json`: integers and strings only: the tempo, swing (permille) and length (ms), every section's start bar
  (from 1), length, kind and lanes, line by line, its cues, the tracks and the filter rides.

## The sections

| Bars | Section | Kind | Region | Lanes |
|---|---|---|---|---|
| 1-32 | Intro | intro | `01_Intro_32bars.mid` | kick, snare, hats, perc |
| 33-64 | Groove | groove | `02_Groove_32bars.mid` | kick, snare, hats, perc, bass, lead, chords, vox, accents |
| 65-80 | Break | break | `03_Break_16bars.mid` | hats, lead, chords, vox, accents, fx |
| 81-96 | Build | build | `04_Build_16bars.mid` | kick, snare, hats, perc, bass, chords, accents, fx |
| 97-128 | Drop | drop | `05_Drop_32bars.mid` | kick, snare, hats, perc, bass, lead, chords, vox, accents, fx |
| 129-144 | Switch-up | switch | `06_SwitchUp_16bars.mid` | kick, snare, hats, perc, bass, lead, chords, accents, fx |
| 145-160 | Break 2 | break | `07_Break2_16bars.mid` | hats, lead, chords, accents, fx |
| 161-192 | Drop 2 | drop2 | `08_Drop2_32bars.mid` | kick, snare, hats, perc, bass, lead, chords, vox, accents, fx |
| 193-224 | Outro | outro | `09_Outro_32bars.mid` | kick, snare, hats, perc |

Line by line (bars of the whole mix):

**1. Intro** (bars 1-32)
- 1-8: kick, perc; the drums' shaker variant; a turnaround (run) in its last bar
- 9-16: kick, off-beat hats, perc; the drums' shaker variant; the hats rise from 50% to 100% velocity; a turnaround (run) in its last bar
- 17-24: kick, clap, hats, perc; the drums' shaker variant; a turnaround (run) in its last bar
- 25-32: kick, clap, hats, perc; a turnaround in its last bar

**2. Groove** (bars 33-64)
- 33-40: kick, clap, hats, perc, bass (Rubber), accents; the drums' lite variant; a turnaround in its last bar
- 41-48: kick, clap, hats, perc, bass (Rubber), chords, accents; the Chords filter ride from 0 to 42; a turnaround in its last bar
- 49-56: kick, clap, hats, perc, bass (Rubber), lead, chords, accents; the drums' lite variant; the Chords filter ride from 42 to 85; a turnaround in its last bar
- 57-64: kick, clap, hats, perc, bass (Rubber), the lead's answer, chords, vox (call), accents; the Chords filter ride from 85 to 127; the drums out in its last bar

**3. Break** (bars 65-80)
- 65-72: pad (Gm9 Cm9), vox (riff), accents
- 73-80: off-beat hats, the lead's call, chords (Gm9 Cm9), pad (Gm9 Cm9), vox (riff), fx; the hats rise from 40% to 100% velocity; the fill in its last bar

**4. Build** (bars 81-96)
- 81-88: kick, off-beat hats, perc, bass (Rubber), chords (Gm9 Cm9), pad (Gm9 Cm9), accents; the drums' shaker variant; the Chords filter ride from 16 to 96; the perc rise from 60% to 100% velocity; a turnaround (run) in its last bar
- 89-96: kick, clap, off-beat hats, 16ths in its second half, perc, chords (Gm9 Cm9), pad (Gm9 Cm9), fx; the drums' shaker variant; the Chords filter ride from 96 to 127; the clap rolls over bars 93-95 up to the fill, rising hit by hit; the fill in its last bar

**5. Drop** (bars 97-128)
- 97-104: kick, clap, hats, perc, bass (Rubber), chords (Gm9 Cm9), pad (Gm9 Cm9), vox (riff), accents, fx; the drums' drop lite variant; the Chords filter ride 127 at bar 0 then 127 at bar 2 then 0 at bar 8; a turnaround (drop) in its last bar
- 105-112: kick, clap, hats, perc, bass (Rubber), the lead's call, chords (Gm9 Cm9), pad (Gm9 Cm9), vox (answer), accents; the drums' drop variant; a turnaround (drop) in its last bar
- 113-120: kick, clap, busy hats, perc, bass (Rubber), lead, chords (Gm9 Gm9 Cm9 Gm9), pad (Gm9 Gm9 Cm9 Gm9), accents; the drums' drop lite variant; a turnaround (drop) in its last bar
- 121-128: kick, clap, hats, busy in its second half, perc, bass (Rubber), the lead's call, chords (Gm9 Cm9), pad (Gm9 Cm9), vox (riff), accents; the drums' drop variant; the drums out in its last bar

**6. Switch-up** (bars 129-144)
- 129-136: kick, clap, busy hats, perc, bass (the rubber riff), chords, accents; the drums' lite variant; the Chords filter ride held at 24; a turnaround in its last bar
- 137-144: kick, clap, busy hats, perc, bass (the rubber riff), lead, chords, accents, fx; the Chords filter ride from 24 to 110; the phrases rest from the fill; the fill in its last bar

**7. Break 2** (bars 145-160)
- 145-152: lead (the keys arp), pad (Gm9 Cm9), accents
- 153-160: off-beat hats, lead (the keys arp), chords (Gm9 Cm9), pad (Gm9 Cm9), accents, fx; the phrases rest from the fill; the hats rise from 40% to 100% velocity; the fill in its last bar

**8. Drop 2** (bars 161-192)
- 161-168: kick, clap, busy hats, perc, bass (Deep), lead (the keys arp), chords (Gm9 Cm9), pad (Gm9 Cm9), accents, fx; the drums' drop lite variant; a turnaround (drop) in its last bar
- 169-176: kick, clap, busy hats, perc, bass (Deep), the lead's call, chords (Gm9 Cm9), pad (Gm9 Cm9), vox (riff), accents; the drums' drop variant; a turnaround (drop) in its last bar
- 177-184: kick, clap, busy hats, 16ths in its second half, perc, bass (Deep), lead (the keys arp), chords (Gm9 Cm9), pad (Gm9 Cm9), accents; the drums' drop lite variant; the Chords filter ride held at 127; a turnaround (drop) in its last bar
- 185-192: kick, clap, busy hats, perc, bass (Deep), chords, accents; the drums' drop variant; the Chords filter ride from 127 to 0; a turnaround (drop) in its last bar

**9. Outro** (bars 193-224)
- 193-200: kick, clap, busy hats, perc; the drums' lite variant; a turnaround in its last bar
- 201-208: kick, busy hats, perc; a turnaround in its last bar
- 209-216: kick, hats, perc; the drums' lite variant; a turnaround in its last bar
- 217-224: kick, hats, perc; the drums' shaker variant; a turnaround (run) in its last bar

## The cues

- bar 57: Vox
- bar 65: Vox
- bar 80: Fill
- bar 89: Riser
- bar 96: Fill
- bar 97: Vox
- bar 121: Vox
- bar 144: Fill
- bar 153: Riser
- bar 160: Fill
- bar 169: Vox

A riser's note ends on its section's first beat; a fill's on the next downbeat; a Vox cue sits on the bar where
its line's vox comes in.

## Tracks

Drums are named as drum racks name them (C1 = 36, so C3 = 60, as Ableton and Logic count); pitched notes as the sample
names count them, C4 = 60. The two meet at MIDI 60: root each sample at its MIDI number. The kit's README (`../../`)
has every instrument's zones and every one-key sound's file.

| Track | Channel | Lane | Notes |
|---|---|---|---|
| Kick | 10 | kick | 36 Kick Deep |
| Clap | 10 | snare | 38 Clap Snare |
| Hats | 10 | hats | 42 Closed Hat Deep, 46 Open Hat Deep |
| Perc | 10 | perc | 37 Rimshot, 41 Conga, 43 Conga, 45 Conga, 47 Bongo, 48 Bongo, 54 Shaker Deep |
| Bass | 1 | bass | 29 F1, 31 G1, 33 A1, 34 A#1, 36 C2, 38 D2, 41 F2, 43 G2 |
| Lead | 2 | lead | 67 G4, 69 A4, 70 A#4, 72 C5, 74 D5, 75 D#5, 77 F5, 79 G5, 81 A5, 82 A#5, 86 D6 |
| Chords | 4 | chords | 55 G3, 60 C4 |
| Chord voicing | 3 | chords | 55 G3, 60 C4, 62 D4, 65 F4, 67 G4, 69 A4, 70 A#4, 74 D5, 75 D#5 |
| Pad Warm | 11 | chords | 55 G3, 60 C4 |
| Vox | 5 | vox | 67 G4, 70 A#4, 72 C5, 74 D5, 75 D#5, 77 F5 |
| Accents | 6 | accents | 48 Accent Glass Ping D6, 49 Accent Dub Chord Cm9, 50 Accent Reverse Chord Gm9, 51 Accent Organ Hit Gm7, 52 Accent Noise Sweep Up |
| FX | 7 | fx | 48 Build Fill, 49 Riser, 50 Impact |

## On a timeline

Set the project to 128 BPM in 4/4 first: every file carries the tempo, but a DAW doesn't always take it from a
dropped file.
- **The whole mix:** drop `FUT03_ExtendedMix_Vox_224bars.mid` on bar 1: a track per instrument, in the order above.
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
