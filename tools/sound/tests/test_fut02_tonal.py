"""FUT02 "Orbit Relay" tonal parts: the written lines keep the round 1 rules (docs/plans/S1-four-kits.md) without
rendering a note. The basslines rest on every kick and clear before the next one, stay in D minor inside the bass-body
range and carry a slide, ghosts and held notes; the arp phrase and the sung lines stay in the key; the lines are 3-6
words; the vocoder's slices sit on the grid. Nothing here needs the vocal models."""
import unittest

import numpy as np

from tools.sound import dsp48 as D
from tools.sound import qa
from tools.sound.kits import fut02 as K
from tools.sound.recipes.fut02 import bass, lead, vox

SR = D.SR


def in_key(note):
    return note % 12 in K.SCALE


class TestFut02Basslines(unittest.TestCase):
    def test_two_lines_and_a_pick(self):
        self.assertEqual({'Orbit', 'Relay'}, set(bass.LINES))
        self.assertIn(bass.PICK, bass.LINES)
        self.assertEqual(tuple(s for s, _ in qa.GROOVES['4x4']), bass.KICK_STEPS)

    def test_in_key_and_in_the_bass_body_range(self):
        for name, line in bass.LINES.items():
            for step, note, length, vel, slide in line:
                with self.subTest(line=name, step=step):
                    self.assertTrue(in_key(note), D.note_name(note))
                    self.assertTrue(26 <= note <= 43, D.note_name(note))   # D1 to G2 (ADR 0011: notes up to G2)
                    self.assertTrue(0 < vel <= 1)
                    self.assertTrue(0 <= step < bass.LINE_STEPS)

    def test_rests_on_every_kick_and_clears_before_the_next(self):
        """No voice sounds over a kick's first 80 ms (the sub's swell starts 95 ms after it at the earliest), and each
        voice's sub has released 44 ms before the next kick (the before-hit window starts 40 ms before)."""
        L = bass.G.length
        kicks = list(bass.KICKS) + [bass.KICKS[0] + L]
        lead_n, rel = int(bass.SUB_LEAD * SR), int(bass.SUB_RELEASE * SR)
        for name in bass.LINES:
            for v in bass.voices(bass.LINES[name]):
                with self.subTest(line=name, on=v['on']):
                    k = max(p for p in kicks if p <= v['on'])
                    nxt = min(p for p in kicks if p > v['on'])
                    self.assertGreaterEqual(v['on'] - lead_n - k, int(qa.KICK_OWN_S * SR))
                    self.assertLessEqual(v['off'] + rel, nxt - int(0.044 * SR))

    def test_short_held_ghost_and_slide(self):
        for name, line in bass.LINES.items():
            with self.subTest(line=name):
                lengths = [ln for _, _, ln, _, _ in line]
                self.assertLessEqual(min(lengths), 0.5)                     # short notes
                self.assertGreaterEqual(max(lengths), 1.2)                  # held ones
                self.assertTrue(any(v <= 0.45 for _, _, _, v, _ in line))   # ghosts
                self.assertTrue(any(sl for *_, sl in line))                 # a slide

    def test_the_motif_comes_back(self):
        """Beats 1 and 2 of bar 2 start the way bar 1 does (the motif), and the bar is not a copy (the variation)."""
        for name, line in bass.LINES.items():
            with self.subTest(line=name):
                bar1 = [(s, m) for s, m, *_ in line if s < 16]
                bar2 = [(s - 16, m) for s, m, *_ in line if s >= 16]
                self.assertEqual([x for x in bar1 if x[0] < 4], [x for x in bar2 if x[0] < 4])
                self.assertNotEqual(bar1, bar2)


class TestFut02DropGlide(unittest.TestCase):
    def test_the_warm_glide_keeps_its_mids_through_the_dive(self):
        """B's mids stay within 6 dB of their start at 0.2 s (the first B was 22 dB down there: a bare sine dive on
        small speakers) and are gone 50 ms before the next kick's before-hit window."""
        tau, a, b = bass.WARM_MIDS
        n = int(K.BEAT * SR)
        env = np.exp(-np.arange(n) / SR / tau) * D.taper(n, a, b)
        self.assertGreater(20 * np.log10(env[int(0.2 * SR)]), -6.0)
        self.assertLessEqual(a + b, K.BEAT - qa.BEFORE_HIT[0] - 0.05)
        self.assertFalse(np.any(env[int((a + b) * SR) + 1:]))


class TestFut02Lead(unittest.TestCase):
    def test_phrase_in_key_on_the_grid(self):
        notes = lead.phrase_notes()
        self.assertEqual(sorted(s for s, *_ in notes), [s for s, *_ in notes])
        for step, note, length, vel in notes:
            with self.subTest(step=step):
                self.assertTrue(in_key(note), D.note_name(note))
                self.assertTrue(0 <= step < lead.PHRASE_STEPS)
                self.assertTrue(0 < vel <= 1)
        self.assertGreaterEqual(len(notes), 28)                             # a 16th arpeggio
        self.assertTrue(all(in_key(n) for n in lead.ARP_NOTES))

    def test_bar_two_answers_bar_one(self):
        mel = [m for _, m, _ in lead.MELODY]
        self.assertEqual(mel[:3], mel[6:9])                                  # the motif comes back
        self.assertNotEqual(mel[3:6], mel[9:12])                             # and ends another way


class TestFut02Vox(unittest.TestCase):
    def test_three_plain_lines_in_key(self):
        self.assertEqual(3, len(vox.LINES))
        self.assertIn(vox.PICK, vox.LINE_BY_NAME)
        for ln in vox.LINES:
            with self.subTest(line=ln.name):
                self.assertTrue(3 <= len(ln.text.split()) <= 6, ln.text)
                self.assertTrue(all(in_key(m) for _, _, m, _ in ln.notes))
                self.assertEqual([w.lower() for w in ln.text.split()],
                                 [w for *_, w in ln.notes if w != '~'])

    def test_six_to_eight_chops_from_the_pick(self):
        self.assertTrue(6 <= len(vox.PICK_CHOPS) <= 8)
        syllables = sum(1 for *_, w in vox.LINE_BY_NAME[vox.PICK].notes)
        for name, first, last, words, root, _ in vox.PICK_CHOPS:
            with self.subTest(chop=name):
                self.assertTrue(0 <= first <= last < syllables)
                self.assertTrue(in_key(root))

    def test_vocoder_slices_on_the_grid(self):
        self.assertEqual(len(vox.ROBOT_SYLLABLES), len(vox.ROBOT_GRID))
        self.assertTrue(2 <= len(vox.ROBOT.split()) <= 4)
        onsets = [n for _, n, _ in vox.ROBOT_SLICES]
        self.assertEqual(sorted(onsets), onsets)
        for (step, _), n in zip(vox.ROBOT_GRID, onsets):
            want = (vox.ROBOT_PRE + vox.swung(step) * K.STEP - vox.ROBOT_LEAD) * SR
            self.assertLessEqual(abs(n - want), 0.5)
        self.assertTrue(all(step + ln <= 16 for step, ln in vox.ROBOT_GRID))  # one bar


if __name__ == '__main__':
    unittest.main()
