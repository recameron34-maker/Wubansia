"""FUT01's round 2 basslines (recipes/fut01/basslines.py), as written: every note in the key and in a zone's reach, room
on every kick, nothing but the kick's own pitch or a ghost on a loud kick's tail, one voice at a time, slides that join,
and what round 1 asked of a line (a motif that comes back varied, short and held notes, ghosts, a slide, the root, fifth
and minor seventh). Plus the sampler stand-in's repitch. No audio is rendered here: the loops' sound is checked by
render.py's QA."""
import unittest

import numpy as np

from tools.sound import dsp48 as D
from tools.sound import phrase as Ph
from tools.sound.item import Out
from tools.sound.kits import fut01 as K
from tools.sound.recipes.fut01 import basslines as B

SR = D.SR
G = B.G


def kick_onsets():
    return sorted(G.at(s) for s, _ in B.KICK)


def spans(line):
    """Each voice as (onset, note-off) in samples on the 2-bar grid (unwrapped: a note-off may pass the loop's end)."""
    return [(on, on + n_on) for on, _, n_on in B.voices(line, G)]


def on_a_kick_tail(line, kicks, grid):
    """The notes of a line (slides too) that start within B.TAIL_S after a kick of B.KICK_TAIL_VEL or more, on another
    pitch class than the kick's and louder than a ghost: [(16th, kick 16th)]."""
    loud = [(grid.at(s), s) for s, v in kicks if v >= B.KICK_TAIL_VEL]
    bad = []
    for step, _, note, vel, _ in line:
        on = grid.at(step)
        for k, ks in loud:
            if 0 <= (on - k) % grid.length < B.TAIL_S * SR and note % 12 != K.KICK_NOTE % 12 and vel > B.GHOST:
                bad.append((step, ks))
    return bad


class TestLines(unittest.TestCase):
    def test_three_lines_and_a_pick(self):
        self.assertEqual(['A', 'B', 'C'], sorted(B.LINES))
        self.assertIn(B.PICK, B.LINES)
        self.assertEqual(set(B.LINES), set(B.WHY))
        names = sorted(it.name for it in B.ITEMS)
        self.assertEqual(sorted(f'Bassline{k}{c}' for k in 'ABC' for c in ('Clean', 'Warm')), names)
        for it in B.ITEMS:
            self.assertIn('r2', it.sets)
            self.assertIn('with_kick', it.tags)
            self.assertEqual('bass-body' if it.name.endswith('Warm') else 'gate', it.qa['mud'])

    def test_notes_in_the_key_and_a_zone_s_reach(self):
        for key, line in B.LINES.items():
            for step, length, note, vel, slide in line:
                with self.subTest(line=key, step=step):
                    self.assertIn(note % 12, K.SCALE)
                    self.assertTrue(0 <= step < B.STEPS and length > 0)
                    self.assertTrue(0 <= note - B.zone_of(note) <= 5, 'repitched up by at most a fourth')
                    self.assertIn(vel, (B.ACC, B.NOTE, B.GHOST))
            for on, run, _ in B.voices(line, G):
                zone = B.zone_of(run[0][1])
                self.assertTrue(all(0 <= n - zone <= 5 for _, n, _ in run), f'line {key}: a slide out of reach')

    def test_room_on_every_kick(self):
        """No voice sounds over a kick's first 80 ms, and every note-off is ROOM before the next kick."""
        kicks = kick_onsets()
        L = G.length
        for key, line in B.LINES.items():
            for on, off in spans(line):
                with self.subTest(line=key, onset=on):
                    before = max(k for k in kicks + [kicks[0] - L] if k <= on)
                    after = min(k for k in kicks + [kicks[0] + L] if k > on)
                    self.assertGreaterEqual(on - before, int(B.OWN * SR), 'starts inside a kick')
                    self.assertLessEqual(off + int(B.ROOM * SR), after, 'runs into the next kick')

    def test_nothing_but_the_kick_s_pitch_or_a_ghost_on_a_loud_kick_s_tail(self):
        """Within 200 ms of a kick of velocity 0.8 or more its F#1 sub is still about 5 dB from its peak: a plain note a
        third away there (round 2's first Walk: A1 on step 27, 128 ms after the and-of-3 kick) beats against it and
        outweighs it. Only the kick's own pitch class, or a ghost (9 dB down), may start there."""
        self.assertGreaterEqual(B.TAIL_S, 0.2)
        for key, line in B.LINES.items():
            with self.subTest(line=key):
                self.assertEqual([], on_a_kick_tail(line, B.KICK, G))

    def test_one_voice_at_a_time(self):
        for key, line in B.LINES.items():
            s = spans(line)
            for (a0, a1), (b0, _) in zip(s, s[1:] + [(s[0][0] + G.length, None)]):
                self.assertLessEqual(a1, b0, f'line {key}: a note runs over the next')

    def test_slides_join(self):
        for key, line in B.LINES.items():
            for prev, cur in zip(line, line[1:]):
                if cur[4]:
                    self.assertEqual(prev[0] + prev[1], cur[0], f'line {key}: a slide needs the note before it held')
            self.assertFalse(line[0][4])

    def test_what_a_line_needs(self):
        root = K.KICK_NOTE % 12
        for key, line in B.LINES.items():
            with self.subTest(line=key):
                notes = {n % 12 for _, _, n, _, _ in line}
                self.assertLessEqual({root, (root + 7) % 12, (root + 10) % 12}, notes, 'root, fifth, minor seventh')
                self.assertTrue(any(s for *_, s in line), 'a slide')
                self.assertTrue(any(v == B.GHOST for _, _, _, v, _ in line), 'a ghost note')
                self.assertTrue(any(v == B.GHOST and not (i + 1 < len(line) and line[i + 1][4])
                                    for i, (_, _, _, v, _) in enumerate(line)), 'a ghost heard as one, not slid out of')
                self.assertTrue(any(n <= 1.0 for _, n, _, _, _ in line), 'a short note')
                self.assertTrue(any(n >= 2.0 for _, n, _, _, _ in line), 'a held note')
                self.assertTrue(any(s % 2 for s, *_ in line), 'notes on the swung 16ths')
                bar1 = [(s, n) for s, _, n, _, _ in line if s < 16]
                bar2 = [(s - 16, n) for s, _, n, _, _ in line if s >= 16]
                self.assertEqual(bar1[0][1], bar2[0][1], 'bar 2 starts the motif on its note')
                self.assertLessEqual(abs(bar1[0][0] - bar2[0][0]), 1)
                self.assertNotEqual(bar1, bar2, 'the motif comes back varied, not repeated')

    def test_lines_differ(self):
        rhythms = {k: tuple(s for s, *_ in line) for k, line in B.LINES.items()}
        self.assertEqual(3, len(set(rhythms.values())))


class TestStandIn(unittest.TestCase):
    def test_hermite_is_exact_on_whole_positions(self):
        r = np.random.default_rng(1)
        x = r.standard_normal((1000, 2))
        pos = np.arange(1500, dtype=float)
        y = B.hermite(x, pos, (200, 1000))
        self.assertTrue(np.array_equal(x, y[:1000]))
        self.assertTrue(np.array_equal(x[200:700], y[1000:1500]), 'wraps into the loop')

    def test_repitch_reads_the_new_pitch(self):
        f = 100.0
        n = 48000
        x = D.st(np.sin(2 * np.pi * f * np.arange(n) / SR))
        files = {(z, la): Out(x, loop=(0, n)) for z in B.ZONES for la in (1, 2)}
        y = B.voice(files, [(0, 40, B.NOTE)], int(0.5 * SR))[:, 0]   # E2 on the C#2 zone
        seg = y[4800:19200] * np.hanning(14400)
        spec = np.abs(np.fft.rfft(seg, 8 * len(seg)))
        peak = np.fft.rfftfreq(8 * len(seg), 1 / SR)[np.argmax(spec)]
        self.assertAlmostEqual(f * 2 ** (3 / 12), peak, delta=0.5)

    def test_a_slide_glides_without_a_new_attack(self):
        n = 48000
        x = D.st(np.ones(n))
        files = {(30, 1): Out(x, loop=(0, n)), (30, 2): Out(x, loop=(0, n))}
        off = int(0.2 * SR)
        y = B.voice(files, [(0, 30, B.NOTE), (off, 32, B.NOTE)], int(0.4 * SR))[:, 0]
        self.assertGreater(y[off - 10:off + 10].min(), 0.99 * y[off - 100], 'no dip at the slide')


if __name__ == '__main__':
    unittest.main()
