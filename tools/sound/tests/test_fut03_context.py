"""FUT03 "Glass Current", the context loop (recipes/fut03/context.py): what the brief and round 1 ask of it that the QA
can't see on its own. The loops are whole bars at 128 BPM; every lane sits on the kit's one grid and swing; every kick
is the same top layer; the loop plays the designers' picks (the line, the chops' line); the bass rests where the vowel
bass answers, once, on a kick, in the loop's character; the stab leaves the kicks to the pump; the fill's second half
leaves the robot's words clear; the pluck plays the signature phrase's own notes; the resonator rings in tune and in
key; the A/B lanes are level-matched; and the low end of both characters passes its rule with the files played exactly
as written (no second sub-sonic high-pass)."""
import unittest

import numpy as np

import tools.sound  # noqa: F401  (pins numpy's kernels before numpy loads)
from tools.sound import dsp48 as D
from tools.sound import kits, phrase, qa

K = kits.load('FUT03')


def item(name):
    return kits.items(K, only=(name,))[0]


class TestFut03Context(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        from tools.sound.recipes.fut03 import context
        cls.C = context

    def test_the_loops_are_whole_bars(self):
        bar = 4 * 60.0 / K.BPM * D.SR
        self.assertEqual(round(4 * bar), self.C.L)
        self.assertEqual(360000, self.C.L)
        self.assertEqual(round(2 * bar), self.C.DG.length)
        self.assertEqual(self.C.G.steps, 64)

    def test_one_grid_and_one_swing(self):
        C = self.C
        self.assertEqual(K.SWING, C.G.swing)
        self.assertEqual(K.SWING, C.DG.swing)
        self.assertEqual(C.G.at(1) - C.G.at(0), int(np.floor((2 * K.SWING) * K.STEP * D.SR + 0.5)))
        self.assertTrue(all(s % 4 == 0 for s, _ in C.KICK))
        self.assertNotIn(C.DROP_OUT, [s for s, _ in C.KICK])          # the drop-out before the downbeat
        self.assertEqual(15, len(C.KICK))
        self.assertEqual({1.0}, {v for _, v in C.KICK})               # every beat on the top layer: no strong-weak
        self.assertFalse([s for s, _, _ in C.HATS if s >= C.DROP_OUT])
        self.assertFalse([s for s, _ in C.STAB if s >= C.DROP_OUT])

    def test_it_plays_the_designers_picks(self):
        from tools.sound.recipes.fut03 import bass, fx, vox
        C = self.C
        self.assertEqual(bass.PICK, C.LINE_NAME)
        for name in C.LINE.values():
            self.assertEqual('loop', item(name).category)
        self.assertEqual({'clean', 'warm'}, set(C.LINE))
        self.assertIn(C.BASS_PICK, C.LINE)
        self.assertIn(C.CHOP[1], vox.CHOP_BY_NAME)                     # a chop of the picked line
        self.assertEqual('VocoderDrop', C.ROBOT[1])
        self.assertEqual('BuildFill', C.FILL[1])
        self.assertEqual((3 * 16 + 8, 8), (C.FILL[0], C.FILL[2]))     # bar 4, beat 3: the fill's second half...
        self.assertEqual(C.L, C.G.at(C.FILL[0]) + fx.BAR - fx.swung(C.FILL[2]))   # ...ending on the downbeat

    def test_the_pluck_plays_the_phrase_and_answers_the_voice(self):
        from tools.sound.recipes.fut03 import lead
        C = self.C
        notes = set(lead.PHRASE)
        for s, m, ln, v in C.PLUCK:
            self.assertIn((s % 32, m, ln, v), notes)
        self.assertEqual(list(lead.PHRASE), [n for n in C.PLUCK if n[0] < 32])   # bars 1-2: the phrase whole
        voice = [(C.CHOP[0], C.CHOP[0] + 8), (C.ROBOT[0], C.ROBOT[0] + 11)]      # the voice's half bars
        for s, _, ln, _ in C.PLUCK:
            for a, b in voice:
                self.assertFalse(a <= s < b, s)                        # the pluck rests while the voice sings

    def test_the_vowel_bass_answers_once_where_the_line_rests(self):
        from tools.sound.recipes.fut03 import bass
        C = self.C
        step, names = C.VOWEL
        self.assertIn(step, [s for s, _ in C.KICK])                    # it starts on a kick, as its groove check plays
        self.assertEqual({'clean': 'gate', 'warm': 'bass-body'}, {c: item(n).qa.get('mud', 'gate')
                                                                  for c, n in names.items()})
        self.assertTrue(all(item(n).category == 'bass' for n in names.values()))
        a, b = C.BASS_REST
        rested = [n for n in bass.LINES[C.LINE_NAME] if a <= n[0] < b]
        self.assertEqual([25, 29, 30], [n[0] for n in rested])
        self.assertTrue(step < a < b <= step + 8)
        # the cut lands in the line's own silence
        for char in C.LINE:
            y = C.bass_lane(char)
            raw = phrase.kit_files(K, C.LINE[char])[1][(None, 1, 1)].audio
            cut = C.G.at(a) - int(C.REST_LEAD * D.SR)
            near = np.abs(D.st(raw)[cut - 240:cut + 240]).max() / np.abs(raw).max()
            self.assertLess(20 * np.log10(near + 1e-30), -95.0, char)       # silence: no fade needed
            self.assertFalse(np.any(y[cut:C.G.at(b)]))
            np.testing.assert_array_equal(y[C.G.at(b):], np.concatenate([D.st(raw), D.st(raw)])[C.G.at(b):])

    def test_the_stab_leaves_the_kicks_to_the_pump(self):
        """A 176 ms stab under a 9 dB pump is swallowed on the kick (the review: 11-18 dB under the mix): the loop plays
        the designer's push without its on-kick hits."""
        from tools.sound.recipes.fut03 import pad
        C = self.C
        kicks = {s for s, _ in C.KICK} | {C.DROP_OUT}
        self.assertFalse([s for s, _ in C.STAB if s in kicks or s % 4 == 0])
        self.assertEqual([3, 6, 10, 13, 19, 22, 27, 30], [s for s, _ in C.STAB if s < 32])
        self.assertLessEqual({(s % 32, v) for s, v in C.STAB}, set(pad.PUMP_HITS))

    def test_the_fill_leaves_the_robot_its_words(self):
        """The robot's syllables before "drop" land on no fill hit: the fill plays from its 16th 8, on "drop"."""
        from tools.sound.recipes.fut03 import fx, vox
        C = self.C
        fill_steps = {C.FILL[0] - C.FILL[2] + s for s, _, _ in fx.FILL if s >= C.FILL[2]}
        robot_steps = [C.ROBOT[0] + s for s, _ in vox.ROBOT_GRID]
        self.assertFalse(set(robot_steps[:-1]) & {int(s) for s in fill_steps})
        self.assertEqual(C.FILL[0], robot_steps[-1])                  # the fill's 16ths start with "drop"
        self.assertGreater(C.LANES['robot'][1], -1.0)                 # 3 dB over the first fader

    def test_the_a_and_b_lanes_are_level_matched(self):
        C = self.C
        for key in C.CHARACTER_LANES:
            a, b = (qa.lufs(C.lane(key, c)) for c in ('clean', 'warm'))
            self.assertLess(abs(a - b), 0.1, key)

    def test_everything_tonal_is_in_the_key(self):
        from tools.sound.recipes.fut03 import perc
        C = self.C
        scale = set(K.SCALE)
        for _, note, _ in C.PERC:
            self.assertIn(note, perc.PLING_NOTES)
        for f in C.resonator_freqs():
            m = 69 + 12 * np.log2(f / 440.0)
            self.assertLess(abs(m - round(m)), 0.01, f)                # in tune within 1 cent
            self.assertIn(int(round(m)) % 12, scale, f)

    def test_the_loops_play_the_files_as_written(self):
        """No renderer high-pass on the loops (their files had it once); a zero-phase DC clean instead."""
        for name in ('ContextLoop', 'ContextLoopWarm', 'ContextLoopWet', 'ContextLoopLow', 'ContextLoopLowWarm',
                     'DrumLoop', 'FillPreview'):
            it = item(name)
            self.assertIsNone(it.hp_hz, name)
            self.assertTrue(it.loop, name)
            self.assertIn('r2', it.sets, name)
        self.assertEqual('gate', item('ContextLoopLow').qa['mud'])
        self.assertEqual('bass-body', item('ContextLoopLowWarm').qa['mud'])
        self.assertIn('with_kick', item('ContextLoopLow').tags)
        self.assertIn('wet', item('ContextLoopWet').tags)

    def test_the_low_end_passes_its_rule_in_both_characters(self):
        C = self.C
        for name, limit in (('ContextLoopLow', qa.MUD_VS_SUB_DB), ('ContextLoopLowWarm', qa.BODY_VS_SUB_DB)):
            it = item(name)
            t = it.takes('FUT03')[0]
            out = it.render(t)
            self.assertEqual(C.L, len(out.audio))
            self.assertEqual(tuple(C.kicks()), tuple(out.hits))
            checks = {c.name: c for c in qa.check_take(out.audio, t.spec(out))}
            for key in ('dc', 'seam', 'low_side', 'mud_share', 'loop_mud_vs_sub', 'loop_body_vs_sub',
                        'loop_mud_before_hit', 'body_over_saw', 'body_share'):
                if key in checks and checks[key].status != 'n/a':
                    self.assertEqual('pass', checks[key].status, f'{name} {key} {checks[key].value}')
            worst = max(r['vs_sub_db'] for r in qa.loop_mud(out.audio, sorted(out.hits)))
            self.assertLess(worst, limit - 2.0, name)                  # with a margin of at least 2 dB


if __name__ == '__main__':
    unittest.main()
