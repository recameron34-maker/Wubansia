"""The streaming references (rt_ref.py) against the offline stages they stand in for, the streaming mix (engine_ref.py)
on a synthetic mix, and the gates against each kit's committed Wet and Club loops.

The rt_ref tests are small and run everywhere. The gates need the round 5 reference loops (the FLAC masters in
build/<kit>-r5, or the WAVs in content/samples/<kit>/Loops) and re-render each kit's recipe once (about 30 s a
variant): they run when WUBANSIA_R5_BUILD points at a folder holding <kit>-r5/ (or the repo's build/ has it), and
skip otherwise.
"""
import math
import os
import unittest

import numpy as np

from tools.sound import dsp48 as D
from tools.sound import master as MS
from tools.sound import phrase as Ph
from tools.sound import rt_ref as R
from tools.sound import sends as S

SR = D.SR


def _loop(n, seed=1, peak=0.9):
    """A loop-shaped test signal: decaying noise bursts every quarter of the loop plus a low sine, stereo."""
    r = np.random.default_rng(seed)
    t = np.arange(n)
    x = 0.3 * np.sin(2 * np.pi * 55.0 * t / SR)[:, None] * np.ones((1, 2))
    for k in range(4):
        p = k * n // 4
        m = min(n - p, 4800)
        x[p:p + m] += r.normal(0, 0.5, (m, 2)) * np.exp(-np.arange(m) / 900.0)[:, None]
    return x * (peak / np.abs(x).max())


class Bands(unittest.TestCase):
    def test_band_and_rest_sum_to_an_all_pass(self):
        x = np.random.default_rng(2).normal(0, 0.3, (48000, 2))
        for lo, hi in ((0, 120), (450, 0), (1200, 6500)):
            rest, band = R.band_split(x, lo, hi)
            y = rest + band
            f, h = _response(lambda u: sum(R.band_split(u, lo, hi)))
            self.assertLess(float(np.max(np.abs(20 * np.log10(np.abs(h[(f > 20) & (f < 20000)]))))), 0.01,
                            f'band {lo}-{hi}: rest + band is not flat')
            self.assertEqual(y.shape, x.shape)

    def test_whole_band(self):
        x = np.random.default_rng(3).normal(0, 0.3, (1000, 2))
        rest, band = R.band_split(x, 0, 0)
        self.assertTrue(np.array_equal(band, x))
        self.assertFalse(np.any(rest))

    def test_band_passes_its_middle(self):
        f, h = _response(lambda u: R.band_split(u, 1200, 6500)[1])
        mid = np.abs(h[np.argmin(np.abs(f - 2800))])
        self.assertGreater(20 * np.log10(mid), -1.0)
        self.assertLess(20 * np.log10(np.abs(h[np.argmin(np.abs(f - 150))])), -30)


def _response(fn, n=1 << 16, at=0):
    """fn's frequency response from an impulse at sample `at` (mid-buffer for a centred filter, whose response
    reaches before the impulse): (frequencies, the complex response, delay removed)."""
    imp = np.zeros(n)
    imp[at] = 1.0
    y = fn(imp)
    f = np.fft.rfftfreq(n, 1 / SR)
    return f, np.fft.rfft(y) * np.exp(2j * np.pi * f * at / SR)


class Ducks(unittest.TestCase):
    def test_onset_duck_is_pump_db_on_the_middle_pass(self):
        L = 30000
        onsets = (0, 7000, 7300, 21000, 29500)
        want = Ph.pump_db(onsets, L, 6.0, 0.002, 0.078, 0.060)
        stream = [p + k * L for k in range(3) for p in onsets]
        got = R.onset_db(stream, 3 * L, 6.0, 0.002, 0.078, 0.060)[L:2 * L]
        self.assertLess(float(np.max(np.abs(got - want))), 1e-12)

    def test_negative_depth_lifts_and_ramp_zero_starts_at_once(self):
        g = R.onset_db([100], 2000, -3.0, 0.0, 0.012, 0.013)
        self.assertEqual(g[99], 0.0)
        self.assertAlmostEqual(g[100], 3.0)
        self.assertAlmostEqual(g[100 + int(0.012 * SR) - 1], 3.0)
        self.assertEqual(g[100 + int(0.025 * SR) + 2], 0.0)
        self.assertTrue(np.all(g >= 0))

    def test_level_duck_is_sends_duck_late_by_one_and_a_half_blocks(self):
        L = 64 * 900
        key = _loop(L, seed=4, peak=0.5)
        top = R.loudest_block_db(key)
        _, want = S.duck(np.ones(2 * L), np.concatenate([key, key]), 6.0, 18.0, 0.010, 0.300)
        got = R.level_duck_db(np.tile(key, (3, 1)), 6.0, 18.0, top, 0.010, 0.300)
        lag = R.LEVEL_LAG
        self.assertLess(float(np.max(np.abs(got[L + lag:2 * L] - want[L:2 * L - lag]))), 1e-9)

    def test_level_duck_starts_at_0_db(self):
        key = np.concatenate([np.zeros(500), _loop(64 * 100, seed=16, peak=0.5)])
        g = R.level_duck_db(key, 6.0, 18.0, R.loudest_block_db(key), 0.010, 0.300)
        self.assertTrue(np.all(g[:R.LEVEL_LAG + 32] == 0.0))

    def test_level_duck_is_causal(self):
        key = _loop(64 * 300, seed=5, peak=0.5)
        top = R.loudest_block_db(key)
        g = R.level_duck_db(key, 9.0, 30.0, top, 0.010, 0.150)
        k2 = key.copy()
        k2[10000:] *= 0.1
        g2 = R.level_duck_db(k2, 9.0, 30.0, top, 0.010, 0.150)
        self.assertTrue(np.array_equal(g[:10000], g2[:10000]))


class Glue(unittest.TestCase):
    def test_fixed_threshold_glue_is_master_glue_once_settled(self):
        L = 16 * 6000                   # 2 s: the slow envelope (1.5 s release) settles over a few passes
        x = _loop(L, seed=6)
        y, rep = MS.glue(x, 2.5)
        got = R.glue(np.tile(x, (8, 1)), rep['threshold_dbfs'])[6 * L:7 * L]
        self.assertLess(float(np.max(np.abs(got - y))), 1e-6)

    def test_glue_latency(self):
        self.assertEqual(23, R.LATENCY['glue'])
        x = _loop(16 * 400, seed=7)
        g = R.glue_db(x, -20.0)
        x2 = x.copy()
        x2[3000:] *= 3.0
        g2 = R.glue_db(x2, -20.0)
        d = R.LATENCY['glue']
        self.assertTrue(np.array_equal(g[:3000 - d], g2[:3000 - d]))
        self.assertFalse(np.array_equal(g[:3000], g2[:3000]))


class Clip(unittest.TestCase):
    def test_soft_clip_is_master_soft_clip_on_the_middle_pass(self):
        L = 20000
        x = _loop(L, seed=8, peak=1.4)
        want, _ = MS.soft_clip(x, -1.0, 1.5)
        got = R.soft_clip(np.tile(x, (3, 1)), -1.0, 1.5)[L:2 * L]
        self.assertLess(float(np.max(np.abs(got - want))), 1e-12)

    def test_latency_48(self):
        x = _loop(8000, seed=9, peak=1.2)
        y = R.soft_clip(x, -1.0)
        x2 = x.copy()
        x2[5000:] *= 2.0
        y2 = R.soft_clip(x2, -1.0)
        d = R.LATENCY['soft_clip']
        self.assertEqual(48, d)
        self.assertLess(float(np.max(np.abs(y[:5000 - d] - y2[:5000 - d]))), 1e-12)


class HalfBand(unittest.TestCase):
    def test_taps(self):
        for h, n in ((R.HB1, 47), (R.HB2, 15)):
            c = (n - 1) // 2
            self.assertEqual(n, len(h))
            self.assertEqual(0.5, h[c])
            self.assertTrue(np.allclose(h, h[::-1]))
            even = (np.arange(n) - c) % 2 == 0
            even[c] = False
            self.assertFalse(np.any(h[even]))
            self.assertAlmostEqual(1.0, float(h.sum()), places=12)

    def test_round_trip_is_flat_to_16_khz(self):
        f, hh = _response(lambda u: R.hb_down4(R.hb_up4(u)), at=1 << 15)
        band = (f >= 20) & (f <= 16000)
        self.assertLess(float(np.max(np.abs(20 * np.log10(np.abs(hh[band]))))), 0.05)

    def test_lane_clip_against_master_soft_clip(self):
        L = 24000
        x = _loop(L, seed=14, peak=1.4)
        want, _ = MS.soft_clip(x, -1.0, 1.5)
        got = R.lane_soft_clip(np.tile(x, (3, 1)), -1.0, 1.5)[L:2 * L]
        lo = D.bfilt(got - want, 16000.0, 'lowpass', order=8)
        self.assertLess(20 * np.log10(np.sqrt(np.mean(lo ** 2)) / np.sqrt(np.mean(want ** 2))), -60.0)

    def test_lane_latency_27(self):
        self.assertEqual(27, R.LATENCY['lane_clip'])
        self.assertEqual(27, R.LATENCY['lane_drive'])
        x = _loop(8000, seed=15, peak=1.2)
        for fn in (lambda u: R.lane_soft_clip(u, -1.0), lambda u: R.lane_drive(u, 2.5, 0.0, -3.0)):
            y = fn(x)
            x2 = x.copy()
            x2[5000:] *= 2.0
            y2 = fn(x2)
            self.assertLess(float(np.max(np.abs(y[:5000 - 27] - y2[:5000 - 27]))), 1e-12)

    def test_lane_drive_is_tanh_adaa_at_4x(self):
        x = 0.3 * np.sin(2 * np.pi * 220.0 * np.arange(4800) / SR)
        want = D.oversampled(lambda u: D.tanh_adaa(u, 2.5), x, 4) * D.undb(-2.0)
        got = R.lane_drive(x, 2.5, 0.0, -2.0)
        self.assertLess(float(np.max(np.abs(got - want)[200:-200])), 1e-3)


class Limiter(unittest.TestCase):
    def test_two_sided_is_master_limit_on_the_middle_pass(self):
        L = 60000
        x = _loop(L, seed=10, peak=1.6)
        want = MS.limit_gain_db(x, -1.02)
        got = R.limit_db(np.tile(x, (3, 1)), -1.02, hold='two_sided')[L:2 * L]
        self.assertLess(float(np.max(np.abs(got - want))), 1e-9)

    def test_latencies(self):
        self.assertEqual(MS.limit_latency(), R.limit_latency(hold='two_sided'))
        self.assertEqual(265, R.limit_latency())
        for hold, d in (('lookback', 265), ('two_sided', 3145)):
            x = _loop(20000, seed=11, peak=0.5)
            g = R.limit_db(x, -1.02, hold=hold)
            x2 = x.copy()
            x2[12000] = 1.5
            g2 = R.limit_db(x2, -1.02, hold=hold)
            first = int(np.flatnonzero(g2 != g)[0])
            self.assertGreaterEqual(first, 12000 - d, hold)     # nothing moves before the lookahead reaches it
            self.assertLess(first, 12000 - d + 32, hold)        # and it acts as soon as the peak is over the ceiling

    def test_true_peak_under_the_ceiling(self):
        x = np.concatenate([np.zeros((4000, 2)), _loop(48000, seed=12, peak=1.0) * D.undb(12.0)])   # a limiter
        # can't reach back before the stream starts: the signal starts after more than its lookahead of silence
        for hold in ('lookback', 'two_sided'):
            y = R.limit(x, -1.02, hold=hold)
            # the gain's own movement between the detector's 4x points can lift a peak by a few mdB: master.py
            # trims that offline (its trim, about -0.003 dB); live it stays
            self.assertLessEqual(D.true_peak_db(y), -1.02 + 0.01, hold)

    def test_sliding_min(self):
        a = np.random.default_rng(13).uniform(0, 1, 500)
        for before, after in ((0, 0), (3, 7), (7, 3), (10, 0), (0, 10), (4, 4)):
            got = R.sliding_min(a, before, after)
            pad = np.concatenate([np.ones(before), a, np.ones(after)])
            want = np.array([pad[k:k + before + after + 1].min() for k in range(len(a))])
            self.assertTrue(np.array_equal(got, want), (before, after))


class Dc(unittest.TestCase):
    def test_causal_and_flat_over_40_hz(self):
        f, h = _response(lambda u: R.dc_block(u, 5.0))
        band = (f >= 40) & (f <= 20000)
        self.assertLess(float(np.max(np.abs(20 * np.log10(np.abs(h[band]))))), 0.07)
        x = np.zeros(1000)
        x[500] = 1.0
        self.assertFalse(np.any(R.dc_block(x)[:500]))


if __name__ == '__main__':
    unittest.main()
