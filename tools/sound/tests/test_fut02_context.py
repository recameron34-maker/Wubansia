"""FUT02 "Orbit Relay" context loop and listening previews (recipes/fut02/context.py, preview.py): what the arrangement
promises holds without rendering a note. The loop is exactly 4 bars on the kit's grid and tiles the 2-bar phrases and
the pad's loop; the kick is four on the floor, every beat on the hard layer, with one gap for the fill; the bass line
rests on every kick, the glide sits between two kicks and lifts before the second; the hats are choked; the fill bar
has no other hats, claps or arp; the arp makes room for the vocals; the vocals call and answer without overlapping;
every pitch is in D minor; the items carry their low-end rules. One class renders: each bassline summed with the kit's
own kick (the groove previews and the loop's low end, no vocal models needed) passes its low-end rule in every gap."""
import unittest

import numpy as np

from tools.sound import dsp48 as D
from tools.sound import kits
from tools.sound import phrase as Ph
from tools.sound import qa
from tools.sound import render as R
from tools.sound.kits import fut02 as K
from tools.sound.recipes.fut02 import bass, context as C, fx, lead, pad, perc, preview as PV, vox

SR = D.SR


def bar_len(bars):
    return bars * 4 * K.BEAT * SR


class TestContextGrid(unittest.TestCase):
    def test_four_bars_that_tile_the_phrases(self):
        self.assertEqual(64, C.STEPS)
        self.assertEqual(Ph.loop_len(K, 64), C.L)
        self.assertLess(abs(C.L - bar_len(4)), 1.0)
        self.assertEqual(C.L, 2 * bass.G.length)                     # the bass line, twice
        self.assertEqual(C.L, 2 * lead.G.length)                     # the arp phrase, twice
        self.assertEqual(C.L, Ph.loop_len(K, pad.LOOP_STEPS))        # the pad's loop region
        self.assertEqual(K.SWING, C.G.swing)

    def test_previews_are_whole_bars(self):
        for steps, bars in ((PV.FILL_STEPS, 2), (PV.RISER_STEPS, 9)):
            self.assertLess(abs(Ph.loop_len(K, steps) - bar_len(bars)), 1.0)
        G = Ph.Grid(K, PV.FILL_STEPS)
        self.assertEqual(G.length, G.at(16) + Ph.loop_len(K, fx.FILL_STEPS))       # the fill ends on the loop's end
        G = Ph.Grid(K, PV.RISER_STEPS)
        self.assertEqual(G.at(128), Ph.loop_len(K, fx.RISER_STEPS))                # the riser ends on bar 9's 1

    def test_the_fill_ends_on_the_downbeat(self):
        self.assertEqual(48, C.FILL_AT)
        self.assertEqual(C.L, C.G.at(C.FILL_AT) + Ph.loop_len(K, fx.FILL_STEPS))


class TestContextParts(unittest.TestCase):
    def test_four_on_the_floor_with_room_for_the_dive(self):
        steps = [s for s, _ in C.KICK]
        self.assertEqual([s for s in range(0, 64, 4) if s != 60], steps)
        self.assertEqual({1.0}, {v for _, v in C.KICK})               # an even pulse: every beat on the hard layer
        for kick in (PV.GROOVE_KICK, PV.groove_bar(0)[0]):
            self.assertEqual({1.0}, {v for _, v in kick})
        self.assertLess(C.LANES['clap'][1], C.LANES['kick'][1])        # the clap sits under the kick

    def test_the_bass_line_rests_on_every_kick(self):
        """Each voice of the line, tiled over the 4 bars, starts its sub's swell after the kick's own 80 ms and is
        released 44 ms before the next kick (the line's own rule, now against the loop's kicks)."""
        kicks = [C.G.at(s) for s, _ in C.KICK] + [C.L]
        lead_n, rel = int(bass.SUB_LEAD * SR), int(bass.SUB_RELEASE * SR)
        for half in (0, bass.G.length):
            for v in bass.voices(bass.LINES[bass.PICK]):
                on, off = v['on'] + half, v['off'] + half
                if C.G.at(C.GLIDE[0]) <= on < C.G.at(C.GLIDE[1]):
                    continue                                          # muted under the glide
                with self.subTest(on=on):
                    k = max(p for p in kicks if p <= on)
                    nxt = min(p for p in kicks if p > on)
                    self.assertGreaterEqual(on - lead_n - k, int(qa.KICK_OWN_S * SR))
                    self.assertLessEqual(off + rel, nxt - int(0.044 * SR))

    def test_the_glide_sits_between_two_kicks(self):
        a, b = C.GLIDE
        kicks = {s for s, _ in C.KICK}
        self.assertIn(a, kicks)
        self.assertIn(b, kicks)
        self.assertEqual(8, b - a)                                    # its two beats
        gone = C.G.at(b) - C.G.at(a) - int(C.GLIDE_EARLY * SR) + int(C.GLIDE_RELEASE * SR)
        self.assertGreaterEqual(C.G.at(b) - C.G.at(a) - gone, int(0.040 * SR))   # silent 40 ms before the kick
        for half in (0, bass.G.length):                               # the mute's edges fall in the line's silence
            for v in bass.voices(bass.LINES[bass.PICK]):
                on, off = v['on'] + half, v['off'] + half + int(bass.SUB_RELEASE * SR)
                for edge in (C.G.at(a), C.G.at(b)):
                    self.assertFalse(on - int(bass.SUB_LEAD * SR) < edge + int(C.MUTE_EDGE * SR) and
                                     off > edge - int(C.MUTE_EDGE * SR), (on, edge))

    def test_the_hats_choke_and_leave_the_fill_bar(self):
        closed = {s for s, _ in C.HAT_CLOSED}
        for s, _ in C.HAT_OPEN:
            self.assertEqual(2, s % 4)
            self.assertNotIn(s, closed)
            self.assertTrue(any(0 < c - s <= 2 for c in closed), s)          # choked within an 8th
        self.assertTrue(all(s < C.FILL_AT for s, _ in C.HAT_CLOSED + C.HAT_OPEN))
        self.assertTrue(all(s < C.FILL_AT for s, _ in C.CLAP))
        for bar in range(3):
            self.assertTrue({16 * bar + 4, 16 * bar + 12} <= {s for s, v in C.CLAP if v == 1.0})
        self.assertTrue(any(v <= 0.2 for _, v in C.CLAP))             # ghosts

    def test_the_vocals_call_and_answer(self):
        robot_end = C.ROBOT_AT + max(s + ln for s, ln in vox.ROBOT_GRID)
        self.assertEqual(0, C.ROBOT_AT % 16)                          # on a downbeat
        starts = [s for s, _, _ in C.CHOPS]
        self.assertTrue(all(s >= robot_end for s in starts))         # the sung line answers after the call
        self.assertEqual(sorted(starts), starts)
        names = {c[0] for c in vox.PICK_CHOPS}
        for _, name, throw in C.CHOPS:
            self.assertIn(name, names)
        self.assertEqual(1, sum(1 for *_, t in C.CHOPS if t))         # one throw

    def test_everything_pitched_is_in_d_minor(self):
        for n in C.RESONATOR_TONES:
            self.assertIn(n % 12, K.SCALE, D.note_name(n))
        notes = [m for _, m, *_ in bass.LINES[bass.PICK]] + [bass.DROP_ROOT, bass.DROP_FROM, perc.ZAP_NOTE]
        for n in notes:
            self.assertIn(n % 12, K.SCALE, D.note_name(n))

    def test_every_lane_is_mixed(self):
        self.assertEqual(set(C.LANES), {'kick', 'bass', 'clap', 'hat', 'zap', 'pad', 'arp', 'chops', 'robot', 'fill'})
        self.assertEqual({C.CLEAN, C.WARM}, set(C.BASS_FADER))
        for k, (_, fader, pan, *sends) in C.LANES.items():
            if k != 'bass':
                self.assertLessEqual(fader, 0.0, k)
            self.assertTrue(-1 <= pan <= 1)
            self.assertTrue(all(0 <= s <= 1 for s in sends))
        self.assertEqual((0.0, 0.0, 0.0), C.LANES['kick'][3:])       # no send on the low end
        self.assertEqual((0.0, 0.0, 0.0), C.LANES['bass'][3:])


class TestArpRides(unittest.TestCase):
    """The arp makes room for the words and rests in the fill bar; elsewhere it plays exactly as written."""

    def span(self, a, b):
        return slice(C.G.at(a) - int(C.ARP_PRE * D.SR), C.G.at(b) - int(C.ARP_RAMP * D.SR))

    def test_it_rests_in_the_fill_bar(self):
        m = C.rest_mask()
        self.assertEqual(48, C.ARP_REST[0])
        self.assertEqual(C.FILL_AT, C.ARP_REST[0])
        self.assertFalse(np.any(m[C.G.at(48):]))
        self.assertTrue(np.all(m[int(C.ARP_REST[2] * D.SR):C.G.at(48) - int(C.ARP_REST[1] * D.SR)] == 1.0))
        self.assertTrue(np.all(np.diff(m[C.G.at(48) - int(C.ARP_REST[1] * D.SR):C.G.at(48) + 1]) <= 0))

    def test_it_rides_under_every_vocal_phrase_and_nowhere_else(self):
        duck, carve = C.ride_db(C.ARP_DUCK), C.ride_db(C.ARP_CARVE)
        robot_end = C.ROBOT_AT + max(s + ln for s, ln in vox.ROBOT_GRID)
        phrases = [(C.ROBOT_AT, robot_end), (28, 32), (36, 48)]       # the robot, "pull me", "into orbit"
        for a, b in phrases:
            with self.subTest(phrase=(a, b)):
                self.assertLessEqual(float(np.max(carve[self.span(a, b)])), -6.0)
        self.assertLessEqual(float(np.max(duck[self.span(C.ROBOT_AT, 32)])), -4.0)
        quiet = np.ones(C.L, bool)                                     # outside the call and answer (the robot
        for a, b in ((C.ROBOT_AT, 32), (36, 48)):                      # through "pull me") and "into orbit"
            quiet[C.G.at(a) - int((C.ARP_PRE + C.ARP_RAMP) * D.SR):C.G.at(b)] = False
        self.assertEqual(0.0, float(np.max(np.abs(duck[quiet]))))
        self.assertEqual(0.0, float(np.max(np.abs(carve[quiet]))))
        self.assertTrue(np.all(carve <= 0) and np.all(duck <= 0))
        self.assertEqual(0.0, float(duck[C.G.at(32)]))                # back for bar 3's downbeat accent


class TestLowEndWithTheKick(unittest.TestCase):
    """Each bassline over the kit's own kick, as the renderer writes it (its high-pass and its one gain, before the
    24-bit rounding): the groove previews (kick, hats and line at the context loop's faders) and the context loop's low
    end pass every per-file check, and their low-end rule in every gap with a margin, A the gate and B the bass-body
    rule. Renders in a few seconds."""

    def check(self, name):
        it = kits.items(K, only=(name,))[0]
        done = R.render_item(it, K.KIT)
        t, out, x = done.takes[0], done.outs[0], done.audio[0]
        self.assertIn('with_kick', it.tags)
        self.assertTrue(out.hits)
        checks = {c.name: c for c in qa.check_take(x, t.spec(out))}
        failed = [(k, c.value) for k, c in checks.items() if c.status == 'fail']
        self.assertEqual([], failed, name)
        rows = qa.loop_mud(x, sorted(out.hits))
        return max(r['vs_sub_db'] for r in rows), max(r['before_hit_db'] for r in rows)

    def test_every_line_with_the_kick(self):
        for name in ('OrbitGrooveClean', 'OrbitGrooveWarm', 'RelayGrooveClean', 'RelayGrooveWarm', 'ContextLoopLow',
                     'ContextLoopLowWarm'):
            with self.subTest(name=name):
                worst, pre = self.check(name)
                warm = name.endswith('Warm')
                limit = qa.BODY_VS_SUB_DB - 1.0 if warm else qa.MUD_VS_SUB_DB - 2.0
                self.assertLessEqual(worst, limit, f'{name}: {worst:.2f} dB')
                self.assertLessEqual(pre, qa.LOOP_BEFORE_HIT_DB - 5.0, f'{name}: before the hit {pre:.1f} dB')

    def test_the_groove_is_the_loop_s_low_end(self):
        """A groove preview's kick and line are the context loop's kick and bass lanes of bars 1-2 (the loop mutes
        the line only under the glide, in bar 4)."""
        for char in (C.CLEAN, C.WARM):
            low, hits = PV.groove_low(bass.PICK, char)
            lanes = C.low_lanes(char)
            n = len(low)
            self.assertEqual(tuple(C.kick_onsets()[:8]), hits)
            np.testing.assert_allclose(D.st(low), (lanes['kick'] + lanes['bass'])[:n], atol=1e-12)


class TestContextItems(unittest.TestCase):
    def test_items(self):
        items = {it.name: it for it in C.ITEMS + PV.ITEMS}
        grooves = {f'{line}Groove{c}' for line in bass.LINES for c in ('Clean', 'Warm')}
        self.assertEqual({'ContextLoop', 'ContextLoopWarm', 'ContextLoopWet', 'ContextLoopLow', 'ContextLoopLowWarm',
                          'StarfieldPadLoop', 'LaserFillPreview', 'LaserRiserPreview'} | grooves, set(items))
        for it in items.values():
            self.assertTrue(it.loop, it.name)
            self.assertIn('r2', it.sets)
            self.assertEqual('loop', it.category)
            self.assertTrue(it.qa.get('why'), it.name)
        self.assertEqual('gate', items['ContextLoopLow'].qa['mud'])
        self.assertEqual('bass-body', items['ContextLoopLowWarm'].qa['mud'])
        for name in ('ContextLoopLow', 'ContextLoopLowWarm'):
            self.assertIn('with_kick', items[name].tags)
        self.assertIn('wet', items['ContextLoopWet'].tags)
        self.assertEqual(C.WARM, C.WET_BASS)
        for name in ('StarfieldPadLoop', 'LaserFillPreview', 'LaserRiserPreview') + tuple(sorted(grooves)):
            self.assertIn('preview', items[name].tags)
        for name in grooves:
            self.assertIn('with_kick', items[name].tags)
            self.assertEqual('bass-body' if name.endswith('Warm') else 'gate', items[name].qa['mud'])


if __name__ == '__main__':
    unittest.main()
