"""FUT01's round 3 drums (recipes/fut01/drums.py), rendered: kick B (KickPunch) is Split with a shorter sub and a
100-150 Hz knock, in the crest band round 1 liked, its attack holding the peak and the mud gate passing; snare C
(SnareClap) has its hands, a short room tail and a body in the standard's windows; the shaker falls within a 16th,
brightens with velocity and keeps its round robins level. Renders KickSplit, KickPunch, SnareClap and Shaker (about
15 s)."""
import unittest

import numpy as np
from scipy import signal

from tools.sound import dsp48 as D
from tools.sound import kits
from tools.sound import qa
from tools.sound import render as R
from tools.sound.kits import fut01 as K

SR = D.SR
STEP_S = 60.0 / K.BPM / 4


def files(name):
    done = R.render_item(kits.items(K, only=(name,))[0], K.KIT)
    return {(t.layer, t.rr): D.st(x) for t, x in zip(done.takes, done.audio)}


def env_db(m, win):
    w = max(1, int(win * SR))
    c = np.concatenate([[0.0], np.cumsum(np.concatenate([np.zeros(w // 2), m ** 2, np.zeros(w - w // 2)]))])
    return 10 * np.log10((c[w:] - c[:-w])[:len(m)] / w + 1e-30)


def band(m, lo, hi):
    return D.bfilt(m, (lo, hi), 'bandpass', order=4, zero_phase=True)


def onset(m):
    e = env_db(m, 0.001)
    return int(np.flatnonzero(e >= e.max() - 40.0)[0])


def falls_ms(e, start, drop):
    """From start, where e stays `drop` dB under its peak after start, ms."""
    pk = start + int(np.argmax(e[start:]))
    above = np.flatnonzero(e[pk:] >= e[pk] - drop)
    return 1000.0 * (pk + (above[-1] + 1 if len(above) else 0) - start) / SR


def sub_20_ms(x):
    m = D.mono(x)
    return falls_ms(env_db(band(m, 30.0, 120.0), 0.005), onset(m), 20.0)


def knock_db(x):
    """100-150 Hz against 30-100 Hz over the first 50 ms, dB."""
    m = D.mono(x)
    o = onset(m)
    w = slice(o, o + int(0.050 * SR))
    lo = D.bfilt(m, 100.0, 'lowpass', order=4, zero_phase=True)
    return float(10 * np.log10(np.sum(band(m, 100.0, 150.0)[w] ** 2) / np.sum(band(lo, 30.0, 100.0)[w] ** 2)))


def share_db(x, lo, hi):
    m = D.mono(x)
    P = np.abs(np.fft.rfft(m, n=max(len(m), SR))) ** 2
    f = np.fft.rfftfreq(max(len(m), SR), 1 / SR)
    return float(10 * np.log10(P[(f >= lo) & (f < hi)].sum() / P.sum()))


class TestKickPunch(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.punch = files('KickPunch')
        cls.split = files('KickSplit')

    def test_a_shorter_sub(self):
        """The standard's 2-step target: the sub 20 dB down by about two 16ths (224 ms at 134 BPM); Split takes 317."""
        for rr in (1, 2):
            with self.subTest(rr=rr):
                punch, split = sub_20_ms(self.punch[(5, rr)]), sub_20_ms(self.split[(5, rr)])
                self.assertTrue(200.0 <= punch <= 240.0, punch)
                self.assertLessEqual(punch, split - 80.0)

    def test_a_knock_at_100_150_hz(self):
        for rr in (1, 2):
            with self.subTest(rr=rr):
                self.assertGreaterEqual(knock_db(self.punch[(5, rr)]) - knock_db(self.split[(5, rr)]), 2.0)

    def test_in_the_crest_band(self):
        """Split read 5.8-6.4 dB (round 1's Cool 4), Tight 7.9-8.9 ("slamming the limiter"): B stays in 5-7."""
        for rr in (1, 2):
            with self.subTest(rr=rr):
                self.assertTrue(5.0 <= qa.kick_crest_db(self.punch[(5, rr)]) <= 7.0)

    def test_the_attack_holds_the_peak(self):
        """The knock sits under the attack: on every layer but the softest (near a sine, as Split's is), the true peak
        is the click and the top's ring, in the first 12 ms, never the knock or the body."""
        for (layer, rr), x in self.punch.items():
            if layer == 1:
                continue
            with self.subTest(layer=layer, rr=rr):
                m = D.mono(x)
                up = np.abs(D.up(m, 4))
                self.assertLess(np.argmax(up) / 4, onset(m) + 0.012 * SR)

    def test_the_mud_gate(self):
        for layer in (3, 5):
            with self.subTest(layer=layer):
                g = qa.mud(self.punch[(layer, 1)])
                self.assertLessEqual(g['fall_150_400_ms'], qa.MUD_FALL_MS)
                self.assertLessEqual(g['vs_sub_db'], qa.MUD_VS_SUB_DB)
                self.assertLessEqual(g['share_db'], qa.MUD_SHARE_DB)
                self.assertLessEqual(g['fall_400_1k_ms'], qa.BOX_FALL_MS)


class TestSnareClap(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.snare = files('SnareClap')

    def test_the_hands(self):
        """After snare A's crack, the hands: at least two more peaks of the 1-6 kHz envelope inside 25 ms, each within
        3 dB of the loudest."""
        for rr in (1, 2):
            m = D.mono(self.snare[(5, rr)])
            o = onset(m)
            e = env_db(band(m, 1000.0, 6000.0), 0.002)[o:o + int(0.025 * SR)]
            pk, _ = signal.find_peaks(e, distance=int(0.004 * SR), prominence=1.0)
            with self.subTest(rr=rr):
                self.assertGreaterEqual(len([p for p in pk if e[p] >= e.max() - 3.0]), 3)

    def test_a_short_room_tail(self):
        """-20 dB at 120-250 ms (the standard's clap tail)."""
        for rr in (1, 2):
            m = D.mono(self.snare[(5, rr)])
            with self.subTest(rr=rr):
                self.assertTrue(120.0 <= falls_ms(env_db(m, 0.005), onset(m), 20.0) <= 250.0)

    def test_a_body(self):
        """The 150-400 Hz share in the standard's window (-15 to -8 dB) and nothing under 100 Hz."""
        for rr in (1, 2):
            with self.subTest(rr=rr):
                self.assertTrue(-15.0 <= share_db(self.snare[(5, rr)], 150.0, 400.0) <= -8.0)
                self.assertLessEqual(qa.low_share_db(self.snare[(5, rr)]), qa.LOWCUT_MAX_DB)

    def test_brighter_with_velocity(self):
        for rr in (1, 2):
            c = [qa.centroid_hz(self.snare[(layer, rr)]) for layer in range(1, 6)]
            with self.subTest(rr=rr):
                self.assertEqual(c, sorted(c))


class TestShaker(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.shaker = files('Shaker')
        cls.item = kits.items(K, only=('Shaker',))[0]

    def test_each_stroke_falls_within_a_16th(self):
        """20 dB down inside a 16th at 134 BPM (112 ms), so swung 16ths stay apart."""
        for (layer, rr), x in self.shaker.items():
            m = D.mono(x)
            with self.subTest(layer=layer, rr=rr):
                self.assertLess(falls_ms(env_db(m, 0.005), onset(m), 20.0), 1000.0 * STEP_S)

    def test_brighter_and_louder_with_velocity(self):
        layers = range(1, len(self.item.layers) + 1)
        for rr in range(1, self.item.rr + 1):
            c = [qa.centroid_hz(self.shaker[(la, rr)]) for la in layers]
            lv = [qa.lufs(self.shaker[(la, rr)]) for la in layers]
            with self.subTest(rr=rr):
                self.assertEqual(c, sorted(c))
                self.assertGreaterEqual(min(np.diff(lv)), qa.VEL_STEP_MIN_DB)

    def test_round_robins_level(self):
        for layer in range(1, len(self.item.layers) + 1):
            lv = [qa.lufs(self.shaker[(layer, rr)]) for rr in range(1, self.item.rr + 1)]
            with self.subTest(layer=layer):
                self.assertLessEqual(max(lv) - min(lv), 2 * qa.RR_SPREAD_MAX_DB)

    def test_all_top(self):
        for (layer, rr), x in self.shaker.items():
            with self.subTest(layer=layer, rr=rr):
                self.assertLess(share_db(x, 20.0, 2000.0), -30.0)

    def test_a_stereo_shaker(self):
        """Its width is its own, so the loop can keep it near the centre: one bead train on both sides and each side
        half its own noise, its side 3-7 dB under its mid (about hat A's 5.0), L and R still correlated (QA's 0.2 with
        room), and the mono sum within 2 dB of the stereo level."""
        for (layer, rr), x in self.shaker.items():
            m, s = 0.5 * (x[:, 0] + x[:, 1]), 0.5 * (x[:, 0] - x[:, 1])
            with self.subTest(layer=layer, rr=rr):
                self.assertTrue(-7.0 <= 10 * np.log10(np.sum(s ** 2) / np.sum(m ** 2)) <= -3.0)
                self.assertGreaterEqual(qa.stereo_corr(x), 0.35)
                self.assertGreater(10 * np.log10(np.sum(m ** 2) / (0.5 * np.sum(x ** 2))), -2.0)


if __name__ == '__main__':
    unittest.main()
