# tools/sound

Wubansia's sound pipeline: 48 kHz DSP helpers, the kits and their recipes, the renderer that writes the 48 kHz / 24-bit FLAC masters, sample QA (with the mud gate) and spectrogram sheets. It is a build-time tool: nothing here ships in the plugin. The plans are `docs/plans/S1-futuristic-slice.md` (§2.2 sounds, §5.2 QA) and `docs/plans/S1-four-kits.md` (the four first-listen kits).

**Four kits, one toolkit.** Each kit is a module, `kits/<kit>.py`, holding its DNA, plus a recipe package of its own, `recipes/<kit>/`. A kit's designer edits only those two files or folders. Everything else here is shared and imports no kit.

| Kit | Direction | BPM | Groove | State |
|---|---|---|---|---|
| FUT01 "Alloy Signal" | future garage / 2-step | 134 | 2-step | first-listen set, round 4 (the UK garage rebuild: `--set r4`) |
| FUT02 "Orbit Relay" | sci-fi club | 130 | four on the floor | first-listen set, round 3 (the modern pass: `--set r3`) |
| FUT03 "Glass Current" | garage-house / deep tech house | 128 | four on the floor | first-listen set, round 4 (the garage-house rebuild: `--set r4`) |
| FUT04 "Furnace Code" | dark industrial / techno | 136 | four on the floor | first-listen set, round 3 |

| File | What it is |
|---|---|
| `kits/__init__.py` | The kit machinery: the DNA fields, finding kits (`names`, `module`, `load`), checking their DNA (`todo`, `problems`, `camelot`, `scale_of`), and a kit's items (`items`, collected from its recipe package) |
| `kits/<kit>.py` | One kit's DNA: `fut01.py` to `fut04.py` |
| `recipes/<kit>/` | One kit's recipes, one module per instrument, each exposing `ITEMS = [Item(...)]`. FUT01: `drums.py`, `bass.py`, `music.py`, `vox.py`, `fx.py`, `context.py` (the 4-bar context loop, dry, wet and its low end, played on the kit's own files) and `example.py`, a working template |
| `dsp48.py` | DSP helpers at `SR = 48000`: seeding (`rng`), oversampling, ADAA shapers, saws, filters, resonators, FDN, envelopes and edges (`finish`), `mono_below`, and the four tools of plan §2.2: `freq_shift`, `ring_mod`, `updown3`, `resample_pass` |
| `parts.py` | Shared building blocks: drum pieces (`sweep`, `gate`, `modes`, noise bands, `soft_clip`, `low_clean`, `vel_gain`), loop pieces (`raised`, `frac_delay`, `hp_steep`, `loop_grid`, `close_loop`, `allpass`) and the tuned string (`ks_tuned`) |
| `phrase.py` | Written phrases and loops: `Grid` (the 16th grid on the kit's one swing), `place`, `pan`, `two_pass`, `second_pass`, `circular`, `pump_db`, `kit_files` (a kit's files exactly as written), `hits_lane` |
| `sampler.py` | Stand-ins of the engine's sampler voice (`sampler_voice`), CC lanes (`cc_curve`) and bass lane filter (`lane_filter`) |
| `sends.py` | Python versions of the engine's planned sends: Space (8-line FDN with a little octave-up shimmer and faster-decaying low mids, ducked under the vox), Echo (tape ping-pong) and a stand-in of the Alloy resonator (2-pole modes on a chord's tones times a ratio set). FUT01's wet context loop runs through them: the target the engine must match |
| `vocals.py` | The vocal chain: a 24 kHz Kokoro take in the kit's voice blend, analysed with WORLD, sung or spoken as a robot on the kit's grid, resynthesised at 48 kHz (ADR 0007); chops cut in the frame domain |
| `qa.py` | Every check of plan §5.2 plus the mud gate, the bass-body mode and the kick crest floor (ADR 0011), the club loop's checks (ADR 0014), category-aware, returning `Check` results with numbers |
| `master.py` | The club master, a deterministic stand-in for the engine's master bus (ADR 0014): see Club masters below |
| `mixmeter.py` | The modern-sound standard's meters (loudness, true peak, PLR and PSR, octave bands and width, the shape checks, the crest per hit, the jump, the left/right lean) and its reference measurement: `python -m tools.sound.mixmeter --reference <file>` |
| `item.py` | `Item` (one instrument), `Take` (one file), `Out` (a render plus what QA needs to know) |
| `render.py` | The CLI: render, one gain per instrument, write FLAC, QA, reports, sheets |
| `sheet.py` | One PNG per instrument: waveform, spectrogram, band envelopes |
| `midi.py` | Exports a kit's written loop as MIDI (drums, basslines, lead, chords, the full loop, and any synth phrases its `PARTS['phrases']` lists, each on channel 4) at its tempo and swing, from the `midi` module of its recipe package, into `content/midi/<kit>/` with a README of the drum map, and the same parts as a DJ-friendly extended mix in its genre's form (`FORMS`: 224 bars, every drop after a break reached through its build) in `extended/`, one file, one region per section and `regions.json`; a test keeps the committed files equal to a fresh export |
| `listen.py` | Copies any kit's render to WAV (the same samples) in plain-named `Drums/`, `Bass/`, `Music/`, `Vox/`, `FX/` and `Loops/` folders, for listening |
| `denylist.py`, `denylists/` | Artist-name, slur and profanity lists, stored as SHA-256 digests |
| `models.py` | Kokoro TTS and Whisper ASR through sherpa-onnx, from `WUBANSIA_MODELS_DIR` |
| `tests/` | `unittest` suite: defect fixtures and controls, a boundary pair for every QA limit, the kits' DNA and isolation, the shared helpers, the DSP tools, the sends, the renderer. `masters.sha256` holds the hashes of two rendered instruments, which CI re-renders and compares, and `test_master.py` pins the club master's own output on its fixture |

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
.venv/bin/python -m unittest discover -s tools/sound/tests -t .                       # tests (about 4 minutes)
.venv/bin/python tools/sound/render.py --kit FUT01                                      # every item, to build/FUT01
.venv/bin/python tools/sound/render.py --kit FUT02 --set r3 --out build/fut02-r3-again  # a kit's round 3 set, in a fresh folder
.venv/bin/python tools/sound/render.py --kit FUT01 --only KickSplit SnareMetal --out build/try   # some items
.venv/bin/python tools/sound/render.py --kit FUT01 --set example --repro                # to build/FUT01-example
.venv/bin/python tools/sound/render.py --kit FUT01 --set s1a --out build/s1a-again      # round 1's set (FUT01's fix round replaced 6 of its 31 items)
.venv/bin/python tools/sound/listen.py build/fut02-r3-again                             # WAV copies to listen to
.venv/bin/python tools/sound/midi.py --kit FUT02                                        # the kit's loop and extended mix as MIDI, to content/midi/fut02
.venv/bin/python -m tools.sound.mixmeter --reference track.flac                        # a reference track, as the standard reads the kits (its S4)
.venv/bin/python tools/sound/denylist.py add artists < names.txt                      # extend a denylist
```

`render.py` writes `<KIT>_<Group>_<Sound>_v<layer>_rr<n>.flac` (a zoned sound's `<Sound>` carries its note: `Reese-Fs1`), `qa_report.json`, `qa_report.md`, `sheets/<kit>_<group>_<name>.png` and, for kicks and bass shots, `qa/<kit>_<group>_<name>_<groove>.flac` (`_2step` for FUT01, `_4x4` for the four-on-the-floor kits). `--out` defaults to `build/<kit>`, or `build/<kit>-<set>` with `--set`; it writes into an existing folder without clearing it, so render into a fresh one. `--set r2` is round 2's item list at today's code: FUT02-FUT04 render their round 3 sound under it and FUT01 lacks its context loops; round 2's audio is in `content/samples/`' history. Options: `--no-sheets`, `--repro` (render each item a second time, each in a fresh process so no cache carries over, and compare the SHA-256), `--jobs N` (items in parallel). It exits 1 when a check fails (the files are written either way). It exits 2 on an unknown kit or item name, on a kit whose DNA still has a TODO field or an invalid value (the message names them and the file), and when a vocal item can't find the models. Any other error is a bug and keeps its traceback. Renders go under `build/` (ignored by git, as are `*.flac`, `*.wav` and `*.onnx` anywhere but the kits' WAVs in `content/samples/`, which go through Git LFS: ADR 0012): no other audio goes in git.

## Starting a kit

1. **Fill the DNA** in `kits/<kit>.py`: replace every `TODO`. The fields are listed in `kits/__init__.py`, and the brief is in the module's docstring and `docs/plans/S1-four-kits.md`.
   - `KEY` and `CAMELOT` (`kits.camelot('D minor')` is `'7A'`); `SCALE` follows `KEY` (`kits.scale_of`).
   - `ROOT`, the key's root in the bass register, and `KICK_NOTE`, in the key, MIDI 24–40.
   - `RATIOS`, the kit's own inharmonic set: 1.0 first, ascending.
   - `VOICE`, the Kokoro blend: weights summing to 1.
   - `SWING`, 0.5 to under 0.75.
   - Already set: `KIT`, `TITLE` (a working name: title-check it), `DIRECTION`, `BPM`, `SPACE_SEED`, `GROOVE` and `RECIPES`.

   `kits.problems(module)` and the tests (`tests/test_kits.py`) say what is wrong. `render.py` refuses the kit until every field is set and valid. No shared test assumes your kit is still a stub, so filling it never means editing a file outside your two.
2. **Add recipes** to `recipes/<kit>/`: copy `recipes/fut01/example.py` and change its kit import to yours, `from tools.sound.kits import fut02 as K`; it renders as it is (a test copies it to a filled-in kit). Never reach another kit's module or recipes, by an import (relative ones too), `importlib.import_module` or `kits.load('FUT01')`; the tests check that. Use the shared helpers in `parts.py`, `phrase.py`, `sampler.py`, `sends.py` and `vocals.py`. If a helper you need lives in FUT01's recipes, ask for it to move to a shared module rather than importing it.
3. **Render and listen:** `render.py --kit FUT02 --set <set>`, then `listen.py build/FUT02-<set>`.

## Writing a recipe

List your instruments in `ITEMS`, in a module of your kit's package. Never edit another designer's module. Item names must be unique across the kit (the registry refuses duplicates).

```python
import numpy as np

from tools.sound import dsp48 as D
from tools.sound import parts as P
from tools.sound.item import Item, Out
from tools.sound.kits import fut02 as K

F_KICK = float(D.midi_hz(K.KICK_NOTE))

def kick(take):                       # one take = one file
    r = take.rng('click')              # this take's own noise; extra keys give independent streams
    n = int(0.6 * D.SR)
    s = np.sin(P.sweep(n, 300 + 300 * take.vel, 0.007, 0.016, 0.28, F_KICK))   # take.vel (0..1), take.layer, take.rr
    ...
    return y                           # float, (n,) or (n, 2), 48 kHz; or Out(y, splices=..., loop=..., f0s=..., ...)

ITEMS = [Item('Drums', 'Kick', 'kick', kick, layers=(0.2, 0.4, 0.6, 0.8, 1.0), rr=2, root=K.KICK_NOTE,
              sets=('r2',), about='split kick on the key root')]
```

**A phrase or loop** sits on the kit's grid, so every lane shares the kit's one swing:

```python
from tools.sound import phrase as Ph
G = Ph.Grid(K, 32)                                         # 2 bars of 16ths at K.BPM, odd 16ths on K.SWING
kick = Ph.hits_lane(K, 'Kick', ((0, 1.0), (4, 0.8), (8, 1.0), (12, 0.8)), G)   # the kit's own files, as written
Ph.place(buf, x, G.at(6))                                  # add a sound on a 16th, its tail wrapping round the loop
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
| `root` | The note an unzoned sound is tuned to (a kick: `K.KICK_NOTE`) |
| `loop` | A sustain or bed that loops: kept whole, seam checked; return `Out(y, loop=(start, end))` if the loop isn't the whole file |
| `level` | LUFS-I target for the loudest take (beds -20, risers -14, wet vocals -16, dry -18); default: true peak at the ceiling |
| `hp_hz` | The renderer's sub-sonic high-pass (default 20 Hz, 4th order); `None` to skip |
| `flags` | `deliberate_alias`, `explicit`, `hf_synthetic`, `cut_end` |
| `tags` | Free tags; QA reads `riser`, `wet`, `dry`, `clean`, `with_kick` (a loop whose kick hits are in the file itself) and `club` (a mastered loop: Club masters below) |
| `sets` | Render sets, e.g. `('s1a',)` |
| `script` | The words of a vocal: Whisper checks dry files against it, the denylists check it |
| `about` | One line for the report and the sheet |
| `qa` | Overrides of the category's QA rules, each listed in the report with its reason ("Exceptions"). See below |

**The `qa` keys:**
- `mud`: `'gate'`, `'bass-body'`, `'lowcut'` or `'off'`. A `Bass` or `Loops` item must say its low-end rule; `'off'` needs a `why`. `'bass-body'` is for the categories `bass` and `loop` only and needs a `why` (it is an exception, ADR 0011). Kicks and toms keep the gate: any other mode is refused, with or without a reason.
- `crest`: `False` turns the kick crest floor off, for a boom with no transient; it needs a `why`.
- `loop_check` (the groove loop), `tonal`, `tune_cents`, `tune_window`, `sustain`, `harm_tol`.
- `loop_choke`: `True` on a bass shot (with a `why`): its groove loop ends each copy on the next hit, as a mono voice plays it, instead of summing a move longer than the gap with the next copy of itself.
- On a `club` loop only, each with a `why`: `club_bands` (`False`, or the names of the bands it exempts, from `qa.CLUB_BANDS`), `club_max_reduction_db` (the master's cap, when it isn't the standard's 4 dB) and `club_crest_loss_db` (the crest the master may cost the hits, when it is more than 1.5 dB). Club masters below.
- `why`: one line of reason.

**`Out` fields:**
- `splices`: sample positions the recipe cut or joined; QA checks each for a click.
- `loop`: (start, end) for a loop that isn't the whole file.
- `f0s`: every fundamental present, such as a chord's notes and every voice of a detuned stack. Without them, the alias check counts the detuning as aliasing.
- `hits`: kick onsets in a loop. With the mud gate, its checks then also run in every gap between them. In a bass-body phrase, they are the kicks the bass plays against.
- `top_hits`: the onsets of a loop's full-velocity kicks: QA reads the kick crest floor on each as the loop plays it (`loop_kick_crest`). Every context loop's low end and every club loop declares them.
- `meta`: facts for the report, such as a robot phrase's slices or a target's settings. `qa_report.json` carries it, and later `index.json`. The report also carries each loop file's loop points. A club loop's `master` is its master's report, which QA reads.

**What the renderer does to every take**, in order:
1. The sub-sonic high-pass.
2. For one-shots, a trim where the sound stays 90 dB under its peak, plus a 5 ms fade-out.
3. A 0.25 ms fade-in (a loop from sample 0 gets neither fade).
4. **One gain for the whole instrument**, across every layer, round robin and zone: the loudest take's true peak at -1.02 dBTP (20 mdB under the ceiling, for the 24-bit rounding), or the `level` target capped there.

Never normalise inside a recipe. Render long: a tail cut by the fade fails QA.

**Rules for recipes**
- All noise from `take.rng(...)`, the room from `K.space_rng()` (seed `K.SPACE_SEED`). No global random state, no `hash()`.
- The kit's material comes from its own kit module: `RATIOS` (FUT01 also calls it `ALLOY`; write `RATIOS`, which every kit has), `VOICE`, `ROOT`, `KICK_NOTE`, `SCALE`, `BPM`, `STEP`, `SWING`. Never a literal from another kit.
- Nonlinear stages run oversampled (`D.oversampled`, the ADAA shapers). Asymmetric drive leaves DC: `D.dc_block` it.
- No mud. Kicks, toms and booms must pass the mud gate. Basses pass the gate, or the bass-body rule (`qa={'mud': 'bass-body', 'why': ...}`, proposed, ADR 0011): a B, warm bass keeps its low mids, for example `sampler.lane_filter(..., mids_hp=None)`, where `mids_hp=450.0` is FUT01's A, clean (the argument has no default). Under bass-body, put a mono sine sub (or one undetuned voice) under any detuned stack: a stack's own fundamentals beat, and where they cancel the window check reads the low mids against a vanishing sub and fails (a raw 3-saw stack, ±12 cents, low-passed at 500 Hz, reads +4 to +21 dB from F#1 to F#2; the same stack high-passed at 120 Hz over a sine sub reads -8 to -15). Everything else with low end is high-passed so that under 100 Hz it is 24 dB down (a root on F#2, 92.5 Hz, needs its high-pass nearer 130 Hz), and `D.mono_below(x, 150)`.
- Written phrases (loops, the context loop) put every lane on `phrase.Grid(K, steps)`, the kit's one swing, so hits that share a step land together. Sung lines pass `swing=K.SWING` to `vocals.sing_track` (it has no default; FUT01's S1a line passes 0.5, straight, until its fix round). Shared code takes the kit's tempo, groove and swing from its caller: `qa.two_step_loop` and `qa.check_loop` need `bpm` and `hits` (`qa.GROOVES[K.GROOVE]`).
- A bass phrase that plays against kicks declares them, `Out(y, hits=onsets)`. Under bass-body, it must leave each kick room (see QA). A loop that contains its kicks adds the tag `with_kick`.
- Vocal masters come only from WORLD or vocoder resynthesis at 48 kHz (ADR 0005, D4), carry `hf_synthetic`, a `script`, and either `explicit` or the `clean` tag. Never resample TTS audio into a master.
- Names and strings never carry an artist's name (the denylist checks).

## QA (`qa.py`)

**Per file:**
- Format: FLAC, PCM_24, 48 kHz; mono where L = R.
- True peak (4x) ≤ -1.0 dBTP; DC (5 Hz zero-phase low-pass) ≤ -60 dBFS; first sample ≤ -80 dBFS; one-shot tail (last 10 ms) ≤ -70 dBFS.
- HF burst at logged splices ≤ +12 dB; loop seam ≤ +3 dB.
- Aliasing on tonal sustains ≤ -60 dB, unless `deliberate_alias`.
- Stereo correlation ≥ 0.2; side under 120 Hz ≤ -20 dB.
- Tuning wherever a pitch is declared: ±3 cents; kicks and toms ±10 cents over 0.10-0.25 s; snares, hats, vocals and fx unchecked.
- Loudness by category.
- The top band on `hf_synthetic` masters: 14-20 kHz at least -40 dB against 6-11 kHz. A 24 kHz take resampled to 48 kHz reads about -61, which D4 bans.
- **The kick crest floor** (below), and on a loop that declares its full-velocity kicks (`Out.top_hits`), the same floor on each kick as the loop plays it (`loop_kick_crest`).
- The low-end rule.

**Per instrument:**
- Velocity steps ≥ 1.5 dB, with a rising centroid; round robins within ±0.5 dB; one gain.
- The groove loop (kicks, toms and bass one-shots).
- The denylists and the vocal flags; Whisper on dry vocals.
- Reproducibility (`--repro`).

Every limit has a pair of fixtures just inside and just outside it in `tests/test_qa.py`.

**The mud gate** (kicks, toms and basses; the 2-step boom is a `kick`). The 150-400 Hz band:
- falls 30 dB within 120 ms;
- stays at least 20 dB under 30-120 Hz after the first 80 ms;
- is at most -12 dB of the sound.

And 400 Hz-1 kHz falls 30 dB within 150 ms. Bands are 4th-order Butterworth, zero-phase, with a 5 ms RMS envelope; a fall counts from the band's own peak until it stays down.

"Stays under" is checked window by window: every 20 ms window (10 ms hop) from 80 ms on, against the sub in that window. Where the sub has faded more than 30 dB under its loudest window, the window is measured against that floor, so a near-silent stretch can't read as mud while a ring with nothing under it still does. On a looped (sustained) sound the two falls don't apply.

**The groove loop.** Kicks, toms and bass one-shots are also checked on a 2-bar loop at the kit's tempo, in its `GROOVE`: `qa.TWO_STEP` for FUT01, `qa.FOUR_FLOOR` for the four-on-the-floor kits. In every gap, 150-400 Hz stays 20 dB under 30-120 Hz from 80 ms after the hit to 20 ms before the next (the same windows). It is also 30 dB under its loop peak 40-15 ms before the next hit. A loop file that declares its kicks (`Out.hits`) gets the same gap checks on itself instead of the whole-file one: the context loop's low end (`ContextLoopLow`) does. Everything else (`'lowcut'`): at most -24 dB of the sound under 100 Hz.

**The kick crest floor** (ADR 0011, proposed; category `kick`, the top velocity layer). True peak over the RMS level of the loudest 50 ms must be at least 4.0 dB. A sine reads 3.01. KickSplit reads 5.8-6.4 and fails once driven 6 dB into a limiter (3.5). A spikier kick starts higher and fails only when limited harder: KickTight reads 7.9-8.9, 4.2-5.0 at 6 dB and 3.1-3.5 at 9 dB. So the floor catches a kick flattened onto its body, not one that is merely hard: it says nothing about round 1's "B sounds a bit hard like its really slamming the limiter", which was KickTight, the spikier one. The report's crest table shows how far each new kick sits from Tight (ADR 0011 leaves a ceiling of about 7 dB open for round 2). Softer layers are near-sine by design and aren't checked. A boom with no transient may turn the floor off (`qa={'crest': False, 'why': ...}`).

**The bass-body rule** (ADR 0011, proposed; `qa={'mud': 'bass-body', 'why': ...}`, categories `bass` and `loop`). A bass keeps its own harmonics in 150-400 Hz, up to a plain saw's level on its note:
- `body_over_saw`: no spectral peak in 150-400 Hz more than +5 dB over a saw's roll-off (-6 dB per octave) from the sound's own partials in 30-120 Hz;
- `body_vs_sub`: in every window (as the gate's), 150-400 Hz at least 2 dB under 30-120 Hz (≤ -2 dB). A sub that beats or fades under its low mids fails it, so keep a mono sine sub under a detuned stack;
- `body_share`: the share at most -5 dB;
- no falls.

A phrase that declares the kicks it plays against (`Out.hits`) also gets:
- in every gap, the same -2 dB window limit;
- 40-15 ms before each kick, 30 dB under its 150-400 Hz peak (the gate's rule);
- `body_duck`: under each kick's first 80 ms, the low mids at least 4 dB under the bass's own level in the gap after that kick (mean power both sides, ≤ -4 dB; a gap more than 30 dB under the loudest one counts at that floor). A steady 6 dB duck reads -4.7 to -5.5 from F#1 to G2 and passes, 3 dB about -2 and fails, a note held through the kick or starting on it undocked 0 to +1, a bass resting under the kick -26 or lower. A sidechain releasing over 120-250 ms needs about 8 dB (6 dB reads -2.9 to -4.2). Not checked with the `with_kick` tag.

Notes up to G2 (98 Hz): above, the fundamental leaves the 30-120 Hz band. In round 2 every kit's bass comes as A "clean" (the gate) and B "warm" (bass-body), for you to pick by ear.

## Club masters (`master.py`, `mixmeter.py`)

A kit's mastered loop, for round 3 (ADR 0014, proposed; the targets come from `docs/plans/S1-modern-sound.md`). Three parts:
- `master.py`: a deterministic stand-in for the engine's master bus. The engine's master (S1e) must match it, as the engine's sends must match `sends.py`.
- `mixmeter.py`: the standard's meters.
- QA: a loop tagged `club` is checked against its genre's targets (`qa.CLUB`).

**Adding a mastered loop to a kit.** In the kit's context module, master the mix it already builds (usually the wet one) and declare an item tagged `club`:

```python
from tools.sound import master as MS

def context_club(take):
    y = the_wet_mix()                                  # the loop as built today: lanes plus returns, DC cleaned
    hits = {'kick': full_velocity_kicks, 'clap': backbeat_claps}       # onsets, samples
    z, report = MS.master(y, MS.settings(K), hits=hits)                # K: the kit module
    return Out(z, splices=(fill_splice(),), top_hits=hits['kick'], meta={**the_meta(), 'master': report})

Item('Loops', 'ContextLoopClub', 'loop', context_club, loop=True, hp_hz=None, tags=('wet', 'club'),
     qa={'mud': 'off', 'why': '...'}, sets=('r3',), about='4-bar context loop, wet, mastered (club)')
```

- `MS.settings(K)` reads the genre from `K.DIRECTION`, with that genre's target loudness and PLR floor from `qa.CLUB`. The rest of the chain is the same for every genre.
- A kit changes its settings with a `MASTER` dict in its kit module, or per call, `MS.settings(K, clip_db=0.0)`. Explicit arguments win over `MASTER`, and `MASTER` wins over the defaults. `master.Settings` lists every field, and an unknown name is refused.
- **A cap other than the standard's 4 dB is declared on the item**, `qa={'club_max_reduction_db': 6.0, 'why': ...}`, and the recipe passes the same number to `MS.settings(K, max_reduction_db=...)`. QA checks the cap the report says the master ran with against it (`club_reduction`), so an undeclared raise fails, and the report lists the declared one under Exceptions.
- **Give the master the loop's hits** (`hits={'kick': ..., 'clap': ...}`, onsets in samples): its report then holds `punch`, ADR 0011's crest of every kick (over 200 ms) and clap (over 100 ms, and in 1-5 kHz) in its input and its output, and `peaks`, how far over the clipper's knee the kicks go and the loudest moments away from them. QA reads `punch` (`club_kick_crest`, `club_crest_loss`), and `qa_report.md` tables both.
- The item must be a loop (category `loop`, `loop=True`) with `hp_hz=None` and `level=None`; `Item` refuses anything else.
  - `hp_hz=None` because the master has its own low cut, and a filter after the limiter moves the peaks off the ceiling.
  - `level=None` because the master sets the loudness.
  - The renderer's one gain then only moves the true peak to -1.02 dBTP, by a few mdB.
- Put the report in the file's metadata. It records the settings the engine's master must match, and what the master did.

**The chain.** Every stage runs round the loop, so filters and envelopes are in their steady state and the seam joins.

| Stage | Default |
|---|---|
| Low cut | 4th-order Butterworth high-pass at 25 Hz |
| Mono | under 150 Hz: the side high-passed, 8th order (`dsp48.mono_below`) |
| Glue | stereo-linked, 2:1, a 6 dB knee, 30 ms attack, auto release (150 ms, over a 1.5 s envelope that follows sustained compression); the threshold is found so the loudest moment takes 2.5 dB of gain reduction |
| Drive | solved for the genre's target loudness, never past 4 dB of peak reduction (the driven true peak over the ceiling: the clipper and limiter together, the standard's cap) |
| Clip | 4x oversampled: linear up to 1 dB under the ceiling, then a tanh curve toward 0.5 dB over it; ADAA on the clipped part only |
| DC clean | a 10 Hz zero-phase high-pass: it removes the sub-sonic ripple that clipping and limiting leave on asymmetric kick cycles. Offline only (ADR 0014) |
| Limit | true peak (4x), stereo-linked: 5 ms lookahead, 60 ms hold, 80 ms release, to -1.02 dBTP. The hold reaches forward too, so the gain starts down 65 ms before a peak: the latency an engine that matches it reports, 3145 samples with the detector's reach (`master.limit_latency()`) |
| DC clean and trim | the clean again, then a trim if the true peak is over the ceiling, read round the loop or as a file (0.000-0.006 dB measured; 0.012 dB at 10.6 dB of reduction) |

**The report** (JSON-ready):
- the input's and output's loudness: LUFS-I, short-term max, true peak round the loop and as a file, PLR, PSR;
- the glue: its threshold and its gain reduction (max and mean);
- the drive, the clipper's peak reduction and how often it acts, the limiter's gain reduction (max, mean, how often it acts);
- the total peak reduction and the trim;
- the target: asked, used, reached, and how far short it stopped;
- the latency (samples, ms);
- given the hits: `punch` and `peaks` (above).

**A sparse mix stops short.** A mix too sparse for its target stops at the cap rather than being crushed, and `club_loudness` fails. The fix is density in the mix (the standard's section 3), not a higher cap; a raised cap is declared on the item and listed under Exceptions (round 3: FUT02 4.25, FUT03 5 and FUT04 6 dB, each costing the kicks more crest; ADR 0014 has the numbers, for your decision by ear). Measured on the round 2 wet loops at the default settings:

| | FUT01 | FUT02 | FUT03 | FUT04 |
|---|---|---|---|---|
| LUFS-I reached (target) | -9.9 (-9) | -11.2 (-8.5) | -12.7 (-8) | -13.3 (-8) |
| Peak reduction the target needs | 5.1 dB | 7.6 dB | 9.7 dB | 10.6 dB |

The test fixture, a 2-bar club loop at PLR 11.2 dB (`tests/test_master.py`), reaches -9 and -8.5 LUFS within the cap and stops 0.46 LU short of -8.

**QA of a `club` loop.** Every check reads the loop round its seam, through `mixmeter`. The genre is the kit's `DIRECTION`, which `Take.spec` looks up.

| Check | Limit (UKG / sci-fi club / future house / techno) |
|---|---|
| `club_loudness` | LUFS-I -9 / -8.5 / -8 / -8, each +-1 LU |
| `club_plr` | true peak minus LUFS-I ≥ 7.5 / 7.0 / 6.5 / 6.5 dB |
| `club_true_peak` | ≤ -1.0 dBTP round the loop: a file's `true_peak` can't see a peak that spans the seam |
| `club_bands` | every octave band in its genre's window, dB re 1 kHz (`qa.CLUB`). `sub` is the louder of the 31.5 and 63 Hz bands, in the 63 Hz window, because the kick's key decides which band holds it. The value is the worst distance outside a window |
| `club_low_width` | side over mid in the 125 and 250 Hz bands ≤ -10 dB (under 120 Hz is the `low_side` check, ≤ -20) |
| `club_reduction` | the cap the master ran with is 4 dB, or the item's declared `club_max_reduction_db`; the peak reduction within it |
| `club_kick_crest` | every full-velocity kick in the club loop ≥ 4.0 dB (ADR 0011's floor, over the 200 ms from each) |
| `club_crest_loss` | the median crest the master costs the kicks, and the claps, against its input ≤ 1.5 dB (the A&R review's proposal, to be calibrated by ear), or the item's declared `club_crest_loss_db` |
| `loop_kick_crest` | (any loop with `Out.top_hits`) every full-velocity kick ≥ 4.0 dB as the loop plays it |

- `qa={'club_bands': False, 'why': ...}` turns the band windows off on a club loop; `qa={'club_bands': ('125', '250'), 'why': ...}` exempts those bands only, the rest still gated. The report lists either under Exceptions.
- The loudness targets rest on measured medians of released records. The band windows and widths are conventions, until measured references replace them (the standard's S4).
- Every limit has a pair of fixtures just inside and just outside it, built at the standard's numbers written out in the test (`tests/test_qa.py`: `STANDARD`, `TestClub`, `TestMasterChecks`, `TestLoopCrest`), so loosening a limit in `qa.py` fails a test.

**The meters** (`mixmeter.py`). Loops are read round their seam (`loop=True`, the default); `loop=False` reads a file as `qa.py` does.
- Loudness: `lufs_i`, `lufs_s` and `lufs_m`, BS.1770-4 with `qa.py`'s K-weighting and gates. In file mode `lufs_i` is `qa.lufs`.
- Peaks: `true_peak_db` (4x; in file mode it is `dsp48.true_peak_db`), and `loudness` for PLR and PSR.
- Bands: `bands`, `balance`, `width` and `side_mid_db`: octave bands from one FFT, level re 1 kHz, side over mid, correlation.
- `shape`: the low-mid hole, the top slope and the sub hump.
- Punch: `crest_db` (the kick crest), `hit_crest_db` (the same crest per hit as a loop plays it, its true peak read with the loop round the span), `attack_ms`, and `jump_db`, a band's jump at each onset in a loop. The jump compares a hit's loudest 10 ms with the bed 60-10 ms before it, so it can't see a peak pulled down onto the hit's own body; the crest per hit can.
- `lean_db`: right over left per third octave (side over mid reads a lean as width).
- `reference`: a released record's loudest 16 s, measured as a file (44.1 and 96 kHz resampled to 48 kHz for measuring only), the standard's S4: `python -m tools.sound.mixmeter --reference <files> [--json]`.

Checked against (`tests/test_mixmeter.py`):
- EBU Tech 3341 cases 1-5, within 0.1 LU;
- BS.1770's quarter-rate true-peak case;
- pink and white noise slopes.

When it was written it also matched the standard's scratch meter within 1e-11 dB on all 16 round 2 context loops; that meter was scratch and is not kept.

**Not verified:** how any of it sounds. Whether a loop at -8 LUFS sounds punchy or crushed, whether 3 dB of clipping on the kicks is clean, and whether the glue glues: all of that is for listening round 3.
