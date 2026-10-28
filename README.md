# Wubansia

A groove construction-kit instrument (VST3 / AU / CLAP and a standalone app), growing out of the **UKG & Minimal Pack** web app. That app has 43 kits across seven genres (UK garage, garage house, minimal, speed garage, house, groove house, tech house), full songs and extended mixes, and about 840 original sounds.

| Where | What |
|---|---|
| [`docs/ROADMAP.md`](docs/ROADMAP.md) | The build plan: decisions, architecture, milestones M0–M7 (including the HD "upscale" of every sound) |
| [`CLAUDE.md`](CLAUDE.md) | Instructions Claude Code follows in this repo |
| [`legacy/web-pack/`](legacy/web-pack/) | The original web app, its sound-design pipeline, manifests and tests. It's the reference for the port |
| Release [`web-pack-v21`](https://github.com/recameron34-maker/Wubansia/releases/tag/web-pack-v21) | The audio: the published site, the Decent Sampler/SFZ instrument library and dev data. Not stored in git |

**Status:** planning. The next step is milestone M0 in the roadmap: an empty plugin that builds in CI and loads in a DAW.

To start with Claude Code, open this repo and say:

> Read CLAUDE.md and docs/ROADMAP.md. We're starting M0. Propose the exact steps first: template, JUCE version pin, CMake presets, CI matrix, CLAP wrapper. Wait for my OK, then implement. Stop when the M0 "Done when" checks pass, and tell me how to load the plugin in my DAW.
