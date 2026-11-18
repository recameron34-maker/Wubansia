"""For my synths (ADR 0027) in dist/INSTALL.md, docs/midi-map.md and the ADR, held to the kits.

For my synths makes a drag's pitched tracks for the person's own synths. The text must say what that can't give and
what it leaves, read here from the content where the content can say it:
- A chord sample plays a whole chord from one key, so its track holds one note per chord, the root: INSTALL names each
  such track (CHORD_TRACKS, checked against each kit's Song) and each phrase chip holding only such tracks (the kit's
  phrase files in content/midi), and points to the Chord voicing track.
- The drums and the one-key Accents and FX tracks stay as they are, while the Accents lane's pitched extras
  (kit.json) are made for synths: INSTALL names each extra.
- A drag played back into Wubansia wants the switch off: INSTALL, midi-map's Live and Logic bullets and ADR 0027's
  Consequences say so.

From the repo root: python3 -m unittest discover -s tools/content/tests -t .
Standard library only, like the rest of tools/content/tests.
"""
import re
import struct
import unittest

from tools.content.tests.test_install_drums import INSTALL_MD, KITS, REPO, bullets, flat, load_kit, section

MIDI = REPO / 'content' / 'midi'
MIDI_MAP = REPO / 'docs' / 'midi-map.md'
ADR = REPO / 'docs' / 'decisions' / '0027-midi-for-your-synths.md'
ROUND_TRIP = 'turn For my synths off first'
# The tracks whose sample is a whole chord, played from its root (the kits' recipes, tools/sound/recipes): channel 4's
# Chords in every kit, and the kits' chord extras.
CHORD_TRACKS = {'FUT01': ('Chords', 'Air Pad'), 'FUT02': ('Chords', 'Laser Chord Stab'),
                'FUT03': ('Chords', 'Pad Warm'), 'FUT04': ('Chords', 'Metal Chord', 'Rave Stab')}


def vlq(data, i):
    value = 0
    while True:
        byte, i = data[i], i + 1
        value = (value << 7) | (byte & 0x7F)
        if byte < 0x80:
            return value, i


def onsets(path):
    """A MIDI file's tracks that hold notes: {track name: the tick of each note-on}."""
    data, i, out = path.read_bytes(), 14, {}
    while i < len(data):
        end = i + 8 + struct.unpack('>I', data[i + 4:i + 8])[0]
        i, tick, status, name, ticks = i + 8, 0, 0, None, []
        while i < end:
            delta, i = vlq(data, i)
            tick += delta
            if data[i] == 0xFF:
                size, body = vlq(data, i + 2)
                if data[i + 1] == 0x03 and name is None:
                    name = data[body:body + size].decode()
                i = body + size
                continue
            if data[i] & 0x80:
                status, i = data[i], i + 1
            assert 0x80 <= status < 0xF0, f'{path.name}: a message this reader does not know, at tick {tick}'
            if status & 0xF0 == 0x90 and data[i + 1] > 0:
                ticks.append(tick)
            i += 1 if status & 0xF0 in (0xC0, 0xD0) else 2
        if ticks:
            out[name] = ticks
    return out


def chip_title(path, kit):
    """A phrase file's chip title without its bar count: FUT01_Phrase_EPianoStabRhythm_2bars.mid is "E Piano Stab
    Rhythm"."""
    stem = re.fullmatch(rf'{kit}_Phrase_(\w+?)_\d+bars\.mid', path.name).group(1)
    return re.sub(r'(?<=[a-z])(?=[A-Z])|(?<=[A-Z])(?=[A-Z][a-z])', ' ', stem)


def paragraphs(text, start):
    """The paragraph starting with start and those after it up to one starting with ** or a heading, read flat."""
    out = []
    for para in text.split('\n\n'):
        if out and para.lstrip().startswith(('**', '#')):
            break
        if out or para.startswith(start):
            out.append(flat(para))
    return ' '.join(out)


class TestForMySynths(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        drag = section(INSTALL_MD.read_text(encoding='utf-8'), 'Drag MIDI into Live or Logic') or ''
        cls.text = paragraphs(drag, '**For my synths**')

    def test_it_names_every_chord_track_and_chord_phrase_chip(self):
        self.assertTrue(self.text, 'INSTALL.md\'s drag section has no "**For my synths**" paragraph')
        chips = []
        for kit in KITS:
            folder = MIDI / kit.lower()
            song = onsets(folder / 'extended' / 'vox' / f'{kit}_ExtendedMix_Vox_224bars.mid')
            for track in CHORD_TRACKS[kit]:
                with self.subTest(kit=kit, track=track):
                    # One note per chord: a track playing its chords' notes would need no word in the text.
                    self.assertIn(track, song)
                    self.assertEqual(len(song[track]), len(set(song[track])), f'{track} plays notes together')
                    self.assertIn(track, self.text)
            for path in sorted(folder.glob(f'{kit}_Phrase_*.mid')):
                tracks = set(onsets(path))
                if tracks and tracks <= set(CHORD_TRACKS[kit]):
                    chips.append(chip_title(path, kit))
                    with self.subTest(kit=kit, chip=chips[-1]):
                        self.assertIn(chips[-1], self.text)
        self.assertTrue(chips, 'no phrase file holds only chord tracks, but FUT03 Dub Chords does')
        for needed in ('root', 'Chord voicing', 'chord device'):
            with self.subTest(needed=needed):
                self.assertIn(needed, self.text)

    def test_it_says_what_stays_and_names_the_pitched_accents(self):
        self.assertIn('one-key Accents and FX tracks stay as they are', self.text)
        self.assertNotIn('accents and FX stay', self.text)
        for kit in KITS:
            for extra in load_kit(kit)['extras']:
                if extra['lane'] == 'accents':
                    with self.subTest(kit=kit, extra=extra['name']):
                        self.assertIn(extra['name'], self.text)

    def test_it_says_to_turn_it_off_before_playing_a_drag_into_wubansia(self):
        # Lit, Wubansia would play the bass an octave up, its slides as separate notes, short notes longer and soft
        # notes louder: INSTALL names each.
        for needed in (ROUND_TRIP, 'octave up', 'slides become separate notes', 'short notes hold longer', 'velocity 72'):
            with self.subTest(needed=needed):
                self.assertIn(needed, self.text)
        items = bullets(section(MIDI_MAP.read_text(encoding='utf-8'), 'In a DAW') or '')
        for daw in ('**Ableton Live.**', '**Logic Pro.**'):
            with self.subTest(daw=daw):
                found = [item for item in items if item.startswith(daw)]
                self.assertEqual(len(found), 1, f'midi-map.md\'s "In a DAW" needs one {daw} bullet')
                self.assertIn(ROUND_TRIP, found[0])
        self.assertIn(ROUND_TRIP, flat(section(ADR.read_text(encoding='utf-8'), 'Consequences') or ''))


if __name__ == '__main__':
    unittest.main()
