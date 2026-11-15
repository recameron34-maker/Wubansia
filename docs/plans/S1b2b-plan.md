# S1b2b Hook and motifs: plan

Step 2 of `docs/plans/S1b2-plan.md`. Gates: G4 and G5 for lead lanes, R10 (lead at most 3%). Four PRs, each on a branch
from main, the first after S1b2a merges.

This plan takes the winning design, engineering-first (both judges picked it), grafts what the judges named from
music-first and reference-first, and corrects every error they found. This revision also answers the completeness
critique (Review notes, at the end). Figures marked "prototype" come from proto3: the plan's prototype (proto2) with
every change below built in. It links the built core at pr6-int fd2c5b4 and runs `generate()`, `renderFiles()` and
`scoreSeeds()` on seeds 0-63 per kit; the planner rules and leaps are also counted on seeds 0-1023 and the snapshot
seeds (42, 977, 65536, 123456789, 2^64−1). Without the hook it reproduces today's pinned verdicts and ADR 0023's
drop-lift figures exactly. Its lead feel is a stand-in (the kit's swing plus 1/f timing and velocity), not
`perform()`, and it skips the lane grid's 920 velocity scaling.

The figures marked "prototype" come from a prototype built on the generator at fd2c5b4, which isn't kept in the repo.
The tests re-measure them.

## Goal and scope

**Goal.** One hook per seed that a producer could have written, placed the way the kits' written extended mixes place
theirs. The lead gates pass because the material is written that way: changes on 8-bar lines, a fixed accent shape, a
different hook in every seed. The brief: "Really nail the coolness", "midis that aren't written like AI", "I hate
muddy", DJ-friendly extended mixes.

**In scope**
- PR 1: the generator's MIDI channels per MIDI v2, alone.
- PR 2, the hook: a 2-bar RIFF per seed, drawn rhythm first and pitched key-relative; drop 2's new phrase in each kit's
  written idiom (FUT01's and FUT03's 4-bar organ period, FUT02's glass period, FUT04's acid-stab riff); one carrier
  lane per kit (new lead lanes 'Lead Pluck', 'Lead Keys' and 'Lead Blips'; FUT02's arp carries the hook as the written
  ChromeArp); the schedule over ADR 0023's drawn forms; the development operators; call and response inside the lead
  and through a half-bar mask.
- PR 3, drop 2's own material: each kit's drop-2 bass line from its written mix (FUT01's warp riff in the bars its
  kick rests), and FUT01's bass talk (CC74).
- PR 4, after ADR 0016: a vox reply lane.

**Out of scope**
- Fills, transitions, throws and accent synths: S1b2c. So are drop 2's busier hats (the written 8-bar counts rise
  93→124, 120→144 and 133→175 in FUT01, FUT03 and FUT04), FUT04's blip ratchets (S1b2c's ratchet operator) and the
  other lanes' counter-motifs from the hook's rhythm (plan §2.3); the vox reply (PR 4) is the first counter-motif.
- Metric definitions, `core/tests/legacy_expected.json`, the G4 unit for leads, the reported bands: S1b2d's gating ADR.
  The written bar-by-bar call and answer waits for that unit.
- Plan §2.3's intro fragment, SENTENCE, re-voicing the chords around the hook, FUT01's growl bar and b2 slide (after
  the S1d listen), FUT03's style retune, FUT03's written drop-2 keys arpeggio (below).

**What the grafts change in the winner**
- FUT02: the written ChromeArp. A key-fixed D5/A4 pedal under the hook's top voice; SKELETON is the top voice alone.
  It replaces engineering-first's chord-ladder cells.
- FUT01: no 'Reese Talk' lane. The bass lane gets the written 8th-note CC74 talk and the written warp riff for drop 2's
  kick-out bars (PR 3).
- Channels: the voiced chord stacks stay on channel 3.
- Scheduling: menus weighted by the written programs, OUT lines included. planHook clears only the carrier's own block
  bits, after planChanges. The groove's last line plants the hook, so the FUT01, FUT03 and FUT04 builds stay
  hook-free, as written.
- Pitch: the push-robust chord context, in the fit table and in the realization.
- Drop 2 always opens on its new phrase.
- Rhythm: bar 2 keeps bar 1's onsets 0-7 and edits at most one of 8-15.
- FUT04: the written blip cell and the written blip intervals.
- Mud: the written-reference rule, enforced by the lead's own fit and a per-section guard.
- Tests from music-first (onTheLine, the accent shape, test_rng vectors, drop 2 against drop 1) and reference-first
  (loader checks of plan §5.1 at every drawable length, byte identity without a hook block).
- The vox reply (music-first), as its own PR gated on ADR 0016.
- Two changes the prototype forced. The call keeps its bar's last onset: without it FUT01's R10 lead is 27‰ against the
  30‰ band. SKELETON keeps accents only: a bar whose one non-accent onset was the peak made SKELETON equal FULL there.

**Errors corrected** (from the earlier review round)
- (engineering-first) MIDI v2 channel 4 plays the kit's chord sample at each chord's root (`docs/midi-map.md`,
  `ChannelRole::chords`). The voiced stacks stay on channel 3, the chord voicing, where the kits already put them (JSON 2).
- (engineering-first) Its FUT02 "pedal" indexed the chord ladder: it followed the chord roots, played the 3rd in the
  break grid, spanned 62-72, and sounded 16th 15 in 24 of 24 drop bars. Replaced as below.
- (engineering-first) Its test 6 and forced FULL failed on its own prototype (FUT01 seed 4, FUT02 seed 5). Fixed by
  the response rule, the push-robust context, the fixed pedal, the texture check and the fallbacks.
- (engineering-first) "A new texture on each line, as every written drop does" was false. Dropped; REPEAT is a texture.
- (engineering-first) Its reese G3 and I3 figures were today's. The warp grid's are measured in its own commit.
- (engineering-first) The FUT04 blip cell is the written 0,3,4,6,7,10,11,12,14, not 0-2-3-6-8-10-11-14.
- (engineering-first) "Bass and chords unchanged" would not hold for a bass-role 'Reese Talk' track (G4 pools every
  bass-role track, `core/metrics/gating.cpp:540-556`). Moot: no such lane.
- (music-first, reference-first) Taking FUT02's arp out of the roster, plans or pull group, or clearing its bits before
  planChanges, moves other lanes' draws (`core/gen/arrange.cpp:375-382`). Here the arp keeps every form entry, planHook
  runs after planChanges and clears only the arp's own bits, and the arp's CC changes only inside its own ranges
  (`core/gen/cc.cpp:32-37`).
- (music-first) "Pushes repeat per 8-bar phrase" is false without `push.alike`; hence the push-robust context.
- (music-first) "Held back, paid off on the drop's downbeat" is not the written convention (Decision 4).
- (music-first) FUT02's kit range is 64-89, not 62-90. Round 1's reese complaint was that its line followed the kick and
  sounded hollow, not a long static note. AUGMENT, SEQUENCE and LIFT appear in no written hook. FUT04's rumble sits on
  JSON channel 3 (MIDI 4, the chord sample).
- (reference-first) The written leads score 9/36, 10/13, 8/23 and 6/18 on G4 per bar, and 4/4, 7/8, 6/6 and 4/4 in
  2-bar units. The written FUT04 lead fails G5 (3 distinct values, SD 10.689). The written FUT01 talk peaks at 82, 114,
  82 and 127 per 4-bar cycle. FUT01's kick rests in 28 of 64 drop 2s. ADR 0019 is Accepted; `docs/midi-map.md:6` still
  says Proposed.

## What the hook is (model and generation)

### Model (`core/gen/song.h`, integers only)
```cpp
struct HookNote   { int32 step; int32 degree; uint8 len16; uint8 velocity; bool accent; bool peak; };
struct HookPhrase { int32 bars; std::vector<HookNote> notes; };            // 2: RIFF; 4: PERIOD
enum class HookForm   : uint8 { out, full, fragment, skeleton, answer, augment };
enum class HookEnding : uint8 { none, vary, cadence, breath };
enum class HookContour: uint8 { late, early, twin };
struct HookLine   { size_t section; int32 bar; HookForm form; uint8 motif;   // motif 0: A, 1: B (the new phrase)
                    int32 shift; HookEnding ending; uint16 mask; };          // shift: SEQUENCE, scale steps
struct SongHook   { size_t lane; Mode mode; int32 base; HookContour contour; HookPhrase a, b, response;
                    int32 varyDegree; std::vector<HookLine> lines; std::vector<Note> notes; };
```
`Song::hook`. `noteFlag::hook = 1 << 10` marks every note of the hook's material; `noteFlag::pedal = 1 << 11` marks
FUT02's pedal. Bits 10-15 are free today. `mask` bit 2×bar+half: bit 0 is bar 0's first half.

### The material
- **Rhythm first.** Bar 1 of A is a style cell (onsets 0-15, lengths, accents). Per seed, at most one non-accent onset
  drops out and at most one extra comes in. Each half of the bar keeps an onset. Each bar keeps a non-accent onset,
  except in a pedal style (FUT02), whose FULL differs from SKELETON by the pedal in every bar; a pedal style's cells
  start on 16th 0, as the written arp does.
- **Bar 2 answers bar 1.** It keeps bar 1's onsets 0-7 exactly and makes at most one edit among 8-15: same, a move by a
  16th, one added, one dropped (style weights). The written phrases: 15 of 15 keep the first half, 14 of 15 make at
  most one second-half edit.
- **The register comes first.** The tonic's octave that puts the start inside the hook's register and the 3rd above
  the start nearest the style's center, once per song. The walk then draws only degrees inside the register, so no
  note is folded into it afterwards (proto2 folded each note on its own and made leaps of up to 15).
- **The contour**, drawn per seed by the style's weights: `late` (one peak, the highest note alone, on a bar-2 onset in
  16ths 18-28), `early` (in 16ths 4-12 of bar 1, as FUT03's vox riff peaks), `twin` (the top struck on two onsets in a
  row, as FUT02's glass repeats its F6 and FUT04's blips hit C6 again). After a twin top the walk redraws the notes that
  follow it from there.
- **Then the degree walk.** Key-relative degrees from the style's pool. Start and end degrees by weight. Moves weighted
  by size; a leap of a 4th or more turns back. A move never exceeds the style's `leap` in semitones, and the riff's
  wrap (its last note into its first, as it repeats) is a move like any other. When no octave of the drawn end keeps the
  wrap under the cap, the next end degree by weight is taken. Bar 2 never transposes bar 1. In FUT01, FUT03 and FUT04,
  bar 2's first half differs from bar 1's in a pitch; FUT02's written arp repeats its opening, so FUT02 allows it. Up
  to 32 attempts, then the attempt that breaks the fewest rules.
- **The redraw.** A material that still holds a move over the cap after its 32 attempts is drawn again from
  `kh.with (tag ("redraw")).with (r)`, up to 8 times. Prototype: 15, 17, 30 and 1 redraws in 1,024 seeds per kit;
  afterwards no material move is over the cap.
- **The fit table.** A strong onset takes a chord tone (1 3 5 7 9) of every chord it can sound over in the first drop's
  first line. Strong is a beat or an accent; in FUT02 a beat only (its accents group the arp; the written arp puts a G5,
  an 11th, on an accent). The context is push-robust: per loop position, the chord the progression lays out unpushed,
  plus the next chord when the onset is within 240 ticks of its change.
- **Velocity.** A fixed accent shape, drawn once: tiers for the downbeat, the other beat accents, the other accents and
  the rest; the peak raised; one offset per onset in ±shape. No per-bar draws, so bars i and i+2 of a line carry equal
  velocities.
- **varyDegree.** An open ending other than A's, by the end weights.
- **B, drop 2's new phrase**, in the style's `period.idiom`, inside its own register, every join under the cap:
  - `organ` (FUT01, FUT03): a b a' c. Bar a is A's first-half rhythm plus a second half from `period.halves`; bar b
    answers it (one second-half edit); a' is a with one note moved a pool step (the top note up where the register and
    the cap allow, else the next-highest, else the lowest down); c keeps a's first half and falls by scale steps to 5, 2
    or b3, never the tonic, its last note held to the bar line. The fall starts within the cap of the note before it
    and ends within the cap of a's first note, else the next end by weight. The walk over a and b counts b's last note
    into a' and a's last note into c as moves.
  - `glass` (FUT02): a b a' c on one rhythm in all four bars (the written glass plays 0-3-6-10-14 four times). a' moves
    a's last note a step down; c keeps a's first half, strikes the period's top again (the highest note the cap
    allows) and ends a step under it, open on 2 as written. Its register is E5-F6 (76-89), with the pedal under it.
  - `stab` (FUT04): a second 2-bar riff from the written acid stab's off-beat cells (2-5-7-10-13-14, its bar 2 adding
    12), on the tonic by repeats and steps (move weights 30, 50, 15, 5), its top last on the 5th.
  - B starts on the tonic, or on the 5th when A does, and its first onset never takes A's key there, so no bar of B
    equals a bar that drop 1 plays.
- **The response** (for ANSWER). A's second-half onsets, walked afresh inside the register from below the call and
  ending on varyDegree, 6 velocity steps softer. It never equals A's own second half in a bar.
- **The texture check.** Every pair of textures the style's menus can put next to each other must differ in every bar
  of a line or in none (bar 7 aside, for VARY, CADENCE and BREATH). It runs on the material, and again on each
  section's realized lines (the clash fix and guard included), since the guard can make two textures meet in one bar.
  A failing pair redraws the material (up to 8 attempts under `.with (attempt)`); after that, the failing texture's
  weight is 0 for the song. FULL and OUT never fail it. Proto3 has no texture check, only the per-line fallback, and
  on 1,024 seeds an ANSWER line met the FULL line after it in one bar after the guard in 2 FUT01 and 4 FUT03 drop 1s,
  so those drops had no FULL line.

### Realization, per line
- tick = (section start + line bar + bar) × 1920 + step × 120. key = base + the degree's semitones in the section's own
  mode (a Dorian 6 or a break's Phrygian b2 colours the hook), + 12 per SEQUENCE octave.
- **The avoid-note fit.** A strong note a semitone over a clash tone (1 3 5 7) of its push-robust context moves to the
  nearest pool degree that isn't, inside the register and within the cap of the notes before and after it.
- **VARY and CADENCE** change the line's last note; its end degree takes the octave inside the register nearest the
  note before, within the cap (else the next end degree by weight).
- **The voicing guard: the written-reference mud rule.** No hook note sounds a minor 9th above or below a chord-lane
  note that sounds with it, and a semitone from one only with the lower note at or above the kit's written floor: 68,
  76, 69 and 78 (FUT01-FUT04). The written mixes never break it: no minor 9th either way, and their semitone rubs start
  at exactly those notes (counted by a script beside the prototype). The guard reads the chord lanes' score per
  section, as the union over the section's lines at each 16th of the 8-bar line, and moves a rubbing note to the
  nearest clean pool degree within two steps that keeps the cap with the note before (as moved) and after (as
  written). A note no move can clean rests.
  FUT02's pedal goes through the same guard: a rubbing pedal note rests. One union per section, so every line moves the
  same notes. Prototype: 27, 59, 40 and 97‰ of top notes moved; no top note rested on seeds 0-63 (47 and 28 of about
  425,000 and 481,000 FUT01 and FUT03 top notes on 1,024 seeds); 440 of 29,446 FUT02 pedal notes rested (A4 under a
  pad's Bb4, for one); no minor 9th either way and no low semitone left.
- Lengths from the cell, never into the next note (10 ticks short) nor past the line. Notes outside the natural minor are
  tagged borrowed. Flags: hook, and pedal on FUT02's pedal.

### Seed to notes
1. `generate()`: arrange, planChanges and the harmony, unchanged; the drums; the chords and holdPads; then **planHook**
   if the style has a hook block; then the bass (and its duck), the arp and **scoreLead**; then the controllers, the feel
   and the cut.
2. planHook, under `kh = root.with (tag ("hook"))`, a new permanent top-level tag. In order: A's rhythm, the register,
   the contour, A's degrees, the velocities, varyDegree, B, the response, the redraw, the texture check; then the lines
   (next section), the realized notes and the masks; then it clears the carrier's block bit on OUT lines, and the
   carrier from any one-bar dropout inside an OUT line.
3. scoreLead copies `song.hook.notes` inside the lane's active ranges, so a pulled bar has no hook note. It scales
   velocity by the lane grid's `vel` and the section's vel, and length by the grid's `len`. The masks come from these
   final notes.
4. cutSong trims `hook.lines` and `hook.notes`, so a --bars cut stays the whole song's head.

Cost: the prototype plans and realizes a song in 0.35-0.57 ms, against today's 3-5 ms per 128 bars and the 50 ms budget.

### Stored and exported
- regions.json, only for a song with a hook: per region `"hook": [ { "startBar", "dawBar", "bars": 8, "form",
  "motif", "shift", "ending", "mask" } ]`, `[]` where none; at the top level `"hook": { "lane" }`. A style without a
  hook block writes neither key, so its files stay byte-identical. `format` stays 1, since fields are only added.
- `shift` is SEQUENCE's scale steps (0 in the shipped styles). `ending` is none, vary, cadence or breath.
- The mask is a 16-character string of 0 and 1 in time order: character i is bar i/2, its first half when i is even. A
  half-bar is busy when a lead onset falls in it or a lead note sounds into it. It shows a producer in Live or Logic
  where the hook leaves room, and it is where the vox reply may play.

## Where it plays (scheduling over the drawn forms)

**The lane in the grammar.** The FUT01, FUT03 and FUT04 lead is appended last to the style's lanes and to the `lanes`
of the groove, break, build, drop and drop2 plans. It has no entry, rest, dropout group or roster place, so every form
draw is unchanged and its blocks are every line of those sections. No intro or outro plan lists it: the mix-in and
mix-out stay drums and bass, as in all four written mixes. FUT02's arp keeps its form exactly as today. planHook only
clears bits, after planChanges, so no other lane's draw moves.

**The line plan,** per section in song order:
1. Menus: a drop and drop 2 use their own; a groove, break or build uses its `before` menu until the first drop and its
   `after` menu from then on.
2. `enter` (grooves): leading OUT lines by weight, clamped to the section's lines − 1 before the first drop, so the
   groove's last line plants the hook.
3. Each line draws one op. `first` goes to the carrier's first active line at or after `enter` (an inactive line stays
   OUT and spends no draw: FUT02's arp enters its groove 8 to 32 bars in, and proto2 spent `first` on the silent line
   0); `last` to the section's last line; `next` otherwise. A one-line section draws `first`, and no `first` menu
   holds VARY, CADENCE or BREATH (the loader refuses one), so an ending never lands on a section's first playing line.
   An empty `next` repeats; an empty `last` uses `next`. Key: `kh.with (tag ("plan")).with (s.index).with (line)
   .with (tag ("op"))`.
4. `continue` (builds after drop 1): every line keeps the section before's last state, so break 2's period plays through
   the build into drop 2.
5. The rules, in this order, at the draw:
   - Withheld kinds: no FULL or NEW before the first drop (FUT01, FUT02: groove, break and build; FUT03: break and
     build; FUT04: build). The loader refuses a menu that breaks it.
   - r2 (drop 1 only): if a break ended with the hook and the build after it had none, drop 1's first line is OUT.
   - r1: if the hook played the last bar into a drop, that drop's first line plays it (FULL).
   - Drop 2's first playing line is the new phrase, always.
   - Drop 1 holds a FULL line (FUT01-FUT03): the first drawn FULL, else the line after the first playing line. If FULL
     can't land there on G4's reading, the next line takes it; if no later line can, the earliest line where it lands
     takes it and the lines after it are realized again (once per section).
   - r3: a line straight after a one-bar pull that takes the carrier plays (FUT02's [kick, bass, arp] group), so the
     pull lands back.
   - The cover floor, counted on the states the lines will realize (a REPEAT after OUT doesn't play): while a drop's
     playing lines are under its `cover` (drop 600‰, drop 2 500‰), the last free OUT line takes FULL (SKELETON in a
     withheld kind; never r2's line; drop 2's last line last).
   Every line a rule makes play is marked: it never falls back to OUT.
6. Realize the line and check it with G4's reading (below). On a refusal: REPEAT, then FULL (never in a withheld kind),
   then OUT; a marked line takes SKELETON, then FRAGMENT, instead of OUT.
7. Check every rule again on the realized lines; a test asserts none is broken on 1,024 seeds and the snapshot seeds.
8. An OUT line clears the carrier's bit in that block and removes the carrier from a one-bar dropout inside it (a
   dropout that then lists no lane is removed), so regions.json lists only what plays. With r3 marked, a pull that takes
   the carrier never lands on an OUT line (prototype: no such pull in 1,024 seeds).

**The weights come from the written programs**, the lead lane of each written mix line by line (- OUT, C the call, A
ANSWER, F FULL, S SKELETON, P the new phrase):

| | Groove | Break | Build | Drop | Groove after drop 1 | Break 2 | Drop 2 |
|---|---|---|---|---|---|---|---|
| FUT01 | --CC | -C | - | -CFC | -- | PP | PCPA- |
| FUT02 | -SSS | -- | SS | FFFS | -S | -- | PSC- |
| FUT03 | --FA | -C | -- | -CFC | -F | PP | PCP- |
| FUT04 | --CFCSFS | -- | -- | S--S | none | none | -S-C |

The written C (bar 1 alone, on odd bars) and A (bar 2 alone, on even bars) fail G4 per bar. They map to the G4-safe
half-bar call (FRAGMENT) and ANSWER until S1b2d decides the lead's G4 unit. FUT02's leading "-" in its groove is the
arp not yet entered, so its `first` has no OUT. Three departures stand in for parts the generator doesn't have yet:
- FUT01's and FUT03's drop 1 opens on SKELETON 2:1 against FULL, where the written programs open on OUT. The written
  no-vox mixes fill that line with accent hits (FUT01 "12110211" notes a bar, FUT03 one or two in every bar) and the vox
  mixes with the vox riff; the generator has neither until S1b2c's accents or PR 4. The written OUT returns as data
  when one of them lands.
- FUT04's drops play the blips in every line (S, then S, C or F), where the written S--S hands lines 2 and 3 to the
  rave stab and the metal chords; S1b2c's stabs give them back.
- FUT02's drop 2 keeps the arp driving every line (P, then P, F or A, then F, S or C), where the written PSC- is thinned
  by the laser chord stab, the glass ping, the vox and the accents, none of which the generator has. Below.

**Measured on the drawn forms** (prototype, 64 seeds per kit; FUT01 and FUT03 in the clearing register):

| | FUT01 | FUT02 | FUT03 | FUT04 |
|---|---|---|---|---|
| Plan §5.1: ≥ 3 sections, before drop 1, ≥ 50% of drop bars | 64/64 | 64/64 | 64/64 | 64/64 |
| Drop and drop-2 bars with the hook, pooled | 895‰ | 989‰ | 870‰ | 1000‰ |
| Drop 1 per section: min, median | 666, 1000‰ | 906, 968‰ | 666, 1000‰ | 1000, 1000‰ |
| Drop 2 per section: min, median | 750, 800‰ | 1000, 1000‰ | 750, 750‰ | 1000, 1000‰ |
| Drop 1 opens on | SKELETON 34, FULL 18, OUT 12 (r2) | FULL 64 | SKELETON 30, FULL 19, OUT 15 (r2) | SKELETON 44, FULL 20 |
| Drop 2 opens on the new phrase | 64/64 | 64/64 | 64/64 | 64/64 |
| Drop 1 drew no FULL (the rule placed one) | 18 | 0 | 13 | not forced |
| The first groove before drop 1 without a carrier note | none | 23 (today 23) | none | none |
| Rules broken on the realized lines, 1,024 seeds | 2 (drop 1 FULL) | 0 | 4 (drop 1 FULL) | 0 |

The six drop-1 misses are the ANSWER-then-FULL meetings the realized texture check removes. On the snapshot seeds no
rule breaks. FUT02's 23 grooves (344 of 1,024 seeds, the same as today) are those its arp never enters.

**What other lanes see.** Nothing. The lead is drawn after the chords and reads the chord lanes' notes for its guard,
a declared dependency (chords → lead, as chords → bass); the chords never read it. Nothing reads the lead until the vox
reply. FUT02: the arp keeps its roster place, so the 'who' draws stay; a pick of the arp now changes nothing audible,
since FUT02 no longer runs scoreArp.

## Development and call and response

Each line holds one texture of the material, repeated note for note inside the line (the written lag-4 repetition is
791-1000‰).

| Operator | What it does to the hook | Where |
|---|---|---|
| FULL | the riff four times (the new phrase: the period twice, the stab riff four times) | drop lines; FUT03's and FUT04's grooves; drop 1 always holds one (FUT01-FUT03) |
| REPEAT | the line before, note for note | grooves (--CC), drops (FFF) |
| FRAGMENT, the call | each bar's first half, plus the bar's last onset when its second half holds two or more; in FUT02, with the pedal under it | plants the hook (FUT01, FUT04 grooves; FUT01, FUT03 breaks); the drops' calls |
| SKELETON | the accented onsets only (FUT02: the top voice, no pedal) | FUT02's grooves and builds; drop 1's first line (FUT01, FUT03); FUT04's drops |
| ANSWER | the call, then the response in each bar's second half | FUT03's groove (--FA); drops |
| VARY | the line's last bar ends on varyDegree | drops' last lines |
| CADENCE | the line's last bar falls by step to the 5th, held to the bar line | drops' last lines, before a strip |
| BREATH | the line's last bar rests | FUT02's grooves, builds and drop 1 (the written arp rests bar 7 in 5 of its 12 playing lines); FUT03's break 2 (1 of 12) |
| NEW | B, drop 2's new phrase | drop 2's first line, break 2, later drop-2 lines |
| AUGMENT | A at half speed, onsets and lengths doubled | the pre-drop break's last line in FUT01 and FUT03, weight 1 of 5 |
| SEQUENCE | the line an octave up | weight 0 in the shipped styles (decision 8) |
| OUT | the lead rests: the hand-off | builds (FUT01, FUT03, FUT04), drop-2 strips, the hand-offs the written mixes make |

Not built: SENTENCE (no written hook is one, and bar 1 = bar 3 rhythm holds in every written 4-bar window), and the
written bar-by-bar call and answer, which G4 per bar counts off phrase. FRAGMENT stands in for the written C: it is
279, 18, 249 and 276‰ of the playing lead lines (prototype), against the written C's 6 of 12 playing lines in FUT01 and
4 of 12 in FUT03.

**The line rule.** G4 compares bar i with bar i−8 and counts a run of differing bars once, on phrase at bar 0 or 7 of a
line. So a line differs from the line before in every bar or in none; bar 7 alone may differ (VARY, CADENCE, BREATH).
- The texture check makes this true of the material and of each section's realized lines. `bool onTheLine (const
  LinePrints& before, const LinePrints& after)` is the predicate, unit-tested on hand-made prints.
- The planner checks every line on its realized notes, the guard included, and falls back as above.
- Prototype: 1, 0, 1 and 0 refusals on seeds 0-63; 5, 15, 14 and 0 on seeds 0-1023.

**Call and response**
1. Inside the hook: bar 2 answers bar 1, with bar 1's first-half rhythm, at most one second-half edit, new pitches and
   the peak (late contour).
2. Inside the lead: ANSWER plays the call and the response in each bar's two halves.
3. Between two voices: the half-bar mask, 16 bits per line, from the lane's final notes. It is song-level data on
   `HookLine`, exported in regions.json.
4. The vox reply (PR 4, after ADR 0016; music-first's design):
   - lane `vox`, kind lead with `"part": "reply"`, track 'Vox Synth' (role vox), MIDI v2 channel 5; in the groove,
     break, drop and drop2 plans only; its kit range is the written vox's: FUT01 69-78, FUT02 62-81, FUT03 67-77,
     FUT04 52-62 (the hook lanes' floor rule exempts a reply part; the guard and the mud test apply to it);
   - its own per-seed 2-bar riff under `root.with (tag ("reply"))`, from the written vox cells (FUT01 6,9,10,15 |
     6,9,10,14; FUT02 0,3,6,10,12 | 0,3,6,8,10,12; FUT03 2,6,10,13 | 2,6,10,14; FUT04 3,7,11 | 3,7,11,14) with the same
     mutations; pitched chord-following (the written vox riffs take the iv chord's b6), mostly by step;
   - the map from the lead's line: FRAGMENT → reply (the riff's notes in the half-bars the mask leaves free); SKELETON
     and AUGMENT → fill (the same, in free half-bars); OUT → riff (the riff alone: the hand-off; with this PR, drop 1's
     first line goes back to OUT 2:1 against FULL in FUT01 and FUT03, and the vox riff takes it, as the written vox
     mixes play it); FULL, ANSWER and NEW → rest;
   - it plays only in free half-bars, so its onsets never land on a lead onset nor inside a sounding lead note, and
     the voicing guard applies to it, against the lead too;
   - it never plays a build, the intro or the outro, nor the last line before a build or a drop, so r1 and r2 hold;
   - FUT02's vowel velocities 64/95/127 and FUT04's 64/127 are kept, with feel vsd 0;
   - FUT02 never has three top lines: the new phrase replaces the arp's top voice on its own lane.

## Per kit

**Channels, every kit (PR 1).** Bass JSON 0 (MIDI 1), lead JSON 1 (MIDI 2), chords stay on JSON 2 (MIDI 3, the chord
voicing), vox JSON 4 (MIDI 5), drums JSON 9 (MIDI 10). FUT04's rumble moves from JSON 3 (MIDI 4, the chord sample) to
JSON 8 (MIDI 9, reserved: Wubansia plays nothing there) until it becomes drum note 35.

**The register (decision 13).** The written leads sit at the written chords' top: their notes are at or under the top
chord-lane note sounding with them 199, 30, 178 and 363‰ of the time (FUT01-FUT04; the Lead track of the vox mixes).
The generated chord voicings reach higher (top note p50 71-72, p90 76-77 in FUT01-FUT03). In the written register
(FUT01 from F#4, FUT03 from G4, medians 73 and 74 as written) the hook sits inside the generated chords 345 and 332‰ of
the time. In the clearing register (FUT01 from G#4, FUT03 from A4, medians 78 and 79) it sits there 148 and 129‰.
Every gate passes either way. Recommended: the clearing register, until the styles' retune lowers the chord tops
(decision 7); the figures below use it.

### FUT01 Alloy Signal (2-step garage, F# minor, 134)
- Lane `lead`, track 'Lead Pluck' (Chrome Pluck in Wubansia), range 68-86, center 74. The written pluck spans 66-80
  (median 73); the drop stab tops sit at p50 72.
- Cells: the pluck 0-3-6-8-11-14 (w30), 0-3-6-10-12 (16), the vox riff's late start 6-9-10-15 (14), off-beat 2-6-10-13
  (14), 0-3-7-8-11-14 (12), a 3-16th cycle 0-3-6-9-12 (14).
- Pool 1 2 b3 4 5 b7 (the written lead riffs' set). Leap cap 9. Contours late 3, early 1 (the written 4-bar riffs peak
  in bar 3 or 4, the vox riff in bar 1). The end falls off the tonic 96 times in 100.
- Velocity, from the written pluck (127/121 on the downbeat, 114 on beat 3, 102 on the and of 2, 76-95 the rest): the
  downbeat 124, beat 3 114, other accents 104, the rest 88, the peak +6, ±4.
- Programs: groove --CC (enter after 8 or 16 bars, 1:2), break -C, build out, drop SCFC (SKELETON 2:1 against FULL
  first), the groove after drop 1 out, break 2 the period, drop 2 PCPA- (the strip 3:1 against CADENCE).
- The period: the organ idiom, halves 8,10,13 / 8,10 / 8,11,13.
- Prototype: 64 distinct hooks, 52 distinct rhythms; 60 of 64 end off the tonic; the peak in bar 1 in 12.

### FUT02 Orbit Relay (sci-fi club, D minor, 130): the arp motif
- The arp lane keeps its id `arp`, track 'Arp Chrome' and CC74 rows. It becomes kind lead, channel JSON 1, kit range
  69-89. FUT02 no longer runs scoreArp; `core/gen/arp.cpp` stays, covered by a fixture style.
- **The written ChromeArp**: the hook's top voice on the cell's onsets, from C5 up (the written top voice is C5-C6, one
  note in 12 under D5; prototype 9.4% of FULL and ANSWER top notes at or under D5, against proto2's 29%). Under it, a
  key-fixed pedal: after each top note, on the free 16ths before the next one, at most the tonic D5 then the 5th below,
  A4, each only under the top note it follows; never on 16th 15 (the note before it holds through it), never before
  the bar's first top note. The pedal never moves with the chord. It plays under FULL, ANSWER, FRAGMENT (in the half
  it plays) and NEW; SKELETON is the top voice alone. Prototype: 953 pedal notes for every 1,000 top notes (the written
  arp 18 for 12), none before a bar's first top note, none at or above its top note.
- Strong onsets are the beats. Leap cap 9. Contours late 2, twin 2, early 1.
- Cells, all from the downbeat: the arp's accents 0-3-6-8-11-14 (all accents, w30), the glass 0-3-6-10-14 (25), the vox
  0-3-6-10-12 (20), 0-3-6-9-12 (12). Proto2's off-beat 2-6-10-14 is gone: under it the pedal played 11 notes against 4.
  Bar 2 may repeat bar 1's opening, as written.
- Velocity: beat accents 127, other accents 112, others 100, the peak +6, ±4; the pedal 80 and 66 (written: 127 and 114,
  bar 2 121; the pedal 79/64 and 84/69).
- Programs: groove -SSS (SKELETON on the arp's first active line, then REPEAT 2, BREATH 2, SKELETON 1, under the arp's
  closed CC74), break out (the choir's), build SS with BREATH last, drop FFFS (with VARY and BREATH), the groove after
  drop 1 -S, break 2 out, drop 2 P then P, F or A, then F, S or C.
- The new phrase: the glass idiom, E5-F6, the pedal under it.
- **Drop 2 against drop 1.** ADR 0023 has FUT02's drop 2 lift on "a driving arp, from its first bar". The written PSC-
  is sparse only because other parts fill it. Prototype: drop 2 54.2 notes a bar against drop 1's 48.3 (the arp 11.8
  and 11.9; today 57.3 and 50.0, the arp 14.9 and 13.6); thinner than drop 1 in 9 of 64 seeds (64 of 1,024), against
  0 of 64 today (23 of 1,024). Proto2's PSC- with the top voice alone: 49 of 64.
- Prototype: R10 lead 71‰ today to 1‰ (3 of 3,212); R6 rest 125‰ to 250‰ (written 375‰); R11 1.66 a bar (written arp
  2.76); G4 lead 246/255 to 314/314; G5 lead 30 values, SD 11.85, to 32 values, SD 20.15.

### FUT03 Glass Current (garage-house, G minor, 128)
- Lane `lead`, track 'Lead Keys' (Keys EP), range 69-86, center 76. It follows the kit's written garage-house material;
  the style is still round 3's future house (decision 9).
- Cells: the keys riff 0-3-6-8-11-14 (28), its answer bar 0-3-6-9-11-14 (14), off-beat 2-6-10-14 (20), the dub stab's
  3-6-11-14 (16), 2-6-10-13 (12), 0-3-6-10-12 (10). Pool hexatonic; leap cap 9; contours late 2, early 2; the end off
  the tonic 94 times in 100.
- Programs: groove --FA (it states the riff early, as written; enter after 8 or 16 bars, 2:1), break -C, build out,
  drop SCFC (SKELETON 2:1 against FULL first), the groove after drop 1 -F, break 2 PP (BREATH 1 in 6 on its later
  lines), drop 2 PCP-.
- The new phrase: the organ period. The written drop 2 and break 2 play a 16th keys arpeggio of the chords; a chord
  arpeggio repeats across seeds (FUT02's arp today: R10 71‰), so the period stays until the S1d listen.
- Prototype: 64 hooks, 44 rhythms; the peak in bar 1 in 29; R10 lead 0; G4 lead 282/282.

### FUT04 Furnace Code (dark industrial techno, E minor, 136)
- Lane `lead`, track 'Lead Blips' (Mod Blip), range 71-88, center 78. The acid stays the protagonist, in the bass role.
- Cells: the written blips 0,3,4,6,7,10,11,12,14 with accents 0, 4 and 12 (w30), the rave stab's 3-16th cycle
  2,5,8,11,14 (15), 0,3,6,9,12,15 (15), the chant's 16ths before the beats 3,7,11,14 (15), 3-3-2 (10), 0,4,8,12,14 (15).
- Pool: all seven degrees, the b6 as colour and as peak. Moves from the written blips: steps 44%, 3rds 11%, 4ths and
  5ths 22%, 6ths to the octave 17%, repeats 6%. Leap cap 10, the written blips' largest move (proto2's 12 let 525 moves
  of 10-13 into 40 riffs). Contours late 2, twin 2 (the written C6 struck again). Velocity: the downbeat 124 (written
  127), accents 114, others 88.
- Programs: groove --CFCSFS (enter after 16, 24 or 32 bars), break and build out, drop S (3:1 against FULL), then S,
  C or F with no OUT until S1b2c's stabs (no forced FULL: the written drop states none), drop 2 the stab riff, then S
  or C.
- The new phrase: the acid-stab riff on the blips, where the written mix plays its acid stab.
- Prototype: R10 lead 0; G4 lead 241/241. The guard bends the blips most: 97‰ of notes, 150-156‰ in the drone's
  sections. The drone often voices F#4 (66), so the blips' G5 sits a minor 9th above it (decision 7).

### Drop 2's own material (PR 3)
ADR 0023 left "drop 2's material of its own" to this step. The written drop 2s change their bass in all four kits. PR 3
builds each as a data grid modelled on the written line, varying per seed through the grid's draws: FUT02's Relay,
FUT03's Deep, FUT04's AcidLine2, and FUT01's warp riff in the bars its kick rests.

**FUT01's warp riff.** The written WarpRiff, bar by bar:
- Bar 1, over F#m: the root F#1 on 16ths 0, 3 and 11; the F#2 octave on 2, 5 and 7; the 5th C#2 on 6 slid into the
  F#2 (the dip); the b7 E2 on 9; the 3rd A1 on 13; the 5th C#2 on 14, 1.75 16ths long, ending at 15.75.
- Bar 2, over Bm: the root B1 on 0 and 3; F#2 held as a pedal on 2, 5 and 7 (the key's tonic, the chord's 5th); the 3rd
  D2 on 6 slid into the F#2; E2 on 9; C#2 on 11; E2 on 13; C#2 on 14 slid into E2 on 15.
- Nothing slides into the next bar.

The bass grid's degrees are chord-relative (`BassDegree`: root, octave, fifth, seventh, third), so degree-set rows alone
would play B octaves and an F#2 fifth over the iv chord: a different riff. PR 3 adds key-relative degrees (the key's
tonic, 5th and b7 in a fixed octave): the pedal on 2, 5 and 7 is the key's tonic, E2 on 9 the key's b7; the roots and
the dip (the chord's 5th, or its 3rd when that sits nearer the pedal) stay chord-relative. The tail follows the written
bar by bar. The riff plays the drop-2 bars where the kick rests (the written bars 161-168), through a bass field
`"rested": { "drop2": "warp" }`; the kick rests in 28 of 64 FUT01 drop 2s, 112 2-bar windows. Its variants (the pedal on
2, 5 and 7 or on 2 and 7; the dip on 6 or 14) keep those windows apart across seeds: R10 for the bass, 4‰ today, is
pinned at 30‰ or under.

**FUT01's bass talk.** CC74 on the bass lane, a point every 60 ticks, an arch per 8th reading 0, d/2, d, d/2 (the
written values at every second point), d = 82, 114, 82 and 127 by bar over each 4-bar cycle from the section's start.
It plays drop 1, the groove after drop 1 and drop 2 but its last 8 bars, as written (bars 89-136 and 153-184; the
written drop 2 runs to bar 192). It writes nothing anywhere else: the written mix sends no CC74 outside the talk, and a
flat 0 at the start of every other bass range would close a Live or Logic user's own synth filter for the mix-in and the
groove. The talk's last point is its arch's, as written.
- Not built: the growl bar and the b2 slide, after the S1d listen. The bass track stays 'Sub + Reese'.

## Style data

Integers only, under the strict reader's rules (unknown keys refused). parseStyle's top-level `only()` list gains
`"hook"`; LaneKind gains `lead`. A lead lane's grid holds only its velocity and length scale, in permille.

### Full example: content/styles/garage2step.json (FUT01)
The new top-level block:
```json
"hook": {
  "lane": "lead",
  "register": { "center": 74, "span": 15 },
  "contours": [ { "shape": "late", "w": 3 }, { "shape": "early", "w": 1 } ],
  "cells": [
    { "on": [0, 3, 6, 8, 11, 14], "len": [2, 2, 3, 2, 2, 2], "acc": [0, 6, 8], "w": 30 },
    { "on": [0, 3, 6, 10, 12], "len": [2, 2, 3, 2, 2], "acc": [0, 6, 10], "w": 16 },
    { "on": [6, 9, 10, 15], "len": [2, 1, 3, 1], "acc": [6, 10], "w": 14 },
    { "on": [2, 6, 10, 13], "len": [2, 2, 2, 2], "acc": [6, 10], "w": 14 },
    { "on": [0, 3, 7, 8, 11, 14], "len": [2, 2, 1, 2, 2, 2], "acc": [0, 8, 14], "w": 12 },
    { "on": [0, 3, 6, 9, 12], "len": [2, 2, 2, 2, 3], "acc": [0, 6, 12], "w": 14 }
  ],
  "mutate": { "drop": 25, "add": 25, "extra": [2, 7, 10, 13, 15] },
  "answer": { "same": 4, "move": 3, "add": 2, "drop": 1, "headDiffers": 1 },
  "pitch": {
    "pool": [0, 1, 2, 3, 4, 6],
    "start": [40, 0, 20, 0, 30, 0, 10],
    "end": [4, 25, 20, 0, 35, 0, 20],
    "moves": [10, 45, 27, 9, 9, 0, 0, 0],
    "leap": 9,
    "strong": "accents",
    "peak": [18, 28],
    "early": [4, 12],
    "tones": [0, 2, 4, 6, 8],
    "clash": [0, 2, 4, 6]
  },
  "vel": { "down": 124, "beat": 114, "accent": 104, "other": 88, "peak": 6, "shape": 4 },
  "guard": { "second": 68 },
  "period": {
    "idiom": "organ",
    "halves": [ { "on": [8, 10, 13], "w": 2 }, { "on": [8, 10], "w": 1 }, { "on": [8, 11, 13], "w": 1 } ],
    "octave": 0,
    "cadence": [0, 2, 1, 0, 2, 0, 0]
  },
  "withhold": ["groove", "break", "build"],
  "plan": {
    "groove": {
      "enter": [ { "lines": 1, "w": 1 }, { "lines": 2, "w": 2 } ],
      "first": [ { "op": "fragment", "w": 3 }, { "op": "skeleton", "w": 1 } ],
      "next": [ { "op": "repeat", "w": 3 }, { "op": "answer", "w": 1 }, { "op": "skeleton", "w": 1 } ] },
    "break": {
      "first": [ { "op": "out", "w": 3 }, { "op": "fragment", "w": 1 } ],
      "next": [ { "op": "fragment", "w": 1 } ],
      "last": [ { "op": "fragment", "w": 2 }, { "op": "repeat", "w": 2 }, { "op": "augment", "w": 1 } ] },
    "build": { "first": [ { "op": "out", "w": 1 } ] },
    "drop": {
      "first": [ { "op": "skeleton", "w": 2 }, { "op": "full", "w": 1 } ],
      "next": [ { "op": "fragment", "w": 2 }, { "op": "full", "w": 2 }, { "op": "answer", "w": 1 } ],
      "last": [ { "op": "fragment", "w": 2 }, { "op": "vary", "w": 1 }, { "op": "cadence", "w": 1 }, { "op": "full", "w": 1 } ],
      "holdFull": 1,
      "cover": 600 },
    "drop2": {
      "first": [ { "op": "new", "w": 1 } ],
      "next": [ { "op": "fragment", "w": 2 }, { "op": "new", "w": 2 }, { "op": "answer", "w": 1 } ],
      "last": [ { "op": "out", "w": 3 }, { "op": "cadence", "w": 1 } ],
      "cover": 500 },
    "after": {
      "groove": { "first": [ { "op": "out", "w": 3 }, { "op": "fragment", "w": 1 } ] },
      "break": {
        "first": [ { "op": "new", "w": 3 }, { "op": "out", "w": 1 } ],
        "next": [ { "op": "repeat", "w": 3 }, { "op": "out", "w": 1 } ] },
      "build": { "continue": 1 } }
  }
}
```
- `pitch`: `pool`, `start` and `end` are scale degrees from the tonic (0-6); `moves` weights moves of 0-7 scale steps;
  `leap` caps a move in semitones, the wrap and the joins included; `strong` is `accents` (a beat or an accent) or
  `beats`; `peak` and `early` are the late and early contours' windows in 16ths of the 2-bar phrase; `tones` and
  `clash` are scale steps above a chord's root (the 1 3 5 7 9 a strong onset takes; the 1 3 5 7 it can't sit a
  semitone over).
- `vel`: `down` is the downbeat's accent, `beat` beat 3's.
- `guard.second`: a semitone between the hook and a chord-lane note only with the lower note at 68 or above. A minor 9th
  either way is never allowed.
- `period.cadence`: weights of the organ fall's end degree (here 2, b3, 5); the tonic's must be 0.

The lane, appended last to `lanes`, with `"lead"` appended to the `lanes` of the groove, break, build, drop and drop2
plans (nothing else in the form changes):
```json
{ "id": "lead", "kind": "lead",
  "use": { "groove": "plant", "break": "plant", "build": "plant", "drop": "main", "drop2": "main" },
  "grids": { "plant": { "vel": 920, "len": 850 }, "main": { "vel": 1000, "len": 1000 } } }
```
No CC rows: no written FUT01 lead rides one.

content/kits/fut01.json: the bass lane's `"channel"` 1 becomes 0 (PR 1), and a new lane (PR 2):
```json
"lead": { "track": "Lead Pluck", "channel": 1, "range": [68, 86] }
```

content/feels/garage2step.json, `lanes` (the written pluck sits on swing 590, the kit on 570):
```json
"lead": { "swing": [5, 20], "sd": [8, 12], "clamp": 6, "anchor": 0, "vsd": [14, 20], "artic": 50 }
```

### The loader's refusals (each with file, JSON path and line)
- The hook: `lane` missing or not a lead lane; two carriers; a lead lane without a hook block (a reply part aside).
- `register`: a center outside the kit range, a span over it. Every hook lane's kit range spans 12 or more and starts at
  `mud.minorSecond` or above (a reply part aside).
- `contours`: an unknown shape, all weights 0, a window outside the phrase.
- `cells`: steps outside 0-15 or not ascending; `len` and `on` of different sizes, a length outside 1-16; an accent not
  among the onsets; a half without an onset; no non-accent onset in a style without a pedal; a pedal style's cell
  without onset 0; all weights 0.
- `mutate` outside 0-100 or extra steps outside 0-15 (and 15 in a pedal style); `answer` weights all 0.
- `pitch`: pool degrees outside 0-6 or repeated, or missing a degree `start` or `end` can draw; `start` or `end` not 7
  weights or all 0; `moves` not 8 weights; `leap` outside 1-12; `strong` other than accents or beats; windows outside
  0-31; `tones` or `clash` outside 0-8.
- `vel` values outside 1-127, `peak` above 20, `shape` above 12; `guard.second` outside 0-127.
- `period`: an idiom other than organ, glass or stab; halves outside 8-15 or not ascending; `octave` outside -1 to 1; a
  cadence weight on the tonic; a `register` outside the kit range; a stab idiom without its cells.
- `pedal` (FUT02): degrees outside -7 to 7, velocities outside 1-127, `in` beyond full, answer, fragment and new, `max`
  outside 1-2.
- `plan`: an unknown op; a menu for a kind the carrier's plans don't play it in, or a played kind without one; VARY,
  CADENCE or BREATH in a `first` menu; FULL or NEW in a withheld kind's `before` menu; `continue` outside an `after`
  build; a drop or drop 2 without `cover`, a cover under 500, or a cover no drawable length can reach (cover × lines >
  (lines − 1) × 1000); drop 2's `first` other than `new`; no kind before the first drop that always plants the hook (the
  clamped groove, or a build menu without OUT). The last two rules are plan §5.1 at every length the form can draw.

### The other styles, in short
- **sciclub (FUT02):** `"lane": "arp"`; `"register": { "center": 79, "span": 14, "floor": 72 }`; `"strong": "beats"`;
  contours late 2, twin 2, early 1; the four cells above; `"answer": { ..., "headDiffers": 0 }`; `"vel": { "beat": 127,
  "accent": 112, "other": 100, "peak": 6, "shape": 4 }`; `"guard": { "second": 76 }`; `"pedal": { "degrees": [0, -3],
  "vel": [80, 66], "in": ["full", "answer", "fragment", "new"], "max": 2 }`; `"period": { "idiom": "glass", "register":
  [76, 89], "halves": [ { "on": [10, 14], "w": 1 } ] }`; withhold groove, break, build; menus as in "Per kit". The arp
  lane: `"kind": "lead"`, lead grids in place of its arp grids, its `cc` unchanged. Kit: arp `"channel": 1`, `"range":
  [69, 89]`.
- **futurehouse (FUT03):** the cells above, center 76, contours late 2, early 2, `"guard": { "second": 69 }`, end
  weights allowing the tonic at 6%; withhold break, build; menus weighted from --FA, -C, --, SCFC, -F, PP, PCP-. Kit:
  'Lead Keys', channel 1, [69, 86]. Feel: lead swing [10, 25].
- **techno (FUT04):** pool `[0, 1, 2, 3, 4, 5, 6]`, `"moves": [6, 44, 11, 11, 11, 6, 6, 5]`, `"leap": 10`, center 78,
  contours late 2, twin 2, `"vel": { "down": 124, "accent": 114, "other": 88, ... }`, `"guard": { "second": 78 }`,
  `"period": { "idiom": "stab", "cells": [ { "on": [2, 5, 7, 10, 13, 14], "len": [1, 1, 1, 1, 1, 2], "acc": [2, 7, 10,
  14], "w": 3 }, { "on": [2, 5, 8, 11, 14], ..., "w": 1 } ] }`; withhold build; drop `"holdFull": 0`, cover 500; menus as
  in "Per kit". Kit: 'Lead Blips', channel 1, [71, 88]; the rumble `"channel": 8` (PR 1). Feel: lead swing [0, 8], sd
  [6, 10], clamp 4, vsd [12, 18], artic 30.

### PR 3 data: drop 2's bass and FUT01's talk
```json
"rested": { "drop2": "warp" },
"cc": [ { "number": 74, "step": 60, "in": ["drop", "drop2", "after.groove"], "rows": {
  "drop": { "from": [0, 0], "to": [0, 0], "pulse": { "every": 240, "depth": [82, 114, 82, 127] } },
  "drop2": { "from": [0, 0], "to": [0, 0], "pulse": { "every": 240, "depth": [82, 114, 82, 127], "endBefore": 8 } },
  "after": { "groove": { "from": [0, 0], "to": [0, 0], "pulse": { "every": 240, "depth": [82, 114, 82, 127] } } } } } ]
```
- `in`: the kinds a CC spec writes in; it writes no point anywhere else, and `rows` needs exactly those kinds. A spec
  without `in` keeps today's rule (a row for every kind the lane plays), so every shipped CC lane is unchanged.
- `pulse`: a triangle per `every` ticks, peaking mid-8th, added to the row's arc; its depth per bar cycles through the
  list from the section's start; the value is floor (depth × (every − |2k − every|) / every) at k ticks into the 8th, so
  0, 41, 82, 41 for 82. `endBefore`: bars before the section's end where the pulse, and the spec's points, stop. The
  loader allows `every` 120, 240 or 480, depths 0-127 and `endBefore` 0-32.
- `after`: rows for the sections after the first drop (the switch-up).
- The bass grids: each kit's drop-2 line as a `drop2` grid in the existing BassGrid fields (p, deg, len, vel, slide);
  the warp grid also uses the new key-relative degrees (`"keyTonic"`, `"keyFifth"`, `"keySeventh"`, each with its
  octave), appended after the existing degree kinds so the existing rows keep their indices and draws.

### PR 4 data: the vox reply
- `hook.reply`: `"lane": "vox"`; its own `cells`, `mutate` and `answer`; `"pitch": { "follow": "chord", ... }`, with
  move weights per kit from its written vox riff (step ratios 717, 529, 230 and 0‰); `vel`, with FUT02's
  `"vowels": [64, 95, 127]` and FUT04's `[64, 127]`.
- `"map": { "fragment": "reply", "skeleton": "fill", "augment": "fill", "out": "riff", "full": "rest", "answer": "rest",
  "new": "rest" }`.
- The lane `{ "id": "vox", "kind": "lead", "part": "reply", ... }` in the groove, break, drop and drop2 plans; in the kits
  `"vox": { "track": "Vox Synth", "channel": 4, "range": [...] }` with the written vox's range; in the feels, vsd 0 for
  FUT02 and FUT04.

## Determinism, timing and snapshots
- **Keys.** Every draw hangs below `root.with (tag ("hook"))` (the vox: `root.with (tag ("reply"))`). The permanent tag
  names, listed in the hook's ADR: `rhythm` (`cell`, `mutate` with `drop`, `which`, `add`, `at`; `bar2` with `edit`,
  `at`), `start`, `end`, `contour`, `walk` (`attempt`, `peak`, `twin`), `vel`, `vary`, `b` (`rhythm` with `half`,
  `bar2`; `end`, `walk`, `cadence`, `vel`), `response` (`walk`), `redraw`, `check` (`attempt`), `plan` (per section
  `enter`; per line `op`). No existing tag or key path changes, so the form, harmony, drums, chords and bass draw
  exactly as today.
- **Arithmetic.** Integers only: pickWeighted over integer weights, between, chance, pickIndex; floorDiv and floorMod;
  stable sorts with total orders; no unordered containers. The guard's union is two uint64 masks (keys 0-127) per 16th.
- **Caps.** 32 walk attempts; 8 material redraws; 8 texture-check redraws; 3 fallbacks per line; one re-realization
  per section for drop 1's FULL. Nothing loops without a cap.
- **test_rng** gains reference vectors for `tag ("hook")`, `tag ("reply")` and the key
  `root.with (tag ("hook")).with (tag ("rhythm")).with (tag ("cell"))`, so a renamed tag fails a test, not only the
  snapshots.
- **Timing.** 0.35-0.57 ms a song for planHook and the guard (prototype), against 3-5 ms used of the 50 ms per 128 bars.
  The existing timing tests guard it in Release.
- **Snapshots.** `core/tests/snapshots/gen.sha256` changes in its own commit in each PR, with its reason: the channels
  (every file, only where it names a channel), the hook (every file of all four kits), drop 2's bass and the talk
  (every file), the vox (every file). The macOS arm64 job checks parity on each PR. `legacy_expected.json` and
  `content/golden/` are never touched.

## Tests first

Written before the code, failing, in their own commit with declarations and stubs so the tree compiles. 64 seeds × 4
kits unless noted; "1,024" means seeds 0-1023 plus the snapshot seeds (42, 977, 65536, 123456789, 2^64−1).

**New: core/tests/test_gen_hook.cpp** (`[gen][hook]`)
1. "one hook per seed, the same bytes from the same seed": two runs give equal `Song::hook`; at least 60 distinct A
   phrases in 64 seeds per kit (prototype 64).
2. "rhythm first": bar 1 is a style cell with at most one non-accent onset out and one extra in; each half of each bar
   holds an onset; each bar holds a non-accent onset in a style without a pedal; a pedal style's bars start on 16th 0.
3. "bar 2 answers bar 1": bar 2's onsets 0-7 equal bar 1's; at most one edit among 8-15; never a transposition of bar 1;
   with `headDiffers`, a pitch difference in the first half.
4. "the walk": every material note inside its register; the late contour's peak the highest note alone in 16ths 18-28,
   the early one's in 4-12, the twin's top struck on two onsets in a row; at least 56 of 64 end off the tonic (prototype
   58-63); at least 850‰ of strong lead notes are chord tones (1 3 5 7 9) of the chord sounding (prototype 895, 939,
   905, 1000).
5. "contours vary": the peak lands in bar 1 in at least 6 of 64 FUT01 seeds and 16 of 64 FUT03 seeds (prototype 12,
   29); a twin top in at least 12 of 64 FUT02 and FUT04 seeds (prototype 25, 35).
6. "no leap over the cap" (1,024): consecutive hook notes of a realized line, the riff's wrap and the new phrase's joins
   included, never further apart than the style's `leap`, except across a rest of a bar or more (a pull). Prototype:
   material clean after the redraws; realized moves over the cap 16, 45, 11 and 0 in 1,024 (from VARY and CADENCE ends
   it placed without the cap, the guard's rests, and FUT02's pulled bars; the ends' cap rule and the exception close
   them, and a rested note's neighbours are checked like a move).
7. "textures differ in every bar or in none": for every pair of textures a style's menus can put side by side, the
   material's 8-bar prints and each section's realized prints; every texture holds a note in every bar (bar 7 aside
   for BREATH).
8. "onTheLine" on hand-made prints. Safe: REPEAT; FULL to the half-bar call; anything to or from OUT; a change in bar 7
   only (BREATH included). Refused: FULL to a bar-level call (A, then silence); a change that starts at bar 3.
9. "changes land on the line": per section of 9 bars or more, the lead track's bar prints against 8 bars earlier; every
   run starts at bar 0 or 7 of a line.
10. "placement, plan §5.1" (1,024): hook notes in at least 3 sections and before the first drop; at least 500‰ of drop
    and drop-2 bars pooled; each drop at its cover floor, counted on realized lines; none in an intro or outro.
11. "held back, then paid off" (1,024): no FULL or NEW in a withheld kind before the first drop; drop 1 holds a FULL
    line where `holdFull` (FUT01-FUT03); drop 2's first playing line is the new phrase; before drop 1, the groove's last
    line plays the hook (FUT01, FUT03, FUT04) or the build does (FUT02).
12. "the DJ rules hold on the realized lines" (1,024): r1, r2 (drop 1 only) and r3, plus fixture forms that force each;
    a line a rule makes play is never OUT. The generic form tests stay unchanged and cover the lead.
13. "FUT02's groove keeps the arp": every groove whose arp is active in some line plays hook notes there, so the
    grooves without the arp are exactly today's (prototype 23 of 64, 344 of 1,024, as today).
14. "drop 2 plays a phrase drop 1 never played": drop 2's first-line bar prints never occur in drop 1 (prototype: 0).
15. "the new phrase in its idiom": organ: bars 1 and 3 share the rhythm and differ in one note, bar 2 keeps bar 1's first
    half, bar 4 keeps it too and falls by scale steps to 5, 2 or b3 with its last note held to the bar line; glass: one
    rhythm in all four bars, bar 4 strikes the top again; stab: a 2-bar riff from the stab cells, on the tonic, its
    last note the 5th.
16. "a fixed accent shape": in FULL lines bars i and i+2 carry equal score velocities; the median score SD over FULL
    lines is at least 9 per kit (prototype 12.9, 21.0, 11.5, 14.5); FUT01's and FUT04's downbeat accents sit above
    their other accents.
17. "no mud against the chords": no lead note a minor 9th above or below a chord-lane note sounding with it; a semitone
    only with the lower note at the kit's floor (68, 76, 69, 78) or above, FUT02's pedal included (prototype: none).
18. "register": every lead note inside its kit range; every hook lane's kit range starts at 64 or above; lead notes at
    or under the top sounding chord-lane note at most 200, 250, 200 and 100‰ in the clearing register (prototype 148,
    174, 129, 17), 400‰ for FUT01 and FUT03 in the written one (345, 332).
19. "FUT02's arp is the written ChromeArp": the top voice on the cell's onsets, at or above C5, at most 12% of FULL and
    ANSWER top notes at or under D5 (prototype 9.4%); after each top note at most two pedal notes, D5 then A4, each under
    the top note it follows; none before the bar's first top note; no onset on 16th 15; SKELETON plays the top voice
    alone; R6 rest median at least 200‰ (prototype 250).
20. "FUT02's drop 2 drives": drop 2's notes a bar pooled at least drop 1's (prototype 54.2 against 48.3); drop 2 thinner
    than drop 1 in at most 12 of 64 seeds (prototype 9).
21. "the hook lane is additive": FUT01, FUT03 and FUT04 seeds 0-15 without the lead lane and hook block give every other
    lane's score, perf and CC equal; FUT02 against a fixture copy of today's sciclub.json (arp kind, no hook block),
    every lane but the arp equal.
22. "a style without a hook block generates the same bytes": a fixture style's file hashes, recorded on main before the
    change; its regions.json holds no `hook` key.
23. "regions.json lists the hook lines": each region's `hook` entries equal `song.hook.lines` in it, masks recomputed
    from the lane's final notes in time order; `[]` where none; `format` 1.
24. "a --bars cut keeps the whole song's hook": at every 8-bar line of 16 seeds per kit.
25. "an OUT line leaves no dropout": no one-bar dropout lists the carrier inside a line where the carrier is OUT.
26. test_rng: the vectors above.

**Loader** (test_gen_styles_content.cpp, test_gen_content.cpp): each refusal in the list above, with its path.

**Edited tests**
- test_gen_gates.cpp: the pins become FUT01 {G7, G9}, FUT02 {G9}, FUT03 {G7, G9}, FUT04 {G1, G2, G5, G9}. The lead joins
  G4's role loop (changes > 0, on phrase at least 950‰). The G5 lead group passes on every kit (FUT04 still fails G5
  on its ghosts alone). R10 lead at most 30‰ per kit.
- test_gen_form.cpp: "every drop plays its plan's lanes from its first block…" exempts the style's hook lane and a reply
  lane, whose lines the hook tests pin. "regions.json lists each region's one-bar dropouts" and "every lane a dropout
  takes out lands back" run unchanged and pass because of the dropout clearing and r3.
- test_gen_styles_export.cpp:370 ("a cut's files are the whole song's regions…") and cli/tests/cut_check.cpp:448-482
  (`groovecore_cut_check`) rebuild the expected regions.json; both also trim the shortened region's `hook` entries to
  the lines before the cut, as they trim its dropouts. Every CLI cut test cuts inside a section that holds hook lines
  after the cut (FUT01 seed 40 at bar 168, seeds 8, 0, 4 and 16 at bars 72, 120, 80 and 96).
- test_gen_styles_song.cpp: the dependents map gains chords → lead (FUT02: chords → arp); perturbLane learns the lead
  kind (its grids' vel and len, the hook block's cell weights). "Sci-fi club: the arp runs 16ths on the chord tones"
  becomes test 19.
- test_gen_harmony.cpp: "the arp's chord tones agree with the chord lane's stacks" moves to a fixture style with an arp
  lane, so arp.cpp stays covered.
- test_gen_styles_feel.cpp: "the arp plays one note at a time" holds for every lead lane.
- test_gen_content.cpp: the kits' channels per MIDI v2 (PR 1).
- test_gen_export.cpp, test_gen_styles_export.cpp: track counts and names.
- Unchanged and still required: in key (the lead's off-key notes tagged borrowed), kick and bass within 2 ticks, the
  timing tests, golden_check, both snapshot checks.

**PR 3 (drop 2's bass and the talk)**
- "garage: the bass talks": CC74 every 60 ticks in drop 1, the groove after drop 1 and drop 2 but its last 8 bars; per
  8th 0, d/2, d, d/2 with d = 82, 114, 82, 127 by bar of each 4-bar cycle; no CC74 point anywhere else.
- "garage: the warp riff plays drop 2's kick-out bars": in drop-2 bars where the kick rests, the root on the downbeat,
  the key's tonic in octave 2 on 2, 5 and 7 whatever the chord, the dip as two slid notes in each bar, no slide across
  a bar line.
- "drop 2 plays its own bass line" per kit: drop-2 bass bars come from the `drop2` grid; every other bass bar byte-equal
  to before.
- R10 bass at most 30‰ per kit (FUT01 4‰ today); G3 and I3 measured and pinned; G4 bass stays at 500‰ or more.
- The loader's `in`, pulse, endBefore, after-row, key-degree and rested-grid refusals; a CC spec without `in` writes
  exactly today's points.

**PR 4 (the vox reply)**
- It plays only in free half-bars of the mask; the map, texture by texture; R6 complementarity at least 850‰ (written
  851-932‰); the vowel velocities exact; inside the written vox range; never in a build, the intro, the outro or the
  last line before a build or a drop; the mud test against the chords and the lead; the dependents map gains lead → vox
  and chords → vox.

## Gates

**How measured.** test_gen_gates runs `scoreSeeds` on seeds 0-63 per kit, both versions rendered in memory; `groovecore
metrics` gives the same figures on `groovecore gen` output. Today's figures: pr6-int fd2c5b4. Expected: proto3, which
has every change above but the realized texture check, the drop-1 re-realization, PR 3 and PR 4; a stand-in feel; no
lane-grid or section velocity scaling.

| | FUT01 | FUT02 | FUT03 | FUT04 |
|---|---|---|---|---|
| G4 lead on phrase, today | none (fails) | 246/255 | none (fails) | none (fails) |
| G4 lead, expected | 259/259 | 314/314 | 282/282 | 241/241 |
| G5 lead windows, distinct, SD, today | none (fails) | 850, 30, 11.85 | none (fails) | none (fails) |
| G5 lead, expected | 723, 20, 13.04 | 606, 32, 20.15 | 754, 21, 11.69 | 749, 14, 10.61 |
| R10 lead, today | none | 71‰ (296/4188) | none | none |
| R10 lead, expected | 0/3052 | 1‰ (3/3212) | 0/3128 | 0/2996 |
| Failing gates, today | G4 G5 G7 G9 | G9 | G4 G5 G7 G9 | G1 G2 G4 G5 G9 |
| Failing gates, expected | G7 G9 | G9 | G7 G9 | G1 G2 G5 G9 |
| Drop 1 against the grooves, pooled (band 1.10) | 1.111 → 1.161 | 1.237 → 1.278 | 1.132 → 1.146 | 1.217 → 1.256 |
| Drop 1 under its grooves, per seed: under 1.0; under 1.10 | 11 → 5; 26 → 13 | 1 → 0; 5 → 2 | 7 → 6; 23 → 19 | 0 → 0; 10 → 6 |
| Drop 2 thinner than drop 1, per seed (ADR 0023's row) | 29 → 31 | 0 → 9 | 27 → 29 | 27 → 26 |

- **G4**: the lead at 1000‰ on every kit, by construction of the line rule. Bass and chords unchanged in PR 2 (FUT01
  496/721 and 517/775). In PR 3 drop 2's bass lines change the bass only on drop-2 lines; measured and pinned there.
- **G5**: the lead passes on every kit. FUT02's SD rose with the pedal's two tiers (80 and 66) under the top voice's
  100-127. FUT04 keeps failing G5 on its ghost group alone (no ghost windows), the open S1b2d item.
- **R10**: 0-1‰ on every kit, pinned at 30‰. The call's last onset keeps FUT01 and FUT03 clear of the band.
- **Unchanged**: G1, G2, G6 and G9 read chords, drums or the form; G3 and G7 read drums and bass; G10 passes on all four
  (the lead's line changes join the salient changes). G8 gains one lane's swing tier; FUT02's histogram moves from 3/4/5
  swings in 18/31/15 seeds to 3/4 in 30/34 in the prototype, whose stand-in feel lacks the arp's feel lane; the real
  implementation keeps it.
- **Invariants**: I1 in key, the off-key lead notes tagged borrowed; I3 1000‰; the timing tests.
- **ADR 0023's drop rows** are re-measured in the hook's ADR: FUT02's drop 2 is now thinner than drop 1 in 9 of 64
  seeds (above).

**Reported** (prototype, lead; then the written leads):
- R1 lag-4: 785, 772, 785, 717‰. Written 791-1000‰; the plan's 300-700 band is for S1b2d.
- R6 rest median: 438, 250, 375, 625‰. Written 375-812‰; today's arp 125‰.
- R7 step ratio: 328, 106, 336, 276‰ (written leads 127-349‰; FUT02's top voice and pedal against the written arp's
  234‰); bar 1 = bar 3: 1000, 969, 1000, 1000‰ (written 1000‰).
- R11 LHL: 4.21, 1.66, 5.50, 3.63 a bar (written leads 1.8-5.5).
- Lead notes at or under the top sounding chord note: 148, 174 (the top voice 50), 129, 17‰; written 199, 30, 178,
  363‰. In the written register FUT01 and FUT03 read 345 and 332‰.
- R10 for the bass and the vox, R6 complementarity: PR 3 and PR 4.

## Commits

**PR 1, S1b2b-1: the generator's MIDI channels**
1. `test: generator kits follow the MIDI v2 channels` (fails).
2. `content: the generator's bass to channel 1, FUT02's arp to 2, FUT04's rumble to 9` (kit JSON only).
3. `snapshots: the MIDI v2 channels` (gen.sha256 only; every file differs only where it names a channel).
4. `docs: midi-map, generated MIDI follows the channels` (`docs/midi-map.md`: its status line reads Accepted, and one
   line says the generator's files follow the channel table only: they are 480 PPQ, a slide overlaps its next note by
   30 ticks, FUT04's rumble sits on reserved channel 9 until it becomes drum note 35, and Wubansia plays no generated
   chords, since they sit on channel 3).

**PR 2, S1b2b-2: the hook**
5. `test: the hook, failing` (test_gen_hook.cpp; the loader cases; the gate pins; the form exemption; dependents and
   perturbLane; FUT02's arp tests rewritten and moved; the export and both cut checks; test_rng vectors; declarations
   and stubs).
6. `content: the loader reads the hook block and the lead lane` (content.h, content.cpp, every refusal). Shipped
   content is unchanged, so the snapshots still pass.
7. `gen: the hook's material` (hook.cpp: rhythm, register, contour, walk and fit table, redraw, accent shape, the new
   phrase's idioms, response, texture check), tested on fixture styles.
8. `gen: the line plan, the voicing guard and the lead lane` (planHook, lead.cpp, generator.cpp order, the dropout
   clearing, cut.cpp, export.cpp).
9. `content: lead lanes for FUT01, FUT03 and FUT04; FUT02's arp carries the hook` (styles, kits, feels, about strings).
   The hook tests and gate pins pass; the snapshots fail.
10. `snapshots: the hook` (gen.sha256 only, with the reason).
11. `docs: ADR (next free number, 0026 if none lands first), the hook and its motifs` (the decision, the permanent tags,
    the prototype figures, the departures from plan §2.3 and §4.1 and from the written programs); ADR 0023 amended:
    §4's FUT02 groove, break and drop-2 rows (the arp now plays the hook's lines: its skeleton in the groove, out in the
    break, the period over the pedal in drop 2) and the re-measured drop rows; ADR 0013 one cross-reference line; the
    ROADMAP's S1b2 row; S1b2-plan step 2 notes. The metrics report on 64 seeds per kit goes in the PR body.

**PR 3, S1b2b-3: drop 2's bass and FUT01's talk**
12. `test: drop 2's bass lines and the talk, failing`.
13. `gen: CC specs by kind, pulses and after-rows; key-relative bass degrees; a bass grid for rested bars`
    (cc.cpp, lanes.cpp, content.cpp).
14. `content: each kit's drop-2 bass line; FUT01's warp riff and talk` (styles).
15. `snapshots: drop 2's bass and the talk`.
16. `docs: the hook's ADR, drop 2's material` (and ADR 0023's open item closed).

**PR 4, S1b2b-4 (after ADR 0016 keeps the vox synth): the vox reply**
17. `test: the vox reply, failing`. 18. `gen: the reply through the half-bar mask`. 19. `content: vox lanes on channel 5
for FUT01-FUT04`. 20. `snapshots: the vox reply`. 21. `docs: the hook's ADR, call and response`.

**Before each PR is called done:** `cmake --workflow --preset release`; ctest, the timing tests in Release; golden_check
with `legacy_expected.json` untouched; both snapshot checks; `groovecore metrics` on 64 seeds per kit against the
ADR's table; the 1,024-seed tests; the macOS arm64 job. No plugin code changes, so no pluginval. Not verifiable here:
how any of it sounds.

## Risks
1. **Sound unverified.** The figures are measured, not heard. The plugin plays generated leads on the kit's multisample
   (no recorded phrase table matches), without the recordings' baked throws. The S1d listen decides.
2. **The guard bends the hook where the generated pads and drones voice their 9th low.** FUT04's drone often voices F#4
   (66) under the blips' G5, and in 44 of its 2,875 chords on seeds 0-63 F#4 with G4, a rub under 68 inside the drone.
   The guard moves 97‰ of FUT04's top notes (150-156‰ in the drone's sections) and rests 1.5% of FUT02's pedal notes.
   Decision 7.
3. **FUT02's drop 2** is thinner than drop 1 in 9 of 64 seeds (none today), and FUT02's pedal sits inside the
   generated pad (174‰ of its notes at or under the pad's top; the written pad rests under the arp). Thinning the pad
   under FULL arp lines is for the retune.
4. **Departures from the written programs** while parts are missing: drop 1 opens on SKELETON in FUT01 and FUT03, and
   FUT04's drops play the blips in every line, until S1b2c's accents and stabs or PR 4's vox. Each goes back as data.
5. **The realized texture check and drop 1's re-realization are not prototyped.** Without them 2 FUT01 and 4 FUT03 drop
   1s in 1,024 seeds had no FULL line, and realized moves over the cap remain (16, 45, 11, 0 in 1,024); tests 6, 7 and
   11 pin both at zero.
6. **FUT02's G5 and G8** are measured with a stand-in feel and without the lane grid's 920 scaling. If perform() moves
   them, the fix is in FUT02's velocity tiers and feel (data).
7. **A new dependency**: the lead reads the chord lanes (the guard). A chords change can move lead notes. Declared in the
   map; the chords never read the lead.
8. **FUT02's roster picks of the arp go silent.** Keeping them keeps every other FUT02 lane byte-identical.
9. **Forced lines.** Drop 1's FULL rule places one in 18 and 13 of 64 FUT01 and FUT03 seeds; r2 forces OUT in 12 and 15.
10. **Test weakening.** One form test exempts the hook and reply lanes; the hook tests pin their placement instead.
11. **The loader grows by about 40 fields.** Each refusal gets a test.
12. **Four PRs, four snapshot regenerations**, each alone; the macOS job must pass each time.
13. **FUT03's drift**: the hook follows the written garage-house keys while the style is future house; its drop-2
    keys arpeggio is replaced by the organ period.
14. **Not prototyped**: PR 3 (drop 2's bass lines, the warp riff, the talk), PR 4 (the vox reply), perform(), the
    loader, export and cut.
15. **The written bar-by-bar call and answer stays out** until S1b2d changes G4's unit for leads; the half-bar call
    stands in (28% of FUT01's playing lead lines).

## Decisions for the user
1. **Channels.** Move the generator's bass to MIDI channel 1, the lead to 2, the vox to 5 and FUT04's rumble to 9, and
   keep the voiced chords on 3. Recommended: yes, as PR 1 on its own. Today generator output routed into Wubansia
   plays its bass on the Lead instrument.
2. **Pitch model.** Key-relative degrees with the avoid-note fit and the voicing guard, as the written lead riffs keep
   one pitch set over i-iv; or the plan's chord-relative re-pitching (an arpeggiator tell). Recommended: key-relative.
3. **Templates.** RIFF for the hook; drop 2's new phrase in each kit's written idiom (the organ period in FUT01 and
   FUT03, the glass period in FUT02, the acid-stab riff in FUT04); no SENTENCE (no written top line is one).
   Recommended: yes; recorded as a plan change in the hook's ADR.
4. **Drop 1's first line.** The written programs open FUT01's and FUT03's drops without the lead, because the accent
   hits (no-vox) or the vox riff (vox) take that line; FUT02's opens on FULL and FUT04's on the skeleton. The generator
   has neither accents nor vox yet. Recommended: SKELETON 2:1 against FULL in FUT01 and FUT03 until S1b2c's accents or
   PR 4's vox land, then the written OUT 2:1 against FULL; the same for FUT04's drop lines 2 and 3 with S1b2c's stabs.
   The alternatives are OUT now (8 bars with nothing on top in 46 of 64 FUT01 drops) or FULL on every drop's downbeat.
5. **Drop 2's own material.** PR 3: each kit's written drop-2 bass line, FUT01's warp riff (key-relative pedal, as
   written) and FUT01's talk only where the written mix talks; drop 2's busier hats in S1b2c. Recommended: yes. The
   alternative for FUT01 is the plan's literal long notes on 1, b2, 5 and b7: no b2 in FUT01's written bass, and round 1
   found the reese undanceable. The growl and the b2 slide wait for the S1d listen.
6. **The vox reply.** Build it after ADR 0016 keeps the vox synth, as PR 4 (recommended), in the written vox ranges. If
   the vox goes, the call's open halves stay as breath until S1b2c's accent synths.
7. **The mud rule.** Recommended: the written-reference rule both ways with each kit's written semitone floor (68, 76,
   69, 78), as planned, with the measured bends; and log the low 9ths in FUT02's pad and FUT04's drone, and the high
   chord tops, for the styles' retune. The alternative gates on the stacks' own thresholds (minor 9th from 60, semitone
   from 64), which moved at most 25‰ of written-like notes in the earlier prototype's mud count.
8. **AUGMENT and SEQUENCE**, which no written hook uses. Recommended: AUGMENT at weight 1 in the pre-drop break's last
   line (FUT01, FUT03), SEQUENCE built and tested on a fixture at weight 0; the S1d listen can raise either.
9. **FUT03's direction.** The hook follows the written garage-house kit while the style is round 3's future house.
   Recommended: accept for S1b2b and retune the style in its own step (ADR 0015).
10. **For the S1b2d gating ADR.** Measure G4 for leads in 2-bar units (the written leads score 4/4, 7/8, 6/6 and 4/4;
    legacy still fails), which would let the written bar-by-bar call and answer replace the half-bar call through
    data; and move R1's lead band and R7's RIFF bands to the written values. Recommended: yes, in S1b2d, with
    `legacy_expected.json`'s G4 values in a commit of their own and your OK.
11. **FUT02's arp in the roster.** Keep it (recommended: no other FUT02 lane moves; its picks go silent), or take it out
    (a review counted 283 non-arp salient changes moving over seeds 0-63).
12. **PR shape.** Four PRs: the channels; the hook; drop 2's bass and FUT01's talk; the vox after ADR 0016. Recommended:
    yes, each with its own snapshot regeneration.
13. **The hook's register in FUT01 and FUT03.** The written register (from F#4 and G4, medians as written; the hook at
    or under the generated chords' top 345 and 332‰ of the time, the written leads 199 and 178‰), or the clearing
    register (from G#4 and A4, medians a 3rd higher; 148 and 129‰). Recommended: the clearing register, since the
    generated chords sit higher than the written ones, until the retune lowers them; it is one number per kit.
14. **FUT02's drop 2.** Keep the arp driving every drop-2 line (recommended: drop 2 thinner than drop 1 in 9 of 64
    seeds, 64 of 1,024), or lighter lines nearer the written PSC- (the period, then BREATH, and a last line of SKELETON,
    the call or OUT): thinner in 10 of 64 and 133 of 1,024. Today: 0 of 64, 23 of 1,024.

## Review notes

The completeness critique, point by point. Each claim was checked against the code, the written mixes and the
prototype (proto2 rebuilt from its source reproduces its summary); the drop-2 hats counts and the CLI cut tests'
hook lines were taken as given.

**Taken**
- 1, FUT02's groove loses the arp: true (proto2 spent `first` on line 0, which the arp's entry keeps silent). `first`
  now goes to the first active line, and FUT02's groove `first` has no OUT (its written "-" is the arp not yet
  entered); test 13. Silent grooves are back to today's 23 of 64 and 344 of 1,024. The fix alone gave 33 of 64.
- 2, FUT02's drop 2 thinner: true (49 of 64). The pedal now plays under the period and the call, and drop 2's menus keep
  the arp driving; thinner in 9 of 64 (decision 14, test 20, risk 3). ADR 0023's drop rows are re-measured above.
- 3, the pedal not under the top voice: true (29% of top notes at or under D5). Top voice from C5, a pedal note only
  under the top note it follows, at most two per gap, none before a bar's first top note, cells from 16th 0; 9.4% now,
  the written 1 in 12; test 19.
- 4, the cut tests: true for test_gen_styles_export.cpp:370 and cut_check.cpp:448-482; both trim the hook entries.
- 5, test 18 against the export: true. Without a hook block neither key is written (test 22).
- 6, FUT02's all-accent cell against the loader: true. The non-accent rule now exempts a pedal style, whose FULL and
  SKELETON differ by the pedal; FUT02's strong onsets are its beats (the written arp's accented G5 is an 11th).
- 7, leaps over the cap: true (the counts reproduce). Register before the walk, no per-note fold, the wrap and the
  period's joins as moves, end-degree fallbacks, capped a', c and twin, capped clash fix and guard, the redraw, FUT04's
  cap 10. FULL-line moves over the cap on seeds 0-63: 1, 2, 1 and 0 (proto2 at 10 or more: 33/347, 87/114, 42/271,
  525/288); test 6 on 1,024.
- 8, mud one way only: true (no minor 9th either way in the written mixes; 2 FUT03 chord notes a minor 9th over the lead
  in proto2's seeds 0-2; the pedal unchecked). Both ways, each kit's written floor, the pedal through the guard; none
  left.
- 9, stale dropouts: true (`arrange.cpp:225`, `export.cpp:187`). An OUT line clears the carrier from its dropouts, r3
  is enforced, the mask comes from the final notes; test 25.
- 10, fallbacks undoing the rules: true. A fixed rule order, marked lines never fall back to OUT, no FULL fallback in a
  withheld kind, cover counted on realized states, every rule checked again, tests on 1,024 seeds and the snapshot
  seeds; the residual (6 drop-1 FULL misses in 2,048) is risk 5.
- 11, the warp riff: true (the WarpRiff's bar over Bm keeps F#2 as a pedal; C#2 on 14 ends at 15.75). Described as
  written, with key-relative degrees for the pedal.
- 12, the talk writes CC74 elsewhere: true (`cc.cpp` writes a range's first point; the loader needs a row per kind). A
  CC spec now names its kinds and writes nothing elsewhere; the talk stops for drop 2's last 8 bars.
- 13, the vox range: true (FUT04 52-62, FUT02 62-81). The reply takes the written ranges.
- 14, "nothing in the no-vox mix": true (FUT01 "12110211", FUT03 every bar; FUT04's lines 2-3 hold the rave stab and
  metal chords). Decision 4 is now conditional, and drop 1 opens on SKELETON until those parts land.
- Missing: drop 2's own material is taken on (PR 3; the hats go to S1b2c); BREATH added for the line-end rest
  (FUT02 5 of 12, FUT03 1 of 12); counter-motifs and blip ratchets listed as deferred; the per-seed drop-1 lift, the
  register share and R10 bass are measured or pinned; the one-line menu, rule order, mask order, `shift` and the busy
  half-bar are specified; test 21 compares FUT02 with a fixture copy of today's style; the reply lane gets the form-test
  exemption; ADR 0023's FUT02 rows and the midi-map line are in the docs commits.
- What would sound written by AI: contours (late, early, twin) drawn per seed; per-kit idioms for drop 2's new phrase
  (FUT04's from its acid stab, FUT02's from its glass); FUT02's sparse cell gone and its pedal capped at two notes per
  gap; FUT01's (and FUT04's) downbeat tier from the written velocities.
- Rules and scope: the channel move is its own PR; the ADR takes the next free number; the prototype caveats (stand-in
  feel, no 920 scaling, G8) are stated, and proto3 now removes FUT02's notes in pulled bars.

**Not taken, or only in part**
- The half-bar FRAGMENT's share (critique, "AI"): kept as the G4-safe stand-in for the written C, at or under the written
  C's share (FUT01 28% against 6 of 12 written lines); the written call waits for S1b2d's G4 unit (decision 10).
- FUT03's drop-2 phrase stays an organ period, not its written keys arpeggio: a chord arpeggio repeats across seeds
  (R10). Noted as a departure.
- The register share: measured with a stated definition (a script beside the prototype), not the critique's (its
  32/11/18/48% and 36/38/59/0% could not be reproduced); the gap it pointed at holds, and decision 13 answers it.
- Zero drop-2 thinning in FUT02: not reached (9 of 64); decision 14 and risk 3 say so.
