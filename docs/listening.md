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

### Template: v0.1, the plugin

v0.1 has no listening round. The fidelity report stands in for one: `python3 tools/sound/compare_render.py --groovecore build/release/cli/groovecore` measures every kit's renders against the approved round 5 loops and writes `build/v0.1-fidelity/report.md`. This entry records those numbers at the build you get, and what you find when you play it in your DAW.

```markdown
## 2026-11-xx · v0.1 · build <sha> · Listener … · DAW … (version, VST3/AU/CLAP) · Mac … (chip, macOS) · Monitoring … · Mono check y/n
### A. Fidelity (measured at <sha>, before anyone listens)
| Kit | Club loop: LUFS-I / short-term / worst 1/3 oct / true peak / kick crest | Wet loop (same) | Dry A / Dry B | Phrase lanes, worst null (pass 1 / 2) | Sampled lanes, worst band | Song sections over the ceiling | Gaps, explained |
Gates: ±0.3 LU, ±0.5 LU, ±0.75 dB, ±0.5 dB, ±0.5 dB; phrase lanes ≤ −40 dB; sampled lanes ±1 dB per 1/3 octave; sections ≤ −0.9 dBTP.
Look-back master against master.py's exact one, per kit: LUFS-I …, true peak … · CPU at 48 kHz / 128 samples, 64 voices, every effect: … % of one core (macos-15 runner; gate 15 %) · your Mac: …
### B. In your DAW (the checklist; not a listening test)
Loads in VST3 / AU / CLAP · content found (status ready) · Loop in sync from bar 1, wraps without a click · Song from any bar (chased) · the kit's tempo, ±1 %, ±5 % (phrase lanes tagged recorded or multisample) · vox/no-vox and A/B mid-bar · lane levels and mutes · Space, Echo, Pump, Club · automation both ways · save/reopen identical · offline bounce right after opening · drag MIDI (Loop, Song, a lane) into a track · CPU meter · editor open/close ×10
### C. What you said
Your words, per kit: keep, or fix (what, where).
### D. Action
```

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

## 2026-10-29 · FUT01 · round 1 (S1a) · Listener: you · Monitoring: not reported · DAW: not reported · Level: not reported · Mono check: not reported

The 12 S1a sounds and the 4-bar context loop, dry and wet, as WAV copies of the 48 kHz / 24-bit masters (`s1a-listen`, not in git: `tools/sound/render.py --kit FUT01 --set s1a` and `tools/sound/listen.py` rebuild it), and on the round 1 listening page, which plays MP3 previews of single hits and 16-bit WAV previews of the loops. Kick, snare and hat came in two characters each (A/B). Not blind: the blind player arrives in S1d.

### A. Sounds

Your ratings, from the listening page (its store, read back on 2026-10-29):

| # | Sound | Pick | keep / fix / cut | Cool 1–5 | One kit? | Notes (yours) |
|---|---|---|---|---|---|---|
| — | Context loop | | fix | 4 | yes | "Reese doesnt sound that musical and doesnt make me want to dance. The high plucks are nice." |
| 1 | Kick | A Split | | 4 | | "B sounds a bit hard like its really slamming the limiter" |
| 2 | Metal snare | | | | | not rated |
| 3 | Closed hat | | | | | not rated |
| 4 | Alloy hit | | | | | not rated |
| 5 | Neuro reese that talks | | fix | 2 | | "reeses needs the most work of all" |
| 6 | Resampled growl shot | | fix | 3 | | |
| 7 | Air pad, F#m9 | | | 4 | | |
| 8 | Chrome pluck phrase | | | 5 | | |
| 9 | Sung chops | | fix | 4 | | "sounds good weird phrase though" |
| 10 | Sliced robot phrase | | keep | 4 | | |
| 11 | Glitch fill | | | 3 | | |
| 12 | Frequency-shifted riser | | | 4 | | |

**Against the S1a gate** (plan §7, before the fix round): 9 of 12 rated; median Cool 4 (met); kept with Cool ≥ 4: 1 marked keep (the robot), 4 more at Cool ≥ 4 with no verdict (kick A, pad, pluck, riser), against 8 needed; "one kit?" answered only for the loop (yes). Not passed yet: the fix round comes next, as the plan allows.

**What it changes:**
- Kick A (Split) is the kit's kick; B (Tight) is cut. The "slamming the limiter" note becomes a QA rule: a crest floor for kicks (ADR 0011).
- The reese is the weakest sound. Two causes: its line in the loop follows the kick instead of grooving against it, and its sound is hollow (its mids start at 450 Hz to pass the strict mud gate). The fix round writes real basslines and offers the bass as A "clean" (strict gate) and B "warm" (a proposed bass-body rule, ADR 0011), so you decide by ear.
- The sung line gets new, more natural lyrics; growl and glitch fill get a clearer musical gesture.
- The pluck (Cool 5) and the high plucks in the loop are the kit's strength.
- Snare, hat and alloy hit stay as they are until you rate them.

**Then:** you asked for four kits ("give me four kits"). The fix round for FUT01 runs together with three new first-listen kits, one per remaining direction (ADR 0010). They come back as round 2.
