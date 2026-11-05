# S1: the modern-sound standard for the four kits

*Written 2026-11-04 to prepare round 3 (the modern production pass). It sets the production standard the four kits must meet, measures their round 2 context loops against it, and ranks the changes for each kit. Nothing was rendered or changed for it: it reads the round 2 files.*

You asked for "a cool modern sound", meaning "high quality sound design that sounds like current house/garage/techno", with a high level of attention and detail. This doc turns that into numbers we can check: the loudness, spectrum, low end, width, punch, sidechain, saturation and space of a current club record in each kit's genre. It then measures where the kits stand today.

**Nobody has listened to anything in this doc.** Every statement about sound is either a measurement or a convention. Whether a change sounds better is for your next listening round.

## Summary

Every finding below is measured. The targets they are measured against are in §1; how sure each target is is in §0.

1. **The context loops are mixes, not masters.** They read −14.9 to −17.1 LUFS-I with a peak-to-loudness ratio (PLR) of 13.9–15.9 dB. Current club records sit around −8 to −9 LUFS-I with peaks at or above 0 dBTP, a PLR of about 8–10 dB. Under our −1 dBTP ceiling, that is 6.5–9 dB.
   - A plain limiter would need 8–12 dB of peak reduction to close that gap, and it would cut the kick's jump out of the mix by 4–7 dB.
   - Clipping the buses first still leaves 5–8 dB of limiting. So the gap is mix density (sustain and transient control), not only a missing limiter.
2. **Every kit has a hole in the low mids.** In the dry loops, the 125 Hz and 250 Hz octaves sit 5.7–10.6 dB under a straight line drawn from the 63 Hz octave to the 500 Hz octave. That is the "hollow" sound round 1 heard in the reese: the strict gates and high-passes leave 100–350 Hz thin. The B "warm" basses fill 0.6–3.1 dB of it.
3. **FUT03 ("hyper-clean" future house) has the darkest top of the four** (8 kHz −10.9 dB, 16 kHz −13.4 dB against 1 kHz), in the genre that should be brightest. FUT04 is dark too, at the edge of a techno range.
4. **FUT02 (sci-fi club) is nearly mono above 500 Hz** (side 12–14 dB under mid). It is the narrowest kit, in the genre that most wants width.
5. **The drums are mostly sound.**
   - All kicks pass the crest floor (5.8–6.4 dB), and their attacks hold their peaks.
   - FUT03's kick has the least click (−23 dB), though its brief asks for a clear one.
   - FUT03's clap starts soft (its first hand is 8 dB under the loudest one), has no body (nothing under 400 Hz) and the shortest tail.
   - In every loop the backbeat sets the peak: the clap or snare (with the kick under it, in the four-on-the-floor kits) peaks 0.4–2.4 dB over a kick on its own.
6. **The low end is clean and mono** (side 61–89 dB down under 120 Hz in the full loops).
   - FUT01's kick sub is long for 2-step: −20 dB only after 317 ms, 2.8 16ths.
   - FUT04's E1 kick (41 Hz) puts its weight under 44 Hz. Its 60–125 Hz region sits 14–21 dB under that sub peak, so the kick will be weak on small speakers.

The change lists (§3) put a club master stage and mix density first for every kit, then each kit's measured gaps, then the genre's signature moves.

## 0. How to read the targets

Every target carries a label and a confidence:

| Label | Meaning |
|---|---|
| **M** | Measured: a published measurement of released records, with its sample and method |
| **D** | Derived: arithmetic on an M number, for example a slope converted to octave bands |
| **C** | Convention: practitioner guidance (tutorials, mastering blogs, forum consensus), with no measurement behind it |
| **O** | Ours: a rule or a calibration from this project (an ADR, round 1 ratings, or our own measurements) |

Confidence: **H**, several independent sources agree, or it is a standard; **M**, one solid source or broad practitioner agreement; **L**, one practitioner source, or our own inference.

How the sources were read: through search-engine summaries. This session could not fetch izotope.com, mixgraph.io or the Pestana et al. PDF directly (their hosts are outside the network allowlist). Numbers from them are as the summaries reported them. Where two summaries disagreed, both are given.

No measured per-genre octave spectra or stereo-width figures of club records were found. Those targets (§1.2, §1.4) are conventions built on a measured chart-record average. §3's shared change S4 replaces them with measurements of reference tracks you own.

## 1. The standard, per genre

The four columns are the kits' directions:
- UK garage / 2-step: FUT01, 134 BPM.
- Sci-fi club, four on the floor at 130, whose neighbours are tech house, electro and melodic techno: FUT02.
- Future house: FUT03, 128 BPM.
- Dark industrial techno: FUT04, 136 BPM.

The targets apply to the *main groove after a club master*, which is what a 4-bar context loop of the whole kit is.

### 1.1 Loudness and dynamics

| | UKG / 2-step | Sci-fi club | Future house | Techno | Basis |
|---|---|---|---|---|---|
| LUFS-I of the loop | −9 (−10 to −8) | −8.5 (−9.5 to −7.5) | −8 (−9 to −7) | −8 (−9 to −7) | D from M, plus C · M |
| LUFS-S max (3 s), never above −5 | a 4-bar drop loop: its own LUFS-I plus a few tenths of a dB (see below); a whole track's loudest section: −8 to −7 | same; a track: −7.5 to −6.5 | same; a track: −7 to −6 | same; a track: −7 to −6 | C · L–M, O |
| True peak | ≤ −1.0 dBTP | ≤ −1.0 | ≤ −1.0 | ≤ −1.0 | O (QA ceiling), C (streaming) · H |
| PLR (true peak − LUFS-I) | 7.5–9 dB | 7–8.5 | 6.5–8 | 6.5–8 | D · M |
| PSR (true peak − LUFS-S max) | ≥ 6.5 dB | ≥ 6 | ≥ 6 | ≥ 6 | D, C · L–M |
| Master peak reduction (clipper and limiter together) | ≤ 4 dB | ≤ 4 | ≤ 4 | ≤ 4 | C · L–M |

**The measured references:**
- **Median released records** (Mixgraph, 2026): 900 tracks in all, about 22–52 per genre, measured from 120 s store previews at 96 kbps. The overall median is −8.8 LUFS-I. By genre:
  - techno −8.6 to −8.7 (two summaries)
  - tech house −8.9
  - house −9.2
  - progressive house −10.1
  - melodic house and techno −10.5
  - "dance" −7.9
  - dubstep −6

  One summary also gave a breaks / UK bass row (−8.1, range −9.9 to −7.3, 46 tracks) that a second search could not confirm. The page also says Beatport plays files without loudness normalisation. *M · L–M: small samples, lossy previews.*
- **Ten top-charting dance and electronic songs** (iZotope), from lossless purchased files:
  - per-track readings in the article include −8.7, −7.7 and −6.2 LUFS-I (a re-check on 2026-11-04, through search
    summaries again). The first version of this standard gave "an average of −8.3 LUFS-I, per track −7.7 to −6.2",
    which can't both be right (an average can't sit outside its own range): the −8.3 average with a 1 LU spread is
    iZotope's 2024 mastering-trends study of Billboard Global 200 top-10 songs, mostly pop, not this dance article.
    So this article puts current chart dance records at about −8.7 to −6.2 LUFS-I, and the loop targets below sit
    at or just under the middle of that range: they are not tightened until S4's measurements say so.
  - true peaks at or above 0 dBTP, one at +1.5 dBTP
  - one song's loudest section reached −4.5 LUFS-S, which iZotope judged too hot

  *M · L–M: ten songs, read second-hand.*
- **Of 54 songs in all genres, only 11 (20 %) peaked under 0 dBTP** (iZotope mastering trends, 2024). *M · M.*
- **The Beatport techno top 100, 500 tracks: about −6 to −7 LUFS-I** (a blog; method not given). *M? · L.*
- **A modern peak-normalised pop master reads a PLR of about 9 and a minimum PSR of about 6** (Sound On Sound's Dynameter review, one example). *M · L–M.*
- **Streaming:** Spotify plays at −14 LUFS-I and asks for a true peak under −1 dBTP. Some of its regional pages ask for −2 dBTP on masters louder than −14 LUFS. *C · H.*

**How the targets follow.**
- A loop of the main groove reads about as loud as a track's drop. A full track's integrated loudness is 0.5–2 LU lower, because its breakdowns count too. So the loop targets sit at or just above the measured medians, which mix full tracks and previews.
- Techno and future house run loudest. Garage guidance puts punch before level ("limit last; preserve punch over loudness"), so UKG sits at −9.
- Keeping our −1.0 dBTP ceiling, where released masters run 0 to +1.5 dBTP, costs about 1 dB of loudness. That is why the PLR targets are 6.5–9 rather than 8–10.
- Short-term targets: practitioners most often cite −6 LUFS-S for the loudest section of a loud dance master, with −5 as the aggressive end. *C · L–M.*
- **The short-term row, for a loop, follows the integrated one.** A 4-bar drop loop is steady: on the round 3 club loops its loudest 3 s sits only 0.27–0.51 dB over its LUFS-I. The first version of this table gave every genre a fixed short-term window for the loop (UKG −8 to −7, sci-fi club −7.5 to −6.5, future house and techno −7 to −6), which a loop at the centre of its integrated window can't reach: every round 3 loop missed it. So for a drop loop the integrated target is the one to aim at, and its short-term maximum comes out a few tenths of a dB over it; the fixed windows describe the loudest section of a whole track, where breakdowns pull the integrated reading down, and S4 checks them on whole tracks. QA reads neither. *O, from the round 3 measurements.*

### 1.2 Spectral balance

**The backbone.**
- Pestana et al. measured the long-term spectra of 772 chart records from 1950–2010. On average they fall about 5 dB per octave from 100 Hz to 4 kHz (as a per-Hz spectrum). In octave bands that is about −2 dB per octave. *M → D · M.*
- Electronic records, like hip-hop, rock and pop, carry more energy below about 150 Hz and above about 5 kHz than jazz or folk. *M (qualitative) · M.*
- Elowsson and Friberg found that how prominent the percussion is explains much of the spread between genres. *M · M.*
- The usual analyser convention, that a balanced mix looks flat with a 3–4.5 dB/oct tilt, agrees with that backbone: it is 0 to −1.5 dB per octave band. *C · M.*

**Target bands**, in dB against the 1 kHz octave, for the main section of a master (*D/C · L–M*):

| Octave (Hz) | UKG / 2-step | Sci-fi club | Future house | Techno |
|---|---|---|---|---|
| 31.5 | −8 to +2 | −4 to +6 | −8 to +2 | −2 to +8 |
| 63 | +6 to +12 | +6 to +12 | +6 to +12 | +4 to +12 |
| 125 | +2 to +6 | +1 to +6 | +1 to +5 | +1 to +6 |
| 250 | +1 to +4 | 0 to +4 | 0 to +3 | 0 to +4 |
| 500 | 0 to +3 | 0 to +3 | 0 to +3 | 0 to +3 |
| 1k | 0 | 0 | 0 | 0 |
| 2k | −3 to 0 | −3 to 0 | −2 to +1 | −3 to 0 |
| 4k | −5 to −2 | −5 to −2 | −4 to −1 | −6 to −2 |
| 8k | −8 to −4 | −8 to −4 | −7 to −3 | −10 to −5 |
| 16k | −14 to −8 | −14 to −8 | −12 to −7 | −17 to −10 |

The 31.5 and 63 Hz bands depend on the key: E1–F#1 (41–46 Hz) straddles the 44 Hz edge between them. Read them together: the louder of the two should sit in the genre's 63 Hz window over the 1 kHz band, +6 to +12 dB, or +4 to +12 for techno, whose 63 Hz row allows a lighter sub (QA reads it so: `qa.CLUB`). FUT04's E1 kick puts its sub in the 31.5 Hz band, at +5.7 after its round 3 fix: inside techno's window, under the other genres'.

**Three shape checks** do not depend on the 1 kHz reference:
- **Low-mid hole:** the 125 and 250 Hz bands against a straight line, in log frequency, from the 63 Hz band to the 500 Hz band (from the louder of 31.5 and 63 when the key puts the sub in the 31.5 band). The target is **≥ −4 dB**. *C/D · L–M.*
  - Club mixes dip their low mids a little, so a healthy record already reads 1–3 dB under the line.
  - The check allows a clean, dipped region, and still catches the hollow sound round 1 rated 2.
  - "No mud" stays as it is: the gates apply to the sounds; this check applies to the mix.
- **Top slope:** the 8 kHz band minus the 500 Hz band, over 4 octaves. *C/D · L–M.*
  - UKG −2.0 ± 0.5 dB/oct
  - sci-fi club −2.0 ± 0.5
  - future house −1.75 ± 0.5 (bright)
  - techno −2.5 ± 0.6 (darker allowed)
  - garage-house / deep tech house −2.0 ± 0.5 (round 4)
- **Sub hump:** the louder of the 31.5 and 63 Hz bands minus the 500 Hz band, +6 to +12 dB. Report only. *C · L.*

### 1.3 Low end

| Rule | UKG / 2-step | Sci-fi club | Future house | Techno | Basis |
|---|---|---|---|---|---|
| Kick fundamental, tuned to the key | 45–65 Hz | 45–60 Hz | 45–60 Hz (often around 55) | 40–60 Hz (often around 50) | C · M |
| Kick sub falls 20 dB (from the onset) | within about 2 16ths (225 ms at 134): tight, leaving room for the bass and the offbeat hats | within about 1.5 16ths (170 ms at 130), so the bass can roll | by the offbeat 8th (234 ms at 128) | within about 3 16ths (331 ms at 136); the rumble carries the tail | C, O · L–M |
| Kick against bass | the bass is the hook: it rests or ducks 4–6 dB under the kick | a rolling bass ducks 5–7 dB (1–2 ms attack, about 100–120 ms release), the sub band only, or rests on the kick | the bass rests on the kick; the music bus pumps | sub and rumble ducked to every kick; the kick leads | C · L–M |
| Mono | under 120–150 Hz (QA: side ≤ −20 dB under 120 Hz; `mono_below` at 150 Hz) | same | same | same | C · H, O |
| Sub-sonic | high-passed under 20–30 Hz | same | same | same | C, O · M |
| Low mids | the strict gate on kicks and toms; the bass-body rule on warm basses (ADR 0011) | same | same | same | O |

### 1.4 Stereo width (side over mid, dB, per band)

Every row here is *C · L*: no measured per-band width figures of club records were found.

| Octave bands | Target | Genre lean |
|---|---|---|
| Under 120 Hz | ≤ −20 (mono) | all |
| 125 and 250 Hz | ≤ −10 | all |
| 500 Hz and 1 kHz | −12 to −6 | |
| 2 and 4 kHz | −10 to −4 | sci-fi club and future house at the wide end |
| 8 and 16 kHz | −8 to −3 | |
| Correlation, whole loop | 0.5 to 0.9; never negative in any octave above 150 Hz | techno may sit drier and narrower, its reverbs wide |

### 1.5 Transients and punch

| Measure | Target | Basis |
|---|---|---|
| Kick crest: true peak over the loudest 50 ms, top layer | ≥ 4.0 dB (the floor). 5–7 dB is the band round 1 liked: Split read 5.8–6.4 (Cool 4), and Tight, at 7.9–8.9, sounded like "slamming the limiter" | O · M |
| Kick attack | the first 5 ms hold the kick's sample peak | O, C · M |
| Kick punch (60–120 Hz) and click (2–5 kHz) | both present: they are what makes a kick read on small speakers | C · M |
| Kick click: 2–8 kHz in the first 10 ms against 30–120 Hz in the first 100 ms | report only, uncalibrated. FUT03's brief asks for a clear click: aim for −18 dB or more | O · L |
| Clap | 3–4 hands within 25–35 ms, the first within 2 dB of the loudest; a body at 150–300 Hz from a snare or tom layer (150–400 Hz share −15 to −8 dB); a short room or plate tail, −20 dB at 120–250 ms | C · L–M |
| The backbeat against the kick, in the loop | the clap or snare (with the kick under it on 2 and 4) peaks no more than about 1 dB over a kick on its own, so the kick, not the backbeat, sets the master's limiting | O · L, from §2.5 |
| In-loop jumps (our measure, §2) | the master costs the kick and clap jumps at most 3–4 dB against the unmastered mix; the clap jumps at least 12 dB in 1–5 kHz | O · L |
| Crest per hit in the loop (ADR 0011's measure over 200 ms from each kick, 100 ms from each clap; `mixmeter.hit_crest_db`) | every full-velocity kick keeps the 4.0 dB floor in the loop (QA: `loop_kick_crest`, `club_kick_crest`); the master costs the kicks and claps a median of at most 1.5 dB against its input (QA: `club_crest_loss`; the A&R review's proposal, to be calibrated by ear) | O · L |

**What the jump can't see.** The jump compares a hit's loudest 10 ms with the bed 60–10 ms before it, so a clipper or limiter that pulls a hit's peak down onto its own body barely moves it: on the first round 3 masters the kicks lost 2.7–2.8 dB of crest while their full-band jump moved 0.2–0.9 dB. The crest per hit reads that flattening; QA now checks it. Two more cautions on the jump: a clap room shaped against its 60–10 ms window (FUT03's) raises the reading by design, so calibrate the 12 dB target with that in mind; and a causal low cut delays a low kick (the master's 25 Hz 4th-order cut delays E1 by 7.6 ms), which the fixed 30 ms window reads as lost 40–120 Hz jump (6.1 dB on FUT04's club loop), so compare 40–120 Hz jumps through the master with the onsets moved by the filter's delay, or read the crest instead.

### 1.6 Sidechain depth and timing

| | UKG / 2-step | Sci-fi club | Future house | Techno | Basis |
|---|---|---|---|---|---|
| Bass under the kick | 4–6 dB, held over the kick's first 80 ms | 5–7 dB, 1–2 ms attack, 100–120 ms release (one tutorial; others give 4–6 or 6–12 dB), band-split so only the sub ducks | the line rests on the kick (the sub duck on the vowel bass as now) | 4–8 dB; the rumble ducked to every kick, the first 100–150 ms only | C · L–M |
| Music bus (pads, chords, returns) | light, 0–3 dB | 2–4 dB, back by the next 8th | the style's audible pump: 6–12 dB on chords, 3–4 dB on returns, 200–250 ms release | returns and rumble ducked; pads 2–4 dB | C · L–M |

The kits today (O):
- FUT01 ducks its bass 6 dB (78 ms hold, 60 ms release).
- FUT02 sidechains only its drop glide (6 dB).
- FUT03 pumps the stab 9 dB (200 ms release), the pluck 3 dB and the returns 4 dB.
- FUT04 dips its bass under the vocals, but nothing ducks under its kick.

### 1.7 Saturation and its limits

- **Kicks:** distortion only above about 120–300 Hz. The sub stays clean: the split kick, binding since round 1. *O, C · H.*
- **Basses:** harmonics so a bass reads on small speakers. Under bass-body, 150–400 Hz may rise up to a plain saw's level, and no further (ADR 0011). *O.*
- **Drum bus:** a soft clip of 1–3 dB of peak reduction, 4× oversampled. A kick alone may take 2–4 dB of hard clip. *C · L–M.*
- **Master:** clip before you limit, with no more than 3–4 dB of peak reduction in total (§1.1). The kicks in the mastered loop must still pass the crest floor's intent: the transient stands over the body. *C · L–M, O.*
- **Techno:** multi-stage distortion with EQ between the stages, on the kick and on the rumble. Heavy saturation belongs to the genre. *C · M.*
- **Every nonlinear stage runs oversampled**, with its DC blocked (ADR 0007). *O · H.*

### 1.8 Reverb and delay

A 1/64 note lasts 60000 / BPM / 16 ms: 29 ms at 128 and 130 BPM, 28 ms at 134 and 136. A 1/32 note is twice that. *C · L–M:*

| | UKG / 2-step | Sci-fi club | Future house | Techno |
|---|---|---|---|---|
| Snare and clap | plate 0.8–1.4 s, pre-delay about a 1/64 note | room or plate 1–1.5 s | short bright plate 1–1.8 s, pumped | gated room, or a long dark hall, ducked to the kick |
| Pads and vocals | hall 2–3 s, pre-delay a 1/32 note (56 ms) | hall 3–4 s, pre-delay a 1/32 (58 ms) | plate or hall 1.5–2.5 s, pumped | dark hall 4–8 s, its tail low-passed, sent hard, ducked |
| Delay | 1/8 or dotted-1/8 throws on the chops | ping-pong on the arp (1/8, dotted 1/8) | 1/8 or 1/4, pumped | dub delays (3/16, 1/4), filtered |

Percussion takes 10–15 ms of pre-delay, small rooms 0–10 ms. The kits today (O): one Space for all four, a 2.6 s hall (its low band 0.8 s) with 20 ms of pre-delay, damped at 6.5 kHz and fed through a 450 Hz high-pass; and one Echo, a dotted 8th at feedback 0.38, filtered LP 2.6 kHz / HP 300 Hz.

### 1.9 Signature sounds and moves that read as current

These are all *C · L–M*: practitioner pages about current production, many of them sample-pack or school marketing.

- **UK garage / 2-step (the 2024–26 wave):**
  - **The swung skip.** Guides put the shuffle on the hats and percussion. One suggests 52–56 %; an Attack Magazine beat breakdown sets MPC swing 68–69 at 124–132 BPM. Our 0.57 is MPC 57 %.
  - **A tight, punchy kick** that leaves room for the bass and the offbeat hats.
  - **The bassline as the hook,** now often a reese or dubstep-style bass over a sine sub; one guide calls a plain sine "too polite".
  - **Short, plucky, rhythmic stabs.**
  - **Short vocal hooks,** "built to be shouted".
  - **Tempo:** a median of 132 BPM (130–138), with a speed-garage strand at 138–142.
  - **Future garage:** re-pitched vocals and atmospheric pads at 130–140 BPM, without wobble basses.
- **Sci-fi club** (four on the floor at 130):
  - Tech house's median tempo has risen from 124 to 129 BPM, and 130 in 2025.
  - Rolling 16th and offbeat basses on three-note motifs, with a band-split sidechain.
  - "Let the kick win" when the kick and bass compete.
  - Electro claps.
  - Arps that evolve under filter rides, vocoders, and lasers and pitch drops as effects.
- **Future house:**
  - The metallic, FM-coloured future-house bass with pitch movement, in 2–4 layers (sub, main, top), with everything under about 200 Hz mono and shaped to the kick.
  - An audible pump.
  - Tight kicks with a click.
  - Crisp, bright claps and tops, and vocal chops.
- **Techno (dark industrial, peak time):**
  - The rumble kick: a copy of the kick sent through a long reverb, distorted, low-passed and ducked to the kick, so it is the sub.
  - Multi-stage distorted kicks.
  - Driving 16th hats, rides and metal percussion.
  - Acid lines.
  - Rave stabs and hoover-style leads, borrowed from hardcore and trance in peak-time and hard techno.
  - Long dark reverbs, sidechained.
  - Tempo: a median of 133 BPM; hard techno runs 145–160.

### 1.10 Extended mixes (for S1b's song builder, not the sounds)

- DJ-friendly intros and outros are 16–32 bars of simple elements, so a DJ can beatmatch.
- Every section is 8, 16 or 32 bars long, so changes land on phrase boundaries.
- One club layout: intro 32, build 16, drop 32, breakdown 32, build 16, drop 32, outro 32 bars. Tech house runs shorter, trance longer.

*C · M.*

What the kits' MIDI does (`tools/sound/midi.py`, `FORMS`; 224 bars each, every drop that follows a break reached through its build, whose riser ends on the drop's first beat over a snare or clap roll, quarters, then 8ths, then 16ths, into the kit's fill, the kick out but in future house):
- Sci-fi club and future house: intro 32, groove 32, break 16, build 16, drop 32, switch-up 16, break 2 16 with its riser, drop 2 32, outro 32.
- Techno: one break and long grooves: intro 32, groove 64, break 16, build 16, then the drop and drop 2 back to back (64 bars), outro 32.
- UK garage: a 16-bar intro of drums (the 2-step kick under the full swung hats and the shaker, which carry the pulse a DJ beatmatches on, then the 2-step beat), the bass and harmony in a 48-bar groove, then the club form, its outro ending on the kick, hats and shaker as the intro began.

## 2. Where the kits stand: round 2, measured

**What was measured:**
- Each kit's context loops in `build/<kit>-r2/`: dry A (`ContextLoop`), dry B (`ContextLoopWarm`), wet (`ContextLoopWet`) and the low-end loops (`ContextLoopLow`, `ContextLoopLowWarm`). They are the same samples as the WAVs `content/samples/<kit>/Loops/` held in round 2, before the round 3 refresh (that folder's history): all 20 matched by SHA-256.
- The drums the loops play: each kit's top-layer kick (`KickSplit` v5), its clap or snare and its hats.

**The meter** is a scratch script (`meter.py`, `measure.py`, the projections; §5), stdlib plus numpy, scipy and soundfile:
- **Loudness:** ITU-R BS.1770-4 / EBU Tech 3341. K-weighting at 48 kHz, 400 ms momentary and 3 s short-term windows every 100 ms, gated integrated loudness, and a 4× true peak.
- **Loops are measured circularly**, as they play: every window may wrap round the seam.
- **Octave bands** (base-2 centres, 31.25 Hz to 16 kHz) come from one FFT over exactly one loop period, so there is no window and no leakage. They are the power of L and R summed.
- **Width:** side over mid, and the L/R correlation per band.

**It was checked before use:**
- EBU Tech 3341 cases 1–4 read −22.99, −32.99, −23.01 and −23.01 LUFS (±0.1 allowed).
- The fs/4, 45° true-peak case reads 0.00 dBTP.
- Pink noise reads flat within 0.4 dB per octave.
- Against `tools/sound/qa.py` it agrees exactly on the kick crest (6.44 / 6.32 / 6.02 / 6.43 dB) and on true peak. LUFS-I agrees within 0.12 LU; the difference is circular against linear windows.

### 2.1 Loudness and dynamics

| Kit | Loop | LUFS-I | LUFS-S max | True peak (dBTP) | PLR | PSR | Target LUFS-I / PLR |
|---|---|---|---|---|---|---|---|
| FUT01 | dry A | **−16.2** | −15.8 | −2.07 | **14.1** | **13.7** | −9 / 7.5–9 |
| | dry B | **−16.2** | −15.8 | −1.88 | **14.3** | **13.9** | |
| | wet | **−16.1** | −15.7 | −2.08 | **14.0** | **13.6** | |
| FUT02 | dry A | **−14.9** | −14.2 | −1.02 | **13.9** | **13.2** | −8.5 / 7–8.5 |
| | dry B | **−14.9** | −14.2 | −1.02 | **13.9** | **13.2** | |
| | wet | **−14.9** | −14.0 | −1.02 | **13.9** | **13.0** | |
| FUT03 | dry A | **−16.2** | −16.0 | −1.02 | **15.2** | **15.0** | −8 / 6.5–8 |
| | dry B | **−16.2** | −16.0 | −1.02 | **15.2** | **14.9** | |
| | wet | **−16.1** | −15.7 | −1.02 | **15.0** | **14.7** | |
| FUT04 | dry A | **−17.0** | −16.3 | −2.67 | **14.4** | **13.7** | −8 / 6.5–8 |
| | dry B | **−17.0** | −16.3 | −1.13 | **15.9** | **15.2** | |
| | wet | **−17.1** | −16.3 | −1.39 | **15.7** | **14.9** | |

Bold marks a reading outside the target.
- The loops are 6.4 to 9.1 LU under their targets, and 5–9 dB too peaky.
- FUT01 and FUT04 sit at a LUFS target (−16.3 and −17.0) under the ceiling. FUT02 and FUT03 are normalised to their peak. None has a master chain: every recipe says so ("no master chain").
- The low-end loops (kick and bass alone) read PLR 11.6–17.1.

### 2.2 Spectral balance

The octave bands, in dB against the 1 kHz band. Bold marks a reading outside its genre's band (§1.2):

| Kit | Loop | 31.5 | 63 | 125 | 250 | 500 | 1k | 2k | 4k | 8k | 16k |
|---|---|---|---|---|---|---|---|---|---|---|---|
| FUT01 | dry A | −5.8 | +7.8 | **−4.2** | **−5.5** | +1.4 | 0 | −1.7 | −5.0 | −6.7 | −10.8 |
| | dry B | −5.7 | +7.9 | **−2.8** | **−3.1** | +1.7 | 0 | −1.5 | −4.8 | −6.4 | −10.6 |
| | wet | −6.8 | +6.7 | **−4.0** | **−4.3** | **+3.3** | 0 | −2.0 | **−5.2** | −7.3 | −11.7 |
| FUT02 | dry A | +2.7 | +7.9 | **−1.0** | **−4.6** | +1.3 | 0 | **−3.3** | **−5.7** | −6.5 | −10.7 |
| | dry B | +1.3 | +8.6 | **+0.6** | **−1.5** | +2.1 | 0 | **−3.3** | **−5.4** | −5.9 | −10.1 |
| | wet | +0.5 | +7.7 | **−0.3** | **−2.4** | +2.5 | 0 | **−3.5** | **−6.0** | −6.8 | −10.9 |
| FUT03 | dry A | **−12.8** | +8.0 | **−3.0** | **−1.7** | +2.0 | 0 | **−3.9** | **−8.4** | **−10.9** | **−13.4** |
| | dry B | **−13.2** | +7.9 | **−2.2** | **−1.1** | +2.2 | 0 | **−4.5** | **−8.8** | **−10.8** | **−13.4** |
| | wet | **−15.6** | +5.5 | **−4.6** | **−3.5** | −0.0 | 0 | **−6.3** | **−11.1** | **−13.2** | **−15.8** |
| FUT04 | dry A | +7.8 | +0.1 | **−5.2** | **−4.5** | +0.6 | 0 | **−4.4** | **−7.0** | −9.2 | **−17.3** |
| | dry B | +8.2 | +0.5 | **−4.0** | **−3.1** | +0.7 | 0 | **−4.1** | **−6.7** | −8.9 | −17.0 |
| | wet | +7.7 | +0.0 | **−4.5** | **−3.2** | +1.4 | 0 | **−4.2** | **−7.0** | −9.4 | **−17.4** |

FUT04's 63 Hz octave reads low because its E1 sub (41 Hz) falls into the 31.5 Hz octave; read together, the two bands pass. FUT03's 31.5 Hz reading is the least certain call: a future-house low end can be tight.

The shape checks (§1.2):

| Kit | Loop | Hole 125 | Hole 250 | Top slope (dB/oct) | Sub hump | Targets: hole / slope |
|---|---|---|---|---|---|---|
| FUT01 | dry A | **−9.9** | **−9.1** | −2.02 | +6.4 | ≥ −4 / −2.0 ± 0.5 |
| | dry B | **−8.7** | **−6.9** | −2.04 | +6.1 | |
| | wet | **−9.6** | **−8.7** | **−2.65** | +3.4 | |
| FUT02 | dry A | **−6.7** | **−8.1** | −1.97 | +6.6 | ≥ −4 / −2.0 ± 0.5 |
| | dry B | **−5.9** | **−5.8** | −2.01 | +6.4 | |
| | wet | **−6.3** | **−6.6** | −2.31 | +5.2 | |
| FUT03 | dry A | **−9.0** | **−5.7** | **−3.21** | +6.0 | ≥ −4 / −1.75 ± 0.5 |
| | dry B | **−8.1** | **−5.2** | **−3.25** | +5.7 | |
| | wet | **−8.2** | **−5.3** | **−3.31** | +5.5 | |
| FUT04 | dry A | **−10.6** | **−7.5** | −2.46 | +7.2 | ≥ −4 / −2.5 ± 0.6 |
| | dry B | **−9.7** | **−6.3** | −2.40 | +7.6 | |
| | wet | **−10.1** | **−6.7** | −2.69 | +6.3 | |

**Third octaves**, 31–1000 Hz, in dB against the 1 kHz third octave, dry A:

| Hz | 31 | 39 | 50 | 62 | 79 | 99 | 125 | 157 | 198 | 250 | 315 | 397 | 500 | 630 | 794 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| FUT01 | −20 | −2 | +10 | +5 | +3 | −2 | −7 | −9 | −6 | −7 | −5 | −1 | +3 | −2 | 0 |
| FUT02 | 0 | +9 | +8 | +12 | +8 | +3 | −2 | −1 | −4 | −3 | −2 | +3 | +2 | +4 | +3 |
| FUT03 | −17 | −8 | +12 | +6 | +6 | 0 | −4 | −5 | −1 | −3 | 0 | +2 | +2 | +3 | +2 |
| FUT04 | −12 | +12 | +2 | −2 | −5 | −7 | −9 | −4 | −5 | −9 | −4 | −4 | 0 | +2 | 0 |

Where the holes are:
- FUT01's spans 125–315 Hz.
- FUT04's runs from 62 Hz to about 400 Hz (−2 to −9 dB), right above its 39 Hz sub at +12.
- FUT02 bulges at 400–800 Hz (+2 to +4).

The B basses fill 125 / 250 Hz by +1.4 / +2.4 dB (FUT01), +1.6 / +3.1 (FUT02), +0.8 / +0.6 (FUT03) and +1.2 / +1.4 (FUT04).

### 2.3 Stereo width (side over mid, dB)

| Kit | Loop | <120 Hz | 125 | 250 | 500 | 1k | 2k | 4k | 8k | 16k | Correlation |
|---|---|---|---|---|---|---|---|---|---|---|---|
| FUT01 | dry A | −76 | −54 | **−8.3** | −8.8 | **−5.2** | −8.3 | **−10.6** | **−10.5** | −5.0 | 0.89 |
| | wet | −61 | −22 | **−5.6** | −6.5 | **−5.1** | −7.7 | −9.0 | **−9.9** | −5.0 | 0.83 |
| FUT02 | dry A | −85 | −45 | **−9.8** | **−12.2** | **−12.2** | **−11.7** | **−13.7** | **−14.3** | **−12.2** | 0.97 |
| | wet | −67 | −31 | −12.1 | −7.2 | −7.8 | −9.4 | **−12.3** | **−13.9** | **−12.1** | 0.91 |
| FUT03 | dry A | −68 | −41 | −11.4 | **−15.3** | **−13.8** | **−12.2** | **−10.1** | −7.1 | −4.6 | 0.96 |
| | wet | −65 | −37 | −11.3 | −11.4 | −11.4 | −9.7 | −9.2 | −7.1 | −4.6 | 0.94 |
| FUT04 | dry A | −89 | −29 | −15.8 | **−12.3** | −9.8 | −9.0 | −9.0 | **−11.0** | **−10.3** | 0.95 |
| | wet | −64 | −21 | −11.4 | −7.4 | −8.0 | −8.0 | −8.8 | **−10.8** | **−10.3** | 0.91 |

Bold marks a reading outside §1.4's conventions.
- Every kit is mono under 120 Hz, by a wide margin.
- FUT02 is the narrowest from 500 Hz up. Its closed hat reads −16.7 dB on its own, its open hat −13.8.
- FUT04's hats are near-mono too (−18.4 and −17.3).
- FUT03's dry mids are narrow (500 Hz–4 kHz at −10 to −15 dB); its wet loop brings them into range.
- FUT01 is the opposite: wide in the low mids (250 Hz −8.3 dry, −5.6 wet, against ≤ −10) and at 1 kHz.

### 2.4 Kick and clap (the loops' top-layer one-shots)

| | FUT01 | FUT02 | FUT03 | FUT04 | Target |
|---|---|---|---|---|---|
| Kick fundamental, 0.10–0.25 s | 46.3 Hz (F#1) | 55.1 Hz (A1, the fifth) | 49.0 Hz (G1) | 41.2 Hz (E1) | genre range (§1.3) |
| Kick sweep at 5 / 20 / 40 ms | 271 / 111 / 56 Hz | 278 / 108 / 62 | 275 / 91 / 52 | 262 / 100 / 48 | — |
| Kick crest, rr1 / rr2 | 6.4 / 5.8 dB | 6.3 / 5.9 | 6.0 / 6.4 | 6.4 / 5.9 | 5–7 (floor 4.0) |
| Kick peak in its first 5 ms | yes | yes | yes | yes | yes |
| Kick sub −10 / −20 / −30 dB, from the onset | 220 / **317** / 381 ms | 102 / 122 / 154 | 164 / 225 / 256 | 254 / 340 / 377 | §1.3 |
| …the −20 dB point in 16ths | **2.83** | 1.05 | 1.92 | 3.08 (at the limit) | 2 / 1.5 / 2 / 3 |
| Kick click / body | −19.9 dB | −20.2 | **−23.3** | −21.8 | FUT03's brief: a clear click |
| Kick 150–400 Hz share | −17.8 dB | −14.3 | −14.0 | −17.4 | gate: ≤ −12 |
| Clap or snare | SnareMetal | ElectroClap | Clap | SnareGated | |
| Hands in the first 50 ms (ms: dB re the loudest) | one (a snare) | 2.3: 0, 9.2: −1.3, 14.9: −0.4, 22.5: −1.2 | **6.2: −8.0**, 11.5: 0, 24.7: −11.2 | **6.7: −4.5**, 12.3: 0, 23.3: −4.1, 33.5: −6.5, 40.9: −7.1 | 3–4, the first within 2 dB |
| Crest | 10.3 dB | 11.5 | **18.4** | 15.2 | — |
| −20 / −40 dB after the onset | 142 / 310 ms | 218 / 375 | **73** / 172 | 187 / 196 (gated) | −20 dB at 120–250 ms |
| Share 150–400 / 400–1k / 1–4k / 4–10k Hz | −8.6 / −11.3 / −2.6 / −7.1 dB | −9.9 / −13.0 / −3.4 / −5.4 | **−32.1** / −8.5 / −1.9 / −7.5 | −9.8 / −7.3 / −3.4 / −6.7 | body −15 to −8 |
| Closed hat to −40 dB, centroid | 83 ms, 11.1 kHz | 57 ms, 10.9 kHz | 29 ms, 11.7 kHz | 37 ms, 8.7 kHz | — |
| Open hat to −40 dB (choked in the loop) | — | 403 ms | 164 ms | 182 ms | — |

### 2.5 Punch in the loop, and where the peaks are

These measures are our own, and no external reference exists for them:
- **The kick jump:** the band's loudest 10 ms in the first 30 ms after each kick, against its mean power 60–10 ms before. It is read in 40–120 Hz and over the full band.
- **The clap jump:** the same in 1–5 kHz, at the claps and snares of velocity 0.8 or more.

| Kit | Kick jump in 40–120 Hz (median, min) | Kick jump, full band | Clap jump in 1–5 kHz | Backbeat peak over a kick alone | Lanes against the kick lane (dry A) |
|---|---|---|---|---|---|
| FUT01 | 44.8, 11.4 dB | 11.0, 6.8 | 13.2, 9.4 | **+1.4 dB** | whole mix +4.4 LU; bass −1.4; snare −0.7; vox −1.2; hats **−11.0**; pad −6.6; pluck −7.5 |
| FUT02 | 20.0, 12.1 | 13.3, 3.0 | 12.1, 9.8 | +0.4 | mix +4.7; bass −1.5; clap −4.0; hats −7.8; arp −6.4; pad −7.0; chops −2.4 |
| FUT03 | 20.1, 9.4 | 10.7, 5.0 | **10.0**, 9.1 | **+2.4** | mix +4.7; bass −2.0; clap −4.9; hats **−11.0**; stab −7.3; pluck −1.9; vox −0.8 |
| FUT04 | 21.6, 17.7 | 11.8, 7.6 | 19.2, 12.4 | +0.5 | mix **+6.0**; bass −3.0; rumble −8.0; snare −2.5; hats −8.0; lead −2.5; chops **0.0**; robot −0.5 |

Where the peaks are:
- In every loop the highest true peaks fall on the backbeat (16ths 4, 12, 20 and so on).
  - In FUT01, FUT03 and FUT04 the snare or clap alone reaches within 1–5 dB of the loop's peak there, while the kick-and-bass bus sits 7–31 dB under it.
  - In FUT02 the kick and clap add up, each about 6 dB under their sum.
- In the four-on-the-floor kits the kick plays under the clap, so the sum peaks: 0.4–2.4 dB over a kick on its own.
- In FUT03 the claps on 16ths 12 and 44 set the loop's peak, 1.4 dB over the next backbeat and at least 3.3 dB over everything else.
- In all four loops, only 0.006–0.08 % of samples come within 3 dB of the peak.

The lane levels come from the loop's metadata in `qa_report.json` (LUFS-I per lane before the loop's gain). FUT04's lanes sum to 6.0 LU over its kick, against 4.4–4.7 in the other kits: its kick leads the least.

### 2.6 What a club master would cost (a projection)

In scratch, each dry A loop was mastered to its genre target through a scratch brickwall limiter:
- 4× oversampled, stereo-linked
- 1.5 ms lookahead
- a release of 75 dB/s
- a ceiling of −1.25 dBTP

The second variant first soft-clipped the two buses, recovered from the loop files and their render gains: everything but the kick and bass by 6 dB, the kick and bass by 3 dB. Both buses were made up to their own loudness before the master.

| Kit (target) | As it is: LUFS-I / PLR / kick jump / clap jump | Plain limiter: peak reduction / reached / kick / clap | Clipped buses, then the limiter: mix PLR / peak reduction / kick / clap |
|---|---|---|---|
| FUT01 (−9) | −16.2 / 14.1 / 11.0 / 13.2 | **8.3 dB** / −9.2 / **6.8** / **8.1** | 12.0 / **5.1 dB** / 7.4 / 8.8 |
| FUT02 (−8.5) | −14.9 / 13.9 / 13.3 / 12.1 | **≥ 9.9** / −9.1 / **6.5** / **4.9** | 11.7 / **6.3** / 8.4 / 6.5 |
| FUT03 (−8) | −16.2 / 15.2 / 10.7 / 10.0 | **≥ 11.9** / −8.8 / **5.2** / **5.3** | 12.0 / **7.3** / 6.1 / 5.2 |
| FUT04 (−8) | −17.0 / 14.4 / 11.8 / 19.2 | **≥ 12.3** / −9.3 / 7.9 / 10.8 | 12.1 / **8.2** / 8.1 / 13.1 |

What the projection shows:
- **The limiter alone fell short of three targets.** On FUT02–FUT04 it stopped 0.6–1.3 LU short, so the reduction those targets need is higher than shown.
- **A soft clipper alone (tanh) reached every target, at PLR 5.2–7.2,** but only by taking 9–13 dB off the peaks, which is heavy distortion.
- **Clipping a kick-and-bass bus on its own** (0–6 dB) moved the mix PLR by 0.4 dB at most: the kick is not what makes the loops peaky.

This uses a scratch limiter, not a mastering chain. A limiter with program-dependent release will need less reduction for the same loudness. The ranking and the order of magnitude hold. The fix is in the mix, not only in the master: the backbeat transients (§2.5), and more sustained energy in the hats, rides, pads, bass length, rumble and reverb. A plain master would squash exactly the clap jump (down 7 dB on FUT02) and the kick jump that make the loops punchy.

### 2.7 The gaps at a glance

The target column gives four values in kit order (UKG / sci-fi / future house / techno) where it differs by genre.

| Measure | Target | FUT01 | FUT02 | FUT03 | FUT04 |
|---|---|---|---|---|---|
| LUFS-I, dry A | −9 / −8.5 / −8 / −8 | **−16.2** | **−14.9** | **−16.2** | **−17.0** |
| PLR, dry A | 7.5–9 / 7–8.5 / 6.5–8 / 6.5–8 | **14.1** | **13.9** | **15.2** | **14.4** |
| Master peak reduction: plain / clipped buses first | ≤ 4 | **8.3 / 5.1** | **9.9 / 6.3** | **11.9 / 7.3** | **12.3 / 8.2** |
| Low-mid hole, 125 / 250 | ≥ −4 | **−9.9 / −9.1** | **−6.7 / −8.1** | **−9.0 / −5.7** | **−10.6 / −7.5** |
| Top slope, 500 Hz–8 kHz | −2.0 / −2.0 / −1.75 / −2.5, each ± 0.5 (techno ± 0.6) | −2.02 | −1.97 | **−3.21** | −2.46 |
| 8 / 16 kHz re 1 kHz | §1.2 | −6.7 / −10.8 | −6.5 / −10.7 | **−10.9 / −13.4** | −9.2 / **−17.3** |
| Side over mid, 2 / 4 / 8 kHz | 2 and 4 kHz −10 to −4; 8 kHz −8 to −3 | −8.3 / **−10.6 / −10.5** | **−11.7 / −13.7 / −14.3** | **−12.2 / −10.1** / −7.1 | −9.0 / −9.0 / **−11.0** |
| Under 120 Hz, side over mid | ≤ −20 | −76 | −85 | −68 | −89 |
| Kick sub −20 dB | §1.3 | **317 ms** | 122 ms | 225 ms | 340 ms (3.1 16ths, at the limit) |
| Kick click / body | FUT03: about −18 or more | −19.9 | −20.2 | **−23.3** | −21.8 |
| Clap's first hand, re the loudest | ≥ −2 dB | (snare) | 0 | **−8.0** | **−4.5** |
| Clap body, 150–400 Hz share | −15 to −8 | −8.6 | −9.9 | **−32.1** | −9.8 |
| Clap tail to −20 dB | 120–250 ms | 142 | 218 | **73** | 187 |
| Backbeat over a kick alone | ≤ +1 dB | **+1.4** | +0.4 | **+2.4** | +0.5 |
| Clap jump in the loop | ≥ 12 dB | 13.2 | 12.1 | **10.0** | 19.2 |

## 3. The changes, per kit (most important first)

Each change names the gap it closes. Every change keeps round 1's binding lessons:
- the split kick and its crest floor
- basslines that groove against the kick
- no mud: the strict gate on kicks and toms
- natural, title-checked vocal lines
- shots and fills with a musical gesture

"B" means the warm bass. Round 2's A/B ratings are not in yet (`docs/listening.md` stops at round 1), so every "use B" waits for your pick.

### FUT01 "Alloy Signal": UK garage / 2-step, 134 BPM, F# minor

1. **Make the mix dense enough to master, then master it.**
   - Gap: −16.2 LUFS-I and PLR 14.1, against a target of −9 ± 1 and 7.5–9.
   - A plain limiter would take 8.3 dB off the peaks and cut the kick's jump from 11.0 to 6.8 dB. Clipping the buses first brings the mix PLR to 12.0 and the limiting to 5.1 dB.
   - Do: clip the snare on the drum bus (it sets the loop's peak, §2.5), add 1–2 dB of glue on the music bus, then master with no more than 3–4 dB of clip and limit.
   - Deliver it as a new mastered context loop. The dry loops stay as the unmastered mix.
2. **Fill 125–315 Hz without mud.**
   - Gap: the hole is −9.9 / −9.1 dB at 125 / 250 Hz, and the third octaves 125–315 Hz read −5 to −9. It is the most hollow kit at 250 Hz.
   - Do:
     - Make the reese B the context bass if round 2 picks it: it measures +1.4 / +2.4 dB.
     - Give the kick a knock at 100–150 Hz over its first 40–50 ms. The gate allows it, since 150–400 Hz still falls 30 dB within 120 ms.
     - Keep the pad's and the chops' lowest voices from 150 Hz up.
     - Keep the low mids central: at 250 Hz the side reads −8.3 dB dry and −5.6 wet, against ≤ −10.
   - Target: a hole of −4 dB or better, with the gates unchanged.
3. **A tighter kick sub for 2-step, as an A/B against kick A.**
   - Gap: the sub falls 20 dB at 317 ms and 30 dB at 381 ms (2.8 and 3.4 16ths). Current garage guides want a tight, punchy kick that leaves the bass and the offbeat hats room.
   - Target: −20 dB by about 220 ms.
   - It must be an A/B: kick A got Cool 4 in round 1 at this length.
4. **Bring the backbeat down to the kick.**
   - Gap: the snare peaks 1.4 dB over the kicks and sets the loop's true peak (its crest is 10.3 dB).
   - Do: soft-clip its first 1–2 ms by 2–3 dB, in the snare or on the drum bus.
5. **Make the bass the hook.**
   - Current UKG puts the bassline up front: often a reese over a clean sub, with a plain sine called "too polite" (C).
   - The reese lane already sits 1.4 LU under the kick.
   - Do: give the reese B a per-bar filter or vowel move, so the line talks in its gaps. Keep the 6 dB kick ducker.
6. **A garage space, not one hall for everything.**
   - The shared Space is a hall: 2.6 s with 20 ms of pre-delay.
   - Garage convention puts the snare and the chops on a short plate (0.8–1.4 s) with a 1/64-note pre-delay (28 ms at 134 BPM), and keeps the hall for throws (C).
7. **A swung top layer.**
   - Gap: the hats sit 11 LU under the kick, the lowest with FUT03, and the shuffle is what drives garage (C).
   - Do: add a 16th shaker or top loop on the kit's swing, about 7–8 LU under the kick. The 8 kHz band (−6.7) has room for 1–2 dB.
   - Optional: an A/B of the shuffle at 0.60–0.62 against today's 0.57. Guides range from 52–56 % on hats to MPC 68–69.
8. **Vocal hooks.** Short, repeated, pitched hooks lead current UKG (C).
   - Do: re-pitch a chop through WORLD (as the chain already does) for a second, higher take, and throw the phrase's end to the echo.
   - Title-check any new words.

### FUT02 "Orbit Relay": sci-fi club, 130 BPM, D minor

1. **Density, then a club master.**
   - Gap: −14.9 LUFS-I and PLR 13.9, against −8.5 ± 1 and 7–8.5.
   - A plain limiter would need at least 9.9 dB and cut the kick's jump from 13.3 to 6.5 dB and the clap's from 12.1 to 4.9: the worst clap loss of the four.
   - With the buses clipped first the mix reads PLR 11.7 and the limiter takes 6.3 dB.
   - Same chain as FUT01.
2. **Widen the music above 500 Hz.**
   - Gap: the side sits 12–14 dB under the mid from 500 Hz to 16 kHz (correlation 0.97). FUT02 is the narrowest kit, and sci-fi club wants width: target −10 to −4 dB at 2–4 kHz and −8 to −3 dB above.
   - Do: decorrelated pad voices, a ping-pong on the arp, wider zaps and fill, and a chorus on the vocoder.
   - Keep everything under 150 Hz mono, and check the mono fold-down. The closed hat may stay central.
3. **Fill the low mids and trim 400–800 Hz.**
   - Gap: the hole is −6.7 / −8.1 dB at 125 / 250 Hz, and the third octaves 400–800 Hz sit +2 to +4 over 1 kHz.
   - The warm bass B measures +1.6 / +3.1 dB at 125 / 250. The clap already has a body (a 150–400 Hz share of −9.9 dB).
4. **A club pump.**
   - Only the drop glide is sidechained today. Tech-house practice ducks the bass 5–7 dB (1–2 ms attack, about 100–120 ms release) and lets the pads breathe (C).
   - The line already rests on every kick, so pump the pad, the arp's sustain and the returns 2–4 dB instead, back by the next 8th.
5. **An arp that evolves.** Ride its cutoff and resonance over the 4 bars, and send it to a 1/8 or dotted-1/8 ping-pong.
   - Current club arps move (C).
   - It is the kit's signature, 6.4 LU under the kick: its melody stays.
6. **A sci-fi space.** A longer, brighter hall on the pad and the vocoder: 3–4 s, with a 1/32-note pre-delay (58 ms at 130 BPM). Keep the arp mostly dry (C).

Keep as they are:
- the kick (A1, 55 Hz, its sub −20 dB at 122 ms: the short, punchy brief)
- the electro clap (four hands in 23 ms, a tail to −20 dB at 218 ms)
- the 130 BPM tempo (tech house's median has risen to about 129–130)
- the top end (8 kHz −6.5, 16 kHz −10.7, within the target)

### FUT03 "Glass Current": future house, 128 BPM, G minor

1. **Density, then a club master: the farthest to go.**
   - Gap: −16.2 LUFS-I and PLR 15.2, against −8 ± 1 and 6.5–8.
   - A plain limiter would need at least 11.9 dB and cut the kick's jump from 10.7 to 5.2 dB. Even with the buses clipped first, it takes 7.3 dB.
   - Part of the cause is the clap (change 3): the kick and clap on 2 and 4 peak 2.4 dB over a kick on its own, and the claps on 16ths 12 and 44 set the loop's peak.
2. **A brighter, cleaner top.**
   - Gap:
     - 4 kHz −8.4, 8 kHz −10.9, 16 kHz −13.4 against 1 kHz
     - a top slope of −3.2 dB/oct, against −1.75 ± 0.5
     - the darkest top of the four, in the genre that should be brightest
     - hats 11 LU under the kick
   - Do: hats and a shaker up 3–4 dB, a high shelf from 8 kHz on the pluck and the vocals, and more of the clap's snap at 2–5 kHz.
   - Target: 4 kHz −4 to −1, 8 kHz −7 to −3.
3. **Rebuild the clap as the brief's three layers.**
   - Gap:
     - the first hand is 8 dB under the loudest one, which comes at 11.5 ms
     - there is nothing under 400 Hz (a 150–400 Hz share of −32 dB)
     - the tail is −20 dB at 73 ms
     - the crest is 18.4 dB
   - Do:
     - a snap layer that peaks on the first hand
     - a body at 180–250 Hz (a share of −15 to −10 dB) that dies away within 150 ms
     - a short, bright plate or room tail, −20 dB at 150–250 ms
     - shape the crest to about 12 dB, so the clap stops setting the loop's peak
   - Target: a clap jump of at least 12 dB in the loop. It is 10.0 today, the lowest of the four.
4. **The clear click the brief asks for.**
   - Gap: the kick's 2–8 kHz click reads −23.3 dB against its sub, the least of the four (the others read −19.9 to −21.8).
   - Do: add 4–6 dB of click in the first 5–10 ms, keeping the crest within 5–7 dB (ADR 0011).
5. **Fill the low mids.**
   - Gap: the 125 Hz band sits 9.0 dB under the line.
   - The clap's body (change 3) and the warm dip bass B (+0.8 dB) help. Voice the stab from about 150 Hz.
6. **Push the future-house bass.**
   - The metallic dip bass is the genre's signature. Current practice stacks 2–4 layers (sub, main, top), keeps everything under about 200 Hz mono, and shapes the bass to the kick (C).
   - Add a bright top layer (FM or formant, 1–4 kHz) that bounces with the pitch dip.
7. **A short, bright, pumped plate** instead of the 2.6 s hall: 1.2–1.8 s, with a 1/64-note pre-delay (29 ms at 128 BPM). The returns already pump 4 dB (C).

Keep as they are:
- the pump (the stab 9 dB, 200 ms release: the style's audible pump)
- the kick's length (−20 dB at 225 ms, by the offbeat)
- the tempo

### FUT04 "Furnace Code": dark industrial techno, 136 BPM, E minor

1. **Density, then a club master.**
   - Gap: −17.0 LUFS-I and PLR 14.4–15.9, against −8 ± 1 and 6.5–8. It needs 12 to 15 dB of gain.
   - A plain limiter would need at least 12.3 dB. With the buses clipped first, 8.2.
   - Saturation belongs to this genre: a distorted drum bus (multi-stage, with EQ between the stages) and a clipper before the limiter (C).
2. **Kick weight where speakers play.**
   - Gap: the E1 kick (41.2 Hz) puts its sub under 44 Hz. Its 63 Hz octave reads +0.1 against +7.8 for the 31.5 Hz octave, and the third octaves 62–125 Hz read −2 to −9 dB.
   - Punch at 60–120 Hz and a click at 2–5 kHz are what make a kick read on small speakers (C). Round 1's lesson asks for body you can hear on small speakers; there it was about the bass.
   - Do: a knock at 80–120 Hz and distortion harmonics over the kick's first 50–60 ms; the gate's falls still apply.
   - Or: an A/B with the kick tuned up to G1 or A1. That is a DNA change (`KICK_NOTE`) with its own commit.
3. **Fill the low mids: the deepest hole of the four** (−10.6 dB at 125 Hz). The kick's knock (change 2) and the warm acid B (+1.2 / +1.4 dB) fill most of it.
4. **The rumble as the groove.**
   - Gap: the rumble sits 8 LU under the kick and plays only in bars 3–4.
   - Current techno runs a reverb-fed, distorted, low-passed rumble under the whole groove, ducked to every kick (C).
   - Raise it to about 4–6 LU under the kick, in all four bars, keeping the loop gap checks.
   - Round 3 found the LU figure at odds with "the kick leads" for a rumble that is all sub: at 4–6 LU under, the low end peaked after the kick in every loop. FUT04 now reads the rumble against the kick's own peak in 35–60 Hz instead (8.4 dB under it, 13 LU under the kick by LUFS), which is the measure to use for a sub-only rumble.
5. **A techno space.**
   - Today the shared Space runs 2.6 s, damped at 6.5 kHz.
   - Make the kit's "dark concrete" space a 4–6 s hall with a darker tail (damped at 3–4 kHz). Send the stabs and the percussion hard, and duck it to the kick (C).
6. **Tops that cut.**
   - Gap: 16 kHz −17.3, at the edge of the techno band; 8 kHz −9.2; the hats' centroid is 8.7 kHz, the darkest of the four.
   - Do: add a ride or a distorted open hat on the offbeats, and lift the hats 1–2 dB.
   - Target: 8 kHz −8 to −6, 16 kHz −14 to −11.
7. **Let the kick lead.**
   - Gap: the lanes sum to 6.0 LU over the kick, against 4.4–4.7 in the other kits. The chops sit level with the kick, the robot 0.5 LU under it.
   - Do: bring the vocals down 2–3 LU and process them (distortion, a band-pass, short delays) so they still read without competing with the kick.
8. **Acid with an edge.** Distortion after the filter, and a cutoff and resonance ride across the four bars (C).

### Shared changes (tools and QA)

- **S1. A shared club master:** `tools/sound/master.py`, deterministic and 4× oversampled. A drum-bus clip, glue compression, then the master's clip and limiter. Each kit gets a mastered context loop, and its settings go into the metadata as the target for the engine's master bus (S1e). **Done in round 3** (ADR 0014, proposed): every kit's `ContextLoopClub`.
- **S2. Report-only QA for the mastered loops first:**
  - LUFS-I, PLR and PSR in the genre's band
  - the octave shape (the low-mid hole, the top slope)
  - width per band
  - the kick and clap jumps against the dry mix
  - the backbeat's peak against the kick

  Each becomes a gate, with a fixture pair like every other limit, once it is calibrated by your listening and by S4. **Done differently in round 3** (ADR 0014, proposed): loudness, PLR, the true peak, the octave windows, the low-mid width, the master's cap and the crest it costs the hits are gates now, each with its fixture pair; the octave windows, the conventions among them, can be turned off or exempted band by band on a club loop with a reason. PSR and the shape checks (the low-mid hole, the top slope) stay in the report, the jumps and the backbeat's peak in the meters and the kits' pages, not gated.
- **S3. Sends per kit.** The `sends.py` defaults stay, and each kit's context loop passes its own Space and Echo (T60, damping, pre-delay in note values).
- **S4. Measured references replace the conventions.** Take 3–5 released tracks per genre that you own, and measure each one's loudest 16 s: `.venv/bin/python -m tools.sound.mixmeter --reference <file> ...` (44.1, 48 or 96 kHz WAV or FLAC, resampled to 48 kHz for measuring only; `--json` for the numbers). Then record the numbers here. The audio itself never goes in the repo.
- **S5. The meter into the repo: done** as `tools/sound/mixmeter.py` (round 3), with its validation cases as unit tests (`tools/sound/tests/test_mixmeter.py`), the reference measurement included.

**Round 4: a fifth genre, garage-house / deep tech house.** FUT03 moves from hyper-clean future house to today's garage-house and rolling deep tech house, on your request (round 4 moves FUT01 and FUT03 toward current UK garage and garage-house; FUT02 and FUT04 stay). Its club windows are conventions like the others, set warmer and less bright than future house: LUFS-I −8.5 ± 1, PLR ≥ 7.0, the octaves sub +6 to +12, 125 Hz +1 to +6, 250 Hz 0 to +4, 500 Hz 0 to +3, 2 kHz −3 to 0, 4 kHz −5 to −2, 8 kHz −8 to −4, 16 kHz −14 to −8 (`qa.CLUB`, pinned in `test_qa.py`). Its extended mix plays the house form, the kick staying in the build. Future house's windows and form stay valid for a kit that takes that direction up again.

**Proposed for round 4, not gated yet: genre targets that separate the kits.** The A&R review of round 3 found the near-identical windows pulling the kits toward one sound: FUT02 and FUT03 read within 1.8 dB of each other in every octave, and FUT02 is as wide as FUT04 from 500 Hz up. It proposed sci-fi club width −7 to −3 dB at 2–8 kHz from real decorrelation, techno drums −12 to −8 with wide returns, future house the brightest top slope (−1.5 ± 0.3 dB/oct) and techno the darkest (−2.5 ± 0.4). These are conventions too, and gating them now would change the mixes after their round 3 fix pass; S4's measurements decide them, and your round 3 notes on how alike the kits sound.

## 4. What is not verified

- **Nothing here has been heard.** That includes whether a filled low-mid region reads as body or as mud, whether a −8 LUFS master sounds punchy or crushed, how the rebuilt clap sounds, and whether a wider FUT02 still sounds focused. Only your listening round can say.
- **The spectral, width and clap targets are conventions.** They are built on a measured chart-record average and on practitioner guidance, not on measured club records in each genre. S4 replaces them.
- **The loudness references are thin.**
  - Mixgraph's medians come from 96 kbps store previews and small samples, and its genre table could not be read directly.
  - iZotope's dance article is ten songs, reported second-hand; the −8.3 average first quoted for it belongs to iZotope's mostly-pop 2024 study (§1.1).
  - The Beatport techno figure comes with no method.
- **The projections (§2.6) use a scratch limiter and clipper.** A mastering chain will need less reduction for the same loudness. The direction and the ranking hold; the exact dB do not.
- **The click, jump and backbeat measures are our own,** uncalibrated against released records.
- **Round 2's A/B ratings are not in yet**, so "use B" in §3 is conditional.

## 5. Scratch tools, and the sources

**The scratch tools** were in this session's scratch folder (scratch, not kept). The meter and the reference measurement are now `tools/sound/mixmeter.py` (S5).
- `meter.py`: the meter.
- `validate_meter.py`: its Tech 3341, true-peak and noise checks.
- `measure.py`: the tables in §2, written to `measure.json`.
- `extra.py`: the low-mid hole, the third octaves and the lane levels.
- `peaks.py` and `backbeat.py`: where the loops peak.
- `project_master.py`, `project_bus.py` and `project_rest.py`: the master projections.
- `measure_ref.py`: S4's reference measurement, now `python -m tools.sound.mixmeter --reference`. It reads 44.1, 48 or 96 kHz files and resamples them to 48 kHz for measuring only.

**Standards:** ITU-R BS.1770-4 (loudness, true peak); EBU Tech 3341 (meter test cases).

**Measured references:**
- Mixgraph, the LUFS meter page and its genre table: https://www.mixgraph.io/tools/lufs-meter
- Mixgraph, tempo medians: https://www.mixgraph.io/bpm-for and https://www.mixgraph.io/bpm-for/tech-house
- iZotope, a LUFS and tonal analysis of 10 top-charting dance and electronic songs: https://www.izotope.com/en/learn/dance-electronic-analysis
- iZotope, mastering trends 2024: https://www.izotope.com/community/blog/mastering-trends
- Universe of Tracks, a data-driven look at the Beatport techno top 100: https://universeoftracks.com/data-driven-techno-analysis/
- Sound On Sound, Dynameter review (PLR and PSR examples): https://www.soundonsound.com/reviews/meterplugs-dynameter
- MeterPlugs, the Dynameter guide (the PLR and PSR definitions): https://www.meterplugs.com/files/dynameter-guide.pdf
- Pestana, Ma, Reiss, Barbosa and Black, "Spectral Characteristics of Popular Commercial Recordings 1950–2010", AES 135th Convention, 2013, paper 8960: https://www.aes.org/e-lib/browse.cfm?elib=17010
- Elowsson and Friberg, "Long-term Average Spectrum in Popular Music and its Relation to the Level of the Percussion", AES 142nd Convention, 2017, paper 9762: https://www.aes.org/e-lib/browse.cfm?elib=18638

**Conventions:**
- Loudness and true peak:
  - Spotify for Artists, loudness normalisation: https://support.spotify.com/artists/article/loudness-normalization/
  - Mastering The Mix, how loud to master (short-term targets): https://www.masteringthemix.com/pages/how-loud-should-you-master
  - Ask.Audio, loudness standards and club music: https://ask.audio/articles/how-loudness-standards-relate-to-edm-and-club-music-production
- Analyser tilt:
  - MusicRadar, what a spectrum analyser is: https://www.musicradar.com/tuition/tech/what-is-a-spectrum-analyser-600748
  - FabFilter forum, analyser slope: https://prod.fabfilter.com/forum/topic/1608/pro-q-feature-request-spectrum-analyzer
- Mono bass: MusicRadar, working with stereo bass: https://www.musicradar.com/how-to/how-to-break-the-rules-and-work-with-stereo-bass
- Kick frequency ranges:
  - Pause Play Repeat: https://www.pauseplayrepeat.com/blog/unraveling-the-kick-drum-a-guide-to-frequency-ranges
  - Masterchannel: https://blog.masterchannel.ai/music-production-tips-hotspots-for-your-kick
- Sidechain:
  - Producer School, mixing sub bass for tech house: https://theproducerschool.com/blogs/featured-blogs/how-to-mix-sub-bass-for-tech-house-without-it-disappearing-on-club-systems
  - The 5–7 dB / 1–2 ms / 100–120 ms figures come from a Producer School tech-house bassline tutorial whose address carries an artist's name, so it is not linked here.
  - iZotope, 9 tips for producing and mixing house music: https://www.izotope.com/en/learn/9-tips-for-producing-and-mixing-house-music
- Clipping:
  - Violet Recording, clipper plugins: https://violetrecording.com/best-clipper-plugins/
  - Mastering.com, intentional clipping: https://mastering.com/intentional-clipping-the-anti-compressor/
- Claps:
  - Attack Magazine, layering claps and snares: https://www.attackmagazine.com/technique/tutorials/layering-claps-snares-tutorial/
  - sequencer.de, the analogue clap's repeating envelope: https://www.sequencer.de/synthesizer/threads/jomox-mod-brane-11-kann-das-claps.97691/
- Reverb:
  - SoundBridge, reverb and delay tips: https://soundbridge.io/en/reverb-and-delay-tips-for-cleaner-deeper-mixes
  - NUGEN Audio, reverb techniques for electronic music: https://nugenaudio.com/reverb-techniques-electronic-music
  - Attack Magazine, dark techno rumble: https://www.attackmagazine.com/technique/beat-dissected/dark-techno-rumble/
- UK garage:
  - Attack Magazine, UK garage beat dissected: https://www.attackmagazine.com/technique/beat-dissected/uk-garage/
  - Mixgraph, UK garage mixing guide: https://www.mixgraph.io/mixing-guide/uk-garage
  - Sample Sound, UK garage in 2025: https://www.samplesoundmusic.com/blogs/news/uk-garage-in-2025-production-tips-for-modern-underground-grooves
  - MusicRadar, a UK garage beat: https://www.musicradar.com/how-to/uk-garage-tutorial
  - Native Instruments, UK garage: https://blog.native-instruments.com/uk-garage-music/
  - Wikipedia, future garage: https://en.wikipedia.org/wiki/Future_garage
- Future house: EDMProd, future house tips: https://www.edmprod.com/36-tips-from-future-house-artists/
- Techno:
  - Future Proof Music School, hard techno: https://futureproofmusicschool.com/blog/making-hard-techno-a-path-to-unique-sound-design
  - Seedj, hard techno production guide: https://www.seedj.com/learn/hard-techno-production-guide
  - Universe of Tracks, techno kick mixing: https://universeoftracks.com/techno-kick-mixing-techniques/
- Extended mixes: Mixed In Key, arranging a dance track: https://mixedinkey.com/captain-plugins/wiki/how-to-arrange-a-dance-music-track/ (a second source on DJ-friendly intros is not linked, because its name is an artist's)

**This project:**
- ADR 0007 (the master spec), ADR 0009 (QA and loudness), ADR 0011 (the kick crest and bass-body)
- `docs/plans/S1-four-kits.md`
- `docs/listening.md`, round 1
- each kit's `tools/sound/recipes/<kit>/context.py`
- `tools/sound/sends.py`
- `build/<kit>-r2/qa_report.json`
