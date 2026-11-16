# 0026. Generator v2, part 2b: the hook and its motifs

**Status:** Accepted, 2026-11-15: the user approved the plan with its recommended choices ("go ahead on the next stage"). The plan is `docs/plans/S1b2b-plan.md`, step 2 of `docs/plans/S1b2-plan.md`, in four PRs; this ADR records the first two as built: PR 1, the generator's MIDI channels, and PR 2, the hook. PR 3 (drop 2's bass lines and FUT01's bass talk) comes next and adds its record here. PR 4, the vox reply, waits for ADR 0016 (whether the kits keep the vox synth), and the S1b2d items (the plan's decision 10: G4's unit for leads, R1's and R7's lead bands) for their own step. It amends ADR 0023 (§4's FUT02 rows, and its Consequences re-measured with the hook) and does ADR 0013 §2's reserved per-seed motif for FUT02's arp; §2's reserved reese drop layer for FUT01 becomes PR 3's written warp riff and talk.

## Context
Before this step, on 64 seeds per style (ADR 0023's Consequences), G4 and G5 failed on FUT01, FUT03 and FUT04 for want of a lead lane, and FUT02's arp, whose cells come from a short list per grid, repeated its 2-bar motifs across seeds: R10 lead 71‰ (296 of 4,188 windows) against a band of 30‰. Generated MIDI routed into Wubansia played its bass on the Lead instrument (channel 2).

Step 2 of the S1b2 plan asks for one hook per seed, drawn rhythm first and scheduled (in 3 sections or more, once before the first drop, in half the drop bars or more), with plan §2.3's and §4.1's templates (RIFF, SENTENCE, PERIOD), development operators and call and response through a half-bar mask (`docs/plans/S1-futuristic-slice.md`). The user's brief: "Really nail the coolness", "midis that aren't written like AI", "I hate muddy", DJ-friendly extended mixes; they work in Ableton Live 12 and Logic Pro.

A rule here earns its place by being how the kits' own written material does it: the written lead riffs and phrases (`content/midi/fut0N/`), each written extended mix's lead lane line by line (the written programs, in `content/midi/fut0N/extended/vox/regions.json`), and the written mixes' chords against their leads (the mud rule). The plan's figures came from a prototype built on the generator at fd2c5b4, not kept in the repo. The tests re-measure them, and every figure below is the code's as built.

## Decision

### 1. The generator's MIDI channels (PR 1)
The four kits' lanes follow MIDI v2's channel table (ADR 0019, `docs/midi-map.md`): the bass on channel 1, the lead on 2 (FUT02's arp included), the voiced chords stay on 3 (the chord voicing, which Wubansia ignores), the drums on 10, and FUT04's rumble on reserved channel 9 until it becomes drum note 35; PR 4's vox reply goes on 5. Notes, controllers and each lane's `channel` in `regions.json` move with their lane, and nothing else changes: 1,190 of the 1,372 snapshot files differ, each only where it names a channel, and `groovecore metrics` gives the same bytes. `docs/midi-map.md` says what holds for the generator's files, and a test pins every lane of the four kits to its channel.

### 2. One hook per seed: the material (`core/gen/hook.cpp`)
- **The model** (`core/gen/song.h`, integers only). `Song::hook` holds the carrier lane, the first drop's mode, the register's tonic, the contour, A (the hook, a 2-bar riff), B (drop 2's new phrase), ANSWER's response, VARY's end degree, every 8-bar line of the sections the carrier plays in, and the realized notes. Every carrier note carries `noteFlag::hook` (1 << 10), and FUT02's pedal `noteFlag::pedal` (1 << 11).
- **Rhythm first.** Bar 1 of A is one of the style's cells (16ths, lengths and accents, by weight), with at most one non-accent onset out and one extra in; each half of the bar keeps an onset, and each bar a non-accent onset except in a pedal style. Bar 2 keeps bar 1's 16ths 0-7 and makes at most one edit among 8-15 (the same, a move by a 16th, one added, one dropped): 15 of 15 written phrases keep their first half, and 14 of 15 make at most one second-half edit.
- **Pitched in the key** (the plan's decision 2): scale degrees from the tonic, out of the style's pool, in the first drop's mode. The register comes first (the tonic's octave that puts the start inside the carrier's range and the 3rd above it nearest the style's center), then a contour drawn per seed (late: one peak in bar 2; early: in bar 1's 16ths 4-12; twin: the top struck twice in a row), then the walk: start and end degrees by weight, moves weighted by size, a leap of a 4th or more turning back, no move over the style's cap (9 semitones; FUT04 10, the written blips' largest), the riff's wrap counted as a move, and bar 2 never a transposition of bar 1. A strong onset (a beat or an accent; in FUT02 a beat) takes a chord tone (1 3 5 7 9) of every chord it can sound over in the first drop's first line: the chord the progression lays out unpushed, plus the next one within 240 ticks of its change. Up to 32 walk attempts, then the attempt that breaks the fewest rules; a material that still breaks a rule is drawn again, up to 8 times.
- **A fixed accent shape**, drawn once: tiers for the downbeat, beat 3, the other accents and the rest, the peak raised, one offset per onset. Bars i and i + 2 of a line carry equal velocities.
- **B, drop 2's new phrase, in each kit's written idiom** (decision 3): FUT01's and FUT03's 4-bar organ period (a; b answering it; a', a with one note moved a step; c, keeping a's first half and falling by steps to 5, 2 or b3, held to the bar line), FUT02's glass period (one rhythm in all four bars, E5-F6 over the pedal, c striking the period's top again and ending a step under it), and FUT04's acid-stab riff on the blips (from the written stab's off-beat cells, on the tonic by repeats and steps, its last note the 5th). B starts on the tonic (the 5th when A does), in an octave that isn't A's key at that 16th, so no bar of B is a bar drop 1 played.
- **The response** (ANSWER's): A's second-half onsets, walked afresh from below the call and ending on VARY's degree, 6 velocity steps softer, never A's own second half.
- **The texture check.** The textures a style plays must differ in every bar of a line or in none, on the material and on each section's realized lines. A failing material is drawn again (8 attempts); a texture that still fails is barred for the song (none is, on 1,027 seeds per kit).

### 3. A line, realized
- Each line plays its texture of the material in its section's own mode (a Dorian 6 or a break's Phrygian b2 colours it), 8 bars repeated note for note. A strong note a semitone over a clash tone (1 3 5 7) of its chord moves to the nearest pool degree that isn't, inside the register and under the cap; VARY and CADENCE end the line under the cap too.
- **The voicing guard, the written-reference mud rule** (decision 7): no hook note a minor 9th above or below a chord-lane note that sounds with it, and a semitone only with the lower note at the kit's written floor or above (68, 76, 69 and 78 for FUT01-FUT04), FUT02's pedal included. The written mixes never break it. The guard reads the chord lanes as their union over the section's lines at each 16th (two 64-bit masks), so every line of a section moves the same notes: a rubbing note moves to the nearest clean pool degree within two steps under the cap, else rests.
- **FUT02's ChromeArp.** The hook is the arp's top voice, from C5 up, over a key-fixed pedal: after each top note, on the free 16ths before the next one, D5 then A4, at most two, each 96 ticks long and under the top note it follows, never on 16th 15 nor before a bar's first top note, never moving with the chord. It plays under FULL, ANSWER, FRAGMENT (in the half the call plays) and NEW; SKELETON is the top voice alone.
- Lengths come from the cell, never into the next note (10 ticks short) nor past the line. Notes outside the natural minor are tagged borrowed.
- **The lead lane** (`core/gen/lead.cpp`) plays the hook's notes inside the lane's active ranges (a pulled bar has none), velocities scaled by the lane grid's `vel` and the section's, lengths by the grid's `len`, and sets each line's half-bar mask from these final notes.

### 4. Where it plays: one carrier per kit, over the drawn forms
| | FUT01 | FUT02 | FUT03 | FUT04 |
|---|---|---|---|---|
| Carrier lane, track, MIDI channel, range | `lead`, 'Lead Pluck', 2, 68-86 | `arp` (now of kind lead), 'Arp Chrome', 2, 69-89 | `lead`, 'Lead Keys', 2, 69-86 | `lead`, 'Lead Blips', 2, 71-88 |
| Register center; leap cap | 74; 9 | 79, the top voice from C5; 9 | 76; 9 | 78; 10 |
| Pool; contours | 1 2 b3 4 5 b7; late 3, early 1 | the same; late 2, twin 2, early 1 | the same; late 2, early 2 | all seven degrees; late 2, twin 2 |
| Cells (`hook.cells`) | six: the written pluck's 0-3-6-8-11-14, the vox riff's late start, an off-beat cell, a 3-16th cycle and two more | four, all from the downbeat: the written arp's accents, the glass's 0-3-6-10-14, the vox's and a 3-16th cycle | six: the written keys riff, its answer bar, the dub stab's, an off-beat cell and two more | six: the written blips (0, 3, 4, 6, 7, 10, 11, 12, 14), the rave stab's 3-16th cycle, the chant's 16ths before the beats and three more |
| B | the organ period | the glass period over the pedal | the organ period | the acid-stab riff |
| No FULL or NEW before drop 1 in | the groove, break and build | the groove, break and build | the break and build | the build |
| Drop 1 holds a FULL line; cover (drop, drop 2) | yes; 600‰, 500‰ | yes; 600‰, 500‰ | yes; 600‰, 500‰ | no; 500‰, 500‰ |

FUT01 and FUT03 sit in the clearing register (decision 13: from G#4 and A4, a 3rd above the written leads, since the generated chords sit higher than the written ones). The lead lane joins the groove, break, build, drop and drop 2 plans last, with no entry, rest, dropout group or roster place, so every form draw is unchanged; no intro or outro plays it, so the mix-in and mix-out stay drums and bass, as in every written mix. FUT02's arp keeps its place in the form and the roster (decision 11): planHook runs after planChanges and clears only the carrier's own block bits, so no other lane's draw moves, and a roster pick of the arp now changes nothing audible. The hook tests compare every other lane, note for note, with the style without its hook (seeds 0-15).

**The line plan,** per section in song order: each line draws an op from its kind's menu (a groove, break or build has one menu before the first drop and one after; `first` on the carrier's first active line, `last` on the section's last line, `next` between), a groove's leading OUT lines are drawn and clamped so that its last line before drop 1 plants the hook, and a build after drop 1 can `continue` the line before it. Then the rules, in order: no FULL or NEW in a withheld kind before the first drop; r2 (drop 1 opens OUT after a break that ended on the hook and a build without it); r1 (a drop opens on the hook if it played the last bar into it); drop 2's first playing line is B; drop 1 holds a FULL line (`holdFull`); r3 (the line after a one-bar pull that takes the carrier plays); the cover floor (a drop plays at least `cover` of its lines). Each line is realized and checked by the line rule: G4 compares a bar with the bar 8 before it and counts a run of differing bars once, so a line differs from the line before in every bar or in none, bar 7 aside (VARY, CADENCE, BREATH). A refused line falls back to REPEAT, FULL, then OUT; a line a rule made play, to SKELETON, then FRAGMENT. An OUT line clears the carrier's block bit and takes it out of any one-bar dropout inside it, so `regions.json` lists only what plays.

**The menus weight the written programs** (`hook.plan` in each style), each written mix's lead lane line by line (- OUT, C the call, A ANSWER, F FULL, S SKELETON, P the new phrase):

| | Groove | Break | Build | Drop | Groove after drop 1 | Break 2 | Drop 2 |
|---|---|---|---|---|---|---|---|
| FUT01 | --CC | -C | - | -CFC | -- | PP | PCPA- |
| FUT02 | -SSS | -- | SS | FFFS | -S | -- | PSC- |
| FUT03 | --FA | -C | -- | -CFC | -F | PP | PCP- |
| FUT04 | --CFCSFS | -- | -- | S--S | none | none | -S-C |

FUT02's leading "-" in its grooves is the arp not yet entered, so those menus have no OUT. §9 lists where the menus depart from these programs.

### 5. The operators
Each line holds one texture of the material, repeated note for note (the written lag-4 repetition is 791-1000‰):

| Operator | What it plays | Where (shipped weights) |
|---|---|---|
| FULL | the riff four times (B: the period twice, the stab riff four times) | drops; FUT03's and FUT04's grooves; drop 1 always holds one in FUT01-FUT03 |
| REPEAT | the line before, note for note | grooves, breaks, drops, break 2 |
| FRAGMENT, the call | each bar's first half, plus the bar's last onset when its second half holds two or more (FUT02: with the pedal) | it plants the hook in the grooves (FUT01, FUT03, FUT04) and the breaks (FUT01, FUT03); the drops' calls |
| SKELETON | the accented onsets only (FUT02: the top voice alone) | FUT02's grooves and builds, drop 1's first line (FUT01, FUT03), FUT04's drops |
| ANSWER | the call, then the response in each bar's second half | FUT03's groove, the drops |
| VARY, CADENCE | the line's last bar ends on VARY's degree, or falls by step to the 5th held to the bar line | the drops' last lines |
| BREATH | the line's last bar rests | FUT02's grooves, builds and drops; FUT03's break 2 |
| NEW | B | drop 2's first line, break 2 (FUT01, FUT03), later drop-2 lines |
| AUGMENT | A at half speed | the last line of the break before drop 1 (FUT01, FUT03), 1 in 5 |
| SEQUENCE | the line before, 7 scale steps up, if it fits the register whole | weight 0 in every shipped style (decision 8) |
| OUT | the lead rests: the hand-off | builds (FUT01, FUT03, FUT04), breaks, drop 2's strips |

### 6. Stored and exported
- **`regions.json`**, only for a style with a hook block: at the top level `"hook": { "lane" }`, and per region `"hook": [ { "startBar", "dawBar", "bars": 8, "form", "motif", "shift", "ending", "mask" } ]`, `[]` where none. `form` is out, full, fragment, skeleton, answer or augment; `motif` is "a" (the hook) or "b" (the new phrase: NEW is full with "b"); `shift` is SEQUENCE's scale steps (0 in the shipped styles); `ending` is none, vary, cadence or breath. The mask is 16 characters of 0 and 1 in time order: character i is bar i / 2, its first half when i is even, and a half is busy when a lead note starts in it or sounds into it. It shows a producer where the hook leaves room, and it is where PR 4's vox reply may play. `format` stays 1, since fields are only added. A style without a hook block writes neither key and generates the same bytes as before (a test pins a fixture style's hashes).
- **A `--bars` cut** keeps the whole song's hook: its lines that start before the cut, with their masks, and its notes, ended by the cut (ADR 0013 §4).

### 7. Style data and the loader
- The hook block (`lane`, `register`, `contours`, `cells`, `mutate`, `answer`, `pitch`, `vel`, `guard`, `period`, `pedal`, `withhold`, `plan`) is documented over `HookSpec` in `core/gen/content.h`; FUT01's is the plan's full example. A lead lane's grids hold only a velocity and a length scale, in permille. Integers only, under the strict reader's rules.
- The loader refuses, with the file, the JSON path and the line, every case of the plan's list: the carrier (missing, not a lead lane, two of them, a lead lane without a hook block), the register, contours, cells, mutate, answer, pitch, velocities, guard, period and pedal out of their ranges, and the menus against the form (a menu for a kind the carrier doesn't play in, or a kind it plays in without one; VARY, CADENCE or BREATH in a `first` menu; FULL or NEW in a withheld kind before the first drop; a drop's cover under 500‰ or out of reach at some drawable length; drop 2's `first` other than NEW; a form whose grooves and builds can all leave the hook out before drop 1). `makeBundle` checks the block against the carrier's kit range, which spans an octave or more from the style's `mud.minorSecond` up.

### 8. Determinism: the permanent tags
- Every draw hangs below `root.with (tag ("hook"))`, a new permanent top-level tag; PR 4's reply will hang below `root.with (tag ("reply"))`. The permanent tag names under it: `rhythm` (`cell`; `mutate` with `drop`, `which`, `add`, `at`; `bar2` with `edit`, `at`), `start`, `end`, `contour`, `walk` (`attempt`, `peak`, `twin`), `vel`, `vary`, `b` (`rhythm` with `half` and `bar2`, the stab's with `cell`, `mutate` and `bar2`; `end`, `walk`, `cadence`, `vel`), `response` (`walk`), `redraw`, `check` (`attempt`), `plan` (per section `enter`; per line `op`). A redraw draws the whole material again below `kh.with (tag ("redraw")).with (r)`, r = 1-8; a texture check's attempt below `kh.with (tag ("check")).with (tag ("attempt")).with (t)`, t = 1-8, each with its own redraws; the line plan below `kh.with (tag ("plan")).with (section)`, with `enter`, and `.with (line).with (tag ("op"))` per line.
- No existing tag or key path changed, so the form, harmony, drums, chords and bass draw exactly as before. `test_rng` pins `tag ("hook")`, `tag ("reply")`, `tag ("rhythm")`, `tag ("cell")` and FUT01 seed 7's hook, reply and cell keys against reference vectors (Python and Node agree), so a renamed tag fails a test, not only the snapshots.
- Integers only; stable sorts with total orders; no unordered containers; a cap on every loop: 32 walk attempts, 8 material redraws, 8 texture-check redraws, 3 fallbacks a line, one re-realization a section for drop 1's FULL line.

### 9. Departures
**From plan §2.3 and §4.1** (`docs/plans/S1-futuristic-slice.md`), as the user approved them:
- Pitches are key-relative scale degrees with the avoid-note fit and the voicing guard (decision 2), not chord-relative degrees re-pitched at every chord change: the written lead riffs keep one pitch set over i-iv, and a motif re-pitched with every chord is an arpeggiator's tell.
- The RIFF is 2 bars on a pool of 6 or 7 degrees with the leap capped at 9 or 10 semitones, repeated note for note through a line. It changes on the 8-bar line through the operators, not by one note or step mutated every 4-8 bars, which G4 would count off the phrase.
- No SENTENCE: no written top line is one, and bar 1 = bar 3's rhythm holds in every written 4-bar window. The PERIOD is drop 2's new phrase (and break 2's in FUT01 and FUT03), not the break's lead; the break plays the call, AUGMENT or nothing (decision 3).
- The hook's appearances: no filtered fragment in the intro (the mix-in stays drums and bass, as every written mix's does); the break's statement is the call (FUT01, FUT03) or nothing (FUT02, FUT04); drop 2 opens on a new phrase, not a register or ANSWER variant of the hook.
- Not built here: the other lanes' counter-motifs from the hook's rhythm (S1b2c; PR 4's vox reply is the first), the vox chops answering the hook (PR 4, after ADR 0016), and the reese drop layer's long notes on 1, b2, 5 and b7, which PR 3 replaces with the written warp riff and bass talk (decision 5: FUT01's written bass has no b2, and round 1 found the reese undanceable). The half-bar mask is built and exported per line; nothing reads it until PR 4.

**From the written programs**, each until its missing part lands, when it goes back as data:
- FUT01's and FUT03's drop 1 opens on SKELETON 2:1 against FULL, where the written programs open on OUT: their no-vox mixes fill that line with accent hits and their vox mixes with the vox riff, which the generator lacks until S1b2c's accents or PR 4 (decision 4).
- FUT04's drops play the blips in every line (drop 1 SKELETON 3:1 against FULL, then SKELETON, the call or FULL; drop 2 the stab riff, then SKELETON or the call), where the written S--S and -S-C hand lines to the rave stab, the metal chords and the acid stab (S1b2c's stabs).
- FUT02's drop 2 keeps the arp driving every line (B; then B, FULL or ANSWER; then FULL, SKELETON or the call), where the written PSC- is thinned by the laser chord stab, the glass ping, the vox and the accents (decision 14).
- The written C (bar 1 alone, on odd bars) and A (bar 2 alone, on even bars) fail G4 per bar, so they map to the half-bar call (FRAGMENT) and ANSWER until S1b2d decides the lead's G4 unit (decision 10). FRAGMENT is 279, 18, 249 and 276‰ of the playing lead lines, against the written C's 6 of 12 playing lines in FUT01 and 4 of 12 in FUT03.
- FUT03's written drop-2 keys arpeggio is the organ period: a chord arpeggio repeats across seeds, as FUT02's old arp did (R10 71‰).

**From the S1b2b plan, as built** (each with its evidence in its commit message):
- PR 1's midi-map line gives each style's slide overlap (30 ticks in FUT01 and FUT04, 24 in FUT03, 20 in FUT02, as every slide of the 64 snapshot songs measures), not a flat 30, and says the generator's files follow the channel table, "not every convention below", rather than only the table: they also keep one tempo and one key signature at tick 0 and note-offs, then controllers, then note-ons on a tick.
- The material is drawn again for any rule the tests pin per seed, not only a move over the cap: a tied or misplaced peak, bar 2's head equal to bar 1's, B's first note on A's key at its 16th, the organ's a', the response. B's first note keeps its degree (the tonic, or the 5th) and takes the octave of B's register that isn't A's key there, where the prototype moved it a pool step, off the tonic or 5th (which test 15 refuses); in FUT02's glass register (76-89, one octave of each start) only a redraw resolves that. Counted on the plan's per-kit data at the material's commit, 210, 335, 181 and 3 redraws in 1,027 seeds per kit, against the prototype's 15, 17, 30 and 1 for the cap alone.
- The glass period's c strikes the top of a, b and a' (the prototype's: of a and b); the organ fall's other ends come by weight, as the plan says; bar 2's change of head keeps the register, the cap and the contour; the response walks below `response`/`walk`, as the plan's tag list has it (the prototype: below `response`).
- The texture check takes every pair of textures the style plays on the material, and per section the textures that section can play on its realized lines; NEW is never barred, like FULL. The realized check, which the prototype lacked, changed the material in 26, 43, 42 and 0 of 1,024 seeds (counted on the plan's per-kit data at the line plan's commit), and no drop 1 lacks its FULL line on 1,027 seeds (the prototype: 2 FUT01 and 4 FUT03 drop 1s in 1,024).
- r1 and r2 read whether the carrier plays a section's last bar by its activity, so a BREATH last bar counts, as the form tests read the lanes; r3 runs again after the cover floor, since a line the cover makes play can hold a pull whose next line was OUT.
- The leap cap binds a realized move where the material's own move keeps it (every move of FULL and NEW, the wraps and joins included), not across SKELETON's or FRAGMENT's own leaps: binding every move rested 59, 64, 27 and 48 top notes in seeds 0-63 (on the plan's per-kit data); now none rest, and no FULL-line move is over the cap in 1,027 seeds per kit.
- Where the plan is silent: a pedal note lasts 96 ticks, its tonic is D5 (the tonic at the register's floor or above), and under FRAGMENT it stays in the half of the top note it follows; a note the section's mode takes out of the register moves to the nearest pool degree inside it, under the cap, else rests; SEQUENCE fits the register whole or is refused; VARY skips an end on the line's last degree, and CADENCE's other ends come by the end weights; the guard's span follows a lane grid's `len` above 1000; B's peak windows (16ths 22-26, the stab's 26-31) and the stab's move weights (30, 50, 15, 5) are constants in the code. The loader also refuses, as the strict reader does any key it doesn't use, a menu key in another kind's menu, a period key its idiom doesn't use and a menu no section of the form can use (so FUT04, whose form has no groove after drop 1, has no after-groove menu).
- FUT02's start weights are [20, 0, 40, 0, 30, 0, 0], the prototype's without the 7th, which always put the riff's first note on C5, the top voice's floor (the written top voice starts on its 3rd and touches C5 only as an ending). With the 7th, 1,858 of 13,688 FULL and ANSWER top notes sit at or under D5 (135.7‰, test 19's bound 120); now 1,537 of 13,626 (112.8‰), against the prototype's 94. [15, 0, 45, 0, 40, 0, 0] gives 100.0‰ if more margin is wanted. Why the code sits above the prototype is not proven; the likely cause is the redraws on B's first note (above).
- FUT02's groove after drop 1 opens on SKELETON alone, as the plan's "-S" reads, not on the prototype's OUT 1 to SKELETON 2: that OUT left 17 of the 752 grooves in 1,024 seeds where the arp plays without a hook note, all after drop 1, against test 13.
- Where the plan gives no figure, the content takes the prototype's: FUT03's span 14 and velocities 112 and 90 (peak 10, shape 6), FUT04's span 15 (peak 10, shape 6), every style's mutate extras, start and end weights (FUT02's start aside), period halves and cadences, FUT02's period an octave up and its arp's grids at 1000/1000. FUT03's and FUT04's lead and feel lanes take FUT01's grids and its sd, clamp, vsd and articulation, with their own swing.
- The tests' fixtures live in `core/tests/fixtures` (`nohook/`: FUT01 and FUT02 before the hook; `hook/`: FUT01 with the plan's full example) and are frozen; the form grammar tests and FUT02's 3-3-2 arp test read `nohook/`, so `arp.cpp` keeps its coverage. Tests 4, 6 and 16 count as the prototype counted for the plan's figures, and test 22's hashes are FUT02's after PR 1's channels.
- SEQUENCE is built and loads, but no test plays it, where decision 8 has it tested on a fixture. A scratch fixture whose drop draws it 3:1, with the lead's range widened to 68-100, played 55 SEQUENCE lines in 64 seeds: 27 are the line before exactly an octave up (the fit and the guard move notes in the others), and a SEQUENCE after a SEQUENCE climbs a second octave. A test should pin what it plays before any style draws it.

## Consequences
**The gates**, on 64 seeds per kit (`groovecore metrics`; `test_gen_gates.cpp` pins the failing lists): before the hook (after PR 1, ADR 0023's figures), now, and the plan's prototype.

| | FUT01 | FUT02 | FUT03 | FUT04 |
|---|---|---|---|---|
| G4 lead on phrase: before; now (prototype) | none; 263/263 (259/259) | 246/255; 315/315 (314/314) | none; 285/285 (282/282) | none; 241/241 (241/241) |
| G5 lead windows, values, SD: before; now (prototype) | none; 723, 20, 12.23 (723, 20, 13.04) | 850, 30, 11.85; 606, 33, 20.33 (606, 32, 20.15) | none; 754, 21, 11.05 (754, 21, 11.69) | none; 749, 14, 10.01 (749, 14, 10.61) |
| R10 lead: before; now (prototype) | none; 0 of 3,052 (0) | 71‰ (296 of 4,188); 8‰ (25 of 3,212) (1‰) | none; 0 of 3,128 (0) | none; 0 of 2,996 (0) |
| Failing gates: before; now (= prototype) | G4 G5 G7 G9; G7 G9 | G9; G9 | G4 G5 G7 G9; G7 G9 | G1 G2 G4 G5 G9; G1 G2 G5 G9 |

- G4 for the bass and chords is unchanged (FUT01 496/721 and 517/775). FUT04 still fails G5 on its ghost group alone (no ghost windows), S1b2d's open item. G8 passes on all four: the new lead lanes add a swing tier (FUT04's seeds with 4 swings, 0 of 64 before, are 34 now), and FUT02 keeps 3, 4 and 5 swings in 18, 32 and 14 seeds (18, 31 and 15 before). FUT02's R10 lead, 8‰ against the prototype's 1‰, is inside the 30‰ pin.
- Reported, lead: R1 lag-4 785, 769, 784 and 717‰ (written 791-1000‰); R6 median rest 406, 250, 375 and 625‰ (FUT02's old arp 125‰; written 375-812‰); R7 step ratio 346, 109, 332 and 271‰, bar 1 = bar 3 1000, 976, 1000 and 1000‰; R11 4.17, 1.74, 5.45 and 3.63 a bar (written leads 1.8-5.5). Every off-key lead note is tagged borrowed (I1).

**ADR 0023's drop rows, with the hook** (64 seeds, every lane's notes a bar; before is ADR 0023's, the prototype's in brackets):

| | FUT01 | FUT02 | FUT03 | FUT04 |
|---|---|---|---|---|
| Drop 1 against the grooves, pooled (band 1.10) | 1.111 → 1.163 (1.161) | 1.237 → 1.281 (1.278) | 1.132 → 1.147 (1.146) | 1.217 → 1.257 (1.256) |
| Seeds with drop 1 under its grooves: under 1.0; under 1.10 | 11; 26 → 5; 13 | 1; 5 → 0; 2 | 7; 23 → 6; 19 | 0; 10 → 0; 6 |
| Seeds with drop 2 thinner than drop 1 | 29 → 31 | 0 → 9 | 27 → 29 | 27 → 26 |

**Where the hook plays** (64 seeds per kit; equal to the prototype's figures but the rules on 1,027 seeds):

| | FUT01 | FUT02 | FUT03 | FUT04 |
|---|---|---|---|---|
| The hook in 3 sections or more, before drop 1, in half the drop bars or more | 64/64 | 64/64 | 64/64 | 64/64 |
| Drop and drop-2 bars with the hook, pooled (1,027 seeds) | 895‰ (881‰) | 989‰ (988‰) | 870‰ (878‰) | 1000‰ (1000‰) |
| Per drop 1, min and median; per drop 2 | 666, 1000‰; 750, 800‰ | 906, 968‰; 1000, 1000‰ | 666, 1000‰; 750, 750‰ | 1000, 1000‰; 1000, 1000‰ |
| Drop 1 opens on | SKELETON 34, FULL 18, OUT 12 (r2) | FULL 64 | SKELETON 30, FULL 19, OUT 15 (r2) | SKELETON 44, FULL 20 |
| Drop 2 opens on B | 64/64 | 64/64 | 64/64 | 64/64 |
| The first groove before drop 1 without a carrier note | 0 | 23 (the arp never enters it, as before) | 0 | 0 |
| On 1,027 seeds: rules broken; lines r1, r2 and r3 decided | 0; 445, 238, 0 | 0; 230, 0, 102 | 0; 438, 240, 0 | 0; 338, 0, 0 |

**The hook tests' figures** (64 seeds unless noted; the prototype's in brackets): 64 distinct hooks per kit (64), on 54, 38, 43 and 45 distinct rhythms (FUT01 52, FUT03 44); 60, 63, 61 and 59 riffs ending off the tonic (58-63); strong lead notes on chord tones 897, 937, 902 and 1000‰ (895, 939, 905, 1000); the peak in bar 1 in 14 FUT01 and 30 FUT03 seeds (12, 29), a twin top in 25 FUT02 and 35 FUT04 seeds (25, 35); no move over the cap in 1,027 seeds per kit (16, 45, 11 and 0 realized); the median SD of a FULL line's velocities 12.99, 21.31, 10.94 and 14.10 (12.9, 21.0, 11.5, 14.5); no rub against the chords in 66,530, 147,148, 73,316 and 45,796 sounding pairs (none); top notes off the material's key, from the fit, the guard and the line ends, 56, 74, 75 and 99‰, none rested (the guard alone: 27, 59, 40, 97‰); lead notes at or under the top sounding chord note 139, 175, 127 and 17‰ (148, 174, 129, 17), FUT01 and FUT03 in the written register 326 and 317‰ (345, 332); FUT02's 927 pedal notes per 1,000 top notes (953) and drop 2 at 54.3 notes a bar against drop 1's 48.5 (54.2, 48.3); no `--bars` cut at an 8-bar line of 16 seeds per kit loses the whole song's hook.

- **The files.** Every FUT01, FUT03 and FUT04 MIDI file gains its lead track on channel 2, FUT02's 'Arp Chrome' plays the hook's lines, and every `regions.json` lists them: 1,266 of the 1,372 snapshot files change, each only where the hook plays (the 106 others are FUT02 regions the arp never plays in). The snapshots were regenerated in a commit of their own in each PR.
- **The cost.** `generate()` takes 3.4, 3.1, 3.5 and 2.8 ms a song (FUT01 2.8 and FUT02 2.8 without the hook); the timing tests' 128 bars with both versions' files take 5.0, 4.5, 5.1 and 3.7 ms, against 50.
- **FUT02's drop 2** is now thinner than drop 1 in 9 of 64 seeds (65 of 1,027), where it never was (plan risk 3, decision 14), and FUT02's pedal sits inside the generated pad (175‰ of the arp's notes at or under the pad's top; the written pad rests under the arp): thinning the pad under FULL arp lines is for the styles' retune.
- **The half-bar mask is nearly full.** A line's mask marks a half busy when a note starts in it or sounds into it, and the call keeps each bar's last onset: FRAGMENT lines leave 0.4, 0.3, 0.3 and 0.0 of their 16 halves free on average, FULL, SKELETON, ANSWER and NEW lines none, a BREATH ending 2, OUT lines all 16. PR 4's plan has the reply play in the free halves under FRAGMENT, SKELETON and AUGMENT, so it would rarely play there: its design needs the mask's resolution or the call revisited first.
- **Open:**
  - PR 3: drop 2's bass lines from the written mixes and FUT01's talk. Its plan pins R10 for the bass at 30‰ or under per kit; FUT04's acid already reads 30‰ (144 of 4,784 windows) on 64 seeds, unchanged by the hook.
  - PR 4, the vox reply: after ADR 0016, and after the mask above.
  - S1b2d (decision 10): G4 for leads in 2-bar units, which would let the written bar-by-bar call and answer replace the half-bar call through data; R1's lead band and R7's RIFF bands at the written values.
  - The S1d listen: the guard's bends (decision 7; the low 9ths in FUT02's pad and FUT04's drone and the high chord tops are logged for the styles' retune), AUGMENT's and SEQUENCE's weights (decision 8), FUT03's direction (decision 9), the register (decision 13) and FUT02's drop 2 (decision 14). Each is data.
  - Not verified: how any of it sounds; nothing was played in Wubansia, Live 12 or Logic Pro. The macOS arm64 snapshot job has not run on this work's branch.

## Alternatives
- **Chord-relative degrees, each motif re-pitched at every chord change** (plan §4.1): an arpeggiator's tell; the written riffs keep one pitch set over the progression.
- **SENTENCE** for the break: no written top line is one.
- **Drop 1 opening on OUT now**, as written (the plan counted 8 bars with nothing on top in 46 of 64 FUT01 drops until the accents or the vox land), or on FULL on every drop's downbeat: the plan's decision 4.
- **The written register** for FUT01 and FUT03 (from F#4 and G4): the hook at or under the generated chords' top 326 and 317‰ of the time, against the written leads' 199 and 178‰ (decision 13).
- **A mud rule on the stacks' own thresholds** (a minor 9th from 60, a semitone from 64): looser than the written mixes, which never sound a minor 9th either way against their lead (decision 7).
- **FUT02's arp out of the roster:** a review counted 283 non-arp salient changes moving over seeds 0-63 (decision 11).
- **A lighter FUT02 drop 2**, nearer the written PSC-: thinner than drop 1 in 10 of 64 seeds and 133 of 1,024 in the prototype (decision 14).
- **FUT02 without the tonic start** ([0, 0, 40, 0, 30, 0, 10]): 97.9‰ of top notes at or under D5, but drop 2's glass phrase then always opens on D6 and recurs across seeds (R10 lead 17‰).

## Sources
- `docs/plans/S1b2b-plan.md` (the plan as approved, its decisions 1-14, Review notes and risks); `docs/plans/S1b2-plan.md` (step 2); `docs/plans/S1-futuristic-slice.md` §2.3, §4.1, §5.1.
- ADR 0013 §1 (the counter RNG, permanent tags), §2 ("Reserved for S1b2"); ADR 0016 (the vox synth); ADR 0019 (MIDI v2) and `docs/midi-map.md`; ADR 0023 (the form grammar).
- The kits' written MIDI: `content/midi/fut0N/` (the lead riffs and phrases, `FUT02_Lead_ChromeArp_2bars.mid` among them) and the written extended mixes, `content/midi/fut0N/extended/vox/regions.json`.
- `core/gen/hook.cpp`, `lead.cpp`, `content.h` (`HookSpec`), `song.h`; `core/tests/test_gen_hook.cpp` (tests 1-26), `test_rng.cpp`, `test_gen_gates.cpp`; `core/tests/fixtures/`.
