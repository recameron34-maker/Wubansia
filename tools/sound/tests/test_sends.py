"""The Python sends (sends.py): Space's decay, low-mid damping, shimmer and clean low end; Echo's timing, ping-pong,
feedback and filters; the duck; the resonator stand-in."""
import unittest

import numpy as np

from tools.sound import dsp48 as D
from tools.sound import qa as Q
from tools.sound import sends as S

SR = D.SR


def burst(f, seconds=0.05, amp=0.1):
    n = int(seconds * SR)
    return amp * np.sin(2 * np.pi * f * np.arange(n) / SR) * np.hanning(n)


def t60_s(y, lo=400.0, hi=800.0):
    """T60 from the Schroeder curve of one band, fitted from -5 to -35 dB."""
    b = D.bfilt(D.mono(y), (lo, hi), 'bandpass', order=4)
    e = np.cumsum((b ** 2)[::-1])[::-1]
    edc = 10 * np.log10(e / e[0] + 1e-30)
    i5, i35 = np.argmax(edc <= -5), np.argmax(edc <= -35)
    return 2.0 * (i35 - i5) / SR


def band_db(y, lo, hi, a, b):
    seg = D.bfilt(D.mono(y), (lo, hi), 'bandpass', order=4)[a:b]
    return 10 * np.log10(np.mean(seg ** 2) + 1e-30)


class TestSpace(unittest.TestCase):
    def test_octave_taps_sum_to_one_and_read_the_past(self):
        t = np.arange(10000)
        (d1, w1), (d2, w2) = S.octave_taps(t, 2048)
        np.testing.assert_allclose(w1 + w2, 1.0, atol=1e-12)
        self.assertTrue(((d1 >= 1) & (d1 <= 2048) & (d2 >= 1) & (d2 <= 2048)).all())
        self.assertTrue(np.all(np.diff(t - d1)[d1[1:] != 2048] == 2))   # twice the speed between jumps

    def test_decay_matches_t60(self):
        x = np.zeros(SR // 10)
        x[0] = 1.0
        for t60 in (1.5, 2.6):
            y = S.space(x, D.rng('test', 'space'), t60=t60, shimmer=0.0, predelay=0.0, n_tail=int(2.2 * t60 * SR))
            self.assertAlmostEqual(t60_s(y) / t60, 1.0, delta=0.10)

    def test_low_mids_decay_faster(self):
        """t60_low: the band under low_hz dies in its own time, the rest keeps t60 (the context loop's Space)."""
        x = np.zeros(SR // 10)
        x[0] = 1.0
        kw = dict(t60=2.6, shimmer=0.0, predelay=0.0, send_hp=20.0, n_tail=6 * SR)
        y = S.space(x, D.rng('test', 'space'), low_hz=500.0, t60_low=0.8, **kw)
        plain = S.space(x, D.rng('test', 'space'), **kw)
        self.assertLess(t60_s(y, 250.0, 350.0), 1.2)
        self.assertGreater(t60_s(plain, 250.0, 350.0), 2.0)
        self.assertAlmostEqual(t60_s(y, 2000.0, 4000.0) / t60_s(plain, 2000.0, 4000.0), 1.0, delta=0.1)

    def test_shimmer_lifts_the_tail_an_octave(self):
        x = burst(500.0, 0.2, 0.5)
        dry = S.space(x, D.rng('test', 'space'), shimmer=0.0, n_tail=3 * SR)
        shim = S.space(x, D.rng('test', 'space'), n_tail=3 * SR)                 # the default: a little
        a, b = int(1.0 * SR), int(2.0 * SR)
        self.assertGreater(band_db(shim, 900, 1100, a, b) - band_db(dry, 900, 1100, a, b), 10.0)
        self.assertLess(band_db(shim, 900, 1100, a, b) - band_db(shim, 450, 550, a, b), -6.0)

    def test_shimmer_stays_stable(self):
        x = np.zeros(SR // 10)
        x[0] = 1.0
        for amount in (0.06, 0.3, 0.6):
            y = S.space(x, D.rng('test', 'space'), t60=4.0, shimmer=amount, n_tail=10 * SR)
            self.assertLess(20 * np.log10(np.abs(y[-SR // 2:]).max() / np.abs(y).max()), -60.0, amount)

    def test_no_low_end_in_the_return(self):
        r = D.rng('test', 'noise')
        x = D.bfilt(r.standard_normal(SR), (30, 8000), 'bandpass') * 0.1
        y = S.space(x, D.rng('test', 'space'), n_tail=3 * SR)
        side = Q.low_side_db(y)
        self.assertTrue(side is None or side <= Q.LOW_SIDE_MAX_DB, side)
        self.assertLess(Q.low_share_db(y), Q.LOWCUT_MAX_DB)

    def test_same_seed_same_bytes(self):
        x = burst(700.0)
        a = S.space(x, D.rng('FUT01/space', 'send'), n_tail=SR)
        b = S.space(x, D.rng('FUT01/space', 'send'), n_tail=SR)
        self.assertTrue(np.array_equal(a, b))


class TestResonator(unittest.TestCase):
    def test_rings_on_its_modes_for_tau(self):
        modes = (1000.0, 1483.0)
        x = np.zeros(2 * SR)
        x[100] = 1.0
        y = S.resonator(x, modes, tau=0.4, send_hp=20.0)
        Y = np.abs(np.fft.rfft(D.mono(y)))
        f = np.fft.rfftfreq(len(y), 1 / SR)
        for m in modes:                                       # peaks on the modes, nothing between them
            self.assertGreater(Y[np.abs(f - m) < 2].max(), 100 * Y[np.abs(f - 1240.0) < 20].max())
        e = D.bfilt(D.mono(y), (900.0, 1100.0), 'bandpass', order=4)
        r = 20 * np.log10(np.abs(e[int(1.0 * SR):int(1.05 * SR)]).max() / np.abs(e[int(0.2 * SR):int(0.25 * SR)]).max())
        self.assertAlmostEqual(r, 20 * np.log10(np.exp(-0.8 / 0.4)), delta=0.5)   # 0.8 s later: down by e^-2

    def test_same_input_same_bytes(self):
        x = burst(2000.0)
        self.assertTrue(np.array_equal(S.resonator(x, (1500.0, 2200.0)), S.resonator(x, (1500.0, 2200.0))))


class TestEcho(unittest.TestCase):
    def repeats(self, f, delay=0.3):
        y = S.echo(burst(f), D.rng('test', 'echo'), delay, n_tail=2 * SR)
        d = int(delay * SR)
        win = lambda k: slice(k * d - 1000, k * d + int(0.05 * SR) + 1000)
        return y, d, win

    def test_ping_pong_timing_and_feedback(self):
        y, d, win = self.repeats(1000.0)
        rms = lambda k, ch: np.sqrt(np.mean(y[win(k), ch] ** 2))
        self.assertLess(np.abs(y[:d - 1000]).max(), 1e-12)                  # nothing before the first repeat
        self.assertGreater(rms(1, 0), 100 * rms(1, 1))                       # repeat 1 on the left
        self.assertGreater(rms(2, 1), 100 * rms(2, 0))                       # repeat 2 on the right
        self.assertGreater(rms(3, 0), 100 * rms(3, 1))
        step = 20 * np.log10(rms(2, 1) / rms(1, 0))
        self.assertAlmostEqual(step, 20 * np.log10(S.ECHO['feedback']), delta=0.6)

    def test_filters_in_the_feedback(self):
        mid = self.repeats(1000.0)
        for f in (100.0, 8000.0):
            y, d, win = self.repeats(f)
            ratio = 20 * np.log10(np.abs(y[win(1), 0]).max() / np.abs(mid[0][win(1), 0]).max())
            self.assertLess(ratio, -15.0, f)

    def test_same_seed_same_bytes(self):
        x = burst(1500.0)
        self.assertTrue(np.array_equal(S.echo(x, D.rng('e'), 0.33, n_tail=SR), S.echo(x, D.rng('e'), 0.33, n_tail=SR)))


class TestDuck(unittest.TestCase):
    def test_depth_follows_the_key(self):
        n = 2 * SR
        key = np.zeros(n)
        key[SR // 2:SR] = 0.5 * np.sin(2 * np.pi * 300 * np.arange(SR // 2) / SR)
        y, g = S.duck(np.ones((n, 2)), key, depth_db=6.0)
        self.assertAlmostEqual(g[:SR // 4].max(), 0.0, places=6)            # silent key: no ducking
        self.assertAlmostEqual(g[int(0.9 * SR)], -6.0, delta=0.1)          # key at its peak: the full depth
        self.assertGreater(g[-1], -0.5)                                     # released afterwards
        np.testing.assert_allclose(y[:, 0], D.undb(g))


if __name__ == '__main__':
    unittest.main()
