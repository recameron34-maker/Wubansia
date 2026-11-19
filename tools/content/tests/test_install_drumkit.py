"""dist/INSTALL.md's "Drum kits into your DAW" (ADR 0030), held to the button and the drum kit texts it saves.

The section names the button "Drum kit" and what its folder holds (`<KIT> <Title> Drum Kit`, Pads/, Samples/, the .sfz
and README.txt); "Where everything goes", "If something's wrong" and "Uninstall" mention the drum kits; and each committed
drum kit README (plugin/tests/data/drumkit/, the texts the plugin writes) names, pad by pad, the same notes and sounds as
INSTALL.md's drum pad table.

From the repo root: python3 -m unittest discover -s tools/content/tests -t .
Standard library only, like the rest of tools/content/tests.
"""
import re
import unittest
from pathlib import Path

from .test_install_drums import INSTALL_MD, KITS, cell_sounds, flat, note_name, section, table

REPO = Path(__file__).resolve().parents[3]
DATA = REPO / 'plugin' / 'tests' / 'data' / 'drumkit'


def readme_pads(text):
    """A drum kit README's pad table: {key: set of sounds}. Rows are "  36 C1   Kick Garage   0 dB   centre   one-shot"."""
    pads = {}
    body = text.split('THE PADS\n', 1)[1].split('\n\n', 2)[1]
    lines = body.splitlines()
    assert lines[0].split()[:2] == ['Note', 'Sound'], lines[0]
    for line in lines[1:]:
        cells = re.split(r' {2,}', line.strip())
        key, note = cells[0].split()
        assert note == note_name(int(key)), line
        pads.setdefault(int(key), set()).add(cells[1])
    return pads


class DrumKitsSection(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.text = INSTALL_MD.read_text(encoding='utf-8')
        cls.kits = section(cls.text, 'Drum kits into your DAW')
        cls.drag = section(cls.text, 'Drag MIDI into Live or Logic')

    def test_the_section_comes_after_the_midi_drags_and_before_if_somethings_wrong(self):
        self.assertIsNotNone(self.kits, 'no "## Drum kits into your DAW" in INSTALL.md')
        headings = re.findall(r'^## (.*)$', self.text, re.M)
        at = headings.index('Drum kits into your DAW')
        self.assertEqual(headings[at - 1], 'Drag MIDI into Live or Logic')
        self.assertEqual(headings[at + 1], "If something's wrong")

    def test_it_names_the_button_and_what_the_folder_holds(self):
        words = flat(self.kits or '')
        self.assertIn('**Drum kit**', words)
        self.assertIn('FUT01 Alloy Signal Drum Kit/', words)
        for part in ('`Pads/`', '`Samples/`', '`.sfz`', '`README.txt`', '36 C1 Kick Garage - FUT01.wav',
                     '36 C1 Kick Garage v1 rr1 - FUT01.wav', 'Kick Garage.sfz', 'Kick Split.sfz', 'Drum Kit (2)'):
            self.assertIn(part, words)
        self.assertIn('never changes or deletes', words)

    def test_where_everything_goes_lists_the_drum_kits(self):
        where = flat(section(self.text, 'Where everything goes') or '')
        self.assertIn('<KIT> <Title> Drum Kit/', where)
        self.assertIn('Drum kit', where)

    def test_if_somethings_wrong_and_uninstall_mention_them(self):
        wrong = flat(section(self.text, "If something's wrong") or '')
        self.assertIn("**Drum kit says Couldn't save.**", wrong)
        uninstall = flat(section(self.text, 'Uninstall') or '')
        self.assertIn('drum kits', uninstall)

    def test_each_readme_names_the_notes_and_sounds_of_the_drum_pad_table(self):
        header, rows = table(self.drag)
        self.assertEqual(header[1:], list(KITS))
        for col, kit_id in enumerate(KITS, start=1):
            with self.subTest(kit=kit_id):
                folders = sorted(DATA.glob(f'{kit_id} * Drum Kit*'))
                self.assertEqual(len(folders), 1, folders)
                readme = readme_pads((folders[0] / 'README.txt').read_text(encoding='utf-8'))
                installed = {}
                for row in rows:
                    key = int(re.search(r'\((\d+)\)', row[0]).group(1))
                    names, _paths = cell_sounds(row[col])
                    if names:
                        installed[key] = set(names)
                self.assertEqual(readme, installed)


if __name__ == '__main__':
    unittest.main()
