"""FUT03's round 4 tonal parts (recipes/fut03/rubber.py, chords.py, accents.py, hook.py): the rubber lines stay in the
key, leave the kick its room and come back to their motif, A and B share one sub on the written notes, the chord is
voiced so its root reads alone, the pad joins on whole cycles, and the vocal hook uses only the kit's existing,
title-checked line. The line tests render the lines (a few seconds, no vocal models)."""
import unittest

import tools.sound  # noqa: F401  (the SIMD pins, before numpy)
import numpy as np

from tools.sound import dsp48 as D
from tools.sound import kits, phrase, qa
from tools.sound.kits import fut03 as K
from tools.sound.recipes.fut03 import accents as A
from tools.sound.recipes.fut03 import chords as Ch
from tools.sound.recipes.fut03 import hook as H
from tools.sound.recipes.fut03 import rubber as B
from tools.sound.recipes.fut03 import vox as X

SR = D.SR


def in_key(note):
    return int(note) % 12 in K.SCALE


class TestRubberLines(unittest.TestCase):
    def test_two_lines_a_pick_and_a_phrase(self):
        self.assertEqual({'Rubber', 'Deep'}, set(B.LINES))
        self.assertEqual('Rubber', B.PICK)
        self.assertEqual({'Riff'}, set(B.PHRASES))
        self.assertFalse(set(B.LINES) & set(B.PHRASES))

    def test_every_note_and_slide_is_in_the_key_and_the_bass_range(self):
        for name, notes in B.NOTES.items():
            for s, ln, m, v, sl in notes:
                self.assertTrue(in_key(m) and 29 <= m <= 43, (name, s, m))
                if sl:
                    self.assertTrue(in_key(sl[0]) and 29 <= sl[0] <= 43, (name, s, sl))

    def test_room_on_every_kick(self):
        """Every note starts at least 100 ms after the kick before it and is released (its 25 ms tail included) at least
        40 ms before the next one: the kick has its own room, and the gate's before-hit window is quiet."""
        for name, notes in B.NOTES.items():
            for s, ln, m, v, sl in notes:
                start = B.G.at(s) / SR
                kick = max(k for k in B.KICKS if k <= s) * B.G.s16 / SR
                nxt = min(k for k in B.KICKS + (B.LOOP_STEPS,) if k > s) * B.G.s16 / SR
                end = start + ln * B.G.s16 / SR + B.SUB_RELEASE
                self.assertGreaterEqual(start - kick, 0.100, (name, s))
                self.assertGreaterEqual(nxt - end, 0.040, (name, s))

    def test_one_note_at_a_time(self):
        for name, notes in B.NOTES.items():
            for a, b in zip(notes, notes[1:]):
                self.assertLessEqual(B.G.at(a[0]) + a[1] * B.G.s16, B.G.at(b[0]) + 1, (name, a, b))

    def test_short_held_ghosts_and_slides(self):
        """Round 1's line rules: short and held notes, ghosts, an occasional slide; the hook's motif comes back varied in
        bar 2 (the G1 accent on the "and" of beat 1 and its octave pop, then other notes)."""
        for name, notes in B.LINES.items():
            lens = [ln for _, ln, *_ in notes]
            self.assertTrue(min(lens) < 0.5 and max(lens) >= 1.3, name)
            self.assertTrue(any(v < B.GHOST_VEL for *_, v, _ in notes), name)
            self.assertTrue(any(sl for *_, sl in notes), name)
        rubber = B.LINES['Rubber']
        bar1 = [(s, m) for s, _, m, *_ in rubber if s < 16]
        bar2 = [(s - 16, m) for s, _, m, *_ in rubber if s >= 16]
        self.assertIn((2, 31), bar1)
        self.assertIn((2, 31), bar2)
        self.assertNotEqual(bar1, bar2)

    def test_a_and_b_share_one_sub_on_the_written_notes(self):
        """Under 110 Hz the two characters are one sine: A's and B's subs differ only by B's quiet second harmonic."""
        a, _ = B.sub_lane('Rubber', B.CLEAN)
        b, _ = B.sub_lane('Rubber', B.WARM)
        low = lambda x: phrase.circular(lambda u: D.lr4(u, 70.0, 'lowpass'), x)   # noqa: E731
        diff = D.db(np.max(np.abs(low(a) - low(b)))) - D.db(np.max(np.abs(low(a))))
        self.assertLess(diff, -20.0)

    def test_the_lines_pass_their_rule_against_the_kicks(self):
        """A, the strict gate in every gap; B, bass-body's -2 dB in every gap (the lines as rendered, before the loop)."""
        for name in B.LINES:
            for char, limit in ((B.CLEAN, -20.0), (B.WARM, -2.0)):
                rows = qa.loop_mud(B.line_lane(name, char), B.kick_onsets())
                self.assertLessEqual(max(r['vs_sub_db'] for r in rows), limit, (name, char))
                if char == B.CLEAN:
                    self.assertLessEqual(max(r['before_hit_db'] for r in rows), -30.0, name)


class TestChords(unittest.TestCase):
    def test_the_dub_stab_is_gm9_voiced_open(self):
        """G3 D4 F4 A4 Bb4: root, fifth, seventh, ninth and the minor third on top; nothing within a fourth of the root,
        so the tuning reads G3 alone (a Bb3 there pulled it 120-200 cents sharp)."""
        self.assertEqual((55, 62, 65, 69, 70), Ch.CHORD)
        self.assertTrue(all(in_key(n) for n in Ch.CHORD))
        self.assertTrue(all(n - Ch.CHORD[0] > 5 for n in Ch.CHORD[1:]))

    def test_the_stab_hits_stay_off_the_kicks(self):
        self.assertTrue(all(s % 4 for s, _ in Ch.HITS))
        self.assertTrue(all(s % 4 for s, _ in A.DUB_RHYTHM))

    def test_the_pad_joins_on_whole_cycles(self):
        """Every pad voice is moved to a whole number of cycles over the 4 bars (so the loop needs no crossfade), at most
        half a cycle per loop off its note: under 0.5 cents on G3."""
        L = phrase.loop_len(K, Ch.PAD_STEPS)
        for note in Ch.CHORD:
            for d in (-Ch.PAD_DETUNE, 0.0, Ch.PAD_DETUNE):
                f = float(D.midi_hz(note)) * 2 ** (d / 1200)
                g = Ch.whole(f, L)
                self.assertAlmostEqual(g * L / SR, round(g * L / SR), places=6)
                self.assertLess(abs(D.cents(g, f)), 0.5)

    def test_the_keys_cover_the_phrases(self):
        """Every note the keys phrases play lies within the Keys EP zones' reach (a fifth either way of a zone)."""
        zones = A.KEYS_NOTES
        for _, m, _, _ in A.KEYS_RIFF + A.ARP:
            self.assertLessEqual(min(abs(m - z) for z in zones), 7, m)


class TestVocalHook(unittest.TestCase):
    def test_the_hook_uses_only_the_kits_title_checked_line(self):
        """No new words: every chop is one of vox.py's chops of its pick, and every word of the hook's script is in
        "Ride the current with me"."""
        words = set(X.LINE_BY_NAME[X.PICK].text.lower().split())
        self.assertLessEqual(set(H.HOOK_WORDS.split()), words)
        self.assertTrue(all(c in X.CHOP_BY_NAME for _, c, _ in H.HOOK))
        for it in kits.items(K, sets=('r4',)):
            if it.category == 'vox':
                self.assertLessEqual(set(it.script.lower().split()), words, it.name)
                self.assertIn('hf_synthetic', it.flags, it.name)
                self.assertIn('clean', it.tags, it.name)

    def test_the_hook_sits_on_the_grid(self):
        self.assertTrue(all(0 <= s < H.HOOK_STEPS for s, _, _ in H.HOOK))
        for (s, _, ln), (s2, _, _) in zip(H.HOOK, H.HOOK[1:]):
            self.assertLessEqual(s + ln, s2 + 0.5, s)


if __name__ == '__main__':
    unittest.main()
