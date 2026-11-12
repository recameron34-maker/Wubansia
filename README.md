# Wubansia

A groove construction-kit instrument (VST3 / AU / CLAP and a standalone app), growing out of the **UKG & Minimal Pack** web app. That app has 43 kits across seven genres (UK garage, garage house, minimal, speed garage, house, groove house, tech house), full songs and extended mixes, and about 840 original sounds.

| Where | What |
|---|---|
| [`docs/ROADMAP.md`](docs/ROADMAP.md) | The build plan: decisions, architecture, milestones M0–M7 (including the HD "upscale" of every sound) |
| [`CLAUDE.md`](CLAUDE.md) | Instructions Claude Code follows in this repo |
| [`legacy/web-pack/`](legacy/web-pack/) | The original web app, its sound-design pipeline, manifests and tests. It's the reference for the port |
| Release [`web-pack-v21`](https://github.com/recameron34-maker/Wubansia/releases/tag/web-pack-v21) | The audio: the published site, the Decent Sampler/SFZ instrument library and dev data. Not stored in git |

## Install on a Mac

Two downloads: the macOS zip from CI (`Wubansia-macOS-<commit>.zip`: the VST3, AU and CLAP plugins and the standalone app) and a content zip per kit (`Wubansia-Content-FUT01-v1.zip` and so on: the samples, about 30-60 MB each).

1. Unzip the macOS zip, and put the content zips next to the `Wubansia` folder it makes.
2. In Terminal, type `bash `, drag `Wubansia/install-macos.sh` onto the window and press Return. With `--dry-run` it shows the steps first.
3. Open your DAW.

[`dist/INSTALL.md`](dist/INSTALL.md), also in the zip, says what the script does, how to do it by hand, and what to try if something's wrong. From a clone instead: `git lfs install && git lfs pull --include 'content/samples/**'`, then `python3 tools/content/install.py --from-repo` puts the samples where the plugin looks, and `python3 tools/content/pack.py --kit FUT01 --out <folder>` makes a content zip ([ADR 0022](docs/decisions/0022-content-location-and-delivery.md)).

**Status:** v0.1: the four round 5 kits play in the plugin, in sync with your DAW (ADR 0017). Install it as above, then run the DAW checklist in [`docs/listening.md`](docs/listening.md). What comes next is in [`docs/ROADMAP.md`](docs/ROADMAP.md).

**v0.2 Samples** ([ADR 0024](docs/decisions/0024-samples-drawer-and-audio-drag-out.md)): the **Samples** button opens a drawer of every kit's sounds; click one to hear it, or drag it into your DAW as the kit's own 48 kHz / 24-bit WAV, saved in `~/Music/Wubansia/Samples` (how, in [`dist/INSTALL.md`](dist/INSTALL.md)'s "Samples into your DAW"; its DAW checklist is v0.2's in [`docs/listening.md`](docs/listening.md)).

To start with Claude Code, open this repo and say:

> Read CLAUDE.md and docs/ROADMAP.md. We're starting M0. Propose the exact steps first: template, JUCE version pin, CMake presets, CI matrix, CLAP wrapper. Wait for my OK, then implement. Stop when the M0 "Done when" checks pass, and tell me how to load the plugin in my DAW.
