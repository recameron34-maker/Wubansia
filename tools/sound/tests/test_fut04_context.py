"""FUT04 "Furnace Code" context loop and listening previews (recipes/fut04/context.py, preview.py): what the
arrangement promises holds without rendering a note. The loop is exactly 4 bars on the kit's grid and tiles the 2-bar
phrases and the drone's loop; the kick is four on the floor with one gap under the fill's last beat; the acid line rests
on every kick; the rumble plays under all 4 bars (round 3) and ends on the downbeat; the hats are choked and leave the
fill bar, the ride rides the off-beats with them; the anvil answers in the blip sequence's gaps; the robot calls
before the chops answer, and the kick leads (round 3: the vocals 2-3.5 LU under it, through their bus) over a bass
that dips under them; every pitch is in E minor; the low end takes no sends; the buses keep the sub out of the drum
bus's grit, and the bass's sub ducks under every kick (the fix pass); the items carry their low-end rules, each with its
own reason, and the club loop its master; the previews are whole bars."""
import unittest

import numpy as np

from tools.sound import dsp48 as D
from tools.sound import phrase as Ph
from tools.sound import qa
from tools.sound.kits import fut04 as K
from tools.sound.recipes.fut04 import bass, context as C, drums, fx, lead, pad, perc, preview as PV, vox

SR = D.SR


def bar_len(bars):
    return bars * 4 * K.BEAT * SR


def items(module):
    return {it.name: it for it in module.ITEMS}


class TestContextGrid(unittest.TestCase):
    def test_four_bars_that_tile_the_phrases(self):
        self.assertEqual(64, C.STEPS)
        self.assertEqual(Ph.loop_len(K, 64), C.L)
        self.assertLess(abs(C.L - bar_len(4)), 1.0)
        self.assertEqual(C.L, 2 * bass.G.length)                     # the acid line, twice
        self.assertEqual(C.L, 2 * Ph.Grid(K, lead.PHRASE_STEPS).length)   # the blip sequence, twice
        self.assertEqual(C.L, Ph.loop_len(K, pad.LOOP_STEPS))        # the drone's loop region
        self.assertEqual(K.SWING, C.G.swing)

    def test_the_fill_and_the_rumble_end_on_the_downbeat(self):
        self.assertEqual(48, C.FILL_AT)
        self.assertEqual(C.L, C.G.at(C.FILL_AT) + fx.swung(16))
        self.assertEqual((0, 32), C.RUMBLE_AT)                        # round 3: its 2 bars twice, all 4 bars
        self.assertEqual(C.L, C.G.at(max(C.RUMBLE_AT)) + bass.L)
        self.assertEqual(C.G.at(32), bass.L)

    def test_previews_are_whole_bars(self):
        for steps, bars in ((PV.FILL_STEPS, 2), (PV.RISER_STEPS, 9), (PV.RUMBLE_STEPS, 2)):
            self.assertLess(abs(Ph.loop_len(K, steps) - bar_len(bars)), 1.0)
        G = Ph.Grid(K, PV.FILL_STEPS)
        self.assertEqual(G.length, G.at(16) + fx.swung(16))                 # the fill ends on the preview's end
        G = Ph.Grid(K, PV.RISER_STEPS)
        self.assertEqual(G.at(128), fx.RISER_N)                             # the riser ends on bar 9's 1


class TestContextParts(unittest.TestCase):
    def test_four_on_the_floor_with_a_gap_under_the_roll(self):
        self.assertEqual([s for s in range(0, 64, 4) if s != C.KICK_OUT], [s for s, _ in C.KICK])
        self.assertEqual(60, C.KICK_OUT)                              # beat 4 of the fill bar
        self.assertTrue(all(v == 1.0 for _, v in C.KICK))

    def test_the_acid_line_rests_on_every_kick(self):
        """Each note of the picked line, tiled over the 4 bars, starts after the kick's own 80 ms and is silent ROOM
        before the next kick of the loop."""
        kicks = [C.G.at(s) for s, _ in C.KICK] + [C.L]
        for half in (0, bass.L):
            for on, end, *_ in bass.note_spans(bass.LINE_BY_NAME[C.LINE]):
                on, end = on + half, end + half
                with self.subTest(on=on):
                    k = max(p for p in kicks if p <= on)
                    nxt = min(p for p in kicks if p > on)
                    self.assertGreaterEqual(on - k, int(qa.KICK_OWN_S * SR))
                    self.assertLessEqual(end + int(bass.NOTE_END * SR), nxt - int(bass.ROOM * SR))

    def test_the_rumble_rests_where_the_kick_does(self):
        self.assertTrue(set(C.RUMBLE_AT) <= {s for s, _ in C.KICK})
        self.assertGreater(C.KICK_OUT, max(C.RUMBLE_AT))
        t, level = bass.PUMP[-1]
        self.assertLess(t, K.BEAT)                                    # its pump is 40 dB down before each next kick,
        self.assertLessEqual(level, -40.0)                            # so the mute falls in its own silence

    def test_the_snare_backbeat_with_ghosts(self):
        steps = {s for s, v in C.SNARE if v >= 0.8}
        for bar in range(4):
            self.assertTrue({16 * bar + 4, 16 * bar + 12} <= steps, bar)
        self.assertTrue(any(v <= 0.4 and s % 2 for s, v in C.SNARE))  # ghosts on swung 16ths

    def test_the_hats_choke_and_leave_the_fill(self):
        closed = {s for s, _ in C.HAT_CLOSED}
        for s, _ in C.HAT_OPEN:
            self.assertEqual(2, s % 4)
            self.assertNotIn(s, closed)
            self.assertTrue(any(0 < c - s <= 2 for c in closed), s)          # choked within an 8th
        self.assertTrue(all(s < C.HAT_END for s, _ in C.HAT_CLOSED + C.HAT_OPEN))
        self.assertLessEqual(C.HAT_END, C.FILL_AT + 4)
        self.assertEqual('ClosedHat', drums.CHOKE['closed'])

    def test_the_ride_rides_the_off_beats(self):
        """Round 3: the ride on every off-beat 8th with the open hat until the hats stop, not in their choke group."""
        self.assertEqual({s for s, _ in C.HAT_OPEN}, {s for s, _ in C.RIDE})
        self.assertTrue(all(s % 4 == 2 and s < C.HAT_END for s, _ in C.RIDE))
        self.assertNotIn('Ride', (drums.CHOKE['closed'], drums.CHOKE['open']))
        self.assertIn('ride', C.DRUM_BUS)

    def test_the_anvil_answers_in_the_blip_gaps(self):
        for s, _ in C.ANVIL:
            t = s % lead.PHRASE_STEPS
            for step, _, length, _, _ in lead.PHRASE:
                self.assertFalse(step <= t < step + max(length, 1.0), (s, step))
            self.assertLess(s, C.FILL_AT)

    def test_the_vocals_call_and_answer(self):
        robot_end = C.ROBOT_AT + max(s + ln for s, ln in vox.ROBOT_GRID)
        self.assertEqual(0, C.ROBOT_AT % 16)                          # the call on a downbeat
        starts = [s for s, _, _ in C.CHOPS]
        self.assertEqual(sorted(starts), starts)
        self.assertTrue(all(s >= robot_end for s in starts))         # the sung line answers after the call
        self.assertTrue(all(s % 2 == 0 for s in starts))             # the chops' own swung timing lines up
        names = {c[0] for c in vox.PICK_CHOPS}
        for _, name, _ in C.CHOPS:
            self.assertIn(name, names)
        self.assertEqual(1, sum(1 for *_, t in C.CHOPS if t))         # one throw, the last chop
        self.assertIsNotNone(C.CHOPS[-1][2])

    def test_everything_pitched_is_in_e_minor(self):
        notes = list(C.RESONATOR_TONES) + list(pad.CHORD) + [perc.ANVIL_NOTE, vox.ROBOT_NOTE, K.KICK_NOTE]
        notes += [m for _, m, *_ in bass.LINE_BY_NAME[C.LINE].notes] + [m for _, m, *_ in lead.PHRASE]
        notes += [m for *_, ns, _ in fx.FILL for m in ns]
        for n in notes:
            self.assertIn(n % 12, K.SCALE, D.note_name(n))

    def test_every_lane_is_mixed(self):
        self.assertEqual(set(C.LANES), set(C.LANE_LUFS))
        self.assertEqual(set(C.LANES), {'kick', 'bass', 'rumble', 'snare', 'hat', 'ride', 'perc', 'pad', 'lead',
                                        'chops', 'robot', 'fill'})
        for k, (_, pan, *sends) in C.LANES.items():
            self.assertTrue(-1 <= pan <= 1, k)
            self.assertTrue(all(0 <= s <= 1 for s in sends), k)
            self.assertLess(C.LANE_LUFS[k], C.LANE_LUFS['kick'] + 0.01, k)   # the kick leads
        for k in C.LOW:
            self.assertEqual((0.0, 0.0, 0.0), C.LANES[k][2:], k)      # no send on the low end
        self.assertEqual(C.WARM, C.WET_BASS)
        self.assertGreaterEqual(C.LANE_LUFS['hat'] - C.LANE_LUFS['ride'], 3.0)      # the fix pass: under the hats

    def test_the_kick_leads_and_the_vocals_still_read(self):
        """Round 3 (the standard's FUT04 change 7): the vocal lanes sit 2-3.5 LU under the kick (round 2: level with it,
        and the lanes summed 6 LU over the kick), the chops through the vocal bus (a band-pass, a drive, a compressor,
        short slaps); the bass lane's band over 500 Hz still dips under them (the low end under that is untouched),
        and so does the fill's hiss over 2.5 kHz, under "the furnace"."""
        for v in C.VOCALS:
            self.assertTrue(2.0 <= C.LANE_LUFS['kick'] - C.LANE_LUFS[v] <= 3.5, v)
        self.assertTrue(C.VOX_BUS['comp']['gr_db'] > 0 and C.VOX_BUS['slaps'] and C.VOX_BUS['hp_hz'] < 200.0)
        self.assertEqual(('chops',), C.VOX_BUS_LANES)                 # the robot keeps its own driven chain
        self.assertEqual({'bass': C.BASS_DUCK_HZ, 'fill': C.FILL_DUCK_HZ}, C.DIPS)
        self.assertGreaterEqual(C.BASS_DUCK_HZ, 400.0)
        self.assertGreaterEqual(C.FILL_DUCK_HZ, 2000.0)
        self.assertGreater(C.BASS_DUCK['depth_db'], 0.0)
        self.assertTrue(any(s + 4 > C.FILL_AT for s, _, _ in C.CHOPS))   # a chop sings into the fill bar
        self.assertEqual(vox.ROBOT_ITEM, C.LANES['robot'][0])
        self.assertEqual(('chops', 'robot'), C.VOCALS)

    def test_the_buses_keep_the_sub_clean(self):
        """The drum bus's grit is high-passed far above the kick's sub and its knocks; its clip sits at the kick's own
        peak; the bass and the rumble are on no bus that distorts; the drone and the returns pump under every kick."""
        self.assertGreaterEqual(C.DRUM_GRIT['hp_hz'], 200.0)
        self.assertEqual(0.0, C.DRUM_CLIP['over_kick_db'])
        self.assertFalse({'bass', 'rumble'} & set(C.DRUM_BUS + C.MUSIC_BUS))
        self.assertEqual(('pad',), C.PUMPED)
        self.assertTrue(0 < C.PUMP['depth_db'] <= 4.0 and 0 < C.RETURN_PUMP['depth_db'] <= 6.0)
        self.assertLess(C.PUMP['ramp'] + C.PUMP['hold'] + C.PUMP['release'], K.BEAT)   # back before the next kick
        self.assertTrue(4.0 <= C.SPACE['t60'] <= 6.0 and C.SPACE['damp_hz'] <= 4000.0)   # the dark 4-6 s hall

    def test_the_bass_sub_ducks_under_every_kick(self):
        """The fix pass: the bass lane's sub, under 120 Hz, ducks 4-8 dB under each kick (the standard's techno row),
        held over the line's pedal (bass.SUB_PEDAL_S, whose E1 starts in phase with the kick's tail) and back before
        the next kick; above the split, and outside the ducks, the lane is untouched (a zero-phase band split)."""
        sc = C.BASS_SIDECHAIN
        self.assertTrue(4.0 <= sc['depth_db'] <= 8.0)
        self.assertLessEqual(sc['split_hz'], 150.0)
        self.assertGreaterEqual(sc['ramp'] + sc['hold'] + sc['release'], bass.SUB_PEDAL_S)
        self.assertLess(sc['ramp'] + sc['hold'] + sc['release'], K.BEAT - bass.ROOM)
        t = np.arange(C.L) / SR
        lo, hi = np.sin(2 * np.pi * 41.2 * t), np.sin(2 * np.pi * 1000.0 * t)
        y = C.sidechained(D.st(lo + hi))
        k = C.kick_onsets()[1]
        back = sc['ramp'] + sc['hold'] + sc['release']
        under = slice(k + int(0.090 * SR), k + int(0.150 * SR))                  # inside the hold
        after = slice(k + int((back + 0.005) * SR), k + int((K.BEAT - 0.050) * SR))   # released, before the next kick
        band = lambda x, f: np.abs(np.sum(x[:, 0] * np.exp(-2j * np.pi * f * t[:len(x)])))  # noqa: E731
        ref = D.st(lo + hi)
        self.assertAlmostEqual(D.db(band(y[under], 41.2) / band(ref[under], 41.2)), -sc['depth_db'], delta=0.5)
        self.assertAlmostEqual(D.db(band(y[under], 1000.0) / band(ref[under], 1000.0)), 0.0, delta=0.05)
        self.assertLess(np.max(np.abs(y[after] - ref[after])), 0.01)

    def test_the_items_carry_their_rules(self):
        it = items(C)
        self.assertEqual({'ContextLoopClub', 'ContextLoop', 'ContextLoopWarm', 'ContextLoopWet', 'ContextLoopLow',
                          'ContextLoopLowWarm'}, set(it))
        club = it['ContextLoopClub']
        self.assertEqual({'club', 'wet'}, set(club.tags))
        self.assertIsNone(club.hp_hz)
        self.assertIsNone(club.level)
        self.assertEqual(('r3',), club.sets)
        self.assertEqual({'max_reduction_db'}, set(C.MASTER))
        full = [it[n] for n in ('ContextLoop', 'ContextLoopWarm', 'ContextLoopWet')]
        self.assertEqual({C.LOOP_LUFS}, {i.level for i in full})     # one loudness for the A/B and the wet
        for i in full + [club]:
            self.assertEqual('off', i.qa['mud'])
            self.assertTrue(i.qa['why'])
        self.assertIn('wet', it['ContextLoopWet'].tags)
        self.assertEqual('gate', it['ContextLoopLow'].qa['mud'])
        self.assertEqual('bass-body', it['ContextLoopLowWarm'].qa['mud'])
        for n in ('ContextLoopLow', 'ContextLoopLowWarm'):
            self.assertIn('with_kick', it[n].tags)
        for i in it.values():
            self.assertTrue(i.loop and 'r3' in i.sets and i.category == 'loop')
            self.assertEqual(i.name != 'ContextLoopClub', 'r2' in i.sets)
        pv = items(PV)
        self.assertEqual({'DroneLoop', 'RumbleUnderKick', 'StutterFillPreview', 'IronRiserPreview'}, set(pv))
        for i in pv.values():
            self.assertTrue(i.loop and {'r2', 'r3'} <= set(i.sets) and 'preview' in i.tags)
        self.assertEqual('gate', pv['RumbleUnderKick'].qa['mud'])
        self.assertIn('with_kick', pv['RumbleUnderKick'].tags)
        self.assertNotIn('bass', pv['StutterFillPreview'].qa['why'])            # it has no bass: its own reason
        self.assertIn('bass', pv['IronRiserPreview'].qa['why'])


if __name__ == '__main__':
    unittest.main()
