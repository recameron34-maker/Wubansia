# legacy/web-pack: the UKG & Minimal Pack web app

This is the web app as published (artifact version 21), with everything used to make it. It's the reference for the plugin: the C++ groove core must match `core.js` exactly, and the plugin's sound should match `app.js`. Change it only to export golden files or to load kits from `content/kits` (see `docs/ROADMAP.md`, M1).

## What's here

| Path | What |
|---|---|
| `core.js` | The 43 kits (data), the pattern compiler, `buildSong` (full songs), `buildExtended` (extended mixes) and the MIDI file writer. Deterministic: same kit, key and BPM give the same bytes. Works in Node (`require('./core.js')`) and the browser. |
| `app.js` | The page: WebAudio engine (about 30 synth voices, the `SAMPLER` multisample player, drum lanes, sidechain, sends), the UI, visualizers, pack zips. |
| `flac.js` | FLAC decoder used by the page to unpack the Ogg FLAC sprites (byte-identical to the original WAVs). |
| `ui/` | Page template (`app.src.html`), per-genre themes (`themes.py`), Sound lab (`lab.js`). |
| `build_site.py` | Assembles `site/index.html` from the files above and the manifests in `out/`. |
| `out/*.json` | Manifests: every sound's metadata (title, genre, key, BPM, pack path, waveform peaks) and where it sits in the FLAC sprites; also kit and song exports used by scripts. |
| `audio/` | Sound-design and vocal pipeline (numpy, scipy, pedalboard; Kokoro TTS and Whisper via sherpa-onnx): `hits.py`, `fx.py`, `keys.py`, `reese.py`, `vocals*.py`, `render_*.py`, and the packers (`pack_timesprite.py`, `pack_vox_flac.py`, `pack_bundle.py`). |
| `inst/` | Instrument renderers (`render_inst.py`, `render_gh.py`, `render_w5.py`, `render_w8*.py`), the Decent Sampler/SFZ library builder (`build_lib.py`, `dslib.py`, `readme_lib.py`, `lint.py`), metadata (`*_meta.json`) and fonts. |
| `validate.js`, `validate_ext.js` | Song checks: in-key notes, note ranges, chord tones on strong beats, MIDI layout. |
| `export_*.js` | Small exporters (kits, leads, wave-8 parts) used by the Python renderers. |
| `tests/` | jsdom smoke tests of the page (`smoke3/5/6/7/8.js`), bit-exact checks (`plain_check`, `sprite_check`, `bundle_check`, `verify_flac`), a Playwright real-browser harness (`browser/`), and `sheet.py` (spectrogram sheets for listening QA). |
| `history/` | One-off generators and patch scripts from the build-up of the pack. Kept for provenance; paths still point at the original workspace. Not maintained. |

Paths: every script finds this folder by itself. Override with `PACK_ROOT=/path/to/legacy/web-pack` if needed. Vocal scripts find the speech models in `models/` (override with `MODELS_DIR`).

## Getting the audio (not in git)

Download from the [`web-pack-v21` release](https://github.com/recameron34-maker/Wubansia/releases/tag/web-pack-v21) and unzip each file into this folder:

| Asset | Unzips to | Contents |
|---|---|---|
| `web-pack-site-v21.zip` | `site/` | The published page and all its audio (Ogg FLAC sprites, ~200 MB) |
| `instrument-library.zip` | `inst/lib/` | Decent Sampler presets, SFZ files and the sampled instruments as WAVs (with loop points), including the groove/tech house renders |
| `dev-data.zip` | `audio/lex/` | Pronunciation lexicons used by the vocal scripts |

```sh
gh release download web-pack-v21 --repo recameron34-maker/Wubansia
for z in web-pack-site-v21.zip instrument-library.zip dev-data.zip; do unzip -o "$z" -d legacy/web-pack; done
shasum -a 256 -c SHA256SUMS.txt   # optional integrity check (run in the download folder)
```

Speech models (only needed to re-render vocals), from the sherpa-onnx releases, unpacked into `models/`:
- `kokoro-multi-lang-v1_0`: https://github.com/k2-fsa/sherpa-onnx/releases/download/tts-models/kokoro-multi-lang-v1_0.tar.bz2
- `sherpa-onnx-whisper-base.en`: https://github.com/k2-fsa/sherpa-onnx/releases/download/asr-models/sherpa-onnx-whisper-base.en.tar.bz2

## Running it

```sh
pip install -r legacy/web-pack/requirements.txt       # Python 3.11+
python3 legacy/web-pack/build_site.py                 # rebuilds site/index.html (byte-identical to v21)
cd legacy/web-pack && node validate.js && node validate_ext.js
cd tests && npm install && QUICK=1 node smoke8.js && node smoke5.js
python3 -m http.server -d legacy/web-pack/site 8000   # then open http://localhost:8000
```

Outside the claude.ai artifact host, playback and browsing work but saving and comments don't: those use the host's runtime capabilities (`window.claude`). `tests/browser/harness.py` shows a small stand-in.

The validators report four known findings: GH02's borrowed Ebmaj7 in its breakdown, and three out-of-range pad and counter notes in GH02 and GH03. All four are deliberate or harmless.

Test notes:
- `smoke5.js` (garage house) and `QUICK=1 smoke8.js` (groove and tech house) need only the site audio.
- The other smoke tests and the bit-exact checks also compare against the original 16-bit staging WAVs, which aren't in the repo. Without them, those checks fail with missing-file errors or skip. The FLAC sprites already hold the same audio losslessly.
- `tests/browser/w8_browser.py` (Playwright) plays kits in headless Chromium, measures levels through an analyser and takes screenshots into `out/shots/`.

## Re-rendering sounds

The renderers write WAVs at 44.1 kHz / 16-bit (`SR` in `audio/dsp.py`). The HD re-render planned in M1b makes sample rate and bit depth parameters and moves this pipeline into `tools/`. Typical flow for one wave:

```sh
python3 inst/render_w8.py kits inst      # drum hits + multisampled instruments
python3 inst/render_w8_loops.py          # loops from the kit MIDI
python3 inst/render_w8_fx.py             # FX
python3 audio/render_wave8.py            # pack into site/audio/s8 + out/manifest_wave8.json
python3 build_site.py
```
