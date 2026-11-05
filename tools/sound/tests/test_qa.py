"""QA flags every defect the prototypes hit and passes every control (plan section 5.2, the mud gate included), and
every limit flips between a fixture just inside it and one just outside."""
import os
import re
import tempfile
import unittest
from pathlib import Path

import numpy as np
import soundfile as sf

from tools.sound import denylist
from tools.sound import dsp48 as D
from tools.sound import mixmeter as MM
from tools.sound import qa as Q
from tools.sound.item import Item
from tools.sound.tests import fixtures as FX
from tools.sound.tests import test_mixmeter as TM

REPO = Path(__file__).resolve().parents[3]
SR = D.SR


def by_name(checks):
    return {c.name: c for c in checks}


class QaCase(unittest.TestCase):
    def assertFlags(self, x, spec, *names):
        got = by_name(Q.check_take(x, spec))
        for name in names:
            self.assertIs(got[name].ok, False, f'{name} should fail: {got[name].value} ({got[name].limit})')

    def assertPasses(self, x, spec, *names):
        got = by_name(Q.check_take(x, spec))
        for name in names:
            self.assertIs(got[name].ok, True, f'{name} should pass: {got[name].value} ({got[name].limit})')

    def assertClean(self, x, spec):
        bad = Q.failed(Q.check_take(x, spec))
        self.assertEqual([], [f'{c.name} = {c.value} ({c.limit}) {c.detail}' for c in bad])


class TestPrototypeDefects(QaCase):
    def test_truncated_tail(self):
        self.assertFlags(FX.truncated_tail(), Q.Spec('perc'), 'tail')
        self.assertClean(FX.full_tail(), Q.Spec('perc'))

    def test_click_at_start(self):
        self.assertFlags(FX.click_start(), Q.Spec('perc'), 'first_sample')
        self.assertClean(FX.soft_start(), Q.Spec('perc'))

    def test_anti_correlated_low_end(self):
        spec = Q.Spec('bass', mud='off', tonal=False)
        self.assertFlags(FX.wide_low(), spec, 'stereo_corr', 'low_side')
        self.assertClean(FX.wide_low_fixed(), spec)

    def test_dc_from_biased_drive(self):
        self.assertFlags(FX.dc_bias(), Q.Spec('perc', mud='off'), 'dc')
        self.assertClean(FX.dc_bias_fixed(), Q.Spec('perc', mud='off'))

    def test_dc_from_drive_on_a_skewed_wave(self):
        self.assertFlags(FX.dc_skew(), Q.Spec('perc', mud='off'), 'dc')
        self.assertClean(FX.dc_skew_fixed(), Q.Spec('perc', mud='off'))

    def test_anti_correlated_freeze_bed(self):
        self.assertFlags(FX.freeze_bed(shared=False), Q.Spec('texture', oneshot=False), 'stereo_corr', 'low_side')
        self.assertClean(FX.freeze_bed(shared=True), Q.Spec('texture', oneshot=False))

    def test_muddy_kick(self):
        spec = Q.Spec('kick', f0=FX.F_KICK)
        self.assertFlags(FX.master(FX.muddy_kick()), spec,
                         'mud_150_400_fall', 'mud_vs_sub_after_80ms', 'mud_share', 'box_400_1k_fall')
        self.assertClean(FX.master(FX.clean_kick()), spec)

    def test_muddy_kick_measures_like_round_0(self):
        # docs/listening.md round 0, kick v1.0: 771 ms (first crossing), -2.4 dB after 80 ms (energy over the whole
        # tail), -6.2 dB share. The gate now takes the worst 20 ms window, which can only read higher than the average
        m = Q.mud(FX.master(FX.muddy_kick()))
        self.assertGreater(m['fall_150_400_ms'], 500)
        self.assertGreaterEqual(m['vs_sub_db'], -2.4)
        self.assertAlmostEqual(m['share_db'], -6.2, delta=1.0)

    def test_mud_on_the_two_step_loop(self):
        muddy, _ = Q.check_loop(FX.shared_layers(FX.muddy_kick, (0.4, 0.7, 1.0)), 134.0, Q.TWO_STEP)
        clean, loop = Q.check_loop(FX.shared_layers(FX.clean_kick, (0.4, 0.7, 1.0)), 134.0, Q.TWO_STEP)
        self.assertEqual([False, False], [c.ok for c in muddy])
        self.assertEqual([True, True], [c.ok for c in clean])
        self.assertEqual(len(loop), round(32 * 60 / 134 / 4 * SR))

    def test_mud_in_the_gaps_of_a_loop_file(self):
        """A loop that declares its kick onsets gets the per-gap mud checks on the file itself."""
        for kick, ok in ((FX.clean_kick, True), (FX.muddy_kick, False)):
            loop, onsets = Q.two_step_loop(FX.shared_layers(kick, (0.4, 0.7, 1.0)), 134.0, Q.TWO_STEP)
            got = by_name(Q.check_take(loop, Q.Spec('loop', oneshot=False, mud='gate', hits=tuple(onsets))))
            self.assertIs(got['loop_mud_vs_sub'].ok, ok, got['loop_mud_vs_sub'].value)
            self.assertIn('loop_mud_before_hit', got)
        plain = by_name(Q.check_take(loop, Q.Spec('loop', oneshot=False, mud='gate')))
        self.assertNotIn('loop_mud_vs_sub', plain)


class TestChecks(QaCase):
    def test_true_peak_is_oversampled(self):
        x = FX.intersample_peak()
        self.assertLess(D.db(np.abs(x).max()), -1.4)          # a sample-peak meter would pass it
        self.assertFlags(x, Q.Spec('hat'), 'true_peak')
        self.assertPasses(D.to_ceiling(x), Q.Spec('hat'), 'true_peak')

    def test_loop_seam(self):
        spec = Q.Spec('pad', oneshot=False, tonal=False)
        self.assertFlags(FX.sine_loop(220.3), spec, 'seam')    # 440.6 periods: the wrap jumps
        self.assertClean(FX.sine_loop(240.0), spec)            # 480 whole periods
        self.assertIsNone(by_name(Q.check_take(FX.sine_loop(240.0), spec))['first_sample'].ok)

    def test_splice_burst(self):
        hard, at = FX.gain_step(crossfade=False)
        soft, _ = FX.gain_step(crossfade=True)
        self.assertFlags(hard, Q.Spec('pad', oneshot=False, splices=at), 'splices')
        self.assertPasses(soft, Q.Spec('pad', oneshot=False, splices=at), 'splices')

    def test_aliasing_on_a_tonal_sustain(self):
        spec = Q.Spec('lead', f0=1760.0, oneshot=False)
        self.assertFlags(FX.naive_saw(1760.0), spec, 'alias')
        self.assertPasses(FX.clean_saw(1760.0), spec, 'alias')
        flagged = Q.Spec('lead', f0=1760.0, oneshot=False, flags=frozenset({'deliberate_alias'}))
        self.assertIsNone(by_name(Q.check_take(FX.naive_saw(1760.0), flagged))['alias'].ok)

    def test_tuning(self):
        for cat in ('bass', 'lead', 'pad', 'stab', 'perc'):              # +-3 cents wherever a pitch is declared
            spec = Q.Spec(cat, f0=110.0, oneshot=False)
            self.assertFlags(FX.tone(110.0 * 2 ** (4 / 1200)), spec, 'tuning')
            self.assertPasses(FX.tone(110.0 * 2 ** (2 / 1200)), spec, 'tuning')
        self.assertIsNone(by_name(Q.check_take(FX.tone(110.0), Q.Spec('pad', oneshot=False)))['tuning'].ok)
        for cat in ('kick', 'tom'):                                       # +-10 cents
            spec = Q.Spec(cat, f0=FX.F_KICK)
            self.assertPasses(FX.master(FX.clean_kick(cents=8)), spec, 'tuning')
            self.assertFlags(FX.master(FX.clean_kick(cents=12)), spec, 'tuning')

    def test_low_cut_for_everything_but_kicks_and_basses(self):
        self.assertFlags(FX.low_pad(highpassed=False), Q.Spec('pad'), 'lowcut_share_below_100hz')
        self.assertPasses(FX.low_pad(highpassed=True), Q.Spec('pad'), 'lowcut_share_below_100hz')
        # a stab on F#2 (92.5 Hz, the sub's second harmonic) has almost nothing under 60 Hz, but its root competes
        self.assertLess(Q.low_share_db(FX.stab(), 60.0), Q.LOWCUT_MAX_DB)
        self.assertFlags(FX.stab(), Q.Spec('stab'), 'lowcut_share_below_100hz')
        self.assertPasses(FX.stab(hp_hz=130.0), Q.Spec('stab'), 'lowcut_share_below_100hz')

    def test_loudness_by_category(self):
        bed = Q.Spec('texture', oneshot=False)
        self.assertFlags(FX.freeze_bed(shared=True, level=-26.0), bed, 'loudness')
        rise = Q.Spec('fx', tags=('riser',))
        self.assertFlags(FX.riser(rising=False), rise, 'rising')
        self.assertPasses(FX.riser(rising=True), rise, 'loudness', 'rising')
        self.assertFlags(FX.riser(rising=True, level=-20.0), rise, 'loudness')

    def test_loudness_targets_and_tolerances(self):
        # just inside and just outside each target: beds -20 +-1, risers -14 +-2, vocals wet -16 / dry -18 +-1
        cases = ((Q.Spec('texture', oneshot=False), -20.0, 1.0, lambda lv: FX.freeze_bed(shared=True, level=lv)),
                 (Q.Spec('fx', tags=('riser',)), -14.0, 2.0, lambda lv: FX.riser(rising=True, level=lv)),
                 (Q.Spec('vox', tags=('wet',)), -16.0, 1.0, lambda lv: FX.freeze_bed(shared=True, level=lv)),
                 (Q.Spec('vox', tags=('dry',)), -18.0, 1.0, lambda lv: FX.freeze_bed(shared=True, level=lv)))
        for spec, target, tol, make in cases:
            for side in (-1, 1):
                self.assertPasses(make(target + side * 0.7 * tol), spec, 'loudness')
                self.assertFlags(make(target + side * 1.3 * tol), spec, 'loudness')

    def test_velocity_layers_and_round_robins(self):
        good = FX.shared_layers(FX.clean_kick, (0.4, 0.7, 1.0))
        rows = [dict(layer=i + 1, rr=1, note=None, x=x) for i, x in enumerate(good.values())]
        self.assertEqual([True, True], [c.ok for c in Q.check_layers(rows)[:2]])
        base = FX.master(FX.clean_kick())
        flat = [dict(layer=i + 1, rr=1, note=None, x=base * D.undb(0.5 * i)) for i in range(3)]
        self.assertEqual([False, False], [c.ok for c in Q.check_layers(flat)[:2]])
        rr = [dict(layer=1, rr=1, note=None, x=base), dict(layer=1, rr=2, note=None, x=base * D.undb(-1.5))]
        self.assertIs(by_name(Q.check_layers(rr))['rr_spread'].ok, False)
        rr[1]['x'] = base * D.undb(-0.2)
        self.assertIs(by_name(Q.check_layers(rr))['rr_spread'].ok, True)

    def test_one_gain_per_instrument(self):
        self.assertTrue(Q.check_gains([-3.0, -3.0, -3.0]).ok)
        self.assertFalse(Q.check_gains([-3.0, -2.0]).ok)

    def test_format_and_mono_storage(self):
        x = FX.full_tail()
        with tempfile.TemporaryDirectory() as tmp:
            bad = os.path.join(tmp, 'a.flac')
            sf.write(bad, x, 44100, subtype='PCM_16', format='FLAC')
            self.assertIs(by_name(Q.check_file(bad, Q.Spec('perc')))['format'].ok, False)
            dup = os.path.join(tmp, 'b.flac')
            sf.write(dup, D.st(x), SR, subtype='PCM_24', format='FLAC')
            self.assertIs(by_name(Q.check_file(dup, Q.Spec('perc')))['mono_storage'].ok, False)
            good = os.path.join(tmp, 'c.flac')
            sf.write(good, x, SR, subtype='PCM_24', format='FLAC')
            got = by_name(Q.check_file(good, Q.Spec('perc')))
            self.assertEqual((True, True), (got['format'].ok, got['mono_storage'].ok))

    def test_results_serialise(self):
        rows = [c.as_dict() for c in Q.check_take(FX.full_tail(), Q.Spec('perc'))]
        self.assertTrue(all(r['status'] in ('pass', 'fail', 'n/a') for r in rows))
        self.assertTrue(all(isinstance(r['value'], (float, int, str, type(None))) for r in rows))


class TestKickCrest(QaCase):
    """Round 1: kick B "sounds a bit hard like its really slamming the limiter". The floor keeps a kick's top layer from
    being flattened onto its body (ADR 0011)."""

    def test_a_kick_slammed_into_a_limiter_fails(self):
        spec = Q.Spec('kick', f0=FX.F_KICK)
        self.assertPasses(FX.master(FX.clean_kick()), spec, 'kick_crest')
        for drive in (6.0, 12.0):
            self.assertFlags(FX.clipped_kick(drive), spec, 'kick_crest')

    def test_a_sine_reads_three_db(self):
        self.assertAlmostEqual(Q.kick_crest_db(FX.tone(1000.0)), 3.01, delta=0.02)     # 50 whole cycles in 50 ms

    def test_the_measure_is_pinned(self):
        """The window and the method, not only the limit: a 25 ms burst fills half the 50 ms window (6.02 dB; a 100 ms
        window would read 9.03), and the fixture kick's top layer reads what the ADR quotes."""
        self.assertAlmostEqual(Q.kick_crest_db(FX.tone_burst()), 6.02, delta=0.02)
        self.assertAlmostEqual(Q.kick_crest_db(FX.master(FX.clean_kick(1.0, 0))), 4.83, delta=0.02)
        self.assertAlmostEqual(Q.kick_crest_db(FX.master(FX.clean_kick(1.0, 1))), 4.39, delta=0.02)

    def test_only_the_top_layer_of_a_kick(self):
        x = FX.clipped_kick(12.0)
        got = lambda spec: by_name(Q.check_take(x, spec))['kick_crest']
        self.assertIsNone(got(Q.Spec('kick', top=False)).ok)                 # softer layers are near-sine by design
        self.assertIsNone(got(Q.Spec('kick', crest=False)).ok)               # an override (a boom), with its why
        for cat in ('tom', 'bass', 'snare', 'perc'):
            self.assertIsNone(got(Q.Spec(cat, mud='off')).ok, cat)

    def test_turning_the_floor_off_needs_a_reason(self):
        with self.assertRaises(ValueError):
            Item('Drums', 'Boom', 'kick', FX.clean_kick, qa={'crest': False})
        Item('Drums', 'Boom', 'kick', FX.clean_kick, qa={'crest': False, 'why': 'a round sub boom has no transient'})


class TestBassBody(QaCase):
    """qa={'mud': 'bass-body'} (ADR 0011, proposed): a bass may keep its own harmonics in 150-400 Hz, up to a plain
    saw's level, but no bump, no low mids without a sub under them, and nothing in the low mids around the kicks."""

    BODY = ('body_vs_sub', 'body_over_saw', 'body_share')
    SPEC = Q.Spec('bass', oneshot=False, tonal=False, mud='bass-body')

    def test_plain_saws_pass_on_every_bass_note(self):
        for note in (28, 30, 31, 33, 37, 38, 40, 42, 43):                    # E1 to G2: the four kits' bass register
            with self.subTest(note=note):
                self.assertPasses(FX.saw_bass(note), self.SPEC, *self.BODY)

    def test_filtered_saws_and_warm_basses_pass(self):
        for note in (30, 37, 42):
            for lp in (200.0, 300.0, 500.0):
                with self.subTest(note=note, lp=lp):
                    self.assertPasses(FX.saw_bass(note, lp_hz=lp), self.SPEC, *self.BODY)
            self.assertPasses(FX.warm_bass(note), self.SPEC, *self.BODY)

    def test_muddy_basses_fail(self):
        fails = {kind: {c.name for c in Q.failed(Q.check_take(make(), self.SPEC))} & set(self.BODY)
                 for kind, make in FX.MUDDY_BASS.items()}
        for kind in ('bump', 'narrow', 'octave', 'fifth', 'resonant', 'boxy'):
            self.assertIn('body_over_saw', fails[kind], kind)
        self.assertIn('body_vs_sub', fails['tail'])
        self.assertEqual(set(FX.MUDDY_BASS), {k for k, v in fails.items() if v})

    def test_the_strict_gate_would_fail_every_saw(self):
        """Why the mode exists: under the strict gate a plain saw fails on every note, so a gated bass is hollow."""
        gate = Q.Spec('bass', oneshot=False, tonal=False, mud='gate')
        for note in (28, 30, 37, 42):
            self.assertFlags(FX.saw_bass(note), gate, 'mud_vs_sub_after_80ms')

    def test_the_round_0_kick_fails_bass_body_too(self):
        self.assertFlags(FX.master(FX.muddy_kick()), Q.Spec('bass', tonal=False, mud='bass-body'), 'body_vs_sub')

    def test_kicks_and_toms_keep_the_strict_gate(self):
        """Kicks, toms and booms can't leave the gate, with or without a reason."""
        for cat in ('kick', 'tom'):
            for mode in ('bass-body', 'lowcut', 'off'):
                with self.assertRaises(ValueError, msg=f'{cat} {mode}'):
                    Item('Drums', 'Test', cat, FX.clean_kick, qa={'mud': mode, 'why': 'no'})
            Item('Drums', 'Test', cat, FX.clean_kick, qa={'mud': 'gate'})
        with self.assertRaises(ValueError):
            Item('Bass', 'Test', 'bass', FX.clean_kick, qa={'mud': 'warm'})

    def test_bass_body_is_for_basses_and_bass_loops_with_a_reason(self):
        for group, cat in (('Bass', 'bass'), ('Loops', 'loop'), ('Drums', 'bass')):
            Item(group, 'Test', cat, FX.clean_kick, qa={'mud': 'bass-body', 'why': 'B, warm'})
            with self.assertRaises(ValueError, msg=f'{cat} without a why'):
                Item(group, 'Test', cat, FX.clean_kick, qa={'mud': 'bass-body'})
        for group, cat in (('Pad', 'pad'), ('Lead', 'lead'), ('Drums', 'snare'), ('Perc', 'perc'), ('Stabs', 'stab')):
            with self.assertRaises(ValueError, msg=cat):
                Item(group, 'Test', cat, FX.clean_kick, qa={'mud': 'bass-body', 'why': 'warm'})

    def test_weak_low_mids_still_read(self):
        """The over-saw measure reads peaks down to 60 dB under the loudest partial: a 250 Hz tone 30 dB under a 50 Hz
        sub reads 30 - 14 = 16 dB under the saw's roll-off, rather than "nothing in 150-400 Hz"."""
        x = D.fade(FX.tone(50.0, 2.0) + FX.tone(250.0, 2.0, 0.5 * D.undb(-30.0)), 0.01, 0.01)
        self.assertAlmostEqual(Q.over_saw_db(x), -16.0, delta=0.2)

    def test_a_phrase_against_kicks(self):
        """A phrase that declares the kicks it plays against: it must leave each kick room (clear 40-15 ms before it,
        ducked under its first 80 ms) and keep its body in the gaps."""
        room, onsets = FX.body_phrase()
        spec = Q.Spec('loop', oneshot=False, mud='bass-body', hits=tuple(onsets))
        self.assertPasses(room, spec, 'loop_body_vs_sub', 'loop_mud_before_hit', 'body_duck', 'body_share')
        self.assertIsNone(by_name(Q.check_take(room, spec))['body_vs_sub'].ok)          # the gaps instead
        held, _ = FX.body_phrase(dip_db=0.0)
        self.assertFlags(held, spec, 'loop_mud_before_hit', 'body_duck')
        with_kick = Q.Spec('loop', oneshot=False, mud='bass-body', hits=tuple(onsets), tags=('with_kick',))
        self.assertIsNone(by_name(Q.check_take(held, with_kick))['body_duck'].ok)       # the kick is in the file

    def duck(self, x, onsets):
        return by_name(Q.check_take(x, Q.Spec('loop', oneshot=False, mud='bass-body', hits=tuple(onsets))))['body_duck']

    def test_the_duck_reads_the_bass_own_level_under_every_kick(self):
        """Each kick against the bass's own level in the gap after it: one loud low-mid accent elsewhere doesn't let
        an undocked kick pass."""
        for accent in (0.0, 10.0, 13.0):
            got = self.duck(*FX.body_phrase(duck_db=0.0, accent_db=accent))
            self.assertIs(got.ok, False, f'accent +{accent}: {got.value}')
            self.assertGreater(got.value, -1.0)

    def test_the_same_duck_reads_the_same_on_any_note(self):
        """F#1, C#2 and F#2 saws (their low mids are harmonics 4-8, 3-5 and 2-4) with the same duck read within 1 dB:
        6 dB passes on every note, none fails."""
        for duck, ok in ((6.0, True), (0.0, False)):
            vals = []
            for note in (30, 37, 42):
                for lp in (300.0, 500.0):
                    got = self.duck(*FX.saw_phrase(note, duck, lp))
                    self.assertIs(got.ok, ok, f'{note} lp {lp} duck {duck}: {got.value}')
                    vals.append(got.value)
            self.assertLess(max(vals) - min(vals), 1.0, vals)

    def test_the_duck_covers_the_kick_s_first_80_ms(self):
        """Low mids that come back 40 ms after the kick fail, however deep the duck was."""
        got = self.duck(*FX.body_phrase(duck_db=40.0, back=0.040))
        self.assertAlmostEqual(got.value, -3.0, delta=0.3)
        self.assertIs(got.ok, False)

    def test_a_bass_that_sits_out_a_kick(self):
        """Resting under a kick and through the gap after it reads against the floor (30 dB under the loudest gap),
        not against the silence of that gap."""
        x, onsets = FX.body_phrase(sits_out=(2,))
        self.assertAlmostEqual(Q.loop_mud(x, onsets)[2]['duck_db'], -9.8, delta=0.3)
        self.assertIs(self.duck(x, onsets).ok, True)

    def test_one_shots_get_the_groove_loop_with_the_body_limits(self):
        layers = FX.shared_layers(FX.clean_kick, (0.4, 0.7, 1.0))
        names = [c.name for c in Q.check_loop(layers, 134.0, Q.TWO_STEP, 'bass-body')[0]]
        self.assertEqual(['loop_body_vs_sub', 'loop_mud_before_hit'], names)
        self.assertEqual(['loop_mud_vs_sub', 'loop_mud_before_hit'],
                         [c.name for c in Q.check_loop(layers, 134.0, Q.TWO_STEP)[0]])


class TestLimits(QaCase):
    """Each limit flips the result between a fixture just inside it and one just outside: loosening a limit fails a
    test. The fixtures are built to read a set value, which each test checks first."""

    def pair(self, inside, outside, spec, name, measure=None, inside_at=None, outside_at=None, tol=1.5):
        if measure:
            self.assertAlmostEqual(measure(inside), inside_at, delta=tol)
            self.assertAlmostEqual(measure(outside), outside_at, delta=tol)
        self.assertPasses(inside, spec, name)
        self.assertFlags(outside, spec, name)

    def test_mud_150_400_fall(self):
        fall = lambda x: Q.mud(x)['fall_150_400_ms']
        self.pair(FX.ringing_kick(0.100), FX.ringing_kick(0.140), Q.Spec('kick'), 'mud_150_400_fall', fall, 95, 135, 8)

    def test_mud_vs_sub(self):
        vs = lambda x: Q.mud(x)['vs_sub_db']
        self.pair(FX.ringing_kick(0.30, amp=0.0062, t0=0.08), FX.ringing_kick(0.30, amp=0.0124, t0=0.08),
                  Q.Spec('kick'), 'mud_vs_sub_after_80ms', vs, -23, -17)

    def test_a_short_bloom_after_80ms_is_caught(self):
        """A 250 Hz bloom from 80 to 118 ms: over the whole tail it averages out under the sub, window by window it
        doesn't (the review's near-miss)."""
        x = FX.ringing_kick(0.118, amp=0.15, t0=0.08)
        self.assertPasses(x, Q.Spec('kick'), 'mud_150_400_fall', 'mud_share')
        self.assertFlags(x, Q.Spec('kick'), 'mud_vs_sub_after_80ms')

    def test_box_400_1k_fall(self):
        fall = lambda x: Q.mud(x)['fall_400_1k_ms']
        self.pair(FX.ringing_kick(0.130, f=650.0), FX.ringing_kick(0.170, f=650.0), Q.Spec('kick'),
                  'box_400_1k_fall', fall, 127, 167, 8)

    def test_loop_mud_vs_sub(self):
        for amp, at, ok in ((0.0062, -23, True), (0.0124, -17, False)):
            checks, _ = Q.check_loop(FX.ringing_layers(0.30, amp), 134.0, Q.TWO_STEP)
            got = by_name(checks)['loop_mud_vs_sub']
            self.assertAlmostEqual(got.value, at, delta=1.5)
            self.assertIs(got.ok, ok)

    def test_low_side(self):
        self.pair(FX.low_side_at(-23.0), FX.low_side_at(-17.0), Q.Spec('bass', mud='off', tonal=False), 'low_side',
                  Q.low_side_db, -23, -17, 0.2)

    def test_tail(self):
        self.pair(FX.tail_at(-75.0), FX.tail_at(-65.0), Q.Spec('perc'), 'tail', Q.tail_db, -75, -65, 1.5)

    def test_first_sample(self):
        self.pair(FX.first_at(-85.0), FX.first_at(-75.0), Q.Spec('perc'), 'first_sample', Q.first_sample_db,
                  -85, -75, 0.1)

    def test_seam(self):
        spec = Q.Spec('pad', oneshot=False, tonal=False)
        self.pair(FX.seam_at(1.0), FX.seam_at(5.0), spec, 'seam', Q.seam_db, 1, 5, 0.5)

    def test_splice(self):
        (soft, at), (hard, _) = FX.hf_burst(9.0), FX.hf_burst(15.0)
        spec = Q.Spec('pad', oneshot=False, splices=at)
        self.pair(soft, hard, spec, 'splices', lambda x: Q.click_at(x, at), 9, 15, 1.5)

    def test_alias(self):
        spec = Q.Spec('lead', f0=1000.0, oneshot=False)
        asr = lambda x: Q.harmonic_asr(x, (1000.0,), 0.1, 0.95)
        self.pair(FX.with_alias(-70.0), FX.with_alias(-50.0), spec, 'alias', asr, -70, -50, 1.0)

    def test_true_peak_ceiling(self):
        x = FX.full_tail()
        self.pair(D.to_ceiling(x, -1.2), D.to_ceiling(x, -0.8), Q.Spec('perc'), 'true_peak', D.true_peak_db,
                  -1.2, -0.8, 0.01)

    def test_low_cut_share(self):
        self.pair(FX.low_share_at(-27.0), FX.low_share_at(-21.0), Q.Spec('pad', oneshot=False, tonal=False),
                  'lowcut_share_below_100hz', Q.low_share_db, -27, -21, 0.5)

    def test_kick_crest(self):
        self.pair(FX.kick_crest_at(4.5), FX.kick_crest_at(3.5), Q.Spec('kick', f0=FX.F_KICK), 'kick_crest',
                  Q.kick_crest_db, 4.5, 3.5, 0.05)

    def test_body_vs_sub(self):
        spec = Q.Spec('bass', oneshot=False, tonal=False, mud='bass-body')
        self.pair(FX.body_tones(-3.0), FX.body_tones(-1.0), spec, 'body_vs_sub', lambda x: Q.mud(x)['vs_sub_db'],
                  -3, -1, 0.3)

    def test_body_over_saw(self):
        spec = Q.Spec('bass', oneshot=False, tonal=False, mud='bass-body')
        self.pair(FX.saw_with_boost(4.0), FX.saw_with_boost(6.0), spec, 'body_over_saw', Q.over_saw_db, 4, 6, 0.3)

    def test_body_share(self):
        spec = Q.Spec('bass', oneshot=False, tonal=False, mud='bass-body')
        self.pair(FX.body_share_at(-6.0), FX.body_share_at(-4.0), spec, 'body_share',
                  lambda x: Q.mud(x)['share_db'], -6, -4, 0.3)

    def phrase_check(self, name, mode='bass-body', **kw):
        """One check on FX.body_phrase(**kw), a phrase that declares its kicks."""
        x, onsets = FX.body_phrase(**kw)
        return by_name(Q.check_take(x, Q.Spec('loop', oneshot=False, mud=mode, hits=tuple(onsets))))[name]

    def test_loop_body_vs_sub(self):
        for ratio, ok in ((-3.0, True), (-1.0, False)):
            got = self.phrase_check('loop_body_vs_sub', ratio_db=ratio, dip_db=0.0)                   # steady
            self.assertAlmostEqual(got.value, ratio, delta=0.5)
            self.assertIs(got.ok, ok)

    def test_body_duck(self):
        """The phrase rests 40 dB before each kick and ducks duck_db under its first 80 ms. The reading is against the
        gap's mean, which the rest before the next kick pulls about 0.7 dB under the held level."""
        for duck, at, ok in ((6.0, -5.3, True), (3.0, -2.4, False)):
            got = self.phrase_check('body_duck', duck_db=duck)
            self.assertAlmostEqual(got.value, at, delta=0.3)
            self.assertIs(got.ok, ok)

    def test_loop_body_vs_sub_of_a_bass_shot(self):
        """A bass one-shot's groove loop under bass-body: the gap limit, four on the floor at 130 BPM."""
        for ratio, ok in ((-3.0, True), (-1.0, False)):
            checks, _ = Q.check_loop({1.0: FX.body_shot(ratio)}, 130.0, Q.FOUR_FLOOR, 'bass-body')
            got = by_name(checks)['loop_body_vs_sub']
            self.assertAlmostEqual(got.value, ratio, delta=0.5)
            self.assertIs(got.ok, ok)

    def test_loop_mud_before_hit(self):
        """The before-hit limit, the same in the strict gate and in bass-body."""
        for mode in ('bass-body', 'gate'):
            for dip, at, ok in ((33.0, -34.3, True), (27.0, -28.3, False)):
                got = self.phrase_check('loop_mud_before_hit', mode, dip_db=dip)
                self.assertAlmostEqual(got.value, at, delta=0.6, msg=mode)
                self.assertIs(got.ok, ok, mode)

    def test_hf_band_on_synthetic_vocals(self):
        """D4: a vocal master has a synthetic top band (extend_hf); a 24 kHz take resampled to 48 kHz doesn't."""
        spec = Q.Spec('vox', tags=('clean', 'dry'), flags=frozenset({'hf_synthetic'}))
        self.pair(FX.vocal_band(-35.0), FX.vocal_band(-45.0), spec, 'hf_band', Q.hf_band_db, -35, -45, 0.5)
        self.assertLess(Q.hf_band_db(FX.resampled_vocal()), -55.0)
        self.assertFlags(FX.resampled_vocal(), spec, 'hf_band')
        self.assertPasses(FX.vocal_band(-15.0), spec, 'hf_band')
        self.assertIsNone(by_name(Q.check_take(FX.resampled_vocal(), Q.Spec('vox')))['hf_band'].ok)

    def test_transcript_score_limit(self):
        script = ' '.join(f'word{c}' for c in 'abcdefghijklmnopqrst')        # 20 words
        heard = lambda k: ' '.join(script.split()[:k])
        self.assertTrue(Q.check_transcript(np.zeros(SR), script, asr=lambda y, sr: heard(17)).ok)    # 0.85
        self.assertFalse(Q.check_transcript(np.zeros(SR), script, asr=lambda y, sr: heard(15)).ok)   # 0.75


CLUB_GENRE = 'hyper-clean future house'
# The standard's club targets (docs/plans/S1-modern-sound.md sections 1.1, 1.2 and 1.4; ADR 0014), written out here so
# that every pair below is built at these numbers, not at qa.CLUB's: loosening a target in qa.py fails the literal
# check and the pairs. Per genre: LUFS-I (target, +- LU), the PLR floor, and each octave's window, dB re 1 kHz.
STANDARD = {
    'future garage / 2-step': dict(lufs=(-9.0, 1.0), plr_min=7.5, bands={
        'sub': (6.0, 12.0), '125': (2.0, 6.0), '250': (1.0, 4.0), '500': (0.0, 3.0), '2k': (-3.0, 0.0),
        '4k': (-5.0, -2.0), '8k': (-8.0, -4.0), '16k': (-14.0, -8.0)}),
    'sci-fi club': dict(lufs=(-8.5, 1.0), plr_min=7.0, bands={
        'sub': (6.0, 12.0), '125': (1.0, 6.0), '250': (0.0, 4.0), '500': (0.0, 3.0), '2k': (-3.0, 0.0),
        '4k': (-5.0, -2.0), '8k': (-8.0, -4.0), '16k': (-14.0, -8.0)}),
    'hyper-clean future house': dict(lufs=(-8.0, 1.0), plr_min=6.5, bands={
        'sub': (6.0, 12.0), '125': (1.0, 5.0), '250': (0.0, 3.0), '500': (0.0, 3.0), '2k': (-2.0, 1.0),
        '4k': (-4.0, -1.0), '8k': (-7.0, -3.0), '16k': (-12.0, -7.0)}),
    'dark industrial / techno': dict(lufs=(-8.0, 1.0), plr_min=6.5, bands={
        'sub': (4.0, 12.0), '125': (1.0, 6.0), '250': (0.0, 4.0), '500': (0.0, 3.0), '2k': (-3.0, 0.0),
        '4k': (-6.0, -2.0), '8k': (-10.0, -5.0), '16k': (-17.0, -10.0)}),
    'garage-house / deep tech house': dict(lufs=(-8.5, 1.0), plr_min=7.0, bands={
        'sub': (6.0, 12.0), '125': (1.0, 6.0), '250': (0.0, 4.0), '500': (0.0, 3.0), '2k': (-3.0, 0.0),
        '4k': (-5.0, -2.0), '8k': (-8.0, -4.0), '16k': (-14.0, -8.0)}),
}


def club_spec(genre=CLUB_GENRE, **kw):
    return Q.Spec('loop', oneshot=False, tags=('club',), genre=genre, **kw)


def centre_levels(genre=CLUB_GENRE, **moved):
    """Octave levels (dB re 1 kHz) at the middle of every one of the genre's windows (the standard's, STANDARD), the
    sub in the 63 Hz band and the 31.5 Hz band 12 dB under it; `moved` sets bands by name ('sub' is the 63 Hz band)."""
    w = STANDARD[genre]['bands']
    lv = {name: 0.5 * (lo + hi) for name, (lo, hi) in w.items() if name != 'sub'}
    lv['63'] = 0.5 * sum(w['sub'])
    lv['31.5'] = lv['63'] - 12.0
    for name, v in moved.items():
        lv['63' if name == 'sub' else name] = v
    return lv


def report(cap=4.0, reduction=4.0, kick=None, clap=None):
    """A master report (master.master) as QA reads it: the cap it ran with, the reduction it used and, given (in, out)
    crest lists, the punch of its kicks and claps."""
    def row(pair, band=False):
        a, b = pair
        loss = [u - v for u, v in zip(a, b)]
        r = {'onsets': list(range(len(a))), 'span_s': 0.2, 'in_db': list(a), 'out_db': list(b),
             'median_loss_db': float(np.median(loss)), 'min_out_db': float(min(b))}
        if band:
            r.update(band_hz=[1000.0, 5000.0], band_in_db=list(a), band_out_db=list(a), band_median_loss_db=0.0,
                     band_min_out_db=float(min(a)))
        return r
    punch = {}
    if kick:
        punch['kick'] = row(kick)
    if clap:
        punch['clap'] = row(clap, band=True)
    return {'settings': {'max_reduction_db': cap}, 'peak_reduction_db': reduction, 'punch': punch}


class TestClub(QaCase):
    """The checks on a mastered 'club' loop (ADR 0014, proposed), against its genre's targets (qa.CLUB): loudness,
    the PLR floor, the true peak round the loop, the octave balance and the low mids' width. Each limit has a pair
    just inside and just outside it, built at the standard's own numbers (STANDARD), so loosening a limit in qa.py
    fails a test; the loops are built to read set values (test_mixmeter's shaped loops)."""

    NAMES = ('club_loudness', 'club_plr', 'club_true_peak', 'club_bands', 'club_low_width')

    def pair(self, inside, outside, spec, name, at=None, tol=0.05, club_only=False):
        run = (lambda x: by_name(Q._check_club(x, spec))) if club_only else (lambda x: by_name(Q.check_take(x, spec)))
        got_in, got_out = (run(x)[name] for x in (inside, outside))
        if at is not None:
            self.assertAlmostEqual(got_in.value, at[0], delta=tol)
            self.assertAlmostEqual(got_out.value, at[1], delta=tol)
        self.assertIs(got_in.ok, True, f'{name}: {got_in.value} ({got_in.limit}) {got_in.detail}')
        self.assertIs(got_out.ok, False, f'{name}: {got_out.value} ({got_out.limit}) {got_out.detail}')

    def test_the_targets_are_the_standard_s(self):
        """qa.CLUB holds the standard's numbers, and the club limits theirs: a change to any of them is a change to
        the standard, made here too."""
        self.assertEqual(STANDARD, Q.CLUB)
        self.assertEqual(-10.0, Q.CLUB_LOW_WIDTH_MAX_DB)
        self.assertEqual(4.0, Q.CLUB_MAX_REDUCTION_DB)
        self.assertEqual(1.5, Q.CLUB_CREST_LOSS_MAX_DB)
        self.assertEqual(4.0, Q.KICK_CREST_MIN_DB)        # ADR 0011's floor, which the crest pairs below sit around
        self.assertEqual(('sub', '125', '250', '500', '2k', '4k', '8k', '16k'), Q.CLUB_BANDS)

    def test_only_club_loops_get_them(self):
        x = TM.shaped_loop(centre_levels(), lufs=-8.0)
        self.assertFalse(set(self.NAMES) & set(by_name(Q.check_take(x, Q.Spec('loop', oneshot=False)))))
        self.assertTrue(set(self.NAMES) <= set(by_name(Q.check_take(x, club_spec()))))

    def test_the_genres_are_the_kits_directions(self):
        from tools.sound import kits
        self.assertEqual(set(Q.CLUB), set(kits.DIRECTIONS))

    def test_a_club_loop_needs_its_genre(self):
        got = by_name(Q.check_take(TM.shaped_loop(lufs=-8.0), club_spec(genre=None)))
        self.assertIs(got['club_genre'].ok, False)
        self.assertFalse(set(self.NAMES) & set(got))

    def test_loudness_window(self):
        """Each genre's LUFS-I target +-1 LU, read round the loop: a pair 0.3 LU inside and outside each edge."""
        for genre, t in STANDARD.items():
            target, tol = t['lufs']
            for side in (-1, 1):
                with self.subTest(genre=genre, side=side):
                    make = lambda lv: TM.shaped_loop(centre_levels(genre), lufs=lv)
                    inside, outside = target + side * (tol - 0.3), target + side * (tol + 0.3)
                    self.pair(make(inside), make(outside), club_spec(genre), 'club_loudness', (inside, outside), 1e-6,
                              club_only=True)

    def test_plr_floor(self):
        for genre, t in STANDARD.items():
            with self.subTest(genre=genre):
                floor = t['plr_min']
                self.pair(TM.plr_loop(floor + 0.3), TM.plr_loop(floor - 0.3), club_spec(genre), 'club_plr',
                          (floor + 0.3, floor - 0.3), 0.01, club_only=True)

    def test_true_peak_round_the_loop(self):
        x = TM.shaped_loop(centre_levels(), lufs=-8.0)
        at = lambda tp: x * D.undb(tp - MM.true_peak_db(x))
        self.pair(at(-1.2), at(-0.8), club_spec(), 'club_true_peak', (-1.2, -0.8), 1e-6)

    def test_a_peak_across_the_seam(self):
        """A file's true peak can't see a peak that spans the seam; the loop's can (test_mixmeter's pulse)."""
        n = SR
        d = (np.arange(n) - (n - 0.5) + n / 2) % n - n / 2
        x = D.st(np.sinc(0.9 * d) * np.where(np.abs(d) <= 64, np.hanning(129)[np.clip(np.round(d).astype(int) + 64,
                                                                                         0, 128)], 0.0))
        x = x * D.undb(-0.9 - MM.true_peak_db(x))
        got = by_name(Q.check_take(x, club_spec()))
        self.assertIs(got['true_peak'].ok, True, got['true_peak'].value)
        self.assertIs(got['club_true_peak'].ok, False, got['club_true_peak'].value)

    def test_balance_windows(self):
        """Every octave in its window (dB re 1 kHz), both edges of all eight windows in all four genres: the value is
        the worst distance outside one, so a pair 0.3 dB either side of an edge reads -0.3 and +0.3."""
        for genre, t in STANDARD.items():
            for name, (lo, hi) in t['bands'].items():
                for v, side in ((lo, -1), (hi, 1)):
                    with self.subTest(genre=genre, band=name, edge=v):
                        mk = lambda u: TM.shaped_loop(centre_levels(genre, **{name: u}), lufs=t['lufs'][0])
                        self.pair(mk(v - side * 0.3), mk(v + side * 0.3), club_spec(genre), 'club_bands',
                                  (-0.3, 0.3), 1e-6, club_only=True)

    def test_some_bands_exempted(self):
        """An override may exempt named bands: they are listed, not gated, and every other band stays gated."""
        lo = STANDARD[CLUB_GENRE]['bands']['125'][0]
        out_125 = TM.shaped_loop(centre_levels(**{'125': lo - 2.0}), lufs=-8.0)
        got = by_name(Q._check_club(out_125, club_spec(club_bands=('125', '250'))))['club_bands']
        self.assertIs(got.ok, True, got.detail)
        self.assertIn('125 -1.0 in +1..+5 (exempt, out)', got.detail)
        self.assertIs(by_name(Q._check_club(out_125, club_spec()))['club_bands'].ok, False)
        hi = STANDARD[CLUB_GENRE]['bands']['16k'][1]
        both = TM.shaped_loop(centre_levels(**{'125': lo - 2.0, '16k': hi + 0.3}), lufs=-8.0)
        got = by_name(Q._check_club(both, club_spec(club_bands=('125', '250'))))['club_bands']
        self.assertIs(got.ok, False, got.detail)
        self.assertAlmostEqual(0.3, got.value, delta=1e-6)
        every = by_name(Q._check_club(both, club_spec(club_bands=Q.CLUB_BANDS)))['club_bands']
        self.assertIsNone(every.ok)

    def test_the_sub_is_read_from_the_louder_of_31_and_63_hz(self):
        """An E1 kick (41 Hz) puts the sub in the 31.5 Hz band: read together, the louder band carries it."""
        lo = STANDARD[CLUB_GENRE]['bands']['sub'][0]
        e1 = centre_levels(**{'31.5': lo + 0.3, '63': lo - 8.0})
        self.assertIs(by_name(Q.check_take(TM.shaped_loop(e1, lufs=-8.0), club_spec()))['club_bands'].ok, True)
        low = centre_levels(**{'31.5': lo - 0.3, '63': lo - 8.0})
        self.assertIs(by_name(Q.check_take(TM.shaped_loop(low, lufs=-8.0), club_spec()))['club_bands'].ok, False)

    def test_turning_the_balance_off_needs_a_reason(self):
        x = TM.shaped_loop(centre_levels(**{'16k': -20.0}), lufs=-8.0)
        self.assertIsNone(by_name(Q.check_take(x, club_spec(club_bands=False)))['club_bands'].ok)
        render = lambda take: np.zeros(10)
        with self.assertRaises(ValueError):
            Item('Loops', 'Club', 'loop', render, loop=True, hp_hz=None, tags=('club',),
                 qa={'mud': 'gate', 'club_bands': False})
        Item('Loops', 'Club', 'loop', render, loop=True, hp_hz=None, tags=('club',),
             qa={'mud': 'gate', 'club_bands': False, 'why': 'the dark top is the kit'})
        with self.assertRaises(ValueError):
            Item('Loops', 'Plain', 'loop', render, loop=True, hp_hz=None,
                 qa={'mud': 'off', 'club_bands': False, 'why': 'not a club loop'})

    def test_low_mid_width(self):
        """Side over mid in the 125 and 250 Hz octaves at most -10 dB, the worse of the two."""
        mk = lambda **wd: TM.shaped_loop(centre_levels(), width=wd, lufs=-8.0)
        self.pair(mk(**{'250': -10.5}), mk(**{'250': -9.5}), club_spec(), 'club_low_width', (-10.5, -9.5), 1e-6)
        self.pair(mk(**{'125': -10.5, '250': -30.0}), mk(**{'125': -9.5, '250': -30.0}), club_spec(), 'club_low_width',
                  (-10.5, -9.5), 1e-6)

    OFF = {'mud': 'off', 'why': 'the full mix, mastered'}

    def test_club_items_are_mastered_loops(self):
        """A 'club' item is a loop that the master ends: no renderer high-pass after it (it would move the peaks off
        the ceiling) and no LUFS target (the master sets the loudness)."""
        render = lambda take: np.zeros(10)
        Item('Loops', 'Club', 'loop', render, loop=True, hp_hz=None, tags=('club',), qa=self.OFF)
        for bad in (dict(loop=False, hp_hz=None), dict(loop=True, hp_hz=20.0), dict(loop=True, hp_hz=None, level=-9.0)):
            with self.assertRaises(ValueError, msg=str(bad)):
                Item('Loops', 'Club', 'loop', render, tags=('club',), qa=self.OFF, **bad)
        with self.assertRaises(ValueError):
            Item('Pad', 'Club', 'pad', render, loop=True, hp_hz=None, tags=('club',))

    def test_the_genre_comes_from_the_kit(self):
        from tools.sound.item import Out
        render = lambda take: np.zeros(10)
        club = Item('Loops', 'Club', 'loop', render, loop=True, hp_hz=None, tags=('club',), qa=self.OFF)
        self.assertEqual(club.takes('FUT03')[0].spec(Out(np.zeros(10))).genre, 'garage-house / deep tech house')
        self.assertEqual(club.takes('FUT01')[0].spec(Out(np.zeros(10))).genre, 'future garage / 2-step')
        plain = Item('Loops', 'Plain', 'loop', render, loop=True, hp_hz=None, qa={'mud': 'off', 'why': 'a test'})
        self.assertIsNone(plain.takes('FUT03')[0].spec(Out(np.zeros(10))).genre)


    def test_the_club_overrides_are_exceptions_with_a_reason(self):
        """club_bands (False or band names), club_max_reduction_db and club_crest_loss_db: on a club loop only, each
        with a why, each the right kind of value. The fixture keeps the mud gate, which needs no reason, so each
        refusal is the override's own (mud 'off' would need a why by itself and hide a missing one)."""
        render = lambda take: np.zeros(10)
        club = lambda **qa: Item('Loops', 'Club', 'loop', render, loop=True, hp_hz=None, tags=('club',),
                                 qa={'mud': 'gate', **qa})
        club()
        club(club_bands=('125', '250'), why='a reason')
        club(club_max_reduction_db=6.0, club_crest_loss_db=2.4, why='a reason')
        for key, v in (('club_bands', ('125',)), ('club_bands', False), ('club_max_reduction_db', 6.0),
                       ('club_crest_loss_db', 2.0)):
            with self.assertRaisesRegex(ValueError, rf"qa {key} is an exception: it needs qa\['why'\]", msg=key):
                club(**{key: v})
        for bad in (dict(club_bands=('125', '300'), why='x'), dict(club_bands='125', why='x'),
                    dict(club_bands=(), why='x'), dict(club_bands=True, why='x')):
            with self.assertRaisesRegex(ValueError, 'qa club_bands is False or the names of the bands', msg=str(bad)):
                club(**bad)
        for bad in (dict(club_max_reduction_db=0.0, why='x'), dict(club_max_reduction_db=True, why='x'),
                    dict(club_crest_loss_db='2', why='x')):
            with self.assertRaisesRegex(ValueError, 'is a number of dB, over 0', msg=str(bad)):
                club(**bad)
        for key, v in (('club_bands', ('125',)), ('club_max_reduction_db', 6.0), ('club_crest_loss_db', 2.0)):
            with self.assertRaisesRegex(ValueError, rf"qa {key} is for a 'club' loop", msg=key):
                Item('Loops', 'Plain', 'loop', render, loop=True, hp_hz=None, qa={'mud': 'off', key: v, 'why': 'x'})

    def test_the_spec_carries_the_master_report_and_the_kicks(self):
        from tools.sound.item import Out
        render = lambda take: np.zeros(10)
        rep = report(kick=([7.0], [5.5]))
        club = Item('Loops', 'Club', 'loop', render, loop=True, hp_hz=None, tags=('club',),
                    qa={'mud': 'off', 'club_bands': ['125'], 'club_max_reduction_db': 5.0, 'why': 'a test'})
        spec = club.takes('FUT03')[0].spec(Out(np.zeros(10), top_hits=(3, 7), meta={'master': rep}))
        self.assertIs(rep, spec.master)
        self.assertEqual((3, 7), spec.top_hits)
        self.assertEqual(('125',), spec.club_bands)
        self.assertEqual(5.0, spec.club_max_reduction_db)
        plain = Item('Loops', 'Plain', 'loop', render, loop=True, hp_hz=None, qa={'mud': 'off', 'why': 'a test'})
        self.assertIsNone(plain.takes('FUT03')[0].spec(Out(np.zeros(10), meta={'master': rep})).master)


class TestMasterChecks(unittest.TestCase):
    """What a club loop's master did, read from its report (qa.check_master): the cap it ran with and the reduction
    it used, the crest floor on every full-velocity kick, and the crest the master cost the hits. Each limit has a pair
    just inside and just outside it."""

    def check(self, rep, name, cap=None, loss=None):
        return by_name(Q.check_master(rep, cap, loss))[name]

    def test_the_cap_is_the_standard_s_or_declared(self):
        self.assertIs(self.check(report(4.0, 3.99), 'club_reduction').ok, True)
        self.assertIs(self.check(report(4.0, 4.0), 'club_reduction').ok, True)
        self.assertIs(self.check(report(4.0, 4.01), 'club_reduction').ok, False)
        raised = report(6.0, 5.6)
        self.assertIs(self.check(raised, 'club_reduction').ok, False, 'a raised cap is never a silent pass')
        self.assertIs(self.check(raised, 'club_reduction', cap=6.0).ok, True)
        self.assertIs(self.check(report(4.0, 3.9), 'club_reduction', cap=6.0).ok, False, 'a stale declaration')
        self.assertIs(self.check(report(6.0, 6.01), 'club_reduction', cap=6.0).ok, False)

    def test_every_full_velocity_kick_keeps_the_crest_floor(self):
        limit = 4.0                                       # ADR 0011's floor (Q.KICK_CREST_MIN_DB)
        inside = self.check(report(kick=([7.0, 7.0], [5.0, limit + 0.1])), 'club_kick_crest')
        outside = self.check(report(kick=([7.0, 7.0], [5.0, limit - 0.1])), 'club_kick_crest')
        self.assertEqual((True, False), (inside.ok, outside.ok))
        self.assertAlmostEqual(limit - 0.1, outside.value, delta=1e-9)

    def test_the_crest_the_master_costs(self):
        """The worse of the kicks' and the claps' median loss, at most 1.5 dB, or what the item declares."""
        lim = 1.5                                         # the A&R review's proposal (Q.CLUB_CREST_LOSS_MAX_DB)
        kicks = lambda loss: ([7.0, 7.0, 7.0], [7.0 - loss, 7.0 - loss, 5.0])
        claps = lambda loss: ([9.0, 9.0, 9.0], [9.0 - loss, 9.0 - loss, 9.0 - loss])
        self.assertIs(self.check(report(kick=kicks(lim - 0.1), clap=claps(0.5)), 'club_crest_loss').ok, True)
        self.assertIs(self.check(report(kick=kicks(lim + 0.1), clap=claps(0.5)), 'club_crest_loss').ok, False)
        got = self.check(report(kick=kicks(0.5), clap=claps(lim + 0.1)), 'club_crest_loss')
        self.assertIs(got.ok, False)
        self.assertAlmostEqual(lim + 0.1, got.value, delta=1e-9)
        self.assertIs(self.check(report(kick=kicks(lim - 0.1)), 'club_crest_loss').ok, True, 'no claps: kicks alone')
        self.assertIs(self.check(report(kick=kicks(2.3)), 'club_crest_loss', loss=2.4).ok, True)
        self.assertIs(self.check(report(kick=kicks(2.5)), 'club_crest_loss', loss=2.4).ok, False)

    def test_a_club_loop_carries_its_master_s_report_and_hits(self):
        for name in ('club_reduction', 'club_kick_crest', 'club_crest_loss'):
            self.assertIs(self.check(None, name).ok, False, name)
        for name in ('club_kick_crest', 'club_crest_loss'):
            self.assertIs(self.check(report(), name).ok, False, name)
        x = TM.shaped_loop(centre_levels(), lufs=-8.0)
        got = by_name(Q.check_take(x, club_spec()))
        self.assertIs(got['club_reduction'].ok, False)


class TestLoopCrest(QaCase):
    """The kick crest floor (ADR 0011) on every full-velocity kick a loop declares (Out.top_hits), read as the loop
    plays it: a kick lane clipped flat in a mix fails though the kick's own file passes."""

    def loop(self, kick, n=4):
        L = int(2.0 * SR)
        y = np.zeros((L, 2))
        hits = [k * L // n for k in range(n)]
        for h in hits:
            y[h:h + len(kick)] += D.st(kick)[:L - h]
        return y, hits

    def test_a_pair_round_the_floor(self):
        lim = 4.0                                         # ADR 0011's floor (Q.KICK_CREST_MIN_DB)
        inside, hits = self.loop(FX.kick_crest_at(lim + 0.15))
        outside, _ = self.loop(FX.kick_crest_at(lim - 0.15))
        spec = Q.Spec('loop', oneshot=False, top_hits=tuple(hits))
        got_in, got_out = (by_name(Q.check_take(x, spec))['loop_kick_crest'] for x in (inside, outside))
        self.assertAlmostEqual(lim + 0.15, got_in.value, delta=0.05)
        self.assertAlmostEqual(lim - 0.15, got_out.value, delta=0.05)
        self.assertEqual((True, False), (got_in.ok, got_out.ok))

    def test_only_declared_kicks_and_the_lowest_counts(self):
        good, hits = self.loop(FX.clean_kick(1.0))
        self.assertIsNone(by_name(Q.check_take(good, Q.Spec('loop', oneshot=False)))['loop_kick_crest'].ok)
        bad = good.copy()
        flat = FX.kick_crest_at(3.5)
        bad[hits[2]:hits[2] + len(flat)] = D.st(flat)[:len(bad) - hits[2]]
        got = by_name(Q.check_take(bad, Q.Spec('loop', oneshot=False, top_hits=tuple(hits))))['loop_kick_crest']
        self.assertIs(got.ok, False)
        self.assertAlmostEqual(3.5, got.value, delta=0.05)


class TestChoke(unittest.TestCase):
    """The groove loop with choke (qa loop_choke): each hit ends where the next starts, as a mono voice plays a bass
    shot, so a move longer than the gap doesn't sum with the next copy of itself."""

    def test_each_hit_ends_on_the_next(self):
        shot = D.st(np.sin(2 * np.pi * 46.25 * np.arange(int(2.0 * SR)) / SR))      # 2 s: longer than every gap
        loose, onsets = Q.two_step_loop({1.0: shot}, 134.0, Q.TWO_STEP)
        tight, same = Q.two_step_loop({1.0: shot}, 134.0, Q.TWO_STEP, choke=True)
        self.assertEqual(onsets, same)
        p, nxt = onsets[3], onsets[4]                                                # the tightest gap, 3 16ths
        np.testing.assert_allclose(tight[p:nxt - int(Q.CHOKE_FADE_S * SR)], shot[:nxt - p - int(Q.CHOKE_FADE_S * SR)],
                                   atol=1e-12)
        self.assertGreater(np.abs(loose[p:nxt] - shot[:nxt - p]).max(), 0.1)          # summed: the shot before rings
        self.assertLess(np.abs(tight[nxt - 1]).max(), 1e-3)                          # faded into the next hit

    def test_short_hits_are_untouched(self):
        short = D.st(np.hanning(int(0.05 * SR)))
        a, _ = Q.two_step_loop({1.0: short}, 134.0, Q.TWO_STEP)
        b, _ = Q.two_step_loop({1.0: short}, 134.0, Q.TWO_STEP, choke=True)
        self.assertTrue(np.array_equal(a, b))

    def test_it_is_declared_with_a_reason(self):
        render = lambda take: np.zeros(10)
        Item('Bass', 'Growl', 'bass', render, qa={'mud': 'gate', 'loop_choke': True, 'why': 'a mono voice'})
        for qa in ({'mud': 'gate', 'loop_choke': True}, {'mud': 'gate', 'loop_choke': 1, 'why': 'x'},
                   {'mud': 'gate', 'loop_choke': True, 'loop_check': False, 'why': 'x'}):
            with self.assertRaises(ValueError, msg=str(qa)):
                Item('Bass', 'Growl', 'bass', render, qa=qa)
        with self.assertRaises(ValueError):
            Item('Loops', 'Line', 'loop', render, loop=True, qa={'mud': 'gate', 'loop_choke': True, 'why': 'x'})


class TestContent(unittest.TestCase):
    LISTS = {'artists': denylist.entry_digests('Zorblax Quinn'), 'slurs': denylist.entry_digests('blorf'),
             'profanity': denylist.entry_digests('zonk')}

    def content(self, **kw):
        kw.setdefault('lists', self.LISTS)
        return by_name(Q.check_content(**kw))

    def test_artist_names_in_any_form(self):
        for name in ('KickZorblaxQuinn', 'kick_zorblax-quinn', 'ZORBLAXQUINN kick', 'Zórblax Quinn'):
            self.assertIs(self.content(names=[name])['artist_names'].ok, False, name)
        self.assertIs(self.content(names=['KickAlloy'], strings=['FUT01_Drums_KickAlloy_v1_rr1.flac'])
                      ['artist_names'].ok, True)

    def test_vocal_rating_flags_and_words(self):
        clean = dict(names=['RobotLine'], script='signal on the wire', category='vox', tags=('clean', 'dry'),
                     flags=frozenset({'hf_synthetic'}))
        self.assertEqual([], [c.name for c in Q.failed(Q.check_content(lists=self.LISTS, **clean))])
        self.assertIs(self.content(**{**clean, 'script': 'zonk the wire'})['clean_words'].ok, False)
        self.assertIs(self.content(**{**clean, 'script': 'zonk the blorf'})['slurs'].ok, False)
        self.assertIs(self.content(**{**clean, 'tags': ('dry',)})['rating'].ok, False)
        self.assertIs(self.content(**{**clean, 'flags': frozenset()})['hf_synthetic'].ok, False)
        explicit = {**clean, 'script': 'zonk the wire', 'tags': ('dry',),
                    'flags': frozenset({'hf_synthetic', 'explicit'})}
        self.assertEqual([], [c.name for c in Q.failed(Q.check_content(lists=self.LISTS, **explicit))])

    def test_shipped_lists(self):
        for name in denylist.LISTS:
            d = denylist.load(name)
            self.assertGreater(len(d), 10, name)
            self.assertTrue(all(re.fullmatch(r'[0-9a-f]{64}', h) for h in d), name)

    def test_legacy_style_names_are_denied(self):
        # the artists legacy app.js names its families after; read from the file so no test spells them out
        line = next(ln for ln in (REPO / 'legacy/web-pack/app.js').read_text(encoding='utf-8').splitlines()
                    if ln.startswith('const STYLE_OF'))
        names = [n for v in re.findall(r"'([^']*)-style'", line) for n in re.split(r'\s*&\s*', v)]
        self.assertGreaterEqual(len(names), 3)
        artists = denylist.load('artists')
        for n in names:
            self.assertTrue(denylist.hits(f'Kick {n} Remix', artists), 'a legacy style name is not denied')

    def test_kit_strings_pass_the_shipped_lists(self):
        strings = ['FUT01', 'Alloy Signal', 'bf_emma', 'af_nicole', 'FUT01/space', 'FUT01_Drums_Kick_v1_rr1.flac',
                   'FUT01_Bass_Reese-Fs1_v2_rr1.flac', 'Perc', 'Textures', 'Shimmer freeze bed']
        self.assertEqual([], [c.name for c in Q.failed(Q.check_content(names=strings))])


class TestTranscript(unittest.TestCase):
    def test_score(self):
        self.assertEqual(1.0, Q.transcript_score('Alloy signal', 'alloy signal.'))
        self.assertLess(Q.transcript_score('Alloy signal', 'a lawyer'), Q.TRANSCRIPT_MIN)

    def test_check_with_a_stub_recognizer(self):
        x = np.zeros(SR)
        self.assertTrue(Q.check_transcript(x, 'alloy signal', asr=lambda y, sr: 'Alloy signal').ok)
        self.assertFalse(Q.check_transcript(x, 'alloy signal', asr=lambda y, sr: 'all eye sick no').ok)

    @unittest.skipUnless(os.environ.get('WUBANSIA_MODELS_DIR'), 'WUBANSIA_MODELS_DIR not set')
    def test_whisper_hears_kokoro(self):
        from tools.sound import models
        try:
            import sherpa_onnx  # noqa: F401
        except ImportError:
            self.skipTest('requirements-vocals.txt not installed')
        x24 = models.tts('Alloy signal.', 'bf_emma')
        heard = models.asr(x24, models.TTS_SR)
        self.assertGreaterEqual(Q.transcript_score('alloy signal', heard), Q.TRANSCRIPT_MIN, heard)


if __name__ == '__main__':
    unittest.main()
