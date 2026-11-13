"""dist/INSTALL.md's "Drag MIDI into Live or Logic" (ADR 0025), held to the kits.

Its drum pad table is the only drum map the downloads carry (the kits' MIDI READMEs stay in the repository), so it
must give every note each kit's drums play, with the sounds on it and where their files are, read here from each
kit's kit.json (content/plugin/<kit>/<KIT>_kit.json, the manifest the plugin embeds): its channel-10 keymap, the
instruments it names and their zones' files.

From the repo root: python3 -m unittest discover -s tools/content/tests -t .
Standard library only, like the rest of tools/content/tests.
"""
import json
import re
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[3]
INSTALL_MD = REPO / 'dist' / 'INSTALL.md'
KITS = ('FUT01', 'FUT02', 'FUT03', 'FUT04')
NOTE_NAMES = ('C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B')
DRUM_CHANNEL = 10


def note_name(key):
    """A MIDI key as Live and Logic name it, middle C (60) being C3: 36 is C1, 54 is F#2."""
    return f'{NOTE_NAMES[key % 12]}{key // 12 - 2}'


def section(text, heading, level=2):
    """The body of a Markdown heading's section, up to the next heading of its level or above; None when there is
    none."""
    hashes = '#' * level
    m = re.search(rf'^{hashes} {re.escape(heading)}[ \t]*\n(.*?)(?=^#{{1,{level}}} |\Z)', text, re.M | re.S)
    return m.group(1) if m else None


def flat(text):
    """Text with its line breaks and indents read as single spaces, so a phrase reads the same however it wraps."""
    return ' '.join(text.split())


def sound_name(path):
    """A sample's sound, as the files name it: "Drums/Conga G3 v2 rr1.wav" is "Conga G3", "Bass/Rumble.wav" "Rumble"."""
    stem = path.rsplit('/', 1)[-1].removesuffix('.wav')
    return re.sub(r'( v\d+)?( rr\d+)?$', '', stem)


def drum_map(kit_id):
    """The kit's drum notes: {key: {sound name: folder}}, every sound its channel-10 keymap puts on the key (both
    sides of an A/B switch), named by the files of the zone that plays the key."""
    kit = json.loads((REPO / 'content' / 'plugin' / kit_id.lower() / f'{kit_id}_kit.json').read_text(encoding='utf-8'))
    instruments = {i['id']: i for i in kit['instruments']}
    files = kit['files']
    out = {}
    for entry in kit['keymap']:
        if entry['ch'] != DRUM_CHANNEL:
            continue
        lo, hi = entry['keys']
        for key in range(lo, hi + 1):
            for side in ('a', 'b'):
                if not entry[side]:
                    continue
                zones = [z for z in instruments[entry[side]]['zones'] if z['lo'] <= key <= z['hi']]
                paths = {files[r]['path'] for z in zones for layer in z['layers'] for r in layer['rr']}
                for path in paths:
                    out.setdefault(key, {})[sound_name(path)] = path.rsplit('/', 1)[0]
    return out


def table(text):
    """The first Markdown table in text: (header cells, rows of cells)."""
    lines = [line.strip() for line in text.splitlines() if line.strip().startswith('|')]
    cells = [[c.strip() for c in line.strip('|').split('|')] for line in lines]
    return (cells[0], [row for row in cells[2:]]) if len(cells) > 2 else ([], [])


class TestDrumPads(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.text = INSTALL_MD.read_text(encoding='utf-8')
        cls.drag = section(cls.text, 'Drag MIDI into Live or Logic')
        cls.maps = {kit: drum_map(kit) for kit in KITS}

    def test_the_kits_play_their_drums_on_channel_10(self):
        # What the table is read from: a few notes every kit has, so an empty map can't pass the tests below.
        for kit, notes in self.maps.items():
            with self.subTest(kit=kit):
                self.assertIn(36, notes)
                self.assertIn(38, notes)
                self.assertIn(42, notes)

    def test_the_table_gives_every_drum_note_of_every_kit_and_nothing_else(self):
        self.assertIsNotNone(self.drag, 'INSTALL.md has no "## Drag MIDI into Live or Logic"')
        header, rows = table(self.drag)
        self.assertEqual(header, ['Note', *KITS])
        keys = sorted({key for notes in self.maps.values() for key in notes})
        self.assertEqual([row[0] for row in rows], [f'{note_name(key)} ({key})' for key in keys])
        for row, key in zip(rows, keys):
            for kit, cell in zip(KITS, row[1:]):
                with self.subTest(note=row[0], kit=kit):
                    sounds = self.maps[kit].get(key, {})
                    if not sounds:
                        self.assertEqual(cell, '')
                    for sound, folder in sounds.items():
                        self.assertIn(sound, cell)
                        if folder != 'Drums':  # the paragraph says Drums/; anything else is named in its cell
                            self.assertIn(f'`{folder}/{sound}.wav`', cell)

    def test_it_says_where_the_files_are_and_what_their_names_mean(self):
        self.assertIsNotNone(self.drag, 'INSTALL.md has no "## Drag MIDI into Live or Logic"')
        text = flat(self.drag)
        for needed in ('Content/<kit>/v1/Drums/', '`v1` the softest', 'rr1', 'content/midi/<kit>/README.md'):
            with self.subTest(needed=needed):
                self.assertTrue(needed in text, f'the drag section never says {needed!r}')
        # The sounds in Drums/ that are on no drum note: each named, the Accent files as one group.
        for kit in KITS:
            files = json.loads((REPO / 'content' / 'plugin' / kit.lower() / f'{kit}_kit.json').read_text(encoding='utf-8'))['files']
            mapped = {sound for sounds in self.maps[kit].values() for sound in sounds}
            others = {sound_name(f['path']) for f in files if f['path'].startswith('Drums/')} - mapped
            for sound in sorted(others):
                with self.subTest(kit=kit, sound=sound):
                    named = 'Accent' if sound.startswith('Accent ') else sound
                    self.assertTrue(named in text, f'the drag section never names {named!r}, which is on no drum note')

    def test_the_logic_steps_name_the_notes_outside_c1_to_d_sharp_2(self):
        # A Drum Rack shows the 16 pads from C1 to D#2 (36-51) first; the notes outside them each get a step in Logic too.
        # (Looked up inside the drag section: "Samples into your DAW" has a Logic part of its own.)
        self.assertIsNotNone(self.drag, 'INSTALL.md has no "## Drag MIDI into Live or Logic"')
        logic = section(self.drag, 'In Logic Pro', level=3)
        self.assertIsNotNone(logic, 'the drag section has no "### In Logic Pro"')
        logic = flat(logic)
        outside = sorted({key for notes in self.maps.values() for key in notes if not 36 <= key <= 51})
        self.assertEqual(outside, [35, 54])
        for key in outside:
            with self.subTest(note=note_name(key)):
                self.assertTrue(f'{note_name(key)} ({key})' in logic, f'the Logic steps never name {note_name(key)} ({key})')

    def test_the_live_steps_say_the_drop_writes_over_the_tracks_after_and_how_to_drop_safely(self):
        # Live puts a dropped file's other tracks on the tracks after the one it lands on, over their clips: the Song
        # brings 7 to 10 more after Drums. The steps say so, and give three drops that write over none of yours.
        self.assertIsNotNone(self.drag, 'INSTALL.md has no "## Drag MIDI into Live or Logic"')
        live = section(self.drag, 'In Ableton Live', level=3)
        self.assertIsNotNone(live, 'the drag section has no "### In Ableton Live"')
        live = flat(live)
        for needed in ('replaces what it covers',  # the warning
                       'a new set', 'below your last track',  # nothing of yours after the drop
                       'six other lanes'):  # the drums alone
            with self.subTest(needed=needed):
                self.assertTrue(needed in live, f'the Live steps never say {needed!r}')


if __name__ == '__main__':
    unittest.main()
