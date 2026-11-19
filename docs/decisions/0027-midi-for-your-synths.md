# 0027. MIDI for your synths

**Status:** Accepted, 2026-11-18, under ADR 0028 (build in, cut later). Built with these choices, which you can change: the switch's words, "For my synths";
on by default; the "(synths)" mark; the octave for the bass lane only; a 16th as the shortest note, 10 ticks between
notes and velocity 72 as the floor.

## Context
You wrote on 2026-11-18: "Ive also been noticing that the midis, especially the bass ones, are hard to use with my own
synths. I want this to be a tool thats more for allowing the user to be creative, than giving prepackaged songs."

Measured on the nine bass files (`content/midi/fut0N/*_Bassline_*.mid`, 960 PPQ, 161 notes; the Loops, Songs and bass
chips carry the same lines):
- They sit an octave below where a synth bass is played: MIDI 26-43, D0 to G1 in Live's and Logic's names.
- 111 of the 161 notes are shorter than a 16th. Wubansia's bass samples carry their own decay; a synth's envelope
  closes at the note-off.
- 23 notes, 1 to 6 per file, are slides written as notes that overlap the next: two voices at once on a polyphonic synth.
- 35 notes are softer than velocity 72, down to 44.
- No pitch bend or controller (the Loops' bass tracks carry a CC74 ride, which stays).

## Decision
1. **A switch on the drag strip, "For my synths"**, between the Drag MIDI label and Loop, **on by default**. Kept for you
   in `settings.xml` beside the drum option (`drag_for_synths="0"` while off, nothing while on), never in a project.
2. **Every MIDI drag follows it**: Loop, Song, each phrase chip and each lane's MIDI chip. It runs when you drag, in
   `core/midi/for_synths.h` (no JUCE), never on the audio thread: 1.4 to 3.6 ms for a 224-bar Song.
3. **On every pitched track** (by name: Bass, Lead, Chord voicing, Chords, Vox and the kits' extras), each note lasts a
   16th at least, unless the next note on its track starts sooner, and ends 10 ticks (a 96th of a beat) before the next
   note starts: no two notes overlap, and a slide becomes two plain notes. Notes that start together, a chord, don't cut
   each other; no note ends after the file. Velocities below 72 come up to 72; louder notes keep theirs.
4. **The bass lane goes up an octave**: Bass, FUT01's Organ Bass and FUT02's Drop Glide, 26-43 to 38-55 (D1 to G2).
5. **Untouched, byte for byte**: the drum tracks (and the merged Drums of ADR 0025), accents, FX and vox chops (one key
   per sound), the conductor (tempo, key, markers); on the rewritten tracks, the names and controllers. A track holding
   what the writer can't carry as it is (a pitch bend, say) stays as it is; no kit file has one.
6. **The name says so**, first among the marks: `... - Loop (synths).mid`, `... - Song NoVox (synths, 1 drum track, no
   Clap, Warp Bass).mid`, `... - Warp Bass Song (synths).mid`. A drag with no pitched track, a drum lane, is the plain
   one. Off, every drag is byte for byte as before.

## The nine bass files, before and after
| | Before | After |
|---|---|---|
| Keys | 26-43 (D0-G1) | 38-55 (D1-G2) |
| Notes shorter than a 16th | 111 of 161 | 54: 50 end 10 ticks before a note less than a 16th later (swung 16ths), 4 at the file's end |
| Notes overlapping the next | 23 | 0 |
| Notes ending less than 10 ticks before the next | 31 | 0 |
| Notes below velocity 72 | 35 (lowest 44) | 0 |

## Consequences
- New drag names. The content, the embedded MIDI and the generator snapshots don't change.
- To play a drag back into Wubansia, turn For my synths off first: lit, the bass comes an octave up, its slides become
  separate notes (212 in a FUT01 or FUT04 Song's Bass), short notes hold longer and soft notes play at velocity 72.
  `docs/midi-map.md` and `dist/INSTALL.md` say so.
- A chord sample's track carries only each chord's root in the kit's files: Chords (every kit), FUT01's Air Pad,
  FUT02's Laser Chord Stab, FUT03's Pad Warm, FUT04's Metal Chord and Rave Stab, and five phrases (FUT01's E Piano
  Stab Rhythm, FUT02's Laser Stab Riff, FUT03's Dub Chords, FUT04's Metal Chords and Rave Stab Riff). As first built, a
  synth played the root alone. Since "Chords as chords" (below), For my synths writes each as the chord its sample
  holds.
- The switch's room comes out of the phrase chips: a chip whose title needs more than an equal share gets it, and the
  others share the rest (FUT01 in DejaVu Sans: 131 px for "E Piano Stab Rhythm", 101 for the other four).
- No new parameter, state or dependency.
- Not checked in a DAW: how the files play on your synths in Live and Logic (`docs/listening.md`, v0.2's checklist).

## Alternatives
- **Off by default.** You asked for MIDI to use with your own synths, and the samples' files are one click away.
- **Slides as glide (pitch bend or legato).** Synths glide in their own ways; plain notes play on every one, and your
  synth's glide can come back on top.
- **New files in `content/midi`.** They would change what Wubansia plays and its tests; the drag-time rewrite changes
  nothing else.

## Chords as chords (2026-11-18)

Added in v0.3 under ADR 0028 (build in, cut later); the status line above stands. With For my synths lit, every note on
a chord sample's track is written as the chord that sample plays: the tracks above, in every drag that goes through For
my synths (Loop, Song, a lane's MIDI chip, the five phrases).

**Where the exact chord comes from (measured).** A note on key K plays its zone's sample repitched by K less the zone's
root, so it sounds K plus the zone's voicing, which the recipes set (`tools/sound/recipes/fut0N`). Over every
chord-sample note of every kit's Loop and Song (vox and no-vox), its 4-bar Chords file and the five chord phrase files,
3,301 notes (`docs/plans/v0.3-plan.md` section 9, command 1):

| Source | Exact chord | Wrong | None |
|---|---|---|---|
| The Chord voicing track's notes on the note's tick | 2,171: every Chords note of every Loop and Song, with the same length and velocity | 0 | 1,130: every note of the extras, the 4-bar Chords files and the phrase files |
| The instrument's name read as a chord symbol | 0: a name gives no voicing | 646: the repitched sample plays another chord than its name says (Air Pad F#m9 played as Bm9) | 1,521: names with no chord in them (Organ Stab, Laser Chord Stab, Pad Warm) |

So the voicing track is exact but covers only Chords. kit.json now carries each chord sample's voicing
(`zones[].voicing`, `docs/plugin/manifest.md`), exported from the recipe constants the voicing track was written from
(each kit's `plugin.py` `VOICINGS`): 13 zones on 10 instruments. A content test holds it to the voicing track at all
2,171 notes it witnesses, and `tools/sound`'s tests hold it to the recipes.

**The register.** The For my synths bass is 38-55 (decision 4; its top key is 54 in FUT01 and 55 in FUT02-FUT04). A
chord is lifted whole octaves until its lowest note is 56 (G#2) or more, less where its top would pass 127. Of the 18
distinct chords (command 2), 6 are lifted an octave: FUT01's F#m9 on the stabs and the Air Pad, FUT02's Starfield Dm9,
FUT03's Gm9 on Dub Stab and Pad Warm, and FUT04's drone; 1,453 of the 3,301 notes. Each chord is lifted on its own, so
on those four FUT01 and FUT03 tracks the lifted chord sits above the other one, the reverse of the samples
(Alternatives). Every chord then lies within keys 58-92. In the drags (the 4-bar Chords files aren't dragged): 3,268
notes come as 15,856 notes on 58-92, the bass lane's on 55 or below.

**Lengths and velocities.** Every note of a chord starts on the chord note's tick, on its channel, with its length
and velocity; then the rules of decision 3 apply. The notes start together, so they never cut each other, and each
ends where the chord note alone would have ended.

**The tracks.** Chords (channel 4: FUT01's Organ Stab and E Piano Stab on the switch voice alike), FUT01's Air Pad,
FUT02's Laser Chord Stab, FUT03's Pad Warm, FUT04's Metal Chord and Rave Stab. The one-key Accents that play the same
samples stay as they are. The Chord voicing track stays a pitched track at the samples' own register.

**The tooltip** says it in 464 characters (699 before) and points to `dist/INSTALL.md`, "Drag MIDI into Live or Logic".

### Consequences
- kit.json gains an optional key, `zones[].voicing`; the engine doesn't play it. No file, hash or other key changes, so
  installed content stays valid.
- The Chord voicing track remains, beside a Chords track that now plays the same chords: on a synth, play one or the
  other.
- To play a drag back into Wubansia, turn For my synths off first: lit, each chord sample's note also comes as a whole
  chord.
- Not checked in a DAW: how the chords sound on your synths in Live and Logic.

### Alternatives
- **The Chord voicing track alone.** Exact, but only beside Chords: it misses the 1,130 notes of the extras, the 4-bar
  Chords files and the phrases.
- **The instrument names.** They give no voicing, and 646 notes play another chord than the name says.
- **One lift per track.** The samples play F#m9 a fourth below Bm9 (FUT01's stabs and Air Pad) and Gm9 a fourth below
  Cm9 (FUT03's Dub Stab and Pad Warm, the Dub Chords phrase among them). Lifted per chord, F#m9 and Gm9 come a fifth
  above instead: on the stabs 66-81 over Bm9's 59-74, on the Air Pad 66-92 over 59-85, on Dub Stab and Pad Warm 67-82
  over Cm9's 60-75. One lift per track would keep the samples' order but put FUT01's Bm9 at 71-86 (71-97 on the Air Pad,
  past the 92 every chord now stays within) and FUT03's Cm9 at 72-87. Yours to ask for if you prefer it.
- **Dropping the Chord voicing track from synth drags.** It is the kit's own written-out voicing, at the samples'
  register; muting it is one click in a DAW.
