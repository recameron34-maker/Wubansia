# Plugin manifests: `kit.json` and `mix.json`

Each kit the plugin plays is described by two files:

| File | Written by | What it holds |
|---|---|---|
| `content/plugin/<kit>/<KIT>_kit.json` | `tools/sound/plugin_kit.py` (P2) | the kit, its sample files, instruments, keymap, extras and phrase tables |
| `content/plugin/<kit>/<KIT>_mix.json` | `tools/sound/plugin_mix.py` (P3) | the mix: channels and inserts, groups, returns, bus EQ, the club master, reference levels |

The plugin embeds both (with the kit's MIDI) and parses them with `parseKitManifest` and `parseMixManifest`
(`engine/kit/manifest.h`, P4), which fill the structs in `engine/kit/manifest.h` and `engine/mix/mix_config.h`.
The engine knows no kit by name: everything kit-specific is in these two files. The synthetic test kit
(`engine/testing/synthetic_kit.h`) has a pair of each, generated from its structs.

This document is the contract between the exporters and the engine. A change to it goes through its owner (P0) and
lands in the same commit in this file, the C++ structs, `tools/sound/plugin_common.py` and the test-side writer
(`engine/testing/manifest_json.cpp`).

## 1. Rules for both files

**Text.** UTF-8 without a byte order mark, LF line endings, a final newline.

**JSON.** Strict JSON as `core/gen/json.h` reads it:
- Numbers are integers that fit int64: no fraction, exponent, `-0` or leading zero.
- No key appears twice in an object. There is no `null`.
- `true` and `false` appear only where a field is a flag.

**Keys.** Every object has exactly the keys its table lists, in that order. Writers emit that order. Parsers
refuse a missing key or an unknown one, but don't depend on the order. The one optional key is `when`, on an
insert.

**Strings.** Non-empty unless a table says otherwise.
- An *id* is `[a-z0-9_]`, 1-32 characters.
- A *name* or *label* is 1-64 characters, with no control characters.
- Every string in both files passes `tools/sound/denylist.py`'s `artists` and `slurs` lists. Artist names are
  style references only and never appear in names (CLAUDE.md).

**Units.** A field's unit is in its name, and its value is an integer:

| Suffix | Unit | Example |
|---|---|---|
| `_mdb` | milli-decibels (dB × 1000; dBFS × 1000 for levels) | −2.5 dB is `-2500` |
| `_pm` | per mille (× 1000) | swing 0.59 is `590`, Q 0.707 is `707` |
| `_ppm` | parts per million | 0.0004 is `400` |
| `_hz` | hertz | `450` |
| `_mhz` | millihertz | 46.25 Hz is `46250` |
| `_us` | microseconds | 30 ms is `30000` |
| `_ms` | milliseconds | 1.1 s is `1100` |
| `_frames` | sample frames at 48 kHz | 20 ms is `960` |
| `_mlu` | milli-LU (LUFS × 1000) | −9.73 LUFS is `-9730` |
| `_mrad` | milliradians | π is `3142` |
| `_16ths` | 16th notes | 4 bars is `64` |

Exporters convert from floats with `plugin_common.iround`: round half up, `floor(x × scale + 0.5)`.

**Notes and channels.** MIDI keys are 0-127 with C4 = 60. MIDI channels are 1-16 in the files, as DAWs count them
(`tools/sound/channels.py`); the engine stores them 0-15.

**References.**
- Inside `kit.json`, a file is named by its index in `files` (from 0) and an instrument by its `id`.
- `kit.json` names `mix.json` channels by their `id`.
- The parsers resolve every reference and refuse a dangling one.

**Canonical layout.** So that a second export is byte-identical:
- Indent by 2 spaces per level.
- An object puts each member on its own line, as `"key": value`.
- An array whose elements are all integers, strings or flags stays on one line: `[1, 2, 3]`.
- Any other array puts each element on its own line.
- Empty containers are `{}` and `[]`.
- Strings escape only `"`, `\` and control characters (`\n`, `\t`, `\uXXXX`). Other characters stay UTF-8.

`plugin_common.dumps` writes this layout.

**Errors.** A parser's error is one line naming the file kind and the JSON path, for example
`kit.json: instruments[3].zones[0].layers[1].rr[0]: file 412 doesn't exist (412 files)`.

## 2. `<KIT>_kit.json`

The top level is an object:

| Key | Type | Meaning |
|---|---|---|
| `kit` | object | the kit (2.1) |
| `files` | array of objects | every sample file the kit plays (2.2) |
| `instruments` | array of objects | the playable instruments (2.3) |
| `keymap` | array of objects | which instrument each (channel, key) plays (2.4) |
| `extras` | array of objects | the kit's instruments on channels 11-16 (2.5) |
| `phrases` | array of objects | the phrase lanes' recorded lines (2.6) |

### 2.1 `kit`

| Key | Type | Range | Meaning |
|---|---|---|---|
| `id` | string | `[A-Z]{3}[0-9]{2}` | `"FUT01"`; the folder is its lowercase, `fut01` |
| `title` | string | name | `"Alloy Signal"` |
| `direction` | string | name | `"future garage / 2-step"` |
| `bpm` | integer | 128-140 | the kit's tempo; every rendered loop and stem is at it |
| `key` | string | `<note> minor` or `<note> major` | `"F# minor"`; `<note>` is A-G, then `#` or `b` or nothing |
| `camelot` | string | 1-12, then `A` or `B` | `"11A"` |
| `swing_pm` | integer | 500-750 | where the second 16th of each 8th lands, as a share of the 8th (0.59 is `590`) |
| `content_version` | integer | 1-999 | the sample set's version: the plugin looks in `<kit>/v<N>/` first |
| `lanes` | object | | the label each lane has in this kit (below) |
| `ab` | object | | the A/B switches (below) |

`lanes` has exactly the ten lane ids as keys, in the permanent lane order (`engine/kit/kit.h`, `Lane`): `kick`,
`snare`, `hats`, `perc`, `bass`, `lead`, `chords`, `vox`, `accents`, `fx`. Each value is the lane's label in
this kit (`"Clap"` for FUT01's snare lane), or `""` where the kit has nothing on that lane.

`ab` has the keys `kick`, `bass` and `stab`, in that order. Each is an object `{"a": label, "b": label}`. Both
labels are `""` where the kit has no such switch, otherwise both are names. On FUT01, for example:
`"kick": {"a": "Kick Split", "b": "Kick Garage"}`.

### 2.2 `files[]`

| Key | Type | Range | Meaning |
|---|---|---|---|
| `path` | string | relative path | from the kit's folder, `/`-separated, ending `.wav`: `"Drums/Kick Garage v1 rr1.wav"`. A relative path with no `..` or `.` part, no leading `/`, no empty part, and no `\`, `:` or control character, so it can't resolve outside the kit folder (on Windows, `"C:/x.wav"` or `"..\..\x.wav"` would). P4's parser and `FileSampleSource` share the rule (`contentPathProblem`, `engine/kit/file_sample_source.h`). |
| `frames` | integer | 1-28 800 000 | the file's length in frames (48 kHz) |
| `channels` | integer | 1 or 2 | mono files stay mono |
| `sha256` | string | 64 lowercase hex digits | of the whole WAV file. The installer checks it; loads don't. |
| `loop_start` | integer | 0 to `frames` − 1 | the sustain loop's first frame |
| `loop_end` | integer | 0, or `loop_start` + 1 to `frames` | the frame after the loop's last; `0` (with `loop_start` 0) means no loop |
| `preroll_frames` | integer | 0-48 000 | frames from the file's start to its musical onset (a vox's breath, a chop's consonant). The sample starts this much before its note. |

Every file is 48 kHz WAV: 16-bit or 24-bit PCM, or 32-bit float. Paths are unique. Every file is played by
at least one instrument or phrase. Every file one instrument plays has the same `preroll_frames`.

### 2.3 `instruments[]`

| Key | Type | Range | Meaning |
|---|---|---|---|
| `id` | string | id, unique | `"kick_garage"` |
| `name` | string | name | `"Kick Garage"`, from the recipe's item |
| `lane` | string | lane id | the lane it plays on: its hit LED, its duck keys. It must be the lane of its mix channel. |
| `mix_channel` | string | a `mix.json` channel id | where its voices play |
| `mode` | string | `one_shot`, `gated`, `sustain_loop` | `one_shot` plays to the end and ignores note-off. `gated` releases at note-off. `sustain_loop` wraps its files' loops while held, then releases. Every file of a `sustain_loop` instrument has a loop. |
| `align` | string | `start`, `end` | `end`: the sample's end lands on its note's end at the current tempo (risers, fills, reverse swells) |
| `attack_us` | integer | 0-10 000 000 | a raised-cosine fade-in at the voice's start (`parts.raised`, `dsp48.fade`) |
| `release_us` | integer | 0-10 000 000 | the raised-cosine release after note-off (`gated`, `sustain_loop`) |
| `gain_mdb` | integer | −60 000 to 24 000 | the voice's own gain, before its mix channel. Use it for per-sound levels where several sounds share a channel (accents). |
| `start_frames` | integer | 0 to the shortest of its files' `frames` − 1 | playback starts this far into each of its files; `attack_us` fades it in (FUT01's Glitch Fill Lift: frame 21 493, with a 2 ms attack) |
| `velocity_law` | string | `layer_only`, `pow15` | `layer_only`: the layer carries the dynamics (drums, perc). `pow15`: × (velocity / layer `vel`)^1.5 (pitched parts). |
| `interp` | string | `sinc`, `hermite` | the resampler: the polyphase sinc, or 4-point Hermite where the recipe used it (`fut01/basslines.hermite`) |
| `mono` | flag | | one voice at a time, legato: a note while one is held moves that voice to the new key |
| `glide` | object | | `{"curve", "us", "dip_us"}` (below) |
| `restrike_continues` | flag | | struck again on its own end tick, the voice continues instead of re-attacking (held pads across loop wraps) |
| `choke` | object | | `{"group", "role", "fade_us", "curve"}` (below) |
| `duck_key` | flag | | its notes key the onset ducks keyed on its lane (`mix.json`). Kicks and claps set it; FUT04's rumble, on the kick lane, doesn't. |
| `pan_cycle_pm` | array of integers | 0-16 values, each −1000 to 1000 | per-hit pans, stepped by the note's pan index (`[]`: centre). Equal power, unity in the centre (`phrase.pan`). |
| `rr_block_16ths` | integer | 1-4096 | round robins and pan indices count this instrument's hits within blocks this long, from the start of the song or loop (64: every 4 bars), the way `phrase.hits_lane` counts |
| `layered_with` | string | an instrument id, or `""` | another instrument that also plays every note this one plays on the multisample (FUT03's Sub Deep under Rubber Bass). Not itself, and not one that is itself layered. |
| `zones` | array of objects | 1-32 | `{"root", "lo", "hi", "layers"}` (below) |

`glide`:

| Key | Type | Range | Meaning |
|---|---|---|---|
| `curve` | string | `none`, `raised_cosine`, `exponential`, `linear` | `none`, with `us` 0, means no glide. The pitch moves in log frequency (semitones): along a raised cosine, an exponential approach, or a straight line. FUT01 and FUT04: `raised_cosine`; FUT02: `exponential`. |
| `us` | integer | 0-2 000 000 | a slide's length (FUT01 70 000, FUT04 50 000), or the exponential's time constant (FUT02 18 000) |
| `dip_us` | integer | 0-2 000 000 | `raised_cosine` only (else 0): a dip's length (FUT01: 30 000); 0 when the instrument has no dips |

A glide needs `mono`. A legato note (one that starts while the voice still holds the last) glides instead of
starting a voice, and keeps the first note's zone and layer. It is a *dip* when the next legato note returns to the
pitch before it (FUT01's pitch dips): the dip and that returning note glide over `dip_us` (the timeline marks both,
`NoteFlag::dip`; live MIDI never does). Every other legato note is a *slide*, over `us`.

`choke`:

| Key | Type | Range | Meaning |
|---|---|---|---|
| `group` | string | id, or `""` | instruments with the same group choke each other. `group` is `""` exactly when `role` is `none` (both ways): `""` is no group |
| `role` | string | `none`, `chokes`, `choked`, `both` | a `chokes` (or `both`) note-on fades every sounding voice of the group's `choked` (or `both`) instruments |
| `fade_us` | integer | 0-1 000 000 | how fast this instrument's voices fade when choked (FUT01 5000, FUT02-04 4000) |
| `curve` | string | `linear`, `cos2` | the choke fade's shape (FUT01 `linear`, FUT02-04 `cos2`) |

`zones[]`:

| Key | Type | Range | Meaning |
|---|---|---|---|
| `root` | integer | 0-127 | the key its files sound at |
| `lo` | integer | 0-127 | the lowest key the zone plays |
| `hi` | integer | `lo`-127 | the highest |
| `layers` | array of objects | 1-8 | `{"vel", "rr"}`, by `vel` ascending |

- Zones are sorted by `root`, and their `lo`..`hi` ranges are disjoint and ascending.
- A key plays the zone whose range holds it, repitched by (key − root) semitones.
- The exporter sets the ranges so each key gets the recipe's zone: the highest root at or below the key. Below
  the lowest root, the lowest zone extends down (FUT03's F1 on the G1 zone).

`layers[]`:

| Key | Type | Range | Meaning |
|---|---|---|---|
| `vel` | integer | 1-127, strictly ascending | `round(layer × 127)`: the velocity this layer was made for. A note plays the layer whose `vel` is nearest its velocity; a tie goes to the louder layer. |
| `rr` | array of integers | 1-8 file indices | the round robins, in turn by the note's round-robin index |

All the files of one zone have the same number of channels.

### 2.4 `keymap[]`

| Key | Type | Range | Meaning |
|---|---|---|---|
| `ch` | integer | 1-16 | the MIDI channel (`channels.py`) |
| `keys` | array of 2 integers | `[lo, hi]`, 0 ≤ lo ≤ hi ≤ 127 | the keys, both ends included |
| `a` | string | an instrument id | what plays: the A side when there is a switch |
| `b` | string | an instrument id, or `""` | the B side, or none |
| `ab` | string | `kick`, `bass`, `stab`, or `""` | the switch choosing between `a` and `b`; `""` exactly when `b` is `""`, and only a switch the kit has (its `kit.ab` labels aren't empty) |

- Ranges on one channel don't overlap.
- Channel 3 (the voicing) and channel 9 (reserved) have no entries: Wubansia ignores them.
- Channel 10's keys are within 35-59.
- Every instrument appears in at least one entry, or is another's `layered_with`.
- The phrase lanes fall back to the multisample on their channel: every slice key, and every key in a slice's `holds`,
  has a keymap entry on the phrase's channel.
- A key that no entry covers plays nothing.

### 2.5 `extras[]`

The kit's instruments on channels 11-16, from `channels.py`'s `EXTRAS[kit]`, in channel order:

| Key | Type | Range | Meaning |
|---|---|---|---|
| `ch` | integer | 11-16, unique | its channel |
| `name` | string | name | its track's name in the kit's MIDI files, and its instrument's: `"Organ Bass"` |
| `lane` | string | lane id | the lane its track plays on |

Every keymap channel from 11 up has an extra, and every instrument the keymap names on that channel is on the
extra's lane. A track of the kit's MIDI resolves to a lane by `channels.py`'s `TRACKS`
(`engine/kit/channel_map.h`, `kTracks`), or else by its extra's name (`trackLane`).

### 2.6 `phrases[]`

A phrase lane plays a recorded line (a stem) sliced per note instead of the multisample, while the host tempo is
near the kit's (ADR 0020). At the kit's tempo and 48 kHz it is sample-exact against the recipe's lane over a loop's
first pass. After that the voice re-syncs at each cycle wrap, with a 1 ms crossfade when it is a frame or more off,
because at the exact BPM a cycle isn't a whole number of samples. FUT03's is: 360 000 frames for 4 bars at 128 BPM.

| Key | Type | Range | Meaning |
|---|---|---|---|
| `lane` | string | lane id | the lane it plays on |
| `ch` | integer | 1-16 | the MIDI channel whose notes it covers |
| `ab` | string | `kick`, `bass`, `stab`, or `""` | the switch that chooses the stem; `""` exactly when `stems.b` is −1, and only a switch the kit has (its `kit.ab` labels aren't empty) |
| `mix_channel` | string | a `mix.json` channel id | where the stem plays; its lane is `lane` |
| `gain_mdb` | integer | −60 000 to 24 000 | the stem's own gain, before its mix channel, as instruments have. Every `Loops/` stem has 0; FUT02's derived vox stem is stored 3 dB down and plays with 3000. |
| `throw` | string | `live`, `baked` | `live` (most stems): the engine sends its slice notes' CC91/CC94 throws live, as for any note. `baked`: the stem has its throw in the audio, and the engine sends no live throw for its slice notes. |
| `stems` | object | | `{"a": file, "b": file or -1}`: the stem per A/B side |
| `cycle_16ths` | integer | 16-256, a multiple of 16 | the phrase's length; cycles start every `cycle_16ths` from song 16th 0 |
| `tolerance_pm` | integer | 0-100 | slices play while abs(host BPM ÷ kit BPM − 1) × 1000 ≤ this (20: 2 %) |
| `fade_us` | integer | 0-100 000 | how long a slice with no follower takes to fade from its `fade` frame |
| `fade_in_us` | integer | 0-100 000 | the fade-in when a run starts on a slice whose `lead` is 0, so plucks, arps and blips keep their transients: 500 for most stems; FUT02's arp keeps 3000, its recipe's rest-mask in-edge on the downbeat after its rest. A run that starts in a slice's pre-roll (`lead` > 0) crossfades over 3 ms instead. |
| `ride` | object | | `{"cc74": [...], "cc71": [...]}`: the controller values the stem was rendered with (below) |
| `slices` | array of objects | 1-256 | the phrase's notes, by `step` ascending (below) |

`slices[]`, in the stems' 48 kHz frames:

| Key | Type | Range | Meaning |
|---|---|---|---|
| `step` | integer | 0 to `cycle_16ths` − 1 | the 16th in the cycle where its note starts |
| `key` | integer | 0-127 | its note's key; (`step`, `key`) is unique in a phrase |
| `start` | integer | 0 to the stem's frames − 1 | the first frame: the onset (`phrase.Grid.at(step)`) less `lead`; modulo the stem's frames in a looped stem (below) |
| `end` | integer | `start` + 1 to the shorter stem's frames; in a looped stem, up to `start` + frames | the frame after the last (below, for a wrapping slice) |
| `fade` | integer | `start` to `end` | where it starts fading when no slice follows it (the recipe's mute edge) |
| `lead` | integer | 0-48 000 | frames of the slice before its onset |
| `holds` | array of integers | 0-8 keys, each 0-127 | the notes after the slice's own that it plays itself (below); `[]` for most slices |

`holds` lists, by key and in order, the notes after the slice's own note that the slice plays itself:
- retriggers of its key (FUT04's blip ratchets: step 10 `[74]`, step 26 `[84, 84]`);
- the targets its note glides to inside it (FUT03's in-note glides, written as legato notes: Rubber 14 `[31]`,
  25 `[31]`, 30 `[38]`; Deep 14 `[36]`, 22 `[38]`, 30 `[31]`).

Each held note starts after the slice's note and before the next note the phrase's pattern matches on that channel.
The matcher gives held notes no sound of their own: the stem already has them. A note that fits neither a slice nor
a hold stays on the multisample.

A looped stem is one the phrase plays cycle after cycle, so it holds what wraps round its cycle:
- A looped stem is read modulo its frames. A slice's start is its onset less its lead, modulo the stem's frames.
  Its end and fade may run past the stem's frames, up to start + frames.
- The last slice owns the tail that wraps to the stem's start. It ends where the next cycle's slice 0 starts:
  frames + slice 0's start, or slice 0's start when slice 0 wraps.
- Slice 0 starts at its own pre-roll. Where that is before its cycle's start, it starts at frames + onset − lead and
  wraps through the stem's end (FUT02's riff twice: [352062, 368899), lead 2400).
- A slice continues into the next one (the last into the next cycle's slice 0) when its end equals that slice's
  start, modulo the stem's frames.

`ride` has the keys `cc74` and `cc71`, in that order. Each is an array of either 0 or `cycle_16ths` × 4 integers,
each 0-127.
- **Non-empty:** the values of that controller the stem was rendered with, one per 60 ticks over the cycle
  (ticks 0, 60, 120... from the cycle's start), as the MIDI files write rides. A cycle matches the phrase only where
  its own values on that grid equal these within 1. A cycle's value at a grid tick is the last CC on the phrase's
  channel at or before that tick; 0 before any. A cycle that doesn't match plays the multisample, whose filter
  follows the arranged ride.
- **Empty:** no constraint on that controller. One array may be empty while the other isn't.
- **Both empty:** the stem follows no controller, and the matcher ignores CCs for it.

Only FUT04's Acid Line 1 Ride stems have a ride. Beside them, FUT04's Acid Line 1 and 2 at rest ride 0/0. So the loop's
ride plays the Ride stem, a ride at 0 the at-rest stem, and any other ride the multisample.

Two phrases may share a channel; the matcher picks per cycle (`engine/sched/phrase_match.h`). Phrases that share a
channel share its `cycle_16ths`, so its cycles line up (P2's export checks this).

## 3. `<KIT>_mix.json`

The top level is an object:

| Key | Type | Meaning |
|---|---|---|
| `kit` | string | the kit's `id`, as in `kit.json` |
| `content_version` | integer | as in `kit.json` |
| `channels` | array of objects | 1-24 mix channels (3.1) |
| `groups` | object | `{"drums": {"inserts": [...]}, "music": {"inserts": [...]}}` |
| `returns` | object | `{"space": ..., "echo": ..., "resonator": ...}` (3.3) |
| `bus_eq` | object | `{"vox": [...], "novox": [...]}`: the inserts on the mix bus, per variant |
| `master` | object | the club master (3.4) |
| `reference` | object | `{"vox": ..., "novox": ...}`: what the reference loops measured (3.5) |

### 3.1 `channels[]`

| Key | Type | Range | Meaning |
|---|---|---|---|
| `id` | string | id, unique | `"bass"`, `"acc_hit"`, `"bass_stem"` |
| `lane` | string | lane id | the lane level and mute that apply to it |
| `group` | string | `none`, `drums`, `music` | where it goes after its fader |
| `inserts` | array of objects | 0-16 | in order (3.2) |
| `sends_pm` | array of 3 integers | each 0-1000 | post-fader sends to Space, Echo and Resonator |

The signal flow is fixed:
- **Channel:** the voices sum into its stereo bus, then go through its inserts. Then come the lane's level and
  mute (10 ms smoothing). Then it goes to its group, or to the mix bus for `none`, and post-fader to the sends.
- **Throws:** a thrown voice (CC91 Space, CC94 Echo, latched at its note-on) sends at
  `base + (1 − base) × value / 127`, where `base` is its channel's send level. The thrown part,
  `(1 − base) × value / 127`, goes from the voice straight into the send input, after the channel's static gain (the
  product of its active `gain` inserts): it is sent post-fader, like the base.
- **Groups:** their inserts, then the mix bus.
- **Returns:** their effect, then their inserts, then `return_mdb`, then the macro. Space scales the Space and
  Resonator returns, Echo the Echo return, each 0-200 %. Then the mix bus.
- **Mix bus:** the variant's `bus_eq`, then the variant's club master. A mono output folds to (L + R) / 2.

### 3.2 Inserts

Every insert is an object whose first key is `type`. The type's fields follow, in its table's order. The last
key, if any, is the optional `when`.

`when` makes an insert active only for one variant, one side of an A/B switch, or both (both must hold). Its
keys are exactly `variant`, `ab`, `is`:
- `variant`: `any`, `vox` or `novox`.
- `ab`: `none`, `kick`, `bass` or `stab`.
- `is`: `a` or `b`; it counts only with an `ab` other than `none`.

At least one of `variant` and `ab` restricts. An inactive insert is bypassed; switching crossfades over 10 ms.
Without `when`, an insert is always active.

`band`, inside highpass, lowpass, cc_filter and duck, is `{"lo_hz", "hi_hz"}`, each 0-24 000. `0` leaves that side
open. When both are set, `lo_hz` < `hi_hz`. The filters and the duck apply their band differently:
- **highpass, lowpass and cc_filter: the crossover form.** LR4 crossovers at `lo_hz` and `hi_hz` (each only if set)
  split the signal into the band and the rest. The filter runs on the band, and the rest is added back unfiltered:
  rest + f(band). `{"lo_hz": 0, "hi_hz": 0}` filters the whole signal, with no split.
- **duck: the additive form.** The duck scales the band (an LR4 high-pass at `lo_hz`, then a low-pass at `hi_hz`,
  each only if set) and adds the difference to the signal: x + (g − 1)·band(x). Outside its band, and while it
  rests (g = 1), the signal passes untouched. A band of `{0, 0}` scales the whole signal: x·g. This is how the
  recipes' band ducks work (FUT02's snap and carve, FUT04's sidechain and dips, with a zero-phase band). The
  crossover form would all-pass the whole channel even at rest, which moved the pre-master kick crest by 1-2.9 dB
  and the true peak by up to 0.85 dB on FUT02-04 in P3's measurements.

| `type` | Fields, in order | Ranges | What it is |
|---|---|---|---|
| `gain` | `gain_mdb` | −60 000 to 24 000 | a static gain: faders, and FUT04's LUFS-solved faders |
| `pan` | `pan_pm` | −1000 to 1000 | equal power, unity in the centre (`phrase.pan`) |
| `width` | `width_pm`, `freq_hz`, `order` | 0-4000; 0-24 000; 1-8 | side′ = side + (`width_pm` / 1000 − 1) × HP(side), HP a Butterworth high-pass of `order` at `freq_hz`. `freq_hz` 0 widens the whole side: side × `width_pm` / 1000. |
| `peak` | `freq_hz`, `gain_mdb`, `q_pm` | 10-24 000; −30 000 to 30 000; 100-30 000 | RBJ peaking EQ (`dsp48.peaking`) |
| `low_shelf`, `high_shelf` | `freq_hz`, `gain_mdb`, `q_pm` | as `peak` | RBJ shelves |
| `highpass`, `lowpass` | `freq_hz`, `order`, `q_pm`, `stages`, `band` | 5-24 000; 1-8; 0, or 100-30 000 with `order` 2; 1-2; band | Butterworth of that order (`dsp48.bfilt`); with `q_pm` > 0, the RBJ 2nd-order filter with that Q. `stages`: the filter applied that many times in series. |
| `cc_filter` | `response`, `cc74_lo_hz`, `cc74_hi_hz`, `cc74_default`, `cc71_lo_pm`, `cc71_hi_pm`, `cc71_default`, `band` | `lowpass`/`bandpass`/`highpass`; 10-24 000; above `cc74_lo_hz` to 24 000; 0-127; 100-30 000; 100-30 000; 0-127 | a TPT state-variable filter (`dsp48.svf_tpt`) driven by CC74 and CC71 (below). Channels only, at most one per channel. |
| `mono_below` | `freq_hz`, `order` | 20-2000; 1-8 | the side high-passed at `freq_hz` (`dsp48.mono_below`, order 8) |
| `clip` | `ceiling_mdb`, `knee_mdb` | −40 000 to 0; 100-24 000 | `master.soft_clip`: linear up to `knee_mdb` under an absolute ceiling (dBFS), ADAA, 4x oversampled |
| `drive` | `drive_pm`, `bias_pm`, `output_mdb` | 100-50 000; −2000 to 2000; −40 000 to 24 000 | `dsp48.tanh_adaa` at 4x: (tanh(d·x + b) − tanh(b)) / tanh(d), then `output_mdb` |
| `duck` (onset) | `mode`, `key_lanes`, `min_velocity`, `depth_mdb`, `ramp_us`, `hold_us`, `release_us`, `pre_us`, `band`, `pump` | `"onset"`; 1-10 unique lane ids; 1-127; −24 000 to 60 000, not 0; 0-1 000 000; 0-2 000 000; 1000-4 000 000; 0-100 000; band; flag | `phrase.pump_db` (below) |
| `duck` (level) | `mode`, `key_channel`, `depth_mdb`, `range_mdb`, `top_mdb`, `attack_us`, `release_us`, `band`, `pump` | `"level"`; another channel's id; −24 000 to 60 000, not 0; 1000-60 000; −60 000 to 12 000; 100-1 000 000; 1000-4 000 000; band; flag | `sends.duck` with a fixed top (below) |
| `comp` | `threshold_mdb`, `ratio_pm`, `knee_mdb`, `attack_us`, `release_fast_us`, `release_slow_us`, `makeup_mdb` | −60 000 to 0; 1001-100 000; 0-24 000; 100-1 000 000; 1000-10 000 000; 1000-10 000 000; −24 000 to 24 000 | `master.py`'s glue at a fixed threshold, the same glue the club master runs (FUT03's vocal glue). Stereo-linked RMS over 5 ms on 16-sample blocks, a soft knee, the attack, and an auto release: the larger of the fast release's envelope and a slow one (`release_slow_us`, with a 0.5 s attack). Then `makeup_mdb`. |
| `parallel` | `level_mdb`, `inserts` | −60 000 to 12 000; 1-16 inserts | `inserts` run on a copy of the signal, which is mixed back at `level_mdb` over the dry signal (FUT04's drum-bus grit). They hold no `parallel` and no `cc_filter`. |

**cc_filter.** It listens to CC74 (cutoff) and CC71 (resonance) on every MIDI channel that reaches its channel:
the channels whose instruments or phrases play on it (`Kit::ccMixChannels`). For a CC value v (0-127):

    cutoff(v) = cc74_lo_hz × (cc74_hi_hz / cc74_lo_hz) ^ (v / 127)       Hz
    Q(v)      = (cc71_lo_pm + (cc71_hi_pm − cc71_lo_pm) × v / 127) / 1000

The engine interpolates the CC value linearly between sequenced points that come within a 16th of each other,
then maps it. Until the first CC, and after a reset, the defaults hold. The MIDI writer inverts the cutoff map:
`v = round(127 × ln(f / lo) / ln(hi / lo))`, clamped to 0-127.

**Onset duck.** At each key onset, the gain goes down `depth_mdb` over `ramp_us`, holds `hold_us`, then comes
back over `release_us` on a raised cosine. Overlapping ducks take the deepest. A negative depth lifts instead
(FUT02's clap snap). A key onset is a note of an instrument with `duck_key` set, on a lane in `key_lanes`, with a
velocity of at least `min_velocity`. Keys count whether or not their lane is muted.

`pre_us` starts the ramp that much before the onset: the timeline places the keys early. Every onset duck keyed
on one lane therefore has the same `pre_us`. Live keys from MIDI in duck at their onset.

**Level duck.** The detector measures the key channel's signal, after its inserts and before its lane level, in
64-frame RMS blocks (at 48 kHz). It smooths the level in dB with `attack_us` and `release_us`, then maps
`top_mdb − range_mdb` .. `top_mdb` onto 0 .. `depth_mdb` of reduction. `top_mdb` is the key's loudest block in
the reference loop: a real-time engine can't measure the whole loop first.

`pump` (both ducks): the Pump macro scales `depth_mdb`, 0-200 %.

### 3.3 `returns`

`space` (`sends.space`):

| Key | Type | Range | Meaning |
|---|---|---|---|
| `delays_frames` | 8 integers | each 1-48 000 | the FDN's line lengths at 48 kHz, scaled to the host rate |
| `signs` | 8 integers | each −1 or 1 | the input signs |
| `t60_ms` | integer | 100-30 000 | the decay |
| `t60_low_ms` | integer | 0-30 000 | the decay under `low_hz`; 0 is `t60_ms` |
| `low_hz` | integer | 20-2000 | |
| `damp_hz` | integer | 500-24 000 | the one-pole damping per line |
| `predelay_us` | integer | 0-500 000 | |
| `send_hp_hz` | integer | 0-2000 | the send's high-pass (Butterworth); 0 is none |
| `send_order` | integer | 1-8 | |
| `shimmer_pm` | integer | 0-500 | 0: no shimmer |
| `shimmer_lo_hz` | integer | 20-24 000 | the shimmer's band, below `shimmer_hi_hz` |
| `shimmer_hi_hz` | integer | 20-24 000 | |
| `window_frames` | integer | 64-16 384 | the octave shifter's window at 48 kHz |
| `mono_hz` | integer | 0-1000 | the return is mono under it; 0 is off |
| `return_mdb` | integer | −60 000 to 12 000 | |
| `inserts` | array | 0-16 | e.g. the pump, and the level duck under the vox |

`echo` (`sends.echo`, a ping-pong of `steps` 16ths at the host tempo):

| Key | Type | Range | Meaning |
|---|---|---|---|
| `steps` | integer | 1-16 | 3: a dotted 8th |
| `feedback_pm` | integer | 0-950 | |
| `lp_hz` | integer | 200-24 000 | the 2nd-order low-pass in the feedback |
| `hp_hz` | integer | 10-5000 | the 2nd-order high-pass in the feedback |
| `drive_pm` | integer | 100-10 000 | the feedback's tanh(drive·u) / drive |
| `wow_ppm` | integer | 0-20 000 | the wobble, as a share of the delay |
| `wow_mhz` | integer | 0-20 000 | |
| `flutter_ppm` | integer | 0-20 000 | |
| `flutter_mhz` | integer | 0-50 000 | |
| `phases_mrad` | 2 integers | each 0-6284 | the wow's and the flutter's start phase |
| `return_mdb` | integer | −60 000 to 12 000 | |
| `inserts` | array | 0-16 | |

`resonator` (`sends.resonator`):

| Key | Type | Range | Meaning |
|---|---|---|---|
| `tones_mhz` | integers | 0-24, each 20 000-20 000 000 | one 2-pole resonator per tone; none means no resonator (FUT03) |
| `tau_ms` | integer | 10-10 000 | the time to fall by e |
| `send_hp_hz` | integer | 0-5000 | |
| `return_mdb` | integer | −60 000 to 12 000 | |
| `inserts` | array | 0-16 | |

### 3.4 `master`

`master.py`'s chain, played live: the limiter's hold looks back only, and a causal DC blocker replaces the
zero-phase clean (ADR 0021). Its glue is the `comp` insert's. Its keys, in order:

| Key | Type | Range | Meaning |
|---|---|---|---|
| `lowcut_hz` | integer | 5-200 | Butterworth high-pass |
| `lowcut_order` | integer | 1-8 | |
| `mono_below_hz` | integer | 0-1000 | 0 is off |
| `glue` | object | | `{"ratio_pm", "knee_mdb", "attack_us", "release_fast_us", "release_slow_us"}`: 1001-20 000; 0-24 000; 100-1 000 000; 1000-10 000 000; 1000-10 000 000 |
| `vox` | object | | `{"glue_threshold_mdb", "drive_mdb"}`, solved for the vox club loop: −60 000 to 0; 0-24 000 |
| `novox` | object | | the same, for the no-vox club loop |
| `clip` | object | | `{"knee_mdb", "ceiling_mdb"}`: 100-12 000; −12 000 to 0 |
| `limiter` | object | | `{"ceiling_mdb", "lookahead_us", "hold_us", "release_us"}`: −12 000 to 0; 0-20 000; 0-200 000; 1000-1 000 000 |
| `dc_hz` | integer | 1-20 | the causal DC blocker; below `lowcut_hz` |

### 3.5 `reference`

`{"vox": {...}, "novox": {...}}`, each:

| Key | Type | Range | Meaning |
|---|---|---|---|
| `pre_master_lufs_mlu` | integer | −60 000 to 0 | the club loop's master input, LUFS-I |
| `club_lufs_mlu` | integer | −60 000 to 0 | the club loop, LUFS-I |
| `channel_true_peak_mdb` | integers | one per channel, each −120 000 to 12 000 | each channel's true peak in the reference loop |

The engine doesn't play these; the fidelity checks (P12) compare against them.

### 3.6 What `parseMixManifest` checks against `kit.json`

- `kit` and `content_version` equal `kit.json`'s.
- Every `mix_channel` that `kit.json` names exists.
- Every instrument's and phrase's lane equals its channel's lane.
- At most 24 channels.
- Every level duck's `key_channel` exists and isn't the duck's own channel.
- Onset ducks keyed on the same lane share one `pre_us`.
- Every `reference` array has one value per channel.

## 4. Versions

`content_version` names a sample set. A plugin build embeds the manifests of exactly one version per kit, and the
loader checks the folder's files against them. If samples change, the manifests, the phrase tables and the
content version are regenerated together and ship as content v2, with a plugin build that embeds them.
