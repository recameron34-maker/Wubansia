# 0012. Kit samples in the repo through Git LFS

**Status:** Accepted, 2026-11-01 (the user: "Save them to the repo"; Git LFS, WAV).

## Context
Round 2's three finished kits (FUT01–FUT03) are WAV samples the user wants to keep with the code and use now. CLAUDE.md keeps audio out of plain git: large files go into the content bundle, as Releases or LFS. This session can't create GitHub releases, and the plan's content route (`fetch_content.py`, S1c) doesn't exist yet.

## Decision
- **The kits' WAV copies live in `content/samples/<kit>/`, stored through Git LFS.** That's `Drums/`, `Bass/`, `Music/`, `Vox/`, `FX/` and `Loops/`, the same 48 kHz / 24-bit samples as the masters, plus the kit's `LISTEN.md`.
- **Only those WAVs go through LFS:** `.gitattributes` matches `content/samples/**/*.wav`, and `.gitignore` lets those files through its global `*.wav` rule. Every other render stays out of git.
- **The FLAC masters stay out of git.** The renderer rebuilds them (`tools/sound/render.py`), and `listen.py` makes the WAV copies from them.
- **The MIDI goes in plain git:** each kit's written loop is in `content/midi/<kit>/`, which `tools/sound/midi.py` makes and a test keeps equal to the recipes.

## Consequences
- Cloning with the samples needs git-lfs: install it before cloning, or run `git lfs pull` after. Without it the files are small pointer files.
- The samples count against the repo's GitHub LFS storage and bandwidth: about 180 MB for FUT01–FUT03.
- CI checks out without LFS (`actions/checkout`'s default), so it neither downloads the samples nor needs them.
- When a kit's sound changes, its samples are re-rendered and committed again, in their own commit, like the generator snapshots. LFS keeps every version.
- The full kits (S1c) may still go out as a release bundle; this ADR covers the first-listen sets.

## Alternatives
- **A GitHub Release:** keeps the repo light. The user would have to upload it, since this session can't create releases.
- **FLAC masters instead of WAV:** half the size, but not every DAW opens FLAC.
