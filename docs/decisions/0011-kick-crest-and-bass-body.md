# 0011. Kick crest floor and the bass-body rule

**Status:** Proposed, 2026-10-29. You decide by ear in listening round 2: every kit's bass comes as A "clean" (the strict mud gate) and B "warm" (the bass-body rule below). Until then the strict gate stays the default for basses, and bass-body is an opt-in that the report lists as an exception.

## Context
Round 1 (`docs/listening.md`) gave two notes that QA should turn into rules:
- **Kick.** You picked A "Split" (Cool 4). Of B "Tight" you said: "B sounds a bit hard like its really slamming the limiter".
- **Reese.** Cool 2: "reeses needs the most work of all". In the context loop: "Reese doesnt sound that musical and doesnt make me want to dance". To pass the strict gate (150–400 Hz at least 20 dB under 30–120 Hz after 80 ms), its mids start at 450 Hz behind a 96 dB/oct high-pass. That leaves 120–450 Hz empty, so the bass is hollow and loses its body on small speakers.

The strict gate came from a kick: the round-0 kick's distortion rang in 150–400 Hz for 771 ms (ADR 0009). On a bass it forbids the bass's own harmonics. A plain saw fails it on every note, by 12 dB on F#1 and by 17 dB on F#2.

All numbers below were measured on the S1a render (`build/s1a-listen`, the same bytes as HEAD) and on test fixtures, 2026-10-29.

## Decision

### 1. Kick crest floor (category `kick`; the 2-step boom too)
**The measure.** True peak (4×) over the RMS level of the loudest 50 ms of the mono sum (`qa.kick_crest_db`). A limiter or a clipper pulls the peak down onto the body. A sine reads 3.01 dB.

**The rule.** On the top velocity layer (every round robin and zone), at least **4.0 dB**: the transient must stand at least 1 dB above a sine's own crest. Softer layers are not checked, because a soft kick layer is close to a pure sub sine by design (KickSplit v1 reads 3.2–3.3). A boom with no transient may turn the floor off with `qa={'crest': False, 'why': ...}`.

**What the four requested measures showed.** On every one of them, KickTight reads *more* crest than KickSplit, at every layer. Tight is not limited: its transient stands about 4.5 dB over a sub plateau that holds −4.4 to −5.5 dB from 30 to 100 ms. Split's peak is its own sub, within 2 dB of the peak from 30 to 60 ms. No crest floor that passes every Split file can fail Tight.

| dB (rr1/rr2) | v1 | v2 | v3 | v4 | v5 |
|---|---|---|---|---|---|
| Split, true peak − LUFS-I (PLR) | 9.89/9.99 | 10.70/10.73 | 11.46/11.22 | 11.36/11.56 | 12.23/11.65 |
| Tight, PLR | 12.66/12.60 | 13.27/13.34 | 13.31/13.20 | 13.44/13.52 | 13.13/14.08 |
| Split, crest of the first 60 ms | 4.71/4.81 | 5.66/5.87 | 6.64/6.39 | 6.85/7.17 | 7.88/7.52 |
| Tight, crest of the first 60 ms | 7.67/7.63 | 8.49/8.48 | 8.55/8.64 | 8.90/8.79 | 8.79/9.60 |
| Split, crest of the whole hit | 9.29/9.40 | 10.13/10.25 | 10.99/10.73 | 11.03/11.23 | 11.98/11.45 |
| Tight, crest of the whole hit | 11.28/11.22 | 11.91/12.16 | 12.11/11.98 | 12.09/12.00 | 11.87/12.83 |
| Split, true peak over the loudest 50 ms (the rule) | 3.17/3.28 | 4.22/4.28 | 5.20/4.98 | 5.32/5.49 | **6.44/5.82** |
| Tight, true peak over the loudest 50 ms | 7.44/7.39 | 8.22/8.37 | 8.31/8.26 | 8.33/8.45 | **7.93/8.87** |

True peak minus the maximum momentary loudness (400 ms) equals PLR on these files: the kicks are 0.33–0.46 s long, so they make one block. PLR and the whole-hit crest mostly measure how long the sub lasts: Tight's sub falls 20 dB by 220–225 ms, Split's by 234–266 ms.

**Why this measure.** To ground "limited", each top layer was driven into a brickwall limiter (1 ms lookahead, 50 ms release) and into a hard clipper:

| v5, rr1/rr2 (dB) | as rendered | limiter +2 | +3 | +6 | +12 | clipper +3 | +6 |
|---|---|---|---|---|---|---|---|
| KickSplit | 6.44/5.82 | 5.08/4.63 | 4.44/4.40 | 3.50/3.46 | 2.80/2.83 | 3.60/3.03 | 2.08/1.79 |
| KickTight | 7.93/8.87 | 7.11/7.63 | 6.41/7.33 | 4.20/5.03 | 3.06/3.30 | 5.51/6.43 | 3.09/3.84 |

Of the measures, peak over the loudest 50 ms falls fastest under limiting. It keeps falling under heavy limiting, where the 60 ms crest flattens out (Split: 5.81 at +12 dB). It also avoids LUFS-I's gating jumps on 0.5–0.8 s one-shots (ADR 0009). It is also what "limited" means: the peak pressed onto the body. The 4.0 dB floor passes every Split top-layer file by 1.8 dB. It fails Split slammed 6 dB into a limiter (by 0.5 dB) or clipped by 3 dB, and it passes the test fixtures' clean kick (4.39–4.83). Tight passes it too, and it still passes driven 6 dB into the same limiter (4.20/5.03): a spikier kick starts higher, so it fails only from about 9 dB of limiting (3.13/3.45, the review's limiter, 2026-10-29). The floor catches a kick flattened onto its body, whatever its shape, but it does not encode round 1's complaint, which was about Tight.

**Open, for round 2.** What Tight's hard layers have that Split's don't is a spike: their peak stands 7.4–8.9 dB over the loudest 50 ms, against 3.2–6.4 for Split. If "slamming the limiter" meant "hits hard enough to slam a bus limiter", the rule would be a crest *ceiling*. About 7 dB on the top layer would fail Tight v5 and pass Split v5 by 0.6 dB. It is not proposed yet: it would also bind the "clear click" planned for FUT03's kick. Your round 2 ratings of the new kicks decide whether it is needed. Meanwhile the report's crest table shows every kick's reading, so a designer sees how far a new kick sits from Tight (7.9); whether round 2 also flags readings over about 7 dB as a report-only warning is the lead's call.

### 2. The bass-body rule: `qa={'mud': 'bass-body'}`
This is for sustained and looped basses, bass shots and bass phrases: `Item` accepts it on the categories `bass` and `loop` only, and only with a reason (`qa['why']`), since it is an exception the report lists. Kicks, toms and booms keep the strict gate: `Item` refuses any other mode on a `kick` or `tom` (`'bass-body'`, `'lowcut'` or `'off'`), with or without a reason. The rule keeps the intent of "no mud": no ringing low mids, nothing in the low mids around the kicks, no low mids without a sub under them. But it lets a bass's own harmonics give it body, up to what a plain saw has on the same note. Bands, windows and the sub floor are the gate's (ADR 0009).

| Check | Pass |
|---|---|
| `body_over_saw`: every spectral peak in 150–400 Hz against a saw's roll-off (−6 dB/oct) from the sound's own strongest partials in 30–120 Hz; long-term spectrum, Welch, Hann, 4× zero-padded | ≤ **+5 dB** |
| `body_vs_sub`: 150–400 Hz against 30–120 Hz, every 20 ms window after 80 ms (the sub floor as the gate): the low mids at least 2 dB under the sub | ≤ **−2 dB** |
| `body_share`: 150–400 Hz share of the sound | ≤ **−5 dB** |
| A phrase that declares the kicks it plays against (`Out.hits`): in every gap, from 80 ms after the kick to 20 ms before the next, the same windows (`loop_body_vs_sub`) | ≤ −2 dB |
| …and 40–15 ms before each kick, re the 150–400 Hz peak (`loop_mud_before_hit`, the gate's rule unchanged) | ≤ −30 dB |
| …and `body_duck`: the mean 150–400 Hz power over each kick's first 80 ms against its mean over the gap after that kick (the bass's own level there; a gap more than 30 dB under the loudest gap counts at that floor). Not checked when the kicks are in the file: tag `with_kick` | ≤ **−4 dB** |
| A bass shot (one-shot): the groove loop at the kit's tempo (its `GROOVE`), with the gap limit above and the gate's before-hit rule | as above |

The two falls (150–400 Hz and 400 Hz–1 kHz falling 30 dB) don't apply, because a held bass note doesn't decay. Low mids that outlast the note fail `body_vs_sub` instead, through the sub floor.

**A detuned stack needs a steady sub under it.** `body_vs_sub` reads the low mids against the sub window by window, so a sub that beats fails it: a stack's detuned fundamentals cancel now and then while their upper harmonics don't. A raw 3-saw stack (±12 cents, low-passed at 500 Hz) reads +4.3 on F#1 and +20 to +21 dB on C#2 and F#2; the same stack high-passed at 120 Hz over a mono sine sub reads −15.0, −12.4 and −7.7, and passes (review, 2026-10-29). So under bass-body, a reese keeps one mono sine (or undetuned) voice under 120 Hz, as FUT01's does under the gate.

**Why relative to a saw.** A plain saw's low mids depend on the note: 150–400 Hz holds harmonics 4–8 of F#1 but harmonics 2–4 of F#2. So no absolute ratio passes saws across a bass register without also passing muddy basses:

| Plain band-limited saw | E1 | F#1 | G1 | A1 | C#2 | D2 | E2 | F#2 | G2 |
|---|---|---|---|---|---|---|---|---|---|
| 150–400 vs 30–120 Hz, worst window (dB) | −7.2 | −8.0 | −8.3 | −5.8 | −4.9 | −4.9 | −3.5 | −3.3 | −3.6 |
| share (dB) | −10.2 | −10.0 | −9.8 | −8.7 | −8.7 | −8.3 | −6.6 | −6.2 | −6.3 |
| over the saw's roll-off (dB) | 0.0 | 0.0 | 0.0 | 0.0 | 0.0 | 0.0 | 0.0 | 0.0 | 0.0 |

The suggested −8 to −10 dB would fail a plain saw on A1 and C#2, two of FUT01's own three Reese zones, and on every octave note of a bassline. Meanwhile some muddy basses read lower than an honest saw on F#2. The window and share limits are therefore set from the top of that register (saws up to G2, 98 Hz, pass both by at least 1.2 dB), and the note-aware check carries the main load.

| Sound (2 s, mono) | windows (dB) | share (dB) | over the saw (dB) | Result |
|---|---|---|---|---|
| Saws low-passed (4th order) at 200 / 300 / 500 Hz, on F#1 | −12.2 / −9.1 / −8.0 | −13.9 / −10.7 / −9.8 | −1.8 / −0.1 / 0.0 | pass |
| The same on F#2 | −7.1 / −4.3 / −3.4 | −8.4 / −6.2 / −5.7 | −1.8 / −0.1 / 0.0 | pass |
| Warm: a sine sub under saw mids high-passed at 200 Hz | −13.6 | −14.9 | −4.0 | pass |
| Warm: tanh(2 × saw); FM, ratio 1, index 2.5; a square | −10.5; −9.2; −11.1 | −12.2; −11.6; −12.7 | −3.2; −3.6; 0.0 | pass |
| Gently resonant low-pass: SVF at 250 Hz q 1.5; at 600 Hz q 3 | −5.6; −6.3 | −7.8; −9.1 | +3.0; +3.5 | pass |
| A broad +6 dB "warmth" boost at 200 Hz (Q 1) on F#1 | −3.6 | −6.6 | +4.3 | pass |
| **Muddy**: saw F#1 + 12 dB at 250 Hz (Q 2) | −0.1 | −4.1 | +10.0 | fail |
| **Muddy**: saw F#1 + 9 dB at 250 Hz (Q 4) | −4.1 | −6.8 | +6.2 | fail |
| **Muddy**: dark saw (low-pass 300 Hz) + 10 dB honk at 220 Hz (Q 3) | −2.9 | −5.7 | +8.2 | fail |
| **Muddy**: resonant low-pass parked at 250 Hz (q 3) | −1.7 | −4.9 | +8.1 | fail |
| **Muddy**: the 250 Hz bump on F#2; +8 dB at 300 Hz on C#2 | +3.0; −0.2 | −2.5; −5.1 | +9.2; +7.1 | fail |
| **Muddy**: a sub note gone by 0.35 s whose driven low mids ring on (tau 0.5 s) | +6.1 | −18.7 | −5.7 | fail (windows) |
| The round-0 kick under bass-body | +0.5 | −6.2 | +9.7 | fail |
| S1a Reese (6 files) / ReeseTalk / ContextLoopLow | −27.5 to −38.5 / −22.0, −26.4 / per gap −25.0 | −33 to −43 / −41.6, −51.4 / −20.3 | nothing in 150–400 Hz / nothing / −16.5 | pass (hollow) |

**Around the kicks.** `body_duck` reads each kick against the bass's own level in the gap after it, with mean power on both sides, so an N dB duck reads about −N dB on any note. These are bass phrases against 2 bars of 2-step at 134 BPM: a sine sub under a saw low-passed at 300 or 500 Hz, on F#1, C#2, F#2 and G2, resting from 55 ms before each kick, each note starting on the kick (`fixtures.saw_phrase`):

| Under each kick's first 80 ms | `body_duck` (dB) | Result |
|---|---|---|
| No duck: the note held through the kick, or starting on it | −0.0 to +1.1 | fail |
| A steady 3 dB duck | −1.8 to −2.4 | fail |
| A steady 6 dB duck | −4.7 to −5.5 | pass |
| A steady 10 dB duck | −8.7 to −9.4 | pass |
| A sidechain (2 ms ramp, 20 ms hold, 120–250 ms release), 6 / 8 / 10 dB, F#1 and F#2 | −2.9 to −4.2 / −4.3 to −5.7 / −5.8 to −7.4 | fail / pass / pass |
| The bass resting under the kick (40 dB down to 80 ms) | −26 | pass |
| Ducked 40 dB, but the low mids back 40 ms after the kick | −2.7 to −3.0 | fail |

The same duck reads within 0.7 dB from F#1 to G2. A steady duck reads about 0.5 to 1.3 dB short of −N: the gap's mean includes the rest before the next kick, and the bands smear the duck's edges. So the limit asks for about 5 dB held over the kick's first 80 ms, or a sidechain of about 8 dB. A note that ends 50 ms before the next kick reads −41 dB on the before-hit window; one that ends 25 ms before reads −7.6 and fails. So the gate's −30 dB rule already says "leave the kick room".

The first version read the duck against the phrase's 150–400 Hz envelope peak, which rides the band's waveform crest (note and timbre dependent) and takes one loud moment for the whole phrase. The review (2026-10-29) found that one +10 dB low-mid accent mid-gap let an undocked phrase pass (−10.8 against a −10 limit), and that the same 6 dB duck read −12.1 on F#1 but −9.4 on F#2, failing. Under the current measure the accented phrase still reads +0.5 and fails, since each kick is read against its own gap.

**Range.** The rule needs the bass's fundamental well inside 30–120 Hz: notes up to G2 (98 Hz), so a line can jump an octave up from the kick's note in each planned key (F#1 to F#2, D1 to D2, G1 to G2, E1 to E2). A plain saw on A2 (110 Hz, at the band's edge) reads −1.5 dB and fails the windows; B♭2 reads +0.8. A part that plays higher belongs under the low cut, like a lead.

### 3. Round 2: A clean, B warm
- Every kit renders its bass as two items: A under the category's strict gate, and B with `qa={'mud': 'bass-body', 'why': 'B, warm: the proposed bass-body rule (ADR 0011)'}`.
- Bass phrases declare the kicks they play against: `Out(y, hits=onsets)`.
- A loop that contains its kicks (a context loop's low end) adds the tag `with_kick`.
- The report lists every bass-body item under "Exceptions" and in its own table.
- You pick A or B by ear, per kit. If B wins, this ADR becomes Accepted, and bass-body becomes the default for the `bass` category in its own commit.

## Consequences
- FUT01's masters are unchanged: the S1a set renders the same bytes as before these rules (133 masters and the 4 two-step loops, all compared). The kicks now show their crest in the report: KickSplit 5.82, KickTight 7.93 (the lowest top-layer file).
- A kick shaped like Split can't ship driven 6 dB into a limiter; a spikier one, like Tight, fails only from about 9 dB. The floor guards against flattening, not against hardness, so it doesn't encode the round 1 complaint about Tight (see "Open, for round 2").
- A warm bass can keep its harmonics down to its fundamental, and still can't carry a low-mid bump, a ringing tail or a note held through the kick.
- Every limit has a fixture pair just inside and just outside it (`tools/sound/tests/test_qa.py`: `test_kick_crest`, `test_body_vs_sub`, `test_body_over_saw`, `test_body_share`, `test_loop_body_vs_sub`, `test_loop_body_vs_sub_of_a_bass_shot`, `test_body_duck`, `test_loop_mud_before_hit`). The measures are pinned too: the crest's window and method (`test_the_measure_is_pinned`), the duck's 80 ms window, its floor and its note-independence, and the accent case (`TestBassBody`). The saws, filtered saws, warm basses and muddy basses above are tests as well.
- Not verified: how either rule sounds. That is round 2.

## Alternatives
- **The suggested absolute ratio, 150–400 Hz at most −8 to −10 dB under 30–120 Hz.** Rejected: it fails plain saws from A1 up, so a warm bass would be hollow again on its upper notes.
- **A note-aware window ratio (each window against a saw on the note sounding then).** It needs every phrase to declare its notes in time, and it reads detuned stacks unevenly. The spectral check gets the same note-awareness from the sound's own partials.
- **A resonance check against the line between a peak's two bases.** Rejected: it reads a high-passed mid layer's knee as a 15 dB "peak". A saw's roll-off from the sub is the trend a bass's harmonics should follow.
- **A crest floor on every layer.** Rejected: soft layers are near-sine by design (3.2 dB), so a floor low enough for them would only catch clipping.
- **PLR or crest of the whole hit.** Rejected: they mostly measure the sub's length, and LUFS-I jumps on 0.5–0.8 s one-shots.

## Sources
- `docs/listening.md`, round 1; ADR 0009 (the strict gate, its method).
- `tools/sound/qa.py` (`kick_crest_db`, `over_saw_db`, `_check_body`, `loop_mud`, `check_loop`); `tools/sound/item.py` (the bass-body and crest rules on items); `tools/sound/tests/fixtures.py` (`clipped_kick`, `kick_crest_at`, `tone_burst`, `saw_bass`, `warm_bass`, `MUDDY_BASS`, `body_tones`, `body_shot`, `saw_with_boost`, `body_phrase`, `saw_phrase`).
- The measuring scripts of 2026-10-29 (kick crest, limiter and clipper calibration, bass body, phrases against kicks). They read the S1a masters and the fixtures above; they are not in the repo.
