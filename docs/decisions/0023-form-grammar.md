# 0023. Generator v2, part 2a: the form grammar

**Status:** Proposed, 2026-11-11, and reworked after its review and re-review, 2026-11-13 (S1b2a, step 1 of `docs/plans/S1b2-plan.md`). It does ADR 0013 §2's last two "Reserved for S1b2" bullets (lanes moving inside the drops; drop and break starts on the 8-bar grid) and replaces §4's one form per style. The figures it changes in ADR 0013 are under Consequences. ADR 0026 (S1b2b, the hook, accepted 2026-11-15) amends it, 2026-11-16: §4's FUT02 rows, since FUT02's arp now plays the hook's lines, and the Consequences re-measured with the hook (after S1b2a's table, which stays its record); `regions.json` also lists each region's hook lines (ADR 0026 §6).

## Context
Part 1 (ADR 0013) gave each style one fixed extended mix, so every seed of a kit had the same arrangement: G10 (form diversity) failed on every style with 1 form in 64 seeds. Every drop and drop 2 listed every lane for all its bars, and FUT01's drop was no denser than its groove (50.8 against 50.7 notes a bar).

The brief for this step: draw the form per seed, as club extended mixes are put together; move lanes inside the drops; pass G10 on 64 seeds per style without G4 regressing; keep each style's conventions in its JSON as data, with loader rules, and one engine; keep each genre honest (FUT01 2-step garage, FUT02 sci-fi club, FUT03 garage-house / deep tech house, FUT04 dark industrial techno). The user's standing brief: "midis that aren't written like AI", "I hate muddy". A rule earns its place by being how producers arrange.

The conventions come from three sources:
- the placement measurement ADR 0013 cites: drop and break starts about 70% on 16-bar lines and 30% on 8 mod 16, on 1,020 annotated four-on-the-floor tracks;
- the moves ADR 0013 §2 reserved for the drops (a lane out for 8 bars, a one-bar dropout before a 16-bar line, drop 2 adding a layer drop 1 lacks), step 1 of `docs/plans/S1b2-plan.md`;
- the four kits' own written extended mixes, which the user heard in v0.1 ("Sounds great"): `content/midi/fut0N/extended/vox/regions.json`, written from `FORM` in `tools/sound/recipes/fut0N/midi.py`.

The written mixes put a break before the first drop's build, a 16-bar switch-up after the first drop (FUT01-FUT03), drop 2 straight out of break 2, and in techno drop 2 straight out of drop 1. They mix in and out on drums alone, bring lanes in line by line through the groove, and keep the bass out of the breaks (FUT04's acid aside, in its break's second half). None rests its first drop's kick or its second drop's hats; FUT02's and FUT03's keep the four on the floor through drop 2, FUT04's drops never rest its acid, and FUT02's zaps and FUT04's snare play every line of both drops. Inside the drops they take the kick out for 8 bars (FUT01's drop 2), the drone out on alternate lines (FUT04's drop 1), the drums out for a bar before the 16-bar line (FUT04's drop 1), and strip drop 2 at its end (FUT01's down to drums and bass, FUT03's lead and vox, FUT04's drone). Where a first draft of the grammar worked against them (drop 1 resting its kick, drop 2 resting its hats or the house kick, FUT04's drop 1 resting its acid, drop 2's layer holding the zaps and the snare out of drop 1, a break playing the sub that the build after it took away, lanes coming back as the mix-out began), the grammar now follows them; §4 lists where it still departs, for the listen.

## Decision

### 1. The grammar, as data
A style's `"form"` (`core/gen/content.h`, `FormSpec`, which documents every field) is:
- **`bars`**, the whole form's length range (200 to 240; techno 208 to 240), and **`sixteen`**, the percent of drop, drop2 and break starts on a 16-bar line (70 in every style, the measured figure).
- **`sections`**, slots in order, each a kind and its lengths by weight (`[ { "bars": 16, "w": 2 }, ... ]`, multiples of 8; a length of 0 leaves an optional slot out). A slot may start its section on its own pattern variant (`"variant"`, 0 to 15), so a kind that comes back plays new patterns: the groove after drop 1 starts on variant 14 in FUT01-FUT03, not on groove 1's opening bars.
- **`plans`**, one per kind. The mix-in and mix-out take a whole 32-bar choice of 4 blocks, by weight. Every other kind plays its `lanes` in every block but for:
  - its **`entries`**: lanes that join `after` N bars, play only the `last` N, or leave N bars `before` the end, N drawn per section;
  - its **`rest`**, with chance `p` per section: one lane, drawn by weight, out for 8 or 16 bars from a multiple of that length, never the section's first block, and only through bars it would play. A drum lane rests inside the section and is back for its last block, so the kick never comes back as the next, quieter section starts; a melodic lane may rest through the end, as the written drop 2s strip theirs;
  - its **`dropout`**, with chance `p` per line: in the bar before each of the section's own lines every 16 or 32 bars inside it (never its last bar), a lane group drawn by weight sits that bar out, every lane of it that plays the bar, and they land back on the line together. A group none of whose lanes plays the bar, or with a lane that leaves on the line (the kick's rest starting there), isn't drawn: the rest of the group alone would be a gap with nothing landing after it. A line less than 32 bars after the section's last pull isn't drawn either: the same pull before every 16-bar line is an edit on a loop;
  - **`change`**: every section of the kind has a lane change inside it; if the draws gave none, a rest, else a dropout.
- **`layer`**, optional: drop 2's layer, a lane drawn per song by weight that plays all through every drop2 section and never in a drop. No shipped style has one (Alternatives); a fixture grammar keeps it tested.

### 2. What a seed draws (`core/gen/arrange.h`)
Under `root.with (tag ("form"))`, in this order:
1. **The lines** (`"line"`, per slot): each drop, drop2 and break start on a 16-bar line with chance `sixteen`, else on 8 mod 16.
2. **The lengths** (`"bars"`): every slot drawn by its own weights (`"attempt"`, per try and slot), the whole form kept if it keeps those lines and ends inside `bars`: the grammar's weights given the lines. After 1,024 tries without one, each slot in order by weight among the lengths that still leave the rest a way to fit (`"fit"`, a feasibility table over 8-bar units). The loader refuses a grammar where any combination of lines it can draw has no fitting lengths, so a form always comes out (and `generate` refuses an empty one). On 100,000 seeds per style only techno needs the fallback, in 453 seeds (0.45%), the first at seed 304: fixture grammars test it, and a test pins it on FUT04 seed 304.
3. **Drop 2's layer** (`"layer"`), for a style that has one.
4. **Each section's blocks**: a choice (`"choice"`), or the plan's lanes with each entry's length (`"entry"`), the layer taken out of drop 1, then the rest (`"rest"`: `"p"`, then its lane, `"lane"`, then its start, `"at"`), the dropouts (`"dropout"`, per line: `"p"`, `"lanes"`) and the forced change. A mix-out choice whose first block brings back a non-drum lane the section before it ends without weighs 0, so lanes only leave from that section on (drums may come back: a DJ mixes on the kick); the loader keeps a choice for every way that section can end.

All integer, no unordered containers, fixed caps; the tag strings are permanent once released (ADR 0013 §1). The salient changes and turnarounds (`"arrange"`) are drawn as before, on the drawn sections.

A **dropout** is part of the section (`Section::dropouts`): `active()` and `activeRanges()` honour it, so every lane it pulls stops for that bar: held pads and bass notes end before it and come back on the line after it. Dropouts sit only in the bar before the section's own 16-bar line (bar 15, 31, 47...), never in its last bar: a dropout into the next section is a transition, which is S1b2c's (fills and transitions).

### 3. The bands, and where they come from
| What | Band | Source |
|---|---|---|
| Drop, drop2 and break starts on a 16-bar line | 70% drawn per start; tested 60% to 80% on 64 seeds (207 to 227 starts a style, so about 3 standard deviations either way) | the measurement ADR 0013 cites |
| The form's length | 200 to 240 bars, techno 208 to 240 (5:58 to 7:30 at the kits' tempos) | ADR 0013 §2; step 1 of `docs/plans/S1b2-plan.md` |
| Builds | 8 to 32 bars, each into a drop | ADR 0013 §2 (8 to 32 bars); the written builds run 8 or 16 |
| Drops | drop 1 24 bars or more, drop 2 32 or more and, in FUT01-FUT03, never shorter than drop 1 | the written mixes: drop 1 and drop 2 are 32 and 40 bars in FUT01, 32 and 32 in the others (a 16-bar drop spends half of itself on its rest) |
| Drop 1 against the grooves | at least 1.10 times the notes a bar, on 64 seeds | ADR 0013 §2 |
| Lanes inside the drops | at least one change inside every drop and drop 2 | ADR 0013 §2 |
| What the drops keep | in every bar but their one-bar pulls: drop 2's hats (FUT01-FUT03), FUT03's drop-2 shaker, FUT02's zaps and FUT04's snare in both drops, FUT04's acid in drop 1 (and in drop 2, whose rest never takes it) | the written mixes, which never rest them |
| The kick | in every bar of drop 1 but its one-bar pulls, and of drop 2 in FUT02 and FUT03 (FUT01's drop 2 rests it for 8 bars, its written move); back, as every drum lane, for a drop's last 8 bars after a rest; never out more than 32 bars in a row | the written mixes, which rest a drop's kick only in FUT01's drop 2 (8 of its 40 bars) and never go more than 16 bars without it; part 1's forms (32) |
| The lines between sections | a one-bar pull takes its whole group and never sits before a line one of its lanes leaves on, so its lanes land back on the line together; a drop opens with every non-drum lane that played into it; no bass or melodic lane plays a break's last bar, sits out the build and comes back with the drop; the mix-out brings no non-drum lane back and carries no chords | the written mixes (whose mix-outs are drums only: §4) |
| G10 | at least 8 forms in 64 seeds, the top one in at most 16, every seed's change intervals taking 2 values or more | plan §5.1 |

### 4. Each style's conventions
| | FUT01 2-step garage | FUT02 sci-fi club | FUT03 garage-house | FUT04 techno |
|---|---|---|---|---|
| Slots | intro 32, groove 16-32, [break 8-16], build 8-16, drop 24 or 32, [groove 8-16, on its own variant], break 16-32, [build 8-16], drop2 32 or 40, outro 32 | the same, but [build 16] before drop 2 and drop2 32 or 48 | the same as FUT01 | intro 32, groove 32-64, break 16-32, build 16 or 32, drop 32, 48 or 64, [break 16 or 24], [build 8 or 16], drop2 32, 48 or 64, outro 32 |
| Mix-in | drums only up to the sub: the kick and hats first (in some, with the data ticks), then the data ticks and the snare in either order, the sub for the last 8 bars; the Alloy perc waits for the groove | the kick (in most, with the hats), then the zaps and the clap in either order, the bass for the last 8 | the kick with the hats or the shaker, then the other one and the clap in either order, the bass for the last 8; the plings wait | the kick, with the hats or followed by them, then the rumble, the snare for the last 8; the anvil and the acid wait |
| Groove | the stabs from its start or after 8 bars, the Alloy perc after 16 to 32 | the pad from its start or after 8, the arp after 8 to 32, playing the hook's skeleton (ADR 0026) | the plings from its start or after 8, the stabs after 8 to 24 | the anvil from its start or after 8, the acid after 8 to 32, the drone after 16 or 32 |
| Build | the 2-step kick through, no sub | the kick and bass out for its last 8 (all of an 8-bar build) | the kick through, the bass for the first half of a 16-bar build (none in an 8) | the kick and rumble out for its last 8 (all of an 8-bar build) |
| Break | the pad, the hats from its start or after 8, the perc after 8 or 16; no sub | the pad (the arp's hook lines rest: ADR 0026), the hats from its start or after 8, the zaps after 8 or 16; no bass | the pad, the shaker leaving 8 or 16 bars before its end, the hats and plings joining after 8; no bass | the drone, the hats from its start or after 8, the anvil after 8 or 16, the snare in its last 8, the acid in its last 8 or 16 |
| Drop 1 | in most, the stabs (or the data ticks) out for 8 bars; in some, the kick and sub (a few with the stabs) pulled for a bar before the drop's own 16-bar line | in most, the pad out for 8; in some, the kick and bass (a few with the arp) pulled for a bar; the zaps all through | in most, the stabs, the plings or the shaker out for 8; in some, the kick and bass (a few with the stabs) pulled for a bar | in most, the drone out for 16 (in a 32-bar drop, its second half); in most, the kick and rumble (a few with the acid) pulled for a bar; the acid all through but those pulls; part 1's sparse snare and clap all through |
| Drop 2 | its own stab grid; the hats all through; in most, the kick (or the data ticks) out for 8, back for the last 8 at least; in some, the stabs strip for its last 8; in some, the kick and sub (a few with the hats) pulled for a bar, never before the kick leaves | the zaps' 3/16 roll, the pad's offbeat stabs and a driving arp, from its first bar, which plays the hook's glass period over its D5/A4 pedal, then the period, the hook or its answer (ADR 0026); the kick and hats all through; in most, the pad out for 8; in about half, the kick and bass (a few with the pad) pulled for a bar | its 3-3-2 stabs; the kick, hats and shaker all through; in most, the plings out for 8; in some, the stabs strip for its last 8; in some, the kick and bass pulled for a bar | the full snare and clap backbeat, the rides and the anvil's busier cycle; in most, the drone out for 16 (or, in a 48- or 64-bar drop 2, the hats); in most, the kick and rumble pulled for a bar |
| Mix-out | lanes leave down to the kick and hats (in some, with the data ticks), the sub gone after 8 or 16 bars; no chords | down to the kick and hats, the bass gone after 8 or 16; no chords | down to the kick and hats, the bass gone after 8 or 16; no chords | down to the kick and hats, the acid kept for its first 16 bars (its filter closing); no drone |

Since ADR 0026, FUT01, FUT03 and FUT04 also have a lead lane in their groove, break, build, drop and drop 2 plans, last and with no entry, rest or dropout group, so it moves no draw of the form: it plays the hook where ADR 0026's line plan puts it, and an OUT line takes it out of that block.

No style uses drop 2's layer (Alternatives). FUT02's zaps and FUT04's snare play every block of both drops, as every line of their written drops does. Drop 2 keeps what its written drop 2 keeps (the hats, FUT02's and FUT03's four on the floor, FUT03's shaker), and its rest takes FUT01's kick (the written drop 2's 8 bars without it) or its data ticks, FUT02's pad (the written drop 2 has none until its last line) or FUT03's plings (the written drop 2 thins its percussion on alternate lines). It lifts on its own grids where it has them (FUT02's zap roll, offbeat pad stabs and driving arp; FUT04's backbeat on 2 and 4, rides and busier anvil; FUT03's 3-3-2 stabs, whose lane plays 10.4 notes a bar over drop 2 against 9.6 over drop 1; FUT01's drop-2 stab grid is its own but no busier, 6.5 against 6.7), on its length and, in some, on a strip at its end. FUT03's form follows the kit's written garage-house extended mix, but its lane grids, its style's `about` and the kit's `direction` are still round 3's future house: ADR 0015 leaves the styles on round 3's kits until a retune step of their own.

**Where the grammar still departs from the written mixes**, for the S1d listen (counts on seeds 0-63):
- the sub comes in for the mix-in's last 8 bars (FUT01-FUT03), where the written mixes bring it with the groove; the mix-out keeps the bass for its first 8 or 16 bars (FUT04's acid for 16), where theirs are drums only;
- FUT04's build takes the kick and rumble out for its last 8 bars, where its written build keeps them to its last bar; FUT02's build plays the zaps, which its written build leaves out;
- FUT04's snare keeps part 1's grids. Its break plays it in its last 8 bars and its build rolls it from quarter notes to 16ths with claps on 2 and 4, where the written break and build have no snare (the written build rolls the anvil in its last bar). Its groove and drop 1 play beat 4 of every second bar with a clap now and then, where the written mix plays 2 and 4 in every groove and drop bar. So drop 1 lands, straight after the build's snare roll, without the backbeat the roll sets up (45 of 64 first bars have no snare or clap), and only drop 2 plays it;
- inside the drops: FUT01's and FUT03's drop 1 rest their stabs for 8 bars (43 and 30 songs), and FUT03's its shaker (13), which their written drop 1s play in every line; FUT04's drop 1 takes its drone out for its second half (42), where the written one takes it out on alternate lines; FUT01's drop 2 rests its kick in a 32-bar drop 2 as well as a 40-bar one, so in 6 songs drop 2 plays the kick in 24 bars against drop 1's 32 (the written drop 2 is 40 bars); FUT04's 48- or 64-bar drop 2 can rest its hats for 16 bars (3 songs), which the written drop 2 keeps;
- the groove after drop 1 is not the written switch-up (FUT01's four-on-the-floor bars and Speed bass need grids of their own);
- drop 2 has no material of its own beyond its grids yet (the written FUT02-FUT04 drop 2s change their bass line, FUT01's and FUT03's busy their hats), which is left to later steps: S1b2b's new phrase on the lead (ADR 0026), its drop-2 bass lines (S1b2b's PR 3) and the busier hats (S1b2c).

Example forms (seeds 1 to 3):
- FUT01: intro 32, groove 32, build 8, drop 32, break 32, build 8, drop2 32, outro 32 (208 bars); intro 32, groove 32, break 8, build 16, drop 32, groove 8, break 24, build 16, drop2 40, outro 32 (240); intro 32, groove 32, break 8, build 8, drop 32, break 16, build 8, drop2 32, outro 32 (200).
- FUT02: intro 32, groove 16, build 16, drop 32, groove 16, break 16, build 16, drop2 32, outro 32 (208); intro 32, groove 24, break 8, build 16, drop 32, break 24, build 16, drop2 32, outro 32 (216); intro 32, groove 16, build 8, drop 24, groove 16, break 16, build 16, drop2 48, outro 32 (208).
- FUT03: intro 32, groove 32, build 16, drop 24, groove 16, break 32, build 8, drop2 32, outro 32 (224); intro 32, groove 16, break 16, build 16, drop 32, groove 16, break 24, build 8, drop2 32, outro 32 (224); intro 32, groove 24, build 8, drop 32, groove 16, break 32, build 8, drop2 32, outro 32 (216).
- FUT04: intro 32, groove 40, break 32, build 16, drop 32, break 24, drop2 32, outro 32 (240); intro 32, groove 32, break 16, build 16, drop 32, build 8, drop2 48, outro 32 (216); intro 32, groove 56, break 24, build 16, drop 32, break 16, drop2 32, outro 32 (240).

FUT01 seed 1's drop, block by block: [all seven lanes] [the stabs out] [all] [all]; its drop 2: [all] [the kick out] [all] [all]; neither has a dropout.

### 5. The loader's rules
The loader refuses, with the file, the JSON path and the line, a grammar that could draw a form breaking a DJ rule:
- a length off the 8-bar grid, or a slot `"variant"` outside 0 to 15;
- anything but a 32-bar intro first and a 32-bar outro last (and only there);
- a build not followed by a drop or drop2 that always plays;
- a mix-in choice that isn't drums only in its first block, lanes only joining, never three blocks alike, and a mix-out the other way round, down to drums only; an empty edge block, or one without the lane a bass follows (the kick a DJ mixes on);
- a lane the mix-in ends on that the section after it doesn't play from its start: a late entry, a "before" entry that can sit the section out, drop 2's layer before a drop (lanes only join from the mix-in on);
- no mix-out choice that starts without the non-drum lanes the section before it can end without;
- an entry, rest or dropout lane the kind doesn't play, a lane in two entries, a rest lane that doesn't play all through the kind (an entry, or drop 2's layer), a dropout group without one that does;
- a "change" kind with a length that has room for neither its rest (twice the rest shorter than the length for a drum lane, which rests inside the section; at most the length for another lane) nor a dropout (a line inside it);
- a layer drop 2 doesn't play all through;
- a plan for a kind no slot names, or a slot kind without a plan;
- more than 12 drop, drop2 and break slots, and lines it can draw that no lengths keep inside the range.

The style loader also needs a controller row for every kind a plan can play a lane with controllers in, and `makeBundle` a grid for every kind a plan can play a lane in (its refusal names the kit, the style and the feel).

### 6. Export
- **`regions.json`** gains, per region, `"dropouts"`, `[]` when it has none (always in the intro and outro; in the shipped styles, also in every groove, build and break, whose plans have no dropout): one entry per one-bar dropout, with `startBar` (the song's 0-based bar, like the region's and its blocks'), `dawBar`, `bars` (always 1) and `lanes`, the lanes of that bar's block that sit it out and play the line after it (never empty). A block's `lanes` are the lanes that play, so a lane listed in a block is active in all its bars but the dropouts that name it. Bars are score positions (ADR 0013 §3: with feel, the next line's downbeat can start a few ticks inside the bar). `format` stays 1: no field is renamed or removed, and the markers are unchanged.
- **Region names** still carry no seed, and seeds of a kit can now share them or not; `writeFiles`' one-song-per-folder rule is unchanged (the full file's name and `regions.json` tell seeds apart).
- **A `--bars` cut** keeps the dropouts before it (`groovecore_cut_check` expects the same).

## Consequences
On 64 seeds per style (`core/tests/test_gen_form.cpp`, `test_gen_gates.cpp`), counted in the files `groovecore gen` writes, every percentage rounded once from its counts:

| | FUT01 | FUT02 | FUT03 | FUT04 |
|---|---|---|---|---|
| Distinct forms; the top one | 58; 2 seeds | 59; 2 | 62; 2 | 53; 3 |
| Starts on a 16-bar line | 158 of 223 (71%) | 159 of 220 (72%) | 163 of 227 (72%) | 142 of 207 (69%) |
| Length in bars (mean); time | 200-240 (213); 5:58-7:09 | 200-240 (220); 6:09-7:23 | 200-240 (214); 6:15-7:30 | 208-240 (228); 6:07-7:03 |
| Builds (8 / 16 / 32 bars) | 76 / 44 / 0 | 11 / 93 / 0 | 71 / 43 / 0 | 30 / 62 / 9 |
| Notes a bar: grooves, drop 1 (the lift) | 44.1, 49.0 (1.11x) | 40.4, 50.0 (1.24x) | 44.9, 50.9 (1.13x) | 32.5, 39.5 (1.22x) |
| Drop 1 against the groove before it | 1.08x (45.4) | 1.20x (41.8) | 1.10x (46.3) | 1.22x (32.5) |
| Notes a bar in drop 2 | 49.0 | 57.3 | 51.9 | 40.2 |
| Seeds with drop 1 thinner than its grooves; with drop 2 thinner than drop 1 | 11; 29 | 1; 0 | 7; 27 | 0; 27 |
| Bar-to-bar lane changes in a drop or drop 2 (mean, fewest) | 2.1, 1 | 2.0, 1 | 2.2, 1 | 1.9, 1 |
| Runs of bars with a lane out, in a drop or drop 2 (mean, most) | 1.12, 2 | 1.12, 2 | 1.13, 2 | 1.05, 2 |
| G4 on phrase lines: bass, chords, lead | 69%, 67%, none | 99%, 69%, 96% | 73%, 68%, none | 87%, 85%, none |
| Failing gates | G4 G5 G7 G9 | G9 | G4 G5 G7 G9 | G1 G2 G4 G5 G9 |

The lane changes count every bar whose lanes differ from the bar before it, as `test_gen_form.cpp` does, so a rest inside a drop or a one-bar dropout counts 2. The runs count each stretch of bars that plays fewer lanes than the drop's whole set (a rest, a pull or a strip): about one move a drop, two at most.

**With the hook** (ADR 0026, re-measured 2026-11-16 on the same 64 seeds, counted the same way; S1b2a's table above stays its record):

| | FUT01 | FUT02 | FUT03 | FUT04 |
|---|---|---|---|---|
| Notes a bar: grooves, drop 1 (the lift) | 45.7, 53.2 (1.16x) | 37.9, 48.5 (1.28x) | 47.8, 54.8 (1.15x) | 34.5, 43.4 (1.26x) |
| Drop 1 against the groove before it | 1.12x (47.3) | 1.26x (38.4) | 1.12x (49.0) | 1.26x (34.5) |
| Notes a bar in drop 2 | 53.3 | 54.3 | 55.7 | 43.9 |
| Seeds with drop 1 thinner than its grooves; with drop 2 thinner than drop 1 | 5; 31 | 0; 9 | 6; 29 | 0; 26 |
| Bar-to-bar lane changes in a drop or drop 2 (mean, fewest) | 2.3, 1 | 2.0, 1 | 2.5, 1 | 1.9, 1 |
| Runs of bars with a lane out, in a drop or drop 2 (mean, most) | 1.27, 3 | 1.12, 2 | 1.40, 3 | 1.05, 2 |
| G4 on phrase lines: bass, chords, lead | 69%, 67%, 100% | 99%, 69%, 100% | 73%, 68%, 100% | 87%, 85%, 100% |
| Failing gates | G7 G9 | G9 | G7 G9 | G1 G2 G5 G9 |

The lead adds more notes to the drops than to the grooves, so drop 1 clears the 1.10 band by more in every style (pooled: 1.163, 1.281, 1.147 and 1.257, against 1.111, 1.237, 1.132 and 1.217), and fewer seeds have a drop 1 under 1.10 times their grooves (13, 2, 19 and 6, against 26, 5, 23 and 10). FUT02's grooves and drop 1 are thinner than before, since its arp plays the hook's skeleton in the grooves and the hook over its pedal in the drops instead of 16ths; its drop 2 is now thinner than its drop 1 in 9 seeds, where it never was (ADR 0026's decision 14). The lead's OUT lines count as the drops' lane moves: FUT01's and FUT03's drop 2 menus strip the lead from their last line 3:1, and a drop 1 that opens without it (r2) brings it in on a later line, so those drops change lanes more often, with up to three runs.

- **G10 passes on all four styles**; every other verdict is part 1's, and G4 still fails only for want of a lead lane (S1b2b's hook; with it, G4 passes on all four and G5 on FUT01-FUT03, ADR 0026). `test_gen_gates.cpp` pins the lists and G4's bass and chords at 50% or more.
- **Drop 1's lift is the grooves'.** Drop 1 is a little thinner than part 1's in every style (49.0, 50.0, 50.9 and 39.5 notes a bar, against 50.8, 50.9, 53.0 and 41.1): its rest and its pulls take notes out. The grooves are thinner still (44.1, 40.4, 44.9 and 32.5, against 50.7, 44.2, 47.8 and 37.5): their lanes join late, as the written mixes bring theirs in, and the groove after drop 1 (FUT01-FUT03), 8 or 16 bars long, ends before most late lanes join. Against the groove before it alone, FUT01's drop 1 is 1.08x; against all its grooves it clears the 1.10 band by 1% (1.111 on these 64 seeds, 1.131 on 1,024), so any further thinning of FUT01's drop 1 trips the test.
- **Per seed, drop 1 can still be thinner than its grooves** (11, 1, 7 and 0 seeds in 64; part 1: 30, 0, 4 and 4): a long groove whose lanes have all joined (in FUT01, its four-note groove stabs against drop 1's two- and three-note ones) against a drop 1 with a rest. S1b2b's hook (in at least half the drop bars) adds the drops' lead line: with it, 5, 0, 6 and 0 seeds (above).
- **Drop 2 keeps its drums; FUT01's is no denser than its drop 1.** Drop 2 keeps its hats in every bar but its pulls, and its kick too but for FUT01's written 8 bars: before the re-review it rested the hats in 13, 16 and 20 of 64 FUT01-FUT03 songs and the house kick in 15 (FUT02) and 13 (FUT03), where drop 1 kept them. On notes, FUT01's drop 2 is no denser than its drop 1 (49.0 against 49.0; thinner in 29 seeds): it lifts on its length and its strip, not on its stab grid. FUT03's is a little denser (51.9 against 50.9; thinner in 27), on its 3-3-2 stabs, with the shaker's 16ths playing through. The written mixes also busy drop 2's hats (and give FUT03's a new bass line): that is left for S1b2b's new material. FUT02's and FUT04's drop 2s lift on their own grids. FUT04's drop 2 is shorter than its drop 1 in 12 of 64 seeds (157 of 1,024), since its drop 1 can draw 48 or 64 bars; the written FUT04 mix has 32 and 32.
- **Lengths.** ADR 0013 §4's "Each style's form is an extended mix of 6 to 7½ minutes" is now 200 to 240 bars (techno 208 to 240): only FUT01 can end under 6 minutes, 200 bars at 134 BPM being 5:58 (14 of 64 seeds); FUT04's floor, 208 bars at 136 BPM, is 6:07. The section lengths follow the written mixes (FUT01's 40-bar drop 2, FUT04's 64-bar groove) and replace plan §2.3's "lengths drawn from {8, 16, 24, 32}", as ADR 0013 replaced its 112 to 160 bars in all: FUT01's and FUT03's drop 2 can be 40 bars, FUT02's 48, and techno's groove, drop and drop 2 up to 64.
- **Plan §2.3's form extras.** Its 8-bar switch-up inside drop 1 is the 8-bar rest, and its second short break the optional break slots; the written mixes' switch-up after drop 1 is the optional groove on its own variant. Its 4+4 false drop isn't built: the grammar has no 4-bar unit, and it is a transition (S1b2c). Its per-seed drop 2 (a four-on-the-floor switch, halftime, or 2-step with a new bass line), written for the one-kit slice, is each style's own drop 2 here. Both stay open (below).
- **The dropouts are G9 fills.** G9 counts a bar that misses two or more of its section's common kick steps as a fill ("a dropout counts as a fill", `tools/legacy_audit/README.md`), and every dropout sits on section bar 15 mod 16, G9's position 7. They raise that position's share of the fills to 19%, 40%, 46% and 50% (FUT03's 70 of 153, FUT04's 161 of 325, against G9's 60% cap), from 17%, 23%, 19% and 31% with every dropout's chance set to 0. FUT02's and FUT03's shares rose when drop 2 stopped resting the kick, whose 8 kickless bars counted as fills on other positions. S1b2c's fills must count them (plan §4.1: "dropouts count as fills").
- Kick and bass within 2 ticks, anchors within ±2 ticks, every section on an 8-bar line, the regions tiling the song and every `--bars` cut equal to the whole song's head all hold on the drawn forms (the existing tests, at every 8-bar line for the cuts). 128 bars take 3 to 5 ms in Release, against 50.
- **The generator snapshots** cover 1,372 files (part 1: 1,312; more sections on average). Clang 18 and GCC 13 give the same bytes; the macOS arm64 job checks AppleClang.
- **ADR 0013 §3's hold gap**, re-measured on the drawn forms: FUT04's rumble has a note on five or six of a song's six to eight inner section lines, and its duck still ends the note before at least 60 ticks short of the line; the acid starts 18 of FUT04's 436 inner lines, its duck at least 48 short; FUT01's to FUT03's bass starts none. So the rule still changes no shipped output: without it, no file of seeds 0-63 and the four large snapshot seeds changes, in either version.
- The CLI's cut tests moved to cuts that show their cases in the drawn forms (FUT01 seed 40 at bar 168, seed 0 at 120, seed 4 at 80, seed 16 at 96), plus a cut before a one-bar dropout (seed 8 at 72).
- **Open, for your decision at the S1d listen** (the lead keeps today's behaviour until then):
  - FUT04's drop 2 shorter than drop 1 (above): making it never shorter is one data line.
  - A melodic lane that strips a drop's end comes back at the next section (the mix-out aside) in 6, 21, 5 and 38 of 64 songs (FUT04's drone, which every drop-1 rest now takes). The other way is a strip in drop 2 only.
  - FUT01's drop-2 kick rest in a 32-bar drop 2 (§4's departures: 6 of 64 songs play the kick in fewer bars in drop 2 than in drop 1). Keeping it to a 40-bar drop 2, the written length, needs a rule the grammar lacks (a rest lane's shortest section), or FUT01's drop 2 always 40 bars.
  - FUT04's first drop without the backbeat its build sets up (§4's departures). Giving drop 1 the written backbeat leaves drop 2 to lift on its rides and anvil alone.
  - FUT03's label: the kit is garage-house, its style still round 3's future house (§4).
  - Plan §2.3's false drop and per-seed drop-2 variants (above).
- **Settled by the lead, 2026-11-13:** FUT04 seed 304 is not a snapshot seed. The form test pins its fallback form, its lines and its range, and core_tests runs on every platform, the macOS job included; a 17th snapshot seed would change all four kits' manifest entries for one song's bytes.

## Alternatives
- **A list of whole forms per style, one drawn per seed:** a handful of forms, the clockwork G10 catches, and no lane moves.
- **Lines left to the lengths:** the 70% share would be an accident of the weights, not the measured placement.
- **Exact weighted sampling by counting completions:** products of weights over ten slots overflow 64 bits; the free draws are the same distribution wherever they succeed, and the fallback is rare.
- **Dropouts on the song's 16-bar lines:** a drop starting on 8 mod 16 would drop out mid-phrase; the drop's own phrase lines are where producers pull the low end.
- **Drop 2's layer, a lane drop 1 holds out:** tried on FUT02's zaps and FUT04's snare, and removed. The written mixes play both in every line of both drops, and the layer hollowed out drop 1 (FUT04's first drop had no snare at all straight after the build's snare roll; it now plays part 1's sparse snare, §4). Held out of drop 1, FUT03's plings or FUT04's anvil break G8, which is measured on the first drop, where they carry its third swing (63 and 0 of 64 seeds keep three swings), and FUT01's Alloy perc leaves drop 1 thinner than its grooves (38.1 against 44.1 notes a bar).
- **A rest or a pull wherever the draws fall:** a drum lane resting through a drop's last block comes back as the next, quieter section starts; a pull before every 16-bar line is an edit on a loop; a pulled lane whose rest starts on the line never lands back, and a pull cut down to the lanes that do land back took the bass alone before a line where the kick's rest starts, a gap that resolved into the kick leaving.
- **Drop 2 resting the hats or the house kick, and FUT04's drop 1 its acid:** the final drop lost for 8 bars what the first keeps, the reverse of every written mix, and a 16-bar acid rest took the hook out for the drop's second half and brought it back in the quieter section after it.
- **Redrawing until each seed's drop 1 is denser than its groove:** a metric chased for itself; the hook is the musical answer.

## Sources
- ADR 0013 §1, §2 ("Reserved for S1b2"), §3, §4; ADR 0015 (the styles follow round 3's kits).
- `docs/plans/S1b2-plan.md` (step 1); `docs/plans/S1-futuristic-slice.md` §2.3 ("Form: a grammar, not one table"), §4.1, §5.1.
- `tools/legacy_audit/README.md` (G4, G8, G9 and G10 definitions).
- The kits' written extended mixes: `content/midi/fut01/extended/vox/regions.json` to `fut04`'s, written from `FORM` in `tools/sound/recipes/fut01/midi.py` to `fut04/midi.py`.
