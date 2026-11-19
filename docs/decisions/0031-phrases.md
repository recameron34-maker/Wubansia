# 0031. The Phrases drawer

**Status:** Accepted, 2026-11-18, in under ADR 0028 (build in, cut later). Built with these choices, which you can cut
or change: the button's place and word (Phrases, left of Samples); From the kit holding the kit's lead line as well as
its phrases; a click on a phrase showing its file; the phrase chips leaving the drag strip. This is part 1, the drawer
with the kit's own phrases; step `phrases` adds the generated library.

## Context
On 2026-11-18 you asked for "more 8 bar melodic samples, dont have to be related to the four songs youve built, but
rather cool melodic phrases that can add to songs that are build using the midis and chords provided in teh app". The
v0.3 plan (`docs/plans/v0.3-plan.md`) gives phrases a drawer of their own, filled in two steps: first the kit's own
written phrases, then a library written over the kit's own chords.

Until now each kit's phrase files were chips on the drag strip. Part drags (ADR 0029) and the Drum kit button (ADR 0030)
need that room: with Part on the strip, FUT01's five phrase chips would get 152 px and need at least 324 (5 chips of
60 px, 4 gaps of 6; `plan-part-drags.md` §4.2, summed up in the v0.3 plan's §1).

## Decision
1. **A Phrases button** in the header, left of Samples, lit while the drawer shows. Its tooltip: "Browse phrases for
   this kit and drag them into your DAW as MIDI".
2. **The Phrases drawer** sits where the Samples drawer does, at (20, 246), 1080 x 396, over the lanes, A/B and FX,
   which hide while it shows. The two drawers never show together (opening one closes the other), and the install panel
   steps aside as for Samples.
   - Rows: the header, 44 px (PHRASES, the kit line `FUT01 Alloy Signal · 134 BPM · F#m 11A`, Close); the filters, 32
     (From the kit with its count, lit, the only filter for now); the grid, 280 (four 256 x 72 tiles a row, 8 px apart,
     scrolling); the footer, 40 (the status line, and `For my synths: on` or `off`, the drag strip's switch).
   - A tile shows the phrase's name and `From the kit · 2 bars`. A 4-pixel drag writes its MIDI file and hands it to the
     OS inside the mouse event; a click without a drag shows the file, as the chips did.
   - Keys: Tab reaches the grid in one stop; the arrows move between tiles, Return shows the file, Escape closes the
     drawer. Space, letters and every shortcut go on to the host, as in the Samples drawer. The drag stays mouse-only.
   - Screen readers hear each tile as a button, "E Piano Stab Rhythm, from the kit, 2 bars", whose press shows the file;
     opening the drawer says "Phrases", and each arrow says the tile it lands on, in those words.
   - It asks the Controller for the list (`phrases ()`) only when `phrasesGeneration ()` moves on, so a kit change keeps
     it open with the new kit's phrases. It registers no listener and saves nothing.
3. **From the kit** lists the kit's phrase files, then its lead line (`dragout::kitPhraseFiles`): 6 for FUT01, 5 for
   each other kit. A tile drags as the chips did, the file byte for byte, named
   `FUT01 Alloy Signal 134 F#m 11A - Phrase Organ Riff 4 bars.mid` (the lead line
   `... - Lead Chrome Pluck 2 bars.mid`), made for synths and marked "(synths)" while For my synths is lit (ADR 0027). A
   tile's name is its file's words without the bar count: Organ Riff, and the lead line Lead Chrome Pluck, as its file
   name has it.
4. **The phrase chips leave the drag strip.** It keeps its label, For my synths, Loop, Song and 1 drum track where they
   were, with 572 of its 1064 px free on the right for Part and Drum kit.
5. **The header's widths.** The right block grows from 180 to 278 px: Phrases, Samples and the 82 px zoom. Measured in
   DejaVu Sans Bold, the widest default font, at the LED button's font, "PHRASES" needs 89.8 px of its button and
   "SAMPLES" 90.1 (the v0.3 plan's command 4), so both buttons are 91 px, 7 apart (the plan's 90 px cut SAMPLES by 0.08
   px). The kit cards' side margins go from 8 to 2 px: each card is 146 px wide with a 114 px direction line, where
   FUT03's direction needs 111.42 px at the 8-point floor in DejaVu Sans. A card's title now shrinks to fit from 14
   points, as its direction line does: at 146 px, Glass Current and Furnace Code need 82.9 and 83.6 px against 73.0 in
   Liberation Sans Bold (92.3 and 92.1 against 69.6 in DejaVu Sans Bold, where they read at 10.5 points).

## Consequences
- `Controller::phraseFiles ()`, `EditorModel::phraseFiles` and `dragout::phraseFiles` are gone; `phrases ()`,
  `phrasesGeneration ()` and `kitPhraseFiles` take their place. No drag's name or bytes change.
- A write that fails says "Couldn't write Organ Riff: its MIDI file couldn't be saved in Wubansia's drag folder.": the
  Controller's `writeDragFile ()` gives no reason, so the line names where it failed, not why.
- No new parameter, project state or setting.
- Not checked: drags into Live and Logic, and the header in macOS's fonts (its widths were measured in DejaVu Sans and
  Liberation Sans only).

## Alternatives
- **Keep the chips on the strip.** Part and Drum kit wouldn't fit beside them.
- **70 px header buttons**, a critique's option: the labels need about 90.
- **Wait for the library**: the strip would keep its chips until the generator and the real-time overlay are built.

Step `phrases` adds the library, its audition, More and Keep here.
