# FUT03 "Glass Current": round 5, a wordless vox synth and the accents in the loops

Written for the kit's render folder (`build/fut03-r5`, from `render.py --kit FUT03 --set r5`), which holds the FLAC masters, `qa/`, `sheets/`, `qa_report.md`, `qa_report.json` and `listen.json` this page names; its copy in `content/samples/fut03/` sits beside the WAVs alone.

Round 5 answers what you said after round 4: "sounds are very good, vocals suck, either ditch or rethink, need those cool accent synths". So:
- **every round 4 sound that is not a vocal is kept byte for byte**: 40 items, 160 files, each with the same SHA-256 as in `build/fut03-r4`;
- **the text-to-speech vocals are gone**: the vocal hook, the three ear-candy chops, and round 4's four context loops that played them;
- **a wordless vox synth (sound 9) and its riff (sound 10) take their place**: synthesis only, no words, no speech engine, no models;
- **the round 4 accent synths play in the loops**, as ear candy;
- **two club loops to compare**: with the vox synth, and without it.

The kit's DNA is round 4's: 128 BPM, G minor (6A), swing 0.56, garage-house / deep tech house. `--set r4` still renders round 4 exactly; round 5's new behaviour lives in new items with new names.

**Nobody has listened to round 5.** Every number below is a measurement made in this session. None of it says how the kit sounds.

## Start here

Play the A/B first:
- `Loops/Context Loop Vox Club.wav`: the loop with the vox synth riff, through the club master;
- `Loops/Context Loop No Vox Club.wav`: the same loop instrumental, the accents carrying the top.

Then the vox synth's chops and multisample (`Vox/`), the riff (`Loops/Phrase Vox Riff.wav`), the wet and dry loops, and the low end.

| Club master | Vox synth | No vox | Target (garage-house / deep tech house) |
|---|---|---|---|
| Loudness, LUFS-I (round the loop) | -9.23 | -9.18 | -8.5 ± 1 |
| True peak | -1.02 dBTP | -1.02 dBTP | ≤ -1.0 |
| PLR / PSR | 8.21 / 7.84 dB | 8.16 / 8.01 dB | ≥ 7.0 |
| Octaves re 1 kHz: sub, 125, 250, 500, 2k, 4k, 8k, 16k | +10.2, +1.7, +2.3, +1.7, -2.5, -4.8, -5.4, -10.2 | +11.3, +2.8, +3.5, +2.4, -2.8, -4.7, -4.1, -9.0 | every one inside its window; the closest 0.17 dB (vox) and 0.07 dB (no vox) inside |
| Side over mid at 125 / 250 Hz | -41.5 / -12.0 dB | -41.6 / -12.1 dB | ≤ -10 |
| Side under 120 Hz | -54.7 dB | -55.7 dB | ≤ -20 |
| Peak reduction (clipper + limiter) | 4.5 dB (3.3 / 1.2) | 4.5 dB (3.3 / 1.2) | round 4's declared cap, 4.5 |
| Crest per kick in the club loop | lowest 5.38, median 6.15 dB | lowest 5.31, median 5.86 dB | ≥ 4.0 |
| What the master costs the hits (median crest loss) | kicks 0.79, claps 1.07 dB | kicks 1.03, claps 1.25 dB | ≤ 1.5 |

The cap is round 4's, declared on both club loops with round 4's reason (`club_max_reduction_db`). Both loops now have room under the crest-loss limit (round 4's claps sat exactly on it, 1.50).

## The sounds

| # | Sound | Round 5 | Files |
|---|---|---|---|
| 1 | Deep kick on G1 | unchanged | `Drums/Kick Deep` |
| 2 | Clap and snare | unchanged | `Drums/Clap Snare` |
| 3 | Hats and shaker | unchanged | `Drums/Closed Hat Deep`, `Drums/Open Hat Deep`, `Drums/Shaker Deep`, `Loops/Drum Loop` |
| 4 | Congas, bongos and rim | unchanged | `Drums/Conga …`, `Drums/Bongo …`, `Drums/Rimshot`, `Loops/Perc Loop` |
| 5 | Rubber bass and its lines (A/B) | unchanged | `Bass/Rubber Bass …`, `Loops/Line Rubber …`, `Loops/Line Deep …` |
| 6 | Deep sub | unchanged | `Bass/Sub Deep G1`, `D2`, `G2` |
| 7 | Dub stab, Gm9 | unchanged | `Music/Dub Stab Gm9`, `Loops/Dub Chord Loop` |
| 8 | Warm pad bed | unchanged | `Music/Pad Warm` |
| 9 | **Vox synth** | **new**, replaces the vocal hook | `Vox/Vox Synth …`, `Vox/Vox Chop …` |
| 10 | **Vox synth riff** | **new**, replaces the ear-candy chops | `Loops/Phrase Vox Riff` |
| 11 | Build fill | unchanged | `FX/Build Fill`, `Loops/Fill Preview` |
| 12 | Riser and impact | unchanged | `FX/Riser`, `FX/Impact` |
| 13 | Synth accents | unchanged, now in the loops | `Music/Accent …`, `Drums/Accent …`, `Bass/Accent Rubber Stab G1`, `FX/Accent …` |
| 14 | Synth phrases | unchanged | `Loops/Phrase …`, `Music/Keys EP …` |

"Unchanged" means the same bytes as round 4. The round 4 vocals (`Vox Hook`, `Candy …`) and loops (`Context Loop`, `Context Loop Club`, `Warm`, `Wet`) stay in the `r4` set, not in this folder.

## What is new

**9. Vox synth (`VoxSynth`, `VoxChop…`)**
- A formant voice built from synthesis alone (`recipes/fut03/voxsynth.py`):
  - the source is glottal-like, every harmonic of the sung pitch up to 16 kHz (band-limited at any pitch), with a breathy spectral tilt that velocity brightens;
  - five formants glide from "ooh" (330, 800, 2350 Hz) to "aah" (760, 1150, 2550 Hz), set a little under an average adult's for depth;
  - a breath layer, noise pulsed with the glottal cycle, runs through the same formants;
  - a soft consonant onset: a breath puff 25 ms before the note and the lips opening from a "w" over 50 ms, so it speaks "hw-ooh";
  - a 40-cent scoop up into each note, gentle delayed vibrato (5 Hz), slow drift, jitter and a little shimmer;
  - a slight double: two voices 8 cents either side, each with its own vibrato, panned apart;
  - a vocal chain's presence (+8 dB at 3.3 kHz) and air, and a high-pass at 280 Hz, so it sits above the low mids.
- **The multisample** (`Vox Synth G3 / D4 / G4 / D5`, two layers): each note opens from "ooh" to "aah" over half a second, holds with vibrato and closes a little as it fades. It is dry. Every zone plays at one loudness; the soft layer is 6 dB down, breathier and darker. Tuning is within 2.3 cents on every file.
- **Four chops**, each through a low-pass that opens over the chop and the kit's dotted-8th dub echo:
  - `Vox Chop Ooh Ah G4`: a beat long;
  - `Vox Chop Ooh Ah C5`: an 8th;
  - `Vox Chop Ah D5`: an open "aah", a dotted 8th, the brightest;
  - `Vox Chop Stack Gm7`: G4 Bb4 D5 F5 at once.
  The single chops read within 3 cents of their roots.

**10. Vox synth riff (`PhraseVoxRiff`)**
- Two bars of off-beat chops on the kit's swing, every chop on an "and" or a swung 16th, none on a kick:
  - bar 1, over Gm9: D5, Bb4, D5, F5 (the F on the swung "e" of 4);
  - bar 2, over Cm9: Eb5, Bb4, C5, then G4 on the "and" of 4, thrown into the dub echo.
- Each chop's low-pass opens as it plays, and the whole riff rides from closed (1.2-3.5 kHz) to open (3-9 kHz) and back over the two bars.
- A light dotted-8th echo on every chop; the throw on the last gives seven repeats, darkening, that run into bar 1.
- It is also MIDI: `FUT03_Phrase_VoxSynth_2bars.mid`, on channel 4.

**The loops (`recipes/fut03/context5.py`)**
- Every round 4 lane plays exactly as it did: the drums, the perc, the bass, the stab, the pad, their faders, pans, sends, buses and pumps.
- **Vox synth** (`Context Loop Vox …`):
  - the riff answers in bars 3-4, where round 4's hook was, through the vocal bus's compressor;
  - its throw echoes into bar 1 as the loop comes round;
  - the vocal bus EQ is round 4's, with 1.5 dB more cut at 950 Hz and 1 dB more lift at 2.8 kHz (the "aah" sits on 1 kHz);
  - the plate ducks under the riff, as it did under the hook.
- **Accents**, on one lane, high-passed at 300 Hz with +4 dB at 3.5 kHz. The lane reads -23.3 LUFS, 9 dB under the kick's and 2.3 dB under round 4's ear candy:
  - the glass ping on the "and" of 4 in bar 1;
  - the Cm9 dub chord on the "and" of 4 in bar 2, its echoes leading into the riff;
  - vox synth loop: the reversed Gm9 swelling into bar 1;
  - no vox loop: the Gm7 organ hit on the "and" of 4 in bar 3, where the riff would be, and the noise sweep into the loop's end instead of the reversed chord.
- **No vox** (`Context Loop No Vox …`): the same loop with no vox lane at all.
- The low end (kick and bass) is round 4's, so `Context Loop Low` and `Context Loop Low Warm` are round 4's files.

## The A/Bs left for you

1. **Vox synth or no vox.** Both club loops are mastered the same way and sit within 0.05 LU of each other.
2. **Bass A (clean) or B (warm)**, in the dry loops (`Context Loop Vox` A, `Context Loop Vox Warm` B; the same for No Vox).
   - They play at one loudness. This time B reads louder at its own peak, so B comes down to A: -12.38 LUFS-I (vox synth), -12.75 (no vox).
   - The wet loops and the club masters play B, round 4's pick.
3. **The master's cap:** 4.5 dB (round 4's), or the standard's 4.

## What to listen for

- **The vox synth.** Does it sound like a cool vocal-synth chop or a fake singer? Is it deep and breathy enough, and does the "hw" onset help or sound like a click?
- **The riff.** Is it hypnotic? Does the throw at the end of bar 4 lead back into the loop, or smear the downbeat?
- **The accents.** Do they sit like a record's ear candy, or poke out over the drums? Would you want more of them, or other ones?
- **Vox synth or no vox.** Which loop would you rather build on?
- **Mud.** The vox synth is high-passed at 280 Hz; listen once in mono at a moderate level.

## QA

- **The render:** 54 items and 181 files, **0 failed checks** (`qa_report.md`).
- **Reproducible:** every item rendered a second time in a fresh process (`--repro`) gave the same SHA-256, all 54.
- **The same bytes as round 4:** all 160 files of the 40 continuing items.
- **Exceptions**, each declared with its reason in `qa_report.md`:
  - round 4's, unchanged;
  - the club master's 4.5 dB cap on both new club loops;
  - the vox synth multisample's alias check is off: its breath and its double sit between the harmonics by design. Its tuning check stays on (±3 cents).
- **No models:** no item in the set is text-to-speech, sings a script or carries the TTS flag, and the vox synth renders with every model loader blocked (`test_fut03_round5`).
- **Tests:** `tools/sound/tests/test_fut03_round5.py`, 16 tests:
  - the set: round 4 without the TTS vocals, plus the new items; round 4's set unchanged;
  - no TTS item, and nothing that needs the models;
  - the vox synth is wordless;
  - the multisample, the chops and the riff are in G minor on their stated roots, the riff on chord tones off the beat, and the MIDI phrase carries the riff's notes;
  - the instrumental loop has no vox lane, and both club loops have 2-4 accents;
  - the club master's cap, the dry A/B loudness, round 4's lanes kept, no artist names.

## MIDI

`content/midi/fut03/` is re-exported. The one new file is `FUT03_Phrase_VoxSynth_2bars.mid`, the riff on channel 4 (play it on the Vox Synth multisample, zones G3 D4 G4 D5); the README lists it. The extended mix's README points its Vox cues at the vox synth now; every other file is unchanged. Nothing has been opened in a DAW.

## Not verified

- **How any of it sounds.** Nobody has listened to round 5: not the vox synth, the riff, the accents in the loops, or the A/B.
- **The judgement calls behind the numbers:** the formant values, the vibrato and breath levels, the accent levels and places, the vocal bus EQ, and the genre's octave windows (conventions, not measured club records).
- **The no vox club loop's octave balance** is 0.07 dB inside its window at its closest. It passes, with little to spare.
