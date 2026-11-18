# 0028. Build in, cut later: options without listening rounds or approval gates

**Status:** Accepted, 2026-11-18. The user's own rule (quoted below).

## Context
Since S1a, work has paused at two kinds of gate: listening rounds (rounds 1 to 5, the v0.1 and v0.2 DAW checklists in `docs/listening.md`) and approval items (the choices ADRs 0016, 0024 and 0025 announced and left waiting for the user's OK). On 2026-11-18, after choosing the four genre looks for the plugin, the user wrote:

> "I am tired of doing listening work; I'd rather have options for myself than do a check on everything each time, I might just have you cut certian things later rather than approving them going in"

Earlier the same day they asked for the plugin to become "a tool thats more for allowing the user to be creative, than giving prepackaged songs", and said that the review stages took too long.

## Decision
1. **New features and content go in as options**, without a listening round or an approval gate. Where it costs little, each one can be avoided or switched off (a switch, a chip of its own, a library entry), so cutting it later is a small change.
2. **The user cuts what they don't want.** A cut is a request like any other: it gets done, and one line in the ADR it touches records it.
3. **Choices that waited on the user stand as built** and can change whenever the user asks:
   - ADR 0016: the wordless vox synth stays, and every Loop and Song still comes without it too (the `NoVox` files).
   - ADR 0024: the Samples drawer's three defaults (dry samples; the kit's own note spelling, with C4 as middle C; the Samples button opens the drawer).
   - ADR 0025: the 1 drum track switch's wording, file-name mark, track name, default (off) and scope (Loop and Song).
4. **Decisions still get an ADR**, written as before. Nobody writes that the user approved an item they didn't: entries say "in under ADR 0028".
5. **`docs/listening.md` and the DAW checklists stay as references** the user can use when they want; they gate nothing. ROADMAP rows from v0.3 on say what the user gets, not what they must check.
6. **Still asked first**, because these are hard to undo or aren't Claude's to change: deleting the user's files or published pages, rewriting git history, changing the legacy goldens, and changes to `CLAUDE.md`. New dependencies are still announced with their licences, and nothing GPL or AGPL is linked into the plugin.
7. **Reviews stay lean**: one engineering review per step and one verifier per finding, then the fixes; no listening review.

## Consequences
- Features reach the user sooner, and the user steers by cutting rather than by signing off.
- Things the user may not like do reach their builds; each delivery lists what changed, so they know what to try and what to cut.
- Engineering quality doesn't depend on the user: tests first, the CI checks, pluginval, determinism and the audio-thread rules apply as before.
