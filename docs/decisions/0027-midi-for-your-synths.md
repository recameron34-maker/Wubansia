# 0027. MIDI for your synths

**Status:** Proposed, 2026-11-18. Built with these choices, which you can change: the switch's words, "For my synths";
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
