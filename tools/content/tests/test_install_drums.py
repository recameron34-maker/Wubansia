"""dist/INSTALL.md's "Drag MIDI into Live or Logic" (ADR 0025), held to the kits.

Its drum pad table is the only drum map the downloads carry (the kits' MIDI READMEs stay in the repository), so it
must give every note each kit's drums play, with the sounds on it and where their files are, read here from each
kit's kit.json (content/plugin/<kit>/<KIT>_kit.json, the manifest the plugin embeds): its channel-10 keymap, the
instruments it names and their zones' files. Each cell names exactly its note's sounds, a drum note on an A/B switch
gets a bullet saying which sound is on each side, and a bullet names exactly the files in Drums/ no drum note plays,
with the channels that do play them.

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
NO_PAD = 'In `Drums/` but on no drum pad:'  # how the bullet on the Drums/ files no drum note plays starts


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


def bullets(text):
    """The Markdown list items in text, each a "- " line with its indented continuation lines, read flat."""
    return [flat(m.group(1)) for m in re.finditer(r'^- (.*(?:\n[ \t]+\S.*)*)', text, re.M)]


def sound_name(path):
    """A sample's sound, as the files name it: "Drums/Conga G3 v2 rr1.wav" is "Conga G3", "Bass/Rumble.wav" "Rumble"."""
    stem = path.rsplit('/', 1)[-1].removesuffix('.wav')
    return re.sub(r'( v\d+)?( rr\d+)?$', '', stem)


def load_kit(kit_id):
    return json.loads((REPO / 'content' / 'plugin' / kit_id.lower() / f'{kit_id}_kit.json').read_text(encoding='utf-8'))


def sounds_on(kit, instrument_id, key):
    """What an instrument plays on a key: {sound name: its files' paths}, from the zones that hold the key."""
    instrument = next(i for i in kit['instruments'] if i['id'] == instrument_id)
    out = {}
    for path in sorted({kit['files'][r]['path'] for z in instrument['zones'] if z['lo'] <= key <= z['hi']
                        for layer in z['layers'] for r in layer['rr']}):
        out.setdefault(sound_name(path), []).append(path)
    return out


def drum_map(kit_id):
    """The kit's drum notes: {key: {sound name: its files' paths}}, every sound its channel-10 keymap puts on the key
    (both sides of an A/B switch)."""
    kit = load_kit(kit_id)
    out = {}
    for entry in kit['keymap']:
        if entry['ch'] != DRUM_CHANNEL:
            continue
        lo, hi = entry['keys']
        for key in range(lo, hi + 1):
            for side in ('a', 'b'):
                if entry[side]:
                    for sound, paths in sounds_on(kit, entry[side], key).items():
                        out.setdefault(key, {}).setdefault(sound, set()).update(paths)
    return {key: {sound: sorted(paths) for sound, paths in sounds.items()} for key, sounds in out.items()}


def drum_switches(kit_id):
    """The kit's drum notes on an A/B switch: [(key, the A side's sounds, the B side's sounds)]."""
    kit = load_kit(kit_id)
    return [(key, sorted(sounds_on(kit, entry['a'], key)), sorted(sounds_on(kit, entry['b'], key)))
            for entry in kit['keymap'] if entry['ch'] == DRUM_CHANNEL and entry['a'] and entry['b']
            for key in range(entry['keys'][0], entry['keys'][1] + 1)]


def channels_playing(kit, sound):
    """The channels the kit's keymap plays one of the sound's files on: those of its entries whose keys reach a zone
    holding one."""
    instruments = {i['id']: i for i in kit['instruments']}
    return {entry['ch'] for entry in kit['keymap'] for side in ('a', 'b') if entry[side]
            for z in instruments[entry[side]]['zones'] if z['lo'] <= entry['keys'][1] and entry['keys'][0] <= z['hi']
            and any(sound_name(kit['files'][r]['path']) == sound for layer in z['layers'] for r in layer['rr'])}


def table(text):
    """The first Markdown table in text: (header cells, rows of cells)."""
    lines = [line.strip() for line in text.splitlines() if line.strip().startswith('|')]
    cells = [[c.strip() for c in line.strip('|').split('|')] for line in lines]
    return (cells[0], [row for row in cells[2:]]) if len(cells) > 2 else ([], [])


def cell_sounds(cell):
    """A table cell's sound names and paths: (the names, split on "," and " or ", the backticked paths)."""
    paths = re.findall(r'`([^`]*)`', cell)
    names = [n.strip() for n in re.split(r',| or ', re.sub(r'`[^`]*`', '', cell)) if n.strip()]
    return names, paths


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
            # zip stops at the shorter: a row short of a cell would leave a kit unchecked, and a cell too many unread.
            self.assertEqual(len(row), len(KITS) + 1, f'the {row[0]} row has {len(row) - 1} kit cells, not {len(KITS)}')
            for kit, cell in zip(KITS, row[1:]):
                with self.subTest(note=row[0], kit=kit):
                    sounds = self.maps[kit].get(key, {})
                    names, paths = cell_sounds(cell)
                    # The note's sounds, each once, and nothing else: "Resonant Zap" isn't "Zap".
                    self.assertEqual(sorted(names), sorted(sounds))
                    # The paragraph says Drums/; a file anywhere else is named in its cell, by its path.
                    elsewhere = [path for files in sounds.values() for path in files if not path.startswith('Drums/')]
                    self.assertEqual(sorted(paths), sorted(elsewhere))

    def test_it_says_where_the_files_are_and_what_their_names_mean(self):
        self.assertIsNotNone(self.drag, 'INSTALL.md has no "## Drag MIDI into Live or Logic"')
        text = flat(self.drag)
        for needed in ('Content/<kit>/v1/Drums/', '`v1` the softest', 'rr1', 'content/midi/<kit>/README.md'):
            with self.subTest(needed=needed):
                self.assertTrue(needed in text, f'the drag section never says {needed!r}')

    def test_it_names_exactly_the_drums_files_on_no_drum_pad_and_the_channels_that_play_them(self):
        # A few files in Drums/ are on no drum note: accent sounds, on channel 6, and FUT02's Glass Ping has a track of
        # its own too. One bullet names exactly those, each kit's after "<kit>'s" and the Accent files as one group, and
        # gives exactly the channels that play them, so no one takes them for files nothing plays.
        self.assertIsNotNone(self.drag, 'INSTALL.md has no "## Drag MIDI into Live or Logic"')
        found = [item for item in bullets(self.drag) if item.startswith(NO_PAD)]
        self.assertEqual(len(found), 1, f'the drag section needs one bullet starting {NO_PAD!r}')
        item = found[0]
        listed = item[len(NO_PAD):].split('.', 1)[0]  # its first sentence
        accents = 'the Accent files' in listed
        parts = re.split(r"\b(FUT\d\d)'s ", listed.replace('the Accent files', ''))
        self.assertEqual(re.sub(r'[\s,]|\band\b', '', parts[0]), '', f'the bullet names {parts[0]!r} for no kit')
        named = {}
        for kit, names in zip(parts[1::2], parts[2::2]):
            named.setdefault(kit, []).extend(n.strip() for n in re.split(r',|\band\b', names) if n.strip())
        self.assertEqual(set(named) - set(KITS), set(), 'the bullet names a kit there is none of')
        unmapped_accents, channels = False, set()
        for kit in KITS:
            data = load_kit(kit)
            mapped = {sound for sounds in self.maps[kit].values() for sound in sounds}
            others = {sound_name(f['path']) for f in data['files'] if f['path'].startswith('Drums/')} - mapped
            with self.subTest(kit=kit):
                self.assertEqual(sorted(named.get(kit, [])), sorted(s for s in others if not s.startswith('Accent ')))
                if accents:  # "the Accent files": every one of them, so none may be on a drum pad
                    self.assertEqual(sorted(s for s in mapped if s.startswith('Accent ')), [])
            unmapped_accents |= any(s.startswith('Accent ') for s in others)
            channels |= {ch for sound in others for ch in channels_playing(data, sound)}
        self.assertEqual(accents, unmapped_accents,
                         'the bullet names "the Accent files" if, and only if, some are on no drum pad')
        said = {int(ch) for ch in re.findall(r'\bchannel (\d+)\b', item)}
        self.assertEqual(said, channels, 'the bullet must give exactly the channels that play these sounds')

    def test_a_drum_note_on_an_a_b_switch_says_which_sound_is_on_each_side(self):
        # FUT01's kick: its cell names both sounds, and a bullet that starts with the kit puts each "on A" or "on B" as
        # kit.json's keymap does. A bullet naming a kit's A/B switch must be for a kit with a drum note on one.
        self.assertIsNotNone(self.drag, 'INSTALL.md has no "## Drag MIDI into Live or Logic"')
        items = bullets(self.drag)
        switched = {kit: drum_switches(kit) for kit in KITS}
        self.assertTrue(any(switched.values()), "no kit has a drum note on an A/B switch, but FUT01's kick is on one")
        for kit, notes in switched.items():
            for key, a, b in notes:
                with self.subTest(kit=kit, note=note_name(key)):
                    sides = [f'{sound} on A' for sound in a] + [f'{sound} on B' for sound in b]
                    told = [item for item in items if item.startswith(f"{kit}'s ")
                            and all(re.search(rf'\b{re.escape(side)}\b', item) for side in sides)]
                    self.assertTrue(told, f"no bullet starts with {kit}'s and says {', '.join(sides)}")
        for item in items:
            named = re.match(r"(FUT\d\d)'s .*\bA/B switch\b", item)
            if named:
                with self.subTest(bullet=item):
                    self.assertTrue(switched.get(named.group(1)), f'{named.group(1)} has no drum note on an A/B switch')

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
