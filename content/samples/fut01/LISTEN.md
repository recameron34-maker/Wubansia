# FUT01 "Alloy Signal": round 5, the vox synth

Written for the kit's render folder (`build/fut01-r5`, from `render.py --kit FUT01 --set r5`), which holds the FLAC masters, `qa/`, `sheets/`, `qa_report.md`, `qa_report.json` and `listen.json` this page names; its copy in `content/samples/fut01/` sits beside the WAVs alone.

Your verdict on round 4: the sounds are very good, the vocals weren't, and the accent synths belong in the loops. So round 5 keeps every sound but the vocals **byte for byte** (216 of 216 carried-over files match round 4's SHA-256), drops the text-to-speech vocals entirely (the sung lines, their chops, the hooks, the call, the throws, the chop riff and the robot phrase, and round 4's four context loops that played them), puts a **wordless vox synth** in their place (sound 9) with its **riff** (sound 10), and plays the **synth accents** in the loops. The loop now comes in two versions, **vox synth** and **no vox**. The kit's DNA is unchanged: 134 BPM, F# minor (11A), swing 0.59.

**Nobody has listened to round 5.** Every number below was measured on the files in this folder. None of it says how anything sounds.

## Start here

1. `Loops/Garage Vox Club.wav` and `Loops/Garage No Vox Club.wav` on loop: the A/B, both through the shared club master.
2. `Loops/Vox Synth Riff.wav`, then the chops in `Vox/` (`Vox Chop *`) and the playable vox synth (`Vox/Vox Synth A4`, `C#5`, `F#5`; `v1` soft, `v2` hard).
3. `Loops/Garage Vox Wet.wav` / `Garage No Vox Wet.wav` (unmastered, -16.3 LUFS), and the dry ones, `Garage Vox.wav` (bass A) and `Garage Vox Warm.wav` (bass B).
4. `Loops/Speed Garage Vox.wav`: the 4x4 variant with the riff and two accents (round 4's `Speed Garage Loop` stays beside it, unchanged).

## The club masters against the UK garage targets

Both are their wet loops through the shared master (`tools/sound/master.py`, ADR 0014) at the genre's settings, nothing overridden and nothing exempted, at the standard's 4 dB cap.

| | Round 4 club loop | Vox synth | No vox | Target |
|---|---|---|---|---|
| LUFS-I (round the loop) | -9.75 | **-9.73** | **-9.71** | -9 ± 1 |
| True peak / PLR / PSR | -1.02 / 8.73 / 8.50 | -1.02 / 8.71 / 8.49 | -1.02 / 8.69 / 8.39 | ≤ -1.0 dBTP; PLR ≥ 7.5 |
| Peak reduction (dB) | 4.00 | 4.00 | 4.00 | ≤ 4 |
| Kick crest in the club loop, lowest (dB) | 5.26 | 4.98 | 4.89 | ≥ 4.0 |
| What the master costs the hits: kicks / claps (dB) | 1.20 / 1.12 | 0.80 / 1.10 | 0.90 / 1.24 | ≤ 1.5 |
| Octaves re 1 kHz: sub, 125, 250, 500 Hz | +9.0 / +2.3 / +1.2 / +0.9 | +9.2 / +2.4 / +1.5 / +1.2 | +10.2 / +3.5 / +2.8 / +0.7 | +6..+12, +2..+6, +1..+4, 0..+3 |
| Octaves re 1 kHz: 2, 4, 8, 16 kHz | -2.6 / -2.8 / -4.7 / -8.6 | -2.0 / -4.3 / -5.0 / -9.4 | -0.2 / -2.5 / -4.4 / -9.1 | -3..0, -5..-2, -8..-4, -14..-8 |
| Closest to a window's edge (dB inside) | 0.21 | 0.42 | 0.19 | > 0 |
| Side over mid, 125-250 Hz (dB) | -26.8 | -24.7 | -23.0 | ≤ -10 |

What it does not meet, plainly: both loops sit inside the loudness window but up to 0.73 LU short of -9, like round 4's (0.75 short): the master stops at its cap rather than crush the mix, and the margins are thin: every octave is inside its window, the closest 0.19 dB from an edge.

How the mix got there (measured, one change at a time). The vox synth first put the 500 Hz and 1 kHz octaves over everything (its fundamentals and first formants), so the low mids and the 4 kHz octave read under their windows; its lane is now 6 dB down at 600 Hz, 3 down at 1 kHz and 9 up at 3.8 kHz, soft-clipped 5.5 dB and ducked 6 dB under every kick. Without the old vocals the master first stopped 1.2-1.6 LU short at its cap: the riff stacked on the kicks, the warp stab rang into the bar-3 kick, the bass's accents and, once the master's low cut had shaved the kicks, the claps set the peaks. So, in the round 5 mixes only (every sample file is round 4's): the kick lane soft-clips 2 dB more, the bass 3.5 dB, the clap 1.5 dB; the organ stabs, the pluck and the accents duck under the kicks (the accents under the claps too); the warp stab is gated to its 16ths. The no-vox mix lifts the pluck 2 dB, drops the stabs 1 dB (and the stab on the and of 4 in bar 2, where the warp stab plays) and takes a bus EQ (3 kHz up 3 dB, 500 Hz up 1, 1 kHz down 1.5, 8 kHz down 1.5, 16 kHz down 2.5); the vox mix takes 1.5 dB off 16 kHz and plays the glitch fill 2 dB down under the riff's throw.

## What changed in round 5

- **The vox synth** (sound 9, `Vox/Vox Synth`, `Vox/Vox Chop *`): no speech engine, no models, no words. Additive harmonics (exactly band-limited at 15 kHz) on a saw-like tilt, weighted at every sample by five formants per vowel ("ah", "ee", "oh", moved up 12 % for the bright, pitched-up chop character) and a body on the fundamental; vowel glides in log frequency; a quick scoop into pitch; vibrato (5.5 Hz, ±20 cents) that fades in on held notes; 1.5 cents of slow jitter; breath noise pulsed at the pitch through the same formants, with air on top; a "d"-like onset (an 8 ms burst at 2.5-7 kHz and the first formant opening); a double 9 cents sharp and flat, 7 and 12 ms late, one each side; a gentle soft clip; high-passed at 280 Hz, so nothing of it sits in the low mids. The playable version holds an open "ah" on A4, C#5 and F#5 (soft layer darker and breathier); the five chops are "ah" on C#5, "ee" on E5, "ah" to "ee" on F#5, "oh" on A4 scooping up a semitone, and "ah" to "oh" on B4 falling a tone as it ends. QA tunes each one on its steady stretch: within 2.16 cents of its root.
- **The vox synth riff** (sound 10, `Loops/Vox Synth Riff`): 2 bars of off-beat chops in the warp bass's gaps: C#5, E5, F#5 climbing over F#m9, then D5, C#5, B4, A4 answering over Bm9, on the kit's 0.59 swing; the last chop is thrown into a dotted-8th ping-pong and a short plate that ring into bar 1. Its notes are a MIDI phrase on the vox synth.
- **The loops** (`Loops/Garage Vox*`, `Garage No Vox*`): round 4's loop, every lane's part as written, with the riff (twice) where the vocal hooks were, or nothing there. The accents, as ear candy: the reverse stab swelling into bar 3's downbeat, the noise uplifter rising through bars 3-4 into the loop's end, the organ hit on the and of 4 in bar 4 (on the riff's last chop, over the fill); the no-vox version adds the warp stab on the and of 4 in bar 2 and plays the accents 2 dB up (the uplifter 4). Around its loudest moment each accent sits this far under the drums and bass (400 ms, dB): vox synth: reverse stab -10.6, uplifter -9.6, organ hit -9.1; no vox: -8.6, -5.6, -7.1, warp stab -11.3. The riff's lane plays at -20.3 LUFS, the drums and bass at -13.0.
- **The speed garage loop** (`Loops/Speed Garage Vox`, 2 bars): round 4's 4x4 variant with the riff, the reverse stab into bar 2 and the organ hit on the and of 4 in bar 2.
- **Unchanged** (the same bytes as round 4): both kicks, the clap, the hats, tambourine and shaker, the rim and the alloy hits, the warp and organ basses and their lines, the organ and e-piano stabs, the air pad, the chrome pluck and phrase, the glitch fill, the riser, the nine accents, the organ lead and the four phrases, the low-end loops, the drum grooves and round 4's speed garage loop.

## The 14 sounds

| # | Sound | Status | Listen to |
|---|---|---|---|
| 1 | Kick, A Split / B Garage | unchanged | `Loops/Drum Groove 4 Garage`, `Drum Groove 4 Split`; `Drums/Kick Garage`, `Kick Split` |
| 2 | Clap-snare | unchanged | `Drums/Clap Garage` |
| 3 | Hats and tambourine | unchanged | `Drums/Hat Closed`, `Hat Open`, `Drums/Tambourine`, `Shaker`; `Loops/Drum Groove 4 Swing 57` |
| 4 | Rim ghosts and the alloy hit | unchanged | `Drums/Rim Shot`, `Alloy Hit` |
| 5 | Warp bass and line, A Clean / B Warm | unchanged | `Loops/Warp Line Clean`, `Warm`; `Garage Context Low`, `Low Warm`; `Bass/Warp Clean`, `Warm` |
| 6 | Organ bass (the option) | unchanged | `Loops/Organ Line`, `Bass/Organ Bass` |
| 7 | Organ stab and e-piano stab | unchanged | `Music/Organ Stab`, `E Piano Stab`; `Loops/Air Pad Hold` |
| 8 | Chrome pluck and phrase | unchanged | `Loops/Chrome Pluck Phrase` |
| 9 | Vox synth | **new** | `Vox/Vox Chop *`; `Vox/Vox Synth A4`, `C#5`, `F#5` |
| 10 | Vox synth riff | **new** | `Loops/Vox Synth Riff`; in `Garage Vox Club`, `Speed Garage Vox` |
| 11 | Glitch fill | unchanged | `FX/Glitch Fill Lift` |
| 12 | Riser | unchanged | `FX/Shift Riser` |
| 13 | Synth accents | unchanged; **now in the loops** | `Music/Organ Hit`, `Warp Stab`, `Rave Stab`, `Chord Sweep`, `Vox Synth Stab`; `FX/Reverse Stab`, `Noise Uplifter`; `Bass/Sub Drop`; `Drums/Alloy Bell` |
| 14 | Synth phrases | unchanged | `Loops/Organ Riff`, `Warp Riff`, `Pluck Riff`, `Stab Rhythm`; `Music/Organ Lead` |

## The A/B: vox synth or no vox

The two club loops are the same loop, mastered the same way, at one loudness (-9.73 and -9.71 LUFS-I): the riff and its throw against the instrumental with the accents carrying the top. The wet loops play at -16.3 LUFS each.

## What to listen for

- The vox synth: a modern vocal-synth chop, or a fake singer? Too bright, too breathy, too clicky on the "d"?
- The riff: does it catch, and does it answer the bass rather than crowd it? Does the throw carry into bar 1?
- The accents: ear candy under the drums, or too loud, too quiet, too many? Which would you move?
- The no-vox loop: does it hold up without a lead?
- On small speakers and in mono.

## MIDI

`content/midi/fut01/`, regenerated: round 4's files, and the vox synth riff as a fifth synth phrase on channel 4 (`FUT01_Phrase_VoxSynth_2bars.mid`, on the vox synth rooted at A4, C#5 and F#5, MIDI 69, 73, 78). The riff's file plays each chop's own vowel; a sampler playing the MIDI on the vox synth plays its open "ah". The ducks, the clips and the accents' placements are mix decisions, not notes.

## QA in one line

61 items, 237 files, 0 failed checks (`qa_report.md` lists each exception and its reason), and 61 of 61 items rendered the same SHA-256 a second time, each in a fresh process (`--repro`). 21 files are new; the other 216 are round 4's, byte for byte. Low end (unchanged): `Garage Context Low` reads -26.62 dB against the sub in its gaps (limit -20) and -49.10 dB before each kick (limit -30); `Low Warm` -4.36 dB (bass-body limit -2); `Speed Garage Low` -31.67 dB.

None of this says how anything sounds.
