# FUT04 "Furnace Code": round 2 first listen

A dark industrial techno kit at 136 BPM in E minor (9A): a heavy split kick on E1, a gated iron snare and a 16th acid bass over a kick-fed rumble, with modular blips on top. Every sound shares one struck-iron tone (the modal ratios of a heavy steel anvil) and one dark concrete room, and every written part swings its odd 16ths by the same 8.8 ms.

Nobody has listened to these yet. The numbers below are measurements, not judgements of the sound.

## What's here

| Folder or file | What |
|---|---|
| `Drums/`, `Bass/`, `Music/`, `Vox/`, `FX/`, `Loops/` | WAV copies to listen to: 115 files, 48 kHz / 24-bit, the same samples as the masters. `v1` is the softest velocity layer, `rr` the round robin |
| `FUT04_*.flac` | The 115 masters (24 MB) |
| `qa/` | The four-on-the-floor loop the kick's mud check ran on |
| `sheets/` | One spectrogram sheet per instrument |
| `qa_report.md`, `qa_report.json` | Every QA check with its number, every exception with its reason, the render environment, the loop points, the robot's slices and the wet loop's send settings |
| `listen.json` | The round 2 listening page's entry for this kit |

Start with `Loops/Context Loop.wav` and `Loops/Context Loop Warm.wav` on loop, then go through the 12 sounds.

## The 12 sounds

| # | Sound | Files | A/B |
|---|---|---|---|
| 1 | Split kick on E1 | `Drums/Kick Split` (5 layers × 2); `qa/FUT04_Drums_KickSplit_4x4.flac` (2 bars) | |
| 2 | Gated iron snare | `Drums/Snare Gated` (5 × 2) | |
| 3 | Dark metal hats | `Drums/Closed Hat`, `Drums/Open Hat` (5 × 3 each); `Loops/Hat Loop` (2 bars, the choke) | |
| 4 | Anvil on E5 | `Drums/Anvil` (5 × 2) | |
| 5 | Acid bass | `Loops/Acid Line 1 Clean`, `Acid Line 1 Warm` (the pick), `Acid Line 2 Clean`, `Acid Line 2 Warm` (2 bars each, the kicks declared, not in the file); `Bass/Acid Clean`, `Acid Warm` E1, A1, E2 (2 layers, 1-bar loops) | **A clean / B warm** |
| 6 | Rumble | `Bass/Rumble` (2 bars); `Loops/Rumble Under Kick` (the same under the kick) | |
| 7 | Drone, Em(add9) | `Music/Drone Em Add 9` (fade-in, then a 4-bar loop); `Loops/Drone Loop` (the loop alone) | |
| 8 | Modular blips | `Loops/Mod Blip Phrase` (2 bars); `Music/Mod Blip` A#4 C#5 E5 G5 A#5 C#6 (2 layers; zones every 3 semitones) | |
| 9 | Sung chops | `Vox/Sung Line 1` ("Come down to the furnace", the pick) and its 7 chops (`Chop Come` … `Chop Down To The Furnace`); `Sung Line 2` ("Move like a machine") and `Sung Line 3` ("Nothing stops the night") to compare | |
| 10 | Robot phrase | `Vox/Robot Fill The Furnace` ("Fill the furnace", dry), `Robot Fill The Furnace Wet` | dry / wet |
| 11 | Stutter fill | `FX/Stutter Fill` (1 bar, ends on the downbeat); `Loops/Stutter Fill Preview` (a bar of groove, then the fill) | |
| 12 | Iron riser | `FX/Iron Riser` (8 bars, ends on the downbeat); `Loops/Iron Riser Preview` (the riser, then the bar it lands on) | |

**The context loop** (`Loops/`, 4 bars, 7.06 s, loops seamlessly). Original, written for this kit. Four on the floor, every kick on the top layer, out on beat 4 of bar 4 under the fill's roll; the gated snare on 2 and 4 with ghosts; driving closed hats round an open off-beat hat that they choke. The acid line AcidLine1 plays twice: it rests on every kick and answers it, root then an accented leap whose target changes, a squelch tick on beat 3's kick, a held B1 to close bar 1, a held C2 in bar 2, then the top G2 and the climb home. The rumble comes in for bars 3 and 4. The modular blips play up front over the Em(add9) drone, and the anvil answers in their gaps. The robot calls "Fill the furnace" in bar 2, the sung chops answer: "come down" (bar 3), "the furnace" (into bar 4). While a word sounds, the bass's top (over 500 Hz) and the fill's hiss (over 2.5 kHz) dip about 6 dB, so the words sit over the acid and the fill. The stutter fill takes bar 4 and lands on the 1.
- `Context Loop`: dry, bass A (clean). `Context Loop Warm`: dry, bass B (warm), the bass lane at the same loudness, so the two differ only in the bass.
- `Context Loop Wet`: bass B (my pick for the wet, see below), plus Space (an FDN with a little shimmer on the kit's dark concrete seed, ducked under the vocals), Echo (tape ping-pong on a dotted 8th) and the resonator stand-in on the drone's chord times the kit's iron ratios; "the furnace" is thrown into the fill. This is **the target the engine's sends must match** (S1e); its settings are in `qa_report.json`.
- `Context Loop Low`: the kick, bass A and the rumble alone, as they sit in the loop (without the bass's dip under the vocals, which is all over 500 Hz), mud-gated in every gap. `Context Loop Low Warm` is the same with bass B under the bass-body rule (a QA file).

## The bass A/B

Round 1's reese was "the most work of all" and didn't make you dance, so every kit's bass comes twice, and you pick by ear (ADR 0011):
- **A, clean**: the strict mud gate. The acid voice starts at 500 Hz behind a steep high-pass, so 150-400 Hz stays empty: tight, the squelch on top, a little hollow.
- **B, warm**: the proposed bass-body rule. A's sub line and A's squelch (in the level-matched loops B sits within 1.0 dB of A over 800 Hz), plus the voice's own harmonics from 120 Hz, which fill the 150-400 Hz that A leaves empty; between 400 and 800 Hz B is 3.7 dB under A. Its sine also carries a quiet second harmonic (82 Hz) on its low notes, and its edges are a little quicker (16-20 ms, A's 24 ms).

A and B play the same sub line. For each beat's first 250 ms, while the kick's own E1 tail still rings, the sine holds E1, starting in phase with that tail, so kick and bass add up into one note instead of beating against each other; notes later in the beat play their own pitch, an octave down from D2 up (in AcidLine1 every one of them is an E, so its sub is E1 throughout). The line's melody is in the voice: A's squelch, B's body.

Both play the same two lines. **AcidLine1, the rolling hook** (the bass designer's pick, in the context loop): kick, root, accented leap, the leap's target changing each time; a squelch-only tick on beat 3's kick; bar 1 closes on a held fifth (B1), bar 2 holds a dark C2 for most of a beat, peaks on G2 and climbs home. **AcidLine2, the off-beat pulse**: held off-beat roots with a blip before each kick, doubled into a rising 16th roll in bar 2. For the wet loop I picked **B**: it is the answer to round 1's hollow reese, and the bass takes no sends, so the wet version doesn't change the comparison. If A wins, the wet loop is re-rendered with A.

## What to listen for

- **Does the context loop make you want to dance?** Is the acid line musical, and does it groove against the kick? Do the blips stand out the way the high plucks did in round 1?
- **Kick.** Split's range, drive only above 300 Hz: top-layer crest 5.86 and 6.43 dB (round 1's Split 5.82-6.44, Tight 7.93-8.87). Does it hit hard without "slamming the limiter"? Is the sub gone before the next beat?
- **Bass A or B**, in the two dry loops first (same level), then on small speakers.
- **The low end.** Kick, bass and rumble should land as one heavy E1 under each beat, with no wobble where the kick's tail meets the bass.
- **Rumble.** Weight or blur? It should swell after each kick and be gone before the next.
- **Vocals.** Is "Come down to the furnace" a line you'd use? Do the chops keep whole consonants and natural breath tails? Can you hear "Fill the furnace" through the robot's distortion (see "Not verified"), and do the robot and the chops read over the acid in the loop? All vocals are Kokoro speech resynthesised with WORLD at 48 kHz; the top band is synthetic.
- **Gestures.** Do the stutter fill and the riser build in key and land on the 1?
- **Mud.** Listen once in mono at a moderate level: anything boomy or boxy between 150 and 400 Hz?
- **One kit?** Do all 12 sound like one family of struck iron?

## QA in brief

All 39 instruments (115 files) pass every check (0 failed); a second render of every item in a fresh process gave the same bytes for every master, the vocals included.
- Kick mud gate: 150-400 Hz falls in 59 ms and sits 68 dB under the sub after 80 ms; in the four-on-the-floor loop, -41.7 dB in the gaps.
- Bass A: the lines read -29.8 (line 1) and -35.0 (line 2) dB in their worst gap (limit -20). In the loop, kick, bass A and rumble read -35.1, and -32.0 or lower with the bass moved or the kick changed as below.
- Bass B (bass-body): the lines read -4.24 and -6.25 dB in their worst gap (limit -2). In the loop, with the kick and the rumble, -5.56; it stays at -4.94 or lower with the bass up to 5 ms early or late, the kick 1 dB up or down, or the bass's phase turned (a kit test checks it). With the kick's own phase turned, as an EQ on the kick alone would: an LR4 all-pass at 110 Hz (63 degrees at E1, the review's test, which read -0.99 before this fix round) reads -5.56; at 60 Hz (123 degrees) -2.67, still inside the limit.
- The low end adds up: on every note of the loop, kick and bass together are never more than 0.1 dB under their power sum in 30-120 Hz, and per beat they read +1.1 to +2.1 dB over the kick alone. The rumble adds +2.3 dB per beat under its kick in the preview and +2.0 to +2.2 dB in the loop's bars 3 and 4 (before its phase was turned it added nothing; with its polarity flipped it would take up to 1.4 dB away).
- The rumble under the kick: -47.9 dB in the gaps.
- Whisper on the dry vocals: line 1 heard "Come down to the furnace", line 2 heard "Move like a machine!", line 3 heard "Nothing stops the night."; the robot heard "fill the furnace."

## The round 2 gate

Per kit, the S1a gate:
- at least **8 of the 12 sounds kept with Cool ≥ 4** after one fix round
- a **median Cool ≥ 4**
- "one kit?" **yes on at least 10**

## Not verified

How anything sounds: nobody has listened. The mix levels in the context loop were set by loudness and masking measurements, not by ear: in the dry loop's formant band (1-4 kHz), the robot sits level with the rest of the mix and the chops 6.9 dB under it (the review measured 7 and 13 dB under before this fix round). The robot's words are the weakest point: Whisper hears the dry file word for word as written, and with band noise 12 dB under it on 3 of 4 noise seeds, but only on 14 of 40 versions with silence added before or after it (FUT01's robot, which round 1 kept, 39 of 40). In bar 2 of the loop it hears no words at all, and the robot alone in that bar as "Village Furnace". Other takes, other notes, a different rhythm and lighter distortion were tried; none read reliably better (the same take on B2 held up better with silence around it, 22 of 40, but worse under noise). Your ear is the judge. The bass's E1 and the rumble are turned in phase with the top kick layer's tail at 136 BPM in these files; the engine will have to keep that relation (softer kick layers ring at other phases).
