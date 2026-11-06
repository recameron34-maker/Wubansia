# 0015. Round 4: FUT01 as current UK garage, FUT03 as garage-house

**Status:** Proposed, 2026-11-05. The user asked for kits made faster and closer to today's leading UK garage and house producers, chose "only FUT01 and FUT03", and asked for "interesting accent and phrase synth samples". The producers they named are style references only and, like every artist name, appear in no file.

## Context
Round 3 (ADR 0014) gave all four kits a modern production pass and a shared club master. After it, the user wanted two of them to sound like current records in their genres, and wanted that done faster than rounds 2 and 3, whose review rounds took most of their time. FUT02 and FUT04 stay as round 3.

## Decision
- **FUT01 "Alloy Signal"** stays future garage / 2-step, 134 BPM, F# minor (11A), rebuilt as current UK garage (`--set r4`, the whole kit):
  - drums: a new kick B, Garage (round 1's split design with a short sub for 2-step, a 100-170 Hz knock and a drum-machine click; kick A, Split, stays), a garage clap-snare, metal hats with the open one choked, a tambourine on every swung 16th, and rim ghosts; the alloy hit plays once a loop as the kit's colour;
  - bass: a warp bass, A clean and B warm, in place of the reese and the growls (a filter opening on every note, octave pops, slides and pitch dips), and an organ bass as the option. The 8th-note wobble is on the loop's bass lane: baked into the samples, it failed the alias check;
  - organ and e-piano minor 9 stabs, F#m9 and Bm9, one file per chord;
  - vocal hooks cut from the kit's own title-checked line, no new words: pitched with formants kept, chipmunk, the whole line as a call, two throws and an off-beat riff;
  - a 4x4 speed garage variant of the context loop beside the 2-step one;
  - swing 0.59, up from 0.57. The plan keeps one swing per kit (§2.3), so every swung item re-renders with it.
- **FUT03 "Glass Current"** moves from hyper-clean future house to garage-house / deep tech house, still 128 BPM in G minor (6A): a deep round kick, a clap and snare in a short room, shuffled hats and a shaker, congas, bongos and a rim with swung ghosts, a rubber bass over a deep sub, a dub Gm9 e-piano stab with its echo, a warm pad bed and a filtered, chopped hook from the kit's own line. Swing 0.56, up from 0.53.
- **A fifth direction, "garage-house / deep tech house"**, in `kits.DIRECTIONS`, with club windows of its own in `qa.CLUB`: conventions like the others, warmer and less bright than future house (LUFS-I −8.5 ± 1, PLR ≥ 7.0; the octaves are in `docs/plans/S1-modern-sound.md`). Its extended mix plays the house form. Future house's windows and form stay for any kit that takes that direction up again.
- **Synth accents and phrases in both kits.** Sound 13 is nine accents per kit, each on a stated root in the key. Sound 14 is four phrase loops per kit, 2 or 4 bars, on new multisamples where they need one (FUT01's organ lead, FUT03's keys EP). The phrases are also MIDI, each on channel 4 in a file of its own (`PARTS['phrases']`); FUT03's keys riff is its lead file.
- **A chord sample with one file per chord gets one trigger note per chord.** `PARTS['chords']['trigger']`, with the `zones` its files are rooted at, plays each stab on its chord's file: FUT01's organ stab plays F#3 for F#m9 and B3 for Bm9. The one-file trigger on C3 would have played F#m9 throughout. FUT01's e-piano stab phrase triggers its files the same way.
- **The club masters.** FUT01's runs at the standard's 4 dB cap with no exception: −9.75 LUFS-I, PLR 8.7 dB. FUT03's cap is raised to 4.5 dB, declared on the item with its reason as ADR 0014 allows: at 4 dB the loop stopped at −9.4 to −9.6 LUFS, on the window's edge. It reaches −8.96 LUFS-I, PLR 7.9 dB, and its claps lose 1.50 dB of crest, at the 1.5 dB limit.
- **The faster process:** one designer per kit, each kit on the listening page as each milestone rendered, no second review round, and one verification pass before the commit (QA with 0 failed checks, `--repro` byte-identical on every item, the full suite).

## Consequences
- Round 4 replaces FUT01's and FUT03's WAVs in `content/samples/` (Git LFS) and their MIDI in `content/midi/`. Round 3's are in git history. Because the swing is one per kit, `--set r3` no longer re-renders their round 3 WAVs byte for byte: round 3 comes back from history, not from HEAD.
- Nobody has listened yet. Every claim is a measurement against the standard. The listening page asks the A/Bs:
  - FUT01: kick A or B, swing 0.59 or 0.57, bass A, B or the organ bass, the organ or the e-piano stab, and the hooks kept or chipmunk;
  - FUT03: bass A or B, and whether the 4.5 dB cap is worth it.
- Reported, not gated:
  - FUT01's top slope, about −1.2 dB per octave, is brighter than the genre's −2.0;
  - FUT03's club loop is narrower than the width convention at 2 kHz and 8-16 kHz.
- The generator's styles (S1b) still follow round 3's kits. Retuning them toward round 4 is a step of its own, with its snapshot change in its own commit.
- CLAUDE.md's product line still names the four directions with future house. The change to garage-house is proposed in the PR and waits for the user's approval.

## Alternatives
- **All four kits:** the user chose FUT01 and FUT03.
- **FUT03 kept as future house, with garage-house as a fifth kit:** more to build before a listen, and not what the user asked for.
- **A swing per render set**, so `--set r3` still reproduces round 3: it breaks the plan's one swing per kit, which keeps hits that share a step together, and git history already keeps round 3.
