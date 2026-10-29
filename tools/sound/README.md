# tools/sound

Wubansia's sound pipeline: 48 kHz DSP helpers, the recipes of each kit, the renderer that writes the 48 kHz / 24-bit FLAC masters, sample QA (with the mud gate) and spectrogram sheets. It is a build-time tool: nothing here ships in the plugin. The plan is `docs/plans/S1-futuristic-slice.md` (§2.2 sounds, §5.2 QA).

| File | What it is |
|---|---|
| `dsp48.py` | DSP helpers at `SR = 48000`: seeding (`rng`), oversampling, ADAA shapers, saws, filters, resonators, FDN, envelopes and edges (`finish`), `mono_below`, and the four tools of plan §2.2: `freq_shift`, `ring_mod`, `updown3`, `resample_pass` |
| `qa.py` | Every check of plan §5.2 plus the mud gate, category-aware, returning `Check` results with numbers |
| `item.py` | `Item` (one instrument), `Take` (one file), `Out` (a render plus what QA needs to know) |
| `kits/fut01.py` | FUT01 "Alloy Signal": the kit DNA and the registry that collects `ITEMS` from `recipes/*.py` |
| `recipes/` | One module per instrument, each exposing `ITEMS = [Item(...)]`: `drums.py`, `bass.py`, `music.py`, `vox.py`, `fx.py`, and `context.py` (the 4-bar context loop, dry, wet and its low end, played on the kit's own files). `example.py` is a working template |
| `render.py` | The CLI: render, one gain per instrument, write FLAC, QA, reports, sheets |
| `sheet.py` | One PNG per instrument: waveform, spectrogram, band envelopes |
| `denylist.py`, `denylists/` | Artist-name, slur and profanity lists, stored as SHA-256 digests |
| `models.py` | Kokoro TTS and Whisper ASR through sherpa-onnx, from `WUBANSIA_MODELS_DIR` |
| `vocals.py` | The vocal chain: a 24 kHz Kokoro take analysed with WORLD, sung or spoken as a robot on the grid, resynthesised at 48 kHz (ADR 0007); chops cut in the frame domain |
| `sends.py` | Python versions of the engine's planned sends: Space (8-line FDN with a little octave-up shimmer and faster-decaying low mids, ducked under the vox), Echo (tape ping-pong, dotted 8th) and a stand-in of the Alloy resonator (2-pole modes on the chord's tones times the alloy ratios). The context loop's wet version runs through them: the target the engine must match |
| `listen.py` | Copies a render's FLAC masters to WAV (the same samples) in plain-named `Drums/`, `Bass/`, `Music/`, `Vox/`, `FX/` and `Loops/` folders, for listening |
| `tests/` | `unittest` suite: defect fixtures and controls, a boundary pair for every QA limit, the DSP tools, the sends, the renderer; `masters.sha256` holds the hashes of two rendered instruments, which CI re-renders and compares |

## Set up

From the repo root, with Python 3.12 or 3.13:

```sh
python3 -m venv .venv
.venv/bin/python -m pip install --only-binary :all: -r tools/sound/requirements.txt   # what CI installs
.venv/bin/python -m pip install -r tools/sound/requirements-vocals.txt               # vocals and the Whisper check
```

`requirements.txt` pins `legacy/web-pack/requirements.txt`'s versions and their dependencies; all of them have wheels for CPython 3.12 and 3.13 on x86_64 Linux. The vocal file adds onnxruntime, sherpa-onnx and pyworld. pyworld has no Linux wheel for any version, so pip builds 0.3.5 from source (a C++ compiler; pip fetches Cython for that build only). pedalboard (GPLv3) is pinned only in the legacy file: no recipe here uses it.

**Same bytes.** numpy picks its SIMD kernels per CPU, and some of them round the last bit differently, so importing `tools.sound` pins numpy to its x86-64-v2 baseline on x86_64 Linux (`NPY_ENABLE_CPU_FEATURES=X86_V2`, unless you set it or `NPY_DISABLE_CPU_FEATURES` yourself). It must happen before numpy loads: `render.py` and unittest discovery import `tools.sound` first; a script of your own should too. OpenBLAS, behind numpy's matrix products (the FDN reverbs), is pinned the same way to its plain kernels (`OPENBLAS_CORETYPE=Prescott`). `qa_report.json` records the environment (CPU, versions, numpy's SIMD level, the OpenBLAS kernel). Vocal masters also depend on onnxruntime's kernels, which choose their instructions per CPU and can't be pinned: they are hashed per render environment (ADR 0007).

**Models** (vocals and the Whisper check only, never in git): unpack both into one folder and point `WUBANSIA_MODELS_DIR` at it.
- `kokoro-multi-lang-v1_0`: https://github.com/k2-fsa/sherpa-onnx/releases/download/tts-models/kokoro-multi-lang-v1_0.tar.bz2
- `sherpa-onnx-whisper-base.en`: https://github.com/k2-fsa/sherpa-onnx/releases/download/asr-models/sherpa-onnx-whisper-base.en.tar.bz2

## Commands

```sh
.venv/bin/python -m unittest discover -s tools/sound/tests -t .                       # tests (about 25 s)
.venv/bin/python tools/sound/render.py --kit FUT01                                      # every item, to build/FUT01
.venv/bin/python tools/sound/render.py --kit FUT01 --set s1a --out build/s1a-listen     # render a set
.venv/bin/python tools/sound/render.py --kit FUT01 --only KickSplit SnareMetal --out build/try   # some items
.venv/bin/python tools/sound/render.py --kit FUT01 --set example --repro                # to build/FUT01-example
.venv/bin/python tools/sound/listen.py build/s1a-listen                                 # WAV copies to listen to
.venv/bin/python tools/sound/denylist.py add artists < names.txt                      # extend a denylist
```

`render.py` writes `FUT01_<Group>_<Sound>_v<layer>_rr<n>.flac` (a zoned sound's `<Sound>` carries its note: `Reese-Fs1`), `qa_report.json`, `qa_report.md`, `sheets/<kit>_<group>_<name>.png` and, for kicks and bass shots, `qa/<kit>_<group>_<name>_2step.flac`. `--out` defaults to `build/<kit>`, or `build/<kit>-<set>` with `--set`. Options: `--no-sheets`, `--repro` (render each item a second time, each in a fresh process so no cache carries over, and compare the SHA-256), `--jobs N` (items in parallel). It exits 1 when a check fails (the files are written either way) and 2 on an unknown kit or item name or when a vocal item can't find the models; any other error is a bug and keeps its traceback. Renders go under `build/` (ignored by git, as are `*.flac`, `*.wav` and `*.onnx` anywhere): audio never goes in git.

## Writing a recipe

Make a module in `recipes/` (copy `example.py`) and list your instruments in `ITEMS`. Never edit another designer's module; item names must be unique across the kit (the registry refuses duplicates).

```python
from tools.sound import dsp48 as D
from tools.sound.item import Item, Out
from tools.sound.kits import fut01 as K

def kick(take):                       # one take = one file
    r = take.rng('click')              # this take's own noise; extra keys give independent streams
    ...                                # take.vel (0..1), take.layer, take.rr, take.note, take.f0
    return y                           # float, (n,) or (n, 2), 48 kHz; or Out(y, splices=..., loop=..., f0s=..., ...)

ITEMS = [Item('Drums', 'Kick', 'kick', kick, layers=(0.4, 0.55, 0.7, 0.85, 1.0), rr=2, root=K.KICK_NOTE,
              sets=('s1a',), about='split kick on F#1')]
```

**`Item` fields**

| Field | Meaning |
|---|---|
| `group` | File group: `Drums`, `Perc`, `Bass`, `Stabs`, `Pad`, `Lead`, `Vox`, `FX`, `Textures`, `Loops` |
| `name` | CamelCase letters and digits, unique in the kit: the file's `<Sound>` |
| `category` | `kick` (the 2-step boom too), `tom`, `snare`, `hat`, `perc`, `bass`, `pad`, `stab`, `lead`, `vox`, `fx`, `texture` or `loop`: picks the QA rules |
| `render` | `render(take)`, a module-level function |
| `layers` | Velocities in (0, 1], softest first: files `v1`..`vN` |
| `rr` | Round robins: files `rr1`..`rrN` |
| `notes` | Zone notes (MIDI, C4 = 60), ascending; empty for one unzoned sound |
| `root` | The note an unzoned sound is tuned to (the kick: `K.KICK_NOTE`, F#1) |
| `loop` | A sustain or bed that loops: kept whole, seam checked; return `Out(y, loop=(start, end))` if the loop isn't the whole file |
| `level` | LUFS-I target for the loudest take (beds -20, risers -14, wet vocals -16, dry -18); default: true peak at the ceiling |
| `hp_hz` | The renderer's sub-sonic high-pass (default 20 Hz, 4th order); `None` to skip |
| `flags` | `deliberate_alias`, `explicit`, `hf_synthetic`, `cut_end` |
| `tags` | Free tags; QA reads `riser`, `wet`, `dry`, `clean` |
| `sets` | Render sets, e.g. `('s1a',)` |
| `script` | The words of a vocal: Whisper checks dry files against it, the denylists check it |
| `about` | One line for the report and the sheet |
| `qa` | Overrides of the category's QA rules: `mud` (`'gate'`, `'lowcut'`, `'off'`), `loop_check` (the 2-step loop), `tonal`, `tune_cents`, `tune_window`, `sustain`, `harm_tol`; and `why`, one line of reason. The report lists every flag and override with its reason ("Exceptions"). A `Bass` or `Loops` item must say its low-end rule: `'gate'`, `'lowcut'`, or `'off'` with a `why` |

**`Out` fields**: `splices` (sample positions the recipe cut or joined: QA checks each for a click), `loop` (start, end) for a loop that isn't the whole file, `f0s` (every fundamental present: a chord's notes, and every voice of a detuned stack, or the alias check counts the detuning as aliasing), `hits` (kick onsets in a loop: the mud gate then also runs in every gap between them), `meta` (facts for the report, such as a robot phrase's slices or a target's settings: `qa_report.json` carries it, and later `index.json`). The report also carries each loop file's loop points.

**What the renderer does to every take**, in order: the sub-sonic high-pass; for one-shots, a trim where the sound stays 90 dB under its peak plus a 5 ms fade-out; a 0.25 ms fade-in (a loop from sample 0 gets neither); then **one gain for the whole instrument**, across every layer, round robin and zone: the loudest take's true peak at -1.02 dBTP (20 mdB under the ceiling, for the 24-bit rounding), or the `level` target capped there. Never normalise inside a recipe. Render long: a tail cut by the fade fails QA.

**Rules for recipes**
- All noise from `take.rng(...)`, the room from `K.space_rng()` (seed `FUT01/space`). No global random state, no `hash()`.
- The kit's material comes from `kits/fut01.py`: `ALLOY`, `VOICE`, `ROOT`, `KICK_NOTE`, `SCALE`, `BPM`, `STEP`.
- Nonlinear stages run oversampled (`D.oversampled`, the ADAA shapers). Asymmetric drive leaves DC: `D.dc_block` it.
- No mud: kicks, toms and basses must pass the mud gate. Everything else with low end is high-passed so that under 100 Hz it is 24 dB down (a root on F#2, 92.5 Hz, needs its high-pass nearer 130 Hz), and `D.mono_below(x, 150)`.
- Written phrases (loops, the context loop) put every lane's odd 16ths on the kit's one swing, `K.SWING`, so hits that share a step land together.
- Vocal masters come only from WORLD or vocoder resynthesis at 48 kHz (ADR 0005, D4), carry `hf_synthetic`, a `script`, and either `explicit` or the `clean` tag. Never resample TTS audio into a master.
- Names and strings never carry an artist's name (the denylist checks).

## QA (`qa.py`)

Per file: format (FLAC, PCM_24, 48 kHz; mono where L = R), true peak (4x) <= -1.0 dBTP, DC (5 Hz zero-phase low-pass) <= -60 dBFS, first sample <= -80 dBFS, one-shot tail (last 10 ms) <= -70 dBFS, HF burst at logged splices <= +12 dB, loop seam <= +3 dB, aliasing on tonal sustains <= -60 dB unless `deliberate_alias`, stereo correlation >= 0.2 and side under 120 Hz <= -20 dB, tuning wherever a pitch is declared (+-3 cents; kicks and toms +-10 over 0.10-0.25 s; snares, hats, vocals and fx unchecked), loudness by category, on `hf_synthetic` masters a top band (14-20 kHz at least -40 dB against 6-11 kHz: a 24 kHz take resampled to 48 kHz reads about -61, which D4 bans), and the low-end rule. Per instrument: velocity steps >= 1.5 dB with a rising centroid, round robins within +-0.5 dB, one gain, the 2-step loop (kicks, toms and bass one-shots), the denylists and vocal flags, Whisper on dry vocals, reproducibility (`--repro`). Every limit has a pair of fixtures just inside and just outside it in `tests/test_qa.py`.

**The mud gate** (kicks, toms and basses; the 2-step boom is a `kick`): the 150-400 Hz band falls 30 dB within 120 ms, stays at least 20 dB under 30-120 Hz after the first 80 ms, and is at most -12 dB of the sound; 400 Hz-1 kHz falls 30 dB within 150 ms. Bands are 4th-order Butterworth, zero-phase, with a 5 ms RMS envelope; a fall counts from the band's own peak until it stays down. "Stays under" is checked window by window: every 20 ms window (10 ms hop) from 80 ms on, against the sub in that window, or, where the sub has faded more than 30 dB under its loudest window, against that floor (a near-silent stretch can't read as mud; a ring with nothing under it still does). On a looped (sustained) sound the two falls don't apply. Kicks, toms and bass one-shots are also checked on a 2-bar 2-step loop at the kit's tempo: in every gap, 150-400 Hz stays 20 dB under 30-120 Hz from 80 ms after the hit to 20 ms before the next (the same windows), and is 30 dB under its loop peak 40-15 ms before the next hit. A loop file that declares its kicks (`Out.hits`) gets the same gap checks on itself instead of the whole-file one: the context loop's low end (`ContextLoopLow`) does. Everything else (`'lowcut'`): at most -24 dB of the sound under 100 Hz.
