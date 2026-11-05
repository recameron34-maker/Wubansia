"""FUT03 "Glass Current", the rhythm and atmosphere track (recipes/fut03: drums, perc, pad, fx): what the brief and round
1 ask of it that the QA can't see on its own. Everything is in the key, the fill and the riser are exactly their bars
long at 128 BPM, the hat loop chokes every open hat, the stab's pitch is exact on every round robin, and the kick's top
layer sits in the crest band round 1 liked (Split's), well under Tight's. Round 3 (the modern-sound standard,
docs/plans/S1-modern-sound.md): the kick's clear click with its length kept, the clap's four hands with the first the
loudest, its body and its tail, and the glass-bead shaker."""
import unittest

import numpy as np
from scipy import signal

import tools.sound  # noqa: F401  (pins numpy's kernels before numpy loads)
from tools.sound import dsp48 as D
from tools.sound import kits, qa

K = kits.load('FUT03')


def item(name):
    return kits.items(K, only=(name,))[0]


def pitch_class(note):
    return int(note) % 12


class TestFut03Rhythm(unittest.TestCase):
    def test_everything_is_in_the_key(self):
        from tools.sound.recipes.fut03 import fx, pad, perc
        scale = set(K.SCALE)
        self.assertLessEqual({pitch_class(n) for n in pad.STAB_CHORD}, scale)
        # Gm9 in thirds (G Bb D F A) over the fifth below (D3, round 3: the 125 Hz octave's body)
        self.assertEqual([2, 7, 10, 2, 5, 9], [pitch_class(n) for n in pad.STAB_CHORD])
        self.assertEqual(pad.STAB_ROOT - 5, pad.STAB_CHORD[0])
        self.assertGreaterEqual(min(np.diff(pad.STAB_CHORD)), 3)        # no cluster: no voices closer than a m3
        self.assertLessEqual({pitch_class(n) for _, n, _ in fx.FILL}, scale)
        for _, note, v in fx.FILL:                                       # every glass mode a tom rings is in the key
            for f, a in fx.tom_modes(float(D.midi_hz(note)), v):
                if a:
                    m = 69 + 12 * np.log2(f / 440.0)
                    self.assertIn(int(round(m)) % 12, scale, (D.note_name(note), round(f, 1)))
        # the hats' FM carriers ring a G minor triad over G7 (the bell's tierce, quint and nominal)
        from tools.sound.recipes.fut03 import drums
        self.assertEqual(K.ROOT + 60, round(69 + 12 * np.log2(drums.HAT_BASE / 440.0), 6))
        for c, _ in drums.HAT_PAIRS:
            m = 69 + 12 * np.log2(drums.HAT_BASE * K.RATIOS[c] / 440.0)
            self.assertLess(abs(m - round(m)), 0.01)
            self.assertIn(int(round(m)) % 12, scale)
        self.assertEqual(7, pitch_class(fx.GLIDE[0]))                   # the riser glides G to G
        self.assertEqual(7, pitch_class(fx.BELL_NOTE))
        # the pling's four low modes are a minor triad and its octave: every zone's chord is in the key
        semis = [round(12 * np.log2(r)) for r in K.RATIOS[:4]]
        self.assertEqual([0, 3, 7, 12], semis)
        for root in perc.PLING_NOTES:
            self.assertLessEqual({pitch_class(root + s) for s in semis}, scale, D.note_name(root))

    def test_the_fill_and_the_riser_are_their_bars_long(self):
        from tools.sound.recipes.fut03 import fx
        bar = 4 * 60.0 / K.BPM * D.SR
        self.assertEqual(round(bar), fx.BAR)
        self.assertEqual(round(8 * bar), fx.RISER_N)
        self.assertEqual(2 * fx.BAR, fx.IMPACT_N)
        self.assertEqual(fx.BAR, len(item('BuildFill').render(item('BuildFill').takes('FUT03')[0]).audio))
        steps = [s for s, _, _ in fx.FILL]
        self.assertEqual(steps, sorted(steps))
        self.assertLess(fx.swung(steps[-1]), fx.BAR)
        self.assertEqual(fx.swung(1), int(np.floor((1 + 2 * K.SWING - 1) * K.STEP * D.SR + 0.5)))   # Grid's swing

    def test_every_open_hat_is_choked(self):
        from tools.sound.recipes.fut03 import drums
        hits = drums.HAT_HITS
        opens = [s for s, kind, _ in hits if kind == 'open']
        closed = sorted(s for s, kind, _ in hits if kind == 'closed')
        self.assertEqual(list(range(2, drums.HAT_STEPS, 4)), opens)    # every offbeat
        for s in opens:
            nxt = min((c for c in closed if c > s), default=closed[0] + drums.HAT_STEPS)
            self.assertIn(nxt - s, (1, 2), s)    # choked a 16th later, or at the next beat where it is let ring

    def test_the_kick_sits_in_splits_crest_range(self):
        """Round 1 picked Split (top layer 5.8-6.4 dB) over Tight (7.9-8.9), which sounded 'like its really slamming
        the limiter'. The floor is 4.0 dB and the standard's band 5-7 dB; this kick keeps its top layer near Split's,
        its body never driven, with round 3's louder click (6.1 and 6.5 dB)."""
        it = item('KickSplit')
        tops = [t for t in it.takes('FUT03') if t.layer == len(it.layers)]
        crest = [qa.kick_crest_db(it.render(t)) for t in tops]
        for c in crest:
            self.assertGreaterEqual(c, 5.5)
            self.assertLessEqual(c, 6.8)

    def test_the_kick_has_a_clear_click_and_keeps_its_length(self):
        """The brief's clear click (the standard's measure: 2-8 kHz in the first 10 ms against 30-120 Hz in the first
        100 ms, -23.3 dB in round 2, aim -18 or more), the sub still 20 dB down by the offbeat 8th (225 ms in round
        2), and the punch: 100-150 Hz over the first 50 ms against 30-100 Hz (round 2 -7.7 dB)."""
        it = item('KickSplit')
        for t in it.takes('FUT03'):
            if t.layer != len(it.layers):
                continue
            m = D.mono(it.render(t))
            bp = lambda lo, hi, a, b: D.bfilt(m, (lo, hi), 'bandpass', order=4, zero_phase=True)[a:b]
            e1 = 10 * np.log10(np.convolve(m ** 2, np.ones(48) / 48, 'same') + 1e-30)
            o = int(np.nonzero(e1 >= e1.max() - 40)[0][0])
            energy = lambda x: 10 * np.log10(np.sum(x ** 2) + 1e-30)
            click = energy(bp(2000, 8000, o, o + 480)) - energy(bp(30, 120, o, o + 4800))
            punch = energy(bp(100, 150, o, o + 2400)) - energy(bp(30, 100, o, o + 2400))
            self.assertGreaterEqual(click, -18.5, t.file)
            self.assertGreaterEqual(punch, -7.0, t.file)
            sub = 10 * np.log10(np.convolve(bp(30, 120, 0, len(m)) ** 2, np.ones(240) / 240, 'same') + 1e-30)
            pk = int(np.argmax(sub))
            fall = (pk + np.nonzero(sub[pk:] >= sub[pk] - 20)[0][-1] - o) / D.SR
            self.assertTrue(0.20 <= fall <= 0.24, (t.file, fall))

    def test_the_stab_is_exactly_on_its_root_on_every_round_robin(self):
        """The mirror-phased supersaw: each detuned pair starts at +-p, so the stack's pitch is the note's own (random
        phases read up to 6 cents off on G3). Read on the root in a band of +-12% (8th order, zero-phase): qa.pitch_hz's
        band (f0 / 1.3 to 1.3 f0) reaches down to the round 3 low voice, D3 (147 Hz), whose skirt pulls it 1-3 cents."""
        it = item('StabGm9')
        for t in it.takes('FUT03'):
            if t.layer != len(it.layers):
                continue
            m = D.mono(it.render(t).audio)
            y = D.bfilt(D.bfilt(m, t.f0 / 1.12, 'highpass', order=8, zero_phase=True), t.f0 * 1.12, 'lowpass', order=8,
                        zero_phase=True)[int(0.10 * D.SR):int(0.28 * D.SR)]
            self.assertGreater(10 * np.log10(np.sum(y ** 2) / np.sum(m[int(0.10 * D.SR):int(0.28 * D.SR)] ** 2)), -30)
            z = signal.hilbert(y)
            e = int(0.15 * len(z))
            z = z[e:len(z) - e]
            hz = np.polyfit(np.arange(len(z)) / D.SR, np.unwrap(np.angle(z)), 1, w=np.abs(z) + 1e-12)[0] / (2 * np.pi)
            self.assertLess(abs(D.cents(hz, t.f0)), 1.5, t.file)

    def test_the_clap_is_four_hands_the_first_loudest(self):
        """Round 3's clap: four hands within 35 ms (the standard's 3-4 hands in 25-35 ms), each at an exact level with
        the first the loudest (round 2's first hand was 8 dB under its loudest); on the top layer a crest of about 12 dB
        (round 2: 18.4, the loop's peak), a body in 150-400 Hz (round 2: -32 dB, none) and a room tail that is 20 dB down
        at 120-250 ms (round 2: 73 ms)."""
        from tools.sound.recipes.fut03 import drums
        self.assertEqual(4, len(drums.CLAP_HANDS))
        self.assertEqual(max(drums.CLAP_LEVELS), drums.CLAP_LEVELS[0])
        self.assertLess(drums.CLAP_HANDS[-1] * max(drums.CLAP_SPREAD), 0.035)
        it = item('Clap')
        for t in it.takes('FUT03'):
            if t.layer != len(it.layers):
                continue
            x = it.render(t)
            m = D.mono(x)
            self.assertTrue(10.5 <= qa.kick_crest_db(x) <= 13.5, t.file)       # the crest: true peak over 50 ms
            P = np.abs(np.fft.rfft(m, n=D.SR)) ** 2
            f = np.fft.rfftfreq(D.SR, 1 / D.SR)
            body = 10 * np.log10(P[(f >= 150) & (f < 400)].sum() / P.sum())
            self.assertTrue(-15.0 <= body <= -10.0, (t.file, body))
            e = 10 * np.log10(np.convolve(m ** 2, np.ones(240) / 240, 'same') + 1e-30)
            pk = int(np.argmax(e))
            tail = (pk + np.nonzero(e[pk:] >= e[pk] - 20)[0][-1]) / D.SR
            self.assertTrue(0.12 <= tail <= 0.25, (t.file, tail))

    def test_the_shaker(self):
        """The glass-bead shaker: 4 layers and 4 round robins for 16ths, everything over 2 kHz, wide (the side within
        10 dB of the mid), and each stroke 20 dB down inside a 16th (117 ms at 128 BPM)."""
        it = item('Shaker')
        self.assertEqual((4, 4), (len(it.layers), it.rr))
        for t in it.takes('FUT03'):
            if t.layer != len(it.layers):
                continue
            x = D.st(it.render(t))
            m = D.mono(x)
            P = np.abs(np.fft.rfft(m, n=D.SR)) ** 2
            f = np.fft.rfftfreq(D.SR, 1 / D.SR)
            self.assertLess(10 * np.log10(P[f < 2000].sum() / P.sum()), -30, t.file)
            side, mid = np.sum((x[:, 0] - x[:, 1]) ** 2), np.sum((x[:, 0] + x[:, 1]) ** 2)
            self.assertGreater(10 * np.log10(side / mid), -10.0, t.file)
            e = 10 * np.log10(np.convolve(m ** 2, np.ones(240) / 240, 'same') + 1e-30)
            pk = int(np.argmax(e))
            self.assertLess((pk + np.nonzero(e[pk:] >= e[pk] - 20)[0][-1]) / D.SR, K.STEP, t.file)


if __name__ == '__main__':
    unittest.main()
