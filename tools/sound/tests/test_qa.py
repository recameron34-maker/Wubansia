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
from tools.sound import qa as Q
from tools.sound.item import Item
from tools.sound.tests import fixtures as FX

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
