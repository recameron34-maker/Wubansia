# 0010. Four first-listen kits before the full render

**Status:** Accepted, 2026-10-29 (the user: "give me four kits").

## Context
ADR 0005 (D1) put S1 on one kit, FUT01 "Alloy Signal", and left the other directions to S2. Round 1 judged FUT01's 12 S1a sounds. The pluck got Cool 5, the kick, pad, chops, robot and riser 4, and the reese 2. The user then asked for four kits, one per direction. The sound pipeline from S1a (recipes, QA, renders, listening page) is cheap to run per kit. The full render (S1c, about 373 files per kit) and everything after it are not.

## Decision
- **S1a widens to four first-listen kits**, one per direction:
  - FUT01 "Alloy Signal": future garage / 2-step, 134 BPM, F# minor (11A)
  - FUT02 "Orbit Relay": sci-fi club, 130 BPM, D minor (7A)
  - FUT03 "Glass Current": hyper-clean future house, 128 BPM, G minor (6A)
  - FUT04 "Furnace Code": dark industrial / techno, 136 BPM, E minor (9A)

  The names are working names until each is title-checked.
- **Each kit is a first-listen set** like FUT01's: 12 sounds and a 4-bar context loop, dry and wet. It has its own DNA (ratio set, voice blend, space seed, root, BPM, swing), its own kit module and its own recipe package. Shared tools stay shared, so the four kits can be built in parallel. The briefs are in `docs/plans/S1-four-kits.md`.
- **FUT01 gets its round 1 fix round in the same pass:**
  - the bass is rebuilt as A clean / B warm, with new basslines
  - new sung lines
  - a growl and a glitch fill with a clear musical gesture
  - kick B is cut
- **Round 1's lessons bind every kit:**
  - the split kick and the kick crest floor
  - basslines that groove against the kick
  - an A/B on every kit's bass between the strict gate and the proposed bass-body rule (ADR 0011)
  - a signature melodic element
  - natural, title-checked vocal lines
  - shots and fills with a musical gesture
- **Round 2 judges all four**, each against the S1a gate, on one listening page that saves ratings per kit.
- **Nothing after S1a changes yet.** Which kits go on to the full render (S1c), and in what order, is decided after round 2. S1b (the generator) doesn't depend on it.

## Consequences
- S1a takes longer, and round 2 is four times round 1. The listening page groups each kit's sounds so a round can be done one kit at a time.
- The toolkit takes more than one kit: `tools/sound/kits/<kit>.py` and `tools/sound/recipes/<kit>/`. FUT01's masters don't change in the move (its byte manifest is compared before and after).
- S2's plan for future house at 128 BPM and industrial / techno at 136 BPM starts here at first-listen depth. S2 keeps the full-depth work: full renders, style specs, feel templates and the kit switcher.
- The ROADMAP's S1a box covers four kits and closes when round 2 passes for the kits you want to carry on.

## Alternatives
- **Finish FUT01 first, then do the others in S2:** the plan as approved, but not what the user asked for.
- **Four full kits now:** about 1,500 files before any of the new kits has been heard. Round 1 showed that the first listen changes the recipes (the reese, the lines), so full renders before a listen would be redone.
