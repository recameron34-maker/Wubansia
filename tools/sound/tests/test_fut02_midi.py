"""FUT02's MIDI (tools/sound/recipes/fut02/midi.py), on top of test_midi.py's kit-agnostic properties: its Loop files
lane by lane against its round 5 context loop (context5.py on context.py), its roots, its phrases' channels, the drop
glide, the vox's vowels and throw, and the arp's ride. FUT02's own tests: its engineer edits them with the kit's
PARTS."""
import unittest

from tools.sound import kits
from tools.sound.recipes.fut02 import bass as B, context as C, context5 as C5, lead as L, pad as Pd
from tools.sound.recipes.fut02 import voxsynth as Vs
from tools.sound.tests.midi_testlib import (LoopCase, at, chord_roots, drum, layers_of, loop, mids, one_shot, parsed,
                                            phrase_channels, ride, v127, value_at)

KIT = 'FUT02'


class TestLoop(LoopCase, unittest.TestCase):
    KIT = KIT

    def expected(self, variant):
        a = lambda s: at(KIT, s)  # noqa: E731
        early = round(C.GLIDE_EARLY * kits.load(KIT).BPM / 60 * 960)
        version = C5.VOX if variant == 'vox' else C5.NOVOX
        out = {
            'Kick': drum(KIT, C.KICK, 36),
            'Clap': drum(KIT, C.CLAP, 38),
            'Hats': drum(KIT, C.HAT_CLOSED, 42) + drum(KIT, C.HAT_OPEN, 46),
            'Perc': drum(KIT, C.ZAP, 47),
            'Bass': [(a(s + 32 * r), n, v, None) for r in (0, 1) for s, n, _, v, _ in B.LINES[B.PICK]
                     if not C.GLIDE[0] <= s + 32 * r < C.GLIDE[1]],
            'Drop Glide': [(a(C.GLIDE[0]), 26, layers_of(KIT, 'DropGlideClean')[C.GLIDE_LAYER - 1],
                            a(C.GLIDE[1]) - early)],
            'Lead': [(a(s + 32 * r), n, v, None) for r in (0, 1) for s, n, _, v in L.phrase_notes()
                     if s + 32 * r < C.ARP_REST[0]],
            'Chords': [(0, 50, 1.0, 64 * 240)],
            'Chord voicing': [(0, n, 1.0, 64 * 240) for n in Pd.CHORD],
            'Accents': [one_shot(KIT, 'accents', item, step, layer, 'end' if ends else 'on', zone=zone)
                        for step, item, zone, layer, *_, ends in C5.ACCENTS[version]],
            'FX': [one_shot(KIT, 'fx', 'LaserFill', C.FILL_AT, 1, 'on')],
        }
        if variant == 'vox':
            out['Vox'] = [(a(s + C5.RIFF_AT), n, v, None) for s, n, _, v in Vs.RIFF]
        return out


class TestParts(unittest.TestCase):
    def test_the_pad_at_its_root(self):
        """The starfield pad Dm9 on D3 (50), on channel 4; its voicing on 3."""
        self.assertEqual(({50}, {4}, {3}), chord_roots(KIT))

    def test_the_phrases_channels(self):
        self.assertEqual({'VoxSynthRiff': 5, 'LaserStabRiff': 12, 'GlassMelody': 13, 'VoxChoirChords': 5},
                         phrase_channels(KIT))

    def test_the_vox_synths_velocity_picks_its_vowel(self):
        """Every vox note sits on a layer's velocity: 64 'oh', 95 'ah', 127 'ee'."""
        layers = layers_of(KIT, 'VoxSynth')
        self.assertEqual([64, 95, 127], [v127(x) for x in layers])
        for path in mids(KIT):
            for t in parsed(KIT, path)[2][1:]:
                if t['name'] == 'Vox':
                    self.assertLessEqual({vel for *_, vel in t['notes']}, {64, 95, 127}, path)

    def test_the_vox_throw(self):
        """The riff's A4 on bar 3's 16th 12 (the loop's 44) thrown: CC91 and CC94 at 127 there."""
        got = sorted(c[0] for c in loop(KIT, 'vox')['Vox']['ccs'] if c[2] == 91 and c[3] == 127)
        self.assertEqual([at(KIT, 44)], got)


class TestRides(unittest.TestCase):
    def test_the_arp_ride(self):
        """The arp's filter ride (context.ride_hz) on CC74, channel 2, 1.6-16 kHz: closed on the downbeat, wide open at
        16th 46, closing again; no CC71 (its Q is fixed)."""
        r = ride(KIT, 'Lead', 74)
        self.assertEqual(0, r[0])
        self.assertEqual(127, max(r.values()))
        self.assertEqual(127, value_at(r, at(KIT, 46)))
        self.assertLess(value_at(r, at(KIT, 30)), 127)
        self.assertFalse(ride(KIT, 'Lead', 71))


if __name__ == '__main__':
    unittest.main()
