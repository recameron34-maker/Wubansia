"""The club master stand-in (tools/sound/master.py): it reaches each genre's loudness under the true-peak ceiling,
keeps the genre's PLR floor, adds no aliasing, leaves the balance of the mix alone, keeps a mono low end mono, joins
round the loop, says what it did and renders the same bytes every time, in a fresh process too.

The fixture is a 2-bar club loop at 128 BPM built here (club_mix): a split kick, a clap, 16th hats, a rolling bass,
a wide pad and an air bed, at PLR 11.2 dB, denser than the round 2 context loops (13.9-15.9) as the standard asks. With
the default 4 dB cap on peak reduction it reaches -9 and -8.5 LUFS but stops 0.5 LU short of -8: a -8 LUFS master needs
a denser mix still. So the targets are tested with a 6 dB cap, and the cap on its own.

Every master of the fixture is cached (mastered()), so the tests share them."""
import dataclasses
import functools
import hashlib
import json
import platform
import subprocess
import sys
import unittest
import unittest.mock
from pathlib import Path

import numpy as np

from tools.sound import dsp48 as D
from tools.sound import kits
from tools.sound import master as MS
from tools.sound import mixmeter as MM
from tools.sound import qa as Q
from tools.sound.tests import fixtures as FX

SR = D.SR
REPO = Path(__file__).resolve().parents[3]
RENDER_HOST = sys.platform.startswith('linux') and platform.machine() == 'x86_64'   # as test_render's (ADR 0007)
BPM = 128.0
STEP = int(round(15.0 / BPM * SR))            # a 16th: 5625 samples
L = 32 * STEP                                 # 2 bars: 180000 samples, 3.75 s


def place(buf, x, pos):
    np.add.at(buf, (pos + np.arange(len(x))) % len(buf), D.st(x))


def noise(key, n):
    return D.rng('test', 'master', key).standard_normal(n)


def whole(f):
    """f rounded to whole cycles in the loop, so a held tone joins at the seam."""
    return round(f * L / SR) * SR / L


@functools.lru_cache(maxsize=None)
def club_mix(wide_low=False):
    """The fixture loop, (L, 2), peak 0.5: a split kick four on the floor, a three-hand clap on 2 and 4, 16th hats
    panned left and right, a rolling 16th bass (three 16ths after each kick, a sine sub on F#1 and a saw an octave up),
    an F#m pad on detuned saws and an air bed; everything but the hats, the pad's top and the air is mono, and all of it
    is mono under 150 Hz. PLR 11.2 dB. wide_low: an anti-phase F#2 under the pad, a low end the master has to fold
    back to mono."""
    y = np.zeros((L, 2))
    kick = FX.clean_kick(1.0)
    for s in range(0, 32, 4):
        place(y, 0.8 * kick, s * STEP)
    n = int(0.25 * SR)
    t = np.arange(n) / SR
    hands = sum(np.roll(np.exp(-t / 0.006), int(d * SR)) * (t >= d) for d in (0.0, 0.009, 0.018))
    clap = D.bfilt(np.stack([noise('clap L', n), noise('clap R', n)], 1), (900.0, 6000.0), 'bandpass', order=2)
    clap = D.mono_below(clap * (hands + 0.4 * np.exp(-t / 0.07))[:, None], 150.0)
    for s in (4, 12, 20, 28):
        place(y, 0.35 * clap, s * STEP)
    m = int(0.08 * SR)
    hat = D.bfilt(noise('hat', m), 7000.0, 'highpass', order=4) * np.exp(-np.arange(m) / SR / 0.02)
    for s in range(32):
        p = 0.3 if s % 2 else -0.3
        pan = np.array([np.cos((p + 1) * np.pi / 4), np.sin((p + 1) * np.pi / 4)])
        place(y, (0.128 if s % 4 == 2 else 0.08) * hat[:, None] * pan, s * STEP)
    tl = np.arange(L) / SR
    f0 = whole(float(D.midi_hz(30)))
    tone = np.sin(2 * np.pi * f0 * tl) + 0.5 * D.bfilt(D.additive_saw(2 * f0, L), 600.0, 'lowpass', order=2)
    gate = np.zeros(L)
    e, held = int(0.004 * SR), int(0.85 * STEP)
    env = np.ones(held)
    env[:e], env[-e:] = np.linspace(0, 1, e), np.linspace(1, 0, e)
    for s in range(32):
        if s % 4:
            gate[s * STEP:s * STEP + held] = env
    y = y + 0.45 * D.st(tone * gate)
    pad = np.zeros((L, 2))
    for i, midi in enumerate((66, 69, 73)):
        f = whole(float(D.midi_hz(midi)))
        for ch, det in ((0, -0.002), (1, 0.002)):
            pad[:, ch] += D.additive_saw(f * (1 + det), L, ph0=0.1 * i + 0.3 * ch, fmax=8000.0)
    pad = D.mono_below(D.bfilt(D.bfilt(pad, 150.0, 'highpass', order=4), 3000.0, 'lowpass', order=2), 150.0)
    y = y + 0.25 * pad
    y = y + 0.02 * D.bfilt(np.stack([noise('air L', L), noise('air R', L)], 1), 3000.0, 'highpass', order=2)
    if wide_low:
        sub = 0.2 * np.sin(2 * np.pi * whole(2 * f0) * tl)
        y = y + np.stack([sub, -sub], 1)
    return y / np.abs(y).max() * 0.5


@functools.lru_cache(maxsize=None)
def mastered(genre, scale_db=0.0, wide_low=False, **overrides):
    x = club_mix(wide_low) * D.undb(scale_db)
    return MS.master(x, MS.settings(genre=genre, **overrides))


def check(x, spec, name):
    return {c.name: c for c in Q.check_take(x, spec)}[name]


class TestTargets(unittest.TestCase):
    def test_the_fixture_is_a_mix_not_a_master(self):
        lo = MM.loudness(club_mix())
        self.assertAlmostEqual(lo['plr'], 11.2, delta=0.3)
        self.assertLess(lo['lufs_i'], -15.0)

    def test_each_genre_s_loudness_under_the_ceiling(self):
        """Within 0.5 LU of the genre's target (the solve stops within 0.01), the true peak (4x) at or under the
        ceiling both round the loop and as a file (the renderer's QA reads the file), and the club checks pass."""
        for genre in kits.DIRECTIONS:
            with self.subTest(genre=genre):
                y, rep = mastered(genre, max_reduction_db=6.0)
                s = MS.settings(genre=genre)
                self.assertAlmostEqual(MM.lufs_i(y), s.target_lufs, delta=0.5)
                self.assertLessEqual(MM.true_peak_db(y), s.ceiling_dbtp + 1e-9)
                self.assertLessEqual(MM.true_peak_db(y, loop=False), s.ceiling_dbtp + 1e-9)
                self.assertTrue(rep['target']['reached'])
                self.assertLess(rep['peak_reduction_db'], 6.0)
                self.assertIs(check(y, Q.Spec('loop', oneshot=False), 'true_peak').ok, True)
                spec = Q.Spec('loop', oneshot=False, tags=('club',), genre=genre)
                for name in ('club_loudness', 'club_plr', 'club_true_peak'):
                    self.assertIs(check(y, spec, name).ok, True, name)

    def test_the_genre_s_plr_floor(self):
        for genre in kits.DIRECTIONS:
            with self.subTest(genre=genre):
                y, _ = mastered(genre, max_reduction_db=6.0)
                self.assertGreaterEqual(MM.loudness(y)['plr'], Q.CLUB[genre]['plr_min'])

    def test_the_default_cap(self):
        """With the default 4 dB, the fixture reaches UK garage's -9 and sci-fi club's -8.5 under the cap; for -8 the
        master stops at the cap and says how far short it is."""
        for genre, reached in (('future garage / 2-step', True), ('sci-fi club', True),
                               ('hyper-clean future house', False)):
            with self.subTest(genre=genre):
                y, rep = mastered(genre)
                self.assertIs(rep['target']['reached'], reached)
                self.assertLessEqual(rep['peak_reduction_db'], 4.0 + 1e-9)
                if not reached:
                    self.assertAlmostEqual(rep['peak_reduction_db'], 4.0, delta=1e-9)
                    self.assertGreater(rep['target']['short_lu'], 0.1)

    def test_never_louder_than_the_plr_floor_allows(self):
        """A target over the ceiling minus the PLR floor is pulled back to it, and the report says so."""
        genre = 'hyper-clean future house'
        y, rep = mastered(genre, target_lufs=-5.0, max_reduction_db=8.0)
        s = MS.settings(genre=genre)
        self.assertAlmostEqual(rep['target']['lufs'], s.ceiling_dbtp - s.plr_min, delta=1e-12)
        self.assertEqual(rep['target']['asked'], -5.0)
        self.assertGreaterEqual(MM.loudness(y)['plr'], s.plr_min - 0.05)

    def test_a_sparse_mix_stops_at_the_reduction_cap(self):
        """A lone kick can't reach a club loudness with 4 dB of peak reduction: the master stops there and reports how
        far short it is, rather than crushing it."""
        x = np.zeros((L, 2))
        for s in range(0, 32, 4):
            place(x, 0.5 * FX.clean_kick(1.0), s * STEP)
        y, rep = MS.master(x, MS.settings(genre='dark industrial / techno'))
        self.assertFalse(rep['target']['reached'])
        self.assertAlmostEqual(rep['target']['short_lu'], rep['target']['lufs'] - MM.lufs_i(y), delta=0.01)
        self.assertGreater(rep['target']['short_lu'], 1.0)
        self.assertLessEqual(rep['peak_reduction_db'], 4.0 + 0.01)
        self.assertLessEqual(MM.true_peak_db(y), D.TP_TARGET + 1e-9)

    def test_a_dense_mix_only_takes_gain(self):
        """A mix already dense enough reaches the target before anything clips: the clipper and limiter stay out."""
        t = np.arange(L) / SR
        f = round(220.0 * L / SR) * SR / L
        x = D.st(0.05 * (np.sin(2 * np.pi * f * t) + 0.5 * np.sin(2 * np.pi * 2 * f * t)))
        y, rep = MS.master(x, MS.settings(genre='future garage / 2-step', glue_gr_db=0.0))
        self.assertAlmostEqual(MM.lufs_i(y), -9.0, delta=0.05)
        self.assertEqual(rep['clip']['reduction_db'], 0.0)
        self.assertEqual(rep['limit']['gr_max_db'], 0.0)
        self.assertLess(MM.true_peak_db(y), D.TP_TARGET)


class TestTransparency(unittest.TestCase):
    def test_no_aliasing_on_a_driven_tone(self):
        """Saws (the alias check's own fixture, plus the bass register) driven 3-6 dB into the clipper and limiter:
        nothing between the harmonics (the per-file alias check, -60 dB). The limiter's hold keeps its gain still on a
        held note, so it adds no sidebands either."""
        genre = 'dark industrial / techno'
        for f0 in (55.0, 220.0, 1760.0, 3520.0, 7040.0):   # ADAA's work shows at 3.5-7 kHz: without it a 6 dB drive
            n = int(1.2 * SR)                                # reads -58.9 and -54.6 dB there (ADR 0014)
            f = round(f0 * n / SR) * SR / n
            x = D.st(0.5 * D.additive_saw(f, n))
            for drive in (3.0, 6.0):
                with self.subTest(f0=f0, drive=drive):
                    # a target nothing reaches, so the master drives to the cap: a high saw clipped toward a square
                    # reads louder than its own true peak (K-weighting lifts 1.76 kHz), and reaches 0 LUFS
                    st = MS.settings(genre=genre, plr_min=-20.0, max_reduction_db=drive, target_lufs=10.0)
                    y, rep = MS.master(x, st)
                    self.assertGreater(rep['clip']['reduction_db'], 1.0)
                    self.assertAlmostEqual(rep['peak_reduction_db'], drive, delta=0.01)
                    got = check(y, Q.Spec('lead', f0=f, oneshot=False), 'alias')
                    self.assertIs(got.ok, True, f'{got.value} dB')

    def test_a_quiet_mix_keeps_its_balance(self):
        """The fixture at -24 LUFS, mastered to -8: every octave band within 1 dB of the input's balance (re 1 kHz).
        The master is gain and dynamics, not an EQ (its low cut is under the 31.5 Hz band's content here)."""
        x = club_mix() * D.undb(-24.0 - MM.lufs_i(club_mix()))
        y, rep = mastered('hyper-clean future house', scale_db=-24.0 - MM.lufs_i(club_mix()), max_reduction_db=6.0)
        self.assertTrue(rep['target']['reached'])
        before, after = MM.balance(x), MM.balance(y)
        for name in MM.BANDS:
            self.assertAlmostEqual(after[name], before[name], delta=1.0, msg=name)

    def test_a_mono_low_end_stays_mono(self):
        """The fixture's low end is mono (-78 dB of side under 120 Hz) and its top wide. The clipper works per channel,
        so where a wide top meets a clipped kick it puts a little side back under 120 Hz: -46 dB here (-43 to -50 over
        the genres, at caps of 4 and 6 dB), held here to 30 dB under the mid, 10 dB inside QA's -20."""
        x = club_mix()
        self.assertLess(MM.side_mid_db(x, 0.0, 120.0), -70.0)
        y, _ = mastered('sci-fi club')
        self.assertLess(MM.side_mid_db(y, 0.0, 120.0), -30.0)
        self.assertIs(check(y, Q.Spec('loop', oneshot=False), 'low_side').ok, True)
        self.assertGreater(MM.width(y)['8k'], -20.0)                         # the top stays wide

    def test_a_wide_low_end_comes_out_mono(self):
        x = club_mix(wide_low=True)
        self.assertGreater(MM.side_mid_db(x, 0.0, 120.0), -12.0)
        y, _ = mastered('sci-fi club', wide_low=True)
        self.assertIs(check(y, Q.Spec('loop', oneshot=False), 'low_side').ok, True)
        self.assertLess(MM.side_mid_db(y, 0.0, 120.0), -30.0)

    def test_mono_in_mono_out(self):
        """L = R in gives L = R out, so the renderer stores a mono loop as mono."""
        m = club_mix()[:, 0]
        y, _ = MS.master(m, MS.settings(genre='hyper-clean future house'))
        self.assertTrue(np.array_equal(y[:, 0], y[:, 1]))

    def test_the_loop_joins(self):
        """Every stage runs round the loop: the seam is as smooth as the rest, and no DC is left (the per-file
        checks)."""
        y, _ = mastered('dark industrial / techno')
        spec = Q.Spec('loop', oneshot=False)
        for name in ('seam', 'dc', 'stereo_corr'):
            self.assertIs(check(y, spec, name).ok, True, name)

    def test_the_low_cut(self):
        """Under 25 Hz goes; a kick's fundamental stays."""
        t = np.arange(L) / SR
        tone = lambda f: D.st(np.sin(2 * np.pi * (round(f * L / SR) * SR / L) * t))
        level = lambda x: 10 * np.log10(np.mean(x ** 2))
        self.assertLess(level(MS.lowcut(tone(12.0))) - level(tone(12.0)), -24.0)
        self.assertGreater(level(MS.lowcut(tone(41.2))) - level(tone(41.2)), -0.15)


class TestReport(unittest.TestCase):
    def test_what_it_did(self):
        genre = 'future garage / 2-step'
        y, rep = mastered(genre)
        json.dumps(rep)
        lo = MM.loudness(y)
        out = rep['output']
        self.assertAlmostEqual(out['lufs_i'], lo['lufs_i'], delta=0.005)
        self.assertAlmostEqual(out['true_peak'], lo['true_peak'], delta=0.005)
        self.assertAlmostEqual(out['plr'], lo['plr'], delta=0.01)
        self.assertAlmostEqual(out['true_peak_file'], MM.true_peak_db(y, loop=False), delta=0.005)
        self.assertAlmostEqual(rep['input']['lufs_i'], MM.lufs_i(club_mix()), delta=0.005)
        self.assertAlmostEqual(rep['glue']['gr_max_db'], MS.settings(genre=genre).glue_gr_db, delta=0.05)
        self.assertGreater(rep['glue']['gr_mean_db'], 0.0)
        self.assertGreater(rep['clip']['reduction_db'], 0.0)
        self.assertLessEqual(rep['peak_reduction_db'], 4.0 + 0.01)
        self.assertEqual(rep['genre'], genre)
        self.assertEqual(rep['settings']['target_lufs'], -9.0)
        for key in ('drive_db', 'trim_db'):
            self.assertIsInstance(rep[key], float)

    def test_a_silent_loop(self):
        y, rep = MS.master(np.zeros((L, 2)), MS.settings(genre='sci-fi club'))
        self.assertTrue(np.array_equal(y, np.zeros((L, 2))))
        self.assertTrue(rep['silent'])
        json.dumps(rep)


class TestSettings(unittest.TestCase):
    def test_from_the_kit_s_direction(self):
        for name in kits.names():
            k = kits.module(name)
            s = MS.settings(k)
            self.assertEqual(s.genre, k.DIRECTION)
            self.assertEqual(s.target_lufs, Q.CLUB[k.DIRECTION]['lufs'][0])
            self.assertEqual(s.plr_min, Q.CLUB[k.DIRECTION]['plr_min'])
            self.assertEqual(s.ceiling_dbtp, D.TP_TARGET)

    def test_a_kit_s_own_settings_and_overrides(self):
        class Kit:
            DIRECTION = 'sci-fi club'
            MASTER = {'max_reduction_db': 5.0, 'clip_db': 0.0}
        s = MS.settings(Kit)
        self.assertEqual((s.max_reduction_db, s.clip_db, s.target_lufs), (5.0, 0.0, -8.5))
        s = MS.settings(Kit, clip_db=1.0, target_lufs=-9.0)
        self.assertEqual((s.max_reduction_db, s.clip_db, s.target_lufs), (5.0, 1.0, -9.0))
        with self.assertRaises(ValueError):
            MS.settings(Kit, attack=0.01)
        with self.assertRaises(ValueError):
            MS.settings(genre='polka')
        with self.assertRaises(ValueError):
            MS.settings()

    def test_the_defaults_are_the_standard_s(self):
        """Every genre's target is the centre of its QA window, and the chain is the one ADR 0014 describes: every
        setting pinned here, so a changed default is a change to the chain the engine must match."""
        self.assertEqual(set(Q.CLUB), set(kits.DIRECTIONS))
        s = MS.settings(genre='dark industrial / techno')
        self.assertEqual({'genre': 'dark industrial / techno', 'target_lufs': -8.0, 'plr_min': 6.5,
                          'ceiling_dbtp': -1.02, 'max_reduction_db': 4.0, 'lowcut_hz': 25.0, 'lowcut_order': 4,
                          'mono_hz': 150.0, 'glue_gr_db': 2.5, 'glue_ratio': 2.0, 'glue_knee_db': 6.0,
                          'glue_attack_s': 0.030, 'glue_release_s': (0.15, 1.5), 'clip_db': 0.5, 'clip_knee_db': 1.5,
                          'clip_os': 4, 'limit_lookahead_s': 0.005, 'limit_hold_s': 0.060, 'limit_release_s': 0.080,
                          'dc_hz': 10.0}, dataclasses.asdict(s))
        self.assertEqual(20, len(MS.FIELDS))


def asymmetric_kick_loop():
    """Two bars of a hard asymmetric kick on every beat (a sine sweep whose positive half cycles are three times its
    negative ones): clipping and limiting it leaves sub-sonic ripple at the kick rate, the DC the master cleans."""
    n = int(0.4 * SR)
    t = np.arange(n) / SR
    s = np.sin(2 * np.pi * np.cumsum(50.0 + 150.0 * np.exp(-t / 0.03)) / SR)
    k = np.where(s > 0, 3.0 * s, s) * np.exp(-t / 0.12)
    y = np.zeros((L, 2))
    for st in range(0, 32, 4):
        place(y, 0.5 * k / np.abs(k).max(), st * STEP)
    return y


class TestDcClean(unittest.TestCase):
    def test_the_two_cleans_take_the_ripple_out(self):
        """The asymmetric kick loop driven 6 dB into the clipper and limiter: its 5 Hz DC (QA's measure) reads -99 dBFS
        through the chain, 39 dB under QA's -60. Without the clean after the clipper it reads -85, without the one
        after the limiter -80 (both under -90 is what the chain is pinned to), without both -35, which QA fails."""
        x = asymmetric_kick_loop()
        st = MS.settings(genre='dark industrial / techno', max_reduction_db=6.0, target_lufs=-3.0, plr_min=0.5)
        y, rep = MS.master(x, st)
        self.assertAlmostEqual(6.0, rep['peak_reduction_db'], delta=0.01)
        self.assertLess(Q.dc_db(y, (0, L)), -90.0)
        orig = MS.dc_clean
        try:
            MS.dc_clean = lambda u, hz=10.0: D.st(u)
            bare, _ = MS.master(x, st)
        finally:
            MS.dc_clean = orig
        self.assertGreater(Q.dc_db(bare, (0, L)), Q.DC_MAX_DB, 'the fixture needs the cleans')


class TestLimiterTiming(unittest.TestCase):
    """The limiter on a lone 2 ms burst over a quiet bed: it starts reducing lookahead + hold before the burst (its
    latency, never earlier), ramps to full reduction over the lookahead (a moving average: halfway at half of it, no
    step between samples), holds it from a hold before the burst to a hold after it, and no longer, then releases
    exponentially, to 1/e of it in 80 ms (in dB)."""

    def setUp(self):
        n = 4 * SR
        t = np.arange(n) / SR
        x = 0.1 * np.sin(2 * np.pi * 1000.0 * t)
        self.b, self.e = SR, SR + int(0.002 * SR)
        x[self.b:self.e] += 0.9 * np.sin(2 * np.pi * 5000.0 * t[self.b:self.e])
        self.r = MS.limit_gain_db(D.st(x), D.TP_TARGET)
        self.full = float(self.r.max())
        self.w, self.h = 240, 2880

    def test_the_latency_it_declares(self):
        self.assertEqual(3145, MS.limit_latency())
        self.assertEqual(self.w + self.h + 1 + MS.TP_REACH, MS.limit_latency())
        first = int(np.flatnonzero(self.r > 1e-6)[0])
        self.assertTrue(np.all(self.r[:self.b - MS.limit_latency()] < 1e-6), 'nothing before the latency')
        self.assertGreaterEqual(self.b - first, self.w + self.h - 30, 'the gain starts down lookahead + hold ahead')
        self.assertLessEqual(self.b - first, MS.limit_latency())

    def test_attack_hold_and_release(self):
        self.assertGreater(self.full, 0.9)
        first = int(np.flatnonzero(self.r > 1e-6)[0])
        self.assertGreater(self.r[first + self.w], 0.95 * self.full, 'full within the lookahead')
        held = self.r[self.b - (self.h - 30):self.e + (self.h - 30)]
        self.assertGreater(held.min(), self.full - 1e-3, 'held a hold either side of the burst')
        end = int(np.flatnonzero(self.r >= self.full - 1e-3)[-1])
        self.assertAlmostEqual(np.exp(-1.0), self.r[end + int(0.080 * SR)] / self.full, delta=0.01)
        self.assertLess(self.r[end + int(0.5 * SR)] / self.full, 0.01)

    def test_the_ramp_and_the_hold_s_length(self):
        """The ramp is the lookahead's moving average: half the reduction half way through it (0.46 here), and no step
        between samples over 0.05 dB (0.004 here; a ramp left out steps 0.8 dB at once). Full reduction lasts from a
        hold before the burst to a hold after it (2869 and 2872 samples here, the hold 2880), never longer."""
        first = int(np.flatnonzero(self.r > 1e-6)[0])
        self.assertAlmostEqual(0.5, self.r[first + self.w // 2] / self.full, delta=0.1)
        self.assertLess(float(np.abs(np.diff(self.r)).max()), 0.05)
        full = np.flatnonzero(self.r >= self.full - 1e-3)
        self.assertAlmostEqual(self.h, self.b - int(full[0]), delta=30, msg='full from a hold before the burst')
        self.assertAlmostEqual(self.h, int(full[-1]) - self.e, delta=30, msg='full until a hold after the burst')

    def test_the_report_says_it(self):
        _, rep = mastered('sci-fi club')
        self.assertEqual({'samples': 3145, 'ms': round(1000 * 3145 / SR, 3)}, rep['latency'])


KICKS = tuple(s * STEP for s in range(0, 32, 4))
CLAPS = tuple(s * STEP for s in (4, 12, 20, 28))


class TestPunch(unittest.TestCase):
    """Given the loop's hits, the report says what the master did to each (master.punch): ADR 0011's crest per hit as
    the loop plays it, in the master's input and its output, and the median loss; a clap also in 1-5 kHz."""

    def test_what_the_master_costs_the_hits(self):
        x = club_mix()
        st = MS.settings(genre='hyper-clean future house', max_reduction_db=6.0)
        y, rep = MS.master(x, st, hits={'kick': KICKS, 'clap': CLAPS})
        k, c = rep['punch']['kick'], rep['punch']['clap']
        json.dumps(rep)
        self.assertEqual(list(KICKS), k['onsets'])
        self.assertEqual([round(v, 3) for v in MM.hit_crest_db(x, KICKS)], k['in_db'])
        self.assertEqual([round(v, 3) for v in MM.hit_crest_db(y, KICKS)], k['out_db'])
        self.assertAlmostEqual(float(np.median(np.subtract(MM.hit_crest_db(x, KICKS), MM.hit_crest_db(y, KICKS)))),
                               k['median_loss_db'], delta=1e-9)
        self.assertGreater(k['median_loss_db'], 0.5, 'driven 4.6 dB into the clipper and limiter, the kicks lose crest')
        self.assertEqual(min(MM.hit_crest_db(y, KICKS)), k['min_out_db'])
        self.assertEqual(0.1, c['span_s'])
        self.assertEqual([1000.0, 5000.0], c['band_hz'])
        self.assertEqual(list(CLAPS), c['onsets'])
        for pre, band in (('', ()), ('band_', (1000.0, 5000.0))):     # the claps over 100 ms; full band and 1-5 kHz
            a, b = MM.hit_crest_db(x, CLAPS, 0.1, *band), MM.hit_crest_db(y, CLAPS, 0.1, *band)
            self.assertEqual([round(v, 3) for v in a], c[f'{pre}in_db'], pre)
            self.assertEqual([round(v, 3) for v in b], c[f'{pre}out_db'], pre)
            self.assertAlmostEqual(float(np.median(np.subtract(a, b))), c[f'{pre}median_loss_db'], delta=1e-9)
            self.assertEqual(min(b), c[f'{pre}min_out_db'])
        self.assertNotEqual(c['in_db'], c['band_in_db'], 'the band is read on its own')
        y2, rep2 = MS.master(x, st)
        self.assertNotIn('punch', rep2)
        self.assertNotIn('peaks', rep2)
        self.assertTrue(np.array_equal(y, y2), 'the hits only change the report')

    def test_where_the_clipper_works(self):
        """The report's peaks: how far over the clipper's knee each kind of hit goes, and the loudest moments away
        from the hits; the deepest of them all is the drive's peak over the knee: the peak reduction plus the knee's
        distance under the clipper's ceiling."""
        st = MS.settings(genre='hyper-clean future house', max_reduction_db=6.0)
        _, rep = MS.master(club_mix(), st, hits={'kick': KICKS, 'clap': CLAPS})
        pk = rep['peaks']
        self.assertAlmostEqual(st.ceiling_dbtp + st.clip_db - st.clip_knee_db, pk['knee_dbfs'], delta=1e-12)
        self.assertEqual(MS.PEAK_TOP, len(pk['away_from_hits']))
        deepest = max([pk['kick']['max_db'], pk['clap']['max_db']] + [a['db'] for a in pk['away_from_hits']])
        self.assertAlmostEqual(rep['peak_reduction_db'] + st.clip_knee_db - st.clip_db, deepest, delta=0.02)
        for a, b in zip(pk['away_from_hits'], pk['away_from_hits'][1:]):
            self.assertGreaterEqual(a['db'], b['db'])
            self.assertGreater(abs(a['s'] - b['s']), 0.049)

    def test_a_silent_loop_reports_no_loss(self):
        _, rep = MS.master(np.zeros((L, 2)), MS.settings(genre='sci-fi club'), hits={'kick': KICKS})
        self.assertEqual(0.0, rep['punch']['kick']['median_loss_db'])


class TestDeterminism(unittest.TestCase):
    CODE = ("import tools.sound, hashlib; from tools.sound.tests import test_master as T; "
            "y, _ = T.mastered('sci-fi club'); print(hashlib.sha256(y.tobytes()).hexdigest())")

    def test_the_same_bytes(self):
        a, _ = mastered('sci-fi club')
        b, _ = MS.master(club_mix(), MS.settings(genre='sci-fi club'))
        self.assertEqual(a.tobytes(), b.tobytes())

    def test_the_same_bytes_in_a_fresh_process(self):
        a, _ = mastered('sci-fi club')
        got = subprocess.run([sys.executable, '-c', self.CODE], cwd=REPO, capture_output=True, text=True, check=True)
        self.assertEqual(got.stdout.strip(), hashlib.sha256(a.tobytes()).hexdigest())


def pcm24_sha256(y):
    """SHA-256 of a master as 24-bit PCM (round half up, clipped), little-endian 32-bit words: what the renderer
    stores, without the FLAC encoder's own bytes."""
    q = np.clip(np.floor(np.asarray(y, dtype=float) * 2 ** 23 + 0.5), -2 ** 23, 2 ** 23 - 1).astype('<i4')
    return hashlib.sha256(q.tobytes()).hexdigest()


class TestTheChainIsPinned(unittest.TestCase):
    """ADR 0014 makes this chain the spec the engine's master (S1e) must match, so what Settings can't see is pinned
    too: the module's own constants, how master() hands each setting to its stage, and the output itself. A
    deliberate change to the chain re-pins them in its own commit, as masters.sha256 is (ADR 0008)."""

    def test_the_chain_s_constants(self):
        """The ones ADR 0014's table names (the glue's block rate and detector, the auto release's slow follower) and
        the rest that shape the sound or the report."""
        self.assertEqual((16, 0.005, 0.5), (MS.BLOCK, MS.GLUE_RMS_S, MS.SLOW_ATTACK_S))
        self.assertEqual((SR, 64, 0.01, 24), (MS.RUN_IN, MS.CLIP_PAD, MS.SOLVE_LU, MS.TP_REACH))
        self.assertEqual(({'kick': 0.2, 'clap': 0.1}, (1000.0, 5000.0)), (MS.PUNCH_SPANS, MS.CLAP_BAND))
        self.assertEqual((0.03, 3), (MS.PEAK_NEAR_S, MS.PEAK_TOP))

    def test_each_setting_reaches_its_stage(self):
        """Settings that differ from every default and from each other reach the stages as set: the limiter's
        lookahead, hold and release (in that order), the clipper's ceiling, knee and oversampling, the glue, the low
        cut, the mono and the DC clean."""
        s = MS.settings(genre='sci-fi club', limit_lookahead_s=0.004, limit_hold_s=0.050, limit_release_s=0.070,
                        clip_db=0.4, clip_knee_db=1.2, clip_os=2, glue_gr_db=2.0, glue_ratio=3.0, glue_knee_db=5.0,
                        glue_attack_s=0.020, glue_release_s=(0.12, 1.2), lowcut_hz=24.0, lowcut_order=3,
                        mono_hz=140.0, dc_hz=9.0)
        spies = {name: unittest.mock.patch.object(MS, name, wraps=getattr(MS, name))
                 for name in ('limit', 'soft_clip', 'glue', 'lowcut', 'mono_below', 'dc_clean')}
        with spies['limit'] as limit, spies['soft_clip'] as clip, spies['glue'] as glue, \
                spies['lowcut'] as lowcut, spies['mono_below'] as mono, spies['dc_clean'] as dc:
            MS.master(club_mix(), s)
        self.assertTrue(limit.call_args_list and clip.call_args_list)
        for c in limit.call_args_list:
            self.assertEqual((s.ceiling_dbtp, 0.004, 0.050, 0.070), c.args[1:])
        for c in clip.call_args_list:
            self.assertEqual((s.ceiling_dbtp + 0.4, 1.2, 2), c.args[1:])
        self.assertEqual([(2.0, 3.0, 5.0, 0.020, (0.12, 1.2))], [c.args[1:] for c in glue.call_args_list])
        self.assertEqual([(24.0, 3)], [c.args[1:] for c in lowcut.call_args_list])
        self.assertEqual([(140.0,)], [c.args[1:] for c in mono.call_args_list])
        self.assertTrue(dc.call_args_list)
        self.assertEqual({(9.0,)}, {c.args[1:] for c in dc.call_args_list})

    @unittest.skipUnless(RENDER_HOST, 'masters are hashed on x86_64 Linux only (ADR 0007)')
    def test_the_output_is_pinned(self):
        """The fixture through the chain at two settings (sci-fi club at the standard's 4 dB cap; future house at a
        6 dB cap), as 24-bit PCM: the same under Python 3.12 and 3.13. A changed constant, a stage wired differently,
        the clipper's knee or the true-peak detector's reach changes them."""
        for (genre, kw), want in ((('sci-fi club', {}),
                                   'fdb734dcf84b5db0325c9a0177de475356fa82c52dce039e8ff68d214246d903'),
                                  (('hyper-clean future house', {'max_reduction_db': 6.0}),
                                   '0d156cccfc74c0c0144f04a20101d39fc93b51cb02a87812f925d76885f06055')):
            with self.subTest(genre=genre):
                y, _ = mastered(genre, **kw)
                self.assertEqual(want, pcm24_sha256(y))


if __name__ == '__main__':
    unittest.main()
