# Kit samples

The first-listen kits as WAV, 48 kHz / 24-bit, one folder per kit: `Drums/`, `Bass/`, `Music/`, `Vox/`, `FX/` and `Loops/`, plus the kit's `LISTEN.md`. Every kit is round 5: its sounds from rounds 3 and 4, a wordless vox synth in place of the sung vocals (`Vox/`, with its riff in `Loops/`), its synth accents and phrases, and its context loops in two versions, with the vox synth and without, each with a mastered club loop. File names end in `v1` up to `v5` (soft to hard velocity layers) and `rr1` up to `rr4` (round robins); an instrument with one layer or one round robin leaves that part out. Each `LISTEN.md` is written for the kit's render folder, so the FLAC masters, `qa/`, `sheets/`, `qa_report.md`, `qa_report.json` and `listen.json` it names are there, not here. The kits' loops as MIDI, with an extended mix each and the synth phrases, are in `content/midi/`; their drum maps name these samples. Earlier rounds' sets, the sung vocals among them, are in this folder's history.

| Kit | Direction | Tempo and key | Render set |
|---|---|---|---|
| `fut01` Alloy Signal | future garage / 2-step | 134 BPM, F# minor (11A) | `--set r5` |
| `fut02` Orbit Relay | sci-fi club | 130 BPM, D minor (7A) | `--set r5` |
| `fut03` Glass Current | garage-house / deep tech house | 128 BPM, G minor (6A) | `--set r5` |
| `fut04` Furnace Code | dark industrial / techno | 136 BPM, E minor (9A) | `--set r5` |

The WAVs are stored with Git LFS (ADR 0012): install git-lfs before cloning, or run `git lfs pull` after. They are made by `tools/sound/render.py --kit <KIT> --set r5` and `tools/sound/listen.py`, which writes them into the render folder; replace `content/samples/<kit>/`'s `Drums/`, `Bass/`, `Music/`, `Vox/`, `FX/` and `Loops/` with the folders it writes (a replace, not a merge, so no stale file stays) and copy the render's `LISTEN.md`. So a change to a kit's sound means re-rendering and committing them again.
