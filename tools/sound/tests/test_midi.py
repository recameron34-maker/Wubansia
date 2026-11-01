"""The MIDI export (tools/sound/midi.py): valid files at the kit's tempo with its swing written in, clips that end on
their bar line, and committed files (content/midi/<kit>/) that equal a fresh export."""
import re
import struct
import unittest
from pathlib import Path

from tools.sound import kits
from tools.sound import midi as Mi

REPO = Path(__file__).resolve().parents[3]
SOUND = REPO / 'tools' / 'sound'
KITS = [k for k in kits.names() if (SOUND / 'recipes' / k.lower() / 'midi.py').exists()]


def parse(data):
    """(format, ppq, tracks); a track is (end tick, [(tick, status, data1, data2)], tempo in us or None)."""
    assert data[:4] == b'MThd'
    _, fmt, ntrk, ppq = struct.unpack('>IHHH', data[4:14])
    i, tracks = 14, []
    for _ in range(ntrk):
        assert data[i:i + 4] == b'MTrk'
        n = struct.unpack('>I', data[i + 4:i + 8])[0]
        d, i = data[i + 8:i + 8 + n], i + 8 + n
        j = t = 0
        events, tempo, ended = [], None, False
        while j < len(d):
            v = 0
            while True:
                c = d[j]
                j += 1
                v = (v << 7) | (c & 0x7F)
                if not c & 0x80:
                    break
            t += v
            st = d[j]
            if st == 0xFF:
                kind, ln = d[j + 1], d[j + 2]
                if kind == 0x51:
                    tempo = int.from_bytes(d[j + 3:j + 6], 'big')
                ended = kind == 0x2F
                j += 3 + ln
            else:
                events.append((t, st, d[j + 1], d[j + 2]))
                j += 3
        assert ended and j == len(d)
        tracks.append((t, events, tempo))
    assert i == len(data)
    return fmt, ppq, tracks


def bars(name):
    return int(re.search(r'_(\d)bars\.mid$', name).group(1))


class TestExport(unittest.TestCase):
    def test_some_kits_export(self):
        self.assertGreaterEqual(len(KITS), 3)

    def test_files_are_valid_and_end_on_their_bar_line(self):
        for kit in KITS:
            K = kits.load(kit)
            for name, data in Mi.files(kit).items():
                if not name.endswith('.mid'):
                    continue
                with self.subTest(kit=kit, file=name):
                    fmt, ppq, tracks = parse(data)
                    self.assertEqual((1, Mi.PPQ), (fmt, ppq))
                    self.assertEqual(round(60_000_000 / K.BPM), tracks[0][2])
                    for end, _, _ in tracks:
                        self.assertEqual(bars(name) * 4 * ppq, end)

    def test_every_note_on_has_its_note_off(self):
        for kit in KITS:
            for name, data in Mi.files(kit).items():
                if not name.endswith('.mid'):
                    continue
                with self.subTest(kit=kit, file=name):
                    for _, events, _ in parse(data)[2][1:]:
                        on = {}
                        for t, st, note, vel in events:
                            key = (st & 0x0F, note)
                            if st & 0xF0 == 0x90:
                                self.assertNotIn(key, on)
                                self.assertTrue(1 <= vel <= 127)
                                on[key] = t
                            else:
                                self.assertGreater(t, on.pop(key))
                        self.assertEqual({}, on)

    def test_drums_are_on_channel_10(self):
        for kit in KITS:
            events = parse(Mi.files(kit)[f'{kit}_Drums_4bars.mid'])[2][1][1]
            self.assertEqual({Mi.DRUM_CHANNEL}, {st & 0x0F for _, st, _, _ in events})
            self.assertIn(36, {n for _, _, n, _ in events})

    def test_swing_is_written_in(self):
        ev = Mi.note_events([(0, 60, 1, 1.0, False), (3, 62, 1, 1.0, False)], 0.57, 0)
        starts = sorted(t for t, order, _ in ev if order == 1)
        self.assertEqual([0, 3 * Mi.S16 + round(0.07 * 2 * Mi.S16)], starts)

    def test_a_slide_overlaps_the_next_note(self):
        ev = Mi.note_events([(0, 40, 2, 1.0, True)], 0.5, 0)
        self.assertEqual(2 * Mi.S16 + Mi.SLIDE_TICKS, max(t for t, order, _ in ev if order == 0))

    def test_committed_files_match_a_fresh_export(self):
        for kit in KITS:
            folder = REPO / 'content' / 'midi' / kit.lower()
            made = Mi.files(kit)
            with self.subTest(kit=kit):
                self.assertEqual(sorted(made), sorted(p.name for p in folder.iterdir()))
                for name, data in made.items():
                    self.assertEqual(data, (folder / name).read_bytes(), name)


if __name__ == '__main__':
    unittest.main()
