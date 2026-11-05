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
- A looped stem's first slice starts on frame 0: its lead holds the tail the stem wraps round from its end.
- A slice with no follower fades over the phrase's fade_us from its fade frame, by default fade_us before its end (the
  recipe's mute edges: FUT02's glide mute on the kick, its arp's 10 ms rest). A slice that doesn't continue the one
  before comes in over the voice's 3 ms crossfade (XFADE_US).
- holds: the notes after the slice's own that it plays itself, by key, in order: retriggers of its key (FUT04's
  ratchets, which MIDI v2 retriggers) and the targets its note glides to inside it (FUT03's in-note glides, which MIDI
  v2 writes as legato notes). They make no sound of their own.

play() and resolve() are the Python model of the engine's phrase voice and matcher, the reference its tests (P6) and
the fidelity checks (P12) use.

    python3 tools/sound/phrase_tables.py --kit FUT02 --render-stems content/samples     # the derived stems
"""
import argparse
import functools
import hashlib
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

SR = 48000
TOLERANCE_PM = 20             # slices play while the host tempo is within 2 % of the kit's (ADR 0020)
FADE_US = 5000                # a slice with no follower fades over this, unless the recipe mutes it otherwise
XFADE_US = 3000               # the phrase voice's crossfade into a slice that doesn't continue the one before
MAX_LEAD = 48000              # docs/plugin/manifest.md: a slice's lead, at most 1 s
PPQ, TICKS_16 = 960, 240      # the MIDI files' grid


def cycle_frames(K, cycle):
    """A cycle of `cycle` 16ths in frames, as the context loops count it: whole 2-bar phrases (context4.L, FUT02-04's
    L = 2 x loop_len(K, 32)), so a 2-bar stem tiles a 4-bar one exactly."""
    if cycle % 32:
        raise ValueError(f'a phrase cycle is whole 2-bar phrases, not {cycle} 16ths')
    return cycle // 32 * Ph.loop_len(K, 32)


def loops_path(name):
    """The WAV of a Loops/ Item, from the kit's folder: its CamelCase name in words (listen.plain)."""
    return f"Loops/{' '.join(listen.note(w) for w in listen.WORD.findall(name))}.wav"


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
    gain_mdb: int = 0         # the stem's own gain: a derived stem stored under the lane's level gets it back
    derived: bool = False     # a stem rendered for the plugin (Stems/), not a Loops/ file
    frames: int = 0           # a derived stem's length (a Loops/ stem is its cycle long)

    def pre_of(self, step):
        return self.pre.get(step, 0) if isinstance(self.pre, dict) else self.pre


# ------------------------------------------------------------------ the lanes, kit by kit
def _fut01():
    from tools.sound.recipes.fut01 import music as M, voxsynth as V
    riff_pre = {s: int(np.floor(V.ONSET_LEAD[o] * SR + 0.5)) for s, _, _, _, _, o in V.RIFF}
    return (
        Spec('FUT01', 'pluck', 'lead', 2, (loops_path('ChromePluckPhrase'), None), '', M.PHRASE_STEPS,
             tuple((s, n, ()) for s, n, _, _ in M.PHRASE), 0, 'pluck',
             'tools.sound.recipes.fut01.music.pluck_phrase'),
        Spec('FUT01', 'riff', 'vox', 5, (loops_path('VoxSynthRiff'), None), '', V.RIFF_STEPS,
             tuple((s, n, ()) for s, n, *_ in V.RIFF), riff_pre, 'vsynth', 'tools.sound.recipes.fut01.voxsynth.riff'),
    )


def _fut02():
    from tools.sound.recipes.fut02 import bass as B, context as C, context5 as C5, lead as Ld, voxsynth as Vs
    kicks, sub = tuple(B.KICK_STEPS), int(B.SUB_LEAD * SR)
    lines = tuple(
        Spec('FUT02', name.lower(), 'bass', 1, (loops_path(f'{name}LineClean'), loops_path(f'{name}LineWarm')),
             'bass', B.LINE_STEPS, tuple((s, n, ()) for s, n, *_ in B.LINES[name]), sub, 'bass',
             'tools.sound.recipes.fut02.bass.line_lane', kicks=kicks, fade_us=int(round(C.MUTE_EDGE * 1e6)))
        for name in (B.PICK,) + tuple(n for n in B.LINES if n != B.PICK))
    return lines + (
        Spec('FUT02', 'arp', 'lead', 2, (loops_path('ChromeArpPhrase'), None), '', Ld.PHRASE_STEPS,
             tuple((s, n, ()) for s, n, _, _ in Ld.phrase_notes()), 0, 'arp',
             'tools.sound.recipes.fut02.lead.arp_phrase', fade_us=int(round(C.ARP_REST[1] * 1e6))),
        Spec('FUT02', 'vox', 'vox', 5, ('Stems/Context Vox Riff.wav', None), '', 64,
             tuple((s + C5.RIFF_AT, n, ()) for s, n, _, _ in Vs.RIFF), int(0.05 * SR), 'vox',
             'tools.sound.recipes.fut02.voxsynth.sung_parts', gain_mdb=-1000 * STEM_GAIN_DB['FUT02'], derived=True,
             frames=C5.L),
    )


def _fut03():
    from tools.sound.recipes.fut03 import context5 as C5, rubber as R, voxsynth as VS
    kicks, sub = tuple(R.KICKS), int(R.SUB_LEAD * SR)
    lines = tuple(
        Spec('FUT03', name.lower(), 'bass', 1, (loops_path(f'Line{name}Clean'), loops_path(f'Line{name}Warm')), 'bass',
             R.LOOP_STEPS, tuple((s, n, (g[0],) if g else ()) for s, _, n, _, g in R.LINES[name]), sub, 'bass',
             'tools.sound.recipes.fut03.rubber.line_lane', kicks=kicks)
        for name in (R.PICK,) + tuple(n for n in R.LINES if n != R.PICK))
    return lines + (
        Spec('FUT03', 'vox', 'vox', 5, ('Stems/Context Vox Riff.wav', None), '', 64,
             tuple((s + C5.VOX_AT, m, ()) for s, m, _, _ in VS.VOX_RIFF), int(np.floor(VS.LEAD * SR + 0.5)), 'vox',
             'tools.sound.recipes.fut03.voxsynth.riff_tail', gain_mdb=-1000 * STEM_GAIN_DB['FUT03'], derived=True,
             frames=C5.G.at(C5.VOX_AT) + _riff_tail_frames()),
    )


def _riff_tail_frames():
    """FUT03's riff before it wraps: its 2 bars and TAIL_STEPS of echoes (voxsynth.riff_tail's length)."""
    from tools.sound.kits import fut03 as K
    from tools.sound.recipes.fut03 import voxsynth as VS
    g = Ph.Grid(K, VS.RIFF_STEPS)
    return g.length + int(VS.TAIL_STEPS * g.s16)


def _fut04():
    from tools.sound.recipes.fut04 import bass as B, lead as Ld
    line = B.LINE_BY_NAME[B.PICK]
    ev = sorted([(s, n) for s, n, *_ in line.notes] + [(s, n) for s, n, *_ in line.ticks])
    ride = B.RIDE_PASSES * B.LOOP_STEPS
    return (
        Spec('FUT04', 'acid', 'bass', 1, (loops_path(f'{B.PICK}RideClean'), loops_path(f'{B.PICK}RideWarm')), 'bass',
             ride, tuple((s + B.LOOP_STEPS * p, n, ()) for p in range(B.RIDE_PASSES) for s, n in ev), 0, 'bass_stem',
             'tools.sound.recipes.fut04.bass.play_line', kicks=tuple(range(0, ride, 4))),
        Spec('FUT04', 'blip', 'lead', 2, (loops_path('ModBlipPhrase'), None), '', Ld.PHRASE_STEPS,
             tuple((s, n, (n,) * (r - 1)) for s, n, _, _, r in Ld.PHRASE), 0, 'lead',
             'tools.sound.recipes.fut04.lead.blip_phrase'),
    )


STEM_GAIN_DB = {'FUT02': -3, 'FUT03': 0}   # each derived stem's level under its lane, whole dB: true peak <= -1 dBTP
DERIVED = {'FUT02': ('Stems/Context Vox Riff.wav',), 'FUT03': ('Stems/Context Vox Riff.wav',)}
_SPECS = {'FUT01': _fut01, 'FUT02': _fut02, 'FUT03': _fut03, 'FUT04': _fut04}


@functools.lru_cache(maxsize=None)
def specs(kit):
    """The kit's phrase lanes, channel by channel; on a channel, the loop's own line first."""
    return _SPECS[kit]() if kit in _SPECS else ()


# ------------------------------------------------------------------ the tables
def slices(spec, frames):
    """The spec's slices on a stem of `frames` frames: [{step, key, start, end, fade, lead, holds}], by step."""
    K = kits.module(spec.kit)
    g = Ph.Grid(K, spec.cycle, cycle_frames(K, spec.cycle))
    on = [g.at(s) for s, _, _ in spec.notes]
    kick = sorted(g.at(k) for k in spec.kicks)
    starts = []
    for i, (s, _, _) in enumerate(spec.notes):
        b = on[i] - spec.pre_of(s)
        ks = [k for k in kick if (on[i - 1] if i else -1) < k <= b]
        starts.append(ks[-1] if ks else b)
    if not spec.derived:
        starts[0] = 0         # a looped stem: the first slice holds the tail wrapped round from the stem's end
    ends = starts[1:] + [frames]
    fade = spec.fade_us * SR // 1_000_000
    out = []
    for i, (s, key, holds) in enumerate(spec.notes):
        a, e = starts[i], ends[i]
        x = dict(step=s, key=key, start=a, end=e, fade=max(a, e - fade), lead=on[i] - a, holds=list(holds))
        if not (0 <= a < e and 0 <= x['lead'] <= MAX_LEAD and len(holds) <= 8):
            raise ValueError(f'{spec.kit} {spec.line}: slice {x} is out of range')
        out.append(x)
    return out


def table(spec, index, frames):
    """The phrase as kit.json holds it. index: {stem path: file index}; frames: {stem path: frames}."""
    stems = [p for p in spec.stems if p]
    return {'lane': spec.lane, 'ch': spec.ch, 'ab': spec.ab, 'mix_channel': spec.mix_channel,
            'gain_mdb': spec.gain_mdb,
            'stems': {'a': index[spec.stems[0]], 'b': index[spec.stems[1]] if spec.stems[1] else -1},
            'cycle_16ths': spec.cycle, 'tolerance_pm': TOLERANCE_PM, 'fade_us': spec.fade_us,
            'slices': slices(spec, min(frames[p] for p in stems))}


# ------------------------------------------------------------------ the matcher and the voice, as models
def resolve(K, notes, tables):
    """The matcher (engine/sched/phrase_match), as a model. notes: [(16th, key)] on one channel, the 16th a number
    whose fraction places a note inside its 16th (tick_16th). Cycle by cycle, from 16th 0, the table with the most
    (16th in the cycle, key) matches wins. Its matched notes get their slice; after one, the notes its slice holds
    (by key, in order, each on a cell that is the slice's own or no slice's) are the slice's; every other note stays
    on the multisample. Returns ([(16th, table, slice)], [unmatched (16th, key)])."""
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
        score = [len(set(cells) & set(pat)) for pat in pattern]
        best = max(range(len(tables)), key=lambda i: (score[i], -i))
        pat, sl = pattern[best], tables[best]['slices']
        used, pending, own = set(), [], None
        for (pos, key), cell in zip(ns, cells):
            if score[best] and pending and key == pending[0] and (cell == own or cell not in pat):
                pending.pop(0)
                continue
            j = pat.get(cell) if score[best] else None
            if j is None or cell in used:
                unmatched.append((pos, key))
                continue
            used.add(cell)
            got.append((pos, best, j))
            pending, own = list(sl[j]['holds']), cell
    return got, unmatched


def play(K, t, audio, resolved, length, table_index=0):
    """The phrase voice, as a model: the notes resolved to table t's slices (resolve(); those of table `table_index`)
    read from `audio`, the stem ((n, 2), at its table gain), round a loop of `length` frames at the kit's tempo and
    48 kHz. Each slice is read from its start at its note's onset on the loop's grid, less its lead; contiguous slices
    read on (across the cycle's wrap too); a slice that doesn't continue the one before comes in over XFADE_US; a slice
    with no follower fades over the phrase's fade_us from its fade frame. Tails wrap round the loop."""
    sl, cyc = t['slices'], t['cycle_16ths']
    notes = sorted((pos, j) for pos, ti, j in resolved if ti == table_index)
    out = np.zeros((length, 2))
    if not notes:
        return out
    if length % cycle_frames(K, cyc):
        raise ValueError(f'a loop of {length} frames is not whole cycles of {cycle_frames(K, cyc)}')
    ring = length // cycle_frames(K, cyc)
    grid = Ph.Grid(K, ring * cyc, length)
    fade, xfade = t['fade_us'] * SR // 1_000_000, XFADE_US * SR // 1_000_000
    cycle_of = lambda pos: math.floor(pos) // cyc  # noqa: E731

    def continues(a, b):
        """Whether note b's slice continues note a's: the next slice of the same cycle, or the stem's wrap into the
        next cycle's first slice."""
        (pa, ja), (pb, jb) = a, b
        if jb == ja + 1 and cycle_of(pa) == cycle_of(pb):
            return sl[ja]['end'] == sl[jb]['start']
        if ja == len(sl) - 1 and jb == 0 and (cycle_of(pb) - cycle_of(pa)) % ring == 1 % ring:
            return sl[ja]['end'] == len(audio) and sl[jb]['start'] == 0
        return False

    for i, (pos, j) in enumerate(notes):
        s = sl[j]
        a, e = s['start'], s['end']
        g = np.ones(e - a)
        if len(notes) == 1 or not continues((pos, j), notes[(i + 1) % len(notes)]):
            f = s['fade'] - a
            m = min(fade, e - a - f)
            g[f:f + m] = 0.5 + 0.5 * np.cos(np.pi * np.arange(m) / fade)
            g[f + m:] = 0.0
        if len(notes) == 1 or not continues(notes[i - 1], (pos, j)):
            m = min(xfade, e - a)
            g[:m] *= 0.5 - 0.5 * np.cos(np.pi * np.arange(m) / xfade)
        Ph.place(out, D.st(audio[a:e]) * g[:, None], grid.at(cycle_of(pos) * cyc + s['step']) - s['lead'])
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
    """A Standard MIDI File's note-ons: [(tick, channel 1-16, key, velocity)], in tick order (track by track)."""
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
                out.append((t, ch, d[j], d[j + 1]))
            j += size
    return sorted(out, key=lambda e: e[0])


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


def file_notes(K, path):
    """{channel: [(16th, key)]}: a MIDI file's notes as 16th positions (tick_16th)."""
    out = {}
    for tick, ch, key, _ in smf_notes(Path(path).read_bytes()):
        out.setdefault(ch, []).append((tick_16th(K, tick), key))
    return {ch: sorted(v) for ch, v in out.items()}


# ------------------------------------------------------------------ the derived stems
def render_derived(kit):
    """The kit's derived stems, rendered from their recipes: {path: (n, 2) float audio at the stem's level}. Each is
    the loop's lane before its inserts, cycle-aligned (frame 0 is the cycle's 16th 0), at STEM_GAIN_DB."""
    out = {}
    if kit == 'FUT02':
        from tools.sound.kits import fut02 as K
        from tools.sound.recipes.fut02 import context5 as C5, voxsynth as Vs
        main, throw = Vs.sung_parts(C5.G, C5.RIFF_AT, C5.L, lambda *k: D.rng(K.KIT, 'ContextVoxRiff', *k))
        out[DERIVED[kit][0]] = (main + throw) * D.undb(STEM_GAIN_DB[kit])
    elif kit == 'FUT03':
        from tools.sound.kits import fut03 as K
        from tools.sound.recipes.fut03 import context5 as C5, voxsynth as VS
        _, _, gain_db = Ph.kit_files(K, 'PhraseVoxRiff')      # context5.vox_lane: the riff at its item's gain
        tail = VS.riff_tail() * D.undb(gain_db)
        at = C5.G.at(C5.VOX_AT)
        y = np.zeros((at + len(tail), 2))
        y[at:] = tail                                       # unwrapped: its echoes run 2 bars past the cycle
        out[DERIVED[kit][0]] = y * D.undb(STEM_GAIN_DB[kit])
    return out


def write_derived(kit, root):
    """Write the kit's derived stems under root/<kit>/ as 48 kHz / 24-bit WAVs (as render.py encodes its masters):
    {path: SHA-256}."""
    import soundfile as sf
    out = {}
    for path, audio in render_derived(kit).items():
        dest = Path(root) / kit.lower() / path
        dest.parent.mkdir(parents=True, exist_ok=True)
        if np.max(np.abs(audio)) >= 1.0:
            raise ValueError(f'{kit} {path} clips at 24 bits: lower STEM_GAIN_DB')
        sf.write(dest, np.asarray(audio, dtype=float), SR, subtype='PCM_24', format='WAV')
        out[path] = hashlib.sha256(dest.read_bytes()).hexdigest()
    return out


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    ap.add_argument('--kit', required=True, choices=sorted(_SPECS))
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
