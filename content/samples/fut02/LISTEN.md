# FUT02 "Orbit Relay": round 2 first listen

A sci-fi club kit at 130 BPM in D minor (7A): a split kick, an electro clap, choked hats and a rolling offbeat bass under a chrome arp. Every sound shares one shimmering stretched-wire tone (the kit's ratios) and a laser pitch drop, from the zap to the fill and the riser.

Nobody has listened to these yet. The numbers below are measurements, not judgements of the sound.

## What's here

| Folder or file | What |
|---|---|
| `Drums/`, `Bass/`, `Music/`, `Vox/`, `FX/`, `Loops/` | WAV copies to listen to: 120 files, 48 kHz / 24-bit, the same samples as the masters. `v1` is the softest velocity layer, `rr` the round robin |
| `FUT02_*.flac` | The 120 masters (28 MB) |
| `qa/` | The four-on-the-floor loops the kick and bass-shot mud checks ran on |
| `sheets/` | One spectrogram sheet per instrument |
| `qa_report.md`, `qa_report.json` | Every QA check with its number, every exception with its reason, the render environment, the loop points, the robot's slices and the wet loop's send settings |
| `listen.json` | The round 2 listening page's entry for this kit |

Start with `Loops/Context Loop.wav` and `Loops/Context Loop Warm.wav` on loop, then go through the 12 sounds.

## The 12 sounds

| # | Sound | Files | A/B |
|---|---|---|---|
| 1 | Split kick on A1 | `Drums/Kick Split` (5 layers × 2); `qa/FUT02_Drums_KickSplit_4x4.flac` (2 bars; QA's groove, softer on 2 and 4) | |
| 2 | Electro clap | `Drums/Electro Clap` (5 × 2) | |
| 3 | Closed and open hats | `Drums/Closed Hat`, `Drums/Open Hat` (5 × 3 each); `Loops/Hat Loop` (2 bars, the choke) | |
| 4 | Laser zap on D5 | `Drums/Zap` (5 × 2) | |
| 5 | Rolling bass | `Loops/Orbit Groove Clean`, `Orbit Groove Warm` (the pick), `Relay Groove Clean`, `Relay Groove Warm` (2 bars each: the line with the kit kick on every beat and the hats, at the context loop's levels); `Loops/Orbit Line …`, `Relay Line …` (the lines alone); `Bass/Roll Bass Clean`, `Roll Bass Warm` D1, A1, D2 (2 layers, 2 s loops) | **A clean / B warm** |
| 6 | Drop glide | `Bass/Drop Glide Clean`, `Drop Glide Warm` (2 layers) | **A / B** |
| 7 | Starfield pad, Dm9 | `Music/Starfield Pad Dm9` (attack, then a 4-bar loop); `Loops/Starfield Pad Loop` (the loop alone) | |
| 8 | Chrome arp | `Loops/Chrome Arp Phrase` (2 bars); `Music/Chrome Arp` D4 G4 C5 F5 A#5 (B♭5) (2 layers) | |
| 9 | Sung chops | `Vox/Sung Line 1` ("Pull me into orbit", the pick) and its 8 chops (`Chop Pull` … `Chop Pull Me Into`); `Sung Line 2` ("Your light keeps calling me") and `Sung Line 3` ("Send your signal to me") to compare | |
| 10 | Vocoder phrase | `Vox/Vocoder Relay` ("Relay online", dry), `Vocoder Relay Wet` | dry / wet |
| 11 | Laser fill | `FX/Laser Fill` (1 bar, ends on the downbeat); `Loops/Laser Fill Preview` (a groove bar, then the fill) | |
| 12 | Laser riser | `FX/Laser Riser` (8 bars, ends on the downbeat); `Loops/Laser Riser Preview` (the riser, then a bar of groove) | |

**The context loop** (`Loops/`, 4 bars, 7.38 s, loops seamlessly). Original, written for this kit. Four on the floor, every beat on the hard layer (out on beat 4 of bar 4 for the fill's dive), the clap on 2 and 4 under it with ghosts, rolling hats with the open hat choked. The Orbit bassline rests on every kick and answers it; bar 4 opens with the drop glide, then the line comes back on D1 and B♭1 and climbs A1-C2-D2 into the loop. The chrome arp plays up front over the Dm9 pad. The robot calls "Relay online" in bar 2, the zap answers, and the sung line replies: "pull me" (bar 2), "into orbit" (bar 3). Under the words the arp steps back (4 dB, its top 1.2-6.5 kHz carved a further 6-10 dB, so its melody stays and the words come through), and it rests in bar 4 while the laser fill plays. Every lane swings its odd 16ths by the kit's one amount (4.6 ms).
- `Context Loop`: dry, bass A (clean). `Context Loop Warm`: dry, bass B (warm), level-matched to A, so the two differ only in the bass.
- `Context Loop Wet`: bass B (my pick for the wet, see below), plus Space (an FDN with a little shimmer, ducked under the vocals), Echo (tape ping-pong on a dotted 8th) and the resonator stand-in on the pad's chord times the kit's ratios; "into orbit" is thrown into the fill bar. This is **the target the engine's sends must match** (S1e); its settings are in `qa_report.json`.
- `Context Loop Low`: the kick and bass A alone, as they sit in the loop, mud-gated in every gap. `Context Loop Low Warm` is the same with bass B under the bass-body rule.

## The bass A/B

Round 1's reese was "the most work of all" and didn't make you dance, so every kit's bass comes twice, and you pick by ear (ADR 0011):
- **A, clean**: the strict mud gate. Its mids start at 520 Hz behind a steep high-pass, so 150-400 Hz stays empty: tight and punchy, a little hollow.
- **B, warm**: the proposed bass-body rule. The same voice keeps its own harmonics down to 120 Hz, up to a plain saw's level, over the same sine sub: body on small speakers.

Both play the same two lines, heard first over the kit's own kick (the groove loops). **Orbit** (the designers' pick, in the context loop) bounces D1-D2 on the offbeats with a pickup that changes every beat; **Relay** is more melodic, around D2 with a slide and a climb. Over the kick, B reads -3.8 dB (Orbit) and -5.6 dB (Relay) against the bass-body limit of -2 dB, A -29.2 and -24.5 dB against the gate's -20. For the wet loop I picked **B**: it is the answer to round 1's hollow reese, and the bass takes no sends, so the wet version doesn't change the comparison. If A wins, the wet loop is re-rendered with A.

## What to listen for

- **Does the context loop make you want to dance?** Is the bass musical now? Does the arp stand out the way the high plucks did in round 1, and do the words come through over it?
- **Kick.** Split's range, never driven on the body: top-layer crest 5.92 (rr2) and 6.32 (rr1) dB (Split 5.82-6.44, Tight 7.93-8.87). It is short on purpose: the sub holds about 60 ms and is 15-18 dB down by the e, where the bass plays the first note of each beat, so the two don't fight (a quiet tail, 30-40 dB down, runs to 0.40 s). Does it punch without "slamming the limiter", and is it long enough for a club kick?
- **Bass A or B**, in the groove loops and the dry context loops first (same level), then on small speakers. Do the lines groove against the kick?
- **Drop glide.** Does the dive land on D1 in time, and does it feel like a transition? B keeps its body through the dive and clears it before the next kick.
- **Vocals.** Is "Pull me into orbit" a line you'd use? Do the chops keep whole consonants and natural breath tails? Is "Relay online" clear in the loop? All vocals are Kokoro speech resynthesised with WORLD at 48 kHz; the top band above 12 kHz is synthetic.
- **Gestures.** Zap, fill and riser all use the laser drop: is it a clear, musical shape, in key, and does each land on the 1? The zap is now mostly its fall (75-95 ms onto D5).
- **Mud.** Listen once in mono at a moderate level: anything boomy or boxy between 150 and 400 Hz?
- **One kit?** Do all 12 sound like one family?

## The round 2 gate

Per kit, the S1a gate:
- at least **8 of the 12 sounds kept with Cool ≥ 4** after one fix round
- a **median Cool ≥ 4**
- "one kit?" **yes on at least 10**

## Not verified

How anything sounds: nobody has listened. The mix levels in the context loop were set by loudness measurements, not by ear, and the arp's room under the vocals was set by band measurements (the arp at or under the vocals in 1.4-5.6 kHz over every vocal phrase), not by ear.
