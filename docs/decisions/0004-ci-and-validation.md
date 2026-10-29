# 0004. CI and plugin validation

**Status:** Accepted, 2026-10-28.

## Context
M0 is done when CI is green on macOS and Windows, pluginval and auval pass, and the user loads the uploaded binaries in a DAW (ROADMAP §5). The repo is private, so Actions minutes cost money, and macOS costs about ten times Linux. The user approved the plan's defaults:
- **D3, budget:** the cost controls below. The user didn't say whether the account is GitHub Free or Pro, or whether a payment method with a budget is set up.
- **D4, macOS:** deployment target 11.0 and one universal arm64 + x86_64 build on `macos-15`. The user didn't say which macOS version or chip their DAW Mac has.
- **D6, where M0 runs:** in the cloud session. macOS and Windows are proven by CI only, and the user checks the CI artifacts in their DAW.
- **Update, 2026-10-29:** the user only needs macOS for now. Windows CI became manual-only (dispatch), and M0's Windows checks wait until the user asks for Windows. The Windows build setup (clang-cl, the 64-bit check, the static runtime) stays in place. Its first CI run was cancelled before it finished, so Windows has never been built.

## Decision
**Runners**, pinned by label:
- `linux`: `ubuntu-24.04` with clang 18 and lld. `ubuntu-latest` moves to 26.04 in November 2026, and 22.04 is deprecated. Clang because JUCE adds `-flto` to Release builds and lld can't link GCC's LTO objects.
- `macos`: `macos-15` (arm64, Xcode 16.4). Pamplejuce's `macos-14` is unsupported from 2026-11-02.
- `windows`: `windows-2025` (VS 2026 18.11, LLVM 20.1.8) with clang-cl, set up by `ilammy/msvc-dev-cmd@v1`. Fallback: `windows-2022`.

**Triggers and cost controls** (`.github/workflows/ci.yml`):
- `linux` runs on every push to any branch, except changes only to `docs/**`, `**/*.md` or `legacy/**`. It never runs on `pull_request`, so nothing runs twice.
- `macos` runs on ready (not draft) PRs to `main`, on pushes to `main` (which seeds the sccache entries every branch can restore), and on `workflow_dispatch` with `platforms: all|linux|macos|windows`. `windows` runs only on `workflow_dispatch` with `all` or `windows`. The same `paths-ignore` applies to PRs, judged on the PR's whole diff, so docs-only PRs cost nothing. If a job ever becomes a required check, `paths-ignore` must come off `pull_request`, or docs-only PRs wait forever on it.
- Three jobs with job-level `if`s, not a matrix: a job-level `if` can't read `matrix`.
- Concurrency group `${{ github.workflow }}-${{ github.event_name }}-${{ github.ref }}-${{ inputs.platforms }}` with `cancel-in-progress: true`, so a push doesn't cancel a dispatch run that is already billed, and dispatching one platform doesn't cancel a dispatch for another. Job timeouts 30 min (Linux) and 40 min (macOS, Windows); 5 min per validator download (curl also aborts a stalled transfer after 30 s and retries); 10 min per validator step. `permissions: contents: read`. `SCCACHE_GHA_ENABLED=true` and `SCCACHE_CACHE_MULTIARCH=1`, without which universal macOS compiles are never cached.
- sccache is pinned to v0.19.0 (`sccache-action` `version:`), like every other CI tool. The action checks each download against the `.sha256` published in the same sccache release.
- Estimate: about $0.61 per warm desktop run (macOS + Windows) and $1.37 cold; about $10 for all of M0, $15–20 worst case. Measured on the first run (2026-10-29): the cold macOS job took 5 min 11 s, about $0.31, so with Windows manual-only M0 should cost well under that estimate. Free includes $12 a month and Pro $18. Without a payment method, jobs stop once that is used up. If the account is Free, the user should add a payment method with a $20 Actions budget set to stop at the limit, or confirm Pro.

**Validators** (`tools/ci/fetch-validators.sh` checks each SHA-256; `tools/ci/validate.sh` runs the same steps in CI and locally):

| Archive | SHA-256 |
|---|---|
| pluginval v1.0.4 `pluginval_Linux.zip` | `c01c49d8063965c4c2dea8324468336768f5c9139e0b1caebde14c2400b55352` |
| pluginval v1.0.4 `pluginval_macOS.zip` | `3c4c533bda0c5059eea3ddaea752d757ee2025041f0f47e6bcb0e87f6082b29f` |
| pluginval v1.0.4 `pluginval_Windows.zip` | `c08e61ce3b96db41636f8ec7e76f4c7e2c13ebdac7fa1b5a1f52b4f32ec715ab` |
| clap-validator `clap-validator-0.4.1-127-g152b982-ubuntu-22.04.zip` | `49edadcfb407ea0dd946ce418300e853fbd2660fa4b0d00e4f19ff8eef24ad90` |
| clap-validator `…-macos-universal.zip` (for later) | `bbec8cd7d18274e549d5d8c12ece3cec54be966129388dd2e742b9957f2ba9f1` |
| clap-validator `…-windows.zip` (for later) | `d935c3af0a45c3911ea2e900f4aa5d6709dac82bb485f0c4ce28648ab2cd0c10` |

The clap-validator hashes match the digests on its release page. The pluginval page publishes no digests, so those three are trust-on-first-use: computed from our downloads, with sizes that match the page.

**Blocking checks:** ctest; pluginval `--strictness-level 10` on the VST3 (all OSes) and on the installed AU (macOS); `auval -strict -v aumu Wbns Wuba` (macOS). At strictness above 5, pluginval also runs `auval -strict -stress 20` itself.

**Report-only:** clap-validator 0.4.1 on Linux, as its own step with `continue-on-error`: `--only-failed --exclude 'state-reproducibility-.*'`. Those tests fail until clap-juce-extensions PR #193 merges (still open on 2026-10-28). After that (M3) it runs on all OSes and blocks.

**macOS ad-hoc re-sign, mandatory, in this order:** build → `codesign --force --sign -` on the VST3, AU, CLAP and app → `codesign --verify --strict --verbose=2` on each, which must pass → `lipo -archs` must show `x86_64 arm64` → install the signed AU → `killall -9 AudioComponentRegistrar || true` → poll `auval -a` for up to 60 s → validate → package. Without it: JUCE signs the VST3 before writing `moduleinfo.json` into it, so the signature is broken; the AU and app are only signed by JUCE's copy step, which CI doesn't run; and the CLAP is never signed.

**Artifacts:** one unsigned zip per desktop OS (`ditto` on macOS, `7z` on Windows) holding the plugins and `THIRD_PARTY_NOTICES.md`, which the bundled licenses require next to the binaries. Uploaded with `actions/upload-artifact@v7`, `archive: false`, kept 14 days. Linux uploads nothing.

**Later:** Dependabot comes in M7 with `interval: monthly`, one `groups:` block and `open-pull-requests-limit: 1`, because its PRs aren't drafts and would each run the paid matrix. Real signing and notarization come in M7, each step gated on a job-level `HAS_*` flag so CI stays green without secrets. No tag or release pipeline in M0.

## Consequences
- Draft PRs and branch pushes cost only Linux minutes. Desktop results arrive when a PR is marked ready, or by `gh workflow run ci.yml --ref <branch> -f platforms=macos` (works once the workflow has run on that branch).
- CI tests only the arm64 macOS slice natively; `lipo` checks that x86_64 exists. An Intel Mac in the DAW check would be its only native run.
- The x86_64 slice, Gatekeeper and SmartScreen behaviour, real times and costs are unverified until the first runs.

## Alternatives
- Pamplejuce's workflow as is: `macos-14`, no auval, and it fails without signing secrets.
- Two macOS runners (`macos-15` and `macos-15-intel`) merged with `lipo`: twice the setup cost and a third job.
- pluginval 2.0.0-beta3: not final. clap-validator blocking now: fails three known tests.

## Sources
- Runners: https://github.com/actions/runner-images README and issues #13518, #14254, #14748. Prices and quotas: `github/docs` at `57f571f`, `data/reusables/billing/actions-standard-runner-prices.md`, `actions-included-quotas.md`, `content/billing/concepts/product-billing/github-actions.md:141`, `content/billing/how-tos/set-up-budgets.md:34`. Caches and `if`: `content/actions/reference/workflows-and-actions/dependency-caching.md:243,247`, `data/reusables/actions/jobs/section-using-conditions-to-control-job-execution.md:4`.
- Re-sign: `libs/JUCE/extras/Build/CMake/JUCEUtils.cmake:1051,1166-1174,1285,1296`; `libs/clap-juce-extensions/cmake/ClapTargetHelpers.cmake:111-116`. LTO: `libs/JUCE/extras/Build/CMake/JUCEHelperTargets.cmake:158-162`.
- https://github.com/Tracktion/pluginval/releases/tag/v1.0.4 and `Source/tests/BasicTests.cpp:692` at that tag; https://github.com/free-audio/clap-validator/releases/tag/0.4.1 and `src/commands/validate.rs:34-46`; https://github.com/free-audio/clap-juce-extensions/pull/193.
- sccache `docs/Configuration.md` (`SCCACHE_CACHE_MULTIARCH`); `actions/upload-artifact@v7` `action.yml:48-53`.
