# Kit samples

The first-listen kits as WAV, 48 kHz / 24-bit, one folder per kit: `Drums/`, `Bass/`, `Music/`, `Vox/`, `FX/` and `Loops/`, plus the kit's `LISTEN.md`. FUT01 and FUT03 are round 4 (rebuilt as UK garage and as garage-house: each kit's sounds, its synth accents and phrases, and its context loops, the mastered club loop among them); FUT02 and FUT04 are round 3 (the modern production pass). File names end in `v1` up to `v5` (soft to hard velocity layers) and `rr1` up to `rr4` (round robins); an instrument with one layer or one round robin leaves that part out. Each `LISTEN.md` is written for the kit's render folder, so the FLAC masters, `qa/`, `sheets/`, `qa_report.md`, `qa_report.json` and `listen.json` it names are there, not here. The kits' loops as MIDI, with an extended mix each and the synth phrases, are in `content/midi/`; their drum maps name these samples. Earlier rounds' sets are in this folder's history.

| Kit | Direction | Tempo and key | Round, render set |
|---|---|---|---|
| `fut01` Alloy Signal | future garage / 2-step | 134 BPM, F# minor (11A) | 4, `--set r4` |
| `fut02` Orbit Relay | sci-fi club | 130 BPM, D minor (7A) | 3, `--set r3` |
| `fut03` Glass Current | garage-house / deep tech house | 128 BPM, G minor (6A) | 4, `--set r4` |
| `fut04` Furnace Code | dark industrial / techno | 136 BPM, E minor (9A) | 3, `--set r3` |

The WAVs are stored with Git LFS (ADR 0012): install git-lfs before cloning, or run `git lfs pull` after. They are made by `tools/sound/render.py --kit <KIT> --set <its set>` and `tools/sound/listen.py`, which writes them into the render folder; replace `content/samples/<kit>/`'s `Drums/`, `Bass/`, `Music/`, `Vox/`, `FX/` and `Loops/` with the folders it writes (a replace, not a merge, so no stale file stays) and copy the render's `LISTEN.md`. So a change to a kit's sound means re-rendering and committing them again.
