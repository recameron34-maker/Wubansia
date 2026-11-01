# 0007. Master spec and oversampling rules

**Status:** Accepted, 2026-10-29, in S1a. The values are what the S1a render uses; the S1a PR is where you check them.

## Context
CLAUDE.md asks for 48 kHz / 24-bit masters rendered natively from the synthesis scripts, never upscaled from the 44.1 kHz / 16-bit legacy files. The prototypes rendered at 48 kHz but failed QA in places: DC from drive, cut tails, clicks at the start, a wide low end (plan §2.2). Kokoro speaks at 24 kHz only, and D4 (ADR 0005) settled how a vocal master may still be 48 kHz. S1a is the first render of real kit sounds: 31 items, 133 files.

## Decision
**Format.** FLAC, PCM 24-bit, 48 kHz; stored mono where L = R. Named `<KIT>_<Group>_<Sound>[-<Note>]_v<layer>_rr<n>.flac`, notes with C4 = 60 and `s` for sharp (`Reese-Fs1`). WAV copies for listening (`tools/sound/listen.py`) carry the same samples, bit for bit. Masters never go in git.

**Native rendering.** Every master is computed at 48 kHz in float64 by its recipe in `tools/sound/recipes/`. Nothing is resampled from another rate or padded from fewer bits. The legacy library stays 44.1/16 for the web app.

**Oversampling and band-limiting** (`tools/sound/dsp48.py`):
- Every nonlinear stage (drive, fold, clip, crush) runs oversampled through `oversampled()`: 4× by default, 8× for the heavy kick chains. The FIR is a Kaiser-windowed low-pass, flat to 20 kHz and about 100 dB down from 28 kHz, so what folds back lands above 20 kHz.
- The shapers are first-order ADAA (`tanh_adaa`, `hardclip_adaa`, `sinefold_adaa`).
- Oscillators are band-limited: PolyBLEP saws rendered at 4×, additive saws where it must be exact, FM pairs at 4×, the ring modulator at 2×.
- Asymmetric drive leaves DC, so it is followed by `dc_block`.
- Aliasing on purpose only with the `deliberate_alias` flag, and still run at 4× so only the intended images fold back (S1a: `GlitchFill`'s sample-and-hold crush).

**Level.** One gain per instrument, over every layer, round robin and zone: the loudest take's true peak (4×) at −1.02 dBTP, 20 mdB under the −1.0 dBTP ceiling to allow for the 24-bit rounding, or the item's LUFS target capped there. A recipe never normalises a take.

**Edges** (`render.py`, every take):
1. A 20 Hz 4th-order high-pass. A file that loops from sample 0 is filtered circularly.
2. One-shots: cut where they stay 90 dB under their peak, plus a 5 ms raised-cosine fade-out. A recipe renders long, so the fade never cuts a sounding tail.
3. A 0.25 ms fade-in.
4. Loops keep their length and points. End-aligned risers keep their last sample loud (`cut_end`).

**Stereo.** Kicks and subs are mono. Anything wide is mono under 150 Hz (`mono_below`, an 8th-order Butterworth on the side).

**Vocal masters (D4).** A vocal master comes only from WORLD resynthesis at 48 kHz (2 × 24 kHz) using the features of the 24 kHz Kokoro take (`tools/sound/vocals.py`):
- WORLD's analysis (harvest, CheapTrick, D4C, 5 ms frames) runs on the take. The performance (sung notes or the robot's grid) edits the frames, and synthesis runs at 48 kHz.
- The band above the take's 12 kHz is synthetic (`extend_hf`, ported from `legacy/web-pack/audio/vocals3.py`), so every vocal master carries `hf_synthetic`.
- Plain resampling of TTS audio into a master is banned, varispeed chops included. Chops are cut in the WORLD frame domain. The only resampled copy is Whisper's 16 kHz input for the QA check, and it is never written.
- QA enforces it: every `hf_synthetic` file must have a top band, 14-20 kHz at least -40 dB against 6-11 kHz (`hf_band`). A plain 2x resample of a 24 kHz take reads about -61 dB; the S1a vocal masters read -22 to -9 dB.
- Kokoro's graph draws random numbers, so each take gets a fresh onnxruntime session (one thread) built right after `set_seed` with a seed of the line, voice, options and take number.
- The models come from `WUBANSIA_MODELS_DIR` and are never in git. Without them, `render.py` stops with exit 2 and a message.

**Tools and pins.** numpy 2.5.3, scipy 1.18.1, soundfile 0.14.0 and matplotlib 3.11.2. The vocal chain adds onnxruntime 1.29.0, sherpa-onnx 1.13.8 and pyworld 0.3.5 (`tools/sound/requirements*.txt`). These are the legacy pins. pyworld has no Linux wheel, so it builds from source and stays out of CI. pedalboard 0.9.26 (GPLv3) stays pinned in `legacy/web-pack/requirements.txt` only: no recipe uses it.

**Same bytes: the pins and numpy's SIMD level.** numpy chooses its kernels per CPU at runtime, and its AVX2 and AVX-512 kernels round the last bit differently from the baseline and from each other: re-rendered as an AVX2-only CPU would, 3 of 107 non-vocal masters changed by 1 LSB (review of 2026-10-29); on 7 instruments of the current set, 2 of 60 files differ from the baseline with AVX2 dispatch and 6 of 60 with AVX-512. So importing `tools.sound` sets `NPY_ENABLE_CPU_FEATURES=X86_V2` on x86_64 Linux before numpy loads: only the x86-64-v2 baseline kernels run, on any CPU since about 2009, with no dispatch (`tools/sound/__init__.py`; a value already in the environment wins). OpenBLAS, which numpy's matrix products call, picks its kernels per CPU too: the FDN reverbs' mixing matrix gave a different ContextLoopWet with its Haswell and SkylakeX kernels, so importing `tools.sound` also sets `OPENBLAS_CORETYPE=Prescott`, its plain SSE3 kernels, on any x86-64 CPU. `qa_report.json` records the environment: the CPU, the versions, numpy's SIMD baseline and dispatch, and the OpenBLAS kernel. onnxruntime's MLAS kernels also choose their instructions per CPU and can't be pinned the same way, so the vocal masters, and the context loops that play the chops, are hashed per render environment.

## Consequences
- The 133 S1a files pass every QA check (ADR 0009). They hash the same rendered twice: in four processes with every item rendered again in a fresh process (`--repro`), and in one.
- Same bytes means the same pins, the same numpy SIMD level and the same OpenBLAS kernel, on x86_64 Linux. CI re-renders two instruments (the example ping and KickSplit) and compares them with `tools/sound/tests/masters.sha256`, and hashes an FDN's float output, so a CPU or a pin that changes the bytes shows up on the next push. The 119 non-vocal masters and the four 2-step loops gave identical bytes on Python 3.12 and 3.13; the vocals and the context loops were rendered on 3.13 only. macOS is not a render host for masters.
- 8× and additive rendering are slow, and the baseline kernels slower still (the S1a set takes a few minutes on 4 cores, more with `--repro`). That is fine at build time.
- Vocals always say `hf_synthetic`. If a permissively licensed 48 kHz-native TTS appears, it would replace `extend_hf` through a new ADR.

## Alternatives
- **Render at 96 kHz and decimate.** Rejected: twice the cost for every stage, while oversampling only the nonlinear stages gives the same alias floor.
- **Normalise each file.** Rejected: velocity layers and round robins would lose their level relationships.
- **Resample Kokoro's 24 kHz output to 48 kHz.** Banned by D4: a plain resample leaves the top octave empty while claiming 48 kHz.
- **pedalboard's effects in recipes.** Not needed so far: the dsp48 versions are deterministic numpy code that the engine ports can read.

## Sources
- CLAUDE.md, "Content rules"; ADR 0005 (D4); plan `docs/plans/S1-futuristic-slice.md` §2.2, §5.2, §6.1.
- `tools/sound/dsp48.py` (`os_fir`, `oversampled`, the ADAA shapers, `mono_below`, `TP_TARGET`), `tools/sound/render.py` (`finish_take`, `shared_gain`, `encode`, `environment`), `tools/sound/__init__.py` (the SIMD and OpenBLAS pins), `tools/sound/vocals.py`, `tools/sound/models.py`, `tools/sound/qa.py` (`hf_band_db`).
- `legacy/web-pack/audio/vocals3.py:315-556` (phrase singing, `extend_hf`), `legacy/web-pack/requirements.txt`.
- `build/s1a-listen/qa_report.json` (S1a render, 2026-10-29).
