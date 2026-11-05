"""FUT01's vox and fx (recipes/fut01/vox.py, fx.py) and the drums it keeps (drums.py) in rounds 2 and 3: what goes in
the r2 and r3 sets, the three sung lines, the chops' title checks, the softened glottal stop, round 3's re-pitched
hooks, and the glitch fill's grid and notes. None of these needs the vocal models."""
import unittest

import numpy as np

from tools.sound import dsp48 as D
from tools.sound import kits
from tools.sound import phrase as Ph
from tools.sound import vocals as V
from tools.sound.kits import fut01 as K
from tools.sound.recipes.fut01 import fx, vox

KEPT = ('KickSplit', 'SnareMetal', 'SnareModal', 'ClosedHatFM', 'ClosedHatRing', 'AlloyHit', 'AlloyHitShort',
        'RobotBoost', 'RobotBoostWet', 'ShiftRiser')     # round 1's items that come back unchanged
ROUND1_ONLY = ('KickTight', 'SungPhrase', 'GlitchFill') + tuple(c[0] for c in vox.CHOPS)


def in_key(midi):
    return midi % 12 in K.SCALE


class TestSets(unittest.TestCase):
    def setUp(self):
        self.r3 = {it.name: it for it in kits.items(K, sets=('r3',))}
        self.r2 = {it.name: it for it in kits.items(K, sets=('r2',))}
        self.s1a = {it.name for it in kits.items(K, sets=('s1a',))}

    def test_round_1_items_kept_unchanged(self):
        for name in KEPT:
            with self.subTest(name=name):
                self.assertIn(name, self.r3)
                self.assertIn(name, self.r2)
                self.assertIn(name, self.s1a)

    def test_round_1_history_stays_out_of_r2_and_r3(self):
        """Kick B (Tight) lost round 1; the old line and its chops and the old fill are replaced."""
        for name in ROUND1_ONLY:
            with self.subTest(name=name):
                self.assertNotIn(name, self.r2)
                self.assertNotIn(name, self.r3)
                self.assertIn(name, self.s1a)

    def test_round_2_vox_and_fx_come_back_in_r3(self):
        new = [ln.name for ln in vox.LINES] + [c[0] for c in vox.PICK_CHOPS] + ['GlitchFillLift']
        for name in new:
            with self.subTest(name=name):
                self.assertIn(name, self.r2)
                self.assertIn(name, self.r3)
                self.assertNotIn(name, self.s1a)
        self.assertTrue(6 <= len(vox.PICK_CHOPS) <= 8)

    def test_every_round_2_item_comes_back_in_r3_but_the_loops_it_rebuilt(self):
        """Round 3 keeps round 2's sounds byte for byte (r2 and r3) and rebuilds only the context loops (r3 only)."""
        rebuilt = {'ContextLoop', 'ContextLoopWarm', 'ContextLoopWet', 'ContextLoopLow', 'ContextLoopLowWarm'}
        self.assertEqual(set(), set(self.r2) - set(self.r3))
        self.assertEqual(set(), rebuilt & set(self.r2))
        self.assertLessEqual(rebuilt, set(self.r3))

    def test_round_3_adds(self):
        added = {'KickPunch', 'SnareClap', 'Shaker', 'ChopMeE5', 'ChopFloorFs5', 'ContextLoopClub',
                 'DrumGrooveSplit', 'DrumGroovePunch'}
        for name in added:
            with self.subTest(name=name):
                self.assertIn(name, self.r3)
                self.assertEqual(('r3',), self.r3[name].sets)


class TestSungLines(unittest.TestCase):
    def test_lines_are_short_clean_and_sung_in_key(self):
        self.assertEqual(3, len(vox.LINES))
        for ln in vox.LINES:
            with self.subTest(line=ln.name):
                words = ln.text.split()
                self.assertTrue(3 <= len(words) <= 6, ln.text)
                self.assertEqual([w.lower() for w in words],
                                 [w for w, _ in V.group_words(ln.notes)])
                self.assertTrue(all(in_key(m) for _, _, m, _ in ln.notes), ln.notes)
                steps = [s for s, _, _, _ in ln.notes]
                self.assertEqual(steps, sorted(steps))
                self.assertTrue(all(s + n <= b for (s, n, _, _), (b, _, _, _) in zip(ln.notes, ln.notes[1:])))
                self.assertLessEqual(ln.notes[-1][0] + ln.notes[-1][1], 32)     # 2 bars
                item = kits.items(K, only=(ln.name,))[0]
                self.assertEqual(ln.text, item.script)
                self.assertIn('clean', item.tags)
                self.assertIn('dry', item.tags)

    def test_lines_swing_with_the_kit(self):
        self.assertEqual(K.SWING, vox.LINE_SWING)

    def test_chops_name_the_words_they_cut(self):
        words = [w for w, _ in V.group_words(vox.LINE_BY_NAME[vox.PICK].notes)]
        for name, first, last, said, root, _ in vox.PICK_CHOPS:
            with self.subTest(chop=name):
                self.assertEqual(' '.join(words[first:last + 1]), said)
                self.assertEqual(kits.items(K, only=(name,))[0].script, said)
                self.assertTrue(in_key(root))
        self.assertTrue(vox.LEAD_IN <= set(vox.PICK_CHOP_BY_NAME))

    def test_no_chop_sings_or_is_named_after_a_release_title(self):
        """Every chop of two words or more sings a piece of the line the title check kept, and no chop's name spells a
        piece it cut (CHOP_CHECKS: the exact phrase with "song" and "lyrics")."""
        cut = [w for w, why in vox.CHOP_CHECKS.items() if why.startswith('cut')]
        self.assertTrue(cut)
        camel = lambda words: ''.join(w.capitalize() for w in words.split())
        for name, _, _, words, _, _ in vox.PICK_CHOPS:
            with self.subTest(chop=name):
                if ' ' in words:
                    self.assertTrue(vox.CHOP_CHECKS[words].startswith('kept'), words)
                self.assertNotIn(words, cut)
                for c in cut:
                    self.assertNotIn(camel(c), name)

    def test_glottal_stops_are_softened(self):
        """The onset frames of a syllable that starts with a glottal stop, and only those, come down GLOTTAL_DB."""
        self.assertLessEqual(vox.GLOTTAL_DB, -12.0)
        n = 12
        kind = np.array([2, 2, 3, 1, 1, 2, 2, 3, 1, 2, 2, 0])
        ev = np.array([0, 0, 0, 1, 1, 1, 1, 1, 2, 2, 2, -1])
        events = [{'syms': 'miː'}, {'syms': 'ʔɒn'}, {'syms': 'ðə'}]
        tr = V.Track(np.ones(n), np.ones((n, 4)), np.zeros((n, 4)), kind, ev, events)
        out = vox.soften_stops(tr)
        want = np.where((ev == 1) & (kind == 1), D.undb(vox.GLOTTAL_DB) ** 2, 1.0)
        self.assertTrue(np.allclose(out.sp, want[:, None]))
        self.assertTrue(np.array_equal(tr.sp, np.ones((n, 4))), 'the track it was given is left as it was')


class TestHooks(unittest.TestCase):
    def test_the_hooks_are_pick_chops_a_fifth_up_in_key(self):
        """Each re-pitched hook is one of the pick's chops up a fifth, its formants raised a little with it (never to a
        cartoon), on a root in the key, singing the chop's words."""
        self.assertEqual(2, len(vox.HIGH_CHOPS))
        for name, chop, semis, formant, _ in vox.HIGH_CHOPS:
            with self.subTest(hook=name):
                self.assertIn(chop, vox.PICK_CHOP_BY_NAME)
                self.assertEqual(7, semis)
                self.assertTrue(1.0 < formant <= 1.1)
                item = kits.items(K, only=(name,))[0]
                self.assertEqual(vox.PICK_CHOP_BY_NAME[chop][4] + semis, item.root)
                self.assertTrue(in_key(item.root))
                self.assertEqual(vox.PICK_CHOP_BY_NAME[chop][3], item.script)
                self.assertIn('clean', item.tags)
                self.assertIn('hf_synthetic', item.flags)

    def test_the_hooks_are_single_words_and_spell_no_cut_title(self):
        cut = [w for w, why in vox.CHOP_CHECKS.items() if why.startswith('cut')]
        camel = lambda words: ''.join(w.capitalize() for w in words.split())
        for name, chop, _, _, _ in vox.HIGH_CHOPS:
            with self.subTest(hook=name):
                self.assertNotIn(' ', vox.PICK_CHOP_BY_NAME[chop][3])
                for c in cut:
                    self.assertNotIn(camel(c), name)

    def test_repitch_moves_f0_and_stretches_the_envelope(self):
        n, bins = 6, 9
        f0 = np.array([0.0, 220.0, 220.0, 330.0, 0.0, 440.0])
        sp = np.tile(np.linspace(1.0, 2.0, bins), (n, 1))
        tr = V.Track(f0, sp, np.zeros((n, bins)), np.ones(n, int), np.zeros(n, int), [{}])
        out = vox.repitched(tr, 12, 1.0)
        self.assertTrue(np.allclose(out.f0, 2 * f0))
        self.assertTrue(np.array_equal(out.f0 == 0, f0 == 0), 'unvoiced frames stay unvoiced')
        self.assertTrue(np.allclose(out.sp, sp))
        wider = vox.repitched(tr, 7, 1.06)
        self.assertTrue(np.all(wider.sp[:, 1:] <= sp[:, 1:] + 1e-12), 'the envelope moves up: each bin reads lower')
        self.assertTrue(np.array_equal(tr.f0, f0), 'the track it was given is left as it was')


class TestGlitchFill(unittest.TestCase):
    def test_swung_positions_match_the_kit_grid(self):
        """The fill's 16ths land where every phrase lane's do (phrase.Grid on K.SWING)."""
        g = Ph.Grid(K, 16, length=int(np.floor(16 * fx.S16 + 0.5)))
        for s in range(8):
            with self.subTest(step=s):
                self.assertLessEqual(abs(fx.swung(s) - g.at(s)), 1)
        pos = [fx.swung(s / 8) for s in range(8 * fx.LIFT_STEPS + 1)]
        self.assertTrue(all(b > a for a, b in zip(pos, pos[1:])))

    def test_half_a_bar_on_the_key(self):
        self.assertEqual(fx.swung(fx.LIFT_STEPS), int(np.floor(2 * K.BEAT * D.SR + 0.5)))
        notes = [m for _, _, (m, _), _, _ in fx.LIFT_MOTIF] + [m for _, m, _, _ in fx.LIFT_ROLL]
        self.assertTrue(all(in_key(m) for m in notes), notes)
        self.assertEqual(K.ROOT % 12, fx.F_S4 % 12)
        roll = [m for _, m, _, _ in fx.LIFT_ROLL]
        self.assertEqual(roll, sorted(roll))                     # the roll climbs
        cuts = [k for *_, k in fx.LIFT_ROLL]
        self.assertEqual(cuts, sorted(cuts))                     # and each 16th is cut finer
        spans = [(a, b) for a, b, *_ in fx.LIFT_MOTIF] + [(s, s + 1) for s, *_ in fx.LIFT_ROLL]
        self.assertEqual((0.0, fx.LIFT_STEPS), (spans[0][0], spans[-1][1]))
        self.assertTrue(all(b == c for (_, b), (c, _) in zip(spans, spans[1:])))

    def test_fill_parts_are_kit_items(self):
        names = {it.name for it in kits.items(K)}
        parts = {n for *_, ps, _ in fx.LIFT_MOTIF for n, _, _ in ps} | {'SnareMetal', 'ClosedHatFM', fx.LIFT_SWELL[0]}
        self.assertLessEqual(parts | {'ChromePluck'}, names)


if __name__ == '__main__':
    unittest.main()
