# 0009. Sample QA and loudness targets

**Status:** Accepted, 2026-10-29, in S1a. The method changes below (marked *own*) are not in the plan: the S1a PR is where you check them. Amended for the four kits, 2026-10-29: every kit loops its kicks in its own groove, and ADR 0011 (proposed) adds the kick crest floor and the bass-body mode.

## Context
Plan §5.2 lists the checks every master must pass, and the user's rule after the prototypes ("the kicks are a bit muddy, I hate muddy") made low-mid clean-up a gate. Writing the checks as code (`tools/sound/qa.py`, with fixtures of the six prototype defects and their controls) showed where the plan's methods needed a precise definition. A review of the S1a render (2026-10-29) then found limits no test pinned, a low cut measured in the wrong place, and D4 unenforced; this version includes its fixes.

## Decision
**Per file** (`check_take`):

| Check | Pass |
|---|---|
| Format | FLAC, PCM 24-bit, 48 kHz; mono where L = R |
| True peak (4×) | ≤ −1.0 dBTP |
| DC (5 Hz zero-phase low-pass, max; a loop measured as it plays) | ≤ −60 dBFS |
| First sample (except a loop from sample 0) | ≤ −80 dBFS |
| One-shot tail: the last 10 ms | ≤ −70 dBFS |
| Splices the recipe logs: HF (≥ 6 kHz) in 1 ms against the median of the 50 ms around it | ≤ +12 dB |
| Loop seam: the jump against the 95th percentile of the steps around it | ≤ +3 dB |
| Aliasing on tonal sustains: energy between the harmonics of every fundamental present | ≤ −60 dB, unless `deliberate_alias` |
| Stereo correlation | ≥ 0.2 |
| Side under 120 Hz against mid under 120 Hz | ≤ −20 dB |
| Tuning wherever a pitch is declared (0.10–0.60 s; kicks and toms 0.10–0.25 s) | ±3 cents; kicks and toms ±10; snares, hats, vocals and fx unchecked |
| Top band on `hf_synthetic` masters: 14–20 kHz against 6–11 kHz | ≥ −40 dB (D4: a 2x resample of a 24 kHz take reads about −61) |
| Kick crest (kicks, the top velocity layer): true peak over the loudest 50 ms (ADR 0011, proposed) | ≥ 4.0 dB |
| Low end | the mud gate, the bass-body mode (basses, ADR 0011) or the low cut, below |
| Loudness by category | below |

**The mud gate** (categories `kick`, which covers the 2-step boom, `tom` and `bass`; any item can opt in with `qa={'mud': 'gate'}`):
- Measured with 4th-order Butterworth bands, zero-phase, and a 5 ms RMS envelope.
- 150–400 Hz falls 30 dB from its own peak within 120 ms.
- After the first 80 ms, 150–400 Hz stays ≥ 20 dB under 30–120 Hz, in every 20 ms window (10 ms hop). Where the sub has faded more than 30 dB under its loudest window, the window is measured against that floor.
- 150–400 Hz is ≤ −12 dB of the whole sound.
- 400 Hz–1 kHz falls 30 dB within 150 ms.
- On a 2-bar groove loop at the kit's tempo (one-shot kicks, toms and basses, or any item with `loop_check`), every gap between hits must pass two checks. The groove is the kit's `GROOVE`: 2-step (`qa.TWO_STEP`, FUT01) or four on the floor (`qa.FOUR_FLOOR`, beats at velocity 1.0 and 0.8; FUT02–FUT04). Its tightest gap is 3 16ths in 2-step and 4 in four on the floor. In every gap, from 80 ms after the hit to 20 ms before the next, 150–400 Hz stays ≥ 20 dB under 30–120 Hz (the same windows); and 40–15 ms before the next hit, it is ≥ 30 dB under the loop's 150–400 Hz peak.
- A loop file that declares its kicks (`Out.hits`) gets the same two gap checks on itself, in place of the whole-file "after 80 ms" check.
- A `Bass` or `Loops` item must state its low-end rule: `'gate'`, `'bass-body'` or `'lowcut'`, or `'off'` with a reason (`qa['why']`), so a bass loop can't go ungated by default.

**The bass-body mode** (`qa={'mud': 'bass-body', 'why': ...}`, ADR 0011, proposed): for the categories `bass` and `loop` only, with a reason; kicks and toms keep the gate and can't take any other mode. A bass may keep its own harmonics in 150–400 Hz up to a plain saw's level on its note (every peak ≤ +5 dB over a saw's roll-off from its sub partials). In every window the low mids stay at least 2 dB under the sub (≤ −2 dB), and the share is ≤ −5 dB. A phrase that declares its kicks gets the same window limit in every gap, the gate's before-hit rule unchanged, and under each kick's first 80 ms its low mids at least 4 dB under its own level in the gap after that kick (mean power both sides). ADR 0011 has the measurements and the fixtures.

**The low cut** (everything else with low end: pads, stabs, textures, chops, hats, snares, perc, fx): ≤ −24 dB of the sound under 100 Hz (4th-order Butterworth, zero-phase). The recipes high-pass at about 100 Hz, higher for a root on F#2, and keep everything under 150 Hz mono.

**Per instrument**:
- Velocity: loudness rises ≥ 1.5 dB per layer, and the spectral centroid rises.
- Round robins within ±0.5 dB of their layer's mean.
- One gain for every file (no per-file normalising).
- The artist-name and slur denylists on every name and string. They are stored as salted SHA-256 digests, so no name appears in the repo.
- Vocals: exactly one of the `explicit` flag or the `clean` tag; no profanity in a clean line; `hf_synthetic` on TTS-derived masters, each file with its top band (the per-file `hf_band`).
- Whisper base.en on dry vocal files only: score ≥ 0.8 (the share of script words heard, minus 0.3 × the share of extra words). Wet versions fail ASR by design.
- Reproducible: the same SHA-256 rendered twice (`--repro` renders each item again in a fresh process, so no cache carries over; and two full renders). CI re-renders two instruments against committed hashes (ADR 0007).

**Loudness targets** (LUFS-I, BS.1770; mono counts as L = R):

| Category | Target |
|---|---|
| Beds (textures) | −20 ±1 |
| Risers | −14 ±2, rising: each quarter at most 1 dB under the one before, ≥ 6 dB first to last |
| Vocals, wet | −16 ±1 |
| Vocals, dry | −18 ±1 (the chops are set to −18 too, unchecked) |
| Everything else | no LUFS target: one gain per instrument, its loudest true peak at −1.02 dBTP |

**Method choices** (*own*):
1. **A 20 Hz 4th-order high-pass on every take.** Without it, the DC check reads −46 to −58 dBFS on any full-scale sine that starts at phase 0.
2. **`mono_below` uses an 8th-order Butterworth on the side.** An LR4 leaves side content an octave down only 18 dB lower, short of the −20 dB gate.
3. **The side under 120 Hz is measured from FFT bins**, so filter skirts from above can't leak in.
4. **A band's fall ends when it *stays* 30 dB down.** That is stricter than the first crossing used in round 0. On a sustained or looped sound the two falls don't apply.
5. **The 2-step loop is checked per gap.** Measured over the whole loop, even the judge's pick of the de-mud study read only −17.5 dB, because the first 80 ms of every later hit count too.
6. **The low cut measures under 100 Hz, at −24 dB.** The plan says "nothing below ~100 Hz competes with the kick and sub"; the first version measured under 60 Hz, which let a stab on F#2 (92.5 Hz, the sub's second harmonic) pass with no high-pass at all. −24 dB is a starting value: the plan gives no number. A 100 Hz high-pass leaves a 92.5 Hz root only about 15 dB down, so a stab voiced on F#2 needs its high-pass nearer 130 Hz.
7. **The full context loops are not mud-gated.** Vocals, snare and pad belong in 150–400 Hz. Their low end, the kick and reese lanes as mixed, is a file of its own (`ContextLoopLow`), gated in every gap between its kicks.
8. **"Stays under the sub" is checked window by window.** Over the whole tail, a long sub hid a bloom between 80 and 120 ms (it read −21 dB while it was −16.7 dB locally); every 20 ms window must pass now. Two details make the windows mean something: where the sub has faded more than 30 dB under its loudest window, the window is compared with that floor (so a near-silent stretch between notes can't read as mud, and a ring with nothing under it still does); and in a loop the windows stop 20 ms before the next hit, whose onset the zero-phase bands smear a few ms ahead (the before-hit check covers that stretch). A loop that declares its kicks is checked per gap only: each kick owns its first 80 ms, which a whole-file window would count as mud.
9. **D4 is checked, not only flagged.** `hf_synthetic` used to pass on the flag alone; a recipe that resampled a Kokoro take into a master would have passed every check. The top-band check (−40 dB) leaves about 18 dB of margin on both sides.
10. **Every limit is pinned by a pair of fixtures**, one just inside and one just outside it (`tests/test_qa.py`, `TestLimits`), so loosening a limit fails a test. Before, most limits, three of the four mud-gate limits among them, could be loosened a lot with every test green.

**Exceptions**: a flag or an override with its reason in the recipe (`qa['why']`), never a silent pass. `qa_report.json` carries every item's flags, tags and overrides, and `qa_report.md` lists them under "Exceptions". In S1a:
- `GlitchFill`: `deliberate_alias` (the crush).
- `ShiftRiser`: `cut_end` (it ends on the downbeat).
- The growl shots: `tonal: False`. A moving shot has no steady tone. GrowlChew reads −29.6 dB at 4× and −29.4 dB with every stage at 16×, GrowlRise −24.5 dB at both, so it isn't aliasing; held still, the same chain reads −61.3 and −69.8 dB (`recipes/fut01/bass.py`).
- The full context loops: mud `off` (point 7).
- The vocals: `hf_synthetic` (D4), now checked (point 9).

## Consequences
- S1a: 31 items, 133 files, 0 failed checks (`build/s1a-listen/qa_report.md`). The mud gate, worst file per instrument, windowed as in point 8:

  | Item | 150–400 Hz fall | vs 30–120 Hz after 80 ms | share | 400 Hz–1 kHz fall | 2-step gaps: vs sub / before the hit |
  |---|---|---|---|---|---|
  | KickSplit | 59.7 ms | −55.9 dB | −17.9 dB | 46.5 ms | −42.7 / −54.2 dB |
  | KickTight | 57.1 ms | −53.9 dB | −13.8 dB | 67.3 ms | −44.1 / −51.9 dB |
  | Reese (loop) | n/a | −27.5 dB | −33.1 dB | n/a | n/a |
  | ReeseTalk (loop) | n/a | −22.0 dB | −41.6 dB | n/a | n/a |
  | GrowlRise | 38.8 ms | −57.2 dB | −36.5 dB | 39.7 ms | −37.7 / −39.2 dB |
  | GrowlChew | 79.1 ms | −53.6 dB | −36.2 dB | 95.5 ms | −37.7 / −37.6 dB |
  | ContextLoopLow | n/a | per gap | −20.3 dB | n/a | −25.0 / −42.1 dB (11 gaps) |

  The round-0 prototype kick read 771 ms, −2.4 dB, −6.2 dB and 360 ms (the window measure reads +0.5 dB on it).
- The kick crest (ADR 0011), lowest top-layer file: KickSplit 5.82 dB, KickTight 7.93 dB. Both pass, and the S1a masters are the same bytes with the new rules and the multi-kit layout.
- **The windows found two real problems the average hid:** the stand-ins started reese notes over 2 ms, which splattered the sub into 150–400 Hz at every note that doesn't land on a kick (the context loop's low end read −15 dB in two gaps), and a near-silent stretch between reese notes read as mud against a −40 dB floor. The stand-ins now start reese notes over 10 ms, as the engine's legato voice will, and the floor is −30 dB.
- **The 150–400 Hz band's skirt** (4th order, zero-phase) still counts content at 450–550 Hz only 11–23 dB down. That is where the reese's mids start, so the reese's readings are mostly that skirt, not its own low mids (−41 to −48 dB of the bass lane). It is part of the gate as approved; a steeper band would be a method change for S1c to decide.
- **Toms at F#3 and above** will likely fail "20 dB under 30–120 Hz", because their fundamental sits above that band. They have their own category now, gated like a kick; S1c decides their rule, for example the falls and the share only.
- **Open, for S1c:**
  - LUFS-I on one-shots of 0.5–0.8 s jumps by up to 6.6 dB when a 400 ms block sits on the relative gate. That made the velocity check fail at random, and the snares and hat B now fade out by 0.47 s to stay a single block. Ungated loudness for one-shot layers would remove the workaround.
  - `updown3`'s upward lift acts while its detector is still rising from silence. It raised SnareMetal's true peak by about 5 dB, so the snares don't use it yet; snare A takes its crest down with a soft clip instead.
  - The spectrogram sheets show faint 20–50 Hz rows on every sound that the audio doesn't have (below −140 dB after 0.1 s). This looks like an artefact of the sheet; the cause is unconfirmed.
- None of this says how anything sounds. The listening round decides keep, fix or cut (plan §5.6).

## Alternatives
- **The plan's methods read literally** (first crossing, the whole-loop ratio, an LR4 `mono_below`). Rejected: each point above shows a case that fails or passes for the wrong reason.
- **Mud-gate the full context loop.** Rejected: it would fail on the vocals and the snare, which belong in the low mids.
- **One LUFS target for every category.** Rejected: drums would lose their peak headroom, and one gain per instrument already sets the relative levels.

## Sources
- Plan `docs/plans/S1-futuristic-slice.md` §2.2 ("No mud"), §5.2; `docs/listening.md`, round 0; ADR 0005 (the user's rule).
- `tools/sound/qa.py`, `tools/sound/tests/test_qa.py` (the defect fixtures and the boundary pairs), `tools/sound/render.py`.
- The review of the S1a render, 2026-10-29 (the low cut, the window measure, D4, the unpinned limits).
- The design-track reports of 2026-10-29 (drums, bass and music, vox and fx); `build/s1a-listen/qa_report.json`.
