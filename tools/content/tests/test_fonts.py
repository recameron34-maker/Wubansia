"""The looks' embedded fonts (ADR 0032): plugin/resources/fonts against tools/fonts/fonts.lock.json, their licences and
the IBM Plex Reserved Font Name.

From the repo root: python3 -m unittest discover -s tools/content/tests -t .

Every face the plugin builds in is listed in the lock with its SHA-256 (and the lock lists no other), no name record
of any face carries "Plex" (the OFL's Reserved Font Name: a subset is a Modified Version), every family carries the
"Wubansia" prefix, and every family has its row and copyright line in THIRD_PARTY_NOTICES.md and its OFL text in
docs/third_party/fonts/. A minimal name-table reader: no fontTools needed, so CI runs it as it is.
"""
import hashlib
import json
import os
import struct
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
FONTS = os.path.join(ROOT, "plugin", "resources", "fonts")
LOCK = os.path.join(ROOT, "tools", "fonts", "fonts.lock.json")
NOTICES = os.path.join(ROOT, "THIRD_PARTY_NOTICES.md")
OFL_DIR = os.path.join(ROOT, "docs", "third_party", "fonts")


def name_records(path):
    """Every name record of a TrueType file's name table, decoded, as (nameID, text)."""
    with open(path, "rb") as f:
        data = f.read()
    num_tables = struct.unpack(">H", data[4:6])[0]
    for i in range(num_tables):
        tag, _, offset, _ = struct.unpack(">4sIII", data[12 + 16 * i: 28 + 16 * i])
        if tag == b"name":
            break
    else:
        raise AssertionError("%s has no name table" % path)
    _, count, strings = struct.unpack(">HHH", data[offset: offset + 6])
    records = []
    for r in range(count):
        platform, encoding, _, name_id, length, start = struct.unpack(
            ">HHHHHH", data[offset + 6 + 12 * r: offset + 18 + 12 * r])
        raw = data[offset + strings + start: offset + strings + start + length]
        text = raw.decode("utf-16-be") if platform in (0, 3) else raw.decode("latin-1")
        records.append((name_id, text))
    return records


def sha256(path):
    with open(path, "rb") as f:
        return hashlib.sha256(f.read()).hexdigest()


class FontsTest(unittest.TestCase):
    def setUp(self):
        with open(LOCK, encoding="utf-8") as f:
            self.lock = json.load(f)
        self.files = sorted(n for n in os.listdir(FONTS) if n.endswith(".ttf"))

    def test_every_font_is_in_the_lock_with_its_hash_and_the_lock_lists_no_other(self):
        outputs = {o["file"]: o for o in self.lock["outputs"]}
        self.assertEqual(13, len(self.files))
        self.assertEqual(sorted(outputs), self.files)
        for name in self.files:
            self.assertEqual(outputs[name]["sha256"], sha256(os.path.join(FONTS, name)), name)
        sources = {s["file"] for s in self.lock["sources"]}
        for o in outputs.values():
            self.assertIn(o["source"], sources, o["file"])
        for s in self.lock["sources"]:
            self.assertRegex(s["sha256"], "^[0-9a-f]{64}$")
            self.assertTrue(s["url"].startswith("https://raw.githubusercontent.com/google/fonts/"), s["url"])

    def test_no_name_record_carries_plex_and_every_family_carries_wubansia(self):
        for name in self.files:
            records = name_records(os.path.join(FONTS, name))
            self.assertTrue(records, name)
            for name_id, text in records:
                self.assertNotIn("Plex", text, "%s name %d" % (name, name_id))
            families = {text for name_id, text in records if name_id in (1, 16)}
            self.assertTrue(families, name)
            for family in families:
                self.assertTrue(family.startswith("Wubansia "), "%s: %s" % (name, family))
            self.assertTrue(any(name_id == 13 for name_id, _ in records), "%s keeps its licence record" % name)

    def test_every_family_has_its_notice_and_its_licence_text(self):
        with open(NOTICES, encoding="utf-8") as f:
            notices = f.read()
        self.assertIn("SIL Open Font License", notices)
        self.assertIn("Wubansia HUD Mono", notices)
        self.assertIn('Reserved Font Name "Plex"', notices)
        for family in self.lock["families"]:
            self.assertIn(family["name"], notices)
            self.assertIn(family["copyright"], notices, family["name"])
            ofl = os.path.join(OFL_DIR, family["ofl"])
            self.assertTrue(os.path.isfile(ofl), ofl)
            with open(ofl, encoding="utf-8") as f:
                text = f.read()
            self.assertIn("SIL OPEN FONT LICENSE Version 1.1", text.upper().replace("  ", " "))
            self.assertIn(family["copyright"], text)
        self.assertEqual(7, len(self.lock["families"]))


if __name__ == "__main__":
    unittest.main()
