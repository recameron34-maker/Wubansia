"""The DSP helpers, the four new tools of plan section 2.2 above all, and deterministic seeding."""
import os
import subprocess
import sys
import unittest
from pathlib import Path

import numpy as np

from tools.sound import dsp48 as D

REPO = Path(__file__).resolve().parents[3]
SR = D.SR


def spectrum(x, t0=0.25, t1=0.75, pad=8):
    """Power spectrum of the middle of a mono sound (Blackman-Harris, zero-padded to 1/pad Hz-ish bins)."""
    m = D.mono(x)[int(t0 * SR):int(t1 * SR)]
    n = len(m) * pad
    P = np.abs(np.fft.rfft(m * np.blackman(len(m)), n)) ** 2
    return np.fft.rfftfreq(n, 1 / SR), P


def band_db(f, P, lo, hi):
    return 10 * np.log10(P[(f >= lo) & (f <= hi)].sum() + 1e-30)


def rms_db(x):
    return 10 * np.log10(np.mean(np.asarray(x) ** 2) + 1e-30)


class TestSeeding(unittest.TestCase):
    KEY = ('FUT01', 'Kick', 1, 1)

    def test_same_key_same_stream(self):
        a, b = D.rng(*self.KEY).uniform(size=8), D.rng(*self.KEY).uniform(size=8)
        np.testing.assert_array_equal(a, b)
        self.assertFalse(np.array_equal(a, D.rng('FUT01', 'Kick', 1, 2).uniform(size=8)))
        self.assertNotEqual(D.seed('a', 'bc'), D.seed('ab', 'c'))

    def test_pinned_value(self):
        # SHA-256 of the key and PCG64 are fixed by design: these values must never change, on any platform
        self.assertEqual(17879102182045690875, D.seed(*self.KEY))
        self.assertEqual(0.05778560576288738, D.rng(*self.KEY).uniform())

    def test_independent_of_python_hash_randomisation(self):
        code = 'from tools.sound import dsp48 as D; print(D.rng("FUT01", "Kick", 1, 1).integers(1 << 62))'
        outs = {subprocess.run([sys.executable, '-c', code], cwd=REPO, capture_output=True, text=True, check=True,
                               env={**os.environ, 'PYTHONHASHSEED': s}).stdout for s in ('1', '2')}
        self.assertEqual(1, len(outs))


class TestHelpers(unittest.TestCase):
    def test_oversampling_round_trip(self):
        x = np.sin(2 * np.pi * 1000 * np.arange(SR) / SR)
        u = D.up(x, 4)
        self.assertEqual(4 * SR, len(u))
        self.assertAlmostEqual(np.abs(u[SR:3 * SR]).max(), 1.0, places=3)
        y = D.down(u, 4)
        self.assertLess(rms_db(y[1000:-1000] - x[1000:-1000]), -80)

    def test_lr4_bands_sum_flat(self):
        for f in (60.0, 120.0, 400.0):
            x = np.sin(2 * np.pi * f * np.arange(SR) / SR)
            y = D.lr4(x, 120, 'lowpass') + D.lr4(x, 120, 'highpass')
            self.assertAlmostEqual(rms_db(y[SR // 2:]), rms_db(x[SR // 2:]), delta=0.05)

    def test_adaa_shapers(self):
        x = 0.1 * np.sin(2 * np.pi * 50 * np.arange(SR // 10) / SR)
        mid = np.concatenate([[0.0], 0.5 * (x[1:] + x[:-1])])   # first-order ADAA is the curve, half a sample late
        np.testing.assert_allclose(D.tanh_adaa(x, 1.0), np.tanh(mid) / np.tanh(1.0), atol=1e-6)
        self.assertLessEqual(np.abs(D.hardclip_adaa(4 * x, 5.0)).max(), 1.0 + 1e-9)

    def test_mono_below(self):
        n = SR
        t = np.arange(n) / SR
        lo, hi = np.sin(2 * np.pi * 50 * t), np.sin(2 * np.pi * 2000 * t)
        y = D.mono_below(np.stack([lo + hi, -lo - hi], 1), 150)
        side = 0.5 * (y[:, 0] - y[:, 1])
        f, P = spectrum(side)
        self.assertLess(band_db(f, P, 40, 60) - band_db(f, P, 1990, 2010), -40)

    def test_finish_edges(self):
        n = 2 * SR
        x = np.cos(2 * np.pi * 440 * np.arange(n) / SR) * D.env_exp(n, 0.05)
        y = D.finish(x)
        self.assertEqual(0.0, y[0])
        self.assertLess(len(y), n)
        self.assertLess(np.abs(y[-int(0.01 * SR):]).max(), np.abs(x).max() * D.undb(-85))

    def test_fdn_is_seeded(self):
        x = np.zeros(SR // 2)
        x[0] = 1.0
        a = D.fdn_reverb(x, D.rng('FUT01/space'), t60=1.0)
        np.testing.assert_array_equal(a, D.fdn_reverb(x, D.rng('FUT01/space'), t60=1.0))
        self.assertEqual((SR // 2, 2), a.shape)


class TestFrequencyShifter(unittest.TestCase):
    def test_moves_a_sine_by_37_hz(self):
        x = np.sin(2 * np.pi * 1000 * np.arange(int(0.9137 * SR)) / SR)
        for shift in (37.0, -37.0):
            f, P = spectrum(D.freq_shift(x, shift))
            target = 1000 + shift
            self.assertAlmostEqual(f[np.argmax(P)], target, delta=0.5)
            on = band_db(f, P, target - 8, target + 8)
            rest = 10 * np.log10(P[(np.abs(f - target) > 8)].sum())
            self.assertLess(rest - on, -40, f'sidebands for {shift:+g} Hz')
            self.assertLess(band_db(f, P, 1000 - shift - 8, 1000 - shift + 8) - on, -40, 'mirror sideband')

    def test_keeps_level_and_shape(self):
        r = D.rng('test', 'shift')
        x = D.bfilt(r.uniform(-1, 1, (SR, 2)), (500, 5000), 'bandpass')
        y = D.freq_shift(x, 120.0)
        self.assertEqual(x.shape, y.shape)
        self.assertAlmostEqual(rms_db(y[2000:-2000]), rms_db(x[2000:-2000]), delta=0.3)

    def test_swept_shift(self):
        n = SR
        y = D.freq_shift(np.sin(2 * np.pi * 1000 * np.arange(n) / SR), np.linspace(0, 200, n))
        f_lo, P_lo = spectrum(y, 0.05, 0.15)
        f_hi, P_hi = spectrum(y, 0.85, 0.95)
        self.assertLess(f_lo[np.argmax(P_lo)], 1030)
        self.assertGreater(f_hi[np.argmax(P_hi)], 1165)


class TestRingModulator(unittest.TestCase):
    def test_sum_and_difference(self):
        x = np.sin(2 * np.pi * 1000 * np.arange(SR) / SR)
        f, P = spectrum(D.ring_mod(x, 300.0, (1.0,)))
        lo, hi = band_db(f, P, 695, 705), band_db(f, P, 1295, 1305)
        self.assertAlmostEqual(lo, hi, delta=0.5)
        self.assertLess(band_db(f, P, 995, 1005) - lo, -60, 'the input must disappear')

    def test_alloy_carriers(self):
        alloy = (1.0, 1.483, 1.932)
        x = np.sin(2 * np.pi * 1000 * np.arange(SR) / SR)
        f, P = spectrum(D.ring_mod(x, 300.0, alloy, r=D.rng('test', 'ring')))
        top = 10 * np.log10(P.max())
        for rt in alloy:
            for p in (1000 - 300 * rt, 1000 + 300 * rt):
                self.assertGreater(band_db(f, P, p - 3, p + 3) - top, -20, f'partial at {p:.1f} Hz')
        self.assertLess(band_db(f, P, 995, 1005) - top, -50)

    def test_mix_zero_is_dry(self):
        x = np.sin(2 * np.pi * 440 * np.arange(1000) / SR)
        np.testing.assert_array_equal(D.ring_mod(x, 300.0, (1.0,), mix=0.0), x)


class TestUpDownCompressor(unittest.TestCase):
    def steps(self, f, loud=-6.0, quiet=-40.0):
        """One-second sections alternating between two RMS levels, as (signal, section length)."""
        seg = SR
        t = np.arange(4 * seg) / SR
        lv = np.repeat([loud, quiet, loud, quiet], seg)
        return np.sqrt(2) * D.undb(lv) * np.sin(2 * np.pi * f * t), seg

    def levels(self, y, seg):
        """RMS dB of the last 0.4 s of the second loud and second quiet sections (the detector has settled)."""
        a = int(0.6 * SR)
        return rms_db(y[2 * seg + a:3 * seg]), rms_db(y[3 * seg + a:4 * seg])

    # -6 dB is 14 dB over -20 at 4:1, so it comes out near -16.5; -40 dB is 10 dB under -30 at 3:1, so near -33.3
    ARGS = dict(down_db=-20.0, down_ratio=4.0, up_db=-30.0, up_ratio=3.0)

    def test_narrows_the_range_in_every_band(self):
        for f in (60.0, 1000.0, 8000.0):
            x, seg = self.steps(f)
            loud, quiet = self.levels(D.updown3(x, **self.ARGS), seg)
            self.assertAlmostEqual(loud, -16.5, delta=1.5, msg=f'{f} Hz: loud part')
            self.assertAlmostEqual(quiet, -33.3, delta=1.5, msg=f'{f} Hz: quiet part')

    def test_each_direction_alone(self):
        x, seg = self.steps(1000.0)
        loud, quiet = self.levels(D.updown3(x, **{**self.ARGS, 'up_ratio': 1.0}), seg)
        self.assertAlmostEqual(quiet, -40.0, delta=0.5)
        self.assertAlmostEqual(loud, -16.5, delta=1.5)
        loud, quiet = self.levels(D.updown3(x, **{**self.ARGS, 'down_ratio': 1.0}), seg)
        self.assertAlmostEqual(loud, -6.0, delta=0.5)
        self.assertAlmostEqual(quiet, -33.3, delta=1.5)

    def test_brings_up_a_decaying_tail(self):
        n = SR
        x = 0.7 * np.sin(2 * np.pi * 1000 * np.arange(n) / SR) * D.env_exp(n, 0.1, attack=0.002)
        y = D.updown3(x, **self.ARGS)
        span = lambda v: rms_db(v[int(0.05 * SR):int(0.1 * SR)]) - rms_db(v[int(0.4 * SR):int(0.5 * SR)])
        self.assertLess(span(y), span(x) - 8.0)   # the input falls 32 dB between the two windows

    def test_neutral_settings_are_flat(self):
        for f in (60.0, 1000.0, 8000.0):
            x = 0.5 * np.sin(2 * np.pi * f * np.arange(SR) / SR)
            y = D.updown3(x, down_ratio=1.0, up_ratio=1.0)
            self.assertAlmostEqual(rms_db(y[SR // 2:]), rms_db(x[SR // 2:]), delta=0.1)

    def test_silence_is_not_lifted(self):
        x = np.zeros((SR, 2))
        x[:100] = 1e-6
        self.assertLess(np.abs(D.updown3(x)).max(), 1e-5)


class TestResamplePass(unittest.TestCase):
    @staticmethod
    def grit(x, r):
        return D.tanh_adaa(x + 0.05 * r.uniform(-1, 1, x.shape), 3.0, bias=0.2)

    @staticmethod
    def comb(x, r):
        d = 37
        return x + 0.6 * np.concatenate([np.zeros(d), x[:-d]])

    def test_deterministic(self):
        x = np.sin(2 * np.pi * 55 * np.arange(SR) / SR)
        a = D.resample_pass(x, [self.grit, self.comb], passes=2, key=('FUT01', 'Growl', 1, 1))
        b = D.resample_pass(x, [self.grit, self.comb], passes=2, key=('FUT01', 'Growl', 1, 1))
        np.testing.assert_array_equal(a, b)
        c = D.resample_pass(x, [self.grit, self.comb], passes=2, key=('FUT01', 'Growl', 1, 2))
        self.assertFalse(np.array_equal(a, c))

    def test_bounces_between_stages(self):
        x = np.sin(2 * np.pi * 55 * np.arange(SR) / SR)
        y = D.resample_pass(x, [self.grit], passes=1, peak_db=-6.0)
        self.assertAlmostEqual(D.db(np.abs(y).max()), -6.0, places=6)
        self.assertLess(abs(np.mean(y[SR // 4:])), 1e-3)          # the biased stage's DC is gone

    def test_two_passes_run_the_chain_twice(self):
        x = np.sin(2 * np.pi * 55 * np.arange(SR // 4) / SR)
        once = D.resample_pass(x, [self.comb], passes=1)
        twice = D.resample_pass(x, [self.comb], passes=2)
        np.testing.assert_allclose(twice, D.resample_pass(once, [self.comb], passes=1), atol=1e-12)


if __name__ == '__main__':
    unittest.main()
