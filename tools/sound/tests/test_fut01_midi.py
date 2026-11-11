"""FUT01's MIDI (tools/sound/recipes/fut01/midi.py), on top of test_midi.py's kit-agnostic properties: its Loop files
lane by lane against its round 5 context loop (context5.py on context4.py), its roots, its phrases' channels, its
wobble ride and the audit's gate example. FUT01's own tests: its engineer edits them with the kit's PARTS."""
import unittest

from tools.sound.recipes.fut01 import context4 as C, context5 as C5, keys as Ky, music as M, voxsynth as V
from tools.sound.tests.midi_testlib import (BAR, LoopCase, at, chord_roots, drum, kit_files, loop, one_shot, parse,
                                            phrase_channels, ride, value_at)

KIT = 'FUT01'


class TestLoop(LoopCase, unittest.TestCase):
    KIT = KIT

    def expected(self, variant):
        a = lambda s: at(KIT, s)  # noqa: E731
        stabs = [(s, z, v) for s, z, v in C.STABS if s not in C5.STAB_DROP[variant]]
        out = {
            'Kick': drum(KIT, C.KICK, 36),
            'Clap': drum(KIT, C.CLAP, 38),
            'Hats': drum(KIT, C.HAT_C, 42) + drum(KIT, C.HAT_O, 46),
            'Perc': drum(KIT, C.TAMB, 54) + drum(KIT, C.RIM, 37) + drum(KIT, C.PERC, 47),
            'Bass': [(a(s + 32 * r), n, v, None) for r in (0, 1) for s, _, n, v, _ in C.WARP_LINE],
            'Lead': [(a(s + 32 * r), n, v, None) for r in (0, 1) for s, n, _, v in M.PHRASE],
            'Chords': [(a(s), z, v, a(s) + 240) for s, z, v in stabs],
            'Chord voicing': [(a(s), z + d, v, a(s) + 240) for s, z, v in stabs for d in Ky.SHAPE],
            'Accents': [one_shot(KIT, 'accents', name, step, layer, how, gate=gate)
                        for _, name, step, layer, how, gate in C5.ACCENTS[variant]],
            'FX': [one_shot(KIT, 'fx', 'GlitchFillLift', C.FILL_AT, 1, 'on', start=C.FILL_FROM)],
        }
        if variant == 'vox':
            out['Vox'] = [(a(s + at_), n, v, None) for at_ in C5.RIFF_AT['vox'] for s, n, _, v, _ in V.RIFF_NOTES]
        return out


class TestParts(unittest.TestCase):
    def test_the_organ_stabs_at_their_roots(self):
        """F#m9 on F#3 (54), Bm9 on B3 (59), on channel 4; the voicing on 3."""
        self.assertEqual(({54, 59}, {4}, {3}), chord_roots(KIT))

    def test_the_phrases_channels(self):
        self.assertEqual({'OrganRiff': 12, 'WarpRiff': 1, 'PluckRiff': 2, 'EPianoStabRhythm': 4, 'VoxSynthRiff': 5},
                         phrase_channels(KIT))

    def test_the_low_line_plays_the_organ_bass_on_channel_11(self):
        notes = parse(kit_files(KIT)['FUT01_Bassline_Low_2bars.mid'])[2][1]['notes']
        self.assertEqual({11}, {ch for _, _, ch, _, _ in notes})
        for name in ('FUT01_Bassline_Bounce_InTheLoop_2bars.mid', 'FUT01_Bassline_Speed_2bars.mid'):
            self.assertEqual({1}, {ch for _, _, ch, _, _ in parse(kit_files(KIT)[name])[2][1]['notes']})

    def test_the_bounce_f_sharp_1_no_longer_runs_into_the_f_sharp_2(self):
        """Audit A defect 5: F#1 on the swung 16th 11 ended 43 ticks into the straight F#2 on 12."""
        notes = {(on, key): off for on, off, _, key, _ in parse(kit_files(KIT)[
            'FUT01_Bassline_Bounce_InTheLoop_2bars.mid'])[2][1]['notes']}
        self.assertEqual(12 * 240, notes[(11 * 240 + 43, 30)])

    def test_the_kicks_halfway_between_layers_play_the_louder(self):
        """The 0.9 kicks sit halfway between the 0.8 and 1.0 layers, where the recipe plays 1.0: velocity 115, not
        round(0.9 x 127) = 114, which a nearest-layer sampler plays on 0.8."""
        kicks = {vel for _, _, _, _, vel in loop(KIT, 'vox')['Kick']['notes']}
        self.assertEqual({70, 115, 127}, kicks)

    def test_no_throws(self):
        """FUT01's riff throw is baked into its stem: no CC91/CC94."""
        for v in ('vox', 'novox'):
            self.assertFalse([c for t in loop(KIT, v).values() for c in t['ccs'] if c[2] in (91, 94)])


class TestRides(unittest.TestCase):
    def test_the_wobble(self):
        """The warp bass's wobble (context4.wobble_cutoff) on CC74, channel 1, 650-2210 Hz: closed on every straight
        8th's edge, open in its middle, widest in bar 4, then bar 2; no CC71 (its Q is fixed)."""
        r = ride(KIT, 'Bass', 74)
        peaks = [max(v for t, v in r.items() if b * BAR <= t < (b + 1) * BAR) for b in range(4)]
        self.assertEqual(127, peaks[3])
        self.assertEqual(sorted(peaks), [peaks[0], peaks[2], peaks[1], peaks[3]])
        self.assertEqual(0, value_at(r, 0))
        self.assertEqual(peaks[0], value_at(r, 240))
        self.assertFalse(ride(KIT, 'Bass', 71))


if __name__ == '__main__':
    unittest.main()
