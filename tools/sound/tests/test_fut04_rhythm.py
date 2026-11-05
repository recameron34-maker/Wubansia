"""FUT04's rhythm and atmosphere recipes (recipes/fut04: drums, perc, pad, fx): what QA can't see. The kick sits in
Split's crest range, not Tight's (round 1), and (round 3) carries its weight where small speakers play, with its sub
20 dB down within about 3 16ths; the snare's first hand is within 2 dB of its loudest; the ride is driven and wide,
and (the fix pass) a noise wash that fades before the next off-beat, not a chord that rings over the 16ths; the
fill and the riser are exactly one bar and eight bars at the kit's tempo and play only the key's notes; the drone's
chord is Em(add9) and its low mids sit in the middle; the hat loop chokes its open hats, rides its off-beats at the
context loop's balance and keeps its 16ths apart."""
import unittest

import tools.sound  # noqa: F401  (pins numpy's kernels before numpy loads)
import numpy as np

from scipy import signal

from tools.sound import dsp48 as D
from tools.sound import kits, qa
from tools.sound import mixmeter as MM
from tools.sound import render as R
from tools.sound.recipes.fut04 import drums, fx, pad

SPLIT_RANGE = (5.5, 6.7)     # dB: FUT01's KickSplit top layer reads 5.82-6.44, KickTight 7.93-8.87 (ADR 0011)
SR = D.SR


def band(m, lo, hi):
    return D.bfilt(m, (lo, hi), 'bandpass', order=4, zero_phase=True)


def env_db(m, win):
    w = max(1, int(win * SR))
    c = np.concatenate([[0.0], np.cumsum(np.concatenate([np.zeros(w // 2), m ** 2, np.zeros(w - w // 2)]))])
    return 10 * np.log10((c[w:] - c[:-w])[:len(m)] / w + 1e-30)


def onset(m):
    e = env_db(m, 0.001)
    return int(np.nonzero(e >= e.max() - 40.0)[0][0])


def line_share(m, t0, t1, lo=1000.0, hi=12000.0, lines=12, width=15.0):
    """The share of m's lo-hi energy from t0 to t1 s (Hann window) within width Hz of its `lines` strongest spectral
    peaks (at least 60 Hz apart): band noise reads about 6%, a chord of squares 80% or more."""
    seg = m[int(t0 * SR):int(t1 * SR)]
    seg = seg * np.hanning(len(seg))
    f = np.fft.rfftfreq(len(seg), 1 / SR)
    p = np.abs(np.fft.rfft(seg)) ** 2
    k = (f >= lo) & (f < hi)
    fk, pk = f[k], p[k]
    peaks, _ = signal.find_peaks(pk, distance=max(1, int(60 / (SR / len(seg)))))
    mask = np.zeros(len(pk), bool)
    for i in peaks[np.argsort(pk[peaks])[-lines:]]:
        mask |= np.abs(fk - fk[i]) <= width
    return float(pk[mask].sum() / pk.sum())


def contrast_16ths(x, steps, k, band_hz=(6000.0, 16000.0), w_s=0.025):
    """The median over a loop's 16ths of its band's power in each 16th's first w_s against its last w_s (dB): the top
    end's articulation. k: the kit module (its grid and swing)."""
    from tools.sound import phrase as Ph
    G = Ph.Grid(k, steps, len(x))
    n = len(x)
    y = D.bfilt(np.tile(D.mono(x), 3), band_hz, 'bandpass', order=4, zero_phase=True)[n:2 * n] ** 2
    w = int(w_s * SR)
    out = []
    for st in range(steps):
        a0 = G.at(st)
        a1 = G.at(st + 1) if st + 1 < steps else n
        first = y[(np.arange(w) + a0) % n].mean()
        last = y[(np.arange(a1 - a0 - w, a1 - a0) + a0) % n].mean()
        out.append(10 * np.log10(first / last))
    return float(np.median(out))


class TestFut04Rhythm(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.k = kits.load('FUT04')

    def item(self, name):
        return kits.items(self.k, only=(name,))[0]

    def test_the_kick_hits_like_split_not_tight(self):
        it = self.item('KickSplit')
        top = [t for t in it.takes(self.k.KIT) if t.layer == len(it.layers)]
        for t in top:
            c = qa.kick_crest_db(R.finish_take(it, R.as_out(it.render(t))))
            self.assertGreaterEqual(c, SPLIT_RANGE[0], t.file)
            self.assertLessEqual(c, SPLIT_RANGE[1], t.file)

    def top_files(self, name):
        it = self.item(name)
        done = R.render_item(it, self.k.KIT)
        return [D.mono(x) for t, x in zip(done.takes, done.audio) if t.layer == len(it.layers)]

    def test_the_kick_has_weight_where_small_speakers_play(self):
        """Round 3: over the kick's first 50 ms, 80-120 Hz within 3 dB of 30-80 Hz (round 2 read 6 dB under: an E1 kick
        puts its weight under 44 Hz), its sub 20 dB down by about 3 16ths (2.6-3.1; the standard's techno kick), and
        the 150-400 Hz knock still gone 30 dB within 120 ms (the gate's fall)."""
        step = self.k.STEP
        for m in self.top_files('KickSplit'):
            o = onset(m)
            first = slice(o, o + int(0.050 * SR))
            knock = 10 * np.log10(np.sum(band(m, 80, 120)[first] ** 2) / np.sum(band(m, 30, 80)[first] ** 2))
            self.assertGreater(knock, -3.0)
            e = env_db(band(m, 30, 120), 0.005)
            pk = int(np.argmax(e))
            above = np.nonzero(e[pk:] >= e[pk] - 20)[0]
            sub20 = (pk + above[-1] + 1 - o) / SR
            self.assertTrue(2.6 * step <= sub20 <= 3.1 * step, sub20)
            self.assertLessEqual(qa.mud(m[o:])['fall_150_400_ms'], 120.0)

    def test_the_snares_first_hand_is_within_2_db(self):
        """Round 3: the clap's hands in 0.8-6 kHz (2 ms RMS peaks in the first 50 ms, the standard's measure): at least
        four within 30 ms, the first within 2 dB of the loudest (round 2: -4.5 dB), the tail 20 dB down by 120-250
        ms."""
        for m in self.top_files('SnareGated'):
            o = onset(m)
            e = env_db(band(m, 800, 6000), 0.002)[o:o + int(0.050 * SR)]
            pk, _ = signal.find_peaks(e, prominence=3.0, distance=int(0.003 * SR))
            hands = [(p / SR, e[p] - e.max()) for p in pk if e[p] > e.max() - 20]
            self.assertGreaterEqual(sum(t <= 0.030 for t, _ in hands), 4)
            self.assertGreaterEqual(hands[0][1], -2.0)
            ee = env_db(m, 0.005)
            top = int(np.argmax(ee))
            above = np.nonzero(ee[top:] >= ee[top] - 20)[0]
            self.assertTrue(0.120 <= (top + above[-1] + 1 - o) / SR <= 0.250)

    def test_the_ride_is_bright_and_wide(self):
        """Round 3: the ride's top layer is the widest thing in the kit's top (side over mid above 4 kHz within -9 to -3
        dB, the standard's 8-16 kHz window), its centroid above 8 kHz, and its file lasts 1-3 beats."""
        it = self.item('Ride')
        done = R.render_item(it, self.k.KIT)
        for t, x in zip(done.takes, done.audio):
            if t.layer != len(it.layers):
                continue
            self.assertTrue(-9.0 <= MM.side_mid_db(x, 4000, 20000, loop=False) <= -3.0)
            self.assertGreater(qa.centroid_hz(x), 8000.0)
            self.assertTrue(1.0 * self.k.BEAT <= len(x) / SR <= 3.0 * self.k.BEAT)

    def test_the_ride_is_a_wash_not_a_chord(self):
        """The fix pass (the A&R review): round 3's first ride sustained seven squares on the anvil's ratios, a bell
        chord: from 0.2 to 0.7 s 82% of its 1-12 kHz energy lay within 15 Hz of 12 lines, and it was 20 dB down only at
        1.02 s, so on the off-beats (441 ms apart) two to three hits rang over the 16ths. Now the top layer's sustain,
        0.1-0.4 s, keeps at most 25% in 12 lines (band noise reads about 6%) and is 20 dB down within 0.45 s of its
        onset; the bell is the ping of its first 80 ms."""
        it = self.item('Ride')
        done = R.render_item(it, self.k.KIT)
        for t, x in zip(done.takes, done.audio):
            if t.layer != len(it.layers):
                continue
            m = D.mono(x)
            with self.subTest(file=t.file):
                self.assertLess(line_share(m, 0.1, 0.4), 0.25)
                o = onset(m)
                e = env_db(m, 0.005)
                top = int(np.argmax(e))
                above = np.nonzero(e[top:] >= e[top] - 20)[0]
                self.assertLess((top + above[-1] + 1 - o) / SR, 0.45)
        self.assertLessEqual(sum(drums.RIDE_PING_END), 0.080)

    def test_the_hat_loop_keeps_its_16ths(self):
        """The fix pass: the hat loop with its ride reads 15 dB or more between each 16th's first and last 25 ms in
        6-16 kHz (median over its 32 16ths; round 2, without a ride, 39.1; round 3's first ride 7.6), and its ride sits
        at the context loop's balance (HAT_LOOP_RIDE_DB: the ride fader less the hat fader, within 0.05 dB), which puts
        the ride 3 LU under the hats lane."""
        from tools.sound.recipes.fut04 import context as C
        it = self.item('HatLoop')
        x = R.finish_take(it, R.as_out(it.render(it.takes(self.k.KIT)[0])))
        self.assertGreaterEqual(contrast_16ths(x, drums.HAT_STEPS, self.k), 15.0)
        self.assertLess(abs(drums.HAT_LOOP_RIDE_DB - (C.fader_db('ride') - C.fader_db('hat'))), 0.05)
        self.assertGreaterEqual(C.LANE_LUFS['hat'] - C.LANE_LUFS['ride'], 3.0)

    def test_the_kick_ends_before_the_next_beat(self):
        it = self.item('KickSplit')
        y = D.mono(R.as_out(it.render(it.takes(self.k.KIT)[-1])).audio)
        self.assertLess(drums.SUB_END[0] + drums.SUB_END[1], self.k.BEAT)
        self.assertLess(D.db(np.abs(y[int(self.k.BEAT * D.SR):]).max() / np.abs(y).max()), -100.0)

    def test_the_fill_and_the_riser_are_whole_bars(self):
        bar = 4 * self.k.BEAT * D.SR
        self.assertEqual(fx.swung(16), int(np.floor(bar + 0.5)))
        self.assertEqual(fx.RISER_N, int(np.floor(8 * bar + 0.5)))
        self.assertEqual(fx.swung(1), int(np.floor(2 * self.k.SWING * self.k.STEP * D.SR + 0.5)))

    def test_the_fill_and_the_riser_play_the_key(self):
        notes = {m for *_, ns, _ in fx.FILL for m in ns} | set(fx.RISER_SCALE)
        self.assertTrue({m % 12 for m in notes} <= set(self.k.SCALE), sorted(notes))
        self.assertEqual(fx.FILL[-1][3][-1] % 12, self.k.SCALE[0])    # it lands on E
        self.assertEqual(fx.RISER_SCALE[-1] % 12, self.k.SCALE[0])
        self.assertEqual(len(fx.RISER_SCALE), fx.RISER_BARS)

    def test_the_fill_slices_halve_every_beat(self):
        starts = [s0 for s0, *_ in fx.FILL]
        lengths = [length for _, length, *_ in fx.FILL]
        self.assertEqual(starts, [0.0, 4.0, 8.0, 12.0])
        self.assertEqual(lengths, [2.0, 1.0, 0.5, 0.25])
        for s0, length, count, notes, _ in fx.FILL:
            self.assertEqual(count * length, 4.0)
            self.assertIn(len(notes), (1, count))

    def test_the_drone_is_e_minor_add9(self):
        self.assertEqual(pad.CHORD[0] % 12, self.k.ROOT % 12)
        e, fs, g, b = (self.k.SCALE[i] for i in (0, 1, 2, 4))       # root, 9, minor third, fifth
        self.assertEqual({m % 12 for m in pad.CHORD}, {e, fs, g, b})
        self.assertGreater(float(D.midi_hz(pad.CHORD[0])), 1.5 * qa.LOWCUT_HZ)
        self.assertGreaterEqual(pad.MONO_HZ, 354.0)          # round 3: the whole 250 Hz octave in the middle

    def test_the_hat_loop_chokes_every_open_hat(self):
        closed = {s for s, _ in drums.HAT_CLOSED}
        opened = {s for s, _ in drums.HAT_OPEN}
        self.assertFalse(closed & opened)
        self.assertTrue(all(0 <= s < drums.HAT_STEPS for s in closed | opened))
        for s in opened:   # a closed hat follows every open one within an 8th
            self.assertTrue(any((c - s) % drums.HAT_STEPS in (1, 2) for c in closed), s)
        self.assertEqual(opened, {s for s, _ in drums.RIDE_HITS})   # round 3: the ride on the open hats' off-beats


if __name__ == '__main__':
    unittest.main()
