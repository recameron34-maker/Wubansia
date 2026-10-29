# Listening log

Every listening round goes here: sounds, MIDI, effects, the look and feel, and the DAW checks. The ratings are yours. Claude writes the entries and, for blind rounds, unblinds the results.

The protocol is in `docs/plans/S1-futuristic-slice.md` §5.6: the blind player (`tools/listening/blind.py`, arrives in S1d), the conditions, the pre-registered seeds and the pass rule. Copy the template for each round, newest last, and leave out the sections a round doesn't cover.

## Template

```markdown
## 2026-11-xx · FUT01 · round N · Listener … · Monitoring … · DAW … · Level ~80 dB SPL + quiet · Mono check y/n
### A. Sounds (S1a/S1c: blind IDs, in the context loop dry and wet, A/B with the nearest legacy sound and your reference track)
| ID | Sound | keep / fix / cut | Cool 1–5 | One kit? | Notes (what, where) |
### B. MIDI (S1d/S1e: from player.html JSON; key-file SHA-256: …)
| date | session | file id | condition* | seed | person | move | interesting | hook | gave it away |
Pass: N ≥ L+1 (median) on person and interesting; N ≥ N0 on move; N median ≥ 5 on person and interesting; no N seed < 3.
### C. Effects
| Effect / macro / pad | musical? | clicks or zips? | level jump? | matches the S1a wet target? | notes |
### D. Look and feel
Pads on the beat · knobs feel right · Studio and Stage both good · readable at a glance · smooth · sound within 2 s · drop within 15 s of first press
### E. DAW
Loads · sync and loop wrap · tempo 128/134/140 · robot phrases on the grid at 128 and 140 · pads and chops from a controller · automation both ways (touch mode) · project marked dirty after a seed change · save/reopen identical · offline bounce right after opening · drag MIDI · CPU meter · editor open/close ×10
```

\*Condition: L = legacy generator, N = new generator, N0 = new generator with feel off.

## 2026-10-29 · FUT01 · round 0 · Listener: you · Monitoring: not reported · DAW: not reported · Level: not reported · Mono check: not reported

Prototype sounds (17). Not blind: the sounds were named. No per-sound ratings.

**What you heard.** 17 prototypes rendered during planning as 48 kHz / 24-bit masters (not in git): the kick at two velocities (v1.0, v0.4), alloy hit, closed and open hats, neuro reese (F#1), alloy stab (F#m9), crushed stab, chrome pluck (F#4), a 1-bar acid line, dry and shimmer pads, a granular bed, a freeze bed, an 8-bar Shepard riser, a stutter on the pad, and a vocoder chord. You most likely heard them on the prototype listening page, which plays 192 kbps MP3 previews of the masters. The WAV pack came after this feedback.

**Your feedback:** "mostly good, the kicks are a bit muddy, I hate muddy". Later, with your approval of S1, you also report "sample pack works", most likely about that WAV pack.

**Measured** on the prototype kick, against the mud gate that came out of it (plan §5.2):

| Measure | Kick v1.0 | Kick v0.4 | Gate |
|---|---|---|---|
| 150–400 Hz falls 30 dB | 771 ms | 498 ms | ≤ 120 ms |
| 150–400 Hz against 30–120 Hz after the first 80 ms | −2.4 dB | −5.9 dB | ≤ −20 dB |
| 150–400 Hz share of the whole sound | −6.2 dB | −6.7 dB | ≤ −12 dB |
| 400 Hz–1 kHz falls 30 dB | 360 ms | 316 ms | ≤ 150 ms |

Method: 4th-order Butterworth band-pass, zero-phase; a 5 ms RMS envelope; the fall is counted from the band's own peak. The distortion keeps ringing in the low mids long after the sub, which is the mud you heard.

**Action:**
- "No mud" is now a QA gate for every kick, bass, tom and the 2-step boom, on single hits and on a 2-bar 2-step loop. Pads, stabs, textures and chops are high-passed at about 100 Hz (plan §2.2, §5.2; ADR 0005).
- The kick is being redesigned to pass the gate. The new kick comes back to you to hear.
- The other sounds: "mostly good", no notes per sound. The QA failures measured during planning (stereo low end on 6 sounds, DC on 4, tails on 4) get fixed in the new recipes, from S1a on (plan §2.2).

**Status:** open until you sign off the new kick.
