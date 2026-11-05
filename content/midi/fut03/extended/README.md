# FUT03 "Glass Current": the extended mix

The kit's written loop arranged as a DJ-friendly extended mix: 224 bars, 7:00 at 128 BPM, in
G minor (6A), swing 56% written in, every section on an 8-bar line. Made by
`tools/sound/midi.py` from the kit's written parts only (the drums, basslines, lead and chords in `../`, whose README
has the drum map); a test keeps these files equal to a fresh export.

- `FUT03_ExtendedMix_224bars.mid`: the whole mix, a track per lane, a marker at every section start, and cue points (MIDI cue point
  events) where the kit's riser, fills and vocals go.
- One region per section, `01_Intro_32bars.mid` to `09_Outro_32bars.mid`: the whole mix cut at its section lines, with
  the same tracks, the section's marker and its cues.
- `regions.json`: every section's start bar, length, kind and lanes, line by line, with its cues and the tracks.

## The sections

| Bars | Section | Kind | Region | Lanes |
|---|---|---|---|---|
| 1-32 | Intro | intro | `01_Intro_32bars.mid` | kick, clap, hats, perc, bass, chords |
| 33-64 | Groove | groove | `02_Groove_32bars.mid` | kick, clap, hats, perc, bass, lead, chords |
| 65-80 | Break | break | `03_Break_16bars.mid` | hats, lead, chords |
| 81-96 | Build | build | `04_Build_16bars.mid` | kick, clap, hats, bass, chords |
| 97-128 | Drop | drop | `05_Drop_32bars.mid` | kick, clap, hats, perc, bass, lead, chords |
| 129-144 | Switch-up | switch | `06_SwitchUp_16bars.mid` | kick, clap, hats, perc, bass, chords |
| 145-160 | Break 2 | break | `07_Break2_16bars.mid` | hats, lead, chords |
| 161-192 | Drop 2 | drop2 | `08_Drop2_32bars.mid` | kick, clap, hats, perc, bass, lead, chords |
| 193-224 | Outro | outro | `09_Outro_32bars.mid` | kick, clap, hats, perc, bass, chords |

Line by line (bars of the whole mix):

**1. Intro** (bars 1-32, `01_Intro_32bars.mid`)
- 1-8: kick
- 9-16: + off-beat hats, perc
- 17-24: + clap, bass (Rubber); full hats
- 25-32: + chords; fill in bar 32

**2. Groove** (bars 33-64, `02_Groove_32bars.mid`)
- 33-40: kick, clap, hats, perc, bass (Rubber), chords
- 41-48: as before; fill in bar 48
- 49-56: + the lead's call (its first bar)
- 57-64: as before; fill in bar 64

**3. Break** (bars 65-80, `03_Break_16bars.mid`)
- 65-72: lead, chords; vox cue
- 73-80: + off-beat hats; the hats rise from 40% to full velocity; vox cue; fill in bar 80

**4. Build** (bars 81-96, `04_Build_16bars.mid`)
- 81-88: kick, clap, off-beat hats, bass (Rubber), chords
- 89-96: − hats, bass; the clap rolls: every quarter for 4 bars, every 8th for 2, then every 16th, into its fill bar; the clap rises from 55% to full velocity; riser cue; fill in bar 96

**5. Drop** (bars 97-128, `05_Drop_32bars.mid`)
- 97-104: every lane, bass (Rubber); vox cue
- 105-112: as before; fill in bar 112
- 113-120: as before; vox cue
- 121-128: as before; drums out in bar 128

**6. Switch-up** (bars 129-144, `06_SwitchUp_16bars.mid`)
- 129-136: kick, clap, busy hats, perc, bass (Deep)
- 137-144: + chords; fill in bar 144

**7. Break 2** (bars 145-160, `07_Break2_16bars.mid`)
- 145-152: chords; vox cue
- 153-160: + off-beat hats, lead; the hats rise from 40% to full velocity; riser cue

**8. Drop 2** (bars 161-192, `08_Drop2_32bars.mid`)
- 161-168: every lane, busy hats, bass (Deep); vox cue
- 169-176: as before; fill in bar 176
- 177-184: as before; vox cue
- 185-192: as before; fill in bar 192

**9. Outro** (bars 193-224, `09_Outro_32bars.mid`)
- 193-200: kick, clap, hats, perc, bass (Rubber), chords
- 201-208: − chords
- 209-216: − clap, bass; off-beat hats
- 217-224: − hats, perc

## The cues

The MIDI plays the kit's drums, bass, lead and chords. Its riser, fills and vocals are audio, in `content/samples/fut03/`;
the cue points, and this list, say where they go:
- **Riser** (bars 89, 153): the kit's riser (`FX/`), 8 bars from this bar, ending on the next section's first beat.
- **Fill** (bars 32, 48, 64, 80, 96, 112, 144, 176, 192): the kit's fill (`FX/`) in this bar, its end on the next downbeat (a half-bar fill starts on beat 3); the drums play their written fill bar here, which leaves it room.
- **Vox** (bars 65, 73, 97, 113, 145, 161, 177): the kit's sung line or chops (`Vox/`) from this bar.

## Tracks

Drums are named as drum racks name them (C1 = 36, so C3 = 60, as Ableton and Logic count); the samples' roots are
named as the sample names count, C4 = 60. The two meet at MIDI 60: root each file by its MIDI number.

| Track | Channel | Load and play |
|---|---|---|
| Kick | 10 | 36 (C1) Kick Deep |
| Clap | 10 | 38 (D1) Clap Snare |
| Hats | 10 | 42 (F#1) Closed Hat Deep, 46 (A#1) Open Hat Deep |
| Perc | 10 | 37 (C#1) Rimshot, 41 (F1) Conga G3, 43 (G1) Conga C4, 45 (A1) Conga D4, 47 (B1) Bongo G4, 48 (C2) Bongo D5, 54 (F#2) Shaker Deep |
| Bass | 1 | Rubber Bass Clean (A) or Rubber Bass Warm (B), with Sub Deep under either, rooted at G1, D2, G2 (MIDI 31, 38, 43): Rubber (the context loop's pick): intro, groove, build, drop and outro; Deep: switch-up and drop 2 |
| Lead | 2 | Keys EP, rooted at G3, D4, G4, D5, G5 (MIDI 55, 62, 67, 74, 79) |
| Chords | 3 | Dub stab Gm9, off the kicks: the voicing, for your own synth |
| Chord trigger | 4 | the Dub Stab Gm9 sample, the whole chord: one note on C3 (MIDI 60) per chord, where samplers play a sample at its own pitch |

The four drum tracks share the kit's drum map on channel 10: give each the kit's drum rack, or move their notes onto
one drum track. The closed and open hats share the Hats track, so the rack's choke group still works.

## On a timeline

Set the project to 128 BPM in 4/4 first: every file carries the tempo, but a DAW doesn't always take it from a
dropped file.
- **The whole mix:** drop `FUT03_ExtendedMix_224bars.mid` on bar 1. It comes in as a track per lane, in the order above. Its section
  markers come in where the DAW reads MIDI markers; the table above has every section's bars either way.
- **Your own order:** drop the region files on bar lines, each where you want that section. A region starts on its
  first downbeat and ends on its last bar line, so regions laid end to end play exactly like the whole mix. Every
  region has the same eight tracks in the same order; a lane that rests for a whole section is an empty track there.
  If a DAW leaves an empty track out, the lanes under it come in one track up: drag those clips onto their tracks.
- **Ableton Live:** drag the files into the Arrangement view: a file's tracks come in as clips on consecutive MIDI
  tracks, all starting where you drop it. Set the locators from the section table.
- **Logic Pro:** open the whole-mix file (File > Open) to bring its tempo and section markers with it: Logic reads
  those when it opens a MIDI file, not when it imports one. Into a project, drag a file into the Tracks area or use
  File > Import > MIDI File: a software instrument track per MIDI track, at the playhead.

## How it uses the loop

Every note is one of the kit's written notes, arranged; only the build's roll plays one (the clap's) on a rhythm
the loop doesn't write:
- **The fill bar.** The written drums' bar 4 leaves room for the kit's fill, as in the context loop.
  Where no fill is due, bar 2 plays in its place, so the groove runs on; at a Fill cue the four bars play as
  written.
- **Hats:** as written; off-beat (the off-beat 8ths alone); busy (as written, plus the kit's closed hat at its softest
  written velocity on every empty "e" and "a" of a beat the hats play), in the switch-up and drop 2.
- **Bass:** Rubber (the context loop's pick): intro, groove, build, drop and outro; Deep: switch-up and drop 2.
- **Lead:** in the groove only its call, the phrase's first bar; the whole phrase in the breaks and the drops.
- **The build's roll:** over the build's last 8 bars, under the riser, the clap rolls on its own note, every quarter
  for 4 bars, every 8th for 2, every 16th for 1, then the written fill bar for the kit's fill, whose end is the drop's
  first beat. The kick plays on under it, as future house builds keep theirs.
- **Rises:** in the build's roll the clap climbs from 55% to full velocity, bar by bar; in the breaks the off-beat
  hats climb from 40%.
- **Drum drop:** the drums sit out the drop's last bar, before the switch-up.
- **Slides:** Where a line glides into a note, the note before is held 12 ticks into it, so a synth set to legato glide slides there. 2 more slides glide inside one note, which a MIDI note can't carry: they play as written.

Not checked yet: how the mix sounds, and how each DAW opens these files. Nothing here has been played in a DAW or
listened to.
