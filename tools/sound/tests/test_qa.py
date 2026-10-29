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
        muddy, _ = Q.check_loop(FX.shared_layers(FX.muddy_kick, (0.4, 0.7, 1.0)))
        clean, loop = Q.check_loop(FX.shared_layers(FX.clean_kick, (0.4, 0.7, 1.0)))
        self.assertEqual([False, False], [c.ok for c in muddy])
        self.assertEqual([True, True], [c.ok for c in clean])
        self.assertEqual(len(loop), round(32 * 60 / 134 / 4 * SR))

    def test_mud_in_the_gaps_of_a_loop_file(self):
        """A loop that declares its kick onsets gets the per-gap mud checks on the file itself."""
        for kick, ok in ((FX.clean_kick, True), (FX.muddy_kick, False)):
            loop, onsets = Q.two_step_loop(FX.shared_layers(kick, (0.4, 0.7, 1.0)))
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
            checks, _ = Q.check_loop(FX.ringing_layers(0.30, amp))
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
