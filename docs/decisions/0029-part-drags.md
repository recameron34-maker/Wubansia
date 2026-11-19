# 0029. Part drags

**Status:** Accepted, 2026-11-18, in under ADR 0028 (build in, cut later). Built with these choices, which you can cut
or change: the seven parts (Vox and Accents beyond the concepts' five) and their words; Accents holding the FX lane;
mutes not applying to a part; the part kept only while the instance lives; the `Song Drums` name form; segments sized
to their labels.

## Context
You asked to "pull in the full extended song midi regions for just specific parts such as just pulling in the full
extended midi for drum kits; and from there being able to put in my own drum samples". Until now Loop and Song gave the
whole kit less its muted lanes, and a lane's MIDI chip gave one lane in the play mode's files. ADR 0025 decision 4 left
out a Drums-only chip for want of room, and `dist/INSTALL.md` told you to mute six lanes to drag the drums alone. The
chosen concepts put a Part control on the drag strip, which has room now that the phrase chips are in the Phrases drawer
(ADR 0031).

Measured on the four kits' Loop and Song files: every track has a lane; Drums, Bass and Lead are byte-identical in the
vox and no-vox files, and so are FUT02-FUT04's Chords; FUT01's Chords and every kit's Accents differ; Vox is in the vox
files only; FUT03's Loops have no lead and no FX. In the Loops the Accents lanes hold only the one-key Accents and FX
tracks; the Songs add FUT02's Glass Ping (40 notes) and FUT04's Acid Stab and Rave Stab (104 and 80).

## Decision
1. **The parts**, in `plugin/source/Controller.h` (`DragPart`, `kDragPartNames`, `dragPartLanes`): Drums is the kick,
   snare, hats and perc lanes; Bass, Chords, Lead and Vox their own lane; Accents the accents and FX lanes. A kit's
   extras go with the lane kit.json gives them, so FUT04's Rumble is Drums and FUT02's Glass Ping Accents. Every lane is
   in exactly one part besides All.
2. **All is Loop and Song as before**: the same file, mutes, 1 drum track, For my synths and names.
3. **A part drag** is the conductor and the part's tracks, byte for byte and in file order, from the chip's own file
   (Loop or Song, whatever the play mode), **whatever you muted**. It picks between the vox and no-vox file as a lane
   drag does: the vox file, unless the switch is off and the part differs in the no-vox file, or the vox file has none
   of it. So a name stands for one file.
4. **The conductor stays whole** (tempo, 4/4, key, and in a Song every section marker and cue) and every kept track
   keeps its end, so a Loop part runs 4 bars and a Song part 224, lining up with the Song when dropped on the same bar.
5. **1 drum track** applies to Drums: two or more drum tracks become one track, Drums. No other part holds a drum track.
6. **For my synths** applies to every part through the same `madeForSynths` pass, its chord tables the kit's
   (`KitInfo::chordTracks`): a Chords part, and FUT04's Song Accents with its Rave Stab, come with whole chords. Drums
   is always the plain file; Accents is "(synths)" only in FUT02's and FUT04's Songs.
7. **Names**: `<KIT> <Title> <BPM> <Key> <Camelot> - <Loop|Song> <Part>[ NoVox][ (<marks>)].mid`, for example
   `FUT01 Alloy Signal 134 F#m 11A - Song Drums (1 drum track).mid` and
   `FUT01 Alloy Signal 134 F#m 11A - Song Chords NoVox (synths).mid`. A part never lists mutes. All keeps its names.
8. **The control**: the engraved Part and a segmented choice, All, Drums, Bass, Chords, Lead, Vox, Accents, between
   Song and 1 drum track (component `drag_part`, titled "Part"). Its segments are sized to their labels (363.7 px in
   DejaVu Sans, 354.7 in Liberation Sans), leaving 164 px right of 1 drum track in DejaVu Sans. Tab reaches it, the
   arrows move one part and stop at the ends, Return steps and wraps, Space goes to the host. Loop and Song say the part
   in their tooltips and titles ("Drag MIDI: Song, drums").
9. **Kept per instance**: `ControllerImpl` holds it, All when the instance starts. It survives closing the window, and
   is in neither `settings.xml` nor the project, so choosing it marks nothing changed.
10. **Loop and Song grey out with the reason** when there is nothing to drag: a part with none of its lanes in either of
    the chip's files ("FUT03's loop has no lead (Keys EP): drag the Song, or pick another part."), or All with every
    lane holding notes muted. The catalog reads each Loop and Song file's lanes once (`KitCard::fileLanes`).
11. **The code**: `dragout::lanesDrag` serves lane and part drags; `partDrag`, `trackLanes`, `whyNoDrag`;
    `SegmentedChoice` gains a fitted mode, Return that steps every time, arrows that tell onChange at once (so the
    strip's 30 Hz tick doesn't take a key back) and a focus outline.

## The four kits
| Kit | Drums | Bass | Chords | Lead | Vox | Accents |
|---|---|---|---|---|---|---|
| FUT01 | Kick, Clap, Hats, Perc | Bass, Organ Bass | Chords, Chord voicing, Air Pad | Lead, Organ Lead | Vox | Accents, FX |
| FUT02 | Kick, Clap, Hats, Perc | Bass, Drop Glide | Chords, Chord voicing, Laser Chord Stab | Lead | Vox | Accents, Glass Ping (Song), FX |
| FUT03 | Kick, Clap, Hats, Perc | Bass | Chords, Chord voicing, Pad Warm | Lead (Song only) | Vox | Accents, FX (Song only) |
| FUT04 | Kick, Rumble, Snare, Hats, Perc | Bass | Chords, Chord voicing, Metal Chord | Lead | Vox | Accents, Acid Stab and Rave Stab (Song), FX |

## Consequences
- New file names, which freeze with v0.3.
- No content, parameter, state, setting or dependency change; goldens and snapshots don't move.
- The SegmentedChoice fixes reach the mode panel's choices too.
- A part drag does an All drag's work on fewer tracks.
- Not checked in a DAW: whether a Song part dropped at bar 1 lines up in Live and Logic, and how they name the regions.

## Alternatives
- Parts in kit.json (`extras[].part`): a parser, doc and exporter change for nothing the four kits need. Add it if a
  future kit wants an extra outside its lane's part.
- The part as a `DragRequest` field: two sources of truth, as ADR 0025 found for its option.
- Kept in `settings.xml`: a remembered Drums would quietly make the next session's Song drag drums only. Kept in the
  project: ADR 0018 fixes the state's attributes, and saving it would mark projects changed.
- Parts that follow the mutes: muting what your own synth now plays and dragging its part again is the likely flow.
- The concepts' five parts: the vox line and the accent synths would be reachable only through All or a lane chip.
- An eighth FX part: about 134 px left after the fixed items, 25-35 px after the Drum kit; FX is 8-14 notes per Song,
  and its lane chip drags it alone.
- A drop-down: a hidden state. One chip per part: no room.
- FUT04's accent synths in Lead: they play on the Accents lane strip, where you mute and hear them.
