# 0003. Deterministic floating point and line endings

**Status:** Accepted, 2026-10-28.

## Context
CLAUDE.md requires that the same kit, key, BPM, seed and options produce the same bytes on every platform, and M2 checks the C++ port byte-for-byte against goldens from `legacy/web-pack/core.js`. Two things break that without anyone noticing:
- **Compiler floating-point settings.** Fast math lets the compiler reorder and approximate. Contraction fuses `a*b+c` into one fused multiply-add (FMA), which rounds once instead of twice, and compilers do it on some targets and not others. A universal macOS binary holds an arm64 slice and an x86_64 slice, so the two halves could compute different bytes.
- **Line endings.** A Windows checkout can turn LF into CRLF, which changes the bytes of text goldens and breaks shell scripts.

Reproduced on 2026-10-28 with `double f(double a, double b, double c) { return a*b + c; }` at `-O2 -std=c++20`:

| Compiler and target | Default | With `-ffp-contract=off` |
|---|---|---|
| clang 18, `aarch64-linux-gnu` and `arm64-apple-macos11` | `fmadd` (fused) | `fmul` + `fadd` |
| clang 18, `x86_64-linux-gnu` and `x86_64-apple-macos11` | `mulsd` + `addsd` | `mulsd` + `addsd` |
| clang 18, `x86_64-apple-macos11` with `-mfma` | `vfmadd213sd` (fused) | `vmulsd` + `vaddsd` |
| GCC 13, x86_64 with `-mfma` (even under `-std=c++20`) | `vfmadd132sd` (fused) | `vmulsd` + `vaddsd` |

## Decision
- **No fast math anywhere:** no `-ffast-math`, `-Ofast` or `/fp:fast`.
- **No contraction:** `-ffp-contract=off` on GCC and Clang, and `/clang:-ffp-contract=off` on clang-cl. `/fp:precise` on the MSVC front end.
- These flags sit on the INTERFACE target `wubansia_build_defaults` in `cmake/WubansiaDefaults.cmake`. The JUCE module sources compiled into the shared-code target `Wubansia` get them too. That is intended. The exceptions keep the compiler default, as `build/release/compile_commands.json` shows on Linux: JUCE's per-format wrapper sources (`juce_audio_plugin_client_*`, compiled in `Wubansia_VST3`, `Wubansia_AU` and `Wubansia_Standalone`), `clap-juce-wrapper.cpp` (in `Wubansia_CLAP`) and the `clap_juce_extensions` library. None of them will hold `core/` code.
- **`.gitattributes`:** `* text=auto eol=lf` and `*.sh text eol=lf`; `*.bat` and `*.cmd` as `text eol=crlf`; `content/golden/** -text`; `*.mid`, `*.wav`, `*.flac` and `*.zip` as `binary`. Text checks out as LF on every OS, and goldens are never converted.

## Consequences
- Release builds lose a little speed where FMA would have helped. `engine/` may opt back into contraction later for DSP that isn't byte-checked, behind tolerance tests and its own ADR. `core/` never does.
- Tested on 2026-10-28: with this `.gitattributes`, a clone with `core.autocrlf=true` still checks out LF; without it, the same clone gives CRLF. Adding the file changes nothing already committed: `git ls-files --eol` shows no CRLF in the index.
- The M2 goldens are the real proof, on all three CI platforms.
- Not verified: Apple clang's default `-ffp-contract`, presumed to match LLVM's. The flag makes it irrelevant.

## Alternatives
- Pamplejuce's defaults (sudara/cmake-includes `5d75b1f`): `/fp:fast` and `-Ofast`, and `-ffast-math` on its newer main. Rejected (ADR 0001).
- Leaving contraction at the compiler default and comparing goldens with a tolerance: CLAUDE.md asks for identical bytes.
- `git config core.autocrlf false` on the Windows runner as well: redundant once `.gitattributes` exists, and it doesn't help local checkouts.

## Sources
- CLAUDE.md, "Determinism and golden files"; ROADMAP §5 M2.
- The table above: clang 18.1.3 and GCC 13.3.0 on Ubuntu 24.04, `-S` output.
- cmake-includes `5d75b1f` `SharedCodeDefaults.cmake:12-28`; `8de22da` `CHANGELOG.md` ("Fix fast math on macOS and Linux Release builds").
- runner-images `images/windows/scripts/build/Install-Git.ps1` (Git installed with the installer's defaults, no CRLF option set).
