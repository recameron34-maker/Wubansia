"""FUT01's round 2 context loop (recipes/fut01/context.py), the music it keeps (music.py) and the listening previews
(preview.py), as written: 4 bars, every lane on a round 2 file, the picked bassline with room on every kick of the
loop's own pattern and nothing but ghosts on a loud kick's tail, its top keyed by the pluck, the chops of the new line
on its swing and clear of release titles, the drums stopping for one beat for the fill's roll, which ends on the
downbeat. No audio is rendered here: the loops' sound is checked by render.py's QA."""
import unittest

from tools.sound import dsp48 as D
from tools.sound import kits
from tools.sound import phrase as Ph
from tools.sound import qa
from tools.sound.kits import fut01 as K
from tools.sound.recipes.fut01 import basslines as B
from tools.sound.recipes.fut01 import context as C
from tools.sound.recipes.fut01 import fx, music, preview, vox
from tools.sound.tests.test_fut01_basslines import on_a_kick_tail

SR = D.SR
LOOPS = ('ContextLoop', 'ContextLoopWarm', 'ContextLoopWet', 'ContextLoopLow', 'ContextLoopLowWarm')
KEPT_MUSIC = ('AirPadFsm9', 'ChromePluck', 'ChromePluckPhrase')


class TestContextLoop(unittest.TestCase):
    def setUp(self):
        self.r2 = {it.name: it for it in kits.items(K, sets=('r2',))}

    def test_four_bars(self):
        self.assertEqual(343880, C.L)
        self.assertLess(abs(C.L - 16 * K.BEAT * SR), 1.0)
        self.assertEqual(64, C.G.steps)
        self.assertEqual(C.L, 2 * B.G.length)        # the 2-bar line and the pluck phrase tile it exactly

    def test_every_lane_plays_a_round_2_file(self):
        names = [v[0] for k, v in C.LANES.items() if v[0]] + list(C.REESE.values()) + [n for _, n, _ in C.CHOPS]
        for name in names:
            with self.subTest(name=name):
                self.assertIn(name, self.r2)
        for _, name, _ in C.CHOPS:
            self.assertIn(name, vox.PICK_CHOP_BY_NAME, 'a chop of the new line, not round 1\'s')

    def test_the_picked_line_and_its_characters(self):
        self.assertEqual(B.PICK, C.LINE)
        self.assertIn(C.WET_BASS, C.REESE)
        self.assertEqual({'clean', 'warm'}, set(C.REESE))

    def test_the_drums_stop_for_the_fill_and_it_ends_on_the_downbeat(self):
        """The drums stop for one beat, as in round 1, for the fill's second beat (its roll and swell), which ends on
        the loop's downbeat; bar 4 keeps bar 2's kicks, the and-of-3 kick that "floor" lands on among them."""
        for part in (C.KICK, C.SNARE, C.HAT, C.PERC):
            self.assertTrue(all(s < C.FILL_AT for s, _ in part))
        self.assertEqual(C.L, C.G.at(C.FILL_AT) + fx.swung(fx.LIFT_STEPS) - fx.swung(C.FILL_FROM))
        self.assertEqual(0, C.FILL_AT % 4, 'the fill starts on a beat')
        self.assertEqual(C.STEPS - 4, C.FILL_AT, 'one beat of drum stop')
        self.assertEqual(fx.LIFT_STEPS - 4, C.FILL_FROM, "the fill's last beat")
        bar = lambda b: [(s - 16 * b, v) for s, v in C.KICK if 16 * b <= s < 16 * b + 16]
        self.assertEqual(bar(1), bar(3))
        self.assertIn((58, 0.8), C.KICK)
        self.assertEqual({58, 59}, {s for s, _ in C.HAT if s >= 56}, "bar 4's hats until the stop")

    def test_nothing_but_ghosts_on_a_loud_kick_s_tail(self):
        line = tuple((s + r * B.STEPS, *rest) for r in range(2) for s, *rest in B.LINES[C.LINE])
        self.assertEqual([], on_a_kick_tail(line, C.KICK, C.G))

    def test_the_bass_top_ducks_under_every_pluck_note(self):
        onsets = C.pluck_onsets()
        self.assertEqual(len(C.PLUCK_AT) * len(music.PHRASE), len(onsets))
        self.assertEqual(sorted(onsets), sorted(C.G.at(a + s) for a in C.PLUCK_AT for s, *_ in music.PHRASE))
        self.assertGreaterEqual(C.PLUCK_DUCK['depth_db'], 6.0)
        self.assertGreaterEqual(C.PLUCK_DUCK['ramp'] + C.PLUCK_DUCK['hold'], 0.040, "the pluck's first 40 ms")
        self.assertLessEqual(C.PLUCK_DUCK['ramp'], 0.003, 'down before the reese mids rise (3 ms after the note)')
        self.assertGreaterEqual(C.PLUCK_XOVER, 1000.0, 'the low end and the body pass untouched')

    def test_the_line_leaves_every_kick_room(self):
        """The picked line, played twice against the loop's own 4-bar kicks: no note starts inside a kick's first
        80 ms, and every release has ended before the next kick's before-hit window (40-15 ms)."""
        kicks = sorted(C.G.at(s) for s, _ in C.KICK)
        line = tuple((s + r * B.STEPS, *rest) for r in range(2) for s, *rest in B.LINES[C.LINE])
        for on, _, n_on in B.voices(line, C.G):
            with self.subTest(onset=on):
                before = max(k for k in kicks + [kicks[0] - C.L] if k <= on)
                after = min(k for k in kicks + [kicks[0] + C.L] if k > on)
                self.assertGreaterEqual(on - before, int(qa.KICK_OWN_S * SR))
                self.assertLessEqual(on + n_on + int(B.RELEASE * SR), after - int(qa.BEFORE_HIT[0] * SR))

    def test_chops_land_on_the_line_s_swing(self):
        """Each chop's first note sits on a 16th of the same parity as in the sung line, so the swing inside the chop
        lines up with the grid; "floor" lands on the and of 3 of bar 4, where the line sings it in its bar."""
        notes = vox.LINE_BY_NAME[vox.PICK].notes
        words = [(s, w) for s, _, _, w in notes if w != '~']
        for step, name, _ in C.CHOPS:
            first = vox.PICK_CHOP_BY_NAME[name][1]
            with self.subTest(chop=name):
                self.assertEqual(words[first][0] % 2, step % 2)
        step, name, _ = C.CHOPS[-1]
        first = vox.PICK_CHOP_BY_NAME[name][1]
        floor = dict((w, s) for s, w in words)['floor']
        self.assertEqual(48 + floor, step + floor - words[first][0])

    def test_every_chop_lands_floor_on_an_and_of_3_kick(self):
        notes = vox.LINE_BY_NAME[vox.PICK].notes
        words = [(s, w) for s, _, _, w in notes if w != '~']
        floor = dict((w, s) for s, w in words)['floor']
        kicks = {s for s, v in C.KICK if v >= 0.8}
        for step, name, _ in C.CHOPS:
            first = vox.PICK_CHOP_BY_NAME[name][1]
            with self.subTest(chop=name):
                at = step + floor - words[first][0]
                self.assertIn(at, kicks)
                self.assertEqual(10, at % 16, 'the and of 3')

    def test_the_loop_sings_no_release_title(self):
        """The loop's chops sing pieces of the line that the title check kept (vox.CHOP_CHECKS); round 2's first loop
        sang two titles of released singles."""
        for _, name, _ in C.CHOPS:
            words = vox.PICK_CHOP_BY_NAME[name][3]
            with self.subTest(chop=name):
                if ' ' in words:
                    self.assertTrue(vox.CHOP_CHECKS[words].startswith('kept'), words)

    def test_the_loop_items(self):
        for name in LOOPS:
            with self.subTest(name=name):
                it = self.r2[name]
                self.assertTrue(it.loop)
                self.assertEqual('loop', it.category)
                self.assertNotIn('s1a', it.sets)
        self.assertEqual('gate', self.r2['ContextLoopLow'].qa['mud'])
        self.assertEqual('bass-body', self.r2['ContextLoopLowWarm'].qa['mud'])
        for name in ('ContextLoopLow', 'ContextLoopLowWarm'):
            self.assertIn('with_kick', self.r2[name].tags)
        self.assertEqual({C.LOOP_LUFS}, {self.r2[n].level for n in LOOPS[:3]}, 'dry A, dry B and wet at one loudness')

    def test_the_music_comes_back_unchanged(self):
        s1a = {it.name for it in kits.items(K, sets=('s1a',))}
        for name in KEPT_MUSIC:
            with self.subTest(name=name):
                self.assertIn(name, self.r2)
                self.assertIn(name, s1a)


class TestPreviews(unittest.TestCase):
    def test_previews_are_tagged_and_in_r2_only(self):
        for it in preview.ITEMS:
            with self.subTest(name=it.name):
                self.assertIn('preview', it.tags)
                self.assertEqual('loop', it.category)
                self.assertEqual(('r2',), it.sets)

    def test_hat_patterns_play_the_loop_s_hat_part(self):
        self.assertEqual(tuple((s, v) for s, v in C.HAT if s < 32), preview.HATS)
        self.assertEqual(Ph.loop_len(K, 32), preview.G2.length)
        hats = [it for it in preview.ITEMS if it.name.startswith('HatPattern')]
        self.assertEqual(2, len(hats))
        self.assertEqual(1, len({it.level for it in hats}), 'the A/B at one loudness')

    def test_the_drop_is_the_picked_bassline(self):
        self.assertIn(preview.DROP_BASS, {it.name for it in B.ITEMS})


if __name__ == '__main__':
    unittest.main()
