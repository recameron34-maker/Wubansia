"""FUT01's MIDI (tools/sound/recipes/fut01/midi.py), on top of test_midi.py's kit-agnostic properties: its Loop files
lane by lane against its round 5 context loop (context5.py on context4.py), its roots, its phrases' channels, its
wobble ride and the audit's gate example; and its own extended mix (audit B's FUT01 must-fixes, in FUT01's own form):
32 bars of drums at each end, the groove on Low, a UK garage build into a drop that is a drop, a 4x4 speed garage
switch-up on Speed, the Air Pad under both breaks, drop 2 with the organ riff, turnarounds, a percussion move every 16
bars, busy hats you hear, the vox riff placed with intent on its harmony and the pluck playing around it, the accents as
notes on their cues, and a change at every line. FUT01's own tests: its engineer edits them with the kit's PARTS."""
import functools
import unittest

from tools.sound import midi as Mi
from tools.sound.recipes.fut01 import (context4 as C, context5 as C5, keys as Ky, music as M, riffs as Rf,
                                       voxsynth as V)
from tools.sound.tests.midi_testlib import (BAR, VARIANTS, LoopCase, at, chord_roots, drum, kit_files, loop, one_shot,
                                            parse, phrase_channels, ride, tick_step, value_at, whole)

KIT = 'FUT01'


class TestLoop(LoopCase, unittest.TestCase):
    KIT = KIT

    def expected(self, variant):
        a = lambda s: at(KIT, s)  # noqa: E731
        stabs = [(s, z, v) for s, z, v in C.STABS if s not in C5.STAB_DROP[variant]]
        out = {
            'Kick': drum(KIT, C.KICK, 36),
            'Clap': drum(KIT, C.CLAP, 38),
            'Hats': drum(KIT, C.HAT_C, 42) + drum(KIT, C.HAT_O, 46),
            'Perc': drum(KIT, C.TAMB, 54) + drum(KIT, C.RIM, 37) + drum(KIT, C.PERC, 47),
            'Bass': [(a(s + 32 * r), n, v, None) for r in (0, 1) for s, _, n, v, _ in C.WARP_LINE],
            'Lead': [(a(s + 32 * r), n, v, None) for r in (0, 1) for s, n, _, v in M.PHRASE],
            'Chords': [(a(s), z, v, a(s) + 240) for s, z, v in stabs],
            'Chord voicing': [(a(s), z + d, v, a(s) + 240) for s, z, v in stabs for d in Ky.SHAPE],
            'Accents': [one_shot(KIT, 'accents', name, step, layer, how, gate=gate)
                        for _, name, step, layer, how, gate in C5.ACCENTS[variant]],
            'FX': [one_shot(KIT, 'fx', 'GlitchFillLift', C.FILL_AT, 1, 'on', start=C.FILL_FROM)],
        }
        if variant == 'vox':
            out['Vox'] = [(a(s + at_), n, v, None) for at_ in C5.RIFF_AT['vox'] for s, n, _, v, _ in V.RIFF_NOTES]
        return out


class TestParts(unittest.TestCase):
    def test_the_organ_stabs_at_their_roots(self):
        """F#m9 on F#3 (54), Bm9 on B3 (59), on channel 4; the voicing on 3."""
        self.assertEqual(({54, 59}, {4}, {3}), chord_roots(KIT))

    def test_the_phrases_channels(self):
        self.assertEqual({'OrganRiff': 12, 'WarpRiff': 1, 'PluckRiff': 2, 'EPianoStabRhythm': 4, 'VoxSynthRiff': 5},
                         phrase_channels(KIT))

    def test_the_low_line_plays_the_organ_bass_on_channel_11(self):
        notes = parse(kit_files(KIT)['FUT01_Bassline_Low_2bars.mid'])[2][1]['notes']
        self.assertEqual({11}, {ch for _, _, ch, _, _ in notes})
        for name in ('FUT01_Bassline_Bounce_InTheLoop_2bars.mid', 'FUT01_Bassline_Speed_2bars.mid'):
            self.assertEqual({1}, {ch for _, _, ch, _, _ in parse(kit_files(KIT)[name])[2][1]['notes']})

    def test_the_bounce_f_sharp_1_no_longer_runs_into_the_f_sharp_2(self):
        """Audit A defect 5: F#1 on the swung 16th 11 ended 43 ticks into the straight F#2 on 12."""
        notes = {(on, key): off for on, off, _, key, _ in parse(kit_files(KIT)[
            'FUT01_Bassline_Bounce_InTheLoop_2bars.mid'])[2][1]['notes']}
        self.assertEqual(12 * 240, notes[(11 * 240 + 43, 30)])

    def test_the_kicks_halfway_between_layers_play_the_louder(self):
        """The 0.9 kicks sit halfway between the 0.8 and 1.0 layers, where the recipe plays 1.0: velocity 115, not
        round(0.9 x 127) = 114, which a nearest-layer sampler plays on 0.8."""
        kicks = {vel for _, _, _, _, vel in loop(KIT, 'vox')['Kick']['notes']}
        self.assertEqual({70, 115, 127}, kicks)

    def test_no_throws(self):
        """FUT01's riff throw is baked into its stem: no CC91/CC94."""
        for v in ('vox', 'novox'):
            self.assertFalse([c for t in loop(KIT, v).values() for c in t['ccs'] if c[2] in (91, 94)])


class TestRides(unittest.TestCase):
    def test_the_wobble(self):
        """The warp bass's wobble (context4.wobble_cutoff) on CC74, channel 1, 650-2210 Hz: closed on every straight
        8th's edge, open in its middle, widest in bar 4, then bar 2; no CC71 (its Q is fixed)."""
        r = ride(KIT, 'Bass', 74)
        peaks = [max(v for t, v in r.items() if b * BAR <= t < (b + 1) * BAR) for b in range(4)]
        self.assertEqual(127, peaks[3])
        self.assertEqual(sorted(peaks), [peaks[0], peaks[2], peaks[1], peaks[3]])
        self.assertEqual(0, value_at(r, 0))
        self.assertEqual(peaks[0], value_at(r, 240))
        self.assertFalse(ride(KIT, 'Bass', 71))



# ------------------------------------------------------------------ the extended mix
FORM = (('Intro', 'intro', 1, 32), ('Groove', 'groove', 33, 32), ('Break', 'break', 65, 16), ('Build', 'build', 81, 8),
        ('Drop', 'drop', 89, 32), ('Switch-up', 'switch', 121, 16), ('Break 2', 'break', 137, 16),
        ('Drop 2', 'drop2', 153, 40), ('Outro', 'outro', 193, 32))
KICK, RIM, CLAP, HAT, OPEN, ALLOY, TAMB = 36, 37, 38, 42, 46, 47, 54
TRACK_OF = {KICK: 'Kick', CLAP: 'Clap', HAT: 'Hats', OPEN: 'Hats', RIM: 'Perc', ALLOY: 'Perc', TAMB: 'Perc'}
DRUMS = ('Kick', 'Clap', 'Hats', 'Perc')
BASSES = ('Bass', 'Organ Bass')
LEADS = ('Lead', 'Organ Lead')
M9 = (0, 3, 7, 10, 14)              # the organ stab's and the Air Pad's m9 over the root they're played on
# the vox in the vox mix, line by line (bars of the whole mix): the riff placed with intent
VOX = {57: 'answer', 65: 'riff', 73: 'riff', 81: 'call', 89: 'riff', 97: 'riff', 113: 'answer', 161: 'riff',
       177: 'call'}


@functools.lru_cache(maxsize=None)
def mix(variant):
    """The whole extended mix: (regions.json, {track: [(on, off, key, velocity)]})."""
    R, tracks, _ = whole(KIT, variant)
    return R, {name: [(on, off, key, vel) for on, off, _, key, vel in t['notes']] for name, t in tracks.items()}


@functools.lru_cache(maxsize=None)
def bars_of(variant):
    """{1-based bar: {track: [(on, off, key, velocity)] starting in it}} of the whole mix."""
    out = {}
    for name, notes in mix(variant)[1].items():
        for n in notes:
            out.setdefault(n[0] // BAR + 1, {}).setdefault(name, []).append(n)
    return out


def lo(bar):
    """A 1-based bar's first tick."""
    return (bar - 1) * BAR


def notes(variant, track, first, last=None):
    """A track's notes starting in bars first to last (1-based, both in)."""
    got = bars_of(variant)
    return sorted(n for b in range(first, (last or first) + 1) for n in got.get(b, {}).get(track, []))


def steps(variant, key, bar, track=None):
    """The 16ths of a bar that a key starts on (on its drum track, or the given track)."""
    return sorted(tick_step(KIT, n[0] - lo(bar)) for n in notes(variant, track or TRACK_OF[key], bar) if n[2] == key)


def at_16th(variant, track, bar, step, key):
    """The velocity of a key's note on a bar's 16th (None if there's none)."""
    got = [n[3] for n in notes(variant, track, bar) if n[2] == key and n[0] == lo(bar) + at(KIT, step)]
    return got[0] if got else None


def phrase(variant, track, first, bars):
    """A track's notes over `bars` bars from `first` as (16th from `first`, key)."""
    return [(tick_step(KIT, n[0] - lo(first)), n[2]) for n in notes(variant, track, first, first + bars - 1)]


def lines(variant='vox'):
    return [ln for s in mix(variant)[0]['sections'] for ln in s['lines']]


def sound_key(name):
    """The key a sound plays on, on channel 6 or 7."""
    return Mi.kit_of(KIT).sound(name)[1]


def drum_bar(variant, bar):
    """A bar's drums: (key, 16th, velocity) of every drum note starting in it."""
    return tuple(sorted((n[2], tick_step(KIT, n[0] - lo(bar)), n[3]) for d in DRUMS for n in notes(variant, d, bar)))


def bar_sig(variant, bar, velocities=True):
    """What a bar plays: (track, tick in the bar, length, key[, velocity]) of every note starting in it."""
    rows = []
    for name, ns in bars_of(variant).get(bar, {}).items():
        rows += [(name, n[0] - lo(bar), n[1] - n[0], n[2]) + ((n[3],) if velocities else ()) for n in ns]
    return tuple(sorted(rows))


class TestExtendedMix(unittest.TestCase):
    """FUT01's own form (PARTS['form']) against audit B's FUT01 must-fixes, on the exported mixes."""

    def test_the_form(self):
        """FUT01's UK garage form (audit B's, with drop 2's strip as its fifth line so the outro is the last 32 bars):
        intro 1-32, the groove on Low 33-64, break 65-80, an 8-bar build, the drop on Bounce 89-120, the 4x4 switch-up
        121-136, break 2 with the organ riff 137-152, drop 2 153-192, the outro 193-224."""
        for v in VARIANTS:
            R = mix(v)[0]
            self.assertEqual(list(FORM), [(s['name'], s['kind'], s['start_bar'], s['bars']) for s in R['sections']])

    def test_drums_alone_until_the_bass_comes_in_on_33_and_after_it_leaves_with_192(self):
        """Audit must-fix 6: 32 bars of drums at each end; the bass's first note in bar 33, its last in bar 192."""
        for v in VARIANTS:
            t = mix(v)[1]
            music = [n for name, ns in t.items() if name not in DRUMS for n in ns]
            bass = sorted(n for name in BASSES for n in t[name])
            with self.subTest(variant=v):
                self.assertEqual(33, min(n[0] for n in music) // BAR + 1)
                self.assertEqual(33, bass[0][0] // BAR + 1)
                self.assertLessEqual(max(n[1] for n in music), lo(193))
                self.assertEqual(192, bass[-1][0] // BAR + 1)

    def test_the_groove_plays_low_on_the_organ_bass(self):
        """Audit must-fix 1 and 7: the groove (33-64) is the Low line on the Organ Bass (channel 11), every 2 bars; the
        warp bass waits for the drop. The two basses never play in the same bar."""
        low = [(s, n) for s, _, n, _, _ in C.ORGAN_LINE]
        for v in VARIANTS:
            with self.subTest(variant=v):
                self.assertFalse(notes(v, 'Bass', 33, 64))
                for bar in range(33, 65, 2):
                    self.assertEqual(low, phrase(v, 'Organ Bass', bar, 2), bar)
                both = [b for b, got in bars_of(v).items() if got.get('Bass') and got.get('Organ Bass')]
                self.assertFalse(both)

    def test_the_drops_are_drops(self):
        """Audit headline 1, must-fix 1: the drop's first line (89-96) against the groove's last (57-64), and drop 2's
        (153-160) after break 2: the warp bass's Bounce line (channel 1) every 2 bars where the groove played Low on the
        Organ Bass, the accents in both 4-bar blocks, the kick, the sub drop and the alloy bell on the first beat; in
        the vox mix the whole riff (all 8 notes every 2 bars) from the drop's downbeat."""
        bounce = [(s, n) for s, _, n, _, _ in C.WARP_LINE]
        riff = [(s, n) for s, n, _, _, _ in V.RIFF_NOTES]
        for v in VARIANTS:
            with self.subTest(variant=v):
                self.assertTrue(notes(v, 'Organ Bass', 57, 64))
                for first in (89, 153):
                    self.assertFalse(notes(v, 'Organ Bass', first, first + 7))
                    for bar in range(first, first + 8, 2):
                        self.assertEqual(bounce, phrase(v, 'Bass', bar, 2), bar)
                    for block in (first, first + 4):
                        self.assertTrue(notes(v, 'Accents', block, block + 3), block)
                    self.assertIn((lo(first), sound_key('SubDrop')), {n[:3:2] for n in notes(v, 'FX', first)})
                    self.assertIn((lo(first), sound_key('AlloyBell')), {n[:3:2] for n in notes(v, 'Accents', first)})
                    self.assertIn(lo(first), {n[0] for n in notes(v, 'Kick', first)})
                if v == 'vox':
                    for bar in range(89, 97, 2):
                        self.assertEqual(riff, phrase(v, 'Vox', bar, 2), bar)

    def test_the_switch_up_is_4x4_speed_garage_on_speed(self):
        """Audit must-fix 1: the switch-up (121-136): context4's KICK_4X4 on every beat (1.0, 0.9: velocities 127 and
        115) and CLAP_4X4 on 2 and 4, through the turnaround bar too; the Speed line on the warp bass every 2 bars; the
        4x4 bars' own fill bar (136) leaves beat 4 to the glitch fill. The Speed line plays nowhere else: written for
        the 4x4 kick, its 16th-10 F#2 would land on the 2-step kick's 'and' of 3."""
        speed = [(s, n) for s, _, n, _, _ in C.SPEED_LINE]
        four = [(0, 127), (4, 115), (8, 127), (12, 115)]
        for v in VARIANTS:
            with self.subTest(variant=v):
                for bar in range(121, 136):
                    kicks = [(tick_step(KIT, n[0] - lo(bar)), n[3]) for n in notes(v, 'Kick', bar)]
                    self.assertEqual(four, kicks, bar)
                    self.assertLessEqual({4, 12}, set(steps(v, CLAP, bar)), bar)
                    if bar != 128:
                        self.assertEqual([4, 12], steps(v, CLAP, bar), bar)
                kicks = [(tick_step(KIT, n[0] - lo(136)), n[3]) for n in notes(v, 'Kick', 136)]
                self.assertEqual(four[:3], kicks)
                self.assertFalse([n for d in DRUMS for n in notes(v, d, 136) if n[0] >= lo(136) + at(KIT, 12)])
                for bar in range(121, 137, 2):
                    self.assertEqual(speed, phrase(v, 'Bass', bar, 2), bar)
                elsewhere = [b for b in range(33, 193, 2) if not 121 <= b < 137 and phrase(v, 'Bass', b, 2) == speed]
                self.assertFalse(elsewhere)

    def test_the_build_is_a_uk_garage_build(self):
        """Audit must-fix 2, the UK garage build (81-88): the 2-step kick, the off-beat open hats and the tambourine
        on every 16th running on through bar 87, the bass out; the clap in 8ths in bar 87 and 16ths on beats 1-2 of
        bar 88, rising hit by hit; the whole glitch fill (half a bar) from beat 3 of bar 88 to the drop, every drum
        out from its start; the 8-bar riser ending on the drop, where the kick and the sub drop land."""
        for v in VARIANTS:
            with self.subTest(variant=v):
                self.assertFalse([n for b in BASSES for n in notes(v, b, 81, 88)])
                for bar in range(81, 88):
                    self.assertLessEqual({0, 10}, set(steps(v, KICK, bar)), bar)
                    self.assertFalse({4, 8, 12} & set(steps(v, KICK, bar)), bar)
                    self.assertEqual([2, 6, 10, 14], steps(v, OPEN, bar), bar)
                    self.assertEqual(list(range(16)), steps(v, TAMB, bar), bar)
                self.assertEqual(list(range(0, 16, 2)), steps(v, CLAP, 87))
                self.assertEqual(list(range(8)), steps(v, CLAP, 88))
                vels = [n[3] for n in notes(v, 'Clap', 87, 88)]
                self.assertEqual(sorted(vels), vels)
                self.assertGreater(vels[-1], vels[0])
                beat3 = lo(88) + 2 * 960
                fx = {(n[0], n[1], n[2]) for n in notes(v, 'FX', 81, 88)}
                self.assertIn((beat3, lo(89), sound_key('GlitchFillLift')), fx)
                self.assertIn((lo(81), lo(89), sound_key('ShiftRiser')), fx)
                self.assertFalse([n for d in DRUMS for n in notes(v, d, 88) if n[0] >= beat3])
                self.assertIn(lo(89), {n[0] for n in notes(v, 'Kick', 89)})
                self.assertIn((lo(89), sound_key('SubDrop')), {n[:3:2] for n in notes(v, 'FX', 89)})

    def test_the_air_pad_holds_both_breaks(self):
        """Audit must-fix 4: the Air Pad (channel 13) under both breaks, a chord sounding through every bar's downbeat,
        F#m9 on F#3 (54) or Bm9 on B3 (59): in break 1 a new chord every bar (F#m9 | Bm9, as the stabs); in break 2
        each chord held for its 2 bars as one note (review #4: a same-key restart every bar would make a plain sampler
        fade the pad in again mid-chord); nowhere else (no pad under a bass)."""
        for v in VARIANTS:
            pad = mix(v)[1].get('Air Pad', [])
            with self.subTest(variant=v):
                for bar in list(range(65, 81)) + list(range(137, 153)):
                    held = [n for n in pad if n[0] <= lo(bar) < n[1]]
                    self.assertEqual(1, len(held), bar)
                    self.assertIn(held[0][2], (54, 59))
                self.assertEqual([(lo(b), (54, 59)[(b - 65) % 2], BAR) for b in range(65, 81)],
                                 [(n[0], n[2], n[1] - n[0]) for n in pad if n[0] < lo(81)])
                self.assertEqual([(lo(b), (54, 59)[(b - 137) // 2 % 2], 2 * BAR) for b in range(137, 153, 2)],
                                 [(n[0], n[2], n[1] - n[0]) for n in pad if n[0] >= lo(137)])
                self.assertFalse([n for n in pad if not (lo(65) <= n[0] < lo(81) or lo(137) <= n[0] < lo(153))])

    def test_drop_2_takes_the_organ_riff_as_its_new_top_line(self):
        """Audit must-fix 1: break 2 brings in the 4-bar organ riff (channel 12) over the pad, and drop 2 opens on it as
        its top line over Bounce, the vox resting (review #3: the riff never rests, so a vox over it stacks two leads in
        one octave rather than answering); the vox comes back at 161, where the organ riff is out. The organ riff plays
        nowhere before break 2, and never with the vox."""
        organ = [(s, n) for s, n, _, _, _ in Rf.ORGAN_RIFF]
        for v in VARIANTS:
            with self.subTest(variant=v):
                self.assertFalse(notes(v, 'Organ Lead', 1, 136))
                for first in (137, 153):
                    self.assertEqual(organ, phrase(v, 'Organ Lead', first, 4), first)
                self.assertTrue(notes(v, 'Bass', 153, 160))
                self.assertFalse(notes(v, 'Vox', 153, 160))
                self.assertEqual(v == 'vox', bool(notes(v, 'Vox', 161, 168)))
                t = mix(v)[1]
                self.assertFalse([a for a in t.get('Organ Lead', []) for b in t.get('Vox', [])
                                  if a[0] < b[1] and b[0] < a[1]])

    def test_turnarounds_end_every_line_that_does_not_end_on_a_fill(self):
        """Audit must-fix 3: every line with drums ends on a fill, a turnaround or (once, into the switch-up) the drums
        dropping out; the default turnaround is the audit's: a kick on the 'a' of 4 at 0.6, the open hat on the 'and'
        of 4 at 1.0 left ringing (no closed hat after it), the tambourine out on beat 4, a rim on 16th 14; and the
        turnarounds aren't one bar repeated (four or more different last bars)."""
        last_bars = set()
        for ln in lines():
            last = ln['start_bar'] + 7
            if not [n for d in DRUMS for n in notes('vox', d, ln['start_bar'], last)]:
                continue
            with self.subTest(line=ln['start_bar']):
                self.assertIn(ln.get('end'), ('fill', 'turn', 'drop'))
                if ln.get('end') != 'turn':
                    continue
                last_bars.add(drum_bar('vox', last))
                if not ln.get('turn'):
                    self.assertEqual(76, at_16th('vox', 'Kick', last, 15, KICK))
                    self.assertEqual(127, at_16th('vox', 'Hats', last, 14, OPEN))
                    self.assertFalse([s for s in steps('vox', HAT, last) if s > 14])
                    self.assertFalse([s for s in steps('vox', TAMB, last) if s >= 12])
                    self.assertIn(14, steps('vox', RIM, last))
        self.assertEqual(1, [ln.get('end') for ln in lines()].count('drop'))
        self.assertGreaterEqual(len(last_bars), 4)

    def test_a_percussion_element_moves_every_16_bars(self):
        """Audit must-fix 3: from one 16-bar block to the next (both with drums), at least one drum element (the kick,
        the clap, the closed or the open hat, the rim, the alloy hit, the tambourine) changes its pattern in the lines'
        bodies, their first 7 bars (the turnarounds and fills aside)."""
        def elements(v, first):
            out = {}
            for bar in range(first, first + 16):
                if (bar - 1) % 8 != 7:
                    for key in TRACK_OF:
                        out.setdefault(key, set()).add(tuple(steps(v, key, bar)))
            return out
        for v in VARIANTS:
            blocks = [(first, elements(v, first)) for first in range(1, 225, 16)]
            blocks = [(first, e) for first, e in blocks if any(any(p) for p in e.values())]
            for (_, a), (first, b) in zip(blocks, blocks[1:]):
                with self.subTest(variant=v, bars=first):
                    self.assertNotEqual(a, b)

    def test_busy_hats_add_closed_hats_you_hear(self):
        """Audit must-fix 3: FUT01's busy hats add the closed hat on the beats' 16ths (0, 4, 8, 12) at 0.4 or more
        (velocity 51: the 0.4 layer, as loud as the softest written hat), four more a bar; four or more busy lines."""
        busy = [ln for ln in lines() if ln.get('hats') == 'busy']
        self.assertGreaterEqual(len(busy), 4)
        for ln in busy:
            for bar in range(ln['start_bar'], ln['start_bar'] + 7):
                with self.subTest(bar=bar):
                    for s in (0, 4, 8, 12):
                        self.assertGreaterEqual(at_16th('vox', 'Hats', bar, s, HAT) or 0, 51, s)

    def test_the_pluck_plays_around_the_vox(self):
        """Audit FUT01 NICE 8, a must in the brief: in every 2 bars where the vox plays, the pluck plays one bar at
        most (its call, or its answer to the vox's call) and never in the bar of the vox's second bar (the pluck's
        C#5 against the vox's D5); one lead line at most under the vox (the organ riff never with it, review #3); no
        lead sounds a minor second from a vox note."""
        t = mix('vox')[1]
        for first in range(33, 193, 2):
            vox = notes('vox', 'Vox', first, first + 1)
            if not vox:
                continue
            with self.subTest(bars=first):
                pluck = {n[0] // BAR + 1 for n in notes('vox', 'Lead', first, first + 1)}
                self.assertLessEqual(len(pluck), 1)
                self.assertFalse(notes('vox', 'Organ Lead', first, first + 1))
                if notes('vox', 'Vox', first + 1):
                    self.assertNotIn(first + 1, pluck)
        rubs = [(a[0] // BAR + 1, name, a[2], b[2]) for name in LEADS for a in t.get(name, []) for b in t['Vox']
                if a[0] < b[1] and b[0] < a[1] and abs(a[2] - b[2]) == 1]
        self.assertFalse(rubs)

    def test_the_vox_sits_on_the_harmony_under_it(self):
        """Every vox note is a tone of the chord sounding under it: the Air Pad's held chord, else the bar's last organ
        stab at or before it (F#m9 on 54, Bm9 on 59, both m9 shapes); no vox note without harmony under it."""
        t = mix('vox')[1]
        for on, off, key, vel in t['Vox']:
            with self.subTest(bar=on // BAR + 1, note=key):
                pad = [n[2] for n in t.get('Air Pad', []) if n[0] <= on < n[1]]
                stabs = [n for n in t['Chords'] if n[0] <= on and n[0] // BAR == on // BAR]
                root = pad[0] if pad else max(stabs)[2] if stabs else None
                self.assertIsNotNone(root)
                self.assertIn(key % 12, {(root + d) % 12 for d in M9})

    def test_the_vox_riff_is_placed_with_intent(self):
        """Audit must-fix 5: the vox riff as notes, line by line: the answer to the pluck's call closing the groove, the
        whole riff over the pad in the break, its call over the build's riser, the whole riff from the drop's
        downbeat, the answer again into the switch-up, the whole riff back over drop 2's warp riff once the organ riff
        has had drop 2's first line to itself, the call to the pluck's answer in its last full line; nothing in the
        intro, the switch-up, break 2, drop 2's first line, the strip or the outro, and nothing at all in the no-vox
        mix (where the same lines play)."""
        got = {ln['start_bar']: ln['vox'] for ln in lines() if ln.get('vox')}
        self.assertEqual(VOX, got)
        for ln in lines():
            with self.subTest(line=ln['start_bar']):
                first = ln['start_bar']
                self.assertEqual(first in VOX, bool(notes('vox', 'Vox', first, first + 7)))
        self.assertNotIn('Vox', mix('novox')[1])

    def test_the_accents_are_notes_on_their_cues(self):
        """Audit must-fix 5, its accent rules: the sub drop and the alloy bell on the drop's, the switch-up's and drop
        2's downbeats; the chord sweep on both breaks' downbeats; the vox synth stab on 16th 6 of the switch-up's bars
        2 and 6 of each line, a rave stab on the 'and' of 4 into its second line; the reverse stab swelling into the
        pluck's entry (49) and into the switch-up (121), the uplifter rising into the break (65) and into the build
        (81), the organ hit on the 'and' of 4 of bar 56 (review #37); the 8-bar riser ending on the drop and on drop
        2; a Fill cue's fill ending on the next downbeat; none of it in the first or last 32 bars."""
        for v in VARIANTS:
            t = mix(v)[1]
            acc = {n[:3:2] for n in t['Accents']}
            fx = {n[:3:2] for n in t['FX']}
            with self.subTest(variant=v):
                for bar in (89, 121, 153):
                    self.assertIn((lo(bar), sound_key('SubDrop')), fx, bar)
                    self.assertIn((lo(bar), sound_key('AlloyBell')), acc, bar)
                for bar in (65, 137):
                    self.assertIn((lo(bar), sound_key('ChordSweep')), acc, bar)
                for bar in (122, 126, 130, 134):
                    self.assertIn((lo(bar) + at(KIT, 6), sound_key('VoxSynthStab')), acc, bar)
                self.assertIn((lo(128) + at(KIT, 14), sound_key('RaveStab')), acc)
                self.assertIn((lo(56) + at(KIT, 14), sound_key('OrganHit')), acc)
                acc_ends = {(n[1], n[2]) for n in t['Accents']}
                for bar in (49, 121):
                    self.assertIn((lo(bar), sound_key('ReverseStab')), acc_ends, bar)
                for bar in (65, 81):
                    self.assertIn((lo(bar), sound_key('NoiseUplifter')), acc_ends, bar)
                ends = {(n[1], n[2]) for n in t['FX']}
                for bar in (89, 153):
                    self.assertIn((lo(bar), sound_key('ShiftRiser')), ends, bar)
                for s in mix(v)[0]['sections']:
                    for c in s['cues']:
                        if c['cue'] == 'Fill':
                            self.assertTrue([n for n in t['FX'] if n[1] == lo(c['bar'] + 1) and n[0] >= lo(c['bar'])])
                early = [n for name in ('Accents', 'FX') for n in t[name] if n[0] < lo(33) or n[1] > lo(193)]
                self.assertFalse(early)

    def test_the_fill_text_is_true(self):
        """Audit FUT01 NICE 11: the README's word on the fill matches the notes: the loop's fill bar clears only beat 4
        (its drums play beat 3 and stop at 16th 60, where the glitch fill plays from its own beat 2), and the build's
        whole fill plays from beat 3, every drum out from there (test_the_build_is_a_uk_garage_build)."""
        text = kit_files(KIT)['README.md'].decode()
        self.assertIn("The loop's fill bar clears only beat 4", text)
        self.assertIn('the whole fill from beat 3', text)
        got = loop(KIT, 'vox')
        drums = [n[0] for d in DRUMS for n in got[d]['notes']]
        self.assertTrue([on for on in drums if at(KIT, 56) <= on < at(KIT, 60)])
        self.assertLess(max(drums), at(KIT, 60))
        self.assertEqual([at(KIT, 60)], [n[0] for n in got['FX']['notes']])

    def test_no_bass_note_lands_on_a_kick(self):
        """Review #2: every bassline and bass phrase sits off the kick, as the warp line bounces in the 2-step kick's
        gaps: no note on the Bass or the Organ Bass starts on a kick's tick, in either mix. The warp riff (written with
        no kick) rolls alone under drop 2's second line, the kick out until 169."""
        for v in VARIANTS:
            t = mix(v)[1]
            kicks = {n[0] for n in t['Kick']}
            with self.subTest(variant=v):
                self.assertFalse([(n[0] // BAR + 1, n[2]) for name in BASSES for n in t[name] if n[0] in kicks])
                self.assertFalse(notes(v, 'Kick', 161, 168))
                self.assertTrue(notes(v, 'Bass', 161, 168))
                self.assertTrue(notes(v, 'Kick', 169))

    def test_the_chord_voicing_follows_the_chords(self):
        """Review #1 and #45: Chord voicing (channel 3, the chords for your own synth) plays wherever Chords plays,
        drop 2's stab rhythm included (each stab's chord as the m9 shape over its root), and swells with it through
        the build (rise on 'chords')."""
        for v in VARIANTS:
            got = bars_of(v)
            with self.subTest(variant=v):
                chords = {b for b, tr in got.items() if tr.get('Chords')}
                voicing = {b for b, tr in got.items() if tr.get('Chord voicing')}
                self.assertEqual(chords, voicing)
                for on, _, root, _ in notes(v, 'Chords', 169, 176):
                    shape = {n[2] for n in notes(v, 'Chord voicing', 169, 176) if n[0] == on}
                    self.assertEqual({root + d for d in Ky.SHAPE}, shape, on)
                means = [sum(n[3] for n in notes(v, 'Chord voicing', b)) / len(notes(v, 'Chord voicing', b))
                         for b in range(81, 88)]
                self.assertEqual(sorted(means), means)
                self.assertLess(means[0], means[-1])

    def test_no_vox_warp_stabs_replace_the_chord_stab(self):
        """Review #5: in the no-vox mix the warp stab takes the chord stab's place (context5.STAB_DROP): no Chords
        note starts on a warp stab's tick, the e-piano stab rhythm in 169-176 included."""
        t = mix('novox')[1]
        warp = {n[0] for n in t['Accents'] if n[2] == sound_key('WarpStab')}
        self.assertTrue(warp)
        self.assertFalse([n[0] // BAR + 1 for n in t['Chords'] if n[0] in warp])

    def test_the_readme_says_where_the_stab_rhythm_plays(self):
        """Review #23: drop 2's e-piano stab rhythm is a channel-4 phrase, so Wubansia plays it on the Stab A/B
        switch's sample, the Organ Stab by default; the kit README says so."""
        text = ' '.join(kit_files(KIT)['README.md'].decode().split())
        self.assertIn('plays on the Stab A/B: the Organ Stab by default, the E Piano Stab on B', text)

    def test_something_changes_at_every_line(self):
        """The brief: no line repeats the line before it, not even in its first 7 bars (the change lands on the new
        line's downbeat, not only in a turnaround), and 24 or more of the 28 lines are distinct, in both mixes."""
        for v in VARIANTS:
            sigs = [bar_sig(v, b) for b in range(1, 225)]
            whole_lines = [tuple(sigs[i:i + 8]) for i in range(0, 224, 8)]
            bodies = [tuple(sigs[i:i + 7]) for i in range(0, 224, 8)]
            with self.subTest(variant=v):
                self.assertFalse([8 * i + 1 for i in range(1, 28) if bodies[i] == bodies[i - 1]])
                self.assertGreaterEqual(len(set(whole_lines)), 24)

    def test_the_outro_is_not_the_intro_reversed(self):
        """Audit cross-kit issue 13: no outro line plays an intro line again (not even in its first 7 bars), and the
        outro doesn't take the intro's lanes away in the order the intro brought them in; the last 8 bars are the kick,
        the hats and the tambourine, not a solo kick."""
        for v in VARIANTS:
            body = lambda first: tuple(bar_sig(v, b) for b in range(first, first + 7))  # noqa: E731
            with self.subTest(variant=v):
                self.assertFalse({body(b) for b in (1, 9, 17, 25)} & {body(b) for b in (193, 201, 209, 217)})
                R = mix(v)[0]
                intro, outro = R['sections'][0]['lines'], R['sections'][-1]['lines']
                self.assertNotEqual([(ln['lanes'], ln.get('hats')) for ln in intro],
                                    [(ln['lanes'], ln.get('hats')) for ln in reversed(outro)])
                for bar in range(217, 225):
                    self.assertTrue(steps(v, KICK, bar) and steps(v, HAT, bar) and steps(v, TAMB, bar), bar)

    def test_something_structural_every_16_bars(self):
        """The brief: every 16 bars something structural: a lane in or out, the bassline, a chord block, a phrase, a
        drum variant. Some track's notes (16ths and keys, velocities aside) change in the lines' bodies from one
        16-bar block to the next, in both mixes."""
        def block(v, first):
            return frozenset(bar_sig(v, b, velocities=False) for b in range(first, first + 16) if (b - 1) % 8 != 7)
        for v in VARIANTS:
            blocks = [block(v, first) for first in range(1, 225, 16)]
            with self.subTest(variant=v):
                self.assertFalse([16 * i + 1 for i in range(1, 14) if blocks[i] == blocks[i - 1]])


if __name__ == '__main__':
    unittest.main()
