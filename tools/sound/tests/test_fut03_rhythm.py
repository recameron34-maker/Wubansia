"""FUT03 "Glass Current", round 4's drums, perc and accents (recipes/fut03/groove.py, accents.py): what the brief and round
1 ask of them that the QA can't see on its own. Every tuned part is in G minor; the deep kick is the split design, its
sub clean and gone by the offbeat, its top layer in the crest band round 1 liked; the clap and snare is four hands over
a tuned body; the hats choke; the perc's ghosts sit on the swing; each accent states its root in its name. Nothing here
needs the vocal models; the kick, clap and perc tests render their recipes (a few seconds)."""
import re
import unittest

import numpy as np

import tools.sound  # noqa: F401  (pins numpy's kernels before numpy loads)
from tools.sound import dsp48 as D
from tools.sound import kits, qa
from tools.sound.item import Take
from tools.sound.kits import fut03 as K
from tools.sound.recipes.fut03 import accents as A
from tools.sound.recipes.fut03 import chords as Ch
from tools.sound.recipes.fut03 import context as C
from tools.sound.recipes.fut03 import groove as Gr

SR = D.SR


def item(name):
    return kits.items(K, only=(name,))[0]


def take(name, layer=None, rr=1, note=None):
    it = item(name)
    return Take('FUT03', it, layer or len(it.layers), rr, note)


def in_key(note):
    return int(note) % 12 in K.SCALE


class TestKick(unittest.TestCase):
    def test_the_split_kick_keeps_the_crest_floor_in_splits_band(self):
        """The top layer's crest (true peak over the loudest 50 ms) is over ADR 0011's 4 dB floor and inside the 5-7 dB
        band round 1 liked (Split read 5.8-6.4, Tight 7.9-8.9, "slamming the limiter"), on both round robins."""
        for rr in (1, 2):
            y = D.bfilt(Gr.kick_deep(take('KickDeep', rr=rr)), 20.0, 'highpass', order=4)
            self.assertTrue(5.0 <= qa.kick_crest_db(y) <= 7.0, (rr, qa.kick_crest_db(y)))

    def test_the_sub_is_clean_on_g1_and_gone_by_the_offbeat(self):
        """Under 110 Hz the kick is its sine on G1 (the key's root), 20 dB down by the offbeat 8th (234 ms at 128 BPM)
        and silent from 0.33 s: the rolling bass has the gap. Nothing drives it: the knock and beater are above 110 Hz."""
        y = Gr.kick_deep(take('KickDeep'))
        sub = D.lr4(y, 110.0, 'lowpass')
        env = qa.env_db(sub)
        peak = env.max()
        self.assertLess(env[int(0.234 * SR)] - peak, -20.0)
        self.assertLess(np.max(np.abs(y[int(0.34 * SR):])), 1e-4)
        hz, share = qa.pitch_hz(sub, float(D.midi_hz(K.KICK_NOTE)), 0.10, 0.20)
        self.assertLess(abs(D.cents(hz, float(D.midi_hz(K.KICK_NOTE)))), 10.0)

    def test_the_round_knock_starts_low(self):
        """Round, not clicky: the sweep starts at most at 380 Hz (round 3's started at 560) and the knock rolls off from
        2.8 kHz; there is no glass ring on the kick any more."""
        self.assertLessEqual(max(Gr.SWEEP_HI), 380.0)
        self.assertLessEqual(Gr.KNOCK_LP, 3000.0)
        self.assertFalse(hasattr(Gr, 'RING_NOTE'))


class TestClapAndHats(unittest.TestCase):
    def test_the_clap_is_four_hands_the_first_loudest_over_a_tuned_snare(self):
        """Four hands within about 30 ms, the first the loudest; the snare's body on G3 and its second mode on Eb4,
        both in the key."""
        self.assertEqual(4, len(Gr.HANDS))
        self.assertLess(max(Gr.HANDS) * max(Gr.SPREAD), 0.035)
        self.assertEqual(max(Gr.HAND_LEVELS), Gr.HAND_LEVELS[0])
        body = float(D.midi_hz(Gr.SNARE_NOTE))
        self.assertTrue(in_key(Gr.SNARE_NOTE))
        eb4 = Gr.SNARE_NOTE + 8                          # G3 + a minor sixth: Eb4, 311 Hz
        self.assertLess(abs(D.cents(body * Gr.SNARE_MODE2, float(D.midi_hz(eb4)))), 10.0)
        self.assertTrue(in_key(eb4))

    def test_every_open_hat_is_choked_or_rings_on_the_bars_last_beat(self):
        closed = sorted(s for s, kind, _ in C.HATS if kind == 'closed')
        for s, kind, _ in C.HATS:
            if kind == 'open':
                nxt = min((c for c in closed if c > s), default=closed[0] + C.STEPS)
                self.assertIn(nxt - s, (1, 2), s)
                if nxt - s == 2:
                    self.assertEqual(C.LET_RING, (s // 4) % 4, s)

    def test_the_shaker_plays_every_16th_the_swung_ones_leading(self):
        self.assertEqual(list(range(C.STEPS)), [s for s, _ in C.SHAKER])
        beat = C.SHAKER_BEAT
        self.assertGreater(beat[3], beat[0])
        self.assertGreater(beat[1], beat[0])


class TestPerc(unittest.TestCase):
    def test_every_hand_drum_zone_is_in_the_key(self):
        for name in ('Conga', 'Bongo'):
            self.assertTrue(all(in_key(n) for n in item(name).notes), name)
        self.assertTrue(in_key(K.ROOT + 43))              # the rim's glass ring, D6

    def test_the_ghosts_sit_on_the_swing(self):
        """Every ghost stroke (velocity 0.3) of the perc pattern is on an odd 16th, the ones the kit's swing moves."""
        ghosts = [s for s, _, _, v in Gr.PERC if v <= 0.3]
        self.assertGreaterEqual(len(ghosts), 6)
        self.assertTrue(all(s % 2 for s in ghosts), ghosts)

    def test_a_conga_settles_on_its_note(self):
        for note in item('Conga').notes:
            y = Gr.conga(take('Conga', layer=2, note=note))
            f = float(D.midi_hz(note))
            hz, share = qa.pitch_hz(y, f, 0.03, 0.20)
            self.assertLess(abs(D.cents(hz, f)), 3.0, note)


class TestAccents(unittest.TestCase):
    NOTE = {'C': 0, 'D': 2, 'E': 4, 'F': 5, 'G': 7, 'A': 9, 'B': 11}

    def test_each_accent_states_its_root_in_its_name_and_the_root_is_in_the_key(self):
        """Every accent's name ends on its root (Cm9 is C, G1 is G), and every one is in G minor."""
        accents = [it for it in kits.items(K, sets=('r4',)) if it.name.startswith('Accent')]
        self.assertTrue(6 <= len(accents) <= 10, len(accents))
        for it in accents:
            m = re.search(r'([A-G])(b?)(m7|m9|m|\d)$', it.name)
            about_root = re.search(r'root ([A-G]b?\d)', it.about)
            self.assertIsNotNone(about_root, it.name)
            if it.root is not None:
                self.assertTrue(in_key(it.root), it.name)
                if m:
                    self.assertEqual(self.NOTE[m.group(1)], it.root % 12, it.name)

    def test_the_chords_are_in_the_key(self):
        for chord in (A.CM9, A.GM7, A.DM7, A.GM_TRIAD, Ch.CHORD):
            self.assertTrue(all(in_key(n) for n in chord), chord)
        for chord in A.ARP_CHORDS:
            self.assertTrue(all(in_key(n) for n in chord), chord)
        self.assertTrue(all(in_key(m) for _, m, _, _ in A.KEYS_RIFF))

    def test_the_cm9_is_the_gm9_voicing_a_fourth_up(self):
        """So the Dub Stab Gm9 sample played on C4 is the Cm9 the dub-chord phrase uses (its MIDI says so)."""
        self.assertEqual(tuple(n + 5 for n in Ch.CHORD), A.CM9)

    def test_the_rubber_stab_keeps_the_sub_a_pedal(self):
        """The stab's sub holds G1 (a gliding sine splatters into the low mids): under 110 Hz it reads G1 at once."""
        y = A.rubber_stab(take('AccentRubberStabG1')).audio
        f = float(D.midi_hz(31))
        hz, _ = qa.pitch_hz(D.lr4(y, 110.0, 'lowpass'), f, 0.02, 0.15)
        self.assertLess(abs(D.cents(hz, f)), 3.0)


if __name__ == '__main__':
    unittest.main()
