"""FUT03 "Glass Current", round 5 (recipes/fut03/voxsynth.py, context5.py): the text-to-speech vocals are gone and a
wordless vox synth with its riff takes their place; the round 4 accents play in the loops. What the brief asks that the
QA can't see on its own: the r5 set holds no TTS item and renders without the models; the vox synth is wordless; every
chop, the multisample and the riff are in G minor on their stated roots; the instrumental loop has no vox lane and both
club loops have accents; every round 4 item that continues is the same item (its bytes are checked against round 4's
render, by this list). The pitch tests render the chops (a second or two); nothing here needs the vocal models."""
import re
import unittest
from unittest import mock

import tools.sound  # noqa: F401  (pins numpy's kernels before numpy loads)
from tools.sound import dsp48 as D
from tools.sound import kits, models, qa
from tools.sound.item import Take
from tools.sound.kits import fut03 as K
from tools.sound.recipes.fut03 import context as C, context5 as C5, midi as M, voxsynth as VS

SR = D.SR
TTS_MODULES = ('tools.sound.recipes.fut03.vox', 'tools.sound.recipes.fut03.hook')
NEW = {'VoxSynth', 'VoxChopOohAhG4', 'VoxChopOohAhC5', 'VoxChopAhD5', 'VoxChopStackGm7', 'PhraseVoxRiff',
       'ContextLoopVoxClub', 'ContextLoopNoVoxClub', 'ContextLoopVox', 'ContextLoopVoxWarm', 'ContextLoopVoxWet',
       'ContextLoopNoVox', 'ContextLoopNoVoxWarm', 'ContextLoopNoVoxWet'}
GONE = {'VoxHook', 'CandyRideFifth', 'CandyMeReverse', 'CandyCurStutter',   # the TTS vocals
        'ContextLoopClub', 'ContextLoop', 'ContextLoopWarm', 'ContextLoopWet'}   # round 4's loops, which play them
CONTINUING = {   # round 4's items that round 5 keeps byte for byte (the r5 render is checked against build/fut03-r4)
    'KickDeep', 'ClapSnare', 'ClosedHatDeep', 'OpenHatDeep', 'ShakerDeep', 'Conga', 'Bongo', 'Rimshot', 'PercLoop',
    'RubberBassClean', 'RubberBassWarm', 'SubDeep', 'LineRubberClean', 'LineRubberWarm', 'LineDeepClean',
    'LineDeepWarm', 'DubStabGm9', 'PadWarm', 'DubChordLoop', 'BuildFill', 'Riser', 'Impact', 'AccentDubChordCm9',
    'AccentOrganHitGm7', 'AccentRubberStabG1', 'AccentPluckChordDm7', 'AccentNoiseSweepUp', 'AccentZapG4',
    'AccentReverseChordGm9', 'AccentVoxSynthGm', 'AccentGlassPingD6', 'KeysEP', 'PhraseRubberRiff', 'PhraseDubChords',
    'PhraseKeysRiff', 'PhraseKeysArp', 'ContextLoopLow', 'ContextLoopLowWarm', 'DrumLoop', 'FillPreview'}
GM9, CM9 = VS.CHORD_PCS['Gm9'], VS.CHORD_PCS['Cm9']


def r5():
    return kits.items(K, sets=('r5',))


def item(name):
    return kits.items(K, only=(name,))[0]


def in_key(note):
    return int(note) % 12 in K.SCALE


class TestTheSet(unittest.TestCase):
    def test_r5_is_round_4_without_the_tts_vocals_plus_the_new_items(self):
        names = {it.name for it in r5()}
        r4 = {it.name for it in kits.items(K, sets=('r4',))}
        self.assertEqual(CONTINUING | NEW, names)
        self.assertEqual(r4 - GONE, CONTINUING)
        self.assertFalse(NEW & r4, NEW & r4)                       # new behaviour, new names: --set r4 is round 4

    def test_r4_still_holds_round_4(self):
        r4 = {it.name for it in kits.items(K, sets=('r4',))}
        self.assertEqual(48, len(r4))
        self.assertLessEqual(GONE, r4)

    def test_no_item_in_r5_is_text_to_speech(self):
        """No r5 item comes from the TTS recipes, sings a script or carries the TTS flag; no loop plays a TTS item."""
        for it in r5():
            self.assertNotIn(it.render.__module__, TTS_MODULES, it.name)
            self.assertIsNone(it.script, it.name)
            self.assertNotIn('hf_synthetic', it.flags, it.name)
        sources = ' '.join(str(v[0]) for v in C5.LANES.values())
        for name in GONE:
            self.assertNotIn(name, sources)
        self.assertFalse(set(C5.LANES) & set(C5.OLD))

    def test_the_vox_synth_renders_without_the_models(self):
        """Every model loader raises; the vox synth's chops still render (the multisample and riff use the same
        voice)."""
        with mock.patch.object(models, '_model', side_effect=AssertionError('a model was loaded')), \
                mock.patch.object(models, 'tts', side_effect=AssertionError('TTS ran')), \
                mock.patch.object(models, 'asr', side_effect=AssertionError('ASR ran')):
            for name in VS.CHOPS:
                it = item(name)
                y = it.render(Take('FUT03', it, 1, 1)).audio
                self.assertGreater(len(y), SR // 4, name)

    def test_the_vox_synth_is_wordless(self):
        import inspect
        src = inspect.getsource(VS)
        for word in ('vocals', 'models', 'kokoro', 'world', 'whisper', 'script='):
            self.assertNotIn(f'import {word}', src.lower())
        self.assertNotIn('script=', src)
        for it in r5():
            if it.render.__module__ == VS.__name__:
                self.assertIn('wordless', it.about, it.name)


class TestInTheKey(unittest.TestCase):
    def test_the_multisample_is_on_stated_roots_in_the_key(self):
        it = item('VoxSynth')
        self.assertEqual((55, 62, 67, 74), it.notes)               # G3 D4 G4 D5
        self.assertTrue(all(in_key(n) for n in it.notes))
        self.assertIn('G3 D4 G4 D5', it.about)

    def test_each_chop_states_its_root_in_the_key(self):
        """The root is in the name and in `root`, inside G3-D5; the stack's notes are Gm7."""
        names = {'VoxChopOohAhG4': 67, 'VoxChopOohAhC5': 72, 'VoxChopAhD5': 74, 'VoxChopStackGm7': 67}
        self.assertEqual(set(names), set(VS.CHOPS))
        for name, root in names.items():
            it = item(name)
            self.assertEqual(root, it.root, name)
            self.assertTrue(55 <= root <= 74 and in_key(root), name)
            self.assertTrue(all(in_key(n) for n in VS.CHOPS[name][0]), name)
            self.assertIn(f'root {D.note_name(root)}', it.about, name)
        self.assertEqual((67, 70, 74, 77), VS.CHOPS['VoxChopStackGm7'][0])

    def test_each_single_chop_sounds_its_root(self):
        """The fundamental over the chop's body (after the scoop) reads within 10 cents of the root."""
        for name, (notes, length, _, _, _) in VS.CHOPS.items():
            if len(notes) > 1:
                continue
            it = item(name)
            y = it.render(Take('FUT03', it, 1, 1)).audio
            t0 = VS.LEAD + VS.SCOOP[1] + 0.02
            f = float(D.midi_hz(it.root))
            hz, share = qa.pitch_hz(y, f, t0, VS.LEAD + length)
            self.assertGreater(share, -30.0, name)
            self.assertLess(abs(D.cents(hz, f)), 10.0, (name, hz, f))

    def test_the_riff_is_off_beat_chord_tones_on_the_kits_grid(self):
        """2 bars: Gm9 tones in bar 1, Cm9 tones in bar 2, every chop off the beat (no 16th on a kick), the throw on the
        last; the MIDI export carries the same notes as a phrase on the vox synth (channel 5)."""
        self.assertEqual(32, VS.RIFF_STEPS)
        self.assertEqual(('Gm9', 'Cm9'), VS.RIFF_CHORDS)
        for s, m, ln, v in VS.VOX_RIFF:
            self.assertTrue(in_key(m), m)
            self.assertIn(m % 12, GM9 if s < 16 else CM9, (s, m))
            self.assertTrue(s % 4, s)
            self.assertTrue(55 <= m <= 79, m)
        self.assertEqual(30, VS.VOX_RIFF[-1][0])                    # the throw: the "and" of 4 in bar 2
        ph = [p for p in M.PARTS['phrases'] if p['item'] == 'VoxSynth']
        self.assertEqual(1, len(ph))
        self.assertEqual([(s, m, ln, v, False) for s, m, ln, v in VS.VOX_RIFF], ph[0]['notes'])
        self.assertEqual(2, ph[0]['bars'])

    def test_the_vox_sits_above_the_low_mids(self):
        self.assertGreaterEqual(VS.HP, 250.0)
        for name in VS.CHOPS:
            self.assertNotIn('mud', item(name).qa)                  # the category's low cut stays on
        self.assertEqual('lowcut', item('PhraseVoxRiff').qa['mud'])


class TestTheLoops(unittest.TestCase):
    def test_the_instrumental_loop_has_no_vox_lane(self):
        self.assertNotIn('vox', C5.KEYS['novox'])
        self.assertIn('vox', C5.KEYS['vox'])
        for name, fn in (('ContextLoopNoVoxClub', C5.novox_club), ('ContextLoopNoVox', C5.novox_clean),
                         ('ContextLoopNoVoxWarm', C5.novox_warm), ('ContextLoopNoVoxWet', C5.novox_wet)):
            self.assertIs(fn, item(name).render)

    def test_both_club_loops_have_two_to_four_accents(self):
        """Round 4's accent synths, each an r5 item, two to four hits per 4 bars, on off-beat 16ths or end-aligned."""
        accents = {it.name for it in r5() if it.name.startswith('Accent')}
        for v in C5.VARIANTS:
            self.assertIn('accent', C5.KEYS[v])
            hits = C5.ACCENTS[v]
            self.assertTrue(2 <= len(hits) <= 4, v)
            for step, name in hits:
                self.assertIn(name, accents)
                self.assertTrue(step == 'end' or step % 4, (v, step))
                self.assertIn(name, C5.ACCENT_TRIM_DB)

    def test_the_club_loops_keep_round_4s_master(self):
        for name in ('ContextLoopVoxClub', 'ContextLoopNoVoxClub'):
            it = item(name)
            self.assertIn('club', it.tags)
            self.assertEqual(C.MASTER_REDUCTION_DB, it.qa['club_max_reduction_db'])
            self.assertTrue(it.qa['why'])
            self.assertIsNone(it.hp_hz)

    def test_the_round_4_lanes_play_as_they_did(self):
        """Every lane but the vox and the accents is round 4's own (context.LANES), with its fader, pan and sends."""
        for k, v in C5.LANES.items():
            if k not in ('vox', 'accent'):
                self.assertEqual(C.LANES[k], v, k)
        self.assertEqual(C.HOOK_AT, C5.VOX_AT)

    def test_the_dry_a_b_plays_at_one_loudness(self):
        for v in C5.VARIANTS:
            name = 'ContextLoopVox' if v == 'vox' else 'ContextLoopNoVox'
            self.assertIsNone(item(name).level)
            self.assertEqual(C5.DRY_B_LUFS[v], item(name + 'Warm').level)

    def test_no_artist_names(self):
        from tools.sound import denylist
        lists = denylist.load('artists')
        texts = [it.about for it in r5() if it.name in NEW] + [ph['name'] for ph in M.PARTS['phrases']]
        for t in texts:
            self.assertFalse(denylist.hits(t, lists), t)
        self.assertTrue(all(re.fullmatch(r'[A-Z][A-Za-z0-9]*', n) for n in NEW))


if __name__ == '__main__':
    unittest.main()
