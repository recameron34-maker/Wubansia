# FUT03 "Glass Current": round 4, garage-house and rolling deep tech house

Written for the kit's render folder (`build/fut03-r4`, from `render.py --kit FUT03 --set r4`), which holds the FLAC masters, `qa/`, `sheets/`, `qa_report.md`, `qa_report.json` and `listen.json` this page names; its copy in `content/samples/fut03/` sits beside the WAVs alone.

Round 4 redesigns the kit as warm, organic garage-house and rolling deep tech house, still at 128 BPM in G minor (6A). It has:
- a deep round kick;
- a clap and snare layered in a short room;
- shuffled hats, an open hat on every offbeat and a shaker on the 16ths;
- congas, bongos and rims with swung ghosts;
- a rubbery rolling bassline as the hook, over a deep sub;
- a dubby Gm9 electric-piano stab with its dub echo, over a warm pad bed;
- a filtered, chopped vocal hook cut from the kit's own line.

Milestone 2 adds nine synth accents and four synth phrases. The hyper-clean glass of rounds 1-3 survives only as a faint colour: the bell's partials 14 dB down in the hats, one shaker grain in five, a ring on the rim, and the glass ping accent. The swing rose from 0.53 to 0.56, a garage-house shuffle. `DIRECTION` is now `garage-house / deep tech house`, with its own club windows (`qa.CLUB`: -8.5 LUFS-I, PLR at least 7 dB, warmer octave windows than future house).

**Nobody has listened to round 4.** Every number below is a measurement made in this session. None of it says how the kit sounds.

## Start here

Start with `Loops/Context Loop Club.wav`, the wet loop through the shared club master. Then play:
- the dry loops, A and B at one loudness;
- the wet loop;
- the low end alone, A and B;
- the sounds, 1 to 14.

| Club master | Reading | Target (garage-house / deep tech house) |
|---|---|---|
| Loudness, LUFS-I (round the loop) | -8.96 | -8.5 ± 1 |
| True peak | -1.02 dBTP | ≤ -1.0 |
| PLR / PSR | 7.94 / 7.50 dB | ≥ 7.0 / ≥ 6 |
| Short-term max (3 s) | -8.52 LUFS | its LUFS-I plus a few tenths (reported, not gated) |
| Octaves re 1 kHz: sub, 125, 250, 500, 2k, 4k, 8k, 16k | +10.8 (in the 63 Hz octave), +2.3, +3.0, +2.8, -2.1, -3.6, -4.6, -9.4 dB | every one inside its window; the closest is 500 Hz, 0.17 dB inside |
| Side over mid at 125 / 250 Hz | -41.5 / -12.1 dB | ≤ -10 |
| Width at 2 / 4 / 8 / 16 kHz | -12.9 / -9.9 / -9.3 / -8.9 dB | the standard's convention: -10 to -4 (2-4k), -8 to -3 (8-16k); reported, not gated |
| Side under 120 Hz | -55 dB | ≤ -20 |
| Peak reduction (clipper + limiter) | 4.5 dB (the clipper 3.3, the limiter 1.3 at most) | the standard's 4; this kit declares 4.5 (below) |
| Crest per kick in the club loop | lowest 5.42 dB, median 5.88 (the master's input 5.77 lowest) | ≥ 4.0 |
| What the master costs the hits (median crest loss) | kicks 1.18 dB, claps 1.50 dB (in their own 1-5 kHz band: -0.16) | ≤ 1.5 |

**The one deviation from the standard is the master's cap: 4.5 dB, where the standard's convention is 4.**
- **At 4 dB the loop stops at -9.4 to -9.6 LUFS-I.** That is on the window's -9.5 edge. Twenty balances were measured through the master, and none moved it more than a tenth or two. They tried:
  - the stab moved off the bass's accents;
  - the pad 6 dB up;
  - 4 or 5 dB of glue;
  - a peak limiter on the vocal hook, or on the bass.
- **Why the master needs more than 4.** The loop's PLR (11.9 dB into the master) comes from its arrangement: a kick on every beat and short, plucked parts. The loudest moments are not the kicks. They are the bass's G1 accent on the "and" of beat 1 stacked with the open hat, a conga, the shaker and, in bars 3-4, the hook's "ride".
- **At 4.5 dB the master reaches -8.96, with 0.46 LU inside the window.** The kicks keep a median crest of 5.88 dB. The claps lose 1.50 dB of crest, the standard's limit, so that check passes with nothing to spare.
- **It is declared** on `ContextLoopClub` (`club_max_reduction_db`, with its reason) and listed under Exceptions in `qa_report.md`. The engine's master must match it.
- **Your decision by ear.** If the kicks or claps sound flattened, the cap comes back to 4 and the loop sits at about -9.5.

## The sounds

| # | Sound | Round 4 | Files |
|---|---|---|---|
| 1 | Deep kick on G1 | new | `Drums/Kick Deep` |
| 2 | Clap and snare | new | `Drums/Clap Snare` |
| 3 | Hats and shaker | new | `Drums/Closed Hat Deep`, `Drums/Open Hat Deep`, `Drums/Shaker Deep`, `Loops/Drum Loop` |
| 4 | Congas, bongos and rim | new | `Drums/Conga …`, `Drums/Bongo …`, `Drums/Rimshot`, `Loops/Perc Loop` |
| 5 | Rubber bass and its lines (A/B) | new | `Bass/Rubber Bass Clean …`, `Bass/Rubber Bass Warm …`, `Loops/Line Rubber …`, `Loops/Line Deep …` |
| 6 | Deep sub | new | `Bass/Sub Deep G1`, `D2`, `G2` |
| 7 | Dub stab, Gm9 | new | `Music/Dub Stab Gm9`, `Loops/Dub Chord Loop` |
| 8 | Warm pad bed | new | `Music/Pad Warm` |
| 9 | Vocal hook | new | `Vox/Vox Hook` |
| 10 | Ear-candy chops | new | `Vox/Candy Ride Fifth`, `Vox/Candy Me Reverse`, `Vox/Candy Cur Stutter` |
| 11 | Build fill | changed: its claps are the new clap and snare | `FX/Build Fill`, `Loops/Fill Preview` |
| 12 | Riser and impact | the impact **unchanged** (the same bytes as round 3); the riser's swung pulses moved with the swing | `FX/Riser`, `FX/Impact` |
| 13 | Synth accents | new (milestone 2) | `Music/Accent …`, `Drums/Accent …`, `Bass/Accent Rubber Stab G1`, `FX/Accent …` |
| 14 | Synth phrases | new (milestone 2) | `Loops/Phrase …`, `Music/Keys EP …` |

Round 3's glass sounds (Kick Split, the glass clap and hats, the metal pling, the dip and vowel basses, the supersaw stab, the glass pluck, the sung lines and the vocoder) are not in round 4. They stay in the `r2` and `r3` sets, and their audio is in this folder's history.

## What changed, sound by sound

**1. Kick (KickDeep)**
- The split design, binding since round 1:
  - a clean sine sub on G1, held a little longer than round 3's for depth: 20 dB down by the offbeat 8th and silent from 0.33 s, so the rolling bass has the gap;
  - a round knock above 110 Hz from a low, slow sweep (it starts at 180-380 Hz; round 3's started at 250-560), gently saturated, rolled off from 2.8 kHz, gone by 80 ms;
  - a soft felt beater above 1 kHz, the only drive on the kick.
- No glass ring.
- The top layer's crest is 5.94 / 6.28 dB, inside the 5-7 dB band round 1 liked (Split 5.8-6.4, Tight 7.9-8.9).
- Under the strict gate, 150-400 Hz falls 30 dB in 26-29 ms and its share is -14.6 dB at worst (limit -12).
- Tuning is within 2 cents.

**2. Clap and snare (ClapSnare)**
- Four hands of band noise within about 30 ms, the first the loudest, over a snare body tuned to G3 whose second mode sits on Eb4 (both in the key), and the snare's wires.
- A short warm room after them: 0.6 s, an 8 ms pre-delay.
- A gentle tanh at 4x.

**3. Hats and shaker**
- The hats are band-passed noise with a stick tick, rolled off from 8.5-15 kHz with velocity, over the bell's upper partials on C7, 14 dB down. In the loop the closed hats shuffle: soft on the beat, louder on the swung "e". The open hat sits on every "and", and the closed one on the "a" chokes it. Beat 4 lets it ring.
- The gourd shaker is seed grains (40-110 µs noise grains, one in five a glass bead) on a swell and a fall, over a soft "sh", on every 16th with the swung "e" and "a" leading.

**4. Congas, bongos and rim**
- Tuned membranes (the modes 1, 1.59, 2.14, 2.30, 2.65): congas on G3, C4 and D4, bongos on G4 and D5.
- Velocity is the stroke: a muffled ghost, an open tone, a slap. Each settles from a few percent sharp onto its note within 3 cents.
- The rimshot is a woody click: two wood modes and a faint glass ring on D6.
- The pattern is a tumbao-like conga figure, bongos answering and rims on the "a"s. Every ghost is on a swung 16th.

**5. Rubber bass and its lines (A clean, B warm)**
- The voice:
  - a saw and a square on the sub's phase, exactly band-limited;
  - a resonant low-pass (q 4.5) that boings down from 2.4-3.6 kHz to its rest in 40-65 ms on every accented note (a smaller, faster boing on the ghosts);
  - a 1.5-semitone scoop up into each accented note;
  - slides inside notes: G2 dropping an octave to G1, F1 lifting to G1, C2 lifting to D2.
- LineRubber, the hook and the loop's pick:
  - bar 1: G1 on the "and" with an octave pop, Bb1-C2, the fifth held, F2, then G2 dropping to G1;
  - bar 2: the motif varied, with Bb1-A1, F1 held and sliding up to G1, and C2 lifting to D2.
- LineDeep: a hypnotic e-and-a roll on G1, walking Bb1 C2 D2.
- Both lines rest on every kick (each note released at least 40 ms before the next) and duck 6 dB under its first 80 ms.
- A is clean: the voice from 480 Hz, under the strict gate. Its worst gap reads -25.5 dB, against the gate's -20.
- B is warm: the voice at rest from 120 Hz (bass-body). Its worst gap reads -6.2 dB against -2, and it sits 1.2 dB under a saw (limit +5).

**6. Deep sub (SubDeep)**
- The lines' sine as a held multisample on G1, D2 and G2: whole cycles over a 1 s loop.
- A whisper of its 2nd and 3rd harmonics (-26 and -32 dB) for small speakers.

**7. Dub stab (DubStabGm9)**
- Gm9 voiced open on an FM electric piano: G3 D4 F4 A4 Bb4, with the ninth rubbing against the minor third on top and nothing within a fourth of the root.
- Each note has its own modulator on its pitch (every partial harmonic), the bark of a struck tine mellowing in 0.3 s, a short tine "ding" and a soft driven amp per note.
- The dub loop plays it off the kicks and off the bass's accents (mostly the swung "a"s, pushing into the beats), through:
  - a dotted-8th ping-pong that darkens as it repeats;
  - a short room;
  - a low-pass that opens over the bars and closes again;
  - a 7 dB pump.

**8. Warm pad bed (PadWarm)**
- The same Gm9: three saws a note, the outer two 6 cents either way across the field, through a low-pass that breathes between 0.9 and 1.6 kHz once over the 4 bars.
- Every voice sits on a whole number of cycles over the loop (under 0.5 cents off its note), so it joins with no crossfade.
- In the loop it is the density under the short parts, pumped 4 dB.

**9. Vocal hook (VoxHook)**
- Cut only from the kit's existing, title-checked line "Ride the current with me": no new words.
- The chops are "ride", a stutter "ri-" on the swung "e", "the current", "me", then "ride", "ri-", "with me". Each is gated to its length like a sampler slice.
- It runs through a band-pass: a high-pass at 320 Hz and a resonant low-pass that opens from 2 to 5.5 kHz over the 2 bars and closes again.
- In the loop it answers in bars 3-4 through the vocal bus's compressor.

**10. Ear candy**
- "ride" a fifth up on F5 (WORLD keeps its formants), with an 8th-note ping-pong.
- "me" through a short room, reversed into a breathy swell that ends on the downbeat.
- "cur-" retriggered on six 32nds, softer and darker each time, into the whole word.
- In the loop, "ride" a fifth up lands on the "and" of beat 4 in bar 1, the stutter on beat 4 of bar 2 leads into the hook, and the reversed "me" swells into the loop's downbeat.

**11. Build fill.** Round 3's glass toms, now doubled by the new clap and snare.

**12. Riser and impact.** The impact is the same bytes as round 3. The riser's pulsed noise sits on the kit's grid, so its swung 16ths moved with the new swing; nothing else in it changed.

**The context loop**
- It is mixed on buses:
  - the drum bus (kick, clap, hats, shaker, perc) soft-clipped at the kick's own peak;
  - the bass bus;
  - the music bus (stab, pad, returns), with -2 dB at 330 Hz, -2.5 dB at 950 Hz and +4.5 dB at 2.8 kHz;
  - the vocal bus, compressed with the same EQ.
- The perc and vocals duck 3 dB under each kick.
- The wet loop adds the kit's sends: a 1.8 s plate with a 64th-note pre-delay, ducked under the vocals, and the dotted-8th dub echo on the stab, the perc and the vocals. Both returns are pumped 4 dB.

## Milestone 2: synth accents and phrases

**Accents** (13), one-shots, each in G minor with its root in its name and in `qa_report.json`:

| Accent | Root | What it is |
|---|---|---|
| `Accent Dub Chord Cm9` | C4 | The electric piano's Cm9 (the stab's open voicing a fourth up) with its dub tail baked in: a dotted-8th ping-pong darkening over four repeats into a short room |
| `Accent Organ Hit Gm7` | G3 | A drawbar organ chord: five bars, a percussion on the 3rd harmonic, a key click, a soft valve drive |
| `Accent Rubber Stab G1` | G1 | The rubber bass as a stab: the full boing, its voice dropping from G2 over a G1 sine pedal; A clean, under the strict gate |
| `Accent Pluck Chord Dm7` | D4 | Dm7 on tuned plucked strings (Karplus-Strong), strummed down 11 ms a string, in a short room |
| `Accent Noise Sweep Up` | G4 (its comb) | 2 beats of noise through a band-pass rising from 400 Hz to 9 kHz and a comb tuned to G4, swelling into the downbeat |
| `Accent Zap G4` | G4 | A perc synth zap: FM falling from G7 onto G4 in about 15 ms, a short tone and a click |
| `Accent Reverse Chord Gm9` | G3 | The dub stab's Gm9 through a long plate, reversed: a swell ending on the downbeat |
| `Accent Vox Synth Gm` | G3 | A vocal-synth stab: Gm on saws through three static "ah" formants |
| `Accent Glass Ping D6` | D6 | The kit's glass bell struck on D6, with an 8th-note ping-pong |

**Phrases** (14), loops on the kit's grid and swing, each also as MIDI:

| Phrase | Length | On | What it plays |
|---|---|---|---|
| `Phrase Rubber Riff` | 2 bars | Rubber Bass Clean | A rolling bass riff resting on every kick: G1 pops, F2 sliding to D2, Bb1 sliding to G1; then a held D2, C2 lifting to D2, C2 sliding down to G1 |
| `Phrase Dub Chords` | 4 bars | Dub Stab Gm9 | Gm9, Gm9, Cm9, Gm9 in the dub rhythm, through the dub echo, a room, a low-pass ride and the pump |
| `Phrase Keys Riff` | 2 bars | Keys EP | D5 F5 D5 C5 Bb4 G4, then answered up to G5 and down to A4, with the dub echo |
| `Phrase Keys Arp` | 4 bars | Keys EP | A 16th arpeggio over Gm9 and Cm9 in turn, up and down, the last bar down first, with an 8th echo |

`Keys EP` is the new electric-piano multisample (G3 D4 G4 D5 G5, two layers) that the keys phrases play. Each phrase file is peak-normalised, so the keys phrases (dense, -9 to -10 LUFS) read louder than the bass riff (-17.4): set them by ear.

## The A/Bs left for you

1. **Bass A (clean) or B (warm)** in the context loop.
   - The dry loops play at one loudness: dry A comes down to dry B's -13.05 LUFS-I.
   - The bass lanes are level-matched inside them: A's fader is up 0.47 dB, because at their own peaks B reads 0.47 LU over A.
   - The wet loop and the club master play B, the designer's pick for its warm middle. It is a pick, not a verdict.
2. **The Rubber and Deep lines, A and B.**
   - The Rubber line's files keep their own peaks, so B reads 0.47 LU over A; match them by ear.
   - The Deep line's B comes down to A (-15.70 LUFS-I).
3. **The master's cap:** 4.5 dB, or the standard's 4 (above).

## What to listen for

- **The groove.** Does it roll and make you move, and does the 0.56 shuffle on the hats, shaker and conga ghosts feel right or too swung?
- **The kick.** Deep and round, does it lead the mix, and is any of it muddy?
- **The bass.** Is the "boing" fun or too much? Do the slides and octave pops bounce against the kick? A or B?
- **Mud.** Once in mono at a moderate level: the pad bed and B's body between 150 and 400 Hz. The club loop's 250 Hz octave is +3.0 dB re 1 kHz, inside its window (0 to +4), and its 500 Hz octave +2.8, 0.17 dB inside.
- **The stab and its echo.** Do they sound deep and dubby, or do they clutter the hook?
- **The vocal hook.** Is the filter deep-house, or just muffled? Is the chop rhythm catchy?
- **The club master.** At 4.5 dB of peak reduction, do the kicks and claps still punch?
- **The accents and phrases.** Which would you drop into a track, and which are cheesy?

## QA

- **The render:** 48 items and 168 files (with the QA's groove loop), **0 failed checks** (`qa_report.md`).
- **Reproducible:** every item rendered a second time in a fresh process (`--repro`) gave the same SHA-256, all 48.
- **The club loop** passes every club check:

  | Check | Reading | Limit |
  |---|---|---|
  | `club_loudness` | -8.96 LUFS-I | window -9.5 to -7.5 |
  | `club_plr` | 7.94 dB | ≥ 7.0 |
  | `club_true_peak` | -1.02 dBTP round the loop | ≤ -1.0 |
  | `club_bands` | every octave inside its window, the closest 0.17 dB | inside |
  | `club_low_width` | -12.1 dB | ≤ -10 |
  | `club_reduction` | 4.5 dB | the declared cap, 4.5 |
  | `club_kick_crest` | 5.42 dB | ≥ 4.0 |
  | `club_crest_loss` | 1.50 dB | ≤ 1.5 |

  It also passes every per-file check:
  - DC -113 dBFS;
  - stereo correlation 0.96;
  - side under 120 Hz -55 dB;
  - the seam -24.5 dB.
- **Exceptions**, each declared with its reason in `qa_report.md`:
  - the club master's 4.5 dB cap;
  - the rubber bass multisamples' alias check, read on the steady note (0.35-1.35 s), since the boing's resonant sweep is still moving before 0.3 s;
  - the hand drums', the zap's and the glass ping's tuning windows (short notes);
  - the Cm9 accent's alias check, read on the stab itself (its echoes are gated copies);
  - the rubber stab's and the plucked chord's alias checks off: a moving shot, and Karplus-Strong partials.
- **The low end over the kit's kick:**
  - A, `Context Loop Low`: -31.3 dB in its worst gap, against the gate's -20, and -62 dB before each kick (limit -30);
  - B, `Context Loop Low Warm`: -2.8 dB in its worst gap, against bass-body's -2, 7.3 dB under a saw;
  - every kick keeps a crest of 5.92 dB as the loop plays it.
- **Tests:** FUT03's four test modules are rewritten for round 4: 43 tests on the DNA, the kick, the clap, the hats, the perc, the accents, the lines, the chords, the hook's words and the context loop. They need no vocal models.

## MIDI

`content/midi/fut03/` is regenerated from the round 4 parts:
- the drums and perc, with the congas and bongos on their own notes and the rim on 37;
- the two rubber basslines (`Rubber` is the one in the loop);
- the keys riff as the lead;
- the dub stab's Gm9 hits;
- three synth phrases on channel 4: the rubber riff, the dub chords (the Gm9 sample on G3 and, a fourth up on C4, Cm9) and the keys arp.

The extended mix (`extended/`, 224 bars) takes the house form, so the kick stays in the builds. A fresh export matches the committed files. Nothing has been opened in a DAW.

## Not verified

- **How any of it sounds.** Nobody has listened to round 4: not the groove, the kick's depth, the bass's boing, the dub echo, the filtered hook, or whether it reads as garage-house and deep tech house at all.
- **The judgement calls behind the numbers.** The targets are conventions, not measured club records:
  - the new genre's octave windows and loudness (`qa.CLUB`);
  - the 4.5 dB cap;
  - the music bus EQ;
  - the faders, the pumps and the swing.
- **Width.** The club loop's 2 kHz and 8-16 kHz octaves are narrower than the standard's width convention (-12.9, -9.3 and -8.9 dB, against at least -10 and -8). Width is reported, not gated.
- **The clap's crest loss** sits exactly at the 1.5 dB limit.
- **Vocal bytes.** The vocal masters depend on this machine's onnxruntime kernels (ADR 0007), so they are hashed per render environment.
