"""FUT01's round 4, the UK garage rebuild (set r4): the set is the whole kit; the garage drums (kick B keeps round 1's
split design and the crest band, its sub short for 2-step; the clap's hands, body and room; the hats and tambourine
inside their 16ths); the warp bass's A/B and lines; the stabs and accents in the key; the hooks cut from the kit's own
title-checked line; the context loop's parts, mix and master settings; the synth phrases and their MIDI. Renders the
drums and the basses (about 30 s); nothing here needs the vocal models."""
import unittest

import numpy as np

from tools.sound import dsp48 as D
from tools.sound import kits
from tools.sound import qa
from tools.sound.kits import fut01 as K
from tools.sound.recipes.fut01 import accents as A
from tools.sound.recipes.fut01 import context4 as C
from tools.sound.recipes.fut01 import garage as Ga
from tools.sound.recipes.fut01 import hooks as H
from tools.sound.recipes.fut01 import keys as Ky
from tools.sound.recipes.fut01 import midi as Mi
from tools.sound.recipes.fut01 import riffs as R
from tools.sound.recipes.fut01 import vox
from tools.sound.tests.test_fut01_drums import env_db, falls_ms, files, knock_db, onset, share_db, sub_20_ms

SR = D.SR
STEP_S = 60.0 / K.BPM / 4
NEW = {'KickGarage', 'ClapGarage', 'HatClosed', 'HatOpen', 'Tambourine', 'RimShot', 'WarpClean', 'WarpWarm',
       'OrganBass', 'OrganStab', 'EPianoStab', 'GarageContext', 'GarageContextWarm', 'GarageContextWet',
       'GarageContextClub', 'GarageContextLow', 'GarageContextLowWarm', 'SpeedGarageLoop', 'SpeedGarageLow',
       'ChopRiff', 'CallLineA4', 'OrganHit', 'WarpStab', 'RaveStab', 'ChordSweep', 'ReverseStab', 'SubDrop',
       'AlloyBell', 'VoxSynthStab', 'NoiseUplifter', 'OrganLead', 'OrganRiff', 'WarpRiff', 'PluckRiff', 'StabRhythm'}
KEPT = {'KickSplit', 'Shaker', 'AlloyHit', 'AlloyHitShort', 'AirPadFsm9', 'ChromePluck', 'ChromePluckPhrase',
        'AirPadHold', 'GlitchFillLift', 'ShiftRiser', 'RobotBoost', 'RobotBoostWet', 'SungLine1', 'SungLine2',
        'SungLine3', 'ChopMeE5', 'ChopFloorFs5'}
REPLACED = {'KickPunch', 'SnareMetal', 'SnareModal', 'SnareClap', 'ClosedHatFM', 'ClosedHatRing', 'ReeseClean',
            'ReeseWarm', 'GrowlTalk', 'GrowlDive', 'ContextLoop', 'ContextLoopClub', 'DrumGroovePunch',
            'ShiftRiserDrop'}


def in_key(note):
    return note % 12 in K.SCALE


class TestTheSet(unittest.TestCase):
    def setUp(self):
        self.r4 = {it.name: it for it in kits.items(K, sets=('r4',))}

    def test_r4_is_the_whole_kit(self):
        """r4 holds every new sound, every unchanged one, and none of the sounds round 4 replaced."""
        self.assertLessEqual(NEW, set(self.r4))
        self.assertLessEqual(KEPT, set(self.r4))
        self.assertLessEqual({c[0] for c in vox.PICK_CHOPS} | {h[0] for h in H.HOOKS} | {t[0] for t in H.THROWS},
                             set(self.r4))
        self.assertEqual(set(), REPLACED & set(self.r4))

    def test_the_heavier_shuffle(self):
        """Round 4 raised the kit's one swing from round 3's 0.57 into the standard's 0.58-0.61; the swing A/B plays
        the old one."""
        self.assertTrue(0.58 <= K.SWING <= 0.61)
        self.assertEqual(K.SWING, C.G.swing)
        self.assertEqual(0.57, C.drum_groove('KickGarage', 0.57).meta['swing'])


class TestKickGarage(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.kick = files('KickGarage')
        cls.split = files('KickSplit')

    def test_a_short_sub_for_2_step(self):
        """The sub 20 dB down inside two 16ths (224 ms at 134 BPM, the standard's target); Split takes about 317."""
        for rr in (1, 2):
            with self.subTest(rr=rr):
                b = sub_20_ms(self.kick[(5, rr)])
                self.assertLessEqual(b, 2 * STEP_S * 1000)
                self.assertLessEqual(b, sub_20_ms(self.split[(5, rr)]) - 80.0)

    def test_a_knock(self):
        for rr in (1, 2):
            with self.subTest(rr=rr):
                self.assertGreaterEqual(knock_db(self.kick[(5, rr)]) - knock_db(self.split[(5, rr)]), 2.0)

    def test_in_the_crest_band_and_the_click_leads(self):
        """5-7 dB (round 1's band), and on the harder layers the true peak is the click, in the first 12 ms."""
        for rr in (1, 2):
            with self.subTest(rr=rr):
                self.assertTrue(5.0 <= qa.kick_crest_db(self.kick[(5, rr)]) <= 7.0)
        for (layer, rr), x in self.kick.items():
            if layer >= 3:
                with self.subTest(layer=layer, rr=rr):
                    m = D.mono(x)
                    self.assertLess(np.argmax(np.abs(D.up(m, 4))) / 4, onset(m) + 0.012 * SR)

    def test_the_split_design(self):
        """A clean sub, the grit only on top: under 110 Hz the sub is a sine on F#1 (QA's tuning), and nothing of the
        click's drive sits in 150-400 Hz after the knock (the mud gate)."""
        for layer in (3, 5):
            with self.subTest(layer=layer):
                g = qa.mud(self.kick[(layer, 1)])
                self.assertLessEqual(g['fall_150_400_ms'], qa.MUD_FALL_MS)
                self.assertLessEqual(g['vs_sub_db'], qa.MUD_VS_SUB_DB)
                self.assertLessEqual(g['share_db'], qa.MUD_SHARE_DB)
                hz, _ = qa.pitch_hz(self.kick[(layer, 1)], float(D.midi_hz(K.KICK_NOTE)), 0.10, 0.25)
                self.assertLess(abs(D.cents(hz, float(D.midi_hz(K.KICK_NOTE)))), 10.0)


class TestGarageTops(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.clap = files('ClapGarage')
        cls.hats = {n: files(n) for n in ('HatClosed', 'HatOpen', 'Tambourine', 'RimShot')}

    def test_the_clap_s_hands_body_and_room(self):
        """4 hands inside 25 ms, the first within 2 dB of the loudest; a body (150-400 Hz share) inside the standard's
        -15 to -8 dB; the room 20 dB down inside 250 ms."""
        self.assertEqual(4, len(Ga.CLAP_HANDS))
        self.assertLess(Ga.CLAP_HANDS[-1][0] * 1.15, 0.035)          # the loosest spread, on the softest layer
        self.assertGreaterEqual(min(g for _, g in Ga.CLAP_HANDS), -2.0)
        for layer in (1, 3, 5):
            with self.subTest(layer=layer):
                x = self.clap[(layer, 1)]
                self.assertTrue(-15.0 <= share_db(x, 150.0, 400.0) <= -8.0)
                m = D.mono(x)
                self.assertLessEqual(falls_ms(env_db(m, 0.005), onset(m), 20.0), 250.0)

    def test_the_hats_sit_in_their_16ths(self):
        """The closed hat and the rim 30 dB down inside a 16th's span of two (the swung pair), the tambourine inside
        two 16ths; the open hat rings longer (the loops choke it)."""
        fall = {}
        for name, got in self.hats.items():
            top = max(k[0] for k in got)
            m = D.mono(got[(top, 1)])
            fall[name] = falls_ms(env_db(m, 0.005), onset(m), 30.0)
        self.assertLessEqual(fall['HatClosed'], 2 * STEP_S * 1000)
        self.assertLessEqual(fall['RimShot'], 2 * STEP_S * 1000)
        self.assertLessEqual(fall['Tambourine'], 2 * STEP_S * 1000)
        self.assertGreater(fall['HatOpen'], 3 * fall['HatClosed'])

    def test_the_open_hat_is_choked(self):
        """In the loop's hat group the open hat stops where the next closed hat starts."""
        g = C.G
        lane = C.choked_lane('HatOpen', ((2, 1.0),), ((3, 0.75),), g)
        cut = g.at(3)
        self.assertGreater(np.abs(lane[g.at(2):cut]).max(), 0.0)
        self.assertEqual(0.0, np.abs(lane[cut:cut + int(0.2 * SR)]).max())


class TestWarpBass(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.a, cls.b = files('WarpClean'), files('WarpWarm')

    def test_a_is_clean_and_b_has_the_body(self):
        for key in self.a:
            with self.subTest(key=key):
                self.assertLess(share_db(self.a[key], 150.0, 400.0), -40.0)
                self.assertGreater(share_db(self.b[key], 150.0, 400.0), share_db(self.a[key], 150.0, 400.0) + 20.0)

    def test_the_lines_are_in_the_key_and_the_zones_reach(self):
        for name, line in (('garage', C.WARP_LINE), ('organ', C.ORGAN_LINE), ('speed', C.SPEED_LINE),
                           ('riff', R.WARP_RIFF)):
            for step, length, note, vel, slide in line:
                with self.subTest(line=name, step=step):
                    self.assertTrue(in_key(note))
                    self.assertTrue(30 <= note <= 42)
                    self.assertIn(slide, (False, 'slide', 'dip'))

    def test_every_slide_and_dip_runs_on_from_the_note_before(self):
        for name, line in (('garage', C.WARP_LINE), ('organ', C.ORGAN_LINE), ('speed', C.SPEED_LINE),
                           ('riff', R.WARP_RIFF)):
            for (s0, l0, *_), (s1, _, _, _, slide) in zip(line, line[1:]):
                if slide:
                    with self.subTest(line=name, step=s1):
                        self.assertGreaterEqual(s0 + l0, s1 - 1.0)

    def test_the_line_leaves_every_kick_room(self):
        """No note starts over a kick's first 80 ms, and every note-off comes at least 60 ms before the next kick, so its
        30 ms release has begun to fall before QA's before-hit window (40-15 ms) and the kick has the low end to
        itself (the low loops' gap checks read the rest)."""
        g = C.G
        kicks = sorted(g.at(s) for s, _ in C.KICK)
        for on, run, n_on in C.line_voices(C.WARP_LINE, g, 2):
            end = on + n_on + int(0.020 * SR)
            for k in kicks:
                with self.subTest(note=on, kick=k):
                    self.assertFalse(k <= on < k + int(qa.KICK_OWN_S * SR))
                    if on < k:
                        self.assertLessEqual(end, k - int(0.040 * SR))

    def test_the_kick_leads(self):
        """The bass lane sits under the kick lane (LUFS), so the kick, not the bass, leads the low end."""
        self.assertLess(C.BASS_LUFS, qa.lufs(C.kick_lane()))


class TestChordsAndAccents(unittest.TestCase):
    def test_the_stabs_are_minor_9_chords_in_the_key(self):
        for zone in Ky.ZONES:
            notes = Ky.chord(zone)
            with self.subTest(zone=zone):
                self.assertTrue(all(in_key(n) for n in notes))
                self.assertEqual((0, 7, 10, 14, 15), tuple(n - zone for n in notes))

    def test_every_tonal_accent_voices_its_root_alone(self):
        """No other tone within 30 % of the root, so the tuning check reads the root."""
        for chord in (A.ORGAN_HIT, A.RAVE, A.SWEEP_CHORD, A.VOX_CHORD, Ky.chord(54), Ky.chord(59)):
            with self.subTest(chord=chord):
                root = float(D.midi_hz(chord[0]))
                self.assertTrue(all(in_key(n) for n in chord))
                self.assertTrue(all(float(D.midi_hz(n)) > 1.3 * root for n in chord[1:]))

    def test_six_to_ten_accents_each_on_a_root_in_the_key(self):
        self.assertTrue(6 <= len(A.ITEMS) <= 10)
        for it in A.ITEMS:
            with self.subTest(item=it.name):
                self.assertIsNotNone(it.root)
                self.assertTrue(in_key(it.root))
                self.assertTrue(it.about.startswith('accent, '))


class TestHooks(unittest.TestCase):
    def test_hooks_come_from_the_title_checked_line_and_stay_in_the_key(self):
        chops = {c[0]: c for c in vox.PICK_CHOPS}
        for name, chop, semis, formant, _ in H.HOOKS:
            with self.subTest(hook=name):
                self.assertIn(chop, chops)
                self.assertTrue(in_key(chops[chop][4] + semis))
                if name.endswith('Chip'):
                    self.assertAlmostEqual(2.0 ** (semis / 12.0), formant)
                else:
                    self.assertEqual(1.0, formant)

    def test_no_hook_spells_a_cut_title(self):
        cut = [t for t, why in vox.CHOP_CHECKS.items() if why.startswith('cut')]
        for name in [h[0] for h in H.HOOKS] + [t[0] for t in H.THROWS] + ['ChopRiff']:
            low = name.lower()
            for title in cut:
                with self.subTest(name=name, title=title):
                    self.assertNotIn(title.replace(' ', ''), low)

    def test_the_riff_sits_on_the_off_beats(self):
        self.assertTrue(all(step % 4 for step, _, _ in H.RIFF))


class TestTheLoop(unittest.TestCase):
    def test_the_drums_stop_for_the_fill(self):
        for part in (C.KICK, C.CLAP, C.HAT_C, C.HAT_O, C.TAMB, C.RIM):
            self.assertTrue(all(s < C.FILL_AT for s, _ in part))

    def test_2_step_and_4x4(self):
        bar1 = {s for s, _ in C.KICK if s < 16}
        self.assertEqual({0, 10}, bar1)
        self.assertEqual([0, 4, 8, 12, 16, 20, 24, 28], [s for s, _ in C.KICK_4X4])
        # the 4x4 line plays between the kicks: every new note on an off-beat 8th or the 16th after it
        self.assertTrue(all(s % 4 in (2, 3) for s, _, _, _, slide in C.SPEED_LINE if not slide))

    def test_the_club_loop(self):
        it = [i for i in C.ITEMS if i.name == 'GarageContextClub'][0]
        self.assertIn('club', it.tags)
        self.assertIsNone(it.hp_hz)
        self.assertNotIn('club_max_reduction_db', it.qa)   # the standard's 4 dB cap


class TestPhrases(unittest.TestCase):
    def test_three_to_five_phrases_of_2_or_4_bars_in_the_key(self):
        ph = Mi.PARTS['phrases']
        self.assertTrue(3 <= len(ph) <= 5)
        for p in ph:
            with self.subTest(phrase=p['name']):
                self.assertIn(p['bars'], (2, 4))
                self.assertTrue(all(in_key(n) for _, n, _, _, _ in p['notes']))
                self.assertTrue(all(0 <= s < 16 * p['bars'] for s, *_ in p['notes']))

    def test_the_midi_phrases_are_the_riffs_as_rendered(self):
        ph = {p['name']: p for p in Mi.PARTS['phrases']}
        self.assertEqual(list(R.ORGAN_RIFF), ph['Organ riff']['notes'])
        self.assertEqual(list(R.PLUCK_RIFF), ph['Pluck riff']['notes'])
        self.assertEqual([(s, n, ln, v, bool(sl)) for s, ln, n, v, sl in R.WARP_RIFF], ph['Warp riff']['notes'])
        self.assertEqual(R.stab_notes(), ph['E-piano stab rhythm']['notes'])

    def test_the_stab_samples_play_one_note_per_stab_on_its_chords_file(self):
        """The organ and e-piano stab samples are one file per chord, each the whole chord (F#3 plays F#m9, B3 Bm9).
        Their MIDI, the chords' sample trigger and the e-piano stab phrase, plays one note per stab on its chord's
        file, where the loop and the riff play it; the chords' voicing file keeps every chord tone, for a synth."""
        ch = Mi.PARTS['chords']
        self.assertEqual(sorted((s, z, 1, v, False) for s, z, v in C.STABS), sorted(ch['trigger']))
        self.assertEqual(set(Ky.ZONES), {n for _, n, *_ in ch['trigger']})
        self.assertEqual('F#3, B3', ch['zones'])
        self.assertEqual(len(C.STABS) * len(Ky.SHAPE), len(ch['notes']))
        stab = {p['name']: p for p in Mi.PARTS['phrases']}['E-piano stab rhythm']
        self.assertEqual([(s, z, R.STAB_LEN, v, False) for s, z, v in R.STAB_RHYTHM], stab['notes'])
        self.assertEqual(set(Ky.ZONES), {n for _, n, *_ in stab['notes']})


if __name__ == '__main__':
    unittest.main()
