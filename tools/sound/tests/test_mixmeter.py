"""The mix meters (tools/sound/mixmeter.py) read what the standards and the fixtures say: EBU Tech 3341's loudness
cases, BS.1770's true peak, octave bands on noise of known slope, side over mid and correlation on signals of known
width, the crest and attack of known bursts, and loops measured as they play (round the seam).

The functions at the top build loops whose meter readings are set by construction (octave levels, width, PLR): the QA
boundary pairs of the club checks (test_qa.py) use them too."""
import functools
import unittest
from pathlib import Path

import numpy as np

from tools.sound import dsp48 as D
from tools.sound import mixmeter as MM
from tools.sound import qa as Q
from tools.sound.tests import fixtures as FX

SR = D.SR


# ------------------------------------------------------------------ loops built to read set values
def edges(name):
    """An octave band's edges, Hz: as the meter cuts them."""
    fc = dict(zip(MM.BANDS, MM.CENTRES))[name]
    return fc / np.sqrt(2), fc * np.sqrt(2)


def shaped_loop(levels=None, width=None, seconds=2.0, lufs=None, seed='shaped'):
    """A noise loop, exactly periodic in its length, whose octave bands read `levels` (dB re the 1 kHz band; a band left
    out reads 0, under 22 Hz and over 22.6 kHz is empty) and whose side over mid reads `width` (dB per band; a band left
    out is mono). Built in the FFT domain over one period: flat magnitude inside each band, random phases from
    rng(seed), mid and side independent. lufs: scaled to that integrated loudness (circular)."""
    levels, width = dict(levels or {}), dict(width or {})
    n = int(seconds * SR)
    f = np.fft.rfftfreq(n, 1 / SR)
    r = D.rng('test', 'mixmeter', seed)
    M = np.zeros(len(f), dtype=complex)
    S = np.zeros(len(f), dtype=complex)
    for name in MM.BANDS:
        lo, hi = edges(name)
        k = (f >= lo) & (f < hi)
        # mid and side per bin; L, R = M + S, M - S, so the band's L+R power is 2 (|M|^2 + |S|^2) per bin
        amp_m = np.sqrt(D.undb(levels.get(name, 0.0)) ** 2 / k.sum())
        M[k] = amp_m * np.exp(2j * np.pi * r.uniform(size=k.sum()))
        if name in width:
            g = D.undb(width[name])
            M[k] /= np.sqrt(1 + g * g)
            S[k] = g * np.abs(M[k]) * np.exp(2j * np.pi * r.uniform(size=k.sum()))
    m, s = np.fft.irfft(M, n), np.fft.irfft(S, n)
    x = np.stack([m + s, m - s], 1)
    x /= np.abs(x).max()
    return x if lufs is None else x * D.undb(lufs - MM.lufs_i(x))


@functools.lru_cache(maxsize=None)
def _plr_burst(plr_db, seconds):
    n = int(seconds * SR)
    t = np.arange(n) / SR
    base = np.sin(2 * np.pi * 1000.0 * t)
    w = int(0.002 * SR)
    burst = np.zeros(n)
    at = int(0.5 * SR) + 12                                   # a crest of the 1 kHz sine (48 samples a cycle)
    burst[at - w // 2:at - w // 2 + w] = np.hanning(w) * np.cos(2 * np.pi * 5000.0 * (np.arange(w) - w // 2) / SR)
    make = lambda b: D.st(base + b * burst)
    plr = lambda b: MM.true_peak_db(make(b)) - MM.lufs_i(make(b)) - plr_db
    a, fa, b, fb = 0.0, plr(0.0), 40.0, plr(40.0)             # PLR rises with the burst: Illinois between them
    side = 0
    for _ in range(60):
        c = (a * fb - b * fa) / (fb - fa)
        fc = plr(c)
        if abs(fc) < 1e-5:
            break
        if fc < 0:
            a, fa = c, fc
            fb = fb / 2 if side == 1 else fb
            side = 1
        else:
            b, fb = c, fc
            fa = fa / 2 if side == -1 else fa
            side = -1
    return make(c)


def plr_loop(plr_db, lufs=-8.0, seconds=2.0):
    """A loop at `lufs` whose PLR (circular true peak over LUFS-I) reads plr_db: a 1 kHz sine (whole cycles; PLR 0.69
    dB) with one 2 ms Hann burst at 5 kHz on top of a crest, its height found by regula falsi (Illinois)."""
    x = _plr_burst(float(plr_db), float(seconds))
    return x * D.undb(lufs - MM.lufs_i(x))


def stereo_sine(f, dbfs, seconds, phase=0.0):
    t = np.arange(int(round(seconds * SR))) / SR
    return D.st(D.undb(dbfs) * np.sin(2 * np.pi * f * t + phase))


class TestLoudness(unittest.TestCase):
    """ITU-R BS.1770-4 as EBU Tech 3341 tests it: a stereo 1 kHz sine at -23 dBFS reads -23.0 LUFS (+-0.1)."""

    def test_tech_3341_cases_1_to_5(self):
        cases = (([(-23.0, 20.0)], -23.0),
                 ([(-33.0, 20.0)], -33.0),
                 ([(-36.0, 10.0), (-23.0, 60.0), (-36.0, 10.0)], -23.0),
                 ([(-72.0, 10.0), (-36.0, 10.0), (-23.0, 60.0), (-36.0, 10.0), (-72.0, 10.0)], -23.0),
                 ([(-26.0, 20.0), (-20.0, 20.1), (-26.0, 20.0)], -23.0))
        for parts, want in cases:
            x = np.concatenate([stereo_sine(1000.0, lv, s) for lv, s in parts])
            with self.subTest(parts=parts):
                self.assertAlmostEqual(MM.lufs_i(x, loop=False), want, delta=0.1)

    def test_momentary_and_short_term_of_a_steady_tone(self):
        x = stereo_sine(1000.0, -23.0, 20.0)
        for curve in (MM.lufs_m(x, loop=False), MM.lufs_s(x, loop=False)):
            self.assertLess(np.abs(curve - -23.0).max(), 0.1)

    def test_linear_mode_is_qa_lufs(self):
        """Not a loop, the meter is qa.lufs exactly: the same K-weighting, blocks and gates (a sound under 400 ms is one
        zero-padded block)."""
        for x in (FX.master(FX.clean_kick()), FX.riser(), FX.tone(440.0, 0.3), FX.low_side_at(-23.0)):
            self.assertAlmostEqual(MM.lufs_i(x, loop=False), Q.lufs(x), delta=1e-9)

    def test_a_loop_is_measured_round_its_seam(self):
        """A 0.3 s burst (+26.4 dB) across the seam is one event in a loop: it reads the same rotated by whole blocks
        (100 ms), and 2.33 LU louder than the same file read as a file, which cuts the burst in two at its ends. By
        hand: as a loop, 7 blocks hold some of the burst and pass the gate, at a mean of 189.6 times the bed's power;
        as a file, 4 blocks at 111 times (10 log10(189.6 / 111) = 2.33)."""
        n = 2 * SR
        burst = np.zeros(n)
        burst[:int(0.15 * SR)] = 1.0
        burst[-int(0.15 * SR):] = 1.0
        x = 0.01 * shaped_loop(seconds=2.0) * (1 + 20 * burst)[:, None]
        ref = MM.lufs_i(x)
        self.assertAlmostEqual(MM.lufs_i(np.roll(x, 4800 * 3, 0)), ref, delta=1e-9)
        self.assertAlmostEqual(MM.lufs_s(np.roll(x, 4800, 0)).max(), MM.lufs_s(x).max(), delta=1e-9)
        self.assertAlmostEqual(ref - MM.lufs_i(x, loop=False), 2.33, delta=0.1)

    def test_a_steady_loop_reads_the_same_from_anywhere(self):
        x = shaped_loop(seconds=2.0)
        self.assertAlmostEqual(MM.lufs_i(np.roll(x, 12345, 0)), MM.lufs_i(x), delta=0.05)

    def test_mono_counts_as_left_and_right(self):
        """A mono file plays as L = R: it reads 3 dB over one channel alone."""
        m = FX.tone(1000.0, 3.0, 0.1)
        self.assertAlmostEqual(MM.lufs_i(m), MM.lufs_i(D.st(m)), delta=1e-12)
        self.assertAlmostEqual(MM.lufs_i(np.stack([m, 0 * m], 1)), MM.lufs_i(m) - 3.01, delta=0.01)


class TestTruePeak(unittest.TestCase):
    def test_the_quarter_rate_sine_between_samples(self):
        """BS.1770-4 Annex 2's case: a sine at fs/4, 45 degrees off the grid: the samples at -3.01 dB, the true peak
        at 0."""
        x = stereo_sine(SR / 4, 0.0, 1.0, np.pi / 4)
        self.assertAlmostEqual(D.db(np.abs(x).max()), -3.01, delta=0.01)
        self.assertAlmostEqual(MM.true_peak_db(x), 0.0, delta=0.05)

    def test_a_low_tone(self):
        self.assertAlmostEqual(MM.true_peak_db(stereo_sine(997.0, -6.0, 1.0), loop=False), -6.0, delta=0.01)

    def test_linear_mode_is_dsp48s(self):
        for x in (FX.master(FX.clean_kick()), FX.intersample_peak(), FX.low_side_at(-17.0)):
            self.assertEqual(MM.true_peak_db(x, loop=False), D.true_peak_db(x))

    def test_a_loop_reads_the_peak_across_its_seam(self):
        """A band-limited pulse centred on the seam: as a file its two halves are cut and read lower; as a loop it is
        whole, and any rotation reads the same."""
        n = SR
        k = np.arange(n)
        pos = n - 0.5                                          # half a sample before the wrap: between n - 1 and 0
        d = (k - pos + n / 2) % n - n / 2
        x = D.st(0.9 * np.sinc(0.9 * d) * np.hanning(129)[np.clip(np.round(d).astype(int) + 64, 0, 128)]
                 * (np.abs(d) <= 64))
        circ = MM.true_peak_db(x)
        self.assertGreater(circ, MM.true_peak_db(x, loop=False) + 0.3)
        for shift in (1, 777, n // 2):
            self.assertAlmostEqual(MM.true_peak_db(np.roll(x, shift, 0)), circ, delta=1e-9)

    def test_plr_and_psr(self):
        x = plr_loop(7.0, lufs=-9.0)
        got = MM.loudness(x)
        self.assertAlmostEqual(got['lufs_i'], -9.0, delta=1e-6)
        self.assertAlmostEqual(got['plr'], 7.0, delta=0.01)
        self.assertAlmostEqual(got['plr'], got['true_peak'] - got['lufs_i'], delta=1e-12)
        self.assertAlmostEqual(got['psr'], got['true_peak'] - got['lufs_s_max'], delta=1e-12)


class TestBands(unittest.TestCase):
    def test_pink_noise_is_flat_and_white_rises_3_db_an_octave(self):
        n = 4 * SR
        f = np.fft.rfftfreq(n, 1 / SR)
        r = D.rng('test', 'mixmeter', 'pink')
        ph = np.exp(2j * np.pi * r.uniform(size=len(f)))
        with np.errstate(divide='ignore'):
            pink = np.fft.irfft(np.where(f > 0, 1 / np.sqrt(np.maximum(f, 1e-9)), 0) * ph, n)
        white = np.fft.irfft(np.where(f > 0, 1.0, 0.0) * ph, n)
        for name, got in MM.balance(D.st(pink)).items():
            self.assertAlmostEqual(got, 0.0, delta=0.15, msg=name)
        for (name, got), fc in zip(MM.balance(D.st(white)).items(), MM.CENTRES):
            self.assertAlmostEqual(got, 10 * np.log10(fc / 1000.0), delta=0.15, msg=name)

    def test_tones_land_in_their_band(self):
        """Tones of whole cycles in the loop sit on one FFT bin each: a band reads exactly its tones. The 1k band ends
        at 1414.2 Hz: 1414.0 Hz is in it, 1414.5 Hz in the 2k band."""
        x = FX.tone(1000.0, 2.0) + FX.tone(250.0, 2.0, 0.25)
        got = MM.balance(D.st(x))
        self.assertAlmostEqual(got['250'], -6.02, delta=0.01)
        self.assertLess(got['4k'], -200.0)
        under = MM.balance(D.st(FX.tone(1000.0, 2.0) + FX.tone(1414.0, 2.0)))
        over = MM.balance(D.st(FX.tone(1000.0, 2.0) + FX.tone(1414.5, 2.0)))
        self.assertLess(under['2k'], -200.0)
        self.assertAlmostEqual(over['2k'], 0.0, delta=0.01)

    def test_a_shaped_loop_reads_its_levels(self):
        levels = {'31.5': -5.0, '63': 8.0, '125': 4.0, '250': 2.0, '500': 1.0, '2k': -2.0, '4k': -4.0, '8k': -6.0,
                  '16k': -11.0}
        got = MM.balance(shaped_loop(levels))
        for name in MM.BANDS:
            self.assertAlmostEqual(got[name], levels.get(name, 0.0), delta=1e-9, msg=name)

    def test_shape(self):
        """The standard's shape checks: the 125 and 250 Hz bands against the line from the sub (the louder of 31.5 and
        63) to 500 Hz, and the top's slope from 500 Hz to 8 kHz."""
        b = {'31.5': -6.0, '63': 9.0, '125': 4.0, '250': 2.0, '500': 1.0, '1k': 0.0, '2k': -2.0, '4k': -5.0,
             '8k': -7.0, '16k': -12.0}
        got = MM.shape(b)
        self.assertAlmostEqual(got['hole_125'], 4.0 - (9.0 - 8.0 / 3), delta=1e-12)
        self.assertAlmostEqual(got['hole_250'], 2.0 - (9.0 - 16.0 / 3), delta=1e-12)
        self.assertAlmostEqual(got['top_slope'], (-7.0 - 1.0) / 4, delta=1e-12)
        self.assertAlmostEqual(got['sub_hump'], 9.0 - 1.0, delta=1e-12)
        e1 = dict(b, **{'31.5': 9.5, '63': 0.0})               # an E1 sub reads in the 31.5 Hz band
        self.assertAlmostEqual(MM.shape(e1)['sub_hump'], 9.5 - 1.0, delta=1e-12)


class TestWidth(unittest.TestCase):
    def test_mono_wide_and_anti_phase(self):
        r = D.rng('test', 'mixmeter', 'width')
        a, b = r.standard_normal(SR), r.standard_normal(SR)
        mono = MM.bands(np.stack([a, a], 1))
        self.assertTrue(all(v['width'] < -200 and v['corr'] > 0.999 for v in mono.values()))
        wide = MM.bands(np.stack([a, b], 1))
        for name in ('500', '1k', '4k', '16k'):
            self.assertAlmostEqual(wide[name]['width'], 0.0, delta=0.6, msg=name)
            self.assertAlmostEqual(wide[name]['corr'], 0.0, delta=0.1, msg=name)
        anti = MM.bands(np.stack([a, -a], 1))
        self.assertTrue(all(v['width'] > 200 and v['corr'] < -0.999 for v in anti.values()))

    def test_a_shaped_loop_reads_its_width(self):
        want = {'250': -9.5, '2k': -6.0, '8k': -4.0}
        got = MM.width(shaped_loop(width=want))
        for name in MM.BANDS:
            if name in want:
                self.assertAlmostEqual(got[name], want[name], delta=1e-9, msg=name)
            else:
                self.assertLess(got[name], -200.0, name)

    def test_under_120_hz_matches_qa(self):
        for side in (-23.0, -17.0):
            x = FX.low_side_at(side)
            self.assertAlmostEqual(MM.side_mid_db(x, 0.0, 120.0), Q.low_side_db(x), delta=1e-9)


class TestPunch(unittest.TestCase):
    def test_crest_is_the_kick_crest(self):
        for x in (FX.tone_burst(), FX.master(FX.clean_kick()), FX.clipped_kick(6.0)):
            self.assertEqual(MM.crest_db(x), Q.kick_crest_db(x))
        self.assertAlmostEqual(MM.crest_db(FX.tone(1000.0)), 3.01, delta=0.02)

    def test_attack(self):
        """A 2 kHz tone rising linearly for 10 ms, then falling: the peak comes about 10 ms after the onset (the
        envelope's first point within 30 dB of it, 0.3 ms in)."""
        n = SR
        t = np.arange(n) / SR
        env = np.minimum(t / 0.010, np.exp(-(t - 0.010) / 0.1))
        x = np.zeros(n + 4800)
        x[4800:] = np.sin(2 * np.pi * 2000.0 * t) * env
        self.assertAlmostEqual(MM.attack_ms(x), 9.7, delta=0.6)
        click = np.zeros(SR)
        click[4800:4848] = np.hanning(48)
        self.assertLess(MM.attack_ms(click), 1.0)

    def test_jump(self):
        """A 1 kHz bed with a 20 dB swell at each onset (1 ms raised-cosine edges, held 20 ms): a 20 dB jump at every
        onset, over the whole band or in a band round the tone."""
        n = 2 * SR
        onsets = [int(0.25 * SR), int(0.75 * SR), int(1.25 * SR), int(1.75 * SR)]
        g = np.ones(n)
        e = int(0.001 * SR)
        ramp = 0.5 - 0.5 * np.cos(np.pi * np.arange(e) / e)
        for p in onsets:
            g[p:p + e] = 1 + 9 * ramp
            g[p + e:p + int(0.02 * SR)] = 10.0
            g[p + int(0.02 * SR):p + int(0.02 * SR) + e] = 1 + 9 * ramp[::-1]
        x = D.st(0.01 * np.sin(2 * np.pi * 1000.0 * np.arange(n) / SR) * g)
        got = MM.jump_db(x, onsets)
        self.assertEqual(len(got), 4)
        for j, b in zip(got, MM.jump_db(x, onsets, 500.0, 2000.0)):
            self.assertAlmostEqual(j, 20.0, delta=0.3)
            self.assertAlmostEqual(b, j, delta=0.3)


    def test_hit_crest(self):
        """ADR 0011's crest per hit as the loop plays it: a kick alone in the loop reads what its own file reads; the
        same kick clipped reads lower, the jump barely moves; a hit across the seam reads whole. The last kick starts
        20 ms before the seam, so its loudest 50 ms (from 27 ms) lies across it, and its 0.3 s ends 70 ms before the
        first kick: read without the wrap, it would be cut short and read 10.3 dB."""
        kick = FX.clean_kick(1.0)[:int(0.3 * SR)]
        n = int(2.0 * SR)
        onsets = [int(t * SR) for t in (0.35, 0.85, 1.35, 1.98)]        # the last one runs across the seam
        place = lambda k: sum(np.roll(np.concatenate([D.st(k), np.zeros((n - len(k), 2))]), o, axis=0)
                              for o in onsets)
        clean, flat = place(kick), place(FX.kick_crest_at(3.5)[:int(0.3 * SR)])
        own = Q.kick_crest_db(kick)
        for got in MM.hit_crest_db(clean, onsets):
            self.assertAlmostEqual(own, got, delta=0.01)
        for got in MM.hit_crest_db(flat, onsets):
            self.assertAlmostEqual(3.5, got, delta=0.05)
        self.assertEqual(4, len(MM.hit_crest_db(clean, onsets, 0.1, 1000.0, 5000.0)))
        self.assertLess(abs(np.median(MM.jump_db(clean, onsets)) - np.median(MM.jump_db(flat, onsets))), 3.0)

    def test_hit_crest_in_a_band(self):
        """With lo and hi, the crest of that band only: a short 2 kHz burst over a loud held 60 Hz tone reads its own
        crest in 1-5 kHz, where the tone doesn't reach (15.5 dB), and little more than the tone's in the full band (4.3
        dB, the tone alone 3.0)."""
        n, o = 2 * SR, int(0.5 * SR)
        t = np.arange(n) / SR
        tone = 0.8 * np.sin(2 * np.pi * 60.0 * t)                       # 120 whole cycles in the loop
        burst = np.zeros(n)
        b = np.arange(int(0.1 * SR)) / SR
        burst[o:o + len(b)] = 0.3 * np.sin(2 * np.pi * 2000.0 * b) * np.exp(-b / 0.005) * np.minimum(1.0, b / 0.0005)
        own = Q.kick_crest_db(burst[o:o + int(0.1 * SR)])
        band = MM.hit_crest_db(tone + burst, [o], 0.1, 1000.0, 5000.0)[0]
        full = MM.hit_crest_db(tone + burst, [o], 0.1)[0]
        self.assertGreater(own, 12.0)
        self.assertAlmostEqual(own, band, delta=0.1)
        self.assertLess(full, 5.0)

    def test_a_hit_s_peak_is_read_with_the_loop_round_it(self):
        """A hit whose span ends inside a held loud sound (a 0.5 plateau, eased in after the kick and held past the
        span's end) reads the plateau's true peak, -6.02 dBTP, as the loop plays it. The span cut out on its own meets
        silence at its end, and the 4x FIR rings on that step: it would read 1.1 dB high."""
        n, o = int(0.2 * SR), int(0.25 * SR)
        x = np.zeros(SR)
        kick = FX.clean_kick(1.0)[:int(0.12 * SR)]
        x[o:o + len(kick)] += 0.4 * kick / np.abs(kick).max()
        a, b, r = o + int(0.15 * SR), o + n + int(0.05 * SR), int(0.02 * SR)
        ease = 0.5 - 0.5 * np.cos(np.pi * np.arange(r) / r)
        x[a:a + r] += 0.5 * ease
        x[a + r:b] += 0.5
        x[b:b + r] += 0.5 * ease[::-1]
        span = x[o:o + n]
        c = np.concatenate([[0.0], np.cumsum(span ** 2)])
        w = int(Q.KICK_CREST_WINDOW * SR)
        want = D.db(0.5) - 10 * np.log10((c[w:] - c[:-w]).max() / w)
        self.assertGreater(Q.kick_crest_db(span) - want, 1.0, 'cut out on its own, the span over-reads')
        self.assertAlmostEqual(want, MM.hit_crest_db(x, [o])[0], delta=0.005)
        late = SR - n // 2                                              # the same hit with its span across the seam
        self.assertAlmostEqual(want, MM.hit_crest_db(np.roll(x, late - o), [late])[0], delta=0.005)


class TestLean(unittest.TestCase):
    def test_a_balanced_wide_sound_and_a_lean(self):
        """Right over left per third octave: 0 for independent noise of equal level on both sides (wide, balanced),
        the lean where one side is louder in one band only."""
        n = 2 * SR
        f = np.fft.rfftfreq(n, 1 / SR)
        r = D.rng('test', 'mixmeter', 'lean')
        X = np.exp(2j * np.pi * r.random((len(f), 2)))                 # flat on both sides, independent phases
        X[0] = X[-1] = 0.0
        x = np.fft.irfft(X, n=n, axis=0)
        self.assertLess(abs(Q.stereo_corr(x)), 0.05)                     # wide
        for fc, v in MM.lean_db(x).items():
            self.assertAlmostEqual(0.0, v, delta=1e-9, msg=fc)
        X[(f >= 2000.0 * 2 ** (-1 / 6)) & (f < 2000.0 * 2 ** (1 / 6)), 1] *= D.undb(3.0)
        lean = MM.lean_db(np.fft.irfft(X, n=n, axis=0))
        self.assertAlmostEqual(3.0, lean[2000.0], delta=1e-9)
        self.assertAlmostEqual(0.0, lean[4000.0], delta=1e-9)
        self.assertEqual(22, len(MM.third_octaves()))


class TestReference(unittest.TestCase):
    """The standard's S4: a reference track measured as the kits are, as a file: its loudest 16 s, resampled to
    48 kHz for measuring only (python -m tools.sound.mixmeter --reference <file>). The test tracks are written to a
    temporary folder that is removed when the class is done."""

    @classmethod
    def setUpClass(cls):
        import tempfile
        cls.tmp = tempfile.TemporaryDirectory()
        cls.addClassCleanup(cls.tmp.cleanup)

    @classmethod
    def track(cls, sr, mono=False):
        """30 s at sr: quiet noise, then 16 s of a loud 1 kHz tone from 8 s, then quiet again; stereo L = R, or one
        channel."""
        import soundfile as sf
        t = np.arange(30 * sr) / sr
        r = D.rng('test', 'mixmeter', 'reference')
        x = 0.001 * r.standard_normal(len(t))
        loud = (t >= 8.0) & (t < 24.0)
        x[loud] += 0.5 * np.sin(2 * np.pi * 1000.0 * t[loud])
        path = Path(cls.tmp.name) / f"ref_{sr}{'_mono' if mono else ''}.wav"
        if not path.exists():
            sf.write(path, x if mono else np.stack([x, x], 1), sr, subtype='PCM_24')
        return path

    def test_the_loudest_16_s_at_any_rate(self):
        for sr in (44100, 48000, 96000):
            with self.subTest(sr=sr):
                r = MM.reference(self.track(sr))
                self.assertAlmostEqual(8.0, r['section_s'][0], delta=0.5)
                self.assertAlmostEqual(16.0, r['section_s'][1] - r['section_s'][0], delta=1e-3)
                tone = stereo_sine(1000.0, D.db(0.5), 16.0)
                self.assertAlmostEqual(MM.lufs_i(tone, loop=False), r['loudness']['lufs_i'], delta=0.1)
                self.assertGreater(r['bands']['1k']['level'] - max(r['bands']['500']['level'],
                                                                   r['bands']['2k']['level']), 30.0)

    def test_the_command(self):
        import subprocess
        import sys
        path = self.track(44100)
        repo = Path(__file__).resolve().parents[3]
        out = subprocess.run([sys.executable, '-m', 'tools.sound.mixmeter', '--reference', str(path)], cwd=repo,
                             capture_output=True, text=True, check=True).stdout
        self.assertIn('LUFS-I', out)
        self.assertIn('octaves re 1 kHz', out)
        js = subprocess.run([sys.executable, '-m', 'tools.sound.mixmeter', '--reference', str(path), '--json'],
                            cwd=repo, capture_output=True, text=True, check=True).stdout
        import json
        self.assertEqual({'section_s', 'loudness', 'bands', 'shape', 'corr', 'lean'}, set(json.loads(js)[str(path)]))

    def test_a_section_by_its_start_and_length(self):
        """--start and --seconds pick the section instead of the loudest 16 s."""
        import contextlib
        import io
        import json
        path = self.track(48000)
        buf = io.StringIO()
        with contextlib.redirect_stdout(buf):
            self.assertEqual(0, MM.main(['--reference', str(path), '--start', '4', '--seconds', '8', '--json']))
        self.assertEqual([4.0, 12.0], json.loads(buf.getvalue())[str(path)]['section_s'])
        self.assertEqual([4.0, 12.0], MM.reference(path, start_s=4.0, seconds=8.0)['section_s'])

    def test_a_mono_file_reads_as_left_and_right(self):
        """A mono file is measured as L = R: the same report as its stereo copy."""
        import json
        mono, stereo = MM.reference(self.track(48000, mono=True)), MM.reference(self.track(48000))
        self.assertEqual(json.dumps(stereo, sort_keys=True), json.dumps(mono, sort_keys=True))

    def test_the_section_is_read_as_a_file(self):
        """A section is a stretch of a record, not a loop: its loudness is qa.lufs of the section as a file, not read
        round a seam it doesn't have. Noise whose last 2 s are 6 dB louder tells the two apart (0.07 LU here)."""
        n = 16 * SR
        x = D.st(0.1 * D.rng('test', 'mixmeter', 'section').standard_normal(n))
        x[-2 * SR:] *= D.undb(6.0)
        r = MM.reference(x, start_s=0.0, seconds=16.0)
        self.assertEqual([0.0, 16.0], r['section_s'])
        self.assertAlmostEqual(Q.lufs(x), r['loudness']['lufs_i'], delta=1e-9)
        self.assertGreater(abs(MM.lufs_i(x, loop=True) - Q.lufs(x)), 0.03)


if __name__ == '__main__':
    unittest.main()
