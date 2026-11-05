"""compare_render.py (P12): its measures, gates, render plan, files and report, on synthetic signals, so they run
everywhere. The fidelity report itself (the real kits against the round 5 loops) needs groovecore render's renders
and the reference folders: the last test runs it when WUBANSIA_RENDERS (a renders folder) and the round 5 folders
are there, and skips otherwise.
"""
import json
import math
import os
import tempfile
import unittest
from pathlib import Path

import numpy as np

from tools.sound import compare_render as C

SR = C.SR


def _noise(n, seed=1, level=0.3):
    return np.random.default_rng(seed).normal(0, level, (n, 2))


def _loop(n, seed=1):
    """Decaying noise bursts on each quarter of the loop over a low sine: something with kicks and bands."""
    r = np.random.default_rng(seed)
    t = np.arange(n)
    x = 0.2 * np.sin(2 * np.pi * 55.0 * t / SR)[:, None] * np.ones((1, 2))
    for k in range(4):
        p = k * n // 4
        m = min(n - p, 4800)
        x[p:p + m] += r.normal(0, 0.5, (m, 2)) * np.exp(-np.arange(m) / 900.0)[:, None]
    return x * (0.8 / np.abs(x).max())


class Nulls(unittest.TestCase):
    def test_null_is_the_residuals_energy_against_the_references(self):
        x = _noise(4800)
        self.assertEqual(-math.inf, C.null_db(x, x))
        self.assertAlmostEqual(-40.0, C.null_db(0.99 * x, x), places=6)
        self.assertAlmostEqual(-20.0, C.null_db(1.1 * x, x), places=6)
        self.assertEqual(math.inf, C.null_db(x, np.zeros_like(x)))

    def test_a_pass_is_cut_from_the_loop_length_and_padded_past_the_end(self):
        x = np.arange(10.0)[:, None] * np.ones((1, 2))
        np.testing.assert_array_equal([4, 5, 6, 7], C.pass_slice(x, 4, 1)[:, 0])
        np.testing.assert_array_equal([5, 6, 7, 8], C.pass_slice(x, 4, 1, shift=1)[:, 0])
        np.testing.assert_array_equal([6, 7], C.pass_slice(x, 4, 1, start=2)[:, 0])
        np.testing.assert_array_equal([8, 9, 0, 0], C.pass_slice(x, 4, 2)[:, 0])
        np.testing.assert_array_equal([0, 0, 1], C.pass_slice(x, 4, 0, shift=-1)[:3, 0])

    def test_each_pass_nulls_at_its_own_best_shift(self):
        """The engine's passes start up to a sample off the recipe's tiled loop: each pass is aligned on its own."""
        L = 9600
        want = _noise(L, 2)
        x = np.concatenate([want, np.zeros((1, 2)), want, np.zeros((2, 2)), want])    # pass 2 one sample late
        n0, s0 = C.best_null(x, want, L, 0)
        n1, s1 = C.best_null(x, want, L, 1)
        self.assertEqual((-math.inf, 0), (n0, s0))
        self.assertEqual((-math.inf, 1), (n1, s1))
        n2, s2 = C.best_null(x, want, L, 2)
        self.assertEqual(3, s2)
        n, s = C.best_null(np.concatenate([np.zeros((5, 2)), want]), want, L, 0, max_shift=3)
        self.assertGreater(n, -10.0, 'five samples late is beyond the search')

    def test_the_first_pass_from_the_chase(self):
        """A start chases a phrase's slices in over 5 ms: the first pass is judged from there."""
        L = 9600
        want = _noise(L, 3)
        got = want.copy()
        got[:240] *= np.linspace(0, 1, 240)[:, None]
        self.assertGreater(C.best_null(got, want, L, 0)[0], -40.0)
        self.assertEqual(-math.inf, C.best_null(got, want, L, 0, start=int(round(C.CHASE_S * SR)))[0])


class Bands(unittest.TestCase):
    def test_equal_lanes_differ_nowhere(self):
        x = _loop(48000)
        worst, judged = C.band_diff(x, x)
        self.assertGreater(judged, 10)
        self.assertAlmostEqual(0.0, worst[1], places=9)

    def test_a_level_change_shows_in_every_band(self):
        x = _loop(48000)
        worst, _ = C.band_diff(x * 10 ** (1.5 / 20), x)
        self.assertAlmostEqual(1.5, worst[1], places=6)

    def test_bands_far_under_the_loudest_are_not_judged(self):
        """A lane's empty bands would read large differences from tiny errors: only bands within 40 dB of the
        loudest count."""
        t = np.arange(48000)
        tone = 0.5 * np.sin(2 * np.pi * 1000.0 * t / SR)
        hiss = 1e-4 * np.random.default_rng(4).normal(0, 1, len(t))
        want = np.stack([tone + hiss, tone + hiss], 1)
        got = np.stack([tone + 3 * hiss, tone + 3 * hiss], 1)    # the hiss 9.5 dB up, 70 dB under the tone
        worst, judged = C.band_diff(got, want)
        self.assertLess(abs(worst[1]), 0.1)
        self.assertLess(judged, 5)


class Clicks(unittest.TestCase):
    def test_a_step_counts_as_a_click_and_a_faded_join_does_not(self):
        t = np.arange(48000)
        ref = 0.5 * np.sin(2 * np.pi * 220.0 * t / SR)
        cut = ref.copy()
        cut[24000:] = 0.0                                       # stopped dead mid-cycle
        faded = ref.copy()
        faded[24000:24480] *= 0.5 + 0.5 * np.cos(np.pi * np.arange(480) / 480)
        faded[24480:] = 0.0
        x = lambda u: np.stack([u, u], 1)  # noqa: E731
        self.assertEqual(0, C.clicks(x(ref), x(ref)))
        self.assertGreater(C.clicks(x(cut), x(ref)), 0)
        self.assertEqual(0, C.clicks(x(faded), x(ref)))


class Gates(unittest.TestCase):
    L = 4 * 24000
    KICKS = [0, 24000, 48000, 72000]

    def test_a_render_equal_to_its_reference_passes_every_gate(self):
        x = _loop(self.L)
        r = C.compare(x, x, self.KICKS)
        self.assertTrue(all(r['pass'].values()), r)
        self.assertAlmostEqual(0.0, r['diff']['lufs_i'], places=9)

    def test_a_level_off_by_more_than_the_tolerance_fails_loudness(self):
        x = _loop(self.L)
        r = C.compare(x * 10 ** (0.4 / 20), x, self.KICKS)
        self.assertFalse(r['pass']['lufs_i'])
        self.assertTrue(r['pass']['kick_crest'], 'a gain leaves the crest as it is')
        self.assertAlmostEqual(0.4, r['diff']['lufs_i'], places=3)
        self.assertAlmostEqual(0.4, r['diff']['true_peak'], places=3)
        self.assertTrue(r['pass']['true_peak'], 'true peak takes 0.5 dB in hop 3')

    def test_a_band_off_fails_the_band_gate(self):
        from scipy import signal
        x = _loop(self.L)
        sos = signal.butter(2, (2000.0, 3000.0), 'bandpass', fs=SR, output='sos')
        y = x + 0.5 * signal.sosfiltfilt(sos, x, axis=0)
        r = C.compare(y, x, self.KICKS)
        self.assertFalse(r['pass']['bands'])
        self.assertTrue(1500 < r['diff']['bands'][0] < 4000, r['diff']['bands'])

    def test_a_gate_check_names_what_fails_and_who_likely_owns_it(self):
        x = _loop(self.L)
        c = C.gate_check('FUT09', 'mix', 'vox: club', x * 10 ** (1 / 20), x, self.KICKS)
        self.assertFalse(c.ok)
        self.assertIn('lufs_i', c.note)
        self.assertIn('P8', c.note)
        self.assertTrue(C.gate_check('FUT09', 'mix', 'vox: club', x, x, self.KICKS).ok)


class Lanes(unittest.TestCase):
    L = 24000

    def _tap(self, lane, passes=3):
        return np.tile(lane, (passes, 1))

    def test_a_phrase_lane_equal_to_the_recipes_passes(self):
        want = _noise(self.L, 5)
        c = C.lane_check('FUT09', 'vox', 'b', 'vox', self._tap(want), want, self.L, phrase=True)
        self.assertTrue(c.ok, c.values)
        self.assertEqual(-math.inf, c.values['null_pass2_db'])

    def test_a_phrase_lane_nulling_short_of_40_db_fails(self):
        want = _noise(self.L, 6)
        got = self._tap(want) + _noise(3 * self.L, 7, level=0.3 * 10 ** (-30 / 20))
        c = C.lane_check('FUT09', 'vox', 'b', 'vox', got, want, self.L, phrase=True)
        self.assertFalse(c.ok)
        self.assertAlmostEqual(-30.0, c.values['null_pass2_db'], delta=0.5)
        self.assertIn('P6', c.note)

    def test_a_sampled_lane_within_a_db_per_band_passes_and_beyond_fails(self):
        want = _loop(self.L, 8)
        self.assertTrue(C.lane_check('FUT09', 'vox', 'b', 'hat', self._tap(want) * 10 ** (0.5 / 20), want, self.L, False).ok)
        c = C.lane_check('FUT09', 'vox', 'b', 'hat', self._tap(want) * 10 ** (1.5 / 20), want, self.L, False)
        self.assertFalse(c.ok)
        self.assertIn('P5', c.note)

    def test_sound_where_the_recipe_has_none_and_silence_where_it_has_some_fail(self):
        x = _noise(self.L, 9)
        c = C.lane_check('FUT09', 'vox', 'b', 'fx', self._tap(x), None, self.L, False)
        self.assertFalse(c.ok)
        self.assertIn('plays nothing here', c.note)
        c = C.lane_check('FUT09', 'vox', 'b', 'fx', np.zeros((3 * self.L, 2)), x, self.L, False)
        self.assertFalse(c.ok)
        self.assertIn('the engine plays nothing', c.note)
        self.assertIsNone(C.lane_check('FUT09', 'vox', 'b', 'fx', np.zeros((3 * self.L, 2)), None, self.L, False))

    def test_the_a_side_where_the_trace_has_one(self):
        class Cap:
            dry = {'bass': np.ones((4, 2)), 'kick': np.full((4, 2), 2.0)}
            sides = {('bass', 'a'): np.full((4, 2), 3.0)}
        self.assertEqual(3.0, C.lane_reference(Cap, 'bass', 'a')[0, 0])
        self.assertEqual(1.0, C.lane_reference(Cap, 'bass', 'b')[0, 0])
        self.assertEqual(2.0, C.lane_reference(Cap, 'kick', 'a')[0, 0])
        self.assertIsNone(C.lane_reference(Cap, 'vox', 'b'))


class Plan(unittest.TestCase):
    def test_the_loop_renders_cover_every_comparison(self):
        names = [r.name for r in C.plan_loops()]
        self.assertEqual(len(names), len(set(names)))
        for v in C.VARIANTS:
            self.assertIn(f'loop_{v}_bassb', names)                 # the Club loop, and its taps
            self.assertIn(f'loop_{v}_bassb_premaster', names)       # the Wet loop
            self.assertIn(f'loop_{v}_bassa_dry', names)             # the dry loops
            self.assertIn(f'loop_{v}_bassb_dry', names)
        taps = {r.name for r in C.plan_loops() if r.taps}
        self.assertEqual({f'loop_{v}_bass{b}' for v in C.VARIANTS for b in 'ab'}, taps)
        pre = next(r for r in C.plan_loops() if r.name == 'loop_vox_bassb_premaster')
        self.assertEqual(('--club', 'off'), pre.args[-2:])
        dry = next(r for r in C.plan_loops() if r.name == 'loop_novox_bassa_dry')
        for opt in (('--space', '0'), ('--echo', '0'), ('--club', 'off'), ('--bass', 'a'), ('--variant', 'novox')):
            i = dry.args.index(opt[0])
            self.assertEqual(opt[1], dry.args[i + 1])

    def test_the_exact_master_and_the_tempos(self):
        self.assertEqual(('--variant', 'vox', '--bass', 'b', '--passes', '3', '--master', 'exact'), C.exact_render('vox').args)
        r = C.tempo_render(134.0, -3)
        self.assertEqual('loop_vox_bassb_tempo-3', r.name)
        self.assertEqual('129.980000', r.args[r.args.index('--tempo') + 1])
        self.assertTrue(r.taps)
        self.assertEqual('loop_vox_bassb_tempo+1', C.tempo_render(134.0, 1).name)

    def test_a_section_render_is_8_bars_of_that_section(self):
        r = C.section_render('novox', 'a', 3)
        self.assertEqual('song_novox_bassa_s03', r.name)
        self.assertEqual(('--mode', 'song', '--variant', 'novox', '--bass', 'a', '--section', '3', '--bars', '8'), r.args)


class Files(unittest.TestCase):
    def test_a_render_and_its_sidecar_read_back(self):
        import soundfile as sf
        with tempfile.TemporaryDirectory() as d:
            p = Path(d) / 'fut09' / 'loop_vox_bassb.wav'
            p.parent.mkdir()
            x = _noise(1000, 10)
            sf.write(str(p), x.astype(np.float32), SR, subtype='FLOAT')
            p.with_suffix('.json').write_text(json.dumps({'kit': 'FUT09', 'latency': 424}))
            got, meta = C.load(C.wav_path(d, 'FUT09', 'loop_vox_bassb'))
            np.testing.assert_array_equal(x.astype(np.float32).astype(np.float64), got)
            self.assertEqual(424, meta['latency'])
            self.assertIsNone(C.load(C.wav_path(d, 'FUT09', 'nothing')))

    def test_the_report_lists_every_check_and_the_failures_with_their_cause(self):
        checks = [C.Check('FUT09', 'mix', 'vox: club vs X', {'lufs_i': 0.1, 'lufs_s': 0.2, 'bands': (1000.0, 0.3),
                                                               'true_peak': 0.1, 'kick_crest': 0.2}, True),
                  C.Check('FUT09', 'lane', 'vox bass B: vox (phrase)', {'null_pass1_db': -35.0, 'shift_pass1': 0,
                                                                         'null_pass2_db': -36.0, 'shift_pass2': 1,
                                                                         'worst_band': (500.0, 0.2), 'level_db': 0.0},
                          False, 'nulls -35.0 / -36.0 dB (gate -40): likely P6'),
                  C.Check('FUT09', 'cpu', 'loop_vox_bassb', {'cpu_percent': 3.2, 'max_voices': 20, 'worst_block_percent': 9.0})]
        with tempfile.TemporaryDirectory() as d:
            path = C.write_report(checks, ['FUT09'], d)
            text = path.read_text()
            self.assertIn('## FUT09', text)
            self.assertIn('**1 failing** of 2 gated checks', text)
            self.assertIn('| mix: vox: club vs X | +0.10 | +0.20 | +0.30 @ 1000 Hz | +0.10 | +0.20 | pass |', text)
            self.assertIn('-35.0 (+0)', text)
            self.assertIn('**FAIL**', text)
            self.assertIn('- lane: vox bass B: vox (phrase): nulls -35.0 / -36.0 dB (gate -40): likely P6', text)
            data = json.loads((Path(d) / 'report.json').read_text())
            self.assertEqual(3, len(data))
            self.assertEqual([1000.0, 0.3], data[0]['values']['bands'])


@unittest.skipUnless(os.environ.get('WUBANSIA_RENDERS'), 'set WUBANSIA_RENDERS to a folder of groovecore renders')
class Report(unittest.TestCase):
    def test_the_fidelity_report_runs_on_the_renders_there(self):
        """On a renders folder (groovecore render, compare_render's plan) with the round 5 folders: every check
        runs, and the kits it covers each get their mix gates."""
        from tools.sound import engine_ref as E
        renders = os.environ['WUBANSIA_RENDERS']
        kits = [k for k in C.KITS if (Path(renders) / k.lower()).is_dir() and E.r5_dir(k)]
        if not kits:
            self.skipTest('no kit has both renders and its round 5 folder')
        checks = []
        for kit in kits:
            checks += C.run_kit(kit, renders, groups=('mix',))
        self.assertEqual(4 * len(kits), len(checks))
