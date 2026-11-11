"""Phrase tables: the lanes the recipes synthesize at render time, which Wubansia plays from their recorded stems,
sliced per note (ADR 0020, docs/plugin/manifest.md section 2.6). plugin_kit.py writes them into each kit's kit.json.

A phrase lane covers one MIDI channel's notes. Its stem is the lane exactly as the recipe renders it at the kit's tempo
(a Loops/ file, or a derived stem in Stems/ where no Loops/ file is what the loop plays). The table ties each note of
the stem to its slice of the stem by (16th in the cycle, key), so the engine's matcher (engine/sched/phrase_match) can
match the notes the MIDI plays, cycle by cycle, and the phrase voice (engine/sampler/phrase_voice) can read the stem:

- A slice runs from its note's onset (phrase.Grid.at(step) on the stem's own grid) less its lead, to where the next
  slice starts. Slices tile the stem, so contiguous slices read on and the lane at the kit's tempo is the stem.
- A slice's lead is the note's own pre-roll (a vox's breath, a sub's swell); on a bass line that rests on every kick, a
  slice that follows a kick starts on that kick, so a slice played alone starts and ends in the line's silence.
- A looped stem (one the phrase plays cycle after cycle, its tails wrapped round to its start) is read round, modulo
  its frames. Its last slice owns the tail that wraps to the stem's start: it ends past the stem's end, where the next
  cycle's slice 0 starts (FUT01's riff: its last chop's release and throw). Slice 0 starts at its own pre-roll; where
  that is before its cycle's start, at the stem's end, wrapping through it (FUT02's riff twice: its first breath).
- A slice with no follower fades over the phrase's fade_us from its fade frame, by default fade_us before its end (the
  recipe's mute edges: FUT02's glide mute on the kick, its arp's 10 ms rest). A run's first slice (one that doesn't
  continue the slice before) comes in from its first frame: over 3 ms through its pre-roll (RUN_FADE_US), and over the
  phrase's fade_in_us where it starts on its onset (lead 0): 0.5 ms (ONSET_FADE_US), so plucks and blips keep their
  transients, except where the recipe brings the lane in more slowly (FUT02's arp: 3 ms on the downbeat after its
  rest).
- throw: "baked" where the stem has its throw in the audio (FUT01's and FUT03's riffs), so the engine sends its slice
  notes no live throw; "live" for the rest.
- holds: the notes after the slice's own that it plays itself, by key, in order: retriggers of its key (FUT04's
  ratchets, which MIDI v2 retriggers) and the targets its note glides to inside it (FUT03's in-note glides, which MIDI
  v2 writes as legato notes). They make no sound of their own.
- ride: a stem with a filter ride baked in (FUT04's Acid Line 1 Ride) carries the CC74 and CC71 values it was rendered
  with, one per 60 ticks over its cycle, as the MIDI files write them; a stem rendered at rest where the channel's
  controllers would move the filter (FUT04's other acid stems) carries its rest values (0). A cycle matches such a
  phrase only where its own CC values equal these within 1: an arranged ride plays the multisample through the
  channel's live filter instead. Empty: the stem follows no controller (a live filter on its mix channel rides it).

play() and resolve() are the Python model of the engine's phrase voice and matcher, the reference its tests (P6) and
the fidelity checks (P12) use. play() renders the song from its start, from silence, each slice anchored on its note's
onset. At the kit's tempo and 48 kHz the engine plays it exactly over a loop's first pass, then re-syncs at each cycle
wrap (a cycle at the exact tempo isn't a whole number of frames): the reference tests compare one pass at a time.

    python3 tools/sound/phrase_tables.py --kit FUT02 --render-stems content/samples     # the derived stems
"""
import argparse
import bisect
import functools
import hashlib
import importlib
import math
import struct
import sys
from dataclasses import dataclass
from pathlib import Path

if __package__ in (None, ''):  # run as a script: import the tools.sound package (it pins numpy's SIMD) first
    sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
    import tools.sound  # noqa: F401

import numpy as np  # noqa: E402

from tools.sound import dsp48 as D  # noqa: E402
from tools.sound import kits, listen  # noqa: E402
from tools.sound import phrase as Ph  # noqa: E402
from tools.sound import plugin_common as PC  # noqa: E402

SR = 48000
TOLERANCE_PM = 20             # slices play while the host tempo is within 2 % of the kit's (ADR 0020)
FADE_US = 5000                # a slice with no follower fades over this, unless the recipe mutes it otherwise
RUN_FADE_US = 3000            # a run's first slice comes in over this from its first frame, through its pre-roll...
ONSET_FADE_US = 500           # ...and over its phrase's fade_in_us on its onset (lead 0): by default this, keeping the
#                               note's transient
THROWS = ('live', 'baked')    # how a phrase's slice notes reach the throw sends (kit.h's kPhraseThrowIds)
MAX_LEAD = 48000              # docs/plugin/manifest.md: a slice's lead, at most 1 s
PPQ, TICKS_16 = 960, 240      # the MIDI files' grid
RIDE_TICKS = 60               # a ride's grid: midi.CC_STEP, one controller value per 60 ticks while it moves
RIDE_SLACK = 1                # a cycle's controller values match a stem's ride within this


iround = PC.iround            # a float in a manifest unit: round half up, floor(x x scale + 0.5)


def words(name):
    """An Item's CamelCase name in plain words, as its WAV is named: 'AcidLine1' -> 'Acid Line 1'."""
    return ' '.join(listen.note(w) for w in listen.WORD.findall(name))


def plugin_module(kit):
    """The kit's plugin module, tools/sound/recipes/<kit>/plugin.py, read through the kit's RECIPES (shared code never
    imports a kit, ADR 0008), or None for a kit without one."""
    try:
        return importlib.import_module(f'{kits.module(kit).RECIPES}.plugin')
    except ModuleNotFoundError:
        return None


def cycle_frames(K, cycle):
    """A cycle of `cycle` 16ths in frames, as the context loops count it: whole 2-bar phrases (context4.L, FUT02-04's
    L = 2 x loop_len(K, 32)), so a 2-bar stem tiles a 4-bar one exactly."""
    if cycle % 32:
        raise ValueError(f'a phrase cycle is whole 2-bar phrases, not {cycle} 16ths')
    return cycle // 32 * Ph.loop_len(K, 32)


def loops_path(name):
    """The WAV of a Loops/ Item, from the kit's folder: its CamelCase name in words (listen.plain)."""
    return f"Loops/{words(name)}.wav"


@dataclass(frozen=True)
class Spec:
    """One phrase lane's table, before its stems' indices are known."""
    kit: str
    line: str                 # the table's own name: 'pluck', 'orbit'...
    lane: str                 # the plugin lane (channels.LANES)
    ch: int                   # the MIDI channel whose notes it covers
    stems: tuple              # (A, B or None): paths from the kit's folder
    ab: str                   # the switch choosing the stem: 'bass' or ''
    cycle: int                # 16ths
    notes: tuple              # ((step, key, holds), ...) in the stem, by step; holds: the keys of the notes it holds
    pre: object               # frames of each note's pre-roll: an int, or {step: frames}
    mix_channel: str
    why: str                  # the recipe function that synthesizes the lane
    kicks: tuple = ()         # the cycle's kick 16ths, where a bass line rests
    fade_us: int = FADE_US
    fade_in_us: int = ONSET_FADE_US   # how a run that starts on its slice's onset (lead 0) comes in
    gain_mdb: int = 0         # the stem's own gain: a derived stem stored under the lane's level gets it back
    derived: bool = False     # a stem rendered for the plugin (Stems/), not a Loops/ file
    frames: int = 0           # a derived stem's length (a Loops/ stem is its cycle long)
    looped: bool = True       # the stem is a loop of its cycle (its tail wraps round to its start)
    ride: tuple = None        # ((CC74 values), (CC71 values)) the stem was rendered with, one per 60 ticks; None: none
    throw: str = 'live'       # 'baked': the stem has its throw in the audio (the engine sends its notes no live throw)

    def pre_of(self, step):
        return self.pre.get(step, 0) if isinstance(self.pre, dict) else self.pre


@functools.lru_cache(maxsize=None)
def specs(kit):
    """The kit's phrase lanes (its plugin module's phrases()), channel by channel; on a channel, the loop's own line
    first."""
    mod = plugin_module(kit)
    return tuple(mod.phrases()) if mod else ()


def derived(kit):
    """The kit's derived stems, paths from its folder (its plugin module's DERIVED)."""
    mod = plugin_module(kit)
    return tuple(mod.DERIVED) if mod else ()


# ------------------------------------------------------------------ the tables
def slices(spec, frames):
    """The spec's slices on a stem of `frames` frames: [{step, key, start, end, fade, lead, holds}], by step. A looped
    stem's are read round it: its last slice ends where the next cycle's slice 0 starts, past the stem's end when slice
    0 starts after frame 0; a slice 0 whose pre-roll starts before its cycle starts at the stem's end and wraps."""
    K = kits.module(spec.kit)
    cf = cycle_frames(K, spec.cycle)
    if spec.looped and frames != cf:
        raise ValueError(f'{spec.kit} {spec.line}: a looped stem is its cycle long ({cf} frames), not {frames}')
    g = Ph.Grid(K, spec.cycle, cf)
    on = [g.at(s) for s, _, _ in spec.notes]
    kick = sorted(g.at(k) for k in spec.kicks)
    if spec.looped:
        kick = [k - frames for k in kick] + kick        # slice 0 may follow the cycle before's last kick
    starts = []                                         # on the cycle's frames: slice 0's may be before its start
    for i, (s, _, _) in enumerate(spec.notes):
        b = on[i] - spec.pre_of(s)
        before = on[i - 1] if i else (on[-1] - frames if spec.looped else -1)
        ks = [k for k in kick if before < k <= b]
        starts.append(ks[-1] if ks else b)
    ends = starts[1:] + [starts[0] + frames if spec.looped else frames]
    fade = spec.fade_us * SR // 1_000_000
    out = []
    for i, (s, key, holds) in enumerate(spec.notes):
        a, e = starts[i], ends[i]
        wrap = frames if a < 0 and spec.looped and i == 0 else 0    # read from the stem's end
        x = PC.ordered('slice', step=s, key=key, start=a + wrap, end=e + wrap, fade=max(a, e - fade) + wrap,
                       lead=on[i] - a, holds=list(holds))
        if not (0 <= x['start'] < frames and a < e and 0 <= x['lead'] <= MAX_LEAD and len(holds) <= 8):
            raise ValueError(f'{spec.kit} {spec.line}: slice {x} is out of range')
        out.append(x)
    return out


def table(spec, index, frames):
    """The phrase as kit.json holds it, in the contract's key order. index: {stem path: file index}; frames: {stem
    path: frames}."""
    stems = [p for p in spec.stems if p]
    cc74, cc71 = spec.ride or ((), ())
    if spec.ride and not len(cc74) == len(cc71) == spec.cycle * TICKS_16 // RIDE_TICKS:
        raise ValueError(f'{spec.kit} {spec.line}: a ride is one value per {RIDE_TICKS} ticks over the cycle')
    if spec.throw not in THROWS:
        raise ValueError(f'{spec.kit} {spec.line}: throw {spec.throw!r} is not one of {THROWS}')
    return PC.ordered(
        'phrase', lane=spec.lane, ch=spec.ch, ab=spec.ab, mix_channel=spec.mix_channel, gain_mdb=spec.gain_mdb,
        throw=spec.throw,
        stems=PC.ordered('stems', a=index[spec.stems[0]], b=index[spec.stems[1]] if spec.stems[1] else -1),
        cycle_16ths=spec.cycle, tolerance_pm=TOLERANCE_PM, fade_us=spec.fade_us, fade_in_us=spec.fade_in_us,
        ride=PC.ordered('ride', cc74=list(cc74), cc71=list(cc71)),
        slices=slices(spec, min(frames[p] for p in stems)))


# ------------------------------------------------------------------ the matcher and the voice, as models
def ride_values(ccs, start, n):
    """A controller's values on the ride grid: n values from tick `start`, every RIDE_TICKS, each the last value at or
    before its tick (0 before the first). ccs: [(tick, value)], by tick."""
    ticks = [t for t, _ in ccs]
    out = []
    for i in range(n):
        j = bisect.bisect_right(ticks, start + i * RIDE_TICKS) - 1
        out.append(ccs[j][1] if j >= 0 else 0)
    return out


def rides_match(t, ccs, c):
    """Whether cycle c's controllers (ccs: {74: [(tick, value)], 71: [...]}, or None to ignore them) fit table t's
    ride: an empty ride fits any; otherwise each value within RIDE_SLACK."""
    if ccs is None:
        return True
    cyc = t['cycle_16ths']
    for cc in (74, 71):
        want = t['ride'][f'cc{cc}']
        if want:
            got = ride_values(ccs.get(cc, []), c * cyc * TICKS_16, len(want))
            if any(abs(a - b) > RIDE_SLACK for a, b in zip(got, want)):
                return False
    return True


def resolve(K, notes, tables, ccs=None):
    """The matcher (engine/sched/phrase_match), as a model. notes: [(16th, key)] on one channel, the 16th a number
    whose fraction places a note inside its 16th (tick_16th); ccs: the channel's controllers, {74: [(tick, value)],
    71: [...]} (None: ignore them). Cycle by cycle, from 16th 0, among the tables whose ride fits the cycle's
    controllers (rides_match), the one with the most (16th in the cycle, key) matches wins. Its matched notes get their
    slice; after one, the notes its slice holds (by key, in order, each on a cell that is the slice's own or no
    slice's) are the slice's; every other note stays on the multisample. The cycle plays the table only when its
    matched and held notes are more than half its notes; otherwise every note of the cycle stays on the multisample, so
    a line that lands on a few of a stem's cells isn't a patchwork of the two (FUT03's switch-up riff on the Deep
    line's cells). Returns ([(16th, table, slice)], [unmatched (16th, key)])."""
    cycles = {t['cycle_16ths'] for t in tables}
    if len(cycles) > 1:
        raise ValueError(f'one channel, one cycle length: {sorted(cycles)}')
    got, unmatched = [], []
    if not tables:
        return got, list(notes)
    cyc = cycles.pop()
    pattern = [{(s['step'], s['key']): j for j, s in enumerate(t['slices'])} for t in tables]
    by_cycle = {}
    for pos, key in sorted(notes):
        by_cycle.setdefault(math.floor(pos) // cyc, []).append((pos, key))
    for c, ns in sorted(by_cycle.items()):
        cells = [(math.floor(p) - c * cyc, k) for p, k in ns]
        score = [len(set(cells) & set(pat)) if rides_match(t, ccs, c) else -1
                 for t, pat in zip(tables, pattern)]
        best = max(range(len(tables)), key=lambda i: (score[i], -i))
        pat, sl = pattern[best], tables[best]['slices']
        used, pending, own = set(), [], None
        cycle_got, cycle_unmatched = [], []
        for (pos, key), cell in zip(ns, cells):
            if score[best] > 0 and pending and key == pending[0] and (cell == own or cell not in pat):
                pending.pop(0)
                continue
            j = pat.get(cell) if score[best] > 0 else None
            if j is None or cell in used:
                cycle_unmatched.append((pos, key))
                continue
            used.add(cell)
            cycle_got.append((pos, best, j))
            pending, own = list(sl[j]['holds']), cell
        if 2 * (len(ns) - len(cycle_unmatched)) > len(ns):    # matched and held: more than half the cycle's notes
            got += cycle_got
            unmatched += cycle_unmatched
        else:
            unmatched += ns
    return got, unmatched


def run_fade(t, s):
    """The frames a run whose first slice is table t's slice s comes in over, from the slice's first frame: the
    phrase's fade_in_us where it starts on its onset (lead 0), RUN_FADE_US through its pre-roll."""
    return (t['fade_in_us'] if s['lead'] == 0 else RUN_FADE_US) * SR // 1_000_000


def slice_frame(K, t, pos, j):
    """Where the note at 16th `pos` starts slice j of table t, in frames from song 16th 0 at the kit's tempo and
    48 kHz: its onset (its cycle's start plus phrase.Grid.at(step) on the stem's grid) less its lead. A wrapping slice
    0 starts in the cycle before (FUT02's riff twice: its breath)."""
    cyc = t['cycle_16ths']
    cf = cycle_frames(K, cyc)
    s = t['slices'][j]
    return math.floor(pos) // cyc * cf + Ph.Grid(K, cyc, cf).at(s['step']) - s['lead']


def play(K, t, audio, resolved, length, table_index=0):
    """The phrase voice, as a model: the notes resolved to table t's slices (resolve(); those of table `table_index`)
    read from `audio`, the stem ((n, 2), at its table gain), into `length` frames from song 16th 0, from silence, at
    the kit's tempo and 48 kHz. Each slice is anchored on its note's onset: it plays from its start at slice_frame()
    (frames before the song's start aren't played). A slice that continues the one before (the next slice of its
    cycle, or slice 0 after the last in the next cycle, starting where that one ends, modulo the stem's length) reads
    on, so a run reads the stem continuously, round a looped stem. A run's first slice comes in from its first frame
    over run_fade() (0.5 - 0.5 cos); a slice that no slice continues fades from its fade frame over the phrase's
    fade_us (0.5 + 0.5 cos) and stops at its end, while the next run may already play."""
    sl, cyc = t['slices'], t['cycle_16ths']
    notes = sorted((pos, j) for pos, ti, j in resolved if ti == table_index)
    out = np.zeros((length, 2))
    n = len(audio)
    stem = D.st(audio)
    if any(s['end'] > n for s in sl):
        stem = np.concatenate([stem, stem])            # a wrapping slice reads on round the stem
    fade = t['fade_us'] * SR // 1_000_000
    cycle_of = lambda pos: math.floor(pos) // cyc  # noqa: E731

    def continues(a, b):
        """Whether note b's slice continues note a's: the next slice of its cycle, or the next cycle's first after the
        last, starting where a's ends on the stem (modulo its length: across a looped stem's end)."""
        (pa, ja), (pb, jb) = a, b
        nxt = (jb == ja + 1 and cycle_of(pb) == cycle_of(pa)) or \
            (ja == len(sl) - 1 and jb == 0 and cycle_of(pb) == cycle_of(pa) + 1)
        return nxt and (sl[ja]['end'] - sl[jb]['start']) % n == 0

    for i, (pos, j) in enumerate(notes):
        s = sl[j]
        a, e = s['start'], s['end']
        g = np.ones(e - a)
        if i + 1 == len(notes) or not continues(notes[i], notes[i + 1]):
            f = s['fade'] - a
            m = min(fade, e - a - f)
            g[f:f + m] = 0.5 + 0.5 * np.cos(np.pi * np.arange(m) / fade)
            g[f + m:] = 0.0
        if i == 0 or not continues(notes[i - 1], notes[i]):
            r = run_fade(t, s)
            m = min(r, e - a)
            g[:m] *= 0.5 - 0.5 * np.cos(np.pi * np.arange(m) / r)
        at = slice_frame(K, t, pos, j)
        lo, hi = max(0, at), min(length, at + e - a)
        if lo < hi:
            out[lo:hi] += stem[a + lo - at:a + hi - at] * g[lo - at:hi - at, None]
    return out


# ------------------------------------------------------------------ the MIDI files' notes
def _vlq(d, j):
    v = 0
    while True:
        c = d[j]
        j += 1
        v = (v << 7) | (c & 0x7F)
        if not c & 0x80:
            return v, j


def smf_notes(data):
    """A Standard MIDI File's note-ons: [(tick, channel 1-16, key, velocity)], in tick order."""
    return [e[1:] for e in smf_events(data) if e[0] == 'note']


def smf_ccs(data):
    """A Standard MIDI File's controllers: {channel: {controller: [(tick, value)]}}, by tick."""
    out = {}
    for _, t, ch, cc, v in (e for e in smf_events(data) if e[0] == 'cc'):
        out.setdefault(ch, {}).setdefault(cc, []).append((t, v))
    return out


def smf_events(data):
    """A Standard MIDI File's note-ons and controllers: [('note' or 'cc', tick, channel 1-16, key or controller,
    velocity or value)], in tick order (track by track, stably)."""
    if data[:4] != b'MThd':
        raise ValueError('not a Standard MIDI File')
    _, _, ntrk, ppq = struct.unpack('>IHHH', data[4:14])
    if ppq != PPQ:
        raise ValueError(f'{ppq} ticks a quarter, not {PPQ}')
    i, out = 14, []
    for _ in range(ntrk):
        n = struct.unpack('>I', data[i + 4:i + 8])[0]
        d, i = data[i + 8:i + 8 + n], i + 8 + n
        j = t = 0
        status = 0
        while j < len(d):
            v, j = _vlq(d, j)
            t += v
            if d[j] & 0x80:
                status = d[j]
                j += 1
            if status == 0xFF:
                ln, j = _vlq(d, j + 1)
                j += ln
                continue
            if status in (0xF0, 0xF7):
                ln, j = _vlq(d, j)
                j += ln
                continue
            kind, ch = status & 0xF0, (status & 0x0F) + 1
            size = 1 if kind in (0xC0, 0xD0) else 2
            if kind == 0x90 and d[j + 1] > 0:
                out.append(('note', t, ch, d[j], d[j + 1]))
            elif kind == 0xB0:
                out.append(('cc', t, ch, d[j], d[j + 1]))
            j += size
    return sorted(out, key=lambda e: e[1])


def step_tick(K, step):
    """A 16th position as midi.py writes it (Kit.tick): its 16th on the swing, a fraction straight from there."""
    base = math.floor(step)
    late = round((K.SWING - 0.5) * 2 * TICKS_16)
    return base * TICKS_16 + (late if base % 2 else 0) + round((step - base) * TICKS_16)


def tick_16th(K, tick):
    """A tick as a 16th position: the 16th it sounds in (its odd 16ths start round((swing - 0.5) x 480) ticks late, as
    midi.py writes them), plus how far into it, as a fraction."""
    late = round((K.SWING - 0.5) * 2 * TICKS_16)
    start = lambda s: s * TICKS_16 + (late if s % 2 else 0)  # noqa: E731
    s = tick // TICKS_16
    if start(s) > tick:
        s -= 1
    return s + (tick - start(s)) / (start(s + 1) - start(s))


def file_ccs(path):
    """{channel: {controller: [(tick, value)]}}: a MIDI file's controllers."""
    return smf_ccs(Path(path).read_bytes())


def file_notes(K, path):
    """{channel: [(16th, key)]}: a MIDI file's notes as 16th positions (tick_16th)."""
    out = {}
    for tick, ch, key, _ in smf_notes(Path(path).read_bytes()):
        out.setdefault(ch, []).append((tick_16th(K, tick), key))
    return {ch: sorted(v) for ch, v in out.items()}


# ------------------------------------------------------------------ the derived stems
def render_derived(kit):
    """The kit's derived stems, rendered from its recipes by its plugin module: {path: (n, 2) float audio at the stem's
    level}, each cycle-aligned (frame 0 is its cycle's 16th 0)."""
    mod = plugin_module(kit)
    return mod.render_derived() if mod else {}


def write_derived(kit, root):
    """Write the kit's derived stems under root/<kit>/ as 48 kHz / 24-bit WAVs (as render.py encodes its masters):
    {path: SHA-256}."""
    import soundfile as sf
    out = {}
    for path, audio in render_derived(kit).items():
        dest = Path(root) / kit.lower() / path
        dest.parent.mkdir(parents=True, exist_ok=True)
        if np.max(np.abs(audio)) >= 1.0:
            raise ValueError(f'{kit} {path} clips at 24 bits: store it lower (its plugin module)')
        audio = np.asarray(audio, dtype=float)
        if audio.ndim == 2 and np.array_equal(audio[:, 0], audio[:, 1]):
            audio = audio[:, 0]                             # mono where L = R, as render.py stores its masters
        sf.write(dest, audio, SR, subtype='PCM_24', format='WAV')
        out[path] = hashlib.sha256(dest.read_bytes()).hexdigest()
    return out


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    ap.add_argument('--kit', required=True, choices=[k for k in kits.names() if plugin_module(k)])
    ap.add_argument('--render-stems', metavar='ROOT', help='write the derived stems under ROOT/<kit>/Stems/')
    a = ap.parse_args(argv)
    if a.render_stems:
        for path, sha in write_derived(a.kit, a.render_stems).items():
            print(f'{a.kit.lower()}/{path}  {sha}')
    for s in specs(a.kit):
        print(f'{s.kit} ch {s.ch} {s.lane:6} {s.line:7} {s.cycle} 16ths, {len(s.notes)} slices, stems {s.stems}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
