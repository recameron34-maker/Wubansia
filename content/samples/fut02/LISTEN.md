# FUT02 "Orbit Relay": round 3, the modern production pass

Written for the kit's render folder (`build/fut02-r3`, from `render.py --kit FUT02 --set r3`), which holds the FLAC masters, `qa/`, `sheets/`, `qa_report.md`, `qa_report.json` and `listen.json` this page names; its copy in `content/samples/fut02/` sits beside the WAVs alone.

A sci-fi club kit at 130 BPM in D minor (7A): a split kick, an electro clap, choked hats and a rolling offbeat bass under a chrome arp, every sound on one shimmering stretched-wire tone and a laser pitch drop. Round 3 keeps round 2's parts and lines and works on how they sound together: a club mix on buses, a wider top, body in the low mids, a pump, a filter ride on the arp, a longer hall, and a mastered loop near the genre's loudness. This render includes the fixes from the A&R review of round 3's first render (below).

**Nobody has listened to round 3.** Every number below is a measurement made in this session; none of it says how it sounds.

## Start here

`Loops/Context Loop Club.wav`: the wet context loop through the club master. Then the dry loops (A and B, same loudness) and the wet one, then the sounds that changed.

| Club master | Reading | Sci-fi club target |
|---|---|---|
| Loudness, LUFS-I (round the loop) | -9.41 | -8.5 ± 1 (see below) |
| True peak | -1.02 dBTP | ≤ -1.0 |
| PLR / PSR | 8.39 / 7.88 dB | ≥ 7.0 / ≥ 6 |
| Octaves re 1 kHz: sub, 125, 250, 500, 2k, 4k, 8k, 16k | +9.9 (in the 63 Hz octave), +1.8, +0.5, +2.7, -1.8, -3.7, -4.7, -8.8 dB | every one inside its window (`qa.CLUB`) |
| Side over mid at 125 / 250 Hz | -44.1 / -13.8 dB | ≤ -10 |
| Width at 2 / 4 / 8 / 16 kHz | -7.8 / -8.9 / -7.5 / -4.3 dB | -10 to -4 (2-4k), -8 to -3 (8-16k) |
| Peak reduction (clipper + limiter) | 4.25 dB (clipper 2.89, limiter 1.36) | the standard's convention: ≤ 4 (see below) |
| Kick crest in the loop, each kick (true peak over its loudest 50 ms, in the 200 ms from it) | median 5.89 dB, every kick 4.27 or more; 1.47 dB lost against the master's input | the floor 4.0; the review asked for at most 1.5 dB lost |
| Clap crest (true peak of its first 30 ms over its loudest 50 ms) | median 5.96 dB; 1.68 dB lost (the medians' difference, on this page's 30 ms measure) | QA reads ADR 0011's crest over the 100 ms from each clap and gates the median of each clap's own loss: 1.53 dB |
| Kick jump in the loop, full band / 40-120 Hz | 11.4 / 17.0 dB (the wet file: 11.6 / 17.7) | the master costs it at most 3-4 dB |
| Clap jump, 1-5 kHz | 12.4 dB, each clap 11.2-16.2 (round 3's first render: 10.0) | ours: ≥ 12 |

**The one deviation from the standard.** Its master convention is at most 4 dB of peak reduction. On the loop the club master gets (the wet mix before the renderer's 20 Hz high-pass, which the Wet file has on top), 4 dB stops at -9.61 LUFS, 0.11 LU under the genre's window. So `ContextLoopClub` lets the master use 4.25 dB (`context.MASTER_REDUCTION_DB`), the least that lands inside the window, and it reaches -9.41, 0.9 LU short of the -8.5 target. Round 3's first render allowed 6 dB and reached -8.5 with 5.58 dB, but the A&R review measured what that cost: each kick lost 2.59 dB of crest through the master, each clap 2.68, and one kick fell to 3.55 dB, under the crest floor. The loop's PLR stays high by design (a tight kick, a bass resting on every kick), and making it denser without new peaks would change the balance.

| Cap (on the master's input) | LUFS-I | Kick crest lost (median / worst kick) | Lowest kick crest | Clap crest lost (the 30 ms measure, the medians' difference) |
|---|---|---|---|---|
| 4.0 dB | -9.61 (outside the window) | 1.29 / 2.12 | 4.45 | 1.48 |
| **4.25 dB (shipped)** | **-9.41** | **1.47 / 2.32** | **4.27** | **1.68** |
| 4.5 dB | -9.22 | 1.66 / 2.52 | 4.11 | 1.86 |
| 6 dB (reaches -8.5 with 5.44) | -8.50 | 2.39 / 3.16 | 3.55 | 2.56 |

Read on the Wet file instead, the master reaches 0.1 LU more (-9.51 at 4 dB, -9.31 at 4.25), and its kicks read 0.35 dB more crest, so the kick's loss at 4.25 reads 1.81 dB against the file (most likely the high-pass's phase shift on the kick's 55 Hz sub, which moves where its peak lands). The figures above are the shipped loop's.

## The A&R review of round 3's first render

Five findings; all were checked by measurement, and the numbers they quoted reproduced.

| Finding | Verdict | What changed |
|---|---|---|
| 1. Fake width on the arp and the zap (major) | Applied, and more | The arp's width is now a detuned unison on its side, the zap's back to round 2's through a plain delay, the hats' side balanced |
| 2. The 6 dB master cap flattens the kick and clap (major) | Applied in part | Cap 4.25 dB. The zap and open-hat velocity drops, the zap on the drum bus and a deeper duck on the fill's first laser were tried and left out (below) |
| 3. The clap is buried (minor) | The problem applied, the proposed dip not | The clap snaps, and the arp makes room before bar 1's backbeats |
| 4. The club loop's text says 4 dB | Applied | It is built from the cap: "at most 4.25 dB of peak reduction" |
| 5. The cap figures read the Wet file, not the club master's input | Applied | This page and `context.py` quote the input, and say where the file differs |

**1. Width.** Round 3's first render doubled the arp's all-pass side (0.70) and widened the zap the same way. An all-pass side makes each speaker play a complementary comb, and this all-pass's direct term (-0.6 of its input at no delay) also put an anti-phase copy of the sound on the side, so the arp's and zap's tops leaned right:

| | Left channel against the mid, 1-8 kHz (5th-95th percentile, 20 Hz smoothing) | L/R correlation of that ripple | Right over left above 3 kHz | 0.7-1.4 kHz | Side over mid 2 / 4 / 8 kHz |
|---|---|---|---|---|---|
| Arp phrase, round 2 | -3.7 to +2.3 dB | -0.94 (a comb) | +2.8 dB | -2.1 dB | -9.5 / -9.1 / -9.1 |
| Arp phrase, round 3 first render | -10.2 to +4.2 | -0.78 (a comb) | +4.4 | -3.8 | -3.4 / -3.1 / -3.1 |
| **Arp phrase, now** | -0.1 to +9.8 | **+0.90** (no comb) | **-0.0** | **-0.2** | -3.9 / -3.0 / -3.0 |
| Zap v5, round 3 first render | -10.2 to +4.2 | -0.78 | +5.1 | -3.7 | -3.4 / -3.1 / -3.1 |
| **Zap v5, now (round 2's width)** | -4.3 to +2.9 | -0.92 | **+0.2** | -2.2 | -8.3 / -8.0 / -8.0 |
| Open hat v5, round 3 first render | -5.9 to +3.6 | -0.12 | +2.6 (the whole hat) | | -19.3 / -9.2 / -6.2 |
| **Open hat v5, now** | -1.1 to +7.8 | +0.36 | **+0.0** | | -18.3 / -7.8 / -4.7 |

- **The arp:** the voice again 7 cents sharp and 7 cents flat, half their difference (above 1 kHz) on the side. The mono sum is the voice itself, the same as round 2's and the first render's (checked down to the 24-bit floor: -132 dB on the phrase, -103 dB or better on the notes), and each speaker hears a three-voice unison that beats differently left and right. Each note starts in the middle and spreads as the copies drift apart. In the phrase the side flips polarity from note to note: the sharp and flat copies leave the body's plucked low-pass not quite alike, so their difference keeps a small image of the voice, always the same way, which alone leans the phrase 0.9 dB to the right (most of it at 1-1.6 kHz, near that filter's resting cutoff). A multisample note can't alternate: each one is within 0.6 dB left to right above 3 kHz and 1 dB over the whole file, with one third-octave up to 3 dB (C5's 1.6 kHz). Its "-0.1 to +9.8" upper figure isn't a comb: at 20 Hz smoothing the detuned partials, 0.4% off the centre's, show as energy where the mid has little, in both channels at once (correlation +0.90), and over 100 Hz it reads +0.4 to +5.4.
- **The zap:** round 2's width (0.15 + 0.25 x velocity, as the review asked), its side now the zap 1.1 ms later instead of through the all-pass: above 3 kHz it is balanced within 0.2 dB. A static side still places single partials: the D5's second partial reads 2 dB to the left, as in round 2.
- **The hats (found while checking this):** their side took the mid's own noise out of the left channel and doubled it on the right, so the open hat read 2.6 dB louder on the right (round 2: 1.2) and the hat loop 2.5. Now the side is a second noise alone, at the same side over mid for the closed hat and 1.2 times that for the open hat, which keeps the loop's 8 kHz width.
- **The loop:** its top no longer leans: right over left at 10 / 16 kHz +1.1 / +1.4 dB, from the hats' 0.15 pan (first render +2.8 / +4.1). Its width at 2 / 4 / 8 / 16 kHz reads -7.8 / -8.9 / -7.5 / -4.3 dB, against -7.1 / -8.4 / -7.9 / -5.2: part of the first render's width was the lean. The review asked for -7 or more at 2-8 kHz, which the first render didn't have either; every value is inside the standard's windows.
- **The pad is unchanged.** It read -8.9 to +5.8 dB on the same measure, but round 2's pad, with no all-pass side, read -12.0 to +6.0: that spread is its two sides' own detuned saws and its stars, genuinely different left and right, not a comb. Its own all-pass side, on its quiet chrome, is masked: the pad is within 1.2 dB left to right above 3 kHz and at 0.7-1.4 kHz (single third-octaves up to 2.9 dB, round 2 3.9, from where its stars sit).
- **Faders:** the arp's and zaps' files got louder at their peak, so their faders came down by the files' gain changes (-1.74 and -2.80 dB): their mono level in the loop is the first render's exactly. The zap lane now reads -25.2 LUFS (round 2's), since its side is round 2's again. The hats' fader keeps their lane at -23.8.

**2. The master.** The cap and its cost are above. The review also suggested lowering the zaps at 16ths 14 and 47, the open hat at 14, putting the zap on the drum bus and ducking the fill's first laser 3 dB more. After the width fix the zap and hat no longer set the loop's hardest peaks (16th 14 dropped from the top to seventh), and each of these, measured at the shipped cap, moved the loudness by +0.001 to -0.012 LU and the kicks' crest loss by nothing; the extra duck lowered the weakest kick's crest from 4.27 to 4.17. They would only change the accents, so they're left out. The hardest-driven 16ths now are bass offbeats (16ths 42, 6, 18), the kick with the clap and "pull me" (28) and the kick with the fill's first laser (48).

**3. The clap.** The review proposed a clap-keyed dip on the music bus (1-5 kHz, from each clap). Measured, it lowers the clap's jump (10.12 to 10.07 dB, the weakest clap 8.20 to 7.59): the jump compares the clap's first 30 ms with the 60-10 ms before it, and a dip that starts at the clap only takes music out from under it. What filled the 1-5 kHz band before bar 1's claps was the arp's tresillo accent on the 16th before each backbeat (about two thirds of the band there). So:
- the arp's top (1.2-6.5 kHz) is carved 4 dB from 100 ms before bar 1's two backbeats to just before the next 16th (bars 2 and 3's backbeats already sit under the vocal carves; the accent's first 10 ms ring before it ramps in);
- the clap gets a snap: its 1-5 kHz band 3 dB up over the first 12 ms of every backbeat clap, back by 25 ms (a transient shaper on the lane). Its fader stays under the kick's; its lane reads -20.5 LUFS, 3.5 LU under the kick.

The clap's jump in the club loop went from 10.0 to 12.4 dB (each clap 11.2 to 16.2; the dry loop 13.7). Neither move changes the loudness (within 0.02 LU).

**4 and 5.** The club loop's text now reads "at most 4.25 dB of peak reduction", built from the constant. The figures on this page and in `context.py` are the shipped loop's input, and the ladder below says it reads the Wet file.

## What changed since round 2, and why

The standard (`docs/plans/S1-modern-sound.md`) measured FUT02's round 2 loop against current sci-fi club records and listed six changes. They were made in its order, each measured before the next, then the review's fixes. The wet loop as delivered (the Wet file: normalised to -1.02 dBTP, after the renderer's 20 Hz high-pass); "club at 4 dB" is what the club master reaches on that file at the standard's cap. The shipped club loop masters the loop before that high-pass, which reaches about 0.1 LU less: at round 3's first render, -9.71 at 4 dB and 5.58 dB of reduction for -8.5, where the file read -9.57 and 5.37.

| After | Wet LUFS-I / PLR | Club at 4 dB (reduction needed for -8.5) | Holes 125 / 250 | 125 / 250 / 500 re 1k | 2k / 4k / 8k / 16k re 1k | Width 2k / 4k / 8k | Kick jump full / 40-120 | Clap jump |
|---|---|---|---|---|---|---|---|---|
| round 2 | -14.89 / 13.87 | -11.24 (7.59) | -6.27 / -6.58 | -0.3 / -2.4 / +2.5 | -3.5 / -6.0 / -6.8 / -10.9 | -9.4 / -12.3 / -13.9 | 12.0 / 20.0 | 11.4 |
| 1 density, first pass | -14.62 / 13.60 | -11.06 (7.39) | -6.38 / -5.48 | -0.1 / -1.0 / +2.6 | -3.5 / -5.9 / -6.0 / -9.7 | -9.3 / -11.9 / -8.7 | 12.0 / 18.0 | 11.1 |
| 2 width | -14.68 / 13.66 | -11.21 (7.54) | -6.14 / -5.14 | +0.1 / -0.9 / +2.2 | -3.2 / -5.4 / -5.6 / -9.4 | -7.1 / -9.0 / -8.0 | 12.2 / 18.4 | 11.2 |
| 3 low mids, the bulge trimmed | -14.37 / 13.35 | -10.80 (7.03) | -5.72 / -4.57 | +1.7 / +0.6 / +2.9 | -2.1 / -4.1 / -4.3 / -8.0 | -6.9 / -9.0 / -8.0 | 12.2 / 18.7 | 11.9 |
| 4 pump | -14.19 / 13.17 | -10.74 (6.95) | -5.67 / -4.61 | +1.9 / +0.7 / +3.0 | -2.0 / -3.9 / -4.0 / -7.7 | -7.2 / -9.3 / -8.1 | 12.1 / 18.7 | 11.7 |
| 5 arp filter ride | -14.17 / 13.15 | -10.73 (6.93) | -5.68 / -4.62 | +1.8 / +0.5 / +2.8 | -2.0 / -4.2 / -4.2 / -7.9 | -7.2 / -9.6 / -8.1 | 12.0 / 18.7 | 11.2 |
| 6 hall | -14.03 / 13.01 | -10.59 (6.77) | -5.73 / -4.73 | +1.5 / +0.2 / +2.7 | -2.2 / -4.4 / -4.4 / -8.2 | -7.1 / -9.3 / -8.1 | 12.0 / 18.7 | 11.0 |
| balance (the kick's room, levels) | -13.34 / 12.32 | -10.10 (6.10) | -5.87 / -5.02 | +0.8 / -0.5 / +2.4 | -2.4 / -5.0 / -5.0 / -8.8 | -6.7 / -8.7 / -7.9 | 11.2 / 18.7 | 9.7 |
| first render (bass +1.5 dB, B's sub, the EQ settled) | -12.82 / 11.80 | -9.57 (5.37) | -5.78 / -4.73 | +1.7 / +0.4 / +2.7 | -1.8 / -3.9 / -4.6 / -8.7 | -7.2 / -8.4 / -7.9 | 11.3 / 17.7 | 10.2 |
| **the review's fixes (this render)** | **-12.91 / 11.89** | **-9.51 (5.30)** | **-5.77 / -4.72** | **+1.8 / +0.5 / +2.8** | **-1.8 / -3.7 / -4.5 / -8.6** | **-7.9 / -9.0 / -7.4** | **11.6 / 17.7** | **12.4** |

Targets: holes ≥ -4 (a shape check, reported); 125 +1..+6, 250 0..+4, 500 0..+3, 2k -3..0, 4k -5..-2, 8k -8..-4, 16k -14..-8; width -10..-4 at 2-4 kHz, -8..-3 at 8 kHz.

**1. Density, then a master.** Round 2's loop was lanes summed at a PLR of 13.9 dB. Splitting its peaks lane by lane showed what set them: the kick plus whatever sounded on it. The clap on 2 and 4 summed with the kick to 3.6 dB over a kick alone; the hats' 1 ms tick reached 0.83 of full scale on its own; and "pull me", the fill's first laser and the bass's offbeat accents each sat on top of a kick. Round 3:
- spreads the hats' tick over 3 ms;
- gives the bass a saturated bite (below);
- compresses the vocals;
- runs kick, clap and hats on a drum bus soft-clipped at the kick's own peak;
- adds ContextLoopClub.

Not enough alone (13.9 to 13.6), as the standard predicted. The pump and the final balance did the rest.

**2. Width above 500 Hz.** Round 2 was the narrowest kit: in the dry loop its side sat 12-14 dB under its mid from 500 Hz up. Round 3 spreads the open hat, the arp and the zap (in this render through the balanced sides above, with each of the loop's four zaps on its own side), gives the vocoder a chorus (its mono sum unchanged) and the arp a bright ping-pong: the kit's Echo is high-passed at 700 Hz (shared: 300 Hz) and low-passed at 7 kHz (2.6 kHz). That also takes its repeats out of the 400-800 Hz bulge: its return was the loudest thing in the 500 Hz octave after the vocals. Width at 2 / 4 / 8 kHz went from -9.4 / -12.3 / -13.9 to -7.9 / -9.0 / -7.4 dB, all inside the standard's windows.

**3. The low mids, and the 400-800 Hz bulge.** B's warm low mids are bound by the bass-body rule over the kick, so the body comes from elsewhere. The pad's low mids, dipped 4 dB in round 2, are lifted 2.5 dB and kept in the middle (it is mono up to 320 Hz now). The music bus trims the bulge: -3.5 dB at 900 Hz, with a 1.5 dB presence lift at 4 kHz. The 125 and 250 Hz octaves went from -0.3 and -2.4 to +1.8 and +0.5 dB re 1 kHz; the holes from -6.3 / -6.6 to -5.77 / -4.72 dB (the standard's ≥ -4 isn't reached; it is reported, not gated).

**4. The pump.** The pad, the arp and the three returns duck 3 dB under every kick and are back by 192 ms, inside the next 8th (231 ms); the standard asks for 2-4 dB.
- It barely moves the numbers: the wet loop is 0.2 LU louder, and the master needs 0.08 dB less.
- The kick's measured jump doesn't change (40-120 Hz 18.7 dB before and after). The pumped parts are high-passed far above 120 Hz, and they are back at full level in the window the jump is compared against, the 60-10 ms before each kick.
- What it changes is how the music breathes: 3 dB down on each kick and back by the next 8th, which is by construction (`context.PUMP`). It is meant to be heard, not measured.

**5. The arp's filter ride.** A resonant low-pass on the arp lane rises from 1.6 kHz on bar 1's downbeat to 16 kHz by bar 3's last beat, then closes while the arp rests in the fill bar. Bar 1's 2-8 kHz share is 3.3 dB under the phrase as written (centroid 1102 to 994 Hz); bar 2 is 4.9 dB brighter than bar 1; bar 3 plays fully open.

**6. A sci-fi hall.** The kit's own Space instead of the shared one: 3.5 s (shared: 2.6 s), a pre-delay of a 32nd note (57.7 ms; shared: 20 ms), damped from 9 kHz (6.5 kHz), a touch more shimmer. The pad and the vocoder go into it harder, the arp and clap less. Its return reads 1.1 LU fuller.

**The balance.** Three last measured moves:
- **The kick's room.** The vocals, zaps and fill get a shorter 3 dB duck under each kick (back by 117 ms). Before it, the loop's loudest moment was a kick with something on it ("pull me" on 16th 28, the fill's first laser on 16th 48), about 1 dB over the next, and the master's drive is limited by the loudest one. With it, the loop's top peak dropped 0.8 dB, and its 16 loudest 16ths sit within 1.5 dB of each other.
- **The music's levels.** The arp, vocals, zaps and fill go back to about their round 2 loudness; the bus trim had taken 2-3 dB of them.
- **The bass.** Both basses go up 1.5 dB. The bass lane reads 0.5 LU over the kick lane; the kick still leads on its transient, jumping 17.7 dB in 40-120 Hz in the Wet file.

## The sounds

| # | Sound | Round 3 | Files |
|---|---|---|---|
| 1 | Split kick on A1 | **unchanged** (the same bytes) | `Drums/Kick Split` |
| 2 | Electro clap | **unchanged** (the same bytes; the snap is the loop's) | `Drums/Electro Clap` |
| 3 | Closed and open hats | changed: the tick, the width, balanced | `Drums/Closed Hat`, `Drums/Open Hat`, `Loops/Hat Loop` |
| 4 | Laser zap on D5 | changed: round 2's width, balanced | `Drums/Zap` |
| 5 | Rolling bass | changed: the bite, B's sub | `Loops/Orbit Groove …`, `Relay Groove …`, `Orbit Line …`, `Relay Line …`, `Bass/Roll Bass …` |
| 6 | Drop glide | **unchanged** (the same bytes) | `Bass/Drop Glide Clean`, `Drop Glide Warm` |
| 7 | Starfield pad, Dm9 | changed: wider, body in the middle | `Music/Starfield Pad Dm9`, `Loops/Starfield Pad Loop` |
| 8 | Chrome arp | changed: wide from a detuned unison | `Loops/Chrome Arp Phrase`, `Music/Chrome Arp` |
| 9 | Sung chops | **unchanged** (the same bytes) | `Vox/Sung Line 1` … |
| 10 | Vocoder phrase | **unchanged** (the same bytes; the chorus is the loop's) | `Vox/Vocoder Relay`, `Vocoder Relay Wet` |
| 11 | Laser fill | changed: its hats are the new hats, its dive balanced | `FX/Laser Fill`, `Loops/Laser Fill Preview` |
| 12 | Laser riser | changed: its laser rise's width balanced, nothing else | `FX/Laser Riser`, `Loops/Laser Riser Preview` |

What changed, sound by sound (each file measured against its round 2 file):
- **Hats.** The tick on the front is spread over about 3 ms instead of a 1 ms needle, 1.5 dB lower, and grows with the square of the velocity. Each hat now reads about 3 dB louder at the same peak (crest 20.9 dB on the closed hat and 15.9 on the open one, round 2: 23.7 and 18.3).
  The open hat spreads (side over mid at 8 kHz -14.8 to -4.7 dB) and the closed one stays near the middle (-17.5 to -14.4). Both sit centred: left and right within 0.4 dB (round 2: 1.2 dB right on the open hat). The hat loop's side at 8 / 16 kHz went from -14.8 / -12.6 to -5.0 / -2.2 dB.
- **Zap.** The same laser onto D5 at round 2's width, through a plain delay instead of an all-pass, so it leans to neither side above 3 kHz (round 2: 3.5 dB right). In the loop the four zaps alternate sides.
- **Rolling bass.** The same voice, lines and notes, with a bite: the mid band is saturated after its high-pass, then filtered again. A saturates all of its mids; B only those over 400 Hz, adding back what the drive changes, so its 150-400 Hz is round 2's within 0.1 dB.
  At round 2's band loudness the lines read denser: PLR 15.2 to 12.1 dB (Orbit A), 14.0 to 11.2 (Relay A), 13.3 to 12.9 (Orbit B), 12.0 to 11.6 (Relay B). Their tone balance stays about round 2's: 2-8 kHz is 0.5-1.5 dB darker against 1 kHz, so it is grittier, not brighter. B also has 0.9 dB more sub.
- **Starfield pad.** The chrome partials get a side through an all-pass (mono-safe), and the two sides' sweeps cross further apart: its side over mid at 1 / 2 kHz goes from -3.4 / -2.9 to -2.1 / -1.5 dB.
  Its low mids, dipped 4 dB in round 2, are lifted 2.5 dB and kept central (mono up to 320 Hz). Against its own 1 kHz octave, 125 / 250 Hz read -2.0 / +4.3 dB (round 2: -5.7 / +1.6).
- **Chrome arp.** The same notes, and the same mono sum as round 2's, wide from the detuned unison (above): the phrase's side over mid at 2 / 4 / 8 kHz goes from -9.5 / -9.1 / -9.1 to -3.9 / -3.0 / -3.0 dB, balanced. The filter ride and the ping-pong are the loop's (changes 2 and 5).
- **Laser fill.** The same lasers and dive; its hat crescendo plays the new hats, and the dive's side is the plain delay too.
- **Laser riser.** Round 2's riser with its laser rise's side through the plain delay; its mono sum is round 2's to -41 dB, and it sat within 1.2 dB left to right before and after.
- **Unchanged, the same bytes as round 2:** the kick, the clap, the drop glide, the three sung lines and their eight chops, and both vocoder files. 37 files in all, compared by SHA-256.

## The bass A/B

Unchanged in principle: A clean (the strict mud gate, its mids from 520 Hz) against B warm (the proposed bass-body rule, its mids from 120 Hz), the same lines, level-matched in the loop (both lanes read -16.5 LUFS). Round 3 gives both the same kind of bite above their low end. Over the kit's kick:
- A reads -29.1 dB against the gate's -20 (round 2: -29.2).
- B reads -4.0 dB against the bass-body limit's -2 (round 2: -3.8).

## What to listen for

- **The club master first.** Is it loud and polished, and does the kick still punch through it, or does it sound squashed? It now takes 4.25 dB off the peaks (the first render 5.6) and sits at -9.4 LUFS, 0.9 LU under the genre's target: is it loud enough next to a club record?
- **The clap.** Does it snap on 2 and 4 now, and does the arp's tuck before bar 1's backbeats go unnoticed?
- **The pump.** Does the pad and arp's breathing under the kick feel like the genre, or is it too much at 3 dB?
- **Width.** Does the arp's unison shimmer, and do the open hat, zaps and vocoder spread without the loop losing its centre? Check it once on headphones (each ear alone should sound whole, no hollow comb) and once in mono: every widening keeps the mono sum.
- **The bass bite.** Is the rolling bass bigger and more driving, and does it read on small speakers? Is B still the warm one?
- **The arp's filter ride.** Does the arp build from bar 1 to bar 3, or is bar 1 too dull for a hook heard alone?
- **The hall.** Is the 3.5 s hall on the pad and vocoder a sci-fi space, or does it wash the groove?
- **Mud.** Once more in mono at a moderate level: anything boomy or boxy between 150 and 400 Hz, now that the pad carries more low mids?

## QA

- **The render:** 45 items, 121 files, **0 failed checks** (`qa_report.md`).
- **Reproducible:** every item was rendered a second time in a fresh process (`--repro`), with the same SHA-256.
- **The club loop** passes every club check:
  - `club_loudness` -9.41 LUFS-I (window -9.5 to -7.5);
  - `club_plr` 8.39 dB (at least 7.0);
  - `club_true_peak` -1.02 dBTP round the loop;
  - `club_bands`: every octave inside its window, the closest 0.29 dB from its edge (500 Hz);
  - `club_low_width` -13.8 dB (at most -10).

  It also passes every per-file check: DC -109 dBFS, stereo correlation 0.94, side under 120 Hz -57 dB, the loop seam. It is the master's output on its input exactly (to -139 dB).
- **The low end over the kit's kick** (the same bytes as the first render):
  - A (the gate): ContextLoopLow -29.1 dB (limit -20; round 2 -29.2); the groove previews -29.1 and -24.7.
  - B (bass-body): ContextLoopLowWarm -3.98 dB (limit -2; round 2 -3.78); the groove previews -3.98 and -6.79.
- **Kick crest:** 5.92 dB on the top layer, unchanged (floor 4).
- **The written loop as MIDI** (`content/midi/fut02`): no pattern changed; the finishing pass rearranged the extended mix (below), and a fresh export matches the regenerated folder file for file.

## The finishing pass: shared QA and the MIDI form

Nothing you hear changed in this pass: every master is byte-identical to the fix pass's, re-rendered from scratch in a fresh folder (`--repro`, every item twice). What changed is what QA reads, and the MIDI's extended form.

- **The master's cap.** The master's 4.25 dB cap, over the standard's 4, is now declared on the club item (`club_max_reduction_db`) and listed under Exceptions; QA checks it against the cap the master ran with. At 4 dB the loop stops at -9.61 LUFS-I, 0.11 LU under its window: the decision is yours.
- **Each kick's crest as the loop plays it** (ADR 0011's measure over the 200 ms from each full-velocity kick; the floor is 4.0 dB): `Context Loop Low` 4.49 dB at the lowest (median 6.53), `Context Loop Low Warm` 5.28, the club loop 4.27 (median 5.89; in the master's input 7.35).
- **What the master costs the hits** (the median loss of that crest from the master's input to the club loop): the kicks 1.47 dB, the claps 1.53 dB. In their own band (1-5 kHz) the claps change by +0.13 dB: the master flattens the kick and the low end, not the clap's own snap. The A&R review proposed at most 1.5 dB, not yet calibrated by ear, so the club loop declares 1.6 dB, with its reason (an exception in `qa_report.md`).
- **Where the clipper works.** The kicks' first 30 ms go up to +5.0 dB over the clipper's knee; the deepest moment away from every kick and clap goes +5.2 dB over it, 4.88 s into the loop.
- **Octave windows.** Every octave window is gated and passes, the closest 0.29 dB inside its edge.
- **Latency.** The stand-in master's limiter starts its gain down 3145 samples (65.5 ms) before a peak: its 5 ms lookahead and its 60 ms hold, which reaches forward too. The engine's master (S1e) reports that latency or redesigns the hold (ADR 0014).
- **MIDI.** The extended mix (`content/midi/fut02/extended/`, regenerated) takes the club form, reordered: the break now comes before the build, so drop 1 (bar 97) arrives on the build's riser and the clap's rise, as drop 2 already did. Still 224 bars; the loop files' notes are unchanged.

## The verification pass

An independent review checked this page, the kit's files and the shared tools against each other; this pass fixed what it confirmed. One file changed: all 45 items re-rendered from scratch (`--repro`, every item twice), 0 failed checks, and 123 of the 124 FLAC files are the same bytes as before.
- **The bass A/B at one loudness.** `Loops/Orbit Groove Clean` comes down 0.96 dB to its B's loudness, -13.17 LUFS-I (`preview.GROOVE_LUFS`); B keeps its own peak and its bytes. Each at its own peak, Orbit's A had read 0.95 LU louder than its B; the Relay pair reads within 0.02 LU at its peaks and keeps them. The held D1 notes and the drop glide are multisample instruments, one gain each, so they keep their peaks: on this note A reads 5.4 LU over B, and A's glide 1.7 (hard) and 1.6 (soft) LU over B's. The listening page says so beside each clip; match their levels by ear before you compare their character.
- **MIDI.** The build's last 8 bars roll the clap on every quarter for 4 bars, every 8th for 2 and every 16th for 1, rising to full velocity under the riser, then the written fill bar with the kit's fill in bar 96; the kick sits the roll out, so drop 1 lands on its return. `content/midi/fut02/` is regenerated; a fresh export matches it.
- **QA's crest per hit** now reads each hit's true peak with the loop round its span, so a span that ends in a loud passage can't over-read; no FUT02 reading moved.
- **This page:** the clap crest loss in the first table is labelled: 1.68 dB is the medians' difference on this page's 30 ms measure; QA gates the median of each clap's own loss, 1.53 dB.

## Not verified

- **How any of it sounds:** nobody has listened to round 3.
- **The judgement calls behind the numbers.** Every choice was set by measurement against the standard's targets, and most of those targets are conventions, not measured club records (the standard's section 0):
  - the 4.25 dB master cap, and the loudness it leaves (-9.4 against the genre's -8.5);
  - the bus clip, the pump depths, the hall and the EQ;
  - the clap's snap and the arp's tuck before the backbeat;
  - the arp's unison (its detune and level), the open hat's width;
  - the bite's drive and the arp ride's range.
- **The clap's jump target (≥ 12 dB) and the review's 1.5 dB crest-loss limit are our own measures**, uncalibrated.
- **Whether the unison arp reads as wider and cleaner than the first render's comb, or just different.** The measures say the comb and the lean are gone; only listening says which sounds better.
