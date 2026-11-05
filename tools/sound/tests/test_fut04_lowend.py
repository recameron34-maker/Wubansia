"""FUT04 "Furnace Code": the low end as the context loop sums it (recipes/fut04/bass.py, context.py), rendered from
the kit's files as written, the kick out of the drum bus as in the low-end loops (round 3). What the per-file QA can't
see:

- the bass's sub pedal is in phase with the kick's tail, so on every note of the loop kick and bass add up (30-120 Hz
  never more than 3 dB under their power sum: no beating, no cancelling);
- the kick leads its own sub (the fix pass, after the A&R review: the rumble and the bass's pedal had stacked in phase
  on the kick's tail, so the low end peaked 140-200 ms after each kick, 3.6 dB over the kick's own peak);
- the rumble carries the gap after the kick's tail, adds weight on every beat and takes little away in any window
  (it sits in quadrature with the tail, whose phase it drifts against);
- the warm low end passes the bass-body rule with room to spare, with the bass up to 5 ms early or late, the kick 1 dB
  up or down and the bass's phase turned (a first-order all-pass), and so does the clean one under the gate;
- the low-end loops render without the vocal models (each lane renders its own files only).

It needs no vocal models and takes about 20 s.
"""
import unittest

import tools.sound  # noqa: F401  (pins numpy's kernels before numpy loads)
import numpy as np
from scipy import signal

from tools.sound import dsp48 as D
from tools.sound import phrase as Ph
from tools.sound import qa
from tools.sound import render as R
from tools.sound.kits import fut04 as K
from tools.sound.recipes.fut04 import bass as B
from tools.sound.recipes.fut04 import context as C
from tools.sound.recipes.fut04 import vox as X

SR = D.SR


def band(x, lo=30, hi=120):
    m = D.mono(x)
    return qa.band_zp(np.tile(m, 3), lo, hi)[len(m):2 * len(m)]


def worst_gap(*lanes):
    """The low-end loop as the renderer writes it (its circular 20 Hz high-pass): the worst gap's window reading."""
    rows = qa.loop_mud(R.subsonic(sum(lanes), 20.0, True), [C.G.at(s) for s, _ in C.KICK])
    return max(r['vs_sub_db'] for r in rows), max(r['before_hit_db'] for r in rows)


def allpass(x, fc=60.0):
    c = (np.tan(np.pi * fc / SR) - 1) / (np.tan(np.pi * fc / SR) + 1)
    return Ph.circular(lambda u: signal.lfilter([c, 1.0], [1.0, c], u, axis=0), x)


class TestAcidAB(unittest.TestCase):
    def test_each_a_b_line_plays_at_one_loudness(self):
        """The listening page's acid A/B (the ride, line 1, line 2), as the renderer writes them: A and B within 0.2 LU
        of each other, neither true peak over -1.02 dBTP. Each file at its own peak, the ride's A read 0.96 LU over
        its B and line 1's 1.18. Renders the six loops (a few seconds)."""
        from tools.sound import kits
        for line in ('AcidLine1Ride', 'AcidLine1', 'AcidLine2'):
            got = []
            for char in ('Clean', 'Warm'):
                x = R.render_item(kits.items(K, only=(f'{line}{char}',))[0], K.KIT).audio[0]
                self.assertLessEqual(D.true_peak_db(x), D.TP_TARGET + 1e-6, (line, char))
                got.append(qa.lufs(x))
            self.assertLess(abs(got[0] - got[1]), 0.2, (line, got))


class TestLowEnd(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.kick = C.drum_bus({'kick': C.mixed('kick')}, ('kick',))   # as the low-end loops play it (context.low)
        cls.rumble = C.mixed('rumble')
        cls.bass = {char: C.mixed('bass', char) for char in (C.CLEAN, C.WARM)}

    def test_the_pedal_starts_on_the_kick_tail_phase(self):
        """PEDAL_PHASE within 30 degrees of the top kick layer's E1 tail, on both round robins, as written."""
        item, got, _ = Ph.kit_files(K, 'KickSplit')
        top = len(item.layers)
        for rr in range(1, item.rr + 1):
            m = D.mono(got[(None, top, rr)].audio)
            t = np.arange(len(m)) / SR
            s = (t >= 0.12) & (t < 0.30)
            phase = np.degrees(np.angle(1j * np.sum(m[s] * np.exp(-2j * np.pi * B.F_KICK * t[s]))))
            with self.subTest(rr=rr):
                self.assertLess(abs((phase - B.PEDAL_PHASE + 180) % 360 - 180), 30.0, phase)

    def test_kick_and_bass_never_cancel(self):
        """On every note of the picked line over the 4 bars, the 30-120 Hz Hilbert envelope of kick + bass stays within
        3 dB under the power sum of the two (and no more than coherent summation over it)."""
        spans = [(on + h + int(0.010 * SR), end + h) for h in (0, B.L)
                 for on, end, *_ in B.note_spans(B.LINE_BY_NAME[C.LINE])]
        k = band(self.kick)
        ek = np.abs(signal.hilbert(k))
        for char, bass in self.bass.items():
            b = band(bass)
            eb, es = np.abs(signal.hilbert(b)), np.abs(signal.hilbert(k + b))
            p = np.sqrt(ek ** 2 + eb ** 2)
            for a, e in spans:
                d = 20 * np.log10((es[a:e] + 1e-12) / (p[a:e] + 1e-12))
                with self.subTest(char=char, at=round(a / SR, 3)):
                    self.assertGreater(d.min(), -3.0)
                    self.assertLess(d.max(), 3.2)

    def test_the_rumble_carries_the_gap(self):
        """The preview's 2 bars: every kick full, the rumble at the loop's faders. Per beat in 30-120 Hz, kick and
        rumble at least 0.3 dB over the kick lane alone (it never takes weight away over a beat), at least 3 dB over it
        200-375 ms after each kick (it carries the gap the kick's tail leaves), and no 40 ms window more than 2 dB under
        it. Round 3 first turned the rumble in phase with the tail (+3.2 dB per beat, a swell over the kick); in
        quadrature the two drift against each other, and the slower swell keeps their crossing short."""
        G = Ph.Grid(K, B.LOOP_STEPS)
        kick = Ph.hits_lane(K, 'KickSplit', tuple((s, 1.0) for s in range(0, B.LOOP_STEPS, 4)), G)
        kick = kick * D.undb(C.fader_db('kick'))
        rumble = D.st(C.kit_file('Rumble').audio) * D.undb(C.fader_db('rumble'))
        bk, bs = band(kick), band(kick + rumble)
        db = lambda a, b: 10 * np.log10(np.sum(bs[a:b] ** 2) / np.sum(bk[a:b] ** 2))  # noqa: E731
        w, hop = int(0.040 * SR), int(0.005 * SR)
        for s in range(0, B.LOOP_STEPS, 4):
            a, e = G.at(s), (G.at(s + 4) if s + 4 < B.LOOP_STEPS else G.length)
            with self.subTest(beat=s // 4):
                self.assertGreater(db(a, e), 0.3)
                self.assertGreater(db(a + int(0.200 * SR), a + int(0.375 * SR)), 3.0)
                self.assertGreater(min(db(i, i + w) for i in range(a, e - w, hop)), -2.0)

    def test_the_kick_leads_its_own_sub(self):
        """The fix pass (the A&R review): in both low-end loops as the renderer writes them (its circular 20 Hz
        subsonic high-pass), 35-60 Hz and 30-120 Hz in 20 ms slices from each kick, median over the kicks: the loudest
        slice lies in the kick's own 40-100 ms, at least 0.5 dB (35-60 Hz) and 1.5 dB (30-120 Hz) over every slice from
        120 ms to the next kick. Round 3 first read 3.5 dB the other way, its loudest slice at 140-180 ms."""
        kicks = [C.G.at(s) for s, _ in C.KICK]
        w = int(0.020 * SR)
        for char, bass in self.bass.items():
            y = D.mono(R.subsonic(self.kick + bass + self.rumble, 20.0, True))
            for lo, hi in ((35, 60), (30, 120)):
                m = qa.band_zp(np.tile(y, 3), lo, hi)[len(y):2 * len(y)] ** 2
                rows = [[10 * np.log10(np.mean(m[(np.arange(w) + k + j * w) % len(y)]) + 1e-30) for j in range(20)]
                        for k in kicks]
                prof = np.median(np.array(rows), 0)
                with self.subTest(char=char, band=(lo, hi)):
                    self.assertGreaterEqual(int(np.argmax(prof)), 2)
                    self.assertLess(int(np.argmax(prof)), 5)
                    self.assertGreaterEqual(prof[2:5].max() - prof[6:].max(), 0.5 if lo == 35 else 1.5)

    def test_the_low_end_holds_when_the_bass_moves(self):
        """ContextLoopLowWarm under bass-body (-2 dB) and ContextLoopLow under the gate (-20 dB), as built and with the
        bass 1-5 ms early or late, the kick fader 1 dB up or down, or the bass through a 60 Hz all-pass; the before-hit
        rule (-30 dB) every time."""
        limits = {C.CLEAN: qa.MUD_VS_SUB_DB, C.WARM: qa.BODY_VS_SUB_DB}
        for char, bass in self.bass.items():
            cases = {'as built': (self.kick, bass), 'kick -1 dB': (self.kick * D.undb(-1.0), bass),
                     'kick +1 dB': (self.kick * D.undb(1.0), bass), 'bass all-passed': (self.kick, allpass(bass))}
            for ms in (-5, -3, -1, 1, 3, 5):
                cases[f'bass {ms:+d} ms'] = (self.kick, np.roll(bass, int(round(ms * SR / 1000)), axis=0))
            for name, (k, b) in cases.items():
                vs_sub, before = worst_gap(k, b, self.rumble)
                with self.subTest(char=char, case=name):
                    self.assertLessEqual(vs_sub, limits[char] - 2.0)    # 2 dB of room, not a lucky phase
                    self.assertLessEqual(before, qa.LOOP_BEFORE_HIT_DB)

    def test_the_low_end_loops_are_the_loops(self):
        """ContextLoopLow and ContextLoopLowWarm are this sum: the kick out of the drum bus, the bass and the rumble as
        the full loops mix them (without the bass's dip under the vocals). Each declares its full-velocity kicks, the
        club master's (Out.top_hits): without them QA's crest floor per kick (loop_kick_crest) would read n/a."""
        for char, bass in self.bass.items():
            with self.subTest(char=char):
                out = C.low(char)
                self.assertTrue(np.allclose(out.audio, self.kick + bass + self.rumble, atol=1e-12))
                self.assertEqual(C.club_hits()['kick'], list(out.top_hits))
                self.assertEqual(15, len(out.top_hits))

    def test_the_low_end_loops_need_no_vocal_models(self):
        """ContextLoopLow, ContextLoopLowWarm and the previews' lanes ask for no vocal file."""
        asked = []
        real = Ph.kit_files

        def spy(kit, name):
            asked.append(name)
            return real(kit, name)
        caches = (C.raw_lane, C.chop_parts, C.chop_parts_bus, C.fader_db, C.lanes, C.returns)
        for f in caches:
            f.cache_clear()
        Ph.kit_files = spy
        try:
            C.low(C.CLEAN), C.low(C.WARM)
            for lane in ('kick', 'snare', 'hat', 'ride', 'rumble', 'fill', 'bass'):
                C.fader_db(lane)
        finally:
            Ph.kit_files = real
            for f in caches:
                f.cache_clear()
        vocal = {it.name for it in X.ITEMS}
        self.assertTrue(asked)
        self.assertEqual([], [n for n in asked if n in vocal])


if __name__ == '__main__':
    unittest.main()
