# 0001. Framework, template and pinned dependencies

**Status:** Accepted, 2026-10-28.

## Context
M0 needs an empty instrument that builds as VST3, AU, CLAP and Standalone in CI and loads in a DAW (ROADMAP §5). ROADMAP §2 chose JUCE with CMake, starting from the Pamplejuce template, with CLAP through clap-juce-extensions. CLAUDE.md asks for approval before adding dependencies. The user approved the M0 plan with its recommended defaults: the pins below (D0), our own CMake helpers (D2), the JUCE lock exception (D5) and the JUCE Starter tier (D7).

## Decision
- **JUCE 9.0.4**, tag commit `3fb5cbc0340758596dac39f7c8de47d562b4896e` (2026-10-12), as the submodule `libs/JUCE`. Used under the JUCE 9 EULA, Starter tier.
- **clap-juce-extensions** `7adee3a1bd4684d4caa5601100e364abccff4b4d` (main, 2026-10-06) at `libs/clap-juce-extensions`. Its nested submodules are CLAP 1.2.7 (`29ffcc27`) and clap-helpers `a61bcdf0`. Its newest tag, `0.26.0`, is from 2022, so we pin a commit. JUCE 9 has no native CLAP.
- **Catch2 v3.8.1** (`2b60af89`) at `libs/Catch2`, for the test binary only. Pamplejuce's JUCE 9 CI is green with it. The bump to v3.16.1 gets its own PR.
- **Template:** files copied from Pamplejuce `ca3cae0` (MIT; text in `docs/third_party/pamplejuce-LICENSE`) and adapted: the CMake (split into the root and `plugin/`), processor, editor, tests, `VERSION`, `.clang-format` and the CI workflow. Left out: the demo, AUv3, IPP, melatonin_inspector, benchmarks, assets, signing and sudara/cmake-includes. Pamplejuce's `packaging/` at `ca3cae0` is the starting point for M7.
- **Our own CMake helpers (D2, option B):** `cmake/WubansiaIdentity.cmake` and `cmake/WubansiaDefaults.cmake`.
- **Build:** CMake 3.25 or later, `CMakePresets.json` v6 (`release`, `debug`), Ninja on every platform, C++20. On Windows the root CMakeLists picks clang-cl and stops on a 32-bit shell. The MSVC runtime is linked statically, so users need no VC++ redistributable. macOS deployment target 11.0 (D4, see ADR 0004).
- **JUCE config:** `JUCE_WEB_BROWSER=0` until M4, `JUCE_USE_CURL=0`, and `JUCE_USE_OGGVORBIS=0`, `JUCE_USE_OPUS=0`, `JUCE_USE_WEBP=0`, which all default to 1. FLAC stays on because the content is FLAC. `JUCE_ASIO` keeps its default of 0, because the bundled ASIO SDK is `LicenseRef-Steinberg-ASIO OR GPL-3.0-only`.
- **Audio-thread lock exception (D5).** Accepted until M3, for code inside JUCE only:
  1. The VST3 wrapper applies host automation on the audio thread: `processParameterChanges` → `setValueAndNotifyIfChanged` → `setValueNotifyingHost`.
  2. `AudioProcessorParameter::sendValueChangedMessageToListeners` takes `listenerLock`. Every JUCE parameter does this, so dropping APVTS wouldn't remove it. APVTS then takes a second lock of its own.
  3. The lock can be contended: when the editor opens or closes, `SliderAttachment` adds or removes its listener under the same lock on the message thread.
  4. While the editor is open, each automated change calls `triggerAsyncUpdate()` from the audio thread, which posts to the message queue.

  This conflicts with ROADMAP §3 ("never … locks") and the "no mutex" rule in CLAUDE.md. Our own code takes no locks. M3 adds an RTSan run, and M3/M4 replace the attachment with one that polls instead of registering a listener. That removes items 3 and 4. Item 2 is inherent to JUCE.
- **Pin bumps:** one PR per bump. Read JUCE's `BREAKING_CHANGES.md`, run the full CI matrix, do a DAW check and update this ADR. `.gitmodules` has no `branch =` lines; Pamplejuce's `branch = develop` is how it ended up on the unreleased commit `620b879`.

## Consequences
- Every dependency is a submodule, so configuring downloads nothing.
- JUCE 9.0.4 and clap-juce-extensions `7adee3a` were never built together upstream; its CI tests JUCE 9.0.0 only. Our CI is the proof. Fallback: `c1a5ad0`, Pamplejuce's pin.
- Open clap-juce-extensions items: PR #193 (loading state doesn't ask the host to rescan parameters, so clap-validator's state tests fail), PR #186 (Windows editors clipped at display scaling other than 100%), issue #184 (Bitwig bounce stalls when a plugin reports latency).
- License terms are unverified. The Starter cap is reported as $20,000 of annual revenue or funding, and JUCE 8 reportedly dropped the splash-screen requirement. Both come from forum posts, because juce.com was blocked from the session. **Check https://juce.com/legal/juce-9-licence/ before selling.**
- Nothing GPL or AGPL is linked into the plugin. Notices are in `THIRD_PARTY_NOTICES.md`.

## Alternatives
- JUCE 8.0.16-security (commit `ea774447`), the newest 8.x tag: older, and ROADMAP allows 9.x.
- JUCE master or develop: master is 9.0.4 plus one CI-only commit, and develop isn't a release.
- Pamplejuce unmodified, with sudara/cmake-includes `5d75b1f` (D2 option A). It has no LICENSE file, turns on fast math (`/fp:fast`, `-Ofast`, and `-ffast-math` on its newer main), FORCE-sets macOS 10.14, downloads Catch2 at configure time and changes behaviour between commits.
- nih-plug (Rust): ROADMAP §2 chose JUCE for its documentation and formats.

## Sources
- `git ls-remote --tags` of JUCE, Catch2 and clap-juce-extensions, run 2026-10-28; `libs/JUCE/LICENSE.md`; `libs/JUCE/JUCE.spdx.json`.
- Defaults, in `libs/JUCE/modules/`: `juce_audio_devices/juce_audio_devices.h:134-135` (ASIO), `juce_audio_formats/juce_audio_formats.h:75,102,111` (FLAC, Ogg Vorbis, Opus), `juce_graphics/juce_graphics.h:128` (WebP).
- Lock chain, in `libs/JUCE/modules/`: `juce_audio_plugin_client/juce_audio_plugin_client_VST3.cpp:827-834,3536,3590`; `juce_audio_processors_headless/processors/juce_AudioProcessorParameter.cpp:111-113,154-163`; `juce_audio_processors/utilities/juce_AudioProcessorValueTreeState.cpp:156,185`; `juce_audio_processors/utilities/juce_ParameterAttachments.cpp:45,100-112`.
- clap-juce-extensions: `libs/clap-juce-extensions/.github/workflows/build-cmake.yml`; https://github.com/free-audio/clap-juce-extensions/pull/193, https://github.com/free-audio/clap-juce-extensions/pull/186, https://github.com/free-audio/clap-juce-extensions/issues/184.
- Pamplejuce `ca3cae0`: `LICENSE`, `CMakeLists.txt`, `.gitmodules`. cmake-includes `5d75b1f`: `SharedCodeDefaults.cmake:12-28`, `PamplejuceMacOS.cmake:5`, `Tests.cmake:25`; at `8de22da`: `CHANGELOG.md`.
- License reports: https://forum.juce.com/t/juce-9-no-pricing-or-eula-changes/69000, https://forum.juce.com/t/revenue-limits-for-juce-tiers/61058, https://forum.juce.com/t/amendments-to-the-juce-end-user-licence-agreement-for-juce-8/61265.
