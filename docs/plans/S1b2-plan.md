# S1b2 Generator, part 2: plan

Source of scope: docs/ROADMAP.md (S1b2 row), ADR 0013 §2 "Reserved for S1b2" and §5 "Open for the S1b2 gating
ADR", plan docs/plans/S1-futuristic-slice.md §4.1 and §5.1. Today (S1b1) on 64 seeds: FUT01 fails G4 G5 G7 G9 G10,
FUT02 G9 G10, FUT03 G4 G5 G7 G9 G10, FUT04 G1 G2 G4 G5 G9 G10 (core/tests/test_gen_gates.cpp pins these).

Steps go one after another on the designated branch, each its own PR into main, the next restarted from main after
the merge.

## Steps (one PR each)
1. **S1b2a Form grammar** (G10; ADR 0013 §2's last two bullets).
   - Per-seed forms on the 8-bar grid: drop and break starts about 70% on 16-bar lines, 30% on 8 mod 16; builds 8-32
     bars; 200-240 bars in all; the 32-bar intro (first 8 drums only) and outro (last 8 drums only) kept, lanes only
     join in the intro and only leave in the outro.
   - Lanes move inside the drops: at least one lane change inside every drop (a lane out for 8 bars, a one-bar
     dropout before a 16-bar line, drop 2 adding a layer drop 1 lacks); drop 1 at least 10% denser than the groove.
   - Gate: G10 (>= 8 distinct form shapes in 64 seeds, top form <= 25%, intervals between salient changes take >= 2
     values), G4 kept. Tests on 64 seeds per style first.
2. **S1b2b Hook and motifs** (G4, G5 for lead lanes; R10).
   - One hook per seed (rhythm first, chord-relative degrees), scheduled: >= 3 sections, once before the first drop,
     >= 50% of drop bars. RIFF / SENTENCE / PERIOD templates, the development operators (REPEAT, VARY, SEQUENCE,
     ANSWER, FRAGMENT, AUGMENT, CADENCE), call and response through a half-bar occupancy mask.
   - FUT02: a per-seed arp motif (R10 lead <= 3% cross-seed duplicate 2-bar motifs). FUT01: the reese drop layer
     (long notes on 1, b2, 5, b7 with slides and talk moves).
   - Notes, 2026-11-16. Planned in docs/plans/S1b2b-plan.md (approved 2026-11-15 with its recommended choices) as
     four PRs; ADR 0026 records them. Done: PR 1, the generator's MIDI channels (bass 1, lead 2, chords 3, FUT04's
     rumble 9); PR 2, one hook per seed on a lead lane in every kit, FUT02's arp carrying it. On 64 seeds G4 and G5
     pass for the leads on all four kits (FUT04 still fails G5 on its ghosts) and R10 lead reads 0, 8, 0 and 0
     permille (FUT02's was 71). Changed from the text above: degrees are key-relative, with an avoid-note fit and a
     voicing guard against the chords, not chord-relative; no SENTENCE (no written top line is one); the PERIOD is
     drop 2's new phrase, in each kit's written idiom; the operators add SKELETON, BREATH and NEW, and SEQUENCE ships
     at weight 0, untested; the half-bar mask is exported in regions.json and nothing reads it yet. Next: PR 3, drop
     2's own bass lines and FUT01's bass talk, in place of the reese layer (FUT01's written bass has no b2). After
     ADR 0016: PR 4, the vox reply through the mask, whose design the mask's fill (ADR 0026, Consequences) sends back
     for a look first. For S1b2d: G4's unit for leads (2 bars), R1's lead band and R7's RIFF bands.
3. **S1b2c Fills, transitions and the rhythm stage** (G9).
   - Earned fills: importance tiers (4, 8, 16 bars, section end), fatigue, no repeat of the last two, dropouts count,
     set-up and pay-off. CC lanes for transitions.
   - Syncopation (LHL) bands per energy level, the displacement and ratchet operators.
4. **S1b2d Gating ADR, parity and the deliverable.**
   - ADR (next free number): the open decisions below. Metric changes they need (G9's rarity rule changes
     core/tests/legacy_expected.json's G9 values: its own commit, with the user's OK).
   - Snapshot parity: macOS arm64 (every PR), plus the dispatched x86_64-under-Rosetta checkpoint.
   - Deliverable s1b-midi: 10 pre-registered seeds per kit as full songs (feel + quantized), docs/midi-map.md, the
     metrics report on 64 seeds vs legacy.

## Decisions for the user (the gating ADR), with recommendations
- G8, swing tiers vs "every lane uses the kit's one swing": keep the tiers (hats/shaker/perc more swung, bass
  straightest) and judge at the S1d listen. Round 1's complaint was the bass following the kick, fixed in round 2.
- G7: measure per track, not per drum group (the hats fail only when merged with an independent shaker). Yes.
- G1/G2 for one-chord (pedal) styles: exempt pedal sections, and gate FUT04 on voicing and bass movement instead.
- G5 ghosts: skip the ghost group for styles that declare no ghost notes (FUT04).
- G4: no ceiling (keep it one-sided).
- G9 rarity rule: a perc key that sounds in most of a section's bars doesn't count as a fill. Yes; changes
  legacy_expected.json's G9 values (own commit + ADR note).

## Rules that hold throughout
- Tests first. Same seed, same bytes on every platform (integers, counter RNG, new tags permanent once released).
- core/tests/legacy_expected.json and content/golden/ never regenerated to pass a test.
- Snapshots (core/tests/snapshots/gen.sha256) change only in their own commit, with the reason.
- 128 bars in under 50 ms (Release).
- Kick and bass within 2 ticks, every section on an 8-bar line, anchors within +-2 ticks, the regions tile the song.
- "Not written like AI": every new rule is a style convention in the data, measured against real tracks or the kits'
  own written MIDI, not a metric chased for itself.
