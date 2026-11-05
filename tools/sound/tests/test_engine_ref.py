"""The streaming references (rt_ref.py) against the offline stages they stand in for, the streaming mix (engine_ref.py)
on a synthetic mix, and the gates against each kit's committed Wet and Club loops.

The rt_ref tests are small and run everywhere. The gates need the round 5 reference loops (the FLAC masters in
build/<kit>-r5, or the WAVs in content/samples/<kit>/Loops) and re-render each kit's recipe once (about 30 s a
variant): they run when WUBANSIA_R5_BUILD points at a folder holding <kit>-r5/ (or the repo's build/ has it), and
skip otherwise.
"""
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
        key = np.concatenate([np.zeros((500, 2)), _loop(64 * 100, seed=16, peak=0.5)])
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
        self.assertEqual(289, R.limit_latency())
        for hold, d in (('lookback', 289), ('two_sided', 3145)):
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
            # the gain's own movement between the detector's 4x points can lift a peak by a few tenths of a mdB:
            # master.py trims that offline (its trim, about -0.003 dB); live it stays
            self.assertLessEqual(D.true_peak_db(y), -1.02 + 0.002, hold)

    def test_lookback_hold_keeps_a_driven_tone_clean(self):
        """master.py's transparency case on the live master's last stages: saws in the bass register driven 3 and
        6 dB into the clip and the look-back limiter read nothing between their harmonics (qa's alias check, -60 dB).
        A low note's 4x peak drifts from cycle to cycle; the hold's 120 ms back window keeps the lowest one."""
        from tools.sound import mixmeter as MM
        from tools.sound import qa as Q
        for f0 in (55.0, 110.0, 220.0):
            n = int(1.2 * SR)
            f = round(f0 * n / SR) * SR / n
            x = D.st(0.5 * D.additive_saw(f, n))
            for drive in (3.0, 6.0):
                with self.subTest(f0=f0, drive=drive):
                    u = np.tile(x * D.undb(-1.02 + drive - MM.true_peak_db(x)), (3, 1))
                    y = R.limit(R.soft_clip(u, -0.52, 1.5), -1.02, hold='lookback')[n:2 * n]
                    got = {c.name: c for c in Q.check_take(y, Q.Spec('lead', f0=f, oneshot=False))}['alias']
                    self.assertIs(got.ok, True, f'{got.value} dB')
                    self.assertLessEqual(MM.true_peak_db(y), -1.02 + 0.002)     # a loop: read round its seam

    def test_sliding_min(self):
        a = np.random.default_rng(13).uniform(0, 1, 500)
        for before, after in ((0, 0), (3, 7), (7, 3), (10, 0), (0, 10), (4, 4)):
            got = R.sliding_min(a, before, after)
            pad = np.concatenate([np.ones(before), a, np.ones(after)])
            want = np.array([pad[k:k + before + after + 1].min() for k in range(len(a))])
            self.assertTrue(np.array_equal(got, want), (before, after))


def synthetic_mix():
    """A small mix manifest with every insert type, in the contract's layout (plugin_mix's constructors)."""
    from tools.sound import plugin_mix as PM
    W = PM.when
    chans = [
        PM.channel('kick', 'kick', 'drums', [PM.gain(-1.0), PM.clip(-6.0), PM.peak(120, 2.0, 1.0)]),
        PM.channel('bass', 'bass', 'none', [
            PM.cc_filter('lowpass', 300, 3000, 0.9, 2.0, 64, 0, (200, 0)),
            PM.duck_onset(('kick',), 6.0, 0.002, 0.03, 0.06), PM.width(0.6), PM.mono_below(150, 8),
            PM.highpass(250, 8, 0.0, 2, (150, 0), W(ab='bass', side='a')), PM.drive(2.0, 0.1, -1.0)]),
        PM.channel('vox', 'vox', 'music', [PM.comp(-20.0, 4.0, 6.0, 0.005, 0.08, 0.5, 2.0), PM.pan(-0.2),
                                           PM.low_shelf(200, -2.0, 0.7), PM.high_shelf(8000, 1.5, 0.7),
                                           PM.lowpass(9000, 2, 0.8)], (0.4, 0.3, 0.2)),
        PM.channel('pad', 'chords', 'music', [
            PM.duck_level('vox', 6.0, 18.0, -10.0, 0.01, 0.3, (1200, 6500)),
            PM.width(2.0, 3000, 4), PM.duck_onset(('snare',), -3.0, 0.0, 0.012, 0.013, 100, 0.0, (1000, 5000), False),
            PM.gain(-3.0, W(variant='novox'))], (0.5, 0.0, 0.1)),
        PM.channel('clap', 'snare', 'drums', [PM.gain(-2.0)], (0.2, 0.0, 0.0)),
    ]
    import numpy as np
    r = np.random.default_rng(1)
    space = {'delays_frames': [515, 583, 682, 799, 885, 1029, 1167, 1361], 'signs': [-1, -1, -1, 1, 1, -1, -1, -1],
             't60_ms': 1100, 't60_low_ms': 400, 'low_hz': 500, 'damp_hz': 9000, 'predelay_us': 27985,
             'send_hp_hz': 500, 'send_order': 4, 'shimmer_pm': 30, 'shimmer_lo_hz': 300, 'shimmer_hi_hz': 7000,
             'window_frames': 2048, 'mono_hz': 150, 'return_mdb': 3000,
             'inserts': [PM.duck_level('vox', 6.0, 18.0, -12.0, 0.01, 0.3), PM.duck_onset(('kick',), 3.0, 0.002, 0.01,
                                                                                          0.18)]}
    echo = {'steps': 3, 'feedback_pm': 380, 'lp_hz': 2600, 'hp_hz': 600, 'drive_pm': 1500, 'wow_ppm': 400,
            'wow_mhz': 500, 'flutter_ppm': 150, 'flutter_mhz': 4700, 'phases_mrad': [3054, 183], 'return_mdb': 3000,
            'inserts': [PM.peak(900, -3.5, 0.8)]}
    res = {'tones_mhz': [739989, 1108731], 'tau_ms': 400, 'send_hp_hz': 600, 'return_mdb': -21000, 'inserts': []}
    m = {'kit': 'TST00', 'content_version': 1, 'channels': chans,
         'groups': {'drums': {'inserts': [PM.parallel(-8.0, [PM.highpass(250, 4), PM.drive(2.5, 0.0, 0.0)])]},
                    'music': {'inserts': [PM.peak(900, -3.5, 0.8)]}},
         'returns': {'space': space, 'echo': echo, 'resonator': res},
         'bus_eq': {'vox': [PM.peak(16000, -1.5, 0.8), PM.highpass(20, 4)], 'novox': [PM.highpass(20, 4)]},
         'master': {'lowcut_hz': 25, 'lowcut_order': 4, 'mono_below_hz': 150,
                    'glue': {'ratio_pm': 2000, 'knee_mdb': 6000, 'attack_us': 30000, 'release_fast_us': 150000,
                             'release_slow_us': 1500000},
                    'vox': {'glue_threshold_mdb': -14000, 'drive_mdb': 4000},
                    'novox': {'glue_threshold_mdb': -14000, 'drive_mdb': 4000},
                    'clip': {'knee_mdb': 1500, 'ceiling_mdb': -520},
                    'limiter': {'ceiling_mdb': -1020, 'lookahead_us': 5000, 'hold_us': 60000, 'release_us': 80000},
                    'dc_hz': 5},
         'reference': {v: {'pre_master_lufs_mlu': -12000, 'club_lufs_mlu': -9000,
                           'channel_true_peak_mdb': [-1000] * len(chans)} for v in ('vox', 'novox')}}
    L = 9600
    t = np.arange(L)
    dry = {'kick': np.exp(-t / 2000.0)[:, None] * np.sin(2 * np.pi * 55 * t / SR)[:, None] * np.ones((1, 2)),
           'bass': 0.4 * np.stack([np.sin(2 * np.pi * 110 * t / SR), np.sin(2 * np.pi * 111 * t / SR)], 1),
           'vox': r.normal(0, 0.1, (L, 2)) * (t > L // 3)[:, None], 'pad': r.normal(0, 0.05, (L, 2)),
           'clap': r.normal(0, 0.3, (L, 2)) * np.exp(-((t - 2400) % L) / 800.0)[:, None]}
    keys = (('kick', 0, 127), ('kick', 4800, 100), ('snare', 2400, 127), ('snare', 7200, 40))
    ccs = {'bass': {74: [(0, 0), (120, 40), (240, 90), (3840, 127), (7680, 20)], 71: [(0, 0), (7680, 127)]}}
    throws = {'vox': [(dry['vox'] * (t > 2 * L // 3)[:, None], 127, 64)]}
    return m, dry, keys, ccs, throws, L


class RenderMix(unittest.TestCase):
    """engine_ref.render_mix on a synthetic mix with every insert type: valid, causal at its declared latency."""

    def test_synthetic_mix_is_valid(self):
        from tools.sound import plugin_mix as PM
        m = synthetic_mix()[0]
        PM.validate(m)
        self.assertLessEqual(max(PM.path_latencies(m).values()), PM.MAX_PATH_LATENCY)

    def test_causal_at_its_latency(self):
        from tools.sound import engine_ref as E
        m, dry, keys, ccs, throws, L = synthetic_mix()
        kw = dict(variant='vox', loop_len=L, bpm=134.0, keys=keys, ccs=ccs, throws=throws, ab={'bass': 'a'},
                  passes=3)
        for master in ('off', 'lookback'):
            y, meta = E.render_mix(m, dry, master=master, **kw)
            d = meta['latency']['total']
            dry2 = {k: np.tile(v, (3, 1)) for k, v in dry.items()}        # whole streams: change one pass only
            P = L + 4000
            for k in ('pad', 'vox', 'kick'):
                dry2[k][P:P + 100] += 0.5
            y2, _ = E.render_mix(m, dry2, master=master, **kw)
            diff = np.flatnonzero(np.any(np.abs(y2 - y) > 1e-12, axis=1))
            first = int(diff[0]) if len(diff) else None
            self.assertIsNotNone(first, master)
            self.assertGreaterEqual(first, P - d, f'{master}: a change at {P} moved the output at {first}, more than '
                                                  f'its latency {d} ahead')
            self.assertEqual(54 if master == 'off' else 54 + 23 + 48 + 289, d)

    def test_variants_and_switches(self):
        from tools.sound import engine_ref as E
        m, dry, keys, ccs, throws, L = synthetic_mix()
        kw = dict(loop_len=L, bpm=134.0, keys=keys, ccs=ccs, throws=throws, passes=2, master='off')
        a, _ = E.render_mix(m, dry, variant='vox', ab={'bass': 'a'}, **kw)
        b, _ = E.render_mix(m, dry, variant='vox', ab={'bass': 'b'}, **kw)
        c, _ = E.render_mix(m, dry, variant='novox', ab={'bass': 'b'}, **kw)
        self.assertGreater(float(np.max(np.abs(a - b))), 1e-3)
        self.assertGreater(float(np.max(np.abs(b - c))), 1e-3)

    def test_space_return_is_sends_space(self):
        """engine_ref.space from the manifest's delays and signs is sends.space with the kit's seed, sample for
        sample."""
        from tools.sound import engine_ref as E
        from tools.sound import plugin_mix as PM
        from tools.sound.kits import fut01 as K
        from tools.sound.recipes.fut01 import context4 as C4
        x = np.random.default_rng(5).normal(0, 0.1, 30000)
        want = S.space(x, K.space_rng('send r4'), **C4.PLATE)
        cfg = PM.space_return(K, 'send r4', C4.PLATE, [])
        got = E.space(x, cfg)
        self.assertLess(float(np.max(np.abs(got - want))), 1e-12)


class Dc(unittest.TestCase):
    def test_causal_and_flat_over_40_hz(self):
        f, h = _response(lambda u: R.dc_block(u, 5.0))
        band = (f >= 40) & (f <= 20000)
        self.assertLess(float(np.max(np.abs(20 * np.log10(np.abs(h[band]))))), 0.07)
        x = np.zeros(1000)
        x[500] = 1.0
        self.assertFalse(np.any(R.dc_block(x)[:500]))


def _heavy():
    from tools.sound import engine_ref as E
    return all(E.r5_dir(k) is not None for k in ('FUT01', 'FUT02', 'FUT03', 'FUT04'))


@unittest.skipUnless(_heavy(), 'the round 5 renders are not here (set WUBANSIA_R5_BUILD to the folder holding '
                                '<kit>-r5/)')
class Gates(unittest.TestCase):
    """engine_ref with the exact master against each kit's committed Wet and Club loops, both variants, second pass:
    LUFS-I +-0.3 LU, short-term +-0.5 LU, 1/3-octave bands 40 Hz-16 kHz +-0.75 dB, true peak +-0.3 dB, kick crest
    +-0.5 dB; and the look-back master's difference from the exact one (expected 0.2 LU or less)."""

    def test_wet_and_club_loops(self):
        from tools.sound import engine_ref as E
        for kit in ('FUT01', 'FUT02', 'FUT03', 'FUT04'):
            for v in ('vox', 'novox'):
                r = E.gates(kit, v, 'exact')
                for which in ('wet', 'club'):
                    with self.subTest(kit=kit, variant=v, loop=which):
                        self.assertTrue(all(r[which]['pass'].values()), f"{r[which]['diff']}")

    def test_lookback_master_difference(self):
        from tools.sound import engine_ref as E
        for kit in ('FUT01', 'FUT02', 'FUT03', 'FUT04'):
            for v in ('vox', 'novox'):
                d = E.master_difference(kit, v)
                with self.subTest(kit=kit, variant=v):
                    self.assertLessEqual(abs(d['diff']['lufs_i']), 0.2, d['diff'])
                    self.assertLessEqual(d['lookback']['true_peak'], -1.02 + 0.05)


if __name__ == '__main__':
    unittest.main()
