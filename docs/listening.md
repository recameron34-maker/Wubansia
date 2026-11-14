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
Loads in VST3 / AU / CLAP · content found (status ready) · Loop in sync from bar 1, wraps without a click · Song from any bar (chased) · the kit's tempo, ±1 %, ±5 % (phrase lanes tagged recorded or multisample) · vox/no-vox and A/B mid-bar · lane levels and mutes · Space, Echo, Pump, Club · automation both ways · save/reopen identical · offline bounce right after opening · drag MIDI (Loop, Song, a lane) into a track · CPU meter · editor open/close ×10 · 1 drum track: a Song onto a Drum Rack (Live) and a Drum Machine Designer (Logic) track, in a new set or project
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

## 2026-11-11 · v0.1 · build e48b3e0 (measured at f079d0b) · Listener: you · DAW: Logic Pro (AU); Ableton Live 12 not checked yet · Mac: not reported · Monitoring: not reported · Mono check: not reported

v0.1 has no listening round (ADR 0017): this entry holds the fidelity numbers measured before anyone listens, and the DAW checklist for you.

### A. Fidelity (measured at f079d0b, before anyone listens)

Linux x86-64, Clang 18 release build, the samples installed from the four content zips (`tools/content/install.py`); `compare_render.py` against the round 5 masters, pass 2, 48 kHz, the plugin's look-back master, bass B unless named. **0 failing of 271 gated checks**; the known gaps are below. The arrangement review's MIDI (f079d0b) left every loop, tempo, rate and multisample number as it was at f1a4276; only the Song changed. Each cell: LUFS-I / short-term / worst 1/3 octave / true peak / kick crest, in LU and dB against the round 5 loop.

| Kit | Club loop, vox; no-vox | Wet loop, vox; no-vox | Dry A / Dry B | Phrase lanes, worst null (pass 1 / 2) | Sampled lanes, worst band | Song sections over the ceiling | Gaps, explained |
|---|---|---|---|---|---|---|---|
| FUT01 | +0.02 / +0.05 / −0.32 @ 50 Hz / +0.00 / +0.33; +0.01 / +0.03 / −0.26 @ 50 Hz / −0.00 / +0.23 | +0.01 / +0.02 / +0.11 @ 1 kHz / −0.01 / +0.20; +0.01 / +0.02 / +0.13 @ 1 kHz / −0.06 / +0.06 | pass / pass (worst +0.13 dB @ 1 kHz, crest +0.21) | bit-exact / −41.3 dB outside the 1 ms re-syncs (−35.8 over the whole pass) | −0.36 dB @ 50 Hz (bass A) | 0 of 36 (peak −1.02 dBTP) | the no-vox warp stab's release (known) |
| FUT02 | −0.03 / +0.17 / +0.31 @ 794 Hz / +0.00 / +0.30; −0.07 / +0.13 / −0.34 @ 1 kHz / −0.00 / +0.27 | +0.02 / +0.07 / +0.36 @ 794 Hz / +0.04 / +0.37; −0.02 / +0.05 / −0.27 @ 5 kHz / +0.17 / +0.25 | pass / pass (worst +0.41 dB @ 794 Hz, crest +0.35) | −87.9 / −87.9 dB (−66.9 over the whole pass) | within 0.01 dB | 0 of 36 (peak −1.02 dBTP) | none |
| FUT03 | +0.01 / +0.05 / +0.23 @ 2.5 kHz / +0.00 / +0.23; −0.01 / +0.04 / −0.11 @ 50 Hz / +0.00 / +0.22 | +0.01 / +0.03 / +0.17 @ 2.5 kHz / −0.03 / +0.17; −0.00 / +0.01 / −0.01 @ 198 Hz / +0.08 / +0.04 | pass / pass (worst +0.01 dB, crest +0.07) | −89.1 / −89.1 dB | within 0.01 dB | 0 of 36 (peak −1.02 dBTP) | none |
| FUT04 | −0.06 / +0.10 / +0.56 @ 79 Hz / +0.00 / +0.24; −0.13 / +0.18 / +0.54 @ 79 Hz / +0.00 / +0.20 | +0.12 / +0.15 / +0.66 @ 79 Hz / +0.04 / +0.41; +0.09 / +0.11 / +0.66 @ 79 Hz / +0.04 / +0.39 | kick crest +0.56 dB on both (known); otherwise worst +0.66 dB @ 79 Hz | bit-exact / bit-exact outside the re-syncs (−49.7 over the whole pass) | within 0.01 dB | 0 of 28 (peak −1.02 dBTP) | the dry kick crest (known); +0.66 dB @ 79 Hz is P3's own margin (engine_ref +0.66) |

Gates: ±0.3 LU, ±0.5 LU, ±0.75 dB, ±0.5 dB, ±0.5 dB; phrase lanes ≤ −40 dB; sampled lanes ±1 dB per 1/3 octave; sections ≤ −0.9 dBTP.

The Song, on the reviewed arrangements: 8 bars of every section (both variants, bass A and B, 136 renders) and each whole extended mix (both variants, bass B, 395-420 s), all finite, none silent; true peak −1.02 dBTP in every section and every 8-bar line (ceiling −0.9). Whole mixes against their Club loop, vox; no-vox: FUT01 −1.5; −1.4 LU, FUT02 −1.3; −1.5, FUT03 −0.5; −0.4, FUT04 −0.9; −0.7. Drops sit within ±1 LU of the Club loop, intros, breaks and outros 1-9 LU under it, except FUT03's Break 2 (145-160: the keys arp and the pad, no drums), +1.3 / +1.6 LU over it, with Drop 2 +0.9 / +1.0: the keys and the fx channels play in no loop, so no gate measured them.

Look-back master against master.py's exact one (same mix), vox; no-vox: FUT01 −0.01; −0.02 LU, FUT02 −0.02; −0.02, FUT03 +0.00; −0.02, FUT04 −0.01; −0.01; true peak 0.00 dB everywhere; worst band FUT01 −0.30 dB @ 50 Hz. Rates, resampled to 48 kHz: 44.1 and 96 kHz read −0.03 to +0.08 LU and +0.05 to +0.11 dB true peak against 48 kHz (P8's fixed-rate clip; FUT01's 96 kHz true peak was +0.55 dB before it).

CPU at 48 kHz / 128 samples, 60-65 voices (about 50 held through MIDI in over the loop), every effect, on this shared Linux machine (not the macos-15 runner; gate 15 %): FUT01 11.4 %, FUT02 8.5 %, FUT03 8.6 %, FUT04 10.2 % of one core on average; p99 blocks 12.2-15.4 %; worst blocks 17.6-31.3 % (the stress's first block and the machine's load, as P12 found). The loops alone at 512 samples: 3.7-6.9 %. Your Mac: …

Off the kit's tempo and the multisample path (past ±2 %, with phrases off, from MIDI in, and the Song cycles no table matches), against the at-tempo Club loop; the gate is ±0.3 LU and ±1.5 dB per 1/3 octave:
- phrases off at the kit's tempo: +0.01 / −0.04 / +0.13 / +0.09 LU (FUT01-04); worst bands +0.83 dB @ 2 kHz, +1.03 @ 630 Hz, +1.21 @ 794 Hz, +1.02 @ 1.26 kHz;
- ±3 %: loudness within +0.20 LU everywhere; three bands over 1.5 dB, accepted as known gaps: FUT04 at −3 % +1.73 dB @ 250 Hz (+1.50 @ 1.26 kHz), where the tempo (+0.7 to +1.0 dB @ 250 Hz at ±1 % on the stems) and the multisample (+0.69 at the kit's tempo) add up; FUT03 at −3 % +1.87 dB @ 794 Hz, the multisample path (+1.21 at the kit's tempo); FUT02 at +3 % −1.50 dB @ 50 Hz, a tempo effect on the kick (−0.52 with phrases off at the kit's tempo). FUT03's +2.01 dB @ 198 Hz at −3 % is a tempo effect on a lane without phrases, as it was before the calibration.
- each phrase lane on its multisample, after its channel's inserts, as P2 measured it (content/plugin/README.md): levels within 0.7 dB of the stems; worst bands FUT01 pluck −3.7 dB @ 3.2 kHz, vox −2.1 @ 250 Hz; FUT02 bass −1.8 @ 99 Hz, arp −4.5 @ 794 Hz, vox +2.9 @ 250 Hz; FUT03 bass +2.7 @ 500 Hz, vox −4.3 @ 630 Hz; FUT04 bass −6.2 @ 16 kHz (the loop's ride; −0.9 dB, worst +2.1 at rest), lead −0.9 @ 99 Hz. The report's multisample lane rows read P12's dry taps, before the `_ms` channels' corrective EQ: informational only.

Known gaps:
- FUT01's no-vox warp stab rings 15-30 ms past its gate: it needs a release that ends on the note-off (a contract change).
- FUT04's dry A/B kick crest (0.56 dB against 0.5): the engine's exact-tempo grid (338823.53 samples a loop) against the recipe's rounded 338824-sample loop puts the kick 1 sample off the bar-anchored stem on some beats.
- FUT04's acid timbre steps where a ride cycle with a low CC plays the live-filtered multisample next to an at-rest stem cycle.
- The three off-tempo bands above.
- In the Song, some phrase-lane cycles play the multisample because no table matches them (`plugin_kit.py --report`): FUT04's bass under the arranged acid rides (26 cycles), FUT03's switch-up Rubber riff in bars 129-144 (8 cycles, every note, by the matcher's majority rule), FUT02's vox in bars 61-80, 125-128 and 145-152 (8).
- FUT01's vox riff entering fresh carries the previous cycle's baked throw, 36 dB under the chops, over its first 0.6 s; FUT04's squelch ticks play the full acid sample for about 33 ms on the multisample path.

### B. In your DAW (the checklist; not a listening test)
- [ ] Loads in VST3 / AU / CLAP (the AU works in Logic Pro, 2026-11-11; Live 12 not checked yet)
- [ ] Content found (status ready) after `install-macos.sh`
- [ ] Loop in sync from bar 1, wraps without a click
- [ ] Song from any bar (chased)
- [ ] The kit's tempo, ±1 %, ±5 % (phrase lanes tagged recorded or multisample)
- [ ] Vox/no-vox and A/B mid-bar
- [ ] Lane levels and mutes
- [ ] Space, Echo, Pump, Club
- [ ] Automation both ways
- [ ] Save/reopen identical
- [ ] Offline bounce right after opening
- [ ] Drag MIDI (Loop, Song, a lane) into a track
- [ ] CPU meter
- [ ] Editor open/close ×10
- [ ] FUT01: drop 2 without its kick in bars 161-168
- [ ] FUT02: the bare choir in bars 67-68; the choir's own A4 against B♭4 (a revoice was declined for v0.1); a pad release tail crossing into the B♭ bars
- [ ] FUT03: the drop landing with the stab open; Break 2 (145-160) against the drops' level
- [ ] FUT04: the anvil pulse in the 97-104 break; the no-vox mix's moved throws

### C. What you said
2026-11-11, after installing the macOS build (e48b3e0): "Sounds great". No kit or part named for a fix. Then: "plugin
worked on Logic Pro"; Ableton Live 12 not checked yet.

### D. Action
- None to the sounds, the MIDI or the mix from this note: every kit keeps its round 5 sounds and the reviewed
  extended mixes.
- The checklist above wasn't reported item by item, so its boxes stay open; tick them as you check each one.

## 2026-11-12 · v0.2: Samples, and Drums on one track · build: the v0.2 PR's macOS CI zip (not made yet) · Listener: you · DAW: not reported yet · Mac: not reported

v0.2 adds the Samples drawer (ADR 0024): every kit's sounds, to hear and to drag into your DAW as WAV files. It also
adds Drums on one track (ADR 0025): the drag strip's 1 drum track switch, which makes Loop and Song drags bring their
drums as one track, Drums. Neither changes a sound, the kits' MIDI files or the mix, so this entry has no fidelity
numbers and no listening test. It records what was checked on Linux and holds the DAW checklist for you; nothing here
says how either behaves in Live or Logic until you report it. How to use them: `dist/INSTALL.md`, "Samples into your
DAW" and "Drag MIDI into Live or Logic".

### A. Checked here (before you try it)

Linux x86-64, Clang 18 release build, at a54f9e9 on the integration branch: both features with their review fixes
and the integration's own (the later commits change only docs). ctest ran without the samples. pluginval with content,
`[.real]`, `[.render]` and the copy benchmark read the four kits' samples from a checkout's `content/samples` through
`WUBANSIA_CONTENT_DIR`: the `<kit>/` layout, not the `<kit>/v1/` that the content zips install.
- ctest: 882 tests, 879 passed and 3 skipped (the real kits' renders, which want the samples; the drawer's keyboard
  focus, which wants a display and passed on its own under `xvfb-run`, as CI's Linux job runs it; a read-only
  folder, which root can write anyway). `golden_check`: the 516 legacy files match `legacy-v21.sha256` and all 5,842
  metric values match.
- The catalog: FUT01 35 sounds, FUT02 37, FUT03 36, FUT04 32, which reach all 618 files. Every export name is unique
  in its kit and the longest is 46 characters.
- The default file of every sound in the four kits (140 sounds, 139 files: two of FUT02's sounds share one) copied
  from those samples byte for byte, each with kit.json's SHA-256, in 846 ms (`[.real]`).
- Offline renders: the processor still plays each kit's Loop as `groovecore render` does, bit for bit (`[.render]`).
  An audition never reaches an offline render.
- Drums on one track: with the switch lit, every Loop and Song drag of the four kits, with the vox synth and without,
  with nothing muted and under the tests' mute sets, gives the conductor, one Drums track where the first drum track
  was (every note as it was, on channel 10, note-offs first on a tick) and every other track byte for byte, and every
  committed MIDI file's drum tracks merge the same way; switched off, or with one drum track left, a drag is the file
  as before. The switch is kept in settings.xml beside the two folders and never marks the project changed. The drag
  strip fits it and every phrase title at 100 %, in this machine's default font and again in DejaVu Sans, the widest.
- pluginval 1.0.4 at strictness 10 on the VST3, with and without content: pass. clap-validator 0.4.1 (report-only):
  41 run, 30 passed, 0 failed, 1 warning, 10 skipped, as in v0.1.
- TSan (clang 18, run locally; CI has no TSan job), no reports: the engine's audition tests (14 cases: 11 passed, 3
  skipped, the allocation counters, which a sanitizer owns); the plugin's audition, sounds, real-time and service
  tests (96 cases: 91 passed, 5 skipped: two allocation counters, the display, root, the samples); its editor,
  controller and drag-out tests, the drum option's included (97 cases: 96 passed, 1 skipped, the display).
- CPU at 48 kHz / 128 samples on this shared machine, % of one core on average, plain and then with an audition
  playing in about 98 % of the blocks (budget 15 %; the test fails above 30 %): FUT01 11.7 and 11.7 %, FUT04 10.6
  and 10.2 %, SYN01 11.5 and 11.2 %. Copying the largest file, FUT03's 8-bar riser (4.3 MB), took 22 ms; its
  44.1 kHz audition was ready in 44 ms.

### B. In your DAW (the checklist; not a listening test)
- [ ] The Samples button opens the drawer in each format your DAWs host (AU, VST3, CLAP); the groove keeps playing
- [ ] A click plays the sound; Up, Down, Return and Esc reach the drawer from the moment it opens, in Live and in
      Logic, and Space still reaches the DAW
- [ ] How the auditions sound, and their level over a loud groove (6 dB down, under Output, with no limiter)
- [ ] Live 12, from the VST3: drag a one-shot to an audio track, to a Drum Rack pad and to a MIDI track (Simpler)
- [ ] Live 12, from the AU: the same three drags (a drop from a JUCE VST3 into Live has been reported to raise an
      error dialog, so each format needs its own try)
- [ ] Live 12: drag a loop; FUT03's Context Vox Riff (6 bars at 128 BPM: a 4-bar loop, then its echo tail) warps
      at the right tempo
- [ ] Logic, from the AU: drag to an audio track and to Quick Sampler
- [ ] The CLAP, if one of your DAWs loads it: drag a one-shot to a track
- [ ] Logic: an audition plays while stopped with Wubansia's track selected; unselected, the drawer shows "No sound
      from your DAW..." with Logic's advice
- [ ] Live 12: with no audio output device (or Live's audio engine off), a click shows "No sound from your DAW: it
      isn't running Wubansia's audio right now..." and never Logic's words
- [ ] Logic, stopped: start FUT03's Riser (8 bars) with Wubansia's track selected, then select another track; when
      you select Wubansia's track again, the riser doesn't play on from where it was
- [ ] Logic: a 44.1 kHz project, with "Convert audio file sample rate when importing" on
- [ ] Before v0.2 ships, when the file names freeze: drag a FUT01 sample into Live and into Logic (every FUT01 name
      holds `#`: Chrome Pluck's is `Chrome Pluck F#4 v2 - FUT01 134 F#m 11A.wav`). It drops intact: it plays, and the
      clip or region and the file the project uses keep the whole name, `#` included. If either DAW cuts a name at
      `#` or refuses the drop, say so: until v0.2 ships, sharps can still be spelled `s` instead
- [ ] Live's Collect All and Save, and Logic's "Copy audio files into project", take the copies into the project
- [ ] The Standalone app: drag to Finder
- [ ] Show in Finder (and a click on the tile) shows the file in Finder; Change... picks a folder, a new one included
- [ ] The first drag from Logic's AU makes `~/Music/Wubansia/Samples`
- [ ] Whether either DAW reads anything from the files besides the audio (a root note, a tempo)

Drums on one track (the switch next to Song on the drag strip):
- [ ] The switch reads "1 drum track" in full in your Mac's font, and FUT01's phrase chips show their titles whole, or
      without their bar count ("E Piano Stab Rhythm")
- [ ] With the switch lit, in a new Live 12 set: drop FUT01's Song on a track holding a Drum Rack. The drums land there
      as one clip, Drums, and the other instruments on the tracks after it (a drop onto your own set writes over the
      tracks after it)
- [ ] With the switch lit, in a new Logic project: drop the Song on a Drum Machine Designer track. The drums are one
      region on it and the rest go on tracks after it; note the region names and how many pads it gives
- [ ] The pad settings in `dist/INSTALL.md`: the hats in one choke group (the ride outside it), FUT04's rumble on B0
      (Simpler on Gate in Live, Quick Sampler on Classic in Logic), and F#2 for FUT01's tambourine and FUT03's shaker
- [ ] A merged Drums clip plays as the separate drum tracks did; muting Wubansia's six other lanes brings only Drums
- [ ] The switch stays lit in another project and after the DAW restarts; switched off, the drags are the files as
      before

Drags on Windows and Linux are not on this list: your setup is Live and Logic on a Mac.

### C. What you said
Nothing yet.

### D. Action
None yet: tick the boxes above as you check each one.
