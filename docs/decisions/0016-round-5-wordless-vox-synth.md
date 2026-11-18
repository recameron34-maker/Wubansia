# 0016. Round 5: a wordless vox synth in place of the sung vocals

**Status:** Accepted, 2026-11-18, under ADR 0028 (build in, cut later): the vox synth stays, and every Loop and Song also comes without it (`NoVox`). Proposed 2026-11-05: after listening to round 4 the user wrote: "sounds are very good, vocals suck, either ditch or rethink, need those cool accent synths".

## Context
Every first-listen kit had sung vocals: lines and chops sung by a text-to-speech voice and re-pitched (ADR 0005's vocal chain, Kokoro and Whisper through sherpa-onnx), plus a robot or vocoder phrase. Round 4's listen kept everything else and rejected those vocals. Rounds 4's synth accents (FUT01, FUT03) were the part the user asked for more of.

## Decision
- **The sung vocals leave every kit's round 5 set.** That covers the sung lines, word chops, hooks, throws, calls and robot and vocoder phrases, and the loops that played them. `--set r5` needs no vocal models. Earlier sets keep their items, so `--set r3` and `--set r4` still render their rounds.
- **Each kit gets a wordless vox synth instead, built by synthesis alone:** a glottal-like source through five formants per vowel, vowel glides, breath, a soft onset, vibrato and jitter, and a detuned double, in each kit's own voice.
  - FUT01: bright, pitched-up UK garage chops.
  - FUT02: a sci-fi vocoder choir.
  - FUT03: deep, breathy "ooh" and "aah" chops.
  - FUT04: dark, metallic "oh" and "oo" stabs.

  Each kit has a playable multisample and chops (sound 9), and a vox synth riff (sound 10), which is also a MIDI phrase on channel 4. The synth has no words, so it needs no title check, no clean or R-rated versions, and no Whisper check.
- **The user chooses by ear between rethinking and ditching.** Every kit's context loop comes as two club masters: one with the vox synth, and one without vocals, where accents carry the top. The no-vox version is the "ditch" option.
- **The synth accents play in every loop:** two to four hits per 4 bars, levelled under the drums. FUT02 and FUT04 get nine accents (sound 13) and three or four phrases (sound 14) of their own, as FUT01 and FUT03 have had since round 4.
- **Everything else is byte-identical to the round it comes from:** round 4 for FUT01 and FUT03, round 3 for FUT02 and FUT04. Two exceptions:
  - FUT04's acid stab accent moved up an octave to E4 with a high-pass, because at E3 half its energy sat in the low mids.
  - Three kits' round 5 loops changed their mix to reach the genre's loudness without the old vocals' density. Only the loops changed, never a sample.
- **The club masters,** both versions passing club QA:

  | Kit | Vox synth: LUFS-I, PLR | No vox: LUFS-I, PLR | Peak reduction |
  |---|---|---|---|
  | FUT01 | −9.73, 8.7 dB | −9.71, 8.7 dB | 4 dB, the standard's cap |
  | FUT02 | −9.26, 8.2 dB | −9.43, 8.4 dB | 4.25 dB, round 3's cap |
  | FUT03 | −9.23, 8.2 dB | −9.18, 8.2 dB | 4.5 dB, round 4's cap |
  | FUT04 | −8.61, 7.6 dB | −8.10, 7.1 dB | 6 dB, round 3's cap |

## Consequences
- The user hears one A/B per kit (vox synth or no vox) and decides whether the kits keep a vocal at all. S1c's plan of about 40 title-checked sung lines waits on that answer; the vocal chain stays in the toolkit for earlier sets and for a later decision.
- CLAUDE.md's content rule that vocals come in clean and R-rated versions still holds for any sung vocal. A wordless vox synth has no words, so the rule doesn't apply to it.
- The loops' mix changes for loudness:
  - FUT01: extra soft clips on the kick, bass and clap; ducking on the stabs, pluck and accents. Its lowest kick crest falls about 0.3 dB, to 4.9 dB, still above the 4 dB floor.
  - FUT02: a soft clip on the vox lane, the zaps 4.5 dB down and the bass 1.5 dB up, so new low-end loops replace round 3's. Raising its cap to about 5.1 dB was the alternative, rejected because the round 3 review found 5.6 dB flattened the kicks.
  - FUT04: its 6 dB cap and its declared crest-loss and low-mid exceptions stay; its 125 and 250 Hz octaves now read lower than round 3's.
- Nothing has been checked by ear: every claim is a measurement against the standard.

## Alternatives
- **Ditch the vocals outright:** that is the no-vox club master, offered side by side rather than chosen for the user.
- **Fix the text-to-speech singing:** the pitch-shifted speech is what the user heard as bad. More processing of it keeps the artifacts.
- **A vocoder over speech:** it keeps words and the speech engine, and round 4's robot phrases were part of what the user rejected.
