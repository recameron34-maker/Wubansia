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
  pre-roll) is the recipe's own rule, each value in `plugin_kit.py` with the recipe it comes from.
- **Files:** each take's WAV (`listen.plain`): frames, channels and SHA-256 read off the WAV, loop points off
  `build/<kit>-r5/qa_report.json`.
- **Phrase tables:** `tools/sound/phrase_tables.py` (ADR 0020).
- **Mix channels:** P3's channel ids, one table in `plugin_kit.py` (`MIX_CHANNELS`, `MIX`).

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
| FUT01 | 98,168 bytes | 33 | 30 | 188 | 66.7 MB | 2 (20) |
| FUT02 | 91,402 bytes | 31 | 29 | 137 | 51.1 MB | 4 (81) |
| FUT03 | 91,207 bytes | 31 | 32 | 163 | 36.9 MB | 3 (44) |
| FUT04 | 99,786 bytes | 28 | 27 | 130 | 43.5 MB | 4 (130) |

A/B switches: the kick on FUT01 only (A Kick Split, B Kick Garage: the loops play B), the bass on all four (A clean,
B warm: the wet and club loops play B), the stab on FUT01 only (A Organ Stab, the loops'; B E Piano Stab). FUT02's
Drop Glide (channel 11) follows the bass switch. FUT03's Sub Deep is layered under both Rubber Basses, 11 dB down,
where its sine meets the Rubber Bass's own sub (measured under 120 Hz on the hard layers' loops).

## Phrase lanes

The lanes the recipes synthesize at render time play their recorded stem, sliced per note, while the host tempo is
within 2 % of the kit's (ADR 0020). Every other time, and always from MIDI in, they play the multisample.

| Kit | Ch | Lane | Stem (A / B) | Cycle | Synthesized by | Null vs the recipe's lane |
|---|---|---|---|---|---|---|
| FUT01 | 2 | lead | `Loops/Chrome Pluck Phrase.wav` | 2 bars | `fut01.music.pluck_phrase` | bit-exact |
| FUT01 | 5 | vox | `Loops/Vox Synth Riff.wav` | 2 bars | `fut01.voxsynth.riff` | bit-exact |
| FUT02 | 1 | bass | `Loops/Orbit Line Clean.wav` / `Warm` | 2 bars | `fut02.bass.line_lane` | -85 / -88 dB |
| FUT02 | 1 | bass | `Loops/Relay Line Clean.wav` / `Warm` | 2 bars | `fut02.bass.line_lane` | (not in the loop) |
| FUT02 | 2 | lead | `Loops/Chrome Arp Phrase.wav` | 2 bars | `fut02.lead.arp_phrase` | bit-exact |
| FUT02 | 5 | vox | `Stems/Context Vox Riff.wav` (derived) | 4 bars | `fut02.voxsynth.sung_parts` | -117 dB |
| FUT03 | 1 | bass | `Loops/Line Rubber Clean.wav` / `Warm` | 2 bars | `fut03.rubber.line_lane` | bit-exact |
| FUT03 | 1 | bass | `Loops/Line Deep Clean.wav` / `Warm` | 2 bars | `fut03.rubber.line_lane` | (not in the loop) |
| FUT03 | 5 | vox | `Stems/Context Vox Riff.wav` (derived) | 4 bars | `fut03.voxsynth.riff_tail` | -89 dB |
| FUT04 | 1 | bass | `Loops/Acid Line 1 Ride Clean.wav` / `Warm` (ride) | 4 bars | `fut04.bass.play_line` | bit-exact |
| FUT04 | 1 | bass | `Stems/Acid Line 1 Clean 4 Bars.wav` / `Warm` (at rest) | 4 bars | `fut04.bass.play_line` | (not in the loop) |
| FUT04 | 1 | bass | `Stems/Acid Line 2 Clean 4 Bars.wav` / `Warm` (at rest) | 4 bars | `fut04.bass.play_line` | (not in the loop) |
| FUT04 | 2 | lead | `Loops/Mod Blip Phrase.wav` | 2 bars | `fut04.lead.blip_phrase` | bit-exact |

The nulls are the stem placed by its table at the Loop files' notes against the recipe's lane before its inserts,
both variants and both bass characters, round the loop (`tools/sound/tests/test_phrase_tables.py`; the gate is -50 dB).
FUT02's bass: the recipe mutes its line under the drop glide with 5 ms edges, the phrase voice comes back after it over
its 3 ms crossfade, both in the line's silence on the kick. FUT02's vox: 24 bits at 3 dB under the lane. FUT03's vox:
the 3 ms crossfade on the riff's first breath.

How the tables tie stems to notes:
- **Slices tile the stem.** A slice runs from its note's onset less its lead to where the next starts, so contiguous
  slices read on and the lane at the kit's tempo is the stem. A looped stem's first slice starts on frame 0: its lead
  holds the tail the stem wraps round (FUT01's riff: the throw's echoes, 0.67 s).
- **Bass lines rest on the kicks**, so their slices break on the kicks: one played alone starts and ends in silence.
- **Fades:** a slice with no follower fades from its fade frame over the phrase's `fade_us`: the recipe's mute edges
  (FUT02's bass 5 ms before its glide mute on the kick, FUT02's arp 10 ms into its rest in the fill bar).
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
| `fut04/Stems/Acid Line 1 Clean 4 Bars.wav` | Acid Line 1 (A) at rest, its 2-bar Loops/ file twice | a phrase for the line without the ride, on the Ride's 4-bar cycle | 1,016,516 bytes |
| `fut04/Stems/Acid Line 1 Warm 4 Bars.wav` | the same, B | | 1,016,516 bytes |
| `fut04/Stems/Acid Line 2 Clean 4 Bars.wav` | Acid Line 2 (A) at rest, twice | the other line at rest | 1,016,516 bytes |
| `fut04/Stems/Acid Line 2 Warm 4 Bars.wav` | the same, B | | 1,016,516 bytes |

Together 9.43 MB; the Loops/ stems the tables use are another 10.46 MB.

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
FUT02 vox ch 5: 12 cycles (161 notes) from bars 57, 61, 65, 69, 73, 77, 145, 149, 153, 157, 177, 181
FUT02 novox ch 1: every cycle resolves
FUT02 novox ch 2: every cycle resolves
FUT02 novox ch 5: every cycle resolves
FUT03 vox ch 1: every cycle resolves
FUT03 vox ch 5: 12 cycles (80 notes) from bars 57, 61, 65, 69, 73, 77, 145, 149, 153, 157, 177, 181
FUT03 novox ch 1: every cycle resolves
FUT03 novox ch 5: every cycle resolves
FUT04 vox ch 1: every cycle resolves
FUT04 vox ch 2: every cycle resolves
FUT04 novox ch 1: every cycle resolves
FUT04 novox ch 2: every cycle resolves
```

FUT02's and FUT03's are the extended mix's call, riff and answer vox lines, which place the riff from 16th 0 and 32
(the loop's pattern starts at 16 on FUT02, 32 on FUT03). A second derived stem per kit (the riff at 16ths 0 and 32,
about 2-3 MB) would play them recorded too; its first note would lose its pre-roll (6 ms on FUT02, 25 ms on FUT03),
since a stem starts on its cycle's 16th 0. Not made: for the lead to decide. When the kit engineers re-arrange the
extended mixes, run `--report` again and update this list (`test_plugin_kit` checks it).

## Mix channels

The channel ids `kit.json` names, P3's (`MIX_CHANNELS` in `plugin_kit.py`):

| Kit | Channels |
|---|---|
| FUT01 | kick, bass, bass_organ, clap, rim, hat_c, hat_o, tamb, perc, stab, pluck, fill, vsynth, acc_rev, acc_up, acc_hit, acc_warp |
| FUT02 | kick, clap, hat, zap, bass, glide, pad, arp, fill, vox, acc_laser, acc_glass, acc_sweep, acc_revchord, acc_arpstab |
| FUT03 | kick, clap, rim, hat, shaker, perc, bass, stab, pad, vox, accent, lead, fx |
| FUT04 | kick, rumble, bass, bass_stem, snare, hat, ride, perc, fill, pad, lead, vox, accent |

Instruments with no lane of their own in the loops take their lane's nearest channel: FUT01's Organ Lead `pluck`, its
vox chops `vsynth`, its FX keys `fill`, its other accents `acc_hit`; FUT02's Laser Chord Stab `pad`, its other accents
`acc_arpstab` (the uplifter `acc_sweep`); FUT04's Metal Chord `pad`, its Acid and Rave Stabs `accent`.

## Not verified

- Nothing here has been heard: the manifests were checked against the recipes, the WAVs and the MIDI, not by ear.
- FUT01's open hat: the recipe fades it out over the 5 ms before the closed hat (`context4.choked_lane`); a choke can
  only fade after the closed hat's note-on.
- FUT01's no-vox warp stab accent: the recipe's 15 ms fade ends on its gate (1.75 16ths); a gated voice's release
  starts there.
- The multisample fallbacks' release times and glides where no recipe plays that multisample (the vox synths, FUT02's
  arp, FUT03's Keys EP, FUT02-04's basses), and Sub Deep's layer level: chosen from the nearest recipe, in
  `plugin_kit.py`.
- FUT01's vox riff entering fresh in the extended mix plays its first slice's lead, which holds the throw's echoes
  wrapped from the riff's end (0.67 s before the first chop).
- FUT02's vox throw: its slice plays with the throw's CC91/CC94 sends, and so do the tails of the notes before it that
  ring under it; the loop sends only the throw.
