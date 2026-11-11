# 0023. Generator v2, part 2a: the form grammar

**Status:** Proposed, 2026-11-11 (S1b2a, step 1 of the S1b2 plan). It does ADR 0013 §2's last two "Reserved for S1b2" bullets (lanes moving inside the drops; drop and break starts on the 8-bar grid) and replaces §4's one form per style.

## Context
Part 1 (ADR 0013) gave each style one fixed extended mix, so every seed of a kit had the same arrangement: G10 (form diversity) failed on every style with 1 form in 64 seeds. Every drop and drop 2 listed every lane for all its bars, and FUT01's drop was no denser than its groove (50.8 against 50.7 notes a bar).

The brief for this step: draw the form per seed, as club extended mixes are put together; move lanes inside the drops; pass G10 on 64 seeds per style without G4 regressing; keep each style's conventions in its JSON as data, with loader rules, and one engine; keep each genre honest (FUT01 2-step garage, FUT02 sci-fi club, FUT03 garage-house / deep tech house, FUT04 dark industrial techno). The user's standing brief: "midis that aren't written like AI", "I hate muddy". A rule earns its place by being how producers arrange.

Two sources set the conventions: the placement measurement ADR 0013 cites (drop and break starts about 70% on 16-bar lines and 30% on 8 mod 16, on 1,020 annotated four-on-the-floor tracks), and the four kits' own written extended mixes (`tools/sound/recipes/fut0N/midi.py`, `FORM`), which the user heard in v0.1 ("Sounds great"). Those put a break before the first drop's build, a switch-up after the first drop, drop 2 straight out of break 2, and in techno drop 2 straight out of drop 1; they bring lanes in line by line through the groove; and inside the drops they take the kick out for 8 bars (FUT01's drop 2), the chords out on alternate lines (FUT04), the drums out for a bar before the 16-bar line (FUT04), and strip drop 2 at its end (FUT01, FUT03).

## Decision

### 1. The grammar, as data
A style's `"form"` (`core/gen/content.h`, `FormSpec`, which documents every field) is:
- **`bars`**, the whole form's length range (200 to 240; techno 208 to 240), and **`sixteen`**, the percent of drop, drop2 and break starts on a 16-bar line (70 in every style, the measured figure).
- **`sections`**, slots in order, each a kind and its lengths by weight (`[ { "bars": 16, "w": 2 }, ... ]`, multiples of 8). A length of 0 leaves an optional slot out.
- **`plans`**, one per kind: the mix-in and mix-out take a whole 32-bar choice of 4 blocks, by weight; every other kind plays its `lanes` in every block but for its **`entries`** (lanes that join `after` N bars, play only the `last` N, or leave N bars `before` the end, N drawn per section), its **`rest`** (one lane, drawn by weight, out for 8 or 16 bars from a multiple of that length, never the section's first, with chance `p`), its **`dropout`** (before each of the section's own lines every 16 or 32 bars inside it, with chance `p`, a lane group drawn by weight sits that one bar out) and **`change`** (every section of the kind has a lane change inside it: if the draws gave none, a rest, else a dropout).
- **`layer`**, optional: drop 2's layer, drawn per song by weight. It plays all through every drop2 section and never in a drop.

### 2. What a seed draws (`core/gen/arrange.h`)
Under `root.with (tag ("form"))`, in this order:
1. **The lines** (`"line"`, per slot): each drop, drop2 and break start on a 16-bar line with chance `sixteen`, else on 8 mod 16.
2. **The lengths** (`"bars"`): every slot drawn by its own weights (`"attempt"`, per try and slot), the whole form kept if it keeps those lines and ends inside `bars`: the grammar's weights given the lines. After 1,024 tries without one, each slot in order by weight among the lengths that still leave the rest a way to fit (`"fit"`, a feasibility table over 8-bar units). The loader refuses a grammar where any combination of lines it can draw has no fitting lengths, so a form always comes out. Measured on the shipped grammars, a seed needs the fallback with a chance of about 0.07% (techno) or less.
3. **Drop 2's layer** (`"layer"`).
4. **Each section's blocks**: a choice (`"choice"`), or the plan's lanes with each entry's length (`"entry"`), the layer taken out of drop 1, then the rest (`"rest"`: `"p"`, `"at"`, `"lane"`) and the dropouts (`"dropout"`: `"p"`, `"lanes"`), and the forced change.

All integer, no unordered containers, fixed caps; the tag strings are permanent once released (ADR 0013 §1). The salient changes and turnarounds (`"arrange"`) are drawn as before, on the drawn sections.

A **dropout** is part of the section (`Section::dropouts`): `active()` and `activeRanges()` honour it, so every lane of the group stops for that bar, held pads and bass notes end before it and come back on the line after it. Dropouts sit only in the bar before the section's own 16-bar line (bar 15, 31, 47...), never in its last bar: a dropout into the next section is a transition, which is S1b2c's (fills and transitions).

### 3. The bands, and where they come from
| What | Band | Source |
|---|---|---|
| Drop, drop2 and break starts on a 16-bar line | 70% drawn per start; tested 60% to 80% on 64 seeds (about 220 starts a style, so ±3 standard deviations) | the measurement ADR 0013 cites |
| The form's length | 200 to 240 bars (5:52 to 7:30 at the kits' tempos) | the S1b2 plan |
| Builds | 8 to 32 bars, each into a drop | ADR 0013 §2; the kits' written builds (8 or 16), techno's long ones (32) |
| Drop 1 against the grooves | at least 1.10 times the notes a bar, on 64 seeds | ADR 0013 §2 |
| Lanes inside the drops | at least one change inside every drop and drop 2 | ADR 0013 §2 |
| G10 | at least 8 forms in 64 seeds, the top one in at most 16, change intervals with 2 values or more | plan §5.1 |

### 4. Each style's conventions
| | FUT01 2-step garage | FUT02 sci-fi club | FUT03 garage-house | FUT04 techno |
|---|---|---|---|---|
| Slots | intro 32, groove 16-32, [break 8-16], build 8-16, drop 16-32, [groove 8-16], break 16-32, [build 8-16], drop2 16-40, outro 32 | the same, drop2 16-48 | the same, drop2 16-32 | intro 32, groove 32-64, break 16-32, build 8-32, drop 32-64, [break 16-32], [build 8-16], drop2 32-64, outro 32 |
| Mix-in | drums only to the sub: the kick and hats first, the data ticks, the snare, the sub; the Alloy perc waits for the groove | the kick (and hats), the zaps, the clap, the bass | the kick with hats or shaker, the clap, the bass; the plings wait | the kick, the hats, the rumble, the snare; the anvil and the acid wait |
| Groove | the stabs with the groove or after 8 bars, the perc after 16 to 32 | the pad with the groove or after 8, the arp after 8 to 32 | the plings first (0 or 8), the stabs after 16 to 32 | the anvil within 8 bars, the acid after 8 to 32, the drone after 16 to 32 |
| Build | no kick for 8 bars, the kick back for the second half of 16 | the kick and bass out for its last 8 | the kick through | the kick and rumble out for its last 8 or 16 |
| Break | the pad alone or with hats, the perc after 8 to 16, the sub in its last 8 | the pad and arp, hats and zaps joining, the bass in its last 8 | the shaker leaving, hats and plings joining, the bass in its last 8 | the drone, hats and anvil joining, the snare and acid in its last 8 to 16 |
| Drop 1 | the kick (or the stabs) out for 8 bars; the kick and sub pulled for a bar before the 16-bar line | the kick (or the pad) out for 8; the kick and bass pulled for a bar | the kick or the stabs (or the hats) out for 8; the kick and bass pulled for a bar | the drone (or the acid) out for 16; the kick and rumble pulled for a bar |
| Drop 2 | the kick (or the hats) out for 8; the stabs strip for its last 8 in some songs; its own stab grid | the laser zaps join (the layer, on their 3/16 roll); the arp in after 8 in some; the pad, hats or kick out for 8 | the hats, shaker or kick out for 8; the stabs strip for its last 8 in some; its 3-3-2 stabs | the snare and clap backbeat join (the layer); the drone (or the hats) out for 16 |
| Mix-out | lanes leave down to the kick and hats (or the data ticks) | down to the kick and hats | down to the kick and hats | down to the kick and hats, the acid kept for its first 16 bars (its filter closing) |

Drop 2's layer is used where it doesn't hollow out drop 1: holding FUT03's plings or FUT04's anvil out of the first drop broke G8 there (G8 is measured on the first drop, and those lanes carry its third swing), and FUT01's percussion layers (about 10 notes a bar each) left its drop 1 thinner than its groove. Their drop 2 differs by its own grids, as in part 1, and by the strip at its end.

Example forms (seeds 1 to 3):
- FUT01: intro 32, groove 32, build 8, drop 32, groove 16, break 16, build 8, drop2 24, outro 32 (200 bars); intro 32, groove 32, break 8, build 16, drop 32, groove 8, break 24, build 16, drop2 40, outro 32 (240); intro 32, groove 32, break 8, build 8, drop 32, groove 16, break 16, build 8, drop2 24, outro 32 (208).
- FUT02: intro 32, groove 16, build 16, drop 32, groove 16, break 16, build 16, drop2 32, outro 32 (208); intro 32, groove 24, break 8, build 16, drop 32, break 24, build 16, drop2 32, outro 32 (216); intro 32, groove 24, build 16, drop 24, groove 16, break 32, drop2 32, outro 32 (208).
- FUT03: intro 32, groove 16, build 16, drop 32, groove 8, break 32, build 8, drop2 32, outro 32 (208); intro 32, groove 16, break 16, build 16, drop 32, groove 16, break 24, build 8, drop2 32, outro 32 (224); intro 32, groove 32, break 16, build 16, drop 16, break 32, build 8, drop2 32, outro 32 (216).
- FUT04: intro 32, groove 56, break 16, build 16, drop 32, break 16, build 8, drop2 32, outro 32 (240); intro 32, groove 32, break 24, build 8, drop 32, break 16, build 8, drop2 32, outro 32 (216); intro 32, groove 56, break 24, build 16, drop 32, break 16, drop2 32, outro 32 (240).

FUT01 seed 1's drop, block by block: [all seven lanes] [the kick out] [all] [all]; its drop 2: [all] [the kick out] [all].

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
