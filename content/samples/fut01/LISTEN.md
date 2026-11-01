# FUT01 "Alloy Signal": listening round 2

A future garage / 2-step kit at 134 BPM in F# minor (11A) with a sci-fi colour: metal drums and an alloy hit tuned to one inharmonic ratio set, a reese bass, chrome plucks and a sung club vocal. This is its fix round: the bass, growl, vocal and glitch fill are rebuilt from your round 1 notes, and the rest comes back unchanged, byte for byte.

Nobody has listened to these yet. Start with `Loops/Context Loop.wav` and `Loops/Context Loop Warm.wav` on loop. The WAVs in `Drums/`, `Bass/`, `Music/`, `Vox/`, `FX/` and `Loops/` carry the same samples as the `FUT01_*.flac` masters; `v1` is the softest layer.

## The 12 sounds

| # | Sound | Status | Listen to |
|---|---|---|---|
| 1 | Kick, Split | unchanged; B (Tight) has left | `Drums/Kick Split v5` … `v1`, `qa/` 2-step loop |
| 2 | Snare, A Metal / B Modal | unchanged, not rated yet | `Drums/Snare Metal`, `Drums/Snare Modal` |
| 3 | Closed hat, A FM / B Ring | unchanged, not rated yet | `Loops/Hat Pattern FM`, `Loops/Hat Pattern Ring` (the loop's hat part, 2 bars) |
| 4 | Alloy hit, F#5 | unchanged, not rated yet | `Drums/Alloy Hit`, `Drums/Alloy Hit Short` |
| 5 | Reese bass, three basslines | **rebuilt**, A clean / B warm | `Loops/Bassline A, B, C` × `Clean`, `Warm`; `Bass/Reese Clean`, `Bass/Reese Warm` |
| 6 | Growl moves | **rebuilt** | `Bass/Growl Talk F#1`, `Bass/Growl Dive F#1` |
| 7 | Air pad, F#m9 | unchanged | `Music/Air Pad F#m9`, `Loops/Air Pad Hold` |
| 8 | Chrome pluck and phrase | unchanged (your Cool 5) | `Loops/Chrome Pluck Phrase`, `Music/Chrome Pluck` |
| 9 | Sung line and chops | **rebuilt** | `Vox/Sung Line 1` (the pick), `2`, `3`, and 6 chops from line 1 |
| 10 | Robot phrase | unchanged | `Vox/Robot Boost`, `Vox/Robot Boost Wet` |
| 11 | Glitch fill | **rebuilt** | `FX/Glitch Fill Lift`; its roll in the context loop's bar 4 |
| 12 | Riser | unchanged | `FX/Shift Riser`, `FX/Shift Riser Drop` |

**Context loop** (4 bars): round 1's loop with bassline B (the Walk) instead of the reese line, two chops of the new line and the new fill's roll. "Find me on the floor" sings in bar 2 and "floor" in bar 4, both with "floor" on the and-of-3 kick; the second is thrown into the fill and held over it into the downbeat. The chops sing only pieces of the line that no released song is titled: "come find me" and "on the floor", the first take's chops, are both titles of released singles. The drums stop for one beat, as in round 1, for the fill's roll. The bass's top ducks under every pluck note, so the plucks keep their attack. Plucks, pad and drums are as you rated them. `Context Loop` has bass A, `Context Loop Warm` bass B, `Context Loop Wet` adds the sends (Space, Echo, the resonator stand-in) with bass B: the engine's target. `Context Loop Low` and `Low Warm` are kick and bass alone.

## The bass A/B

The same reese (five drifting saws over a mono sine sub, a filter that opens on each note) in two low ends:
- **A, Clean:** nothing between 150 and 400 Hz, the strict mud gate as in round 1.
- **B, Warm:** A plus the saws' own body from 120 to 450 Hz, up to a plain saw's level (the proposed bass-body rule, ADR 0011). Above 450 Hz it is A, attack included, so at the same loudness B is A with body and about 1 dB quieter elsewhere.

Each plays three original 2-bar lines with kick A and hat A, at the same loudness. Every line leaves each kick its first 80 ms, starts nothing but ghosts on a loud kick's tail, and answers in the gaps, on the kit's swing:
- **B, the Walk** (in the context loop): the root lands late, then bar 1 climbs and bar 2 walks down.
- **A, the Bounce:** a root-octave answer after every kick, with swung ghosts.
- **C, Deep:** long roots, the fifth and seventh answering late.

Pick a character and a line.

## What to listen for

- Does the bass make you want to dance now? Is the line musical, and does the kick keep its room?
- Clean or warm, on small speakers too: is A hollow, is B muddy?
- Which of the three sung lines sounds like a real club vocal? Do the chops keep their consonants, and do "find me on the floor" and "floor" work as the hook?
- Do the growl moves and the fill read as gestures (a rhythm, a vowel move, a pitch move), not texture? Try the growls in mono too.
- Snare A or B, hat A or B, and the alloy hit: keep or fix?
- The loop as a whole: do the plucks still lead, is it one kit, is the wet version too wet?

## The round 2 gate

Per kit: **at least 8 of the 12 sounds kept with Cool ≥ 4** after one fix round, a **median Cool ≥ 4**, and **"one kit?" yes on at least 10**.

## QA in one line

42 items, 138 files, every check passes (`qa_report.md` lists each exception and its reason), and every master rendered the same SHA-256 a second time, each item in a fresh process (`--repro`), vocals included. Low end: `Context Loop Low` reads −30.0 dB against the sub in its gaps (limit −20) and −54.6 dB before each kick (limit −30); `Low Warm` reads −4.8 dB (bass-body limit −2) and −7.6 dB over a saw (limit +5). Each bass lane alone, without the kick that sets the floor in those files, passes its own rule too: A −24.9 dB at worst, B −3.1 dB. Reese B against A at the same loudness: +18 dB at 150–400 Hz, −0.6 to −1.1 dB above 400 Hz, the same attack. Growls: every hit at its accent in mono, the side at least 8 dB under the mid. Kick A's top-layer crest is 6.44 / 5.82 dB (floor 4.0). The three full loops sit at −16.3 LUFS and the hat patterns at −25 LUFS, so each A/B is by sound, not level. `tracks/` keeps the bass and vox designers' QA reports of their first takes, superseded by this render.

None of this says how anything sounds.
