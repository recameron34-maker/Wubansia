"""FUT02 "Orbit Relay" round 5 (set r5): round 3's kit byte for byte without its text-to-speech vocals, a wordless vox
synth in their place, and synth accents and phrases. The set is the whole kit and needs no vocal models; the vox synth
is synthesis with no words; the vox synth, its chops and riff, the accents and the phrases are in D minor on their
stated roots (the rendered pitch reads true); the instrumental loop has no vox lane; both club loops carry two to four
accents, levelled under the drums; the continuing items are round 3's, and the cheap ones render round 3's bytes.
Renders the vox synth, the pitched accents and the loops' lanes (about a minute); nothing here needs the models."""
import hashlib
import platform
import sys
import unittest
from unittest import mock

import numpy as np

from tools.sound import dsp48 as D
from tools.sound import kits
from tools.sound import mixmeter as MM
from tools.sound import phrase as Ph
from tools.sound import qa
from tools.sound import render as R
from tools.sound import vocals
from tools.sound.kits import fut02 as K
from tools.sound.recipes.fut02 import accents as A
from tools.sound.recipes.fut02 import context as C
from tools.sound.recipes.fut02 import context5 as C5
from tools.sound.recipes.fut02 import lead as Ld
from tools.sound.recipes.fut02 import midi as Mi
from tools.sound.recipes.fut02 import vox
from tools.sound.recipes.fut02 import voxsynth as Vs

RENDER_HOST = sys.platform.startswith('linux') and platform.machine() == 'x86_64'
TTS = {it.name for it in vox.ITEMS}                      # round 3's text-to-speech vocals: three lines, eight chops,
VOCAL_LOOPS = {'ContextLoopClub', 'ContextLoop', 'ContextLoopWarm', 'ContextLoopWet'}   # the vocoder; the loops on them
LOW_LOOPS = {'ContextLoopLow', 'ContextLoopLowWarm'}     # round 3's low end: the round 5 loops play the bass 1.5 dB up
CONTINUING = {'KickSplit', 'ElectroClap', 'ClosedHat', 'OpenHat', 'HatLoop', 'Zap', 'RollBassClean', 'RollBassWarm',
              'OrbitLineClean', 'OrbitLineWarm', 'RelayLineClean', 'RelayLineWarm', 'DropGlideClean', 'DropGlideWarm',
              'StarfieldPadDm9', 'StarfieldPadLoop', 'ChromeArp', 'ChromeArpPhrase', 'LaserFill', 'LaserRiser',
              'OrbitGrooveClean', 'OrbitGrooveWarm', 'RelayGrooveClean', 'RelayGrooveWarm', 'LaserFillPreview',
              'LaserRiserPreview'}
NEW = {'VoxSynth', 'VoxChopAhD5', 'VoxChopEeF5', 'VoxChopOhA4', 'VoxChopEeAhC5', 'VoxChopAhOhA5', 'VoxSynthRiff',
       'LaserChordStab', 'StarfieldSweep', 'GlassPing', 'ResonantZap', 'SubDrop', 'ReverseChord', 'NoiseUplifter',
       'ArpStab', 'VoxSynthStab', 'LaserStabRiff', 'GlassMelody', 'VoxChoirChords', 'ContextVoxSynthClub',
       'ContextNoVoxClub', 'ContextVoxSynthWet', 'ContextNoVoxWet', 'ContextVoxSynth', 'ContextVoxSynthWarm',
       'ContextNoVox', 'ContextNoVoxWarm', 'ContextLow5', 'ContextLow5Warm'}
ACCENTS = {'LaserChordStab', 'StarfieldSweep', 'GlassPing', 'ResonantZap', 'SubDrop', 'ReverseChord', 'NoiseUplifter',
           'ArpStab', 'VoxSynthStab'}
# round 3's masters of three continuing items (build/fut02-r3/qa_report.json): round 5 renders the same bytes
R3_SHA = {
    'FUT02_Perc_Zap_v1_rr1.flac': 'bc84ead12df927361e16c626d9d4e293f147a4295eb5ffd73596ffcbe8a67459',
    'FUT02_Perc_Zap_v3_rr2.flac': '628b840dc1f9abbfe6474621e4acbe6bbcb8fd9b06e8de4391784141e9cbd62d',
    'FUT02_Perc_Zap_v5_rr1.flac': '3d5b125021e0bec4d05853fbae79215ba5e709cbf44bc902c91f687e19807fd6',
    'FUT02_Drums_ElectroClap_v1_rr1.flac': 'f272a6ea50f34926c719c155052a76fc838c62f7ecf03b9aa145278e23c202eb',
    'FUT02_Drums_ElectroClap_v5_rr2.flac': '1f0ceaa615ad456a90d8fc915d21012e7aaaa9e970eb57aab24d492adbf5bd30',
    'FUT02_FX_LaserFill_v1_rr1.flac': '9f70356a24384f3a003fea082df1f24ab77b7743134e71ed9f4b00b068376f0d',
}


def in_key(note):
    return int(note) % 12 in K.SCALE


def cents_off(x, f0, window):
    hz, share = qa.pitch_hz(x, f0, *window)
    return float(D.cents(hz, f0)), share


class TestTheSet(unittest.TestCase):
    def setUp(self):
        self.r5 = {it.name: it for it in kits.items(K, sets=('r5',))}
        self.r3 = {it.name: it for it in kits.items(K, sets=('r3',))}

    def test_r5_is_the_whole_kit_without_the_tts_vocals(self):
        self.assertEqual(CONTINUING | NEW, set(self.r5))
        self.assertEqual(CONTINUING, set(self.r3) - TTS - VOCAL_LOOPS - LOW_LOOPS)
        self.assertEqual(set(), (TTS | VOCAL_LOOPS | LOW_LOOPS) & set(self.r5))

    def test_continuing_items_are_round_3_s(self):
        """Every continuing item is round 3's own (r3 still renders it), so its audio is round 3's; the r3 set is
        untouched: 45 items, with the vocals and their loops."""
        for name in CONTINUING:
            with self.subTest(name=name):
                self.assertIs(self.r3[name], self.r5[name])
        self.assertEqual(45, len(self.r3))
        self.assertLessEqual(TTS | VOCAL_LOOPS | LOW_LOOPS, set(self.r3))

    def test_no_item_needs_the_vocal_models(self):
        """No r5 item has a script, the hf_synthetic flag (a resynthesised voice) or a render from the TTS module."""
        for it in self.r5.values():
            with self.subTest(name=it.name):
                self.assertIsNone(it.script)
                self.assertNotIn('hf_synthetic', it.flags)
                self.assertNotEqual(vox.__name__, it.render.__module__)

    def test_the_vox_synth_renders_without_the_speech_chain(self):
        """The vox synth, a chop and a stab render with every entry point of the speech engine and the vocoder chain
        made to fail: none is called."""
        boom = mock.Mock(side_effect=AssertionError('the speech chain was called'))
        names = [n for n in dir(vocals) if callable(getattr(vocals, n)) and not n.startswith('_')
                 and getattr(getattr(vocals, n), '__module__', '') == vocals.__name__]
        with mock.patch.multiple(vocals, **{n: boom for n in names}):
            for name, note, layer in (('VoxSynth', 69, 2), ('VoxChopEeAhC5', None, 1), ('VoxSynthStab', None, 2)):
                it = kits.items(K, only=(name,))[0]
                take = [t for t in it.takes(K.KIT) if t.note == note and t.layer == layer][0]
                self.assertTrue(np.all(np.isfinite(R.as_out(it.render(take)).audio)))
        boom.assert_not_called()

    @unittest.skipUnless(RENDER_HOST, 'masters are hashed on x86_64 Linux only (ADR 0007)')
    def test_continuing_items_render_round_3_s_bytes(self):
        for name in ('Zap', 'ElectroClap', 'LaserFill'):
            done = R.render_item(kits.items(K, only=(name,))[0], K.KIT)
            got = {t.file: hashlib.sha256(R.encode(x)).hexdigest() for t, x in zip(done.takes, done.audio)}
            for f, want in R3_SHA.items():
                if f in got:
                    with self.subTest(file=f):
                        self.assertEqual(want, got[f])


class TestTheVoxSynth(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.items = {n: kits.items(K, only=(n,))[0] for n in ('VoxSynth',) + tuple(c[0] for c in Vs.CHOPS)}
        cls.files = {n: Ph.kit_files(K, n)[1] for n in cls.items}

    def test_wordless(self):
        """No script, no words: every vowel it sings is one of the formant tables, the velocity picks 'oh', 'ah' or
        'ee', and the riff, the chops and the choir chords sing only those."""
        for it in self.items.values():
            self.assertIsNone(it.script)
        self.assertEqual(('oh', 'ah', 'ee'), Vs.LAYER_VOWEL)
        vowels = {v for _, _, path, _, _, _ in Vs.CHOPS for v, _ in path} | {Vs.vowel_of(v) for *_, v in Vs.RIFF}
        vowels |= {Vs.vowel_of(v) for _, v in A.CHOIR_BARS}
        self.assertLessEqual(vowels, set(Vs.VOWELS) - {'d'})

    def test_zones_and_chops_in_the_key_on_their_roots(self):
        """D4 to A5 in D minor; each zone, layer and chop reads its stated root within QA's 3 cents, on the steady
        note before the vibrato."""
        self.assertTrue(all(in_key(n) for n in Vs.ZONES))
        self.assertEqual((62, 81), (min(Vs.ZONES), max(Vs.ZONES)))
        for (note, layer, rr), o in self.files['VoxSynth'].items():
            with self.subTest(note=note, layer=layer):
                c, share = cents_off(o.audio, float(D.midi_hz(note)), Vs.VOX_QA['tune_window'])
                self.assertLess(abs(c), 3.0)
                self.assertGreater(share, -30.0)
        for name, root, *_ in Vs.CHOPS:
            with self.subTest(chop=name):
                self.assertTrue(in_key(root))
                self.assertEqual(root, self.items[name].root)
                c, _ = cents_off(self.files[name][(None, 1, 1)].audio, float(D.midi_hz(root)),
                                 Vs.CHOP_QA['tune_window'])
                self.assertLess(abs(c), 3.0)

    def test_velocity_opens_the_vowel(self):
        """Each layer brighter and at least 1.5 dB louder than the one under it, on every zone (QA's layer rules)."""
        for note in Vs.ZONES:
            xs = [self.files['VoxSynth'][(note, layer, 1)].audio for layer in (1, 2, 3)]
            with self.subTest(note=note):
                self.assertTrue(all(b > a for a, b in zip(map(qa.centroid_hz, xs), map(qa.centroid_hz, xs[1:]))))
                self.assertGreaterEqual(min(np.diff([qa.lufs(x) for x in xs])), qa.VEL_STEP_MIN_DB)

    def test_above_the_low_mids_and_centred_below_150(self):
        for (note, layer, rr), o in self.files['VoxSynth'].items():
            with self.subTest(note=note, layer=layer):
                self.assertLessEqual(qa.low_share_db(o.audio), qa.LOWCUT_MAX_DB)
                side = qa.low_side_db(o.audio)
                self.assertTrue(side is None or side <= qa.LOW_SIDE_MAX_DB)


class TestRiffAccentsPhrases(unittest.TestCase):
    def test_the_riff_locks_with_the_arp(self):
        """2 bars in D minor; each bar opens on the arp's 3-3-2 accents a third under its motif (D5 F5 E5 under F5 A5
        G5), and ends on the throw."""
        self.assertTrue(all(in_key(m) for _, m, _, _ in Vs.RIFF))
        arp = {s: m for s, m, _ in Ld.MELODY}
        for bar in (0, 16):
            got = {s - bar: m for s, m, _, _ in Vs.RIFF if bar <= s < bar + 8}
            for s in (0, 3, 6):
                with self.subTest(step=bar + s):
                    self.assertIn(arp[bar + s] - got[s], (3, 4))          # a minor or major third under
        self.assertEqual(Vs.THROW_AT, max(s for s, *_ in Vs.RIFF))
        self.assertEqual(Vs.RIFF_STEPS, 32)

    def test_accents_in_the_key_on_their_roots(self):
        items = {it.name: it for it in kits.items(K, sets=('r5',)) if it.name in ACCENTS}
        self.assertEqual(ACCENTS, set(items))
        for it in items.values():
            with self.subTest(name=it.name):
                for n in it.notes or (it.root,):
                    self.assertIsNotNone(n)
                    self.assertTrue(in_key(n))
        for chord in list(A.STAB_CHORDS.values()) + [A.ARP_STAB, A.VOX_STAB, A.STARS]:
            self.assertTrue(all(in_key(n) for n in chord))
        for root, chord in A.STAB_CHORDS.items():
            self.assertEqual(root, chord[0])
            self.assertGreaterEqual(chord[1] - chord[0], 5)               # the root alone under the rest

    def test_pitched_accents_read_their_roots(self):
        for name in ('LaserChordStab', 'GlassPing', 'ResonantZap', 'ArpStab', 'VoxSynthStab'):
            it, got, _ = Ph.kit_files(K, name)
            window = it.qa.get('tune_window', (0.10, 0.60))
            for (note, layer, rr), o in got.items():
                f0 = float(D.midi_hz(note if note is not None else it.root))
                with self.subTest(name=name, note=note, layer=layer, rr=rr):
                    c, _ = cents_off(o.audio, f0, (window[0], min(window[1], len(o.audio) / D.SR)))
                    self.assertLess(abs(c), 3.0)

    def test_phrases_in_the_key_and_in_the_midi(self):
        """Every phrase note is in D minor; the MIDI carries the vox riff and the three phrases (at most 5), each
        on its sample, on 2 or 4 bars."""
        phrases = Mi.PARTS['phrases']
        self.assertEqual(4, len(phrases))
        self.assertEqual(Vs.riff_midi(), phrases[0]['notes'])
        for ph in phrases:
            with self.subTest(phrase=ph['name']):
                self.assertIn(ph['bars'], (2, 4))
                self.assertTrue(all(in_key(n) for _, n, _, _, _ in ph['notes']))
                self.assertTrue(all(0 <= s < 16 * ph['bars'] for s, *_ in ph['notes']))


class TestTheLoops(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.mix = {v: C5.lanes(v, C.CLEAN) for v in (C5.VOX, C5.NOVOX)}

    def test_the_instrumental_has_no_vox_lane(self):
        mix, vparts, aparts = self.mix[C5.NOVOX]
        self.assertNotIn('vox', mix)
        self.assertEqual((), vparts)
        self.assertFalse(any('Vox' in row[1] for _, row in aparts))
        self.assertIn('vox', self.mix[C5.VOX][0])

    def test_both_loops_feature_two_to_four_accents(self):
        """Two to four accents per 4 bars, every one an r5 accent, each part at its declared momentary level and, over
        its loudest 3 dB, at least 5 LU under the percussion (the drum bus and the fill, which is bar 4's drums)."""
        for v in (C5.VOX, C5.NOVOX):
            mix, _, aparts = self.mix[v]
            self.assertTrue(2 <= len(aparts) <= 4)
            perc = MM.lufs_m(sum(mix[k] for k in C.DRUM_BUS) + mix['fill'])
            for part, row in aparts:
                with self.subTest(version=v, accent=row[1]):
                    self.assertIn(row[1], ACCENTS)
                    m = MM.lufs_m(part)
                    self.assertAlmostEqual(row[4], float(m.max()), places=6)
                    loud = m > m.max() - 3.0
                    self.assertGreaterEqual(float(np.min(perc[loud] - m[loud])), 5.0)

    def test_the_vox_riff_takes_the_old_vocals_place(self):
        """From bar 2's downbeat, where round 3's robot called; its throw lands in bar 3 and falls into the fill bar."""
        self.assertEqual(C.ROBOT_AT, C5.RIFF_AT)
        self.assertTrue(C5.RIFF_AT + Vs.THROW_AT < C.FILL_AT)
        self.assertAlmostEqual(C5.VOX_LUFS, qa.lufs(self.mix[C5.VOX][0]['vox']), places=6)

    def test_the_low_end_is_round_3_s_with_the_bass_up(self):
        mix = self.mix[C5.NOVOX][0]
        self.assertTrue(np.array_equal(C.faded('kick', C.kick_raw(), C.CLEAN), mix['kick']))
        ratio = mix['bass'] / np.where(np.abs(C.low_lanes(C.CLEAN)['bass']) > 1e-9, C.low_lanes(C.CLEAN)['bass'],
                                       np.inf)
        self.assertAlmostEqual(D.undb(C5.R5_FADERS['bass']), float(np.max(ratio)), places=9)

    def test_the_club_loops_declare_round_3_s_cap(self):
        for name in ('ContextVoxSynthClub', 'ContextNoVoxClub'):
            it = kits.items(K, only=(name,))[0]
            self.assertIn('club', it.tags)
            self.assertEqual(C.MASTER_REDUCTION_DB, it.qa['club_max_reduction_db'])
            self.assertEqual(C.CLUB_QA['club_crest_loss_db'], it.qa['club_crest_loss_db'])


if __name__ == '__main__':
    unittest.main()
