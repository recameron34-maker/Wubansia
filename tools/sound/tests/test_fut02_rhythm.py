"""FUT02 "Orbit Relay", the rhythm and atmosphere recipes (recipes/fut02: drums, perc, pad, fx): what they promise the
listener holds without rendering a whole item. Every written note is in D minor, the laser gesture lands exactly on
its note, the fill and the riser are a bar and eight bars on the kit's grid and swing, ending on the downbeat, and the
hat loop's open hats are choked by a closed one."""
import unittest

import numpy as np
from scipy import signal

from tools.sound import dsp48 as D
from tools.sound import kits
from tools.sound import parts as P
from tools.sound import phrase as Ph
from tools.sound import qa
from tools.sound.kits import fut02 as K
from tools.sound.recipes.fut02 import drums, fx, pad, perc


def in_key(note):
    return note % 12 in K.SCALE


class TestFut02Rhythm(unittest.TestCase):
    def test_every_written_note_is_in_d_minor(self):
        notes = [fx.NOTE[n] for _, n, *_ in fx.FILL_LASERS] + [fx.NOTE[fx.FILL_DIVE[1]]]
        notes += [fx.NOTE[n] for _, n, _ in fx.stair_steps()] + [fx.NOTE[fx.RISE[0]]]
        notes += list(pad.CHORD) + [perc.ZAP_NOTE, drums.SNARE_NOTE, K.KICK_NOTE]
        for note in notes:
            self.assertTrue(in_key(note), D.note_name(note))
        for name, note in fx.NOTE.items():
            self.assertEqual(name.replace('Bb', 'As'), D.note_name(note))

    def test_the_pad_is_dm9(self):
        self.assertEqual({2, 5, 9, 0, 4}, {n % 12 for n in pad.CHORD})   # D F A C E
        self.assertEqual(K.ROOT % 12, pad.CHORD[0] % 12)
        self.assertGreaterEqual(float(D.midi_hz(pad.CHORD[0])), 130.0)    # the root over the 100 Hz high-pass

    def test_the_laser_lands_on_its_note(self):
        """The kit's gesture: a fall of 1-3 octaves that ends on the note with no glide left over."""
        f0 = float(D.midi_hz(perc.ZAP_NOTE))
        for depth, drop in ((1.0, 0.03), (3.0, 0.07)):
            y = perc.laser(int(0.4 * D.SR), f0, depth, drop)
            hz, _ = qa.pitch_hz(y, f0, drop + 0.01, 0.35)
            self.assertLess(abs(float(D.cents(hz, f0))), 0.5, (depth, drop))
            head = y[:int(0.15 * drop * D.SR)] * np.hanning(int(0.15 * drop * D.SR))
            Y = np.abs(np.fft.rfft(head, 8192)) ** 2
            f = np.fft.rfftfreq(8192, 1 / D.SR)
            self.assertGreater(Y[f > 1.5 * f0].sum(), 10 * Y[f <= 1.5 * f0].sum())   # it starts far above

    def test_fill_and_riser_sit_on_the_grid_and_end_on_the_downbeat(self):
        for steps, bars in ((fx.FILL_STEPS, 1), (fx.RISER_STEPS, 8)):
            G = Ph.Grid(K, steps)
            self.assertEqual(16 * bars, steps)
            self.assertLess(abs(G.length - bars * 4 * K.BEAT * D.SR), 1.0)
            for s in range(steps):
                self.assertEqual(G.at(s), fx.swung(G, s))                   # 32nds keep the grid's 16ths
        G = Ph.Grid(K, fx.FILL_STEPS)
        self.assertLess(G.at(14), fx.swung(G, 14.5))
        self.assertLess(fx.swung(G, 14.5), G.at(15))
        self.assertGreater(fx.swung(G, 15.5), G.at(15))
        items = {it.name: it for it in fx.ITEMS}
        for name in ('LaserFill', 'LaserRiser'):
            self.assertIn('cut_end', items[name].flags)
            self.assertIn('r2', items[name].sets)
        self.assertIn('riser', items['LaserRiser'].tags)
        self.assertEqual(-14.0, items['LaserRiser'].level)

    def test_the_riser_climbs_and_gets_denser(self):
        steps = fx.stair_steps()
        notes = [fx.NOTE[n] for _, n, _ in steps]
        self.assertTrue(all(b >= a for a, b in zip(notes, notes[1:])))
        self.assertEqual(fx.NOTE['C6'], notes[-1])
        self.assertEqual(fx.NOTE['D6'], fx.NOTE[fx.RISE[0]] + 12 * fx.RISE[1])   # the rise lands on D6
        every = [e for _, _, e in steps]
        self.assertTrue(all(b <= a for a, b in zip(every, every[1:])))
        self.assertEqual((4, 1), (every[0], every[-1]))

    def test_hat_loop_open_hats_are_choked(self):
        closed = {s for s, _ in drums.HAT_CLOSED}
        for s, _ in drums.HAT_OPEN:
            self.assertEqual(2, s % 4)                                        # the off-beat 8th
            self.assertNotIn(s, closed)
            self.assertTrue(any((c - s) % drums.HAT_STEPS <= 2 for c in closed), s)   # a closed hat within an 8th
        self.assertEqual(drums.CHOKE, {'group': 'hats', 'closed': 'ClosedHat', 'open': 'OpenHat', 'fade_s': 0.004})

    def test_the_kick_leaves_the_e_to_the_bass(self):
        """The sub is at least 15 dB down by the e, where the rolling bass plays its first note of each beat, at every
        velocity; its quiet tail still carries the A1 where QA tunes a kick (0.10-0.25 s); it is exactly 0 by
        0.40 s, inside the beat."""
        t = np.arange(int(0.6 * D.SR)) / D.SR
        e = Ph.Grid(K, 32).at(1)                                        # the e of beat 1, on the swing
        for v in drums.LAYERS:
            with self.subTest(velocity=v):
                a = drums.sub_env(t, v)
                level = 20 * np.log10(a + 1e-30)
                self.assertLessEqual(level[e], -15.0)
                self.assertGreater(level[int(0.25 * D.SR)], -45.0)
                self.assertFalse(np.any(a[int(0.40 * D.SR):]))
                self.assertGreater(level[int(0.05 * D.SR)], -1.5)       # the hold

    def test_the_zap_is_mostly_its_fall(self):
        """Its fall (with the round robin's 3%) has landed before QA starts tuning it (0.10 s), and at least half of
        each take's energy is in the fall, the softest and the hardest layer."""
        self.assertLess(max(P.lerp(perc.ZAP_DROP, v) for v in perc.LAYERS) * 1.03, 0.10)
        item = kits.items(K, only=('Zap',))[0]
        for take in item.takes(K.KIT):
            if take.rr == 1 and take.layer in (1, len(item.layers)):
                with self.subTest(layer=take.layer):
                    y = D.mono(perc.zap(take))
                    k = int(P.lerp(perc.ZAP_DROP, take.vel) * D.SR)
                    self.assertGreater(np.sum(y[:k] ** 2) / np.sum(y ** 2), 0.5)

    def test_the_pad_is_mono_at_its_root(self):
        """mono_below's side high-pass (8th-order Butterworth at MONO_HZ) leaves the root D3 at least 20 dB down."""
        sos = signal.butter(8, pad.MONO_HZ, 'highpass', fs=D.SR, output='sos')
        _, h = signal.sosfreqz(sos, worN=[float(D.midi_hz(pad.CHORD[0]))], fs=D.SR)
        self.assertLess(20 * np.log10(abs(h[0])), -20.0)
        self.assertGreaterEqual(pad.MONO_HZ, 150.0)

    def test_drums_layers(self):
        for it in drums.ITEMS + perc.ITEMS:
            if it.category in ('kick', 'snare', 'hat', 'perc'):
                self.assertEqual(5, len(it.layers), it.name)
                self.assertEqual(3 if it.category == 'hat' else 2, it.rr, it.name)
                self.assertIn('r2', it.sets)


if __name__ == '__main__':
    unittest.main()
