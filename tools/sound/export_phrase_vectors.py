"""The phrase lanes' test vectors (P6): what tools/sound/phrase_tables.py's models of the engine's matcher (resolve)
and phrase voice (play) give on the kits' tables and Loop files, for the C++ tests (engine/tests/test_phrase_match.cpp,
test_phrase_voice.cpp). CI has no samples, and the stems are megabytes, so the voice's vectors play noise stems made
from a hash the C++ makes too, and keep only the SHA-256 of what the model plays.

    python3 tools/sound/export_phrase_vectors.py                  # writes engine/tests/vectors/phrase/
    python3 tools/sound/export_phrase_vectors.py --check          # exits 1 if a committed file differs
    python3 tools/sound/export_phrase_vectors.py --midi DIR       # the Loop files from DIR/<kit>/ (content/midi)

vectors.sha256 lists each file's SHA-256, which the C++ tests check before they trust a file. The JSON is the strict
integer-only kind core/gen/json.h reads: frames at 48 kHz, positions in milliticks (960 PPQ x 1000).

- match_<kit>.json: the kit's phrase tables (as kit.json's phrases hold them, with the stems' frames; a slice is
  [step, key, start, end, fade, lead, [holds]]), its BPM and swing, and per Loop file (vox, novox) every note and the
  CC74 and CC71 of each phrase channel, with what resolve () makes of each note. A note is [tick, ch, key, velocity,
  phrase, slice, continues, slice_pos, slice_len, slice_preroll]: a slice of a phrase (the kit's phrase index, the
  phrase's own slice index), a note a slice holds (its phrase, slice -1), or the multisample (phrase -1). A slice also
  has whether it reads on into the next (continues, 0 or 1: play ()'s rule), where the engine's slice note goes
  (slice_pos: its cycle's start plus its start frame at the kit's BPM, round the loop), how long it is (slice_len:
  its frames at the kit's BPM) and its preroll (its 16th on the swung grid less slice_pos); 0 otherwise. A
  controller is [tick, ch, cc, value].
- voice.json: per Loop file and phrase lane that plays, play () of a noise stem (noise () below) at the lane's table
  over one pass of the loop from silence, at the kit's tempo and 48 kHz: the stem, the slices, the notes as [the
  frame its slice starts on (slice_frame (): before 0 where a slice starts before the song), slice, continues], and
  the SHA-256 of the float32 output (L then R, planar), whole and per block of BLOCK frames (16 hex digits each).
  Plus FUT02's riff twice over two cycles (no Loop file plays it): its slice 0 starts before its cycle and wraps
  through the stem's end. Its masks are FUT02's recipe masks where a phrase voice fades: the bass's mute before the
  drop glide (context.mute_mask) and the arp's rest (context.rest_mask) out and back in, as float32 in masks.f32,
  with the notes that play them on a stem of 1.0. The model equals them (checked here); the C++ checks the voice
  within 1e-6.
"""
import argparse
import hashlib
import importlib
import json
import math
import sys
from pathlib import Path

if __package__ in (None, ''):  # run as a script: import the tools.sound package (it pins numpy's SIMD) first
    sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
    import tools.sound  # noqa: F401

import numpy as np  # noqa: E402

from tools.sound import kits  # noqa: E402
from tools.sound import phrase_tables as PT  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'engine' / 'tests' / 'vectors' / 'phrase'
KITS = ('FUT01', 'FUT02', 'FUT03', 'FUT04')
VARIANTS = (('vox', 'Vox'), ('novox', 'NoVox'))
BLOCK = 8192                      # frames per block hash
LOOP_MT = 15360 * 1000            # the 4-bar loop in milliticks
MT_16TH = 240000                  # milliticks in a 16th
MASK_PAD = 16                     # frames compared each side of a mask's edge
MASK_KIT = 'FUT02'                # whose recipe masks the voice's fades are checked against
TWICE = ('FUT02', 'vox_twice')    # a table whose slice 0 starts before its cycle and wraps


# ------------------------------------------------------------------ what the C++ makes the same way
def noise(seed, frames, channels):
    """A test stem, (frames, channels): every value a 24-bit step ((h >> 8) - 2^23) / 2^23 of a 32-bit hash h of
    (seed, frame, channel) (lowbias32), exact in float32. engine/tests/phrase_fixtures.h makes the same."""
    m = np.uint64(0xFFFFFFFF)
    n = np.arange(frames, dtype=np.uint64)
    out = []
    for c in range(channels):
        base = np.uint64((seed * 0x85EBCA77 + c * 0xC2B2AE3D) & 0xFFFFFFFF)
        x = (n * np.uint64(0x9E3779B1) + base) & m
        x ^= x >> np.uint64(16)
        x = (x * np.uint64(0x7FEB352D)) & m
        x ^= x >> np.uint64(15)
        x = (x * np.uint64(0x846CA68B)) & m
        x ^= x >> np.uint64(16)
        out.append(((x >> np.uint64(8)).astype(np.int64) - (1 << 23)) / float(1 << 23))
    return np.stack(out, 1)


def frames_to_mt(frames, bpm):
    """48 kHz frames as milliticks at bpm, rounded half up: frames x bpm / 3 (the timeline's pre-roll step)."""
    return (2 * frames * bpm + 3) // 6


def grid_mt(sixteenth, swing_pm):
    """A 16th's start in milliticks: odd 16ths (swing_pm - 500) x 480 late, as the timeline's swing snap puts them."""
    return sixteenth * MT_16TH + ((swing_pm - 500) * 480 if sixteenth % 2 else 0)


# ------------------------------------------------------------------ the tables and the files
def kit_tables(kit):
    """The kit's phrase tables as its committed kit.json holds them (content/plugin/<kit>/<KIT>_kit.json, in its order:
    the engine's phrase indices), each paired with phrase_tables' spec for its line's name, and the stems' frames.
    Returns (K, specs, tables, frames)."""
    K = kits.module(kit)
    manifest = json.loads((ROOT / 'content' / 'plugin' / kit.lower() / f'{kit}_kit.json').read_text(encoding='utf-8'))
    tables, specs = manifest['phrases'], PT.specs(kit)
    if [(s.ch, s.cycle, len(s.notes)) for s in specs] != [(t['ch'], t['cycle_16ths'], len(t['slices'])) for t in tables]:
        raise ValueError(f'{kit}: kit.json\'s phrases are not phrase_tables\' (re-export the manifest)')
    frames = [min(manifest['files'][i]['frames'] for i in t['stems'].values() if i >= 0) for t in tables]
    return K, specs, tables, frames


def loop_frames(K):
    return 2 * PT.cycle_frames(K, 32)


def read_loop(midi, kit, variant):
    """The Loop file's notes per channel, [(tick, 16th, key, velocity)] by tick then key, and its controllers."""
    data = (Path(midi) / kit.lower() / f'{kit}_Loop_{variant}_4bars.mid').read_bytes()
    K = kits.module(kit)
    notes = {}
    for tick, ch, key, vel in PT.smf_notes(data):
        notes.setdefault(ch, []).append((tick, PT.tick_16th(K, tick), key, vel))
    return {ch: sorted(v, key=lambda n: (n[0], n[2])) for ch, v in notes.items()}, PT.smf_ccs(data)


def resolve_channel(K, specs, tables, ch, notes, ccs):
    """resolve () on one channel: (the kit's phrase indices on it, matched [(16th, local table, slice)], unmatched)."""
    on_ch = [i for i, s in enumerate(specs) if s.ch == ch]
    got, unmatched = PT.resolve(K, [(p, key) for _, p, key, _ in notes], [tables[i] for i in on_ch], ccs)
    return on_ch, got, unmatched


def continues_flags(K, t, notes, length, frames, loop):
    """play ()'s rule (phrase_tables.play () keeps it inside), for one table's notes [(16th, slice)] sorted as play ()
    sorts them: whether each note's slice reads on into the next note's: the next slice in the same cycle, or slice 0
    in the next cycle after the last, starting where it ends modulo the stem's `frames` (docs/plugin/manifest.md 2.6).
    loop: round a loop of `length` frames, as the engine's timeline (annotatePhrases) has it; else one pass from
    silence, as play () has it (the last note has no follower)."""
    sl, cyc = t['slices'], t['cycle_16ths']
    ring = length // PT.cycle_frames(K, cyc)
    cycle_of = lambda pos: math.floor(pos) // cyc  # noqa: E731

    def continues(a, b):
        (pa, ja), (pb, jb) = a, b
        gap = cycle_of(pb) - cycle_of(pa)
        nxt = (jb == ja + 1 and gap == 0) or \
            (ja == len(sl) - 1 and jb == 0 and (gap % ring == 1 % ring if loop else gap == 1))
        return nxt and (sl[ja]['end'] - sl[jb]['start']) % frames == 0

    if loop:
        return [len(notes) > 1 and continues(notes[i], notes[(i + 1) % len(notes)]) for i in range(len(notes))]
    return [i + 1 < len(notes) and continues(notes[i], notes[i + 1]) for i in range(len(notes))]


def slice_json(x):
    return [x['step'], x['key'], x['start'], x['end'], x['fade'], x['lead'], list(x['holds'])]


def phrase_json(s, t, frames):
    return {'line': s.line, 'lane': t['lane'], 'ch': t['ch'], 'cycle_16ths': t['cycle_16ths'],
            'tolerance_pm': t['tolerance_pm'], 'fade_us': t['fade_us'], 'fade_in_us': t['fade_in_us'],
            'throw': t['throw'], 'gain_mdb': t['gain_mdb'], 'stem_frames': frames,
            'ride74': list(t['ride']['cc74']), 'ride71': list(t['ride']['cc71']),
            'slices': [slice_json(x) for x in t['slices']]}


# ------------------------------------------------------------------ match_<kit>.json
def twice_loop(K, specs, tables):
    """FUT02's riff twice, as a Loop file would hold it (none does): its table's own notes on its channel, at the ticks
    midi.py writes, in one 4-bar cycle; no controllers. Its slice 0 starts before the cycle."""
    gi = next(i for i, s in enumerate(specs) if s.line == TWICE[1])
    ch = specs[gi].ch
    notes = sorted((PT.step_tick(K, x['step']), x['key']) for x in tables[gi]['slices'])
    return {ch: [(tick, PT.tick_16th(K, tick), key, 100) for tick, key in notes]}, {}


def match_vectors(kit, midi):
    K, specs, tables, frames = kit_tables(kit)
    bpm, swing_pm = int(K.BPM), int(round(K.SWING * 1000))
    length = loop_frames(K)
    out_files = []
    sources = [(variant, *read_loop(midi, kit, name)) for variant, name in VARIANTS]
    if kit == TWICE[0]:
        sources.append(('twice', *twice_loop(K, specs, tables)))
    for variant, notes, ccs in sources:
        notes_out, ccs_out = [], []
        for ch in sorted({s.ch for s in specs}):
            on_ch, got, unmatched = resolve_channel(K, specs, tables, ch, notes.get(ch, []), ccs.get(ch, {}))
            matched = {}                       # (16th, key) -> [(kit phrase, slice)], as resolve () matched them
            for pos, ti, j in got:
                gi = on_ch[ti]
                matched.setdefault((pos, tables[gi]['slices'][j]['key']), []).append((gi, j))
            flags = {}
            for ti, gi in enumerate(on_ch):
                mine = sorted((pos, j) for pos, i, j in got if i == ti)
                flags.update({(pos, gi, j): f for (pos, j), f in
                              zip(mine, continues_flags(K, tables[gi], mine, length, frames[gi], True))})
            left = list(unmatched)
            for tick, pos, key, vel in notes.get(ch, []):
                e = [tick, ch, key, vel, -1, -1, 0, 0, 0, 0]
                if matched.get((pos, key)):
                    gi, j = matched[(pos, key)].pop(0)
                    sl, cyc = tables[gi]['slices'][j], tables[gi]['cycle_16ths']
                    c = math.floor(pos) // cyc
                    at = c * cyc * MT_16TH + frames_to_mt(sl['start'], bpm)
                    if sl['start'] + sl['lead'] >= frames[gi]:
                        at -= cyc * MT_16TH       # a slice 0 whose pre-roll precedes its cycle: in the cycle before
                    preroll = grid_mt(c * cyc + sl['step'], swing_pm) - at
                    e[4:] = [gi, j, int(flags[(pos, gi, j)]), at % LOOP_MT,
                             max(1000, frames_to_mt(sl['end'] - sl['start'], bpm)), preroll]
                elif (pos, key) in left:
                    left.remove((pos, key))
                else:                          # neither matched nor left on the multisample: a slice holds it
                    e[4] = on_ch[max((p, ti) for p, ti, _ in got if p <= pos)[1]]
                notes_out.append(e)
            for cc in (74, 71):
                ccs_out += [[t, ch, cc, v] for t, v in ccs.get(ch, {}).get(cc, [])]
        out_files.append({'variant': variant, 'notes': notes_out, 'ccs': sorted(ccs_out)})
    return {'kit': kit, 'bpm': bpm, 'swing_pm': swing_pm,
            'phrases': [phrase_json(s, t, f) for s, t, f in zip(specs, tables, frames)], 'files': out_files}


# ------------------------------------------------------------------ voice.json and masks.f32
def output_hashes(out):
    f = np.asarray(out, dtype=np.float32)
    planar = lambda a, b: f[a:b, 0].tobytes() + f[a:b, 1].tobytes()  # noqa: E731
    blocks = [hashlib.sha256(planar(i, min(i + BLOCK, len(f)))).hexdigest()[:16] for i in range(0, len(f), BLOCK)]
    return hashlib.sha256(planar(0, len(f))).hexdigest(), blocks


def voice_events(K, t, got, ti, length, frames):
    """Table ti's notes as the voice gets them over one pass from silence, in play ()'s order: the frame its slice
    starts on (slice_frame (); below 0 for a slice that starts before the song), the slice and whether it continues.
    The stem has `frames`."""
    notes = sorted((pos, j) for pos, i, j in got if i == ti)
    return [[int(PT.slice_frame(K, t, pos, j)), j, int(f)]
            for (pos, j), f in zip(notes, continues_flags(K, t, notes, length, frames, False))]


def undb(mdb):
    return float(10.0 ** (np.asarray(mdb / 1000.0, dtype=float) / 20.0))    # the model's: D.undb(gain_mdb / 1000)


def voice_case(K, kit, variant, s, t, frames, got, ti, length, seed, channels):
    stem = noise(seed, frames, channels)
    out = PT.play(K, t, stem * undb(t['gain_mdb']), got, length, table_index=ti)
    digest, blocks = output_hashes(out)
    return {'kit': kit, 'variant': variant, 'line': s.line, 'seed': seed, 'channels': channels, 'loop_frames': length,
            'phrase': phrase_json(s, t, frames), 'events': voice_events(K, t, got, ti, length, frames),
            'sha256': digest, 'blocks': blocks}


def voice_cases(midi):
    cases, seed = [], 1
    for kit in KITS:
        K, specs, tables, frames = kit_tables(kit)
        length = loop_frames(K)
        for variant, name in VARIANTS:
            notes, ccs = read_loop(midi, kit, name)
            for ch in sorted({s.ch for s in specs}):
                on_ch, got, _ = resolve_channel(K, specs, tables, ch, notes.get(ch, []), ccs.get(ch, {}))
                for ti, gi in enumerate(on_ch):
                    if not any(i == ti for _, i, _ in got):
                        continue
                    channels = 1 if (kit, specs[gi].line) == ('FUT04', 'blip') else 2   # one mono stem among them
                    cases.append(voice_case(K, kit, variant, specs[gi], tables[gi], frames[gi], got, ti, length, seed,
                                            channels))
                    seed += 1
    # FUT02's riff twice over two cycles, its notes from its table: its slice 0 starts before its cycle (cycle 0's
    # part before the song isn't played) and wraps through the stem's end; the last slice reads on into it
    K, specs, tables, frames = kit_tables(TWICE[0])
    gi = next(i for i, s in enumerate(specs) if s.line == TWICE[1])
    t, cyc = tables[gi], tables[gi]['cycle_16ths']
    on_ch = [i for i, s in enumerate(specs) if s.ch == specs[gi].ch]
    riff = [(c * cyc + x['step'], x['key']) for c in (0, 1) for x in t['slices']]
    got, unmatched = PT.resolve(K, riff, [tables[i] for i in on_ch])
    if unmatched or {i for _, i, _ in got} != {on_ch.index(gi)}:
        raise ValueError(f'{TWICE}: its own notes don\'t resolve to it')
    cases.append(voice_case(K, TWICE[0], 'twice', specs[gi], t, frames[gi], got, on_ch.index(gi),
                            2 * PT.cycle_frames(K, cyc), seed, 2))
    return cases


def mask_cases(midi):
    """FUT02's recipe masks where a phrase voice fades, and the notes that play them: the bass's slice before the drop
    glide fades on the mute's 5 ms edge, and the arp goes out over 10 ms into its rest and comes back over 3 ms on the
    downbeat. Each: the line, its phrase, its notes, and the edges [from, to) with their mask values' offset."""
    C = importlib.import_module(f'{kits.module(MASK_KIT).RECIPES}.context')   # through the registry (ADR 0008)
    K, specs, tables, frames = kit_tables(MASK_KIT)
    length = C.L
    if length != loop_frames(K):
        raise ValueError(f'{MASK_KIT}: the context loop is not the Loop file')
    notes, ccs = read_loop(midi, MASK_KIT, 'Vox')
    cases, values = [], []
    for line, mask, edges in (
            ('orbit', C.mute_mask(), [(C.G.at(C.GLIDE[0]) - int(C.MUTE_EDGE * PT.SR), C.G.at(C.GLIDE[0]))]),
            ('arp', C.rest_mask(), [(C.G.at(C.ARP_REST[0]) - int(C.ARP_REST[1] * PT.SR), C.G.at(C.ARP_REST[0])),
                                    (0, int(C.ARP_REST[2] * PT.SR))])):
        gi = next(i for i, s in enumerate(specs) if s.line == line)
        s, t = specs[gi], tables[gi]
        on_ch, got, _ = resolve_channel(K, specs, tables, s.ch, notes.get(s.ch, []), ccs.get(s.ch, {}))
        ti = on_ch.index(gi)
        model = PT.play(K, t, np.ones((frames[gi], 2)), got, length, table_index=ti)
        spans = []
        for lo, hi in edges:
            lo, hi = max(0, lo - MASK_PAD), hi + MASK_PAD
            if not np.array_equal(model[lo:hi, 0], mask[lo:hi]):
                raise ValueError(f'{MASK_KIT} {line}: the model leaves the recipe mask in [{lo}, {hi})')
            spans.append({'from': int(lo), 'to': int(hi), 'offset': len(values)})
            values += [float(v) for v in mask[lo:hi]]
        cases.append({'line': line, 'loop_frames': length, 'phrase': phrase_json(s, t, frames[gi]),
                      'events': voice_events(K, t, got, ti, length, frames[gi]), 'spans': spans})
    return cases, np.asarray(values, dtype='<f4')


# ------------------------------------------------------------------ output
def dumps(x):
    return json.dumps(x, separators=(',', ':'), ensure_ascii=False) + '\n'


def build(midi):
    out = {f'match_{kit.lower()}.json': dumps(match_vectors(kit, midi)).encode() for kit in KITS}
    masks, values = mask_cases(midi)
    out['voice.json'] = dumps({'block': BLOCK, 'cases': voice_cases(midi), 'masks': masks}).encode()
    out['masks.f32'] = values.tobytes()
    out['vectors.sha256'] = ''.join(f'{hashlib.sha256(b).hexdigest()}  {n}\n' for n, b in sorted(out.items())).encode()
    return out


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    ap.add_argument('--check', action='store_true', help='exit 1 if a committed file differs')
    ap.add_argument('--midi', default=str(ROOT / 'content' / 'midi'), help='where the Loop files are: <kit>/...')
    ap.add_argument('--out', default=str(OUT))
    a = ap.parse_args(argv)
    got, out = build(a.midi), Path(a.out)
    if a.check:
        bad = [n for n, b in got.items() if not (out / n).exists() or (out / n).read_bytes() != b]
        for n in bad:
            print(f'{n} differs', file=sys.stderr)
        return 1 if bad else 0
    out.mkdir(parents=True, exist_ok=True)
    for n, b in sorted(got.items()):
        (out / n).write_bytes(b)
        print(f'{out / n}  {len(b)} bytes')
    return 0


if __name__ == '__main__':
    sys.exit(main())
