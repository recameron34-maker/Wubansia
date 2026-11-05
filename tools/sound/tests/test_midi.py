"""MIDI v2 (tools/sound/midi.py; ADR 0019, docs/midi-map.md): every kit's written music as Standard MIDI Files on the
one channel map (tools/sound/channels.py), the files Wubansia plays and drags out.

What these tests pin (the design's midi_plan section 9, and the guarantees the v1 export already kept):
- committed files equal a fresh export, the same bytes in fresh processes;
- every note's channel is its instrument's, its key in its instrument's zones, keymap or drum map (35-59); one role a
  channel a kit; one lane a track; channel 3 holds only the voicing;
- valid format 1 files at 960 PPQ, the kit's tempo, 4/4 and key (FF59); on a tick note-offs, then controllers, then
  note-ons; no same-key overlaps; no note past its track's end; tracks end on bar lines; swing written in; gates that
  never run a swung note into the next one unless it slides; slides that reach their note;
- the Loop files reproduce each kit's round 5 context loop (context5.py) lane by lane, with the vox synth or without;
  the vox and no-vox files differ only where they should;
- velocities pick the recipe's layers; end-aligned notes end on bar or beat lines; CC74/71 rides only on the channels
  whose filter rides, CC91/94 only around thrown notes;
- the extended mixes: their forms, regions that tile them, an integer-only regions.json, 32-bar drums-only ends, cues
  with their notes, builds that roll up to the fill, busier hats that are busier, drops that aren't grooves.
"""
import functools
import hashlib
import json
import math
import os
import re
import struct
import subprocess
import sys
import types
import unittest
from pathlib import Path

from tools.sound import channels as CH
from tools.sound import kits
from tools.sound import midi as Mi
from tools.sound import qa as Q
from tools.sound.item import Item

REPO = Path(__file__).resolve().parents[3]
SOUND = REPO / 'tools' / 'sound'
KITS = [k for k in kits.names() if (SOUND / 'recipes' / k.lower() / 'midi.py').exists()]
MARKER, CUE, TEMPO, METER, KEYSIG = 0x06, 0x07, 0x51, 0x58, 0x59
VARIANTS = ('vox', 'novox')
NAME = {'vox': 'Vox', 'novox': 'NoVox'}
LATE = {'FUT01': 43, 'FUT02': 10, 'FUT03': 29, 'FUT04': 19}           # round((swing - 0.5) x 480)
KEYSIGS = {'FUT01': (3, 1), 'FUT02': (-1, 1), 'FUT03': (-2, 1), 'FUT04': (1, 1)}   # sharps (+) or flats (-), minor
RIDE_CHANNELS = {'FUT01': {1}, 'FUT02': {2}, 'FUT03': {4}, 'FUT04': {1}}
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


def at(kit, step):
    """A 16th as a tick on the kit's swing (a fractional 16th: its whole 16th's tick plus the rest, straight)."""
    base = math.floor(step)
    return round(base * 240) + (LATE[kit] if base % 2 else 0) + round((step - base) * 240)


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


# ------------------------------------------------------------------ each kit's round 5 context loop, from its recipes
def expected_loop(kit, variant):
    """The kit's context5 loop, lane by lane, read off its recipe modules (not off PARTS): {track: sorted [(on tick,
    key, the recipe's velocity 0-1, off tick or None)]}, None where the gate is checked separately (the melodic
    lanes)."""
    k = Mi.kit_of(kit)
    a = lambda s: at(kit, s)  # noqa: E731
    drum = lambda pairs, key: [(a(s), key, v, a(s) + 120) for s, v in pairs]  # noqa: E731
    out = {}

    def one_shot(role, item, step, layer, align, zone=None, start=0, gate=None, vel=None):
        key = k.key_of(role, item, zone, start)
        v = layers_of(kit, item)[layer - 1] if vel is None else vel
        length = round(gate * 240) if gate else ticks(kit, k.frames(item, zone)) - round(start * 240)
        if align == 'end':
            return (a(step) - length, key, v, a(step))
        return (a(step), key, v, min(a(step) + length, 64 * 240))

    if kit == 'FUT01':
        from tools.sound.recipes.fut01 import context4 as C, context5 as C5, keys as Ky, music as M, voxsynth as V
        out['Kick'] = drum(C.KICK, 36)
        out['Clap'] = drum(C.CLAP, 38)
        out['Hats'] = drum(C.HAT_C, 42) + drum(C.HAT_O, 46)
        out['Perc'] = drum(C.TAMB, 54) + drum(C.RIM, 37) + drum(C.PERC, 47)
        out['Bass'] = [(a(s + 32 * r), n, v, None) for r in (0, 1) for s, _, n, v, _ in C.WARP_LINE]
        out['Lead'] = [(a(s + 32 * r), n, v, None) for r in (0, 1) for s, n, _, v in M.PHRASE]
        stabs = [(s, z, v) for s, z, v in C.STABS if s not in C5.STAB_DROP[variant]]
        out['Chords'] = [(a(s), z, v, a(s) + 240) for s, z, v in stabs]
        out['Chord voicing'] = [(a(s), z + d, v, a(s) + 240) for s, z, v in stabs for d in Ky.SHAPE]
        if variant == 'vox':
            out['Vox'] = [(a(s + at_), n, v, None) for at_ in C5.RIFF_AT['vox'] for s, n, _, v, _ in V.RIFF_NOTES]
        out['Accents'] = [one_shot('accents', name, step, layer, how, gate=gate)
                          for _, name, step, layer, how, gate in C5.ACCENTS[variant]]
        out['FX'] = [one_shot('fx', 'GlitchFillLift', C.FILL_AT, 1, 'on', start=C.FILL_FROM)]
    elif kit == 'FUT02':
        from tools.sound.recipes.fut02 import bass as B, context as C, context5 as C5, lead as L, pad as Pd
        from tools.sound.recipes.fut02 import voxsynth as Vs
        out['Kick'] = drum(C.KICK, 36)
        out['Clap'] = drum(C.CLAP, 38)
        out['Hats'] = drum(C.HAT_CLOSED, 42) + drum(C.HAT_OPEN, 46)
        out['Perc'] = drum(C.ZAP, 47)
        out['Bass'] = [(a(s + 32 * r), n, v, None) for r in (0, 1) for s, n, _, v, _ in B.LINES[B.PICK]
                       if not C.GLIDE[0] <= s + 32 * r < C.GLIDE[1]]
        early = round(C.GLIDE_EARLY * kits.load(kit).BPM / 60 * 960)
        out['Drop Glide'] = [(a(C.GLIDE[0]), 26, layers_of(kit, 'DropGlideClean')[C.GLIDE_LAYER - 1],
                              a(C.GLIDE[1]) - early)]
        out['Lead'] = [(a(s + 32 * r), n, v, None) for r in (0, 1) for s, n, _, v in L.phrase_notes()
                       if s + 32 * r < C.ARP_REST[0]]
        out['Chords'] = [(0, 50, 1.0, 64 * 240)]
        out['Chord voicing'] = [(0, n, 1.0, 64 * 240) for n in Pd.CHORD]
        if variant == 'vox':
            out['Vox'] = [(a(s + C5.RIFF_AT), n, v, None) for s, n, _, v in Vs.RIFF]
        version = C5.VOX if variant == 'vox' else C5.NOVOX
        out['Accents'] = [one_shot('accents', item, step, layer, 'end' if ends else 'on', zone=zone)
                          for step, item, zone, layer, *_, ends in C5.ACCENTS[version]]
        out['FX'] = [one_shot('fx', 'LaserFill', C.FILL_AT, 1, 'on')]
    elif kit == 'FUT03':
        from tools.sound.recipes.fut03 import accents as A, context as C, context5 as C5, rubber as B
        from tools.sound.recipes.fut03 import voxsynth as VS
        from tools.sound.recipes.fut03 import midi as P3
        out['Kick'] = drum(C.KICK, 36)
        out['Clap'] = drum(C.CLAP, 38)
        out['Hats'] = sorted(drum([(s, v) for s, kind, v in C.HATS if kind == 'closed'], 42)
                             + drum([(s, v) for s, kind, v in C.HATS if kind == 'open'], 46))
        out['Perc'] = drum(C.SHAKER, 54) + [(a(s), 37 if n == 'Rimshot' else P3.HAND[(n, z)], v, a(s) + 120)
                                            for s, n, z, v in C.PERC]
        out['Bass'] = []
        for r in (0, 1):
            for s, ln, n, v, glide in B.LINES[B.PICK]:
                out['Bass'].append((a(s + 32 * r), n, v, None))
                if glide:
                    out['Bass'].append((at(kit, s + 32 * r + ln * glide[1]), glide[0], v, None))
        out['Chords'] = [(a(s), 55, v, a(s) + 360) for s, v in C.STAB]
        out['Chord voicing'] = [(a(s), n, v, a(s) + 360) for s, v in C.STAB for n in A.GM9]
        out['Pad Warm'] = [(0, 55, 1.0, 64 * 240)]
        if variant == 'vox':
            out['Vox'] = [(a(s + C5.VOX_AT), m, v, None) for s, m, _, v in VS.VOX_RIFF]
        out['Accents'] = [one_shot('accents', name, 64 if step == 'end' else step, 1, 'end' if step == 'end' else 'on')
                          for step, name in C5.ACCENTS[variant]]
    elif kit == 'FUT04':
        from tools.sound import dsp48 as D
        from tools.sound.recipes.fut04 import bass as B, context as C, context5 as C5, lead as L, pad as Pd
        line = B.LINE_BY_NAME[B.PICK]
        out['Kick'] = drum(C.KICK, 36)
        out['Rumble'] = [(0, 35, 1.0, 32 * 240), (a(32), 35, 1.0, a(C.KICK_OUT))]
        out['Snare'] = drum(C.SNARE, 38)
        out['Hats'] = drum(C.HAT_CLOSED, 42) + drum(C.HAT_OPEN, 46) + drum(C.RIDE, 51)
        out['Perc'] = drum(C.ANVIL, 47)
        out['Bass'] = [(a(s + 32 * r), n, v, None) for r in (0, 1)
                       for s, n, _, v, *_ in tuple(line.notes) + tuple(line.ticks)]
        out['Lead'] = [(at(kit, s + 32 * r) + round(k_ * 240 / rt), n, v * D.undb(L.RATCHET_DB * k_), None)
                       for r in (0, 1) for s, n, _, v, rt in L.PHRASE for k_ in range(rt)]
        out['Chords'] = [(0, 52, 1.0, 64 * 240)]
        out['Chord voicing'] = [(0, n, 1.0, 64 * 240) for n in Pd.CHORD]
        if variant == 'vox':
            out['Vox'] = [(a(s), m, v, None) for s, m, _, v in C5.VOX_NOTES]
        mode = C5.VOX if variant == 'vox' else C5.NOVOX
        out['Accents'] = [one_shot('accents', name, 64 if s == 'end' else s, 1, 'end' if s == 'end' else 'on')
                          for s, name in C5.ACCENTS[mode]]
        out['FX'] = [one_shot('fx', 'StutterFill', C.FILL_AT, 1, 'on')]
    return {name: sorted(rows) for name, rows in out.items()}


class TestChannels(unittest.TestCase):
    def test_some_kits_export(self):
        self.assertEqual(['FUT01', 'FUT02', 'FUT03', 'FUT04'], KITS)

    def test_the_table_is_the_designs(self):
        """The design's midi_plan section 1, which the engine's C++ table copies: change it only through ADR 0019."""
        self.assertEqual({'bass': 1, 'lead': 2, 'voicing': 3, 'chords': 4, 'vox': 5, 'accents': 6, 'fx': 7,
                          'chops': 8, 'reserved': 9, 'drums': 10}, CH.CHANNELS)
        self.assertEqual({'accents': 48, 'fx': 48, 'chops': 48}, CH.KEY_BASE)
        self.assertEqual((35, 59), CH.DRUM_RANGE)
        self.assertEqual(('kick', 'snare', 'hats', 'perc', 'bass', 'lead', 'chords', 'vox', 'accents', 'fx'),
                         CH.LANES)
        self.assertEqual({'FUT01': ['Organ Bass', 'Organ Lead'],
                          'FUT02': ['Drop Glide', 'Laser Chord Stab', 'Glass Ping'], 'FUT03': ['Pad Warm'],
                          'FUT04': ['Acid Stab', 'Metal Chord', 'Rave Stab']},
                         {k: [n for _, n, _ in v] for k, v in CH.EXTRAS.items()})
        for kit, extras in CH.EXTRAS.items():
            self.assertEqual(list(range(11, 11 + len(extras))), [ch for ch, _, _ in extras], kit)
            self.assertLessEqual({lane for _, _, lane in extras}, set(CH.LANES))

    def test_every_track_resolves_to_one_lane_on_its_channel(self):
        for kit in KITS:
            for path in mids(kit):
                with self.subTest(kit=kit, file=path):
                    for t in parsed(kit, path)[2][1:]:
                        lane = CH.track_lane(kit, t['name'])
                        self.assertIn(lane, CH.LANES)
                        chans = {ch for _, _, ch, _, _ in t['notes']} | {ch for _, ch, _, _ in t['ccs']}
                        self.assertLessEqual(chans, {CH.track_channel(kit, t['name'])}, t['name'])

    def test_one_role_a_channel_and_channel_3_only_the_voicing(self):
        for kit in KITS:
            roles = CH.channel_roles(kit)
            for path in mids(kit):
                for t in parsed(kit, path)[2][1:]:
                    for _, _, ch, _, _ in t['notes']:
                        self.assertIn(ch, roles, (kit, path, t['name']))
                        if ch == CH.CHANNELS['voicing']:
                            self.assertEqual('Chord voicing', t['name'], (kit, path))
                    self.assertNotIn(CH.CHANNELS['reserved'], {ch for _, _, ch, _, _ in t['notes']})

    def test_keys_are_in_their_instruments_zones_or_maps(self):
        """Drums inside 35-59 on the kit's drum map; channels 6-8 on their keymaps from 48 up; every pitched note
        within an octave of its instrument's zones (a sample is repitched at most an octave)."""
        for kit in KITS:
            k = Mi.kit_of(kit)
            self.assertEqual([], [n for n in k.drum_map if not 35 <= n <= 59], kit)
            for role in ('accents', 'fx', 'chops'):
                keys = sorted(k.keymap(role))
                self.assertEqual(list(range(48, 48 + len(keys))), keys, (kit, role))
            for path in mids(kit):
                for t in parsed(kit, path)[2][1:]:
                    for on, _, ch, key, _ in t['notes']:
                        with self.subTest(kit=kit, file=path, track=t['name'], key=key):
                            if ch == 10:
                                self.assertIn(key, k.drum_map)
                            elif ch in (6, 7, 8):
                                self.assertIn(key, k.keymap({6: 'accents', 7: 'fx', 8: 'chops'}[ch]))
                            elif ch != 3:
                                zones = k.zones(ch)
                                self.assertTrue(min(zones) - 12 <= key <= max(zones) + 12, zones)

    def test_every_sound_is_one_of_the_kits_items(self):
        for kit in KITS:
            k = Mi.kit_of(kit)
            names = {it.name for it in kits.items(kits.load(kit))}
            used = set(k.drum_map.values()) | {s.item for r in ('accents', 'fx', 'chops') for s in k.keymap(r).values()}
            used |= {name for ch in k.pitched_channels() for name in k.items_on(ch)}
            self.assertLessEqual(used, names, kit)


class TestFiles(unittest.TestCase):
    def test_the_kits_files(self):
        """The two loops, the part files (drums, basslines, lead, chords and their voicing, the accents in each
        variant and the FX) and the synth phrases, with a README; the extended mix in each variant's folder."""
        for kit in KITS:
            made = kit_files(kit)
            k = Mi.kit_of(kit)
            with self.subTest(kit=kit):
                for name in (f'{kit}_Loop_Vox_4bars.mid', f'{kit}_Loop_NoVox_4bars.mid', f'{kit}_Drums_4bars.mid',
                             f'{kit}_Accents_Vox_4bars.mid', f'{kit}_Accents_NoVox_4bars.mid', 'README.md'):
                    self.assertIn(name, made)
                self.assertFalse([n for n in made if 'FullLoop' in n or 'SampleTrigger' in n])
                chords = [n for n in made if n.startswith(f'{kit}_Chords_')]
                self.assertEqual(2, len(chords), chords)
                self.assertEqual(1, len([n for n in chords if n.endswith('_Voicing_4bars.mid')]))
                lines = [n for n in made if n.startswith(f'{kit}_Bassline_')]
                self.assertEqual(len(k.P['bass']['lines']), len(lines))
                self.assertEqual(1, len([n for n in lines if '_InTheLoop_' in n]))
                self.assertEqual(len(k.P['phrases']), len([n for n in made if n.startswith(f'{kit}_Phrase_')]))
                self.assertEqual(any(k.loop_fx()), f'{kit}_FX_4bars.mid' in made)
                for n in made:
                    self.assertRegex(n, rf'^({kit}_[A-Za-z0-9_]+_\d+bars\.mid|README\.md)$')

    def test_committed_files_match_a_fresh_export(self):
        for kit in KITS:
            folder = REPO / 'content' / 'midi' / kit.lower()
            self.assertEqual([Mi.EXTENDED], sorted(p.name for p in folder.iterdir() if p.is_dir()), kit)
            self.assertEqual(list(VARIANTS), sorted(p.name for p in (folder / Mi.EXTENDED).iterdir()), kit)
            subs = [(folder, kit_files(kit))] + [(folder / Mi.EXTENDED / v, ext(kit, v)[0]) for v in VARIANTS]
            for sub, made in subs:
                with self.subTest(kit=kit, folder=str(sub.relative_to(folder))):
                    self.assertEqual(sorted(made), sorted(p.name for p in sub.iterdir() if p.is_file()))
                    for name, data in made.items():
                        self.assertEqual(data, (sub / name).read_bytes(), name)

    def test_the_bytes_are_the_same_in_fresh_processes(self):
        """Two fresh processes with different hash seeds (string and set order) export the same bytes as this one."""
        code = ('import hashlib, sys; sys.path.insert(0, sys.argv[1]); from tools.sound import midi as Mi\n'
                'for kit in sys.argv[2:]:\n'
                '    made = dict(Mi.files(kit))\n'
                '    for v in Mi.VARIANTS:\n'
                '        made.update({v + "/" + k: d for k, d in Mi.extended(kit, v).items()})\n'
                '    for n, d in sorted(made.items()):\n'
                '        print(kit, n, hashlib.sha256(d).hexdigest())\n')
        runs = [subprocess.run([sys.executable, '-c', code, str(REPO), *KITS], capture_output=True, text=True,
                               check=True, env={**os.environ, 'PYTHONHASHSEED': seed}).stdout
                for seed in ('0', '20261105')]
        here = ''
        for kit in KITS:
            made = dict(kit_files(kit))
            for v in VARIANTS:
                made.update({f'{v}/{n}': d for n, d in ext(kit, v)[0].items()})
            here += ''.join(f'{kit} {n} {hashlib.sha256(d).hexdigest()}\n' for n, d in sorted(made.items()))
        self.assertEqual(runs[0], runs[1])
        self.assertEqual(here, runs[0])

    def test_strings_pass_the_denylists(self):
        strings = []
        for kit in KITS:
            strings += list(kit_files(kit)) + [kit_files(kit)['README.md'].decode()]
            for v in VARIANTS:
                made, R = ext(kit, v)
                strings += list(made) + [made['README.md'].decode(), made['regions.json'].decode()]
            for path in mids(kit):
                tracks = parsed(kit, path)[2]
                strings += [tr['name'] for tr in tracks]
                strings += [b.decode() for _, k, b in tracks[0]['meta'] if k in (MARKER, CUE)]
        self.assertEqual([], [c.name for c in Q.failed(Q.check_content(names=strings))])

    def test_the_readmes(self):
        """The kit's README names every file, its channel and what it plays, the roots as MIDI numbers, the keys of
        channels 6-8, the vox synth's vowels by velocity, and which lanes Wubansia plays from recorded phrases; the
        extended READMEs name every file of their folder and say which octave names they use."""
        for kit in KITS:
            text = kit_files(kit)['README.md'].decode()
            k = Mi.kit_of(kit)
            with self.subTest(kit=kit):
                for name in kit_files(kit):
                    if name.endswith('.mid'):
                        self.assertIn(f'`{name}`', text)
                for words in ('channel 10', 'channel 1', 'channel 4', 'channel 3', 'MIDI number', 'C4 = 60',
                              'recorded', 'CC74', 'FF 59'):
                    self.assertIn(words, text)
                for role in ('accents', 'fx'):
                    for key, sound in k.keymap(role).items():
                        self.assertIn(f'| {key} |', text)
                for v in VARIANTS:
                    made, R = ext(kit, v)
                    etext = made['README.md'].decode()
                    for name in [R['file'], 'regions.json'] + [s['file'] for s in R['sections']]:
                        self.assertIn(f'`{name}`', etext)
                    for words in ('C1 = 36', 'C3 = 60', 'C4 = 60', 'MIDI number'):
                        self.assertIn(words, etext)
                self.assertIn(f'`{Mi.EXTENDED}/vox/`', text)
                self.assertIn(f'`{Mi.EXTENDED}/novox/`', text)


class TestTiming(unittest.TestCase):
    def test_files_are_valid_with_the_kits_tempo_meter_and_key(self):
        """Format 1 at 960 PPQ; one tempo at tick 0, the integer microseconds nearest the kit's BPM (it reads the BPM
        to within 1.1 parts per million); 4/4; the key signature (FF 59); every track ends on the file's last bar
        line."""
        for kit in KITS:
            K = kits.load(kit)
            us = round(60_000_000 / K.BPM)
            self.assertEqual(us, Mi.tempo_us(K.BPM))
            self.assertLess(abs(60_000_000 / us / K.BPM - 1), 1.1e-6)
            sf, mi = KEYSIGS[kit]
            self.assertEqual((sf, mi), Mi.key_signature(K.KEY))
            for path in mids(kit):
                with self.subTest(kit=kit, file=path):
                    fmt, ppq, tracks = parsed(kit, path)
                    self.assertEqual((1, 960), (fmt, ppq))
                    self.assertEqual(us, tracks[0]['tempo'])
                    self.assertEqual(1, sum(k == TEMPO for _, k, _ in tracks[0]['meta']))
                    self.assertIn((0, METER, b'\x04\x02\x18\x08'), tracks[0]['meta'])
                    self.assertIn((0, KEYSIG, bytes([sf & 0xFF, mi])), tracks[0]['meta'])
                    for tr in tracks:
                        self.assertEqual(bars(path) * BAR, tr['end'])

    def test_on_a_tick_offs_then_controllers_then_ons(self):
        rank = {'off': 0, 'cc': 1, 'on': 2}
        for kit in KITS:
            for path in mids(kit):
                for t in parsed(kit, path)[2][1:]:
                    last = (-1, -1)
                    for tick, kind, *_ in t['events']:
                        if kind == 'meta':
                            continue
                        here = (tick, rank[kind])
                        self.assertGreaterEqual(here, last, (kit, path, t['name'], tick))
                        last = here

    def test_no_note_overlaps_its_own_key_or_runs_past_the_end(self):
        for kit in KITS:
            for path in mids(kit):
                for t in parsed(kit, path)[2][1:]:
                    last = {}
                    for on, off, ch, key, vel in t['notes']:
                        with self.subTest(kit=kit, file=path, track=t['name'], key=key, on=on):
                            self.assertLessEqual(off, t['end'])
                            self.assertTrue(1 <= vel <= 127)
                            self.assertGreaterEqual(on, last.get((ch, key), 0))
                            last[(ch, key)] = off

    def test_the_swing_is_written_in(self):
        """Odd 16ths late by round((swing - 0.5) x 480) ticks, every other note on the straight grid: the drums of
        each Loop file sit on exactly those ticks."""
        for kit in KITS:
            K = kits.load(kit)
            self.assertEqual(LATE[kit], round((K.SWING - 0.5) * 480))
            self.assertEqual(LATE[kit], Mi.kit_of(kit).late)
            for v in VARIANTS:
                for t in lane_tracks(kit, loop(kit, v), 'hats'):
                    offsets = {on % 480 for on, *_ in t['notes']}
                    self.assertLessEqual(offsets, {0, 240 + LATE[kit]}, (kit, t['name']))

    def test_gates_never_run_a_swung_note_into_the_next_one(self):
        """On a bass or vox track (a mono line) a note ends by the next note's start, unless that note is a written
        slide, which it runs into by SLIDE_TICKS at most (audit A, defect 5): FUT01's Bounce F#1 on its swung 16th 11
        no longer runs into the straight F#2 on 12, nor FUT04's E1 on 1 into E2 on 2. In each bassline file the
        overlaps are exactly the slides the line writes."""
        for kit in KITS:
            for path in mids(kit):
                for t in parsed(kit, path)[2][1:]:
                    if CH.track_lane(kit, t['name']) not in ('bass', 'vox'):
                        continue
                    notes = t['notes']
                    starts = sorted({on for on, *_ in notes})
                    for on, off, ch, key, vel in notes:
                        later = [s for s in starts if s > on]
                        if later and off > later[0]:
                            with self.subTest(kit=kit, file=path, track=t['name'], on=on):
                                self.assertNotEqual('vox', CH.track_lane(kit, t['name']))
                                self.assertTrue(all(n[3] != key for n in notes if n[0] == later[0]))
                                self.assertLessEqual(off, later[0] + Mi.SLIDE_TICKS)
        for kit in KITS:
            k = Mi.kit_of(kit)
            for name, line in k.P['bass']['lines'].items():
                f = next(n for n in kit_files(kit)
                         if re.fullmatch(rf'{kit}_Bassline_{name}(_InTheLoop)?_2bars\.mid', n))
                notes = parse(kit_files(kit)[f])[2][1]['notes']
                overlaps = {n[0] for m in notes for n in notes if m[0] < n[0] < m[1]}
                slid = {at(kit, n.step) for n in (Mi.Note(*row) for row in line) if n.slide}
                with self.subTest(kit=kit, line=name):
                    self.assertLessEqual(overlaps, slid)
        bounce = {(on, key): off for on, off, _, key, _ in parse(kit_files('FUT01')[
            'FUT01_Bassline_Bounce_InTheLoop_2bars.mid'])[2][1]['notes']}
        self.assertEqual(12 * 240, bounce[(11 * 240 + 43, 30)])
        acid = {(on, key): off for on, off, _, key, _ in parse(kit_files('FUT04')[
            'FUT04_Bassline_AcidLine1_InTheLoop_2bars.mid'])[2][1]['notes']}
        self.assertEqual(2 * 240, acid[(240 + 19, 28)])

    def test_a_slide_holds_the_note_before_into_it(self):
        """A slide marks a note glided into from the note before (FUT01 "legato from the note before", FUT02 "slide
        from the note before", FUT04 "glides into the note from the one before"): that note is held SLIDE_TICKS into
        it, so a synth set to legato glide slides there; a note before that stops more than a 16th early, the same
        pitch, or no note before plays as written."""
        k = Mi.kit_of('FUT02')
        got = k.timed([(0, 40, 2, 1.0, False), (2, 43, 1, 1.0, True)], end=64 * 240)
        self.assertEqual({40: 2 * 240 + Mi.SLIDE_TICKS, 43: 3 * 240}, {key: off for _, off, key, *_ in got})
        got = k.timed([(0, 40, 1, 1.0, False), (2, 43, 1, 1.0, True)], end=64 * 240)
        self.assertEqual(2 * 240 + Mi.SLIDE_TICKS, {key: off for _, off, key, *_ in got}[40])
        for notes in ([(0, 40, 0.5, 1.0, False), (4, 43, 1, 1.0, True)],
                      [(0, 40, 2, 1.0, False), (2, 40, 1, 1.0, True)], [(2, 43, 1, 1.0, True)]):
            got = k.timed(notes, end=64 * 240)
            self.assertEqual(sorted(round((s + ln) * 240) for s, _, ln, _, _ in notes),
                             sorted(off for _, off, *_ in got))

    def test_the_kits_slides_reach_their_notes(self):
        """In every bassline file, the note before a slid note still sounds SLIDE_TICKS into it wherever it runs up to
        it as written (FUT01, FUT02 and FUT04: 11 slides at least)."""
        joined = 0
        for kit in KITS:
            k = Mi.kit_of(kit)
            made = kit_files(kit)
            for name, line in k.P['bass']['lines'].items():
                f = next(n for n in made if re.fullmatch(rf'{kit}_Bassline_{name}(_InTheLoop)?_2bars\.mid', n))
                notes = parse(made[f])[2][1]['notes']
                rows = sorted(Mi.Note(*n) for n in line)
                for p, n in zip(rows, rows[1:]):
                    if n.slide and p.key != n.key and at(kit, p.step) + round(p.length * 240) >= at(kit, n.step) - 240:
                        with self.subTest(kit=kit, line=name, step=n.step):
                            (off,) = [o for on, o, _, key, _ in notes if on == at(kit, p.step) and key == p.key]
                            self.assertGreaterEqual(off, at(kit, n.step) + Mi.SLIDE_TICKS)
                            joined += 1
        self.assertGreaterEqual(joined, 11)


class TestLoops(unittest.TestCase):
    def test_each_loop_reproduces_its_context_loop_lane_by_lane(self):
        """The Loop files are the kit's round 5 context loop (context5.py) as its club master plays it, lane by lane:
        every onset, key and velocity, and every length but the melodic lanes' gates (checked by the timing tests):
        FUT02's line muted under its drop glide (on channel 11), FUT04's rumble on 35 muted from 16th 60, FUT03's pad
        on channel 11, FUT04's blip ratchets as retriggers, the variant's accents and FX."""
        for kit in KITS:
            for v in VARIANTS:
                got = loop(kit, v)
                want = expected_loop(kit, v)
                with self.subTest(kit=kit, variant=v):
                    self.assertEqual(sorted(want), sorted(n for n, t in got.items() if t['notes']))
                k = Mi.kit_of(kit)
                for name, rows in want.items():
                    with self.subTest(kit=kit, variant=v, track=name):
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
        for kit in KITS:
            k = Mi.kit_of(kit)
            for v in VARIANTS:
                got = loop(kit, v)
                for name, rows in expected_loop(kit, v).items():
                    for (on, key, want, _), n in zip(rows, got[name]['notes']):
                        ch, vel = n[2], n[4]
                        if ch == 3:
                            continue
                        with self.subTest(kit=kit, variant=v, track=name, on=on, key=key):
                            layers = k.layers(ch, key)
                            self.assertEqual(nearest(layers, want), nearest(layers, vel / 127))

    def test_the_vox_synths_velocity_picks_its_vowel(self):
        """On FUT02 and FUT04 the vox synth's velocity picks the vowel: every vox note sits on a layer's velocity."""
        for kit, vowels in (('FUT02', {64: 'oh', 95: 'ah', 127: 'ee'}), ('FUT04', {64: 'oo', 127: 'oh'})):
            layers = layers_of(kit, 'VoxSynth')
            self.assertEqual(sorted(vowels), [v127(x) for x in layers])
            for path in mids(kit):
                for t in parsed(kit, path)[2][1:]:
                    if t['name'] == 'Vox':
                        self.assertLessEqual({vel for *_, vel in t['notes']}, set(vowels), path)

    def test_vox_and_novox_differ_only_where_they_should(self):
        """The loops (and the extended mixes, below) differ only in the Vox and Accents tracks, the throw controllers,
        and FUT01's stab drop at 16th 30, where its no-vox warp stab plays instead."""
        for kit in KITS:
            a, b = loop(kit, 'vox'), loop(kit, 'novox')
            with self.subTest(kit=kit):
                self.assertIn('Vox', a)
                self.assertNotIn('Vox', b)
                for name in set(a) | set(b):
                    if name in ('Vox', 'Accents'):
                        continue
                    na, nb = a[name]['notes'], b[name]['notes']
                    if kit == 'FUT01' and name in ('Chords', 'Chord voicing'):
                        self.assertEqual({at(kit, 30)}, {n[0] for n in na} - {n[0] for n in nb})
                        self.assertEqual([n for n in na if n[0] != at(kit, 30)], nb)
                    else:
                        self.assertEqual(na, nb, name)
                    plain = lambda t: [c for c in t['ccs'] if c[2] not in (91, 94)]  # noqa: E731
                    self.assertEqual(plain(a[name]), plain(b[name]), name)

    def test_ccs(self):
        """CC74 and CC71 lie in 0-127, every 60 ticks on the 60-tick grid, only on the channels whose filter rides;
        CC91 and CC94 only around thrown notes: 127 on its tick before its note-on, 0 at its note-off."""
        for kit in KITS:
            for path in mids(kit):
                for t in parsed(kit, path)[2][1:]:
                    with self.subTest(kit=kit, file=path, track=t['name']):
                        rides = [c for c in t['ccs'] if c[2] in (74, 71)]
                        for tick, ch, cc, val in rides:
                            self.assertIn(ch, RIDE_CHANNELS[kit])
                            self.assertEqual(0, tick % 60)
                        for cc in (74, 71):
                            ticks_ = [c[0] for c in rides if c[2] == cc]
                            self.assertEqual(sorted(set(ticks_)), ticks_)
                        self.assertEqual(set(), {c[2] for c in t['ccs']} - {71, 74, 91, 94})
                        throws = [c for c in t['ccs'] if c[2] in (91, 94)]
                        thrown = [n for n in t['notes'] if (n[0], n[2], 91, 127) in throws]
                        want = sorted([(n[0], n[2], cc, 127) for n in thrown for cc in (91, 94)]
                                      + [(n[1], n[2], cc, 0) for n in thrown for cc in (91, 94)])
                        self.assertEqual(want, sorted(throws))
        for kit, steps in (('FUT02', (44,)), ('FUT04', (30, 46))):
            got = sorted(c[0] for c in loop(kit, 'vox')['Vox']['ccs'] if c[2] == 91 and c[3] == 127)
            self.assertEqual([at(kit, s) for s in steps], got, kit)
        got = sorted(c[0] for c in loop('FUT04', 'novox')['Accents']['ccs'] if c[2] == 91 and c[3] == 127)
        self.assertEqual([at('FUT04', 30), at('FUT04', 46)], got)
        for kit in ('FUT01', 'FUT03'):
            self.assertFalse([c for t in loop(kit, 'vox').values() for c in t['ccs'] if c[2] in (91, 94)])

    def test_the_rides_follow_the_recipes(self):
        """FUT02's arp ride closed on the downbeat and widest at 16th 46; FUT03's stab low-pass closed at the loop's
        ends and open at its middle; FUT01's wobble open in the middle of every 8th, widest in bar 4; FUT04's acid
        resonance at rest over bars 1-2, highest on the fill bar."""
        def ride(kit, track, cc):
            return {tick: val for tick, _, c, val in loop(kit, 'vox')[track]['ccs'] if c == cc}
        def value_at(r, tick):
            return r[max(t for t in r if t <= tick)]
        r = ride('FUT02', 'Lead', 74)
        self.assertEqual(0, r[0])
        self.assertEqual(127, max(r.values()))
        self.assertEqual(127, value_at(r, at('FUT02', 46)))
        self.assertLess(value_at(r, at('FUT02', 30)), 127)
        r = ride('FUT03', 'Chords', 74)
        self.assertEqual(0, r[0])
        self.assertEqual(127, value_at(r, 32 * 240))
        self.assertLess(value_at(r, 16 * 240), 127)
        r = ride('FUT01', 'Bass', 74)
        peaks = [max(v for t, v in r.items() if b * BAR <= t < (b + 1) * BAR) for b in range(4)]
        self.assertEqual(127, peaks[3])
        self.assertEqual(sorted(peaks), [peaks[0], peaks[2], peaks[1], peaks[3]])
        self.assertEqual(0, r[0])
        self.assertEqual(peaks[0], r[240])
        for cc in (74, 71):
            r = ride('FUT04', 'Bass', cc)
            self.assertEqual({0}, {v for t, v in r.items() if t < 32 * 240})
            self.assertEqual(127, max(r.values()))
            self.assertEqual(at('FUT04', 56), max(r, key=lambda t: (r[t], -t)))


class TestRideLaw(unittest.TestCase):
    def test_one_law_for_the_rides(self):
        """A cutoff ride's value is round(127 ln(f / lo) / ln(hi / lo)), held to 0-127, and stands for lo x (hi /
        lo)^(v / 127): channels.py's one law, which the mix manifests read back."""
        self.assertEqual((0, 127, 64), (CH.cc_value(650, 650, 2210), CH.cc_value(2210, 650, 2210),
                                        CH.cc_value(math.sqrt(650 * 2210), 650, 2210)))
        self.assertEqual((0, 127), (CH.cc_value(100, 650, 2210), CH.cc_value(9000, 650, 2210)))
        for v in range(128):
            self.assertEqual(v, CH.cc_value(CH.cc_hz(v, 1600, 16000), 1600, 16000))
        self.assertEqual((0, 127, 64), (CH.cc_linear(0.9, 0.9, 0.96), CH.cc_linear(0.96, 0.9, 0.96),
                                        CH.cc_linear(0.93, 0.9, 0.96)))

    def test_the_ride_ranges(self):
        """The mix manifests' maps: FUT01's wobble 650-2210 Hz, FUT02's arp 1.6-16 kHz, FUT03's dub stab 900-5000 Hz,
        FUT04's acid ride 5400-14040 Hz (its multiplier, x1-x2.6) and its resonance 0.90-0.96; no resonance ride where
        the Q is fixed."""
        want = {'FUT01': [(74, 650, 2210)], 'FUT02': [(74, 1600, 16000)], 'FUT03': [(74, 900, 5000)],
                'FUT04': [(74, 5400, 14040), (71, 900, 960)]}
        for kit in KITS:
            self.assertEqual(want[kit], [(r['cc'], r['lo'], r['hi']) for r in Mi.kit_of(kit).rides], kit)


class TestPartFiles(unittest.TestCase):
    def test_the_chords_on_channel_4_at_their_roots_and_the_voicing_on_3(self):
        """The chord sample sits on channel 4 at its musical root (no more note 60): FUT01's organ stabs on 54 and 59,
        FUT02's starfield pad on 50, FUT03's dub stab on 55 (Gm9) and 60 (Cm9), FUT04's drone on 52; the voicing alone
        on channel 3 (the v1 trigger shared it, audit A defect 1)."""
        roots = {'FUT01': {54, 59}, 'FUT02': {50}, 'FUT03': {55}, 'FUT04': {52}}
        for kit in KITS:
            made = kit_files(kit)
            (trig,) = [n for n in made if n.startswith(f'{kit}_Chords_') and not n.endswith('_Voicing_4bars.mid')]
            (voic,) = [n for n in made if n.endswith('_Voicing_4bars.mid')]
            with self.subTest(kit=kit):
                notes = parse(made[trig])[2][1]['notes']
                self.assertEqual({4}, {ch for _, _, ch, _, _ in notes})
                self.assertEqual(roots[kit], {key for _, _, _, key, _ in notes})
                self.assertEqual({3}, {ch for _, _, ch, _, _ in parse(made[voic])[2][1]['notes']})
        phrase = parse(kit_files('FUT03')['FUT03_Phrase_DubChords_4bars.mid'])[2][1]['notes']
        self.assertEqual({55, 60}, {key for *_, key, _ in [(n[0], n[1], n[2], n[3], n[4]) for n in phrase]})

    def test_phrases_on_their_instruments_channels(self):
        """Every phrase file plays on its instrument's channel (no more channel 4 for all, audit A defect 2)."""
        want = {
            'FUT01': {'OrganRiff': 12, 'WarpRiff': 1, 'PluckRiff': 2, 'EPianoStabRhythm': 4, 'VoxSynthRiff': 5},
            'FUT02': {'VoxSynthRiff': 5, 'LaserStabRiff': 12, 'GlassMelody': 13, 'VoxChoirChords': 5},
            'FUT03': {'RubberRiff': 1, 'DubChords': 4, 'KeysArp': 2, 'VoxSynthRiff': 5},
            'FUT04': {'VoxSynthRiff': 5, 'AcidStabRiff': 11, 'MetalChords': 12, 'RaveStabRiff': 13},
        }
        for kit in KITS:
            made = kit_files(kit)
            got = {}
            for n, d in made.items():
                m = re.fullmatch(rf'{kit}_Phrase_([A-Za-z0-9]+)_\d+bars\.mid', n)
                if m:
                    (chans,) = [{ch for _, _, ch, _, _ in t['notes']} for t in parse(d)[2][1:]]
                    got[m[1]] = chans.pop()
            self.assertEqual(want[kit], got, kit)

    def test_the_low_line_plays_the_organ_bass_on_channel_11(self):
        notes = parse(kit_files('FUT01')['FUT01_Bassline_Low_2bars.mid'])[2][1]['notes']
        self.assertEqual({11}, {ch for _, _, ch, _, _ in notes})
        for name in ('FUT01_Bassline_Bounce_InTheLoop_2bars.mid', 'FUT01_Bassline_Speed_2bars.mid'):
            self.assertEqual({1}, {ch for _, _, ch, _, _ in parse(kit_files('FUT01')[name])[2][1]['notes']})

    def test_the_accent_and_fx_files_are_the_loops(self):
        for kit in KITS:
            made = kit_files(kit)
            for v in VARIANTS:
                (tr,) = parse(made[f'{kit}_Accents_{NAME[v]}_4bars.mid'])[2][1:]
                self.assertEqual(loop(kit, v)['Accents']['notes'], tr['notes'], (kit, v))
            if f'{kit}_FX_4bars.mid' in made:
                (tr,) = parse(made[f'{kit}_FX_4bars.mid'])[2][1:]
                self.assertEqual(loop(kit, 'vox')['FX']['notes'], tr['notes'], kit)

    def test_end_aligned_notes_end_on_their_lines(self):
        """Reverse swells, sweeps, uplifters, risers and fills (cut_end) end on a beat line in every file, the sample's
        length before it (the length a test below checks against the WAVs)."""
        for kit in KITS:
            k = Mi.kit_of(kit)
            for path in mids(kit):
                for t in parsed(kit, path)[2][1:]:
                    for on, off, ch, key, _ in t['notes']:
                        if ch in (6, 7) and k.end_aligned(ch, key):
                            with self.subTest(kit=kit, file=path, key=key, on=on):
                                self.assertEqual(0, off % 960)

    def test_the_sample_lengths_match_the_wavs(self):
        """PARTS' one-shot lengths (frames at 48 kHz) are the WAVs', where the samples are pulled (LFS pointers skip;
        WUBANSIA_SAMPLES may point at a folder of pulled kits)."""
        import soundfile as sf
        root = Path(os.environ.get('WUBANSIA_SAMPLES', REPO / 'content' / 'samples'))
        checked = 0
        for kit in KITS:
            k = Mi.kit_of(kit)
            for item, frames in k.P['frames'].items():
                path = root / kit.lower() / k.wav(item)
                if not path.exists() or path.read_bytes()[:7] == b'version':
                    continue
                with self.subTest(kit=kit, item=item):
                    self.assertEqual(frames, sf.info(str(path)).frames)
                    checked += 1
        if not checked:
            self.skipTest('no pulled samples (LFS pointers)')


class TestExtendedMix(unittest.TestCase):
    """The extended mixes: the kit's written parts arranged on 8-bar lines in its genre's form, with the vox synth or
    without, as one file and one region per section, listed in regions.json."""

    def test_the_folders(self):
        for kit in KITS:
            for v in VARIANTS:
                made, R = ext(kit, v)
                with self.subTest(kit=kit, variant=v):
                    self.assertEqual(f'{kit}_ExtendedMix_{NAME[v]}_{R["bars"]}bars.mid', R['file'])
                    self.assertEqual({R['file'], 'regions.json', 'README.md'} | {s['file'] for s in R['sections']},
                                     set(made))
                    for i, s in enumerate(R['sections'], 1):
                        self.assertRegex(s['file'], rf'^{i:02d}_[A-Z][A-Za-z0-9]*_{s["bars"]}bars\.mid$')

    def test_regions_json_is_integers_and_strings_only(self):
        """core's strict reader takes integers (no fraction, no exponent, no -0, no leading zeros), strings, booleans,
        null, arrays and objects with no key twice."""
        def no_float(text):
            raise AssertionError(f'a float in regions.json: {text}')

        def pairs(kv):
            keys = [k for k, _ in kv]
            assert len(keys) == len(set(keys)), keys
            return dict(kv)
        for kit in KITS:
            for v in VARIANTS:
                text = ext(kit, v)[0]['regions.json'].decode()
                R = json.loads(text, parse_float=no_float, parse_constant=no_float, object_pairs_hook=pairs)
                self.assertIsNone(re.search(r'(?<![\w.])-0(?![\d])|(?<![\w.])0\d', re.sub(r'"[^"]*"', '', text)))
                K = kits.load(kit)
                with self.subTest(kit=kit, variant=v):
                    self.assertEqual((int(K.BPM), round(K.SWING * 1000), 960, v), (R['bpm'], R['swing_pm'], R['ppq'],
                                                                                R['variant']))
                    self.assertEqual(round(R['bars'] * 240_000 / K.BPM), R['length_ms'])
                    self.assertEqual(1, R['sections'][0]['start_bar'])

    def test_it_runs_6_to_8_minutes(self):
        for kit in KITS:
            K = kits.load(kit)
            for v in VARIANTS:
                R, tracks, conductor = whole(kit, v)
                self.assertEqual(R['bars'] * BAR, conductor['end'])
                self.assertTrue(360 <= R['bars'] * 240 / K.BPM <= 480, kit)
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
        """Each genre's form: 32-bar intro and outro, every section and line on an 8-bar line, the sections tiling
        the mix; both variants share it."""
        self.assertEqual(set(self.FORM_KINDS), set(Mi.FORMS))
        for kit in KITS:
            K = kits.load(kit)
            for v in VARIANTS:
                R = ext(kit, v)[1]
                with self.subTest(kit=kit, variant=v):
                    self.assertEqual(self.FORM_KINDS.get(K.DIRECTION, self.FORM_KINDS['default']),
                                     [s['kind'] for s in R['sections']])
                    self.assertEqual(32, R['sections'][0]['bars'])
                    self.assertEqual(32, R['sections'][-1]['bars'])
                    start = 1
                    for s in R['sections']:
                        self.assertEqual(start, s['start_bar'])
                        self.assertEqual([s['start_bar'] + 8 * i for i in range(s['bars'] // 8)],
                                         [ln['start_bar'] for ln in s['lines']])
                        start += s['bars']
                    self.assertEqual(R['bars'] + 1, start)
            self.assertEqual([s['name'] for s in ext(kit, 'vox')[1]['sections']],
                             [s['name'] for s in ext(kit, 'novox')[1]['sections']])

    def test_the_regions_tile_the_whole_mix(self):
        """Every region has the whole mix's tracks in the same order and ends on its last bar line; its notes,
        controllers, marker and cues, shifted to its start bar, are the whole mix's."""
        for kit in KITS:
            for v in VARIANTS:
                made, R = ext(kit, v)
                _, _, wtracks = parse(made[R['file']])
                names = [t['name'] for t in wtracks[1:]]
                got = {n: [] for n in names}
                gcc = {n: [] for n in names}
                marks = []
                for s in R['sections']:
                    _, _, reg = parse(made[s['file']])
                    off = (s['start_bar'] - 1) * BAR
                    with self.subTest(kit=kit, variant=v, region=s['file']):
                        self.assertEqual(names, [t['name'] for t in reg[1:]])
                        self.assertEqual(wtracks[0]['tempo'], reg[0]['tempo'])
                        for t in reg:
                            self.assertEqual(s['bars'] * BAR, t['end'])
                    for t in reg[1:]:
                        got[t['name']] += [(a + off, b + off, c, d, e) for a, b, c, d, e in t['notes']]
                        gcc[t['name']] += [(a + off, b, c, d) for a, b, c, d in t['ccs']]
                    marks += [(tick + off, k, b) for tick, k, b in reg[0]['meta'] if k in (MARKER, CUE)]
                for t in wtracks[1:]:
                    self.assertEqual(t['notes'], sorted(got[t['name']]), (kit, v, t['name']))
                    self.assertEqual(sorted(t['ccs']), sorted(gcc[t['name']]), (kit, v, t['name']))
                self.assertEqual([m for m in wtracks[0]['meta'] if m[1] in (MARKER, CUE)], marks)

    def test_a_marker_names_every_section_at_its_start(self):
        for kit in KITS:
            R, _, conductor = whole(kit, 'vox')
            want = [((s['start_bar'] - 1) * BAR, s['name']) for s in R['sections']]
            self.assertEqual(want, [(t, b.decode()) for t, k, b in conductor['meta'] if k == MARKER], kit)

    def test_32_bars_of_drums_at_each_end(self):
        """The first and the last 32 bars are drums only (channel 10, FUT04's rumble among them): no bass, harmony,
        vox, accents or FX sounding there; the kick in every bar of the first and last 8, for a DJ to beatmatch on (UK
        garage's 2-step also the hats and the perc, which carry its pulse)."""
        for kit in KITS:
            garage = kits.load(kit).DIRECTION == 'future garage / 2-step'
            for v in VARIANTS:
                R, tracks, conductor = whole(kit, v)
                end = conductor['end']
                for name, t in tracks.items():
                    lane = CH.track_lane(kit, name)
                    with self.subTest(kit=kit, variant=v, track=name):
                        if CH.track_channel(kit, name) == 10:
                            continue
                        self.assertFalse([n for n in t['notes'] if n[0] < 32 * BAR or n[1] > end - 32 * BAR])
                kick = lane_tracks(kit, {k: t for k, t in tracks.items() if k == 'Kick'}, 'kick')[0]
                for lo in (0, end - 8 * BAR):
                    self.assertEqual(set(range(8)), {(on - lo) // BAR for on, *_ in ons(kick, lo, lo + 8 * BAR)}, kit)
                    if garage:
                        for lane in ('hats', 'perc'):
                            got = {(on - lo) // BAR for on, *_ in lane_ons(kit, tracks, lane, lo, lo + 8 * BAR)}
                            self.assertEqual(set(range(8)), got, lane)

    def test_the_intro_brings_the_drums_in_on_8_bar_lines_and_the_bass_at_33(self):
        for kit in KITS:
            R, tracks, _ = whole(kit, 'vox')
            first = {}
            for name, t in tracks.items():
                if t['notes']:
                    lane = CH.track_lane(kit, name)
                    first[lane] = min(first.get(lane, 10 ** 9), t['notes'][0][0] // BAR)
            with self.subTest(kit=kit):
                self.assertEqual(0, first['kick'])
                for lane in ('snare', 'hats', 'perc'):
                    self.assertLess(first[lane], 32)
                    self.assertEqual(0, first[lane] % 8, lane)
                self.assertEqual(32, first['bass'])
                self.assertLessEqual(first['bass'], first['chords'])
                self.assertEqual(0, first['chords'] % 8)

    def test_the_breaks_have_no_kick_and_the_drops_every_lane(self):
        for kit in KITS:
            for v in VARIANTS:
                made, R = ext(kit, v)
                for s in R['sections']:
                    tracks = {t['name']: t for t in parse(made[s['file']])[2][1:]}
                    with self.subTest(kit=kit, variant=v, section=s['name']):
                        if s['kind'] == 'break':
                            self.assertEqual([], lane_ons(kit, {'Kick': tracks['Kick']}, 'kick'))
                            self.assertIn('chords', s['lanes'])
                        if s['kind'] in ('drop', 'drop2'):
                            for lane in ('kick', 'snare', 'hats', 'bass', 'chords', 'lead', 'accents'):
                                self.assertIn(lane, s['lanes'])
                            self.assertEqual(v == 'vox', 'vox' in s['lanes'])

    def test_the_drop_is_not_the_groove(self):
        """Audit B, headline 1: the drop's first line differs from the groove's last line beyond the drums: the lead,
        the vox or the accents change; drop 2 plays a bassline other than the switch-up's (cross-kit issue 4)."""
        for kit in KITS:
            R, tracks, _ = whole(kit, 'vox')
            groove = next(s for s in R['sections'] if s['kind'] == 'groove')
            drop = next(s for s in R['sections'] if s['kind'] == 'drop')
            g0 = (groove['lines'][-1]['start_bar'] - 1) * BAR
            d0 = (drop['start_bar'] - 1) * BAR
            changed = []
            for lane in ('lead', 'vox', 'accents'):
                a = [(on - g0, key) for on, key, _ in lane_ons(kit, tracks, lane, g0, g0 + 8 * BAR)]
                b = [(on - d0, key) for on, key, _ in lane_ons(kit, tracks, lane, d0, d0 + 8 * BAR)]
                if a != b:
                    changed.append(lane)
            self.assertTrue(changed, kit)
            secs = {s['kind']: s for s in R['sections']}
            if 'switch' in secs:
                self.assertNotEqual({ln.get('bass') for ln in secs['switch']['lines'] if ln.get('bass')},
                                    {ln.get('bass') for ln in secs['drop2']['lines'] if ln.get('bass')}, kit)

    def test_the_cues_have_their_notes(self):
        """Every Vox cue has vox notes in its line in the vox file and none in the no-vox file; every Riser cue an
        FX note ending on the next section's first beat; every Fill cue an FX note ending on the next downbeat."""
        for kit in KITS:
            for v in VARIANTS:
                R, tracks, conductor = whole(kit, v)
                starts = {(s['start_bar'] - 1) * BAR for s in R['sections']}
                fx = tracks.get('FX', {'notes': []})['notes']
                cues = [(t, b.decode()) for t, k, b in conductor['meta'] if k == CUE]
                self.assertEqual([((c['bar'] - 1) * BAR, c['cue']) for s in R['sections'] for c in s['cues']], cues)
                self.assertTrue(any(c == 'Riser' for _, c in cues))
                for t, c in cues:
                    with self.subTest(kit=kit, variant=v, cue=c, bar=t // BAR + 1):
                        self.assertEqual(0, t % BAR)
                        if c == 'Vox':
                            got = [n for n in tracks.get('Vox', {'notes': []})['notes'] if t <= n[0] < t + 8 * BAR]
                            self.assertEqual(v == 'vox', bool(got))
                        if c == 'Riser':
                            self.assertIn(t + 8 * BAR, starts)
                            self.assertTrue([n for n in fx if n[1] == t + 8 * BAR and n[1] - n[0] >= 4 * BAR])
                        if c == 'Fill':
                            self.assertTrue([n for n in fx if n[1] == t + BAR and n[0] >= t])

    def test_the_builds_roll_up_to_the_fill(self):
        """Each genre's build: its roll's last hit falls before its fill starts, inside the fill's bar or the bar
        before, its velocities rise hit by hit; the kick sits the roll out in sci-fi club and plays on in house,
        techno and UK garage."""
        for kit in KITS:
            K = kits.load(kit)
            R, tracks, _ = whole(kit, 'vox')
            build = next(s for s in R['sections'] if s['kind'] == 'build')
            ln = build['lines'][-1]
            lo = (ln['start_bar'] - 1) * BAR
            fills = [n for n in tracks['FX']['notes'] if n[1] == lo + 8 * BAR and n[0] >= lo + 7 * BAR]
            roll = ln['roll']
            hits = lane_ons(kit, tracks, roll, lo, lo + 8 * BAR)
            rolled = [h for h in hits if h[0] >= lo + (8 - ln['roll_bars']) * BAR]
            with self.subTest(kit=kit):
                self.assertTrue(fills)
                fill_on = min(n[0] for n in fills)
                self.assertLess(max(h[0] for h in rolled), fill_on)
                self.assertGreaterEqual(max(h[0] for h in rolled), fill_on - BAR)
                vels = [vel for _, _, vel in rolled]
                self.assertEqual(sorted(vels), vels)
                self.assertGreater(vels[-1], vels[0])
                kick = lane_ons(kit, tracks, 'kick', lo, lo + 7 * BAR)
                self.assertEqual(K.DIRECTION == 'sci-fi club', not kick)

    def test_busy_hats_are_busier(self):
        """A busy line plays the kit's own busy pattern, or a hat on every 16th its full hats leave empty, at a
        velocity of 0.35 or more (audit B: FUT01's v1 'busy' added nothing, the others one ghost a bar at 0.2)."""
        for kit in KITS:
            R, tracks, _ = whole(kit, 'vox')
            k = Mi.kit_of(kit)
            busy = [ln for s in R['sections'] for ln in s['lines'] if ln.get('hats') == 'busy']
            self.assertTrue(busy, kit)
            authored = k.P['drums'].get('busy')
            for ln in busy:
                lo = (ln['start_bar'] - 1) * BAR
                for b in range(7):
                    hats = lane_ons(kit, tracks, 'hats', lo + b * BAR, lo + (b + 1) * BAR)
                    with self.subTest(kit=kit, bar=ln['start_bar'] + b):
                        steps = {round((on - lo - b * BAR - (LATE[kit] if on % 480 else 0)) / 240) for on, _, _ in hats}
                        if authored:
                            self.assertLessEqual({round(n[0]) % 16 for n in authored}, steps)
                        else:
                            self.assertEqual(set(range(16)), steps)
                            full = {round(n.step) % 16 for n in k.drums('hats', 'groove') if n.step // 16 == b % 4}
                            added = [vel for on, _, vel in hats if round((on - lo - b * BAR
                                                                          - (LATE[kit] if on % 480 else 0)) / 240)
                                     not in full]
                            self.assertGreaterEqual(min(added, default=127), 44)

    def test_the_turnarounds_change_the_lines_last_bar(self):
        for kit in KITS:
            R, tracks, _ = whole(kit, 'vox')
            turns = [ln for s in R['sections'] for ln in s['lines'] if ln.get('end') == 'turn']
            self.assertTrue(turns, kit)
            for ln in turns:
                lo = (ln['start_bar'] - 1) * BAR
                drums = lambda a, b: sorted((on - a, key) for lane in CH.DRUM_LANES  # noqa: E731
                                            for on, key, _ in lane_ons(kit, tracks, lane, a, b))
                self.assertNotEqual(drums(lo + 5 * BAR, lo + 6 * BAR), drums(lo + 7 * BAR, lo + 8 * BAR),
                                    (kit, ln['start_bar']))

    def test_the_mixes_differ_only_where_they_should(self):
        for kit in KITS:
            Ra, a, _ = whole(kit, 'vox')
            Rb, b, _ = whole(kit, 'novox')
            with self.subTest(kit=kit):
                self.assertEqual(set(a) - {'Vox'}, set(b))
                for name in b:
                    if name == 'Accents':
                        continue
                    if kit == 'FUT01' and name in ('Chords', 'Chord voicing'):
                        extra = [n for n in a[name]['notes'] if n not in b[name]['notes']]
                        self.assertEqual({at(kit, 30) % (4 * BAR)}, {n[0] % (4 * BAR) for n in extra})
                        self.assertFalse([n for n in b[name]['notes'] if n not in a[name]['notes']])
                        continue
                    self.assertEqual(a[name]['notes'], b[name]['notes'], name)

    def test_regions_json_lists_the_lanes_and_the_tracks(self):
        for kit in KITS:
            made, R = ext(kit, 'vox')
            _, tracks, _ = whole(kit, 'vox')
            self.assertEqual([t['track'] for t in R['tracks']], list(tracks))
            for t in R['tracks']:
                self.assertEqual((CH.track_lane(kit, t['track']), CH.track_channel(kit, t['track'])),
                                 (t['lane'], t['channel']))
            for s in R['sections']:
                reg = {t['name']: t for t in parse(made[s['file']])[2][1:]}
                for ln in s['lines']:
                    lo = (ln['start_bar'] - s['start_bar']) * BAR
                    playing = [lane for lane in CH.LANES if lane_ons(kit, reg, lane, lo, lo + 8 * BAR)]
                    self.assertEqual(playing, ln['lanes'], (kit, ln['start_bar']))

    def test_only_the_kits_material_plays(self):
        """Every note is one of its track's written keys: no note is made up."""
        for kit in KITS:
            k = Mi.kit_of(kit)
            allowed = k.allowed_keys()
            for v in VARIANTS:
                _, tracks, _ = whole(kit, v)
                for name, t in tracks.items():
                    self.assertLessEqual({key for *_, key, _ in t['notes']}, allowed[name], (kit, v, name))


class TestSchema(unittest.TestCase):
    """A kit's PARTS (tools/sound/README.md, 'MIDI'): the required fields alone export a complete kit."""

    @staticmethod
    def kit():
        K = types.SimpleNamespace(KIT='FUT99', TITLE='Test Kit', BPM=132.0, KEY='A minor', CAMELOT='8A', SWING=0.55,
                                  RECIPES='tools.sound.recipes.fut99', DIRECTION='sci-fi club')
        one = lambda name, group, **kw: Item(group, name, 'stab', print, **kw)  # noqa: E731
        items = [one('Kick', 'Drums', layers=(0.5, 1.0)), one('Clap', 'Drums', layers=(0.5, 1.0)),
                 one('Hat', 'Drums', layers=(0.5, 1.0)), one('Bass', 'Bass', notes=(33, 45)),
                 one('Lead', 'Lead', notes=(69,)), one('Pad', 'Pad', root=57), one('Riser', 'FX', flags={'cut_end'}),
                 one('Fill', 'FX', flags={'cut_end'}), one('Hit', 'Stabs', layers=(0.6, 1.0), root=69),
                 one('Vox', 'Vox', notes=(69, 76))]
        bar = [(0, 36, 0.5, 1.0, False), (4, 38, 0.5, 1.0, False), (8, 36, 0.5, 1.0, False), (12, 38, 0.5, 1.0, False)]
        bar += [(s, 42, 0.5, 0.5, False) for s in (2, 6, 10, 14)]
        drums = [(s + 16 * b, *rest) for b in range(4) for s, *rest in bar]
        P = dict(
            drums=dict(map={36: 'Kick', 38: 'Clap', 42: 'Hat'}),
            bass=dict(items=('Bass',), lines={'One': [(2, 33, 1.0, 1.0, False)], 'Two': [(6, 45, 1.0, 1.0, False)]},
                      pick='One'),
            lead=dict(item='Lead', notes=[(0, 69, 2, 1.0, False), (16, 72, 2, 1.0, False)]),
            chords=dict(items=('Pad',), trigger=[(0, 57, 64, 1.0, False)],
                        voicing=[(0, n, 64, 1.0, False) for n in (57, 60, 64)]),
            vox=dict(item='Vox', riff=[(2, 69, 1, 1.0, False), (18, 76, 2, 1.0, False)]),
            accents=(Mi.Key('Hit'),), fx=(Mi.Key('Riser'), Mi.Key('Fill')),
            frames={'Hit': 24000, 'Riser': 8 * 4 * 48000 * 60 // 132, 'Fill': 48000 * 60 // 132 * 4},
            cues=dict(riser='Riser', fill='Fill'),
            loop=dict(drums=drums, accents={'vox': [Mi.Hit(14, 'Hit', 2)], 'novox': [Mi.Hit(30, 'Hit', 2)]},
                      vox=[(s + 16, *rest) for s, *rest in [(2, 69, 1, 1.0, False), (18, 76, 2, 1.0, False)]]),
            phrases=[],
        )
        return Mi.Kit('FUT99', K, P, items={it.name: it for it in items})

    def test_the_required_fields_export_every_file(self):
        k = self.kit()
        made = Mi.files_of(k)
        for name in ('FUT99_Loop_Vox_4bars.mid', 'FUT99_Loop_NoVox_4bars.mid', 'FUT99_Drums_4bars.mid',
                     'FUT99_Bassline_One_InTheLoop_2bars.mid', 'FUT99_Bassline_Two_2bars.mid',
                     'FUT99_Lead_Lead_2bars.mid', 'FUT99_Chords_Pad_4bars.mid', 'FUT99_Chords_Pad_Voicing_4bars.mid',
                     'FUT99_Accents_Vox_4bars.mid', 'FUT99_Accents_NoVox_4bars.mid', 'README.md'):
            self.assertIn(name, made)
        for v in VARIANTS:
            out = Mi.extended_of(k, v)
            R = json.loads(out['regions.json'])
            self.assertEqual(224, R['bars'])
            self.assertIn(R['file'], out)
        loop_ = {t['name']: t for t in parse(made['FUT99_Loop_Vox_4bars.mid'])[2][1:]}
        self.assertEqual([(16 * 240 + 2 * 240, 69, 127), (34 * 240, 76, 127)], ons(loop_['Vox']))
        self.assertEqual([(2 * 240, 33, 127), (34 * 240, 33, 127)], ons(loop_['Bass']))
        self.assertEqual({1}, {ch for _, _, ch, _, _ in loop_['Bass']['notes']})

    def test_a_velocity_picks_the_recipes_layer(self):
        """round(v x 127), unless it would pick another layer than v: 0.751 is nearer the 1.0 layer of (0.5, 1.0) but
        round(0.751 x 127) = 95 is nearer 0.5, so it is written 96; FUT01's 0.9 kicks, halfway between 0.8 and 1.0
        (the recipe plays the louder), 115."""
        k = self.kit()
        self.assertEqual(96, k.velocity(10, 36, 0.751))
        self.assertEqual(64, k.velocity(10, 36, 0.5))
        self.assertEqual(127, k.velocity(10, 36, 1.0))
        self.assertEqual(115, Mi.written_velocity((0.2, 0.4, 0.6, 0.8, 1.0), 0.9))
        self.assertEqual(89, Mi.written_velocity(None, 0.7))

    def test_a_note_off_the_map_is_refused(self):
        k = self.kit()
        P = {**k.P, 'loop': {**k.P['loop'], 'drums': k.P['loop']['drums'] + [(1, 44, 0.5, 1.0, False)]}}
        with self.assertRaises(ValueError):
            Mi.files_of(Mi.Kit('FUT99', k.K, P, items=k.items))

    def test_the_fill_bar_and_the_groove_block(self):
        """A lane's fill block plays as the loop has it where a fill is due; elsewhere its groove block, by default
        bar 2 again in place of bar 4."""
        n = lambda s, key: Mi.Note(s, key, 0.5, 1.0)  # noqa: E731
        pattern = [n(0, 36), n(4, 38), n(16, 36), n(23, 36), n(32, 36), n(40, 42), n(48, 36)]
        self.assertEqual([n(0, 36), n(4, 38), n(16, 36), n(23, 36), n(32, 36), n(40, 42), n(48, 36), n(55, 36)],
                         Mi.swapped(pattern))
        self.assertEqual([n(0, 36), n(4, 38), n(16, 36), n(23, 36), n(32, 36), n(36, 38), n(48, 36)],
                         Mi.swapped(pattern, fill_bar=3, swap=1))


if __name__ == '__main__':
    unittest.main()
