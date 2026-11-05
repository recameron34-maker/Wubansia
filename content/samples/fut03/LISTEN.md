# FUT03 "Glass Current": round 3, the modern production pass

Written for the kit's render folder (`build/fut03-r3`, from `render.py --kit FUT03 --set r3`), which holds the FLAC masters, `qa/`, `sheets/`, `qa_report.md`, `qa_report.json` and `listen.json` this page names; its copy in `content/samples/fut03/` sits beside the WAVs alone.

A hyper-clean future house kit at 128 BPM in G minor (6A): a split kick, a clap, choked hats and a bouncy metallic bass that dips in pitch on every note, under a supersaw Gm9 stab that pumps with the kick and a glass pluck up front. Every metallic sound rings on one tuned glass bell. Round 3 keeps round 2's parts and lines. It rebuilds the clap, gives the kick its clear click, adds a glass-bead shaker and a bright top on the bass, and mixes the loop on buses. It also gives the kit its own plate and adds a loop mastered to the genre's loudness.

An A&R review of the first round 3 render found three problems, and this render fixes them: the master clipped a vocal chord hardest, the bass's new top was buried in the mix, and the pluck stood in the clap's way. See "The review's fixes" below.

**Nobody has listened to round 3.** Every number below is a measurement made in this session; none of it says how it sounds.

## Start here

`Loops/Context Loop Club.wav`: the wet context loop through the club master. Then the dry loops (A and B, same loudness) and the wet one, then the sounds that changed.

| Club master | Reading | Future house target |
|---|---|---|
| Loudness, LUFS-I (round the loop) | -8.46 | -8 ± 1 |
| True peak | -1.02 dBTP | ≤ -1.0 |
| PLR / PSR | 7.44 / 7.02 dB | ≥ 6.5 / ≥ 6 |
| Short-term max (3 s) | -8.04 LUFS | a 4-bar loop: its LUFS-I plus a few tenths (+0.42 here; reported, not gated) |
| Octaves re 1 kHz: sub, 125, 250, 500, 2k, 4k, 8k, 16k | +9.6 (in the 63 Hz octave), +1.3, +1.9, +2.3, -1.4, -3.5, -4.9, -7.8 dB | every one inside its window (`qa.CLUB`), the closest (125 Hz) 0.28 dB inside |
| Top slope, 500 Hz-8 kHz | -1.80 dB/oct | -1.75 ± 0.5 |
| Side over mid at 125 / 250 Hz | -43.7 / -14.6 dB | ≤ -10 |
| Width at 2 / 4 / 8 / 16 kHz | -8.3 / -5.8 / -6.7 / -5.6 dB | -10 to -4 (2-4k), -8 to -3 (8-16k) |
| Side under 120 Hz | -53 dB | ≤ -20 |
| Peak reduction (clipper + limiter) | 5.0 dB (the clipper 3.5, the limiter 1.5 at most) | the standard's convention: ≤ 4 (see below) |
| Kick jump in the loop, full band / 40-120 Hz | 8.2 / 13.5 dB (unmastered wet: 8.7 / 16.6) | the master costs it at most 3-4 dB |
| Clap jump, 1-5 kHz | 11.9 dB (unmastered wet: 12.4) | ours: ≥ 12 (met in the mix; the master costs 0.5) |
| Crest per kick / per clap (median), unmastered wet to club | 7.94 to 5.72 / 8.20 to 6.23 dB: -2.2 / -2.0 (the medians' difference; QA gates the median of each hit's own loss, 1.86 / 1.97) | ≥ 4 on the kick (the floor's intent) |

**The one deviation from the standard: the master's cap, 5 dB.** Its convention is at most 4 dB of peak reduction. At 4 dB this loop stops at -9.23 LUFS-I, outside the window. So `ContextLoopClub` lets the master use up to 5 dB (`context.MASTER_REDUCTION_DB`), and it stops there at -8.46, 0.46 LU short of -8 and inside the window; the master's report says so.
- **It was 7 dB in the first round 3 render,** the heaviest master of the four kits. What set that was one tonal peak, the robot's "the" through the music bus's low-mid lift (fix 1 below). With that peak gone, 5 dB is enough.
- **Why the mix still needs more than 4.** The arrangement: a kick on every beat carries both the loop's peaks and much of its loudness, the bass rests on every kick, and the parts are short and plucked (stab, pluck, bass bite, shaker). The cap now binds on the kick and the offbeat stacks together: the six deepest 16ths sit within 0.12 dB of each other, and the deepest is the kick with the clap on beat 2, a transient.
- **What was tried and not taken:** a sustained Gm9 pad bed pumped like the stab, the review's proposal for density. At the same loudness it buys a tenth or two of a dB of crest (measured, below).
- **What the cap costs.** Against the unmastered wet loop, the kicks' median crest falls 2.2 dB and the claps' 2.0 dB (the medians' difference; the median of each hit's own loss, which QA gates, is 1.86 and 1.97 dB); the kick's jump loses 0.5 dB (full band) and 3.1 dB (40-120 Hz), the clap's 0.5 dB: inside the standard's "at most 3-4 dB". At the standard's 4 dB the crests would lose 1.4 and 1.2 dB, with the loop out of the window.
- **The engine's master must match it.** The report records the cap.

## The review's fixes

An A&R review of the first round 3 render made three findings about this kit. Each was measured again on the recipes before anything changed (its evidence reproduced, to the tenth of a dB on the master's clipper and crests), then fixed and measured in the full loop, with the sends hearing the changed lanes. The club master, first render against this one:

| | First round 3 render | Now |
|---|---|---|
| Club master, LUFS-I | -8.20 (cap 7 dB) | -8.46 (cap 5 dB) |
| Peak reduction (the clipper's part) | 7.0 dB (5.4) | 5.0 dB (3.5) |
| The master's deepest clip | 16th 51, the robot's "the": 8.0 dB over the clipper's knee, 1.4 dB over the loudest kick | the kick and clap on beat 2: 6.0 dB; 16th 51 at 5.9 |
| The clip's error in 1-6 kHz against the music, worst 16th / median | -12.4 / -24.3 dB | -14.7 / -26.2 dB |
| 120-500 Hz peak per 16th (wet) | 16th 51 2.9 dB over every other 16th | the top three within 0.4 dB |
| Crest per kick / clap, wet to club (the medians' difference) | -2.83 / -2.55 dB | -2.22 / -1.98 dB |
| Kick jump, full / 40-120 Hz, wet to club | 8.6 to 7.7 / 16.6 to 13.5 dB | 8.7 to 8.2 / 16.6 to 13.5 dB |
| Clap jump, wet to club | 10.8 to 10.1 dB | 12.4 to 11.9 dB |
| Backbeat over a kick alone (wet) | +1.30 dB | +0.98 dB |
| The bass under the mix in 2-4 / 1-2 kHz while it plays | 10.6 / 6.7 dB | 5.5 / 4.2 dB |
| 125 Hz octave re 1 kHz (window +1..+5) | +1.3 | +1.3 |

**1. The master clipped a vocal chord hardest.**
- **The finding:** the "hyper-clean" kit got the heaviest master of the four, and its deepest clip was a low-mid peak of the vocoder phrase ("let the glass drop", a G minor chord from G3) through the music bus's +4 dB at 150 Hz: in-band distortion on a sustained chord.
- **Verified.** The clipper took 5.43 dB, with 3.05 % of the 4x samples over its knee. 16th 51 was the deepest clip, 8.0 dB over the knee, 51 % of its peak value in 120-500 Hz, and 74 % of that band's peak was the robot. The clip's 1-6 kHz error sat 12.4 dB under the music there. The cause is narrower than the finding: the robot file itself has a 20-30 ms swell at the end of "the" (a 19 dB crest in 120-500 Hz, 2.7 dB over the phrase's other syllables there), and at the robot's level in the loop that swell is the loop's single highest peak. The lift is a small part of it: taking the lift away alone moved that peak 0.06 dB.
- **Fixed:**
  - the vocal bus (the chop and the robot) takes the music bus's EQ without its 150 Hz body lift (`context.VOCAL_EQ`);
  - a fast peak limiter on the robot lane after its compressor (`context.ROBOT_LIMIT`: 4 dB under the lane's own peak, 5 ms lookahead and hold, 50 ms release), so "the" comes down to the other syllables; the words' 1-4 kHz moves 0.5 dB;
  - the cap from 7 to 5 dB (above);
  - the music bus's body lift from 4 to 5 dB, which now reaches only the stab, the fill, the pling and the pluck: with the vocals gone from it and the bass's top louder, the 125 Hz octave had fallen to 0.09 dB inside its window, and it is back at 0.28.
- **Done differently:** the review's other route, a 180 Hz high-pass and -4 dB at 200 Hz on the robot, reached the same master only with the lift gone too. Against the first render's chain it cost the robot 7.9 dB of its 150-300 Hz and 2.8 LU in the mix; this route costs 3.8 dB and 2.2 LU, and the words' 1-4 kHz 0.5 dB.
- **Not done:**
  - **the pad bed.** A Gm9 pad, five saws a note, low-passed at 1.6 kHz and pumped 9 dB like the stab, on this mix at the 5 dB cap. Voiced as the stab and 5 LU under it, it lowers the crest costs to 1.6 / 1.5 dB, but mostly because its own peaks in bar 4 now bind the cap: the master stops at -8.88 LUFS-I, 0.12 LU inside the window. Without the pad, at that loudness (a 4.4-4.5 dB cap), the costs are 1.7-1.8 / 1.5-1.6 dB, so the bed's own gain is about 0.1-0.2 dB of crest. It also adds 0.6 dB to the 250 Hz octave, and from 1 LU under the stab up it takes that octave out of its window. Voiced without its root, it moves the master's loudness by -0.03 to +0.13 LU and lowers its crest costs by 0.05-0.25 dB, and from 1 LU under the stab up it fails the band windows too. It was not added: it is a sustained layer the brief doesn't have (the chord is a short, sidechained stab), for a gain of a tenth or two of a dB.
  - **≤ 1.5 dB of crest loss per kick and clap.** It needs the 4 dB cap (1.4 / 1.2 dB), where the loop stops at -9.23 LUFS-I, out of the window.

**2. The bass's top was buried.**
- **The finding:** the dip bass's "yow", the top layer that is the genre's signature, was masked in the mix by the pluck, the stab, the clap and the vocals, and the music bus's +4.5 dB at 2.8 kHz lifted all of them but not the bass.
- **Verified.** While the bass played (42 % of the loop), it sat 10.6 dB under the mix in 2-4 kHz and 6.7 dB under in 1-2 kHz; the pluck (-5.5), the stab (-7.0) and the chop (-7.8) owned that band. The line's own 2 / 4 kHz octaves sat 21 / 24 dB under its sub.
- **Fixed:** the top layer 6 dB up in the voice itself (`bass.TOP_LEVEL`, 0.9 to 1.8, after its drive, so only its level moves), so the multisamples and both lines have it. While the bass plays it now sits 5.5 dB under the mix in 2-4 kHz (the review's target: within about 6 dB). The lines' 2 / 4 kHz octaves read 16 / 18 dB under the sub (round 2: 23 / 34). The louder top lowered the lines' renderer gain (B 0.55 dB, A 0.67), so B's fader is up by that and its sub sits where it was; A is matched to B, both at -15.07 LUFS in the loop.
- **Not done:**
  - **the +6 dB shelf from 1.2 kHz on the bass lane,** the review's other route: it also lifted the bite's glass tine and 4-8 kHz, left A and B 0.13 LU apart, and took the 125 Hz octave out of its window;
  - **a bass-keyed -3 dB dip at 1.5-3 kHz on the pluck and the stab:** the target was met without it, and the bass plays almost every offbeat, so the dip would dull the pluck, the kit's lead, most of the time;
  - **"keep the bass out of the 2.8 kHz lift":** it already was (the bass bus never reaches the music bus).

**3. The clap didn't cut through.**
- **The finding:** the clap jumped 10.8 dB over the wet mix (the standard: at least 12), the backbeat peaked 1.30 dB over a kick alone (at most about 1), and the pluck and the chop sat just before 2 and 4.
- **Verified.** In each clap's reference window (1-5 kHz, 60-10 ms before it) the pluck was 1.0-3.3 dB under the whole mix there (most of it: its notes on the "a", 110 ms ahead, ring through), and at the clap on 16th 36 the chop was nearly all of it. Without the pluck the clap would jump 17.0 dB.
- **Fixed:** the clap's room (`context.CLAP_ROOM`): the pluck's 1-5 kHz ducked 3 dB from 60 ms before to 80 ms after each clap of 0.9 or more, with 10 ms raised-cosine ramps, keyed to the written claps as the pump is to the kicks. A note's attack, its first 40 ms, is untouched; only its top then falls 3 dB faster, and comes back 80 ms after the clap. The clap jumps 12.4 dB, and the backbeat reads +0.98 dB, because the pluck was also adding to the backbeat's peak.
- **Not done:**
  - **the chop's duck:** "cur-" of "the current" lands on the clap at 16th 36, so its consonant is what would be ducked; the clap there still jumps 7.6 dB (it did before), and the median meets the target without it;
  - **the 0.5 dB clap trim:** the backbeat reaches +0.98 without it; on its own it brought the backbeat only to +1.23 and cost 0.2 dB of jump.

## What changed since round 2, and why

The standard (`docs/plans/S1-modern-sound.md`) measured FUT03's round 2 loop against current future house records. It found the farthest gap to the master and the darkest top of the four kits, and listed seven changes. They were made in its order, each measured before the next. The wet loop, normalised to -1.02 dBTP as rendered; "club at 4 dB" is what the club master reaches at the standard's cap, with the reduction the -8 target needs in brackets:

| After | Wet LUFS-I / PLR | Club at 4 dB (reduction for -8) | Holes 125 / 250 | 125 / 250 / 500 re 1k | 2k / 4k / 8k / 16k re 1k | Width 2k / 4k / 8k | Kick jump full / 40-120 | Clap jump | Backbeat over a kick |
|---|---|---|---|---|---|---|---|---|---|
| round 2 | -16.05 / 15.03 | -12.72 (9.70) | -8.2 / -5.3 | -4.6 / -3.5 / -0.0 | -6.3 / -11.1 / -13.2 / -15.8 | -9.7 / -9.2 / -7.1 | 9.5 / 19.7 | 10.1 | +1.82 |
| 1 density: the buses and the master | -13.35 / 12.33 | -10.49 (7.12) | -8.2 / -5.3 | -4.5 / -3.5 / -0.1 | -6.3 / -11.0 / -13.2 / -15.7 | -9.6 / -9.2 / -7.0 | 9.1 / 19.7 | 9.8 | +0.39 |
| 2 a brighter top: hats, shaker, air | -13.54 / 12.52 | -10.19 (6.88) | -8.2 / -5.3 | -4.5 / -3.5 / -0.1 | -6.3 / -10.3 / -7.2 / -9.7 | -9.6 / -8.9 / -6.8 | 9.0 / 19.7 | 9.7 | +0.42 |
| 3 the clap rebuilt | -13.45 / 12.43 | -10.11 (6.83) | -8.0 / -5.1 | -3.9 / -3.0 / -0.0 | -6.1 / -7.2 / -6.3 / -8.9 | -7.6 / -4.6 / -6.5 | 9.6 / 19.7 | 11.1 | +0.86 |
| 4 the kick's click | -12.35 / 11.33 | -10.07 (6.81) | -8.1 / -5.1 | -3.9 / -3.1 / -0.1 | -5.8 / -7.2 / -6.2 / -8.7 | -8.0 / -4.6 / -6.5 | 9.7 / 20.0 | 11.3 | +1.98 |
| 5 the low mids | -12.50 / 11.48 | -9.82 (6.47) | -7.2 / -4.3 | -3.2 / -2.3 / -0.1 | -5.8 / -7.1 / -6.1 / -8.7 | -7.9 / -4.6 / -6.6 | 8.8 / 17.9 | 11.2 | +0.99 |
| 6 the bass's top layer | -12.50 / 11.48 | -9.84 (6.50) | -7.2 / -4.3 | -3.3 / -2.4 / -0.1 | -5.7 / -7.1 / -6.1 / -8.8 | -8.0 / -4.7 / -6.6 | 8.8 / 18.1 | 11.2 | +0.99 |
| 7 the plate and the 8th echo | -12.25 / 11.23 | -10.27 (7.09) | -7.1 / -4.2 | -2.2 / -1.3 / +0.8 | -4.6 / -5.9 / -5.0 / -7.6 | -7.7 / -4.6 / -6.5 | 9.8 / 18.1 | 11.9 | +1.23 |
| the balance (music bus EQ, levels) | -12.42 / 11.40 | -10.51 (7.27) | -6.1 / -2.7 | +1.2 / +2.2 / +2.5 | -1.4 / -3.4 / -4.9 / -7.8 | -7.9 / -5.2 / -6.8 | 8.6 / 16.6 | 10.8 | +1.30 |
| the review's fixes (above) | -12.54 / 11.52 | -9.23 (5.62) | -5.9 / -2.8 | +1.1 / +1.8 / +2.2 | -1.5 / -3.3 / -5.1 / -8.0 | -8.3 / -5.7 / -6.8 | 8.7 / 16.6 | 12.4 | +0.98 |

Targets:
- Bands re 1 kHz: 125 +1..+5, 250 0..+3, 500 0..+3, 2k -2..+1, 4k -4..-1, 8k -7..-3, 16k -12..-7.
- Holes ≥ -4: a shape check, reported, not gated.
- Width -10..-4 at 2-4 kHz, -8..-3 at 8 kHz.
- The backbeat no more than about 1 dB over a kick on its own (ours).

**1. Density, then a master.** Round 2's loop was lanes summed, at a PLR of 15.0 dB. The clap lane alone peaked 3.4 dB over the kick lane, and with the kick under it on 2 and 4 it set the loop's peak. Round 3:
- clips the drum bus (kick, clap, hats, shaker) at the kick lane's own true peak;
- compresses the chop and the robot (4:1, 6 dB at their loudest), made up to their loudness;
- gives the kick its room: the vocals, pling and fill duck 3 dB under each kick, back by 117 ms;
- adds `ContextLoopClub`.

The wet PLR went from 15.0 to 12.3 and the backbeat from +1.8 to +0.4 dB. The pump stays as round 2 set it (the stab 9 dB, the pluck 3 dB, the returns 4 dB), as the standard asked.

**2. A brighter top.**
- The hats are 3.5 dB louder and the new shaker plays the 16ths. The final balance trimmed both: the hats end 2 dB over round 2 and the shaker sits 8.8 LU under the kick.
- A 3 dB shelf from 7 kHz on the pluck and a 4 dB shelf from 8 kHz on the vocal bus give them air.

8 kHz went from -13.2 to -7.2 and 16 kHz from -15.8 to -9.7 against 1 kHz, and the top slope from -3.3 to -1.8 dB/oct (target -1.75). 4 kHz needed the clap.

**3. The clap, rebuilt** (below): it now peaks 6 dB less for its loudness and carries the 2-5 kHz snap. 4 kHz went to -7.2 and the clap's jump from 9.7 to 11.1 dB. The pluck came down 1.5 dB: its notes before 2 and 4 filled the clap's reference window (1-5 kHz, 60-10 ms before each clap). The review's clap room finished that job (above).

**4. The kick's clear click** (below): the wet PLR dropped 1.1 dB, because the kick now reads louder for its peak.

**5. The low mids.**
- The kick's punch: a longer glide through 100-150 Hz.
- The stab voiced from D3.
- 2.5 dB more of B's warm body in the lines.

The holes at 125 / 250 Hz went from -8.1 / -5.1 to -7.2 / -4.3 dB.

**6. The bass's top layer** (below): a timbre change. At first the line's 4 kHz octave was 10 dB higher against its own 1 kHz, but in the mix the bass sat under the clap and the pluck there, so the loop's bands hardly moved. The review's fix raised it 6 dB more (above).

**7. The plate.** The kit's own Space instead of the shared one:
- 1.5 s (shared: 2.6 s);
- a pre-delay of a 64th note (29.3 ms; shared: 20 ms);
- dense (small lines), damped only from 12 kHz (shared: 6.5 kHz);
- its send high-passed at 600 Hz and its low band gone in 0.5 s;
- pumped with the returns.

The Echo is a ping-pong on the 8th instead of the dotted 8th. Two 16ths keep every repeat on the kit's swing, where a dotted 8th lands an unswung 16th off the grid. It is also brighter and thinner: 600 Hz-7 kHz (shared: 300 Hz-2.6 kHz). The shared hall had been the wet loop's darkest part, so the wet loop's 2k / 4k / 8k rose 1.1 / 1.2 / 1.1 dB.

**The balance.** Measured on the club master, against the genre's band windows:
- **The music bus EQ** (everything but the drums, the bass and, since the review, the vocals; and the returns):
  - -3.5 dB at 900 Hz, where the pluck, pling, vocals and returns all sat;
  - +4.5 dB at 2.8 kHz;
  - +5 dB at 150 Hz (4 dB before the review's fix).
- **The vocal bus** takes the same EQ without the 150 Hz lift.
- **Levels:**
  - the kick 1 dB up (it carries the 125 Hz octave and the click's 2 kHz);
  - the bass 2.5 dB up, both characters; with the review's louder top it reads 0.5 LU under the kick;
  - the stab 1.5 dB up;
  - the hats and shaker trimmed back so 16 kHz stays in its window.

Every octave of the club master sits inside its window.

## The sounds

| # | Sound | Round 3 | Files |
|---|---|---|---|
| 1 | Split kick on G1 | changed: the click, the punch | `Drums/Kick Split` |
| 2 | Clap | changed: rebuilt | `Drums/Clap` |
| 3 | Hats and shaker | the shaker is new; the hats are **unchanged** (the same bytes) | `Drums/Shaker`, `Drums/Closed Hat`, `Drums/Open Hat`, `Loops/Hat Loop` |
| 4 | Metal pling | **unchanged** (the same bytes) | `Drums/Metal Pling G4` … |
| 5 | Dip bass and its lines | changed: the top layer (6 dB louder since the review), B's body in the lines | `Loops/Line Bounce …`, `Loops/Line Roll …`, `Bass/Dip Bass Clean`, `Bass/Dip Bass Warm` |
| 6 | Vowel bass | **unchanged** (the same bytes) | `Bass/Vowel Bass …` |
| 7 | Chord stab, Gm9 | changed: voiced from D3 | `Music/Stab Gm9`, `Loops/Stab Pump` |
| 8 | Glass pluck and its phrase | **unchanged** (the same bytes; the air and the clap's room are the loop's) | `Loops/Glass Pluck Phrase`, `Music/Glass Pluck …` |
| 9 | Sung chops | **unchanged** (the same bytes) | `Vox/Sung Line 1` … |
| 10 | Vocoder phrase | **unchanged** (the same bytes; the peak limiter is the loop's) | `Vox/Vocoder Drop`, `Vocoder Drop Wet` |
| 11 | Build fill | changed: its claps are the new clap | `FX/Build Fill`, `Loops/Fill Preview` |
| 12 | Riser and impact | **unchanged** (the same bytes) | `FX/Riser`, `FX/Impact` |

What changed, sound by sound (each measured against its round 2 file, the top layer):
- **Kick.**
  - **The click.** The glass ring on the click moves up a fifth, from G6 to D7 (its modes ring D, F and A, in the key), so all of it lands in the 2-8 kHz the click is read in. The top is 12 % louder. The knock rises over 2 ms, so the click owns the first milliseconds. The standard's click measure (2-8 kHz in the first 10 ms against 30-120 Hz in the first 100 ms) went from -23.3 / -23.0 to -17.5 / -17.4 dB: the brief's "clear click", the standard's "-18 or more".
  - **The punch.** The sweep's glide is slower (18 ms instead of 13 ms), so it holds 100-150 Hz longer: 100-150 Hz against 30-100 Hz over the first 50 ms went from -7.7 to -5.9 dB.
  - **What stayed.** The sub is unchanged: 20 dB down at 227 ms (round 2: 224), by the offbeat 8th. The crest, true peak over the loudest 50 ms, is 6.08 / 6.54 dB (round 2: 6.02 / 6.36; Split, your pick, read 5.8-6.4; Tight 7.9-8.9). The 150-400 Hz share is -13.8 dB (gate: ≤ -12).
- **Clap.** Rebuilt as four hands of band noise in 23-31 ms, the first the loudest (each hand's level is exact, so every take has the same shape), with a stick on it, plus:
  - the glass tone on D5, struck in phase on every hand;
  - a short glass body on G3 (the bell's prime and quint, 196 and 294 Hz);
  - air over 6 kHz;
  - a bright room after a 12 ms pre-delay;
  - a gentle 4x tanh for the crest.

  | Top layer, rr1 / rr2 | Round 2 | Round 3 | The standard |
  |---|---|---|---|
  | First hand, dB re the loudest | -8.0 (its second at 11.5 ms the loudest) | 0.0 / -0.2 | within 2 dB |
  | Crest (true peak over the loudest 50 ms) | 18.4 / 18.5 dB | 12.5 / 13.3 | about 12 |
  | Body, 150-400 Hz share | -32.0 / -32.7 dB | -12.2 / -13.2 | -15 to -8 |
  | Body gone (150-400 Hz down 20 dB) | 23 ms | 83 / 93 ms | within 150 ms |
  | Tail, 20 dB down | 73 / 68 ms | 136 / 136 ms | 120-250 ms |
  | 2-5 kHz share | -3.3 / -2.6 dB | -2.7 / -2.9 | more snap |
  | Side over mid | -9.1 / -8.5 dB | -4.5 / -4.6 | |
- **Shaker (new).** Glass beads: each stroke is a cloud of 70-180 tiny struck beads ringing the bell's partials over G8, under a millisecond each, their density following the stroke's swell and fall, over a soft "sh" through the shell. Each bead sits at its own place across the field.
  - 4 layers and 4 round robins, which agree within 0.24 dB.
  - The hardest stroke is 20 dB down at 69 ms, inside a 16th (117 ms).
  - Its centroid is 10.4 kHz and its side sits 7.9 dB under its mid.
  - In the loop it plays every 16th, the swung "e" and "a" leading, 8.8 LU under the kick.
- **Dip bass.**
  - **The top layer.** A saw and an FM pair (the modulator at 3x: the harmonics 1, 2, 4, 5, 7, 8…) on the sub's phase, through a formant at 2-2.8 kHz that falls an octave with each note's pitch dip and springs back with it, so every note says "yow" up top; then a soft drive at 4x. It rides on the bite, from 480 Hz, so A and B share it. Line Bounce B's octaves against its own sub (63 Hz): 2k -23.3 to -16.1 dB and 4k -34.1 to -17.7 since round 2 (the first round 3 render: -21.2 and -23.6); against its own 1 kHz, 4k -22.2 to -7.8.
  - **B's body.** It plays 2.5 dB louder in the lines, so Line Bounce B's 250 Hz octave rose from -0.8 to +1.0 dB re 1 kHz in the first round 3 render. Over a saw it reads 2.5-3.1 dB (bass-body allows 5). The held B notes keep round 2's body: at the lines' level, D2 read 6.6 dB over a saw, past the rule.
- **Chord stab.** Gm9 in thirds over the fifth below, D3 (147 Hz): the standard's "voice the stab from about 150 Hz". Its 125 Hz octave against its own 1 kHz went from -10.0 to +3.2 dB. It is mono up to 300 Hz (round 2: 150), so its low voices sit in the middle: side over mid at 125 Hz from -19.3 to -56.2 dB. The root is on pitch: QA reads +0.6 to +2.0 cents (its band reaches down to the D3), and the kit test, in a band that leaves D3 out, 0.03-0.08.
- **Build fill.** The same toms and rhythm; its claps are the new clap.
- **Unchanged, the same bytes as round 2:** the hats and the hat loop, the pling, the pluck and its phrase, the vowel shots, the riser and the impact, the three sung lines and their seven chops, and both vocoder files. Of the render's 175 files (with the QA's groove loops), 153 are the same bytes as the first round 3 render's; the 22 that changed are the dip bass's 12 multisamples, the 4 lines and the 6 context loops.

## The bass A/B

Unchanged in principle: A clean (the strict mud gate, its mids from 480 Hz) against B warm (the proposed bass-body rule, its mids from 120 Hz). They play the same lines, level-matched in the loop: both lanes read -15.07 LUFS, within 0.01 LU. Both get the same top layer; B also gets 2.5 dB more body in the lines. Over the kit's kick:
- A, `Context Loop Low`: -22.6 dB in its worst gap, against the gate's -20 (round 2: -22.7).
- B, `Context Loop Low Warm`: -5.9 dB in its worst gap, against bass-body's -2 (round 2: -7.7); +0.1 dB over a saw (limit +5).

The wet loop and the club master use B, the designer's pick for its body. It's a pick, not a verdict.

## What to listen for

- **The club master first.** Is it loud, bright and clean, and does the kick still punch through it, or is it squashed? The master takes 5 dB off the peaks, over the standard's 4 dB convention (above), down from 7 in the first round 3 render.
- **The bass's top.** It is 6 dB louder than in the first round 3 render: does every note's "yow" read on small speakers, bouncy and current, or is it harsh or does it fight the pluck? A or B?
- **The clap against the pluck.** Just before beats 2 and 4 the pluck's top now dips 3 dB so the clap cuts through: does the clap land crisper, and is the dip audible as a dip in the pluck?
- **The robot.** It comes through a peak limiter and without the low-mid lift: are "let the glass drop" clear, and is it too thin or audibly limited on "the"?
- **The kick's click.** Is it clear and glassy without sounding hard? Its crest stays near Split's, but the click is 5-6 dB louder than round 2's.
- **The clap.** Four hands, a body and a room: crisp and full, or too wet?
- **The shaker.** Does the glass-bead 16th drive the groove, or is it too busy with the hats?
- **The stab's D3.** Warmth or mud under the bass? Check once in mono at a moderate level; the music bus's 150 Hz lift on it is 1 dB more than in the first render.
- **The plate.** Is the 1.5 s bright plate clean, or does it wash the groove?
- **Mud.** Once more in mono: anything boomy or boxy between 150 and 400 Hz?

## QA

- **The render:** 44 items, 170 files, **0 failed checks** (`qa_report.md`).
- **Reproducible:** every item was rendered a second time in a fresh process (`--repro`), with the same SHA-256, all 44.
- **The club loop** passes every club check:
  - `club_loudness` -8.46 LUFS-I (window -9 to -7);
  - `club_plr` 7.44 dB (at least 6.5);
  - `club_true_peak` -1.02 dBTP round the loop;
  - `club_bands`: every octave inside its window, the closest (125 Hz) 0.28 dB from its edge;
  - `club_low_width` -14.6 dB (at most -10).

  It also passes every per-file check: DC -109 dBFS, stereo correlation 0.94, side under 120 Hz -53 dB, the seam -28 dB, the fill's splice -14 dB.
- **The kick:**
  - the strict gate: 150-400 Hz falls 30 dB in 26-30 ms; it stays 89.7 dB under the sub after 80 ms; its share is -14.4 dB at worst (limit -12);
  - the four-on-the-floor loop: -52.9 dB in the gaps, -58.0 before each hit;
  - its crest 6.08 / 6.54 dB (floor 4);
  - its tuning within 1.4 cents.
- **The low end over the kit's kick:** A (the gate) and B (bass-body) as above. The lines pass on their own:
  - A, the gate: -22.7 / -22.8 dB in the gaps;
  - B, bass-body: -5.9 / -6.1 dB in the gaps, +2.5 / +3.1 over a saw, resting under every kick.
- **The kit's tests** guard each of the review's fixes, and each fails with its fix undone: the vocals off the body lift, the robot's limiter, the clap's room (in a model-free mix of the drums, stab, pling and pluck: every clap the pluck leads into gains 1.6-2.4 dB of jump), the bass top's octaves in all four lines, the 5 dB cap.

## MIDI

The review's fixes changed no notes: the drums carry the shaker's 16ths on note 54 (General MIDI's tambourine: GM has no shaker inside 35-59, the plan's drum range; round 3 first wrote them on 82, inside the sung chops' 60-84), the chord voicing the stab's D3. The finishing pass rearranged the extended mix (below) and regenerated `content/midi/fut03/`; the test that compares it with a fresh export passes.

## The finishing pass: shared QA and the MIDI form

Nothing you hear changed in this pass: every master is byte-identical to the fix pass's, re-rendered from scratch in a fresh folder (`--repro`, every item twice). What changed is what QA reads, and the MIDI's extended form.

- **The master's cap.** The master's 5 dB cap, over the standard's 4, is now declared on the club item (`club_max_reduction_db`) and listed under Exceptions; QA checks it against the cap the master ran with. At 4 dB the loop stops at -9.23 LUFS-I, outside its window: the decision is yours.
- **Each kick's crest as the loop plays it** (ADR 0011's measure over the 200 ms from each full-velocity kick; the floor is 4.0 dB): `Context Loop Low` 6.08 dB at the lowest (median 6.54), `Context Loop Low Warm` 6.08, the club loop 5.22 (median 5.72; in the master's input 7.94).
- **What the master costs the hits** (the median loss of that crest from the master's input to the club loop): the kicks 1.86 dB, the claps 1.97 dB. In their own band (1-5 kHz) the claps change by -0.22 dB: the master flattens the kick and the low end, not the clap's own snap. The A&R review proposed at most 1.5 dB, not yet calibrated by ear, so the club loop declares 2 dB, with its reason (an exception in `qa_report.md`).
- **Where the clipper works.** The kicks' first 30 ms go up to +6.0 dB over the clipper's knee; the deepest moment away from every kick and clap goes +6.0 dB over it, 6.80 s into the loop.
- **Octave windows.** Every octave window is gated and passes, the closest 0.28 dB inside its edge.
- **Latency.** The stand-in master's limiter starts its gain down 3145 samples (65.5 ms) before a peak: its 5 ms lookahead and its 60 ms hold, which reaches forward too. The engine's master (S1e) reports that latency or redesigns the hold (ADR 0014).
- **MIDI.** The extended mix (`content/midi/fut03/extended/`, regenerated) takes the club form, reordered: the break now comes before the build, so drop 1 (bar 97) arrives on the build's riser and the clap's rise, as drop 2 already did. Still 224 bars; the loop files' notes are unchanged.

## The verification pass

An independent review checked this page, the kit's files and the shared tools against each other; this pass fixed what it confirmed. One file changed: all 44 items re-rendered from scratch (`--repro`, every item twice), 0 failed checks, and 174 of the 175 FLAC files are the same bytes as before.
- **The dry A/B at one loudness.** `Loops/Context Loop Warm` (dry B) comes down 0.57 dB to dry A's loudness, -13.16 LUFS-I (`context.DRY_B_LUFS`); A keeps its own peak and its bytes. Each at its own peak, B had read 0.57 LU louder, so "A and B, same loudness" above was not true until now. Three pairs keep their own peaks, and the listening page says so beside each clip: the vowel shots, one-shot instruments with one gain each, where B reads 2.0 LU over A on Talk and 3.9 LU on Rise, and the bounce line, whose B reads 0.3 LU over its A. Every B context loop plays those B files through fixed faders (`context.BASS_FADER`, `VOWEL_FADER`), so a level of their own would change those loops too. Match their levels by ear before you compare their character.
- **MIDI.** The drums' shaker moved from note 82 (inside the sung chops' 60-84) to 54. The build's last 8 bars roll the clap on every quarter for 4 bars, every 8th for 2 and every 16th for 1, rising to full velocity under the riser, then the written fill bar with the kit's fill (the Build Fill) in bar 96; the kick plays on under the roll, as future house builds keep theirs. `content/midi/fut03/` is regenerated; a fresh export matches it.
- **QA's crest per hit** now reads each hit's true peak with the loop round its span, so a span that ends in a loud passage can't over-read; no FUT03 reading moved.
- **The robot's limiter** has a test that runs without the vocal models (a sung phrase built in the test, its 4th 16th swelling), so CI guards it too.
- **This page:** the short-term loudness target is the standard's row for a 4-bar loop, and the crest losses at the top are labelled as the medians' difference (2.2 / 2.0 dB); QA gates the median of each hit's own loss, 1.86 / 1.97.

## Not verified

- **How any of it sounds:** nobody has listened to round 3.
- **The judgement calls behind the numbers.** Every choice was set by measurement against the standard's targets, and most of those targets are conventions, not measured club records (the standard's section 0):
  - the 5 dB master cap;
  - the music bus EQ (its 150 Hz lift now 5 dB), the vocal bus without it, the air shelves and the bus clip;
  - the robot's peak limiter (does "the" sound held down?);
  - the clap's room on the pluck (is the 3 dB dip before 2 and 4 heard as a dip?);
  - the bass top's level and formant;
  - the clap's room level and crest;
  - the shaker's level and accents;
  - the plate's settings.
- **The clap's jump target (≥ 12 dB) and the backbeat target are our own measures**, uncalibrated; the room on the pluck was designed with the jump's 60-10 ms window in mind, so the jump reading partly reflects that choice.
- **Kick and clap.** The kick's click measure, its crest band and the clap's hand detector are our measures too. The detector misses a first hand that starts on the onset, so the first hand's level comes from the recipe (`drums.CLAP_LEVELS`), and the measure confirms it on most takes.
- **Whisper on the club loop is no clearer than before:** bar 4 reads "what a glass drawing" (first render: "left a glass drawn"); bar 3's chop reads "I think right now" in both. It was never a pass on a full mix; the dry vocal files pass it.
- **Vocal bytes.** The vocal masters depend on this machine's onnxruntime kernels (ADR 0007): they are hashed per render environment. They are byte-identical to round 2's here.
