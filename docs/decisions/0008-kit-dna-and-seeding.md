# 0008. Kit DNA and per-sound seeding

**Status:** Accepted, 2026-10-29, in S1a.

## Context
A kit must sound like one instrument family, not a folder of unrelated presets: the S1a gate asks "one kit?" of every sound (plan §7). The plan gives each kit a DNA (§2.1): one set of inharmonic ratios, one voice blend, one space. Every sound must also render the same bytes on every run in the pinned environment (plan §5.2): the same pins, numpy SIMD level and OpenBLAS kernel (ADR 0007). Several designers write recipes in parallel, so a sound can't depend on what rendered before it.

## Decision
**Kit DNA lives in one module per kit**, `tools/sound/kits/<kit>.py`. For FUT01 "Alloy Signal" (`kits/fut01.py`):

| Name | Value | Used by (S1a) |
|---|---|---|
| `BPM`, `BEAT`, `STEP` | 134, one beat, one 16th | grids: the robot slices, phrases, the fill, the riser's 8 bars, the context loop |
| `SWING` | 0.57: odd 16ths land 0.14 of a 16th late (15.7 ms) | every lane of the context loop and the pluck phrase, so hits that share a step land together (plan §2.3) |
| `KEY`, `CAMELOT`, `SCALE` | F# minor, 11A, F# G# A B C# D E | every pitched sound |
| `ROOT`, `KICK_NOTE` | F#2 (42), F#1 (46.25 Hz) | the kick and every sub end on F#1 |
| `ALLOY` | 1.0, 1.483, 1.932, 2.546, 3.011, 3.715, 4.468 | the kick's click and ring, both snares' FM and modes, both hats, the alloy hit and its short take, the reese's two combs, the growl's second comb, the pluck's tine, the robot's ring modulator, the fill and the riser; the resonator stand-in in the wet context loop (the engine's Alloy resonator send in S1e) |
| `VOICE` | `bf_emma` 0.6 / `af_nicole` 0.4 | every Kokoro take |
| `SPACE_SEED` | `FUT01/space` | every room and FDN: snare B's room, the wet robot, the context loop's Space and Echo |

**One module per instrument family** in the kit's own recipe package, `tools/sound/recipes/<kit>/` (FUT01: `recipes/fut01/`), each exposing `ITEMS`. The registry (`tools/sound/kits/__init__.py`) collects them in module-name order and refuses duplicate names, so designers never edit the same file. A kit's recipes import only their own kit and the shared tools.

**Seeding.** All randomness comes from `dsp48.rng(*key)`: a numpy PCG64 generator seeded with the first 8 bytes (little-endian) of the SHA-256 of the key's parts joined by U+001F.
- A take's own noise is `take.rng(*extra) = rng(kit, sound, layer, rr, *extra)`. The extra keys give a recipe independent streams ("click", "ring", "side").
- The room is `fut01.space_rng(*extra) = rng("FUT01/space", *extra)`.
- Never Python's `hash()` (salted per process) and never global random state.
- So a take renders the same bytes whatever ran before it, in any order and in any number of processes.
- Where round robins or layers must match, a recipe may share a stream across takes. The alloy hit and hat A use the same mode and modulator phases on every take (random phases moved their round robins by 1.4 and 1.6 dB); both layers of a pluck zone pluck the same noise, so only velocity makes the hard layer brighter.

**Kokoro takes** are seeded the same way: `onnxruntime.set_seed` with a seed of the line, voice, options and take number, and a fresh one-thread session per take (ADR 0007). A line's take number is chosen once by ear-proxy (Whisper) and fixed in the recipe (`SUNG_DRAW` in `recipes/fut01/vox.py`). onnxruntime's kernels pick their instructions per CPU, so the same seed gives the same take on one render environment, not on every CPU: vocal masters are hashed per render environment.

**Musical rules that come with the DNA:** zones every 3 semitones; nothing under ~100 Hz except kicks and basses (ADR 0009); lyrics original and plain, in the kit's key.

## Consequences
- The same kit, key, BPM and options give the same bytes with the same pins, numpy SIMD level and OpenBLAS kernel: rendered twice, the S1a set hashes the same in one process and in four, and with every item rendered again in a fresh process (`--repro`). CI re-renders two instruments against `tools/sound/tests/masters.sha256`.
- A new kit is a new `kits/<kit>.py` with new DNA and its own recipes; "new sound sets" in ROADMAP M5 are new DNA, rendered offline.
- Changing a DNA value, a seed key or numpy's version changes the sound. Such a change re-renders the kit and gets its own commit, like the generator snapshots.
- numpy is pinned (`tools/sound/requirements.txt`): its PCG64 streams are stable across platforms, but a version bump is checked by re-rendering before it is merged.

## Alternatives
- **`random.seed(n)` per file or numpy's global generator.** Rejected: the result depends on render order and on which items run in a process.
- **Python's `hash()` of the name.** Rejected: salted per process.
- **A DNA per instrument.** Rejected: sounds made from unrelated material don't read as one kit, which is what S1a tests.

## Sources
- Plan `docs/plans/S1-futuristic-slice.md` §2.1 (kit DNA), §5.2 (reproducibility), §7 (the S1a gate).
- `tools/sound/kits/fut01.py`, `tools/sound/dsp48.py` (`seed`, `rng`), `tools/sound/item.py` (`Take.rng`), `tools/sound/vocals.py` (`Kokoro`), `tools/sound/recipes/fut01/drums.py` (`alloy_hit`).
