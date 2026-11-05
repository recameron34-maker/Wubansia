"""FUT03's tonal parts (recipes/fut03/bass.py, lead.py, vox.py): the lines stay in the key, leave the kick its room and
sit on their written pitch, A and B share one sub, the vowel bass keeps its first formants in B, the vocal lines keep
to the brief, the robot's slices sit on the grid. The bass tests render the lines and shots (a few seconds, no vocal
models); nothing here needs the vocal models."""
import unittest

import tools.sound  # noqa: F401  (the SIMD pins, before numpy)
import numpy as np
from scipy import signal

from tools.sound import dsp48 as D
from tools.sound import kits, phrase, qa
from tools.sound import render as R
from tools.sound.kits import fut03 as K
from tools.sound.recipes.fut03 import bass as B
from tools.sound.recipes.fut03 import lead as Ld
from tools.sound.recipes.fut03 import vox as X

SR = D.SR


def in_key(note):
    return note % 12 in K.SCALE


def rendered(name):
    """An item's first file as render.py writes it (its high-pass and its gain), with its Out."""
    it = kits.items(kits.load('FUT03'), only=(name,))[0]
    done = R.render_item(it, 'FUT03')
    return done.outs[0], done.takes[0]


def sub_pitch(y, f0):
    """The fundamental's instantaneous frequency and amplitude near f0 (band f0 / 1.3 to 1.3 f0, zero-phase, the loop
    filtered circularly), per sample."""
    m = D.mono(y)
    sos = signal.butter(4, [f0 / 1.3, f0 * 1.3], 'bandpass', fs=SR, output='sos')
    a = signal.hilbert(signal.sosfiltfilt(sos, np.tile(m, 3))[len(m):2 * len(m)])
    return np.gradient(np.unwrap(np.angle(a))) * SR / (2 * np.pi), np.abs(a)


class TestBasslines(unittest.TestCase):
    def test_two_lines_and_a_pick(self):
        self.assertEqual(set(B.LINES), {'Bounce', 'Roll'})
        self.assertIn(B.PICK, B.LINES)

    def test_every_note_is_in_the_key(self):
        for name, line in B.LINES.items():
            for step, length, note, vel, slide in line:
                self.assertTrue(in_key(note), (name, step, D.note_name(note)))
                if slide:
                    self.assertTrue(in_key(slide[0]), (name, step, 'slide', D.note_name(slide[0])))

    def test_the_bass_range_ends_at_g2(self):
        """ADR 0011: the bass-body rule holds for notes up to G2 (98 Hz)."""
        for line in B.LINES.values():
            self.assertLessEqual(max(max(n[2], n[4][0] if n[4] else 0) for n in line), 43)

    def test_room_on_every_kick(self):
        """No note sounds over a kick's first 80 ms (the sub's early swell included), and every note is released CLEAR_S
        before the next kick."""
        for name, line in B.LINES.items():
            for step, length, note, vel, slide in line:
                on = B.G.at(step) / SR
                last_kick = max(k for k in B.KICKS if k <= step) * B.G.s16 / SR
                self.assertGreaterEqual(on - last_kick, qa.KICK_OWN_S, (name, step))
                self.assertLessEqual(B.note_end_s(step, length), B.next_kick_s(step) - B.CLEAR_S + 1e-9,
                                     (name, step, length))
            for a, *_ in B.sub_gaps(name):
                kick = max(B.G.at(k) for k in B.KICKS if B.G.at(k) <= a)
                self.assertGreaterEqual((a - kick) / SR, qa.KICK_OWN_S, name)

    def test_one_note_at_a_time(self):
        for name, line in B.LINES.items():
            for (s0, l0, *_), (s1, *_) in zip(line, line[1:]):
                self.assertLess(s0, s1)
                self.assertLessEqual(B.G.at(s0) + l0 * B.G.s16, B.G.at(s1), (name, s0))

    def test_short_held_ghosts_and_a_slide(self):
        """Short notes, ghosts, one slide, and in every bar a held note: at least 2 16ths, longer than an 8th."""
        for name, line in B.LINES.items():
            lengths = [n[1] for n in line]
            self.assertLess(min(lengths), 0.5, name)
            for bar in (0, 1):
                self.assertGreaterEqual(max(n[1] for n in line if 16 * bar <= n[0] < 16 * (bar + 1)), 2.0, (name, bar))
            self.assertTrue(any(n[3] <= 0.5 for n in line), f'{name}: no ghost notes')
            self.assertEqual(sum(1 for n in line if n[4]), 1, f'{name}: one slide')

    def test_the_line_dip_is_short(self):
        """The bite's dip on a line note is back within 30 cents by 32 ms (the multisamples' takes 90 ms); ghosts don't
        dip at all."""
        n = int(0.2 * SR)
        for vel in (0.7, 0.85, 1.0):
            d = B.line_dip(n, vel)
            self.assertAlmostEqual(-B.P.lerp(B.DIP_ST, vel), d.min(), places=3)
            self.assertTrue(np.all(np.abs(d[int(0.032 * SR):]) < 0.3), vel)
        for vel in (0.4, 0.55, 0.69):
            self.assertFalse(np.any(B.line_dip(n, vel)), vel)

    def test_a_and_b_share_one_sub_on_the_written_notes(self):
        """The sub is the same in both characters, holds every held note's written pitch (no octave folding, no dip),
        and holds the gap's pitch under the pops."""
        for name, line in B.LINES.items():
            gaps = B.sub_gaps(name)
            self.assertEqual(len(B.gap_notes(name)), len(gaps))
            for (k, notes), (a, pitch, level, env) in zip(B.gap_notes(name), gaps):
                cur = None
                for s, ln, note, vel, slide in notes:
                    i = B.G.at(s) - a + int(B.SUB_GLIDE * SR) + 1
                    if not B.is_pop((s, ln)):
                        cur = slide[0] if slide else note
                        self.assertAlmostEqual(note, pitch[i], places=9, msg=(name, s))
                    else:
                        self.assertAlmostEqual(cur if cur is not None else
                                               [n for n in notes if not B.is_pop(n)][0][2], pitch[i], places=9,
                                               msg=(name, s, 'pop'))
            for char in B.CHARACTERS:
                sub, where = B.sub_lane(name, char)
                self.assertTrue(np.array_equal(where, B.sub_lane(name, B.CLEAN)[1]), (name, char))

    def test_the_sub_sits_on_every_held_note(self):
        """Measured on the rendered lines (the round 1 review: the old sub dipped with the voice, 0-59% of each note
        within 30 cents): at least 70% of each held note's sub energy within 30 cents of the written pitch, from its
        onset to the next held note in its gap (or its release), up to any slide."""
        for name in B.LINES:
            for char in B.CHARACTERS:
                out, _ = rendered(f'Line{name}{char.title()}')
                for (k, notes) in B.gap_notes(name):
                    held = [n for n in notes if not B.is_pop(n)]
                    for j, (s, ln, note, vel, slide) in enumerate(held):
                        f0 = float(D.midi_hz(note))
                        f, amp = sub_pitch(out.audio, f0)
                        a = B.G.at(s)
                        b = B.G.at(held[j + 1][0]) if j + 1 < len(held) else a + int((ln * B.G.s16 / SR + 0.025) * SR)
                        if slide:
                            b = min(b, a + int(slide[1] * ln * B.G.s16))
                        w = amp[a:b] ** 2
                        share = np.sum(w * (np.abs(1200 * np.log2(np.maximum(f[a:b], 1) / f0)) < 30)) / np.sum(w)
                        self.assertGreaterEqual(share, 0.7, (name, char, s, D.note_name(note)))

    def test_the_lines_keep_their_margin_under_one_more_high_pass(self):
        """The engine or a DAW may high-pass the bass again: after one more circular 20 Hz high-pass (the renderer's
        own), every line still passes its gap rule with at least 1 dB to spare (A the gate's -20, B bass-body's -2)."""
        for name in B.LINES:
            for char, limit in ((B.CLEAN, qa.MUD_VS_SUB_DB), (B.WARM, qa.BODY_VS_SUB_DB)):
                out, _ = rendered(f'Line{name}{char.title()}')
                for y in (out.audio, phrase.circular(lambda u: D.dc_block(u, 20.0), out.audio)):
                    worst = max(r['vs_sub_db'] for r in qa.loop_mud(y, sorted(out.hits)))
                    self.assertLessEqual(worst, limit - 1.0, (name, char))

    def test_the_lines_swing_on_the_kit_grid(self):
        self.assertEqual(B.G.swing, K.SWING)
        self.assertEqual(B.G.at(3) - B.G.at(2), int(np.floor(3.06 * B.G.s16 + 0.5)) - int(np.floor(2 * B.G.s16 + 0.5)))

    def test_the_dip_is_over_before_the_tuning_window(self):
        d = B.dip(int(0.2 * SR), 1.0)
        self.assertAlmostEqual(d.min(), -B.DIP_ST[1], places=3)
        self.assertTrue(np.all(d[int(0.091 * SR):] == 0.0))

    def test_the_envelopes_settle_exactly(self):
        t = np.arange(int(0.5 * SR)) / SR
        for tau in (B.IDX_TAU, B.FC_TAU):
            e = B.settle(t, tau)
            self.assertEqual(e[0], 1.0)
            self.assertTrue(np.all(e[int(B.SETTLE * SR) + 1:] == 0.0))
        self.assertLess(B.SETTLE, B.ATTACK_S)

    def test_the_top_layer_bounces_with_the_dip(self):
        """Round 3's bright top (the standard: a current future-house bass stacks a top layer that bounces with the
        pitch dip): its formant falls TOP_SWEEP octaves at the bottom of the dip and is back at rest with the note; it
        rides on the bite, so A and B share it, through the bite's steep high-pass at MID_HP: as played, at least 70% of
        it lies over 1 kHz and it is at least 35 dB down under 400 Hz (the low-end rules read 150-400 Hz)."""
        n = int(0.3 * SR)
        for note in (31, 38, 43):
            f = float(D.midi_hz(note))
            for vel in B.BASS_LAYERS:
                freq = f * 2.0 ** (B.dip(n, vel) / 12.0)
                depth = B.dip_depth(freq, f, vel)
                self.assertAlmostEqual(1.0, depth.max(), places=3)
                self.assertEqual(0.0, depth[-1])
                x = B.P.hp_steep(B.top(freq, n, vel, f, D.phase(freq, n), depth), B.MID_HP)
                X = np.abs(np.fft.rfft(x, n=SR)) ** 2
                fr = np.fft.rfftfreq(SR, 1 / SR)
                self.assertGreater(10 * np.log10(X[fr >= 1000].sum() / X.sum()), -1.5, (note, vel))
                self.assertLess(10 * np.log10(X[fr < 400].sum() / X.sum()), -35, (note, vel))
        self.assertGreater(B.TOP_LEVEL, 0.0)
        self.assertEqual(-B.line_dip(n, 1.0).min() / B.P.lerp(B.DIP_ST, 1.0), 1.0)   # a line note's depth, 0..1
        self.assertLess(B.BODY_LEVEL, B.LINE_BODY_LEVEL)      # the lines' body up, the held notes' as round 2 (bass-body)

    def test_the_top_reads_in_the_lines(self):
        """The review: at its first level the top sat 21 / 24 dB under the line's sub (its 2 / 4 kHz octaves against its
        63 Hz one), 10.6 dB under the context loop's 2-4 kHz band while the bass played, so the "yow" was hardly in the
        mix. Raised 6 dB it reads 16 / 18 dB under the sub, the same in A and B; the sub still leads by 12 dB or more."""
        from tools.sound import mixmeter as MM
        for name in B.LINES:
            for char in B.CHARACTERS:
                b = MM.balance(B.line_out(name, char).audio)
                for band, lo in (('2k', -17.0), ('4k', -19.0)):
                    self.assertTrue(lo < b[band] - b['63'] < -12.0, (name, char, band, round(b[band] - b['63'], 1)))

    def test_items_declare_their_low_end(self):
        items = {it.name: it for it in B.ITEMS}
        for name in ('DipBassWarm', 'LineBounceWarm', 'LineRollWarm', 'VowelBassTalkWarm', 'VowelBassRiseWarm'):
            self.assertEqual(items[name].qa['mud'], 'bass-body', name)
            self.assertTrue(items[name].qa['why'])
        for name in ('DipBassClean', 'LineBounceClean', 'LineRollClean', 'VowelBassTalkClean', 'VowelBassRiseClean'):
            self.assertEqual(items[name].qa.get('mud', 'gate'), 'gate', name)
        self.assertEqual(B.kick_onsets(), tuple(B.G.at(s) for s in range(0, 32, 4)))
        self.assertTrue(all('r2' in it.sets for it in B.ITEMS))


class TestVowelBass(unittest.TestCase):
    def test_the_moves_leave_the_kick_room(self):
        """Both moves fall silent ROOM before beat 2 and before the end, and speak again only after beat 2's kick has
        had its 80 ms."""
        beat2, end = B.step_s(4), B.step_s(B.SHOT_STEPS)
        for move in ('talk', 'rise'):
            (a0, b0), (a1, b1) = B.spans(move)
            self.assertLessEqual(b0, beat2 - B.ROOM + 1e-9, move)
            self.assertGreaterEqual(a1 - beat2, qa.KICK_OWN_S, move)
            self.assertLessEqual(b1, end - B.ROOM + 1e-9, move)
        self.assertTrue(all(in_key(m) for _, m, *_ in B.TALK))
        self.assertTrue(in_key(B.RISE_SLIDE[1]) and in_key(B.VOWEL_NOTE))

    def test_b_keeps_the_first_formants_and_a_lifts_them(self):
        """After the first 16th, the old shots (A's design) had nothing between 120 Hz and 1.5 kHz (480 Hz-1.5 kHz read
        -26.5 and -42 dB of the shot): B keeps the first formants there; A, the gate's version, still lifts them."""
        for move in ('Talk', 'Rise'):
            share = {}
            for char in ('Clean', 'Warm'):
                out, _ = rendered(f'VowelBass{move}{char}')
                m = D.mono(out.audio)[int(0.12 * SR):int(0.94 * SR)]
                X = np.abs(np.fft.rfft(m * np.hanning(len(m)))) ** 2
                fr = np.fft.rfftfreq(len(m), 1 / SR)
                share[char] = 10 * np.log10(X[(fr >= 480) & (fr < 1500)].sum() / X.sum())
            self.assertGreater(share['Warm'], -12.0, move)
            self.assertLess(share['Clean'], -24.0, move)

    def test_b_sub_follows_the_move(self):
        """B's sub sings the Talk syllables' notes (G1 G1 G2 | Bb1 D2) instead of A's G1 pedal."""
        out, _ = rendered('VowelBassTalkWarm')
        for s, note, _, _, ln in B.TALK:
            f0 = float(D.midi_hz(note))
            f, amp = sub_pitch(out.audio, f0)
            a = int((B.step_s(s) + B.GLIDE + 0.01) * SR)
            b = int(B.step_s(s + 0.8 * ln) * SR)
            c = np.median(1200 * np.log2(np.maximum(f[a:b], 1) / f0))
            self.assertLess(abs(c), 30, (s, D.note_name(note)))


class TestLead(unittest.TestCase):
    def test_the_phrase_is_two_bars_in_the_key(self):
        steps = [n[0] for n in Ld.PHRASE]
        self.assertEqual(steps, sorted(set(steps)))
        self.assertTrue(0 <= steps[0] and steps[-1] < Ld.PHRASE_STEPS == 32)
        for step, note, length, vel in Ld.PHRASE:
            self.assertTrue(in_key(note), (step, D.note_name(note)))
            self.assertTrue(0 < vel <= 1 and length >= 1)

    def test_the_answer_varies_the_call(self):
        """Bar 2 opens like bar 1 (the same first note and rhythm) and then goes elsewhere."""
        bar = lambda b: [(s - 16 * b, m) for s, m, _, _ in Ld.PHRASE if 16 * b <= s < 16 * (b + 1)]
        one, two = bar(0), bar(1)
        self.assertEqual([s for s, _ in one], [s for s, _ in two])
        self.assertEqual(one[0], two[0])
        self.assertNotEqual(one, two)

    def test_zones_cover_the_phrase(self):
        notes = [n[1] for n in Ld.PHRASE]
        self.assertLessEqual(Ld.PLUCK_NOTES[0], min(notes))
        self.assertGreaterEqual(Ld.PLUCK_NOTES[-1], max(notes))
        self.assertTrue(all(in_key(n) for n in Ld.PLUCK_NOTES))


class TestVocals(unittest.TestCase):
    def test_three_lines_of_three_to_six_words(self):
        self.assertEqual(len(X.LINES), 3)
        for ln in X.LINES:
            words = ln.text.split()
            self.assertTrue(3 <= len(words) <= 6, ln.text)
            sung = [w for _, _, _, w in ln.notes if w != '~']
            self.assertEqual(sung, [w.lower() for w in words], ln.name)
            self.assertTrue(all(in_key(m) for _, _, m, _ in ln.notes), ln.name)
            self.assertIn(ln.accent, ('us', 'gb'))

    def test_the_chops_come_from_the_pick(self):
        self.assertIn(X.PICK, X.LINE_BY_NAME)
        self.assertTrue(6 <= len(X.CHOPS) <= 8)
        n_events = 6                                    # ride the cur rent with me
        for name, first, last, words, root, about in X.CHOPS:
            self.assertTrue(0 <= first <= last < n_events, name)
            self.assertTrue(in_key(root), name)
            self.assertTrue(set(words.split()) <= set(X.LINE_BY_NAME[X.PICK].text.lower().split()), name)

    def test_the_robot_phrase_sits_on_the_grid(self):
        self.assertTrue(2 <= len(X.ROBOT.split()) <= 4)
        self.assertEqual(len(X.ROBOT_GRID), len(X.ROBOT_SYLLABLES))
        steps = [s for s, _ in X.ROBOT_GRID]
        self.assertEqual(steps, sorted(set(steps)))
        onsets = [n for _, n, _ in X.SLICES]
        self.assertEqual(onsets, sorted(set(onsets)))
        for step, n, _ in X.SLICES:
            self.assertEqual(n, int(np.floor((X.ROBOT_PRE + step * K.STEP - X.ROBOT_LEAD) * SR + 0.5)))
        self.assertTrue(in_key(X.ROBOT_NOTE) and in_key(X.DROP_TO))
        self.assertTrue(all(in_key(X.ROBOT_NOTE + st) for st, _ in X.CHORD))

    def test_every_vocal_is_clean_and_synthetic(self):
        for it in X.ITEMS:
            self.assertIn('clean', it.tags, it.name)
            self.assertIn('hf_synthetic', it.flags, it.name)
            self.assertTrue(it.script, it.name)
            self.assertIn('r2', it.sets, it.name)


if __name__ == '__main__':
    unittest.main()
