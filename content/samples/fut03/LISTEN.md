# FUT03 "Glass Current": round 2 first listen

Glass Current is a hyper-clean future house kit at 128 BPM in G minor (6A): a split kick, a bouncy metallic bass that dips in pitch on every note, a supersaw Gm9 stab pumped by the kick, and a bright glass pluck up front. Every metallic sound rings on one tuned glass bell whose lowest partials form a minor chord, so the plings, the hats, the clap's tone, the kick's click and the fill's toms all sit in the key.

Nobody has listened to these yet. Every number below is a measurement, not a judgement of the sound.

## What's here

| Folder | What |
|---|---|
| `Drums/`, `Bass/`, `Music/`, `Vox/`, `FX/`, `Loops/` | WAV copies to listen to: 153 files, 48 kHz / 24-bit, the same samples as the masters. `v1` is the softest velocity layer, `rr` the round robin |
| `FUT03_*.flac` | The 153 masters, named as the kit will ship them |
| `qa/` | The four-on-the-floor loops the kick and the vowel-bass mud checks ran on |
| `sheets/` | One spectrogram sheet per instrument |
| `qa_report.md`, `qa_report.json` | Every QA check with its number, every exception with its reason, the render environment, and the wet loop's send settings |
| `listen.json` | The clips for the round 2 listening page |

Start with `Loops/Context Loop.wav` and `Loops/Context Loop Warm.wav`, on loop. Then go through the sounds one by one.

## The 12 sounds

| # | Sound | Files | A/B |
|---|---|---|---|
| 1 | Kick, split, on G1 | `Drums/Kick Split` (5 layers × 2 round robins); `Loops/Drum Loop` (with the clap and hats); `qa/FUT03_Drums_KickSplit_4x4.flac` (alone, the QA groove: beats 2 and 4 a layer softer) | |
| 2 | Clap | `Drums/Clap` (5 × 2); `Loops/Drum Loop` (with the kick and hats, 2 bars) | |
| 3 | Hats | `Drums/Closed Hat`, `Drums/Open Hat` (5 × 3 each); `Loops/Hat Loop` (2 bars on the swing) | |
| 4 | Metal pling | `Drums/Metal Pling G4`, `C5`, `D5`, `G5` (5 × 2 each) | |
| 5 | Dip bass and its bassline | `Loops/Line Bounce Clean` / `Warm` (the pick), `Loops/Line Roll Clean` / `Warm`; `Bass/Dip Bass Clean` / `Warm` `G1`, `D2`, `G2` (2 layers, 1 s loops) | **A Clean / B Warm** |
| 6 | Vowel bass | `Bass/Vowel Bass Talk Clean G1` / `Warm G1`, `Bass/Vowel Bass Rise Clean G1` / `Warm G1` (2 beats each) | **A Clean / B Warm** |
| 7 | Chord stab, Gm9 | `Music/Stab Gm9` (3 × 2); `Loops/Stab Pump` (2 bars, pumped) | |
| 8 | Glass pluck and its phrase | `Loops/Glass Pluck Phrase` (2 bars); `Music/Glass Pluck D4` … `A#5` (2 layers) | |
| 9 | Sung chops | `Vox/Sung Line 1` ("Ride the current with me", the pick) and its 7 chops (`Vox/Chop …`); `Vox/Sung Line 2`, `Vox/Sung Line 3`, the other candidates | |
| 10 | Vocoder phrase | `Vox/Vocoder Drop` ("Let the glass drop", dry), `Vox/Vocoder Drop Wet` | dry / wet |
| 11 | Build fill | `FX/Build Fill` (1 bar, ends on the downbeat); `Loops/Fill Preview` (a bar of groove, then the fill) | |
| 12 | Riser and impact | `FX/Riser` (8 bars, ends on the downbeat), `FX/Impact` (the hit on that downbeat) | |

## What changed after the first build's review

- **The basslines sit on their notes.** The sub used to dip with the voice, 1-3 semitones flat for most of every short note. Now, in each gap between two kicks, one sine holds the written notes, gliding 15 ms into each. Only the metallic bite (and B's body) dips, for 38 ms, and not at all on the ghosts. Every held note now has 73-100% of its sub within 30 cents of its pitch; before it was 0-59%.
- **A and B play the same bassline.** A used to play every note above Bb1 an octave down in the sub. The A/B is now the low mids only.
- **Each bar of a line holds one note** from the "e" to just before the next kick (270 ms).
- **The vowel bass comes as A and B too.** A is the old design, with the vowels lifted above 1.5 kHz, so it loses their first formants. B keeps them from 370 Hz, over a sub that sings the syllables' notes. Both now speak around the kick on beat 2 instead of over it.
- **The stab is voiced in thirds** (G3 Bb3 D4 F4 A4). The ninth no longer sits a semitone under the third.
- **The fill's toms ring only partials in the key.** The Bb and F toms rang a Db and an Ab.
- **The hats are tuned to G7**, so their carriers ring Bb, D and G.
- **The vocoder sings in the kit's one voice blend**, a new take chosen with Whisper.

## The context loop

`Loops/`, 4 bars, 7.5 s, loops seamlessly. It is original, written for this kit:
- **Rhythm:**
  - the kick on every beat, every hit the same full layer (it was strong-weak);
  - the clap on 2 and 4;
  - open hats on the offbeats, choked by closed 16ths.
- **Bass and chords:** the bounce bassline rests on every kick and answers in the gaps. The Gm9 stab pushes on the off-beats only, pumped 9 dB by each kick; its hits on the kick were swallowed by the pump.
- **Lead and voice:** the pluck plays its phrase up front in bars 1–2. In bars 3–4 the voice and the pluck trade half bars: the chop "ride the current", then the pluck; the robot "let the glass drop", then the pluck leading back to the top.
- **Extras:**
  - the vowel bass answers once (bar 2, beats 3–4);
  - the pling rings on the "and" of 3 in every bar;
  - the build fill's second half (its 16ths and 32nds) takes bar 4 into the downbeat, after the robot's first three words.
- **Swing:** every lane swings its odd 16ths by the same 7 ms (swing 0.53).

The files:
- `Context Loop`: dry, both basses A (Clean).
- `Context Loop Warm`: dry, both basses B (Warm). The A and B lanes are level-matched, so the A/B is fair.
- `Context Loop Wet`: B, plus the three sends:
  - Space, a reverb with a little shimmer, ducked under the vocals;
  - Echo, a tape ping-pong on a dotted 8th;
  - the resonator stand-in, ringing Gm9's own notes on the kit's bell partials.

  The Space and Echo returns pump with the kick. **This is the target the engine's sends must match** (S1e); its settings are in `qa_report.json`.
- `Context Loop Low`: the kick and both A basses alone, the mud-gated low end. `Context Loop Low Warm` is the same with both B basses, checked under the bass-body rule.

## The bass A/B

The dip bass is a mono sine sub under a bright metallic bite from 480 Hz. A held note (the `Bass/` multisamples) dips 2.2–3 semitones and springs back within 90 ms. In the lines the sub holds each note's pitch and only the bite (and B's body) dips, for 38 ms. A and B share the sub, the bite and the notes:
- **A, Clean** keeps 150–400 Hz empty: the strict mud gate.
- **B, Warm** adds a steady body from 120 Hz, the voice at rest, starting 10 ms after the sub. It is held to the proposed bass-body rule (ADR 0011).

**What B changes.** At the same loudness, B has 8.6 dB more in 120–480 Hz and its bite is 0.7 dB quieter. So B is fuller and darker, but not louder on small speakers. Through a phone-like 200 Hz high-pass its bass reads 0.1 LU over A's, and through a laptop-like 150 Hz one 0.4 LU. The full loops differ by under 0.1 LU.

The context loop uses the designer's pick of line, **Bounce**:
- G1 on the offbeats with an octave pop;
- the fifth held from the "e" of beat 3;
- the seventh and back;
- the motif back varied in bar 2, with a held F2 that slides to D2.

The wet loop uses **B, Warm**, my pick for its body in the low mids. It's a pick, not a verdict: the dry pair is there for you to decide.

## What to listen for

- **Kick:** tight and clean, not "slamming the limiter". Its hardest layer's crest is 6.02–6.36 dB, in round 1's Split range (5.8–6.4) and well under Tight's 7.9. Soft layers should be darker, not just quieter.
- **Bass:** does A or B make you want to dance?
  - Do the notes and the octave pops sound in tune, with a bounce on each note?
  - Listen on small speakers too: B should sound fuller there, not louder.
  - Check B against the kick in `Context Loop Low Warm`: no boom, no mud.
- **Vowel bass:** clear vowels and a gesture, not a texture. Does B's fuller "oo" and "ah" read better than A's?
- **Pluck and phrase:** the signature element. Is it as good as round 1's chrome pluck, and does the phrase stick?
- **Chops and robot:** is "Ride the current with me" a natural club line? Are "let the glass drop" and its slices clear?
  - Every vocal is Kokoro speech resynthesised with WORLD at 48 kHz, so its top band above 12 kHz is synthetic.
  - The lines are original and clean; a web search found no song or lyric with these exact phrases.
- **Mud:** the low end should stay clean in mono and at about 80 dB SPL. Try `Context Loop Low`.
- **Fill and riser:** each should be a clear gesture that lands on the downbeat.
- **Wet loop:** is it too wet, too dry, or too much shimmer? Does the resonator help the hats and plings sit in the chord, or does it ring?
- **One kit?** Do all of these sound like one family?

## The round 2 gate

Per kit, as in S1a. A sound is **kept** only with **Cool ≥ 4** (1–5). The kit passes with all three:
- at least **8 of the 12 sounds kept** after one fix round;
- a **median Cool ≥ 4**;
- **"one kit?" yes** on at least 10.

## Numbers that matter

All 42 items and 153 files pass every check (`qa_report.md`). The figures below are the worst file of each item, against its limit.

| Check | Result | Limit |
|---|---|---|
| Kick crest, top layer (rr1 / rr2) | 6.02 / 6.36 dB | ≥ 4.0 (Split read 5.8–6.4, Tight 7.9–8.9) |
| Kick mud gate: 150–400 Hz fall, vs the sub after 80 ms, share, 400 Hz–1 kHz fall | 29 ms, −90 dB, −14.8 dB, 18 ms | 120 ms, −20, −12, 150 ms |
| Kick, four on the floor: gaps vs sub / before each hit | −51.3 / −56.0 dB | −20 / −30 |
| Bass A lines, Bounce / Roll: gaps vs sub; before each kick | −22.7 / −22.8; −48.8 / −39.8 dB | −20; −30 |
| Bass B lines, Bounce / Roll: gaps vs sub; over a saw; share; under the kick | −8.4 / −8.7; +0.4 / +0.7; −12.5 / −12.7; −88 / −91 dB (the bass rests) | −2; +5; −5; −4 |
| Every line again after one more 20 Hz high-pass (a DAW's or the engine's): worst gap, A / B | −22.5 / −8.3 dB | −20 / −2 |
| Bass lines: each held note's sub within 30 cents of its written pitch | 73–100 % of its energy (it was 0–59 %) | ≥ 70 % (a kit test) |
| Bass B notes (G1 D2 G2): vs sub, over a saw, share | −5.6, +3.8, −9.9 dB | −2, +5, −5 |
| Vowel bass A, Talk / Rise: groove loop before each hit | −31.4 / −31.4 dB | −30 (a thin margin) |
| Vowel bass B, Talk / Rise: vs sub; over a saw; share; groove gaps; before each hit | −10.6 / −3.9; −1.8 / −19.3; −20.3 / −13.1; −13.2 / −7.1; −35.7 / −37.3 dB | −2; +5; −5; −2; −30 |
| Context loop low end, A (strict gate): gaps vs sub, before each kick, share | −22.7, −55.6, −16.6 dB | −20, −30, −12 |
| Context loop low end, B (bass-body): gaps vs sub, before each kick, share, over a saw | −7.7, −44.5, −14.1, −3.8 dB | −2, −30, −5, +5 |

- **Same bytes:**
  - Every item rendered a second time in a fresh process (`--repro`) matched, all 42.
  - A second full render matched on all 153 masters and the 5 `qa/` loops, vocals included, on this machine. Both hash lists are kept with the fix's notes.
  - The 98 masters this fix didn't touch are byte-identical to the first build: the kick, clap, pling, pluck and its phrase, the sung lines and chops, the bass multisamples, the riser and the impact.
- **Exceptions, each with its reason in `qa_report.md`:**
  - the B basses, the B vowel shots and `Context Loop Low Warm` use bass-body;
  - the full context loops and the two drum previews switch the mud rule off: the clap, vocals, stab and fill belong in the low mids, and their low end is gated as `Context Loop Low` and `Context Loop Low Warm`;
  - the fill and the riser are end-aligned (`cut_end`);
  - the vowel shots skip the alias check (`tonal` off);
  - the vocals are `hf_synthetic`.

## Not verified

- How any of it sounds: nobody has heard it.
- The balance in the loops was set from measured loudness, not by ear:
  - the bass and the vowel bass about 2 LU under the kick;
  - the pluck as loud as the bass;
  - the chop and the robot about 1 LU under the kick;
  - the hats 11 LU down.
- Whisper is only a proxy for the words in the loop. It hears "let the glass drop" word for word in 6 of 9 windows of the A loop and 9 of 9 of the B loop; before the fix it was 1 of 9. "Ride the current" is heard whole alone, but in the loop mostly as "right the current", in either character.
- The vocal masters' bytes depend on this machine's onnxruntime kernels (ADR 0007). They are hashed per render environment.
