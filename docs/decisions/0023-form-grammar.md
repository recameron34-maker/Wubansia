# 0023. Generator v2, part 2a: the form grammar

**Status:** Proposed, 2026-11-11, and reworked after its review, 2026-11-13 (S1b2a, step 1 of `docs/plans/S1b2-plan.md`). It does ADR 0013 §2's last two "Reserved for S1b2" bullets (lanes moving inside the drops; drop and break starts on the 8-bar grid) and replaces §4's one form per style. The figures it changes in ADR 0013 are under Consequences.

## Context
Part 1 (ADR 0013) gave each style one fixed extended mix, so every seed of a kit had the same arrangement: G10 (form diversity) failed on every style with 1 form in 64 seeds. Every drop and drop 2 listed every lane for all its bars, and FUT01's drop was no denser than its groove (50.8 against 50.7 notes a bar).

The brief for this step: draw the form per seed, as club extended mixes are put together; move lanes inside the drops; pass G10 on 64 seeds per style without G4 regressing; keep each style's conventions in its JSON as data, with loader rules, and one engine; keep each genre honest (FUT01 2-step garage, FUT02 sci-fi club, FUT03 garage-house / deep tech house, FUT04 dark industrial techno). The user's standing brief: "midis that aren't written like AI", "I hate muddy". A rule earns its place by being how producers arrange.

The conventions come from three sources:
- the placement measurement ADR 0013 cites: drop and break starts about 70% on 16-bar lines and 30% on 8 mod 16, on 1,020 annotated four-on-the-floor tracks;
- the moves ADR 0013 §2 reserved for the drops (a lane out for 8 bars, a one-bar dropout before a 16-bar line, drop 2 adding a layer drop 1 lacks), step 1 of `docs/plans/S1b2-plan.md`;
- the four kits' own written extended mixes, which the user heard in v0.1 ("Sounds great"): `content/midi/fut0N/extended/vox/regions.json`, written from `FORM` in `tools/sound/recipes/fut0N/midi.py`.

The written mixes put a break before the first drop's build, a 16-bar switch-up after the first drop (FUT01-FUT03), drop 2 straight out of break 2, and in techno drop 2 straight out of drop 1. They mix in and out on drums alone, bring lanes in line by line through the groove, and keep the bass out of the breaks (FUT04's acid aside, in its break's second half). None rests its first drop's kick, and FUT02's zaps and FUT04's snare play every line of both drops. Inside the drops they take the kick out for 8 bars (FUT01's drop 2), the drone out on alternate lines (FUT04's drop 1), the drums out for a bar before the 16-bar line (FUT04's drop 1), and strip drop 2 at its end (FUT01's down to drums and bass, FUT03's lead and vox, FUT04's drone). Where a first draft of the grammar worked against them (drop 1 resting its kick, drop 2's layer holding the zaps and the snare out of drop 1, a break playing the sub that the build after it took away, lanes coming back as the mix-out began), the grammar now follows them; §4 says where it still departs.

## Decision

### 1. The grammar, as data
A style's `"form"` (`core/gen/content.h`, `FormSpec`, which documents every field) is:
- **`bars`**, the whole form's length range (200 to 240; techno 208 to 240), and **`sixteen`**, the percent of drop, drop2 and break starts on a 16-bar line (70 in every style, the measured figure).
- **`sections`**, slots in order, each a kind and its lengths by weight (`[ { "bars": 16, "w": 2 }, ... ]`, multiples of 8; a length of 0 leaves an optional slot out). A slot may start its section on its own pattern variant (`"variant"`, 0 to 15), so a kind that comes back plays new patterns: the groove after drop 1 starts on variant 14 in FUT01-FUT03, not on groove 1's opening bars.
- **`plans`**, one per kind. The mix-in and mix-out take a whole 32-bar choice of 4 blocks, by weight. Every other kind plays its `lanes` in every block but for:
  - its **`entries`**: lanes that join `after` N bars, play only the `last` N, or leave N bars `before` the end, N drawn per section;
  - its **`rest`**, with chance `p` per section: one lane, drawn by weight, out for 8 or 16 bars from a multiple of that length, never the section's first block, and only through bars it would play. A drum lane rests inside the section and is back for its last block, so the kick never comes back as the next, quieter section starts; a melodic lane may rest through the end, as the written drop 2s strip theirs;
  - its **`dropout`**, with chance `p` per line: in the bar before each of the section's own lines every 16 or 32 bars inside it (never its last bar), a lane group drawn by weight sits that bar out. It pulls only the group's lanes that play that bar and the line after it, so each lands back on the line, and a line less than 32 bars after the section's last pull isn't drawn: the same pull before every 16-bar line is an edit on a loop;
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
| Builds | 8 to 32 bars, each into a drop | ADR 0013 §2; the written builds (8 or 16), techno's long ones (32) |
| Drops | drop 1 24 bars or more, drop 2 32 or more and, in FUT01-FUT03, never shorter than drop 1 | the written mixes: drop 1 and drop 2 are 32 and 40 bars in FUT01, 32 and 32 in the others (a 16-bar drop spends half of itself on its rest) |
| Drop 1 against the grooves | at least 1.10 times the notes a bar, on 64 seeds | ADR 0013 §2 |
| Lanes inside the drops | at least one change inside every drop and drop 2 | ADR 0013 §2 |
| The kick | in every bar of drop 1 but its one-bar pulls; back, as every drum lane, for a drop's last 8 bars after a rest; never out more than 32 bars in a row | the written mixes, which never rest drop 1's kick or go more than 16 bars without it; part 1's forms (32) |
| The lines between sections | a dropout's lanes play the line after it; a drop opens with every non-drum lane that played into it; no bass or melodic lane plays a break's last bar, sits out the build and comes back with the drop; the mix-out brings no non-drum lane back and carries no chords | the written mixes (whose mix-outs are drums only: §4) |
| G10 | at least 8 forms in 64 seeds, the top one in at most 16, every seed's change intervals taking 2 values or more | plan §5.1 |

### 4. Each style's conventions
| | FUT01 2-step garage | FUT02 sci-fi club | FUT03 garage-house | FUT04 techno |
|---|---|---|---|---|
| Slots | intro 32, groove 16-32, [break 8-16], build 8-16, drop 24 or 32, [groove 8-16, on its own variant], break 16-32, [build 8-16], drop2 32 or 40, outro 32 | the same, but [build 16] before drop 2 and drop2 32 or 48 | the same as FUT01 | intro 32, groove 32-64, break 16-32, build 16 or 32, drop 32, 48 or 64, [break 16 or 24], [build 8 or 16], drop2 32, 48 or 64, outro 32 |
| Mix-in | drums only up to the sub: the kick and hats first (in some, with the data ticks), then the data ticks and the snare in either order, the sub for the last 8 bars; the Alloy perc waits for the groove | the kick (in most, with the hats), then the zaps and the clap in either order, the bass for the last 8 | the kick with the hats or the shaker, then the other one and the clap in either order, the bass for the last 8; the plings wait | the kick, with the hats or followed by them, then the rumble, the snare for the last 8; the anvil and the acid wait |
| Groove | the stabs from its start or after 8 bars, the Alloy perc after 16 to 32 | the pad from its start or after 8, the arp after 8 to 32 | the plings from its start or after 8, the stabs after 8 to 24 | the anvil from its start or after 8, the acid after 8 to 32, the drone after 16 or 32 |
| Build | the 2-step kick through, no sub | the kick and bass out for its last 8 (all of an 8-bar build) | the kick through, the bass for the first half of a 16-bar build (none in an 8) | the kick and rumble out for its last 8 (all of an 8-bar build) |
| Break | the pad, the hats from its start or after 8, the perc after 8 or 16; no sub | the pad and arp, the hats from its start or after 8, the zaps after 8 or 16; no bass | the pad, the shaker leaving 8 or 16 bars before its end, the hats and plings joining after 8; no bass | the drone, the hats from its start or after 8, the anvil after 8 or 16, the snare in its last 8, the acid in its last 8 or 16 |
| Drop 1 | in most, the stabs (or the data ticks) out for 8 bars; in some, the kick and sub (a few with the stabs) pulled for a bar before the drop's own 16-bar line | in most, the pad out for 8; in some, the kick and bass (a few with the arp) pulled for a bar; the zaps all through | in most, the stabs, the plings or the shaker out for 8; in some, the kick and bass (a few with the stabs) pulled for a bar | in most, the drone (or the acid) out for 16; in most, the kick and rumble (a few with the acid) pulled for a bar; the sparse snare and clap all through |
| Drop 2 | its own stab grid; in most, the kick (or the hats) out for 8, back for the last 8 at least; in some, the stabs strip for its last 8; in some, the kick and sub (a few with the hats) pulled for a bar | the zaps' 3/16 roll, the pad's offbeat stabs and a driving arp, from its first bar; in most, the pad, the hats or the kick out for 8; in about half, the kick and bass (a few with the pad) pulled for a bar | its 3-3-2 stabs; in most, the hats, the shaker or the kick out for 8; in some, the stabs strip for its last 8; in some, the kick and bass pulled for a bar | the full snare and clap backbeat, the rides and the anvil's busier cycle; in most, the drone out for 16 (or, in a 48- or 64-bar drop 2, the hats); in most, the kick and rumble pulled for a bar |
| Mix-out | lanes leave down to the kick and hats (in some, with the data ticks), the sub gone after 8 or 16 bars; no chords | down to the kick and hats, the bass gone after 8 or 16; no chords | down to the kick and hats, the bass gone after 8 or 16; no chords | down to the kick and hats, the acid kept for its first 16 bars (its filter closing); no drone |

No style uses drop 2's layer (Alternatives). FUT02's zaps and FUT04's snare play both drops, as every line of their written drops does, and drop 2 lifts on its own grids (FUT02's zap roll, offbeat pad stabs and driving arp; FUT04's backbeat on 2 and 4, rides and busier anvil; FUT01's and FUT03's stabs), on its length and, in some, on a strip at its end. FUT03's form follows the kit's written garage-house extended mix, but its lane grids, its style's `about` and the kit's `direction` are still round 3's future house: ADR 0015 leaves the styles on round 3's kits until a retune step of their own.

**Where the grammar still departs from the written mixes**, for the S1d listen: the sub comes in for the mix-in's last 8 bars (FUT01-FUT03), where the written mixes bring it with the groove; the mix-out keeps the bass for its first 8 or 16 bars (FUT04's acid for 16), where theirs are drums only; FUT04's build takes the kick and rumble out for its last 8 bars, where its written build keeps them to its last bar; FUT02's build plays the zaps, which its written build leaves out; the groove after drop 1 is not the written switch-up (FUT01's four-on-the-floor bars and Speed bass need grids of their own); and drop 2 has no material of its own beyond its grids yet (the written FUT02-FUT04 drop 2s change their bass line, FUT01's and FUT03's busy their hats), which is left to a later step (S1b2b's new material).

Example forms (seeds 1 to 3):
- FUT01: intro 32, groove 32, build 8, drop 32, break 32, build 8, drop2 32, outro 32 (208 bars); intro 32, groove 32, break 8, build 16, drop 32, groove 8, break 24, build 16, drop2 40, outro 32 (240); intro 32, groove 32, break 8, build 8, drop 32, break 16, build 8, drop2 32, outro 32 (200).
- FUT02: intro 32, groove 16, build 16, drop 32, groove 16, break 16, build 16, drop2 32, outro 32 (208); intro 32, groove 24, break 8, build 16, drop 32, break 24, build 16, drop2 32, outro 32 (216); intro 32, groove 16, build 8, drop 24, groove 16, break 16, build 16, drop2 48, outro 32 (208).
- FUT03: intro 32, groove 32, build 16, drop 24, groove 16, break 32, build 8, drop2 32, outro 32 (224); intro 32, groove 16, break 16, build 16, drop 32, groove 16, break 24, build 8, drop2 32, outro 32 (224); intro 32, groove 24, build 8, drop 32, groove 16, break 32, build 8, drop2 32, outro 32 (216).
- FUT04: intro 32, groove 40, break 32, build 16, drop 32, break 24, drop2 32, outro 32 (240); intro 32, groove 32, break 16, build 16, drop 32, build 8, drop2 48, outro 32 (216); intro 32, groove 56, break 24, build 16, drop 32, break 16, drop2 32, outro 32 (240).

FUT01 seed 1's drop, block by block: [all seven lanes] [the stabs out] [all] [all]; its drop 2: [all] [the kick out] [all] [all]; neither has a dropout.

### 5. The loader's rules
The loader refuses, with the file, path, line and column, a grammar that could draw a form breaking a DJ rule: a length off the 8-bar grid; anything but a 32-bar intro first and a 32-bar outro last (and only there); a build not followed by a drop or drop2 that always plays; a mix-in choice that isn't drums only in its first block, lanes only joining, never three blocks alike (and a mix-out the other way round, down to drums only); a lane the mix-in ends on that the section after it doesn't play from its start (lanes only join from the mix-in on); an entry, rest or dropout lane the kind doesn't play, a lane in two entries, a rest lane that doesn't play all through the kind (an entry, or drop 2's layer), a dropout group without one that does; a "change" kind with a length too short for its rest and its dropouts; a layer drop 2 doesn't play all through; a plan for a kind no slot names, or a slot kind without a plan; and lines it can draw that no lengths keep inside the range. The style loader also needs a controller row for every kind a plan can play a lane with controllers in, and `makeBundle` a grid for every kind a plan can play a lane in.

### 6. Export
- **`regions.json`** gains, per region, `"dropouts"`: each one-bar dropout's `startBar`, `dawBar`, `bars` (1) and `lanes` (`[]` for none). No field is renamed or removed; the markers are unchanged.
- **Region names** still carry no seed, and seeds of a kit can now share them or not; `writeFiles`' one-song-per-folder rule is unchanged (the full file's name and `regions.json` tell seeds apart).
- **A `--bars` cut** keeps the dropouts before it (`groovecore_cut_check` expects the same).

## Consequences
On 64 seeds per style (`core/tests/test_gen_form.cpp`, `test_gen_gates.cpp`):

| | FUT01 | FUT02 | FUT03 | FUT04 |
|---|---|---|---|---|
| Distinct forms; the top one | 62; 2 seeds | 62; 2 | 61; 2 | 57; 3 |
| Starts on a 16-bar line | 161 of 226 (71%) | 167 of 225 (74%) | 167 of 231 (72%) | 146 of 211 (69%) |
| Length (mean) | 200-240 (214) | 200-240 (218) | 200-240 (212) | 208-240 (227) |
| Builds (8 / 16 / 32 bars) | 69 / 54 / 0 | 31 / 83 / 0 | 65 / 52 / 0 | 47 / 49 / 7 |
| Notes a bar: grooves, drop 1 | 44.0, 50.1 (1.14x) | 40.7, 47.5 (1.17x) | 42.1, 51.2 (1.22x) | 32.5, 38.1 (1.17x) |
| Seeds with drop 1 thinner than their grooves | 7 | 5 | 0 | 4 |
| Lane changes inside a drop (mean, fewest) | 2.0, 1 | 2.0, 1 | 1.9, 1 | 2.0, 1 |
| G4 on phrase lines: bass, chords, lead | 66%, 70%, none | 98%, 76%, 95% | 72%, 67%, none | 87%, 84%, none |
| Failing gates | G4 G5 G7 G9 | G9 | G4 G5 G7 G9 | G1 G2 G4 G5 G9 |

- **G10 passes on all four styles**; every other verdict is part 1's, and G4 still fails only for want of a lead lane (S1b2b's hook). `test_gen_gates.cpp` pins the lists and G4's bass and chords at 50% or more.
- Kick and bass within 2 ticks, anchors within ±2 ticks, every section on an 8-bar line, the regions tiling the song and every `--bars` cut equal to the whole song's head all hold on the drawn forms (the existing tests, at every 8-bar line for the cuts). 128 bars take 4 to 5 ms in Release, against 50.
- **The generator snapshots** cover 1,398 files (was 1,312: more sections on average). Clang 18 and GCC 13 give the same bytes; the macOS arm64 job checks AppleClang.
- **Per seed, drop 1 can still be thinner than its grooves** (7, 5, 0 and 4 seeds in 64): a long groove whose lanes have all joined, in FUT01 under its four-note groove stabs, against drop 1's two-note stabs. The aggregate gate holds with room; S1b2b's hook (in at least half the drop bars) adds the drops' lead line.
- ADR 0013 §4's "Each style's form is an extended mix of 6 to 7½ minutes" is now 200 to 240 bars: at 134 and 136 BPM, 200 bars is 5:58 and 5:52.
- The CLI's cut tests moved to cuts that show their cases in the drawn forms (FUT01 seed 40 at bar 168, seed 0 at 120, seed 4 at 104, seed 16 at 96), plus a cut before a one-bar dropout (seed 8 at 72).

## Alternatives
- **A list of whole forms per style, one drawn per seed:** a handful of forms, the clockwork G10 catches, and no lane moves.
- **Lines left to the lengths:** the 70% share would be an accident of the weights, not the measured placement.
- **Exact weighted sampling by counting completions:** products of weights over ten slots overflow 64 bits; the free draws are the same distribution wherever they succeed, and the fallback is rare.
- **Dropouts on the song's 16-bar lines:** a drop starting on 8 mod 16 would drop out mid-phrase; the drop's own phrase lines are where producers pull the low end.
- **Drop 2's layer in every style:** broke G8 on FUT03 and FUT04, and the density lift on FUT01 (§4).
- **Redrawing until each seed's drop 1 is denser than its groove:** a metric chased for itself; the hook is the musical answer.

## Sources
- ADR 0013 §1, §2 ("Reserved for S1b2"), §4; the S1b2 plan (step 1); `docs/plans/S1-futuristic-slice.md` §4.1, §5.1.
- `tools/legacy_audit/README.md` (G4, G8, G10 definitions).
- The kits' written extended mixes: `tools/sound/recipes/fut01/midi.py` to `fut04/midi.py`, `FORM`.
