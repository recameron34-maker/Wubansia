# Kit samples

The round 2 first-listen kits as WAV, 48 kHz / 24-bit, one folder per kit: `Drums/`, `Bass/`, `Music/`, `Vox/`, `FX/` and `Loops/`, plus the kit's `LISTEN.md`. File names end in `v1` to `v5` (soft to hard velocity layers) and `rr1` to `rr3` (round robins). The kits' loops as MIDI are in `content/midi/`.

| Kit | Direction | Tempo and key |
|---|---|---|
| `fut01` Alloy Signal | future garage / 2-step | 134 BPM, F# minor (11A) |
| `fut02` Orbit Relay | sci-fi club | 130 BPM, D minor (7A) |
| `fut03` Glass Current | hyper-clean future house | 128 BPM, G minor (6A) |
| `fut04` Furnace Code | dark industrial / techno | 136 BPM, E minor (9A) |

The WAVs are stored with Git LFS (ADR 0012): install git-lfs before cloning, or run `git lfs pull` after. They are made by `tools/sound/render.py --kit <KIT> --set r2` and `tools/sound/listen.py`, so a change to a kit's sound means re-rendering and committing them again.
