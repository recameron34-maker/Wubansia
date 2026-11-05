"""FUT04 round 5 (set r5): the text-to-speech vocals are out, a wordless vox synth, its riff, nine synth accents and
three synth phrases are in, and every other round 3 sound continues as it was.

The structural checks run without rendering. The renders that prove the continuing items' bytes are round 3's are
render.py's (`--set r5` against `--set r3`, item by item); here the item list makes sure the set holds exactly those
items, from the modules round 5 leaves alone.
"""
import re
import unittest

from tools.sound import dsp48 as D
from tools.sound import kits
from tools.sound.item import Take
from tools.sound.kits import fut04 as K
from tools.sound.recipes.fut04 import accents as A, context as C, context5 as C5, midi as M, voxsynth as V

ITEMS = kits.items(K)
R3 = {it.name: it for it in ITEMS if 'r3' in it.sets}
R5 = {it.name: it for it in ITEMS if 'r5' in it.sets}
SCALE = set(K.SCALE)

# the round 3 items round 5 drops: the text-to-speech vocals, and the context loops that play them
DROPPED = {'SungLine1', 'SungLine2', 'SungLine3', 'ChopCome', 'ChopDown', 'ChopComeDown', 'ChopFur', 'ChopFurnace',
           'ChopTheFurnace', 'ChopDownToTheFurnace', 'RobotFillTheFurnace', 'RobotFillTheFurnaceWet',
           'ContextLoopClub', 'ContextLoop', 'ContextLoopWarm', 'ContextLoopWet'}
# the round 3 items round 5 keeps, byte for byte
CONTINUING = {'KickSplit', 'SnareGated', 'ClosedHat', 'OpenHat', 'Ride', 'HatLoop', 'Anvil', 'AcidClean', 'AcidWarm',
              'AcidLine1Clean', 'AcidLine1Warm', 'AcidLine2Clean', 'AcidLine2Warm', 'AcidLine1RideClean',
              'AcidLine1RideWarm', 'Rumble', 'DroneEmAdd9', 'ModBlip', 'ModBlipPhrase', 'StutterFill', 'IronRiser',
              'DroneLoop', 'RumbleUnderKick', 'StutterFillPreview', 'IronRiserPreview', 'ContextLoopLow',
              'ContextLoopLowWarm'}
NEW_MODULES = ('voxsynth', 'accents', 'context5')
ACCENTS = {'AccentRaveStabEm', 'AccentMetalChordEm', 'AccentAcidStabE4', 'AccentDistChordEm', 'AccentVoxChordEm',
           'AccentZapE5', 'AccentSubDropE2', 'AccentNoiseSweepE', 'AccentReverseHitEm'}
PHRASES = {'PhraseAcidStabs', 'PhraseMetalChords', 'PhraseRaveRiff'}
VOX = {'VoxSynth', 'VoxStabOhE3', 'VoxStabOoB3', 'VoxSynthRiff'}


def module(it):
    return it.render.__module__.rsplit('.', 1)[-1]


def in_key(notes):
    return all(n % 12 in SCALE for n in notes)


class TestRound5Set(unittest.TestCase):
    def test_r5_is_round_3_without_the_vocals_plus_the_new_items(self):
        self.assertEqual(DROPPED, set(R3) - set(R5))
        self.assertEqual(CONTINUING, set(R3) & set(R5))
        new = set(R5) - set(R3)
        self.assertEqual(VOX | ACCENTS | PHRASES | {'ContextLoopClubVox', 'ContextLoopClubNoVox', 'ContextLoopWetVox',
                                                   'ContextLoopWetNoVox', 'ContextLoopVox', 'ContextLoopVoxWarm'}, new)
        for name in new:
            self.assertIn(module(R5[name]), NEW_MODULES, name)       # new behaviour lives in new items only
        for name in CONTINUING:
            self.assertNotIn(module(R5[name]), NEW_MODULES, name)    # the continuing items render as round 3's

    def test_r5_has_no_text_to_speech_and_needs_no_models(self):
        for it in R5.values():
            self.assertNotEqual('vox', module(it), it.name)          # nothing from the TTS / WORLD / Whisper chain
            self.assertNotIn('hf_synthetic', it.flags, it.name)
            self.assertIsNone(it.script, it.name)                    # no words, so no transcript check
            self.assertNotEqual('vox', it.category, it.name)
        for mod in (V, A, C5):
            self.assertNotIn('vocals', vars(mod))                    # no speech engine imported
        for mode in (C5.VOX, C5.NOVOX):
            self.assertFalse({'chops', 'robot'} & set(C5.lane_keys(mode)))

    def test_the_vox_synth_is_wordless(self):
        for name in VOX | {'AccentVoxChordEm'}:
            it = R5[name]
            self.assertIsNone(it.script)
            self.assertNotIn('robot', it.tags)
        self.assertIn('wordless', R5['VoxSynth'].about)
        self.assertEqual({'oh', 'oo'}, set(V.LAYER_VOWEL.values()))
        self.assertEqual(5, len(V.VOWELS5['oh']))                 # five formants a vowel


class TestInKey(unittest.TestCase):
    NAME_ROOT = re.compile(r'([A-G]s?)(\d)?$')

    def test_every_new_tonal_item_is_on_a_stated_root_in_e_minor(self):
        for name in (VOX | ACCENTS) - {'VoxSynthRiff'}:
            it = R5[name]
            notes = it.notes or (it.root,)
            self.assertTrue(notes and None not in notes, name)
            self.assertTrue(in_key(notes), name)
        self.assertEqual((52, 55, 59, 62, 64, 67, 71), R5['VoxSynth'].notes)   # E3 G3 B3 D4 E4 G4 B4

    def test_names_carry_their_roots(self):
        for name in ACCENTS | {'VoxStabOhE3', 'VoxStabOoB3'}:
            it = R5[name]
            stem = name.removesuffix('m') if name.endswith('Em') else name
            m = self.NAME_ROOT.search(stem)
            self.assertTrue(m, name)
            pc, octave = m.groups()
            self.assertEqual(pc, D.note_name(it.root)[:-1], name)
            if octave:
                self.assertEqual(D.note_name(it.root), pc + octave, name)

    def test_chords_and_riffs_are_in_the_key(self):
        for chord in (A.EM, A.AM, A.BM, A.DIST, A.VOX_CHORD, A.ACID_STAB):
            self.assertTrue(in_key(chord), chord)
        self.assertTrue(in_key([m for _, m, _, _ in V.RIFF]))
        self.assertTrue(all(m in V.VOX_NOTES for _, m, _, _ in V.RIFF))          # the riff plays zones, unpitched
        self.assertTrue(in_key([m for _, m, _, _ in A.ACID_RIFF]))
        self.assertTrue(in_key([m for _, m, _, _ in C5.VOX_NOTES]))

    def test_the_phrases_are_midi_phrases_on_channel_4(self):
        phrases = M.PARTS['phrases']
        self.assertLessEqual(len(phrases), 5)
        self.assertEqual(['Vox synth riff', 'Acid stab riff', 'Metal chords, Em Em Am Bm', 'Rave stab riff'],
                         [p['name'] for p in phrases])
        self.assertEqual(V.riff_midi(), phrases[0]['notes'])
        shapes = {64: A.EM, 69: A.AM, 71: A.BM}                     # the Em sample played on E4, A4 and B4
        for p in phrases:
            self.assertIn(p['bars'], (2, 4))
            for s, n, ln, v, _ in p['notes']:
                self.assertTrue(0 <= s < 16 * p['bars'] and 0 < v <= 1 and ln > 0, p['name'])
                tones = shapes[n] if 'chord' in p['name'].lower() or 'Rave' in p['name'] else (n,)
                self.assertTrue(in_key(tones), (p['name'], n))


class TestLoops(unittest.TestCase):
    def test_the_instrumental_loop_has_no_vox_lane(self):
        self.assertIn('vox', C5.lane_keys(C5.VOX))
        self.assertNotIn('vox', C5.lane_keys(C5.NOVOX))
        self.assertEqual(C5.BASE + ('accent',), C5.lane_keys(C5.NOVOX))
        for k in C5.BASE:                                           # round 3's lanes, as round 3 mixes them
            self.assertEqual(C.LANES[k], C5.LANES5[k])

    def test_both_club_loops_feature_two_to_four_accents(self):
        for mode in (C5.VOX, C5.NOVOX):
            hits = C5.ACCENTS[mode]
            self.assertTrue(2 <= len(hits) <= 4, mode)
            for at, name in hits:
                self.assertIn(name, ACCENTS)
                self.assertTrue(at == 'end' or 0 <= at < C.STEPS)
                self.assertLess(C5.ACCENT_LUFS[name], C.LANE_LUFS['kick'] - 4.0)   # ear candy, under the drums
            self.assertTrue(any(at == 'end' for at, _ in hits))                     # one lands on the downbeat
            self.assertIn((14, 'AccentRaveStabEm'), hits)                           # the stab on the "and" of 4
        for name, mode in (('ContextLoopClubVox', C5.VOX), ('ContextLoopClubNoVox', C5.NOVOX)):
            it = R5[name]
            self.assertEqual({'club', 'wet'}, set(it.tags))
            self.assertIsNone(it.hp_hz)
            self.assertEqual(C.MASTER['max_reduction_db'], it.qa['club_max_reduction_db'])

    def test_the_vox_lane_rests_where_the_accents_play(self):
        vox_steps = {s for s, _, _, _ in C5.VOX_NOTES}
        for at, _ in C5.ACCENTS[C5.VOX]:
            self.assertNotIn(at, vox_steps)
        self.assertFalse([s for s in vox_steps if s >= C.FILL_AT])   # the fill bar is the fill's
        self.assertEqual(set(C5.VOX_THROWS), set(C5.ACCENT_THROWS[C5.NOVOX]))   # the accents take the vox's throws

    def test_the_riff_answers_the_acid_leaps(self):
        line = {s: v for s, _, _, v, _ in C.bass.LINE_BY_NAME[C.LINE].notes}
        bar1 = [s for s, _, _, _ in V.RIFF if s < 16]
        for s in bar1:                                               # on the 16th after the acid's leap on the
            self.assertEqual(2, (s - 1) % 4, s)                      # off-beat 8th, a loud note (0.85 or more)
            self.assertGreaterEqual(line.get(s - 1, 0.0), 0.85, s)


class TestVoice(unittest.TestCase):
    def test_a_short_vox_note_renders_wordless_and_in_tune(self):
        n = int(0.5 * D.SR)
        y = V.finish(V.voice(V.hz(59), 1.0, n, 'oh', 0.16, 0.04, D.rng('test', 'vox'), window=V.STAB_TUNE))
        self.assertEqual((n, 2), y.shape)
        from tools.sound import qa
        hz, share = qa.pitch_hz(y, V.hz(59), *V.STAB_TUNE)
        self.assertLess(abs(D.cents(hz, V.hz(59))), 3.0)
        self.assertGreater(share, -30.0)

    def test_the_stab_items_take_their_files_from_the_kit(self):
        it = R5['VoxStabOhE3']
        self.assertEqual('FUT04_Vox_VoxStabOhE3_v1_rr1.flac', Take(K.KIT, it, 1, 1).file)


if __name__ == '__main__':
    unittest.main()
