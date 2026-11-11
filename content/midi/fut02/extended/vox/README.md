# FUT02 "Orbit Relay": the extended mix, with the vox synth

The kit's written music arranged as a DJ-friendly extended mix: 224 bars, 6:54 at 130 BPM, in
D minor (7A), swing 52% written in, every section on an 8-bar line, 32 bars of drums at
each end. Made by `tools/sound/midi.py` from the kit's PARTS on the channel map in `docs/midi-map.md`; `../novox/` is
the same mix without the vox synth. A test keeps these files equal to a fresh
export.

- `FUT02_ExtendedMix_Vox_224bars.mid`: the whole mix, a track per instrument, a marker at every section start and cue points (labels: the
  notes play) at the risers, fills and vox entries.
- One region per section, `01_Intro_32bars.mid` to `09_Outro_32bars.mid`: the mix cut at its section lines, with the same
  tracks in the same order, the section's marker and its cues.
- `regions.json`: integers and strings only: the tempo, swing (permille) and length (ms), every section's start bar
  (from 1), length, kind and lanes, line by line, its cues, the tracks and the filter rides.

## The sections

| Bars | Section | Kind | Region | Lanes |
|---|---|---|---|---|
| 1-32 | Intro | intro | `01_Intro_32bars.mid` | kick, snare, hats, perc |
| 33-64 | Groove | groove | `02_Groove_32bars.mid` | kick, snare, hats, perc, bass, lead, vox, accents, fx |
| 65-80 | Break | break | `03_Break_16bars.mid` | hats, chords, vox, accents, fx |
| 81-96 | Build | build | `04_Build_16bars.mid` | kick, snare, hats, bass, lead, chords, fx |
| 97-128 | Drop | drop | `05_Drop_32bars.mid` | kick, snare, hats, perc, bass, lead, chords, vox, accents, fx |
| 129-144 | Switch-up | switch | `06_SwitchUp_16bars.mid` | kick, snare, hats, perc, bass, lead, accents |
| 145-160 | Break 2 | break | `07_Break2_16bars.mid` | hats, chords, vox, accents, fx |
| 161-192 | Drop 2 | drop2 | `08_Drop2_32bars.mid` | kick, snare, hats, perc, bass, lead, chords, vox, accents, fx |
| 193-224 | Outro | outro | `09_Outro_32bars.mid` | kick, snare, hats, perc |

Line by line (bars of the whole mix):

**1. Intro** (bars 1-32)
- 1-8: kick
- 9-16: kick, off-beat hats, perc; a turnaround in its last bar
- 17-24: kick, clap, hats, perc; a turnaround in its last bar
- 25-32: kick, clap, busy hats, perc; a turnaround in its last bar

**2. Groove** (bars 33-64)
- 33-40: kick, clap, hats, perc, bass (Orbit), accents; a turnaround in its last bar
- 41-48: kick, clap, hats, perc, bass (Orbit), the lead's skeleton, accents, fx; the Lead filter ride held at 12; the fill in its last bar
- 49-56: kick, clap, busy hats, perc, bass (Orbit), the lead's skeleton, accents; the Lead filter ride held at 20; a turnaround in its last bar
- 57-64: kick, clap, hats, perc, bass (Orbit), the lead's skeleton, the vox's last note thrown, accents, fx; the Lead filter ride from 20 to 40; the fill in its last bar

**3. Break** (bars 65-80)
- 65-72: chords (choir), vox (the vox choir chords), accents
- 73-80: off-beat hats, chords (choir), vox (the vox choir chords), accents, fx; the phrases rest from the fill; the hats rise from 40% to 100% velocity; the fill in its last bar

**4. Build** (bars 81-96)
- 81-88: kick, clap, off-beat hats, bass (Orbit), the lead's skeleton, chords; the Lead filter ride from 0 to 64; a turnaround in its last bar
- 89-96: clap, off-beat hats, 16ths in its second half, the lead's skeleton, chords, fx; the Lead filter ride from 64 to 127; the clap rolls over the line's last 8 bars up to the fill, rising hit by hit; the fill in its last bar

**5. Drop** (bars 97-128)
- 97-104: kick, clap, hats, perc, bass (Orbit), lead, vox (loop), accents, fx; the drums' pingpong variant; the Lead filter ride 127 at bar 0 then 127 at bar 4 then 0 at bar 8; a turnaround in its last bar
- 105-112: kick, clap, busy hats, perc, bass (Orbit), lead, vox (loop), accents, fx; the fill in its last bar
- 113-120: kick, clap, hats, perc, bass (Orbit), lead, chords (the laser stab riff); the drums' pingpong variant; a turnaround in its last bar
- 121-128: kick, clap, busy hats, perc, bass (Orbit), the lead's skeleton, chords (the laser stab riff), vox (call), thrown, accents; the drums out in its last bar

**6. Switch-up** (bars 129-144)
- 129-136: kick, clap, busy hats, perc, bass (Orbit), accents; the drums' tresillo variant; a turnaround in its last bar
- 137-144: kick, clap, hats, perc, bass (Orbit), the lead's skeleton, accents; the drums' tresillo variant; the Lead filter ride held at 24; a turnaround in its last bar

**7. Break 2** (bars 145-160)
- 145-152: off-beat hats, chords (choir), vox (the vox choir chords), accents
- 153-160: off-beat hats, chords (the laser stab riff), vox (call), accents, fx; the phrases rest from the fill; the hats rise from 40% to 100% velocity; the fill in its last bar

**8. Drop 2** (bars 161-192)
- 161-168: kick, clap, hats, perc, bass (Relay), vox (call), accents (the glass melody), fx; the drums' tresillo variant; a turnaround in its last bar
- 169-176: kick, clap, busy hats, perc, bass (Relay), the lead's skeleton, vox (call), accents, fx; the fill in its last bar
- 177-184: kick, clap, hats, perc, bass (Relay), the lead's call, vox (answer), accents; the drums' pingpong variant; a turnaround in its last bar
- 185-192: kick, clap, busy hats, perc, bass (Relay), chords (the laser stab riff), fx; the phrases rest from the fill; the fill in its last bar

**9. Outro** (bars 193-224)
- 193-200: kick, clap, busy hats, perc; the drums' pingpong variant; a turnaround in its last bar
- 201-208: kick, clap, hats, perc; the drums' tresillo variant; a turnaround in its last bar
- 209-216: kick, hats, perc; a turnaround in its last bar
- 217-224: kick, off-beat hats

## The cues

- bar 48: Fill
- bar 64: Fill
- bar 65: Vox
- bar 80: Fill
- bar 89: Riser
- bar 96: Fill
- bar 97: Vox
- bar 112: Fill
- bar 121: Vox
- bar 145: Vox
- bar 153: Riser
- bar 160: Fill
- bar 161: Vox
- bar 176: Fill
- bar 177: Vox
- bar 192: Fill

A riser's note ends on its section's first beat; a fill's on the next downbeat; the vox plays from a Vox cue.

## Tracks

Drums are named as drum racks name them (C1 = 36, so C3 = 60, as Ableton and Logic count); pitched notes as the sample
names count them, C4 = 60. The two meet at MIDI 60: root each sample at its MIDI number. The kit's README (`../../`)
has every instrument's zones and every one-key sound's file.

| Track | Channel | Lane | Notes |
|---|---|---|---|
| Kick | 10 | kick | 36 Kick Split |
| Clap | 10 | snare | 38 Electro Clap |
| Hats | 10 | hats | 42 Closed Hat, 46 Open Hat |
| Perc | 10 | perc | 47 Zap |
| Bass | 1 | bass | 26 D1, 29 F1, 31 G1, 33 A1, 34 A#1, 36 C2, 38 D2, 41 F2, 43 G2 |
| Drop Glide | 11 | bass | 26 D1 |
| Lead | 2 | lead | 69 A4, 72 C5, 74 D5, 76 E5, 77 F5, 79 G5, 81 A5, 84 C6 |
| Chords | 4 | chords | 50 D3 |
| Chord voicing | 3 | chords | 50 D3, 57 A3, 60 C4, 65 F4, 69 A4, 76 E5 |
| Laser Chord Stab | 12 | chords | 58 A#3, 62 D4 |
| Vox | 5 | vox | 62 D4, 65 F4, 69 A4, 70 A#4, 72 C5, 74 D5, 76 E5, 77 F5, 79 G5, 81 A5 |
| Accents | 6 | accents | 48 Laser Chord Stab D4, 49 Laser Chord Stab A#3, 50 Glass Ping, 51 Starfield Sweep, 52 Reverse Chord, 53 Arp Stab, 54 Noise Uplifter, 55 Resonant Zap |
| Glass Ping | 13 | accents | 77 F5, 79 G5, 81 A5, 84 C6, 86 D6, 88 E6, 89 F6 |
| FX | 7 | fx | 48 Laser Fill, 49 Laser Riser, 50 Sub Drop |

## On a timeline

Set the project to 130 BPM in 4/4 first: every file carries the tempo, but a DAW doesn't always take it from a
dropped file.
- **The whole mix:** drop `FUT02_ExtendedMix_Vox_224bars.mid` on bar 1: a track per instrument, in the order above.
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
