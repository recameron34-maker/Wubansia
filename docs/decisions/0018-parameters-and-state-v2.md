# 0018. Parameters and state version 2

**Status:** Proposed, 2026-11-05 (draft, for the user's approval). Part of the plugin v0.1 plan (ADR 0017; packages P10, the parameter layout, and P9, the processor). Amends ADR 0002, whose state table (`stateVersion="1"`) and frozen tests change here. Nothing below reaches main or your Mac before you sign it off: the parameter IDs become permanent the moment a project is saved with v0.1.

## Context
M0 has one parameter, `master_gain` (version hint 1), and state version 1: the parameter tree with `stateVersion="1"`. v0.1 plays four kits with modes, A/B switches, ten lanes, three macros and a club master, all of which a DAW should automate and save; and a few settings that belong to the project but not to automation: which kit, which content version it was made with, the Standalone app's tempo, how MIDI in is routed, and whether phrase lanes play their recordings.

Hosts are strict about both. Parameter IDs and version hints are stored in projects and automation lanes (ADR 0002); pluginval at strictness 10 checks that `getStateInformation` right after `setStateInformation` returns the same bytes; and some hosts save and load state off the message thread, while the audio thread plays.

## Decision

### Parameters
`master_gain` stays exactly as M0 shipped it (hint 1, -60 to +6 dB, default 0 dB, JUCE's own text). 29 parameters are appended after it, all with version hint 2, in this order (`plugin/source/ParameterIds.h`, `Parameters.cpp`):

| ID | Name | Range | Default |
|---|---|---|---|
| `play_mode` | Play Mode | Off, Loop, Song | Loop |
| `vox_synth` | Vox Synth | off, on | on |
| `kick_ab`, `bass_ab`, `stab_ab` | Kick A/B, Bass A/B, Stab A/B | A, B | B, B, A (what the club loops play) |
| `<lane>_level` for kick, snare, hats, perc, bass, lead, chords, vox, accents, fx | Kick Level ... FX Level | -60 to +6 dB (-60 is silence) | 0 dB |
| `<lane>_mute`, each after its level | Kick Mute ... FX Mute | off, on | off |
| `space`, `echo`, `pump` | Space, Echo, Pump | 0-200 % | 100 % |
| `club_master` | Club Master | off, on | on |

The lane IDs follow the engine's permanent lane order (`engine/kit/kit.h`), and the A/B IDs its switches; a compile-time check keeps them together. Names and labels may change later; IDs, ranges and defaults may not. The kit is not a parameter (below): a kit change loads samples for up to half a second and can fail, which automation shouldn't do mid-song.

### State version 2
The state stays JUCE's binary XML with the root `WubansiaState`:

```
<WubansiaState stateVersion="2" kit="FUT01" kit_content="1" standalone_bpm="0" midi_in="multi" phrase_lanes="auto">
  <PARAM id="master_gain" value="0.0"/> ... one per parameter, in the order above
</WubansiaState>
```

- **Five state-only settings, as root attributes:**
  - `kit`: the kit's id. Any string is kept, so a project naming a kit this build doesn't carry keeps it (and plays nothing, saying so).
  - `kit_content`: the content version the project was made with. The plugin plays the version it carries, and its content status says when the two differ.
  - `standalone_bpm`: the Standalone app's tempo, 40-300, or 0 for the kit's own.
  - `midi_in`: `multi` (MIDI in routes by channel, `docs/midi-map.md`) or `lane:<lane id>` (every channel plays that lane, for clips that drop their channels).
  - `phrase_lanes`: `auto` (recorded phrases while the tempo is within 2 % of the kit's, ADR 0020) or `off`.
- **Versions.** Saving writes `stateVersion` = max(2, the version read), so a newer project saved here keeps its version. Whatever this version doesn't know (attributes, parameters, children) is kept and written back.
- **Migration from version 1.** Every parameter M0's state lacks takes its default, and the kit is FUT01. A state without `stateVersion` is version 1.
- **Robustness.** A root other than `WubansiaState`, junk or a truncated block is ignored. A parameter value that isn't a finite number takes its default; a missing or unreadable setting takes its default.
- **The same bytes back.** Attributes are written in the order above (then the kept ones), parameters in the host's order whatever order the tree holds them in (then the kept children), and every value as it was read; so the bytes `getStateInformation` returns right after `setStateInformation` are the bytes it was given, after any parameter changes and kit changes in between (pluginval's restoration test).

### Threads and the host
- The state-only settings live in a mutex-guarded struct, never in the APVTS's tree (a ValueTree isn't safe across threads); the audio thread reads atomic copies of the ones it needs. State loads and saves take a second (recursive) lock, so a host can't replace the tree while another thread copies it. The audio thread takes neither.
- `setStateInformation` restores every parameter at once and only queues a kit load when the kit changed (or none was queued yet): it returns in well under 5 ms, even with a load in progress.
- A state-only change made in the editor calls `updateHostDisplay` with `nonParameterStateChanged`, so the host marks the project changed. Starting and stopping the Standalone transport, Preview, and the content folder don't.
- The content folder chosen with "Choose content folder…" is the person's, not the project's: it lives in `settings.xml` beside the default content folder (ADR 0022), never in state.
- No kit loads before the first `prepareToPlay` or `setStateInformation`: the constructor reads no file and starts no thread, since hosts and validators make instances by the dozen to scan them.

## Consequences
- 30 parameter IDs are frozen with the first project you save; a wrong name can be relabelled, never renamed.
- `plugin/tests/StateTests.cpp` changes: M0's "saves exactly the v1 fixture" retires (v0.1 saves version 2), "saved state is versioned" expects 2, and M0's exact v1 bytes must keep loading forever, now beside a hand-written v2 fixture. ParameterTests freeze the 30 IDs in order on both the plugin's processor and P10's test processor.
- An M0 build opening a v0.1 project restores `master_gain` and keeps the rest, since M0 already kept unknown data and never lowered `stateVersion`.
- A project made with a content version this build doesn't carry plays the version it has, and the editor says so: content versions are explicit (ADR 0022).

## Alternatives
- **Fewer frozen IDs** (the lane levels state-only): 10 fewer permanent IDs, but the levels couldn't be automated.
- **The kit as a choice parameter:** automatable, but automation would load and switch kits mid-song, a host's parameter list would grow with every kit, and a choice's range can't grow without breaking saved values.
- **The state-only settings as properties of the APVTS tree:** simplest, but not thread-safe against hosts that save off the message thread, and the parameter callbacks would see them.
- **A JSON state:** no gain over JUCE's XML, and M0's v1 bytes would need a second reader.
