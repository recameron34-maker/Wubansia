"""FUT03 "Glass Current", round 4's context loop (recipes/fut03/context.py): what the brief and round 1 ask of it that the
QA can't see on its own. The loops are whole bars at 128 BPM on the kit's one grid and swing (0.56); every kick is the
same top layer; the loop plays the designer's picks (the rubber hook, bass B in the wet loop); the stab stays off the
kicks and the bass's accents; the vocal hook answers in bars 3-4; the club master's cap is declared and matches what
the recipe runs; the dry A/B plays at one loudness; and the low end passes its rule in both characters. The low-end
test renders the kick and the lines (no vocal models); nothing here needs the models."""
import unittest

import tools.sound  # noqa: F401  (pins numpy's kernels before numpy loads)
from tools.sound import dsp48 as D
from tools.sound import kits, qa
from tools.sound import master as MS
from tools.sound.kits import fut03 as K
from tools.sound.recipes.fut03 import chords, context as C, groove, rubber

SR = D.SR
R3 = ('KickSplit', 'Clap', 'ClosedHat', 'OpenHat', 'Shaker', 'MetalPling', 'DipBassClean', 'DipBassWarm', 'StabGm9',
      'GlassPluck', 'GlassPluckPhrase', 'SungLine1', 'VocoderDrop', 'LineBounceClean', 'VowelBassTalkClean')


def item(name):
    return kits.items(K, only=(name,))[0]


class TestFut03Context(unittest.TestCase):
    def test_the_loops_are_whole_bars(self):
        self.assertEqual(64, C.STEPS)
        self.assertEqual(int(round(64 * K.STEP * SR)), C.L)
        self.assertEqual(360000, C.L)
        self.assertEqual(32, C.DRUM_STEPS)

    def test_one_grid_and_one_swing(self):
        self.assertEqual(0.56, K.SWING)
        for g in (C.G, C.DG, rubber.G):
            self.assertEqual(K.SWING, g.swing)
        self.assertEqual(round(0.12 * C.G.s16), C.G.at(1) - round(C.G.s16))   # an odd 16th lands 14 ms late

    def test_every_kick_is_the_top_layer_on_every_beat(self):
        self.assertEqual([(s, 1.0) for s in range(0, 64, 4)], list(C.KICK))

    def test_it_plays_the_designers_picks(self):
        self.assertEqual({'clean': 'LineRubberClean', 'warm': 'LineRubberWarm'}, C.LINE)
        self.assertEqual('Rubber', rubber.PICK)
        self.assertEqual('warm', C.BASS_PICK)

    def test_the_stab_stays_off_the_kicks_and_the_bass_accents(self):
        """No stab on a kick (the pump would swallow it) and none on the hook's accents (velocity 1.0): a first version
        stacked the stab on the G1 accent on the "and" of beat 1, the loop's loudest moment."""
        accents = {s % 32 for s, _, _, v, _ in rubber.LINES[rubber.PICK] if v >= 1.0}
        for s, _ in C.STAB:
            self.assertTrue(s % 4, s)
            self.assertNotIn(s % 32, accents, s)
        self.assertEqual(len(chords.HITS) * 2, len(C.STAB))

    def test_the_hook_answers_in_the_second_half(self):
        self.assertEqual(32, C.HOOK_AT)
        self.assertTrue(all(s < 32 for s, _ in C.CANDY))

    def test_the_perc_is_its_preview_twice(self):
        self.assertEqual(tuple(groove.PERC), C.PERC[:len(groove.PERC)])
        self.assertEqual(tuple((s + 32, n, z, v) for s, n, z, v in groove.PERC), C.PERC[len(groove.PERC):])

    def test_the_club_cap_is_declared_and_run(self):
        """The cap over the standard's 4 dB is declared on the item with its reason, and the recipe passes the same
        number to the master; the genre is the kit's direction, with its own loudness and PLR floor."""
        it = item('ContextLoopClub')
        self.assertIn('club', it.tags)
        self.assertEqual(C.MASTER_REDUCTION_DB, it.qa['club_max_reduction_db'])
        self.assertTrue(4.0 < C.MASTER_REDUCTION_DB <= 5.0)
        self.assertTrue(it.qa['why'])
        self.assertIsNone(it.hp_hz)
        s = MS.settings(K, max_reduction_db=C.MASTER_REDUCTION_DB)
        self.assertEqual(('garage-house / deep tech house', -8.5, 7.0), (s.genre, s.target_lufs, s.plr_min))
        self.assertIn('r4', it.sets)

    def test_the_dry_a_b_plays_at_one_loudness(self):
        """A, louder at its own peak, comes down to B's loudness, which keeps its peak; inside the loops the two bass
        lanes are level-matched (A's fader up by what B reads over it), so the A/B is the character alone."""
        self.assertEqual(C.DRY_A_LUFS, item('ContextLoop').level)
        self.assertIsNone(item('ContextLoopWarm').level)
        a, b = (qa.lufs(C.bass_lane(c)) + C.BASS_FADER[c] for c in ('clean', 'warm'))
        self.assertLess(abs(a - b), 0.05)

    def test_the_r4_set_is_the_whole_round_4_kit(self):
        names = {it.name for it in kits.items(K, sets=('r4',))}
        for name in ('KickDeep', 'ClapSnare', 'ClosedHatDeep', 'OpenHatDeep', 'ShakerDeep', 'Conga', 'Bongo', 'Rimshot',
                     'RubberBassClean', 'RubberBassWarm', 'SubDeep', 'LineRubberClean', 'LineRubberWarm', 'DubStabGm9',
                     'PadWarm', 'VoxHook', 'ContextLoopClub', 'ContextLoop', 'ContextLoopWarm', 'ContextLoopWet',
                     'ContextLoopLow', 'ContextLoopLowWarm', 'BuildFill', 'Riser', 'Impact', 'KeysEP',
                     'PhraseRubberRiff', 'PhraseDubChords', 'PhraseKeysRiff', 'PhraseKeysArp'):
            self.assertIn(name, names)
        self.assertFalse(names & set(R3), names & set(R3))

    def test_the_sends_are_the_kits_own(self):
        """A 1.8 s plate with a 64th-note pre-delay (29 ms at 128 BPM) and a dotted-8th dub echo, darker than the shared
        one's top."""
        self.assertEqual(1.8, C.SPACE['t60'])
        self.assertAlmostEqual(K.STEP / 4, C.SPACE['predelay'])
        self.assertEqual(3, C.ECHO_STEPS)
        self.assertLessEqual(C.ECHO['lp_hz'], 5000.0)

    def test_the_low_end_passes_its_rule_in_both_characters(self):
        """The kick and bass lanes as the loop plays them: A under the strict gate in every gap (150-400 Hz 20 dB under
        30-120 Hz, 30 dB down before each kick), B under bass-body's -2 dB; every kick keeps the crest floor."""
        for char, limit in (('clean', -20.0), ('warm', -2.0)):
            out = C.context_low(None, char)
            rows = qa.loop_mud(out.audio, out.hits)
            self.assertLessEqual(max(r['vs_sub_db'] for r in rows), limit, char)
            if char == 'clean':
                self.assertLessEqual(max(r['before_hit_db'] for r in rows), -30.0)


if __name__ == '__main__':
    unittest.main()
