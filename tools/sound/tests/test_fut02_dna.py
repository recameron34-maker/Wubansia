"""FUT02 "Orbit Relay": its DNA is what its recipes assume (docs/plans/S1-four-kits.md): D minor, the stiff-wire ratio
set, a female sung blend and a male robot blend from FUT02's own speaker pool, the kick on the key's fifth."""
import math
import unittest

from tools.sound import dsp48 as D
from tools.sound import kits, models
from tools.sound.kits import fut02 as K

POOL = {'af_nova', 'af_sky', 'am_echo', 'am_puck'}   # FUT02's speakers: the four kits never share one
STIFFNESS = 7.645e-4                                  # B of the steel wire behind RATIOS


class TestFut02Dna(unittest.TestCase):
    def test_ready(self):
        self.assertEqual([], kits.todo(K))
        self.assertEqual([], kits.problems(K))
        self.assertEqual(('D minor', '7A'), (K.KEY, K.CAMELOT))

    def test_ratios_are_a_stiff_wire(self):
        """Partials 1-7 of n sqrt((1 + B n^2) / (1 + B)), to 4 decimals: near-harmonic, stretched sharper with n,
        partial 7 on the equal-tempered minor seventh two octaves up."""
        want = [n * math.sqrt((1 + STIFFNESS * n * n) / (1 + STIFFNESS)) for n in range(1, 8)]
        self.assertEqual(7, len(K.RATIOS))
        for got, w in zip(K.RATIOS, want):
            self.assertAlmostEqual(w, got, delta=6e-5)
        stretch = [1200 * math.log2(r / n) for n, r in enumerate(K.RATIOS, 1)]
        self.assertEqual(0.0, stretch[0])
        self.assertTrue(all(b > a for a, b in zip(stretch, stretch[1:])), stretch)
        self.assertLess(stretch[-1], 35.0)
        self.assertAlmostEqual(0.0, 1200 * math.log2(K.RATIOS[6] / 2 ** (34 / 12)), delta=0.1)

    def test_voices_from_the_kits_pool(self):
        for blend, sex in ((K.VOICE, 'f'), (K.ROBOT_VOICE, 'm')):
            with self.subTest(blend=blend):
                self.assertEqual(2, len(blend))
                self.assertAlmostEqual(1.0, sum(w for _, w in blend), places=12)
                for voice, w in blend:
                    self.assertIn(voice, models.SPEAKERS)
                    self.assertIn(voice, POOL)
                    self.assertEqual(f'a{sex}_', voice[:3])
                    self.assertGreater(w, 0)

    def test_kick_and_root(self):
        self.assertEqual(K.SCALE[4], K.KICK_NOTE % 12)       # A: the key's fifth
        self.assertEqual(K.SCALE[0], K.ROOT % 12)
        self.assertAlmostEqual(55.0, float(D.midi_hz(K.KICK_NOTE)), places=6)
        self.assertEqual(38, K.ROOT)                          # D2: the bass's lines stay in D1-G2 (ADR 0011's range)

    def test_swing(self):
        self.assertAlmostEqual(4.615, (2 * K.SWING - 1) * K.STEP * 1000, places=3)


if __name__ == '__main__':
    unittest.main()
