"""FUT01's round 5 (set r5): the text-to-speech vocals gone, a wordless vox synth and its riff in their place, the synth
accents in the loops. The set is round 4's kit without any item of the speech chain, plus the new items; the vox
synth is synthesis only (no script, no models, no speech modules) and every chop, zone and riff note sits in F# minor
on its stated root; the instrumental loop has no vox lane; both club loops play two to four accents; the continuing
items keep round 4's bytes (a sample of them rendered against round 4's hashes on the render host). Renders a few
small items (about 20 s); nothing here needs the vocal models."""
import ast
import inspect
import os
import platform
import sys
import tempfile
import unittest
from unittest import mock

import numpy as np

from tools.sound import dsp48 as D
from tools.sound import kits
from tools.sound import qa
from tools.sound import render as Rn
from tools.sound.kits import fut01 as K
from tools.sound.recipes.fut01 import accents as A
from tools.sound.recipes.fut01 import context4 as C4
from tools.sound.recipes.fut01 import context5 as C5
from tools.sound.recipes.fut01 import hooks as H
from tools.sound.recipes.fut01 import midi as Mi
from tools.sound.recipes.fut01 import vox
from tools.sound.recipes.fut01 import voxsynth as V

SR = D.SR
RENDER_HOST = sys.platform.startswith('linux') and platform.machine() == 'x86_64'
SPEECH_MODULES = ('tools.sound.recipes.fut01.vox', 'tools.sound.recipes.fut01.hooks')
# round 4's loops that played the speech-chain vocals: round 5's own loops (context5.py) replace them
SPEECH_LOOPS = {'GarageContext', 'GarageContextWarm', 'GarageContextWet', 'GarageContextClub'}
NEW = {'VoxSynth', 'VoxChopAhCs5', 'VoxChopEeE5', 'VoxChopAhEeFs5', 'VoxChopOhA4', 'VoxChopAhOhB4', 'VoxSynthRiff',
       'GarageVox', 'GarageVoxWarm', 'GarageVoxWet', 'GarageVoxClub', 'GarageNoVox', 'GarageNoVoxWarm',
       'GarageNoVoxWet', 'GarageNoVoxClub', 'SpeedGarageVox'}
CONTINUING = {
    'KickGarage', 'KickSplit', 'ClapGarage', 'HatClosed', 'HatOpen', 'Tambourine', 'Shaker', 'RimShot', 'AlloyHit',
    'AlloyHitShort', 'WarpClean', 'WarpWarm', 'OrganBass', 'OrganStab', 'EPianoStab', 'AirPadFsm9', 'AirPadHold',
    'ChromePluck', 'ChromePluckPhrase', 'GlitchFillLift', 'ShiftRiser', 'OrganHit', 'WarpStab', 'RaveStab',
    'ChordSweep', 'ReverseStab', 'SubDrop', 'AlloyBell', 'VoxSynthStab', 'NoiseUplifter', 'OrganLead', 'OrganRiff',
    'WarpRiff', 'PluckRiff', 'StabRhythm', 'GarageContextLow', 'GarageContextLowWarm', 'SpeedGarageLoop',
    'SpeedGarageLow', 'DrumGroove4Garage', 'DrumGroove4Split', 'DrumGroove4Swing57', 'WarpLineClean', 'WarpLineWarm',
    'OrganLine'}
# round 4's masters of a sample of the continuing items (build/fut01-r4/qa_report.json): round 5 renders them unchanged
R4_SHA = {
    'FUT01_Drums_HatClosed_v1_rr1.flac': '6418facce09140a5f66a642a3ea40299e4c09765cd6c76e65cc64957fec68ab2',
    'FUT01_Drums_HatClosed_v1_rr2.flac': '33e7f21dbed8ed2987fdc4c5ebc6f14ca55960f67c9a8e4d288c1ac6ce8cb584',
    'FUT01_Drums_HatClosed_v1_rr3.flac': '6f1a0e2ee63756e39f7acaacb689b59ef18774cb6f8d2ff9479f952df70e0823',
    'FUT01_Drums_HatClosed_v2_rr1.flac': '1f96c036c9039cc7b976cfff99657964a3861f5cb2b9fea82dde3e6867703c19',
    'FUT01_Drums_HatClosed_v2_rr2.flac': '5e99dc075b9892a5e19c2fdfc54dbee613a245155ad343796ccf8332b7539713',
    'FUT01_Drums_HatClosed_v2_rr3.flac': '792634c567d5fe788a4ad4b69ff7f089acc74f60e3bf5cf302717decbd56904c',
    'FUT01_Drums_HatClosed_v3_rr1.flac': '8119689a5671d8ca870855543ed475dd566e9e68458f436132244b7ef18598d4',
    'FUT01_Drums_HatClosed_v3_rr2.flac': '50e074a9634b3c36088fe6893eeedcd2fd0f90a26550df4dd416e169296ef064',
    'FUT01_Drums_HatClosed_v3_rr3.flac': '0cc1dedc4d9851552970bfa3ce8a6315f84ea8cd5ea355661f7afdee4c7ea89a',
    'FUT01_Drums_HatClosed_v4_rr1.flac': 'b4a371a00ff77307bc62db9a2dd9a0c4e4af3e92072e659e4154de7f56957dea',
    'FUT01_Drums_HatClosed_v4_rr2.flac': '56ff8b1d0e324d39379d7a387c9834a40c735d241accadf58ecda957b0942045',
    'FUT01_Drums_HatClosed_v4_rr3.flac': '55004c56514d8078209aa35f1b985b2904ddf9f3226eb74009e911f3df76a9fa',
    'FUT01_Drums_HatClosed_v5_rr1.flac': '53c091d5aacdace968869688d80c8948fe1ebeedab78a44633925953b1c22f20',
    'FUT01_Drums_HatClosed_v5_rr2.flac': 'f2bd68b9eece5e9cf3236cb742339b11a93f50f2a273869c4dca8ed613bc204e',
    'FUT01_Drums_HatClosed_v5_rr3.flac': '6d44d771efb1d65eb3fc4ef151df19795007c3091899e3b04b53f8c28a7b1652',
    'FUT01_Perc_AlloyHitShort_v1_rr1.flac': 'cf5cad48556fa87923c6cbe910296ed53411c38796183221cb8393afd5b4e2f2',
    'FUT01_Perc_AlloyHitShort_v1_rr2.flac': '808055cdbd0dda51c91bb065d028c33bf61f5bc2aacbeb82dedc2e198ebd724d',
    'FUT01_Perc_AlloyHitShort_v2_rr1.flac': '1f9dd7a01874d5730058a246f0cc0b4dabb49e9983d7248821684ee48c02e191',
    'FUT01_Perc_AlloyHitShort_v2_rr2.flac': '027779a38e36eac9020be17658e264f41824e12a28d5124799b5502fb2d01b6a',
    'FUT01_Perc_AlloyHitShort_v3_rr1.flac': '1cd7e80ef0e3e6052511d0d73b88d928aa3a9a95cf8c651217282f7a007cec9d',
    'FUT01_Perc_AlloyHitShort_v3_rr2.flac': 'd15ab5e1449d2367a4a06701c939fdf59884f421e74bbc3f562f24043aaeea40',
    'FUT01_Perc_AlloyHitShort_v4_rr1.flac': 'e1725624036d9555cb8f51e781cbd47f34128e162ea74543212154346212e152',
    'FUT01_Perc_AlloyHitShort_v4_rr2.flac': 'd2436471e11622cb6c72cbdb735df62523027580eabe7881716b956c25322c3a',
    'FUT01_Perc_RimShot_v1_rr1.flac': '559c3eb446aff7b6bdbee482dfda9865045107eaaac099ed4e00a7f364f3cb10',
    'FUT01_Perc_RimShot_v1_rr2.flac': 'e73b24f0a6e1fd8b720d1334393e7da4f7684a644e177f46a0ce06fe0c8834ef',
    'FUT01_Perc_RimShot_v2_rr1.flac': 'a2e7de7d23c2598543c80a9d95fe7cb7cf85f37631dbdccfdb72b086a453fd2b',
    'FUT01_Perc_RimShot_v2_rr2.flac': 'b41039e6ed42735953511cd610ba295761ce68df108e4fc1cd7b0f40cc3834ce',
    'FUT01_Perc_RimShot_v3_rr1.flac': '1a530eef86411c1be728211b9bb09f7c069612f51ce0d89026f9daf62dee8653',
    'FUT01_Perc_RimShot_v3_rr2.flac': 'e42067682faf4b9984af4ad6e12fc31552e56abac8e05a3e1a1959b6cd7d99e0',
    'FUT01_Perc_RimShot_v4_rr1.flac': 'bc118308b141dc55be9cbd0bccd531945a0d380d89c9f0f5490f507f998719d7',
    'FUT01_Perc_RimShot_v4_rr2.flac': 'd7ba273d8df9693007b63e4371205c42198d7b1285ddd428033bf80fc36845bd',
    'FUT01_Perc_RimShot_v5_rr1.flac': 'f66ea4f133e59f9c8da3861b7467df039b961035b6ec7dda957d294ac3dbb37c',
    'FUT01_Perc_RimShot_v5_rr2.flac': '9cc819d7360534cb83b98ef3aef8c9464edd19587e78e3f2602bb56d80ceb31e',
    'FUT01_Stabs_OrganHit_v1_rr1.flac': 'fafc437aea657aa5d1ffd76e21553ff8f3e468e8e666acf4dbd21753ecf99151',
    'FUT01_Stabs_OrganHit_v2_rr1.flac': 'ecfabc952c2778ed3b79c2ea304e0728cbe931e1880ed143a5ff92877e51ed5d',
    'FUT01_Stabs_OrganStab-B3_v1_rr1.flac': '54c58ccfe49ce226979fbc26a3364772964314374728ad70eed85689dae43049',
    'FUT01_Stabs_OrganStab-B3_v2_rr1.flac': '90d0093dd6fef3accd0a7fecfa7cfad5760ae2d2b5a6c5a95888208c237c8de0',
    'FUT01_Stabs_OrganStab-Fs3_v1_rr1.flac': 'f553814485296355a8354f1e9cb535f7751f55de526744cba4921b60a1f6ad3d',
    'FUT01_Stabs_OrganStab-Fs3_v2_rr1.flac': '4e3cb63dc93fc2ffab4bdde632218141e86e62a72e16fd2f42caf06e58af34cf',
    'FUT01_Stabs_VoxSynthStab_v1_rr1.flac': '50d93b06a550ceb7393de6a0d4baede95d4021084eef825c2c0c0fd58aa0aebd',
    'FUT01_Stabs_VoxSynthStab_v2_rr1.flac': '82c96289208ec92b9e5adf084b3ce2fd8b5703fd47491caf256de0a437d9b41b',
}
PINNED = ('HatClosed', 'RimShot', 'OrganStab', 'OrganHit', 'VoxSynthStab', 'AlloyHitShort')


def in_key(note):
    return note % 12 in K.SCALE


def speech(it):
    """An item of the speech chain: from the vocal modules, with words, or the vocal tags round 4 gave them."""
    return (it.render.__module__ in SPEECH_MODULES or it.script is not None or 'hf_synthetic' in it.flags
            or {'sung', 'robot'} & set(it.tags) or it.name in SPEECH_LOOPS)


class TestTheSet(unittest.TestCase):
    def setUp(self):
        self.r4 = {it.name: it for it in kits.items(K, sets=('r4',))}
        self.r5 = {it.name: it for it in kits.items(K, sets=('r5',))}

    def test_r5_is_round_4_without_the_speech_chain_plus_the_new_items(self):
        kept = {n for n, it in self.r4.items() if not speech(it)}
        self.assertEqual(CONTINUING, kept)
        self.assertEqual(CONTINUING | NEW, set(self.r5))
        self.assertEqual(set(), {n for n, it in self.r5.items() if speech(it)})
        self.assertLessEqual({c[0] for c in vox.PICK_CHOPS} | {h[0] for h in H.HOOKS} | {t[0] for t in H.THROWS},
                             set(self.r4) - set(self.r5))

    def test_the_kit_s_dna_holds(self):
        self.assertEqual((134.0, 'F# minor', '11A', 0.59), (K.BPM, K.KEY, K.CAMELOT, K.SWING))

    def test_the_new_loops_play_only_round_5_files(self):
        """Every file the round 5 loops read is an r5 item outside the speech chain."""
        names = set(C4.FILES.values()) | set(C4.WARP.values()) | {'GlitchFillLift', 'VoxSynthRiff'}
        names |= {a[1] for acc in C5.ACCENTS.values() for a in acc}
        self.assertLessEqual(names, set(self.r5))
        self.assertFalse(any(speech(self.r5[n]) for n in names))

    def test_r5_renders_without_the_models(self):
        """With the models folder empty and the speech engines refusing, the vox synth renders."""
        from tools.sound import models
        boom = mock.Mock(side_effect=AssertionError('a speech model was called'))
        with tempfile.TemporaryDirectory() as empty, mock.patch.dict(os.environ, {models.ENV: empty}):
            with mock.patch.object(models, 'tts', boom), mock.patch.object(models, 'asr', boom):
                done = Rn.render_item(kits.items(K, only=('VoxChopEeE5',))[0], K.KIT)
        self.assertTrue(np.abs(done.audio[0]).max() > 0.0)
        boom.assert_not_called()


class TestTheVoxSynth(unittest.TestCase):
    def test_it_is_wordless_synthesis(self):
        """No item of it has a script; its module imports no speech module and no model."""
        for it in V.ITEMS:
            with self.subTest(item=it.name):
                self.assertIsNone(it.script)
                self.assertNotIn('hf_synthetic', it.flags)
        imported = set()
        for node in ast.walk(ast.parse(inspect.getsource(V))):
            if isinstance(node, ast.Import):
                imported |= {a.name for a in node.names}
            elif isinstance(node, ast.ImportFrom):
                imported |= {node.module} | {f'{node.module}.{a.name}' for a in node.names}
        speechy = ('vocals', 'models', 'hooks', 'fut01.vox', 'kokoro', 'pyworld', 'whisper', 'onnx', 'sherpa')
        self.assertEqual(set(), {m for m in imported for w in speechy if w in m and m != 'tools.sound.recipes.fut01'})

    def test_every_zone_chop_and_riff_note_is_in_the_key_on_its_root(self):
        self.assertTrue(all(in_key(n) for n in V.ROOTS))
        self.assertEqual((69, 73, 78), V.ROOTS)                     # A4, C#5, F#5
        items = {it.name: it for it in V.ITEMS}
        self.assertEqual(V.ROOTS, items['VoxSynth'].notes)
        for name, note, *_ in V.CHOPS:
            with self.subTest(chop=name):
                self.assertTrue(in_key(note))
                self.assertEqual(note, items[name].root)
                self.assertTrue(name.endswith(D.note_name(note)))
        for step, note, length, vel, path, onset in V.RIFF:
            with self.subTest(step=step):
                self.assertTrue(in_key(note))
                self.assertTrue(69 <= note <= 78)
                self.assertTrue(all(v in V.FORMANTS for _, v in path))

    def test_the_chops_sound_on_their_roots(self):
        """A chop and a held zone, rendered: the pitch on its steady stretch within 5 cents of the root."""
        r = D.rng('test', 'vox synth')
        for note, gate, path, window in ((76, V.STEP, V.EE, (0.045, V.STEP)), (73, 0.6, V.AH, (0.06, 0.30))):
            with self.subTest(note=note):
                y = V.voice(note, 1.0, gate, path, 'd', r, rel_s=0.06, vib_start=0.30)
                hz, share = qa.pitch_hz(y, float(D.midi_hz(note)), *window)
                self.assertLess(abs(D.cents(hz, float(D.midi_hz(note)))), 5.0)
                self.assertGreater(share, -30.0)

    def test_it_sits_above_the_low_mids(self):
        y = V.voice(69, 1.0, 0.3, V.OH, 'd', D.rng('test', 'low'))
        self.assertLess(qa.low_share_db(y), -40.0)
        lo = D.bfilt(D.mono(y), 250.0, 'lowpass', order=4, zero_phase=True)
        self.assertLess(D.db(np.sqrt(np.mean(lo ** 2)) / np.sqrt(np.mean(D.mono(y) ** 2))), -20.0)

    def test_the_riff_is_2_bars_off_the_beat_with_its_throw_last(self):
        self.assertEqual(32, V.RIFF_STEPS)
        steps = [s for s, *_ in V.RIFF]
        self.assertEqual(sorted(steps), steps)
        self.assertTrue(all(s % 4 for s in steps))
        self.assertTrue(all(0 <= s < 32 for s in steps))
        self.assertEqual(3, V.S.ECHO_STEPS)                         # the throw's dotted 8th
        self.assertEqual([(s, n, ln, v, False) for s, n, ln, v, _, _ in V.RIFF], list(V.RIFF_NOTES))

    def test_the_riff_is_a_midi_phrase(self):
        ph = {p['name']: p for p in Mi.PARTS['phrases']}
        self.assertTrue(3 <= len(Mi.PARTS['phrases']) <= 5)
        p = ph['Vox synth riff']
        self.assertEqual(list(V.RIFF_NOTES), p['notes'])
        self.assertEqual((2, 'VoxSynth', 'vox'), (p['bars'], p['item'], p['on']))   # on the vox synth, channel 5


class TestTheLoops(unittest.TestCase):
    def test_the_instrumental_has_no_vox_lane(self):
        self.assertEqual((), C5.RIFF_AT['novox'])
        self.assertNotIn('vsynth', C5.new_lanes('novox'))
        self.assertEqual((0, 32), C5.RIFF_AT['vox'])

    def test_both_club_loops_play_two_to_four_accents(self):
        names = {it.name for it in A.ITEMS}
        for variant in ('vox', 'novox', 'speed'):
            acc = C5.ACCENTS[variant]
            with self.subTest(variant=variant):
                self.assertTrue(2 <= len(acc) <= 4)
                self.assertLessEqual({a[1] for a in acc}, names)
                self.assertEqual(len(acc), len({a[0] for a in acc}))
        lanes = C5.new_lanes('vox')
        self.assertLessEqual({'acc_rev', 'acc_up', 'acc_hit'}, set(lanes))
        for lane in ('acc_rev', 'acc_up', 'acc_hit'):
            self.assertGreater(np.abs(lanes[lane]).max(), 0.0)

    def test_the_accents_land_on_their_beats(self):
        """The reverse stab and the uplifter end on their downbeats; the organ hit starts on the and of 4."""
        g = C5.G
        for lane, name, step, layer, how, gate in C5.ACCENTS['vox']:
            x = C5.accent(g, name, step, layer, how, gate)
            e = np.abs(x).max(1)
            at = g.at(step % g.steps)
            with self.subTest(lane=lane):
                if how == 'end':                                        # loud up to the beat, silent from it
                    before = e[np.arange(at - 480, at) % g.length]
                    self.assertGreater(before.max(), 0.1 * e.max())
                    self.assertEqual(0.0, e[np.arange(at, at + 200) % g.length].max())
                else:
                    self.assertEqual(0.0, e[at - 50:at].max())
                    self.assertGreater(e[at:at + 480].max(), 0.0)

    def test_the_club_loops(self):
        items = {it.name: it for it in C5.ITEMS}
        for name in ('GarageVoxClub', 'GarageNoVoxClub'):
            it = items[name]
            with self.subTest(item=name):
                self.assertIn('club', it.tags)
                self.assertIsNone(it.hp_hz)
                self.assertNotIn('club_max_reduction_db', it.qa)    # the standard's 4 dB cap
                self.assertNotIn('club_bands', it.qa)
                self.assertNotIn('club_crest_loss_db', it.qa)


@unittest.skipUnless(RENDER_HOST, 'the masters are pinned on the render host (Linux x86-64), ADR 0007')
class TestContinuingBytes(unittest.TestCase):
    def test_a_sample_of_the_continuing_items_renders_round_4_s_bytes(self):
        import hashlib
        for name in PINNED:
            done = Rn.render_item(kits.items(K, only=(name,))[0], K.KIT)
            for take, x in zip(done.takes, done.audio):
                with self.subTest(file=take.file):
                    self.assertEqual(R4_SHA[take.file], hashlib.sha256(Rn.encode(x)).hexdigest())
        self.assertEqual(len(R4_SHA), sum(len(kits.items(K, only=(n,))[0].takes(K.KIT)) for n in PINNED))


if __name__ == '__main__':
    unittest.main()
