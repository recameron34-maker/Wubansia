"""FUT02 "Orbit Relay" tonal parts: the written lines keep the round 1 rules (docs/plans/S1-four-kits.md) without
rendering a note. The basslines rest on every kick and clear before the next one, stay in D minor inside the bass-body
range and carry a slide, ghosts and held notes; the arp phrase and the sung lines stay in the key; the lines are 3-6
words; the vocoder's slices sit on the grid. Round 3's bass bite saturates the mid band and nothing in 150-400 Hz: A
keeps that band empty, B passes it as it was; and the arp's width is a detuned unison on its side: its mono sum is the
voice, it starts in the middle with each note, and a note sits balanced between the speakers. Nothing here needs the
vocal models."""
import unittest

import numpy as np

from tools.sound import dsp48 as D
from tools.sound import parts as P
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


class TestFut02Bite(unittest.TestCase):
    """bass.bite on test tones: the low mids stay as the low-end rules measured them, the band above gets denser."""

    def tone(self, f, amp=0.5, n=int(0.5 * SR)):
        return amp * np.sin(2 * np.pi * f * np.arange(n) / SR)

    def level_db(self, x):
        k = slice(int(0.1 * SR), len(x) - int(0.05 * SR))               # past the filters' start
        return 10 * np.log10(np.mean(x[k] ** 2) + 1e-30)

    def test_b_passes_its_low_mids_linearly(self):
        self.assertGreaterEqual(bass.BITE_SPLIT, 400.0)                  # over the bass-body band
        for f in (180.0, 260.0):
            with self.subTest(f=f):
                x = self.tone(f)
                ref = D.lr4(x, bass.MID_HP[bass.WARM], 'highpass')
                self.assertLess(abs(self.level_db(bass.bite(x, bass.WARM)) - self.level_db(ref)), 0.1)

    def test_a_keeps_150_400_hz_empty(self):
        for f in (180.0, 260.0, 380.0):
            with self.subTest(f=f):
                x = self.tone(f)
                self.assertLess(self.level_db(bass.bite(x, bass.CLEAN)) - self.level_db(x), -60.0)

    def test_the_bite_makes_the_mid_band_denser(self):
        """A high-passed saw on D2 (the A mids): its crest falls by at least 3 dB through the bite, at about its
        loudness."""
        n = int(0.5 * SR)
        saw = D.down(D.polyblep_saw(float(D.midi_hz(38)), n, 0.0, k=4), 4) * 0.8
        band = P.hp_steep(saw, bass.MID_HP[bass.CLEAN])
        out = bass.bite(saw, bass.CLEAN)
        k = slice(int(0.1 * SR), n - int(0.05 * SR))
        crest = lambda y: 20 * np.log10(np.abs(y[k]).max() / np.sqrt(np.mean(y[k] ** 2)))
        self.assertGreater(crest(band) - crest(out), 3.0)
        self.assertLess(abs(self.level_db(out) - self.level_db(band)), 3.0)


class TestFut02Lead(unittest.TestCase):
    def test_the_arp_widens_and_keeps_its_mono_sum(self):
        cents, level, hp = lead.UNISON
        self.assertTrue(3.0 <= cents <= 12.0 and level > 0 and hp >= 1000.0)
        self.assertFalse(hasattr(lead, 'SIDE'))                          # no all-pass side any more
        r = D.rng('test', 'arp-width')
        y, d = r.standard_normal(9600) * 0.1, r.standard_normal(9600) * 0.1
        ref = D.mono_below(D.bfilt(np.stack([y, y], 1), lead.HP, 'highpass', order=2), 150.0)
        for loop in (False, True):
            np.testing.assert_allclose(D.mono(lead.stereo(y, d, loop)), D.mono(ref), atol=1e-12)

    def test_a_note_is_wide_and_sits_in_the_middle(self):
        """The side starts at zero with the note (its two detuned copies start in phase) and spreads; the note's left
        and right carry the same level above 2 kHz (an all-pass side's direct term leaned round 3's first arp 4.4 dB
        to the right), and its side sits within 6 dB of its mid there."""
        f, vel, n = float(D.midi_hz(77)), 1.0, int(0.4 * SR)
        d = lead.detuned(f, vel, n, None, 0.0)
        y = lead.note_voice(f, vel, n, None, 0.0)
        early, later = slice(0, int(0.002 * SR)), slice(int(0.05 * SR), int(0.15 * SR))
        self.assertLess(np.sqrt(np.mean(d[early] ** 2)), 0.05 * np.sqrt(np.mean(y[early] ** 2)))
        self.assertGreater(np.sqrt(np.mean(d[later] ** 2)), 0.2 * np.sqrt(np.mean(y[later] ** 2)))
        x = D.bfilt(lead.stereo(y, d), 2000.0, 'highpass', order=4, zero_phase=True)
        lr = 10 * np.log10(np.sum(x[:, 0] ** 2) / np.sum(x[:, 1] ** 2))
        self.assertLess(abs(lr), 1.0, lr)
        m, s = 0.5 * (x[:, 0] + x[:, 1]), 0.5 * (x[:, 0] - x[:, 1])
        self.assertGreater(10 * np.log10(np.sum(s ** 2) / np.sum(m ** 2)), -6.0)

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
