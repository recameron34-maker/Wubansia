"""FUT01's round 2 bass sounds (recipes/fut01/bass.py), rendered: B, warm is A, clean plus a body band and nothing else
(the A/B tests the body alone), and the growl moves keep their accents in mono, keep their side well under their mid,
and keep their vowels' low formants through the move. Renders two reese takes and the two growls (about 5 s)."""
import unittest

import numpy as np

from tools.sound import dsp48 as D
from tools.sound import kits
from tools.sound.item import Take
from tools.sound.kits import fut01 as K
from tools.sound.recipes.fut01 import bass

SR = D.SR


def level(x):
    return float(D.db(np.sqrt(np.mean(np.square(x))) + 1e-15))


def band(x, lo, hi=None):
    x = D.mono(x)
    return D.bfilt(x, (lo, hi), 'bandpass', order=4) if hi else D.bfilt(x, lo, 'highpass', order=4)


def take(name, layer=1, note=None):
    item = kits.items(K, only=(name,))[0]
    return item, Take(K.KIT, item, layer, 1, note)


class TestReeseAB(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        n = int(bass.ATTACK_S * SR)          # the attack, before each file's own loop points
        cls.clean = bass.reese_clean(take('ReeseClean', 2, 30)[1]).audio[:n]
        cls.warm = bass.reese_warm(take('ReeseWarm', 2, 30)[1]).audio[:n]

    def test_b_is_a_plus_a_body_band(self):
        """Above the wall B is A: what B adds is under 800 Hz (round 2's first B was 3-5 dB darker and quieter there,
        on a slower attack, so the A/B tested three things at once)."""
        s = slice(int(0.1 * SR), None)
        extra = level(band(self.warm - self.clean, 800)[s]) - level(band(self.clean, 800)[s])
        self.assertLessEqual(extra, -25.0)
        attack = slice(0, int(0.03 * SR))
        self.assertAlmostEqual(level(band(self.warm, 450, 800)[attack]), level(band(self.clean, 450, 800)[attack]),
                               delta=0.5)

    def test_b_has_the_body(self):
        s = slice(int(0.1 * SR), None)
        self.assertGreaterEqual(level(band(self.warm, 150, 400)[s]) - level(band(self.clean, 150, 400)[s]), 10.0)


class TestGrowls(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.moves = {}
        for name, move in (('GrowlTalk', 'talk'), ('GrowlDive', 'dive')):
            item, t = take(name, 1, bass.GROWL_NOTE)
            hits, _, _ = bass.move_plan(move, t.f0)
            cls.moves[move] = (item.render(t).audio, hits)

    def per_hit(self, move, fn):
        y, hits = self.moves[move]
        return [fn(y, slice(int(h['on'] * SR), int(h['on'] * SR) + int(h['gate'] * SR))) for h in hits]

    def test_the_accents_hold_in_mono(self):
        """Each hit's level in the mono sum, against the first, within 1.5 dB of its accent (round 2's first growls
        lost up to 14 dB on some hits in mono: their side, from a reese stage shifted up on the left and down on the
        right, outweighed their mid)."""
        for move in ('talk', 'dive'):
            mono = self.per_hit(move, lambda y, w: level(band(y, 150)[w]))
            with self.subTest(move=move):
                got = np.array(mono) - mono[0]
                self.assertLessEqual(np.max(np.abs(got - np.array(bass.ACCENTS[move]))), 1.5, list(np.round(got, 2)))

    def test_the_side_stays_under_the_mid(self):
        for move in ('talk', 'dive'):
            def side_minus_mid(y, w):
                lo, hi = band(y[:, 0], 150)[w], band(y[:, 1], 150)[w]
                return level(0.5 * (lo - hi)) - level(0.5 * (lo + hi))
            with self.subTest(move=move):
                self.assertLessEqual(max(self.per_hit(move, side_minus_mid)), -6.0)

    def test_the_chain_stays_mono(self):
        x = np.random.default_rng(1).standard_normal((4800, 2))
        y = bass.reese_stage(x, None)
        self.assertTrue(np.array_equal(y[:, 0], y[:, 1]))

    def test_the_vowels_keep_their_low_formants(self):
        """The talk's oo hits after the first keep OO's F2 (900 Hz): 450 Hz-1.3 kHz over 1.3-2.5 kHz. With the mids
        lifted to 1.3 kHz after the first 16th (round 2's first take) they read 10-11 dB the other way."""
        diff = self.per_hit('talk', lambda y, w: level(band(y, 450, 1300)[w]) - level(band(y, 1300, 2500)[w]))
        oo = [i for i, v in enumerate(bass.TALK_VOWELS) if v == bass.OO and i]
        self.assertTrue(oo)
        for i in oo:
            self.assertGreaterEqual(diff[i], 3.0, f'hit {i}')


if __name__ == '__main__':
    unittest.main()
