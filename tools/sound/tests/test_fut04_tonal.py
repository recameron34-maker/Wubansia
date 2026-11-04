"""FUT04's tonal recipes (recipes/fut04/bass.py, lead.py, vox.py): what the written parts promise, checked without
rendering (and without the vocal models).

- The acid lines: in E minor and in the bass-body range (E1-G2), resting under every kick (no note in a kick's first
  80 ms; only a squelch tick, with no sub and no body, on a kick's 16th), silent ROOM before the next kick, each with
  accents, ghosts, a slide, short and held notes, and a motif that comes back varied; the pick holds two notes for most
  of a beat and doesn't play one cell on every beat; no slide crosses the sub octave split; the sub holds the kick's
  note in phase with each kick under the pedal; the pick is one of them.
- The blip sequence: in E minor, inside the multisample zones.
- The vocals: three candidate lines of 3-6 plain words, the robot 2-4, all clean and in key, none a title a release
  uses; the chops come from the pick, the robot's slices sit on the kit's grid.
"""
import unittest

import numpy as np

from tools.sound import denylist
from tools.sound import dsp48 as D
from tools.sound import qa
from tools.sound.kits import fut04 as K
from tools.sound.recipes.fut04 import bass as B
from tools.sound.recipes.fut04 import lead as Ld
from tools.sound.recipes.fut04 import vox as X

SR = D.SR


def in_key(midi):
    return midi % 12 in K.SCALE      # the kit's pitch classes: E F# G A B C D


class TestAcidLines(unittest.TestCase):
    def test_two_lines_and_a_pick(self):
        self.assertEqual(2, len(B.LINES))
        self.assertIn(B.PICK, B.LINE_BY_NAME)
        self.assertNotEqual(*(tuple(n[:2] for n in ln.notes) for ln in B.LINES))

    def test_in_key_and_in_the_bass_body_range(self):
        for ln in B.LINES:
            for step, note, length, vel, slide in ln.notes:
                with self.subTest(line=ln.name, step=step):
                    self.assertTrue(in_key(note), D.note_name(note))
                    self.assertTrue(28 <= note <= 43, D.note_name(note))   # E1 to G2 (ADR 0011's range)
                    self.assertTrue(0 <= step < B.LOOP_STEPS)

    def test_room_on_every_kick(self):
        """No note sounds in a kick's own 80 ms, and every note is silent ROOM before the next kick."""
        own = int(qa.KICK_OWN_S * SR)
        for ln in B.LINES:
            for on, end, note, vel, slide in B.note_spans(ln):
                with self.subTest(line=ln.name, on=on):
                    self.assertFalse(any(k <= on < k + own for k in B.KICKS), 'a note starts under a kick')
                    self.assertLessEqual(end + int(B.NOTE_END * SR), B.next_kick(on) - int(B.ROOM * SR))

    def test_each_line_grooves(self):
        """Accents, ghosts, at least one slide, short and held notes; the leap motif comes back."""
        for ln in B.LINES:
            kinds = {B.kind(v) for _, _, _, v, _ in ln.notes}
            lengths = [n[2] for n in ln.notes]
            with self.subTest(line=ln.name):
                self.assertEqual({'accent', 'normal', 'ghost'}, kinds)
                self.assertTrue(any(n[4] for n in ln.notes))
                self.assertLessEqual(min(lengths), 0.5)
                self.assertGreaterEqual(max(lengths), 1.4)
                bar1 = [(n[0] % 16, n[1]) for n in ln.notes if n[0] < 16]
                bar2 = [(n[0] % 16, n[1]) for n in ln.notes if n[0] >= 16]
                self.assertEqual([n for _, n in bar1[:2]], [n for _, n in bar2[:2]])   # the motif comes back...
                self.assertNotEqual(bar1, bar2)                                       # ...varied, not copied

    def test_ticks_play_the_squelch_alone_on_a_kick(self):
        """A tick sits on a kick's 16th and plays only the voice above SPLIT: no sub or body gate starts in a kick's
        first 80 ms."""
        own = int(qa.KICK_OWN_S * SR)
        for ln in B.LINES:
            for step, note, length, vel in ln.ticks:
                with self.subTest(line=ln.name, tick=step):
                    self.assertIn(B.G.at(step), B.KICKS)
                    self.assertTrue(in_key(note) and 28 <= note <= 43)
                    self.assertEqual('ghost', B.kind(vel))
            p = B.line_parts(ln.name)
            for on, end, _ in p.low:
                self.assertFalse(any(k <= on < k + own for k in B.KICKS), (ln.name, on))
            self.assertEqual(len(p.groups) - len(p.low), len(ln.ticks))

    def test_the_pick_holds_and_breaks_its_cell(self):
        """Round 2's lines must mix short and held notes: the pick holds two notes for most of a beat (200 ms or more,
        a dotted 8th at 136 BPM), and its beats don't all play the same rhythm."""
        ln = B.LINE_BY_NAME[B.PICK]
        gates = [(end - on) / SR for on, end, *_ in B.note_spans(ln)]
        self.assertGreaterEqual(sum(g >= 0.2 for g in gates), 2)
        self.assertGreaterEqual(len(ln.ticks), 1)
        cells = {tuple(sorted({e[0] % 4 for e in B.events(ln) if e[0] // 4 == b})) for b in range(B.LOOP_STEPS // 4)}
        self.assertGreaterEqual(len(cells), 3)

    def test_the_sub_pedal_holds_the_kick_note_in_phase(self):
        """A note starting within SUB_PEDAL_S of a kick plays its sine on the kick's note, its phase counted from that
        kick (PEDAL_PHASE); every other sine on the kick's note does too."""
        for ln in B.LINES:
            p = B.line_parts(ln.name)
            t = np.arange(len(p.sub_freq))
            for on, end, ped in p.low:
                span = slice(on, end)
                k = B.last_kick(on)
                with self.subTest(line=ln.name, on=on):
                    self.assertEqual(ped, on - k < int(B.SUB_PEDAL_S * SR))
                    if ped:
                        self.assertTrue(np.all(p.sub_freq[span] == B.F_KICK))
                    if np.all(p.sub_freq[span] == B.F_KICK):
                        want = B.F_KICK * (t[span] - k) / SR + B.PEDAL_PHASE / 360.0
                        self.assertTrue(np.allclose(p.sub_cycles[span], want))
        self.assertEqual(B.KICKS, tuple(sorted(B.KICKS)))

    def test_no_slide_crosses_the_sub_octave_split(self):
        for ln in B.LINES:
            for prev, cur in zip(ln.notes, ln.notes[1:]):
                if cur[4]:
                    with self.subTest(line=ln.name, step=cur[0]):
                        self.assertEqual(prev[1] >= B.SUB_OCTAVE_FROM, cur[1] >= B.SUB_OCTAVE_FROM)

    def test_kicks_are_four_on_the_floor(self):
        self.assertEqual(tuple(B.G.at(4 * k) for k in range(8)), B.KICKS)

    def test_the_characters(self):
        items = {it.name: it for it in B.ITEMS}
        for name in ('AcidWarm', 'AcidLine1Warm', 'AcidLine2Warm'):
            self.assertEqual('bass-body', items[name].qa['mud'])
        for name in ('AcidClean', 'AcidLine1Clean', 'AcidLine2Clean', 'Rumble'):
            self.assertEqual('gate', items[name].qa.get('mud', 'gate'))
        self.assertTrue(all('r2' in it.sets for it in B.ITEMS))


class TestBlips(unittest.TestCase):
    def test_sequence_in_key_and_in_the_zones(self):
        for step, note, length, vel, ratchet in Ld.PHRASE:
            with self.subTest(step=step):
                self.assertTrue(in_key(note), D.note_name(note))
                self.assertTrue(Ld.BLIP_NOTES[0] - 1.5 <= note <= Ld.BLIP_NOTES[-1] + 1.5)
                self.assertTrue(0 <= step < Ld.PHRASE_STEPS and ratchet in (1, 2, 3))

    def test_zones_every_three_semitones(self):
        self.assertEqual({3}, {b - a for a, b in zip(Ld.BLIP_NOTES, Ld.BLIP_NOTES[1:])})


class TestVocals(unittest.TestCase):
    def test_three_plain_clean_lines(self):
        bad = denylist.load('profanity')
        self.assertEqual(3, len(X.LINES))
        self.assertIn(X.PICK, X.LINE_BY_NAME)
        for ln in X.LINES:
            with self.subTest(line=ln.name):
                self.assertTrue(3 <= len(ln.text.split()) <= 6)
                self.assertEqual([], denylist.hits(ln.text, bad))
                self.assertTrue(all(in_key(m) for _, _, m, _ in ln.notes))
                self.assertEqual([w.lower() for w in ln.text.split()],
                                 [w for _, _, _, w in ln.notes if w != '~'])

    def test_no_line_is_a_title_a_release_uses(self):
        clash = {t.lower() for t in X.TITLE_CLASHES}
        lines = [ln.text for ln in X.LINES] + [X.ROBOT] + [c[3] for c in X.PICK_CHOPS]
        self.assertEqual([], [w for w in lines if w.lower() in clash])

    def test_robot(self):
        self.assertTrue(2 <= len(X.ROBOT.split()) <= 4)
        self.assertEqual(X.ROBOT_ITEM, 'Robot' + ''.join(w.title() for w in X.ROBOT.split()))
        self.assertEqual([X.ROBOT_ITEM, X.ROBOT_ITEM + 'Wet'], [it.name for it in X.ITEMS if 'robot' in it.tags])
        self.assertEqual([], denylist.hits(X.ROBOT, denylist.load('profanity')))
        self.assertEqual(len(X.ROBOT_SYLLABLES), len(X.ROBOT_GRID))
        self.assertTrue(in_key(X.ROBOT_NOTE))
        onsets = [n for _, n, _ in X.ROBOT_SLICES]
        self.assertEqual(sorted(onsets), onsets)
        for (step, n, _), (s, _) in zip(X.ROBOT_SLICES, X.ROBOT_GRID):
            self.assertEqual(s, step)
            on = X.ROBOT_PRE + (s + (2 * K.SWING - 1) * (s % 2)) * K.STEP    # the step on the kit's swing
            self.assertAlmostEqual(on - X.ROBOT_LEAD, n / SR, delta=1.0 / SR)

    def test_chops_come_from_the_pick(self):
        self.assertTrue(6 <= len(X.PICK_CHOPS) <= 8)
        for name, first, last, words, root, _ in X.PICK_CHOPS:
            with self.subTest(chop=name):
                self.assertTrue(0 <= first <= last <= 5)        # come down to the fur nace: six syllables
                self.assertTrue(in_key(root))
                self.assertIn(words.split()[0], X.LINE_BY_NAME[X.PICK].text.lower().split() + ['fur'])


if __name__ == '__main__':
    unittest.main()
