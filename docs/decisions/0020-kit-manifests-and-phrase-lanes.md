# 0020. Kit manifests and phrase lanes: the lanes the recipes synthesize play their recorded stems

**Status:** Proposed, 2026-11-05 (draft, for the user's approval; the derived stems approved by the lead for the
build).

## Context
Plugin v0.1 (ADR 0017) plays each round 5 kit's committed MIDI (MIDI v2, ADR 0019) through a C++ sampler and the
recipes' mix. The kits are data: each is two integer-only manifests, `content/plugin/<kit>/<KIT>_kit.json` and
`<KIT>_mix.json`, embedded with the MIDI (`docs/plugin/manifest.md`). Nothing describing the samples was in git: the
layer velocities and roots lived only in the recipes' Items, the loop points only in `build/<kit>-r5/qa_report.json`,
which isn't in git, and the WAV names drop both.

Most lanes of the round 5 loops are samples on the 16th grid, which a sampler plays exactly: drums, stabs, pads,
accents, FX, FUT01's bass, FUT04's vox. But some lanes were synthesized at render time and exist only as rendered
audio: FUT02-04's bass lines (mono synths with filter envelopes, accents and glides), FUT01/02/04's leads (synthesized
at exact pitches) and FUT01-03's vox riffs (sung note by note). The same notes on the multisamples sound different,
and these lanes are the heart of each loop.

## Decision
- **Each kit's `kit.json` is exported** by `tools/sound/plugin_kit.py` from the r5 Items, the round 5 report, the
  WAVs, MIDI v2's PARTS (which Item each channel and key plays) and the phrase tables. Every playing rule (mode,
  envelopes, velocity law, interpolator, glide, chokes, pans, pre-roll) is the rule of the recipe that plays that
  instrument, cited where it is set. The exporter is deterministic and integer-only, and every string passes the
  artist and slur denylists. Because `build/` isn't in git, **the committed manifest is the source of truth** for each
  file's frames, channels, SHA-256 and loop points; the exporter re-reads them where the WAVs and the report are here.
  Every kit-specific fact (the phrase specs, the derived stems' renders, the playing rules' recipe constants, the lane
  labels, the mix-channel map) lives in the kit's own package, `tools/sound/recipes/<kit>/plugin.py`, read through the
  kit's `RECIPES` as `midi.py` reads its PARTS: shared code never imports a kit (ADR 0008).
- **Phrase lanes.** Those lanes play their recorded stem, sliced per note, while the host tempo is within 2 % of the
  kit's (`tolerance_pm` 20); otherwise, and always from MIDI in, they play the multisample. A state-only switch turns
  them off. The notes stay the real notes, so the MIDI stays right for any synth.
  - FUT01: the lead (Chrome Pluck Phrase) and the vox (Vox Synth Riff). FUT02: the bass (the Orbit line, and the Relay
    line beside it), the lead (Chrome Arp Phrase) and the vox. FUT03: the bass (the Rubber line, and the Deep line
    beside it) and the vox. FUT04: the bass (Acid Line 1 Ride, and Acid Lines 1 and 2 at rest beside it) and the lead
    (Mod Blip Phrase).
  - **Loops/ files first.** A stem is the lane exactly as the recipe renders it. Where the loop plays a Loops/ file
    as written, the table uses it.
  - **Derived stems where none fits**, rendered from the recipe at 48 kHz / 24-bit into `content/samples/<kit>/Stems/`
    (Git LFS, their own commit), re-rendered byte for byte by the tests:
    - FUT02's vox: the loop sings its riff with its own seed and sends its throw apart; the Loops/ riff has another
      seed and the throw's echo baked in. 4 bars, 2,126,816 bytes, stored 3 dB down (`gain_mdb` 3000).
    - FUT03's vox: the loop places the riff unwrapped from 16th 32, its throw's echoes ringing into bars 1-2; the
      Loops/ riff wraps them over its own start. 6 bars (its last slice runs 2 bars past the 4-bar cycle), 3,240,044
      bytes.
    - FUT02's and FUT03's riff twice, from 16ths 0 and 32, for the extended mixes' call, riff and answer vox lines,
      which place it there: 2,126,816 bytes (FUT02, wrapped, 3 dB down) and 3,240,044 bytes (FUT03, 6 bars, 1 dB
      down). On its channel the loop's own stem wins the loop's cycles (on FUT03 by the tie rule: the first table).
    - FUT04's Acid Lines 1 and 2 at rest, each 2-bar Loops/ file twice, so every phrase on channel 1 shares the
      Ride's 4-bar cycle: 4 mono files of 1,016,516 bytes.
  - **Tables.** Each phrase lists its stems per A/B side, its cycle (2 or 4 bars from the song's 16th 0) and a slice
    per note, keyed by (16th in the cycle, key). Slices tile the stem, so contiguous slices read on and the lane at the
    kit's tempo is the stem. A slice starts at its onset (`phrase.Grid.at(step)`) less its lead: the note's own
    pre-roll, or, on a bass line that rests on the kicks, the kick before it. A looped stem's first slice starts on
    frame 0 and its lead holds the tail wrapped from the stem's end. A slice with no follower fades from its fade frame
    (the recipe's mute edges: FUT02's glide mute, 5 ms; its arp's rest, 10 ms); a slice that doesn't continue the one
    before comes in over 3 ms. `holds` gives a slice the notes it already plays (FUT04's ratchet retriggers, FUT03's
    in-note glides). `ride` gives a stem with a filter ride baked in (FUT04's Ride) the CC74/CC71 values it was
    rendered with; a cycle plays it only where its own controllers equal them within 1, so an arranged ride plays the
    multisample through the live filter.
  - **The proof.** Placed by its table at the Loop files' notes, each stem nulls against the recipe's lane before its
    inserts at -50 dB or lower, both variants and both bass characters: bit-exact for FUT01's lead and vox, FUT02's
    arp, FUT03's bass and FUT04's bass and lead; -85/-88 dB for FUT02's bass; -117 dB and -89 dB for the derived vox
    stems (-118 and -92 dB for the riff-twice stems against the recipe's riff placed twice). Every cycle of the Loop
    files resolves to slices, and every cycle of the extended mixes but one note (FUT02's call line throws its last
    note on 16th 62, which no stem has).

## Consequences
- The plugin plays the round 5 loops' synthesized lanes as recorded at the kit's tempo, and P6's phrase voice and
  matcher have a Python model (`phrase_tables.play`, `resolve`) and these nulls to be proven against.
- Those lanes change character beyond ±2 % of the kit's tempo, and from MIDI in (the editor shows which source plays).
- The content gains 14.80 MB of derived stems (8 files); the Loops/ stems the tables use are 10.46 MB.
- A stem starts on its cycle's 16th 0, so a note there can't have a pre-roll of its own: FUT02's riff-twice stem is
  wrapped, the first note's 50 ms pre-roll sitting at the stem's end, played when the riff repeats and lost when it
  enters fresh; its last slice fades before it.
- `content/plugin/README.md` lists the extended mixes' cycles that play the multisample on a phrase lane; a test keeps
  the list current when the MIDI is re-arranged.
- Whenever samples, recipes or the MIDI change, the manifests, the phrase tables and the content version are
  regenerated together (`plugin_kit.py --check` and the tests catch a stale one).

## Alternatives
- **Every lane on the multisamples** (design A). Any tempo, but the synthesized lanes would sound unlike the loops.
- **Time-stretched stems at any tempo.** Out of scope for v0.1 (M3).
- **Slices keyed to positions in the song.** Replayed or edited MIDI would no longer drive them; keying by (16th in
  the cycle, key) keeps the notes in charge.
- **Only Loops/ files, no derived stems.** FUT02's and FUT03's vox would play a different riff (another seed, wrapped
  echoes) or the multisample, a gap the fidelity report would show.
