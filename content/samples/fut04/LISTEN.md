# FUT04 "Furnace Code": round 3, the modern production pass

Written for the kit's render folder (`build/fut04-r3`, from `render.py --kit FUT04 --set r3`), which holds the FLAC masters, `qa/`, `sheets/`, `qa_report.md`, `qa_report.json` and `listen.json` this page names; its copy in `content/samples/fut04/` sits beside the WAVs alone.

A dark industrial techno kit at 136 BPM in E minor (9A): a heavy split kick on E1, a gated iron snare and a 16th acid bass over a kick-fed rumble, with modular blips on top, every sound on one struck-iron tone (an anvil's modal ratios) and one dark concrete room. Round 3 keeps round 2's parts and lines and works on how they sound together, in the standard's order (`docs/plans/S1-modern-sound.md`, FUT04): a distorted drum bus and a club master, weight in the kick where small speakers play, body in the low mids, the rumble under the whole groove, a dark hall and a pump, a ride on the off-beats, the vocals under the kick, and an acid with an edge and a four-bar filter ride. A fix pass after the A&R review then rebuilt the ride and made the kick lead its own sub (below).

**Nobody has listened to round 3.** Every number below is a measurement made in this session; none of it says how it sounds.

## Start here

`Loops/Context Loop Club.wav`: the wet context loop through the club master. Then the dry loops (A and B, same loudness) and the wet one, then the sounds that changed.

| Club master | Reading | Techno target |
|---|---|---|
| Loudness, LUFS-I (round the loop) | -8.04 | -8 ± 1 |
| Loudest 3 s (LUFS-S) | -7.69 | a 4-bar loop: its LUFS-I plus a few tenths (+0.35 here; not gated) |
| True peak | -1.02 dBTP | ≤ -1.0 |
| PLR / PSR | 7.02 / 6.67 dB | 6.5-8 / ≥ 6 |
| Octaves re 1 kHz: sub, 125, 250, 500, 2k, 4k, 8k, 16k | +5.7 (in the 31.5 Hz octave: E1), -1.1, -1.6, +1.1, -1.3, -3.3, -6.1, -15.7 dB | +4..+12, +1..+6, 0..+4, 0..+3, -3..0, -6..-2, -10..-5, -17..-10 |
| Low-mid hole, 125 / 250 Hz (against the line from the sub to 500 Hz) | -5.3 / -4.2 dB | ≥ -4 (reported) |
| Sub hump: the sub over 500 Hz | 4.7 dB (the design pass 7.8) | +6 to +12 (reported) |
| Top slope, 500 Hz-8 kHz | -1.79 dB/oct, 0.1 brighter than the window's edge | -2.5 ± 0.6 (reported) |
| Side over mid at 125 / 250 Hz; under 120 Hz | -29.2 / -14.1; -45.4 dB | ≤ -10; ≤ -20 |
| Width at 2 / 4 / 8 / 16 kHz | -8.3 / -8.3 / -7.4 / -6.7 dB | -10 to -4 (2-4k), -8 to -3 (8-16k) |
| Peak reduction (clipper + limiter) | 6.0 dB, the whole cap (4.5 clip, 1.5 limit) | the standard's convention: ≤ 4 (see below) |
| Kick jump in the loop, full band / 40-120 Hz | 10.5 / 19.8 dB; the master's glue, clip and limiter cost it 0.1 / 0.4 dB (its 25 Hz low cut moves the 40-120 Hz reading 6.1 dB more, by delay: below) | the master costs it at most 3-4 dB |
| Clap jump, 1-5 kHz | 18.4 dB; the master costs it 0.5 dB | ours: ≥ 12 |
| The low end per beat, 35-60 / 30-120 Hz: the loudest 20 ms slice from each kick (median) | on the kick, 60 / 40 ms after it, 0.1 / 0.9 dB over anything later (the unmastered loops: 0.9-1.0 / 2.2-2.3 dB) | on the kick (the A&R review) |
| 6-16 kHz, each 16th's first 25 ms over its last (median) | 10.8 dB (the hat loop 15.0) | the other kits' club loops 10.9-20.1 (no target) |

**Three deviations from the standard, each declared in the recipe:**
- **The master's cap.** The standard's convention is at most 4 dB of peak reduction. At 4 dB this loop stops at -9.55 LUFS, 0.55 LU under the techno window, at 5 dB at -8.78; the 6 dB cap (`context.MASTER`) takes it to -8.04, 0.04 short of the target and well inside the window. The design pass's loop reached -8.0 with 5.6 dB; the fix pass's reads 0.3 LU quieter at the 4 and 5 dB caps (the design pass's -9.25 and -8.47), because its low end no longer stacks the rumble and the bass's sub on the kick's tail: less sub under the same peaks. What the cap is there to protect holds: the master's glue, clip and limiter cost the kick's jump 0.1 dB (full band) and 0.4 dB (40-120 Hz) and the clap's 0.5 dB, and the master moves the low mids at most 0.15 dB (125 and 250 Hz against the wet loop). One reading needs care: against the wet loop that goes in, the kick's 40-120 Hz jump reads 6.5 dB lower after the master (26.3 to 19.8 dB), and 6.1 dB of that is the master's 25 Hz low cut, which is flat in that band to 0.1 dB but delays it (7.6 ms at E1, 3.1 ms at 60 Hz). The jump reads the loudest 10 ms in the first 30 ms after each kick, and this E1 kick builds its 40-120 Hz late in that window: the same loop played 2 ms late reads 3.4 dB lower. So that 6.1 dB is the measure reading a delay, not lost punch, and the render's own 20 Hz subsonic filter does the same to `ContextLoopWet` (21.6 dB). Your ear decides whether the low cut softens the kick.
- **The octave windows at 125 and 250 Hz** (`qa={'club_bands': ('125', '250'), 'why': ...}`: those two exempt, every other band gated). Every other octave sits in its window; 125 and 250 Hz read -1.1 and -1.6 dB re 1 kHz, 2.1 and 1.6 dB under theirs. Those windows are conventions built on a chart-record average (the standard's section 0), which its S4 replaces with measured references. The kit's low end keeps round 1's rules: the kick, the rumble and bass A keep the strict mud gate, bass B the bass-body rule with its 2 dB margin, and every other lane is high-passed. The hole reads -5.3 / -4.2 dB (round 2 -10.1 / -6.7, the design pass -7.1 / -5.3); the fix pass's share of that is not fuller low mids but a lower sub octave, which the line is drawn from (below).
- **The rumble's level and the ducks' length** (`context.LANE_LUFS`, `bass.PUMP`, `context.BASS_SIDECHAIN`). The standard's change 4 asks for the rumble about 4-6 LU under the kick; the design pass set it 5 LU under, and with its swell on the kick's tail that is what peaked the low end after the kick (finding 2, below). Its lane now reads -30 LUFS, 13 LU under the kick. With the later swell that is the loudest level, in 1 dB steps, at which the kick still leads in the club master: at -28 the club's 35-60 Hz peaked 0.2 dB over the kick after it; at -29 and -30 the kick leads by 0.1 dB, and -30 gives the wet loop 1.0 dB (-29: 0.6). A loudness reading is a poor ruler for a sound that is nearly all sub (K-weighting reads 41 Hz 6 dB under 1 kHz): in the band it plays, its plateau sits 8.4 dB under the kick's 35-60 Hz peak, 4.1 dB lower than the design pass's (4.3), the 3-4 dB the review asked for; its loudness fell further, 8 LU, because its swell is also shorter (full for 30 ms, the design pass's for 90). Both ducks are longer than the standard's techno row (the rumble ducked for the first 100-150 ms only): the rumble is 40 dB down to 90 ms and full only from 270 ms, the bass's sub 8 dB down for 250 ms. That is this kick's length: its own sub is 20 dB down only at 315 ms, and the same row of the standard lets the rumble carry the kick's tail.

## The fix pass: the A&R review's three findings

The A&R review measured the design pass's render and filed three findings for FUT04. Each was re-measured here first; two were fixed, one is the lead's call. The before column is the design pass's render (the same bytes as before this pass), the after column this render.

### 1. The ride was a bell chord: rebuilt

The design pass's ride sustained seven squares on the anvil's ratios, a chord: from 0.2 to 0.7 s, 82% of its 1-12 kHz energy lay within 15 Hz of 12 lines (band noise reads 6% on the same measure), and it was 20 dB down only at 1.02 s. It plays once per beat, on the off-beat (441 ms apart, not every 220 ms as the review had it), so two to three hits rang at once over the closed 16ths, and the hat loop's 16ths lost their articulation.

Now the anvil's modes on E6 are the stick's ping, gone by 80 ms, and the sustain is dense noise, band-passed from 5-11 kHz on the softest hits to 6-14 kHz on the hardest, coloured by four broad resonators on the anvil's ratios over E7 (4.1, 6.4, 8.5 and 11.1 kHz, 6 dB each, Q 4). It fades 20 dB in 0.38 s, so each hit has faded more than 20 dB when the next lands, and the ride sits 3 LU under the hats (-28 LUFS, the design pass -26).

| Ride, top layer (rr1 / rr2) | Before | After |
|---|---|---|
| 1-12 kHz in 12 lines, 0.2-0.7 s / 0.1-0.4 s | 81.7 / 82.0% ; 79.2 / 79.3% | 10.5 / 11.1% ; 10.5 / 12.0% |
| 10 / 20 / 30 dB down, from the onset | 0.69 / 1.02 / 1.12 s | 0.22 / 0.38 / 0.52 s |
| File length | 1.25 s | 0.78 s |
| Centroid; side over mid above 4 kHz | 8.7 kHz; -5.5 dB | 9.2 kHz; -4.3 dB |
| Velocity steps; round robins | 2.4-3.2 dB; within 0.02 dB | 2.8-3.0 dB; within 0.11 dB |

| 6-16 kHz, each 16th's first 25 ms over its last (median) | Before | After |
|---|---|---|
| Hat loop (round 2, no ride: 39.1) | 7.6 dB | 15.0 dB |
| Dry A / dry B / wet / club | 7.2 / 7.2 / 7.3 / 7.1 dB | 10.7 / 10.7 / 10.7 / 10.8 dB |

- In the club loop, the old ride's 12 lines stood 16.8-37.1 dB over the spectrum 40-200 Hz around them; now 15.1-25.5. What is left there is the hats' own squares: in the wet loop, with the hats muted they fall from 14.6-25.2 to 10.6-17.2 dB, with the ride muted they don't move (14.3-25.3). The classic metal hat is a stack of squares, but a short one.
- The other kits' club loops read 10.9 (FUT02), 14.2 (FUT01) and 20.1 dB (FUT03) on the same 16th measure, as rendered today.
- **Not changed:** the review asked to choke the open hat on the next closed hat again. It already is: in the context loop's hat lane and the hat loop, every open hat is cut by the next closed one over 4 ms (its energy after the choke reads -300 dB, nothing), as in round 2.
- **Whisper still hears no words** in the club loop (bars 1-4: "(upbeat music)", "I'm in a new family", "(upbeat music)", "(upbeat music)"; the design pass: no words). In the dry loop (bass A) it hears none either, with the ride muted none; it reads the vocals only with every lane but the vocals muted ("fill the furnace.", word for word, and "Come down me furnace"). So the ride was not what hid them; the vocals sit 2.5 LU under the kick by design (the standard's change 7), and Whisper is a poor judge of words in a dense mix.

### 2. The low end peaked after the kick: the kick now leads its own sub

The review found the low end's 35-60 Hz loudest 140-200 ms after each kick, 3-4 dB over the kick's own 60-100 ms, because the rumble's swell and the acid's E1 pedal were both turned in phase with the kick's tail (`bass.RUMBLE_PHASE`, `bass.PEDAL_PHASE`) and added to it there; the master clipped that swell hardest. Measured here the same way (20 ms slices from each kick, median over the kicks; "margin" is the loudest slice from 120 ms on against the loudest in the kick's 40-100 ms, positive when the low end peaks after the kick):

| 35-60 Hz / 30-120 Hz margin, loudest slice | Before | After |
|---|---|---|
| Club master | +3.2 / +3.4 dB at 180 ms | -0.1 / -0.9 dB at 60 / 40 ms |
| Wet | +3.4 / +3.6 dB at 180 ms | -1.0 / -2.3 dB at 60 / 40 ms |
| Dry A / dry B | +3.5 / +3.4 dB (35-60) | -0.9 / -1.0 dB |
| Low end, A / B (kick, bass and rumble) | +3.5 / +3.4 dB (35-60) | -1.0 / -1.0 dB (30-120: -2.2 / -2.3) |
| Rumble under the kick (the preview) | +2.3 dB at 160 ms | -1.4 dB at 60 ms |

The master's hardest clips that aren't on a kick:

| 16th (ms after its kick): dB over the clipper's knee, % of the peak under 120 Hz, the clipper's 1-6 kHz excess re the music | Before | After |
|---|---|---|
| Hardest | 45 (198 ms): 6.6 dB, 41%, -12.8 dB | 43 (341 ms): 7.0 dB, 20%, -18.0 dB |
| Second | 25 (139 ms): 6.4 dB, 43%, -15.1 dB | 26 (234 ms): 6.2 dB, 22%, -25.2 dB |
| Third | 2 (269 ms): 6.4 dB, 38%, -19.5 dB | 58 (225 ms): 5.8 dB, 16%, -19.0 dB |

So the clipper no longer works hardest on a sub swell: its three hardest clips off the kick are the acid's accents (16th 43, bar 3's accented D2 from the 8th before it, under a closed hat; 26 and 58, the accented G2), 78-84% of the peak above 120 Hz.

What changed, and why this way:
- **The rumble swells later** (`bass.PUMP`): held 40 dB down to 90 ms, -3 dB at 210 ms, full from 270 to 300 ms, 40 dB down by 380 ms (the design pass: full from 150 to 240 ms, down by 375). It takes the E1 over as the kick's tail fades instead of stacking on it, and peaks 270-300 ms after each kick, under the kick's own peak. It is gone 60 ms before the next kick: a swell held to 340 ms took 4 dB off the kick's 40-120 Hz jump.
- **The rumble sits lower**: its lane at -30 LUFS (the design pass -22), the loudest level at which the kick still leads in the club master; its plateau in 35-60 Hz 8.4 dB under the kick's peak (the design pass 4.3). That is 13 LU under the kick against the standard's 4-6, a deviation (above). It still adds weight on every beat (30-120 Hz, kick and rumble against the kick alone: +0.9 dB per beat, +4.9 dB in 200-375 ms after each kick, where it carries the gap) and takes almost none away anywhere (the worst 40 ms window -0.6 dB).
- **The acid's sub ducks under the kick** (`context.BASS_SIDECHAIN`): the bass lane's band under 120 Hz, 8 dB down from each kick, held 250 ms over the line's pedal and back by 332 ms, after the fader, the way the dips under the vocals work. The standard noted that nothing ducked FUT04's bass under its kick; its techno row asks 4-8 dB. The lines themselves are unchanged files.
- **The rumble's phase stays at 45 degrees.** The review asked for an A/B against round 2's 135. At 135 the kick led too (1.2-1.4 dB in the unmastered loops), but the rumble then sat in quadrature with the bass's pedal, which follows the kick's tail: with the bass 3-5 ms late the two cancelled on the off-beats, the warm low end's bass-body reading went from -6.2 to +3.0 dB (limit -2), and the rumble drifted against the kick's tail (a 40 ms window 1.6 dB under the kick alone). In phase with both, nothing cancels at any offset: the low-end test's worst case, the bass 5 ms late, reads -4.9 dB (B, bass-body, limit -2) and -33.5 dB (A, the gate, limit -20).

What it costs:
- **Less sub.** The club master's sub octave (31.5 Hz, E1) reads +5.7 dB re 1 kHz (the design pass +8.8), the 63 Hz octave -0.8 (+0.6). That is inside the window the QA reads it in (+4 to +12, the standard's techno row for 63 Hz), but 0.3 dB under the +6 to +12 its section 1.2 asks of the louder of the two, and the sub hump over 500 Hz (report only) reads 4.7 dB against +6 to +12 (the design pass 7.8). It is the lightest of the four kits' club loops (FUT01-FUT03: +7.3 to +9.9). The swell stacked on the kick's tail was what made the design pass's low end heavier. Whether it now sounds tight or too light is for your ear.
- **The whole cap.** The master needs its full 6 dB and reaches -8.04 LUFS (above).
- The kick's jump holds (full band 10.5 dB, the design pass 10.1; 40-120 Hz 19.8, 20.5).

### 3. One sub note, E1: not changed here

The review measured the loop's whole sub on E1 (41 Hz): through a 2nd-order 50 Hz high-pass, a stand-in for club rigs that roll off at 45-50 Hz, FUT04's 30-120 Hz loses 4.1 dB against 1.9-2.2 for the other kits. Confirmed (club loop: -4.13 dB; third octaves 39 Hz +12.7, 50 / 62 / 79 Hz +1.5 / -1.3 / -1.5 re 1 kHz). After the fix pass the loss is 3.84 dB (39 Hz +9.6, 50 / 62 / 79 Hz +0.8 / -3.9 / -4.1): less E1, not more weight higher up.
- The review's first fix, the kick on G1 (49 Hz) with the rumble and the pedal following it, is a DNA change (`KICK_NOTE` in `tools/sound/kits/fut04.py`), outside this pass; it goes to the lead with a caveat: the bass's pedal puts the kick's note under each beat's first 250 ms, so a G1 kick would sound G under an E minor line unless the pedal changes too, and a G1 fundamental alone still loses 3.2 dB through that filter (E1: 5.0).
- Its second fix, the kick's low knock centred at 55-80 Hz instead of 60-140 Hz, was measured and not taken: at 45-90 Hz, 1.4 and 2 times as loud, the loss moved by -0.1 and +0.15 dB (measured before the low end's final balance), and the quieter one dropped the kick's 80-120 Hz more than 3 dB under its 30-80 Hz, which the kick's own test refuses.
- A third way inside this pass, the rumble's E2 resonance twice and three times as loud (`bass.RUMBLE_MIX`: 1.6 and 2.4 against 0.8), moved the loss 0.02 and 0.05 dB and took the warm low end's worst case from -4.9 to -4.2 and -3.6 dB (its test asks -4): not taken either.

## What's here

| Folder or file | What |
|---|---|
| `Drums/`, `Bass/`, `Music/`, `Vox/`, `FX/`, `Loops/` | WAV copies to listen to: 128 files, 48 kHz / 24-bit, the same samples as the masters. `v1` is the softest velocity layer, `rr` the round robin |
| `FUT04_*.flac` | The 128 masters |
| `qa/` | The four-on-the-floor loop the kick's mud check ran on |
| `sheets/` | One spectrogram sheet per instrument |
| `qa_report.md`, `qa_report.json` | Every QA check with its number, every exception with its reason, the render environment, the loop points, the club master's report and the wet loop's send and bus settings |
| `listen.json` | The listening page's entry for this kit, its first clip the club master |

## What changed since round 2, and why (the design pass)

The standard measured FUT04's round 2 loop against current techno records and listed eight changes. The design pass made them in its order and measured them one at a time: the ladder below rebuilds round 2's wet loop from round 3's context module (round 2's files and settings; it matches the round 2 file to 6e-8), then switches each change on. "Wet PLR" is the wet loop as `ContextLoopWet` renders it (after the render's 20 Hz subsonic filter), normalised to its ceiling; "club at 4 dB" is what the club master makes of that loop at the standard's cap, with the reduction the -8 target needs; "club at 6 dB" is the same at round 3's cap. The shipped `ContextLoopClub` masters the wet loop before that subsonic filter (the master's own 25 Hz low cut does that job), so the design pass's shipped loop read a little differently from the last row: the same -8.00 LUFS and 6.98 dB PLR with 5.6 dB of reduction instead of 5.8, -9.25 LUFS at the 4 dB cap instead of -9.40. These are the design pass's numbers: the fix pass then changed steps 4 (the rumble) and 6 (the ride) and added the bass's sidechain, and its loop reads as the tables above.

| After | Wet PLR | Club at 4 dB (needs) | Club at 6 dB: LUFS / PLR / reduction | Holes 125 / 250 | 125 / 250 / 500 re 1k | 2k / 4k / 8k / 16k re 1k | Width 2k / 4k / 8k | Kick jump full / 40-120 | Clap jump | Lanes over the kick | Low end A / B |
|---|---|---|---|---|---|---|---|---|---|---|---|
| 0 round 2 | 15.67 | -13.29 (10.63) | -11.54 / 10.52 / 6.00 | -10.1 / -6.7 | -4.3 / -3.3 / +1.3 | -4.2 / -7.1 / -9.3 / -17.4 | -8.0 / -8.8 / -10.8 | 10.9 / 22.3 | 17.4 | 6.08 | -35.1 / -5.56 |
| 1 density: drum bus, snare, master | 15.46 | -12.89 (10.05) | -11.11 / 10.09 / 6.00 | -10.2 / -7.3 | -5.1 / -4.2 / +1.0 | -2.6 / -4.2 / -8.4 / -19.3 | -7.5 / -8.4 / -10.6 | 11.8 / 22.3 | 19.6 | 6.02 | -35.1 / -5.56 |
| 2 kick knock | 15.39 | -12.98 (10.16) | -11.23 / 10.21 / 6.00 | -6.4 / -6.8 | -1.8 / -4.0 / +1.1 | -2.6 / -4.0 / -8.2 / -19.2 | -7.4 / -8.5 / -10.7 | 11.6 / 21.0 | 19.3 | 5.96 | -34.7 / -5.56 |
| 3 low mids | 15.13 | -12.59 (9.73) | -10.89 / 9.87 / 6.00 | -5.8 / -4.5 | -0.4 / -0.8 / +1.9 | -1.8 / -3.1 / -7.4 / -18.4 | -7.5 / -8.5 / -10.7 | 10.8 / 21.0 | 19.3 | 6.13 | -34.7 / -5.56 |
| 4 rumble, all 4 bars | 15.59 | -11.60 (8.62) | -10.00 / 8.98 / 6.00 | -7.0 / -5.1 | -0.3 / -0.8 / +2.0 | -1.8 / -3.1 / -7.5 / -18.5 | -7.5 / -8.5 / -10.7 | 10.8 / 21.1 | 19.3 | 6.71 | -35.3 / -5.56 |
| 5 dark hall, pump | 15.39 | -11.27 (8.26) | -9.72 / 8.70 / 6.00 | -7.0 / -5.3 | -0.4 / -1.3 / +1.7 | -1.9 / -3.2 / -7.6 / -18.5 | -7.7 / -8.5 / -10.8 | 10.9 / 21.1 | 18.7 | 6.71 | -35.3 / -5.56 |
| 6 ride, hats | 15.18 | -11.17 (8.13) | -9.63 / 8.61 / 6.00 | -7.0 / -5.3 | -0.4 / -1.3 / +1.7 | -1.9 / -2.9 / -6.5 / -15.1 | -7.7 / -8.1 / -7.9 | 10.4 / 21.1 | 16.3 | 6.80 | -35.3 / -5.56 |
| 7 vocals down, vocal bus | 15.48 | -11.31 (8.32) | -9.77 / 8.75 / 6.00 | -7.2 / -5.4 | -0.7 / -1.7 / +1.0 | -1.8 / -2.7 / -6.4 / -15.0 | -7.6 / -8.0 / -7.7 | 10.3 / 21.1 | 16.5 | 6.49 | -35.3 / -5.56 |
| 8 acid edge and ride | 12.17 | -9.40 (5.84) | -8.00 / 6.98 / 5.84 | -7.2 / -5.4 | -0.9 / -1.7 / +1.0 | -1.2 / -2.5 / -6.2 / -14.8 | -8.2 / -8.2 / -7.8 | 10.3 / 22.2 | 16.3 | 6.41 | -35.8 / -6.22 |

Targets: holes ≥ -4 (a shape check, reported); 125 +1..+6, 250 0..+4, 500 0..+3, 2k -3..0, 4k -6..-2, 8k -10..-5, 16k -17..-10 (dB re 1 kHz); width -10..-4 at 2-4 kHz, -8..-3 at 8 kHz; low end: the gate ≤ -20 dB (A), bass-body ≤ -2 dB (B).

**What set round 2's peaks.** Not the kick. Read lane by lane against the kick lane's own true peak, the acid's accents peaked 5.4 dB over it, the snare 3.9, the fill 1.7 and the hats' ticks 1.3; the lanes summed 6.1 LU over the kick. So the big move in the ladder is the acid's edge (step 8, PLR -3.3 dB), not the drum bus (step 1, -0.2).

**1. Density with genre saturation.** The drum bus (kick, snare, hats, ride, anvil, fill): a parallel grit, the bus high-passed at 250 Hz, lifted 3 dB at 3 kHz, driven through a tanh at 4x, dipped at 600 Hz and low-passed at 9 kHz, mixed 8 dB under (the standard's multi-stage distortion with EQ between the stages, none of it under 250 Hz, so the sub stays clean); then a soft clip at the kick lane's own true peak, so the backbeat, the ticks and the fill's roll stop setting the loop's peak. The kick alone passes the bus at its own peak (its true peak 0.02 dB lower). The snare's transient (below) is part of this step, and so is the club master. On its own the step moved the PLR only 0.2 dB, because the drums weren't what set the peaks; it brightened 2-4 kHz by 1.6-2.9 dB and lifted the clap's jump from 17.4 to 19.6 dB.

**2. Kick weight where speakers play.** The E1 kick put its weight under 44 Hz; its 80-120 Hz sat 14 dB under the whole hit. It gains a second, lower knock: the sweep's own octave, clean, in cosine phase (in sine phase the two summed into a peak 2.4 dB higher and the crest left Split's range), low-passed at 140 Hz, held over its first 50 ms and gone by 130 ms; the drive chain's crunch (still only above 300 Hz) lasts 50-60 ms; the iron top is 3.6 dB hotter. The 125 Hz hole went from -10.2 to -6.4 dB. The standard's alternative, tuning the kick up to G1 or A1, is a DNA change and wasn't made.

**3. The low mids.** The drone's 8 dB dip at 230 Hz is gone and its low mids are held in the middle up to 450 Hz (round 2: 150), the drone is 3 LU up, and the music bus (drone, blips, vocals, returns) has a 3 dB body at 250 Hz, a 2.5 dB dip at 1 kHz and a 1.5 dB lift at 3.5 kHz. Holes -6.4 / -6.8 to -5.8 / -4.5 dB. (Two sound changes belong to this fill and are already in steps 1 and 2: the kick's knock dips 2 dB at 240 Hz instead of 6, and the snare's plate keeps more of its B3 body.) The later steps give some of it back: the rumble, the ride and the brighter acid add to the octaves the holes are read against.

**4. The rumble as the groove.** Under all four bars (round 2: bars 3 and 4), out with the kick on bar 4's last beat. Its sub band reaches 100 Hz (round 2: 75) with a resonance on E2, the kick's octave; the wider band turned its phase at E1, so it was turned again (45 degrees, the best of a 15-degree sweep). The design pass set it 5 LU under the kick with its swell full from 150 to 240 ms, adding about 3 dB in 30-120 Hz under every kick; the fix pass moved the swell after the kick's tail and set the lane 13 LU under the kick (the fix pass, above).

**5. A techno space and the pump.** The kit's own Space: a dark 5 s hall (shared: 2.6 s), damped from 3.5 kHz (6.5), a 64th-note pre-delay (27.6 ms), half the shimmer, its send high-passed at 250 Hz (450), its low mids still gone in 0.8 s; the snare, the anvil and the drone go into it harder. Echo is a darker dub delay on the dotted 8th (LP 2.2 kHz, HP 400 Hz). The three returns duck 5 dB under every kick and are back by 270 ms; the drone ducks 3 dB, back by 210 ms.

**6. Tops that cut.** An iron ride on every off-beat 8th with the open hat (not choked), and the brighter, wider hats (below). In the design pass 8 / 16 kHz went from -7.6 / -18.5 to -6.5 / -15.1 dB re 1 kHz, and the width at 8 kHz from -10.8 to -7.9 dB, into the standard's windows; the fix pass's shorter ride reads -6.1 / -15.7 and -7.4.

**7. Let the kick lead.** The chops 2.5 LU down (-19.5 LUFS, round 2 -17, level with the kick) through a vocal bus: band-passed 120 Hz-8 kHz with a 3 dB body at 250 Hz, driven at 4x, compressed 4:1 (6 dB at its loudest) and given two short slaps (a 16th left at -9 dB, an 8th right at -13). The robot 2.5 LU down on its own chain: through the bus Whisper heard "and" for "Fill the furnace", without it word for word. The blips 0.5 LU down. The kick leads: the lanes sum 6.5 LU over it (6.8 before this step, round 2 6.1). The loop read 0.3 dB peakier, because the vocals were loudness that the acid's accents now stand over; step 8 takes those down.

**8. Acid with an edge.** The squelch runs into a distortion after the filter and after the VCA, as a 303 runs into a pedal: a presence lift into an asymmetric tanh at 4x, a dip at 1 kHz, a low-pass at 12 kHz and a steep high-pass at 700 Hz that keeps the drive's difference tones out of the body. A and B share that one squelch. The context loop plays the pick as its four-bar ride: the cutoff and resonance (bars 1-2 as written; 1.8 times and 0.94 by bar 4; 2.6 times and 0.96 on its third beat; closed by the downbeat). The loop's PLR fell from 15.5 to 12.2 dB, the club loop at the 4 dB cap rose from -11.3 to -9.4 LUFS, and the reduction the -8 target needs from 8.3 to 5.8 dB.

## The sounds

| # | Sound | Round 3 | Files |
|---|---|---|---|
| 1 | Split kick on E1 | changed: the low knock, a longer crunch, a hotter top | `Drums/Kick Split`, `qa/FUT04_Drums_KickSplit_4x4.flac` |
| 2 | Gated iron snare | changed: four even hands, more body, a harder clip | `Drums/Snare Gated` |
| 3 | Dark metal hats and the iron ride | changed: brighter hats, a wider open hat; **new**: the ride, rebuilt in the fix pass | `Drums/Closed Hat`, `Open Hat`, `Ride`, `Loops/Hat Loop` |
| 4 | Anvil on E5 | **unchanged** (the same bytes) | `Drums/Anvil` |
| 5 | Acid bass | changed: the edge; **new**: the pick's 4-bar ride; in the loop its sub ducks under the kick (fix pass) | `Loops/Acid Line 1 Ride Clean`, `Ride Warm`, `Acid Line 1/2 Clean`, `Warm`, `Bass/Acid Clean`, `Acid Warm` |
| 6 | Rumble | changed: the band to 100 Hz, E2, its phase; the fix pass: a later swell | `Bass/Rumble`, `Loops/Rumble Under Kick` |
| 7 | Drone, Em(add9) | changed: its low mids back, central to 450 Hz | `Music/Drone Em Add 9`, `Loops/Drone Loop` |
| 8 | Modular blips | **unchanged** (the same bytes) | `Loops/Mod Blip Phrase`, `Music/Mod Blip` |
| 9 | Sung chops | **unchanged** (the same bytes) | `Vox/Sung Line 1` … |
| 10 | Robot phrase | **unchanged** (the same bytes) | `Vox/Robot Fill The Furnace`, `Robot Fill The Furnace Wet` |
| 11 | Stutter fill | **unchanged** (the same bytes; its preview plays the round 3 drums and the new ride) | `FX/Stutter Fill`, `Loops/Stutter Fill Preview` |
| 12 | Iron riser | **unchanged** (the same bytes; its preview lands on the round 3 groove) | `FX/Iron Riser`, `Loops/Iron Riser Preview` |

Each changed sound against its round 2 file (the top layer, both round robins where there are two):
- **Kick.** Over its first 50 ms, 80-120 Hz now reads -0.7 / -0.9 dB against 30-80 Hz (round 2: -5.9 / -6.1); 80-120 Hz is -8.3 dB of the hit (-14.3) and the 125 Hz octave -8.0 (-14.5). The sub is untouched and reads 20 dB down by 315 ms, 2.85 16ths (340 ms, 3.08). Crest 6.34 / 6.55 dB (6.43 / 5.86; Split's range, the floor 4.0). The mud gate: 150-400 Hz falls 30 dB in 53-54 ms (limit 120), sits 71-72 dB under the sub after 80 ms (-20), its share -15.3 dB (-17.9; limit -12), 400 Hz-1 kHz falls in 44 ms (150); in the four-on-the-floor loop -42.7 dB in the gaps (-41.7). The click against the body reads -22.3 / -24.1 dB (-21.8 / -22.8): the top is hotter, the body bigger.
- **Snare.** Four hands in 24-28 ms, the first within 1.8 / 0.0 dB of the loudest (round 2: 4.5 / 1.6 dB under); 150-400 Hz -8.2 / -8.6 dB of the hit (-9.1 / -9.4; the standard's clap body -15 to -8); the tail 20 dB down at 190 ms (187), still chopped by the gate; PLR 15.3 dB (16.6 / 16.0).
- **Hats.** The closed hat's centroid 9.7 kHz (8.7), its side -15.8 dB (-18.4); the open hat 9.5 kHz (8.3) and -8.4 dB (-17.3), wide. The tick is spread over about 2 ms instead of a 1 ms needle.
- **Ride (new; rebuilt in the fix pass).** A ping and a noise wash: 10.5-12% of its sustain in 12 lines, 20 dB down by 0.38 s, the centroid 9.2 kHz, its side 4.3 dB under its mid (the kit's widest top); velocity steps 2.8-3.0 dB, the round robins within 0.11 dB. In the loop it sits 3 LU under the hats lane (above).
- **Acid.** The 2-bar lines: PLR 17.2 to 14.0 dB (A, line 1), 19.8 to 15.1 (B, line 1), 20.9 to 17.7 (A, line 2), 23.4 to 17.5 (B, line 2); A line 1's 2 / 4 / 8 kHz against 1 kHz +2.6 / -4.5 / -11.9 dB (-3.0 / -9.3 / -17.4), B's +1.6 / -5.4 / -12.8 (-1.4 / -7.5 / -15.7). The ride: over 600 Hz its centroid rises by up to a third against the line as written (bar 4's first half, 2.7 kHz against 2.1 in bar 2). The files are the design pass's; the fix pass's sidechain is in the loop's mix.
- **Rumble.** Its 63 Hz octave 2.2 dB fuller against its own 1 kHz octave (the design pass); the fix pass's swell comes in as the kick's tail fades, full 270-300 ms after each kick and 40 dB down by 380 ms.
- **Drone.** 125 and 250 Hz against its own 1 kHz octave: -3.8 and +0.3 dB (-8.7 and -3.2); the 250 Hz octave's side -28.6 dB (-5.5).
- **Unchanged, the same bytes as round 2:** the anvil, the blips (phrase and notes), the stutter fill, the riser, the three sung lines and their seven chops, and both robot files: 37 files, compared by SHA-256.

The fix pass changed 21 files against the design pass's render (the ride's 10, the rumble, the six context loops, the hat loop and the three previews); the other 107 are the same bytes.

## The bass A/B

Unchanged in principle: A clean (the strict mud gate, the voice from 500 Hz) against B warm (the proposed bass-body rule, the voice from 120 Hz over its mono sine sub), the same lines, level-matched in the loop (both lanes -18.5 LUFS before their dips under the vocals and the sidechain; -20.4 and -20.3 after them, the design pass -20.0 and -19.8 after the dips alone). Round 3 gives both the same edge, on the one squelch they share, and the fix pass the same sidechain under the kick. Over the kit's kick, in the loop:
- A reads -37.1 dB against the gate's -20 (the design pass -35.8, round 2 -35.1).
- B reads -6.2 dB against the bass-body limit's -2 (the design pass -6.2, round 2 -5.6); it holds -4.9 or lower with the bass up to 5 ms early or late, the kick 1 dB up or down, or the bass through a 60 Hz all-pass (a kit test, which asks for 2 dB of room: -4).

## What to listen for

- **The club master first.** Is it loud and heavy, does the kick still punch through it, and does it sound crushed? It takes the whole 6 dB cap off the peaks, over the standard's 4 dB convention (above).
- **The low end.** Kick, rumble and bass should land as one heavy E1 under each beat with the kick on top, the rumble rolling in after each kick and gone before the next. Is it tight and heavy, or too light now that the sub reads 3.1 dB lower? Listen once in mono at a moderate level for anything boomy or boxy between 150 and 400 Hz.
- **The kick on small speakers.** Does the low knock give it weight on a laptop or phone, without sounding boomy?
- **The ride and the top.** Do the off-beats lift the groove while the closed 16ths stay apart, and does the ride still read as a ride (a ping and a wash), not a long open hat?
- **The acid.** Does the edge sound like acid through a pedal, does the ride build into bar 4, and is B still the warm one with its sub ducking under the kick?
- **The vocals.** Do the words still read 2.5 LU under the kick? Whisper doesn't hear them in the mix (above). The robot should read "Fill the furnace" and nothing else: in the dry loop Whisper misheard its bar as other words, and in one of nine part-muted mixes as a profane phrase (on its own it reads the robot word for word; see "Not verified").
- **The space.** Is the 5 s dark hall a warehouse, or does it wash the groove?

## QA

- **The render:** 43 items, 128 files, **0 failed checks** (`qa_report.md`).
- **Reproducible:** every item was rendered a second time in a fresh process (`--repro`), with the same SHA-256 (43 of 43).
- **The club loop:** `club_loudness` -8.04 LUFS-I (window -9 to -7); `club_plr` 7.02 dB (at least 6.5); `club_true_peak` -1.02 dBTP round the loop; `club_low_width` -14.1 dB (at most -10); `club_bands` passes with 125 and 250 Hz exempt, the closest other band 1.06 dB inside its window. It also passes every per-file check: DC -111.0 dBFS, stereo correlation 0.90, side under 120 Hz -45.4 dB, the loop seam.
- **The low end over the kit's kick:** ContextLoopLow -37.1 dB (the gate, limit -20), ContextLoopLowWarm -6.2 dB (bass-body, limit -2); the rumble -52.1 dB in its own gaps (the design pass -56) and -45.3 under its kick (`Rumble Under Kick`, -46.9); the 4-bar rides -30.3 (A, the gate) and -4.4 (B, bass-body) dB.
- **Kick crest:** 6.34 and 6.55 dB on the top layer (floor 4).
- **MIDI:** the patterns didn't change; the finishing pass gave the extended mix techno's form (below) and regenerated `content/midi/fut04/` (18 files, `extended/` included), which a fresh export matches.

## The finishing pass: shared QA and the MIDI form

Nothing you hear changed in this pass: every master is byte-identical to the fix pass's, re-rendered from scratch in a fresh folder (`--repro`, every item twice). What changed is what QA reads, and the MIDI's extended form.

- **The master's cap.** The master's 6 dB cap, the highest of the four kits, is now declared on the club item (`club_max_reduction_db`) and listed under Exceptions; QA checks it against the cap the master ran with. At 4 dB the loop stops at -9.55 LUFS-I, at 5 dB at -8.78: the decision is yours.
- **Each kick's crest as the loop plays it** (ADR 0011's measure over the 200 ms from each full-velocity kick; the floor is 4.0 dB): `Context Loop Low` 6.82 dB at the lowest (median 6.86), `Context Loop Low Warm` 6.82, the club loop 6.03 (median 6.57; in the master's input 8.89).
- **What the master costs the hits** (the median loss of that crest from the master's input to the club loop): the kicks 2.33 dB, the snares 2.25 dB. In their own band (1-5 kHz) the snares change by -0.29 dB: the master flattens the kick and the low end, not the snare's own snap. The A&R review proposed at most 1.5 dB, not yet calibrated by ear, so the club loop declares 2.4 dB, with its reason (an exception in `qa_report.md`).
- **Where the clipper works.** The kicks' first 30 ms go up to +6.3 dB over the clipper's knee; the deepest moment away from every kick and snare goes +7.0 dB over it, 4.75 s into the loop.
- **Octave windows.** The 125 and 250 Hz windows are exempted by name now (they read -1.1 and -1.6 dB re 1 kHz); every other band is gated again and passes, the closest 1.06 dB inside its window.
- **Latency.** The stand-in master's limiter starts its gain down 3145 samples (65.5 ms) before a peak: its 5 ms lookahead and its 60 ms hold, which reaches forward too. The engine's master (S1e) reports that latency or redesigns the hold (ADR 0014).
- **MIDI.** The extended mix (`content/midi/fut04/extended/`, regenerated) takes techno's own form now: one break and long grooves (a 64-bar groove, the break, its build, then the drop and drop 2 back to back, 64 bars on the two acid lines), so the drop at bar 129 arrives on the build's riser. Still 224 bars; the loop files' notes are unchanged.

## The verification pass

An independent review checked this page, the kit's files and the shared tools against each other; this pass fixed what it confirmed. All 43 items re-rendered from scratch (`--repro`, every item twice), 0 failed checks; 124 of the 129 FLAC files are the same bytes as before.
- **The acid A/B at one loudness.** In each pair the louder side comes down to the quieter's loudness, and the quieter keeps its own peak and its bytes (`bass.LINE_LUFS`, `bass.RIDE_LUFS`): the ride's A by 0.96 dB to -17.14 LUFS-I, line 1's A by 1.17 dB to -16.09, line 2's B by 0.22 dB to -18.79. Each at its own peak, A had read 0.96 and 1.18 LU louder and line 2's B 0.22. The dry A loop and its low end (`Context Loop`, `Context Loop Low`) play the ride's A file, so their bytes moved too, by 1-2 LSB of 24 bits (the difference peaks at -138.5 and -132.5 dBFS); no reading on this page moved. The held E1 notes are multisample instruments, one gain each, so they keep their peaks: on this note B reads 0.3 LU over A. The listening page says so beside each clip.
- **MIDI.** The build's last 8 bars roll the snare on every quarter for 4 bars, every 8th for 2 and every 16th for 1, rising to full velocity under the riser, then the written fill bar with the kit's fill in bar 128; the kick sits the roll out, so the drop lands on its return. `content/midi/fut04/` is regenerated; a fresh export matches it.
- **QA's crest per hit** now reads each hit's true peak with the loop round its span, so a span that ends in a loud passage can't over-read. In the club loop the snare at 3.09 s reads 6.59 dB (7.15 before) and the one at 1.32 s 6.68 (6.70); the medians, so what the master costs the hits (the kicks 2.33 dB, the snares 2.25), and every gate are unchanged.
- **This page:** the short-term loudness target is the standard's row for a 4-bar loop, and the band override reads as shipped: 125 and 250 Hz exempt, every other octave gated.

## Not verified

- **How any of it sounds:** nobody has listened to round 3, the fix pass included.
- **The vocals in the mix.** Whisper reads the chops on their own through the vocal bus, and the vocals with every other lane muted; in the whole loop it hears no words, with or without the ride. Whisper is a poor judge of words in a dense mix; your ear decides. It also invents words there: in the dry loop with the ride, the anvil and the blips muted, and in one of this pass's ride trials, it heard the robot's bar as a profane phrase. The robot's line is the clean "Fill the furnace" (every FUT04 vocal is tagged clean), so check by ear that the bar can't be misheard that way in the loop.
- **The low end's weight.** The kick now leads by small margins (0.1-1.0 dB in 35-60 Hz, 0.9-2.3 in 30-120) on a measure of our own (the A&R review's), and the sub octave is 3.1 dB lighter against 1 kHz, the sub hump under its report-only window; whether the groove sounds tighter or thinner is the ear's call. The engine must keep the kick, the rumble and the pedal in phase, and the sidechain on the bass's sub.
- **The judgement calls behind the numbers.** Every choice was set by measurement against the standard's targets and the A&R review's, and most of those targets are conventions, not measured club records (the standard's section 0): the drum bus's grit and clip, the pump depths, the hall, the EQ, the 6 dB master cap, the edge's drive, the ride's decay and level, the rumble's swell and level, the sidechain's depth and hold.
- **The engine.** The bass's E1 and the rumble are turned in phase with the top kick layer's tail at 136 BPM; the engine will have to keep that relation. The club master's DC clean is offline only (ADR 0014).
