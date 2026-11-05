"""The phrase tables (tools/sound/phrase_tables.py, ADR 0020): the lanes the recipes synthesize at render time, played
by Wubansia from their recorded stems sliced per note.

The contract is docs/plugin/manifest.md section 2.6. What these tests hold:
- every table is well formed: slices by step, (step, key) unique, each starting at phrase.Grid.at(step) less its lead,
  the slices of a stem contiguous and covering it, integers only;
- each table's notes are the recipe's notes of its stem;
- the reference test: placing the stem by its table at the Loop file's notes reproduces the recipe's lane before its
  inserts, nulling at -50 dB or lower on the second pass, for each variant and bass character. The Loop notes come from
  the recipes here (as tools/sound/tests/test_midi.py's expected_loop reads them), and from the committed Loop files
  once MIDI v2 has landed;
- the derived stems re-render byte for byte;
- the matcher (the Python model of engine/sched/phrase_match) resolves every cycle of the Loop files.

The audio tests need the real WAVs: $WUBANSIA_CONTENT_DIR (a folder of <kit>/ folders) or the checkout's
content/samples with git lfs pulled. They skip without them.
"""
import functools
import hashlib
import os
import tempfile
import unittest
from pathlib import Path

import numpy as np

from tools.sound import dsp48 as D
from tools.sound import kits, listen
from tools.sound import phrase as Ph
from tools.sound import phrase_tables as PT
from tools.sound.item import Out

REPO = Path(__file__).resolve().parents[3]
KITS = ('FUT01', 'FUT02', 'FUT03', 'FUT04')
NULL_DB = -50.0


# ------------------------------------------------------------------ the samples, when they are here
def sample_roots():
    roots = [Path(p) for p in (os.environ.get('WUBANSIA_CONTENT_DIR'),) if p]
    return roots + [REPO / 'content' / 'samples']


def is_wav(path):
    try:
        with open(path, 'rb') as f:
            return f.read(4) == b'RIFF'
    except OSError:
        return False


def sample(kit, rel):
    """The real WAV of a kit's file (its path from the kit's folder), or None (missing, or an LFS pointer)."""
    for root in sample_roots():
        p = root / kit.lower() / rel
        if is_wav(p):
            return p
    return None


def read(path):
    import soundfile as sf
    y, sr = sf.read(path, dtype='float64')
    assert sr == 48000, path
    return D.st(y)


def wav_kit_files(K, name):
    """phrase.kit_files from the committed WAVs (bit-identical to the FLAC masters render.py writes), for the recipes'
    lane functions: (item, {(note, layer, rr): Out}, gain dB unknown). Skips the test when a file isn't here."""
    item = kits.items(K, only=(name,))[0]
    takes = item.takes(K.KIT)
    lay, rrs = len(item.layers) > 1, item.rr > 1
    got = {}
    for t in takes:
        rel = f'{listen.FOLDERS[item.group]}/{listen.plain(t.file, lay, rrs)}'
        p = sample(K.KIT, rel)
        if p is None:
            raise unittest.SkipTest(f'{K.KIT}/{rel} is not here (set WUBANSIA_CONTENT_DIR or git lfs pull)')
        got[(t.note, t.layer, t.rr)] = Out(read(p))
    return item, got, float('nan')


class WavFiles:
    """Run the recipes' lane functions on the committed WAVs instead of rendering their items."""

    def __enter__(self):
        self.real = Ph.kit_files
        Ph.kit_files = wav_kit_files
        return self

    def __exit__(self, *exc):
        Ph.kit_files = self.real


def stem_audio(kit, path):
    p = sample(kit, path)
    if p is None:
        raise unittest.SkipTest(f'{kit}/{path} is not here (set WUBANSIA_CONTENT_DIR or git lfs pull)')
    return read(p)


def null_db(got, want):
    return 10 * np.log10(np.sum((got - want) ** 2) / np.sum(want ** 2) + 1e-300)


# ------------------------------------------------------------------ the recipes' lanes and the Loop files' notes
def twice(notes, period=32):
    return [(s + period * r, k) for r in (0, 1) for s, k in notes]


@functools.lru_cache(maxsize=None)
def reference(kit, line, variant, char):
    """The recipe's lane before its inserts, and the notes the Loop file plays on it: (lane (L, 2), [(16th, key)]),
    or None where the variant has no such lane. 16ths may be fractions (FUT04's ratchets, retriggered). The lanes
    with a derived stem render from their recipes (synthesis); the others read the committed WAVs."""
    if (kit, line) == ('FUT02', 'vox'):
        from tools.sound.recipes.fut02 import context5 as C5, voxsynth as Vs
        if variant != 'vox':
            return None
        main, throw = Vs.sung_parts(C5.G, C5.RIFF_AT, C5.L, lambda *k: D.rng(C5.K.KIT, 'ContextVoxRiff', *k))
        return main + throw, [(s + C5.RIFF_AT, n) for s, n, _, _ in Vs.RIFF]
    if (kit, line) == ('FUT03', 'vox'):
        from tools.sound.recipes.fut03 import context5 as C5, voxsynth as VS
        if variant != 'vox':
            return None
        return C5.vox_lane(), [(s + C5.VOX_AT, m) for s, m, _, _ in VS.VOX_RIFF]
    with WavFiles():
        if kit == 'FUT01':
            from tools.sound.recipes.fut01 import context4 as C, context5 as C5, music as M, voxsynth as V
            if line == 'pluck':
                return C.pluck_lane(C.G), twice([(s, n) for s, n, _, _ in M.PHRASE])
            if line == 'riff':
                if not C5.RIFF_AT[variant]:
                    return None
                notes = [(s + a, n) for a in C5.RIFF_AT[variant] for s, n, *_ in V.RIFF]
                return C5.riff_lane(C.G, C5.RIFF_AT[variant]), notes
        if kit == 'FUT02':
            from tools.sound.recipes.fut02 import bass as B, context as C, lead as Ld
            if line == 'orbit':
                stem = Ph.kit_files(C.K, f"OrbitLine{char.title()}")[1][(None, 1, 1)].audio
                notes = [(s, k) for s, k in twice([(s, n) for s, n, *_ in B.ORBIT]) if not 48 <= s < 56]
                return C.tile(stem) * C.mute_mask()[:, None], notes
            if line == 'arp':
                stem = Ph.kit_files(C.K, 'ChromeArpPhrase')[1][(None, 1, 1)].audio
                notes = [(s, k) for s, k in twice([(s, n) for s, n, _, _ in Ld.phrase_notes()])
                         if s < C.ARP_REST[0]]
                return C.tile(stem) * C.rest_mask()[:, None], notes
        if kit == 'FUT03':
            from tools.sound.recipes.fut03 import context as C, rubber as R
            if line == 'rubber':
                notes = [(s, n) for s, _, n, _, _ in R.LINES['Rubber']]
                notes += [(s + ln * g[1], g[0]) for s, ln, _, _, g in R.LINES['Rubber'] if g]   # the in-note glides
                return C.bass_lane(char), twice(notes)
        if kit == 'FUT04':
            from tools.sound.recipes.fut04 import bass as B, context as C, lead as Ld
            if line == 'acid':
                ln = B.LINE_BY_NAME[B.PICK]
                return C.bass_raw(char), twice([(s, n) for s, n, *_ in tuple(ln.notes) + tuple(ln.ticks)])
            if line == 'blip':
                notes = [(s + 32 * r + k / rt, n) for r in (0, 1) for s, n, _, _, rt in Ld.PHRASE for k in range(rt)]
                return C.lead_lane(), notes
    raise KeyError((kit, line))


# the lanes the Loop files play from recorded phrases: (kit, the table's line, its recipe reference)
PLAYED = (('FUT01', 'pluck'), ('FUT01', 'riff'), ('FUT02', 'orbit'), ('FUT02', 'arp'), ('FUT02', 'vox'),
          ('FUT03', 'rubber'), ('FUT03', 'vox'), ('FUT04', 'acid'), ('FUT04', 'blip'))


def spec(kit, line):
    return next(s for s in PT.specs(kit) if s.line == line)


def stem_frames(K, s):
    """A stem's frames as the spec gives them (a Loops/ stem is its cycle long; a derived one its own length)."""
    return s.frames if s.derived else PT.cycle_frames(K, s.cycle)


def table_of(K, s):
    """The spec's table, its stems indexed 0 (A) and 1 (B)."""
    index = {p: i for i, p in enumerate(p for p in s.stems if p)}
    return PT.table(s, index, {p: stem_frames(K, s) for p in index})


# ------------------------------------------------------------------ the tables
class TestTables(unittest.TestCase):
    def test_every_kit_has_its_phrase_lanes(self):
        """FUT02-04's bass lines, FUT01/02/04's leads and FUT01-03's vox (the lanes the recipes synthesize at render
        time), plus the alternate lines that have their own Loops/ files (FUT02's Relay, FUT03's Deep)."""
        want = {'FUT01': {('lead', 2), ('vox', 5)},
                'FUT02': {('bass', 1), ('lead', 2), ('vox', 5)},
                'FUT03': {('bass', 1), ('vox', 5)},
                'FUT04': {('bass', 1), ('lead', 2)}}
        for kit in KITS:
            self.assertEqual(want[kit], {(s.lane, s.ch) for s in PT.specs(kit)}, kit)
        self.assertEqual(['orbit', 'relay'], [s.line for s in PT.specs('FUT02') if s.ch == 1])
        self.assertEqual(['rubber', 'deep'], [s.line for s in PT.specs('FUT03') if s.ch == 1])
        self.assertEqual({s.cycle for s in PT.specs('FUT02') if s.ch == 1}, {32})

    def test_the_notes_are_the_recipes(self):
        from tools.sound.recipes.fut01 import music as M, voxsynth as V1
        from tools.sound.recipes.fut02 import bass as B2, lead as L2, voxsynth as V2
        from tools.sound.recipes.fut03 import rubber as R3, voxsynth as V3
        from tools.sound.recipes.fut04 import bass as B4, lead as L4
        acid = B4.LINE_BY_NAME[B4.PICK]
        acid = sorted([(s, n) for s, n, *_ in acid.notes] + [(s, n) for s, n, *_ in acid.ticks])
        want = {
            ('FUT01', 'pluck'): [(s, n) for s, n, _, _ in M.PHRASE],
            ('FUT01', 'riff'): [(s, n) for s, n, *_ in V1.RIFF],
            ('FUT02', 'orbit'): [(s, n) for s, n, *_ in B2.ORBIT],
            ('FUT02', 'relay'): [(s, n) for s, n, *_ in B2.RELAY],
            ('FUT02', 'arp'): [(s, n) for s, n, _, _ in L2.phrase_notes()],
            ('FUT02', 'vox'): [(s + 16, n) for s, n, _, _ in V2.RIFF],
            ('FUT03', 'rubber'): [(s, n) for s, _, n, _, _ in R3.LINES['Rubber']],
            ('FUT03', 'deep'): [(s, n) for s, _, n, _, _ in R3.LINES['Deep']],
            ('FUT03', 'vox'): [(s + 32, m) for s, m, _, _ in V3.VOX_RIFF],
            ('FUT04', 'acid'): acid + [(s + 32, n) for s, n in acid],
            ('FUT04', 'blip'): [(s, n) for s, n, *_ in L4.PHRASE],
        }
        for (kit, line), notes in want.items():
            self.assertEqual(notes, [(s, k) for s, k, _ in spec(kit, line).notes], (kit, line))
        self.assertEqual([(n,) * (r - 1) for _, n, _, _, r in L4.PHRASE],
                         [h for _, _, h in spec('FUT04', 'blip').notes])
        self.assertEqual([(g[0],) if g else () for *_, g in R3.LINES['Rubber']],
                         [h for _, _, h in spec('FUT03', 'rubber').notes])

    def test_slices_are_well_formed(self):
        """By step, (step, key) unique; start = Grid.at(step) - lead, contiguous, the first from frame 0 where its
        lead can hold the frames before it, the last to the stem's end; fade between start and end; all integers."""
        for kit in KITS:
            K = kits.module(kit)
            for s in PT.specs(kit):
                frames = PT.cycle_frames(K, s.cycle) if not s.derived else s.frames
                sl = PT.slices(s, frames)
                with self.subTest(kit=kit, line=s.line):
                    g = Ph.Grid(K, s.cycle, PT.cycle_frames(K, s.cycle))
                    self.assertEqual(sorted(sl, key=lambda x: x['step']), sl)
                    self.assertEqual(len(sl), len({(x['step'], x['key']) for x in sl}))
                    for x in sl:
                        self.assertTrue(all(type(v) is int for k, v in x.items() if k != 'holds'), x)
                        self.assertTrue(all(type(v) is int for v in x['holds']), x)
                        self.assertEqual(g.at(x['step']) - x['lead'], x['start'])
                        self.assertLessEqual(x['start'], x['fade'])
                        self.assertLessEqual(x['fade'], x['end'])
                        self.assertLess(x['start'], x['end'])
                        self.assertTrue(0 <= x['lead'] <= 48000)
                        self.assertLessEqual(len(x['holds']), 8)
                    for a, b in zip(sl, sl[1:]):
                        self.assertEqual(a['end'], b['start'])
                    self.assertEqual(frames, sl[-1]['end'])
                    if not s.derived:
                        self.assertEqual(0, sl[0]['start'], 'a looped stem: its first slice holds the wrapped tail')

    def test_bass_slices_break_on_the_kicks(self):
        """The bass lines rest on every kick: a slice that follows a kick starts on it, so one played alone starts
        and ends in the line's silence (and FUT02's slice before its glide mute fades on the recipe's mute edge)."""
        from tools.sound.recipes.fut02 import context as C2
        K = kits.module('FUT02')
        s = spec('FUT02', 'orbit')
        sl = PT.slices(s, PT.cycle_frames(K, 32))
        g = Ph.Grid(K, 32, PT.cycle_frames(K, 32))
        before = next(x for x in sl if x['step'] == 14)
        self.assertEqual(g.at(16), before['end'])
        mute_edge = C2.G.at(C2.GLIDE[0]) - int(C2.MUTE_EDGE * D.SR) - C2.G.at(32)
        self.assertEqual(mute_edge, before['fade'])
        self.assertEqual(5000, s.fade_us)

    def test_the_arp_fades_into_its_rest_as_the_recipe(self):
        """FUT02's arp rests from 16th 48: its last slice fades over the recipe's 10 ms (context.ARP_REST) to the
        rest's start, and comes back on the downbeat over the phrase voice's 3 ms crossfade, the recipe's."""
        from tools.sound.recipes.fut02 import context as C2
        s = spec('FUT02', 'arp')
        self.assertEqual(int(C2.ARP_REST[1] * 1e6), s.fade_us)
        self.assertEqual(int(C2.ARP_REST[2] * 1e6), PT.XFADE_US)

    def test_tables_follow_the_contract(self):
        """Key order, units, A/B and stems as docs/plugin/manifest.md section 2.6 has them."""
        keys = ['lane', 'ch', 'ab', 'mix_channel', 'gain_mdb', 'stems', 'cycle_16ths', 'tolerance_pm', 'fade_us',
                'slices']
        for kit in KITS:
            K = kits.module(kit)
            for s in PT.specs(kit):
                t = table_of(K, s)
                with self.subTest(kit=kit, line=s.line):
                    self.assertEqual(keys, list(t))
                    self.assertEqual(['a', 'b'], list(t['stems']))
                    self.assertEqual(['step', 'key', 'start', 'end', 'fade', 'lead', 'holds'], list(t['slices'][0]))
                    self.assertEqual(20, t['tolerance_pm'])
                    self.assertEqual(t['ab'] == '', t['stems']['b'] == -1)
                    self.assertEqual(0, t['stems']['a'])
                    self.assertEqual(s.lane == 'bass', t['ab'] == 'bass')
                    self.assertIn(t['cycle_16ths'], (32, 64))

    def test_every_phrase_says_why(self):
        """Each phrase lane records the recipe function that synthesizes it, and its stem is a Loops/ file or a
        derived stem in Stems/."""
        for kit in KITS:
            for s in PT.specs(kit):
                self.assertRegex(s.why, r'^tools\.sound\.recipes\.fut0\d\.\w+\.\w+', (kit, s.line))
                for p in s.stems:
                    if p:
                        self.assertTrue(p.startswith('Stems/' if s.derived else 'Loops/'), p)


# ------------------------------------------------------------------ the reference test
class TestNulls(unittest.TestCase):
    def test_each_stem_placed_by_its_table_is_the_recipes_lane(self):
        """At the Loop file's notes, the stem placed by its table (the phrase voice's rules: contiguous slices read
        on, a slice with no follower fades from its fade frame, a new run comes in over 3 ms) nulls against the
        recipe's lane before inserts at -50 dB or lower on the second pass (round the loop, tails wrapped), for each
        variant and bass character."""
        for kit, line in PLAYED:
            K = kits.module(kit)
            s = spec(kit, line)
            chars = ('clean', 'warm') if s.ab == 'bass' else ('clean',)
            for variant in ('vox', 'novox'):
                for char in chars:
                    ref = reference(kit, line, variant, char)
                    if ref is None:
                        continue
                    lane, notes = ref
                    path = s.stems[1] if char == 'warm' and s.stems[1] else s.stems[0]
                    with self.subTest(kit=kit, line=line, variant=variant, char=char):
                        if sample(kit, path) is None:
                            self.skipTest(f'{kit}/{path} is not here (set WUBANSIA_CONTENT_DIR or git lfs pull)')
                        stem = stem_audio(kit, path)
                        self.assertEqual(stem_frames(K, s), len(stem))
                        t = table_of(K, s)
                        placed = [(PT.tick_16th(K, PT.step_tick(K, p)), k) for p, k in notes]
                        resolved, unmatched = PT.resolve(K, placed, [t])
                        self.assertEqual([], unmatched)
                        got = PT.play(K, t, stem * D.undb(t['gain_mdb'] / 1000.0), resolved, len(lane))
                        self.assertLessEqual(null_db(got, D.st(lane)), NULL_DB)


class TestDerivedStems(unittest.TestCase):
    def test_derived_stems_render_byte_for_byte(self):
        """FUT02's and FUT03's vox stems (Stems/), rendered again from their recipes, equal the committed WAVs byte
        for byte (where the WAVs are pulled)."""
        for kit in KITS:
            for path in PT.DERIVED.get(kit, ()):
                have = sample(kit, path)
                if have is None:
                    continue
                with tempfile.TemporaryDirectory() as d, self.subTest(kit=kit, path=path):
                    out = PT.write_derived(kit, Path(d))
                    self.assertEqual(hashlib.sha256(have.read_bytes()).hexdigest(),
                                     hashlib.sha256((Path(d) / kit.lower() / path).read_bytes()).hexdigest())
                    self.assertIn(path, out)

    def test_derived_stems_fit_24_bits(self):
        """A derived stem is the recipe's lane at the stem's gain (a whole dB): its true peak at -1 dBTP or under,
        and the table's gain_mdb gives the lane its level back."""
        for kit, line in (('FUT02', 'vox'), ('FUT03', 'vox')):
            s = spec(kit, line)
            self.assertTrue(s.derived)
            self.assertEqual(0, s.gain_mdb % 1000)
            audio = PT.render_derived(kit)[s.stems[0]]
            from tools.sound import mixmeter as MM
            self.assertLessEqual(MM.true_peak_db(audio), -1.0, kit)


# ------------------------------------------------------------------ the matcher's model
class TestMatcher(unittest.TestCase):
    def table(self, kit, line):
        K = kits.module(kit)
        return K, table_of(K, spec(kit, line))

    def test_a_full_cycle_matches_every_slice(self):
        K, t = self.table('FUT02', 'orbit')
        notes = [(s['step'] + 32, s['key']) for s in t['slices']]
        got, unmatched = PT.resolve(K, notes, [t])
        self.assertEqual([], unmatched)
        self.assertEqual(len(t['slices']), len(got))

    def test_a_call_matches_its_subset(self):
        """A 'call' (the first bar only) plays its slices; the rest of the cycle is silent."""
        K, t = self.table('FUT01', 'pluck')
        notes = [(s['step'], s['key']) for s in t['slices'] if s['step'] < 16]
        got, unmatched = PT.resolve(K, notes, [t])
        self.assertEqual([], unmatched)

    def test_a_transposed_cycle_falls_back(self):
        K, t = self.table('FUT01', 'pluck')
        notes = [(s['step'], s['key'] + 2) for s in t['slices']]
        got, unmatched = PT.resolve(K, notes, [t])
        self.assertEqual([], got)
        self.assertEqual(len(notes), len(unmatched))

    def test_two_lines_on_one_channel_resolve_to_the_one_with_more_matches(self):
        K = kits.module('FUT02')
        tables = [table_of(K, spec('FUT02', line)) for line in ('orbit', 'relay')]
        relay = [(s['step'], s['key']) for s in tables[1]['slices']]
        got, unmatched = PT.resolve(K, relay, tables)
        self.assertEqual([], unmatched)
        self.assertEqual({1}, {p for _, p, _ in got})

    def test_retriggers_belong_to_their_slice(self):
        """FUT04's ratchets: the retriggers of a slice's key inside its 16th are the slice's own (holds), not notes
        of their own; a fourth retrigger is one too many."""
        K, t = self.table('FUT04', 'blip')
        notes = [(10, 74), (10.5, 74), (26, 84), (26 + 1 / 3, 84), (26 + 2 / 3, 84)]
        got, unmatched = PT.resolve(K, notes, [t])
        self.assertEqual([], unmatched)
        self.assertEqual(2, len(got))
        got, unmatched = PT.resolve(K, notes + [(26.9, 84)], [t])
        self.assertEqual([(26.9, 84)], unmatched)

    def test_in_note_glides_belong_to_their_slice(self):
        """FUT03's glides inside a note (MIDI v2 writes the target slid into at a fraction of the note): the target
        is the slice's own, even in the next 16th; the same key elsewhere is a note of its own."""
        K, t = self.table('FUT03', 'rubber')
        line = [(s['step'], s['key']) for s in t['slices']]
        glides = [(14.39, 31), (26.1, 31), (30.36, 38)]
        got, unmatched = PT.resolve(K, line + glides, [t])
        self.assertEqual([], unmatched)
        self.assertEqual(len(line), len(got))
        got, unmatched = PT.resolve(K, line + [(12, 31)], [t])
        self.assertEqual([(12, 31)], unmatched)


# ------------------------------------------------------------------ the committed Loop files (MIDI v2)
def loop_file(kit, variant):
    p = REPO / 'content' / 'midi' / kit.lower() / f"{kit}_Loop_{'Vox' if variant == 'vox' else 'NoVox'}_4bars.mid"
    return p if p.exists() else None


class TestLoopFiles(unittest.TestCase):
    def test_every_cycle_of_the_loop_files_resolves_to_slices(self):
        """Every note of every phrase lane's channel in both Loop files resolves to a slice of one of its tables, at
        every bass character (waiting on P1's MIDI v2 Loop files)."""
        if loop_file('FUT01', 'vox') is None:
            self.skipTest('waiting on P1: MIDI v2 Loop files (content/midi/<kit>/<KIT>_Loop_Vox_4bars.mid)')
        for kit in KITS:
            K = kits.module(kit)
            for variant in ('vox', 'novox'):
                notes = PT.file_notes(K, loop_file(kit, variant))
                for ch in sorted({s.ch for s in PT.specs(kit)}):
                    tables = [table_of(K, s) for s in PT.specs(kit) if s.ch == ch]
                    _, unmatched = PT.resolve(K, notes.get(ch, []), tables)
                    with self.subTest(kit=kit, variant=variant, ch=ch):
                        self.assertEqual([], unmatched)

    def test_the_loop_files_play_the_recipes_notes(self):
        """The phrase lanes' notes in the committed Loop files are the ones this test file reads off the recipes."""
        if loop_file('FUT01', 'vox') is None:
            self.skipTest('waiting on P1: MIDI v2 Loop files')
        for kit, line in PLAYED:
            K = kits.module(kit)
            s = spec(kit, line)
            for variant in ('vox', 'novox'):
                ref = reference(kit, line, variant, 'clean')
                got = sorted((t, k) for t, ch, k, _ in PT.smf_notes(loop_file(kit, variant).read_bytes())
                             if ch == s.ch)
                want = [] if ref is None else sorted((PT.step_tick(K, p), k) for p, k in ref[1])
                with self.subTest(kit=kit, line=line, variant=variant):
                    self.assertEqual(want, got)


if __name__ == '__main__':
    unittest.main()
