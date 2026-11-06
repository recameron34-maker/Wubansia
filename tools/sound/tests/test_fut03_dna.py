"""FUT03 "Glass Current": its DNA as briefed (docs/plans/S1-four-kits.md; round 4 moved it to garage-house and deep tech
house at swing 0.56), and the fields kits.problems() can't check (the derivation of RATIOS, the voice pool, one voice
blend)."""
import math
import unittest

import tools.sound  # noqa: F401  (pins numpy's kernels before numpy loads)
from tools.sound import kits, models

POOL = {'af_bella', 'af_heart', 'af_jessica', 'am_liam'}   # FUT03's Kokoro pool, kept apart from the other kits'


def stretched(r, b=0.004):
    """A glass shell's upper partial: r stretched sharp by stiffness, the nominal (2.0) kept exact."""
    return r * math.sqrt((1 + b * r * r) / (1 + 4 * b))


class TestFut03Dna(unittest.TestCase):
    def setUp(self):
        self.k = kits.load('FUT03')

    def test_brief(self):
        k = self.k
        self.assertEqual(('G minor', '6A', 128.0, '4x4'), (k.KEY, k.CAMELOT, k.BPM, k.GROOVE))
        self.assertEqual((43, 31), (k.ROOT, k.KICK_NOTE))           # G2 and G1: the kick on the tonic
        self.assertAlmostEqual(49.0, 440 * 2 ** ((k.KICK_NOTE - 69) / 12), places=1)
        self.assertEqual(0.56, k.SWING)                          # round 4's garage-house shuffle (rounds 1-3: 0.53)
        self.assertEqual('garage-house / deep tech house', k.DIRECTION)
        self.assertEqual([], kits.problems(k))

    def test_ratios_are_a_tuned_glass_bell(self):
        """Below the nominal: a minor-third bell's tuned partials in equal temperament (prime, tierce, quint, nominal).
        Above it: the minor tenth, the eleventh and the double octave, stretched sharp like a stiff glass shell's."""
        et = lambda s: 2 ** (s / 12)
        want = (1.0, et(3), et(7), 2.0, stretched(2 * et(3)), stretched(2 * et(5)), stretched(4.0))
        self.assertEqual(len(want), len(self.k.RATIOS))
        for got, w in zip(self.k.RATIOS, want):
            self.assertAlmostEqual(w, got, places=4)
        cents = [1200 * math.log2(g / r) for g, r in zip(self.k.RATIOS[4:], (2 * et(3), 2 * et(5), 4.0))]
        self.assertTrue(0 < cents[0] < cents[1] < cents[2] < 45, cents)

    def test_the_triad_is_in_key_on_the_tonic_fourth_and_fifth(self):
        """A pling struck on G, C or D rings its tuned partials (root, minor third, fifth) inside G minor."""
        steps = [round(12 * math.log2(r)) % 12 for r in self.k.RATIOS[:4]]
        for note in (67, 72, 74):                                     # G4, C5, D5
            self.assertTrue(all((note + s) % 12 in self.k.SCALE for s in steps), note)

    def test_voices_come_from_the_kits_own_pool(self):
        blend = self.k.VOICE
        self.assertEqual(2, len(blend))
        self.assertLessEqual({v for v, _ in blend}, POOL)
        self.assertTrue(all(v in models.SPEAKERS and w > 0 for v, w in blend))
        self.assertAlmostEqual(1.0, sum(w for _, w in blend), places=12)
        self.assertTrue(all(v.startswith('af_') for v, _ in blend))           # the sung chops: a female blend

    def test_one_voice_blend(self):
        """ADR 0008: one Kokoro blend per kit, for every take; the vocoder phrase sings on K.VOICE too."""
        import inspect
        from tools.sound.recipes.fut03 import vox
        self.assertFalse([n for n in dir(self.k) if n.endswith('VOICE') and n != 'VOICE'])
        self.assertIn('K.VOICE', inspect.getsource(vox.drop_track))


if __name__ == '__main__':
    unittest.main()
