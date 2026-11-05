# CLAUDE.md

This is a VST3/AU/CLAP instrument and standalone app. It plays futuristic groove kits in four directions (sci-fi club, future garage / 2-step, garage-house / deep tech house, dark industrial / techno) in sync with the DAW, builds full songs and extended mixes, generates new kits per direction, and exports MIDI and audio. It grows out of the "UKG & Minimal Pack" web app, kept in `legacy/web-pack/` as the reference and test baseline.

**The plan is `docs/ROADMAP.md`. Work one milestone at a time, in order, and tick its boxes as you go.**

## Layout
- `core/`: groove core. C++20, no JUCE, deterministic. Kits → patterns → songs → events → MIDI files.
- `engine/`: real-time audio engine (voices, sampler, mixer, FX). JUCE `dsp` is allowed.
- `plugin/`: JUCE processor and editor, parameters, state, host sync, and the WebView bridge.
- `ui/`: TypeScript + Vite front end loaded in the WebView. It talks to C++ only through `ui/src/bridge.ts`.
- `cli/`: the `groovecore` tool (export MIDI, render WAV, validate).
- `libs/`: pinned submodules (JUCE, clap-juce-extensions, Catch2). Bump only through an ADR.
- `cmake/`: our CMake helpers (identity, build defaults).
- `content/`: `kits/*.json` (+ `schema.json`), `styles/` and `feels/` (the generator's style and feel templates), `lyrics/` (vocal lines, clean and R-rated), `midi/` (each kit's written loop and synth phrases as MIDI, and in `midi/<kit>/extended/` the same parts as a DJ-friendly 224-bar extended mix: one file, one region per section and `regions.json`; both from `tools/sound/midi.py`), `index.json` (the sound library), `golden/` (reference outputs), `samples/` (the kits' WAVs, through Git LFS: ADR 0012). Other audio is not in git.
- `tools/`: Python sound design and vocal pipelines (`sound/`), the legacy metrics audit (`legacy_audit/`), the blind listening player (`listening/`), content packer, golden exporter, plus `tools/ci/` (validator scripts used by CI and locally).
- `legacy/web-pack/`: the original web app. It's the reference: read it and run it, but only change it to export goldens or to load kits from `content/kits`. It holds:
  - the page source (`core.js`, `app.js`, `flac.js`, `ui/`, `build_site.py`)
  - the sound pipeline (`audio/`, `inst/`) and manifests (`out/`)
  - the old tests (`tests/`)
  - one-off scripts (`history/`, not maintained)

  Its audio lives in the `web-pack-v21` GitHub release, not in git. See `legacy/web-pack/README.md`.
- `docs/`: the roadmap, `decisions/` (one short ADR per decision) and `listening.md` (listening sign-offs).
- `THIRD_PARTY_NOTICES.md`, `docs/third_party/`: license notices.

## Commands
Run from the repo root. Keep these commands accurate; entries marked "arrives in …" don't exist yet.
Needs CMake 3.25+, Ninja and a C++20 compiler:
- macOS: Xcode 16.
- Windows: Visual Studio 2022 or later with the "C++ Clang Compiler for Windows" component, in an x64 shell: the "x64 Native Tools Command Prompt for VS", or PowerShell after `& "<VS folder>\Common7\Tools\Launch-VsDevShell.ps1" -Arch amd64 -HostArch amd64`. CMake picks clang-cl by itself and stops if the shell is 32-bit.
- Linux: Clang 18 with lld (what CI uses) or GCC 13.

Node: the exact LTS version in `.nvmrc` (with nvm: `nvm install`), used by `golden_check`, the legacy audit (`tools/legacy_audit`) and the UI build. With any other version `golden_check` refuses and its Node tests are skipped.

- First checkout: `git submodule update --init --recursive`, and `git lfs pull` for the kits' samples (needs git-lfs)
- Configure and build: `cmake --preset release && cmake --build --preset release` (Debug: `cmake --preset debug && cmake --build --preset debug`)
- Tests: `ctest --preset release --output-on-failure` (no display needed; the validators below handle that themselves)
- Everything at once: `cmake --workflow --preset release` (configure, build, test). It doesn't run `golden_check`, so the legacy-metric tests are skipped until `golden_check` has exported `build/golden/`
- Plugins land in `build/release/plugin/Wubansia_artefacts/Release/` (`VST3/`, `AU/`, `CLAP/`, `Standalone/`). Configuring with `-DWUBANSIA_COPY_PLUGINS=ON` also installs them: the VST3, AU and CLAP on macOS; the VST3 and CLAP on Linux (`~/.vst3`, `~/.clap`); on Windows only the VST3 (elevated prompt), so copy the CLAP by hand. The Standalone app is never copied.
- macOS: re-sign the bundles ad hoc before validating, as CI does (ADR 0004): `for b in build/release/plugin/Wubansia_artefacts/Release/{VST3/Wubansia.vst3,AU/Wubansia.component,CLAP/Wubansia.clap,Standalone/Wubansia.app}; do codesign --force --sign - "$b"; done`
- Plugin validation, same pinned tools and flags as CI: `tools/ci/validate.sh build/release` (Windows: Git Bash; on macOS it installs the AU into `~/Library/Audio/Plug-Ins/Components`). By hand:
  - `pluginval --strictness-level 10 --verbose --validate build/release/plugin/Wubansia_artefacts/Release/VST3/Wubansia.vst3` (pluginval v1.0.4)
  - macOS AU, after re-signing and copying `Wubansia.component` to `~/Library/Audio/Plug-Ins/Components`: `killall -9 AudioComponentRegistrar; auval -a | grep Wuba; auval -strict -v aumu Wbns Wuba`
  - CLAP, report-only until clap-juce-extensions PR #193 lands: `clap-validator validate build/release/plugin/Wubansia_artefacts/Release/CLAP/Wubansia.clap --only-failed --exclude 'state-reproducibility-.*'` (clap-validator 0.4.1)
- Golden check (the legacy baseline): `cmake --build --preset release --target golden_check`. It regenerates the 516 legacy MIDI files from frozen `legacy/web-pack/core.js` into `build/golden/`, checks them against `content/golden/legacy-v21.sha256`, then runs `groovecore metrics` on them and passes only if legacy fails every gating band on its primary set, with the values in `core/tests/legacy_expected.json` (exactly, but for DFA alpha's ±1 in thousandths). Run it before `ctest`: the legacy-metric tests read `build/golden/` and are skipped without it
- Export the legacy baseline into `build/golden/`: `node tools/export_golden.js` (`--out <dir>` for another folder). Change the committed manifest only when asked.
- Generator CLI, `build/release/cli/groovecore` (`<command> --help` for the options): `groovecore gen --kit FUT01 --seed 7 --out <dir>` writes an extended mix as MIDI (the full arrangement, one region per section and `regions.json`, with feel and quantized; one `--out` folder per song); `groovecore metrics <files or folders>` scores MIDI with the generator metrics (`--legacy` on `build/golden/`). `groovecore render` (WAV through the engine) arrives in S1d
- Render a kit's sounds (`.venv` and the vocal models as in `tools/sound/README.md`): `.venv/bin/python tools/sound/render.py --kit FUT02 --set r3 --out build/fut02-r3-again`, then `.venv/bin/python tools/sound/listen.py build/fut02-r3-again` for WAV copies. Render into a fresh folder: `render.py` doesn't clear an existing one. Kits FUT01–FUT04: FUT01 and FUT03 are round 4 (`--set r4`), FUT02 and FUT04 round 3 (`--set r3`). An earlier round's set (`--set r2`, or FUT01's and FUT03's `--set r3`) is its item list at today's code, and that round's audio is in the history of `content/samples/`. The full core set arrives in S1c
- A kit's loop and extended mix as MIDI: `.venv/bin/python tools/sound/midi.py --kit FUT02` (writes `content/midi/fut02/`, `extended/` included; a test keeps it equal to a fresh export)
- Install a kit's sounds: `python3 tools/fetch_content.py --kit FUT01 --version 1`, or `--from-zip <path>` for a downloaded zip (arrives in S1c)
- UI build, before configuring on Apple: `npm --prefix ui ci && npm --prefix ui run build`; hot reload: `npm --prefix ui run dev` (arrives in S1f)
- Legacy web pack (these work today):
  - Get the audio: `gh release download web-pack-v21 -p web-pack-site-v21.zip && unzip web-pack-site-v21.zip -d legacy/web-pack`
  - Rebuild the page (byte-identical to the published v21): `python3 legacy/web-pack/build_site.py`
  - Validators: `cd legacy/web-pack && node validate.js && node validate_ext.js`
  - Smoke test: `cd legacy/web-pack/tests && npm install && QUICK=1 node smoke8.js`

## Audio-thread rules
- `processBlock` and everything it calls must not:
  - allocate memory
  - lock a mutex
  - log
  - read or write files or the network
  - throw
- The single exception: in non-realtime (offline) renders only, `processBlock` may wait, bounded at 30 s, on an atomic "ready" flag for the kit and timeline (ADR 0005 D5; the offline-wait ADR of plan §8.2, not written yet). It never waits in real time.
- The UI and worker threads share data with the audio thread only through atomics, lock-free FIFOs or immutable objects swapped by pointer. Old objects are freed off the audio thread.
- Generate timelines and decode samples on worker threads, never in `processBlock`.
- Parameter IDs are permanent once released. Rename the label, never the ID.

## Determinism and golden files
- The same kit, key, BPM, seed and options must produce the same bytes on every platform.
- `content/golden/` comes from `legacy/web-pack/core.js`. Never edit or regenerate the legacy baseline or `core/tests/legacy_expected.json` to make a test pass; fix the metric code.
- Generator snapshots (`core/tests/snapshots/gen.sha256`) change only in their own commit.
- Audio renders are exact on Linux and within tolerance on macOS (plan §3.5; its ADR isn't written yet).
- If a deliberate musical change breaks goldens:
  - say so and explain why
  - regenerate in a separate commit
  - note it in `docs/decisions/`
- JS parity pitfalls when porting:
  - `Math.round` rounds .5 up, so use `floor(x + 0.5)`.
  - `Math.imul` and `>>> 0` wrap at 32 bits, so use `uint32_t`.
  - JS sort is stable, so use `std::stable_sort`.
  - Object key order is insertion order, except integer-like keys, which come first in ascending order.
  - Keep doubles and the same operation order.
  - MIDI strings are UTF-8.

## Content rules (carried over from the pack)
- Everything is original. No audio, melodies, lyrics or song titles from existing records. Artist names are only style references ("in the spirit of"). Never put them in sound names, file names, preset names or product strings.
- Every kit is 128–140 BPM and is labelled with its key and Camelot code.
- Sound masters are 48 kHz / 24-bit, rendered natively from the synthesis scripts. Never "upscale" by resampling or bit-padding existing 44.1 kHz / 16-bit files. Keep the old set only as the web app's derivative.
- Vocals come in clean and R-rated versions. R-rated means profanity and innuendo only, never slurs.
- Murder-mystery (Halloween) content: no spoilers, and never use guests' real names.

## How to work
- Start each milestone by proposing a plan: the files you'll touch, the tests you'll add, the risks. Wait for approval before anything hard to undo (deleting files, rewriting history, changing goldens, adding dependencies).
- Write tests first for ports and generators. Keep changes small, with one step per branch, each with its own PR (S1 runs as S1.0–S1h).
- Before calling something done:
  - build
  - run the tests
  - run pluginval for plugin changes
  - say plainly what you couldn't verify (for example, how it sounds in a DAW)
- Record each decision as `docs/decisions/NNNN-title.md`, and update the ROADMAP checkboxes.
- New dependencies: note the license in the PR. Nothing GPL or AGPL gets linked into the plugin. JUCE is used under its commercial tiers unless an ADR says otherwise. GPL tools such as pluginval and pedalboard are fine at build or test time.
- Large files (audio, renders) go into the content bundle (Releases or LFS), never plain git.
