# 0014. The club master and the mix targets

**Status:** Proposed, 2026-11-04, for round 3. You decide by ear in listening round 3, where each kit adds a mastered "club" loop next to its dry and wet ones. Until then the targets are the standard's, and the checks bind only loops tagged `club`. Numbered 0014, the next free number (ADR 0013: the plan's ADR numbers only name topics). ADR 0005's "ADR 0014" for the sampler's offline-bounce wait names that topic, not this ADR. Amended the same day by the round 3 finishing pass: the crest checks, the declared cap, the partial band override and the limiter's latency (below).

## Context
Round 2's context loops are mixes, not masters (`docs/plans/S1-modern-sound.md`, the standard):
- They read -14.9 to -17.1 LUFS-I at a PLR of 13.9-15.9 dB.
- Current club records sit around -8 to -9 LUFS-I, with true peaks at 0 to +1.5 dBTP.

You asked for "high quality sound design that sounds like current house/garage/techno". The standard set targets for each kit's genre and labelled each one: measured, derived, convention or ours, with a confidence. It asked for:
- S1, a shared, deterministic master;
- S2, QA for mastered loops;
- S5, its meter in the repo.

This ADR records all three.

Nothing in it has been heard. Every number below was measured, in this session's scratch scripts or in the unit tests:
- the round 2 wet loops (`build/<kit>-r2/`, the same samples `content/samples/<kit>/Loops/` held in round 2, before the round 3 refresh, in that folder's history);
- saw fixtures;
- the test fixture of `tools/sound/tests/test_master.py`.

## Decision

### 1. The master: `tools/sound/master.py`
A deterministic stand-in for the engine's master bus, for loops. The engine's master (S1e) must match it, as the engine's sends must match `sends.py`.

Every stage runs round the loop, because a loop plays round and round:
- causal filters run in from a second of the loop's own end;
- envelopes run until they repeat exactly;
- lookahead wraps.

So the seam joins (`seam` check). A loop mastered from another starting point is the same loop rotated, to -68 dB: the solved drive moves by up to 0.004 dB with the block grid.

| Stage | Settings (`master.Settings`) | Why |
|---|---|---|
| Low cut | 4th-order Butterworth high-pass, 25 Hz | Sub-sonic energy out (12 Hz -25 dB; 41 Hz -0.08 dB). On the round 2 loops it lowers the true peak (FUT01 by 1.48 dB) and masters louder than a 2nd-order cut: +0.63, +0.16, -0.03 and +0.45 LU at 4 dB of reduction |
| Mono | under 150 Hz: the side high-passed, 8th order (`dsp48.mono_below`) | The samples' own rule (ADR 0007), on the mix |
| Glue | stereo-linked, feed-forward, 2:1, 6 dB soft knee, 30 ms attack, auto release; a block rate of 16 samples, an RMS detector over about 5 ms | The standard's "light glue". The auto release has a 150 ms envelope over a 1.5 s one that follows sustained compression: a lone hit releases fast, a dense passage slowly. The threshold is solved so the loudest moment takes 2.5 dB (Illinois, to 1e-4 dB): 1.0-1.7 dB on average on the round 2 loops |
| Drive | solved so LUFS-I hits the target (to 0.01 LU), but never past `max_reduction_db` = 4 dB of peak reduction (the driven true peak over the ceiling: the clipper and limiter together) | The standard's cap (§1.1, convention). A mix too sparse stops at the cap and the report says how far short; it is not crushed |
| Target | the genre's (§2), but never louder than ceiling minus the PLR floor | The default targets leave 0.48 dB over every floor |
| Soft clip | 4x oversampled. Linear up to 1 dB under the limiter's ceiling, then a tanh curve (C1 at the knee) toward 0.5 dB over it. First-order ADAA on the clipped excess only | Clipping first is louder and keeps more punch than limiting first (Alternatives). ADAA on the excess suppresses aliasing; what stays under the knee passes untouched |
| DC clean | 10 Hz zero-phase high-pass (4th order each way), round the loop, after the clipper and again after the limiter | Clipping and limiting asymmetric kick cycles leaves sub-sonic ripple at the kick rate: -44 to -73 dBFS on the 5 Hz DC check (limit -60). Zero-phase, it takes 0.003 dB at 25 Hz, and the trim after it measures 0.000-0.012 dB. Offline only (§5) |
| Limit | true peak, stereo-linked: the 4x peak within a sample of each sample sets the gain each sample needs; that need is held from 60 ms before to 65 ms after, ramped over a 5 ms lookahead (a moving average, never over the need), and released exponentially in dB over 80 ms; to -1.02 dBTP. So the gain starts down 65 ms before a peak: the limiter's latency is the lookahead and the hold, plus the detector's sample and its FIR's 24, 3145 samples (65.5 ms, `master.limit_latency()`, in every report) | The 60 ms hold keeps the gain still across a held low note, so it adds no sidebands (Alternatives). A hold that only looks back fails the alias test on a driven 55 Hz saw (-59.7 / -54.6 dB at 3 / 6 dB of drive) |
| Trim | the true peak, round the loop and as a file, at or under the ceiling | 0.000-0.012 dB measured |

The ceiling is `dsp48.TP_TARGET`, -1.02 dBTP: 20 mdB under QA's -1.0, for the 24-bit rounding. The renderer's one gain then only moves a club loop's true peak to -1.02 dBTP, by a few mdB.

**Settings** come from `master.settings(kit, **overrides)`, in this order:
1. The genre's defaults: the target and PLR floor from `qa.CLUB`. The chain is the same for every genre.
2. The kit module's `MASTER` dict, if it has one.
3. The explicit overrides.

Unknown names are refused. A cap on peak reduction other than the standard's 4 dB is an exception, so it is declared on the club item itself, `qa={'club_max_reduction_db': ..., 'why': ...}`, and the recipe passes the same number to `settings()`: QA compares it with the cap the report says the master ran with (`club_reduction`, §3). The round 3 kits pass it per call, so `master.settings(K)` alone reads 4 dB for them.

**The report**, JSON-ready, goes in the loop's metadata:
- every setting;
- the input's and output's loudness: LUFS-I, short-term max, true peak round the loop and as a file, PLR, PSR;
- the glue's threshold and gain reduction;
- the drive, the clipper's peak reduction and active share, the limiter's gain reduction and active share;
- the total peak reduction and the trim;
- the target: asked, used, reached, how far short.

### 2. The targets: `qa.CLUB`, per genre (the kit's `DIRECTION`)
All read round the loop. The basis labels are the standard's (§0): M measured, D derived, C convention, O ours; confidence H, M or L.

| | UKG / 2-step | Sci-fi club | Future house | Techno | Basis |
|---|---|---|---|---|---|
| LUFS-I | -9 ±1 | -8.5 ±1 | -8 ±1 | -8 ±1 | D from M, plus C · M |
| PLR floor | 7.5 | 7.0 | 6.5 | 6.5 | D · M |
| True peak | ≤ -1.0 dBTP | | | | O, C · H |
| Sub: the louder of 31.5 and 63 Hz, re 1 kHz | +6..+12 | +6..+12 | +6..+12 | +4..+12 | D/C · L–M |
| 125 Hz | +2..+6 | +1..+6 | +1..+5 | +1..+6 | D/C · L–M |
| 250 Hz | +1..+4 | 0..+4 | 0..+3 | 0..+4 | D/C · L–M |
| 500 Hz | 0..+3 | 0..+3 | 0..+3 | 0..+3 | D/C · L–M |
| 2 kHz | -3..0 | -3..0 | -2..+1 | -3..0 | D/C · L–M |
| 4 kHz | -5..-2 | -5..-2 | -4..-1 | -6..-2 | D/C · L–M |
| 8 kHz | -8..-4 | -8..-4 | -7..-3 | -10..-5 | D/C · L–M |
| 16 kHz | -14..-8 | -14..-8 | -12..-7 | -17..-10 | D/C · L–M |
| Side over mid, 125 and 250 Hz | ≤ -10 dB | | | | C · L |

- The 31.5 and 63 Hz bands are read together, as the standard says: the kick's key decides which one holds the sub. FUT04's E1 (41 Hz) sits in the 31.5 Hz band.
- The 31.5 Hz band's own window is not checked. FUT03's tight low end reads -12.8 dB there, the standard's "least certain call".
- Under 120 Hz, the width is the existing `low_side` check (≤ -20 dB).
- Not checked, but in the report and the meters: PSR, the short-term max, the standard's shape checks (the low-mid hole, the top slope) and the punch measures. The band windows already bound the shape. PSR and the punch measures have no calibration yet.

### 3. The checks on a `club` loop (`qa._check_club`)
An item tagged `club` must be a mastered loop. `Item` refuses anything else:
- category `loop` and `loop=True`;
- `hp_hz=None`: a filter after the limiter moves the peaks off the ceiling;
- `level=None`: the master sets the loudness.

`Take.spec` gives QA the genre, the kit's `DIRECTION`. A club loop without one fails `club_genre`.

| Check | Pass |
|---|---|
| `club_loudness` | LUFS-I within the genre's window |
| `club_plr` | PLR ≥ the genre's floor |
| `club_true_peak` | ≤ -1.0 dBTP round the loop. The file's `true_peak` can't see a peak that spans the seam: a pulse centred on it reads 3.1 dB higher round the loop than as a file |
| `club_bands` | every band in its window, but those an override exempts by name. The value is the worst distance outside one, and the detail lists every band, the exempted ones marked |
| `club_low_width` | the worse of 125 and 250 Hz ≤ -10 dB |
| `club_reduction` | the cap the master ran with (its report's settings) is the standard's 4 dB, or the cap the item declares, `qa={'club_max_reduction_db': 6.0, 'why': ...}`, which the recipe passes to `MS.settings`; and the peak reduction stays within it. A raised cap is never a silent pass: undeclared it fails, declared it is listed under Exceptions |
| `club_kick_crest` | every full-velocity kick (the `hits` the recipe gives `master()`) keeps ADR 0011's 4.0 dB floor in the club loop: true peak over the loudest 50 ms in the 200 ms from it |
| `club_crest_loss` | the median crest the master costs the kicks, and the claps (100 ms from each), against its input: the worse of the two at most 1.5 dB (the A&R review's proposal, to be calibrated by ear), or what the item declares, `qa={'club_crest_loss_db': ..., 'why': ...}`. The report also reads each clap in 1-5 kHz, its own band, where a kick under it doesn't count |

Any loop that declares its full-velocity kicks (`Out(top_hits=...)`: every context loop's low end and every club loop) also gets `loop_kick_crest`, the same 4.0 dB floor per kick as the loop plays it. A kick lane clipped flat in a mix fails it though the kick's own file passes the per-file floor: FUT01's first round 3 lane read 3.97-4.19 dB.

These are gates, as the task asked, rather than the standard's report-only first step (S2). The band windows, conventions, can be turned off on a club loop with a reason, `qa={'club_bands': False, 'why': ...}`, or some of them exempted by name, `qa={'club_bands': ('125', '250'), 'why': ...}`, the rest still gated; both are listed under Exceptions. Every limit has a pair of fixtures just inside and just outside it, built at the standard's numbers written out in the test (`tools/sound/tests/test_qa.py`: `STANDARD`, `TestClub`, `TestMasterChecks`, `TestLoopCrest`), so loosening a limit in `qa.py` fails a test.

### 4. The meters: `tools/sound/mixmeter.py`
- **Loudness:** BS.1770-4 integrated, momentary and short-term, built on `qa.py`'s K-weighting and gates.
- **Peaks:** true peak, 4x with dsp48's FIR; PLR and PSR.
- **Octave bands** from one FFT: level re 1 kHz, side over mid, correlation. Also the standard's shape checks.
- **Punch:** the kick crest (`qa.kick_crest_db`), the same crest per hit as a loop plays it (`hit_crest_db`, its true peak read with the loop's own samples round the span, as a loop's true peak is, so a span cut from a loud passage doesn't ring at its edge and over-read), attack time, and the in-loop jump at each onset. The jump compares a hit's loudest 10 ms with the bed before it, so it can't see a peak pulled down onto the hit's own body; the crest per hit can.
- **Lean:** right over left per third octave (`lean_db`): side over mid reads a lean to one side as width.
- **A reference track** (the standard's S4): `python -m tools.sound.mixmeter --reference <file>`, the loudest 16 s of a released record as a file, 44.1 or 96 kHz resampled to 48 kHz for measuring only.

Loops are read round their seam. In file mode, `lufs_i` is `qa.lufs` and `true_peak_db` is `dsp48.true_peak_db`.

Checked against:
- EBU Tech 3341 cases 1-5, within 0.1 LU;
- BS.1770's quarter-rate true-peak case, within 0.05 dB;
- pink and white noise slopes, within 0.15 dB per band;
- the standard's scratch meter, within 1e-11 dB on all 16 round 2 context loops (loudness, true peak, PLR, PSR, bands, width, correlation), when it was written. That meter was scratch and is not kept; the repo's own checks are the cases above, in `tools/sound/tests/test_mixmeter.py`.

### 5. What the engine's master (S1e) takes from this
- The chain and every setting: each club loop's metadata holds them.
- The drive and the glue's threshold are solved offline over the whole loop. The engine uses them as fixed settings.
- The limiter's latency, to report to the host: 3145 samples (65.5 ms), the 5 ms lookahead and the 60 ms hold, which reaches forward as well as back, plus the true-peak detector's reach (`master.limit_latency()`; the report's `latency`). A real-time master that keeps the two-sided hold delays its audio that long. S1e may instead redesign the hold to look back only, with a short lookahead; that changes the sound, so it re-measures aliasing (a back-only hold failed the 55 Hz alias test here) and loudness against this stand-in first.
- The zero-phase DC clean can't run in real time. S1e decides between:
  - (a) a causal 5 Hz DC blocker after the limiter, then a catch limiter: DC -61 to -68 dBFS at 4 dB of reduction, -55 to -60 at 8;
  - (b) a linear-phase high-pass, with its latency;
  - (c) leaving the ripple in real-time playback: -44 to -73 dBFS at 1-5 Hz, below hearing; the DC clean then stays a property of rendered files only.

## Consequences
**The round 2 wet loops through the master**, default settings:

| | FUT01 | FUT02 | FUT03 | FUT04 |
|---|---|---|---|---|
| LUFS-I reached (target) | -9.86 (-9) | -11.24 (-8.5) | -12.72 (-8) | -13.29 (-8) |
| PLR | 8.84 | 10.22 | 11.70 | 12.27 |
| Glue, max / mean (dB) | 2.50 / 1.02 | 2.50 / 1.01 | 2.50 / 1.74 | 2.50 / 1.49 |
| Clipper / limiter (dB) | 2.81 / 1.22 | 2.66 / 1.35 | 2.94 / 1.07 | 2.77 / 1.23 |
| Kick jump, median (dB): dry → mastered | 9.2 → 8.4 | 12.0 → 11.2 | 9.5 → 8.5 | 10.9 → 10.4 |
| Clap jump in 1-5 kHz (dB): dry → mastered | 13.7 → 13.4 | 11.4 → 11.3 | 10.1 → 10.0 | 17.4 → 17.4 |
| Side over mid under 120 Hz (dB) | -50.4 | -57.6 | -67.3 | -79.0 |
| Peak reduction to reach the target (dB) | 5.1 | 7.6 | 9.7 | 10.6 |
| …and the kick / clap jumps there | 8.5 / 13.1 | 11.0 / 10.9 | 7.8 / 9.0 | 9.6 / 16.6 |

- **The mixes still need density.** Only FUT01 lands in its loudness window (at -9.86) under the default cap. The peak reduction each needs is between the standard's two scratch projections: a plain limiter at 8.3, ≥9.9, ≥11.9 and ≥12.3 dB, and buses clipped first at 5.1, 6.3, 7.3 and 8.2. Its order holds.
- **The other club checks.** PLR and the true peak pass on all four. `club_bands` fails on all four, by 5.8, 2.2, 7.1 and 5.4 dB at the worst band: the low-mid holes, FUT03's dark top, FUT02's 2-4 kHz. `club_low_width` fails FUT01 (-5.5 dB at 250 Hz). These are the standard's findings, now gated.
- **What a master costs the punch.** At the default cap, the kick's in-loop jump drops 0.5-1.0 dB and the clap's 0-0.3 dB. At the reduction the targets need today, up to 1.7 and 1.1 dB, inside the standard's "at most 3-4 dB".
- **The test fixture** is a 2-bar club loop at PLR 11.2 dB:
  - At the default cap it reaches -9.00 LUFS (3.34 dB of reduction) and -8.50 (3.95 dB), and stops 0.46 LU short of -8.
  - With a 6 dB cap it reaches -7.99 (4.58 dB).
  - The PLR comes out at 7.98, 7.48 and 6.97.
  - At -24 LUFS in, mastered to -8, its balance moves at most 0.17 dB in any octave.
- **Aliasing on driven saws.** Saws driven 3 and 6 dB into the clipper and limiter read, between their harmonics:
  - 55 Hz: -68.3 / -71.4 dB;
  - 220 Hz: -103.4 / -99.8 dB;
  - 1760 Hz: -88.8 / -86.5 dB.

  The alias check's limit is -60 dB.
- **The per-channel clipper puts a little side back under 120 Hz** where a wide top meets a clipped kick. It reads -43 to -50 dB on the fixture and -42 to -79 dB on the round 2 loops, against QA's -20 (Alternatives: what fixing it costs).
- **Same bytes.** The same input and settings give the same bytes, in the same process and in a fresh one (`TestDeterminism`). Like every master here, that holds with ADR 0007's pins. Because the engine's master must match this chain, its output is pinned too: the test fixture mastered at two settings, hashed as 24-bit PCM on x86_64 Linux, with the chain's constants outside `Settings` and how `master()` hands each setting to its stage (`TestTheChainIsPinned`). A deliberate change to the chain re-pins them in its own commit.
- **Time.** 1.7-2.2 s per 7.2-7.5 s loop when the cap binds, 5-7 s when the drive is solved. The new tests add about 45 s to the suite.
- **Not verified:** how any of it sounds. That covers whether -8 LUFS reads as punchy or crushed, whether 3 dB of soft clipping on the kicks is clean, whether the glue glues, and whether a kit that meets the band windows sounds current. All of it is for listening round 3. The band and width targets are conventions until measured references replace them (the standard's S4).

**Round 3, after the fix pass** (the four kits' club loops as rendered, `build/<kit>-r3`; the crest per hit against each master's input, kicks over 200 ms, claps over 100 ms):

| | FUT01 | FUT02 | FUT03 | FUT04 |
|---|---|---|---|---|
| Cap (dB) | 4 | 4.25 (declared) | 5 (declared) | 6 (declared) |
| LUFS-I (target) | -9.70 (-9) | -9.41 (-8.5) | -8.46 (-8) | -8.04 (-8) |
| PLR | 8.68 | 8.39 | 7.44 | 7.02 |
| Kick crest, median in -> out (dB) | 7.18 -> 5.68 | 7.35 -> 5.89 | 7.94 -> 5.72 | 8.89 -> 6.57 |
| Lowest kick crest in the club loop (dB) | 5.63 | 4.27 | 5.22 | 6.03 |
| Median crest loss: kicks / claps (dB) | 1.52 / 1.54 | 1.47 / 1.53 | 1.86 / 1.97 | 2.33 / 2.25 |
| ...the claps in 1-5 kHz (dB) | 0.14 | -0.13 | 0.22 | 0.29 |
| Declared crest loss (dB) | 1.6 | 1.6 | 2.0 | 2.4 |

- **Even the standard's 4 dB cap costs 1.5 dB.** FUT01 masters at the standard's cap and its snares still lose 1.54 dB of crest (median), over the proposed 1.5: the limit needs calibrating by ear, so every kit declares its measured loss, rounded up, with its reason.
- **The clap's own transient survives.** In its own band (1-5 kHz) each clap keeps its crest within 0.3 dB through every master; what the master flattens is the kick and the low end under it, in the four-on-the-floor kits the kick under the clap.
- **The raised caps are your call.** At 4 dB FUT02 stops at -9.61 LUFS-I (0.11 LU under its window), FUT03 at -9.23 and FUT04 at -9.55 (both outside). Each raised cap costs crest: 1.86-1.97 dB at FUT03's 5, 2.25-2.33 at FUT04's 6. If the kicks sound flattened, the caps come down and the loudness windows (or the mixes' density) give way.

## Alternatives
All measured on the round 2 wet loops unless a fixture is named, with scratch versions of the chain that differ from the module only in the stages not being compared.

- **A plain limiter, no clipper** (the standard's projection): 8.3 to ≥12.3 dB of reduction to the targets.
- **The clipper's ceiling 2 dB over the limiter's** (a 2 dB knee), so the limiter does more. It was 0.05-0.48 LU quieter at 4 dB, with the kick and clap jumps up to 1.1 dB lower.
- **A causal high-pass after the limiter** for the DC. It raises the true peak 0.5-3.3 dB (5-25 Hz, 2nd-4th order): the peaks sit at the ceiling, and any phase shift lifts them.
- **That high-pass, then a second, catch limiter.** DC -61 to -68 dBFS at 4 dB of reduction, but -55 to -60 at 8 dB, failing the DC check. Kept as the engine's option (a).
- **A 0.5 ms hold** in the limiter. The gain ripples from cycle to cycle on a held low note, because each cycle's detected peak differs: 55 Hz and 220 Hz saws read -54 and -51 dB between their harmonics. A 20 ms hold reads -59 and -72, a 60 ms hold -68 and -103. From 20 to 60 ms the hold costs 0.03-0.27 LU at the same reduction, with jumps equal or up to 1 dB better.
- **No ADAA** at 4x: a 3520 Hz saw reads -65.4 / -58.9 dB, 7040 Hz -64.5 / -54.6 (3 and 6 dB). ADAA on the excess gives -85.3 / -83.2 and -84.3 / -77.7. ADAA on the whole signal suppresses the same aliases, but averages every sample: -0.42 dB at the 16 kHz band of white noise under the knee.
- **Clipping mid and side separately.** The side under 120 Hz drops to -72 to -84 dB, but the loop is 0.1-0.8 LU quieter at 4 dB (0.7-2.0 at 8): the limiter takes what M/S clipping leaves on L and R.
- **A second mono stage after the clipper.** The side under 120 Hz drops to -64 to -96 dB, but the loop is up to 1.5 LU quieter, and the side's low-mid phase rotation changes the waveform by -9 to -13 dB.
- **A 90 Hz high-pass in the glue's sidechain.** -0.20 to +0.06 LU, no consistent gain.
- **A 2nd-order low cut.** Quieter on three of four loops (Decision, the low cut row).
- **A different chain per genre.** No measurement asks for one. A kit sets its own through `MASTER` or per call, and the report records it; a raised cap is declared on the item (§1).
- **Report-only QA first** (the standard's S2). The task asked for checks. The only conventions among the gates, the band windows, can be turned off per loop with a reason. The loudness and PLR windows can't: the master controls them.

## Sources
- `docs/plans/S1-modern-sound.md`: the standard (§0 labels, §1 targets, §2 measurements, §3 S1, S2 and S5).
- ITU-R BS.1770-4; EBU Tech 3341.
- ADR 0007 (the ceiling, oversampling, `mono_below`), ADR 0009 (QA, the DC and low-side checks), ADR 0011 (the kick crest).
- `tools/sound/master.py`, `tools/sound/mixmeter.py`, `tools/sound/qa.py` (`CLUB`, `_check_club`), `tools/sound/item.py`.
- Tests: `tools/sound/tests/test_master.py`, `test_mixmeter.py`, `test_qa.py` (`TestClub`).
- The design measurements of 2026-11-04 (DC, aliasing, the hold, the clip split, M/S, the low cut, the round 2 loops through the master). They are scratch scripts that read the round 2 renders, and they are not in the repo.
