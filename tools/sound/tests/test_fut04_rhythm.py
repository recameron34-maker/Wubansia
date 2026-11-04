"""FUT04's rhythm and atmosphere recipes (recipes/fut04: drums, perc, pad, fx): what QA can't see. The kick sits in
Split's crest range, not Tight's (round 1); the fill and the riser are exactly one bar and eight bars at the kit's
tempo and play only the key's notes; the drone's chord is Em(add9); the hat loop chokes its open hats."""
import unittest

import tools.sound  # noqa: F401  (pins numpy's kernels before numpy loads)
import numpy as np

from tools.sound import dsp48 as D
from tools.sound import kits, qa
from tools.sound import render as R
from tools.sound.recipes.fut04 import drums, fx, pad

SPLIT_RANGE = (5.5, 6.7)     # dB: FUT01's KickSplit top layer reads 5.82-6.44, KickTight 7.93-8.87 (ADR 0011)


class TestFut04Rhythm(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.k = kits.load('FUT04')

    def item(self, name):
        return kits.items(self.k, only=(name,))[0]

    def test_the_kick_hits_like_split_not_tight(self):
        it = self.item('KickSplit')
        top = [t for t in it.takes(self.k.KIT) if t.layer == len(it.layers)]
        for t in top:
            c = qa.kick_crest_db(R.finish_take(it, R.as_out(it.render(t))))
            self.assertGreaterEqual(c, SPLIT_RANGE[0], t.file)
            self.assertLessEqual(c, SPLIT_RANGE[1], t.file)

    def test_the_kick_ends_before_the_next_beat(self):
        it = self.item('KickSplit')
        y = D.mono(R.as_out(it.render(it.takes(self.k.KIT)[-1])).audio)
        self.assertLess(drums.SUB_END[0] + drums.SUB_END[1], self.k.BEAT)
        self.assertLess(D.db(np.abs(y[int(self.k.BEAT * D.SR):]).max() / np.abs(y).max()), -100.0)

    def test_the_fill_and_the_riser_are_whole_bars(self):
        bar = 4 * self.k.BEAT * D.SR
        self.assertEqual(fx.swung(16), int(np.floor(bar + 0.5)))
        self.assertEqual(fx.RISER_N, int(np.floor(8 * bar + 0.5)))
        self.assertEqual(fx.swung(1), int(np.floor(2 * self.k.SWING * self.k.STEP * D.SR + 0.5)))

    def test_the_fill_and_the_riser_play_the_key(self):
        notes = {m for *_, ns, _ in fx.FILL for m in ns} | set(fx.RISER_SCALE)
        self.assertTrue({m % 12 for m in notes} <= set(self.k.SCALE), sorted(notes))
        self.assertEqual(fx.FILL[-1][3][-1] % 12, self.k.SCALE[0])    # it lands on E
        self.assertEqual(fx.RISER_SCALE[-1] % 12, self.k.SCALE[0])
        self.assertEqual(len(fx.RISER_SCALE), fx.RISER_BARS)

    def test_the_fill_slices_halve_every_beat(self):
        starts = [s0 for s0, *_ in fx.FILL]
        lengths = [length for _, length, *_ in fx.FILL]
        self.assertEqual(starts, [0.0, 4.0, 8.0, 12.0])
        self.assertEqual(lengths, [2.0, 1.0, 0.5, 0.25])
        for s0, length, count, notes, _ in fx.FILL:
            self.assertEqual(count * length, 4.0)
            self.assertIn(len(notes), (1, count))

    def test_the_drone_is_e_minor_add9(self):
        self.assertEqual(pad.CHORD[0] % 12, self.k.ROOT % 12)
        e, fs, g, b = (self.k.SCALE[i] for i in (0, 1, 2, 4))       # root, 9, minor third, fifth
        self.assertEqual({m % 12 for m in pad.CHORD}, {e, fs, g, b})
        self.assertGreater(float(D.midi_hz(pad.CHORD[0])), 1.5 * qa.LOWCUT_HZ)

    def test_the_hat_loop_chokes_every_open_hat(self):
        closed = {s for s, _ in drums.HAT_CLOSED}
        opened = {s for s, _ in drums.HAT_OPEN}
        self.assertFalse(closed & opened)
        self.assertTrue(all(0 <= s < drums.HAT_STEPS for s in closed | opened))
        for s in opened:   # a closed hat follows every open one within an 8th
            self.assertTrue(any((c - s) % drums.HAT_STEPS in (1, 2) for c in closed), s)


if __name__ == '__main__':
    unittest.main()
