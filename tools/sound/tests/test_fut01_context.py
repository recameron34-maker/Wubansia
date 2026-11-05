"""FUT01's round 3 context loop (recipes/fut01/context.py), the music it keeps (music.py) and the listening previews
(preview.py), as written: 4 bars, every lane on a round 3 file, the picked bassline with room on every kick of the
loop's own pattern and nothing but ghosts on a loud kick's tail, its top keyed by the pluck and riding the move, the
chops of the line on its swing and clear of release titles, the re-pitched hooks answering after a snare, the shaker on
every 16th up to the fill, the drums stopping for one beat for the fill's roll, the sends a garage plate and a thinner
echo, and the club loop the wet loop through the shared master at the genre's own settings. No audio is rendered here:
the loops' sound is checked by render.py's QA."""
import unittest
from unittest import mock

import numpy as np

from tools.sound import dsp48 as D
from tools.sound import kits
from tools.sound import master as MS
from tools.sound import phrase as Ph
from tools.sound import qa
from tools.sound import sends as S
from tools.sound.kits import fut01 as K
from tools.sound.recipes.fut01 import basslines as B
from tools.sound.recipes.fut01 import context as C
from tools.sound.recipes.fut01 import fx, music, preview, vox
from tools.sound.tests.test_fut01_basslines import on_a_kick_tail

SR = D.SR
LOOPS = ('ContextLoop', 'ContextLoopWarm', 'ContextLoopWet', 'ContextLoopClub', 'ContextLoopLow', 'ContextLoopLowWarm')
KEPT_MUSIC = ('AirPadFsm9', 'ChromePluck', 'ChromePluckPhrase')


def chop_words(name):
    """A chop's words and first and last syllable: from the picked line's chops, or the chop a re-pitched hook is cut
    from."""
    if name in vox.HIGH_BY_NAME:
        name = vox.HIGH_BY_NAME[name][1]
    _, first, last, words, _, _ = vox.PICK_CHOP_BY_NAME[name]
    return words, first, last


class TestContextLoop(unittest.TestCase):
    def setUp(self):
        self.r3 = {it.name: it for it in kits.items(K, sets=('r3',))}

    def test_four_bars(self):
        self.assertEqual(343880, C.L)
        self.assertLess(abs(C.L - 16 * K.BEAT * SR), 1.0)
        self.assertEqual(64, C.G.steps)
        self.assertEqual(C.L, 2 * B.G.length)        # the 2-bar line and the pluck phrase tile it exactly

    def test_every_lane_plays_a_round_3_file(self):
        names = [v[0] for k, v in C.LANES.items() if v[0]] + list(C.REESE.values()) + [c[1] for c in C.CHOPS]
        for name in names:
            with self.subTest(name=name):
                self.assertIn(name, self.r3)
        for _, name, _, _ in C.CHOPS:
            self.assertTrue(name in vox.PICK_CHOP_BY_NAME or name in vox.HIGH_BY_NAME, "a chop of the pick's line")

    def test_the_round_3_sounds_play(self):
        """The loop plays kick B, snare C and the shaker; kick A and snares A and B wait in the kit for the A/B."""
        self.assertEqual('KickPunch', C.LANES['kick'][0])
        self.assertEqual('SnareClap', C.LANES['snare'][0])
        self.assertEqual('Shaker', C.LANES['shaker'][0])
        for name in ('KickSplit', 'SnareMetal', 'SnareModal'):
            self.assertIn(name, self.r3)

    def test_the_picked_line_and_its_characters(self):
        self.assertEqual(B.PICK, C.LINE)
        self.assertIn(C.WET_BASS, C.REESE)
        self.assertEqual({'clean', 'warm'}, set(C.REESE))

    def test_the_drums_stop_for_the_fill_and_it_ends_on_the_downbeat(self):
        """The drums stop for one beat, as in rounds 1 and 2, for the fill's second beat (its roll and swell), which
        ends on the loop's downbeat; bar 4 keeps bar 2's kicks, the and-of-3 kick that "floor" lands on among them."""
        for part in (C.KICK, C.SNARE, C.HAT, C.PERC, C.SHAKER):
            self.assertTrue(all(s < C.FILL_AT for s, _ in part))
        self.assertEqual(C.L, C.G.at(C.FILL_AT) + fx.swung(fx.LIFT_STEPS) - fx.swung(C.FILL_FROM))
        self.assertEqual(0, C.FILL_AT % 4, 'the fill starts on a beat')
        self.assertEqual(C.STEPS - 4, C.FILL_AT, 'one beat of drum stop')
        self.assertEqual(fx.LIFT_STEPS - 4, C.FILL_FROM, "the fill's last beat")
        bar = lambda b: [(s - 16 * b, v) for s, v in C.KICK if 16 * b <= s < 16 * b + 16]
        self.assertEqual(bar(1), bar(3))
        self.assertIn((58, 0.8), C.KICK)
        self.assertEqual({58, 59}, {s for s, _ in C.HAT if s >= 56}, "bar 4's hats until the stop")

    def test_the_shaker_swings_on_every_16th(self):
        """Every 16th up to the fill, the same accent shape in every beat: the "a" (the beat's last 16th, swung late)
        the loudest, the "and" with the hat next, the "e" the softest; every accent on a layer of its own."""
        self.assertEqual(list(range(C.FILL_AT)), [s for s, _ in C.SHAKER])
        beat = C.SHAKER_BEAT
        self.assertEqual(3, int(np.argmax(beat)), 'the "a" loudest')
        self.assertEqual(1, int(np.argmin(beat)), 'the "e" softest')
        self.assertGreater(beat[2], beat[0])
        self.assertTrue(all(v == beat[s % 4] for s, v in C.SHAKER))
        self.assertEqual({2}, {s % 4 for s, v in C.HAT if v >= 0.8}, 'the loud hats on the "and"')
        shaker = kits.items(K, only=('Shaker',))[0]
        nearest = {min(range(len(shaker.layers)), key=lambda i: abs(shaker.layers[i] - v)) for v in beat}
        self.assertEqual(len(beat), len(nearest))

    def test_the_top_spreads(self):
        """The hats keep rounds 1 and 2's place (+0.2); the shaker, the 16th that drives the shuffle, mirrors them near
        the centre (at most 0.3 away, under 4.5 dB louder on one side), its width its own stereo noise
        (drums.SHAKER_SIDE): the round 3 review found it at -0.6, 9.8 dB louder on the left."""
        hat, shaker = C.LANES['hat'][2], C.LANES['shaker'][2]
        self.assertEqual(0.2, hat)
        self.assertLess(hat * shaker, 0.0)
        self.assertLessEqual(abs(shaker), 0.3)
        gains = Ph.pan(np.ones(1), shaker)[0]
        self.assertLess(abs(D.db(gains[0] / gains[1])), 4.5)

    def test_nothing_but_ghosts_on_a_loud_kick_s_tail(self):
        line = tuple((s + r * B.STEPS, *rest) for r in range(2) for s, *rest in B.LINES[C.LINE])
        self.assertEqual([], on_a_kick_tail(line, C.KICK, C.G))

    def test_the_bass_top_ducks_under_every_pluck_note(self):
        onsets = C.pluck_onsets()
        self.assertEqual(len(C.PLUCK_AT) * len(music.PHRASE), len(onsets))
        self.assertEqual(sorted(onsets), sorted(C.G.at(a + s) for a in C.PLUCK_AT for s, *_ in music.PHRASE))
        self.assertGreaterEqual(C.PLUCK_DUCK['depth_db'], 6.0)
        self.assertGreaterEqual(C.PLUCK_DUCK['ramp'] + C.PLUCK_DUCK['hold'], 0.040, "the pluck's first 40 ms")
        self.assertLessEqual(C.PLUCK_DUCK['ramp'], 0.003, 'down before the reese mids rise (3 ms after the note)')
        self.assertGreaterEqual(C.PLUCK_XOVER, 1000.0, 'the low end and the body pass untouched')

    def test_the_line_leaves_every_kick_room(self):
        """The picked line, played twice against the loop's own 4-bar kicks: no note starts inside a kick's first
        80 ms, and every release has ended before the next kick's before-hit window (40-15 ms)."""
        kicks = sorted(C.G.at(s) for s, _ in C.KICK)
        line = tuple((s + r * B.STEPS, *rest) for r in range(2) for s, *rest in B.LINES[C.LINE])
        for on, _, n_on in B.voices(line, C.G):
            with self.subTest(onset=on):
                before = max(k for k in kicks + [kicks[0] - C.L] if k <= on)
                after = min(k for k in kicks + [kicks[0] + C.L] if k > on)
                self.assertGreaterEqual(on - before, int(qa.KICK_OWN_S * SR))
                self.assertLessEqual(on + n_on + int(B.RELEASE * SR), after - int(qa.BEFORE_HIT[0] * SR))

    def test_the_reese_move(self):
        """The move rides over A's 450 Hz wall only (the sub and B's body pass), as a CC74 lane mapped as the engine's
        lane filter maps it: shut (under 650 Hz) from 50 ms before every backbeat (beats 2 and 4) to 50 ms after it,
        and under 900 Hz from 60 ms before (the clap's jump reads the mix from there; the lane's smoother lags the
        CC), open (over 2.5 kHz) between each backbeat and the next, the widest in bar 4, into the turnaround. Round 3's
        first move (shut on the downbeats, where the line rests) moved each note's top by 2.4 dB at most."""
        self.assertEqual(450.0, C.BASS_MOVE_XOVER)
        self.assertEqual(C.backbeat_move(), C.BASS_MOVE)
        steps = [s for s, _ in C.BASS_MOVE]
        self.assertEqual(steps, sorted(steps))
        self.assertTrue(all(0 <= s < C.STEPS and 0 <= cc <= 127 for s, cc in C.BASS_MOVE))
        fc = C.move_cutoff()
        self.assertEqual(C.L, len(fc))
        self.assertGreater(fc.min(), C.BASS_MOVE_XOVER)
        self.assertLess(fc.max(), 6500.0)
        ms = lambda t: int(t * SR)
        beats = [16 * bar + b for bar in range(4) for b in C.BACKBEATS]
        self.assertEqual(sorted({s for s, v in C.SNARE if v >= 0.8}), [b for b in beats if b < C.FILL_AT])
        for i, beat in enumerate(beats):
            at = C.G.at(beat)
            nxt = C.G.at(beats[i + 1]) if i + 1 < len(beats) else C.L + C.G.at(beats[0])
            with self.subTest(backbeat=beat):
                self.assertLess(fc[at - ms(0.050):at + ms(0.050)].max(), 650.0, 'shut into the backbeat')
                self.assertLess(fc[at - ms(0.060)], 900.0, 'closed over the clap jump window')
                self.assertGreater(fc[np.arange(at + ms(0.050), nxt - ms(0.060)) % C.L].max(), 2500.0,
                                   'open before the next')
        bar_max = [fc[C.G.at(16 * b):C.G.at(16 * b + 16) if b < 3 else C.L].max() for b in range(4)]
        self.assertEqual(3, int(np.argmax(bar_max)), 'the widest in bar 4')
        self.assertLess(0.7, C.BASS_MOVE_Q)

    def test_the_bass_ducks_under_every_backbeat(self):
        """The bass lane dips 3-6 dB under every snare of velocity 0.8 or more, not under the ghosts, and for less time
        than the kick ducker holds: the louder bass's sub and body under the snare would set the loop's peaks
        there."""
        g = D.db(C.snare_duck())
        self.assertEqual(C.L, len(g))
        ms = lambda t: int(t * SR)
        for step, v in C.SNARE:
            at = C.G.at(step)
            with self.subTest(snare=step):
                if v >= C.SNARE_DUCK_VEL:
                    self.assertTrue(-6.0 <= g[at + ms(0.010)] <= -3.0)
                    self.assertGreater(g[at + ms(0.120)], -0.1, 'back before the next 16th')
                else:
                    self.assertGreater(g[at + ms(0.010)], -0.01)
        self.assertLess(C.SNARE_DUCK['ramp'] + C.SNARE_DUCK['hold'], B.DUCK['ramp'] + B.DUCK['hold'])
        self.assertLessEqual(C.SNARE_DUCK['depth_db'], B.DUCK['depth_db'])

    def test_the_low_mids_sit_central(self):
        """The lanes with low mids are mono under their cut: the bass under 450 Hz (its body in the centre), the pad
        under 350 Hz; the bass's side over that narrowed."""
        self.assertGreaterEqual(C.MONO_BELOW['bass'], C.BASS_MOVE_XOVER)
        self.assertGreaterEqual(C.MONO_BELOW['pad'], 300.0)
        self.assertTrue(0.0 < C.BASS_WIDTH < 1.0)

    def test_the_eq_lifts_only_the_low_mids_and_cuts_only_above(self):
        """Boosts sit in the low mids (under 300 Hz), cuts at 450 Hz and up: the fill of the hole, never a lift of the
        crowded mids, and never more than 6.5 dB."""
        for lane, bands in C.EQ.items():
            for fc, gain, q in bands:
                with self.subTest(lane=lane, fc=fc):
                    self.assertTrue((gain > 0 and fc < 300.0) or (gain < 0 and fc >= 450.0))
                    self.assertLessEqual(abs(gain), 6.5)
                    self.assertTrue(0.5 <= q <= 2.0)

    def test_the_inserts_can_be_off(self):
        """With no inserts a lane passes as it is, so the loop's round 2 balance can be rebuilt from this module."""
        x = np.random.default_rng(3).standard_normal((4800, 2))
        self.assertIs(x, C.width(x, 1.0))
        saved = (C.MONO_BELOW, C.EQ)
        try:
            C.MONO_BELOW, C.EQ = {}, {}
            self.assertIs(x, C.inserts('pad', x))
        finally:
            C.MONO_BELOW, C.EQ = saved

    def test_the_kick_clip_and_the_master_stay_inside_the_standard(self):
        """The standard lets a kick alone take 2-4 dB of clip; the master keeps the genre's own settings, so its peak
        reduction stays inside 4 dB."""
        self.assertTrue(2.0 <= C.KICK_CLIP_DB <= 4.0)
        s = MS.settings(K, **C.MASTER)
        self.assertLessEqual(s.max_reduction_db, 4.0)
        self.assertEqual(qa.CLUB[K.DIRECTION]['lufs'][0], s.target_lufs)

    def test_the_sends(self):
        """The Space is a garage plate (0.8-1.4 s, a 1/64-note pre-delay), the Echo's loop high-passed over the low
        mids; the hall and the round 2 echo stay the shared defaults (sends.py)."""
        self.assertTrue(0.8 <= C.PLATE['t60'] <= 1.4)
        self.assertAlmostEqual(K.STEP / 4, C.PLATE['predelay'], delta=0.0005)
        self.assertLess(C.PLATE['t60_low'], C.PLATE['t60'])
        self.assertGreaterEqual(C.ECHO['hp_hz'], 450.0)
        self.assertEqual(set(S.SPACE), set(C.PLATE))
        self.assertEqual(set(S.ECHO), set(C.ECHO))
        self.assertEqual(2.6, S.SPACE['t60'])

    def test_chops_land_on_the_line_s_swing(self):
        """A chop of more than one note starts on a 16th of the same parity as in the sung line, so the swing inside
        it lines up with the grid; "floor" lands on the and of 3 of bar 4, where the line sings it in its bar."""
        notes = vox.LINE_BY_NAME[vox.PICK].notes
        words = [(s, w) for s, _, _, w in notes if w != '~']
        for step, name, _, _ in C.CHOPS:
            _, first, last = chop_words(name)
            if last > first:
                with self.subTest(chop=name):
                    self.assertEqual(words[first][0] % 2, step % 2)
        step, name, _, _ = next(c for c in C.CHOPS if c[1] == 'ChopFloor')
        _, first, _ = chop_words(name)
        floor = dict((w, s) for s, w in words)['floor']
        self.assertEqual(48 + floor, step + floor - words[first][0])

    def test_every_floor_lands_on_an_and_of_3_kick(self):
        notes = vox.LINE_BY_NAME[vox.PICK].notes
        words = [(s, w) for s, _, _, w in notes if w != '~']
        floor = dict((w, s) for s, w in words)['floor']
        kicks = {s for s, v in C.KICK if v >= 0.8}
        sung = 0
        for step, name, _, _ in C.CHOPS:
            said, first, _ = chop_words(name)
            if 'floor' in said:
                with self.subTest(chop=name):
                    at = step + floor - words[first][0]
                    self.assertIn(at, kicks)
                    self.assertEqual(10, at % 16, 'the and of 3')
                    sung += 1
        self.assertEqual(3, sung)

    def test_the_hooks_start_clear_of_a_backbeat(self):
        """A re-pitched hook's onset (its consonant leads the note by up to 60 ms) never falls in the 60-10 ms before a
        backbeat, which the in-loop clap jump reads: "me" a 16th before bar 3's last snare took 3 dB off it. Each
        plays under the line it comes from."""
        snares = [C.G.at(s) for s, v in C.SNARE if v >= 0.8]
        hooks = [c for c in C.CHOPS if c[1] in vox.HIGH_BY_NAME]
        self.assertEqual(2, len(hooks))
        ms = lambda t: int(t * SR)
        for step, name, _, level in hooks:
            p = C.G.at(step)
            with self.subTest(hook=name):
                for s in snares:
                    self.assertFalse(s - ms(0.070) < p < s + ms(0.050), f'{name} on 16th {step}')
                self.assertLess(level, 0.0, 'under the line')

    def test_only_the_phrase_s_end_is_thrown(self):
        """The garage convention the standard cites: the chops on a short plate, the echo for the throw at the phrase's
        end. "floor" and its double take the full throw into the fill; "me" (bar 3's answer) keeps a little plate and no
        echo, so its repeats never sit on bar 4's kicks."""
        sends = {name: own for _, name, own, _ in C.CHOPS}
        self.assertEqual(C.THROW, tuple(sends['ChopFloor']))
        self.assertEqual(C.THROW, tuple(sends['ChopFloorFs5']))
        space, echo = sends['ChopMeE5']
        self.assertEqual(0.0, echo)
        self.assertTrue(0.0 < space < C.LANES['vox'][3])
        self.assertEqual(max(s for s, _, own, _ in C.CHOPS if own and own[1] > 0), max(s for s, *_ in C.CHOPS),
                         'the echo only at the end')

    def test_the_loop_sings_no_release_title(self):
        """The loop's chops sing pieces of the line that the title check kept (vox.CHOP_CHECKS); round 2's first loop
        sang two titles of released singles."""
        for _, name, _, _ in C.CHOPS:
            words, _, _ = chop_words(name)
            with self.subTest(chop=name):
                if ' ' in words:
                    self.assertTrue(vox.CHOP_CHECKS[words].startswith('kept'), words)

    def test_the_loop_items(self):
        for name in LOOPS:
            with self.subTest(name=name):
                it = self.r3[name]
                self.assertTrue(it.loop)
                self.assertEqual('loop', it.category)
                self.assertEqual(('r3',), it.sets)
        self.assertEqual('gate', self.r3['ContextLoopLow'].qa['mud'])
        self.assertEqual('bass-body', self.r3['ContextLoopLowWarm'].qa['mud'])
        for name in ('ContextLoopLow', 'ContextLoopLowWarm'):
            self.assertIn('with_kick', self.r3[name].tags)
        self.assertEqual({C.LOOP_LUFS}, {self.r3[n].level for n in LOOPS[:3]}, 'dry A, dry B and wet at one loudness')

    def test_the_club_loop(self):
        """ContextLoopClub is a mastered loop (tag club: no high-pass and no level of its own), and its octave windows
        are exempted (125 and 250 Hz) only with the reason in its qa."""
        it = self.r3['ContextLoopClub']
        self.assertIn('club', it.tags)
        self.assertIsNone(it.hp_hz)
        self.assertIsNone(it.level)
        self.assertEqual(('125', '250'), it.qa['club_bands'])
        self.assertIn('125 and 250 Hz', it.qa['why'])

    def test_the_music_comes_back_unchanged(self):
        s1a = {it.name for it in kits.items(K, sets=('s1a',))}
        r2 = {it.name for it in kits.items(K, sets=('r2',))}
        for name in KEPT_MUSIC:
            with self.subTest(name=name):
                self.assertIn(name, self.r3)
                self.assertIn(name, r2)
                self.assertIn(name, s1a)


class TestKickLane(unittest.TestCase):
    """The kick the loops play and the kick the A/B grooves play: renders KickPunch, KickSplit and the grooves' other
    drums (about 5 s). The round 3 review found the loops' kick B clipped 4 dB onto ADR 0011's floor (3.6 dB before
    the renderer's high-pass) while the A/B played it unclipped."""

    def test_every_full_velocity_kick_keeps_its_crest(self):
        """In the clipped lane, before the renderer's high-pass, each full-velocity kick reads 5-7 dB (qa.kick_crest_db,
        the band round 1 liked), and the clip still takes over 1 dB off the lane's peak."""
        lane, _ = C.kick_lane()
        crests = C.kick_crests(lane, C.full_kicks())
        self.assertEqual(4, len(crests))
        for at, c in zip(C.full_kicks(), crests):
            with self.subTest(kick=at):
                self.assertTrue(5.0 <= c <= 7.0, c)
        raw = C.fader('kick', Ph.hits_lane(K, C.LANES['kick'][0], C.KICK, C.G))
        self.assertGreater(D.true_peak_db(raw) - D.true_peak_db(lane), 1.0)

    def test_the_bass_leads_the_kick_lane(self):
        """The bass, UK garage's hook, sits over the kick lane in loudness (ducked under every kick)."""
        lane, _ = C.kick_lane()
        self.assertGreater(C.BASS_LUFS, qa.lufs(lane) + 0.5)

    def test_the_low_loops_declare_their_full_velocity_kicks(self):
        """ContextLoopLow and ContextLoopLowWarm name their four full-velocity kicks (Out.top_hits), the club master's
        own, so QA reads the crest floor on each (loop_kick_crest); without them that check reads n/a and passes.
        Renders both bass lanes (about 20 s)."""
        self.assertEqual(C.full_kicks(), C.club_hits()['kick'])
        for char in ('clean', 'warm'):
            with self.subTest(char=char):
                out = C.context_low(char)
                self.assertEqual(C.full_kicks(), list(out.top_hits))
                self.assertEqual(4, len(out.top_hits))
                self.assertEqual(C.kick_lane()[1], out.hits)

    def test_the_grooves_play_the_kick_the_loops_play(self):
        """Both drum grooves run their kick lane through the loops' clip: kick B's full-velocity kicks read what the
        loop's first two do (both rr1) within 0.1 dB, and kick A's sit in the same 5-7 dB band."""
        loop = C.kick_crests(C.kick_lane()[0], C.full_kicks())
        for fn in (preview.drum_groove_punch, preview.drum_groove_split):
            out = fn(None)
            crests = out.meta['kick_crest_db_full_kicks']
            with self.subTest(groove=fn.__name__):
                self.assertEqual(C.KICK_CLIP_DB, out.meta['kick_clip_db'])
                self.assertEqual(2, len(crests))
                self.assertTrue(all(5.0 <= c <= 7.0 for c in crests), crests)
                if out.meta['kick'] == C.LANES['kick'][0]:
                    for got, want in zip(crests, loop[:2]):
                        self.assertAlmostEqual(want, got, delta=0.1)


class TestPreviews(unittest.TestCase):
    def test_previews_are_tagged(self):
        for it in preview.ITEMS:
            with self.subTest(name=it.name):
                self.assertIn('preview', it.tags)
                self.assertEqual('loop', it.category)
                self.assertIn('r3', it.sets)
        r2 = {it.name for it in preview.ITEMS if 'r2' in it.sets}
        self.assertEqual({'HatPatternFM', 'HatPatternRing', 'AirPadHold', 'ShiftRiserDrop'}, r2)

    def test_hat_patterns_play_the_loop_s_hat_part(self):
        self.assertEqual(tuple((s, v) for s, v in C.HAT if s < 32), preview.HATS)
        self.assertEqual(Ph.loop_len(K, 32), preview.G2.length)
        hats = [it for it in preview.ITEMS if it.name.startswith('HatPattern')]
        self.assertEqual(2, len(hats))
        self.assertEqual(1, len({it.level for it in hats}), 'the A/B at one loudness')

    def test_the_drum_grooves_are_the_kick_a_b(self):
        """The grooves play the same drums at one loudness: Split and Punch differ only in the kick, Punch and
        Swing61 only in the swing."""
        grooves = {it.name: it for it in preview.ITEMS if it.name.startswith('DrumGroove')}
        self.assertEqual({'DrumGrooveSplit', 'DrumGroovePunch', 'DrumGrooveSwing61'}, set(grooves))
        self.assertEqual(1, len({it.level for it in grooves.values()}))
        self.assertEqual({'snare', 'hat', 'shaker'}, set(preview.GROOVE_LANES))
        for it in grooves.values():
            self.assertIn('with_kick', it.tags)

    def test_the_heavier_swing_a_b(self):
        """The standard's optional A/B: the shuffle at 0.60-0.62 against the kit's 0.57. Swing61 lays every drum lane
        on the heavier swing (even 16ths where they were, odd ones later); Punch keeps the kit's."""
        self.assertTrue(0.60 <= preview.HEAVY_SWING <= 0.62)
        self.assertEqual(0.57, K.SWING)
        g = preview.groove_grid(preview.HEAVY_SWING)
        self.assertEqual(preview.G2.length, g.length)
        late = (2 * preview.HEAVY_SWING - 1 - (2 * K.SWING - 1)) * g.s16
        for step in range(32):
            with self.subTest(step=step):
                self.assertAlmostEqual(g.at(step) - preview.G2.at(step), late if step % 2 else 0.0, delta=1.0)
        self.assertEqual(K.SWING, preview.G2.swing, 'the shared 2-bar grid keeps the kit swing')
        seen = {}

        def lane(kit, name, hits, grid):
            seen.setdefault(name, set()).add(grid.swing)
            return np.zeros((grid.length, 2))
        for fn, swing in ((preview.drum_groove_swing61, preview.HEAVY_SWING), (preview.drum_groove_punch, K.SWING)):
            seen.clear()
            with mock.patch.object(preview.Ph, 'hits_lane', lane):
                out = fn(None)
            with self.subTest(fn=fn.__name__):
                self.assertEqual({'KickPunch', 'SnareClap', 'ClosedHatFM', 'Shaker'}, set(seen))
                self.assertEqual({swing}, set().union(*seen.values()))
                self.assertEqual(swing, out.meta['swing'])

    def test_the_drop_is_the_picked_bassline(self):
        self.assertIn(preview.DROP_BASS, {it.name for it in B.ITEMS})


if __name__ == '__main__':
    unittest.main()
