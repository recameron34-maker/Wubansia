# The looks' fonts (ADR 0032)

`plugin/resources/fonts/` holds the thirteen faces the editor's four looks draw with: subsets of seven SIL OFL 1.1
families from Google Fonts, built into the plugin as binary data (`WubansiaFonts`, `plugin/CMakeLists.txt`). They are
build input, committed as they are; nothing here runs in CI or the build.

| Look | Faces |
|---|---|
| Night bus (FUT01) | Archivo Regular and ExtraBold, JetBrains Mono Regular |
| Mission HUD (FUT02) | Chakra Petch Regular and Bold, Wubansia HUD Mono (IBM Plex Mono) Regular |
| Deep glass (FUT03) | Manrope Regular, SemiBold and ExtraBold |
| Machine room (FUT04) | Barlow Condensed SemiBold, Bold and ExtraBold, Space Mono Regular |

## Rebuilding them

You need the network (to fetch the sources) and fontTools 4.55.3 (MIT), in a venv of its own:

```
python3 -m venv /tmp/fonts-venv && /tmp/fonts-venv/bin/pip install -r tools/fonts/requirements.txt
mkdir -p /tmp/fonts-src && cd /tmp/fonts-src
# each source's url in tools/fonts/fonts.lock.json, e.g.
curl -LO 'https://raw.githubusercontent.com/google/fonts/main/ofl/archivo/Archivo%5Bwdth,wght%5D.ttf'
cd - && /tmp/fonts-venv/bin/python -I tools/fonts/subset_fonts.py /tmp/fonts-src /tmp/fonts-out
```

The script refuses a source whose SHA-256 differs from the lock (Google Fonts updates its files in place: a new source
is a deliberate change, with its new hash, the new subsets and their hashes in the same commit). It instances the
variable fonts at the weights the looks use, keeps the UI's characters (printable ASCII, Latin-1, `·`, `…` and some
punctuation), `kern` (and `liga` for the proportional faces), no hinting, and the copyright and licence name records.
Every family gets a "Wubansia " prefix, so the process-wide registration can't clash with an installed font of the
same name; IBM Plex Mono becomes "Wubansia HUD Mono", since "Plex" is its Reserved Font Name.

Two runs give byte-identical files. Compare the output with `plugin/resources/fonts/` (or run with `--update-lock`
to write the new output hashes into the lock), copy the files in, and run
`python3 -m unittest discover -s tools/content/tests -t .` (`test_fonts.py` checks the lock, the names and the
notices). The licences: `THIRD_PARTY_NOTICES.md` and `docs/third_party/fonts/`.
