"""FUT04 "Furnace Code": its DNA as briefed (docs/plans/S1-four-kits.md), and what kits.problems() can't check: the
derivation of RATIOS (the anvil slab of kits/fut04.py, recomputed here), its distance from FUT01's alloy set, and the
voice pool."""
import math
import unittest

import tools.sound  # noqa: F401  (pins numpy's kernels before numpy loads)
import numpy as np
from scipy.linalg import eigh

from tools.sound import kits, models

POOL = {'am_onyx', 'am_fenrir', 'bm_george', 'af_kore', 'bf_lily'}   # FUT04's Kokoro pool, apart from the other kits'
E, NU, RHO = 200e9, 0.29, 7850.0                                       # steel
SLAB = (0.75, 0.216, 0.12)                                             # length, height, width (m)


def cents(a, b):
    return 1200 * math.log2(a / b)


def bending(L, d, w, ne=300):
    """Free-free Timoshenko beam bending across its depth d (width w): frequencies in Hz, rigid-body modes dropped.
    Friedman and Kosmatka's element (1993): exact stiffness, consistent translational and rotary mass."""
    G = E / (2 * (1 + NU))
    A, I = w * d, w * d ** 3 / 12
    le = L / ne
    p = 12 * E * I / ((10 * (1 + NU) / (12 + 11 * NU)) * G * A * le ** 2)
    k = E * I / ((1 + p) * le ** 3) * np.array([
        [12, 6 * le, -12, 6 * le], [6 * le, (4 + p) * le ** 2, -6 * le, (2 - p) * le ** 2],
        [-12, -6 * le, 12, -6 * le], [6 * le, (2 - p) * le ** 2, -6 * le, (4 + p) * le ** 2]])
    a1, a2, a3 = 13 / 35 + 7 * p / 10 + p ** 2 / 3, (11 / 210 + 11 * p / 120 + p ** 2 / 24) * le, \
        9 / 70 + 3 * p / 10 + p ** 2 / 6
    a4, a5, a6 = (13 / 420 + 3 * p / 40 + p ** 2 / 24) * le, (1 / 105 + p / 60 + p ** 2 / 120) * le ** 2, \
        (1 / 140 + p / 60 + p ** 2 / 120) * le ** 2
    mt = RHO * A * le / (1 + p) ** 2 * np.array([
        [a1, a2, a3, -a4], [a2, a5, a4, -a6], [a3, a4, a1, -a2], [-a4, -a6, -a2, a5]])
    b1, b2 = 6 / 5, (1 / 10 - p / 2) * le
    b3, b4 = (2 / 15 + p / 6 + p ** 2 / 3) * le ** 2, (1 / 30 + p / 6 - p ** 2 / 6) * le ** 2
    mr = RHO * I / ((1 + p) ** 2 * le) * np.array([
        [b1, b2, -b1, b2], [b2, b3, -b2, -b4], [-b1, -b2, b1, -b2], [b2, -b4, -b2, b3]])
    n = 2 * (ne + 1)
    K, M = np.zeros((n, n)), np.zeros((n, n))
    for e in range(ne):
        K[2 * e:2 * e + 4, 2 * e:2 * e + 4] += k
        M[2 * e:2 * e + 4, 2 * e:2 * e + 4] += mt + mr
    f = np.sqrt(np.clip(eigh(K, M, eigvals_only=True), 0, None)) / (2 * np.pi)
    return f[f > 1.0]


def twist(L, d, w, n=2):
    """Free-free Saint-Venant torsion: harmonic, f_k = k / 2L sqrt(G J / (rho Ip)), J from Roark's rectangle."""
    G = E / (2 * (1 + NU))
    a, c = max(d, w) / 2, min(d, w) / 2
    J = a * c ** 3 * (16 / 3 - 3.36 * (c / a) * (1 - (c / a) ** 4 / 12))
    Ip = (w * d ** 3 + d * w ** 3) / 12
    return [k / (2 * L) * math.sqrt(G * J / (RHO * Ip)) for k in range(1, n + 1)]


def slab_ratios(L, h, w):
    f = sorted([*bending(L, w, h)[:4], *bending(L, h, w)[:3], *twist(L, h, w)])[:7]
    return [x / f[0] for x in f], f[0]


class TestFut04Dna(unittest.TestCase):
    def setUp(self):
        self.k = kits.load('FUT04')

    def test_brief(self):
        k = self.k
        self.assertEqual(('E minor', '9A', 136.0, '4x4'), (k.KEY, k.CAMELOT, k.BPM, k.GROOVE))
        self.assertEqual((40, 28), (k.ROOT, k.KICK_NOTE))           # E2 and E1: the kick on the tonic, an octave under
        self.assertAlmostEqual(41.2, 440 * 2 ** ((k.KICK_NOTE - 69) / 12), places=1)
        self.assertEqual(0.54, k.SWING)
        self.assertEqual('FUT04/space', k.SPACE_SEED)
        self.assertEqual([], kits.problems(k))

    def test_the_thin_bar_limit(self):
        """The element reduces to Euler-Bernoulli on a slender bar: 1, 2.7565, 5.4039."""
        f = bending(1.0, 0.002, 0.01, ne=200)[:3]
        self.assertAlmostEqual(2.7565, f[1] / f[0], places=3)
        self.assertAlmostEqual(5.4039, f[2] / f[0], places=2)

    def test_ratios_are_the_anvil_slab(self):
        r, f0 = slab_ratios(*SLAB)
        self.assertEqual(len(r), len(self.k.RATIOS))
        for got, want in zip(self.k.RATIOS, r):
            self.assertAlmostEqual(want, got, places=4)
        self.assertAlmostEqual(1020, f0, delta=5)                    # the slab itself rings at about 1.02 kHz
        self.assertAlmostEqual(150, RHO * SLAB[0] * SLAB[1] * SLAB[2], delta=5)

    def test_ratios_are_heavy_iron_not_another_kits(self):
        """Every ratio after 1.0 at least 60 cents from FUT01's alloy set, a thin bar's (glockenspiel, 2.7565 and
        5.4039) and the harmonics 2-8; the second clears QA's 1.3 x f0 tuning band; the beating pairs are there."""
        r = self.k.RATIOS
        alloy = kits.module('FUT01').RATIOS
        for x in r[1:]:
            self.assertGreaterEqual(min(abs(cents(x, a)) for a in alloy[1:]), 60, x)
            self.assertGreaterEqual(min(abs(cents(x, g)) for g in (2.7565, 5.4039)), 60, x)
            self.assertGreaterEqual(min(abs(cents(x, h)) for h in range(2, 9)), 60, x)
        self.assertGreater(r[1], 1.4)
        self.assertTrue(30 < cents(r[2], r[1]) < 60)                 # the clank's warble
        self.assertTrue(60 < cents(r[5], r[4]) < 90)
        for x in r[1:3]:                                              # the pair sits on the minor sixth: C over E
            self.assertIn((4 + round(12 * math.log2(x))) % 12, self.k.SCALE, x)

    def test_voice_is_a_low_male_blend_from_the_kits_own_pool(self):
        blend = self.k.VOICE
        self.assertEqual(2, len(blend))
        self.assertLessEqual({v for v, _ in blend}, POOL)
        self.assertTrue(all(v in models.SPEAKERS and w > 0 for v, w in blend))
        self.assertAlmostEqual(1.0, sum(w for _, w in blend), places=12)
        self.assertTrue(all(v[1] == 'm' for v, _ in blend))           # am_ / bm_: male voices
        self.assertFalse(hasattr(self.k, 'ROBOT_VOICE'))              # one blend for the chops and the robot


if __name__ == '__main__':
    unittest.main()
