"""The helpers every kit's recipes share: parts.py (loop and drum pieces, the tuned string), phrase.py (the grid on the
kit's one swing, placing hits, two passes, the ducker, a kit's files as written) and sampler.py (the stand-ins of the
engine's sampler voice and lane filter); and no kit's setting hides in a shared default."""
import inspect
import types
import unittest

import numpy as np
from scipy import signal

from tools.sound import dsp48 as D
from tools.sound import parts as P
from tools.sound import phrase as Ph
from tools.sound import qa as Q
from tools.sound import sampler as S
from tools.sound import vocals as V
from tools.sound.item import Out
from tools.sound.kits import fut01

SR = D.SR


def kit(bpm=130.0, swing=0.55):
    return types.SimpleNamespace(KIT='TEST', BPM=bpm, STEP=15.0 / bpm, SWING=swing)


class TestGrid(unittest.TestCase):
    def test_one_swing_for_every_lane(self):
        g = Ph.Grid(kit(130.0, 0.55), 64)
        s16 = 15.0 / 130.0 * SR
        self.assertEqual(int(round(64 * s16)), g.length)
        self.assertEqual(int(np.floor(4 * g.s16 + 0.5)), g.at(4))                  # even 16ths on the grid
        self.assertAlmostEqual(g.at(5) - g.at(4), 1.1 * g.s16, delta=1.0)        # odd ones 0.1 of a 16th late
        self.assertEqual(g.at(0), g.at(64))                                       # it wraps
        straight = Ph.Grid(kit(130.0, 0.5), 64)
        self.assertAlmostEqual(straight.at(5) - straight.at(4), straight.s16, delta=1.0)

    def test_fut01_context_grid(self):
        """FUT01's context loop: 4 bars twice the 2-bar phrases, so the pluck phrase tiles exactly."""
        g = Ph.Grid(fut01, 64, 2 * Ph.loop_len(fut01, 32))
        self.assertEqual(343880, g.length)
        self.assertEqual(g.length / 64, g.s16)

    def test_place_wraps_the_tail(self):
        buf = np.zeros((100, 2))
        Ph.place(buf, np.ones(30), 90)
        self.assertEqual(30.0, buf[:, 0].sum())
        self.assertEqual(20.0, buf[:20, 1].sum())

    def test_two_passes_join_at_the_seam(self):
        L = SR
        x = D.rng('test', 'two_pass').standard_normal((L, 2)) * 0.1
        y = Ph.two_pass(lambda u: D.bfilt(u, 2000.0, 'lowpass', order=2), x, 0.02)
        self.assertEqual((L, 2), y.shape)
        self.assertLess(Q.seam_db(y), Q.SEAM_MAX_DB)
        circ = Ph.circular(lambda u: D.bfilt(u, 2000.0, 'lowpass', order=2), x)
        self.assertLess(Q.seam_db(circ), Q.SEAM_MAX_DB)

    def test_pump(self):
        L = SR
        g = Ph.pump_db([0, SR // 2], L, depth_db=10.0, ramp=0.002, hold=0.02, release=0.18)
        self.assertEqual(-10.0, g[int(0.01 * SR)])
        self.assertEqual(0.0, g[int(0.3 * SR)])
        self.assertEqual(-10.0, g[SR // 2 + int(0.01 * SR)])

    def test_kit_files_are_the_files_as_written(self):
        from tools.sound import render as R
        item, got, gain = Ph.kit_files(fut01, 'Ping')
        done = R.render_item(item, 'FUT01')
        self.assertEqual(done.gain_db, gain)
        self.assertEqual(4, len(got))
        for t, o in zip(done.takes, done.outs):
            ours = got[(t.note, t.layer, t.rr)].audio
            self.assertEqual(R.encode(o.audio), R.encode(ours[:, 0]))       # the same 24-bit samples

    def test_hits_lane_picks_the_nearest_layer(self):
        g = Ph.Grid(fut01, 64, 2 * Ph.loop_len(fut01, 32))
        lane = Ph.hits_lane(fut01, 'Ping', ((0, 1.0), (32, 0.4)), g)                 # 3.6 s apart: no overlap
        _, got, _ = Ph.kit_files(fut01, 'Ping')
        first, second = got[(None, 2, 1)].audio, got[(None, 1, 2)].audio              # v2; then v1 (0.5), rr2
        self.assertTrue(np.array_equal(lane[:len(first)], first))
        a = g.at(32)
        self.assertTrue(np.array_equal(lane[a:a + len(second)], second))


class TestParts(unittest.TestCase):
    def test_loop_grid_holds_whole_cycles(self):
        L, grid, f = P.loop_grid(46.25)
        self.assertAlmostEqual(f * L / SR, round(f * L / SR), places=9)
        self.assertAlmostEqual(grid, SR / L)
        self.assertLess(abs(D.cents(f, 46.25)), 1.0)

    def test_close_loop_joins(self):
        L, _, f = P.loop_grid(55.0)
        n = int(0.3 * SR) + 2 * L
        y = D.st(D.additive_saw(f, n, 0.3) * P.raised(n, 0.05))
        out, (a, b) = P.close_loop(y, int(0.3 * SR), L)
        self.assertEqual(b - a, L)
        self.assertEqual(b, len(out))
        self.assertLess(Q.seam_db(out, a, b), Q.SEAM_MAX_DB)

    def test_hp_steep(self):
        """About 96 dB per octave: an octave under the cutoff the band is 90 dB down, two octaves up it is flat."""
        x = D.rng('test', 'hp').standard_normal(4 * SR)
        y = P.hp_steep(x, 450.0)
        f, X = signal.welch(x, SR, nperseg=2 ** 14)
        _, Y = signal.welch(y, SR, nperseg=2 ** 14)
        band = lambda P_, lo, hi: P_[(f >= lo) & (f < hi)].sum()
        self.assertLess(10 * np.log10(band(Y, 180, 225) / band(X, 180, 225)), -90)
        self.assertGreater(10 * np.log10(band(Y, 2000, 4000) / band(X, 2000, 4000)), -0.1)

    def test_ks_tuned_sounds_on_pitch(self):
        for f0 in (261.63, 1046.5, 2093.0):
            for S_ in (0.15, 0.45):
                self.assertLess(abs(D.cents(P.ks_pitch(P.ks_tuned(f0, S_, -0.3), S_, -0.3), f0)), 0.01)

    def test_frac_delay(self):
        x = np.zeros(100)
        x[10] = 1.0
        y = P.frac_delay(x, 2.25)
        self.assertAlmostEqual(0.75, y[12])
        self.assertAlmostEqual(0.25, y[13])

    def test_sweep_ends_on_its_note(self):
        n = int(0.4 * SR)
        ph = P.sweep(n, 600.0, 0.006, 0.016, 0.28, 46.25)
        self.assertAlmostEqual(np.diff(ph)[-1] * SR / (2 * np.pi), 46.25, delta=0.01)


class TestSampler(unittest.TestCase):
    def test_a_held_note_loops_then_releases(self):
        L = 1000
        y = np.sin(2 * np.pi * np.arange(3 * L) / L)
        out = Out(D.st(y), loop=(L, 2 * L))
        v = S.sampler_voice(out, 5 * L, attack=0.0, release=0.02)
        self.assertEqual(5 * L + int(0.02 * SR), len(v))
        self.assertTrue(np.allclose(v[3 * L:4 * L, 0], y[L:2 * L]))
        self.assertLess(abs(v[-1, 0]), 1e-5)                                   # released to silence

    def test_cc_curve_joins_at_the_seam(self):
        L = Ph.loop_len(fut01, 32)
        c = S.cc_curve(((0, 0), (16, 127), (32, 0)), L, fut01.STEP)
        self.assertEqual(L, len(c))
        self.assertLess(abs(c[0] - c[-1]), 1.0)
        self.assertAlmostEqual(127.0, c.max(), delta=0.5)

    def test_lane_filter_keeps_the_sub_and_its_mid_cut_is_a_choice(self):
        n = SR
        t = np.arange(n) / SR
        x = D.st(0.3 * np.sin(2 * np.pi * 46.25 * t) + 0.3 * D.additive_saw(46.25, n))
        cc = np.full(n, 90.0)
        sub = D.lr4(x, 120.0, 'lowpass')                                            # the branch under 120 Hz
        clean = S.lane_filter(x, cc, cc, mids_hp=450.0) - sub
        warm = S.lane_filter(x, cc, cc, mids_hp=None) - sub
        level = lambda y, lo, hi: 10 * np.log10(np.mean(Q.band_zp(y[SR // 4:], lo, hi) ** 2) + 1e-30)
        self.assertGreater(level(warm, 200, 300) - level(clean, 200, 300), 30.0)   # the mids' own low end, kept



class TestNoKitDefaults(unittest.TestCase):
    def test_a_kit_setting_is_never_a_default(self):
        """Shared code takes a kit's tempo, groove and swing, and the bass lane's mid cut, from its caller: a forgotten
        argument is an error, not FUT01's setting."""
        for fn, arg in ((Q.two_step_loop, 'bpm'), (Q.two_step_loop, 'hits'), (Q.check_loop, 'bpm'),
                        (Q.check_loop, 'hits'), (V.sing_track, 'swing'), (S.lane_filter, 'mids_hp')):
            with self.subTest(fn=fn.__name__, arg=arg):
                self.assertIs(inspect.Parameter.empty, inspect.signature(fn).parameters[arg].default)


if __name__ == '__main__':
    unittest.main()
