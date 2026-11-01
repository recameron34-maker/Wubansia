"""Each kit's written loop as Standard MIDI Files: the drums, basslines, lead phrase and chords its context loop plays,
at the kit's tempo with its swing written in, so they play the kit's samples in any DAW. A kit opts in with a `midi`
module in its recipe package that defines PARTS (see recipes/fut01/midi.py). Usage, from the repo root:

    .venv/bin/python tools/sound/midi.py --kit FUT02 [--out content/midi/fut02]

Format 1, 960 ticks a quarter, a tempo and 4/4 track first. Every track ends on the clip's last bar line, so a DAW
loops it there. Drums are on channel 10 (C1 = 36 is the kick), short triggers that let the samples ring out. A note
marked as a slide overlaps the next by 12 ticks, which a synth set to legato glide plays as a slide.
"""
import argparse
import importlib
import struct
import sys
from pathlib import Path

if __package__ in (None, ''):  # run as a script: import the tools.sound package from the repo root
    sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from tools.sound import kits  # noqa: E402

PPQ = 960
S16 = PPQ // 4
SLIDE_TICKS = 12
DRUM_CHANNEL = 9                    # channel 10
NOTE_NAMES = ('C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B')


def note_name(n):
    """MIDI note to name with C1 = 36 (the drum-rack convention: C3 = 60)."""
    return f'{NOTE_NAMES[n % 12]}{n // 12 - 2}'


# ------------------------------------------------------------------ parts, as recipes write them
def hits(pairs, note, length=0.5):
    """(16th, velocity) pairs as drum notes: short triggers."""
    return [(s, note, length, v, False) for s, v in pairs]


def per_bar(bar_hits, note, bars=4, extra=None, drop=None, length=0.5):
    """One bar's (16th, velocity) hits repeated over `bars`, with per-bar extra hits and dropped 16ths."""
    out = []
    for b in range(bars):
        for s, v in list(bar_hits) + list((extra or {}).get(b, ())):
            if s not in (drop or {}).get(b, ()):
                out.append((16 * b + s, note, length, v, False))
    return out


# ------------------------------------------------------------------ Standard MIDI File
def vlq(n):
    out = [n & 0x7F]
    n >>= 7
    while n:
        out.append(0x80 | (n & 0x7F))
        n >>= 7
    return bytes(reversed(out))


def track(name, events, end):
    """A track chunk: its name, then events (tick, order, message), note-offs before note-ons at the same tick, then
    the end of track on tick `end` (or the last event, if later)."""
    data = b'\x00\xff\x03' + vlq(len(name.encode())) + name.encode()
    t = 0
    for tick, _, msg in sorted(events, key=lambda e: (e[0], e[1], e[2])):
        data += vlq(tick - t) + msg
        t = tick
    data += vlq(max(0, end - t)) + b'\xff\x2f\x00'
    return b'MTrk' + struct.pack('>I', len(data)) + data


def note_events(notes, swing, channel, repeat=1, period=32):
    """notes: (16th, MIDI note, length in 16ths, velocity 0-1, slide). Odd 16ths land late by the kit's swing."""
    late = round((swing - 0.5) * 2 * S16)
    out = []
    for r in range(repeat):
        for step, note, length, vel, slide in notes:
            s = step + r * period
            on = s * S16 + (late if s % 2 else 0)
            off = on + max(1, round(length * S16)) + (SLIDE_TICKS if slide else 0)
            out.append((on, 1, bytes([0x90 | channel, note, max(1, min(127, round(vel * 127)))])))
            out.append((off, 0, bytes([0x80 | channel, note, 0])))
    return out


def smf(bpm, title, tracks, swing):
    """tracks: (name, notes, channel, repeat, period in 16ths). The clip is as long as its longest track."""
    end = max(rep * per for _, _, _, rep, per in tracks) * S16
    us = round(60_000_000 / bpm)
    head = [(0, 0, b'\xff\x51\x03' + us.to_bytes(3, 'big')), (0, 0, b'\xff\x58\x04\x04\x02\x18\x08')]
    chunks = [track(title, head, end)] + [track(n, note_events(notes, swing, ch, rep, per), end)
                                          for n, notes, ch, rep, per in tracks]
    return b'MThd' + struct.pack('>IHHH', 6, 1, len(chunks), PPQ) + b''.join(chunks)


# ------------------------------------------------------------------ a kit's files
def parts(kit):
    """The kit's PARTS (its recipe package's midi module), or None if it has none."""
    K = kits.load(kit)
    try:
        return K, importlib.import_module(f'{K.RECIPES}.midi').PARTS
    except ModuleNotFoundError:
        return K, None


def files(kit):
    """{file name: bytes} for every clip of the kit, plus its README.md."""
    K, P = parts(kit)
    if P is None:
        raise kits.UnknownName(f'{kit} has no midi module in {K.RECIPES}')
    title = f'{kit} {K.TITLE}, {K.BPM:g} BPM, {K.KEY}'
    lead, chords = P['lead'], P['chords']
    root = min(n for _, n, _, _, _ in chords['notes'])
    trigger = sorted({(s, root, ln, v, False) for s, _, ln, v, _ in chords['notes']})
    out = {f'{kit}_Drums_4bars.mid': smf(K.BPM, title, [('Drums', P['drums'], DRUM_CHANNEL, 1, 64)], K.SWING)}
    for name, line in P['lines'].items():
        tag = '_InTheLoop' if name == P['pick'] else ''
        out[f'{kit}_Bassline_{name}{tag}_2bars.mid'] = smf(K.BPM, title, [(f'Bass {name}', line, 0, 1, 32)], K.SWING)
    out[f'{kit}_Lead_{lead["sample"].replace(" ", "")}_2bars.mid'] = smf(
        K.BPM, title, [(lead['name'], lead['notes'], 1, 1, 32)], K.SWING)
    chord_file = chords['sample'].replace(' ', '').replace('#', 's')
    out[f'{kit}_Chords_{chord_file}_Voicing_4bars.mid'] = smf(
        K.BPM, title, [(chords['name'], chords['notes'], 2, 1, 64)], K.SWING)
    out[f'{kit}_Chords_{chord_file}_SampleTrigger_4bars.mid'] = smf(
        K.BPM, title, [('Chord sample trigger', trigger, 2, 1, 64)], K.SWING)
    out[f'{kit}_FullLoop_4bars.mid'] = smf(K.BPM, title, [
        ('Drums', P['drums'], DRUM_CHANNEL, 1, 64), (f'Bass {P["pick"]}', P['lines'][P['pick']], 0, 2, 32),
        (lead['name'], lead['notes'], 1, 2, 32), (chords['name'], chords['notes'], 2, 1, 64)], K.SWING)
    out['README.md'] = readme(kit, K, P, root, sorted(out)).encode()
    return out


def readme(kit, K, P, root, names):
    drums = '\n'.join(f'| {n} ({note_name(n)}) | {name} |' for n, name in sorted(P['drum_map'].items()))
    clips = '\n'.join(f'- `{n}`' for n in names if n.endswith('.mid'))
    return f"""# {kit} "{K.TITLE}": the written loop as MIDI

{K.BPM:g} BPM, {K.KEY} ({K.CAMELOT}), swing {round(K.SWING * 100)}% written in. Made by `tools/sound/midi.py` from the
kit's recipes (`{K.RECIPES.replace('.', '/')}/midi.py`); a test keeps these files equal to a fresh export.

{clips}

**Drums** (channel 10): a drum rack with the kit's samples on these notes (C1 = 36).

| Note | Sample |
|---|---|
{drums}

{P['drum_note']}

**Basslines** (2 bars; `InTheLoop` is the one the context loop plays): load {P['bass_samples']} into a sampler as a
multisample, each file rooted at its note ({P['bass_zones']}). A is clean, B is warm. A slide overlaps the next note
by {SLIDE_TICKS} ticks, so a synth set to legato glide slides there.

**Lead** (2 bars): {P['lead']['sample']}, rooted at {P['lead']['zones']}.

**Chords:** the voicing file is for your own synth. The {P['chords']['sample']} sample is the whole chord, so play it
with the sample trigger file: one note on {note_name(root)}.

**Full loop** (4 bars): all of the above in one file, the bassline and lead played twice.
"""


def write(kit, out_dir):
    out_dir.mkdir(parents=True, exist_ok=True)
    made = files(kit)
    for name, data in made.items():
        (out_dir / name).write_bytes(data)
    return sorted(made)


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    ap.add_argument('--kit', required=True)
    ap.add_argument('--out', help='folder (default: content/midi/<kit>)')
    a = ap.parse_args(argv)
    out = Path(a.out) if a.out else Path(__file__).resolve().parents[2] / 'content' / 'midi' / a.kit.lower()
    try:
        names = write(a.kit, out)
    except (kits.UnknownName, kits.KitNotReady) as e:
        print(f'error: {e}', file=sys.stderr)
        return 2
    print(f'{a.kit}: {len(names)} files -> {out}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
