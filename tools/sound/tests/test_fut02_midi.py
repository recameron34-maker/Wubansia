"""FUT02's MIDI (tools/sound/recipes/fut02/midi.py), on top of test_midi.py's kit-agnostic properties: its Loop files
lane by lane against its round 5 context loop (context5.py on context.py), its roots, its phrases' channels, the drop
glide, the vox's vowels and throw, and the arp's ride; and its own extended mix (PARTS['form']) on the exported files,
audit B's FUT02 musts: the groove on the arp's skeleton, a drop that is a drop, harmony that moves, break 2 filled,
the switch-up on Orbit with a new zap figure, drop 2 on Relay with the glass melody, 32-bar drum ends, the arp thinned
where the vox sings, a 16-bar percussion cycle in the drops, the arp's filter ride, a change at every line. FUT02's
own tests: its engineer edits them with the kit's PARTS."""
import unittest

from tools.sound import channels as CH
from tools.sound import kits
from tools.sound import midi as Mi
from tools.sound.recipes.fut02 import accents as A, bass as B, context as C, context5 as C5, lead as L, pad as Pd
from tools.sound.recipes.fut02 import voxsynth as Vs
from tools.sound.tests.midi_testlib import (BAR, VARIANTS, LoopCase, at, chord_roots, drum, layers_of, loop, mids,
                                            one_shot, parsed, phrase_channels, ride, tick_step, v127, value_at, whole)

KIT = 'FUT02'


class TestLoop(LoopCase, unittest.TestCase):
    KIT = KIT

    def expected(self, variant):
        a = lambda s: at(KIT, s)  # noqa: E731
        early = round(C.GLIDE_EARLY * kits.load(KIT).BPM / 60 * 960)
        version = C5.VOX if variant == 'vox' else C5.NOVOX
        out = {
            'Kick': drum(KIT, C.KICK, 36),
            'Clap': drum(KIT, C.CLAP, 38),
            'Hats': drum(KIT, C.HAT_CLOSED, 42) + drum(KIT, C.HAT_OPEN, 46),
            'Perc': drum(KIT, C.ZAP, 47),
            'Bass': [(a(s + 32 * r), n, v, None) for r in (0, 1) for s, n, _, v, _ in B.LINES[B.PICK]
                     if not C.GLIDE[0] <= s + 32 * r < C.GLIDE[1]],
            'Drop Glide': [(a(C.GLIDE[0]), 26, layers_of(KIT, 'DropGlideClean')[C.GLIDE_LAYER - 1],
                            a(C.GLIDE[1]) - early)],
            'Lead': [(a(s + 32 * r), n, v, None) for r in (0, 1) for s, n, _, v in L.phrase_notes()
                     if s + 32 * r < C.ARP_REST[0]],
            'Chords': [(0, 50, 1.0, 64 * 240)],
            'Chord voicing': [(0, n, 1.0, 64 * 240) for n in Pd.CHORD],
            'Accents': [one_shot(KIT, 'accents', item, step, layer, 'end' if ends else 'on', zone=zone)
                        for step, item, zone, layer, *_, ends in C5.ACCENTS[version]],
            'FX': [one_shot(KIT, 'fx', 'LaserFill', C.FILL_AT, 1, 'on')],
        }
        if variant == 'vox':
            out['Vox'] = [(a(s + C5.RIFF_AT), n, v, None) for s, n, _, v in Vs.RIFF]
        return out


class TestParts(unittest.TestCase):
    def test_the_pad_at_its_root(self):
        """The starfield pad Dm9 on D3 (50), on channel 4; its voicing on 3."""
        self.assertEqual(({50}, {4}, {3}), chord_roots(KIT))

    def test_the_phrases_channels(self):
        self.assertEqual({'VoxSynthRiff': 5, 'LaserStabRiff': 12, 'GlassMelody': 13, 'VoxChoirChords': 5},
                         phrase_channels(KIT))

    def test_the_vox_synths_velocity_picks_its_vowel(self):
        """Every vox note sits on a layer's velocity: 64 'oh', 95 'ah', 127 'ee'."""
        layers = layers_of(KIT, 'VoxSynth')
        self.assertEqual([64, 95, 127], [v127(x) for x in layers])
        for path in mids(KIT):
            for t in parsed(KIT, path)[2][1:]:
                if t['name'] == 'Vox':
                    self.assertLessEqual({vel for *_, vel in t['notes']}, {64, 95, 127}, path)

    def test_the_vox_throw(self):
        """The riff's A4 on bar 3's 16th 12 (the loop's 44) thrown: CC91 and CC94 at 127 there."""
        got = sorted(c[0] for c in loop(KIT, 'vox')['Vox']['ccs'] if c[2] == 91 and c[3] == 127)
        self.assertEqual([at(KIT, 44)], got)


class TestRides(unittest.TestCase):
    def test_the_arp_ride(self):
        """The arp's filter ride (context.ride_hz) on CC74, channel 2, 1.6-16 kHz: closed on the downbeat, wide open at
        16th 46, closing again; no CC71 (its Q is fixed)."""
        r = ride(KIT, 'Lead', 74)
        self.assertEqual(0, r[0])
        self.assertEqual(127, max(r.values()))
        self.assertEqual(127, value_at(r, at(KIT, 46)))
        self.assertLess(value_at(r, at(KIT, 30)), 127)
        self.assertFalse(ride(KIT, 'Lead', 71))


# ------------------------------------------------------------------ the extended mix: FUT02's own form
FORM = (('Intro', 'intro', 1, 32), ('Groove', 'groove', 33, 32), ('Break', 'break', 65, 16),
        ('Build', 'build', 81, 16), ('Drop', 'drop', 97, 32), ('Switch-up', 'switch', 129, 16),
        ('Break 2', 'break', 145, 16), ('Drop 2', 'drop2', 161, 32), ('Outro', 'outro', 193, 32))
SKELETON = (0, 3, 6, 8, 11, 14)                        # the arp's 3-3-2 accents, 16ths of a bar
ARP = {s: (m, v) for s, m, _, v in L.phrase_notes()}  # the arp phrase (2 bars) by its 16th
CHOIR = {frozenset(chord) for chord, _ in A.CHOIR_BARS}
CALL = sorted((s, m) for s, m, _, _ in Vs.RIFF if s < 16)   # the riff's first bar: D5 F5 E5 D5 C5
WHOLE = 224 * BAR


def span(x):
    """A section's or a line's (lo, hi) in ticks of the whole mix."""
    lo = (x['start_bar'] - 1) * BAR
    return lo, lo + x['bars'] * BAR


def sec(R, name):
    return next(s for s in R['sections'] if s['name'] == name)


def notes_in(tracks, name, lo=0, hi=WHOLE):
    """A track's notes starting in [lo, hi): (on, off, key, velocity)."""
    t = tracks.get(name)
    return [(on, off, key, vel) for on, off, _, key, vel in (t['notes'] if t else ()) if lo <= on < hi]


def step_of(tick, lo):
    """A note-on's 16th counted from a bar line `lo`, back on its grid."""
    return tick_step(KIT, tick - lo)


def bar_notes(tracks, lo, names=None):
    """Every note starting in the bar from `lo`, relative to it, with its track: what a bar plays."""
    return tuple(sorted((n, on - lo, off - lo, key, vel) for n in (names or sorted(tracks))
                        for on, off, key, vel in notes_in(tracks, n, lo, lo + BAR)))


def chord_groups(tracks, lo, hi):
    """The vox's chords in [lo, hi): the sets of three or more vox notes starting together (the choir)."""
    at_ = {}
    for on, _, key, _ in notes_in(tracks, 'Vox', lo, hi):
        at_.setdefault(on, set()).add(key)
    return {frozenset(keys) for keys in at_.values() if len(keys) >= 3}


def riff_notes(tracks, lo=0, hi=WHOLE):
    """The vox's single notes in [lo, hi) (the riff, its call and answer, its throws), not the choir's chords."""
    on_ = [n[0] for n in notes_in(tracks, 'Vox')]
    return [n for n in notes_in(tracks, 'Vox', lo, hi) if on_.count(n[0]) == 1]


def sounding(tracks, name, tick):
    """A track's keys sounding at a tick."""
    return [key for on, off, key, _ in notes_in(tracks, name) if on <= tick < off]


class TestExtendedMix(unittest.TestCase):
    """FUT02's own extended mix (PARTS['form']), both variants, on the exported files."""

    def test_the_form(self):
        """The sections FUT02's form plays, by name, kind, start bar and length: the club form's section lines."""
        for v in VARIANTS:
            R = whole(KIT, v)[0]
            self.assertEqual(FORM, tuple((s['name'], s['kind'], s['start_bar'], s['bars']) for s in R['sections']))

    def test_32_bars_of_drums_at_each_end_and_an_outro_of_its_own(self):
        """Audit B FUT02 must 4: the bass (the Orbit line and its glide) enters on bar 33 and is gone by bar 193
        (test_midi checks that nothing but the drums sounds in the first and last 32 bars). The outro is no mirror of
        the intro: no outro line plays the drums of the intro line it faces (1 and 217, 9 and 209, 17 and 201, 25 and
        193) in its first 7 bars (a turnaround alone is no difference), and the mix ends on the kick and the hats in
        every bar."""
        for v in VARIANTS:
            _, tracks, _ = whole(KIT, v)
            bass = sorted(notes_in(tracks, 'Bass') + notes_in(tracks, 'Drop Glide'))
            self.assertEqual(32, bass[0][0] // BAR, v)
            self.assertLessEqual(max(off for _, off, _, _ in bass), 192 * BAR, v)
            drums = [t for t in tracks if CH.track_channel(KIT, t) == 10]
            seven = lambda b: tuple(bar_notes(tracks, (b + i) * BAR, drums) for i in range(7))  # noqa: E731
            for i in range(4):
                self.assertNotEqual(seven(8 * i), seven(216 - 8 * i), (v, 8 * i + 1))
            for b in range(216, 224):
                for name in ('Kick', 'Hats'):
                    self.assertTrue(notes_in(tracks, name, b * BAR, (b + 1) * BAR), (v, name, b + 1))

    def test_the_groove_plays_the_arps_skeleton_and_no_pad(self):
        """Audit B FUT02 must 1: in the groove the arp plays its skeleton, its 3-3-2 accents (16ths 0, 3, 6, 8, 11 and
        14 of every bar, the phrase's own notes and velocities), in every bar of every line it plays but a fill's last
        (the call left every other bar silent, which read as broken); not the drone that fills the phrase's other
        16ths, and no pad."""
        layers = layers_of(KIT, 'ChromeArp')
        for v in VARIANTS:
            R, tracks, _ = whole(KIT, v)
            g = sec(R, 'Groove')
            self.assertFalse(notes_in(tracks, 'Chords', *span(g)), v)
            played = 0
            for ln in g['lines']:
                lo, _ = span(ln)
                for j in range(8):
                    b = lo + j * BAR
                    got = sorted((step_of(on, b), key, vel) for on, _, key, vel in notes_in(tracks, 'Lead', b, b + BAR))
                    if 'lead' not in ln['lanes'] or (j == 7 and ln.get('end') == 'fill'):
                        self.assertEqual([], got, (v, ln['start_bar'] + j))
                        continue
                    phrase = [ARP[s + 16 * (j % 2)] for s in SKELETON]
                    self.assertEqual([(s, m, v127(vel, layers)) for s, (m, vel) in zip(SKELETON, phrase)], got,
                                     (v, ln['start_bar'] + j))
                    played += 1
            self.assertGreaterEqual(played, 16, v)

    def test_the_drop_is_a_drop(self):
        """Audit B FUT02 must 1, the brief's bar: the drop's first line is not the groove's last. It brings the full
        arp (the drone between the accents, which the groove leaves out), an impact on its first beat (the sub drop and
        a glass ping) and the accents in both its 4-bar blocks, and in the vox mix the riff as notes, held back until
        then but for the groove's one throw; the groove's last line has neither the drone nor an impact."""
        k = Mi.kit_of(KIT)
        sub, ping = k.key_of('fx', 'SubDrop'), k.key_of('accents', 'GlassPing')
        for v in VARIANTS:
            R, tracks, _ = whole(KIT, v)
            g, d = sec(R, 'Groove')['lines'][-1], sec(R, 'Drop')['lines'][0]
            for ln, hits in ((g, False), (d, True)):
                lo, hi = span(ln)
                drone = [n for n in notes_in(tracks, 'Lead', lo, hi) if step_of(n[0], lo) % 16 not in SKELETON]
                impact = (sub in {n[2] for n in notes_in(tracks, 'FX', lo, lo + 1)}
                          and ping in {n[2] for n in notes_in(tracks, 'Accents', lo, lo + 1)})
                self.assertEqual(hits, bool(drone), (v, ln['start_bar']))
                self.assertEqual(hits, impact, (v, ln['start_bar']))
                if hits:
                    for b in (lo, lo + 4 * BAR):
                        self.assertTrue(notes_in(tracks, 'Accents', b, b + 4 * BAR), (v, b // BAR + 1))
            if v == 'vox':
                lo, hi = span(d)
                self.assertEqual(1, len(riff_notes(tracks, 0, lo)))
                self.assertTrue(riff_notes(tracks, lo, hi))

    def test_the_harmony_moves(self):
        """Audit B FUT02 must 2: in the vox mix the choir chords (Dm7, Dm9, B-flat maj7 over D, Am7: the Vox choir
        chords phrase, as recorded) sing in both breaks; in both mixes the laser stab riff's Dm9 and B-flat maj9 move the
        drop's harmony from its third line (its first two are the arp's and the vox's), and every break line keeps a
        harmony of its own in the no-vox mix too (the pad, or the laser stabs)."""
        for v in VARIANTS:
            R, tracks, _ = whole(KIT, v)
            for name in ('Break', 'Break 2'):
                s = sec(R, name)
                self.assertEqual(CHOIR if v == 'vox' else set(), chord_groups(tracks, *span(s)), (v, name))
                for ln in s['lines']:
                    self.assertTrue(notes_in(tracks, 'Chords', *span(ln)) or
                                    notes_in(tracks, 'Laser Chord Stab', *span(ln)), (v, ln['start_bar']))
            for i, ln in enumerate(sec(R, 'Drop')['lines']):
                keys = {n[2] for n in notes_in(tracks, 'Laser Chord Stab', *span(ln))}
                self.assertEqual({58, 62} if i >= 2 else set(), keys, (v, ln['start_bar']))

    def test_no_b_flat_stab_over_the_pad(self):
        """The laser stab's B-flat maj9 (its root B-flat 3) never sounds over the pad, whose Dm9 holds A3 a semitone
        under it in the low mids: where the stabs move the harmony the pad is out."""
        bb = Mi.kit_of(KIT).key_of('accents', 'LaserChordStab', 58)
        for v in VARIANTS:
            _, tracks, _ = whole(KIT, v)
            pad = [(on, off) for on, off, _, _ in notes_in(tracks, 'Chords')]
            stabs = [n for n in notes_in(tracks, 'Laser Chord Stab') if n[2] == 58]
            stabs += [n for n in notes_in(tracks, 'Accents') if n[2] == bb]
            self.assertTrue(stabs, v)
            for on, off, _, _ in stabs:
                self.assertFalse([p for p in pad if p[0] < off and on < p[1]], (v, on // BAR + 1))

    def test_break_2_is_filled(self):
        """Audit B FUT02 must 3: break 2 is no pad alone (two chord attacks in 8 bars): at least four notes start in
        every bar but its fill bar, in both mixes; the vox mix sings the choir chords and the riff's call (its first
        bar, D5 F5 E5 D5 C5) there; it ends on a 1-bar fill and the riser, both ending on drop 2's downbeat."""
        k = Mi.kit_of(KIT)
        fill, riser = k.key_of('fx', 'LaserFill'), k.key_of('fx', 'LaserRiser')
        for v in VARIANTS:
            R, tracks, _ = whole(KIT, v)
            lo, hi = span(sec(R, 'Break 2'))
            for b in range(lo, hi - BAR, BAR):
                self.assertGreaterEqual(sum(len(notes_in(tracks, t, b, b + BAR)) for t in tracks), 4,
                                        (v, b // BAR + 1))
            fx = notes_in(tracks, 'FX', lo, hi)
            self.assertTrue([n for n in fx if n[2] == fill and n[0] >= hi - BAR and n[1] == hi], v)
            self.assertTrue([n for n in fx if n[2] == riser and n[1] == hi], v)
            if v == 'vox':
                self.assertEqual(CHOIR, chord_groups(tracks, lo, hi))
                calls = {}
                for on, _, key, _ in riff_notes(tracks, lo, hi):
                    b = lo + (on - lo) // BAR * BAR
                    calls.setdefault(b, []).append((step_of(on, b), key))
                self.assertIn(CALL, [sorted(x) for x in calls.values()])

    def test_the_switch_up_stays_on_orbit_with_a_new_zap_figure(self):
        """Audit B FUT02 must 1: the switch-up keeps the Orbit line under a zap figure: each of its 4-bar perc blocks
        is one that no block before it played."""
        for v in VARIANTS:
            R, tracks, _ = whole(KIT, v)
            s = sec(R, 'Switch-up')
            self.assertEqual({'Orbit'}, {ln['bass'] for ln in s['lines'] if 'bass' in ln}, v)
            lo, hi = span(s)
            block = lambda b: tuple((on - b, key, vel) for on, _, key, vel in notes_in(tracks, 'Perc', b, b + 4 * BAR))  # noqa: E731,E501
            before = {block(b) for b in range(0, lo, 4 * BAR)}
            for b in range(lo, hi, 4 * BAR):
                self.assertTrue(block(b), (v, b // BAR + 1))
                self.assertNotIn(block(b), before, (v, b // BAR + 1))

    def test_drop_2_plays_relay_the_vox_and_the_glass_melody(self):
        """Audit B FUT02 must 1: drop 2 is on the Relay line (the switch-up kept Orbit), with the glass melody on
        channel 13 (its phrase's notes) and, in the vox mix, the vox."""
        glass = {m for _, m, _ in A.GLASS_MELODY}
        for v in VARIANTS:
            R, tracks, _ = whole(KIT, v)
            s = sec(R, 'Drop 2')
            self.assertEqual({'Relay'}, {ln['bass'] for ln in s['lines'] if 'bass' in ln}, v)
            self.assertEqual(glass, {n[2] for n in notes_in(tracks, 'Glass Ping', *span(s))}, v)
            self.assertEqual(v == 'vox', bool(riff_notes(tracks, *span(s))), v)

    def test_the_arp_thins_where_the_vox_sings(self):
        """Audit B FUT02 nice 5: the arp and the vox share MIDI 69-84. In every bar where the vox sings more than the
        loop's own pattern (its riff every 2 bars, its call, its answer), the arp plays only its accents (the skeleton:
        a third over the riff on their shared 3-3-2) or rests: no drone note there, and no arp note sounding a second or
        a minor ninth from a riff note as it starts. The loop's own pattern (the drop's first lines) keeps the full
        arp, as the record plays them."""
        R, tracks, _ = whole(KIT, 'vox')
        thinned = 0
        for s in R['sections']:
            for ln in s['lines']:
                if ln.get('vox') not in ('riff', 'call', 'answer'):
                    continue
                lo, _ = span(ln)
                for b in range(lo, lo + 8 * BAR, BAR):
                    riff = riff_notes(tracks, b, b + BAR)
                    if not riff:
                        continue
                    thinned += 1
                    drone = [n for n in notes_in(tracks, 'Lead', b, b + BAR) if step_of(n[0], b) % 16 not in SKELETON]
                    self.assertFalse(drone, b // BAR + 1)
                    for on, _, key, _ in riff:
                        near = [m for m in sounding(tracks, 'Lead', on) if abs(m - key) in (1, 2, 13)]
                        self.assertFalse(near, (on // BAR + 1, key))
        self.assertGreaterEqual(thinned, 16)

    def test_the_vox_sits_on_the_harmony(self):
        """Every riff note is a tone of the harmony sounding as it starts (the pad's Dm9, the laser stabs' Dm9 or
        B-flat maj9, the accents' chords), where any sounds: no riff E5 against a B-flat maj9 stab (the riff calls over
        the stabs' Dm9 and leaves their B-flat maj9 bars to them); and no riff over the pad, whose E5 rubs a semitone
        under the riff's F5."""
        stab = {62: (62, 69, 72, 76, 77), 58: (58, 65, 69, 72, 74)}     # accents.STAB_CHORDS: Dm9, B-flat maj9
        k = Mi.kit_of(KIT)
        acc = {k.key_of('accents', 'LaserChordStab', z): stab[z] for z in stab}
        _, tracks, _ = whole(KIT, 'vox')
        riff = riff_notes(tracks)
        self.assertTrue(riff)
        for on, _, key, _ in riff:
            self.assertFalse(sounding(tracks, 'Chords', on), (on // BAR + 1, key))
            tones = [t for c in sounding(tracks, 'Laser Chord Stab', on) for t in stab[c]]
            tones += [t for c in sounding(tracks, 'Accents', on) for t in acc.get(c, ())]
            if tones:
                self.assertIn(key % 12, {t % 12 for t in tones}, (on // BAR + 1, key))

    def test_a_16_bar_percussion_cycle_in_the_drops(self):
        """Audit B FUT02 nice 7: the drops' percussion turns over every 16 bars, not every 4: in each 16 bars of a
        drop the zaps play two written figures, one a line, and one of each pair is not the loop's."""
        zaps = [(on, key) for on, _, _, key, _ in loop(KIT, 'vox')['Perc']['notes']]
        bar2 = [(on + 2 * BAR, key) for on, key in zaps if BAR <= on < 2 * BAR]
        looped = {tuple(sorted(zaps)), tuple(sorted([z for z in zaps if z[0] < 3 * BAR] + bar2))}   # fill, groove
        for v in VARIANTS:
            R, tracks, _ = whole(KIT, v)
            for name in ('Drop', 'Drop 2'):
                lines = sec(R, name)['lines']
                for pair in (lines[0:2], lines[2:4]):
                    seven = [tuple(sorted((on - span(ln)[0], key) for on, _, key, _ in
                                          notes_in(tracks, 'Perc', span(ln)[0], span(ln)[0] + 7 * BAR)))
                             for ln in pair]
                    self.assertNotEqual(seven[0], seven[1], (v, pair[0]['start_bar']))
                    first = [tuple(n for n in x if n[0] < 4 * BAR) for x in seven]
                    self.assertTrue([f for f in first if f not in looped], (v, pair[0]['start_bar']))

    def test_the_arps_filter_ride(self):
        """The arp's CC74 ride (the plugin rides the arp's filter by it on its mix channel): held low in the groove (at
        most 40, about 3.1 kHz, where the loop's opens to 16 kHz), opening across the build from closed to fully open
        (never closing on the way), and the loop's own ride in the drops, 4 bars at a time."""
        looped = ride(KIT, 'Lead', 74)
        for v in VARIANTS:
            R, tracks, _ = whole(KIT, v)
            cc = {t: val for t, _, c, val in tracks['Lead']['ccs'] if c == 74}
            g = span(sec(R, 'Groove'))
            gv = [val for t, val in cc.items() if g[0] <= t < g[1]]
            self.assertTrue(gv, v)
            self.assertLessEqual(max(gv), 40, v)
            b = span(sec(R, 'Build'))
            bv = [cc[t] for t in sorted(cc) if b[0] <= t < b[1]]
            self.assertEqual(sorted(bv), bv, v)
            self.assertLessEqual(bv[0], 8, v)
            self.assertEqual(127, bv[-1], v)
            for name in ('Drop', 'Drop 2'):
                for ln in sec(R, name)['lines']:
                    lo, hi = span(ln)
                    if not notes_in(tracks, 'Lead', lo, hi):
                        continue
                    for t in range(lo, hi, 60):
                        self.assertEqual(value_at(looped, (t - lo) % (4 * BAR)), value_at(cc, t), (v, t // BAR + 1))

    def test_something_changes_at_every_line(self):
        """The brief's bar: every 8-bar line changes something before its last bar (its first 7 bars are not the line
        before's), every 16 bars the downbeat itself changes (a 16-bar half's first bar is not the bar 8 before it),
        and at least 24 of the 28 lines differ in each mix."""
        for v in VARIANTS:
            _, tracks, _ = whole(KIT, v)
            bars_ = [bar_notes(tracks, b * BAR) for b in range(224)]
            bodies = [tuple(bars_[i:i + 7]) for i in range(0, 224, 8)]
            self.assertEqual([], [8 * i + 1 for i in range(1, 28) if bodies[i] == bodies[i - 1]], v)
            self.assertEqual([], [b + 1 for b in range(16, 224, 16) if bars_[b] == bars_[b - 8]], v)
            self.assertGreaterEqual(len({tuple(bars_[i:i + 8]) for i in range(0, 224, 8)}), 24, v)

    def test_the_vox_throws_into_the_section_ends(self):
        """The riff's last note (A4) thrown (CC91 and CC94 at 127) on the 'and' of 4 of the groove's last bar, into
        the break, and of the drop's, where the drums sit out under it into the switch-up; none in the no-vox mix."""
        _, tracks, _ = whole(KIT, 'vox')
        thrown = {t for t, _, c, val in tracks['Vox']['ccs'] if c == 91 and val == 127}
        for bar in (64, 128):
            t = (bar - 1) * BAR + at(KIT, 14)
            self.assertIn(t, thrown, bar)
            self.assertIn((t, 69), [(on, key) for on, _, key, _ in notes_in(tracks, 'Vox')], bar)
        drums = [t for t in tracks if CH.track_channel(KIT, t) == 10]
        self.assertEqual((), bar_notes(tracks, 127 * BAR, drums))
        self.assertNotIn('Vox', whole(KIT, 'novox')[1])

    def test_the_build_opens_the_arp_under_the_roll(self):
        """The sci-fi club build, FUT02's: the arp plays through both its lines (its filter opening: the ride test),
        and the kick and the bass sit out its last line under the clap roll and the 8-bar laser riser, which ends on
        the drop's downbeat."""
        riser = Mi.kit_of(KIT).key_of('fx', 'LaserRiser')
        for v in VARIANTS:
            R, tracks, _ = whole(KIT, v)
            s = sec(R, 'Build')
            first, last = s['lines']
            for ln in s['lines']:
                self.assertTrue(notes_in(tracks, 'Lead', *span(ln)), (v, ln['start_bar']))
            for name in ('Kick', 'Bass', 'Drop Glide'):
                self.assertFalse(notes_in(tracks, name, *span(last)), (v, name))
            self.assertTrue(notes_in(tracks, 'Kick', *span(first)), v)
            self.assertTrue([n for n in notes_in(tracks, 'FX', *span(last)) if n[2] == riser and n[1] == span(s)[1]],
                            v)


if __name__ == '__main__':
    unittest.main()
