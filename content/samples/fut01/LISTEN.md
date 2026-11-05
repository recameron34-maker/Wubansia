# FUT01 "Alloy Signal": round 4, the UK garage rebuild

Written for the kit's render folder (`build/fut01-r4`, from `render.py --kit FUT01 --set r4`), which holds the FLAC masters, `qa/`, `sheets/`, `qa_report.md`, `qa_report.json` and `listen.json` this page names; its copy in `content/samples/fut01/` sits beside the WAVs alone.

A UK garage / speed garage kit at 134 BPM in F# minor (11A), rebuilt in round 4 as current UK garage in the spirit of the scene's leading producers (no names, no borrowed melody, lyric or sound): classic drum-machine and sampler punch, a speed garage warp bass as the hook, house organ stabs, pitched-up R&B-style vocal hooks cut from the kit's own line, and the kit's alloy colour kept only as a hint in the perc. The swing is up from 0.57 to **0.59** (the swung 16ths 20.1 ms late instead of 15.7). Round 3's chrome pluck (your Cool 5), the pad, the robot, the fill and the riser stay.

**Nobody has listened to round 4.** Every number below was measured on the files in this folder. None of it says how anything sounds.

## Start here

1. `Loops/Garage Context Club.wav` on loop: the wet loop through the shared club master, the loud reference.
2. `Loops/Garage Context Wet.wav`, `Garage Context.wav` (bass A) and `Garage Context Warm.wav` (bass B): the same mix unmastered, at one loudness (-16.3 LUFS).
3. `Loops/Speed Garage Loop.wav`: the 4x4 speed garage variant.
4. The A/Bs: `Loops/Drum Groove 4 Garage` against `Drum Groove 4 Split` (kick B against kick A) and against `Drum Groove 4 Swing 57` (the new swing against round 3's); `Loops/Warp Line Clean` against `Warp Line Warm`.
5. The new sounds in `Drums/`, `Bass/`, `Music/`, `Vox/` and `FX/`, then the accents (sound 13) and the phrases (sound 14).

The WAVs carry the same samples as the `FUT01_*.flac` masters; `v1` is the softest layer.

## The club master against the UK garage targets

`GarageContextClub` is `GarageContextWet` through the shared master (`tools/sound/master.py`, ADR 0014, proposed) at the genre's settings, nothing overridden and nothing exempted: every octave window is gated, at the standard's 4 dB cap.

| | Round 3 club loop | Round 4 club loop | Target |
|---|---|---|---|
| LUFS-I (round the loop) | -9.70 | **-9.75** | -9 ± 1 |
| True peak / PLR / PSR | -1.02 dBTP / 8.68 / 8.41 dB | -1.02 dBTP / 8.73 / 8.50 dB | ≤ -1.0; PLR 7.5-9 |
| Peak reduction | 4.0 dB | 4.00 dB | ≤ 4 |
| Kick crest in the club loop, lowest | 5.63 dB | 5.26 dB | ≥ 4.0 (5-7 the band round 1 liked) |
| What the master costs the hits | kicks 1.52 / snares 1.54 dB (declared 1.6) | kicks 1.20, claps 1.12 | ≤ 1.5 |
| Octaves re 1 kHz: sub (63), 125, 250, 500 Hz | +7.3 / -1.0 / -2.4 / +0.3 (125, 250 exempted) | +9.0 / +2.3 / +1.2 / +0.9 | +6..+12, +2..+6, +1..+4, 0..+3 |
| Octaves re 1 kHz: 2, 4, 8, 16 kHz | -2.8 / -4.9 / -5.3 / -9.8 | -2.6 / -2.8 / -4.7 / -8.6 | -3..0, -5..-2, -8..-4, -14..-8 |
| Low-mid hole, 125 / 250 Hz | -5.95 / -4.96 | -4.01 / -2.43 | ≥ -4 |
| Top slope, 500 Hz-8 kHz | -1.38 dB/oct | -1.40 | -2.0 ± 0.5 (not checked) |
| Side over mid, 250 Hz / 1 / 2 / 4 / 8 / 16 kHz | -16.3 / -5.9 / -8.1 / -8.9 / -7.6 / -4.2 | -26.8 / -9.8 / -9.0 / -9.5 / -7.2 / -6.7 | ≤ -10; -12..-6; -10..-4; -10..-4; -8..-3; -8..-3 |

What it does not meet, plainly:
- **Loudness: inside the window, 0.75 LU short of -9**, at the standard's cap (the master stops at its cap rather than crush the mix; its report says how far short).
- **The top slope** is -1.40 dB per octave, brighter than the genre's -2.0 ± 0.5 (a shape check QA doesn't run), though every top octave sits in its window.
- **Thin margins**: 125 Hz (+2.34 in +2..+6), 250 Hz (+1.21 in +1..+4), 2kHz (-2.56 in -3..+0), each within 0.6 dB of an edge of its window.
- Width: every octave from 250 Hz up inside its window (the standard's conventions; QA gates 125-250 Hz only).

How the mix got there (measured, one change at a time): the first mix put the 1 kHz octave over everything (the warp bass's top and the vocals), so every other octave read low, and the bass's pitch dips (one dip back up landed as an accent) set the master's peaks away from the kicks, which held it 1.4 LU short. The lanes now take EQ (the bass 4.5 dB down at 1 kHz and 1 dB up at 200 Hz, the vocals 2.5 dB down at 500 Hz and 2 up at 3.2 kHz, the kick 2 dB up at 125 Hz, the stabs and the clap lifted at 220-250 Hz, the clap's, the hats' and the tambourine's air cut) and gentle soft clips (vocals 3 dB, stabs 1.5, the alloy hit 4), the dips keep their note's velocity, and the bass sits 0.3 LU under the kick lane: the kicks now set the master's peaks, as the standard asks. The second pass found the top too narrow (side 13-14 dB under mid at 4-16 kHz, against the standard's -10..-4 and -8..-3): the clap's and the hats' own side is now scaled up over 1.5-3 kHz (their mid, so the mono sum, untouched), with a little more of their air taken down so the top octaves stay in their windows. The third pass found bass B over the bass-body rule in the loop (-1.7 dB against the sub after the bar-2 kick, limit -2, on the F#2 notes repitched up from the C#2 zone): its body band is now 0.38 of the stack (0.45 before), and the stabs and the clap carry more of the low mids instead.

## What changed in round 4, and why

- **Kick B, Garage** (`Drums/Kick Garage`): round 1's split design kept (a clean F#1 sine sub, a short knock, grit only on top) with a snappier pitch drop, the sub 20 dB down by about 205 ms (two 16ths are 224; kick A takes 317), a knock on the sweep's own second harmonic (100-170 Hz, its first 45 ms) and round 1's driven knock, and a drum-machine click (a 0.3 ms pulse, a noise burst and a short "tok", driven at 4x and only above 1 kHz) in place of the alloy top. On the top layer the click sets the peak: crest 6.17 / 6.57 dB (kick A 6.44 / 5.82). Kick A, Split, stays for the A/B.
- **Clap-snare** (`Drums/Clap Garage`): four hands of band noise inside 24 ms (the first within 1.2 dB of the loudest), a snare body on F#3 and C#4 dropping into pitch, snappy wires, a 0.6 s bright room; softly clipped. Round 3's three metal snares leave the kit.
- **Hats and tambourine** (`Drums/Hat Closed`, `Hat Open`, `Perc/Tambourine`): classic metal hats from six square waves on inharmonic pitches and noise; the open hat rings an 8th or so and the loops choke it on the next closed hat; a tambourine of struck jingles for the 16ths, loudest on each beat's swung last 16th. Round 3's FM and ring hats leave; the shaker stays as the alternative.
- **Rim ghosts** (`Perc/Rim Shot`): a short rim click with one mode on the alloy ratio 1.483, the hint of the kit's colour; the alloy hit plays once in bar 3.
- **Warp bass, A clean / B warm** (`Bass/Warp Clean`, `Warp Warm`): a detuned saw pair over a square, each driven at 4x, a resonant low-pass that opens on every note, over a mono sine sub; B adds the stack's own 120-450 Hz body. The line bounces between the 2-step kicks over F#m9 and Bm9 with octave pops, two slides and two pitch dips (30 ms glides down and back, as the engine's pitch bend will play them); the lane's filter wobbles once an 8th, deepest in bars 2 and 4. A baked-in wobble read as inharmonic energy in the files (-35 dB against the alias check's -60), so the wobble is the lane's, as the engine plays it. After a dip, bass A is walled at 450 Hz again so the lane keeps the strict gate. The reese, its three basslines and the growls leave the kit.
- **Organ bass** (`Bass/Organ Bass`, the option): additive drawbars (16', 8', 5 1/3', 4'), a percussion partial and a key click, its drawbars rising 6 ms behind its fundamental so no note's start puts more low mids than sub in a 20 ms window; its line (`Loops/Organ Line`) stays in the low octave.
- **Stabs** (`Music/Organ Stab`, `E Piano Stab`): minor 9 chords in two zones, F#m9 on F#3 and Bm9 on B3 (root, 5, m7, 9, m3: the 9th rubs against the 3rd). The organ is gated short; the FM e-piano holds 0.3 s with a tremolo.
- **Vocal hooks** (`Vox/`): all from the title-checked line "Come find me on the floor", re-pitched in WORLD (never resampled): formants kept a fourth up (`Chop Me D5`, `Chop Floor E5`, `Chop Find D5`), chipmunk a fifth or an octave up (`Chop Me A5 Chip`, `Chop Come B4 Chip`, `Chop Floor F#5 Chip`), the call (`Call Line A4`, the whole line a fourth up), two delay throws (`Throw Floor E5`, `Throw Me A5 Chip`) and the off-beat riff (`Loops/Chop Riff`). No new words.
- **The swing**: 0.57 to 0.59, in the kit's DNA, so every lane moves together (the vocals were re-sung on it; Whisper still hears line 1 word for word). The pluck phrase and the fill move with it (their swung 16ths 4.4 ms later), nothing else in them.
- **The context loop** (`Loops/Garage Context*`): 4 bars, the 2-step kick (1 and the and of 3, a skip kick in bars 2 and 4), the clap on 2 and 4 with two ghosts, the hats and tambourine, rim ghosts, the warp line, organ stabs on the off-beats, the chrome pluck phrase on top, the riff in bars 1-2, the call in bars 3-4 and "floor" thrown into the glitch fill. The garage plate (1.1 s, a 1/64-note pre-delay) and the dotted-8th echo as in round 3, a little resonator on the alloy hit.
- **The speed garage variant** (`Loops/Speed Garage Loop`, 2 bars): the kick on every beat, the warp line on the off-beats between the kicks, the same tops and stabs.

## The 14 sounds

| # | Sound | Status | Listen to |
|---|---|---|---|
| 1 | Kick, A Split / B Garage | **B new**; A unchanged | `Loops/Drum Groove 4 Garage`, `Drum Groove 4 Split`; `Drums/Kick Garage v5` … `v1` |
| 2 | Clap-snare | **new** | `Drums/Clap Garage v5` … `v1` |
| 3 | Hats and tambourine | **new** (the shaker unchanged) | `Drums/Hat Closed`, `Hat Open`, `Perc/Tambourine`; `Loops/Drum Groove 4 Swing 57` |
| 4 | Rim ghosts and the alloy hit | **rim new**; alloy hit unchanged | `Perc/Rim Shot`, `Perc/Alloy Hit` |
| 5 | Warp bass and line, A Clean / B Warm | **new** | `Loops/Warp Line Clean`, `Warm`; `Garage Context Low`, `Low Warm`; `Bass/Warp Clean`, `Warm` |
| 6 | Organ bass (the option) | **new** | `Loops/Organ Line`, `Bass/Organ Bass` |
| 7 | Organ stab and e-piano stab | **new** (the air pad unchanged) | `Music/Organ Stab`, `E Piano Stab`; `Loops/Air Pad Hold` |
| 8 | Chrome pluck and phrase | the swing only | `Loops/Chrome Pluck Phrase` |
| 9 | Vocal hooks | **new hooks**; the lines and chops re-sung on the new swing | `Loops/Chop Riff`, `Vox/Call Line A4`, `Vox/Chop * Chip`, `Vox/Throw *` |
| 10 | Robot phrase | unchanged | `Vox/Robot Boost`, `Robot Boost Wet` |
| 11 | Glitch fill | the swing only | `FX/Glitch Fill Lift` |
| 12 | Riser | unchanged | `FX/Shift Riser` |
| 13 | Synth accents | **new** | `Music/Organ Hit`, `Warp Stab`, `Rave Stab`, `Chord Sweep`, `Vox Synth Stab`; `FX/Reverse Stab`, `Noise Uplifter`; `Bass/Sub Drop`; `Perc/Alloy Bell` |
| 14 | Synth phrases | **new** | `Loops/Organ Riff`, `Warp Riff`, `Pluck Riff`, `Stab Rhythm`; `Music/Organ Lead` |

**13, the accents**, each in F# minor on a stated root: an organ hit (F#m9 over two octaves, F#3, with a 6 Hz rotary shimmer), a warp stab (F#3, the warp voice scooping up a whole tone into pitch), an original rave stab (F#3 F#4 A4 C#5 on detuned saws, dropping a semitone into pitch), a chord hit with a filter sweep (F#m9, 9 kHz down to 400 Hz over a second), a reverse stab (the organ hit and its plate reversed, ending on the beat), a sub drop (F#2, two octaves down over 1.4 s), an alloy bell (C#6, the kit's alloy hit with modes three times longer), a vocal-synth stab (F#m7 through formants moving from "oo" to "ah": a synth vowel, no voice) and a noise uplifter (2 bars, a comb tuned to F#3, ending on the downbeat).

**14, the phrases**, each a loop on the kit's swing and a MIDI file (`content/midi/fut01/FUT01_Phrase_*`): a 4-bar organ riff on a new drawbar organ lead (C4 to C6, every 3 semitones), a 2-bar warp bass riff on bass A (16ths, pops, slides, three dips, the wobble), a 4-bar chrome pluck riff (a new phrase on the pluck you rated 5) and a 2-bar e-piano stab rhythm (F#m9 then Bm9).

## The A/Bs

- **Kick: A, Split or B, Garage.** The two drum grooves play bars 1-2 of the loop's drums with each kick through the loop's 1.5 dB kick clip, both at -16.5 LUFS (-16.50 and -16.50 as rendered), so the difference is the kick alone.
- **Swing: 0.59 or 0.57.** `Drum Groove 4 Swing 57` is the same 2 bars on round 3's swing (every odd 16th 4.4 ms earlier), at the same loudness. If 0.57 grooves better, the kit's swing goes back down (one DNA value; every lane re-renders).
- **Bass: A, clean or B, warm**, at one loudness in every loop; the wet and club loops play B. The organ bass is a third option on its own line.
- **Stabs: organ or e-piano**, and the four phrases against each other.

## What to listen for

- The club loop next to a current UK garage record you know, at matched loudness: is it as punchy and as full? Does the kick lead, the sub sit deep and tight, and is there any mud?
- Does the warp line bounce and make you want to dance? Do the dips and the 8th-note wobble read as the speed garage "warp", or as wobble for its own sake?
- Kick B: tight and punchy with a click that reads on small speakers, or too clicky?
- The clap: crisp and snappy, or too roomy? The open hats choked by the closed ones: a natural hat group?
- The hooks: a current garage vocal or a cartoon? Kept or chipmunk? Does the riff catch, does the call land, and does the throw carry into the fill?
- The organ stabs under the pluck and the vocals: room for all three, or crowded?
- The 4x4 variant: speed garage, or a house loop?
- The accents and phrases: which would you drop into a track?
- On small speakers and in mono.

## MIDI

`content/midi/fut01/`, regenerated from the kit's parts (`tools/sound/recipes/fut01/midi.py`): the round 4 drums (kick B on 36, the rim on 37, the clap on 38, the closed and open hats on 42 and 46, the alloy hit on 47, the tambourine on 54, all inside 35-59 under the sung chops), three basslines (`Bounce`, the one in the loop; `Low`, the organ bass's; `Speed`, the 4x4 line; each dip written as two slides, down and back), the pluck phrase, the organ stabs (voicing and sample trigger), and the four synth phrases on channel 4. The extended mix (`extended/`, 224 bars) is regenerated from them; its switch-up and drop 2 play the `Low` line. One limit of the shared export: the chord sample trigger plays every stab on one note, so on the Organ Stab sample it plays F#m9 throughout; for the Bm9 stabs, play the B3 zone (or the voicing file on a synth). The wobble, the duckers and the lane EQ are mix automation, not notes.

## QA in one line

72 items, 243 files, 0 failed checks (`qa_report.md` lists each exception and its reason: the full mixes' low-end rule, the vocals' synthetic top band, the end-aligned FX, bass B's, the organ bass's and the warm lines' bass-body rule, the sub drop's tuning window and steady-tone exemption, the organ lead's and the rave stab's alias windows), and 72 of 72 items rendered the same SHA-256 a second time, each in a fresh process (`--repro`). Low end: `Garage Context Low` reads -26.62 dB against the sub in its gaps (limit -20) and -49.10 dB before each kick (limit -30); `Low Warm` -4.36 dB (bass-body limit -2); `Speed Garage Low` -31.67 dB. Every full-velocity kick in the low loops keeps a crest of at least 5.46 dB (floor 4.0).

None of this says how anything sounds.
