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
| FUT01 | 99,638 bytes | 34 | 31 | 189 | 68.3 MB | 2 (20) |
| FUT02 | 96,364 bytes | 31 | 29 | 138 | 53.2 MB | 5 (103) |
| FUT03 | 94,998 bytes | 31 | 32 | 164 | 40.1 MB | 4 (60) |
| FUT04 | 99,978 bytes | 28 | 27 | 130 | 43.5 MB | 4 (130) |

A/B switches: the kick on FUT01 only (A Kick Split, B Kick Garage: the loops play B), the bass on all four (A clean,
B warm: the wet and club loops play B), the stab on FUT01 only (A Organ Stab, the loops'; B E Piano Stab). FUT02's
Drop Glide (channel 11) follows the bass switch. FUT03's Sub Deep is layered under both Rubber Basses, 11 dB down,
where its sine meets the Rubber Bass's own sub (measured under 120 Hz on the hard layers' loops).

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
FUT02 vox ch 5: 1 cycle (1 note) from bar 61
FUT02 novox ch 1: every cycle resolves
FUT02 novox ch 2: every cycle resolves
FUT02 novox ch 5: every cycle resolves
FUT03 vox ch 1: every cycle resolves
FUT03 vox ch 5: every cycle resolves
FUT03 novox ch 1: every cycle resolves
FUT03 novox ch 5: every cycle resolves
FUT04 vox ch 1: every cycle resolves
FUT04 vox ch 2: every cycle resolves
FUT04 novox ch 1: every cycle resolves
FUT04 novox ch 2: every cycle resolves
```

The one left is FUT02's call line in bars 61-64: it throws its last note on 16th 62, where neither of its vox stems
has one, so that note plays the multisample. When the kit engineers re-arrange the extended mixes, run `--report`
again and update this list (`test_plugin_kit` checks it).

## Mix channels

The channel ids `kit.json` names, P3's (`MIX_CHANNELS` in each kit's `plugin.py`):

| Kit | Channels |
|---|---|
| FUT01 | kick, bass, bass_organ, clap, rim, hat_c, hat_o, tamb, perc, stab, pad, pluck, fill, vsynth, acc_rev, acc_up, acc_hit, acc_warp |
| FUT02 | kick, clap, hat, zap, bass, glide, pad, arp, fill, vox, acc_laser, acc_glass, acc_sweep, acc_revchord, acc_arpstab |
| FUT03 | kick, clap, rim, hat, shaker, perc, bass, stab, pad, vox, accent, lead, fx |
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
  arp, FUT03's Keys EP, FUT02-04's basses), and Sub Deep's layer level: chosen from the nearest recipe, in each
  kit's `plugin.py`.
- FUT01's vox riff entering fresh: from slice 0's pre-roll on, the stem still carries the last of the previous cycle's
  throw (it rings 3 s), 36 dB under the chops over the first 0.6 s (RMS, measured on the recipe's riff) and fading.
- FUT02's riff twice entering on the song's 16th 0 loses its first 50 ms breath: there is no cycle before it to start
  in. Everywhere else its slice 0 starts 2400 frames before its cycle.
- FUT02's vox throw: its slice plays with the throw's CC91/CC94 sends, and so do the tails of the notes before it that
  ring under it; the loop sends only the throw.
