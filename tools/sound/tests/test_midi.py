"""The MIDI export (tools/sound/midi.py): valid files at the kit's tempo with its swing written in, clips that end on
their bar line, slides that reach their note, the extended mix (its form, its regions and their list), and committed
files (content/midi/<kit>/ and its extended/) that equal a fresh export."""
import functools
import hashlib
import json
import os
import re
import struct
import subprocess
import sys
import types
import unittest
from pathlib import Path

from tools.sound import kits
from tools.sound import midi as Mi
from tools.sound import qa as Q

REPO = Path(__file__).resolve().parents[3]
SOUND = REPO / 'tools' / 'sound'
KITS = [k for k in kits.names() if (SOUND / 'recipes' / k.lower() / 'midi.py').exists()]
MARKER, CUE, TEMPO, METER = 0x06, 0x07, 0x51, 0x58


def vlq_at(d, j):
    v = 0
    while True:
        c = d[j]
        j += 1
        v = (v << 7) | (c & 0x7F)
        if not c & 0x80:
            return v, j


def parse(data):
    """(format, ppq, tracks); a track is a dict: name, end (its end-of-track tick), notes [(tick, status, note,
    velocity)], meta [(tick, type, data)] (every meta event but the end of track) and tempo (us a quarter, or None)."""
    assert data[:4] == b'MThd'
    _, fmt, ntrk, ppq = struct.unpack('>IHHH', data[4:14])
    i, tracks = 14, []
    for _ in range(ntrk):
        assert data[i:i + 4] == b'MTrk'
        n = struct.unpack('>I', data[i + 4:i + 8])[0]
        d, i = data[i + 8:i + 8 + n], i + 8 + n
        j = t = 0
        tr = dict(name=None, end=None, notes=[], meta=[], tempo=None)
        while j < len(d):
            assert tr['end'] is None, 'an event after the end of track'
            v, j = vlq_at(d, j)
            t += v
            st = d[j]
            if st == 0xFF:
                kind = d[j + 1]
                ln, j = vlq_at(d, j + 2)
                body, j = d[j:j + ln], j + ln
                if kind == 0x2F:
                    tr['end'] = t
                    continue
                if kind == 0x03:
                    tr['name'] = body.decode()
                if kind == TEMPO:
                    tr['tempo'] = int.from_bytes(body, 'big')
                tr['meta'].append((t, kind, body))
            else:
                assert st & 0xF0 in (0x80, 0x90), hex(st)
                tr['notes'].append((t, st, d[j + 1], d[j + 2]))
                j += 3
        assert tr['end'] is not None and j == len(d)
        tracks.append(tr)
    assert i == len(data)
    return fmt, ppq, tracks


HOUSE_GENRES = ('hyper-clean future house', 'garage-house / deep tech house')   # the house form: the kick stays in the build


def bars(name):
    return int(re.search(r'_(\d+)bars\.mid$', name).group(1))


@functools.lru_cache(maxsize=None)
def ext(kit):
    """The kit's extended mix: (its files, regions.json as data)."""
    made = Mi.extended(kit)
    return made, json.loads(made['regions.json'])


def mids(kit):
    """Every MIDI file of the kit, the loop's and the extended mix's, by its path in content/midi/<kit>/."""
    out = {n: d for n, d in Mi.files(kit).items() if n.endswith('.mid')}
    out.update({f'{Mi.EXTENDED}/{n}': d for n, d in ext(kit)[0].items() if n.endswith('.mid')})
    return out


def ons(track, lo=0, hi=None):
    """A parsed track's note-ons in [lo, hi): (tick, note, velocity)."""
    return [(t, n, v) for t, st, n, v in track['notes'] if st & 0xF0 == 0x90 and lo <= t and (hi is None or t < hi)]


def offs(track):
    """A parsed track's note-off ticks."""
    return [t for t, st, _, _ in track['notes'] if st & 0xF0 == 0x80]


def lane_of(R):
    """{track name: lane} from regions.json."""
    return {t['track']: t['lane'] for t in R['tracks']}


def has(R, lane):
    """Whether the kit has anything to play on the lane."""
    return any(t['lane'] == lane and t['notes'] for t in R['tracks'])


def full(kit):
    """(regions.json, the whole mix's parsed tracks, ticks a bar)."""
    made, R = ext(kit)
    _, ppq, tracks = parse(made[R['file']])
    return R, tracks, 4 * ppq


def region(kit, kind, nth=0):
    """(section, its region's parsed tracks) of the nth section of that kind."""
    made, R = ext(kit)
    s = [s for s in R['sections'] if s['kind'] == kind][nth]
    return s, parse(made[s['file']])[2]


def lane_notes(R, tracks, lane, lo=0, hi=None):
    """The note-ons of every track on the lane."""
    lanes = lane_of(R)
    return sorted(n for tr in tracks[1:] if lanes[tr['name']] == lane for n in ons(tr, lo, hi))


class TestExport(unittest.TestCase):
    def test_some_kits_export(self):
        self.assertGreaterEqual(len(KITS), 3)

    def test_files_are_valid_and_end_on_their_bar_line(self):
        for kit in KITS:
            K = kits.load(kit)
            for name, data in mids(kit).items():
                with self.subTest(kit=kit, file=name):
                    fmt, ppq, tracks = parse(data)
                    self.assertEqual((1, Mi.PPQ), (fmt, ppq))
                    self.assertEqual(round(60_000_000 / K.BPM), tracks[0]['tempo'])
                    self.assertIn((0, METER, b'\x04\x02\x18\x08'), tracks[0]['meta'])
                    for tr in tracks:
                        self.assertEqual(bars(name) * 4 * ppq, tr['end'])

    def test_every_note_on_has_its_note_off(self):
        for kit in KITS:
            for name, data in mids(kit).items():
                with self.subTest(kit=kit, file=name):
                    for tr in parse(data)[2][1:]:
                        on = {}
                        for t, st, note, vel in tr['notes']:
                            key = (st & 0x0F, note)
                            if st & 0xF0 == 0x90:
                                self.assertNotIn(key, on)
                                self.assertTrue(1 <= vel <= 127)
                                on[key] = t
                            else:
                                self.assertGreater(t, on.pop(key))
                        self.assertEqual({}, on)

    def test_drum_notes_stay_under_the_sung_chops(self):
        """Every drum and perc note, in every file of every kit and in its drum map, sits inside 35-59: the plan's
        note map (docs/plans/S1-futuristic-slice.md section 2.6) gives 60-84 to the sung chops, so a file dragged back
        onto a Wubansia track plays the kit's drums, not its chops. The shakers sit on 54, GM's tambourine (GM has no
        shaker inside 35-59), as the generator's shaker lanes do."""
        for kit in KITS:
            K, P = Mi.parts(kit)
            self.assertEqual([], [n for n in P['drum_map'] if not 35 <= n <= 59], kit)
            for name, data in mids(kit).items():
                for tr in parse(data)[2][1:]:
                    drums = {n for _, st, n, _ in tr['notes'] if st & 0x0F == Mi.DRUM_CHANNEL}
                    out = sorted(n for n in drums if not 35 <= n <= 59)
                    self.assertEqual([], out, (kit, name, tr['name']))

    def test_drums_are_on_channel_10(self):
        for kit in KITS:
            notes = parse(Mi.files(kit)[f'{kit}_Drums_4bars.mid'])[2][1]['notes']
            self.assertEqual({Mi.DRUM_CHANNEL}, {st & 0x0F for _, st, _, _ in notes})
            self.assertIn(36, {n for _, _, n, _ in notes})
            R, tracks, _ = full(kit)
            lanes = lane_of(R)
            for tr in tracks[1:]:
                with self.subTest(kit=kit, track=tr['name']):
                    chans = {st & 0x0F for _, st, _, _ in tr['notes']}
                    if lanes[tr['name']] in Mi.DRUM_LANES:
                        self.assertEqual({Mi.DRUM_CHANNEL}, chans)
                    else:
                        self.assertNotIn(Mi.DRUM_CHANNEL, chans)

    def test_swing_is_written_in(self):
        ev = Mi.note_events([(0, 60, 1, 1.0, False), (3, 62, 1, 1.0, False)], 0.57, 0)
        starts = sorted(t for t, order, _ in ev if order == 1)
        self.assertEqual([0, 3 * Mi.S16 + round(0.07 * 2 * Mi.S16)], starts)

    def test_a_slide_holds_the_note_before_into_it(self):
        """A slide marks a note glided into from the note before (the kits' recipes: FUT01 "legato from the note
        before", FUT02 "slide from the note before", FUT04 "glides into the note from the one before"). That note is
        held SLIDE_TICKS into it, so a synth set to legato glide slides there; the slid note keeps its own length."""
        ev = Mi.note_events([(0, 40, 2, 1.0, False), (2, 43, 1, 1.0, True)], 0.5, 0)
        self.assertEqual({40: 2 * Mi.S16 + Mi.SLIDE_TICKS, 43: 3 * Mi.S16}, {m[1]: t for t, o, m in ev if o == 0})
        # a note before that stops up to a 16th short of the slid note still runs into it
        ev = Mi.note_events([(0, 40, 1, 1.0, False), (2, 43, 1, 1.0, True)], 0.5, 0)
        self.assertEqual(2 * Mi.S16 + Mi.SLIDE_TICKS, {m[1]: t for t, o, m in ev if o == 0}[40])

    def test_a_slide_with_no_note_running_up_to_it_plays_as_written(self):
        """Not joined: a note before that stops more than a 16th early (FUT03's slides glide inside one note, which a
        MIDI note can't carry), the same pitch again, and a slide with no note before it."""
        for notes in ([(0, 40, 0.5, 1.0, False), (4, 43, 1, 1.0, True)],
                      [(0, 40, 2, 1.0, False), (2, 40, 1, 1.0, True)],
                      [(2, 43, 1, 1.0, True)]):
            ev = Mi.note_events(notes, 0.5, 0)
            self.assertEqual(sorted((s + ln) * Mi.S16 for s, _, ln, _, _ in notes),
                             sorted(t for t, o, _ in ev if o == 0), notes)

    def test_the_kits_slides_reach_their_notes(self):
        """In every exported bassline, the note before a slid note still sounds SLIDE_TICKS into it, wherever it runs up
        to it as written (FUT01, FUT02 and FUT04: 11 slides)."""
        joined = 0
        for kit in KITS:
            K, P = Mi.parts(kit)
            late = round((K.SWING - 0.5) * 2 * Mi.S16)
            at = lambda s: s * Mi.S16 + (late if s % 2 else 0)  # noqa: E731
            made = Mi.files(kit)
            for name, line in P['lines'].items():
                f = next(n for n in made if re.fullmatch(rf'{kit}_Bassline_{name}(_InTheLoop)?_2bars\.mid', n))
                spans, sounding = [], {}
                for t, st, note, _ in parse(made[f])[2][1]['notes']:
                    if st & 0xF0 == 0x90:
                        sounding[note] = t
                    else:
                        spans.append((sounding.pop(note), t, note))
                notes = sorted(line)
                for (ps, pn, pln, _, _), (s, n, _, _, slide) in zip(notes, notes[1:]):
                    if slide and pn != n and at(ps) + round(pln * Mi.S16) >= at(s) - Mi.S16:
                        with self.subTest(kit=kit, line=name, step=s):
                            (off,) = [e for o, e, m in spans if o == at(ps) and m == pn]
                            self.assertGreaterEqual(off, at(s) + Mi.SLIDE_TICKS)
                            joined += 1
        self.assertGreaterEqual(joined, 11)

    def test_committed_files_match_a_fresh_export(self):
        for kit in KITS:
            folder = REPO / 'content' / 'midi' / kit.lower()
            self.assertEqual([Mi.EXTENDED], sorted(p.name for p in folder.iterdir() if p.is_dir()), kit)
            for sub, made in ((folder, Mi.files(kit)), (folder / Mi.EXTENDED, ext(kit)[0])):
                with self.subTest(kit=kit, folder=sub.name):
                    self.assertEqual(sorted(made), sorted(p.name for p in sub.iterdir() if p.is_file()))
                    for name, data in made.items():
                        self.assertEqual(data, (sub / name).read_bytes(), name)


class TestExtendedMix(unittest.TestCase):
    """The extended mix: a DJ-friendly arrangement of the kit's written parts, 6-8 minutes on 8-bar lines, as one file
    and one region per section, listed in regions.json (midi.py, FORMS)."""

    def test_the_folder_holds_the_mix_its_regions_and_their_list(self):
        for kit in KITS:
            made, R = ext(kit)
            self.assertEqual(f'{kit}_ExtendedMix_{R["bars"]}bars.mid', R['file'])
            self.assertEqual({R['file'], 'regions.json', 'README.md'} | {s['file'] for s in R['sections']}, set(made))
            for i, s in enumerate(R['sections'], 1):
                self.assertRegex(s['file'], rf'^{i:02d}_[A-Z][A-Za-z0-9]*_{s["bars"]}bars\.mid$')

    def test_it_runs_6_to_8_minutes_at_the_kits_tempo(self):
        for kit in KITS:
            K = kits.load(kit)
            R, tracks, bar = full(kit)
            self.assertEqual(R['bars'] * bar, tracks[0]['end'])
            seconds = R['bars'] * 240 / K.BPM
            self.assertTrue(360 <= seconds <= 480, f'{kit}: {seconds:.1f} s')
            self.assertEqual(round(seconds, 2), R['seconds'])
        # every genre's form fits every tempo the content rules allow a kit, 128-140 BPM
        for genre, form in Mi.FORMS.items():
            total = Mi.arrangement_bars(form)
            self.assertTrue(360 <= total * 240 / 140 and total * 240 / 128 <= 480, (genre, total))

    FORM_KINDS = {
        'default': ['intro', 'groove', 'break', 'build', 'drop', 'switch', 'break', 'drop2', 'outro'],
        'hyper-clean future house': ['intro', 'groove', 'break', 'build', 'drop', 'switch', 'break', 'drop2', 'outro'],
        'garage-house / deep tech house': ['intro', 'groove', 'break', 'build', 'drop', 'switch', 'break', 'drop2',
                                           'outro'],
        'dark industrial / techno': ['intro', 'groove', 'break', 'build', 'drop', 'drop2', 'outro'],
        'future garage / 2-step': ['intro', 'groove', 'break', 'build', 'drop', 'switch', 'break', 'drop2', 'outro']}

    def test_the_form(self):
        """Each genre's form (midi.FORMS): an intro and outro of 16 or 32 bars to mix in and out, a break, its build
        into the drop, a second drop; every section and every line in it starts on an 8-bar line, and the sections
        tile the mix."""
        self.assertEqual(set(self.FORM_KINDS), set(Mi.FORMS))
        for kit in KITS:
            _, R = ext(kit)
            K = kits.load(kit)
            kinds = [s['kind'] for s in R['sections']]
            self.assertEqual(self.FORM_KINDS.get(K.DIRECTION, self.FORM_KINDS['default']), kinds, kit)
            self.assertIn(R['sections'][0]['bars'], (16, 32))
            self.assertEqual(32, R['sections'][-1]['bars'])
            at = 1
            for s in R['sections']:
                with self.subTest(kit=kit, section=s['name']):
                    self.assertEqual(at, s['start_bar'])
                    self.assertEqual(0, (s['start_bar'] - 1) % Mi.LINE)
                    self.assertEqual(0, s['bars'] % Mi.LINE)
                    self.assertEqual([s['start_bar'] + Mi.LINE * i for i in range(s['bars'] // Mi.LINE)],
                                     [ln['start_bar'] for ln in s['lines']])
                at += s['bars']
            self.assertEqual(R['bars'] + 1, at)

    def test_the_regions_tile_the_full_arrangement_exactly(self):
        """Every region has the full file's tracks in the same order and ends on its last bar line, and its events,
        shifted to its start bar, are the full file's: the notes, the section marker and the cues."""
        for kit in KITS:
            made, R = ext(kit)
            _, ppq, whole = parse(made[R['file']])
            bar = 4 * ppq
            names = [t['name'] for t in whole[1:]]
            got, marks = {n: [] for n in names}, []
            for s in R['sections']:
                with self.subTest(kit=kit, region=s['file']):
                    _, _, reg = parse(made[s['file']])
                    self.assertEqual(names, [t['name'] for t in reg[1:]])
                    self.assertEqual(whole[0]['tempo'], reg[0]['tempo'])
                    off = (s['start_bar'] - 1) * bar
                    for t in reg:
                        self.assertEqual(s['bars'] * bar, t['end'])
                    for t in reg[1:]:
                        got[t['name']] += [(tick + off, st, a, b) for tick, st, a, b in t['notes']]
                    marks += [(tick + off, k, body) for tick, k, body in reg[0]['meta'] if k in (MARKER, CUE)]
            for t in whole[1:]:
                self.assertEqual(t['notes'], got[t['name']], (kit, t['name']))
            self.assertEqual([m for m in whole[0]['meta'] if m[1] in (MARKER, CUE)], marks, kit)

    def test_a_marker_names_every_section_at_its_start(self):
        for kit in KITS:
            made, R = ext(kit)
            _, tracks, bar = full(kit)
            want = [((s['start_bar'] - 1) * bar, s['name']) for s in R['sections']]
            self.assertEqual(want, [(t, b.decode()) for t, k, b in tracks[0]['meta'] if k == MARKER], kit)
            for s in R['sections']:
                meta = parse(made[s['file']])[2][0]['meta']
                self.assertEqual([(0, s['name'])], [(t, b.decode()) for t, k, b in meta if k == MARKER])

    def test_the_intro_and_outro_have_drums_only_bars_at_their_outer_ends(self):
        """The first and the last 8 bars: the kick in every bar, for a DJ to beatmatch on. On four on the floor the
        kick alone; on UK garage's 2-step kick (two or three hits a bar, none on beats 2 to 4) also the full swung
        hats and the perc lane (the shaker on every 16th), which carry the pulse; never the snare. The first and the
        last 16 bars: drums only, with nothing tonal sounding into them (no bass or harmony under the other track's)."""
        for kit in KITS:
            R, tracks, bar = full(kit)
            end, lanes = tracks[0]['end'], lane_of(R)
            garage = kits.load(kit).DIRECTION == 'future garage / 2-step'
            pulse = ('kick', 'hats', 'perc') if garage else ('kick',)
            for lo in (0, end - 8 * bar):
                for tr in tracks[1:]:
                    got = ons(tr, lo, lo + 8 * bar)
                    with self.subTest(kit=kit, track=tr['name'], from_bar=lo // bar + 1):
                        if lanes[tr['name']] in pulse:
                            self.assertEqual(set(range(8)), {(t - lo) // bar for t, _, _ in got})
                        else:
                            self.assertEqual([], got)
                if garage:                                          # every 16th has a hat or the shaker on it
                    steps = {(t - lo) // Mi.S16 for tr in tracks[1:] if lanes[tr['name']] in ('hats', 'perc')
                             for t, _, _ in ons(tr, lo, lo + 8 * bar)}
                    self.assertEqual(set(range(8 * Mi.BAR)), steps, kit)
            for tr in tracks[1:]:
                if lanes[tr['name']] not in Mi.DRUM_LANES:
                    with self.subTest(kit=kit, track=tr['name']):
                        self.assertGreaterEqual(min(t for t, _, _ in ons(tr)), 16 * bar)
                        self.assertLessEqual(max(offs(tr)), end - 16 * bar)

    def test_the_intro_brings_each_lane_in_on_an_8_bar_line(self):
        """Kick, then hats and perc, then the full drums; bass and harmony on 8- or 16-bar lines, inside the intro. The
        outro takes them out on its lines: each lane plays a run of its first lines, then rests to the end with nothing
        sounding on, the kick to the last bar; the harmony leaves first, the bass no later than the snare."""
        for kit in KITS:
            R, tracks, bar = full(kit)
            lanes = lane_of(R)
            intro, outro = R['sections'][0], R['sections'][-1]
            first = {}
            for tr in tracks[1:]:
                k = lanes[tr['name']]
                if ons(tr):
                    first[k] = min(first.get(k, 10 ** 9), ons(tr)[0][0] // bar)
            o0, span, n = (outro['start_bar'] - 1) * bar, Mi.LINE * bar, outro['bars'] // Mi.LINE
            plays = {k: [i for i in range(n) if lane_notes(R, tracks, k, o0 + i * span, o0 + (i + 1) * span)]
                     for k in Mi.LANES if has(R, k)}
            brk = next(s for s in R['sections'] if s['kind'] == 'break')['start_bar'] - 1
            garage = kits.load(kit).DIRECTION == 'future garage / 2-step'
            with self.subTest(kit=kit):
                self.assertEqual(0, first['kick'])
                for lane in ('hats', 'perc', 'snare'):
                    if has(R, lane):
                        self.assertLess(first[lane], intro['bars'], lane)    # the intro is drums
                for lane in ('hats', 'perc', 'snare', 'bass', 'chords'):
                    if has(R, lane):
                        self.assertLess(first[lane], brk, lane)             # every lane is in before the break
                        self.assertEqual(0, first[lane] % Mi.LINE, lane)
                for lane in ('hats', 'perc'):
                    if has(R, lane):                                        # the 2-step beat comes in as one
                        (self.assertLessEqual if garage else self.assertLess)(first[lane], first['snare'], lane)
                self.assertLessEqual(first['snare'], first['bass'])
                self.assertLessEqual(first['bass'], first['chords'])
                for lane, got in plays.items():
                    self.assertEqual(list(range(len(got))), got, lane)
                    ends = [t for tr in tracks[1:] if lanes[tr['name']] == lane for t in offs(tr) if t > o0]
                    self.assertLessEqual(max(ends, default=o0), o0 + len(got) * span, lane)
                self.assertEqual(list(range(n)), plays['kick'])
                self.assertLess(len(plays['chords']), len(plays['bass']))
                self.assertLessEqual(len(plays['bass']), len(plays['snare']))

    def test_the_breaks_have_no_kick(self):
        for kit in KITS:
            R = ext(kit)[1]
            for nth in range(sum(s['kind'] == 'break' for s in R['sections'])):
                s, tracks = region(kit, 'break', nth)
                with self.subTest(kit=kit, section=s['name']):
                    self.assertEqual([], lane_notes(R, tracks, 'kick'))
                    self.assertNotIn('kick', s['lanes'])
                    self.assertIn('chords', s['lanes'])
                    self.assertIn('lead', s['lanes'])

    def test_the_drop_plays_every_lane(self):
        for kit in KITS:
            R = ext(kit)[1]
            for kind in ('drop', 'drop2'):
                s, tracks = region(kit, kind)
                with self.subTest(kit=kit, section=s['name']):
                    self.assertEqual([k for k in Mi.LANES if has(R, k)], s['lanes'])
                    for lane in s['lanes']:
                        self.assertTrue(lane_notes(R, tracks, lane), lane)

    def test_drop_2_plays_another_bassline_and_busier_hats(self):
        for kit in KITS:
            R = ext(kit)[1]
            (d1, t1), (d2, t2) = region(kit, 'drop'), region(kit, 'drop2')
            with self.subTest(kit=kit):
                self.assertNotEqual(d1['lines'][0]['bass'], d2['lines'][0]['bass'])
                self.assertNotEqual(lane_notes(R, t1, 'bass'), lane_notes(R, t2, 'bass'))
                self.assertEqual(('full', 'busy'), (d1['lines'][0]['hats'], d2['lines'][0]['hats']))
                self.assertGreater(len(lane_notes(R, t2, 'hats')), len(lane_notes(R, t1, 'hats')))

    def test_the_build_thins_the_drums_and_rises(self):
        """The build drops drum lanes in its second half, raises the snare or clap bar by bar there (every bar it
        plays in), and carries the riser cue 8 bars before its end."""
        for kit in KITS:
            R = ext(kit)[1]
            s, tracks = region(kit, 'build')
            bar = 4 * Mi.PPQ
            half = s['bars'] // 2 * bar
            active = lambda lo, hi: {k for k in Mi.DRUM_LANES if lane_notes(R, tracks, k, lo, hi)}  # noqa: E731
            with self.subTest(kit=kit):
                self.assertLess(active(half, 2 * half), active(0, half))
                snare = lane_notes(R, tracks, 'snare', half)
                peaks = [max(v for t, _, v in snare if t // bar == b) for b in range(half // bar, 2 * half // bar)
                         if any(t // bar == b for t, _, _ in snare)]
                self.assertGreaterEqual(len(peaks), 7)
                self.assertEqual(sorted(set(peaks)), peaks)
                self.assertEqual(len(peaks), len(set(peaks)))
                cues = [(t, b.decode()) for t, k, b in tracks[0]['meta'] if k == CUE]
                self.assertIn((s['bars'] * bar - Mi.LINE * bar, 'Riser'), cues)

    def test_the_build_rolls_into_the_drop(self):
        """The build's second line rolls the snare or clap: a hit on every quarter for 4 bars, every 8th for 2, every
        16th for 1, on the lane's own note, rising from 55% to full velocity under the riser; its last bar is the
        written drums' fill bar, with a Fill cue for the kit's fill, whose end is the drop's first beat. The kick sits
        the line out, so the drop's first kick lands after 8 bars without one, but in future house, where it plays on
        under the roll, as the genre's builds do. regions.json says so."""
        for kit in KITS:
            K, P = Mi.parts(kit)
            M = Mi.material(K, P)
            R = ext(kit)[1]
            s, tracks = region(kit, 'build')
            bar = 4 * Mi.PPQ
            ln = s['lines'][-1]
            lo = (ln['start_bar'] - s['start_bar']) * bar
            house = K.DIRECTION in HOUSE_GENRES
            snare = lane_notes(R, tracks, 'snare', lo, lo + Mi.LINE * bar)
            steps = [sorted({(t - lo) % bar // Mi.S16 for t, _, _ in snare if (t - lo) // bar == b})
                     for b in range(Mi.LINE)]
            fb = M['fill_bar']
            written = sorted({n[0] - Mi.BAR * (fb - 1) for n in M['drums']['snare']
                              if Mi.BAR * (fb - 1) <= n[0] < Mi.BAR * fb})
            main = {n[1] for n in M['drums']['snare']}
            with self.subTest(kit=kit):
                self.assertEqual(('snare', 'fill'), (ln.get('roll'), ln.get('end')))
                self.assertEqual([[0, 4, 8, 12]] * 4 + [list(range(0, 16, 2))] * 2 + [list(range(16))], steps[:7])
                self.assertEqual(written, steps[7])
                self.assertLessEqual({n for t, n, _ in snare}, main)
                peaks = [max(v for t, _, v in snare if (t - lo) // bar == b) for b in range(7)]
                self.assertEqual(sorted(set(peaks)), peaks)
                self.assertAlmostEqual(0.55 * 127, peaks[0], delta=1.0)
                kick = lane_notes(R, tracks, 'kick', lo, lo + Mi.LINE * bar)
                if house:
                    self.assertIn('kick', ln['lanes'])
                    self.assertEqual(set(range(Mi.LINE)), {(t - lo) // bar for t, _, _ in kick})
                else:
                    self.assertNotIn('kick', ln['lanes'])
                    self.assertEqual([], kick)
                cues = [(t, b.decode()) for t, k, b in tracks[0]['meta'] if k == CUE]
                self.assertIn((lo, 'Riser'), cues)
                self.assertIn((lo + (Mi.LINE - 1) * bar, 'Fill'), cues)

    def test_each_line_plays_what_regions_json_says(self):
        """regions.json's per-line moves, read off the notes of the whole mix: a drum drop (no drum notes in the
        line's last bar, drums in the bar before); a fill (the line's last bar is the written drums' fill bar, in
        every drum lane the line plays, its hats thinned or busied as the line's are); thin hats (every hat on the
        off-beat 8th); busy hats (a hat on the "e" and the "a" of every beat the hats play)."""
        for kit in KITS:
            K, P = Mi.parts(kit)
            M = Mi.material(K, P)
            R, tracks, bar = full(kit)
            fb = M['fill_bar']
            written = {k: {(n[0] - Mi.BAR * (fb - 1), n[1]) for n in M['drums'][k]
                           if Mi.BAR * (fb - 1) <= n[0] < Mi.BAR * fb} for k in Mi.DRUM_LANES}
            ghost = M['ghost'][0]
            drums = lambda a, b: [n for k in Mi.DRUM_LANES for n in lane_notes(R, tracks, k, a, b)]  # noqa: E731
            for s in R['sections']:
                for ln in s['lines']:
                    lo = (ln['start_bar'] - 1) * bar
                    last = lo + (Mi.LINE - 1) * bar
                    hats = lane_notes(R, tracks, 'hats', lo, lo + Mi.LINE * bar)
                    with self.subTest(kit=kit, line=ln['start_bar']):
                        if ln.get('end') == 'drum drop':
                            self.assertEqual([], drums(last, last + bar))
                            self.assertTrue(drums(last - bar, last))
                        if ln.get('end') == 'fill':
                            for k in (k for k in ln['lanes'] if k in Mi.DRUM_LANES):
                                got = {((t - last) // Mi.S16, n)
                                       for t, n, _ in lane_notes(R, tracks, k, last, last + bar)}
                                want = written[k]
                                if k == 'hats' and ln.get('hats') == 'thin':
                                    want = {(st, n) for st, n in want if st % 4 == Mi.OFFBEAT}
                                if k == 'hats' and ln.get('hats') == 'busy':
                                    self.assertLessEqual(want, got)
                                    odd = {(st, n) for st, n in got - want if st % 2 == 0 or n != ghost}
                                    self.assertEqual(set(), odd, 'busy hats add ghosts on the e and the a only')
                                else:
                                    self.assertEqual(want, got, k)
                        if ln.get('hats') == 'thin':
                            self.assertTrue(hats)
                            self.assertEqual({Mi.OFFBEAT}, {(t - lo) // Mi.S16 % 4 for t, _, _ in hats})
                        if ln.get('hats') == 'busy':
                            steps = {(t - lo) // Mi.S16 for t, _, _ in hats}
                            for beat in {st // 4 for st in steps}:
                                self.assertLessEqual({4 * beat + 1, 4 * beat + 3}, steps, beat)

    def test_the_cues(self):
        """Cue points (FF 07) for the kit's audio: each riser ends on a section's first beat, each fill sits in the last
        bar of a line, every break has a vocal cue; regions.json lists them all."""
        for kit in KITS:
            R, tracks, bar = full(kit)
            starts = {(s['start_bar'] - 1) * bar for s in R['sections']}
            cues = [(t, b.decode()) for t, k, b in tracks[0]['meta'] if k == CUE]
            self.assertEqual([((c['bar'] - 1) * bar, c['cue']) for s in R['sections'] for c in s['cues']], cues)
            with self.subTest(kit=kit):
                self.assertLessEqual({c for _, c in cues}, set(Mi.CUES))
                for t, c in cues:
                    self.assertEqual(0, t % bar)
                    if c == 'Riser':
                        self.assertIn(t + Mi.LINE * bar, starts)
                    if c == 'Fill':
                        self.assertEqual(Mi.LINE - 1, t // bar % Mi.LINE)
                for s in R['sections']:
                    if s['kind'] == 'break':
                        self.assertIn('Vox', [c['cue'] for c in s['cues']], s['name'])
                after_break = [b for a, b in zip(R['sections'], R['sections'][1:])
                               if b['kind'] in ('drop', 'drop2') and a['kind'] in ('break', 'build')]
                self.assertEqual(len(after_break), sum(c == 'Riser' for _, c in cues))

    def test_every_drop_after_a_break_comes_through_its_build(self):
        """Drop 1 is reached through its build, which follows the break: the riser cue and the snare's rise end on
        the drop's first beat. Every drop that follows a break (or its build) starts where a riser ends."""
        for kit in KITS:
            R, tracks, bar = full(kit)
            secs = R['sections']
            risers = {c['bar'] + Mi.LINE for s in secs for c in s['cues'] if c['cue'] == 'Riser'}
            i = next(j for j, s in enumerate(secs) if s['kind'] == 'drop')
            with self.subTest(kit=kit):
                self.assertEqual(('break', 'build'), (secs[i - 2]['kind'], secs[i - 1]['kind']))
                self.assertIn(secs[i]['start_bar'], risers)
                rise = secs[i - 1]['lines'][-1]['rise']
                self.assertEqual(('snare', 1.0), (rise['lane'], rise['to']))
                for a, b in zip(secs, secs[1:]):
                    if b['kind'] in ('drop', 'drop2') and a['kind'] in ('break', 'build'):
                        self.assertIn(b['start_bar'], risers, b['name'])
                    if a['kind'] == 'build':
                        self.assertEqual('drop', b['kind'])

    def test_each_genre_has_its_form(self):
        """Techno: one break and grooves of 48 bars and more (the groove, and the drop running into drop 2); UK
        garage: a 16-bar intro of drums; sci-fi club the default form; future house the default's sections, its build
        keeping the kick under the roll."""
        for kit in KITS:
            K = kits.load(kit)
            R = ext(kit)[1]
            secs = R['sections']
            with self.subTest(kit=kit):
                self.assertIs(Mi.FORMS.get(K.DIRECTION, Mi.FORMS['default']), Mi.arrangement(K))
                if K.DIRECTION == 'dark industrial / techno':
                    self.assertEqual(1, sum(s['kind'] == 'break' for s in secs))
                    self.assertGreaterEqual(next(s for s in secs if s['kind'] == 'groove')['bars'], 48)
                    d = [s for s in secs if s['kind'] in ('drop', 'drop2')]
                    self.assertEqual(d[0]['start_bar'] + d[0]['bars'], d[1]['start_bar'])
                    self.assertGreaterEqual(d[0]['bars'] + d[1]['bars'], 48)
                elif K.DIRECTION == 'future garage / 2-step':
                    self.assertEqual(16, secs[0]['bars'])
                elif K.DIRECTION in HOUSE_GENRES:
                    house, club = Mi.FORMS[K.DIRECTION], Mi.FORMS['default']
                    self.assertIs(Mi.FORMS['hyper-clean future house'], house)
                    self.assertEqual([s[:2] for s in club], [s[:2] for s in house])
                    self.assertEqual([s for s in club if s[1] != 'build'], [s for s in house if s[1] != 'build'])
                    self.assertIn('kick', next(s for s in house if s[1] == 'build')[2][-1]['lanes'])
                else:
                    self.assertIs(Mi.FORMS['default'], Mi.arrangement(K))

    def test_only_the_kits_written_notes_play(self):
        """Every note is one of its lane's written notes: the drums by lane, the basslines, the lead phrase, the chord
        voicing and the chord sample's trigger. No note is made up."""
        for kit in KITS:
            K, P = Mi.parts(kit)
            R, tracks, _ = full(kit)
            M = Mi.material(K, P)
            allowed = {lane: {n for _, n, *_ in notes} for lane, notes in M['drums'].items()}
            allowed['bass'] = {n for line in P['lines'].values() for _, n, *_ in line}
            allowed['lead'] = {n for _, n, *_ in P['lead']['notes']}
            allowed['Chords'] = {n for _, n, *_ in P['chords']['notes']}
            allowed['Chord trigger'] = {n for _, n, *_ in Mi.chord_trigger(P)}
            lanes = lane_of(R)
            for tr in tracks[1:]:
                key = tr['name'] if tr['name'] in ('Chords', 'Chord trigger') else lanes[tr['name']]
                with self.subTest(kit=kit, track=tr['name']):
                    self.assertLessEqual({n for _, n, _ in ons(tr)}, allowed[key])

    def test_regions_json_lists_each_regions_lanes(self):
        """A section's lanes, and each line's, are exactly the lanes with notes there."""
        for kit in KITS:
            made, R = ext(kit)
            bar = 4 * Mi.PPQ
            for s in R['sections']:
                tracks = parse(made[s['file']])[2]
                playing = lambda lo, hi: [k for k in Mi.LANES if lane_notes(R, tracks, k, lo, hi)]  # noqa: E731
                with self.subTest(kit=kit, section=s['name']):
                    self.assertEqual(playing(0, s['bars'] * bar), s['lanes'])
                    for ln in s['lines']:
                        lo = (ln['start_bar'] - s['start_bar']) * bar
                        self.assertEqual(playing(lo, lo + ln['bars'] * bar), ln['lanes'], ln['start_bar'])

    def test_the_readme_names_every_file(self):
        for kit in KITS:
            made, R = ext(kit)
            text = made['README.md'].decode()
            for name in [R['file'], 'regions.json'] + [s['file'] for s in R['sections']]:
                self.assertIn(f'`{name}`', text, (kit, name))
            self.assertIn(f'`{Mi.EXTENDED}/`', Mi.files(kit)['README.md'].decode())

    def test_the_readme_says_which_octave_names_it_uses(self):
        """The Tracks table names the drums as drum racks do (C1 = 36) and the samples' roots as the sample names do
        (C4 = 60), which Ableton and Logic call C3: the README says so above the table, as the loop README does."""
        for kit in KITS:
            text = ext(kit)[0]['README.md'].decode()
            above = text[text.index('## Tracks'):text.index('| Track | Channel |')]
            for words in ('C1 = 36', 'C3 = 60', 'C4 = 60', 'MIDI number'):
                self.assertIn(words, above, kit)

    def test_strings_pass_the_denylists(self):
        strings = []
        for kit in KITS:
            made, R = ext(kit)
            strings += list(made) + [made['README.md'].decode(), made['regions.json'].decode()]
            for data in (d for n, d in made.items() if n.endswith('.mid')):
                tracks = parse(data)[2]
                strings += [tr['name'] for tr in tracks]
                strings += [b.decode() for t, k, b in tracks[0]['meta'] if k in (MARKER, CUE)]
        self.assertEqual([], [c.name for c in Q.failed(Q.check_content(names=strings))])

    def test_the_bytes_are_the_same_in_fresh_processes(self):
        """Two fresh processes with different hash seeds (string and set order) export the same bytes as this one."""
        code = ('import hashlib, sys; sys.path.insert(0, sys.argv[1]); from tools.sound import midi as Mi\n'
                'for kit in sys.argv[2:]:\n'
                '    made = {**Mi.files(kit), **{"x/" + k: v for k, v in Mi.extended(kit).items()}}\n'
                '    for n, d in sorted(made.items()):\n'
                '        print(kit, n, hashlib.sha256(d).hexdigest())\n')
        runs = [subprocess.run([sys.executable, '-c', code, str(REPO), *KITS], capture_output=True, text=True,
                               check=True, env={**os.environ, 'PYTHONHASHSEED': seed}).stdout
                for seed in ('0', '20261104')]
        here = ''.join(f'{kit} {n} {hashlib.sha256(d).hexdigest()}\n' for kit in KITS for n, d in
                       sorted({**Mi.files(kit), **{'x/' + k: v for k, v in ext(kit)[0].items()}}.items()))
        self.assertEqual(runs[0], runs[1])
        self.assertEqual(here, runs[0])

    def test_parts_without_the_new_fields_still_export(self):
        """A kit's PARTS needs none of the extended mix's optional fields: the drum lanes default by General MIDI
        number, the switch-up's and drop 2's basslines to the lines that aren't the pick. Set, the fields are used."""
        K = types.SimpleNamespace(KIT='FUT99', TITLE='Test Kit', BPM=132.0, KEY='A minor', CAMELOT='8A', SWING=0.55,
                                  RECIPES='tools.sound.recipes.fut99')
        bar = [(s, 36, 0.5, 1.0, False) for s in (0, 4, 8, 12)] + [(4, 39, 0.5, 0.9, False), (12, 37, 0.5, 0.9, False)]
        bar += [(s, 44, 0.5, 0.6, False) for s in range(2, 16, 4)] + [(s, 51, 0.5, 0.4, False) for s in (3, 11)]
        bar += [(7, 50, 0.5, 0.7, False)]
        P = dict(drums=[(s + 16 * b, *rest) for b in range(4) for s, *rest in bar],
                 drum_map={36: 'Kick', 37: 'Rim', 39: 'Clap', 44: 'Pedal Hat', 50: 'Tom', 51: 'Ride'},
                 drum_note='', lines={'One': [(2, 33, 1.0, 0.8, False)], 'Two': [(3, 45, 1.0, 0.8, False)],
                                      'Three': [(6, 40, 1.0, 0.8, False)]},
                 pick='Two', bass_samples='Bass', bass_zones='A1',
                 lead=dict(name='Lead', sample='Lead', zones='A4', notes=[(0, 69, 2, 1.0, False)]),
                 chords=dict(name='Pad', sample='Pad', notes=[(0, n, 64, 0.7, False) for n in (57, 60, 64)]))
        R = json.loads(Mi.extended_from('FUT99', K, P)['regions.json'])
        tracks = {t['lane']: t for t in R['tracks'] if t['lane'] in Mi.DRUM_LANES}
        self.assertEqual(['36'], list(tracks['kick']['notes']))
        self.assertEqual(['37', '39'], list(tracks['snare']['notes']))
        self.assertEqual(['44', '51'], list(tracks['hats']['notes']))
        self.assertEqual(['50'], list(tracks['perc']['notes']))
        self.assertEqual({'main': 'Two', 'switch': 'Three', 'drop2': 'One'}, R['basslines'])
        P = {**P, 'drum_lanes': {50: 'hats'}, 'switch_line': 'One', 'drop2_line': 'Three'}
        R = json.loads(Mi.extended_from('FUT99', K, P)['regions.json'])
        tracks = {t['lane']: t for t in R['tracks'] if t['lane'] in Mi.DRUM_LANES}
        self.assertEqual(['44', '50', '51'], list(tracks['hats']['notes']))
        self.assertEqual({}, tracks['perc']['notes'])
        self.assertEqual({'main': 'Two', 'switch': 'One', 'drop2': 'Three'}, R['basslines'])

    def test_synth_phrases_export_as_their_own_files(self):
        """PARTS['phrases'] (optional): each synth phrase is its own file, `<kit>_Phrase_<sample>_<bars>bars.mid`, on
        MIDI channel 4, ending on its last bar line, its notes as written (the kit's swing on the odd 16ths). The
        README lists each one with its sample and roots. A kit without phrases exports as before."""
        K = types.SimpleNamespace(KIT='FUT99', TITLE='Test Kit', BPM=132.0, KEY='A minor', CAMELOT='8A', SWING=0.5,
                                  RECIPES='tools.sound.recipes.fut99')
        P = dict(drums=[(0, 36, 0.5, 1.0, False)], drum_map={36: 'Kick'}, drum_note='', lines={'One': [(2, 33, 1.0, 0.8, False)]},
                 pick='One', bass_samples='Bass', bass_zones='A1',
                 lead=dict(name='Lead', sample='Lead', zones='A4', notes=[(0, 69, 2, 1.0, False)]),
                 chords=dict(name='Pad', sample='Pad', notes=[(0, n, 64, 0.7, False) for n in (57, 60, 64)]))
        plain = Mi.files_from('FUT99', K, P)
        self.assertFalse([n for n in plain if '_Phrase_' in n])
        riff = [(0, 57, 1, 1.0, False), (3, 60, 2, 0.8, False), (6, 64, 1, 0.9, True), (60, 62, 4, 0.7, False)]
        P2 = {**P, 'phrases': [dict(name='Organ riff', sample='Organ Riff', zones='A3', notes=riff, bars=4),
                               dict(name='Stab rhythm', sample='Stab A', zones='A4', notes=riff[:2], bars=2)]}
        out = Mi.files_from('FUT99', K, P2)
        self.assertEqual(set(plain) | {'FUT99_Phrase_OrganRiff_4bars.mid', 'FUT99_Phrase_StabA_2bars.mid'}, set(out))
        for name, n_bars, want in (('FUT99_Phrase_OrganRiff_4bars.mid', 4, riff),
                                   ('FUT99_Phrase_StabA_2bars.mid', 2, riff[:2])):
            self.assertEqual(n_bars, bars(name))
            _, _, tracks = parse(out[name])
            on = [(st, n) for tr in tracks for _, st, n, v in tr['notes'] if st & 0xF0 == 0x90 and v > 0]
            self.assertEqual(sorted(n for _, n, *_ in want), sorted(n for _, n in on), name)
            self.assertEqual({0x93}, {st for st, _ in on}, name)                     # note-ons on channel 4
            self.assertEqual({n_bars * 16 * Mi.S16}, {tr['end'] for tr in tracks}, name)  # every track ends on the bar
        text = out['README.md'].decode()
        self.assertIn('**Synth phrases**', text)
        self.assertIn('Organ Riff, rooted at A3 (MIDI 57)', text)
        self.assertNotIn('**Synth phrases**', plain['README.md'].decode())

    def test_a_chord_sample_with_a_file_per_chord_triggers_each_stab_on_its_file(self):
        """P['chords']['trigger'] (optional, with its 'zones'): a chord sample with one file per chord, each file the
        whole chord rooted at its note, plays from the kit's own trigger notes, one per stab on its chord's file, in
        the sample trigger file and in the extended mix's trigger track, and both READMEs and regions.json say which
        file each note plays. Without it, every stab plays the one file on TRIGGER_NOTE, as before. A trigger without
        its zones is refused."""
        K = types.SimpleNamespace(KIT='FUT99', TITLE='Test Kit', BPM=132.0, KEY='A minor', CAMELOT='8A', SWING=0.5,
                                  RECIPES='tools.sound.recipes.fut99', DIRECTION='sci-fi club')
        stabs = [(0, 57, 0.9), (6, 57, 0.7), (32, 62, 1.0), (38, 62, 0.8)]       # Am9 on A3, then Dm9 on D4
        shape = (0, 3, 7, 10, 14)
        P = dict(drums=[(0, 36, 0.5, 1.0, False)], drum_map={36: 'Kick'}, drum_note='',
                 lines={'One': [(2, 33, 1.0, 0.8, False)]}, pick='One', bass_samples='Bass', bass_zones='A1',
                 lead=dict(name='Lead', sample='Lead', zones='A4', notes=[(0, 69, 2, 1.0, False)]),
                 chords=dict(name='Stabs, Am9 and Dm9', sample='Stab',
                             notes=[(s, z + d, 1, v, False) for s, z, v in stabs for d in shape]))
        trigger = [(s, z, 1, v, False) for s, z, v in stabs]
        zoned = {**P, 'chords': {**P['chords'], 'trigger': trigger, 'zones': 'A3, D4'}}
        name = 'FUT99_Chords_Stab_SampleTrigger_4bars.mid'
        for parts, want in ((P, [Mi.TRIGGER_NOTE] * 4), (zoned, [57, 57, 62, 62])):
            out = Mi.files_from('FUT99', K, parts)
            _, ppq, tracks = parse(out[name])
            on = sorted((t, st, n) for tr in tracks for t, st, n, v in tr['notes'] if st & 0xF0 == 0x90 and v > 0)
            self.assertEqual(want, [n for _, _, n in on])
            self.assertEqual([s * ppq // 4 for s, _, _ in stabs], [t for t, _, _ in on])   # one note per stab
            self.assertEqual({0x92}, {st for _, st, _ in on})                            # on the chords' channel 3
            self.assertEqual(sorted(trigger) if parts is zoned else sorted({(s, Mi.TRIGGER_NOTE, 1, v, False)
                                                                           for s, _, v in stabs}),
                             Mi.material(K, parts)['trigger'])
        plain, files = Mi.files_from('FUT99', K, P)['README.md'].decode(), Mi.files_from('FUT99', K, zoned)
        text = files['README.md'].decode()
        self.assertIn(f'one note on {Mi.note_name(Mi.TRIGGER_NOTE)}', plain)
        self.assertNotIn('one file per chord', plain)
        self.assertIn('one file per chord', text)
        self.assertIn('A3, D4 (MIDI 57, 62)', text)
        self.assertNotIn(f'one note on {Mi.note_name(Mi.TRIGGER_NOTE)}', text)
        made = Mi.extended_from('FUT99', K, zoned)
        R = json.loads(made['regions.json'])
        self.assertEqual({'57': 'Stab A3', '62': 'Stab D4'},
                         next(t for t in R['tracks'] if t['track'] == 'Chord trigger')['notes'])
        self.assertIn('one file per chord', made['README.md'].decode())
        R = json.loads(Mi.extended_from('FUT99', K, P)['regions.json'])
        self.assertEqual({str(Mi.TRIGGER_NOTE): 'Stab'},
                         next(t for t in R['tracks'] if t['track'] == 'Chord trigger')['notes'])
        no_zones = {**P, 'chords': {**P['chords'], 'trigger': trigger}}
        with self.assertRaises(ValueError):
            Mi.files_from('FUT99', K, no_zones)
        with self.assertRaises(ValueError):
            Mi.material(K, no_zones)

    def test_the_fill_bar_plays_as_written_and_other_blocks_swap_it(self):
        """The written 4-bar drums leave their last bar to the kit's fill. Where no fill is due, bar 2 plays again in
        its place, so the groove runs on; where one is (a Fill cue), the 4 bars play as written."""
        n = lambda s, note: (s, note, 0.5, 1.0, False)  # noqa: E731
        pattern = [n(0, 36), n(4, 38), n(16, 36), n(23, 36), n(32, 36), n(40, 42), n(48, 36)]
        self.assertEqual(pattern, Mi.block(pattern, True))
        self.assertEqual([n(0, 36), n(4, 38), n(16, 36), n(23, 36), n(32, 36), n(40, 42), n(48, 36), n(55, 36)],
                         Mi.block(pattern, False))
        self.assertEqual([n(0, 36), n(4, 38), n(16, 36), n(23, 36), n(32, 36), n(36, 38), n(48, 36)],
                         Mi.block(pattern, False, fill_bar=3, swap=1))


if __name__ == '__main__':
    unittest.main()
