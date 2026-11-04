"""FUT04 "Furnace Code": the low end as the context loop sums it (recipes/fut04/bass.py, context.py), rendered from
the kit's files as written. What the per-file QA can't see:

- the bass's sub pedal is in phase with the kick's tail, so on every note of the loop kick and bass add up (30-120 Hz
  never more than 3 dB under their power sum: no beating, no cancelling);
- the rumble adds weight under its kick instead of taking it away (1 dB or more per beat in 30-120 Hz);
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


class TestLowEnd(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.kick, cls.rumble = C.mixed('kick'), C.mixed('rumble')
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

    def test_the_rumble_adds_weight_under_its_kick(self):
        """The preview's 2 bars: every kick full, the rumble at the loop's faders; per beat, 30-120 Hz at least 1 dB over
        the kick lane alone."""
        G = Ph.Grid(K, B.LOOP_STEPS)
        kick = Ph.hits_lane(K, 'KickSplit', tuple((s, 1.0) for s in range(0, B.LOOP_STEPS, 4)), G)
        kick = kick * D.undb(C.fader_db('kick'))
        rumble = D.st(C.kit_file('Rumble').audio) * D.undb(C.fader_db('rumble'))
        bk, bs = band(kick), band(kick + rumble)
        for s in range(0, B.LOOP_STEPS, 4):
            a, e = G.at(s), (G.at(s + 4) if s + 4 < B.LOOP_STEPS else G.length)
            with self.subTest(beat=s // 4):
                self.assertGreater(10 * np.log10(np.sum(bs[a:e] ** 2) / np.sum(bk[a:e] ** 2)), 1.0)

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

    def test_the_low_end_loops_need_no_vocal_models(self):
        """ContextLoopLow, ContextLoopLowWarm and the previews' lanes ask for no vocal file."""
        asked = []
        real = Ph.kit_files

        def spy(kit, name):
            asked.append(name)
            return real(kit, name)
        for f in (C.raw_lane, C.chop_parts, C.fader_db, C.lanes, C.returns):
            f.cache_clear()
        Ph.kit_files = spy
        try:
            C.low(C.CLEAN), C.low(C.WARM)
            for lane in ('kick', 'snare', 'hat', 'rumble', 'fill', 'bass'):
                C.fader_db(lane)
        finally:
            Ph.kit_files = real
            for f in (C.raw_lane, C.chop_parts, C.fader_db, C.lanes, C.returns):
                f.cache_clear()
        vocal = {it.name for it in X.ITEMS}
        self.assertTrue(asked)
        self.assertEqual([], [n for n in asked if n in vocal])


if __name__ == '__main__':
    unittest.main()
