"""FUT04's MIDI (tools/sound/recipes/fut04/midi.py), on top of test_midi.py's kit-agnostic properties: its Loop files
lane by lane against its round 5 context loop (context5.py on context.py), the rumble on drum note 35, the blips'
ratchets, its roots, its phrases' channels, the vox's vowels and throws, the thrown accents, the acid's rides and the
audit's gate example. FUT04's own tests: its engineer edits them with the kit's PARTS."""
import unittest

from tools.sound import dsp48 as D
from tools.sound.recipes.fut04 import bass as B, context as C, context5 as C5, lead as L, pad as Pd
from tools.sound.tests.midi_testlib import (LoopCase, at, chord_roots, drum, kit_files, layers_of, loop, mids, one_shot,
                                            parse, parsed, phrase_channels, ride, v127)

KIT = 'FUT04'


class TestLoop(LoopCase, unittest.TestCase):
    KIT = KIT

    def expected(self, variant):
        a = lambda s: at(KIT, s)  # noqa: E731
        line = B.LINE_BY_NAME[B.PICK]
        mode = C5.VOX if variant == 'vox' else C5.NOVOX
        out = {
            'Kick': drum(KIT, C.KICK, 36),
            'Rumble': [(0, 35, 1.0, 32 * 240), (a(32), 35, 1.0, a(C.KICK_OUT))],
            'Snare': drum(KIT, C.SNARE, 38),
            'Hats': drum(KIT, C.HAT_CLOSED, 42) + drum(KIT, C.HAT_OPEN, 46) + drum(KIT, C.RIDE, 51),
            'Perc': drum(KIT, C.ANVIL, 47),
            'Bass': [(a(s + 32 * r), n, v, None) for r in (0, 1)
                     for s, n, _, v, *_ in tuple(line.notes) + tuple(line.ticks)],
            'Lead': [(at(KIT, s + 32 * r) + round(k * 240 / rt), n, v * D.undb(L.RATCHET_DB * k), None)
                     for r in (0, 1) for s, n, _, v, rt in L.PHRASE for k in range(rt)],
            'Chords': [(0, 52, 1.0, 64 * 240)],
            'Chord voicing': [(0, n, 1.0, 64 * 240) for n in Pd.CHORD],
            'Accents': [one_shot(KIT, 'accents', name, 64 if s == 'end' else s, 1, 'end' if s == 'end' else 'on')
                        for s, name in C5.ACCENTS[mode]],
            'FX': [one_shot(KIT, 'fx', 'StutterFill', C.FILL_AT, 1, 'on')],
        }
        if variant == 'vox':
            out['Vox'] = [(a(s), m, v, None) for s, m, _, v in C5.VOX_NOTES]
        return out


class TestParts(unittest.TestCase):
    def test_the_drone_at_its_root(self):
        """The drone Em(add9) on E3 (52), on channel 4; its voicing on 3."""
        self.assertEqual(({52}, {4}, {3}), chord_roots(KIT))

    def test_the_phrases_channels(self):
        self.assertEqual({'VoxSynthRiff': 5, 'AcidStabRiff': 11, 'MetalChords': 12, 'RaveStabRiff': 13},
                         phrase_channels(KIT))

    def test_the_vox_synths_velocity_picks_its_vowel(self):
        """Every vox note sits on a layer's velocity: 64 'oo', 127 'oh'."""
        self.assertEqual([64, 127], [v127(x) for x in layers_of(KIT, 'VoxSynth')])
        for path in mids(KIT):
            for t in parsed(KIT, path)[2][1:]:
                if t['name'] == 'Vox':
                    self.assertLessEqual({vel for *_, vel in t['notes']}, {64, 127}, path)

    def test_the_throws(self):
        """The vox's throws on 16ths 30 and 46; without the vox, the zap and the metal chord thrown there."""
        got = sorted(c[0] for c in loop(KIT, 'vox')['Vox']['ccs'] if c[2] == 91 and c[3] == 127)
        self.assertEqual([at(KIT, 30), at(KIT, 46)], got)
        got = sorted(c[0] for c in loop(KIT, 'novox')['Accents']['ccs'] if c[2] == 91 and c[3] == 127)
        self.assertEqual([at(KIT, 30), at(KIT, 46)], got)

    def test_the_acid_e1_no_longer_runs_into_the_e2(self):
        """Audit A defect 5: E1 on the swung 16th 1 ended 19 ticks into E2 on 2."""
        notes = {(on, key): off for on, off, _, key, _ in parse(kit_files(KIT)[
            'FUT04_Bassline_AcidLine1_InTheLoop_2bars.mid'])[2][1]['notes']}
        self.assertEqual(2 * 240, notes[(240 + 19, 28)])


class TestRides(unittest.TestCase):
    def test_the_acid_ride(self):
        """The acid's 4-bar ride (bass.RIDES) on channel 1: CC74 its cutoff multiplier (x1-x2.6 on 5400-14040 Hz), CC71
        its resonance (0.90-0.96): both at rest (0) over bars 1-2, at their top on the fill bar's third beat."""
        for cc in (74, 71):
            r = ride(KIT, 'Bass', cc)
            self.assertEqual({0}, {v for t, v in r.items() if t < 32 * 240})
            self.assertEqual(127, max(r.values()))
            self.assertEqual(at(KIT, 56), max(r, key=lambda t: (r[t], -t)))


if __name__ == '__main__':
    unittest.main()
