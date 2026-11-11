"""FUT03's MIDI (tools/sound/recipes/fut03/midi.py), on top of test_midi.py's kit-agnostic properties: its Loop files
lane by lane against its round 5 context loop (context5.py on context.py), its roots, its phrases' channels, the pad on
channel 11, the dub stab's ride, and its written fill bar; and its own extended mix (PARTS['form']) on the exported
files: audit B's FUT03 must-fixes (the fill bar, the vox riff's Cm9, the repetition broken up, a drop that is a drop,
break 2's downbeat, a build that breathes, 32 bars of drums at each end) and the musical bar (something changes at every
line, the harmony moves under the vox and never clashes with it, the lead thins where the vox plays). FUT03's own
tests: its engineer edits them with the kit's PARTS."""
import functools
import unittest

from tools.sound import channels as CH
from tools.sound.recipes.fut03 import accents as A, context as C, context5 as C5, rubber as B, voxsynth as VS
from tools.sound.recipes.fut03 import midi as P3
from tools.sound.tests.midi_testlib import (BAR, VARIANTS, LoopCase, at, chord_roots, drum, kit_files, lane_ons, loop,
                                            one_shot, parse, phrase_channels, ride, tick_step, value_at, whole)

KIT = 'FUT03'


class TestLoop(LoopCase, unittest.TestCase):
    KIT = KIT

    def expected(self, variant):
        a = lambda s: at(KIT, s)  # noqa: E731
        bass = []
        for r in (0, 1):
            for s, ln, n, v, glide in B.LINES[B.PICK]:
                bass.append((a(s + 32 * r), n, v, None))
                if glide:                   # a glide inside the note: its target slid into from the glide's start
                    bass.append((at(KIT, s + 32 * r + ln * glide[1]), glide[0], v, None))
        out = {
            'Kick': drum(KIT, C.KICK, 36),
            'Clap': drum(KIT, C.CLAP, 38),
            'Hats': drum(KIT, [(s, v) for s, kind, v in C.HATS if kind == 'closed'], 42)
            + drum(KIT, [(s, v) for s, kind, v in C.HATS if kind == 'open'], 46),
            'Perc': drum(KIT, C.SHAKER, 54) + [(a(s), 37 if n == 'Rimshot' else P3.HAND[(n, z)], v, a(s) + 120)
                                               for s, n, z, v in C.PERC],
            'Bass': bass,
            'Chords': [(a(s), 55, v, a(s) + 360) for s, v in C.STAB],
            'Chord voicing': [(a(s), n, v, a(s) + 360) for s, v in C.STAB for n in A.GM9],
            'Pad Warm': [(0, 55, 1.0, 64 * 240)],
            'Accents': [one_shot(KIT, 'accents', name, 64 if step == 'end' else step, 1,
                                 'end' if step == 'end' else 'on') for step, name in C5.ACCENTS[variant]],
        }
        if variant == 'vox':
            out['Vox'] = [(a(s + C5.VOX_AT), m, v, None) for s, m, _, v in VS.VOX_RIFF]
        return out


class TestParts(unittest.TestCase):
    def test_the_dub_stab_at_its_root(self):
        """The dub stab on G3 (55): Gm9, on channel 4; its voicing on 3; the dub chords phrase plays Cm9 on C4 (60),
        the same file a fourth up."""
        self.assertEqual(({55}, {4}, {3}), chord_roots(KIT))
        phrase = parse(kit_files(KIT)['FUT03_Phrase_DubChords_4bars.mid'])[2][1]['notes']
        self.assertEqual({55, 60}, {key for *_, key, _ in phrase})

    def test_the_phrases_channels(self):
        self.assertEqual({'RubberRiff': 1, 'DubChords': 4, 'KeysArp': 2, 'VoxSynthRiff': 5}, phrase_channels(KIT))

    def test_the_pad_plays_on_channel_11_and_no_loop_lead(self):
        tracks = loop(KIT, 'vox')
        self.assertEqual({11}, {ch for _, _, ch, _, _ in tracks['Pad Warm']['notes']})
        self.assertNotIn('Lead', tracks)

    def test_no_throws(self):
        """FUT03's riff throw is baked into its stem (the riff's last note): no CC91/CC94 in the loops or the extended
        mixes, whose riffs and answers end on that note."""
        for v in ('vox', 'novox'):
            self.assertFalse([c for t in loop(KIT, v).values() for c in t['ccs'] if c[2] in (91, 94)])
            _, tracks, _ = whole(KIT, v)
            self.assertFalse([c for t in tracks.values() for c in t['ccs'] if c[2] in (91, 94)], v)

    def test_the_readme_names_the_vox_stems_the_plugin_slices(self):
        """Review #28: the README's recorded lanes name the stems the plugin's vox tables read (P2's derived
        Stems/Context Vox Riff and its Twice version), not the Loops/ phrase, which wraps its echoes."""
        text = kit_files(KIT)['README.md'].decode()
        self.assertIn('`Stems/Context Vox Riff.wav`', text)
        self.assertIn('`Stems/Context Vox Riff Twice.wav`', text)
        self.assertNotIn('Loops/Phrase Vox Riff.wav', text)

    def test_the_fill_bar_leaves_the_build_fill_its_room(self):
        """Audit A defect 7: at every Fill cue the drums play the kit's FillPreview bar (context.FILL_*): the kick,
        hats and shaker on beats 1-3, the clap on beat 2, no congas, bongos or rim, beat 4 empty, under the Build
        Fill."""
        R, tracks, _ = whole(KIT, 'vox')
        fills = [c['bar'] for s in R['sections'] for c in s['cues'] if c['cue'] == 'Fill']
        self.assertTrue(fills)
        for bar in fills:
            lo = (bar - 1) * BAR
            with self.subTest(bar=bar):
                drums = [(tick_step(KIT, on - lo), key) for lane in CH.DRUM_LANES
                         for on, key, _ in lane_ons(KIT, tracks, lane, lo, lo + BAR)]
                self.assertFalse([d for d in drums if d[0] >= 12])
                self.assertFalse([d for d in drums if d[1] in (37, 41, 43, 45, 47, 48)])
                self.assertLessEqual({s for s, key in drums if key == 38}, {4})


class TestRides(unittest.TestCase):
    def test_the_dub_stab_ride(self):
        """The dub stab's low-pass (context.stab_lane) on CC74, channel 4, 900-5000 Hz: closed on the loop's downbeat,
        open at its middle; no CC71 (its Q is fixed)."""
        r = ride(KIT, 'Chords', 74)
        self.assertEqual(0, r[0])
        self.assertEqual(127, value_at(r, 32 * 240))
        self.assertLess(value_at(r, 16 * 240), 127)
        self.assertFalse(ride(KIT, 'Chords', 71))


# ------------------------------------------------------------------ the extended mix: FUT03's own form
DRUMS = ('Kick', 'Clap', 'Hats', 'Perc')
STRUCTURAL = ('Kick', 'Clap', 'Perc', 'Bass', 'Lead', 'Chords', 'Pad Warm', 'Vox')   # the hats' modes are small moves
TONES = {55: {n % 12 for n in A.GM9}, 60: {n % 12 for n in A.CM9}}   # the stab and the pad: Gm9 on G3, Cm9 on C4
RING = 5 * 240                      # the dub stab rings about 5 16ths (gone by 0.6 s)
OPEN_HAT, RIM, BONGO_D5, CONGAS = 46, 37, 48, (41, 43, 45)
BUILD_FILL, RISER, IMPACT = 48, 49, 50                      # channel 7's keys
CONGA_RUN = {(12, 45), (13, 43), (14, 41), (15, 41)}        # D4 C4 G3 G3 on 16ths 12-15
GLASS_PING, DUB_CM9, REVERSE_GM9, SWEEP = 48, 49, 50, 52     # channel 6's keys (PARTS['accents'] from 48)
# the chord accents' tones: the dub chord Cm9, the reverse Gm9, the organ Gm7, the pluck Dm7, the vox synth Gm
ACCENT_TONES = {49: TONES[60], 50: TONES[55], 51: {7, 10, 2, 5}, 53: {2, 5, 9, 0}, 55: {7, 10, 2}}
RIFF = sorted((at(KIT, s), m) for s, m, *_ in VS.VOX_RIFF)  # the riff's 2 bars as (tick, key)


@functools.lru_cache(maxsize=None)
def mix(variant):
    """The whole mix in a variant: (regions.json, {track: notes (on, off, channel, key, velocity)}, {track: ccs})."""
    R, tracks, _ = whole(KIT, variant)
    return R, {n: t['notes'] for n, t in tracks.items()}, {n: t['ccs'] for n, t in tracks.items()}


def form_lines(R):
    """Every line of the mix: (its section, its regions.json entry, its 1-based place in the section)."""
    return [(s, ln, i) for s in R['sections'] for i, ln in enumerate(s['lines'], 1)]


def section(R, kind):
    return next(s for s in R['sections'] if s['kind'] == kind)


def in_bar(notes, bar):
    """A track's notes starting in a bar (1-based) as (16th, key, velocity)."""
    lo = (bar - 1) * BAR
    return sorted((tick_step(KIT, on - lo), key, vel) for on, _, _, key, vel in notes if lo <= on < lo + BAR)


def in_span(notes, bar, bars):
    """A track's notes starting in `bars` bars from a bar (1-based)."""
    lo = (bar - 1) * BAR
    return [n for n in notes if lo <= n[0] < lo + bars * BAR]


def harmony(notes, tick):
    """The chords sounding at a tick, by their roots: the stabs triggered in the RING before it (it included) and the
    pad held over it."""
    stab = {key for on, _, _, key, _ in notes.get('Chords', ()) if tick - RING <= on <= tick}
    return stab | {key for on, off, _, key, _ in notes.get('Pad Warm', ()) if on <= tick < off}


def consonant(key, root):
    """A note against a chord: one of its tones, or a tension no semitone or tritone from any of them."""
    return key % 12 in TONES[root] or all((key - t) % 12 not in (1, 6, 11) for t in TONES[root])


def cc74(ccs, tick):
    """The stab's CC74 in force at a tick (the Chords track's last point at or before it)."""
    return value_at({t: v for t, _, cc, v in ccs['Chords'] if cc == 74}, tick)


def bar_sig(notes, bar):
    """Everything a bar plays: (track, tick in the bar, key, velocity, length)."""
    lo = (bar - 1) * BAR
    return tuple(sorted((name, on - lo, key, vel, off - on) for name, ns in notes.items()
                        for on, off, _, key, vel in ns if lo <= on < lo + BAR))


class TestExtendedMix(unittest.TestCase):
    """FUT03's own 224-bar extended mix (PARTS['form']) in both variants, on the exported files: audit B's FUT03
    must-fixes and the musical bar."""

    def test_it_plays_its_own_form(self):
        self.assertTrue(P3.PARTS.get('form'))

    def test_32_bars_of_drums_at_each_end(self):
        """Must-fix 7: the bass from bar 33 to bar 192 (it entered at 17), nothing but the drums before or after."""
        for v in VARIANTS:
            R, notes, _ = mix(v)
            with self.subTest(variant=v):
                self.assertEqual(33, notes['Bass'][0][0] // BAR + 1)
                self.assertEqual(192, max(on for on, *_ in notes['Bass']) // BAR + 1)
                for name, ns in notes.items():
                    if CH.track_channel(KIT, name) != 10:
                        self.assertFalse([n for n in ns if n[0] < 32 * BAR or n[1] > 192 * BAR], name)

    def test_every_fill_bar_is_the_fill_preview_under_the_build_fill(self):
        """Must-fixes 1 and 6: every line that ends on a fill plays the FillPreview's fill bar (context.FILL_*) in the
        lanes it plays: the kick on beats 1-3 wherever the line has the kick, the build's included (the drop breathes
        on beat 4, not for a whole bar); the clap on beat 2 unless the line rolls it (the Build Fill has its own); the
        Build Fill ending on the next downbeat."""
        preview = {(int(n.step) - 16, n.key) for n in P3.PREVIEW if n.step >= 16}
        for v in VARIANTS:
            R, notes, _ = mix(v)
            fills = [ln for _, ln, _ in form_lines(R) if ln.get('end') == 'fill']
            self.assertGreaterEqual(len(fills), 4)
            for ln in fills:
                bar = ln['start_bar'] + 7
                got = {(st, key) for name in DRUMS for st, key, _ in in_bar(notes.get(name, ()), bar)}
                with self.subTest(variant=v, bar=bar):
                    self.assertLessEqual(got, preview)
                    if 'kick' in ln['lanes']:
                        self.assertEqual({0, 4, 8}, {st for st, key in got if key == 36})
                    claps = {st for st, key in got if key == 38}
                    self.assertEqual(set() if ln.get('roll') or 'snare' not in ln['lanes'] else {4}, claps)
                    self.assertEqual(1, len([n for n in notes['FX'] if n[3] == BUILD_FILL and n[1] == bar * BAR]))

    def test_the_vox_sings_chord_tones_and_the_keys_never_clash(self):
        """Must-fix 2: every vox note is a tone of each chord sounding under it, the stab (ringing 5 16ths) and the pad:
        no more E-flat 5 over the Gm9 stab (a minor 9th against its D4, a tritone against its A4). The keys play tones
        or tensions, never a semitone or a tritone off a chord tone (their answer's A4 over Cm9)."""
        R, notes, _ = mix('vox')
        self.assertTrue([on for on, *_ in notes['Vox'] if harmony(notes, on)])
        self.assertEqual([], [(on // BAR + 1, key, root) for on, _, _, key, _ in notes['Vox']
                              for root in harmony(notes, on) if key % 12 not in TONES[root]])
        self.assertEqual([], [(on // BAR + 1, key, root) for on, _, _, key, _ in notes['Lead']
                              for root in harmony(notes, on) if not consonant(key, root)])

    def test_gm9_cm9_bar_by_bar_wherever_the_vox_plays_the_riffs_cm9_bar(self):
        """Must-fix 2: wherever the vox plays the riff's Cm9 bar (E-flat 5 B-flat 4 C5 G4), the stab and the pad play
        Cm9 (C4) in it and Gm9 (G3) in the bar before, the stab's push on that bar's last 'a' anticipating the Cm9."""
        R, notes, _ = mix('vox')
        cm9 = sorted({on // BAR + 1 for on, _, _, key, _ in notes['Vox'] if key == 75})
        self.assertGreaterEqual(len(cm9), 16)
        for bar in cm9:
            here = in_bar(notes['Chords'], bar) + in_bar(notes.get('Pad Warm', ()), bar)
            before = in_bar(notes['Chords'], bar - 1) + in_bar(notes.get('Pad Warm', ()), bar - 1)
            with self.subTest(bar=bar):
                self.assertTrue(here)
                self.assertEqual({60}, {key for _, key, _ in here})
                self.assertEqual({55}, {key for st, key, _ in before if st < 15})

    def test_the_drop_is_a_drop(self):
        """Must-fix 4: the drop's first line against the groove's last, in what is heard first: the impact on its
        downbeat, harmony that moves (Gm9 | Cm9 where the groove holds Gm9), the vox's whole riff (both its bars, every
        2 bars) where the groove had its call, the accents in both blocks. Both drops open on an impact after an 8-bar
        riser ending on their downbeat (review #34, #36); every drop line has accents."""
        for v in VARIANTS:
            R, notes, _ = mix(v)
            g, d, d2 = section(R, 'groove'), section(R, 'drop'), section(R, 'drop2')
            last = g['lines'][-1]['start_bar']
            roots = lambda bar: {n[3] for n in in_span(notes['Chords'], bar, 8)}  # noqa: E731
            with self.subTest(variant=v):
                self.assertEqual({55}, roots(last))
                self.assertEqual({55, 60}, roots(d['start_bar']))
                for sec in (d, d2):
                    lo = (sec['start_bar'] - 1) * BAR
                    self.assertTrue([n for n in notes['FX'] if n[3] == IMPACT and n[0] == lo], sec['name'])
                    self.assertEqual([(lo - 8 * BAR, lo)], [(n[0], n[1]) for n in notes['FX'] if n[3] == RISER
                                                            and lo - 8 * BAR <= n[0] < lo], sec['name'])
                    for blk in (0, 4):
                        self.assertTrue(in_span(notes['Accents'], sec['start_bar'] + blk, 4), (sec['name'], blk))
                    for ln in sec['lines']:
                        self.assertTrue(in_span(notes['Accents'], ln['start_bar'], 8), ln['start_bar'])
                if v == 'vox':
                    vox = lambda bar: {n[3] for n in in_span(notes['Vox'], bar, 8)}  # noqa: E731
                    self.assertTrue(vox(last))
                    self.assertNotIn(75, vox(last))
                    for bar in range(d['start_bar'], d['start_bar'] + 8, 2):     # the whole riff, call and answer
                        lo = (bar - 1) * BAR
                        self.assertEqual(RIFF, sorted((n[0] - lo, n[3]) for n in in_span(notes['Vox'], bar, 2)), bar)

    def test_the_switch_up_rides_the_rubber_riff_and_drop_2_the_deep_line_and_the_keys_arp(self):
        """Must-fix 4: the switch-up's bassline is the Rubber riff (the phrase's own notes, bar by bar; it rests under
        the fill), drop 2's the Deep line (both differ from the drop's Rubber line); the keys arp, its 4 bars, plays in
        drop 2."""
        R, notes, _ = mix('novox')
        rows = lambda line: sorted((at(KIT, s), key) for s, key, *_ in line)  # noqa: E731
        for kind, want in (('switch', rows(P3.glided(B.PHRASES['Riff']))), ('drop2', rows(P3.glided(B.LINES['Deep']))),
                           ('drop', rows(P3.glided(B.LINES['Rubber'])))):
            sec = section(R, kind)
            fills = {ln['start_bar'] + 7 for ln in sec['lines'] if ln.get('end') == 'fill'}
            for b in range(sec['bars']):
                lo = (sec['start_bar'] - 1 + b) * BAR
                half = (b % 2) * BAR
                with self.subTest(section=sec['name'], bar=sec['start_bar'] + b):
                    got = sorted((on - lo, key) for on, _, _, key, _ in notes['Bass'] if lo <= on < lo + BAR)
                    self.assertEqual([] if sec['start_bar'] + b in fills else
                                     [(t - half, key) for t, key in want if half <= t < half + BAR], got)
        arp = sorted((at(KIT, s), key) for s, key, _, _ in A.ARP)
        d2 = section(R, 'drop2')
        blocks = [sorted((on - lo, key) for on, _, _, key, _ in notes['Lead'] if lo <= on < lo + 4 * BAR)
                  for lo in range((d2['start_bar'] - 1) * BAR, (d2['start_bar'] - 1 + d2['bars']) * BAR, 4 * BAR)]
        self.assertGreaterEqual(blocks.count(arp), 2)

    def test_pad_warm_under_both_breaks(self):
        """Must-fix 5: Pad Warm under both breaks from their first downbeat (break 2 was the dub stabs alone, all off
        the beat), held through every bar of them."""
        R, notes, _ = mix('novox')
        breaks = [s for s in R['sections'] if s['kind'] == 'break']
        self.assertEqual(2, len(breaks))
        for s in breaks:
            lo = (s['start_bar'] - 1) * BAR
            with self.subTest(section=s['name']):
                self.assertTrue([n for n in notes['Pad Warm'] if n[0] == lo])
                for b in range(s['bars']):
                    t = lo + b * BAR + BAR // 2
                    self.assertTrue([n for n in notes['Pad Warm'] if n[0] <= t < n[1]], s['start_bar'] + b)

    def test_the_build_rolls_three_bars_over_the_kick_then_breathes(self):
        """Must-fix 6: the build's last line keeps the kick on every beat under a clap roll over the 3 bars before the
        fill bar only (its first 4 bars play the written claps); the fill bar is the FillPreview's, the kick out on
        beat 4 alone, under the Build Fill; the riser ends and the impact lands on the drop's downbeat."""
        R, notes, _ = mix('novox')
        first = section(R, 'build')['lines'][-1]['start_bar']
        claps = [{st for st, key, _ in in_bar(notes['Clap'], first + b)} for b in range(8)]
        kicks = [{st for st, key, _ in in_bar(notes['Kick'], first + b)} for b in range(8)]
        for b in range(4):
            self.assertLessEqual(claps[b], {4, 12, 15}, first + b)
        for b, every in ((4, 4), (5, 2), (6, 1)):
            self.assertEqual(set(range(0, 16, every)), claps[b], first + b)
        self.assertEqual(set(), claps[7])
        self.assertEqual([{0, 4, 8, 12}] * 7 + [{0, 4, 8}], kicks)
        drop = (first + 7) * BAR
        self.assertTrue([n for n in notes['FX'] if n[3] == BUILD_FILL and n[1] == drop])
        self.assertTrue([n for n in notes['FX'] if n[3] == RISER and n[1] == drop and n[0] == drop - 8 * BAR])
        self.assertTrue([n for n in notes['FX'] if n[3] == IMPACT and n[0] == drop])

    def test_the_dub_stab_ride_opens_across_the_groove(self):
        """The stab's low-pass (CC74, channel 4) arranged: dark where the stab comes into the groove, brighter line by
        line, wide open by the groove's last line; the plugin rides it on the stab's mix channel."""
        R, notes, ccs = mix('vox')
        points = {t: v for t, _, cc, v in ccs['Chords'] if cc == 74}
        curves = []
        for ln in section(R, 'groove')['lines']:
            if in_span(notes['Chords'], ln['start_bar'], 8):
                lo = (ln['start_bar'] - 1) * BAR
                curves.append([value_at(points, t) for t in range(lo, lo + 8 * BAR, 60)])
        self.assertGreaterEqual(len(curves), 3)
        self.assertLessEqual(curves[0][0], 8)
        means = [sum(c) / len(c) for c in curves]
        self.assertEqual(sorted(set(means)), means)
        self.assertEqual(127, max(curves[-1]))

    def test_the_bongo_pair_only_in_each_sections_even_lines(self):
        """Must-fix 3: the bongo D5 pair (16ths 7 and 11 of the loop's second bar) plays only in lines 2 and 4 of a
        section, and in them wherever the congas play."""
        R, notes, _ = mix('novox')
        for s, ln, i in form_lines(R):
            got = in_span(notes['Perc'], ln['start_bar'], 7)
            with self.subTest(bar=ln['start_bar']):
                if i % 2:
                    self.assertFalse([n for n in in_span(notes['Perc'], ln['start_bar'], 8) if n[3] == BONGO_D5])
                elif [n for n in got if n[3] in CONGAS]:
                    self.assertTrue([n for n in got if n[3] == BONGO_D5])

    def test_a_conga_run_turns_every_line_that_does_not_end_on_a_fill(self):
        """Must-fix 3: every line with drums ends on a fill, a turnaround or (into a break or the switch-up) the drums
        out, and every turnaround is the conga run, D4 C4 G3 on 16ths 12-15 of its last bar (the drops keep their rim
        through it; the lines whose perc is the shaker alone play the run alone)."""
        R, notes, _ = mix('novox')
        turns = 0
        for s, ln, i in form_lines(R):
            if not set(ln['lanes']) & {'kick', 'snare', 'hats', 'perc'}:
                continue
            with self.subTest(bar=ln['start_bar']):
                self.assertIn(ln.get('end'), ('fill', 'turn', 'drop'))
                if ln.get('end') == 'turn':
                    turns += 1
                    last = in_bar(notes['Perc'], ln['start_bar'] + 7)
                    self.assertEqual(CONGA_RUN, {(st, key) for st, key, _ in last
                                                 if st >= 12 and key not in (P3.SHAKER, RIM)})
        self.assertGreaterEqual(turns, 12)

    def test_the_rim_moves_to_the_ands_of_3_and_4_in_the_drops(self):
        """Must-fix 3: in the drops (drop and drop 2) the rim plays 16ths 10 and 14 of every bar but each line's last
        (its turnaround or fill bar); elsewhere the loop's 16th 13."""
        R, notes, _ = mix('novox')
        for s, ln, i in form_lines(R):
            drop = s['kind'] in ('drop', 'drop2')
            congas = [n for n in in_span(notes['Perc'], ln['start_bar'], 7) if n[3] in CONGAS]
            for b in range(7):
                rims = {st for st, key, _ in in_bar(notes['Perc'], ln['start_bar'] + b) if key == RIM}
                with self.subTest(bar=ln['start_bar'] + b):
                    if drop and congas:
                        self.assertEqual({10, 14}, rims)
                    elif rims:
                        self.assertEqual({13}, rims)

    def test_the_open_hat_opens_up_on_the_and_of_4_in_bars_4_and_8(self):
        """Must-fix 3: the open hat at full velocity (127) on the 'and' of 4 (16th 14) of each line's bars 4 and 8,
        the fill bar left as written; at its written 0.8 everywhere else (where no rise scales the hats)."""
        R, notes, _ = mix('novox')
        for s, ln, i in form_lines(R):
            if 'hats' not in ln['lanes'] or ln.get('rise'):
                continue
            for b in range(8):
                opens = [(st, vel) for st, key, vel in in_bar(notes['Hats'], ln['start_bar'] + b) if key == OPEN_HAT]
                if not opens:
                    continue
                with self.subTest(bar=ln['start_bar'] + b):
                    want = {14} if b == 3 or (b == 7 and ln.get('end') != 'fill') else set()
                    self.assertEqual(want, {st for st, vel in opens if vel == 127})

    def test_something_changes_at_every_line(self):
        """No two adjacent lines alike even without their last bars (the turnaround or the fill): something changes
        on every 8-bar line; at least 24 distinct lines of 28; and every 16 bars something structural (an
        instrument in or out, another drum pattern, bassline, chord block or lead part), in either variant."""
        for v in VARIANTS:
            R, notes, _ = mix(v)
            n = R['bars'] // 8
            sigs = [bar_sig(notes, b + 1) for b in range(R['bars'])]
            heads = [tuple(sigs[8 * i:8 * i + 7]) for i in range(n)]
            texture = [{name: frozenset((on % BAR, key) for on, _, _, key, _ in in_span(ns, 8 * i + 1, 7))
                        for name, ns in notes.items() if name in STRUCTURAL} for i in range(n)]
            with self.subTest(variant=v):
                self.assertEqual([], [8 * i + 1 for i in range(1, n) if heads[i] == heads[i - 1]])
                self.assertGreaterEqual(len({tuple(sigs[8 * i:8 * i + 8]) for i in range(n)}), 24)
                self.assertEqual([], [8 * i + 1 for i in range(2, n, 2) if texture[i] == texture[i - 1]])

    def test_the_switch_up_has_its_own_ear_candy(self):
        """The switch-up's ear candy is its own, dry: a Gm7 organ hit on the 'and' of 4 every 4 bars (132, 136, 140),
        none of the loop's accents; the noise sweep up into break 2."""
        keys = {a.item: 48 + i for i, a in enumerate(P3.PARTS['accents'])}     # channel 6, one key a sound
        for v in VARIANTS:
            R, notes, _ = mix(v)
            sw = section(R, 'switch')
            got = in_span(notes['Accents'], sw['start_bar'], sw['bars'])
            end = (sw['start_bar'] - 1 + sw['bars']) * BAR
            self.assertEqual([(sw['start_bar'] + b, 14, keys[P3.ORGAN]) for b in (3, 7, 11)],
                             [(n[0] // BAR + 1, tick_step(KIT, n[0] % BAR), n[3]) for n in got
                              if n[3] != keys['AccentNoiseSweepUp']], v)
            self.assertEqual([end], [n[1] for n in got if n[3] == keys['AccentNoiseSweepUp']], v)

    def test_the_phrases_rest_under_the_fill(self):
        """Where the switch-up's Rubber riff and break 2's keys arp end on a fill, they rest from its first beat: the
        Build Fill alone in their place, the bass out before break 2 and the arp out before drop 2."""
        R, notes, _ = mix('novox')
        for kind, track in (('switch', 'Bass'), ('break', 'Lead')):
            sec = [s for s in R['sections'] if s['kind'] == kind][-1]
            last = sec['start_bar'] + sec['bars'] - 1
            with self.subTest(section=sec['name']):
                self.assertTrue(in_span(notes[track], last - 1, 1))
                self.assertFalse(in_span(notes[track], last, 1))

    def test_the_stab_lands_open_on_the_drop(self):
        """Review #13: the build opens the stab's low-pass for 16 bars and the drop lands on it open: its first hit no
        darker than the build's last, held open, then closing into the loop's ride on the next line. Nowhere does the
        stab, playing on through a line, snap shut at the line (a fall of more than a quarter of the range)."""
        for v in VARIANTS:
            R, notes, ccs = mix(v)
            stab = sorted(n[0] for n in notes['Chords'])
            d = section(R, 'drop')['start_bar']
            last = max(t for t in stab if t < (d - 1) * BAR)
            first = min(t for t in stab if t >= (d - 1) * BAR)
            with self.subTest(variant=v):
                self.assertGreaterEqual(cc74(ccs, first), cc74(ccs, last))
                self.assertEqual(127, min(cc74(ccs, t) for t in stab if (d - 1) * BAR <= t < (d + 1) * BAR))
                for _, ln, i in form_lines(R):
                    lo = (ln['start_bar'] - 1) * BAR
                    before = [t for t in stab if lo - BAR <= t < lo]
                    after = [t for t in stab if lo <= t < lo + BAR]
                    if before and after:
                        self.assertLessEqual(cc74(ccs, before[-1]) - cc74(ccs, after[0]), 32, ln['start_bar'])

    def test_a_vox_cue_at_every_vox_entry(self):
        """Review #16, #26: the vox mix's cue points mark every vox entry: every line whose vox comes in after a line
        without it (the riff's return at 121 included) starts on a Vox cue."""
        R, notes, _ = mix('vox')
        cues = {c['bar'] for s in R['sections'] for c in s['cues'] if c['cue'] == 'Vox'}
        sings = [bool(in_span(notes['Vox'], ln['start_bar'], 8)) for _, ln, _ in form_lines(R)]
        entries = [ln['start_bar'] for (_, ln, _), now, before in zip(form_lines(R), sings, [False] + sings)
                   if now and not before]
        self.assertIn(121, entries)
        self.assertLessEqual(set(entries), cues)

    def test_the_accents_leave_the_riff_its_notes(self):
        """Review #30: where the vox sings, no accent starts on one of its notes (the dub chord on the riff's thrown G4
        masked it) and no chord accent sounding under a vox note clashes with it (the reverse Gm9 swelling under the
        riff's Cm9 bar): the loop's call and response, accent then vox, kept."""
        R, notes, _ = mix('vox')
        vox = notes['Vox']
        ons = {n[0] for n in vox}
        self.assertEqual([], [(n[0] // BAR + 1, n[3]) for n in notes['Accents'] if n[0] in ons])
        clash = [(on // BAR + 1, key, a[3]) for on, _, _, key, _ in vox for a in notes['Accents']
                 if a[3] in ACCENT_TONES and a[0] <= on < a[1] and key % 12 not in ACCENT_TONES[a[3]]
                 and any((key - t) % 12 in (1, 6, 11) for t in ACCENT_TONES[a[3]])]
        self.assertEqual([], clash)

    def test_the_cues_land_where_the_form_puts_them(self):
        """Review #34: the cue sounds sit on their lines: the reverse Gm9 swells into break 1, the build and the
        switch-up (in the no-vox mix; in the vox mix only where the vox rests, into break 1); the noise sweep into 49,
        89, 113, 145 and 177; the Cm9 dub chord on the 'and' of 4 of 40, 56, 120, 168 and 184, and of 104 where no vox
        throws its last note there."""
        def ends(v, key):   # the bars an end-aligned sound ends on (its note-off on their downbeat)
            return sorted(n[1] // BAR + 1 for n in mix(v)[1]['Accents'] if n[3] == key and n[1] % BAR == 0)
        self.assertEqual([65, 81, 129], ends('novox', REVERSE_GM9))
        self.assertEqual([65], [b for b in ends('vox', REVERSE_GM9) if b in (65, 81, 129)])
        self.assertEqual([49, 89, 113, 145, 177], ends('vox', SWEEP))
        for v, bars in (('vox', (40, 56, 120, 168, 184)), ('novox', (40, 56, 104, 120, 168, 184))):
            got = {n[0] // BAR + 1 for n in mix(v)[1]['Accents']
                   if n[3] == DUB_CM9 and tick_step(KIT, n[0] % BAR) == 14}
            self.assertLessEqual(set(bars), got, v)
        self.assertNotIn(104, {n[0] // BAR + 1 for n in mix('vox')[1]['Accents'] if n[3] == DUB_CM9})

    def test_the_outro_is_not_the_intro_reversed(self):
        """Review #35, audit cross-kit issue 13: no outro line plays an intro line again, not even in its first 7 bars,
        and the outro doesn't take the intro's lanes away in the order the intro brought them in."""
        for v in VARIANTS:
            R, notes, _ = mix(v)
            body = lambda first: tuple(bar_sig(notes, b) for b in range(first, first + 7))  # noqa: E731
            intro, outro = R['sections'][0], R['sections'][-1]
            with self.subTest(variant=v):
                self.assertFalse({body(ln['start_bar']) for ln in intro['lines']}
                                 & {body(ln['start_bar']) for ln in outro['lines']})
                self.assertNotEqual([(ln['lanes'], ln.get('hats')) for ln in intro['lines']],
                                    [(ln['lanes'], ln.get('hats')) for ln in reversed(outro['lines'])])

    def test_the_keys_thin_where_the_vox_plays(self):
        """Where the vox plays, the keys play at most one bar of each 2-bar cycle (their call, or their answer to the
        vox's call), never the arp: the two share a register (G4 to G5)."""
        R, notes, _ = mix('vox')
        for c in sorted({on // (2 * BAR) for on, *_ in notes['Vox']}):
            bars = {on // BAR for on, *_ in notes['Lead'] if c * 2 * BAR <= on < (c + 1) * 2 * BAR}
            self.assertLessEqual(len(bars), 1, 2 * c + 1)

    def test_nothing_but_the_bass_under_g3(self):
        """Not muddy: the bass alone under G3; the pad, the stab, the keys and the vox from G3 up."""
        for v in VARIANTS:
            R, notes, _ = mix(v)
            for name in ('Lead', 'Chords', 'Chord voicing', 'Pad Warm', 'Vox'):
                self.assertGreaterEqual(min((n[3] for n in notes.get(name, ())), default=55), 55, (v, name))

    def test_the_vox_comes_in_late_and_sits_out_the_build_the_switch_up_and_the_strip(self):
        """The vox placed with intent: first its call, on the groove's last line; its whole riff over the pad in break
        1 and on the drop's first line; out of the build, the switch-up and drop 2's last line (the strip into the
        outro)."""
        R, notes, _ = mix('vox')
        bars = {on // BAR + 1 for on, *_ in notes['Vox']}
        g, brk, d = section(R, 'groove'), section(R, 'break'), section(R, 'drop')
        self.assertEqual(g['lines'][-1]['start_bar'], min(bars))
        for sec in (brk, d):
            self.assertIn(75, {n[3] for n in in_span(notes['Vox'], sec['start_bar'], 8)}, sec['name'])
        for sec in (section(R, 'build'), section(R, 'switch')):
            self.assertFalse(bars & set(range(sec['start_bar'], sec['start_bar'] + sec['bars'])), sec['name'])
        strip = section(R, 'drop2')['lines'][-1]['start_bar']
        self.assertFalse(bars & set(range(strip, strip + 8)))


if __name__ == '__main__':
    unittest.main()
