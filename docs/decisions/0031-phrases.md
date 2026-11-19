# 0031. The Phrases drawer and the phrase library

**Status:** Accepted, 2026-11-18, in under ADR 0028 (build in, cut later). Built with these choices, which you can cut
or change: the button's place and word (Phrases, left of Samples); From the kit holding the kit's lead line as well as
its phrases; the phrase chips leaving the drag strip. Part 1 was the drawer with the kit's own phrases; part 2 (the
amendment below, the same status) adds the generated library, its audition, More and Keep, and a click now plays a
phrase instead of showing its file.

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

## Amendment, 2026-11-19: the phrase library, its audition, More and Keep (part 2)

**Status:** Accepted, 2026-11-18, in under ADR 0028 (build in, cut later). The choices you can cut or change: the eight
kinds and their words, 64 phrases per kit and the quotas, the names, Preview starting for an audition, the kit's lead
sitting out while a phrase plays, More's 8, Keep, and a click that plays instead of showing the file. The design is
`docs/plans/v0.3-plan.md` and its phrases plan; this records what was built and measured.

### Context
The user's words above, and ADR 0028's "options for myself". Part 1 moved the kit's own phrases into the drawer; the
library fills it with phrases written over the kit's own chords (the app's chord loop: FUT01 F#m9 and Bm9, a bar each;
FUT02 Dm9; FUT03 Gm9; FUT04 Em(add9)).

### Decision (continued)
6. **The generator** (`core/gen/phrase.{h,cpp}`) writes 8-bar phrases from the hook's material (ADR 0026), moved into
   `gen/hook_material.h` byte for byte (the generator snapshots and hook tests unchanged). A phrase is a pure function of
   (spec, kind, seed), every draw below `phraseKey (kit, kind, seed)`, so no song draw moves and every platform gets the
   same bytes.
7. **Eight kinds**, each with its forms: hook (the 2-bar riff four times, the last varied), call and answer, period,
   sparse (the accents only: room for a vocal), arp, stabs, counter-line and bounce; every phrase ends on the key's 1, b3
   or 5, or rests from bar 8's beat 3.
8. **Register and lengths**: each kit's phrase register (FUT01 64-86, FUT02 67-86, FUT03 64-83, FUT04 69-88), above the
   bass; notes a 16th at least unless the next starts sooner, ending 10 ticks before it, never past bar 8. A kind may
   carry its own `"register"` (new in the spec format): FUT02's glass periods use `[77, 88]`, the kit's written glass
   register, so Glass Ping (one zone, rooted at 86) is repitched down at most 9 semitones and up at most 2.
9. **The checks** (`checkPhrase`): in key, in range, a note in every bar, a motif that comes back varied, a real ending,
   rhythm, clean overlaps and lengths, synth-ready, a melody (not one key), strong beats on chord tones, and no rub with
   the chord voicing (no minor 9th either way). The library keeps only phrases that pass them all and are under 700
   permille alike (the Jaccard similarity of their 16th-and-key sets) to every phrase kept before.
10. **The spec and the library are files**: `content/phrases/<kit>/<KIT>_phrases.json` (the chord loop, register,
    swing, the hook's material, the kinds, the name words, renames and cuts) and `<KIT>_phrase_library.json`, the 64
    rows `groovecore phrases --library` chose, each with its file's SHA-256, a snapshot changed only in its own commit.
    The choice runs in two passes, so **a cut touches only what is cut**: pass 1 takes each kind's first passing seeds
    up to its quota, cuts ignored; pass 2 removes the cut rows and refills each kind short of its quota from the seeds
    after its last pass-1 seed. `"cut": {"<kind>": "all"}` removes a kind and doesn't refill it; a quota is never 0.
    **Names come from (kind, seed)**: kind k owns 72 of the kit's 576 names, a library seed s takes index
    72k + s mod 48 and a More seed s' = s - 1,000,000 takes 72k + 48 + s' mod 24, " 2", " 3" from a second lap on. The
    plugin embeds both files (about 76 KB for the four kits) and refuses a library chosen for another spec.
11. **The drawer's library.** Filters All, Hooks, Call & answer, Periods, Sparse, Arps, Stabs, Counter-lines, Bounce,
    From the kit and Kept, each with its count, narrowed (padding 14 down to 4 px) until the row fits with every word.
    A tile shows its name, `<Kind> · 8 bars · <tags>`, its roll, a New badge on More's phrases until played or dragged,
    and a star. The header gains **More** and **Stop**; the kit line reads `FUT01 Alloy Signal · over F#m9 – Bm9 ·
    134 BPM · F#m 11A`. **A click (or Return) plays a tile; on the playing or waiting tile it stops** (this replaces part
    1's click and Return that showed the file). The arrows and Return work in the grid; Escape closes; Space, K, M,
    letters and shortcuts go on to the host. Tab reaches the filters, the grid, the selected tile's star, More, Stop and
    Close. The status line says, in this order: a failed write; when the host ran no block, the Samples drawer's
    per-host line (`noSoundLine`, now shared); `<lane> is muted: unmute it to hear <Name>.` while a phrase waits or plays
    on a muted lane; the kit's samples missing; More's `Making 8 more…`, then `<n> new <kind> at the end of the list.`
    (or `Only <n> new … this time.`); `<Name> starts at bar <N>…`; `Playing <Name> on <sound>, in place of the kit's
    <lanes>.` with `Wubansia started playing for it; Stop ends both.` when it did; else `Click a phrase to hear it with
    the groove; drag it into your DAW as MIDI.` Each change is said to a screen reader.
12. **The overlay** (`engine/audition/phrase_clip.h`, `BlockLoop`): a second scheduler plays the clip over the groove,
    anchored on the main scheduler's position (so Preview and the Standalone, which give the engine no host position,
    keep time): bar 1 on the next chord line while playing (8 quarters on FUT01, 4 elsewhere), the one at or before it
    while stopped. The groove's own notes on the clip's lanes sit out while it sounds; offline renders and another kit's
    clips play nothing; a bypassed block drops an offer; reset lets the clip go. Telemetry gains `phraseTicks` (ticks
    from the clip's bar 1, negative while it waits) for the status line.
13. **The phrase service** (`plugin/source/services/PhraseService`), one thread per processor, started by the drawer's
    first request: the lists, More, the clips and Keep's writes. Clips are given up as the Samples drawer's are: an
    offer no block takes in 300 ms is withdrawn, a held clip is dropped at the next block once none has run for 300 ms,
    and the state says the host is silent; either way the Preview the audition started goes off. It makes and deletes
    every clip. An audition starts Preview when the last block wasn't playing, and Stop, a kit change, closing the
    drawer, opening Samples or closing the editor stop the audition and that Preview.
14. **The drag**: `<KIT> <Title> <BPM> <Key> <Camelot> - Phrase <Name> <Kind> 8 bars.mid`, one track (`Lead` on channel
    2, FUT02's periods `Glass Ping` on 13), through `madeForSynths` with " (synths)" while For my synths is lit. The
    kit's own phrases drag as before.
15. **More and Keep** live in settings.xml, per kit: `phrases_next_<kit>` (More's next seed; 1,000,000 when absent) and
    `phrases_kept_<kit>` (ids). More makes 8 of the kind shown (one of each kind under All, From the kit or Kept), each
    passing every check and unlike the list, at most 1,000 candidates a kind. Both are read from the file every time and
    each write re-reads its attribute under the lock: the Standalone and a DAW open at once keep both people's stars,
    and the next seed never goes back. A kept phrase is its id: it is regenerated from kind and seed, from the library
    or from More, so one cut from the library later still lists under Kept.

### Consequences (part 2)
Measured on the code as built (`groovecore phrases --kit FUT0N --scan 400 --report`, seeds 0-399; the library's files
with `phrase-proto-out/scripts/library_stats.py`):

| Pass rate of 400 | hook | answer | period | sparse | arp | stabs | counter | bounce |
|---|---|---|---|---|---|---|---|---|
| FUT01 | 363 | 341 | 396 | 317 | 369 | 298 | 337 | 356 |
| FUT02 | 367 | 363 | 400 | 346 | 348 | 322 | 356 | 350 |
| FUT03 | 353 | 328 | 396 | 343 | 335 | 338 | 317 | 318 |
| FUT04 | 382 | 331 | 389 | 171 | 357 | 319 | 358 | 363 |

- The lowest is FUT04's sparse, 171 (the test's floor is 160). Keys over the 400 seeds: FUT01 64-85, FUT02 65-88 (its
  periods 77-88), FUT03 62-81, FUT04 67-88. A phrase takes 1.7 ms on average at most (per kind and kit), 4.4 ms at the slowest
  (FUT01's hooks).
- The library, 64 phrases per kit, names unique: no note overlaps the next and no gap is under 10 ticks; notes a bar run
  from 1.8 (FUT03's sparse) to 16 (FUT04's arps); FUT02's hooks with the pedal 11.5-15. For my synths changes only
  velocities under 72: 20, 532, 22 and 14 notes per kit (FUT02's pedal and the counter-lines' soft notes), never a tick,
  length or key.
- Library files (SHA-256): FUT01 3e4bcee390d57c0a..., FUT02 0c938ab6f8e99663..., FUT03 3af28059cc3dcb95..., FUT04
  5ed1c259ab4c1e59... A later change to the hook's material can move phrases: the library is then regenerated in its
  own commit, and a kept More phrase may change.
- **FUT02's CC74 filter ride still reaches the overlay's voices**, by design: they play on the kit's lead channel, as
  its own lead does. You can ask for the ride to skip auditioned phrases.
- A muted lane silences a phrase on it (a clip's lanes play through their mix channels); the status line says so
  rather than unmuting anything.
- FUT03's Song plays Cm9 in 42 of its 136 chord bars, where a phrase written over Gm9 may rub; FUT01's Song shifts 4.
- No new parameter or project state; settings.xml gains only the two attributes above.
- Not checked: how any phrase sounds; auditions in Live and Logic, stopped and playing; Preview starting in a host;
  macOS byte identity (CI's `core_tests`).

### Alternatives (part 2)
- **Phrases cut from generated songs** (the preview's 68): fewer, tied to songs and to a song's harmony.
- **Committed `.mid` files**: 256 files where one row each does.
- **Choosing the library at load**: slower to open, and no snapshot to review.
- **A rendered audio clip** for the audition: no tempo sync. **MIDI injected through the router**: its held-note table
  would collide with the person's MIDI in.
- **Playing over the kit's lead**: two leads at once. **A separate window**: the drawer is where Samples is.
- **Lowering a quota to cut**: it would move later kinds' rows; the cut list moves only what it cuts.

