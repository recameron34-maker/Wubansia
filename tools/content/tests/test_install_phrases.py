"""The Phrases drawer (ADR 0031) in dist/INSTALL.md.

The kit's own phrases and its lead line left the Drag MIDI strip for the header's Phrases drawer: the drag section must
not send anyone to a phrase chip, and a section of its own, after the Samples drawer's, must say where they are now and
what a drag and a click do there. The library (part 2): the drawer's words, a click that plays (no longer one that
shows the file), More, the star, Stop, and the name a drag gives.

From the repo root: python3 -m unittest discover -s tools/content/tests -t .
Standard library only, like the rest of tools/content/tests.
"""
import unittest

from tools.content.tests.test_install_drums import INSTALL_MD, flat, section

DRAG = 'Drag MIDI into Live or Logic'
SAMPLES = 'Samples into your DAW'
PHRASES = 'Phrases into your DAW'


class TestPhrasesDrawer(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.text = INSTALL_MD.read_text(encoding='utf-8')

    def test_the_drag_section_has_no_phrase_chips(self):
        drag = section(self.text, DRAG)
        self.assertIsNotNone(drag, f'INSTALL.md has no "## {DRAG}" section')
        self.assertNotIn('phrase chip', flat(drag).lower())

    def test_a_phrases_section_names_the_drawer(self):
        phrases = section(self.text, PHRASES)
        self.assertIsNotNone(phrases, f'INSTALL.md has no "## {PHRASES}" section')
        headings = [line.strip() for line in self.text.splitlines() if line.startswith('## ')]
        self.assertIn(f'## {SAMPLES}', headings)
        self.assertGreater(headings.index(f'## {PHRASES}'), headings.index(f'## {SAMPLES}'),
                           f'"## {PHRASES}" comes after "## {SAMPLES}"')
        body = flat(phrases)
        for needed in ('**Phrases**', 'From the kit'):
            with self.subTest(needed=needed):
                self.assertIn(needed, body)
        self.assertIn('drag', body.lower())
        self.assertNotIn('show the file', body.lower())  # a click plays it now

    def test_the_phrases_section_names_the_drawer_s_words(self):
        body = flat(section(self.text, PHRASES) or '')
        for needed in ('Hooks', 'Call & answer', 'Periods', 'Sparse', 'Arps', 'Stabs', 'Counter-lines', 'Bounce', 'From the kit',
                       '**More**', 'Kept', 'Stop', 'For my synths', '64 per kit', 'Click one to hear it',
                       '`FUT01 Alloy Signal 134 F#m 11A - Phrase Night Bus Hook 8 bars (synths).mid`'):
            with self.subTest(needed=needed):
                self.assertIn(needed, body)


if __name__ == '__main__':
    unittest.main()
