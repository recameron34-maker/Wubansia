"""FUT02 "Orbit Relay" context loop and listening previews (recipes/fut02/context.py, preview.py): what the arrangement
promises holds without rendering a note. The loop is exactly 4 bars on the kit's grid and tiles the 2-bar phrases and
the pad's loop; the kick is four on the floor, every beat on the hard layer, with one gap for the fill; the bass line
rests on every kick, the glide sits between two kicks and lifts before the second; the hats are choked; the fill bar
has no other hats, claps or arp; the arp makes room for the vocals and for every backbeat, and the clap snaps; the
vocals call and answer without overlapping; every pitch is in D minor; the items carry their low-end rules. Round 3's
buses do what the mix promises: the drum bus
shaves the backbeat to the kick's own peak and leaves a kick alone as it is, the pump and the kick's room are back
before the next 8th, the vocoder's chorus keeps the mono sum, the arp's filter ride opens over three bars and closes in
the fill bar, the club loop is mastered at the genre's settings. Two classes render: each bassline summed with the
kit's own kick (the groove previews and the loop's low end, no vocal models needed) passes its low-end rule in every
gap, and the two bass lanes read the same loudness."""
import unittest

import numpy as np

from tools.sound import dsp48 as D
from tools.sound import kits
from tools.sound import master as MS
from tools.sound import mixmeter as MM
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

    def test_it_makes_room_for_every_backbeat(self):
        """Over the 60-10 ms before every backbeat clap (the clap's jump reads against it), the arp's top is carved at
        least 4 dB, by a vocal ride or by one of bar 1's own backbeat rides; those are shallower than the vocal rides,
        in bar 1 only, and leave the attack of the accent on the 16th before alone (they ramp in 100 ms ahead)."""
        carve = C.ride_db(C.ARP_CARVE + C.ARP_BACKBEAT)
        for s, v in C.CLAP:
            if v < 0.8:
                continue
            with self.subTest(clap=s):
                o = C.G.at(s)
                self.assertLessEqual(float(np.max(carve[o - int(0.060 * D.SR):o - int(0.010 * D.SR)])), -3.9)
        for a, b, depth in C.ARP_BACKBEAT:
            self.assertTrue(0 <= a < 16 and b == a + 1, (a, b))
            self.assertIn((a, 1.0), C.CLAP)
            self.assertTrue(-6.0 <= depth < 0.0)
        own = C.ride_db(C.ARP_BACKBEAT)
        for s in (3, 11):                                              # the tresillo accents before 2 and 4
            self.assertEqual(0.0, float(own[C.G.at(s) + int(0.008 * D.SR)]))

    def test_the_clap_snaps(self):
        """The clap's snap: its 1-5 kHz band 3 dB up over the first 12 ms of every backbeat clap, back by 25 ms, and
        nowhere else; the ghost claps are left alone."""
        g = C.snap_gain()
        up = float(D.undb(C.CLAP_SNAP['db']))
        for s, v in C.CLAP:
            o = C.G.at(s)
            with self.subTest(clap=s):
                if v >= C.CLAP_SNAP['min_vel']:
                    self.assertAlmostEqual(up, g[o + int(0.006 * D.SR)], places=12)
                    self.assertEqual(1.0, g[o + int(0.026 * D.SR)])
                else:
                    self.assertEqual(1.0, float(np.max(g[o:o + int(0.03 * D.SR)])))
        self.assertTrue(np.all(g >= 1.0) and 1.0 < up <= D.undb(4.0))
        self.assertAlmostEqual(1.0, float(np.median(g)), places=12)
        self.assertLess(C.LANES['clap'][1], C.LANES['kick'][1])          # its fader still under the kick's


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
        if name.startswith('ContextLoopLow'):       # the club master's kicks, so the crest floor reads each one
            self.assertEqual(C.club_hits()['kick'], list(out.top_hits))
            self.assertEqual(15, len(out.top_hits))
            self.assertEqual('pass', checks['loop_kick_crest'].status)
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


class TestBuses(unittest.TestCase):
    """Round 3's mix on buses (context.py, 'the buses'): what each insert promises, on test signals."""

    def lanes_with(self, **given):
        """Every lane of the loop silent, but the ones given."""
        z = np.zeros((C.L, 2))
        return {k: given.get(k, z) for k in C.LANES}

    def kick_like(self, amp=0.5):
        """A 55 Hz burst on every kick of the loop: a kick lane's shape, peak about amp."""
        n = int(0.12 * D.SR)
        t = np.arange(n) / D.SR
        hit = amp * np.sin(2 * np.pi * 55.0 * t) * np.exp(-t / 0.05)
        buf = np.zeros((C.L, 2))
        for o in C.kick_onsets():
            Ph.place(buf, hit, o)
        return buf

    def test_the_drum_bus_shaves_the_backbeat_to_the_kick(self):
        kick = self.kick_like()
        clap = np.zeros((C.L, 2))
        burst = 0.4 * np.sin(2 * np.pi * 1500.0 * np.arange(int(0.02 * D.SR)) / D.SR)
        for s, _ in C.CLAP:
            Ph.place(clap, burst, C.G.at(s))
        mix = self.lanes_with(kick=kick, clap=clap)
        kick_tp = MM.true_peak_db(kick)
        self.assertAlmostEqual(kick_tp + C.DRUM_CLIP_DB, C.drum_ceiling_db(mix), places=9)
        self.assertGreater(MM.true_peak_db(kick + clap), kick_tp + 2.0)      # the backbeat sums over the kick...
        y = C.mixdown(mix)
        # ...and the bus shaves it to the ceiling (its true peak a few hundredths of a dB over it: the clipper's
        # oversampled output, downsampled, rings a little between samples)
        self.assertLessEqual(MM.true_peak_db(y), kick_tp + C.DRUM_CLIP_DB + 0.25)
        alone = C.mixdown(self.lanes_with(kick=kick))
        self.assertLess(abs(MM.true_peak_db(alone) - kick_tp), 0.4)        # a kick alone passes about as it is
        self.assertTrue(-1.0 <= C.DRUM_CLIP_DB <= 1.0)
        self.assertEqual(('kick', 'clap', 'hat'), C.DRUM_BUS)

    def test_the_pump_breathes_and_is_back_by_the_next_8th(self):
        eighth = 2 * K.STEP
        for kw, depth in ((C.PUMP, (2.0, 4.0)), (C.KICK_ROOM, (1.0, 4.0))):
            with self.subTest(kw=kw):
                self.assertTrue(depth[0] <= kw['depth_db'] <= depth[1])
                self.assertLessEqual(kw['ramp'] + kw['hold'] + kw['release'], eighth)
                g = Ph.pump_db(C.kick_onsets(), C.L, **kw)
                o = C.kick_onsets()[1]
                self.assertAlmostEqual(-kw['depth_db'], g[o + int(kw['ramp'] * D.SR)], places=2)
                self.assertEqual(0.0, g[o + int(eighth * D.SR)])
        self.assertLessEqual(C.KICK_ROOM['ramp'] + C.KICK_ROOM['hold'] + C.KICK_ROOM['release'], 0.12)
        self.assertFalse(set(C.PUMPED) & set(C.ROOMED))
        self.assertTrue(set(C.PUMPED) | set(C.ROOMED) <= set(C.LANES) - set(C.DRUM_BUS) - {'bass'})
        self.assertEqual({'pad', 'arp'}, set(C.PUMPED))                    # and the returns

    def test_the_music_bus_trims_the_bulge(self):
        cut = C.MUSIC_EQ[0]
        self.assertTrue(400.0 <= cut[0] <= 1000.0 and cut[1] < 0)
        self.assertTrue(all(abs(g) <= 4.0 for _, g, _ in C.MUSIC_EQ))
        tone = lambda f: np.stack([np.sin(2 * np.pi * f * np.arange(C.L) / D.SR)] * 2, 1)
        for f, want in ((cut[0], cut[1]), (60.0, 0.0)):
            y = C.music_eq(tone(f))
            got = 10 * np.log10(np.mean(y ** 2) / np.mean(tone(f) ** 2))
            self.assertLess(abs(got - want), 0.6, f)

    def test_the_vocoder_chorus_keeps_the_mono_sum(self):
        r = D.rng('test', 'chorus')
        x = np.stack([r.standard_normal(C.L)] * 2, 1) * 0.1
        y = C.chorus(x, **C.CHORUS)
        np.testing.assert_allclose(D.mono(y), D.mono(x), atol=1e-12)
        self.assertGreater(MM.side_mid_db(y, 1000, 20000), -10.0)          # wide above 1 kHz
        self.assertLess(MM.side_mid_db(y, 0, 500), -20.0)                  # not under it
        self.assertEqual(int(C.CHORUS['cycles']), C.CHORUS['cycles'])      # the sway joins at the seam

    def test_the_zaps_bounce_across_the_field(self):
        self.assertEqual(len(C.ZAP), len(C.ZAP_PAN))
        self.assertTrue(all(-1 <= p <= 1 for p in C.ZAP_PAN))
        self.assertTrue(all(a * b < 0 for a, b in zip(C.ZAP_PAN, C.ZAP_PAN[1:])))
        self.assertEqual(0.0, C.LANES['zap'][2])

    def test_the_arp_ride_opens_over_three_bars_and_closes_in_the_fill_bar(self):
        f = C.ride_hz()
        top = C.G.at(C.ARP_RIDE['top_step'])
        self.assertAlmostEqual(C.ARP_RIDE['lo_hz'], f[0], places=6)
        self.assertAlmostEqual(C.ARP_RIDE['hi_hz'], f[top], delta=1.0)
        self.assertTrue(np.all(np.diff(f[:top]) >= 0))
        self.assertTrue(np.all(np.diff(f[top:]) <= 0))
        self.assertLess(f[-1], 1.01 * C.ARP_RIDE['lo_hz'])
        self.assertLessEqual(C.ARP_RIDE['top_step'], C.ARP_REST[0])        # open before the arp rests
        top_melody = max(D.midi_hz(m) for _, m, _ in lead.MELODY)
        self.assertGreater(C.ARP_RIDE['lo_hz'], top_melody)                # bar 1 is dark, its melody whole
        self.assertGreater(C.ARP_RIDE['hi_hz'], 12000.0)

    def test_the_vocal_bus_and_the_sends(self):
        self.assertTrue(3.0 <= C.VOX_COMP['gr_db'] <= 8.0 and C.VOX_COMP['attack_s'] <= 0.010)
        self.assertEqual(set(C.VOCALS), set(C.VOX_MAKEUP_DB))
        self.assertTrue(all(v > 0 for v in C.VOX_MAKEUP_DB.values()))
        self.assertTrue(3.0 <= C.SPACE['t60'] <= 4.0)                      # the standard's 3-4 s hall
        self.assertAlmostEqual(K.STEP / 2, C.SPACE['predelay'], places=12)  # a 32nd note: 57.7 ms
        self.assertGreater(C.SPACE['damp_hz'], C.S.SPACE['damp_hz'])        # brighter than the shared hall
        self.assertGreaterEqual(C.ECHO['hp_hz'], 600.0)                    # the echo out of the 400-800 Hz bulge
        self.assertGreater(C.ECHO['lp_hz'], C.ECHO['hp_hz'])
        self.assertLessEqual(C.LANES['arp'][3], 0.10)                      # the arp mostly dry in the hall
        self.assertGreater(C.LANES['arp'][4], 0.0)                         # ...and on the ping-pong

    def test_the_club_master_settings(self):
        """The genre's settings, with a cap a quarter dB over the standard's 4 dB (the A&R review asked for 4.5 at
        most: 6 dB flattened the kick and clap); the item's text states the cap it renders with."""
        st = MS.settings(K, max_reduction_db=C.MASTER_REDUCTION_DB)
        self.assertEqual(K.DIRECTION, st.genre)
        self.assertEqual(qa.CLUB[K.DIRECTION]['lufs'][0], st.target_lufs)
        self.assertEqual(st.target_lufs, C.CLUB_TARGET_LUFS)
        self.assertTrue(4.0 <= C.MASTER_REDUCTION_DB <= 4.5)
        about = {i.name: i for i in C.ITEMS}['ContextLoopClub'].about
        self.assertIn(f'at most {C.MASTER_REDUCTION_DB:g} dB of peak reduction', about)
        self.assertIn(f'{C.CLUB_TARGET_LUFS:g} LUFS-I', about)


class TestBassLevels(unittest.TestCase):
    """The two bass lanes read the same loudness in the loop (the A/B is the character alone). Renders the lines."""

    def test_a_and_b_are_level_matched(self):
        a, b = (qa.lufs(C.low_lanes(ch)['bass']) for ch in (C.CLEAN, C.WARM))
        self.assertLess(abs(a - b), 0.1, (a, b))

    def test_the_a_b_grooves_play_at_one_loudness(self):
        """The listening page's bass A/B: each line's groove preview, A and B, as the renderer writes them, within
        0.2 LU of each other with neither true peak over -1.02 dBTP (each file at its own peak, Orbit's B read 0.95
        LU under its A). Renders the four grooves (a few seconds)."""
        got = {}
        for name in ('OrbitGrooveClean', 'OrbitGrooveWarm', 'RelayGrooveClean', 'RelayGrooveWarm'):
            x = R.render_item(kits.items(K, only=(name,))[0], K.KIT).audio[0]
            got[name] = qa.lufs(x)
            self.assertLessEqual(D.true_peak_db(x), D.TP_TARGET + 1e-6, name)
        for line in ('Orbit', 'Relay'):
            a, b = got[f'{line}GrooveClean'], got[f'{line}GrooveWarm']
            self.assertLess(abs(a - b), 0.2, (line, a, b))


class TestContextItems(unittest.TestCase):
    def test_items(self):
        items = {it.name: it for it in C.ITEMS + PV.ITEMS}
        grooves = {f'{line}Groove{c}' for line in bass.LINES for c in ('Clean', 'Warm')}
        self.assertEqual({'ContextLoopClub', 'ContextLoop', 'ContextLoopWarm', 'ContextLoopWet', 'ContextLoopLow',
                          'ContextLoopLowWarm', 'StarfieldPadLoop', 'LaserFillPreview', 'LaserRiserPreview'} | grooves,
                         set(items))
        for it in items.values():
            self.assertTrue(it.loop, it.name)
            self.assertIn('r3', it.sets)
            if it.name != 'ContextLoopClub':
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

    def test_the_club_loop_is_a_mastered_loop(self):
        it = {i.name: i for i in C.ITEMS}['ContextLoopClub']
        self.assertEqual(('r3',), it.sets)
        self.assertEqual({'wet', 'club'}, set(it.tags))
        self.assertIsNone(it.hp_hz)
        self.assertIsNone(it.level)
        self.assertEqual('off', it.qa['mud'])
        self.assertIs(C.context_club, it.render)
        self.assertEqual(C.ITEMS[0], it)                                   # first: the listening page starts there


if __name__ == '__main__':
    unittest.main()
