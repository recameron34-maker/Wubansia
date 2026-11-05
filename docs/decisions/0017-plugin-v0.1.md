# 0017. Plugin v0.1: the round 5 kits in the plugin, ahead of the ROADMAP's order

**Status:** Proposed, 2026-11-05 (draft, for the user's approval). The user asked: "Really nail the coolness of these and make sure to get the midis right; I'm over hearing the tests, give me the best version of it all in a plugin!"

## Context
The ROADMAP runs S1 in order (ADR 0005): generator v2 (S1b2), the core kit (S1c), the dry engine (S1d), effects (S1e), the WebView UI (S1f), and the plugin in a DAW (S1g). The user wants a plugin now instead of more listening rounds.

Round 5's four kits (FUT01-FUT04, ADR 0016) are finished:
- 723 WAVs at 48 kHz / 24-bit, in Git LFS (ADR 0012);
- the written MIDI for every kit, in `content/midi/`;
- a mix recipe and two club masters per kit, in `tools/sound/recipes/`.

The plugin is still M0: an 8-voice sine placeholder, one parameter and a 400 × 300 editor. `engine/` doesn't exist. The groove core (`wubansia_core`) isn't linked into the plugin, and on Linux it can't be: it isn't built as position-independent code, so lld refuses it in the VST3 and CLAP shared objects.

## Decision
- **Go for v0.1 now.** v0.1 is the four round 5 kits playing their committed MIDI, in sync with the host, through a C++ sampler, the recipes' mix and a real-time club master, with a native editor and MIDI drag-out.
  - It plays round 5's written MIDI, fixed at its source (MIDI v2, ADR 0019), not generator v2.
  - It takes pieces of S1d (sampler, scheduler, offline wait, `groovecore render`), S1e (mix and master) and S1g (parameters, state v2, Standalone transport, MIDI map, drag strip) ahead of S1b2 and S1c.
  - A ROADMAP row records the re-scope, as ADRs 0010, 0015 and 0016 recorded theirs. The S1 boxes are ticked only for what v0.1 really delivers.
- **`engine/` is plain C++20 with no JUCE.** This amends ADR 0006, which planned `juce_dsp` in `engine/fx`.
  - It is the static library `wubansia_engine`, linked by the plugin and `groovecore`, and tested by `engine_tests` (Catch2, no JUCE).
  - Every DSP block is ported from the Python stand-ins that define the approved sound (`sends.py`, `master.py`, `dsp48`, `phrase.pump_db`), then proven against them with exported vectors.
  - Mixing and gain are plain scalar loops. A grep test (`engine/tests/rules.cmake`) fails the build's tests if `engine/` includes a JUCE header or uses JUCE's vector or buffer arithmetic. Results then don't depend on Accelerate or on JUCE's SIMD paths (plan §3.5).
  - `juce_dsp` stays unlinked, and no third-party code is added.
- **Our own `wubansia_core` is linked into the plugin, built as position-independent code.** Both `wubansia_core` and `wubansia_engine` set `POSITION_INDEPENDENT_CODE ON`, so the Linux VST3 and CLAP shared objects link them under clang-18 and lld. The engine uses core's SMF reader and strict integer-only JSON reader on the service thread, never on the audio thread. The generator's snapshots stay byte-identical.
- **The v0.1 editor is native JUCE.** It sits behind a pure-virtual `Controller` API (`plugin/source/Controller.h`), the editor's only way to reach the plugin besides parameter IDs. The WebView stays the plan for S1f/M4 (ROADMAP §2, ADR 0006): its bridge will call the same `Controller`. `JUCE_WEB_BROWSER` stays 0.
- **The engine's timing unit is the millitick: 960 PPQ × 1000, in int64.** The content is 960 PPQ with each kit's swing written in. ADR 0006's 120 ticks per 16th (480 PPQ) can't hold FUT01's, FUT03's and FUT04's odd swing offsets (43.2, 28.8 and 19.2 ticks at 960 PPQ). This amends ADR 0006 for the engine; core's generator keeps 480 PPQ.
- **Kits are data.** Each kit is two integer-only manifests, `<KIT>_kit.json` and `<KIT>_mix.json` (`docs/plugin/manifest.md`), embedded in the binary with the kit's MIDI. The samples stay outside the binary. The engine knows no kit by name.
- **Kits cross threads as raw pointers.** One immutable `Kit` per (kit, content version) is shared by every instance.
  - The service thread offers it through a `PendingSlot`, and the audio thread adopts it.
  - The audio thread gives the old one back through a 16-slot `RetireFifo`, and the service thread frees it (`engine/rt`).
  - The audio thread never touches a `shared_ptr`: libc++ guards atomic `shared_ptr` operations with a mutex pool, and macOS 11 has no `atomic<shared_ptr>`.
  - This is ADR 0006's design with the hand-off types named.
- **The MIDI channel map is one table:** `tools/sound/channels.py`, owned by ADR 0019. `engine/kit/channel_map.h` is its C++ copy, with the same numbers.

## Consequences
- Many packages can be built in parallel against `engine/`'s headers and the manifest contract. Changes to those go through their owner.
- The plugin and `groovecore render` share one engine, so a render from the command line is the plugin's output.
- CI builds a complete plugin without the samples: the manifests and MIDI are embedded. With no content folder the plugin is silent and the editor says why. Every validator runs as it does today.
- The engine can't use JUCE's DSP classes, so the filters, the FDN, the oversamplers and the limiter are our own code, each tested against the Python reference.
- The parameter IDs, state v2 (ADR 0018), deleting `SineVoices`, MIDI v2 (ADR 0019), phrase lanes (ADR 0020) and the real-time master (ADR 0021) are separate decisions that need the user's approval before anything reaches main.

## Alternatives
- **Follow the ROADMAP's order.** S1b2 and S1c come before a playable plugin, so the user would wait through more listening rounds, which they asked to stop.
- **An engine on `juce_dsp`.** Faster to start, but the vector and buffer helpers use Accelerate on Apple, which makes renders platform-dependent. JUCE would also have to be linked into `engine_tests` and `groovecore render`.
- **`juce::MidiFile` and `juce::JSON` instead of linking core.** No PIC change would be needed, but there would be two SMF readers, a JSON reader that accepts floats, and the plugin's reading would differ from the tools'.
- **The WebView editor for v0.1.** `ui/`, the bridge and the WebView spike don't exist. They would put Node and a UI build into the macOS job and a WKWebView into the user's first build. The `Controller` API keeps that route open.
