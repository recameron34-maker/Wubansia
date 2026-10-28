# CLAUDE.md

This is a VST3/AU/CLAP instrument and standalone app. It plays groove construction kits (UK garage, garage house, minimal, speed garage, house, groove house, tech house) in sync with the DAW, builds full songs and extended mixes, generates new kits per genre, and exports MIDI and audio. It grows out of the "UKG & Minimal Pack" web app, kept in `legacy/web-pack/`.

**The plan is `docs/ROADMAP.md`. Work one milestone at a time, in order, and tick its boxes as you go.**

## Layout
- `core/`: groove core. C++20, no JUCE, deterministic. Kits → patterns → songs → events → MIDI files.
- `engine/`: real-time audio engine (voices, sampler, mixer, FX). JUCE `dsp` is allowed.
- `plugin/`: JUCE processor and editor, parameters, state, host sync, and the WebView bridge.
- `ui/`: TypeScript + Vite front end loaded in the WebView. It talks to C++ only through `ui/src/bridge.ts`.
- `cli/`: the `groovecore` tool (export MIDI, render WAV, validate).
- `content/`: `kits/*.json` (+ `schema.json`), `index.json` (the sound library), `golden/` (reference outputs). Audio files are not in git.
- `tools/`: Python sound design and vocal pipelines, content packer, golden exporter.
- `legacy/web-pack/`: the original web app. It's the reference: read it and run it, but only change it to export goldens or to load kits from `content/kits`. It holds:
  - the page source (`core.js`, `app.js`, `flac.js`, `ui/`, `build_site.py`)
  - the sound pipeline (`audio/`, `inst/`) and manifests (`out/`)
  - the old tests (`tests/`)
  - one-off scripts (`history/`, not maintained)

  Its audio lives in the `web-pack-v21` GitHub release, not in git. See `legacy/web-pack/README.md`.
- `docs/`: the roadmap, `decisions/` (one short ADR per decision) and `listening.md` (listening sign-offs).

## Commands
Replace these placeholders with the real commands during M0, and keep them accurate.
- Configure and build: `cmake --preset release && cmake --build --preset release`
- Tests: `ctest --preset release --output-on-failure`
- Golden check: `cmake --build --preset release --target golden_check`
- Plugin validation: `pluginval --strictness-level 10 --validate <path-to-plugin>` (and `auval -a` on macOS)
- UI: `cd ui && npm run dev` / `npm run build`
- Regenerate goldens (only when asked): `node tools/export_golden.js`
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
- The UI and worker threads share data with the audio thread only through atomics, lock-free FIFOs or immutable objects swapped by pointer. Old objects are freed off the audio thread.
- Generate timelines and decode samples on worker threads, never in `processBlock`.
- Parameter IDs are permanent once released. Rename the label, never the ID.

## Determinism and golden files
- The same kit, key, BPM, seed and options must produce the same bytes on every platform.
- `content/golden/` comes from `legacy/web-pack/core.js`. Never edit or regenerate goldens to make a test pass; fix the port.
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
- Write tests first for ports and generators. Keep changes small, with one milestone per branch and a PR at the end.
- Before calling something done:
  - build
  - run the tests
  - run pluginval for plugin changes
  - say plainly what you couldn't verify (for example, how it sounds in a DAW)
- Record each decision as `docs/decisions/NNNN-title.md`, and update the ROADMAP checkboxes.
- New dependencies: note the license in the PR. Nothing GPL or AGPL gets linked into the plugin. JUCE is used under its commercial tiers unless an ADR says otherwise. GPL tools such as pluginval and pedalboard are fine at build or test time.
- Large files (audio, renders) go into the content bundle (Releases or LFS), never plain git.
