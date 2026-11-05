"""FUT04's MIDI (tools/sound/recipes/fut04/midi.py), on top of test_midi.py's kit-agnostic properties: its Loop files
lane by lane against its round 5 context loop (context5.py on context.py), the rumble on drum note 35, the blips'
ratchets, its roots, its phrases' channels, the vox's vowels and throws, the thrown accents, the acid's rides and the
audit's gate example; and its own extended mix (PARTS['form']), audit B's FUT04 musts on the exported files: the acid's
filter told on CC74/CC71, a techno build, a groove that develops, the drop and drop 2 on their own material, the rumble
under the kick, the vox clear of the drone, 32 bars of drums at each end and a change at every line. FUT04's own tests:
its engineer edits them with the kit's PARTS."""
import unittest

from tools.sound import dsp48 as D
from tools.sound import midi as Mi
from tools.sound.recipes.fut04 import accents as A, bass as B, context as C, context5 as C5, lead as L, pad as Pd
from tools.sound.recipes.fut04 import midi as KM, voxsynth as V
from tools.sound.tests.midi_testlib import (BAR, VARIANTS, LoopCase, at, chord_roots, drum, kit_files, layers_of, loop,
                                            mids, one_shot, parse, parsed, phrase_channels, ride, tick_step, v127,
                                            value_at, whole)

KIT = 'FUT04'
RIDE_CCS = (74, 71)
OPEN_HAT, CLOSED_HAT, RIDE, ANVIL, SNARE, KICK, RUMBLE = 46, 42, 51, 47, 38, 36, 35
STAB_SHAPE = {64: A.EM, 69: A.AM, 71: A.BM}          # the metal and rave stab chords by the root they're played on


class TestLoop(LoopCase, unittest.TestCase):
    KIT = KIT

    def expected(self, variant):
        a = lambda s: at(KIT, s)  # noqa: E731
        line = B.LINE_BY_NAME[B.PICK]
        mode = C5.VOX if variant == 'vox' else C5.NOVOX
        out = {
            'Kick': drum(KIT, C.KICK, 36),
            'Rumble': [(0, 35, 1.0, 32 * 240), (a(32), 35, 1.0, a(C.KICK_OUT))],
            'Snare': drum(KIT, C.SNARE, 38),
            'Hats': drum(KIT, C.HAT_CLOSED, 42) + drum(KIT, C.HAT_OPEN, 46) + drum(KIT, C.RIDE, 51),
            'Perc': drum(KIT, C.ANVIL, 47),
            'Bass': [(a(s + 32 * r), n, v, None) for r in (0, 1)
                     for s, n, _, v, *_ in tuple(line.notes) + tuple(line.ticks)],
            'Lead': [(at(KIT, s + 32 * r) + round(k * 240 / rt), n, v * D.undb(L.RATCHET_DB * k), None)
                     for r in (0, 1) for s, n, _, v, rt in L.PHRASE for k in range(rt)],
            'Chords': [(0, 52, 1.0, 64 * 240)],
            'Chord voicing': [(0, n, 1.0, 64 * 240) for n in Pd.CHORD],
            'Accents': [one_shot(KIT, 'accents', name, 64 if s == 'end' else s, 1, 'end' if s == 'end' else 'on')
                        for s, name in C5.ACCENTS[mode]],
            'FX': [one_shot(KIT, 'fx', 'StutterFill', C.FILL_AT, 1, 'on')],
        }
        if variant == 'vox':
            out['Vox'] = [(a(s), m, v, None) for s, m, _, v in C5.VOX_NOTES]
        return out


class TestParts(unittest.TestCase):
    def test_the_drone_at_its_root(self):
        """The drone Em(add9) on E3 (52), on channel 4; its voicing on 3."""
        self.assertEqual(({52}, {4}, {3}), chord_roots(KIT))

    def test_the_phrases_channels(self):
        self.assertEqual({'VoxSynthRiff': 5, 'AcidStabRiff': 11, 'MetalChords': 12, 'RaveStabRiff': 13},
                         phrase_channels(KIT))

    def test_the_vox_synths_velocity_picks_its_vowel(self):
        """Every vox note sits on a layer's velocity: 64 'oo', 127 'oh'."""
        self.assertEqual([64, 127], [v127(x) for x in layers_of(KIT, 'VoxSynth')])
        for path in mids(KIT):
            for t in parsed(KIT, path)[2][1:]:
                if t['name'] == 'Vox':
                    self.assertLessEqual({vel for *_, vel in t['notes']}, {64, 127}, path)

    def test_the_throws(self):
        """The vox's throws on 16ths 30 and 46; without the vox, the zap and the metal chord thrown there."""
        got = sorted(c[0] for c in loop(KIT, 'vox')['Vox']['ccs'] if c[2] == 91 and c[3] == 127)
        self.assertEqual([at(KIT, 30), at(KIT, 46)], got)
        got = sorted(c[0] for c in loop(KIT, 'novox')['Accents']['ccs'] if c[2] == 91 and c[3] == 127)
        self.assertEqual([at(KIT, 30), at(KIT, 46)], got)

    def test_the_acid_e1_no_longer_runs_into_the_e2(self):
        """Audit A defect 5: E1 on the swung 16th 1 ended 19 ticks into E2 on 2."""
        notes = {(on, key): off for on, off, _, key, _ in parse(kit_files(KIT)[
            'FUT04_Bassline_AcidLine1_InTheLoop_2bars.mid'])[2][1]['notes']}
        self.assertEqual(2 * 240, notes[(240 + 19, 28)])


class TestRides(unittest.TestCase):
    def test_the_acid_ride(self):
        """The acid's 4-bar ride (bass.RIDES) on channel 1: CC74 its cutoff multiplier (x1-x2.6 on 5400-14040 Hz), CC71
        its resonance (0.90-0.96): both at rest (0) over bars 1-2, at their top on the fill bar's third beat."""
        for cc in (74, 71):
            r = ride(KIT, 'Bass', cc)
            self.assertEqual({0}, {v for t, v in r.items() if t < 32 * 240})
            self.assertEqual(127, max(r.values()))
            self.assertEqual(at(KIT, 56), max(r, key=lambda t: (r[t], -t)))


# ------------------------------------------------------------------ the extended mix
def bar_tick(bar):
    """A bar's first tick (bars count from 1)."""
    return (bar - 1) * BAR


def notes_in(tracks, name, lo, hi):
    """A track's notes (on, off, ch, key, vel) starting in bars lo-hi, both included."""
    return [n for n in tracks.get(name, {'notes': []})['notes'] if bar_tick(lo) <= n[0] < bar_tick(hi + 1)]


def bars_with(tracks, name, lo=1, hi=224):
    """The bars (from 1) where a track starts a note."""
    return sorted({n[0] // BAR + 1 for n in notes_in(tracks, name, lo, hi)})


def step_in_bar(tick):
    """A note-on's 16th in its bar (a swung odd 16th back on its grid)."""
    return tick_step(KIT, tick) % 16


def curve(tracks, cc, track='Bass'):
    """A controller's points on a track: {tick: value}."""
    return {t: v for t, _, c, v in tracks[track]['ccs'] if c == cc}


def sounding(tracks, name, lo_tick, hi_tick):
    """A track's notes sounding anywhere in [lo_tick, hi_tick)."""
    return [n for n in tracks.get(name, {'notes': []})['notes'] if n[0] < hi_tick and n[1] > lo_tick]


def section(R, kind):
    (sec,) = [s for s in R['sections'] if s['kind'] == kind]
    return sec


def lines_of(R):
    return [ln for s in R['sections'] for ln in s['lines']]


def chord_tones(tracks, tick):
    """The harmony at a tick: the drone's voicing (channel 3) where it sounds; the metal and rave stab chords (by their
    root's shape) of the last hit at most 2 beats before, which the ear holds until the next; the acid stab's note
    where it sounds."""
    out = set()
    for n in sounding(tracks, 'Chord voicing', tick, tick + 1):
        out.add(n[3])
    for name in ('Metal Chord', 'Rave Stab'):
        last = [n for n in tracks.get(name, {'notes': []})['notes'] if tick - 2 * 960 <= n[0] <= tick]
        if last:
            out |= set(STAB_SHAPE[max(last)[3]])
    for n in sounding(tracks, 'Acid Stab', tick, tick + 1):
        out.add(n[3])
    return out


class TestExtendedMix(unittest.TestCase):
    """Audit B's FUT04 musts on the exported extended mixes, both variants: FUT04's own form (PARTS['form'])."""

    def test_the_kits_own_form(self):
        """Intro, groove, break, build, drop, drop 2, outro: 32 + 64 + 16 + 16 + 32 + 32 + 32 bars."""
        for v in VARIANTS:
            R, _, _ = whole(KIT, v)
            self.assertTrue(Mi.kit_of(KIT).P.get('form'))
            self.assertEqual([('intro', 1, 32), ('groove', 33, 64), ('break', 97, 16), ('build', 113, 16),
                              ('drop', 129, 32), ('drop2', 161, 32), ('outro', 193, 32)],
                             [(s['kind'], s['start_bar'], s['bars']) for s in R['sections']])

    def test_32_bars_of_drums_at_each_end_the_acid_from_33(self):
        """Audit B 6: the acid is the first thing over the drums, at bar 33; nothing but drums from bar 193."""
        for v in VARIANTS:
            R, tracks, _ = whole(KIT, v)
            music = {name: t for name, t in tracks.items() if t['notes'] and name not in ('Kick', 'Rumble', 'Snare',
                                                                                          'Hats', 'Perc')}
            first = min(t['notes'][0][0] for t in music.values())
            with self.subTest(variant=v):
                self.assertEqual(33, first // BAR + 1)
                self.assertEqual(first, tracks['Bass']['notes'][0][0])
                self.assertLessEqual(max(n[1] for t in music.values() for n in t['notes']), bar_tick(193))

    def test_the_rumble_under_the_kick_every_2_bars(self):
        """Audit B 4: the rumble (note 35, a 2-bar loop) starts every 2 bars over 9-96 and 113-208, never in the
        break or the first and last 8 bars, and sounds only in beats where the kick plays (it stops with the kick under
        the fills, the build's last bar and any drum drop)."""
        want = list(range(9, 97, 2)) + list(range(113, 209, 2))
        for v in VARIANTS:
            R, tracks, _ = whole(KIT, v)
            rumble = tracks['Rumble']['notes']
            kicks = {n[0] // 960 for n in tracks['Kick']['notes']}
            with self.subTest(variant=v):
                self.assertEqual(want, [n[0] // BAR + 1 for n in rumble])
                self.assertEqual({RUMBLE}, {n[3] for n in rumble})
                for on, off, *_ in rumble:
                    self.assertLessEqual(off - on, 2 * BAR)
                    self.assertEqual(set(range(on // 960, -(-off // 960))) - kicks, set(), (on // BAR + 1, off))

    def test_the_acid_filter_tells_the_story(self):
        """Audit B 1: CC74 and CC71 on the Bass track: closed (0) for the acid's first line, opening across the groove
        (never falling, past 64 by its end), the loop's 4-bar ride (bars 3-4 of every 4 swell, the recorded stem) all
        through the drop, wide open (127) in the build's last 2 bars, opening to 127 in drop 2 and closed again (0) as
        it strips into the outro."""
        cut, res = KM.RIDES[B.PICK]
        for v in VARIANTS:
            R, tracks, _ = whole(KIT, v)
            for cc, loop_ride in zip(RIDE_CCS, (cut, res)):
                pts = curve(tracks, cc)
                val = lambda t: value_at(pts, t)  # noqa: E731
                with self.subTest(variant=v, cc=cc):
                    self.assertEqual({0}, {val(t) for t in range(bar_tick(33), bar_tick(41), 60)})
                    groove = [val(t) for t in range(bar_tick(33), bar_tick(97), 60)]
                    self.assertEqual(sorted(groove), groove)
                    self.assertGreaterEqual(groove[-1], 64)
                    drop = section(R, 'drop')
                    lo = bar_tick(drop['start_bar'])
                    for t in range(lo, lo + drop['bars'] * BAR, 60):
                        self.assertEqual(loop_ride[(t - lo) % (4 * BAR) // 60], val(t), (cc, t // BAR + 1))
                    build_end = bar_tick(section(R, 'drop')['start_bar'])
                    self.assertEqual({127}, {val(t) for t in range(build_end - 2 * BAR, build_end, 60)})
                    d2 = section(R, 'drop2')
                    lo, hi = bar_tick(d2['start_bar']), bar_tick(d2['start_bar'] + d2['bars'])
                    self.assertEqual(127, max(val(t) for t in range(lo, hi, 60)))
                    self.assertEqual(0, val(hi - 60))

    def test_a_techno_build(self):
        """Audit B 2: the kick and the rumble on through the build but its last bar; the hats off-beat, then the
        loop's, then on every 16th; a roll (snare or anvil) only in the bar before the fill, on every 16th, rising;
        the stutter fill alone in its bar (no drum sounds there); the iron riser over the build's last 8 bars; the acid
        on throughout."""
        k = Mi.kit_of(KIT)
        fill_key, riser_key = k.sound('StutterFill')[1], k.sound('IronRiser')[1]
        for v in VARIANTS:
            R, tracks, _ = whole(KIT, v)
            b = section(R, 'build')
            lo, last = b['start_bar'], b['start_bar'] + b['bars'] - 1          # 113, 128
            with self.subTest(variant=v):
                self.assertEqual(list(range(lo, last)), bars_with(tracks, 'Kick', lo, last))
                drums = [n for name in ('Kick', 'Rumble', 'Snare', 'Hats', 'Perc')
                         for n in sounding(tracks, name, bar_tick(last), bar_tick(last + 1))]
                self.assertEqual([], drums)
                fx = notes_in(tracks, 'FX', last, last)
                self.assertIn((bar_tick(last), bar_tick(last + 1), fill_key), [(n[0], n[1], n[3]) for n in fx])
                riser = [n for n in tracks['FX']['notes'] if n[3] == riser_key and n[1] == bar_tick(last + 1)]
                self.assertEqual([bar_tick(last - 7)], [n[0] for n in riser])
                for bar in range(lo, last - 1):
                    steps = {step_in_bar(n[0]) for n in notes_in(tracks, 'Hats', bar, bar)}
                    if bar < lo + 4:
                        self.assertEqual({2, 6, 10, 14}, steps, bar)
                    elif bar < lo + 7:                     # (bar lo + 7 ends the first line: its turnaround)
                        self.assertGreaterEqual(len(steps), 15, bar)
                    elif bar >= lo + 8:
                        self.assertEqual(set(range(16)), steps, bar)
                track = {'snare': 'Snare', 'perc': 'Perc'}[b['lines'][-1]['roll']]
                rolled = notes_in(tracks, track, last - 1, last - 1)
                self.assertEqual(list(range(16)), sorted(step_in_bar(n[0]) for n in rolled))
                vels = [n[4] for n in sorted(rolled)]
                self.assertEqual(sorted(vels), vels)
                self.assertLess(vels[0], vels[-1])
                for name in ('Snare', 'Perc'):
                    for bar in range(lo, last - 1):
                        self.assertLessEqual(len(notes_in(tracks, name, bar, bar)), 4, (name, bar))
                self.assertEqual(list(range(lo, last + 1)), bars_with(tracks, 'Bass', lo, last))

    def test_the_groove_develops(self):
        """Audit B 3, the groove: the metal chords (Em Em Am Bm on the off-beats, channel 12) from bar 65, not
        before; the ride on 8ths from bar 81 (on the off-beats only before it)."""
        for v in VARIANTS:
            R, tracks, _ = whole(KIT, v)
            metal = tracks['Metal Chord']['notes']
            with self.subTest(variant=v):
                self.assertEqual(65, metal[0][0] // BAR + 1)
                for n in metal:
                    bar = n[0] // BAR + 1
                    self.assertEqual((64, 64, 69, 71)[(bar - 65) % 4], n[3], bar)
                    self.assertIn(step_in_bar(n[0]), (2, 6, 10, 13))
                on_beat = lambda lo, hi: sorted({n[0] // BAR + 1 for n in notes_in(tracks, 'Hats', lo, hi)  # noqa
                                                 if n[3] == RIDE and step_in_bar(n[0]) % 4 == 0})
                self.assertEqual([], on_beat(1, 80))
                self.assertEqual(list(range(81, 88)), on_beat(81, 87))
                for bar in range(81, 88):
                    self.assertEqual(set(range(0, 16, 2)), {step_in_bar(n[0]) for n in notes_in(tracks, 'Hats', bar,
                                                                                                bar) if n[3] == RIDE})

    def test_the_drop(self):
        """Audit B 3, the drop: the rave stab riff (channel 13) in two of its lines and the vox in the other two, never
        together; the impact (the sub drop and the distorted chord) on its first beat; the loop's accents in every
        block of its vox lines."""
        k = Mi.kit_of(KIT)
        sub, dist = k.sound('AccentSubDropE2')[1], k.sound('AccentDistChordEm')[1]
        for v in VARIANTS:
            R, tracks, _ = whole(KIT, v)
            d = section(R, 'drop')
            rave, vox = [], []
            for ln in d['lines']:
                lo = ln['start_bar']
                rave.append(bool(notes_in(tracks, 'Rave Stab', lo, lo + 7)))
                vox.append(bool(notes_in(tracks, 'Vox', lo, lo + 7)))
            with self.subTest(variant=v):
                self.assertEqual(2, sum(rave))
                if v == 'vox':
                    self.assertEqual([not r for r in rave], vox)
                else:
                    self.assertEqual([False] * 4, vox)
                t0 = bar_tick(d['start_bar'])
                self.assertIn(sub, [n[3] for n in tracks['FX']['notes'] if n[0] == t0])
                self.assertIn(dist, [n[3] for n in tracks['Accents']['notes'] if n[0] == t0])
                for ln, has_rave in zip(d['lines'], rave):
                    if not has_rave:
                        for block in (0, 4):
                            lo = ln['start_bar'] + block
                            self.assertTrue(notes_in(tracks, 'Accents', lo, lo + 3), lo)

    def test_drop_2(self):
        """Audit B 3, drop 2: Acid Line 2 all through, the acid stab riff (channel 11), the anvil on 8ths, a ride on
        8ths or 16ths (the NICE), an impact on its first beat."""
        k = Mi.kit_of(KIT)
        sub = k.sound('AccentSubDropE2')[1]
        line2 = {n[1] for n in B.LINE_BY_NAME['AcidLine2'].notes}
        for v in VARIANTS:
            R, tracks, _ = whole(KIT, v)
            d = section(R, 'drop2')
            lo, hi = d['start_bar'], d['start_bar'] + d['bars'] - 1
            with self.subTest(variant=v):
                self.assertEqual({'AcidLine2'}, {ln['bass'] for ln in d['lines']})
                self.assertLessEqual({n[3] for n in notes_in(tracks, 'Bass', lo, hi)}, line2)
                self.assertTrue(notes_in(tracks, 'Acid Stab', lo, hi))
                self.assertEqual([], [n for n in tracks['Acid Stab']['notes'] if not bar_tick(lo) <= n[0]
                                      < bar_tick(hi + 1)])
                anvil8 = [bar for bar in range(lo, hi + 1) if {0, 2, 4, 6, 8, 10, 12, 14} <= {
                    step_in_bar(n[0]) for n in notes_in(tracks, 'Perc', bar, bar) if n[3] == ANVIL}]
                self.assertGreaterEqual(len(anvil8), 16)
                ride8 = [bar for bar in range(lo, hi + 1) if {0, 2, 4, 6, 8, 10, 12, 14} <= {
                    step_in_bar(n[0]) for n in notes_in(tracks, 'Hats', bar, bar) if n[3] == RIDE}]
                self.assertGreaterEqual(len(ride8), 16)
                self.assertIn(sub, [n[3] for n in tracks['FX']['notes'] if n[0] == bar_tick(lo)])

    def test_the_vox_is_the_riff_clear_of_the_drone(self):
        """Audit B 5: the vox plays the riff's notes as recorded (each note's 16th in its 2 bars, key and vowel, or the
        loop's own D4 on its bar 3's 'and' of 4); the drone (channels 3 and 4) is out wherever the vox sounds; no vox
        note doubles a chord tone sounding with it or sits a semitone from one."""
        riff = {(s, m, round(vel * 127)) for s, m, _, vel in V.RIFF} | {(14, 62, 127)}
        R, tracks, _ = whole(KIT, 'vox')
        vox = tracks['Vox']['notes']
        self.assertTrue(vox)
        for on, off, ch, key, vel in vox:
            with self.subTest(bar=on // BAR + 1, step=step_in_bar(on)):
                self.assertIn((tick_step(KIT, on) % 32, key, vel), riff)
                self.assertEqual([], sounding(tracks, 'Chords', on, off) + sounding(tracks, 'Chord voicing', on, off))
                under = chord_tones(tracks, on)
                self.assertNotIn(key, under)
                self.assertEqual([], [c for c in under if (key - c) % 12 in (1, 11)])

    def test_the_vox_placement(self):
        """The vox's riff in the break (over the metal chords: the NICE), in two of the drop's lines and in drop 2;
        in the groove only its throw into the break; thrown (CC91 at 127) into the break and into the outro."""
        R, tracks, _ = whole(KIT, 'vox')
        vox = tracks['Vox']
        brk = section(R, 'break')
        lo = brk['start_bar']
        self.assertTrue(notes_in(tracks, 'Vox', lo, lo + 7))
        self.assertTrue(notes_in(tracks, 'Metal Chord', lo, lo + 7))
        self.assertTrue(notes_in(tracks, 'Vox', section(R, 'drop2')['start_bar'], 192))
        self.assertEqual([96], bars_with(tracks, 'Vox', 33, 96))
        thrown = {t // BAR + 1 for t, _, c, val in vox['ccs'] if c == 91 and val == 127}
        self.assertLessEqual({96, 192}, thrown)
        for t, _, c, val in vox['ccs']:
            if c == 91 and val == 127:
                self.assertIn(t, [n[0] for n in vox['notes']])

    def test_without_the_vox_the_accents_answer(self):
        """Drop 2's last line is a call and response, the blips calling and the vox answering; without the vox, thrown
        accents answer in the same bars."""
        R, sung_tracks, _ = whole(KIT, 'vox')
        _, tracks, _ = whole(KIT, 'novox')
        lo = section(R, 'drop2')['lines'][-1]['start_bar']
        sung = bars_with(sung_tracks, 'Vox', lo, lo + 7)
        called = bars_with(tracks, 'Lead', lo, lo + 7)
        thrown = sorted({t // BAR + 1 for t, _, c, val in tracks['Accents']['ccs']
                         if c == 91 and val == 127 and bar_tick(lo) <= t < bar_tick(lo + 8)})
        self.assertEqual([lo + 1, lo + 3, lo + 5, lo + 7], sung)
        self.assertEqual([lo, lo + 2, lo + 4, lo + 6], called)
        self.assertEqual(sung, thrown)

    def test_a_change_at_every_line(self):
        """Every 8-bar line differs from the one before in its first 7 bars (not only in its turnaround or fill);
        26 or more of the 28 lines are distinct; every 16 bars the first bar of the new 16 differs from the first bar
        of the 16 before; every line with drums but the mix's first and last ends on a fill, a turnaround, a drum drop
        or a roll."""
        for v in VARIANTS:
            R, tracks, _ = whole(KIT, v)
            bars = [set() for _ in range(224)]
            for name, t in tracks.items():
                for on, off, ch, key, vel in t['notes']:
                    bars[on // BAR].add((name, on % BAR, key, vel))
            bars = [frozenset(b) for b in bars]
            lines = [tuple(bars[i:i + 8]) for i in range(0, 224, 8)]
            with self.subTest(variant=v):
                for i in range(1, 28):
                    self.assertNotEqual(lines[i - 1][:7], lines[i][:7], 8 * i + 1)
                self.assertGreaterEqual(len(set(lines)), 26)
                for b in range(16, 224, 16):
                    self.assertNotEqual(bars[b - 16], bars[b], b + 1)
                lns = lines_of(R)
                for ln in lns[1:-1]:
                    if any(lane in ln['lanes'] for lane in ('kick', 'snare', 'hats', 'perc')):
                        self.assertTrue(ln.get('end') in ('fill', 'turn', 'drop') or ln.get('roll'), ln['start_bar'])

    def test_the_fills_set_up_the_sections(self):
        """The stutter fill ends the groove's two halves, the build, the drop and drop 2 (into the break, the drop,
        drop 2 and the outro), and nowhere in the drums-only ends."""
        k = Mi.kit_of(KIT)
        fill_key = k.sound('StutterFill')[1]
        for v in VARIANTS:
            R, tracks, _ = whole(KIT, v)
            fills = [n[0] // BAR + 1 for n in tracks['FX']['notes'] if n[3] == fill_key]
            with self.subTest(variant=v):
                self.assertEqual([64, 96, 128, 160, 192], fills)


if __name__ == '__main__':
    unittest.main()
