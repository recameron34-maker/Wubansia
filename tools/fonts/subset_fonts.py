#!/usr/bin/env python3
"""Build the looks' thirteen embedded faces (ADR 0032) from their Google Fonts sources.

Instances the variable fonts at the weights the skins use, subsets every face to the UI's characters with kern (and
liga for the proportional faces), drops hinting, keeps the licence name records, and gives every family a "Wubansia "
prefix, so the process-wide registration can't clash with a font the person has installed. IBM Plex Mono becomes
"Wubansia HUD Mono": "Plex" is its OFL Reserved Font Name, which a modified version may not carry.

Refuses a source whose SHA-256 differs from tools/fonts/fonts.lock.json. Two runs give byte-identical files.
Needs fontTools 4.55.3 (tools/fonts/requirements.txt); run by hand only, never in CI or the build.

usage: subset_fonts.py <sources dir> <out dir> [--update-lock]
"""
import hashlib
import json
import os
import sys

from fontTools import subset
from fontTools.ttLib import TTFont
from fontTools.varLib import instancer

HERE = os.path.dirname(os.path.abspath(__file__))
LOCK = os.path.join(HERE, "fonts.lock.json")

# What the skins draw: printable ASCII, the middle dot and the ellipsis the editor's strings use, plus Latin-1 and
# common punctuation for the drawers' run-time text.
UI_REQUIRED = [chr(c) for c in range(0x20, 0x7F)] + ["·", "…"]
EXTRA = ([chr(c) for c in range(0xA0, 0x100)]
         + ["Œ", "œ", "–", "—", "‘", "’", "‚", "“", "”",
            "„", "•", "′", "″", "←", "↑", "→", "↓", "−"])

# (output, source, variable axes or None, layout features, family)
FACES = [
    ("Archivo-Regular.ttf", "Archivo[wdth,wght].ttf", {"wght": 400, "wdth": 100}, ["kern", "liga"], "Wubansia Archivo"),
    ("Archivo-ExtraBold.ttf", "Archivo[wdth,wght].ttf", {"wght": 800, "wdth": 100}, ["kern", "liga"], "Wubansia Archivo"),
    ("JetBrainsMono-Regular.ttf", "JetBrainsMono[wght].ttf", {"wght": 400}, ["kern"], "Wubansia JetBrains Mono"),
    ("ChakraPetch-Regular.ttf", "ChakraPetch-Regular.ttf", None, ["kern", "liga"], "Wubansia Chakra Petch"),
    ("ChakraPetch-Bold.ttf", "ChakraPetch-Bold.ttf", None, ["kern", "liga"], "Wubansia Chakra Petch"),
    ("WubansiaHudMono-Regular.ttf", "IBMPlexMono-Regular.ttf", None, ["kern"], "Wubansia HUD Mono"),
    ("Manrope-Regular.ttf", "Manrope[wght].ttf", {"wght": 400}, ["kern", "liga"], "Wubansia Manrope"),
    ("Manrope-SemiBold.ttf", "Manrope[wght].ttf", {"wght": 600}, ["kern", "liga"], "Wubansia Manrope"),
    ("Manrope-ExtraBold.ttf", "Manrope[wght].ttf", {"wght": 800}, ["kern", "liga"], "Wubansia Manrope"),
    ("BarlowCondensed-SemiBold.ttf", "BarlowCondensed-SemiBold.ttf", None, ["kern", "liga"], "Wubansia Barlow Condensed"),
    ("BarlowCondensed-Bold.ttf", "BarlowCondensed-Bold.ttf", None, ["kern", "liga"], "Wubansia Barlow Condensed"),
    ("BarlowCondensed-ExtraBold.ttf", "BarlowCondensed-ExtraBold.ttf", None, ["kern", "liga"], "Wubansia Barlow Condensed"),
    ("SpaceMono-Regular.ttf", "SpaceMono-Regular.ttf", None, ["kern"], "Wubansia Space Mono"),
]


def sha256(path):
    with open(path, "rb") as f:
        return hashlib.sha256(f.read()).hexdigest()


def rename(font, family):
    """The family names become family, the style kept; the PostScript names follow."""
    names = font["name"]
    style = names.getDebugName(17) or names.getDebugName(2) or "Regular"
    ps = family.replace(" ", "") + "-" + style.replace(" ", "")
    for record in names.names:
        if record.nameID in (1, 16):
            record.string = family
        elif record.nameID in (2, 17):
            record.string = style
        elif record.nameID == 4:
            record.string = family + " " + style
        elif record.nameID == 3:
            record.string = ps + ";subset"
        elif record.nameID == 6:
            record.string = ps
    if "CFF " in font:
        raise SystemExit("CFF rename not handled")


def main():
    if len(sys.argv) < 3:
        raise SystemExit(__doc__)
    src, out = sys.argv[1], sys.argv[2]
    update = "--update-lock" in sys.argv[3:]
    with open(LOCK, encoding="utf-8") as f:
        lock = json.load(f)
    sources = {s["file"]: s for s in lock["sources"]}
    os.makedirs(out, exist_ok=True)
    outputs = []
    for name, source, axes, features, family in FACES:
        path = os.path.join(src, source)
        if sha256(path) != sources[source]["sha256"]:
            raise SystemExit("%s: SHA-256 differs from fonts.lock.json; refusing" % source)
        font = TTFont(path, recalcTimestamp=False)
        if axes is not None:
            font = instancer.instantiateVariableFont(font, axes, updateFontNames=True)
        options = subset.Options()
        options.layout_features = features
        options.hinting = False
        options.name_IDs = [0, 1, 2, 3, 4, 5, 6, 13, 14, 16, 17]
        options.name_languages = [0x0409]
        options.notdef_outline = True
        options.glyph_names = False
        options.drop_tables += ["DSIG", "STAT", "MVAR", "HVAR", "fvar", "gvar", "avar"]
        subsetter = subset.Subsetter(options)
        subsetter.populate(text="".join(UI_REQUIRED + EXTRA))
        subsetter.subset(font)
        rename(font, family)
        cmap = font.getBestCmap()
        missing = [c for c in UI_REQUIRED if ord(c) not in cmap]
        if missing:
            raise SystemExit("%s lacks %r" % (name, "".join(missing)))
        if any("Plex" in str(n.toUnicode()) for n in font["name"].names):
            raise SystemExit("%s still names Plex" % name)
        target = os.path.join(out, name)
        font.save(target)
        digest = sha256(target)
        outputs.append({"file": name, "source": source, "family": family, "sha256": digest})
        print("%-32s %7d B  glyphs %4d  %s" % (name, os.path.getsize(target), len(font.getGlyphOrder()), digest))
    print("TOTAL %d B" % sum(os.path.getsize(os.path.join(out, o["file"])) for o in outputs))
    if update:
        lock["outputs"] = outputs
        with open(LOCK, "w", encoding="utf-8") as f:
            json.dump(lock, f, indent=2, ensure_ascii=False)
            f.write("\n")
    else:
        recorded = {o["file"]: o["sha256"] for o in lock.get("outputs", [])}
        for o in outputs:
            if recorded.get(o["file"]) != o["sha256"]:
                print("note: %s differs from the lock" % o["file"])


if __name__ == "__main__":
    main()
