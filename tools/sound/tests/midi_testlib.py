"""Shared helpers for the MIDI tests (test_midi.py, the kit-agnostic properties, and test_fut0N_midi.py, each kit's own
expectations): an independent SMF reader, the exported files of a kit, the kit's swing grid, and LoopCase, the test a
kit's file runs to check its Loop files against its round 5 context loop. Not a test module itself."""
import functools
import json
import math
import re
import struct

from pathlib import Path

from tools.sound import channels as CH
from tools.sound import kits
from tools.sound import midi as Mi

REPO = Path(__file__).resolve().parents[3]
SOUND = REPO / 'tools' / 'sound'
KITS = [k for k in kits.names() if (SOUND / 'recipes' / k.lower() / 'midi.py').exists()]
MARKER, CUE, TEMPO, METER, KEYSIG = 0x06, 0x07, 0x51, 0x58, 0x59
VARIANTS = ('vox', 'novox')
NAME = {'vox': 'Vox', 'novox': 'NoVox'}
BAR = 4 * 960


# ------------------------------------------------------------------ an independent SMF reader
def vlq_at(d, j):
    v = 0
    while True:
        c = d[j]
        j += 1
        v = (v << 7) | (c & 0x7F)
        if not c & 0x80:
            return v, j


def parse(data):
    """(format, ppq, tracks). A track: name, end (its end-of-track tick), events [(tick, kind, ch, a, b)] in file
    order, kind 'on', 'off', 'cc' or 'meta' (a: the meta type, b: its body); notes [(on, off, ch, key, vel)] paired;
    ccs [(tick, ch, controller, value)]; meta [(tick, type, body)]; tempo (us a quarter, or None)."""
    assert data[:4] == b'MThd'
    _, fmt, ntrk, ppq = struct.unpack('>IHHH', data[4:14])
    i, tracks = 14, []
    for _ in range(ntrk):
        assert data[i:i + 4] == b'MTrk'
        n = struct.unpack('>I', data[i + 4:i + 8])[0]
        d, i = data[i + 8:i + 8 + n], i + 8 + n
        j = t = 0
        tr = dict(name=None, end=None, events=[], meta=[], tempo=None)
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
                tr['events'].append((t, 'meta', None, kind, body))
            else:
                hi = st & 0xF0
                assert hi in (0x80, 0x90, 0xB0), f'status {st:#x}: running status or an unexpected message'
                a, b = d[j + 1], d[j + 2]
                assert a < 128 and b < 128
                kind = 'cc' if hi == 0xB0 else ('off' if hi == 0x80 or b == 0 else 'on')
                tr['events'].append((t, kind, (st & 0x0F) + 1, a, b))
                j += 3
        assert tr['end'] is not None and j == len(d)
        sounding, notes = {}, []
        for t, kind, ch, a, b in tr['events']:
            if kind == 'on':
                assert (ch, a) not in sounding, f'{tr["name"]}: a second note-on of {a} on {ch} at {t}'
                sounding[(ch, a)] = (t, b)
            elif kind == 'off':
                on, vel = sounding.pop((ch, a))
                assert t > on, f'{tr["name"]}: a zero-length note {a} at {t}'
                notes.append((on, t, ch, a, vel))
        assert not sounding, f'{tr["name"]}: notes left on: {sounding}'
        tr['notes'] = sorted(notes)
        tr['ccs'] = [(t, ch, a, b) for t, kind, ch, a, b in tr['events'] if kind == 'cc']
        tracks.append(tr)
    assert i == len(data)
    return fmt, ppq, tracks


def bars(name):
    return int(re.search(r'_(\d+)bars\.mid$', name).group(1))


@functools.lru_cache(maxsize=None)
def kit_files(kit):
    return Mi.files(kit)


@functools.lru_cache(maxsize=None)
def ext(kit, variant):
    """The kit's extended mix in a variant: (its files, regions.json as data)."""
    made = Mi.extended(kit, variant)
    return made, json.loads(made['regions.json'])


@functools.lru_cache(maxsize=None)
def parsed(kit, path):
    """A MIDI file of the kit by its path in content/midi/<kit>/, parsed."""
    return parse(mids(kit)[path])


@functools.lru_cache(maxsize=None)
def mids(kit):
    """Every MIDI file of the kit, by its path in content/midi/<kit>/."""
    out = {n: d for n, d in kit_files(kit).items() if n.endswith('.mid')}
    for v in VARIANTS:
        out.update({f'{Mi.EXTENDED}/{v}/{n}': d for n, d in ext(kit, v)[0].items() if n.endswith('.mid')})
    return out


def loop(kit, variant):
    """The kit's Loop file in a variant: {track name: track}."""
    _, _, tracks = parsed(kit, f'{kit}_Loop_{NAME[variant]}_4bars.mid')
    return {t['name']: t for t in tracks[1:]}


def whole(kit, variant):
    """(regions.json, the whole mix's tracks by name, its conductor)."""
    made, R = ext(kit, variant)
    _, _, tracks = parse(made[R['file']])
    return R, {t['name']: t for t in tracks[1:]}, tracks[0]


def ons(track, lo=0, hi=None):
    """A track's notes starting in [lo, hi): (on, key, vel)."""
    return [(on, key, vel) for on, _, _, key, vel in track['notes'] if lo <= on and (hi is None or on < hi)]


def lane_tracks(kit, tracks, lane):
    return [t for name, t in tracks.items() if CH.track_lane(kit, name) == lane]


def lane_ons(kit, tracks, lane, lo=0, hi=None):
    return sorted(n for t in lane_tracks(kit, tracks, lane) for n in ons(t, lo, hi))


def late(kit):
    """The kit's swing in ticks: odd 16ths land round((swing - 0.5) x 480) ticks late."""
    return round((kits.load(kit).SWING - 0.5) * 480)


def at(kit, step):
    """A 16th as a tick on the kit's swing (a fractional 16th: its whole 16th's tick plus the rest, straight)."""
    base = math.floor(step)
    return round(base * 240) + (late(kit) if base % 2 else 0) + round((step - base) * 240)


def v127(v, layers=None):
    """A velocity 0-1 as written: round(v x 127), unless that picks another layer than v does (the recipe takes the
    louder layer on a tie), then the integer nearest v x 127 that picks v's layer."""
    vel = max(1, min(127, round(v * 127)))
    if not layers or nearest(layers, vel / 127) == nearest(layers, v):
        return vel
    ok = [c for c in range(1, 128) if nearest(layers, c / 127) == nearest(layers, v)]
    return min(ok, key=lambda c: (abs(c - v * 127), c))


def nearest(layers, v):
    """The layer the recipes pick for a velocity: the nearest, the louder on a tie (phrase.hits_lane)."""
    return min(range(len(layers)), key=lambda i: (abs(layers[i] - v), -i))


def layers_of(kit, name):
    K = kits.load(kit)
    return kits.items(K, only=(name,))[0].layers


def ticks(kit, frames):
    """A sample's length at 48 kHz in ticks at the kit's tempo."""
    return round(frames * kits.load(kit).BPM / 3000)


def tick_step(kit, tick):
    """A note-on tick as its 16th (a swung odd 16th back on its grid)."""
    off = late(kit)
    return (tick - off) // 240 if tick % 480 == 240 + off else tick // 240


class LoopCase:
    """A kit's Loop files against its round 5 context loop, lane by lane (mixed into a kit's TestCase, which sets KIT
    and defines expected(variant): {track: [(on tick, key, the recipe's velocity 0-1, off tick or None)]}, None where
    the gate is checked by the timing tests: the melodic lanes)."""
    KIT = None

    def expected(self, variant):
        raise NotImplementedError

    def test_each_loop_reproduces_the_context_loop_lane_by_lane(self):
        """Every onset, key and velocity, and every length but the melodic lanes' gates; the variant's tracks and no
        others."""
        kit, k = self.KIT, Mi.kit_of(self.KIT)
        for v in VARIANTS:
            got = loop(kit, v)
            want = {name: sorted(rows) for name, rows in self.expected(v).items()}
            with self.subTest(variant=v):
                self.assertEqual(sorted(want), sorted(n for n, t in got.items() if t['notes']))
            for name, rows in want.items():
                with self.subTest(variant=v, track=name):
                    notes = got[name]['notes']
                    ch = CH.track_channel(kit, name)
                    self.assertEqual([(on, key, v127(vel, k.layers(ch, key))) for on, key, vel, _ in rows],
                                     [(on, key, vel) for on, _, _, key, vel in notes])
                    for (on, key, vel, off), n in zip(rows, notes):
                        if off is not None:
                            self.assertEqual(off, n[1], (on, key))

    def test_velocities_pick_the_recipes_layers(self):
        """Every Loop file velocity picks the layer the recipe picks from its own velocity (the nearest layer, the
        louder on a tie), on the r5 Items; one-shots placed by layer carry round(layer x 127)."""
        kit, k = self.KIT, Mi.kit_of(self.KIT)
        for v in VARIANTS:
            got = loop(kit, v)
            for name, rows in self.expected(v).items():
                for (on, key, want, _), n in zip(sorted(rows), got[name]['notes']):
                    ch, vel = n[2], n[4]
                    if ch == 3:
                        continue
                    with self.subTest(variant=v, track=name, on=on, key=key):
                        layers = k.layers(ch, key)
                        self.assertEqual(nearest(layers, want), nearest(layers, vel / 127))


def one_shot(kit, role, item, step, layer, align, zone=None, start=0, gate=None, vel=None):
    """A one-shot on channels 6-8 as the recipe places it: (on, key, velocity 0-1, off): its key, its layer's velocity,
    its length (the gate, else the sample's from its start), started on its 16th or ended on it."""
    k = Mi.kit_of(kit)
    key = k.key_of(role, item, zone, start)
    v = layers_of(kit, item)[layer - 1] if vel is None else vel
    length = round(gate * 240) if gate else ticks(kit, k.frames(item, zone)) - round(start * 240)
    if align == 'end':
        return (at(kit, step) - length, key, v, at(kit, step))
    return (at(kit, step), key, v, min(at(kit, step) + length, 64 * 240))


def drum(kit, pairs, key):
    """(16th, velocity) hits as the drums' rows: 120-tick triggers."""
    return [(at(kit, s), key, v, at(kit, s) + 120) for s, v in pairs]


def ride(kit, track, cc, variant='vox'):
    """A Loop file track's ride: {tick: value}."""
    return {tick: val for tick, _, c, val in loop(kit, variant)[track]['ccs'] if c == cc}


def value_at(points, tick):
    """A ride's value at a tick: its last point at or before it."""
    return points[max(t for t in points if t <= tick)]


def phrase_channels(kit):
    """{phrase file stem: channel} of the kit's phrase files."""
    out = {}
    for n, d in kit_files(kit).items():
        m = re.fullmatch(rf'{kit}_Phrase_([A-Za-z0-9]+)_\d+bars\.mid', n)
        if m:
            (chans,) = [{ch for _, _, ch, _, _ in t['notes']} for t in parse(d)[2][1:]]
            (out[m[1]],) = chans
    return out


def chord_roots(kit):
    """The keys of the kit's chord file (channel 4) and the channels of its two chord files."""
    made = kit_files(kit)
    (trig,) = [n for n in made if n.startswith(f'{kit}_Chords_') and not n.endswith('_Voicing_4bars.mid')]
    (voic,) = [n for n in made if n.endswith('_Voicing_4bars.mid')]
    notes = parse(made[trig])[2][1]['notes']
    return ({key for *_, key, _ in notes}, {ch for _, _, ch, _, _ in notes},
            {ch for _, _, ch, _, _ in parse(made[voic])[2][1]['notes']})
