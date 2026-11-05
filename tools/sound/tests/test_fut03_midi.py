"""FUT03's MIDI (tools/sound/recipes/fut03/midi.py), on top of test_midi.py's kit-agnostic properties: its Loop files
lane by lane against its round 5 context loop (context5.py on context.py), its roots, its phrases' channels, the pad on
channel 11, the dub stab's ride, and its written fill bar. FUT03's own tests: its engineer edits them with the kit's
PARTS."""
import unittest

from tools.sound import channels as CH
from tools.sound.recipes.fut03 import accents as A, context as C, context5 as C5, rubber as B, voxsynth as VS
from tools.sound.recipes.fut03 import midi as P3
from tools.sound.tests.midi_testlib import (BAR, LoopCase, at, chord_roots, drum, kit_files, lane_ons, loop, one_shot,
                                            parse, phrase_channels, ride, tick_step, value_at, whole)

KIT = 'FUT03'


class TestLoop(LoopCase, unittest.TestCase):
    KIT = KIT

    def expected(self, variant):
        a = lambda s: at(KIT, s)  # noqa: E731
        bass = []
        for r in (0, 1):
            for s, ln, n, v, glide in B.LINES[B.PICK]:
                bass.append((a(s + 32 * r), n, v, None))
                if glide:                   # a glide inside the note: its target slid into from the glide's start
                    bass.append((at(KIT, s + 32 * r + ln * glide[1]), glide[0], v, None))
        out = {
            'Kick': drum(KIT, C.KICK, 36),
            'Clap': drum(KIT, C.CLAP, 38),
            'Hats': drum(KIT, [(s, v) for s, kind, v in C.HATS if kind == 'closed'], 42)
            + drum(KIT, [(s, v) for s, kind, v in C.HATS if kind == 'open'], 46),
            'Perc': drum(KIT, C.SHAKER, 54) + [(a(s), 37 if n == 'Rimshot' else P3.HAND[(n, z)], v, a(s) + 120)
                                               for s, n, z, v in C.PERC],
            'Bass': bass,
            'Chords': [(a(s), 55, v, a(s) + 360) for s, v in C.STAB],
            'Chord voicing': [(a(s), n, v, a(s) + 360) for s, v in C.STAB for n in A.GM9],
            'Pad Warm': [(0, 55, 1.0, 64 * 240)],
            'Accents': [one_shot(KIT, 'accents', name, 64 if step == 'end' else step, 1,
                                 'end' if step == 'end' else 'on') for step, name in C5.ACCENTS[variant]],
        }
        if variant == 'vox':
            out['Vox'] = [(a(s + C5.VOX_AT), m, v, None) for s, m, _, v in VS.VOX_RIFF]
        return out


class TestParts(unittest.TestCase):
    def test_the_dub_stab_at_its_root(self):
        """The dub stab on G3 (55): Gm9, on channel 4; its voicing on 3; the dub chords phrase plays Cm9 on C4 (60),
        the same file a fourth up."""
        self.assertEqual(({55}, {4}, {3}), chord_roots(KIT))
        phrase = parse(kit_files(KIT)['FUT03_Phrase_DubChords_4bars.mid'])[2][1]['notes']
        self.assertEqual({55, 60}, {key for *_, key, _ in phrase})

    def test_the_phrases_channels(self):
        self.assertEqual({'RubberRiff': 1, 'DubChords': 4, 'KeysArp': 2, 'VoxSynthRiff': 5}, phrase_channels(KIT))

    def test_the_pad_plays_on_channel_11_and_no_loop_lead(self):
        tracks = loop(KIT, 'vox')
        self.assertEqual({11}, {ch for _, _, ch, _, _ in tracks['Pad Warm']['notes']})
        self.assertNotIn('Lead', tracks)

    def test_no_throws(self):
        """FUT03's riff throw is baked into its stem: no CC91/CC94."""
        for v in ('vox', 'novox'):
            self.assertFalse([c for t in loop(KIT, v).values() for c in t['ccs'] if c[2] in (91, 94)])

    def test_the_fill_bar_leaves_the_build_fill_its_room(self):
        """Audit A defect 7: at every Fill cue the drums play the kit's FillPreview bar (context.FILL_*): the kick,
        hats and shaker on beats 1-3, the clap on beat 2, no congas, bongos or rim, beat 4 empty, under the Build
        Fill."""
        R, tracks, _ = whole(KIT, 'vox')
        fills = [c['bar'] for s in R['sections'] for c in s['cues'] if c['cue'] == 'Fill']
        self.assertTrue(fills)
        for bar in fills:
            lo = (bar - 1) * BAR
            with self.subTest(bar=bar):
                drums = [(tick_step(KIT, on - lo), key) for lane in CH.DRUM_LANES
                         for on, key, _ in lane_ons(KIT, tracks, lane, lo, lo + BAR)]
                self.assertFalse([d for d in drums if d[0] >= 12])
                self.assertFalse([d for d in drums if d[1] in (37, 41, 43, 45, 47, 48)])
                self.assertLessEqual({s for s, key in drums if key == 38}, {4})


class TestRides(unittest.TestCase):
    def test_the_dub_stab_ride(self):
        """The dub stab's low-pass (context.stab_lane) on CC74, channel 4, 900-5000 Hz: closed on the loop's downbeat,
        open at its middle; no CC71 (its Q is fixed)."""
        r = ride(KIT, 'Chords', 74)
        self.assertEqual(0, r[0])
        self.assertEqual(127, value_at(r, 32 * 240))
        self.assertLess(value_at(r, 16 * 240), 127)
        self.assertFalse(ride(KIT, 'Chords', 71))


if __name__ == '__main__':
    unittest.main()
