# FUT01 "Alloy Signal": round 3, the modern production pass

Written for the kit's render folder (`build/fut01-r3`, from `render.py --kit FUT01 --set r3`), which holds the FLAC masters, `qa/`, `sheets/`, `qa_report.md`, `qa_report.json` and `listen.json` this page names; its copy in `content/samples/fut01/` sits beside the WAVs alone.

A future garage / 2-step kit at 134 BPM in F# minor (11A) with a sci-fi colour: metal drums and an alloy hit tuned to one inharmonic ratio set, a reese bass, chrome plucks and a sung club vocal. Round 3 keeps round 2's parts, lines and sounds and works on how they sound together, against the UK garage targets of the standard (`docs/plans/S1-modern-sound.md`): a mastered club loop, a tighter kick B, a snare-clap level with the kick, a swung shaker, the bass up front with its filter talking against the backbeat, a garage plate, the low mids filled and centred, and two re-pitched vocal hooks. Every round 2 sound comes back byte for byte.

After the first round 3 render, a review measured it against the standard and found four problems; the fix round below took three of them and records why the fourth waits. **Nobody has listened to round 3.** Every number below was measured in this session on the files in this folder, or on the same mix rebuilt one change at a time. None of it says how anything sounds.

## Start here

1. `Loops/Context Loop Club.wav` on loop: the wet loop through the club master, the loud reference.
2. `Loops/Context Loop Wet.wav`, `Context Loop.wav` (bass A) and `Context Loop Warm.wav` (bass B): the same mix unmastered, all three at -16.3 LUFS, so they compare by sound, not level.
3. The A/Bs, each pair at one loudness: `Loops/Drum Groove Punch` against `Drum Groove Split` (kick B against kick A, each through the loop's kick clip), and `Drum Groove Swing 61` against `Drum Groove Punch` (a heavier shuffle).
4. The new sounds: `Drums/Kick Punch`, `Drums/Snare Clap`, `Drums/Shaker`, `Vox/Chop Me E5`, `Vox/Chop Floor F#5`.

The WAVs in `Drums/`, `Bass/`, `Music/`, `Vox/`, `FX/` and `Loops/` carry the same samples as the `FUT01_*.flac` masters; `v1` is the softest layer.

## The club master against the UK garage targets

`ContextLoopClub` is `ContextLoopWet` through the shared master (`tools/sound/master.py`, ADR 0014, proposed) at the genre's settings, with nothing overridden. Round 2's wet loop through the same master is the fair "before"; the middle column is round 3 as first rendered, before the fix round.

| | Round 2 through the master | Round 3, first render | Round 3 now | Target |
|---|---|---|---|---|
| Loudness, LUFS-I (round the loop) | -9.86 | -9.92 | -9.70 | -9 ± 1 |
| LUFS-S max | -9.49 | -9.60 | -9.43 | a 4-bar loop: its LUFS-I plus a few tenths (not checked) |
| True peak | -1.02 dBTP | -1.02 dBTP | -1.02 dBTP | ≤ -1.0 |
| PLR / PSR | 8.84 / 8.47 dB | 8.90 / 8.58 | 8.68 / 8.41 | PLR 7.5-9 |
| Peak reduction | 4.0 dB (clip 2.81, limiter at most 1.22) | 4.0 (clip 2.44, limiter 1.56) | 4.0 (clip 2.70, limiter 1.30) | ≤ 4, the standard's cap |
| Kick lane: crest of each full-velocity kick | 5.82-6.84 dB (kick A, no clip) | **3.61-3.62** | 5.18-5.26 | 5-7 (round 1's band; floor 4.0) |
| Low-mid hole, 125 / 250 Hz | **-9.26 / -8.77** dB | **-6.59 / -5.10** | **-5.95 / -4.96** | ≥ -4 |
| Octaves re 1 kHz: sub (63), 125, 250, 500 Hz | +6.5 / **-3.8 / -4.4** / +3.3 | +6.1 / **-2.5 / -3.1** / 0.0 | +7.3 / **-1.0 / -2.4** / +0.3 | +6..+12, +2..+6, +1..+4, 0..+3 |
| Octaves re 1 kHz: 2, 4, 8, 16 kHz | -2.2 / **-5.4** / -7.3 / -11.6 | **-3.01** / -4.9 / -5.0 / -9.6 | -2.8 / -4.9 / -5.3 / -9.8 | -3..0, -5..-2, -8..-4, -14..-8 |
| Top slope, 500 Hz-8 kHz | -2.65 dB/oct | **-1.26** | **-1.38** | -2.0 ± 0.5 (not checked) |
| Side over mid, 250 Hz | **-5.5** | -15.4 | -16.3 | ≤ -10 |
| Side over mid, 1 / 2 / 4 / 8 / 16 kHz | **-5.1** / -7.5 / -8.9 / **-9.9** / -4.8 | **-5.9** / -8.2 / -8.9 / -7.7 / -4.3 | **-5.9** / -8.1 / -8.9 / -7.6 / -4.2 | -12..-6, -10..-4, -10..-4, -8..-3, -8..-3 |
| Backbeat over a kick alone (wet loop) | **+1.44 dB** | +0.87 | -0.20 | ≤ +1 |
| Kick jump, full band: club (wet) | 8.36 (9.18) dB | 7.83 (8.19) | 7.72 (8.18) | the master costs it ≤ 3-4 dB |
| Clap jump, 1-5 kHz: club (wet) | 13.39 (13.67) dB | 12.02 (12.14) | 13.54 (13.49) | ≥ 12 |

Bold marks a reading outside its target. What round 3 does not meet, plainly:
- **Loudness: inside the window, 0.70 LU short of -9.** At the standard's 4 dB cap the master stops at -9.70, and its report says how far short (ADR 0014: a mix too sparse stops at the cap, it is not crushed). The short-term maximum (-9.43) sits 0.27 dB over the loop's LUFS-I, as the standard's row for a 4-bar loop expects (its fixed -8..-7 describes a whole track's loudest section, which a loop can't reach); QA doesn't check it.
- **125 and 250 Hz stay 3.0 and 3.4 dB under their windows**, and the hole short of -4. Round 3 filled them as far as the low-end rules allow: the kick and bass A keep the strict mud gate (-26.35 dB against the sub in the gaps, limit -20), bass B sits 1.7 dB inside the bass-body window (-3.69, limit -2), and every other lane is high-passed by rule. The octave windows are conventions built on a chart-record average (the standard's S4 replaces them with measured references), so `qa_report.md` records the exception and its reason instead of failing the loop.
- **The top slope is -1.38 dB per octave**, brighter than the standard's UK garage -2.0 ± 0.5 (a shape check QA doesn't run): 8 kHz sits in its window, but 500 Hz sits at the bottom of its own.
- **1 kHz is 0.1 dB wide of its width window** (-5.9 against -12..-6).
- **The kick jump is 0.6 dB lower than round 2's through the same master** (7.72 against 8.36, the median over all 11 kicks, as the standard reads it): the bass now leads the mix. The four full-velocity kicks jump 1.0 dB more than round 2's (9.38 against 8.40); the 0.8-velocity skip kicks on 16ths 10, 42 and 58 lose 3.2-3.4 dB (13.2 to 9.8, 12.8 to 9.4, 8.4 to 5.1).

## The fix round

The review's four findings, as measured and as done:

**1. The kick the loops play (applied).** The kick lane's soft clip was 4 dB (3.2 dB of real peak reduction), which left each full-velocity kick of kick B at a crest of 3.61-3.62 dB in the lane: under ADR 0011's 4.0 dB floor, the reading of a kick flattened onto its body (the renderer's 20 Hz high-pass lifts it to 3.97-4.19 in `Context Loop Low`). And the A/B grooves played both kicks unclipped, so the A/B was not the kick the loops play. Now the clip is 2 dB (1.5 dB of reduction): the lane's kicks read 5.18-5.26 dB, `Context Loop Low` 5.38-5.56, and the drum grooves run each kick lane through the same clip (kick B 5.18, kick A 5.17). The lighter clip cost the master 0.57 LU (-10.49, outside the window). The review's fix, the bass 3 dB up, brought it back to -9.93 but measured worse elsewhere: the clap jump fell to 10.67 dB, the backbeat rose to +1.45 dB over the kick, and 2 and 4 kHz fell out of their windows (-3.44 and -5.69), because the reese's top sat in the 60 ms before every snare and its sub and body under it. So the bass comes up 3 dB with two more changes (finding 3): its top closes into every backbeat, and the lane dips 4 dB under each snare. Together: -9.70 LUFS-I, a clap jump of 13.54 dB and the backbeat at -0.20. Other ways to the loudness were measured and turned down: clipping the snare or the hats (no gain: the kicks set the master's peaks), the pad up 1.5 dB or the vox 1 dB (+0.17 LU each, the clap jump under 12), a light pump (+0.11), and the bass's sub or body up (quieter: -11.81 and -10.63, low notes add peak faster than loudness).

**2. Warm growls (not done; it waits on a shared QA change).** GrowlTalk and GrowlDive are hollow from 90 to 400 Hz (Talk: 125 Hz -35.3 dB and 250 Hz -43.7 dB under its sub octave), and the other kits give their bass shots a warm B. But a warm growl can't pass this kit's groove check for bass shots (ADR 0011): the 2-step groove loop plays each 2-beat move from every kick and sums the copies, and the copies 3 16ths apart (the skip kick to the and of 3) cancel each other's F#1 sub by about 14 dB. Measured on prototypes: the largest body that passes lifts 250 Hz only to -27 (Talk) or -26 dB (Dive) under the sub, against the -10 the review asked for; at -10, Talk reads 18.6 dB over the groove limit. With a choked groove loop (each new hit ends the one before, as a mono bass voice plays), a warm Talk with its body resting before the kit's kick offsets passes every check with 250 Hz at -16.5 dB, a warm Dive at -17. That change is a request for the shared QA; until then the growls stay A only, and the page asks whether they sound thin.

**3. The bass as the hook (applied, with one part turned down).** The bass lane is 3 dB up (-15.8 LUFS before the loop's gain, 1.2 LU over the kick lane; it ducks 6 dB under every kick as before), and the reese move is rebuilt with a wider range: round 3's first move (0.8 to 3.6 kHz, shut on the downbeats where the line rests for the kick) changed each note's top by 2.4 dB at most. Now it shuts to 577 Hz into beats 2 and 4 of every bar and opens to 3.2, 4.7, 3.6 and 6.1 kHz after them in bars 1-4: on the 10 notes that cross a backbeat, the top (1-4 kHz) swings 20-34 dB inside the note, against 8-19 dB with no move. The sub octave reads +7.3 dB re 1 kHz (+6.1 before). Raising ReeseWarm's own sine sub 2-3 dB, as the review also suggested, measured the wrong way: the same lift on the lane made the master 1.3 LU quieter (sub notes add peak faster than loudness), and it would change a round 2 sound under its A/B. The top slope is now in the list of misses above.

**4. The shaker near the centre (applied).** At pan -0.6 the shaker, the 16th that drives the shuffle, played 9.8 dB louder on the left. Now it sits at -0.25 (3.5 dB), mirroring the hats at +0.2, and its width is its own: one bead train on both sides and each side half its own noise (its files: side 3.8-6.2 dB under mid, correlation 0.41-0.62, about hat A's). The 8 kHz side reads -7.6 dB (-7.7 before; at -0.25 with the old shaker it read -9.1).

## What changed since round 2, and why

The standard measured FUT01's round 2 loop against current UK garage and listed eight changes. They were made in its order, and the same loop was rebuilt and measured after each one (round 2's row is bit-exact with `build/fut01-r2`); the last row is the fix round. Club readings unless marked:

| After | LUFS-I / PLR | Wet PLR | Hole 125 / 250 | 125 / 250 / 500 re 1k | 2k / 4k / 8k / 16k re 1k | Side 250 / 8k | Kick jump club (wet) | Clap jump | Backbeat (wet) |
|---|---|---|---|---|---|---|---|---|---|
| round 2 | -9.86 / 8.84 | 14.04 | -9.26 / -8.77 | -3.8 / -4.4 / +3.3 | -2.2 / -5.4 / -7.3 / -11.6 | -5.5 / -9.9 | 8.36 (9.18) | 13.39 | +1.44 |
| 1 density: kick clip, pad up | -9.69 / 8.67 | 13.94 | -9.35 / -7.99 | -4.7 / -4.3 / +2.7 | -2.8 / -6.3 / -8.1 / -12.5 | -5.0 / -9.5 | 7.82 (8.52) | 12.71 | +1.70 |
| 2 low mids filled, centred | -9.96 / 8.94 | 14.12 | -8.19 / -6.10 | -2.5 / -1.9 / +2.6 | -1.8 / -5.0 / -6.9 / -11.3 | -15.6 / -9.5 | 8.21 (8.77) | 13.53 | +2.61 |
| 3 kick B (Punch) | -10.39 / 9.37 | 14.02 | -7.23 / -5.89 | -1.7 / -1.8 / +2.6 | -1.7 / -5.0 / -6.8 / -11.3 | -15.8 / -9.6 | 7.70 (8.45) | 13.56 | +1.70 |
| 4 snare C (Clap) | -10.38 / 9.36 | 13.24 | -7.21 / -6.28 | -1.7 / -2.2 / +2.6 | -2.0 / -4.6 / -6.3 / -11.2 | -15.4 / -9.8 | 7.68 (8.45) | 12.83 | +0.93 |
| 5 the reese move | -10.39 / 9.37 | 13.29 | -7.23 / -6.56 | -2.0 / -2.8 / +2.3 | -2.5 / -5.2 / -6.7 / -11.6 | -15.1 / -9.8 | 7.66 (8.45) | 12.77 | +0.91 |
| 6 garage plate, thinner echo | -10.02 / 9.00 | 12.64 | -6.67 / -5.12 | -1.8 / -2.3 / +0.8 | -2.4 / -4.9 / -6.5 / -11.4 | -15.3 / -9.7 | 7.55 (8.21) | 12.27 | +1.00 |
| 7 the shaker, hats up | -10.10 / 9.08 | 12.44 | -6.64 / -5.13 | -1.8 / -2.3 / +0.8 | -2.3 / -4.6 / -4.4 / -8.9 | -15.4 / -7.6 | 8.65 (8.77) | 12.25 | +0.78 |
| 8 the hooks (first render) | -9.92 / 8.90 | 12.23 | -6.59 / -5.10 | -2.5 / -3.1 / 0.0 | -3.0 / -4.9 / -5.0 / -9.6 | -15.4 / -7.7 | 7.83 (8.19) | 12.02 | +0.87 |
| 9 the fix round = round 3 | -9.70 / 8.68 | 11.81 | -5.95 / -4.96 | -1.0 / -2.4 / +0.3 | -2.8 / -4.9 / -5.3 / -9.8 | -16.3 / -7.6 | 7.72 (8.18) | 13.54 | -0.20 |

Low end alone, every step: `Context Loop Low` -28.7 to -30.0 dB against the sub in the gaps through step 8 and -26.35 after the fix round (limit -20), -54.3 to -54.9 dB before each kick (limit -30); `Low Warm` -4.78 dB in round 2, -3.72 to -3.68 from step 2 on, -3.69 after the fix round (bass-body limit -2).

**1. Density, then a master.** Round 2's loop was a mix at a PLR of 14 dB. The kick lane takes a soft clip (4x oversampled; 4 dB at first, 2 dB since the fix round), the pad comes up 3 dB to carry the harmony, and `ContextLoopClub` runs the wet loop through the master: its glue (2:1, 2.5 dB at the loudest moment), the clipper and the limiter.

**2. The low mids, filled and centred.** The bass lane is mono under 450 Hz and narrower over it (its side × 0.6), its body +2 dB at 180 Hz and its top -2.5 dB at 900 Hz; the pad is mono under 350 Hz, +5.5 dB at 250 Hz (its file dips 5 dB at 280 Hz) and -3 dB at 700 Hz; the chops -2 dB at 500 Hz. The side at 250 Hz went from -5.0 to -15.6 dB (target ≤ -10).

**3. Kick B, Punch, as an A/B against kick A.** Round 1 rated kick A (Split) Cool 4 at its long sub, so B joins it instead of replacing it. Top layer, round robins 1 / 2:

| | A, Split (round 1's pick) | B, Punch | Target |
|---|---|---|---|
| Sub 30-120 Hz, 20 dB down | 316.9 / 319.4 ms (2.8 16ths) | **220.2 / 220.6 ms** (2.0 16ths) | about 220 ms |
| Knock, 100-150 Hz against 30-100 Hz, first 50 ms | -7.9 / -7.6 dB | -4.2 / -4.1 dB | a knock at 100-150 Hz over 40-50 ms |
| Crest, true peak over the loudest 50 ms (ADR 0011), the file | 6.44 / 5.82 dB | 6.65 / 5.96 dB | floor 4; round 1 liked 5-7 |
| …in the loop's kick lane, through its 2 dB clip (full-velocity kicks) | 5.17 (the A/B groove) | 5.18-5.26 | |
| Click re body | -19.9 / -20.1 dB | -19.2 / -19.7 dB | |
| Share at 150-400 Hz | -17.8 / -18.2 dB | -15.8 / -16.0 dB | the mud gate passes on every layer |

The loop plays B.

**4. The backbeat down to the kick.** Snare C, Clap, is snare A's crack under three tight hands and a short room, through snare A's soft clip. On the top layer its hands peak 9 and 16 ms after the crack, all three within 2.3 dB of the loudest; its tail falls 20 dB in 143 / 142 ms (target 120-250); its 150-400 Hz body reads -11.2 dB (target -15 to -8). The loop plays C at -2.2 dB; with the louder bass dipping under each snare, the backbeat sits 0.20 dB under a kick alone. A (Metal) and B (Modal) stay unchanged for the A/B.

**5. The reese talks.** Over 450 Hz the reese runs through a resonant low-pass on a CC74-style lane. Since the fix round it closes to 577 Hz into beats 2 and 4 (under 650 Hz from 50 ms before each snare to 50 ms after it) and opens after them, to 3.2-6.1 kHz, the widest in bar 4 into the turnaround: a note pushed onto the swung 16th before the snare starts open and closes into it, one that starts on the snare opens out of it. The sub and B's body pass untouched, and the 6 dB kick ducker stays; the lane also dips 4 dB for 32 ms under every backbeat.

**6. A garage plate.** The Space send is a plate in place of the shared 2.6 s hall: 1.1 s, a 1/64-note pre-delay (28 ms), its low band gone in 0.4 s, fed from 500 Hz up. The Echo's loop is high-passed at 600 Hz instead of 300. The 500 Hz octave went from +2.3 to +0.8 dB (the hall's wash) and the wet PLR from 13.29 to 12.64.

**7. A swung top.** The shaker plays every 16th up to the fill on the kit's swing (0.57), loudest on each beat's "a" (the last 16th, swung late), then the "and", the downbeat and the "e"; since the fix round it sits near the centre (-0.25) opposite the hats (+0.2, as in rounds 1 and 2), which come up 2 dB, its width its own stereo noise. 8 kHz went from -6.5 to -4.4 dB re 1 kHz at this step and 16 kHz from -11.4 to -8.9. The lanes sit at -26.7 (hats) and -25.6 LUFS (shaker) against the kick's -17.0: 8.6-9.6 LU under, against the standard's 7-8 LU for a top loop.

**8. Vocal hooks.** Two of round 2's chops, re-pitched a fifth up through WORLD with their formants raised 6 % (the shared Kokoro to WORLD chain): "me" on E5, the answer on the swung 16th after bar 3's last snare, on a little plate and no echo; and "floor", F#5 falling to E5, doubling round 2's "floor" 5 dB under it on the throw into the fill. The echo is for the phrase's end only: with the full vox sends, "me"'s repeats sat on bar 4's kicks. They sing no new words, so no title check; they are named by pitch, not "high", since "high me" is the title of a released single and "me high" ends others.

## The 12 sounds

| # | Sound | Status | Listen to |
|---|---|---|---|
| 1 | Kick, A Split / B Punch | **B new**; A unchanged | `Loops/Drum Groove Punch`, `Drum Groove Split`; `Drums/Kick Punch v5` … `v1`; `qa/FUT01_Drums_KickPunch_2step.flac` |
| 2 | Snare, A Metal / B Modal / C Clap | **C new**; A, B unchanged | `Drums/Snare Clap v5` … `v1`, `Snare Metal`, `Snare Modal` |
| 3 | Closed hat, A FM / B Ring, and the shaker | **shaker new**; hats unchanged | `Drums/Shaker v4` … `v1`, `Loops/Drum Groove Swing 61`, `Hat Pattern FM`, `Hat Pattern Ring` |
| 4 | Alloy hit, F#5 | unchanged | `Drums/Alloy Hit`, `Drums/Alloy Hit Short` |
| 5 | Reese bass, three basslines | unchanged; in the loop 3 dB up, the backbeat move | `Loops/Bassline B Clean`, `Warm`; `Context Loop Low`, `Low Warm` |
| 6 | Growl moves | unchanged (a warm B waits on a QA change: the fix round, 2) | `Bass/Growl Talk F#1`, `Bass/Growl Dive F#1` |
| 7 | Air pad, F#m9 | unchanged; in the loop +3 dB, centred low mids | `Loops/Air Pad Hold` |
| 8 | Chrome pluck and phrase | unchanged (your Cool 5) | `Loops/Chrome Pluck Phrase` |
| 9 | Sung line, chops and hooks | **two hooks new**; lines and chops unchanged | `Vox/Chop Me E5`, `Vox/Chop Floor F#5`, `Vox/Sung Line 1` |
| 10 | Robot phrase | unchanged | `Vox/Robot Boost`, `Vox/Robot Boost Wet` |
| 11 | Glitch fill | unchanged | `FX/Glitch Fill Lift` |
| 12 | Riser | unchanged | `FX/Shift Riser`, `FX/Shift Riser Drop` |

The new sounds, two sentences each:
- **Kick B, Punch:** kick A's split design (a clean F#1 sine sub under a separate knock and an alloy click) with its sub held flatter and gone by about two 16ths, and a 100-150 Hz knock on the sweep's second harmonic over its first 50 ms. The knock rises in after the click, and the sub ducks under it, so the click still holds the true peak on layers 2 to 5.
- **Snare C, Clap:** snare A's FM crack and alloy ring under three hands of band-passed noise over its first 16-21 ms (tighter and brighter on harder hits) and a short room (0.45 s). It goes through snare A's 4x soft clip, 2 dB lower into it so the clip stays as gentle as snare A's.
- **Shaker:** Poisson-timed grains of band-passed noise (about 4-14 kHz) with a faint alloy glint at 5.4 kHz, a soft attack and a fall that is 20 dB down in 55-89 ms, inside a 16th. One bead train plays on both sides and each side has half its own noise, so it is about as wide as hat A; four velocity layers move its brightness and level, and the four round robins sit within 0.02 dB of each other.
- **Chop Me E5 and Chop Floor F#5:** round 2's "me" and "floor" chops from line 1, sung a fifth higher by scaling WORLD's pitch and stretching its spectral envelope 6 %. Each is cut and polished as the chops are, so its consonant leads the note as in the line.

## The A/Bs

- **Kick: A, Split or B, Punch.** The two drum grooves play bars 1-2 of the loop's drums (snare C, hat A, the shaker, at the loop's balance) with each kick through the loop's 2 dB kick clip, both at -16.5 LUFS, so the difference is the kick alone, as the loops play it. The page's two one-shots of each kick are each at their own peak, so Split reads 0.65 LU louder there (-13.25 against -13.89 LUFS-I): the grooves are the level-matched A/B.
- **Swing: 0.57 or 0.61.** `Drum Groove Swing 61` is `Drum Groove Punch` with every odd 16th 9 ms later (24.6 ms behind straight instead of 15.7), at the same loudness. It is the standard's optional A/B (it names 0.60-0.62; guides run from 52-56 % on hats to MPC 68-69). Only the drums move: the vocals, pluck phrase and basslines are rendered on the kit's swing, so picking 0.61 would change the kit's DNA and re-render them.
- **Bass: A, clean or B, warm.** Unchanged from round 2, one loudness in every loop; the wet and club loops play B, the bass designer's lean.
- **Snare: A, B or C**, and the hats A or B, all still unrated.

## What to listen for

- The club loop next to a current UK garage record you know, at matched loudness: is it as full, as punchy, as wide? Does the bass lead now, and does the kick still punch through it?
- Kick B against A: does B hit harder and leave the bass more room, or did A's longer boom feel better?
- Does the snare-clap sound crisp and tight, or too roomy? Does it still crack over the louder bass?
- Does the reese talk against the snare now, closing into each backbeat and opening after it, or does it sound like a wobble?
- Does the shaker lift the shuffle or clutter it? Is the top too bright (the slope says it may be)? Does 0.61 groove harder than 0.57?
- The plate against round 2's hall: is the snare tighter, and are the chops still in a space?
- The hooks: a garage vocal, or a cartoon? Does "me" answer the snare, and does the doubled "floor" land the throw?
- The growls next to the warm reese: do they sound thin?
- On small speakers and in mono: is anything lost (the shaker's width, the reese's top)?

## MIDI

`content/midi/fut01/`, from the kit's parts (`tools/sound/recipes/fut01/midi.py`): the drum part maps kick B (36), snare C (38) and the shaker (54, General MIDI's tambourine: GM has no shaker inside 35-59, the plan's drum range, so a file played back into Wubansia can't trigger a sung chop on 60-84) beside hat A (42) and the alloy hit (47), the shaker on every swung 16th up to the fill; kick A, snares A and B, hat B and the short alloy hit are the alternates. The basslines, lead and chords are round 2's. The shared exporter also arranges these parts as a 224-bar extended mix (6:41) in `extended/`: one file, one region per section and `regions.json`. The fix round changed no note; the finishing pass rearranged the extended mix (below) and regenerated the folder (21 files). The reese move and the snare dip are mix automation, not notes, so the MIDI doesn't carry them.

## The finishing pass: shared QA and the MIDI form

Nothing you hear changed in this pass: every master is byte-identical to the fix pass's, re-rendered from scratch in a fresh folder (`--repro`, every item twice). What changed is what QA reads, and the MIDI's extended form.

- **The master's cap.** The club loop masters at the standard's own 4 dB, nothing declared; QA now checks the cap the master ran with, so a raise would have to be declared.
- **Each kick's crest as the loop plays it** (ADR 0011's measure over the 200 ms from each full-velocity kick; the floor is 4.0 dB): `Context Loop Low` 5.38 dB at the lowest (median 5.39), `Context Loop Low Warm` 5.38, the club loop 5.63 (median 5.68; in the master's input 7.18).
- **What the master costs the hits** (the median loss of that crest from the master's input to the club loop): the kicks 1.52 dB, the snares 1.54 dB. In their own band (1-5 kHz) the snares change by -0.14 dB: the master flattens the kick and the low end, not the snare's own snap. The A&R review proposed at most 1.5 dB, not yet calibrated by ear, so the club loop declares 1.6 dB, with its reason (an exception in `qa_report.md`).
- **Where the clipper works.** The kicks' first 30 ms go up to +5.0 dB over the clipper's knee; the deepest moment away from every kick and snare goes +4.9 dB over it, 6.94 s into the loop.
- **Octave windows.** The 125 and 250 Hz windows are exempted by name now (they read -1.0 and -2.4 dB re 1 kHz); every other band is gated again and passes, the closest 0.10 dB inside its window.
- **Latency.** The stand-in master's limiter starts its gain down 3145 samples (65.5 ms) before a peak: its 5 ms lookahead and its 60 ms hold, which reaches forward too. The engine's master (S1e) reports that latency or redesigns the hold (ADR 0014).
- **MIDI.** The extended mix (`content/midi/fut01/extended/`, regenerated) takes UK garage's own form now: a 16-bar intro of drums (the 2-step kick under the full swung hats and the shaker, then the 2-step beat), a 48-bar groove in which the bassline and then the harmony come in, then the break, its build and drop 1 at bar 97. Still 224 bars; the loop files' notes are unchanged.

## The verification pass

An independent review checked this page, the kit's files and the shared tools against each other; this pass fixed what it confirmed. Nothing you hear changed: all 51 items re-rendered from scratch (`--repro`, every item twice) give the same 184 FLAC files byte for byte, with 0 failed checks.
- **MIDI.** The extended mix's first and last 8 bars now play the 2-step kick under the full swung hats and the shaker, so a DJ has every 16th to beatmatch on instead of two or three kicks a bar. The build's last 8 bars roll the clap (snare C) on every quarter for 4 bars, every 8th for 2 and every 16th for 1, rising to full velocity under the riser, then the written fill bar with the kit's fill in bar 96; the kick sits the roll out, so drop 1 lands on its return. The shaker moved from note 82 (inside the sung chops' 60-84) to 54. `content/midi/fut01/` is regenerated; a fresh export matches it.
- **QA's crest per hit** now reads each hit's true peak with the loop round its span, so a span that ends in a loud passage can't over-read; no FUT01 reading moved.
- **This page:** the short-term loudness target is the standard's row for a 4-bar loop, the kick jump is given over all 11 kicks and over the full-velocity four, the kick one-shots' 0.65 LU difference is named, and the MIDI section gives the shaker's new note.

## QA in one line

51 items, 180 files, every check passes (`qa_report.md` lists each exception and its reason: the full mixes' low-end rule, the club loop's octave windows, the vocals' synthetic top band, the two end-aligned FX), and every master rendered the same SHA-256 a second time, each item in a fresh process (`--repro`, 51 of 51), vocals included. 133 of the 138 files shared with round 2 are byte-identical; the 5 that differ are the rebuilt context loops. Against round 3's first render, the fix round changed the shaker (16 files), the three drum grooves and the six context loops, and nothing else (kick B, snare C and the hooks keep their bytes). Low end: `Context Loop Low` reads -26.35 dB against the sub in its gaps (limit -20) and -54.41 dB before each kick (limit -30); `Low Warm` -3.69 dB (bass-body limit -2) and -3.35 dB over a saw (limit +5). Kick B's top-layer crest is 6.65 / 5.96 dB and kick A's 6.44 / 5.82 (floor 4.0); in the loops' kick lane, through its 2 dB clip, every full-velocity kick reads 5.18-5.26. Whisper hears the "me" hook as "me" as it is, padded and 10 dB down; the "floor" hook as "The floor!" as it is, but as wordless singing padded and as a sigh 10 dB down (round 2's "floor" reads "the floor.", "FLOOR!" and "FL"); the hooks' bytes are those that were checked. The kit's 80 unit tests pass, and its MIDI matches a fresh export.

None of this says how anything sounds.
