# Legacy audit: the generator metrics, defined

This folder holds the "test of the test" for the generator metrics (`docs/plans/S1-futuristic-slice.md` §5.1). It has:
- one audit of the legacy MIDI baseline, in Node with no npm dependencies;
- a written definition of every gating metric (G1–G10), and of the reported metrics and invariants that legacy data can measure;
- hand-built fixture files with known answers.

The audit's output on the 516-file baseline is committed as `core/tests/legacy_expected.json`. The C++ metrics (`core/metrics`) read the same SMF bytes and must reproduce it: exactly, or within the tolerance stated per metric below. Both read MIDI bytes only, so the same code scores the legacy baseline and generator v2.

**`legacy_expected.json` is treated like the golden manifest.** Never regenerate it to make a test pass: if the C++ metrics disagree with it, fix the metric code (CLAUDE.md, "Determinism and golden files"). It changes only when a definition in this README changes, in its own commit, with the reason.

## Commands

From the repo root, with the Node that `.nvmrc` pins:

```sh
node tools/export_golden.js                         # the baseline, into build/golden/
node tools/legacy_audit/audit.js                    # a one-screen summary
node tools/legacy_audit/audit.js --check            # exit 1 unless the result equals core/tests/legacy_expected.json
node --test tools/legacy_audit/test/audit.test.js   # fixtures, reader, unit checks, and the baseline (about 5 s)
node tools/legacy_audit/fixtures/make_fixtures.js   # rewrite the fixtures (only after changing that script)
node tools/legacy_audit/section_table.js --check    # legacy_sections.json still matches core.js
```

`audit.js` also takes `--golden <dir>`, `--json` (prints the result) and `--write <file>`. The baseline test re-exports the baseline into a temporary folder, so it needs no `build/golden/`. Without the pinned Node it is reported as skipped, with the reason. `audit.js` refuses any other Node version (exit 2): the one floating-point library call is `Math.log`, in the DFA exponent (G7).

`ctest` runs these tests as `golden.legacy_audit` (`cmake/WubansiaGolden.cmake`). The C++ port is checked against the same expected file twice: by `core_tests` on `build/golden/`, and by `golden_check`'s last step, `groovecore metrics --legacy --expect-fail --check core/tests/legacy_expected.json`.

The pin is a safety margin, not a hair trigger. Measured on Linux x86_64 on 2026-11-04:
- moving every `Math.log` result by 1 to 256 ulps either way, or by a relative 1e-6, changes no byte of the result;
- with the pin lifted, Node 20.20.0 (V8) and Bun 1.4.4 (JavaScriptCore) give the same bytes as Node 22.22.0;
- `--jitless`, `--max-opt=0` and `--always-turbofan`, another time zone (Pacific/Chatham), the Turkish locale and another working directory give the same bytes too.

The fixture tests don't depend on the Node version (they pass on Node 20.20.0 as well); only the baseline test needs the pinned Node.

Before measuring, the audit checks every file against `content/golden/legacy-v21.sha256`, so the result always describes the committed baseline. The result records the Node version, the manifest's SHA-256 (`manifest_sha256`) and core.js's SHA-256 (from the manifest header).

## Files

| File | What it is |
|---|---|
| `audit.js` | The command line: checks the baseline, runs every metric on the legacy file sets, writes the JSON. |
| `smf.js` | The SMF reader. It follows `core/midi/reader.cpp` rule for rule. |
| `model.js` | From a file to sections, lanes and drum groups. |
| `metrics.js` | Every metric. The definitions below are normative; the code follows them. |
| `legacy_sections.json`, `section_table.js` | Each legacy marker name and its section kind, derived from core.js (the files carry only the name). |
| `fixtures/make_fixtures.js` | Writes the fixture `.mid` files and `fixtures/expected.json`, the answers worked out by hand in its comments. |
| `fixtures/*.mid`, `fixtures/expected.json` | The fixtures and their answers. They are integers and strings only, so the C++ tests can use them too. |
| `test/audit.test.js` | The tests. |

## Reading the files

The audit reads files exactly as `core/midi/reader.cpp` does:

- **Header.** Formats 0 and 1 only, with a ticks-per-quarter division. A format 0 file must have exactly one track. The audit then also requires 480 ticks per quarter (both generators write 480).
- **Chunks.** `MTrk` chunks are read in order. Any other chunk is skipped. Bytes after the last track are ignored.
- **Events.** Delta times and lengths are variable-length numbers of at most 4 bytes. Running status is kept after meta and sysex events too. A track ends at its end-of-track event (FF 2F); a track without one is an error. Status bytes F1–FE, other than FF, F0 and F7, are errors.
- **Notes.** A note-on with velocity 0 is a note-off. Notes pair first in, first out per channel and key: a note-off ends the oldest sounding note on its channel and key. A note-off with nothing to end is ignored. A note still sounding at the end of its track ends on that track's end tick.
- **Note order.** Notes sort by (tick, channel, key), stably, so notes that tie keep the order of their note-ons.
- **Track name.** The text of the track's first FF 03 event.
- **Markers.** Every FF 06 event in any track, sorted by tick; on one tick, track order, then file order.
- **Key signatures.** FF 59 events with a 2-byte payload; the first byte is signed.

## The model

### Time
- 480 ticks per quarter note, 4/4 throughout: a 16th is 120 ticks, a beat 480, a bar 1920.
- A file's end tick is the largest end-of-track tick among its tracks. Its length in bars is `ceil(end / 1920)`.
- **Exact positions.** A note belongs to bar `floor(tick / 1920)`, at position `tick mod 1920`.
- **Step attribution.** Where a metric says so, a note belongs to the nearest 16th: step `s = floor((tick + 60) / 120)`, so a tie (60 ticks) goes to the later step. It then sits in bar `floor(s / 16)` at step `s mod 16`. This keeps a note that a feel pass pulls up to 59 ticks early in its own bar and step. Swing delays below 60 ticks keep odd 16ths on their own step.

### Sections
- Each marker starts a section, which runs to the next marker's tick or to the file's end. A section that would be empty (two markers on one tick) is dropped.
- A section's first bar is `floor(tick / 1920)` and its length `ceil((end − tick) / 1920)` bars.
- A file without markers is one section of kind `none` over the whole file. This applies to the legacy part files and FullArrangement.
- **Kind, generator v2.** If the marker text's first word (up to the first space) is one of `intro`, `groove`, `build`, `drop`, `drop2`, `break`, `outro`, in lower case, that is the kind. For example: `drop2`, or `drop2 four to the floor`.
- **Kind, legacy.** Otherwise the whole text is looked up in `legacy_sections.json`. It lists the 105 names the legacy full songs and extended mixes use, with core.js's own kind for each; core.js's `breakdown` is called `break` here. No legacy name starts with one of those lower-case words (they start with a capital or a digit, like "4x4 switch"), so the two rules never meet.
- Anything else is `unknown`.

### Lanes and roles
- A track whose name's first word is "Sidechain" is ignored, on any channel: the legacy ghost-kick trigger track. Any other track with notes, all of them on channel 10 (index 9), is a drum track.
- Any other track's role comes from the first word of its name, lower-cased (letters only):

  | Role | First words |
  |---|---|
  | `bass` | bass, sub, reese, acid, donk |
  | `chords` | chords, stabs, stab, pad, organ, strings, piano, brass, orchestra, choir |
  | `lead` | melody, lead, pluck, arp, counter, talkbox, hoover |
  | `vox` | vocal, vox, chops, shouts |
  | `fx` | riser, fx, impact, impacts, dub |
  | `other` | anything else, for example the legacy "Break Slices" |

- **Legacy part files.** These are type 0, with one track named after the kit, for example `UKG01/UKG01_Bass-A.mid`. They take their role from the file's slot instead: Drums-* is drums (by channel), Bass-* bass, Chords-* chords, Melody-* lead.
- **Drum groups.** Drum notes (all notes of drum tracks) get a group from their key alone:

  | Group | GM keys |
  |---|---|
  | kick | 35, 36 |
  | snare | 38, 40 |
  | clap | 39 |
  | rim | 37 |
  | hats | 42, 44, 46 (closed, pedal, open); 51, 53, 59 (rides); 54, 69, 70, 82 (tambourine, cabasa, maracas, shaker) |
  | cymbal | 49, 52, 55, 57 |
  | tom | 41, 43, 45, 47, 48, 50 |
  | perc | every other key |

- **Metric groups.**
  - *hats* is the hats group.
  - *perc* is perc, tom and rim.
  - The *snare family* is snare and clap.
  - Drum notes are listed by (tick, track, key).
- **What generator v2 must write for this to work.**
  - every drum and perc lane on channel 10, on keys from this table
  - track names whose first word gives the role
  - markers that start with the kind word

### With feel or quantized
Generator v2 exports every file twice. Each metric names the version it reads:
- **Quantized:** G1, G2, G3, G4, G9, G10, R1–R3, R5–R11, I1, I4. These are about what is written, not how it is played.
- **With feel:** G5, G6, G7, G8, R4, I2, I3, I5. Velocity humanization, chord spread, microtiming, swing, articulation and the low-end lock are all feel.
- A legacy file is its own score and performance, so it is read as it is.

## Building blocks

- **Rounding.**
  - `permille(a, b)` is `floor((2000a + b) / (2b))`, for `a ≥ 0` and `b > 0`: a / b in thousandths, half up.
  - Signed ratios, such as lag-1 autocorrelation, round half up toward +∞ on exact integers.
  - Gates compare exact integers, for example `10·top ≤ measured`. The rounded permille is only for reading.
- **Lower median.** The element at index `floor((n − 1) / 2)` once sorted. Medians are always of integers.
- **SD in thousandths (`sd_milli`).** For integers x₁…xₙ, with S1 = Σx and S2 = Σx²: `floor(1000 · sqrt(n·S2 − S1²) / n + 0.5)`. Compute it in IEEE doubles in exactly that order. `n·S2 − S1²` is an exact integer below 2⁵³, sqrt is correctly rounded, and floating-point contraction is off in core (`wubansia_build_defaults`), so the result is exact.
- **Stacks.**
  - Take one track's notes, or several merged, sorted by onset.
  - A stack is a run of notes whose onsets are each at most 10 ticks after the run's first onset. The next note starts a new stack.
  - A stack's *spread* is its last onset minus its first.
- **Harmony stacks.**
  - Stacks of all chords-role notes merged, sorted by (tick, track, key).
  - Only stacks with at least 2 distinct pitch classes count. Single notes, such as a stab riff or an arp, never define harmony.
  - Each harmony stack has a pitch-class mask (bit `key mod 12`) and a lowest key.
- **Harmony slots** (half bars) from a tick w₀. Slot j starts at `w₀ + 960j`.
  - Its harmony is the harmony stack whose onset is nearest the slot's start, among those with onset in `[start − 250, start + 710)`. On a tie, the later stack wins.
  - The window lets a chord pushed early by up to an 8th (plus 10 ticks of feel) belong to the slot it pushes into. The tie rule gives offbeat stabs (step 2) to their own bar rather than the previous bar's step-14 stab.
  - A slot with no stack carries the previous slot's harmony. Slots before the first filled slot take that slot's harmony. With no stack at all, the window isn't measurable.
- **Bar fingerprints.**
  - *Exact*: the notes with that bar's `floor(tick / 1920)`.
  - *Step*: the set of `step·128 + key` over notes attributed to the bar, written as sorted numbers. It has no velocity.
- **Swing delay** of a set of notes.
  - Its *odd-16th offsets* are `(tick mod 240) − 120` for every onset with `tick mod 240` in `[60, 180)`.
  - The delay is their lower median, when there are at least the stated number of them.
- **Distinct swings.** Sort the delays. A cluster takes every delay within 2 ticks of its smallest, then the next cluster starts; count the clusters.
- **Residual** of an onset t, given a delay d:
  - Let g = 240·floor(t/240). Of `t − g`, `t − (g + 120 + d)` and `t − (g + 240)`, take the smallest in absolute value. A tie keeps the earlier of the three.
  - Onsets more than 30 ticks from every grid point are left out: they belong to another subdivision.
- **Histograms** are written as lists sorted by key, like `[{"value": 7, "count": 405}]`. Keys such as "8" would otherwise be reordered by JavaScript's integer-key rule.
- **The most common value** (a top shape, form or fill position) is the one with the highest count; on a tie, the smallest key compared as a string in byte order.

## Gating metrics

Each gate is computed across a set of seeds (for legacy, kits stand in for seeds), with the band from the plan. "Legacy" below is the audit's value on the file set that matches the plan's "Legacy (audit)" column, then the plan's own figure. Where they differ, the reason follows. Legacy fails all ten gates on those sets, but G4 fails only through the lead.

The bands of G3, G5, G6 and G7 are pure functions of each gate's aggregate, `bands` in `metrics.js` and `bands::` in `core/metrics`. `fixtures/expected.json` ("bands") pins every bound at its edge: the bound itself passes and one unit past it fails, with the band's other parts well inside. `test/audit.test.js` and `core/tests/test_metrics_fixtures.cpp` check both copies against those answers.

### G1 Progression-shape diversity
- **Measured on.**
  - The *break window*: the first 8 bars (16 harmony slots) of the first section of kind `break`.
  - The *drop window*: the same for the first `drop`.
- **Shape.** Each slot's pitch-class mask, rotated so that the lowest key of slot 0's stack is pitch class 0. It is written as 16 three-digit hex masks, for example `895.895.895.895.a44…`. Rotating makes the shape independent of key and transposition.
- **Gate**, for each window: every seed measurable, `2·distinct ≥ seeds`, and `10·top ≤ seeds`. G1 passes if both windows pass.
- **Legacy (FullSong, 43 kits)**, against the plan's "12 of 43 kits share one breakdown shape (28%)":
  - break: 12 of 43 share the top shape (279‰), exactly the plan's figure. Only 21 shapes are distinct (488‰), so it fails twice.
  - drop: 38 distinct, top 3 (70‰), which passes.
  - G1 fails on the break.
  - The 12 kits include UKG05, which is in a major key. Relative to the first chord, its bVI–bVII–v–i is the same shape. Relative to the tonic it would not be, which is why the planning audit's tonic-relative count was 11.
- **ExtendedMix:** break top 10 (233‰), 23 distinct.

### G2 Harmonic rhythm
- **Measured on** the first 4 bars (8 slots) of the drop window. A file without markers uses its first 4 bars.
- **Pattern.** The run lengths of equal consecutive slot masks, in half bars, for example `2,2,2,2` or `4,4` or `3,1,2,2`. "4 × 1-bar slots" is `2,2,2,2`.
- **Gate:** every seed measurable, `10·count(2,2,2,2) ≤ 3·seeds`, and at least 4 distinct patterns.
- **Legacy (FullSong).** 26 of 43 are `2,2,2,2` (605‰), against a ceiling of 30%, so G2 fails; there are 12 patterns. Chords-A gives the same 26.
  - The plan's figure is "43 of 43". No planning script computes it. It most likely counted every kit's 4-bar chord part as four one-bar slots, which is how core.js's chord data is laid out, even where neighbouring bars hold the same chord.
  - The MIDI shows chord *changes*: 32 of 43 change only on bar lines (`changes_on_bar_lines_only`), and 26 change on every bar line.
  - audit.js counted 22 at whole-bar resolution. It took the first chord in `[bar − 250, next bar − 250)`, which gives a bar the previous bar's step-14 stab, and it compared chord names. The same first-chord rule on pitch-class sets gives 21: D6 and Bm7, for example, are one set under two names. The nearest-stack rule above gives the 26.

### G3 One-bar repetition in 4-bar windows
- **Windows.** Every 4-bar window aligned to its section's start (bars `[b₀ + 4k, b₀ + 4k + 4)`), wholly inside the section and the file. A legacy part file is one window.
- **Drum lanes.** One per (drum track, key). Each bar's content is the multiset of (exact position, velocity).
- **Bass lanes.** One per bass track. Each bar's content is its set of exact onset positions, rhythm only.
- **Counting.**
  - A lane counts in a window only if it has a note in it.
  - It is *repeated* if all four bars are identical and non-empty.
  - A kick-group lane whose four bars each have an onset within 2 ticks of all four beats (0, 480, 960, 1440) is a four-on-the-floor window: exempt, counted apart, and in neither total.
- **Gate:** drum windows and bass windows both exist, `10·drum repeated ≤ 4·drum windows`, and `10·bass repeated ≤ 3·bass windows`.
- **Legacy (Drums-Main, Drums-Variation, Bass-A, Bass-B), against the plan's 82% / 70%:**
  - drums 426 of 508 (839‰), with 70 four-on-the-floor kick windows exempt.
  - Without the exemption it is 474 of 578 = 820‰, exactly the plan's 82%.
  - bass 60 of 86 (698‰), exactly the plan's 70%.
- **FullSong:** drums 385‰ (488‰ with velocity ignored: section velocity ramps account for part of it, fills and lanes entering mid-window for the rest), bass 615‰.

### G4 Macro changes on phrase-start or phrase-end bars
- **Lanes:** each bass, chords and lead track. Only sections of 9 bars or more count (a file without markers is one section), and comparisons never cross a section boundary.
- **Changes.** With step fingerprints f(i) for section bars i:
  - Bar i ≥ 8 *differs* if f(i) ≠ f(i − 8), the same bar one 8-bar phrase earlier.
  - A *macro change* is a differing bar whose previous bar did not differ (bar 7 never differs). A run of differing bars is one change, at its first bar.
  - It is on a *phrase bar* if i mod 8 is 0 (phrase start) or 7 (phrase end).
- **Gate:** for each of bass, chords and lead, changes exist and `2·on phrase ≥ changes` (≥ 50%; random placement gives about 25%).
- **Legacy (ExtendedMix): bass 291 of 318 (915‰), chords 412 of 438 (941‰), lead 272 of 787 (346‰).** G4 fails, but only through the lead.
  - FullSong: 881‰, 922‰ and 1000‰, which **passes**.
- **Why this differs from the plan's 30% / 29% / 33%.** Those came from `analyze4.js`, which compared each bar with the bar 4 before and counted differing bars at positions 3 and 7. That measure:
  - turns one new pattern at a phrase start into four differing bars (positions 0–3), three of them off-phrase;
  - counts every bar of an 8-bar loop as differing.
- **The fixed definition** compares with the bar one phrase earlier, and counts a run once. It finds that the legacy arrangement does change on phrase lines: every switch lands on an 8-bar line. That clockwork is the legacy tell, and G9 and G10 catch it. G4 is one-sided, so clockwork passes it.
- **Not decided here.** The plan says every band is two-sided but gives G4 only a floor. The S1b2 ADR on gating metrics should decide whether G4 gets a ceiling. For example, ≤ 90% would fail the FullSong's chords (922‰) and lead (1000‰), though not its bass (881‰). Until then, `golden_check`'s "legacy fails" assertion must use each gate's `legacy_fails` flag, not assume every gate fails on every set.

### G5 Velocity resolution per 8 bars
- **Windows.** 8-bar windows aligned to section starts, wholly inside the section. Builds are skipped: their rolls ramp velocity on purpose. Notes count by exact onset in `[t₀, t₀ + 8·1920)`.
- **Groups, with the fewest notes a window needs:**
  - hats (16)
  - perc (16)
  - ghosts (10): snare-family notes with `tick mod 1920` outside the backbeat windows [450, 510] and [1410, 1470]
  - bass, all bass tracks (8)
  - lead, all lead tracks (8)
- **Per window:** the number of distinct velocities and `sd_milli`. The group's value is the lower median over all windows of all seeds, pooled.
- **Gate:**
  - hats, perc and ghosts: median distinct ≥ 10 and 6000 ≤ median SD ≤ 22000
  - bass and lead: median distinct ≥ 6 and median SD ≥ 8000
- **Legacy (FullSong): median distinct values per window: hats 4, perc 2, ghosts 6, bass 1, lead 1.** It fails.
  - The plan's figure is about the parts' levels, and `G5.part_levels` measures exactly that. Across all 387 part files the drums use the velDigit values only: hats 7, snare family 8, kick 6, with at most 8 in one file. That matches "≤ 9 levels (velDigit)". Bass uses 64, 100 and 118, and lead 64, 92 and 118: the plan's "3 per melodic lane".
- **ExtendedMix:** its random velocity jitter gives hats 44 and perc 23 values (passing), but ghosts' SD is 24.8 (over 22), bass has 5 values and lead 4. It fails.

### G6 Chord stacks with two or more velocities
- **Stacks:** per chords-role track, every stack of 2 or more notes.
- **Gate:** `10·(stacks with ≥ 2 distinct velocities) ≥ 7·stacks`, and every stack's spread ≤ 4 ticks (the plan's "pad spread 0–4 ticks", applied to every chord stack).
- **Legacy (Chords-A and Chords-B): 0 of 1,018**, exactly the plan's figure; spread 0 throughout.
  - FullSong 0 of 11,137.
  - ExtendedMix 365‰: its velocity jitter, still under 70%.

### G7 Microtiming on feel lanes
- **Lanes:** hats and perc (drum notes).
- **Residuals.** Per section, the lane's delay d is the lower median of its odd-16th offsets in that section (at least 1; 0 if none). Each onset's residual is taken against d, in onset order across sections.
- **Per file and lane:**
  - n
  - `sd_milli`
  - lag-1 autocorrelation in thousandths: exact, as `[n²P − n·S1·(A + B) + (n − 1)·S1²] / [n·(n·S2 − S1²)]`, where P = Σ xᵢxᵢ₋₁, A = Σᵢ₌₁ⁿ⁻¹ xᵢ and B = Σᵢ₌₀ⁿ⁻² xᵢ
  - DFA-1 α in thousandths (below)
  - Lag-1 and α need n ≥ 64; otherwise they are −9999. A constant series has neither.
- **DFA-1, in doubles, in this order:**
  1. m = Σx / n.
  2. The profile yₖ = yₖ₋₁ + (xₖ − m), summed in order from 0.
  3. Box sizes s = 4, 8, 16, … while s ≤ floor(n/4). Each takes floor(n/s) boxes from the start; the rest of the series is dropped.
  4. In each box, with k̄ = (s − 1)/2: ȳ = Σy/s, slope b = Σ(k − k̄)(y − ȳ) / Σ(k − k̄)², and RSS = Σ(y − ȳ − b(k − k̄))².
  5. F²(s) = (Σ over boxes of RSS/s) / boxes.
  6. α is the least-squares slope of ½·ln F²(s) against ln s, over at least 3 sizes. It is undefined if any F² is 0.
  7. α milli = floor(1000α + 0.5).
- **Aggregate:** per lane, over files with n ≥ 64, the lower medians of SD, lag-1 and α. A sentinel never enters a median.
- **Gate:** for hats and perc, 1500 ≤ median SD ≤ 6000, median lag-1 ≥ 200, and 650 ≤ median α ≤ 1100.
- **Legacy (ExtendedMix): hats SD 2.376 ticks, lag-1 −0.029, α 0.524; perc SD 2.517, lag-1 −0.011, α 0.534.** That is white noise, as the plan says ("lag-1 ≈ 0"). The SD sits inside the band; lag-1 and α fail.
  - FullSong has no microtiming: SD 0, with lag-1 and α undefined.

### G8 Per-lane swing
- **Measured on** the first `drop` section.
- **Lanes.**
  - The snare family, hats and perc, as drum notes.
  - Each bass, chords, lead and vox track.
  - Kick is the anchor and is left out.
- **Delay.** A lane's delay is the lower median of its odd-16th offsets in the section, if it has at least 16 of them; otherwise it is unmeasured.
- **Value:** the number of distinct swings among the measured lanes. The per-file value lists every lane's delay.
- **Gate:** every seed has a drop, and every seed has at least 3 distinct swings.
- **Legacy (FullSong): 1 distinct swing in all 43 kits**, the plan's "one value per kit". Every measured lane shows exactly core.js's own delay, `round(240·swing) − 120`.
  - ExtendedMix: 1 in 40 kits and 2 in 3, where a layer plays unswung runs: CH07's strings (on 32nds, −60), and CH08's and GH02's piano runs (straight, 0). It fails.

### G9 Fills
- **Fill bars**, per section of 4 bars or more, against the section's own profile.
  - For each bar, take the set of (group, step) of its snare, clap, rim, tom and perc notes (step attribution), and the set of its kick steps.
  - A (group, step) is *rare* if it occurs in at most an eighth of the section's bars (`8·count ≤ bars`).
  - A kick step is *common* if it occurs in at least three quarters of them (`4·count ≥ 3·bars`).
  - A bar is a fill if at least 2 common kick steps are missing (a dropout counts as a fill), or if rare steps plus missing kick steps reach 3. This catches rolls, tom and conga fills, and kick dropouts.
  - Hats never count: their micro variation is per bar by design.
- **Boundary gestures.** At each section start s after the first, any one of these counts:
  - the previous section's last bar is a fill
  - an fx-role note starts in `[s − 3840, s + 480)` (risers, impacts)
  - a cymbal-group onset falls within 60 ticks of s
  - a gap: no drum onset in `[s − 240, s)`, though there is one in `[s − 1920, s − 240)`
- **Positions.** For fill bars in non-build sections (a build is one long gesture), the position is the section bar index mod 8.
- **Distinctness.** A fill's content is the sorted set of `step·128 + key` of its non-hats drum notes. A song's distinct share is distinct contents / fill bars, for songs with at least 2 fills.
- **Gate:** every boundary has a gesture; `10·(count at the most common position) ≤ 6·fills`; and the lower median of the songs' distinct shares ≥ 700‰.
- **Legacy (FullSong):**
  - gestures at 344 of 352 boundaries
  - fill positions: 399 of 403 at position 7 (990‰), against the plan's "100% on bar 8 of 8"; the planning audit found 412 by matching core.js's fill pattern, which the MIDI can't
  - median distinct 111‰ (the same fill bar again and again)
  - It fails all three parts.
- **Known limit, for the S1b2 gating ADR:** a perc key that sounds in most of a section's bars still makes "rare" steps when it is a moving cycle: FUT01's 3/16 rim polymeter lands on a different step of the bar each time, so most of its G9 fills are the rim's (in round 2's review, seeds 11 to 30 had 457 fills, and 63 without the rim; ADR 0013 §5). A rule that a key sounding in most of a section's bars can't make a fill would fix that; it changes the legacy values of G9, so it waits for that ADR.
- **Known miss:** GV02 and MTH08 fill by swapping bongos for congas on the same steps. In one group that isn't new material. Identity by key would catch it, but would flag every pitched-perc phrase in generator v2 (blips on 8 keys) as a fill.
- **ExtendedMix:** 401 of 411, position 7 at 816‰, median distinct 375‰.

### G10 Form diversity
- **Form.** Each section's kind and length in bars, for example `intro16 build8 drop16 …`.
- **Salient changes.** The section starts, bar 0, and every G4 macro change (bass, chords and lead) as an absolute bar. The intervals are the differences between consecutive sorted bars.
- **Gate:** every seed has markers, at least 8 distinct forms, `4·top ≤ seeds`, and every seed's intervals take at least 2 values.
- **Legacy (FullSong): 7 forms for 43 kits** (the plan's figure; top 8, 186‰). It fails.
  - ExtendedMix: 16 forms (its 19 custom forms), top 6, intervals fine. That set passes G10.
  - Legacy forms come one per kit family, never from a seed. 43 kits across 7 families stand in for 64 seeds of one style. The plan's comparison is the FullSong.

## Reported metrics (not gating)

Reported metrics are computed on the legacy sets named in the result, and none of them gate.

- **R1 Lag-4 repetition.**
  - Lanes: the kick group, and each bass, chords and lead track.
  - Inside sections, every bar i ≥ 4 where bars i and i − 4 are both non-empty counts once, and once more as equal if their step fingerprints are equal.
  - Band: kick 0.70–0.95, chords 0.50–0.85, bass 0.40–0.80, lead 0.30–0.70.
  - Legacy ExtendedMix: kick 861, bass 571, chords 547, lead 421‰. FullSong: 870, 953, 960, 785‰.
- **R2 Loop tiling.** Per lane, the longest run of consecutive identical, non-empty 4-bar blocks (blocks from bar 0, step fingerprints), in bars. Per file, the longest of each role; reported as the median and maximum over files.
  - FullSong: bass median 32, maximum 48 bars.
- **R3 Hat accent period.**
  - Closed hats only (keys 42 and 44), per bar with at least 4 onsets (step attribution).
  - The bar's 16-step velocity string has, per step, the highest velocity there or 0 for a rest.
  - Its period is the smallest p in {1, 2, 4, 8} with v[s] = v[s − p] for all s ≥ p, else 16.
  - Band: period ≤ 4 in ≤ 40% of bars.
  - **Legacy (Drums-Main and Drums-Variation): 260 of 300 bars (867‰).** See below for the 76.5% / 81% disagreement.
  - The ExtendedMix's velocity jitter breaks every period (0‰); this measure reads the score.
- **R4 Gate rule.**
  - A melodic note (bass, chords, lead) follows the legacy gate rule if its end tick e = on + length satisfies one of:
    - e + 14 is on the swung 16th grid (a legato note: e − 30 is), exactly, or
    - e is the file's end.
  - The grid uses the track's delay if it has 16 odd-16th onsets, otherwise the whole file's delay, which is the lower median over every note in the file (0 if none).
  - Band ≤ 50%.
  - Legacy FullArrangement: 7,313 of 7,313 (1000‰), the planning audit's 100%.
- **R5 Chord-tone profile.**
  - The harmony at tick t is the latest harmony stack with onset ≤ t; before the first one, the last one (a loop).
  - Reported: the share of lead and bass onsets in that mask, on strong steps (step mod 4 = 0) and weak steps.
  - Legacy FullArrangement lead: 400 of 520 strong (769‰), 659 of 834 weak (790‰), exactly analyze.js's 0.77 and 0.79 on the same 520 and 834 notes.
- **R6 Breath and complementarity.**
  - *Breath:* per 4-bar window in which the lead has an onset, the share of its 64 sixteenths at whose start no lead note sounds. Reported: the median, in thousandths.
  - *Complementarity:* over half bars where the lead or the vox has an onset, the share where only one does.
  - ExtendedMix: median rest 453‰, one-only 652‰. Legacy lead parts: median rest 625‰.
- **R7 Lead shape.**
  - Step ratio: over consecutive onsets of each lead track (the top note of each stack), the share of intervals of 1 or 2 semitones (unisons count in the total).
  - Bar 1 = bar 3: per 4-bar window, whether bars 0 and 2 have the same set of onset steps, when both have notes.
  - Band: RIFF step ratio 0.2–0.6 and bar 1 = bar 3 0.6–0.9; SENTENCE 0.5–0.8 and ≤ 0.6. Legacy has no templates, so this reports all lead parts.
  - Legacy Melody-A/B: step ratio 372 of 1,268 (293‰; audit.js 29.34%), **bar 1 = bar 3 84 of 86 (977‰), exactly the plan's 97.7%**.
- **R8 Voicing texture.**
  - Each stack of 2 or more notes on a chords track has a type:
    - *quartal* if it has 3 or more distinct keys, each 5 semitones above the one below
    - otherwise by its distinct pitch classes: *octave* (1), *dyad* (2), *triad* (3), *four_plus* (4 or more)
  - Reported: the median number of types per song; four_plus as a share of stacks.
  - *Recurring* stacks are those whose mask occurred earlier in the song. Re-voiced: those whose sorted keys differ from the most recent stack with that mask.
  - Band: ≥ 3 types per song, four_plus ≤ 60%, ≥ 25% of recurring chords re-voiced.
  - Legacy FullSong: 1 type per song, four_plus 973‰, re-voiced 11‰.
- **R9 Odd cycles.**
  - Per drum lane (track, key) and 8-bar window, take the 128-step onset sequence (step attribution) and its smallest period p ≤ 64 (seq[i] = seq[i − p] for all i ≥ p).
  - The window has an odd cycle if p exists, doesn't divide 16, and isn't a multiple of 16 (3, 5, 6, 7, 24…).
  - ExtendedMix: 95 of 11,045 lane windows, from the one kit with 3/5/7-step poly percussion.
- **R10 Cross-seed duplicate 2-bar motifs.**
  - Per file, take the 2-bar windows of a role at even section bars (step attribution). Each is the set of (step from the window start, key minus the key of the window's first note, by onset, then track, then key): transposition-invariant.
  - A window is duplicated if another file has the same one.
  - Band ≤ 3%.
  - Legacy lead parts: 2 of 172 (12‰); bass parts 0 of 172.
- **R11 Syncopation (Longuet-Higgins and Lee).**
  - Weights per 16th: 0 −4 −3 −4 −2 −4 −3 −4 −1 −4 −3 −4 −2 −4 −3 −4. The bar is cyclic.
  - For each onset step i, the next onset j is the next in the bar, or the first plus 16. If the strongest rest between them (weights mod 16) is stronger than i, add the difference.
  - Reported per lane group (kick, snare family, hats, and the bass, chords and lead roles): the mean in thousandths, rounded half up (`floor((2000·sum + n) / (2n))`), and `sd_milli`, over bars with onsets. The plan's style band per energy level belongs to the style JSON, not here.

## Invariants measured on legacy

- **I1 In key.** The share of non-drum notes whose pitch class is in the natural minor or major scale of the key signature in effect. The tonic is (9 + 7·sharps) mod 12 for minor, 7·sharps mod 12 for major.
  - Legacy FullSong: 985–1000‰ per role. The legacy extended mixes carry key changes, which are respected.
- **I2 Risers and rolls on the swung grid.**
  - Covers fx-role onsets, and snare-family onsets in build sections.
  - Each is *swung* if within 2 ticks of the swung grid (the file delay), else *straight only* if within 2 ticks of a straight 16th.
  - **Legacy FullSong risers: 3,969 of 7,938 straight only (500‰), the plan's "50% of riser notes straight".** Build rolls: 313‰.
- **I3 Kick and bass.** For bass onsets with a kick onset within 30 ticks: the share whose nearest kick is within 2 ticks.
  - Legacy: 5,905 of 5,905 (FullSong).
- **I4 Sections on 8-bar lines.** The share of markers at a tick that is a multiple of 15,360.
  - Legacy 100%.
- **I5 Kick anchors.** For kick onsets within 30 ticks of a beat: the share within 2 ticks.
  - Legacy 100%.
- **Not measurable on legacy data:**
  - same seed, same bytes
  - the 50 ms budget
  - "changing one lane leaves the others alone"
  - the hook's placement (it needs S1b2's hook)

## Where the two planning audits disagreed, and the one definition fixed

The planning-time audits (`audit.js`, `audit2.js`, and the `midi-gen/analyze*.js` scripts the plan also quotes) worked on core.js's pattern data, not on MIDI. Every definition here reads the MIDI.

- **Hat accent period: 76.45% or 80.85%.** Both figures reproduce exactly from core.js's pattern strings.
  - audit.js: 211 of 276 strings, over five lanes (closed hat, shaker, open hat, ride, tambourine), periods {1, 2, 4, 8}.
  - analyze2.js: 76 of 94 closed-hat strings, periods {2, 4, 8}.
  - The difference is the lane set. **Fixed: closed hat only, per bar from the MIDI (867‰ on the drum parts).**
    - An offbeat open hat, a straight shaker or a ride has a short period by its rhythm alone, with no accents at all, so counting them measures rhythm, not accenting. The closed hat is where an accent pattern is a choice.
    - Counting bars instead of pattern strings weights a pattern by how long it plays. The five-lane family per bar would read 941‰.
- **G4: 30% / 29% / 33%, or about 90%.** See G4: the bar-minus-4 comparison was an artifact. Fixed: the bar one phrase earlier, one count per run.
- **G2: 43 of 43, or 22 of 43.** See G2. Fixed: the nearest-stack slot rule at half-bar resolution: 26 of 43.
- **G3 drums: 82%.** Reproduced exactly (474 of 578) when four-on-the-floor kick windows are counted. The plan's band exempts them: 83.9%.
- **G9: 100% on bar 8 of 8.** The MIDI can't see core.js's fill pattern, so the profile rule finds 399 of 403 fills on bar 8 (990‰).

## Tolerances for the C++ port

| Values | Tolerance |
|---|---|
| Every count, pattern, shape, form, lane list and pass flag | exact |
| Every permille built from counts, every lower median | exact |
| `sd_milli` (G5, G7, R11), computed as specified | exact |
| `lag1_pm` (G7), from the integer formula | exact |
| `alpha_milli` (G7): doubles in the stated order; only `log` can differ by an ulp | ±1 |
| R11 means (integer arithmetic) | exact |

## `core/tests/legacy_expected.json`

- **Contents:**
  - integers and strings only, so core's integer-only JSON reader can read it: no floats, booleans or nulls
  - −1 means "no value"; −9999 marks lag-1 or α that is undefined
  - the header (Node version, manifest SHA-256, core.js SHA-256, file and note counts, the size of each file set)
  - `section_kinds`, the legacy table, so the C++ test needs nothing from this folder
  - `gating`: per gate the band, the primary set, `legacy_fails`, and the full result for each file set, with per-file values
  - `reported` and `invariants`, as aggregates
- **The file sets:**
  - `parts` is Drums-Main, Drums-Variation, Bass-A and Bass-B
  - `chord_parts` is Chords-A and Chords-B
  - `drum_parts`, `bass_parts` and `lead_parts` are what they say
  - `all_parts` is all nine part slots
  - FullArrangement, FullSong, ExtendedMix and Chords-A are those slots alone

**Determinism.** The audit sorts files by path before measuring. Two runs, and a run with the files in reverse order, give byte-identical results; the baseline test checks both.
