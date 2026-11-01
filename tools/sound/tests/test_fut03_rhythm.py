"""FUT03 "Glass Current", the rhythm and atmosphere track (recipes/fut03: drums, perc, pad, fx): what the brief and round
1 ask of it that the QA can't see on its own. Everything is in the key, the fill and the riser are exactly their bars
long at 128 BPM, the hat loop chokes every open hat, the stab's pitch is exact on every round robin, and the kick's top
layer sits in Split's crest range (round 1's pick), well under Tight's."""
import unittest

import numpy as np

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
        self.assertEqual([7, 10, 2, 5, 9], [pitch_class(n) for n in pad.STAB_CHORD])   # G Bb D F A: Gm9 in thirds
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
        the limiter'. The floor is 4.0 dB; this kick keeps its top layer in Split's range, its body never driven."""
        it = item('KickSplit')
        tops = [t for t in it.takes('FUT03') if t.layer == len(it.layers)]
        crest = [qa.kick_crest_db(it.render(t)) for t in tops]
        for c in crest:
            self.assertGreaterEqual(c, 5.5)
            self.assertLessEqual(c, 6.5)

    def test_the_stab_is_exactly_on_its_root_on_every_round_robin(self):
        """The mirror-phased supersaw: each detuned pair starts at +-p, so the stack's pitch is the note's own (random
        phases read up to 6 cents off on G3)."""
        it = item('StabGm9')
        for t in it.takes('FUT03'):
            if t.layer != len(it.layers):
                continue
            y = it.render(t).audio
            hz, share = qa.pitch_hz(y, t.f0, 0.10, 0.28)
            self.assertGreater(share, -30)
            self.assertLess(abs(D.cents(hz, t.f0)), 1.5, t.file)


if __name__ == '__main__':
    unittest.main()
