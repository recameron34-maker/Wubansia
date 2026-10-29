# 0006. Slice architecture: plugin-native, render first

**Status:** Accepted, 2026-10-29.

## Context
S1 tests four goals with one kit (ADR 0005): the sounds, the MIDI, the effects and the UI. Each needs the user's ears or eyes early, long before the plugin can play the whole kit in a DAW. The S1 plan (`docs/plans/S1-futuristic-slice.md` §3) weighed three ways to get there. The user approved it with the recommended defaults.

## Decision
**Architecture A: plugin-native, built "render first".** Everything that ships is C++ (plus the WebView UI) from the start. Each goal gets an early check through files, before the plugin plays:
- `.mid` files from the generator (S1b)
- 48 kHz / 24-bit sample renders (S1a, S1c)
- WAV renders through the real C++ sampler and effects (S1d, S1e)
- the UI in Safari on a mocked bridge, the same WebKit engine as WKWebView (S1f)

**Threads:**
- **Message thread:** the processor's parameters, state and `prepareToPlay`. While the editor is open, also the WKWebView, `PollingRelayAttachment` (no parameter listeners) and a frame timer: 60 Hz while playing, 10 Hz idle, off when hidden. The timer runs the FFT and sends one `frame` event.
- **Service thread:** owned by the processor and always running, so a project reopened with its editor closed still works. It runs generator jobs ("latest wins") and publishes each immutable Timeline. It drains the retire FIFO every 50 ms and starts kit loads.
- **Loader thread:** decodes FLAC into `KitSamples` in a process-wide, reference-counted cache keyed by (kit ID, content version), so instances share memory. Loading is staged: drums, perc and bass first.
- **Audio thread:** playhead to ticks (120 per 16th), loop wraps, Jam and Song modes, P-center pre-roll, sampler voices (16-tap sinc) and the reese lane voice, 4 buses, the Space, Echo and Alloy resonator sends, the master. It never allocates, locks, logs or does I/O, except the bounded offline wait (D5).

**Hand-offs, all lock-free:**
- Timeline: atomic pointer swap. A new seed takes over at the next 4-bar line, a Feel change at the next bar.
- `KitSamples`: atomic pointer swap per load stage.
- Editor to audio: an SPSC command FIFO for pads, mutes and FX.
- Audio to editor: atomics for meters, an 8,192-float SPSC sample ring and a 256-slot ring of live events. Lane lights come from the timeline, scheduled in JS.
- Audio to service: retired pointers through an SPSC FIFO, freed off the audio thread.

**Where code lives:**

| Path | Holds | JUCE? |
|---|---|---|
| `core/gen`, `core/model`, `core/midi`, `core/metrics` | Generator v2 (integer only, counter RNG), Timeline, SMF writer, metrics | No |
| `engine/kit`, `engine/sampler`, `engine/sched` | Shared cache and staged loader; sampler; ticks to samples, loop wrap, Jam/Song | Loader only |
| `engine/fx`, `engine/perf`, `engine/render` | Effects; performance FX; the offline renderer | `juce_dsp` |
| `cli/` | `groovecore gen`, `metrics`, `render` | `juce_core`, `juce_audio_formats` |
| `plugin/source` | Processor, services, parameters, state v2, MIDI map; editor, drag strip, Standalone transport | Yes |
| `ui/` | Vite + TypeScript, `src/bridge.ts` with a mock, raw WebGL2 stage | Interop from the JUCE submodule |
| `tools/sound`, `tools/legacy_audit`, `tools/listening` | 48 kHz sound design and QA; legacy metrics; the blind player | — |

**Build switches:** `JUCE_WEB_BROWSER=1` on Apple only; Linux CI keeps a native placeholder editor and no `libwebkit2gtk`. `juce_dsp` and `juce_gui_extra` are linked. The UI bundle is embedded with `juce_add_binary_data` (ADR 0017).

## Consequences
- Every listening check judges code that ships, so nothing is signed off twice.
- The user hears sounds in S1a, MIDI in S1b and the whole kit through the engine in S1d, but plays the plugin in a DAW only in S1f (the spike) and S1g.
- `core/` stays free of JUCE and libm, so WebAssembly stays possible after v1.
- The generator is bit-identical everywhere. Audio renders are exact on Linux and within tolerance on macOS (ADR 0015).
- Linux can't run the WebView. The UI is checked in Playwright's WebKit on a mocked bridge, and by the user in Safari and in the DAW.

## Alternatives
- **B, a WebAudio prototype first.** Rejected: the user would sign off effects that never ship, and the generator would be written twice.
- **C, WebAssembly.** Rejected: it puts a second host in front of the first listen. It stays possible after v1, because `core/` stays free of JUCE and libm.

## Sources
- `docs/plans/S1-futuristic-slice.md` §3 (architecture), §5.3 (engine tests), §7 (steps).
- ADR 0001 (FLAC on; the D5 lock exception, which ADR 0018 amends in S1f); ADR 0004 (CI).
- `plugin/CMakeLists.txt:30` (`JUCE_WEB_BROWSER=0` today); `.github/workflows/ci.yml:97-98` (no `libwebkit2gtk` on Linux).
