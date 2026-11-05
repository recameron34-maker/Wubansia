"""FUT03 "Glass Current", the context loop (recipes/fut03/context.py): what the brief and round 1 ask of it that the QA
can't see on its own. The loops are whole bars at 128 BPM; every lane sits on the kit's one grid and swing; every kick
is the same top layer; the loop plays the designers' picks (the line, the chops' line); the bass rests where the vowel
bass answers, once, on a kick, in the loop's character; the stab leaves the kicks to the pump; the fill's second half
leaves the robot's words clear; the pluck plays the signature phrase's own notes; the resonator rings in tune and in
key; the A/B lanes are level-matched; and the low end of both characters passes its rule with the files played exactly
as written (no second sub-sonic high-pass). Round 3 (the modern-sound standard, docs/plans/S1-modern-sound.md): the
shaker's 16ths, the buses, the kit's own plate and 8th-note echo, and the club loop, mastered at the genre's settings
and first on the listening page. The review's fixes: the vocals off the music bus's body lift, the robot's peak limiter,
the clap's room on the pluck, and the master's cap down from 7 to 5 dB."""
import unittest

import numpy as np

import tools.sound  # noqa: F401  (pins numpy's kernels before numpy loads)
from tools.sound import dsp48 as D
from tools.sound import kits, phrase, qa
from tools.sound import master as MS

K = kits.load('FUT03')


def item(name):
    return kits.items(K, only=(name,))[0]


class TestFut03Context(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        from tools.sound.recipes.fut03 import context
        cls.C = context

    def test_the_loops_are_whole_bars(self):
        bar = 4 * 60.0 / K.BPM * D.SR
        self.assertEqual(round(4 * bar), self.C.L)
        self.assertEqual(360000, self.C.L)
        self.assertEqual(round(2 * bar), self.C.DG.length)
        self.assertEqual(self.C.G.steps, 64)

    def test_one_grid_and_one_swing(self):
        C = self.C
        self.assertEqual(K.SWING, C.G.swing)
        self.assertEqual(K.SWING, C.DG.swing)
        self.assertEqual(C.G.at(1) - C.G.at(0), int(np.floor((2 * K.SWING) * K.STEP * D.SR + 0.5)))
        self.assertTrue(all(s % 4 == 0 for s, _ in C.KICK))
        self.assertNotIn(C.DROP_OUT, [s for s, _ in C.KICK])          # the drop-out before the downbeat
        self.assertEqual(15, len(C.KICK))
        self.assertEqual({1.0}, {v for _, v in C.KICK})               # every beat on the top layer: no strong-weak
        self.assertFalse([s for s, _, _ in C.HATS if s >= C.DROP_OUT])
        self.assertFalse([s for s, _ in C.STAB if s >= C.DROP_OUT])
        # the shaker: every 16th up to the drop-out, the swung "e" and "a" leading, the "a" most
        self.assertEqual(list(range(C.DROP_OUT)), [s for s, _ in C.SHAKER])
        self.assertEqual([C.SHAKER_BEAT[s % 4] for s, _ in C.SHAKER], [v for _, v in C.SHAKER])
        self.assertEqual(max(C.SHAKER_BEAT), C.SHAKER_BEAT[3])
        self.assertGreater(min(C.SHAKER_BEAT[1], C.SHAKER_BEAT[3]), max(C.SHAKER_BEAT[0], C.SHAKER_BEAT[2]))

    def test_it_plays_the_designers_picks(self):
        from tools.sound.recipes.fut03 import bass, fx, vox
        C = self.C
        self.assertEqual(bass.PICK, C.LINE_NAME)
        for name in C.LINE.values():
            self.assertEqual('loop', item(name).category)
        self.assertEqual({'clean', 'warm'}, set(C.LINE))
        self.assertIn(C.BASS_PICK, C.LINE)
        self.assertIn(C.CHOP[1], vox.CHOP_BY_NAME)                     # a chop of the picked line
        self.assertEqual('VocoderDrop', C.ROBOT[1])
        self.assertEqual('BuildFill', C.FILL[1])
        self.assertEqual((3 * 16 + 8, 8), (C.FILL[0], C.FILL[2]))     # bar 4, beat 3: the fill's second half...
        self.assertEqual(C.L, C.G.at(C.FILL[0]) + fx.BAR - fx.swung(C.FILL[2]))   # ...ending on the downbeat

    def test_the_pluck_plays_the_phrase_and_answers_the_voice(self):
        from tools.sound.recipes.fut03 import lead
        C = self.C
        notes = set(lead.PHRASE)
        for s, m, ln, v in C.PLUCK:
            self.assertIn((s % 32, m, ln, v), notes)
        self.assertEqual(list(lead.PHRASE), [n for n in C.PLUCK if n[0] < 32])   # bars 1-2: the phrase whole
        voice = [(C.CHOP[0], C.CHOP[0] + 8), (C.ROBOT[0], C.ROBOT[0] + 11)]      # the voice's half bars
        for s, _, ln, _ in C.PLUCK:
            for a, b in voice:
                self.assertFalse(a <= s < b, s)                        # the pluck rests while the voice sings

    def test_the_vowel_bass_answers_once_where_the_line_rests(self):
        from tools.sound.recipes.fut03 import bass
        C = self.C
        step, names = C.VOWEL
        self.assertIn(step, [s for s, _ in C.KICK])                    # it starts on a kick, as its groove check plays
        self.assertEqual({'clean': 'gate', 'warm': 'bass-body'}, {c: item(n).qa.get('mud', 'gate')
                                                                  for c, n in names.items()})
        self.assertTrue(all(item(n).category == 'bass' for n in names.values()))
        a, b = C.BASS_REST
        rested = [n for n in bass.LINES[C.LINE_NAME] if a <= n[0] < b]
        self.assertEqual([25, 29, 30], [n[0] for n in rested])
        self.assertTrue(step < a < b <= step + 8)
        # the cut lands in the line's own silence
        for char in C.LINE:
            y = C.bass_lane(char)
            raw = phrase.kit_files(K, C.LINE[char])[1][(None, 1, 1)].audio
            cut = C.G.at(a) - int(C.REST_LEAD * D.SR)
            near = np.abs(D.st(raw)[cut - 240:cut + 240]).max() / np.abs(raw).max()
            self.assertLess(20 * np.log10(near + 1e-30), -95.0, char)       # silence: no fade needed
            self.assertFalse(np.any(y[cut:C.G.at(b)]))
            np.testing.assert_array_equal(y[C.G.at(b):], np.concatenate([D.st(raw), D.st(raw)])[C.G.at(b):])

    def test_the_stab_leaves_the_kicks_to_the_pump(self):
        """A 176 ms stab under a 9 dB pump is swallowed on the kick (the review: 11-18 dB under the mix): the loop plays
        the designer's push without its on-kick hits."""
        from tools.sound.recipes.fut03 import pad
        C = self.C
        kicks = {s for s, _ in C.KICK} | {C.DROP_OUT}
        self.assertFalse([s for s, _ in C.STAB if s in kicks or s % 4 == 0])
        self.assertEqual([3, 6, 10, 13, 19, 22, 27, 30], [s for s, _ in C.STAB if s < 32])
        self.assertLessEqual({(s % 32, v) for s, v in C.STAB}, set(pad.PUMP_HITS))

    def test_the_fill_leaves_the_robot_its_words(self):
        """The robot's syllables before "drop" land on no fill hit: the fill plays from its 16th 8, on "drop"."""
        from tools.sound.recipes.fut03 import fx, vox
        C = self.C
        fill_steps = {C.FILL[0] - C.FILL[2] + s for s, _, _ in fx.FILL if s >= C.FILL[2]}
        robot_steps = [C.ROBOT[0] + s for s, _ in vox.ROBOT_GRID]
        self.assertFalse(set(robot_steps[:-1]) & {int(s) for s in fill_steps})
        self.assertEqual(C.FILL[0], robot_steps[-1])                  # the fill's 16ths start with "drop"
        self.assertGreater(C.LANES['robot'][1], -1.0)                 # 3 dB over the first fader

    def test_the_a_and_b_lanes_are_level_matched(self):
        C = self.C
        for key in C.CHARACTER_LANES:
            a, b = (qa.lufs(C.lane(key, c)) for c in ('clean', 'warm'))
            self.assertLess(abs(a - b), 0.1, key)

    def test_everything_tonal_is_in_the_key(self):
        from tools.sound.recipes.fut03 import perc
        C = self.C
        scale = set(K.SCALE)
        for _, note, _ in C.PERC:
            self.assertIn(note, perc.PLING_NOTES)
        for f in C.resonator_freqs():
            m = 69 + 12 * np.log2(f / 440.0)
            self.assertLess(abs(m - round(m)), 0.01, f)                # in tune within 1 cent
            self.assertIn(int(round(m)) % 12, scale, f)

    def test_the_loops_play_the_files_as_written(self):
        """No renderer high-pass on the loops (their files had it once); a zero-phase DC clean instead."""
        for name in ('ContextLoopClub', 'ContextLoop', 'ContextLoopWarm', 'ContextLoopWet', 'ContextLoopLow',
                     'ContextLoopLowWarm', 'DrumLoop', 'FillPreview'):
            it = item(name)
            self.assertIsNone(it.hp_hz, name)
            self.assertTrue(it.loop, name)
            self.assertIn('r3', it.sets, name)
            if name != 'ContextLoopClub':
                self.assertIn('r2', it.sets, name)
        self.assertEqual('gate', item('ContextLoopLow').qa['mud'])
        self.assertEqual('bass-body', item('ContextLoopLowWarm').qa['mud'])
        self.assertIn('with_kick', item('ContextLoopLow').tags)
        self.assertIn('wet', item('ContextLoopWet').tags)

    def test_the_dry_a_b_plays_at_one_loudness(self):
        """The listening page's dry A and B at one loudness: at their own peaks B read 0.57 LU over A (-12.59 against
        -13.16 LUFS-I), so B takes A's loudness as its target (DRY_B_LUFS) and A keeps its peak. The wet loop, no A/B,
        keeps its own peak. The rendered pair is checked below, with the vocal models."""
        a, b = item('ContextLoop'), item('ContextLoopWarm')
        self.assertEqual((None, self.C.DRY_B_LUFS), (a.level, b.level))
        self.assertAlmostEqual(-13.16, self.C.DRY_B_LUFS, delta=0.005)
        self.assertIsNone(item('ContextLoopWet').level)

    @unittest.skipUnless(__import__('os').environ.get('WUBANSIA_MODELS_DIR'), 'WUBANSIA_MODELS_DIR not set')
    def test_the_dry_a_b_renders_at_one_loudness(self):
        """As the renderer writes them, dry A and B within 0.2 LU of each other, neither true peak over -1.02 dBTP."""
        from tools.sound import render as R
        got = []
        for name in ('ContextLoop', 'ContextLoopWarm'):
            x = R.render_item(item(name), 'FUT03').audio[0]
            self.assertLessEqual(D.true_peak_db(x), D.TP_TARGET + 1e-6, name)
            got.append(qa.lufs(x))
        self.assertLess(abs(got[0] - got[1]), 0.2, got)

    def test_the_club_loop_is_a_mastered_loop(self):
        """ContextLoopClub: the wet loop through the club master at the genre's settings, tagged club (QA checks it
        against qa.CLUB), with no renderer high-pass or LUFS target, new in round 3, and first among the context loops
        (the listening page starts there). The kit's cap on peak reduction is its one deviation from the standard's
        defaults; the report records it."""
        it = item('ContextLoopClub')
        self.assertEqual(('r3',), it.sets)
        self.assertEqual({'wet', 'club'}, set(it.tags))
        self.assertIsNone(it.hp_hz)
        self.assertIsNone(it.level)
        self.assertEqual('off', it.qa['mud'])
        self.assertIs(self.C.context_club, it.render)
        self.assertEqual('ContextLoopClub', self.C.ITEMS[0].name)
        st = MS.settings(K, max_reduction_db=self.C.MASTER_REDUCTION_DB)
        self.assertEqual(K.DIRECTION, st.genre)
        self.assertEqual(qa.CLUB[K.DIRECTION]['lufs'][0], st.target_lufs)
        self.assertEqual(-8.0, st.target_lufs)
        # the review's fix: 7 dB was the heaviest master of the four kits, set by one tonal peak (the robot's "the"
        # through the music bus's body lift); with that peak gone, 5 dB reaches the genre's window
        self.assertTrue(4.0 <= self.C.MASTER_REDUCTION_DB <= 5.0)
        self.assertEqual({k: v for k, v in MS.settings(K).__dict__.items() if k != 'max_reduction_db'},
                         {k: v for k, v in st.__dict__.items() if k != 'max_reduction_db'})   # the chain as shared

    def test_the_buses(self):
        """The drum bus (kick, clap, hats, shaker) clipped at the kick lane's own true peak, the vocal bus compressed
        and made up, the music bus's EQ (a cut at 900 Hz, presence, low-mid body) on everything but the drums and the
        bass, the kick's room on the vocals, the pling and the fill; the pump as round 2 set it."""
        C = self.C
        self.assertEqual(('kick', 'clap', 'hat', 'shaker'), C.DRUM_BUS)
        self.assertEqual(0.0, C.DRUM_CLIP_DB)
        self.assertEqual(('bass', 'vowel'), C.BASS_BUS)
        self.assertFalse(set(C.DRUM_BUS) & set(C.BASS_BUS))
        self.assertEqual(set(C.VOCALS), set(C.VOX_MAKEUP_DB))
        self.assertTrue(all(v > 0 for v in C.VOX_MAKEUP_DB.values()))
        self.assertLessEqual(set(C.ROOMED), set(C.LANES) - set(C.DRUM_BUS) - set(C.BASS_BUS))
        cut = [b for b in C.MUSIC_EQ if b[1] < 0]
        self.assertEqual(1, len(cut))
        self.assertTrue(700 <= cut[0][0] <= 1200)                         # the 1 kHz octave's owners, trimmed
        self.assertEqual(9.0, C.PUMP['stab']['depth_db'])                 # keep the pump: the style's audible one
        self.assertEqual(4.0, C.RETURN_PUMP['depth_db'])
        from tools.sound import mixmeter as MM
        kick = {'kick': C.lane('kick')}
        self.assertEqual(MM.true_peak_db(kick['kick']) + C.DRUM_CLIP_DB, C.drum_ceiling_db(kick))   # round the loop
        # the review's fixes: the vocal bus takes the music bus's EQ without its body lift; the robot's peak limiter;
        # the clap's room on the pluck alone (the chop's "cur-" lands on a clap)
        body = [b for b in C.MUSIC_EQ if b[0] == C.BODY_HZ]
        self.assertEqual(1, len(body))
        self.assertGreater(body[0][1], 0.0)
        self.assertEqual(tuple(b for b in C.MUSIC_EQ if b[0] != C.BODY_HZ), C.VOCAL_EQ)
        self.assertTrue(0.0 < C.ROBOT_LIMIT['under_db'] <= 6.0)
        self.assertLessEqual(C.ROBOT_LIMIT['hold_s'] + C.ROBOT_LIMIT['lookahead_s'], 0.010)   # "glass" is 30 ms on
        self.assertEqual(('pluck',), C.CLAP_ROOMED)

    def test_the_vocals_skip_the_body_lift(self):
        """A tone at the body band's centre comes out of the mix the lift's gain louder on a music lane (the pling) than
        on a vocal lane (the chop), both under the kick's room; at 3 kHz both get the same presence."""
        C = self.C
        n = np.arange(C.L)
        kick = np.zeros((C.L, 2))
        for p in C.kicks():
            kick[p] = 0.5                                  # the drum bus needs a kick lane to set its clip's ceiling
        silent = {k: np.zeros((C.L, 2)) for k in C.LANES}
        silent['kick'] = kick
        base = C.mixdown(silent)                           # the kick alone: the tone's path is the difference
        out = {}
        for lane in ('vox', 'perc'):
            for hz in (C.BODY_HZ, 3000.0):
                k = int(round(hz * C.L / D.SR))            # whole cycles in the loop: one FFT bin
                y = C.mixdown({**silent, lane: D.st(0.01 * np.sin(2 * np.pi * k * n / C.L))}) - base
                out[lane, hz] = 20 * np.log10(np.abs(np.fft.rfft(y[:, 0]))[k])
        gain = [b for b in C.MUSIC_EQ if b[0] == C.BODY_HZ][0][1]
        self.assertAlmostEqual(gain, out['perc', C.BODY_HZ] - out['vox', C.BODY_HZ], delta=0.05)
        self.assertAlmostEqual(0.0, out['perc', 3000.0] - out['vox', 3000.0], delta=0.05)

    def test_the_clap_room(self):
        """The clap's room: 1 from 60 ms before to 80 ms after each clap of 0.9 or more, raised-cosine ramps 10 ms long
        outside that, 0 elsewhere (the ghost claps get none); only the lane's 1-5 kHz is ducked, and outside the room
        the lane passes as it was."""
        C = self.C
        cr = C.CLAP_ROOM
        env = C.clap_room_env()
        big = [C.G.at(s) for s, v in C.CLAP if v >= cr['min_vel']]
        ghosts = [C.G.at(s) for s, v in C.CLAP if v < cr['min_vel']]
        self.assertEqual(6, len(big))
        sr = D.SR
        for o in big:
            self.assertTrue(np.all(env[o - int(cr['before'] * sr):o + int(cr['after'] * sr)] == 1.0))
            self.assertEqual(0.0, env[o - int((cr['before'] + cr['ramp']) * sr) - 2])
            self.assertEqual(0.0, env[o + int((cr['after'] + cr['ramp']) * sr) + 2])
        for o in ghosts:
            self.assertEqual(0.0, env[o])
        n = np.arange(C.L)
        tones = {hz: D.st(0.1 * np.sin(2 * np.pi * round(hz * C.L / sr) * n / C.L)) for hz in (300.0, 2500.0, 9000.0)}
        x = sum(tones.values())
        y = C.clap_room(x)
        out = env == 0.0
        np.testing.assert_allclose(y[out], x[out], atol=1e-12)
        for hz, t in tones.items():
            w = slice(big[0] - int(0.05 * sr), big[0] + int(0.05 * sr))    # 100 ms inside the room, whole cycles
            got = np.abs(np.fft.rfft(y[w, 0]))[int(round(hz * 0.1))] / np.abs(np.fft.rfft(t[w, 0]))[int(round(hz * 0.1))]
            want = -cr['depth_db'] if hz == 2500.0 else 0.0
            self.assertAlmostEqual(want, 20 * np.log10(got), delta=0.3, msg=hz)

    def test_the_clap_jumps_over_the_pluck(self):
        """The review: the pluck's notes on the "a" before 2 and 4 rang through each clap's reference window (1-5 kHz,
        60-10 ms before it), so the clap jumped 10.8 dB over the wet mix. In the loop's drums, stab, pling and pluck (no
        vocals or returns, no models needed), the room lifts every clap the pluck leads into by more than 1.2 dB
        (measured: 1.6-2.4), and none of them jumps less than 11 dB (without it: 9.1-14.4)."""
        from tools.sound import master as MS2, mixmeter as MM
        C = self.C
        keys = ('kick', 'clap', 'hat', 'shaker', 'perc', 'stab', 'pluck')
        lanes = {k: C.lane(k) for k in keys}

        def mixed(ls):
            drums, _ = MS2.soft_clip(sum(ls[k] for k in C.DRUM_BUS), C.drum_ceiling_db(ls), C.DRUM_KNEE_DB, 4)
            room = C.pump(**C.KICK_ROOM)
            return C.dc_clean(drums + C.music_eq(sum(ls[k] * (room if k in C.ROOMED else 1.0)
                                                     for k in ('perc', 'stab', 'pluck'))))
        raw = dict(lanes, pluck=C.panned(C.air(C.pluck_lane(), 'pluck') * C.pump(**C.PUMP['pluck']), 0)
                   * D.undb(C.LANES['pluck'][1]))
        claps = [(s, C.G.at(s)) for s, v in C.CLAP if v >= C.CLAP_ROOM['min_vel']]
        led = [o for s, o in claps if any(s - 2 <= p < s for p, *_ in C.PLUCK)]   # a pluck note in the 16th before
        self.assertEqual(5, len(led))
        with_room = MM.jump_db(mixed(lanes), led, 1000.0, 5000.0)
        without = MM.jump_db(mixed(raw), led, 1000.0, 5000.0)
        for a, b in zip(with_room, without):
            self.assertGreater(a - b, 1.2)
        self.assertGreater(min(with_room), 11.0)
        self.assertLess(min(without), 11.0)

    @unittest.skipUnless(__import__('os').environ.get('WUBANSIA_MODELS_DIR'), 'WUBANSIA_MODELS_DIR not set')
    def test_the_robot_limiter_levels_the_phrase(self):
        """The review: the robot's "the" (16th 51), a 20-30 ms swell at the end of its syllable, peaked 1.8 dB over
        "let", the phrase's next loudest syllable, and set the club master's cap. After the limiter no 16th of the
        phrase peaks more than 0.3 dB over the rest, and its 1-4 kHz (the words) is within 1 dB of what it was."""
        C = self.C
        from tools.sound import mixmeter as MM
        y = C.panned(C.robot_lane(), 0) * D.undb(C.fader_db('robot'))
        before = C.air(C.vox_comp(y, 'robot'), 'robot')
        after = C.lane('robot')

        def peaks(x):
            up = np.abs(D.up(MM.wrap(x, 64, 64), 4)[256:256 + 4 * C.L]).max(1)
            return [20 * np.log10(up[4 * C.G.at(s):4 * C.G.at(s + 1)].max()) for s in range(C.ROBOT[0], C.ROBOT[0] + 11)]
        pb, pa = peaks(before), peaks(after)
        self.assertEqual(3, int(np.argmax(pb)))                      # "the", 16th 51, the phrase's peak...
        self.assertGreater(sorted(pb)[-1] - sorted(pb)[-2], 1.5)    # ...by 1.8 dB
        self.assertLess(sorted(pa)[-1] - sorted(pa)[-2], 0.3)
        band = lambda x: np.sum(D.bfilt(np.concatenate([x, x, x]), (1000.0, 4000.0), 'bandpass', order=4,  # noqa
                                        zero_phase=True)[C.L:2 * C.L] ** 2)
        self.assertLess(abs(10 * np.log10(band(after) / band(before))), 1.0)

    def test_the_robot_lane_goes_through_its_limiter(self):
        """The same fix without the vocal models (CI has none): a sung phrase built here on the robot's 11 16ths, its
        4th swelling 6 dB over its last 25 ms, stands in for the robot file (RAW['robot']). Through the vocal bus it
        peaks 1.8 dB over the next loudest 16th, as the real "the" did; lane('robot') brings it within 0.3 dB of the
        rest, so a lane that skips the limiter fails here."""
        import unittest.mock
        C = self.C
        from tools.sound import mixmeter as MM
        y = np.zeros(C.L)
        for s in range(11):
            a, b = C.G.at(C.ROBOT[0] + s), C.G.at(C.ROBOT[0] + s + 1)
            n = int(0.85 * (b - a))
            t = np.arange(n) / D.SR
            env = np.minimum(1.0, t / 0.005) * np.minimum(1.0, (n - 1 - np.arange(n)) / (0.005 * D.SR))
            if s == 3:
                m = int(0.025 * D.SR)
                k = np.arange(n) - (n - m)
                env = env * np.where(k > 0, 1 + (D.undb(6.0) - 1) * np.sin(np.pi * np.clip(k / m, 0, 1)), 1.0)
            y[a:a + n] += 0.12 * env * sum(np.sin(2 * np.pi * 196.0 * h * t) / h for h in range(1, 12))
        phrase_ = D.st(y)

        def peaks(x):
            up = np.abs(D.up(MM.wrap(x, 64, 64), 4)[256:256 + 4 * C.L]).max(1)
            return [20 * np.log10(up[4 * C.G.at(s):4 * C.G.at(s + 1)].max()) for s in range(C.ROBOT[0], C.ROBOT[0] + 11)]
        before = C.air(C.vox_comp(C.panned(phrase_, C.LANES['robot'][2]) * D.undb(C.fader_db('robot')), 'robot'),
                       'robot')
        C.lane.cache_clear()
        self.addCleanup(C.lane.cache_clear)
        with unittest.mock.patch.dict(C.RAW, robot=lambda: phrase_):
            after = C.lane('robot')
        pb, pa = peaks(before), peaks(after)
        self.assertEqual(3, int(np.argmax(pb)))
        self.assertGreater(sorted(pb)[-1] - sorted(pb)[-2], 1.5)
        self.assertLess(sorted(pa)[-1] - sorted(pa)[-2], 0.3)

    def test_the_sends_are_the_kits_own(self):
        """The standard's future-house space: a short, bright, pumped plate (1.2-1.8 s, a 1/64-note pre-delay, 29 ms at
        128 BPM) instead of the shared 2.6 s hall, and an echo on the 8th (1/8 or 1/4 is the convention), brighter and
        thinner than the shared tape echo. Two 16ths keep every repeat on the kit's swing: odd 16ths land on odd 16ths."""
        C = self.C
        self.assertTrue(1.2 <= C.SPACE['t60'] <= 1.8)
        self.assertAlmostEqual(K.STEP / 4, C.SPACE['predelay'], places=12)
        self.assertGreater(C.SPACE['damp_hz'], C.S.SPACE['damp_hz'])
        self.assertLess(C.SPACE['t60'], C.S.SPACE['t60'])
        self.assertEqual(2, C.ECHO_STEPS)
        self.assertGreater(C.ECHO['hp_hz'], C.S.ECHO['hp_hz'])
        self.assertGreater(C.ECHO['lp_hz'], C.S.ECHO['lp_hz'])
        d = C.ECHO_STEPS * K.STEP * D.SR
        for s in range(C.STEPS - C.ECHO_STEPS):
            self.assertLess(abs(C.G.at(s) + d - C.G.at(s + C.ECHO_STEPS)), 1.0, s)

    def test_the_low_end_passes_its_rule_in_both_characters(self):
        C = self.C
        for name, limit in (('ContextLoopLow', qa.MUD_VS_SUB_DB), ('ContextLoopLowWarm', qa.BODY_VS_SUB_DB)):
            it = item(name)
            t = it.takes('FUT03')[0]
            out = it.render(t)
            self.assertEqual(C.L, len(out.audio))
            self.assertEqual(tuple(C.kicks()), tuple(out.hits))
            self.assertEqual(tuple(C.club_hits()['kick']), tuple(out.top_hits))   # the crest floor reads each kick
            self.assertEqual(15, len(out.top_hits))
            checks = {c.name: c for c in qa.check_take(out.audio, t.spec(out))}
            self.assertEqual('pass', checks['loop_kick_crest'].status, name)
            for key in ('dc', 'seam', 'low_side', 'mud_share', 'loop_mud_vs_sub', 'loop_body_vs_sub',
                        'loop_mud_before_hit', 'body_over_saw', 'body_share'):
                if key in checks and checks[key].status != 'n/a':
                    self.assertEqual('pass', checks[key].status, f'{name} {key} {checks[key].value}')
            worst = max(r['vs_sub_db'] for r in qa.loop_mud(out.audio, sorted(out.hits)))
            self.assertLess(worst, limit - 2.0, name)                  # with a margin of at least 2 dB


if __name__ == '__main__':
    unittest.main()
