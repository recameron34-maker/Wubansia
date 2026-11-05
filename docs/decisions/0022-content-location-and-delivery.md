# 0022. Content location and delivery

**Status:** Proposed, 2026-11-05 (draft, for the user's approval).

## Context
v0.1 plays the four round 5 kits from samples outside the plugin (ADR 0017). The plugin embeds each kit's two manifests and its MIDI, about 1 MB, so it builds, validates and shows its kits in CI without a single sample. Each kit's `kit.json` lists exactly the files it plays (`docs/plugin/manifest.md`, section 2.2): path, frames, channels and SHA-256. For the four kits that is 618 WAVs, 198 MB, out of about 333 MB in `content/samples` (Git LFS, ADR 0012): the reference loops (Club, Wet, dry, Low) aren't played, so they don't ship.

The user installs on a Mac and wants the least friction: unzip, run one command, open the DAW. The constraints:
- CI checks out without LFS, and this session can't create GitHub releases. Whether the account is Free or Pro is unknown.
- The bundles are signed ad hoc, not notarized (ADR 0004), so a browser download is quarantined.
- A Mac has no `python3` until Xcode's Command Line Tools are installed (and then it's 3.9), and its bash is 3.2.

## Decision

### Where the plugin looks
Per kit, the first of these that has it:
1. `$WUBANSIA_CONTENT_DIR`, for development, tests and `groovecore render`.
2. The folder chosen with "Choose content folder…" in the editor, saved per user in `~/Library/Application Support/Wubansia Audio/Wubansia/settings.xml`, never in projects.
3. The content folder: `~/Library/Application Support/Wubansia Audio/Wubansia/Content` on macOS (ROADMAP §3), `%APPDATA%\Wubansia Audio\Wubansia\Content` on Windows (JUCE's `userApplicationDataDirectory` there), and `~/.local/share/Wubansia Audio/Wubansia/Content` on Linux.
4. Debug builds only: the checkout's `content/samples`, set at configure time. Never in CI or Release builds.

Inside each, `<kit>/v<N>/` first, where N is the embedded `kit.json`'s `content_version`, then `<kit>/` (the repo's layout). Kit folders are lowercase (`fut01`). A kit is ready when every listed file exists, isn't a Git LFS pointer, and is a 48 kHz WAV with the listed frames and channels. The installers check SHA-256; loads don't. The locator is P9's `ContentLocator`; `tools/content/kitfiles.py` (`default_content_dir`) holds the same paths, and both follow this ADR.

### The content zips
One zip per kit and content version, `Wubansia-Content-<KIT>-v<N>.zip`, made by `python3 tools/content/pack.py --kit <KIT> --out <folder>`:
- `Wubansia Content/README.txt`: `dist/README.txt`, the same in every zip;
- `Wubansia Content/<kit>/v<N>/SHA256SUMS`: `<sha256>  <path>` a line, which `shasum -a 256 -c` reads;
- `Wubansia Content/<kit>/v<N>/<path>`: exactly the files `kit.json` lists, from `Drums/`, `Bass/`, `Music/`, `Vox/`, `FX/`, `Loops/` (the phrase stems) and `Stems/` (the derived stems, ADR 0020), and nothing else.

Before packing, every file is checked as the plugin checks it, and against `kit.json`'s SHA-256; the zip holds the bytes that were checked. Entries go folder by folder, dated 1980-01-01 with modes 644 and 755, deflated at level 9, so two runs give the same bytes. With the manifests at 8d6e102 the zips are 58.3 MB (FUT01), 45.4 MB (FUT02), 31.8 MB (FUT03) and 38.1 MB (FUT04): 173.7 MB, deflate saving about 12 %.

### How content reaches the Mac (an approval item; A is recommended)
- **A. The session sends the macOS CI zip and the four content zips.** You unzip the CI zip, put the content zips next to the `Wubansia` folder it makes, and run `bash install-macos.sh` in Terminal. The script:
  - copies `Wubansia.vst3`, `.component` and `.clap` into `~/Library/Audio/Plug-Ins/{VST3,Components,CLAP}` and `Wubansia.app` into `/Applications` (`~/Applications` when `/Applications` isn't writable), through a temporary copy and a swap;
  - clears `com.apple.quarantine` on each and checks it with `codesign --verify --strict`;
  - finds the content zips, or the "Wubansia Content" folders Safari leaves when it unzips them, in its own folder and the one above (or takes them as arguments), checks every file against `SHA256SUMS` in a staging folder inside the content folder, and moves each kit into `<kit>/v<N>/` by renaming;
  - restarts `AudioComponentRegistrar`, so AU hosts rescan;
  - prints every step. `--dry-run` checks everything and changes nothing; `--plugins-only` and `--content-only` do half.

  bash reads the script, so Gatekeeper doesn't stop it. It refuses `sudo` (it installs for your user) and runs only as a dry run on other systems. It is written for bash 3.2 and the BSD tools. `INSTALL.md`, in the zip, gives the same steps by hand. Or unzip the content anywhere and choose it with "Choose content folder…".
- **B. From your own clone:** `git lfs install && git lfs pull --include 'content/samples/**'`, then `python3 tools/content/install.py --from-repo`, or choose `content/samples` with "Choose content folder…".
- **C. A dispatch-only CI job:** `gh workflow run ci.yml --ref <branch> -f platforms=content`, never part of `all`. One job per kit restores `.git/lfs/objects` from a cache keyed by the hash of that kit's manifest, pulls only the files the manifest lists through LFS, packs the zip and keeps it for 3 days. The first run of a content version uses about 200 MB of LFS bandwidth, billed to the repo owner, and a cached run uses none. The zips take about 175 MB of artifact storage for 3 days (500 MB is included on Free, 1 GB on Pro). It needs your OK and your plan.
- **D. Later (S1c):** a FLAC content release with `fetch_content.py`, once releases can be made.

### What both installers do
`install-macos.sh` and `python3 tools/content/install.py (--from-repo [PATH] | --from-zip ZIP…) [--kit] [--dest] [--dry-run]` follow the same rules:
- Every file is checked before anything is written. A kit with any problem is left exactly as it was, and the message names the file: an LFS pointer (with the `git lfs pull` to run), a missing file, other bytes, another rate or frame count.
- A sample already in `<kit>/v<N>/` with other bytes is another build of that content version. They never replace it: they stop that kit, change nothing in it and say how to clear it. A sample already there with the right bytes stays, so a second run changes nothing. A later build that only adds files to a version adds them, and `SHA256SUMS` then lists that build.
- Each file goes in under a temporary name and is renamed into place, so there is never a half-written sample.
- Another content version gets its own `v<N>` folder, and no install touches another version's.
- `install.py` checks a zip against its `SHA256SUMS`, and against `kit.json` too when the checkout has that kit at that version. Both routes give the same folder, byte for byte (checked on the four real kits).

### Versions
`content_version` names a sample set. A plugin build embeds one version's manifests per kit and looks in `<kit>/v<N>/`. Once a version's zips reach you, any change to its files, their bytes or their list, ships as the next version with a plugin build that embeds its manifests; the versions sit side by side.

### The macOS CI zip
- It gains `INSTALL.md` and `install-macos.sh`.
- Before the upload, CI checks that it holds exactly `Wubansia/` with the four bundles, `THIRD_PARTY_NOTICES.md`, `INSTALL.md` and an executable `install-macos.sh`, that every bundle still passes `codesign --verify --strict` after packaging, and that the installer's dry run works on the runner.
- After the upload, CI gives the unpacked bundles a browser's quarantine flag, installs them for real with the script, and checks each installed bundle's signature and that the flag is gone. Then `tools/content`'s tests run on the runner's bash 3.2: dry runs, and a real content install into a fake home folder.
- `VERSION` goes from 0.0.1 to 0.1.0 (an approval item), so hosts rescan.

## Consequences
- You get one CI zip and a zip per kit, and run one command.
- Nothing new ships in the binaries. The zip's two new files are this repo's own, so `THIRD_PARTY_NOTICES.md` doesn't change. CI gains `actions/cache@v5` (MIT), used by the content job only.
- The tests run the installer for real only on the macOS runner (the plugins, and content into a fake home folder), and on Linux with stand-ins for `uname`, `xattr`, `codesign` and `killall`. bash 3.2 itself runs only there. The Python tools are tested on Python 3.13 and 3.9.
- Not verified from this session: Gatekeeper on a zip downloaded in a browser, the installed AU in Logic, Safari's names for the folders it unzips, the x86_64 slice.
- P9's `ContentLocator` uses the three default folders above and ignores the `SHA256SUMS` file in each `<kit>/v<N>/`; its tests should check both. `install.py` needs no `--dest` on any of the three systems.
- The manifests must be final before content zips go to you. plugin-p2's head lists one more file each for FUT01-FUT03 than 8d6e102, still as version 1. Either route would add those files to an installed version 1, but only zips from the final manifests match the plugin build.

## Alternatives
- **A signed `.pkg` installer:** needs a Developer ID and notarization (M7).
- **A `.command` file to double-click:** Gatekeeper blocks a quarantined script that isn't notarized.
- **The samples inside the bundles:** about 200 MB in each of the VST3, AU, CLAP and app, and every sample fix would need a new plugin build.
- **A Python installer for the zips:** a Mac has no `python3` until the Command Line Tools are installed.
- **A GitHub release for the content** (ADR 0012's alternative): this session can't create releases. That is D, later.
- **CI's checkout with `lfs: true`:** it pulls every LFS file, reference loops included (333 MB instead of 198 MB). `actions/checkout` narrows the LFS download only to a sparse checkout, whose file list would have to be known before the checkout.

## Sources
- `docs/plugin/manifest.md`, section 2.2 (`files[]`) and section 4 (versions); ADR 0004 (ad-hoc signing, packaging); ADR 0012 (samples in LFS); ADR 0017 (v0.1); ROADMAP §3 (the content folder).
- `actions/checkout` v7.0.1, `src/git-source-provider.ts:243-249` (no LFS fetch with a sparse checkout) and `src/git-command-manager.ts:669-673` (`GIT_LFS_SKIP_SMUDGE` without `lfs`).
- The Git LFS pointer format: https://github.com/git-lfs/git-lfs/blob/main/docs/spec.md.
