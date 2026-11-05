"""The phrase tables (tools/sound/phrase_tables.py, ADR 0020): the lanes the recipes synthesize at render time, played
by Wubansia from their recorded stems sliced per note.

The contract is docs/plugin/manifest.md section 2.6. What these tests hold:
- every table is well formed: slices by step, (step, key) unique, each starting at phrase.Grid.at(step) less its lead,
  integers only, the slices of a stem tiling it. A looped stem is read round: its last slice owns the tail that wraps
  to the stem's start, and its slice 0 starts at its own pre-roll (before the cycle's start, at the stem's end, where
  that is where its pre-roll starts);
- each table's notes are the recipe's notes of its stem, and its throw is "baked" exactly where the stem has its
  throw in the audio;
- the phrase voice's model (play()): a run's fade-in (0.5 ms on an onset, 3 ms through a pre-roll), a wrapping slice
  read round the stem, the last slice reading on into the next cycle's slice 0;
- the reference test: the model playing the stem by its table at the Loop file's notes, pass after pass from silence,
  reproduces the recipe's lane before its inserts one pass at a time (the first from where the voice has come in, the
  second whole), nulling at -50 dB or lower, for each variant and bass character. The Loop notes come from the recipes
  here (as tools/sound/tests/test_midi.py's expected_loop reads them), and from the committed Loop files once MIDI v2
  has landed;
- the derived stems re-render byte for byte;
- FUT04's acid phrases carry the controller rides their stems were rendered with, and the Loop files' rides equal
  the Ride stem's;
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
from tools.sound import plugin_common as PC
from tools.sound.item import Out

REPO = Path(__file__).resolve().parents[3]
KITS = ('FUT01', 'FUT02', 'FUT03', 'FUT04')
NULL_DB = -50.0
TWICE = (0, 32)     # the extended mixes' call, riff and answer vox lines start FUT02's and FUT03's riff here
LOOP_16THS = 64     # a Loop file: 4 bars
# the stems with their throw in the audio (the engine sends their slice notes no live throw): FUT01's vox-synth riff
# (its last chop's dotted-8th ping-pong and plate) and FUT03's riff (its dub echo and dub throw). FUT02's riff stems
# hold the throw note dry: the loop sends it, and so does the engine
BAKED = {('FUT01', 'riff'), ('FUT03', 'vox'), ('FUT03', 'vox_twice')}


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
    if (kit, line) == ('FUT02', 'vox_twice'):     # the extended mix's riff line: the riff from 16ths 0 and 32
        from tools.sound.recipes.fut02 import context5 as C5, voxsynth as Vs
        lane = sum(sum(Vs.sung_parts(C5.G, at, C5.L, lambda *k: D.rng(C5.K.KIT, 'ContextVoxRiff', *k)))
                   for at in TWICE)
        return lane, [(s + at, n) for at in TWICE for s, n, _, _ in Vs.RIFF]
    if (kit, line) == ('FUT03', 'vox_twice'):
        from tools.sound.recipes.fut03 import context5 as C5, voxsynth as VS
        _, _, gain_db = Ph.kit_files(C5.K, 'PhraseVoxRiff')
        lane = np.zeros((C5.L, 2))
        for at in TWICE:
            Ph.place(lane, VS.riff_tail() * D.undb(gain_db), C5.G.at(at))
        return lane, [(s + at, m) for at in TWICE for s, m, _, _ in VS.VOX_RIFF]
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
# and the extended mixes' riff lines, which the loops don't play: the recipe's riff from 16ths 0 and 32
TWICE_LINES = (('FUT02', 'vox_twice'), ('FUT03', 'vox_twice'))


def spec(kit, line):
    return next(s for s in PT.specs(kit) if s.line == line)


def stem_frames(K, s):
    """A stem's frames as the spec gives them (a Loops/ stem is its cycle long; a derived one its own length)."""
    return s.frames if s.derived else PT.cycle_frames(K, s.cycle)


def table_of(K, s):
    """The spec's table, its stems indexed 0 (A) and 1 (B)."""
    index = {p: i for i, p in enumerate(p for p in s.stems if p)}
    return PT.table(s, index, {p: stem_frames(K, s) for p in index})


def voice_passes(K, t, stem, notes, length, n=3):
    """The phrase voice's model over n passes of a loop of `length` frames, from silence: the notes of one pass
    (16ths in the loop) every LOOP_16THS 16ths, the stem at its table gain. Returns (the render, n x length frames;
    the frame of the first pass from which the recipe's loop is what a fresh entry plays: where the first run has come
    in, and past the tail a pass before rings into the loop's start, a stem's frames past its cycle)."""
    if PT.cycle_frames(K, LOOP_16THS) != length:
        raise ValueError(f'a {LOOP_16THS}-16th loop is {PT.cycle_frames(K, LOOP_16THS)} frames, not {length}')
    every = [(p + k * LOOP_16THS, key) for k in range(n) for p, key in notes]
    resolved, unmatched = PT.resolve(K, every, [t])
    if unmatched:
        raise AssertionError(f'notes no slice matches: {unmatched[:4]}')
    pos, _, j = min(resolved)
    overhang = max(0, len(stem) - PT.cycle_frames(K, t['cycle_16ths']))   # FUT03's 6-bar vox stems: 2 bars
    entry = max(0, PT.slice_frame(K, t, pos, j) + PT.run_fade(t, t['slices'][j]), overhang)
    return PT.play(K, t, stem, resolved, n * length), entry


# ------------------------------------------------------------------ the tables
class TestTables(unittest.TestCase):
    def test_each_kit_has_its_plugin_module(self):
        """The kit facts live in tools/sound/recipes/<kit>/plugin.py, read through the kit's RECIPES (shared code
        imports no kit, ADR 0008)."""
        for kit in KITS:
            mod = PT.plugin_module(kit)
            self.assertEqual(f'tools.sound.recipes.{kit.lower()}.plugin', mod.__name__)
        for kit in ('FUT02', 'FUT03'):
            self.assertEqual(TWICE, PT.plugin_module(kit).TWICE)

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
        self.assertEqual(['acid', 'line1', 'line2'], [s.line for s in PT.specs('FUT04') if s.ch == 1])
        for kit in ('FUT02', 'FUT03'):
            self.assertEqual(['vox', 'vox_twice'], [s.line for s in PT.specs(kit) if s.ch == 5])
        for kit in KITS:
            for ch in {s.ch for s in PT.specs(kit)}:
                self.assertEqual(1, len({s.cycle for s in PT.specs(kit) if s.ch == ch}), (kit, ch))

    def test_the_notes_are_the_recipes(self):
        from tools.sound.recipes.fut01 import music as M, voxsynth as V1
        from tools.sound.recipes.fut02 import bass as B2, lead as L2, voxsynth as V2
        from tools.sound.recipes.fut03 import rubber as R3, voxsynth as V3
        from tools.sound.recipes.fut04 import bass as B4, lead as L4
        def line4(name):
            ln = B4.LINE_BY_NAME[name]
            ev = sorted([(s, n) for s, n, *_ in ln.notes] + [(s, n) for s, n, *_ in ln.ticks])
            return ev + [(s + 32, n) for s, n in ev]
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
            ('FUT02', 'vox_twice'): sorted((s + a, n) for a in (0, 32) for s, n, _, _ in V2.RIFF),
            ('FUT03', 'vox_twice'): sorted((s + a, m) for a in (0, 32) for s, m, _, _ in V3.VOX_RIFF),
            ('FUT04', 'acid'): line4(B4.PICK),
            ('FUT04', 'line1'): line4('AcidLine1'),
            ('FUT04', 'line2'): line4('AcidLine2'),
            ('FUT04', 'blip'): [(s, n) for s, n, *_ in L4.PHRASE],
        }
        for (kit, line), notes in want.items():
            self.assertEqual(notes, [(s, k) for s, k, _ in spec(kit, line).notes], (kit, line))
        self.assertEqual([(n,) * (r - 1) for _, n, _, _, r in L4.PHRASE],
                         [h for _, _, h in spec('FUT04', 'blip').notes])
        self.assertEqual([(g[0],) if g else () for *_, g in R3.LINES['Rubber']],
                         [h for _, _, h in spec('FUT03', 'rubber').notes])

    def test_slices_are_well_formed(self):
        """By step, (step, key) unique, all integers, in the contract's key order; start = Grid.at(step) - lead, on
        the stem (0 to its frames - 1); start < end, start <= fade <= end. The slices tile the stem: each ends where
        the next starts, the last where the stem ends. A looped stem is read round (modulo its frames), so there each
        ends where the next starts or a stem's length on (it wraps through the stem's end), the last where the next
        cycle's slice 0 starts, and together they cover the stem once."""
        for kit in KITS:
            K = kits.module(kit)
            for s in PT.specs(kit):
                frames = stem_frames(K, s)
                sl = PT.slices(s, frames)
                with self.subTest(kit=kit, line=s.line):
                    g = Ph.Grid(K, s.cycle, PT.cycle_frames(K, s.cycle))
                    if s.looped:
                        self.assertEqual(PT.cycle_frames(K, s.cycle), frames, 'a looped stem is its cycle long')
                    self.assertEqual(sorted(sl, key=lambda x: x['step']), sl)
                    self.assertEqual(len(sl), len({(x['step'], x['key']) for x in sl}))
                    for i, x in enumerate(sl):
                        self.assertEqual(list(PC.KEYS['slice']), list(x))
                        self.assertTrue(all(type(v) is int for k, v in x.items() if k != 'holds'), x)
                        self.assertTrue(all(type(v) is int for v in x['holds']), x)
                        self.assertTrue(0 <= x['start'] < frames, x)
                        wraps = s.looped and i == 0 and g.at(x['step']) - x['lead'] < 0
                        self.assertEqual(g.at(x['step']) - x['lead'] + (frames if wraps else 0), x['start'])
                        self.assertTrue(x['start'] <= x['fade'] <= x['end'], x)
                        self.assertLess(x['start'], x['end'])
                        self.assertTrue(0 <= x['lead'] <= 48000)
                        self.assertLessEqual(len(x['holds']), 8)
                    if s.looped:
                        for a, b in zip(sl, sl[1:] + sl[:1]):
                            self.assertIn(a['end'] - b['start'], (0, frames), (a, b))
                        self.assertEqual(frames, sum(x['end'] - x['start'] for x in sl))
                    else:
                        for a, b in zip(sl, sl[1:]):
                            self.assertEqual(a['end'], b['start'])
                        self.assertEqual(frames, sl[-1]['end'])

    def test_looped_stems_wrap_round(self):
        """A looped stem holds its last slice's tail at its start: that slice wraps through the stem's end and owns
        the tail, and slice 0 starts at its own pre-roll. FUT01's vox-synth riff: the last chop's release and throw
        run on to slice 0's pre-roll (31951, lead 288), so a fresh entry plays no ghost of the previous cycle's last
        chop, and an exit keeps it. FUT02's riff twice: slice 0's breath starts 2400 frames before its cycle, at the
        stem's end, so slice 0 wraps the other way, the last slice ending where the breath starts. Every other looped
        stem's slice 0 starts on its cycle's start (on its note, or on the kick before a bass note), and its last
        slice ends on the stem's end."""
        riff = PT.slices(spec('FUT01', 'riff'), 171940)        # FUT01's 2-bar cycle
        self.assertEqual((6, 31951, 288), (riff[0]['step'], riff[0]['start'], riff[0]['lead']))
        self.assertEqual((160906, 171940 + 31951, 171940 + 31951 - 240),
                         (riff[-1]['start'], riff[-1]['end'], riff[-1]['fade']))
        twice = PT.slices(spec('FUT02', 'vox_twice'), 354462)  # FUT02's 4-bar cycle
        self.assertEqual((0, 354462 - 2400, 354462 + 14437, 2400),
                         (twice[0]['step'], twice[0]['start'], twice[0]['end'], twice[0]['lead']))
        self.assertEqual(14437, twice[1]['start'])
        self.assertEqual((354462 - 2400, 354462 - 2400 - 240), (twice[-1]['end'], twice[-1]['fade']))
        for kit in KITS:
            K = kits.module(kit)
            for s in PT.specs(kit):
                if s.looped and (kit, s.line) not in (('FUT01', 'riff'), ('FUT02', 'vox_twice')):
                    sl = PT.slices(s, stem_frames(K, s))
                    self.assertEqual((0, stem_frames(K, s)), (sl[0]['start'], sl[-1]['end']), (kit, s.line))

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
        rest's start, and comes back on the downbeat over the recipe's 3 ms (its fade_in_us: a run starting on a
        slice's onset). Every other phrase comes in on an onset over 0.5 ms, keeping the note's transient."""
        from tools.sound.recipes.fut02 import context as C2
        s = spec('FUT02', 'arp')
        self.assertEqual((10000, 3000), (s.fade_us, s.fade_in_us))
        self.assertEqual((int(C2.ARP_REST[1] * 1e6), int(C2.ARP_REST[2] * 1e6)), (s.fade_us, s.fade_in_us))
        for kit in KITS:
            for x in PT.specs(kit):
                self.assertEqual(3000 if (kit, x.line) == ('FUT02', 'arp') else 500, x.fade_in_us, (kit, x.line))

    def test_tables_follow_the_contract(self):
        """Key order (plugin_common's), units, A/B, stems and throws as docs/plugin/manifest.md section 2.6 has them:
        "baked" exactly for the stems with their throw in the audio."""
        self.assertEqual(('lane', 'ch', 'ab', 'mix_channel', 'gain_mdb', 'throw', 'stems', 'cycle_16ths',
                          'tolerance_pm', 'fade_us', 'fade_in_us', 'ride', 'slices'), PC.KEYS['phrase'])
        for kit in KITS:
            K = kits.module(kit)
            for s in PT.specs(kit):
                t = table_of(K, s)
                with self.subTest(kit=kit, line=s.line):
                    self.assertEqual(list(PC.KEYS['phrase']), list(t))
                    self.assertEqual(list(PC.KEYS['stems']), list(t['stems']))
                    self.assertEqual(list(PC.KEYS['slice']), list(t['slices'][0]))
                    self.assertEqual('baked' if (kit, s.line) in BAKED else 'live', t['throw'])
                    self.assertEqual(20, t['tolerance_pm'])
                    self.assertEqual(t['ab'] == '', t['stems']['b'] == -1)
                    self.assertEqual(0, t['stems']['a'])
                    self.assertEqual(s.lane == 'bass', t['ab'] == 'bass')
                    self.assertIn(t['cycle_16ths'], (32, 64))
                    self.assertEqual(list(PC.KEYS['ride']), list(t['ride']))
                    for values in t['ride'].values():
                        self.assertIn(len(values), (0, t['cycle_16ths'] * 4))
                        self.assertTrue(all(type(v) is int and 0 <= v <= 127 for v in values))
                    baked = (kit, t['ch']) == ('FUT04', 1)   # only FUT04's acid stems: every other ride is live
                    self.assertEqual(baked, bool(t['ride']['cc74']) and bool(t['ride']['cc71']))

    def test_every_phrase_says_why(self):
        """Each phrase lane records the recipe function that synthesizes it, and its stem is a Loops/ file or a
        derived stem in Stems/."""
        for kit in KITS:
            for s in PT.specs(kit):
                self.assertRegex(s.why, r'^tools\.sound\.recipes\.fut0\d\.\w+\.\w+', (kit, s.line))
                for p in s.stems:
                    if p:
                        self.assertTrue(p.startswith('Stems/' if s.derived else 'Loops/'), p)


# ------------------------------------------------------------------ the phrase voice's model
def ramp(n):
    """A stem whose frame k holds k (exact in float64), so a render shows which stem frame it read."""
    return np.repeat(np.arange(n, dtype=float)[:, None], 2, axis=1)


class TestVoiceModel(unittest.TestCase):
    def test_a_run_comes_in_over_its_run_fade(self):
        """A run's first slice comes in from its first frame (0.5 - 0.5 cos): over its phrase's fade_in_us where it
        starts on its onset (lead 0: 0.5 ms, 24 frames, for a pluck's or blip's transient; FUT02's arp 3 ms, its
        recipe's), over 3 ms (144 frames) through its pre-roll (a bass line's from the kick, a vox's breath)."""
        self.assertEqual((500, 3000), (PT.ONSET_FADE_US, PT.RUN_FADE_US))
        for kit, line, frames in (('FUT04', 'blip', 24), ('FUT01', 'pluck', 24), ('FUT02', 'arp', 144),
                                  ('FUT02', 'orbit', 144), ('FUT03', 'vox', 144)):
            K = kits.module(kit)
            s = spec(kit, line)
            t = table_of(K, s)
            cf = PT.cycle_frames(K, s.cycle)
            x = t['slices'][1]
            self.assertEqual(frames, PT.run_fade(t, x), (kit, line))
            pos = s.cycle + x['step']                           # in the second cycle
            at = PT.slice_frame(K, t, pos, 1)
            self.assertEqual(cf + Ph.Grid(K, s.cycle, cf).at(x['step']) - x['lead'], at)
            got = PT.play(K, t, np.ones((stem_frames(K, s), 2)), [(pos, 0, 1)], 3 * cf)
            fade_in = 0.5 - 0.5 * np.cos(np.pi * np.arange(frames) / frames)
            np.testing.assert_array_equal(np.stack([fade_in, fade_in], 1), got[at:at + frames], (kit, line))
            self.assertEqual((0.0, 1.0), (got[at - 1, 0], got[at + frames, 0]))

    def test_a_wrapping_slice_reads_round_the_stem(self):
        """FUT01's riff: its last slice alone (an exit) reads the stem on through its end and round from frame 0 to
        slice 0's pre-roll, fading from its fade frame over the phrase's 5 ms: it keeps the last chop's release and
        throw. Followed by the next cycle's slice 0, it reads on round the stem without a seam."""
        K = kits.module('FUT01')
        t = table_of(K, spec('FUT01', 'riff'))
        n = 171940
        last, first = len(t['slices']) - 1, t['slices'][0]
        x = t['slices'][last]
        f = t['fade_us'] * 48 // 1000
        self.assertEqual(x['start'], PT.slice_frame(K, t, x['step'], last))   # cycle 0: on the stem's own frames
        got = PT.play(K, t, ramp(n), [(x['step'], 0, last)], 3 * n)[:, 0]
        k = np.arange(x['start'] + PT.run_fade(t, x), x['fade'])
        np.testing.assert_array_equal(k % n, got[k])                           # frame k, read round the stem
        self.assertTrue(0 < got[x['fade'] + f // 2] < x['fade'] + f // 2 - n)
        np.testing.assert_array_equal(np.zeros(3 * n - x['end']), got[x['end']:])
        got = PT.play(K, t, ramp(n), [(x['step'], 0, last), (32 + first['step'], 0, 0)], 3 * n)[:, 0]
        k = np.arange(x['start'] + PT.run_fade(t, x), n + first['fade'])
        np.testing.assert_array_equal(k % n, got[k])                           # on into slice 0, to its fade

    def test_a_wrapping_slice_0_starts_in_the_cycle_before(self):
        """FUT02's riff twice: slice 0's breath starts 2400 frames before its cycle, read from the stem's end round
        into its start, and on into slice 1. In the song's first cycle the frames before the song's start aren't
        played: it comes in on its onset."""
        K = kits.module('FUT02')
        t = table_of(K, spec('FUT02', 'vox_twice'))
        n = 354462
        x, y = t['slices'][0], t['slices'][1]
        self.assertEqual(n - 2400, PT.slice_frame(K, t, 64, 0))
        got = PT.play(K, t, ramp(n), [(64, 0, 0), (64 + y['step'], 0, 1)], 3 * n)[:, 0]
        k = np.arange(n - 2400 + PT.run_fade(t, x), n + y['fade'])
        np.testing.assert_array_equal(k % n, got[k])
        self.assertEqual(-2400, PT.slice_frame(K, t, 0, 0))
        got = PT.play(K, t, ramp(n), [(0, 0, 0)], n)[:, 0]
        k = np.arange(x['fade'] - x['start'] - 2400)
        np.testing.assert_array_equal(k, got[k])


# ------------------------------------------------------------------ the reference test
def pass_nulls(K, t, stem, notes, lane):
    """The model's nulls against the recipe's lane (the loop, its tails wrapped round), one pass at a time: (the
    first pass from where the voice has come in, the second pass whole)."""
    L = len(lane)
    got, entry = voice_passes(K, t, stem * D.undb(t['gain_mdb'] / 1000.0), notes, L)
    want = D.st(lane)
    return null_db(got[entry:L], want[entry:]), null_db(got[L:2 * L], want)


class TestNulls(unittest.TestCase):
    def test_each_stem_played_by_its_table_is_the_recipes_lane(self):
        """At the Loop file's notes, pass after pass from silence, the phrase voice's model playing the stem by its
        table (contiguous slices read on, round a looped stem too; a slice with no follower fades from its fade
        frame; a run comes in over 0.5 ms on an onset, 3 ms through a pre-roll) nulls against the recipe's lane
        before inserts at -50 dB or lower one pass at a time: the first from where the voice has come in (a fresh
        entry has no tail from a pass before), the second whole. For each variant and bass character."""
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
                        placed = [(PT.tick_16th(K, PT.step_tick(K, p)), k) for p, k in notes]
                        first, second = pass_nulls(K, table_of(K, s), stem, placed, lane)
                        self.assertLessEqual(first, NULL_DB, 'the first pass')
                        self.assertLessEqual(second, NULL_DB, 'the second pass')


class TestTwiceNulls(unittest.TestCase):
    def test_the_riff_twice_stems_played_by_their_tables_are_the_recipes_riffs(self):
        """The extended mixes' riff lines (FUT02's and FUT03's riff from 16ths 0 and 32): the phrase voice's model
        playing the derived stem by its table at those notes, pass after pass, nulls against the recipe's two riffs
        round a 4-bar loop at -50 dB or lower, one pass at a time. FUT02's slice 0 comes in with its breath from the
        pass before."""
        for kit, line in TWICE_LINES:
            K = kits.module(kit)
            s = spec(kit, line)
            lane, notes = reference(kit, line, 'vox', 'clean')
            with self.subTest(kit=kit):
                if sample(kit, s.stems[0]) is None:
                    self.skipTest(f'{kit}/{s.stems[0]} is not here (git lfs pull)')
                first, second = pass_nulls(K, table_of(K, s), stem_audio(kit, s.stems[0]), notes, lane)
                self.assertLessEqual(first, NULL_DB, 'the first pass')
                self.assertLessEqual(second, NULL_DB, 'the second pass')

    def test_the_loops_vox_cycles_pick_the_loops_stem(self):
        """In the Loop files the loop's own vox stem wins its cycle (the riff-twice stem only ties on FUT03)."""
        if loop_file('FUT02', 'vox') is None:
            self.skipTest('waiting on P1: MIDI v2 Loop files')
        for kit in ('FUT02', 'FUT03'):
            K = kits.module(kit)
            tables = [table_of(K, s) for s in PT.specs(kit) if s.ch == 5]
            got, unmatched = PT.resolve(K, PT.file_notes(K, loop_file(kit, 'vox'))[5], tables)
            self.assertEqual(([], {0}), (unmatched, {p for _, p, _ in got}), kit)


class TestDerivedStems(unittest.TestCase):
    def test_derived_stems_render_byte_for_byte(self):
        """FUT02's and FUT03's vox stems (Stems/), rendered again from their recipes, equal the committed WAVs byte
        for byte (where the WAVs are pulled)."""
        for kit in KITS:
            for path in PT.derived(kit):
                have = sample(kit, path)
                if have is None:
                    continue
                with tempfile.TemporaryDirectory() as d, self.subTest(kit=kit, path=path):
                    out = PT.write_derived(kit, Path(d))
                    self.assertEqual(hashlib.sha256(have.read_bytes()).hexdigest(),
                                     hashlib.sha256((Path(d) / kit.lower() / path).read_bytes()).hexdigest())
                    self.assertIn(path, out)

    def test_the_acid_lines_at_rest_are_their_loops_twice(self):
        """FUT04's at-rest acid stems are their 2-bar Loops/ files twice, sample for sample."""
        import soundfile as sf
        for path in PT.derived('FUT04'):
            have = sample('FUT04', path)
            src = sample('FUT04', 'Loops/' + path.split('/')[1].replace(' 4 Bars', ''))
            if have is None or src is None:
                self.skipTest('the samples are not here')
            a, _ = sf.read(have, dtype='int32')
            b, _ = sf.read(src, dtype='int32')
            self.assertTrue(np.array_equal(a, np.concatenate([b, b])), path)

    def test_derived_stems_fit_24_bits(self):
        """A derived stem is the recipe's lane at the stem's gain (a whole dB): its true peak at -1 dBTP or under,
        and the table's gain_mdb gives the lane its level back."""
        for kit, line in (('FUT02', 'vox'), ('FUT03', 'vox'), ('FUT02', 'vox_twice'), ('FUT03', 'vox_twice')):
            s = spec(kit, line)
            self.assertTrue(s.derived)
            self.assertEqual(line == 'vox_twice' and kit == 'FUT02', s.looped)
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


class TestRides(unittest.TestCase):
    def test_the_loop_files_rides_are_the_ride_stems(self):
        """FUT04's Loop files ride CC74 and CC71 on channel 1 exactly as its Ride stem was rendered (the table's
        ride, on the 60-tick grid); the at-rest acid stems' rides are 0 and 0."""
        if loop_file('FUT04', 'vox') is None:
            self.skipTest('waiting on P1: MIDI v2 Loop files')
        K = kits.module('FUT04')
        t = table_of(K, spec('FUT04', 'acid'))
        for variant in ('vox', 'novox'):
            ccs = PT.file_ccs(loop_file('FUT04', variant))[1]
            for cc in (74, 71):
                got = PT.ride_values(ccs[cc], 0, len(t['ride'][f'cc{cc}']))
                self.assertEqual(t['ride'][f'cc{cc}'], got, (variant, cc))
        for line in ('line1', 'line2'):
            r = table_of(K, spec('FUT04', line))['ride']
            self.assertEqual({0}, set(r['cc74']) | set(r['cc71']))

    def test_a_cycle_matches_a_ridden_stem_only_on_its_ride(self):
        """The acid line's notes resolve to the Ride stem under the Ride's controllers, to the at-rest stem at 0/0,
        and to nothing (the multisample) under an arranged ride."""
        K = kits.module('FUT04')
        tables = [table_of(K, spec('FUT04', line)) for line in ('acid', 'line1', 'line2')]
        notes = [(s['step'], s['key']) for s in tables[0]['slices']]
        ride = {cc: [(i * 60, v) for i, v in enumerate(tables[0]['ride'][f'cc{cc}'])] for cc in (74, 71)}
        got, unmatched = PT.resolve(K, notes, tables, ride)
        self.assertEqual(([], {0}), (unmatched, {p for _, p, _ in got}))
        got, unmatched = PT.resolve(K, notes, tables, {74: [(0, 0)], 71: [(0, 0)]})
        self.assertEqual(([], {1}), (unmatched, {p for _, p, _ in got}))
        got, unmatched = PT.resolve(K, notes, tables, {})
        self.assertEqual(([], {1}), (unmatched, {p for _, p, _ in got}))
        arranged = {74: [(i * 60, min(127, i)) for i in range(256)], 71: [(0, 0)]}
        got, unmatched = PT.resolve(K, notes, tables, arranged)
        self.assertEqual(([], len(notes)), (got, len(unmatched)))
        nudged = {cc: [(t_, min(127, v + 1)) for t_, v in vals] for cc, vals in ride.items()}
        self.assertEqual({0}, {p for _, p, _ in PT.resolve(K, notes, tables, nudged)[0]})


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
                ccs = PT.file_ccs(loop_file(kit, variant))
                for ch in sorted({s.ch for s in PT.specs(kit)}):
                    tables = [table_of(K, s) for s in PT.specs(kit) if s.ch == ch]
                    got, unmatched = PT.resolve(K, notes.get(ch, []), tables, ccs.get(ch, {}))
                    if (kit, ch) == ('FUT04', 1):
                        self.assertEqual({0}, {p for _, p, _ in got}, 'the Loop files play the Ride stem')
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
