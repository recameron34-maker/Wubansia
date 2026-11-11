# Plugin manifests

Each kit the plugin plays is two integer-only manifests here, embedded in the plugin with the kit's MIDI
(`docs/plugin/manifest.md` has every field, its unit and range):

| File | Written by | What it holds |
|---|---|---|
| `<kit>/<KIT>_kit.json` | `tools/sound/plugin_kit.py` | the kit, its sample files, instruments, keymap, extras and phrase tables |
| `<kit>/<KIT>_mix.json` | `tools/sound/plugin_mix.py` | the mix: channels and inserts, groups, returns, bus EQ, the club master |

The samples stay outside the plugin, in a content folder with the repo's layout (`<kit>/Drums/…`, `Bass/`, `Music/`,
`Vox/`, `FX/`, `Loops/`, `Stems/`). A kit's `kit.json` lists exactly the files it plays.

## Exporting

```
python3 tools/sound/plugin_kit.py --kit all             # writes content/plugin/<kit>/<KIT>_kit.json
python3 tools/sound/plugin_kit.py --kit all --check     # compares, writes nothing (exit 1 if stale)
python3 tools/sound/plugin_kit.py --kit all --report    # the extended mixes' multisample cycles (below)
python3 tools/sound/phrase_tables.py --kit FUT02 --render-stems content/samples   # a kit's derived stems
```

Where everything comes from:
- **Instruments:** the kit's r5 Items (`kits.items(..., sets=['r5'])`) as MIDI v2 plays them: `tools/sound/midi.py`'s
  PARTS say which Item each channel and key plays (`docs/midi-map.md`). Layers are `round(layer x 127)`, round robins
  and zones are the Items'. How each instrument plays (mode, envelopes, velocity law, interpolator, glide, chokes, pans,
  pre-roll) is the recipe's own rule, each value in the kit's `plugin.py` with the recipe it comes from.
- **Files:** each take's WAV (`listen.plain`): frames, channels and SHA-256 read off the WAV, loop points off
  `build/<kit>-r5/qa_report.json`.
- **Phrase tables:** `tools/sound/phrase_tables.py` (ADR 0020).
- **Mix channels:** P3's channel ids.

Everything kit-specific (the phrase specs, the derived stems' renders, the recipe constants the playing rules come
from, the lane labels, the mix-channel map) is in the kit's own package, `tools/sound/recipes/<kit>/plugin.py`;
`plugin_kit.py` and `phrase_tables.py` are shared and import no kit (ADR 0008).

`build/` isn't in git and CI has no samples, so **the committed manifests are the source of truth** for every file's
facts: where a WAV or the round 5 report isn't here, the exporter takes the fact from the committed manifest. The
exporter searches `$WUBANSIA_CONTENT_DIR` (a folder of `<kit>/` folders) for the WAVs before the checkout's
`content/samples`, and `$WUBANSIA_BUILD_DIR` for the reports before the checkout's `build/`. On this machine:

```
WUBANSIA_CONTENT_DIR=/home/user/wubansia-s1a/content/samples WUBANSIA_BUILD_DIR=/home/user/wubansia-s1a/build \
    python3 tools/sound/plugin_kit.py --kit all --check
```

The derived stems (`Stems/`) are only in this checkout's `content/samples` (Git LFS), not in that folder: the
exporter falls back to the checkout for them. A content folder for the plugin needs both.

## The kits

| Kit | `kit.json` | Instruments | Keymap entries | Files | WAV listed | Phrase lanes (slices) |
|---|---|---|---|---|---|---|
| FUT01 | 99,646 bytes | 34 | 31 | 189 | 68.3 MB | 2 (20) |
| FUT02 | 96,381 bytes | 31 | 29 | 138 | 53.2 MB | 5 (103) |
| FUT03 | 92,908 bytes | 30 | 32 | 161 | 39.5 MB | 4 (60) |
| FUT04 | 99,981 bytes | 28 | 27 | 130 | 43.5 MB | 4 (130) |

A/B switches: the kick on FUT01 only (A Kick Split, B Kick Garage: the loops play B), the bass on all four (A clean,
B warm: the wet and club loops play B), the stab on FUT01 only (A Organ Stab, the loops'; B E Piano Stab). FUT02's
Drop Glide (channel 11) follows the bass switch. FUT03's Rubber Basses play alone: the Sub Deep layer under them put
2 dB too much under 120 Hz and 3 dB too little over it against the Rubber and Deep lines.

Duck keys (`duck_key`): the kicks and claps key the onset ducks (FUT04's rumble, on the kick lane, doesn't), and so
does FUT02's vox synth: its riff keys the arp's pocket (`context5.ARP_POCKET`, two onset ducks on the vox lane, 100 ms
early, in `FUT02_mix.json`). The timeline makes the duck keys from the notes before it matches phrases, so the riff
keys the pocket whether its notes play the phrase lane's stem or the multisample.

## Phrase lanes

The lanes the recipes synthesize at render time play their recorded stem, sliced per note, while the host tempo is
within 2 % of the kit's (ADR 0020). Every other time, and always from MIDI in, they play the multisample.

| Kit | Ch | Lane | Stem (A / B) | Cycle | Synthesized by | Null vs the recipe's lane: first pass, second |
|---|---|---|---|---|---|---|
| FUT01 | 2 | lead | `Loops/Chrome Pluck Phrase.wav` | 2 bars | `fut01.music.pluck_phrase` | bit-exact, bit-exact |
| FUT01 | 5 | vox | `Loops/Vox Synth Riff.wav` | 2 bars | `fut01.voxsynth.riff` | bit-exact, bit-exact |
| FUT02 | 1 | bass | `Loops/Orbit Line Clean.wav` / `Warm` | 2 bars | `fut02.bass.line_lane` | -85 / -88 dB, the same |
| FUT02 | 1 | bass | `Loops/Relay Line Clean.wav` / `Warm` | 2 bars | `fut02.bass.line_lane` | (not in the loop) |
| FUT02 | 2 | lead | `Loops/Chrome Arp Phrase.wav` | 2 bars | `fut02.lead.arp_phrase` | bit-exact, bit-exact |
| FUT02 | 5 | vox | `Stems/Context Vox Riff.wav` (derived) | 4 bars | `fut02.voxsynth.sung_parts` | -117 dB, -117 dB |
| FUT02 | 5 | vox | `Stems/Context Vox Riff Twice.wav` (derived) | 4 bars | `fut02.voxsynth.sung_parts` | -118 dB, -118 dB (its riffs) |
| FUT03 | 1 | bass | `Loops/Line Rubber Clean.wav` / `Warm` | 2 bars | `fut03.rubber.line_lane` | bit-exact, bit-exact |
| FUT03 | 1 | bass | `Loops/Line Deep Clean.wav` / `Warm` | 2 bars | `fut03.rubber.line_lane` | (not in the loop) |
| FUT03 | 5 | vox | `Stems/Context Vox Riff.wav` (derived) | 4 bars | `fut03.voxsynth.riff_tail` | -125 dB, -89 dB |
| FUT03 | 5 | vox | `Stems/Context Vox Riff Twice.wav` (derived) | 4 bars | `fut03.voxsynth.riff_tail` | -124 dB, -92 dB (its riffs) |
| FUT04 | 1 | bass | `Loops/Acid Line 1 Ride Clean.wav` / `Warm` (ride) | 4 bars | `fut04.bass.play_line` | bit-exact, bit-exact |
| FUT04 | 1 | bass | `Stems/Acid Line 1 Clean 4 Bars.wav` / `Warm` (at rest) | 4 bars | `fut04.bass.play_line` | (not in the loop) |
| FUT04 | 1 | bass | `Stems/Acid Line 2 Clean 4 Bars.wav` / `Warm` (at rest) | 4 bars | `fut04.bass.play_line` | (not in the loop) |
| FUT04 | 2 | lead | `Loops/Mod Blip Phrase.wav` | 2 bars | `fut04.lead.blip_phrase` | bit-exact, bit-exact |

The nulls are the phrase voice's model (`phrase_tables.play`) playing the stem by its table at the Loop files' notes,
pass after pass from silence, against the recipe's lane before its inserts, both variants and both bass characters,
one pass at a time (`tools/sound/tests/test_phrase_tables.py`; the gate is -50 dB). The first pass counts from where
the voice has come in: past its first run's fade-in, and past the 2 bars a FUT03 vox stem's echoes ring into the next
pass, which a fresh entry hasn't got. The second pass counts whole. At the kit's tempo and 48 kHz the engine is exact
against the model over a loop's first pass and re-syncs at each cycle wrap after it (`docs/plugin/manifest.md` 2.6).
The riff-twice stems play the extended mixes' call, riff and answer vox lines, which place the riff (or a bar of it)
from 16ths 0 and 32: their nulls are against the recipe's riff placed there twice.
FUT02's bass: the recipe mutes its line under the drop glide with 5 ms edges, and the phrase voice comes back after
it over 3 ms through the next slice's pre-roll, both in the line's silence on the kick. FUT02's vox: 24 bits at 3 dB
under the lane. FUT03's vox: its second pass has the 3 ms run fade on the riff's first breath.

How the tables tie stems to notes:
- **Slices tile the stem.** A slice runs from its note's onset less its lead to where the next starts, so contiguous
  slices read on and the lane at the kit's tempo is the stem.
- **Looped stems are read round.** A stem the phrase plays cycle after cycle holds its tails wrapped round to its start.
  Its last slice owns that wrapped tail: it runs past the stem's end, read modulo its length, to where the next cycle's
  slice 0 starts. FUT01's riff: the last chop's release and throw, [160906, 203891), slice 0 from its own pre-roll at
  31951; a fresh entry plays no ghost of a last chop, an exit keeps it. Where slice 0's pre-roll starts before its
  cycle, slice 0 starts at the stem's end and wraps: FUT02's riff twice, its first breath, [352062, 368899), lead 2400,
  starts 50 ms before its cycle. Every other looped stem's slice 0 starts on frame 0.
- **Bass lines rest on the kicks**, so their slices break on the kicks: one played alone starts and ends in silence.
- **Fades:** a slice with no follower fades from its fade frame over the phrase's `fade_us`: the recipe's mute edges
  (FUT02's bass 5 ms before its glide mute on the kick, FUT02's arp 10 ms into its rest in the fill bar). A run (a
  slice that doesn't continue the one before) comes in from its first frame: over 3 ms through a pre-roll, and over
  the phrase's `fade_in_us` on an onset (lead 0): 0.5 ms, so plucks and blips keep their transients, but FUT02's
  arp's 3 ms, the recipe's in-edge on the downbeat after its rest.
- **throw:** `baked` where the stem has its throw in the audio (FUT01's riff: its last chop's ping-pong and plate;
  FUT03's riff: its dub echo and dub throw), so the engine sends their notes no live throw; `live` for the rest
  (FUT02's riff stems hold its throw note dry, and the loop sends it).
- **holds:** FUT04's blip ratchets (MIDI v2 retriggers them) and FUT03's glides inside a note (written as legato
  notes) belong to their slice.
- **ride:** FUT04's Ride stem has the loop's acid ride baked in, so its table carries the CC74/CC71 values it was
  rendered with; the at-rest stems carry 0/0. A cycle plays an acid stem only where its controllers equal the stem's;
  an arranged ride plays the multisample through the live filter on `bass`. Every other stem follows no controller
  (FUT02's arp ride is the live filter on `arp`).

### Derived stems

Rendered for the plugin from their recipes, 48 kHz / 24-bit, in Git LFS (`content/samples/<kit>/Stems/`), re-rendered
byte for byte by the tests. Approved by the lead (ADR 0020).

| Stem | What | Why | Size |
|---|---|---|---|
| `fut02/Stems/Context Vox Riff.wav` | the loop's vox riff from 16th 16: `voxsynth.sung_parts` with the loop's seed (`ContextVoxRiff`), the notes and the throw, 4 bars, stored 3 dB down (`gain_mdb` 3000) | the Loops/ `Vox Synth Riff` has its own seed and the throw's echo baked in; the loop sings it with another seed and sends the throw | 2,126,816 bytes |
| `fut03/Stems/Context Vox Riff.wav` | the loop's vox riff from 16th 32: `voxsynth.riff_tail` at `PhraseVoxRiff`'s gain, unwrapped, 6 bars (its throw's echoes run 2 bars past the 4-bar cycle) | the Loops/ `Phrase Vox Riff` wraps the echoes over its own start; the loop rings them into bars 1-2 | 3,240,044 bytes |
| `fut02/Stems/Context Vox Riff Twice.wav` | the same riff sung from 16ths 0 and 32, wrapped round 4 bars, 3 dB down | the extended mix's call, riff and answer vox lines place the riff there; the loop's stem has it from 16th 16 | 2,126,816 bytes |
| `fut03/Stems/Context Vox Riff Twice.wav` | the same riff from 16ths 0 and 32, unwrapped, 6 bars, 1 dB down (`gain_mdb` 1000) | the extended mix's call, riff and answer vox lines; the loop's stem has it from 16th 32 | 3,240,044 bytes |
| `fut04/Stems/Acid Line 1 Clean 4 Bars.wav` | Acid Line 1 (A) at rest, its 2-bar Loops/ file twice | a phrase for the line without the ride, on the Ride's 4-bar cycle | 1,016,516 bytes |
| `fut04/Stems/Acid Line 1 Warm 4 Bars.wav` | the same, B | | 1,016,516 bytes |
| `fut04/Stems/Acid Line 2 Clean 4 Bars.wav` | Acid Line 2 (A) at rest, twice | the other line at rest | 1,016,516 bytes |
| `fut04/Stems/Acid Line 2 Warm 4 Bars.wav` | the same, B | | 1,016,516 bytes |

Together 14.80 MB; the Loops/ stems the tables use are another 10.46 MB.

### The extended mixes' multisample cycles

Every cycle of both Loop files resolves to slices. In the extended mixes, these cycles have notes on a phrase lane that
no table matches, so those notes play the multisample (`plugin_kit.py --report`):

```
FUT01 vox ch 2: every cycle resolves
FUT01 vox ch 5: every cycle resolves
FUT01 novox ch 2: every cycle resolves
FUT01 novox ch 5: every cycle resolves
FUT02 vox ch 1: every cycle resolves
FUT02 vox ch 2: every cycle resolves
FUT02 vox ch 5: 8 cycles (94 notes) from bars 61, 65, 69, 73, 77, 125, 145, 149
FUT02 novox ch 1: every cycle resolves
FUT02 novox ch 2: every cycle resolves
FUT02 novox ch 5: every cycle resolves
FUT03 vox ch 1: 8 cycles (143 notes) from bars 129, 131, 133, 135, 137, 139, 141, 143
FUT03 vox ch 5: every cycle resolves
FUT03 novox ch 1: 8 cycles (143 notes) from bars 129, 131, 133, 135, 137, 139, 141, 143
FUT03 novox ch 5: every cycle resolves
FUT04 vox ch 1: 26 cycles (976 notes) from bars 41, 45, 49, 53, 57, 61, 65, 69, 73, 77, 81, 85, 89, 93, 105, 109, 113, 117, 121, 125, 169, 173, 177, 181, 185, 189
FUT04 vox ch 2: every cycle resolves
FUT04 novox ch 1: 26 cycles (976 notes) from bars 41, 45, 49, 53, 57, 61, 65, 69, 73, 77, 81, 85, 89, 93, 105, 109, 113, 117, 121, 125, 169, 173, 177, 181, 185, 189
FUT04 novox ch 2: every cycle resolves
```

They are the kit arrangements' new lines (`tools/sound/recipes/<kit>/midi.py`):
- FUT02's vox in bars 61-80, 125-128 and 145-152: the re-arranged vox lines place notes neither vox stem has.
- FUT03's bass in bars 129-144, the switch-up Rubber riff: fewer than half of each cycle's notes are a bass table's,
  so the whole cycle plays the multisample (the matcher's majority rule, `docs/plugin/manifest.md` 2.6).
- FUT04's bass in its grooves and drops: the arranged acid rides (CC74/CC71) differ from the Ride stem's baked values
  and the at-rest stems' 0/0, so those cycles play the multisample through the live filter on `bass`; without the
  controllers every one of their notes would match.

When the extended mixes change, run `--report` again and update this list (`test_plugin_kit` checks it).

## The multisample path

Off the kit's tempo (past 2 %), with phrases off, from MIDI in, and in the Song cycles no table matches (FUT04's
arranged acid rides, FUT03's switch-up Rubber riff, FUT02's call lines), a phrase channel's notes play its multisample.
Each of those instruments is set to its stem's level (`gain_mdb`, each kit's `MS_GAIN_MDB`), and where the spectra
differ it plays on a channel with a corrective EQ in front: P3's `<channel>_ms` (the stem's channel, copied, with the
EQ first), or FUT04's `bass`, which only the acid multisamples play. Measured with groovecore render (P12's build),
phrases off against phrases auto, pass 2 of the loop, bass B unless named, each lane after its channel's inserts
(before: the dry taps P12 reported):

| Kit | Lane | Instrument gain, channel | Level before → after | Worst 1/3 octave before → after |
|---|---|---|---|---|
| FUT01 | lead | Chrome Pluck +0.6 dB, `pluck` | -0.6 → -0.0 dB | -4.1 @ 3.2 kHz → -3.7 @ 3.2 kHz |
| FUT01 | vox | Vox Synth +7.55 dB, `vsynth_ms` | -7.6 → -0.2 dB | -10.8 @ 315 Hz → -2.1 @ 250 Hz |
| FUT02 | bass | Roll Bass Warm +4.16 dB, Clean -0.9 dB, `bass_ms` | -2.8 → -0.0 dB (A +0.8 → -0.3) | -5.8 @ 99 Hz → -1.8 @ 99 Hz (A +8.9 @ 158 Hz → -1.9) |
| FUT02 | lead | Chrome Arp +0.13 dB, `arp` | -0.1 → +0.0 dB | -4.6 @ 794 Hz → -4.5 @ 794 Hz |
| FUT02 | vox | Vox Synth +1.85 dB, `vox` | -2.1 → -0.1 dB | -3.7 @ 630 Hz → +2.9 @ 250 Hz |
| FUT03 | bass | Rubber Bass Warm +2.24 dB, Clean +1.73 dB, no Sub Deep, `bass_ms` | +2.1 → -0.0 dB (A +2.6 → -0.7) | +3.0 @ 50 Hz → +2.7 @ 500 Hz (A +12.2 @ 125 Hz → +2.8) |
| FUT03 | vox | Vox Synth +6.12 dB, `vox_ms` | -6.1 → -0.7 dB | -16.9 @ 1.26 kHz → -4.3 @ 630 Hz |
| FUT04 | bass | Acid 0 dB, `bass` (EQ in front of the cc_filter) | +3.0 → +0.1 dB (A +3.3 → +0.1) | +16.3 @ 99 Hz → -6.2 @ 16 kHz (A +26.2 → -6.0) |
| FUT04 | lead | Mod Blip -0.47 dB, `lead` | +0.5 → +0.0 dB | +1.2 @ 10 kHz → -0.9 @ 99 Hz |

FUT04's bass is matched at rest: the acid multisample at CC 0/0 through `bass` against the at-rest Acid Line 1 stems,
-0.9 dB, worst +2.1 dB (A +2.2 dB), with the cc_filter open while no CC has come (P3's `cc74_default` 127). The table's
row plays the loop's ride, where the filter's top shelves off 16 kHz. Not met, and why:
- FUT01's pluck and FUT02's arp (-3.7 and -4.5 dB in one 1/3 octave): each phrase note was excited with its own noise,
  and with one or two harmonics a band at these pitches, the bands swing ±3-4 dB note by note. An EQ fitted to the
  loop's notes would colour every other melody.
- FUT03's vox (-4.3 dB at 630 Hz): its riff is chops through a riding low-pass with a dub echo, not the vox synth; the
  EQ is the one that keeps the club mix within 1.5 dB (a closer lane fit pushed 794 Hz 1.9 dB over in the mix).
- FUT01's no-vox warp stab rings 15 ms past its gate: the recipe's 15 ms fade ends on the gate, and a gated voice's
  release starts at its note-off. A shorter release cuts the stab with a click (+4-5 dB at 125 Hz). Only a release that
  ends on the note-off would match (a contract change: P0, P5).

The club mix 3 % off the kit's tempo, against the at-tempo Club loop (before → after): FUT01 -0.31 → +0.00 LU, -2.9 →
+1.0 dB (worst band, -3 %); FUT02 -1.0 → -0.12 LU, -2.5 → +1.1 dB; FUT03 +0.10 → +0.15 LU, -2.4 → +2.0 dB at 198 Hz
(already +1.6 dB there before: a tempo effect, not a phrase lane); FUT04 +0.20 → +0.06 LU, +5.2 → +1.7 dB at 250 Hz.
At +3 % every kit is within 0.3 LU and 1.5 dB per band.

## Mix channels

The channel ids `kit.json` names, P3's (`MIX_CHANNELS` in each kit's `plugin.py`):

| Kit | Channels |
|---|---|
| FUT01 | kick, bass, bass_organ, clap, rim, hat_c, hat_o, tamb, perc, stab, pad, pluck, fill, vsynth, vsynth_ms, acc_rev, acc_up, acc_hit, acc_warp |
| FUT02 | kick, clap, hat, zap, bass, bass_ms, glide, pad, arp, fill, vox, acc_laser, acc_glass, acc_sweep, acc_revchord, acc_arpstab |
| FUT03 | kick, clap, rim, hat, shaker, perc, bass, bass_ms, stab, pad, vox, vox_ms, accent, lead, fx |
| FUT04 | kick, rumble, bass, bass_stem, snare, hat, ride, perc, fill, pad, lead, vox, accent |

Instruments with no lane of their own in the loops take their lane's nearest channel: FUT01's Organ Lead `pluck`, its
vox chops `vsynth`, its FX keys `fill`, its other accents `acc_hit` (its Air Pad, channel 13, has its own `pad`); FUT02's Laser Chord Stab `pad`, its other accents
`acc_arpstab` (the uplifter `acc_sweep`); FUT04's Metal Chord `pad`, its Acid and Rave Stabs `accent`.

## Not verified

- Nothing here has been heard: the manifests were checked against the recipes, the WAVs and the MIDI, not by ear.
- FUT01's open hat: the recipe fades it out over the 5 ms before the closed hat (`context4.choked_lane`); a choke can
  only fade after the closed hat's note-on.
- FUT01's no-vox warp stab accent: the recipe's 15 ms fade ends on its gate (1.75 16ths); a gated voice's release
  starts there.
- The multisample fallbacks' release times and glides where no recipe plays that multisample (the vox synths, FUT02's
  arp, FUT03's Keys EP, FUT02-04's basses): chosen from the nearest recipe, in each
  kit's `plugin.py`.
- FUT01's vox riff entering fresh: from slice 0's pre-roll on, the stem still carries the last of the previous cycle's
  throw (it rings 3 s), 36 dB under the chops over the first 0.6 s (RMS, measured on the recipe's riff) and fading.
- FUT02's riff twice entering on the song's 16th 0 loses its first 50 ms breath: there is no cycle before it to start
  in. Everywhere else its slice 0 starts 2400 frames before its cycle.
- FUT02's vox throw: its slice plays with the throw's CC91/CC94 sends, and so do the tails of the notes before it that
  ring under it; the loop sends only the throw.
