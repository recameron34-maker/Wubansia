# FUT02 "Orbit Relay": round 5, the vox synth and the accents

Written for the kit's render folder (`build/fut02-r5`, from `render.py --kit FUT02 --set r5`), which holds the FLAC masters, `qa/`, `qa_report.md`, `qa_report.json` and `listen.json` this page names; its copy in `content/samples/fut02/` sits beside the WAVs alone.

A sci-fi club kit at 130 BPM in D minor (7A): a split kick, an electro clap, choked hats and a rolling offbeat bass under a chrome arp, every sound on one shimmering stretched-wire tone and a laser pitch drop. After round 3 you said: "sounds are very good, vocals suck, either ditch or rethink, need those cool accent synths". So round 5 keeps every non-vocal sound byte for byte, drops the text-to-speech vocals, puts a wordless vox synth in their place, and adds synth accents and phrases, featured in the loops.

**Nobody has listened to round 5.** Every number below is a measurement made in this session; none of it says how it sounds.

## Start here

The A/B: `Loops/Context Vox Synth Club.wav` (the vox synth riff in bars 2-3, where the old vocals were) against `Loops/Context No Vox Club.wav` (the same loop instrumental, the arp and the accents on top). Then the vox synth itself (`Vox/`), the riff (`Loops/Vox Synth Riff.wav`), the accents and the phrases.

| Club master | Vox synth | No vox | Sci-fi club target |
|---|---|---|---|
| Loudness, LUFS-I (round the loop) | -9.26 | -9.43 | -8.5 ± 1 |
| True peak | -1.02 dBTP | -1.02 dBTP | ≤ -1.0 |
| PLR / PSR | 8.24 / 7.80 dB | 8.41 / 8.09 dB | ≥ 7.0 / ≥ 6 |
| Peak reduction (clipper + limiter) | 4.25 dB (clipper 2.84, limiter 1.41) | 4.25 dB (clipper 2.89, limiter 1.36) | round 3's declared cap, 4.25 |
| Kick crest in the loop, each full-velocity kick | median 6.17 dB, every kick 4.22 or more | median 6.19 dB, every kick 4.19 or more | the floor 4.0 |
| Crest the master costs (median per hit) | kicks 0.84, claps 0.92 dB | kicks 0.91, claps 0.84 dB | declared 1.6 (round 3: 1.47 / 1.53) |
| Octaves re 1 kHz: sub, 125, 250, 500, 2k, 4k, 8k | +10.4, +1.8, +0.7, +1.0, -0.6, -2.8, -4.8 dB | +10.5, +1.9, +0.8, +1.1, -1.4, -3.5, -5.0 dB | every one inside its window |
| Side over mid at 125 / 250 Hz | -49.8 / -14.4 dB | -50.8 / -14.1 dB | ≤ -10 |

## What changed, and why

**1. The vocals are gone.** The three sung lines, the eight chops, and the vocoder phrase dry and wet are out of the set (they stay in set r3, which still renders round 3 exactly). Nothing in r5 needs the speech models.

**2. The vox synth (sound 9).** A wordless vocal synth made by synthesis alone (`recipes/fut02/voxsynth.py`), a robotic but musical vocoder choir:
- a band-limited, glottal-like buzz (additive, every partial a harmonic, a saw's tilt with a pulse's ripple);
- five formant band-passes per vowel, applied frame by frame, so vowels glide: 'ee', 'ah', 'oh';
- a short 'd'-like onset: a 7 ms noise burst, the formants opening from a closed position in 35 ms, the pitch scooping up 35 cents;
- vibrato from 0.25 s into a note (±13 cents at 5.4 Hz), a slow jitter of about 1 cent, breath through the same formants;
- the choir: a double 8 cents sharp on the left and 8 cents flat on the right, its formants 2% higher, above the note's fundamental;
- the sparkle: its top above 3.5 kHz frequency-shifted 9 Hz up on the left and down on the right, 18 dB under.

The multisample `Vox Synth` sits on D4 F4 A4 C5 F5 A5, and **the velocity picks the vowel**: soft 'oh' (v1), medium 'ah' (v2), hard 'ee' (v3), each layer brighter and louder. Every zone and layer reads its root within 2 cents. Five single chops: 'ah' D5, 'ee' F5, 'oh' A4, 'ee' opening to 'ah' C5, and 'ah' closing to 'oh' A5 (a beat, with vibrato).

**3. The vox synth riff (sound 10).** Two bars that lock with the chrome arp. Each bar opens on the arp's 3-3-2 accents a third under its motif (D5 F5 E5 under F5 A5 G5), answers in the arp's held notes (D5 C5; A5 C5), and ends on a throw: A4 held a beat into a dotted-8th ping-pong whose repeats fall away in pitch. Its vowels follow the velocities ('ah', 'ee' on the accents, 'oh'), so `content/midi/fut02/FUT02_Phrase_VoxSynth_2bars.mid` plays it back on the multisample.

**4. Synth accents (sound 13).** Nine, each on a stated root in D minor:
- laser chord stab (D4 Dm9, B♭3 B♭maj9): the arp's chrome voice as a chord, a laser falling onto it;
- starfield sweep (D): the pad's stars and a whoosh rising over 1 bar into the downbeat;
- glass ping (D6), with a ping-pong;
- resonant zap (A5);
- arp stab (D6): D5 F5 A5 D6 in 32nds;
- reverse chord (D4): two beats of swell into the beat;
- vox synth stab (D4, Dm7 'ah');
- noise uplifter (D, 2 bars);
- sub drop (D2, two octaves down).

**5. Synth phrases (sound 14).** Each a loop and a MIDI file on channel 4 (`content/midi/fut02/FUT02_Phrase_*`), four with the vox riff:
- a 2-bar laser stab riff: Dm9 on the 3-3-2 accents, then B♭maj9;
- a 4-bar glass melody;
- 4 bars of vox choir chords: Dm7 'oh', Dm9 'ah', B♭maj7 'ee', Am7 'ah'.

**6. The two loops** (`recipes/fut02/context5.py`), both through round 3's buses, sends and club master:
- Vox synth: the riff from bar 2's downbeat, where round 3's robot called. Its throw goes fully into the hall and the echo and falls into the fill bar. The arp keeps a light pocket under it (2 dB down, 1.2-6.5 kHz 5 dB further), not round 3's word pocket.
- No vox: no pocket on the arp, and four accents carrying the top.
- Accents in both, as ear candy: each one's loudest 400 ms reads -22.5 to -23.5 LUFS (momentary), 5.3-7.1 LU under the drums and the fill where it plays:
  - vox synth: the laser chord stab on the and of 4 in bar 1 (a pickup into the riff), the glass ping on the and of 4 in bar 2 (in the riff's rest), the starfield sweep into the loop's end;
  - no vox: the stab on the and of 4 in bar 1, the reverse chord swelling into bar 3, the arp stab on the and of 2 in bar 3, the sweep into the loop's end.

**7. Three mix moves the master needed.** Without round 3's two compressed vocal lanes, the loop is sparser between its kicks. At the 4.25 dB cap it stopped under the genre's window, and the peaks were the music's, not the kick's: the vox synth's bright 'ee' notes (0.8 dB over the kicks on their own) and the zaps (round 3's loudest peak too). Measured on the club master's output, at the 4.25 dB cap:

| Step | Vox synth, LUFS-I | No vox, LUFS-I |
|---|---|---|
| Round 3's mix, the vox riff in place of the vocals | -10.86 | -10.15 |
| The vox lane through a soft clip, 6 dB under its peak (a vocal synth driven, its level kept) | -9.96 | (no vox lane) |
| The zaps 4.5 dB under round 3's fader (their sends unchanged), the bass 1.5 dB over it (it rests on every kick, so it adds loudness and no peaks) | **-9.26** | **-9.43** |

Raising the cap instead would have needed about 5.1 dB for the instrumental (5.0 dB reaches -9.57), near the 5.6 dB the A&R review found flattened the kicks. Raising the music, the pad or the hats made it quieter (more peaks); compressing the vox lane made it quieter too. The low end changes with the bass. `ContextLow5` and `ContextLow5Warm` are the kick and bass as round 5 plays them, and replace round 3's `ContextLoopLow` and `ContextLoopLowWarm` in this set.

## The sounds

| # | Sound | Round 5 | Files |
|---|---|---|---|
| 1 | Split kick on A1 | **unchanged** (the same bytes) | `Drums/Kick Split` |
| 2 | Electro clap | **unchanged** | `Drums/Electro Clap` |
| 3 | Closed and open hats | **unchanged** | `Drums/Closed Hat`, `Drums/Open Hat`, `Loops/Hat Loop` |
| 4 | Laser zap on D5 | **unchanged** (4.5 dB lower in the round 5 loops) | `Drums/Zap` |
| 5 | Rolling bass | **unchanged** (1.5 dB higher in the round 5 loops) | `Loops/Orbit Groove …`, `Relay Groove …`, `Orbit Line …`, `Relay Line …`, `Bass/Roll Bass …` |
| 6 | Drop glide | **unchanged** | `Bass/Drop Glide Clean`, `Drop Glide Warm` |
| 7 | Starfield pad, Dm9 | **unchanged** | `Music/Starfield Pad Dm9`, `Loops/Starfield Pad Loop` |
| 8 | Chrome arp | **unchanged** | `Loops/Chrome Arp Phrase`, `Music/Chrome Arp` |
| 9 | Vox synth | **new**, in place of the sung chops | `Vox/Vox Synth …`, `Vox/Vox Chop …` |
| 10 | Vox synth riff | **new**, in place of the vocoder phrase | `Loops/Vox Synth Riff` |
| 11 | Laser fill | **unchanged** | `FX/Laser Fill`, `Loops/Laser Fill Preview` |
| 12 | Laser riser | **unchanged** | `FX/Laser Riser`, `Loops/Laser Riser Preview` |
| 13 | Synth accents | **new** | `Music/Laser Chord Stab …`, `FX/Starfield Sweep`, `Drums/Glass Ping …`, `Drums/Resonant Zap …`, `Music/Arp Stab …`, `FX/Reverse Chord …`, `Music/Vox Synth Stab …`, `FX/Noise Uplifter`, `Bass/Sub Drop` |
| 14 | Synth phrases | **new** | `Loops/Laser Stab Riff`, `Loops/Glass Melody`, `Loops/Vox Choir Chords` |

All 102 files of the 26 continuing items are byte-identical to round 3's render (SHA-256), as is the kick's 4x4 groove loop in `qa/`.

## What to listen for

- **The A/B.** Vox synth or no vox: does the riff make the loop, or is the instrumental with the accents on top the stronger record?
- **The vox synth.** Does it sound like a cool modern vocal synth (robotic, shimmering, musical) and not a fake singer? Which vowel do you reach for: 'oh', 'ah' or 'ee'? Is the 'd' onset too clicky or too soft?
- **The riff with the arp.** Does it lock with the arp's accents, or do the two crowd each other in bars 2-3? Does the throw lead into the fill?
- **The accents.** Do they read as ear candy, under the drums and not over them? Which would you drop into a track, and which are missing?
- **The mix moves.** Are the zaps still clear enough 4.5 dB lower? Is the bass, 1.5 dB up, too big?
- **Mud.** Once in mono: anything boomy or boxy between 150 and 400 Hz?

## QA

- **The render:** 55 items, 158 files, **0 failed checks** (`qa_report.md`).
- **Reproducible:** every item was rendered a second time in a fresh process (`--repro`), with the same SHA-256.
- **Both club loops** pass every club check: loudness, PLR, true peak round the loop, every octave window (the closest 0.64 dB inside its edge), low width, the declared cap and crest loss, and the kick crest floor.
- **The low end over the kit's kick:**
  - A (the gate): `ContextLow5` -29.1 dB (limit -20).
  - B (bass-body): `ContextLow5Warm` -3.88 dB (limit -2).
- **Tuning:** the vox synth reads its root within 2 cents on every zone, layer and chop; the pitched accents within 1.3 cents, the resonant zap 2.6 (its filter's fall; QA's limit is 3).
- **The written loop as MIDI** (`content/midi/fut02`): the four synth phrases are new files on channel 4 and the README lists them. The extended mix's README points its Vox cues at the vox synth now; every other file is unchanged.

## Not verified

- **How any of it sounds:** nobody has listened to round 5.
- **The judgement calls behind the numbers:**
  - the vox synth's formants, onset, vibrato and sparkle depth;
  - the accents' levels (set by momentary loudness, not by ear);
  - the three mix moves, made to fit the master at round 3's cap.
