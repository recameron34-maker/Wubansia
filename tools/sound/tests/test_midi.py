"""MIDI v2 (tools/sound/midi.py; ADR 0019, docs/midi-map.md): every kit's written music as Standard MIDI Files on the
one channel map (tools/sound/channels.py), the files Wubansia plays and drags out. The kit-agnostic properties; each
kit's own expectations (its context loop lane by lane, its roots, phrase channels, rides and throws) are in
test_fut0N_midi.py, so each kit's engineer edits only their own.

What these tests pin (the design's midi_plan section 9, and the guarantees the v1 export already kept):
- committed files equal a fresh export, the same bytes in fresh processes;
- every note's channel is its instrument's, its key in its instrument's zones, keymap or drum map (35-59); one role a
  channel a kit; one lane a track; channel 3 holds only the voicing;
- valid format 1 files at 960 PPQ, the kit's tempo, 4/4 and key (FF59); on a tick note-offs, then controllers, then
  note-ons; no same-key overlaps; no note past its track's end; tracks end on bar lines; swing written in; gates that
  never run a swung note into the next one unless it slides; slides that reach their note;
- the vox and no-vox files differ only where they should; end-aligned notes end on beat lines; CC74/71 rides only on
  the channels whose filter rides, on the shared law, CC91/94 only around thrown notes;
- the extended mixes: regions that tile them, an integer-only regions.json, 32-bar drums-only ends, cues with their
  notes, rolls that rise up to the fill, busier hats that are busier, turnarounds, drops that aren't grooves; and the
  genres' default forms' own rules, on every kit with its own form taken out (genre_kit()), so they hold whatever form
  a kit plays.
"""
import hashlib
import json
import math
import os
import re
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
from tools.sound.tests.midi_testlib import (BAR, CUE, KEYSIG, KITS, MARKER, METER, NAME, REPO, TEMPO, VARIANTS, at,
                                            bars, ext, form_ext, form_whole, forms, genre_kit, int_layer, kit_files,
                                            lane_ons, lane_tracks, late, loop, mids, nearest, ons, parse, parsed, whole)

MINOR = ('A', 'E', 'B', 'F#', 'C#', 'G#', 'D#', 'A#')      # the minor keys with 0-7 sharps
MINOR_FLATS = ('A', 'D', 'G', 'C', 'F', 'Bb', 'Eb', 'Ab')  # ... and with 0-7 flats


def signature(key):
    """A minor key's signature, read off the circle of fifths: (sharps, or minus the flats; 1 for minor)."""
    root = key.split()[0]
    return (MINOR.index(root) if root in MINOR else -MINOR_FLATS.index(root)), 1


DRUMS_RUMBLE_BASS = Mi.DRUMS + ' rumble bass'


def slid_files(kit):
    """The kit's files that write slides, as (file name, its notes, the rows written): every bassline file, and the
    phrase file of every phrase with a slide."""
    k = Mi.kit_of(kit)
    made = kit_files(kit)
    for name, line in k.P['bass']['lines'].items():
        f = next(n for n in made if re.fullmatch(rf'{kit}_Bassline_{name}(_InTheLoop)?_2bars\.mid', n))
        yield f, parse(made[f])[2][1]['notes'], Mi.as_notes(line)
    for ph in k.phrases:
        rows = Mi.as_notes(ph['notes'])
        if any(n.slide for n in rows):
            f = Mi.phrase_file(kit, ph)
            yield f, parse(made[f])[2][1]['notes'], rows


def fill_notes(kit, tracks, ln):
    """In a whole mix's tracks, the notes of the fill a line (regions.json's) ends on, ending on the line's end and
    starting in its last bar: its own (fill=<sound>, on that sound's channel) or the kit's (FX)."""
    lo = (ln['start_bar'] - 1) * BAR
    if ln.get('fill'):
        role, key = Mi.kit_of(kit).sounds[ln['fill']]
        pool = [n for n in tracks.get(Mi.track_of_role(role), {'notes': []})['notes'] if n[3] == key]
    else:
        pool = tracks.get('FX', {'notes': []})['notes']
    return [n for n in pool if n[1] == lo + 8 * BAR and n[0] >= lo + 7 * BAR]


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
        self.assertEqual({'FUT01': ['Organ Bass', 'Organ Lead', 'Air Pad'],
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

    def test_every_velocity_plays_one_layer_under_both_rules(self):
        """Every layered note in every kit's files (both loops, the parts, the phrases and both extended mixes) sits on
        a velocity where the engine's integer rule (the layer whose round(layer x 127) is nearest, the louder on a tie)
        and the float rule on velocity / 127 play the same layer: no velocity on the boundary between two layers (P5
        found FUT01's and FUT03's 0.3 ghost claps written 38, which the integer rule plays on the 0.4 layer)."""
        for kit in KITS:
            k = Mi.kit_of(kit)
            for path in mids(kit):
                for t in parsed(kit, path)[2][1:]:
                    for on, _, ch, key, vel in t['notes']:
                        layers = k.layers(ch, key)
                        if layers and len(layers) > 1:
                            with self.subTest(kit=kit, file=path, track=t['name'], on=on, key=key):
                                self.assertEqual(nearest(layers, vel / 127), int_layer(layers, vel))

    def test_any_recipe_velocity_is_written_on_its_layer(self):
        """For every layered instrument of every kit and every recipe velocity (0.001 apart), the written velocity
        plays the recipe's layer under both rules, within 1 of round(v x 127) where it moves: the extended mixes'
        rolls, rises and ghosts compute their own velocities."""
        sets = {it.layers for kit in KITS for it in Mi.kit_of(kit).items.values() if len(it.layers) > 1}
        self.assertGreaterEqual(len(sets), 8)
        for layers in sorted(sets):
            for i in range(1, 1001):
                v = i / 1000
                c = Mi.written_velocity(layers, v)
                with self.subTest(layers=layers, v=v):
                    self.assertEqual(nearest(layers, v), int_layer(layers, c))
                    self.assertEqual(nearest(layers, v), nearest(layers, c / 127))
                    self.assertLessEqual(abs(c - max(1, min(127, round(v * 127)))), 1)

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
            self.assertEqual(sorted(VARIANTS), sorted(p.name for p in (folder / Mi.EXTENDED).iterdir()), kit)
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
                vowels = k.P.get('vox', {}).get('vowels')
                if vowels:                      # audit A #6: the vowel each layer picks, by its velocity
                    row = next(r for r in text.splitlines() if r.startswith('| 5 | Vox |'))
                    layers = k.item(k.items_on(5)[0]).layers
                    self.assertEqual(len(layers), len(vowels))
                    for v, vowel in zip(layers, vowels):
                        self.assertIn(f"{math.floor(v * 127 + 0.5)} '{vowel}'", row)
                drums = next(r for r in text.splitlines() if r.startswith(f'| `{kit}_Drums_4bars.mid` |'))
                block = k.drums('perc', 'loop') + k.drums('kick', 'loop') + k.drums('snare', 'loop') + \
                    k.drums('hats', 'loop')
                bar = lambda b: sorted((n.step - 16 * b, n.key)  # noqa: E731
                                       for n in block if b * 16 <= n.step < (b + 1) * 16)
                has_fill = bool(k.loop_fx())
                self.assertEqual(has_fill, bar(3) != bar(1))       # a fill bar: bar 4 isn't bar 2 again
                self.assertEqual(has_fill, 'its fill bar included' in drums, drums)
                self.assertEqual(not has_fill, 'no fill bar' in drums, drums)
                for v in VARIANTS:
                    made, R = ext(kit, v)
                    etext = made['README.md'].decode()
                    for name in [R['file'], 'regions.json'] + [s['file'] for s in R['sections']]:
                        self.assertIn(f'`{name}`', etext)
                    for words in ('C1 = 36', 'C3 = 60', 'C4 = 60', 'MIDI number'):
                        self.assertIn(words, etext)
                    if v == 'novox':            # no vox entries in a mix with no vox
                        self.assertNotIn('Vox cue', etext)
                        self.assertNotIn('vox entr', etext)
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
            sf, mi = signature(K.KEY)
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
            self.assertEqual(late(kit), Mi.kit_of(kit).late)
            for v in VARIANTS:
                for t in lane_tracks(kit, loop(kit, v), 'hats'):
                    offsets = {on % 480 for on, *_ in t['notes']}
                    self.assertLessEqual(offsets, {0, 240 + late(kit)}, (kit, t['name']))

    def test_gates_never_run_a_swung_note_into_the_next_one(self):
        """On a bass or vox track (a mono line) a note ends by the next note's start, unless that note is a written
        slide, which it runs into by SLIDE_TICKS at most (audit A, defect 5; the kits' files check the audit's
        examples). In each bassline file the overlaps are exactly the slides the line writes."""
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
            for name, notes, rows in slid_files(kit):
                overlaps = {n[0] for m in notes for n in notes if m[0] < n[0] < m[1]}
                slid = {at(kit, n.step) for n in rows if n.slide}
                with self.subTest(kit=kit, file=name):
                    self.assertLessEqual(overlaps, slid)

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
        """In every bassline file and every phrase file that slides (FUT01's warp riff, FUT03's rubber riff), the note
        before a slid note still sounds SLIDE_TICKS into it wherever it runs up to it as written (FUT01, FUT02 and
        FUT04's basslines: 11 slides at least; the phrases: 10 at least)."""
        joined = {'line': 0, 'phrase': 0}
        for kit in KITS:
            for name, notes, rows in slid_files(kit):
                rows = sorted(rows)
                for p, n in zip(rows, rows[1:]):
                    if n.slide and p.key != n.key and at(kit, p.step) + round(p.length * 240) >= at(kit, n.step) - 240:
                        with self.subTest(kit=kit, file=name, step=n.step):
                            (off,) = [o for on, o, _, key, _ in notes if on == at(kit, p.step) and key == p.key]
                            self.assertGreaterEqual(off, at(kit, n.step) + Mi.SLIDE_TICKS)
                            joined['phrase' if '_Phrase_' in name else 'line'] += 1
        self.assertGreaterEqual(joined['line'], 11)
        self.assertGreaterEqual(joined['phrase'], 10)


class TestLoops(unittest.TestCase):
    def test_vox_and_novox_differ_only_where_they_should(self):
        """The loops (and the extended mixes, below) differ only in the Vox and Accents tracks, the throw controllers,
        and the chord stabs the no-vox accents take over (PARTS['loop']['stab_drop']: FUT01's at 16th 30)."""
        for kit in KITS:
            a, b = loop(kit, 'vox'), loop(kit, 'novox')
            drop = {at(kit, st) for st in Mi.kit_of(kit).P['loop'].get('stab_drop', {}).get('novox', ())}
            with self.subTest(kit=kit):
                self.assertIn('Vox', a)
                self.assertNotIn('Vox', b)
                for name in set(a) | set(b):
                    if name in ('Vox', 'Accents'):
                        continue
                    na, nb = a[name]['notes'], b[name]['notes']
                    if name in ('Chords', 'Chord voicing'):
                        self.assertEqual([n for n in na if n[0] not in drop], nb, name)
                    else:
                        self.assertEqual(na, nb, name)
                    plain = lambda t: [c for c in t['ccs'] if c[2] not in (91, 94)]  # noqa: E731
                    self.assertEqual(plain(a[name]), plain(b[name]), name)

    def test_ccs(self):
        """CC74 and CC71 lie in 0-127, on the 60-tick grid, only on the channels (and controllers) the kit's rides
        name; CC91 and CC94 only around thrown notes: 127 on its tick before its note-on, back to 0 at its note-off or
        at the next note-on on the track that isn't thrown, whichever comes first (the kits' files check where the
        throws fall); and, on every track of a channel that throws anywhere in the kit's files, 0 on tick 0 before
        any note-on, so a file, a region or a loop's first pass starts dry whatever played before it."""
        for kit in KITS:
            throwing = {c[1] for path in mids(kit) for t in parsed(kit, path)[2][1:] for c in t['ccs']
                        if c[2] == 91 and c[3] == 127}
            for path in mids(kit):
                for t in parsed(kit, path)[2][1:]:
                    with self.subTest(kit=kit, file=path, track=t['name']):
                        rides = [c for c in t['ccs'] if c[2] in (74, 71)]
                        for tick, ch, cc, val in rides:
                            self.assertIn((ch, cc), {(CH.track_channel(kit, r['on']), r['cc'])
                                                     for r in Mi.kit_of(kit).rides})
                            self.assertEqual(0, tick % 60)
                        for cc in (74, 71):
                            ticks_ = [c[0] for c in rides if c[2] == cc]
                            self.assertEqual(sorted(set(ticks_)), ticks_)
                        self.assertEqual(set(), {c[2] for c in t['ccs']} - {71, 74, 91, 94})
                        throws = [c for c in t['ccs'] if c[2] in (91, 94)]
                        thrown = [n for n in t['notes'] if (n[0], n[2], 91, 127) in throws]
                        plain = sorted({n[0] for n in t['notes'] if n not in thrown})
                        back = lambda n: min([n[1]] + [on for on in plain if on > n[0]][:1])  # noqa: E731
                        ch = CH.track_channel(kit, t['name'])
                        reset = [(0, ch, cc, 0) for cc in (91, 94)] if ch in throwing else []
                        want = sorted([(n[0], n[2], cc, 127) for n in thrown for cc in (91, 94)]
                                      + [(back(n), n[2], cc, 0) for n in thrown for cc in (91, 94)] + reset)
                        self.assertEqual(want, sorted(throws))
                        if reset:               # on tick 0, before any note-on
                            ev = t['events']
                            first_on = next((i for i, e in enumerate(ev) if e[1] == 'on'), len(ev))
                            for cc in (91, 94):
                                self.assertIn((0, 'cc', ch, cc, 0), ev[:first_on])

    def test_a_throw_reaches_no_other_note(self):
        """No note-on that isn't thrown starts while CC91 or CC94 is up on its channel, in any file: Wubansia latches
        the sends at each note-on, so a throw held over a later note (FUT04's no-vox metal chord, thrown, over the
        accents after it) would throw that note too. A note-on is thrown where CC91 goes to 127 on its tick; on a
        tick the controllers come before the note-ons."""
        for kit in KITS:
            for path in mids(kit):
                by_ch = {}
                for t in parsed(kit, path)[2][1:]:
                    for tick, kind, ch, a, b in t['events']:
                        if kind == 'on' or (kind == 'cc' and a in (91, 94)):
                            by_ch.setdefault(ch, []).append((tick, kind == 'on', a, b))
                for ch, events in by_ch.items():
                    events.sort(key=lambda e: (e[0], e[1]))
                    thrown = {tick for tick, on, a, b in events if not on and a == 91 and b == 127}
                    up, leaks = {91: 0, 94: 0}, []
                    for tick, on, a, b in events:
                        if not on:
                            up[a] = b
                        elif any(up.values()) and tick not in thrown:
                            leaks.append(tick)
                    with self.subTest(kit=kit, file=path, channel=ch):
                        self.assertEqual([], leaks[:5])

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

class TestPartFiles(unittest.TestCase):
    def test_the_chords_on_channel_4_at_their_roots_and_the_voicing_on_3(self):
        """The chord sample sits on channel 4 at its roots, the zones (or the root) of its Items (no more note 60); the
        voicing alone on channel 3 (the v1 trigger shared it, audit A defect 1). The kits' files name the roots."""
        for kit in KITS:
            k = Mi.kit_of(kit)
            made = kit_files(kit)
            (trig,) = [n for n in made if n.startswith(f'{kit}_Chords_') and not n.endswith('_Voicing_4bars.mid')]
            (voic,) = [n for n in made if n.endswith('_Voicing_4bars.mid')]
            with self.subTest(kit=kit):
                notes = parse(made[trig])[2][1]['notes']
                self.assertEqual({4}, {ch for _, _, ch, _, _ in notes})
                self.assertLessEqual({key for _, _, _, key, _ in notes}, set(k.zones(4)))
                self.assertEqual({3}, {ch for _, _, ch, _, _ in parse(made[voic])[2][1]['notes']})

    def test_phrases_on_their_instruments_channels(self):
        """Every phrase file plays on its instrument's channel (no more channel 4 for all, audit A defect 2), and the
        instrument there is the phrase's Item; every bassline on its line's track (FUT01's Low on Organ Bass)."""
        for kit in KITS:
            k = Mi.kit_of(kit)
            made = kit_files(kit)
            for ph in k.phrases:
                with self.subTest(kit=kit, phrase=ph['name']):
                    (tr,) = parse(made[Mi.phrase_file(kit, ph)])[2][1:]
                    ch = CH.track_channel(kit, Mi.phrase_track(k, ph))
                    self.assertEqual({ch}, {c for _, _, c, _, _ in tr['notes']})
                    self.assertIn(ph['item'], k.items_on(ch))
            on = k.P['bass'].get('on', {})
            for name in k.P['bass']['lines']:
                f = next(n for n in made if re.fullmatch(rf'{kit}_Bassline_{name}(_InTheLoop)?_2bars\.mid', n))
                chans = {c for _, _, c, _, _ in parse(made[f])[2][1]['notes']}
                self.assertEqual({CH.track_channel(kit, on.get(name, 'Bass'))}, chans, (kit, name))

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
        """Integers (no fraction, no exponent, no -0, no leading zeros), strings, arrays and objects with no key twice:
        what core's strict reader takes, less its booleans and null (the content JSON's rule: a flag is the integer
        1)."""
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
                bare = re.sub(r'"[^"]*"', '', text)
                self.assertIsNone(re.search(r'(?<![\w.])-0(?![\d])|(?<![\w.])0\d', bare))
                self.assertIsNone(re.search(r'true|false|null', bare))
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
        """Every form, a kit's own or its genre's: a 32-bar intro and outro, every section and line on an 8-bar line,
        the sections tiling the mix; both variants share it. On its genre's default form (genre_kit()) a kit plays
        that form's sections."""
        self.assertEqual(set(self.FORM_KINDS), set(Mi.FORMS))
        for genre, form in Mi.FORMS.items():
            self.assertEqual(self.FORM_KINDS[genre], [kind for _, kind, _ in form], genre)
        for kit in KITS:
            K = kits.load(kit)
            for form in forms(kit):
                for v in VARIANTS:
                    R = form_ext(kit, form, v)[1]
                    with self.subTest(kit=kit, form=form, variant=v):
                        if form == 'genre':
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
                self.assertEqual([s['name'] for s in form_ext(kit, form, 'vox')[1]['sections']],
                                 [s['name'] for s in form_ext(kit, form, 'novox')[1]['sections']])

    def test_the_default_forms_use_the_machinery(self):
        """Every genre's default form has busy hats, turnarounds, a roll in its build's last line, vox modes, the
        accents' rules and the cues' sounds: what a kit's own form can build on."""
        for genre, form in Mi.FORMS.items():
            lines = [ln for _, _, ls in form for ln in ls]
            with self.subTest(genre=genre):
                self.assertTrue(any(ln.get('hats') == 'busy' for ln in lines))
                self.assertTrue(any(ln.get('end') == 'turn' for ln in lines))
                self.assertTrue(next(ls for _, kind, ls in form if kind == 'build')[-1].get('roll'))
                self.assertLessEqual({'loop', 'riff', 'call'}, {ln.get('vox') for ln in lines})
                self.assertLessEqual({'loop', 'alternate', 'lead-in'}, {ln.get('accents') for ln in lines})
                self.assertTrue(any(ln.get('downbeat') for ln in lines) and any(ln.get('into') for ln in lines))

    def test_the_regions_tile_the_whole_mix(self):
        """Every region has the whole mix's tracks in the same order and ends on its last bar line; its notes,
        controllers, marker and cues, shifted to its start bar, are the whole mix's, but for a note held over the
        section line: the region stops it on the line, the whole mix holds it (its throw's return to 0 follows it,
        checked file by file in test_ccs)."""
        def not_a_return(cc):
            return not (cc[2] in (91, 94) and cc[3] == 0)
        for kit in KITS:
            for v in VARIANTS:
                made, R = ext(kit, v)
                _, _, wtracks = parse(made[R['file']])
                names = [t['name'] for t in wtracks[1:]]
                got = {n: [] for n in names}
                gcc = {n: [] for n in names}
                marks = []
                ends = [(s['start_bar'] - 1 + s['bars']) * BAR for s in R['sections']]
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
                        gcc[t['name']] += [(a + off, b, c, d) for a, b, c, d in t['ccs'] if not_a_return((a, b, c, d))]
                    marks += [(tick + off, k, b) for tick, k, b in reg[0]['meta'] if k in (MARKER, CUE)]
                for t in wtracks[1:]:
                    cut = [(a, min(b, next(e for e in ends if e > a)), c, d, e) for a, b, c, d, e in t['notes']]
                    self.assertEqual(sorted(cut), sorted(got[t['name']]), (kit, v, t['name']))
                    self.assertEqual(sorted(filter(not_a_return, t['ccs'])), sorted(gcc[t['name']]),
                                     (kit, v, t['name']))
                self.assertEqual([m for m in wtracks[0]['meta'] if m[1] in (MARKER, CUE)], marks)

    def test_the_whole_mix_holds_a_note_over_a_section_line(self):
        """A note held over a section line keeps its whole length in the whole mix (FUT04's noise sweep, a one-bar
        one-shot, on the last 16th of a section) and stops on the line in its region, as a region must."""
        k = Mi.kit_of('FUT04')
        sweep = Mi.Hit(127, 'AccentNoiseSweepE')
        role, note = k.hit_note(sweep)
        form = (Mi.section('Drop', 'drop', (Mi.line(Mi.GROOVE),) * 3 + (Mi.line(Mi.GROOVE, hits=(sweep,)),)),
                Mi.section('Outro', 'outro', (Mi.line(Mi.DRUMS),) * 4))
        made = Mi.extended_of(Mi.Kit('FUT04', k.K, {**k.P, 'form': form}, items=k.items), 'vox')
        R = json.loads(made['regions.json'])
        line = 32 * BAR
        on = line - 240 + late('FUT04')
        for name, end, file in ((R['file'], on + note.ticks, 'whole'), (R['sections'][0]['file'], line, 'region')):
            tracks = {t['name']: t for t in parse(made[name])[2][1:]}
            with self.subTest(file=file):
                self.assertEqual('accents', role)
                self.assertGreater(on + note.ticks, line)
                self.assertIn((on, end), {(a, b) for a, b, *_ in tracks['Accents']['notes']})

    def test_a_marker_names_every_section_at_its_start(self):
        for kit in KITS:
            R, _, conductor = whole(kit, 'vox')
            want = [((s['start_bar'] - 1) * BAR, s['name']) for s in R['sections']]
            self.assertEqual(want, [(t, b.decode()) for t, k, b in conductor['meta'] if k == MARKER], kit)

    def test_32_bars_of_drums_at_each_end(self):
        """The first and the last 32 bars are drums only (channel 10, FUT04's rumble among them): no bass, harmony,
        vox, accents or FX starting there, and none sounding past the outro's first bar (the whole mix lets a note held
        over the section line ring on: FUT01's last stab of drop 2); the kick in every bar of the first and last 8, for
        a DJ to beatmatch on (UK garage's 2-step also the hats and the perc, which carry its pulse)."""
        for kit in KITS:
            garage = kits.load(kit).DIRECTION == 'future garage / 2-step'
            for form in forms(kit):
                for v in VARIANTS:
                    R, tracks, conductor = form_whole(kit, form, v)
                    end = conductor['end']
                    for name, t in tracks.items():
                        with self.subTest(kit=kit, form=form, variant=v, track=name):
                            if CH.track_channel(kit, name) == 10:
                                continue
                            self.assertFalse([n for n in t['notes'] if n[0] < 32 * BAR or n[0] >= end - 32 * BAR
                                              or n[1] > end - 31 * BAR])
                    kick = lane_tracks(kit, {k: t for k, t in tracks.items() if k == 'Kick'}, 'kick')[0]
                    for lo in (0, end - 8 * BAR):
                        self.assertEqual(set(range(8)), {(on - lo) // BAR for on, *_ in ons(kick, lo, lo + 8 * BAR)},
                                         (kit, form))
                        if garage:
                            for lane in ('hats', 'perc'):
                                got = {(on - lo) // BAR for on, *_ in lane_ons(kit, tracks, lane, lo, lo + 8 * BAR)}
                                self.assertEqual(set(range(8)), got, (kit, form, lane))

    def test_the_intro_brings_the_drums_in_on_8_bar_lines_and_the_bass_after_it(self):
        """The kick from the first bar, the other drums within the 32-bar intro on 8-bar lines, the bass after it on a
        line and the chords from it on; in the genres' default forms the bass at bar 33."""
        for kit in KITS:
            for form in forms(kit):
                R, tracks, _ = form_whole(kit, form, 'vox')
                first = {}
                for name, t in tracks.items():
                    if t['notes']:
                        lane = CH.track_lane(kit, name)
                        first[lane] = min(first.get(lane, 10 ** 9), t['notes'][0][0] // BAR)
                with self.subTest(kit=kit, form=form):
                    self.assertEqual(0, first['kick'])
                    for lane in ('snare', 'hats', 'perc'):
                        self.assertLess(first[lane], 32)
                        self.assertEqual(0, first[lane] % 8, lane)
                    self.assertGreaterEqual(first['bass'], 32)
                    self.assertEqual(0, first['bass'] % 8)
                    if form == 'genre':
                        self.assertEqual(32, first['bass'])
                    self.assertLessEqual(first['bass'], first['chords'])
                    self.assertEqual(0, first['chords'] % 8)

    def test_the_breaks_have_no_kick_and_the_drops_every_lane(self):
        for kit in KITS:
            for form in forms(kit):
                for v in VARIANTS:
                    made, R = form_ext(kit, form, v)
                    for s in R['sections']:
                        tracks = {t['name']: t for t in parse(made[s['file']])[2][1:]}
                        with self.subTest(kit=kit, form=form, variant=v, section=s['name']):
                            if s['kind'] == 'break':
                                self.assertEqual([], lane_ons(kit, {'Kick': tracks['Kick']}, 'kick'))
                                self.assertIn('chords', s['lanes'])
                            if s['kind'] in ('drop', 'drop2'):
                                for lane in ('kick', 'hats', 'bass', 'accents'):
                                    self.assertIn(lane, s['lanes'])
                                self.assertEqual(v == 'vox', 'vox' in s['lanes'])

    def test_the_drop_is_not_the_groove(self):
        """Audit B, headline 1: the drop's first line differs from the groove's last line beyond the drums: the lead,
        the vox or the accents change; in the form a kit plays (its own, else its genre's), drop 2 plays a bassline
        other than the switch-up's (cross-kit issue 4: a kit picks its switch-up's and drop 2's basslines for the form
        it plays)."""
        for kit in KITS:
            for form in forms(kit):
                R, tracks, _ = form_whole(kit, form, 'vox')
                groove = next((s for s in R['sections'] if s['kind'] == 'groove'), None)
                drop = next((s for s in R['sections'] if s['kind'] == 'drop'), None)
                if groove is None or drop is None:
                    continue
                g0 = (groove['lines'][-1]['start_bar'] - 1) * BAR
                d0 = (drop['start_bar'] - 1) * BAR
                changed = []
                for lane in ('lead', 'vox', 'accents'):
                    a = [(on - g0, key) for on, key, _ in lane_ons(kit, tracks, lane, g0, g0 + 8 * BAR)]
                    b = [(on - d0, key) for on, key, _ in lane_ons(kit, tracks, lane, d0, d0 + 8 * BAR)]
                    if a != b:
                        changed.append(lane)
                self.assertTrue(changed, (kit, form))
                secs = {s['kind']: s for s in R['sections']}
                if 'switch' in secs and form == forms(kit)[0]:
                    self.assertNotEqual({ln.get('bass') for ln in secs['switch']['lines'] if ln.get('bass')},
                                        {ln.get('bass') for ln in secs['drop2']['lines'] if ln.get('bass')},
                                        (kit, form))

    def test_the_cues_have_their_notes(self):
        """A Vox cue only in the vox file, on the bar where its line's vox comes in (its first vox note; none before it
        in the line); every Riser cue an FX note ending on the next section's first beat; every Fill cue its fill's
        note (FX, or the line's own fill sound) ending on the next downbeat. The genres' default forms have risers
        and vox cues."""
        for kit in KITS:
            for form in forms(kit):
                for v in VARIANTS:
                    R, tracks, conductor = form_whole(kit, form, v)
                    starts = {(s['start_bar'] - 1) * BAR for s in R['sections']}
                    fx = tracks.get('FX', {'notes': []})['notes']
                    lines = {ln['start_bar']: ln for s in R['sections'] for ln in s['lines']}
                    cues = [(t, b.decode()) for t, k, b in conductor['meta'] if k == CUE]
                    self.assertEqual([((c['bar'] - 1) * BAR, c['cue']) for s in R['sections'] for c in s['cues']],
                                     cues)
                    if form == 'genre':
                        self.assertTrue(any(c == 'Riser' for _, c in cues))
                        self.assertEqual(v == 'vox', any(c == 'Vox' for _, c in cues))
                    vox = tracks.get('Vox', {'notes': []})['notes']
                    for t, c in cues:
                        with self.subTest(kit=kit, form=form, variant=v, cue=c, bar=t // BAR + 1):
                            self.assertEqual(0, t % BAR)
                            if c == 'Vox':
                                self.assertEqual('vox', v)
                                lo = (max(b for b in lines if b <= t // BAR + 1) - 1) * BAR
                                self.assertTrue([n for n in vox if t <= n[0] < t + BAR])
                                self.assertFalse([n for n in vox if lo <= n[0] < t])
                            if c == 'Riser':
                                self.assertIn(t + 8 * BAR, starts)
                                self.assertTrue([n for n in fx if n[1] == t + 8 * BAR and n[1] - n[0] >= 4 * BAR])
                            if c == 'Fill':
                                self.assertTrue(fill_notes(kit, tracks, lines[t // BAR + 1 - 7]))

    def test_the_builds_roll_up_to_the_fill(self):
        """A roll, in any form: where its line ends on a fill, its last hit falls before the fill starts, inside the
        fill's bar or the bar before; its velocities rise hit by hit. The genres' default forms roll in their build's
        last line up to its fill, the kick sitting the roll out in sci-fi club and playing on in house, techno and UK
        garage."""
        for kit in KITS:
            K = kits.load(kit)
            for form in forms(kit):
                R, tracks, _ = form_whole(kit, form, 'vox')
                rolls = [ln for s in R['sections'] for ln in s['lines'] if ln.get('roll')]
                build = next((s for s in R['sections'] if s['kind'] == 'build'), None)
                if form == 'genre':
                    self.assertIn(build['lines'][-1], rolls, kit)
                    self.assertEqual('fill', build['lines'][-1].get('end'), kit)
                for ln in rolls:
                    lo = (ln['start_bar'] - 1) * BAR
                    hits = lane_ons(kit, tracks, ln['roll'], lo, lo + 8 * BAR)
                    first = ln['roll_from'] - ln['start_bar']           # the bars the roll plays, of the line's 8
                    rolled = [h for h in hits if h[0] >= lo + first * BAR]
                    with self.subTest(kit=kit, form=form, bar=ln['start_bar']):
                        self.assertEqual(set(range(first, first + ln['roll_bars'])),
                                         {(h[0] - lo) // BAR for h in rolled})
                        if ln.get('end') == 'fill':
                            fills = fill_notes(kit, tracks, ln)
                            self.assertTrue(fills)
                            fill_on = min(n[0] for n in fills)
                            self.assertLess(max(h[0] for h in rolled), fill_on)
                            self.assertGreaterEqual(max(h[0] for h in rolled), fill_on - BAR)
                        vels = [vel for _, _, vel in rolled]
                        self.assertEqual(sorted(vels), vels)
                        self.assertGreater(vels[-1], vels[0])
                        if form == 'genre' and ln == build['lines'][-1]:
                            kick = lane_ons(kit, tracks, 'kick', lo, lo + 7 * BAR)
                            self.assertEqual(K.DIRECTION == 'sci-fi club', not kick)

    def test_busy_hats_are_busier(self):
        """A busy line plays the kit's own busy pattern, or a hat on every 16th its full hats leave empty, at a
        velocity of 0.35 or more (audit B: FUT01's v1 'busy' added nothing, the others one ghost a bar at 0.2)."""
        for kit in KITS:
            k = Mi.kit_of(kit)
            authored = k.P['drums'].get('busy')
            for form in forms(kit):
                R, tracks, _ = form_whole(kit, form, 'vox')
                busy = [ln for s in R['sections'] for ln in s['lines'] if ln.get('hats') == 'busy']
                for ln in busy:
                    lo = (ln['start_bar'] - 1) * BAR
                    for b in range(7):
                        hats = lane_ons(kit, tracks, 'hats', lo + b * BAR, lo + (b + 1) * BAR)
                        with self.subTest(kit=kit, form=form, bar=ln['start_bar'] + b):
                            steps = {round((on - lo - b * BAR - (late(kit) if on % 480 else 0)) / 240)
                                     for on, _, _ in hats}
                            if authored:
                                self.assertLessEqual({round(n[0]) % 16 for n in authored}, steps)
                            else:
                                self.assertEqual(set(range(16)), steps)
                                full = {round(n.step) % 16 for n in k.drums('hats', 'groove')
                                        if n.step // 16 == b % 4}
                                added = [vel for on, _, vel in hats if round((on - lo - b * BAR
                                                                              - (late(kit) if on % 480 else 0)) / 240)
                                         not in full]
                                self.assertGreaterEqual(min(added, default=127), 44)

    def test_the_turnarounds_change_the_lines_last_bar(self):
        for kit in KITS:
            for form in forms(kit):
                R, tracks, _ = form_whole(kit, form, 'vox')
                turns = [ln for s in R['sections'] for ln in s['lines'] if ln.get('end') == 'turn']
                for ln in turns:
                    lo = (ln['start_bar'] - 1) * BAR
                    drums = lambda a, b: sorted((on - a, key) for lane in CH.DRUM_LANES  # noqa: E731
                                                for on, key, _ in lane_ons(kit, tracks, lane, a, b))
                    self.assertNotEqual(drums(lo + 5 * BAR, lo + 6 * BAR), drums(lo + 7 * BAR, lo + 8 * BAR),
                                        (kit, form, ln['start_bar']))

    def test_the_mixes_differ_only_where_they_should(self):
        """The vox and no-vox mixes differ only in Vox, Accents (and their throws) and the chord stabs the no-vox
        accents take over (PARTS['loop']['stab_drop'], only in the blocks those accents play)."""
        for kit in KITS:
            drop = {at(kit, st) for st in Mi.kit_of(kit).P['loop'].get('stab_drop', {}).get('novox', ())}
            for form in forms(kit):
                Ra, a, _ = form_whole(kit, form, 'vox')
                Rb, b, _ = form_whole(kit, form, 'novox')
                with self.subTest(kit=kit, form=form):
                    self.assertEqual(set(a) - {'Vox'}, set(b))
                    for name in b:
                        if name == 'Accents':
                            continue
                        if name in ('Chords', 'Chord voicing'):
                            extra = [n for n in a[name]['notes'] if n not in b[name]['notes']]
                            self.assertLessEqual({n[0] % (4 * BAR) for n in extra}, drop)
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


class TestLineRules(unittest.TestCase):
    """Line options a kit's own form uses, on a kit's real PARTS with a scratch form (one 32-bar drop): vox-channel
    phrases, a lead's call resting in the fill bar where the loop's lead rests there, phrases resting from the kit's
    fill start when a line asks (phrases_rest), and what the READMEs and regions.json say of a line's phrases."""
    CHOIR = 'Vox choir chords, the velocity picks the vowel'
    STAB = 'Laser stab riff, Dm9 and B-flat maj9'

    @staticmethod
    def mix(kit, lines, variant, **parts):
        """The scratch form's mix (PARTS overridden by `parts`): (regions.json, its tracks by name, its README)."""
        k = Mi.kit_of(kit)
        scratch = Mi.Kit(kit, k.K, {**k.P, **parts, 'form': (Mi.section('Drop', 'drop', lines),)}, items=k.items)
        made = Mi.extended_of(scratch, variant)
        R = json.loads(made['regions.json'])
        return R, {t['name']: t for t in parse(made[R['file']])[2][1:]}, made['README.md'].decode()

    @staticmethod
    def notes(tracks, name, line):
        """A track's notes (on, off, ch, key, vel) in the scratch form's nth line, on ticks from the line's start."""
        lo = line * 8 * BAR
        return [(on - lo, off - lo, ch, key, vel) for on, off, ch, key, vel in tracks.get(name, {'notes': []})['notes']
                if lo <= on < lo + 8 * BAR]

    def test_a_vox_phrase_plays_in_the_vox_mix_only_and_in_place_of_the_vox_mode(self):
        """FUT02's vox choir chords (channel 5) on a line with a vox mode: in the vox mix the Vox track plays the
        choir's notes alone (the riff's would cut its held chords on the mono vox line); the no-vox mix has no vox."""
        k = Mi.kit_of('FUT02')
        choir = next(ph for ph in k.phrases if ph['name'] == self.CHOIR)
        lines = (Mi.line(Mi.EVERYTHING, vox='riff', phrases=(self.CHOIR,)), Mi.line(Mi.EVERYTHING, vox='riff'),
                 Mi.line(Mi.GROOVE), Mi.line(Mi.GROOVE))
        R, tracks, _ = self.mix('FUT02', lines, 'vox')
        got = self.notes(tracks, 'Vox', 0)
        want = sorted({(at('FUT02', n[0] + 64 * r), n[1]) for r in (0, 1) for n in choir['notes']})
        self.assertEqual(want, sorted({(on, key) for on, _, _, key, _ in got}))
        self.assertTrue(self.notes(tracks, 'Vox', 1))           # the next line sings its riff
        R, tracks, _ = self.mix('FUT02', lines, 'novox')
        self.assertNotIn('Vox', tracks)

    def test_the_leads_call_rests_in_the_fill_bar_where_the_loops_lead_does(self):
        """On a fill line the lead's skeleton (here its whole phrase, so its second bar falls in the fill bar) and its
        call rest from the fill bar, a note held into it cut there, where the loop's lead rests there (FUT02's arp,
        context.ARP_REST); FUT01's pluck plays on through its fill bar, as its loop does; a line with no fill plays its
        last bar."""
        lines = (Mi.line(Mi.EVERYTHING, lead='skeleton', end='fill'), Mi.line(Mi.EVERYTHING, lead='skeleton'),
                 Mi.line(Mi.EVERYTHING, lead='call', end='fill'), Mi.line(Mi.GROOVE))
        for kit, rests in (('FUT02', True), ('FUT01', False)):
            lead = Mi.kit_of(kit).P['lead']
            for v in VARIANTS:
                _, tracks, _ = self.mix(kit, lines, v, lead={**lead, 'skeleton': lead['notes']})
                with self.subTest(kit=kit, variant=v):
                    self.assertEqual(rests, not [n for n in self.notes(tracks, 'Lead', 0) if n[0] >= 7 * BAR])
                    self.assertTrue([n for n in self.notes(tracks, 'Lead', 1) if n[0] >= 7 * BAR])
                    if rests:
                        for line in (0, 2):
                            self.assertFalse([n for n in self.notes(tracks, 'Lead', line) if n[1] > 7 * BAR])

    def test_phrases_rest_from_the_fill_start_only_when_a_line_asks(self):
        """phrases_rest=True on a fill line: the line's phrases rest from the kit's fill start (FUT02's laser fill
        takes the whole last bar; FUT01's glitch fill plays from its 16th 12), a note held over it cut there. Without
        it, the phrases play on under the fill (FUT01's loop plays its lead and bass under its fill)."""
        for kit, phrase, start in (('FUT02', self.STAB, 0), ('FUT01', 'Organ riff', 12)):
            lines = (Mi.line(Mi.EVERYTHING, phrases=(phrase,), phrases_rest=True, end='fill'),
                     Mi.line(Mi.EVERYTHING, phrases=(phrase,), end='fill'), Mi.line(Mi.GROOVE), Mi.line(Mi.GROOVE))
            k = Mi.kit_of(kit)
            track = Mi.phrase_track(k, next(ph for ph in k.phrases if ph['name'] == phrase))
            _, tracks, _ = self.mix(kit, lines, 'vox')
            rest = (7 * 16 + start) * 240
            with self.subTest(kit=kit):
                rested = self.notes(tracks, track, 0)
                self.assertTrue(rested)
                self.assertFalse([n for n in rested if n[0] >= rest or n[1] > rest])
                if start:
                    self.assertTrue([n for n in rested if 7 * BAR <= n[0] < rest])
                self.assertTrue([n for n in self.notes(tracks, track, 1) if n[0] >= rest])

    def test_the_readme_and_regions_name_the_phrases_that_play(self):
        """A vox-channel phrase's line reads 'vox (the vox choir chords)', not the vox mode it replaces; regions.json
        lists only the phrases that play in its variant (no choir in the no-vox mix) and no vox mode a phrase
        replaced."""
        lines = (Mi.line(Mi.EVERYTHING, vox='riff', phrases=(self.CHOIR, self.STAB)), Mi.line(Mi.GROOVE),
                 Mi.line(Mi.GROOVE), Mi.line(Mi.GROOVE))
        R, _, text = self.mix('FUT02', lines, 'vox')
        info = R['sections'][0]['lines'][0]
        self.assertEqual([self.CHOIR, self.STAB], info['phrases'])
        self.assertNotIn('vox', info)
        self.assertIn('vox (the vox choir chords)', text)
        self.assertIn('chords with the laser stab riff', text)
        self.assertNotIn('vox (riff)', text)
        R, _, text = self.mix('FUT02', lines, 'novox')
        info = R['sections'][0]['lines'][0]
        self.assertEqual([self.STAB], info['phrases'])
        self.assertNotIn('choir', text)

    def test_a_line_ends_on_its_own_fill(self):
        """fill=<sound>: the line ends on that sound, end-aligned on its end, in place of the kit's fill; every drum
        lane (busy hats and a roll included) rests from its start (FUT01's whole glitch fill from beat 3, where its
        loop's plays from beat 4). regions.json and the README say so."""
        k = Mi.kit_of('FUT01')
        whole, beat4 = k.key_of('fx', 'GlitchFillLift'), k.key_of('fx', 'GlitchFillLift', start=4)
        roll = dict(lane='snare', bars=1, every=((1, 2),), last=1)
        lines = (Mi.line(Mi.DRUMS + ' bass chords', hats='busy', fill='GlitchFillLift', end='fill'),
                 Mi.line(Mi.DRUMS + ' bass chords', end='fill'),
                 Mi.line(Mi.DRUMS, roll=roll, fill='GlitchFillLift', end='fill'), Mi.line(Mi.GROOVE))
        start = 7 * BAR + 8 * 240
        for v in VARIANTS:
            R, tracks, text = self.mix('FUT01', lines, v)
            with self.subTest(variant=v):
                for line in (0, 2):
                    fx = [(on, off, key) for on, off, _, key, _ in self.notes(tracks, 'FX', line)]
                    self.assertIn((start, 8 * BAR, whole), fx)
                    self.assertNotIn(beat4, {key for *_, key in fx})
                    drums = [n for name in ('Kick', 'Clap', 'Hats', 'Perc') for n in self.notes(tracks, name, line)]
                    self.assertFalse([n for n in drums if n[0] >= start])
                    self.assertTrue([n for n in drums if 7 * BAR <= n[0] < start])
                self.assertIn(beat4, {key for *_, key, _ in self.notes(tracks, 'FX', 1)})
                info = R['sections'][0]['lines']
                self.assertEqual(('GlitchFillLift', None), (info[0].get('fill'), info[1].get('fill')))
                self.assertIn('the fill (Glitch Fill Lift) in its last bar, the drums out from its start', text)

    def test_a_drum_variant_on_a_fill_line(self):
        """A variant lane plays its own bars 1-3; in the fill bar its own fill bar (variants[name]['fill'][lane]) if
        it gives one, else the kit's fill bar for that lane, which leaves room for the kit's fill. regions.json and
        the README name the variant."""
        k = Mi.kit_of('FUT01')
        variants = {'4x4': {'kick': [(4 * b, 36, 0.5, 1.0, False) for b in range(4)],
                            'snare': [(4, 38, 0.5, 1.0, False), (12, 38, 0.5, 1.0, False)],
                            'fill': {'kick': [(0, 36, 0.5, 1.0, False)]}}}
        lines = (Mi.line(Mi.DRUMS, drums='4x4', end='fill'), Mi.line(Mi.DRUMS, drums='4x4'), Mi.line(Mi.GROOVE),
                 Mi.line(Mi.GROOVE))
        R, tracks, text = self.mix('FUT01', lines, 'vox', drums={**k.P['drums'], 'variants': variants})
        self.assertEqual([960 * i for i in range(28)] + [7 * BAR], [n[0] for n in self.notes(tracks, 'Kick', 0)])
        kit_fill = sorted(at('FUT01', n.step - 48) for n in k.block('drums', 'fill', 'snare') if n.step >= 48)
        self.assertEqual(kit_fill, sorted(n[0] - 7 * BAR for n in self.notes(tracks, 'Clap', 0) if n[0] >= 7 * BAR))
        self.assertEqual([960 * i for i in range(32)], [n[0] for n in self.notes(tracks, 'Kick', 1)])
        self.assertEqual('4x4', R['sections'][0]['lines'][0]['drums'])
        self.assertIn("the drums' 4x4 variant", text)

    def test_a_roll_can_rest_its_lane_alone(self):
        """roll rest='lane': only the rolled lane rests from the fill's start, the other lanes play the kit's fill bar
        under it (FUT03's FillPreview bar under its one-bar Build Fill); rest='all', the default, rests every drum."""
        k = Mi.kit_of('FUT03')
        roll = dict(lane='snare', bars=1, every=((1, 2),), last=1)
        lines = (Mi.line(Mi.DRUMS, roll={**roll, 'rest': 'lane'}, end='fill'), Mi.line(Mi.DRUMS, roll=roll, end='fill'),
                 Mi.line(Mi.GROOVE), Mi.line(Mi.GROOVE))
        _, tracks, _ = self.mix('FUT03', lines, 'vox')
        want = sorted(at('FUT03', n.step - 48) + 7 * BAR for n in k.block('drums', 'fill', 'kick') if n.step >= 48)
        self.assertTrue(want)
        self.assertEqual(want, [n[0] for n in self.notes(tracks, 'Kick', 0) if n[0] >= 7 * BAR])
        self.assertFalse([n for n in self.notes(tracks, 'Clap', 0) if n[0] >= 7 * BAR])
        self.assertFalse([n for name in ('Kick', 'Clap', 'Hats', 'Perc') for n in self.notes(tracks, name, 1)
                          if n[0] >= 7 * BAR])

    def test_a_named_turnaround(self):
        """turn=<name> plays PARTS['drums']['turns'][name] in the line's last bar; PARTS['drums']['turn'] stays the
        default; a lane the turnaround doesn't name plays its bar as written."""
        k = Mi.kit_of('FUT01')
        drums = {**k.P['drums'], 'turn': {'snare': [(15, 38, 0.5, 0.6, False)]},
                 'turns': {'tamb_out': {'perc': [(0, 54, 0.5, 0.4, False)]}}}
        lines = (Mi.line(Mi.DRUMS, end='turn', turn='tamb_out'), Mi.line(Mi.DRUMS, end='turn'), Mi.line(Mi.GROOVE),
                 Mi.line(Mi.GROOVE))
        R, tracks, text = self.mix('FUT01', lines, 'vox', drums=drums)
        last = lambda name, line: [(n[0] - 7 * BAR, n[3]) for n in self.notes(tracks, name, line)  # noqa: E731
                                   if n[0] >= 7 * BAR]
        self.assertEqual([(0, 54)], last('Perc', 0))
        self.assertEqual([(15 * 240 + late('FUT01'), 38)], last('Clap', 1))
        groove = sorted((at('FUT01', n.step - 48), n.key) for n in k.block('drums', 'groove', 'snare') if n.step >= 48)
        self.assertEqual(groove, last('Clap', 0))
        info = R['sections'][0]['lines']
        self.assertEqual(('tamb_out', None), (info[0].get('turn'), info[1].get('turn')))
        self.assertIn('a turnaround (tamb_out) in its last bar', text)

    def test_hits_on_a_line(self):
        """hits: one-shots on channels 6 and 7 at 16ths from the line's start, in both variants, or per variant from a
        dict; regions.json lists them as (16th, sound); a vox chop or a 16th off the grid is refused."""
        k = Mi.kit_of('FUT01')
        lines = (Mi.line(Mi.GROOVE, hits=(Mi.Hit(14, 'OrganHit', 2), Mi.Hit(32, 'ReverseStab', 2, 'end'))),
                 Mi.line(Mi.GROOVE, hits={'vox': (Mi.Hit(6, 'VoxSynthStab', 2),),
                                          'novox': (Mi.Hit(6, 'WarpStab', 2),)}),
                 Mi.line(Mi.GROOVE), Mi.line(Mi.GROOVE))
        for v in VARIANTS:
            R, tracks, _ = self.mix('FUT01', lines, v)
            want = 'VoxSynthStab' if v == 'vox' else 'WarpStab'
            with self.subTest(variant=v):
                acc = self.notes(tracks, 'Accents', 0)
                self.assertIn((14 * 240, k.key_of('accents', 'OrganHit')), [(n[0], n[3]) for n in acc])
                self.assertIn((32 * 240, k.key_of('accents', 'ReverseStab')), [(n[1], n[3]) for n in acc])
                self.assertEqual([(6 * 240, k.key_of('accents', want))],
                                 [(n[0], n[3]) for n in self.notes(tracks, 'Accents', 1)])
                info = R['sections'][0]['lines']
                self.assertEqual(([[14, 'OrganHit'], [32, 'ReverseStab']], [[6, want]]),
                                 (info[0]['hits'], info[1]['hits']))
        for bad in (Mi.Hit(4, 'VoxChopAhCs5'), Mi.Hit(4.5, 'OrganHit', 2)):
            with self.assertRaises(ValueError):
                self.mix('FUT01', (Mi.line(Mi.GROOVE, hits=(bad,)),) + lines[1:], 'vox')

    def test_the_leads_answer(self):
        """lead='answer': the lead's second bar alone, where it falls, every 2 bars."""
        k = Mi.kit_of('FUT01')
        lines = (Mi.line(Mi.GROOVE + ' lead', lead='answer'), Mi.line(Mi.GROOVE), Mi.line(Mi.GROOVE),
                 Mi.line(Mi.GROOVE))
        _, tracks, text = self.mix('FUT01', lines, 'vox')
        second = [n for n in Mi.as_notes(k.P['lead']['notes']) if 16 <= n.step < 32]
        want = sorted((at('FUT01', n.step + 32 * r), n.key) for r in range(4) for n in second)
        self.assertEqual(want, sorted((n[0], n[3]) for n in self.notes(tracks, 'Lead', 0)))
        self.assertIn("the lead's answer", text)

    def test_the_kit_readme_names_its_own_form(self):
        """The kit README says the extended mix is in the kit's own form when PARTS['form'] is set, else in its
        genre's (FUT01 with a scratch form, and with its form taken out)."""
        k = Mi.kit_of('FUT01')
        own = Mi.Kit('FUT01', k.K, {**k.P, 'form': (Mi.section('Drop', 'drop', (Mi.line(Mi.GROOVE),) * 4),)},
                     items=k.items)
        flat = lambda text: ' '.join(text.split())  # noqa: E731   (the README wraps its lines)
        self.assertIn("a 32-bar mix in the kit's own form", flat(Mi.files_of(own)['README.md'].decode()))
        self.assertIn("a 224-bar mix in the genre's form", flat(Mi.files_of(genre_kit('FUT01'))['README.md'].decode()))

    def test_a_ride_on_points(self):
        """ride={track: ('points', ((bar, value), ...))}: breakpoints over the line (bars 0-8) joined linearly, held
        before the first and after the last, on every ride of the track; regions.json records it (integers and
        strings) and the README names it."""
        lines = (Mi.line(DRUMS_RUMBLE_BASS, ride={'Bass': ('points', ((2, 0), (6, 127)))}), Mi.line(DRUMS_RUMBLE_BASS),
                 Mi.line(Mi.GROOVE), Mi.line(Mi.GROOVE))
        R, tracks, text = self.mix('FUT04', lines, 'vox')
        made = Mi.extended_of(Mi.Kit('FUT04', Mi.kit_of('FUT04').K, {**Mi.kit_of('FUT04').P, 'form': (
            Mi.section('Drop', 'drop', lines),)}, items=Mi.kit_of('FUT04').items), 'vox')
        json.loads(made['regions.json'], parse_float=lambda t: self.fail(t))
        for cc in (74, 71):
            r = {t: v for t, _, c, v in tracks['Bass']['ccs'] if c == cc and t < 8 * BAR}
            value = lambda tick: r[max(t for t in r if t <= tick)]  # noqa: E731
            with self.subTest(cc=cc):
                self.assertEqual((0, 0, 64, 127, 127), (value(0), value(2 * BAR), value(4 * BAR), value(6 * BAR),
                                                        value(8 * BAR - 60)))
        info = R['sections'][0]['lines']
        self.assertEqual({'Bass': ['points', [[2, 0], [6, 127]]]}, info[0]['ride'])
        self.assertNotIn('ride', info[1])
        self.assertIn('the Bass filter ride 0 at bar 2 then 127 at bar 6', text)

    def test_a_rest_cuts_what_would_sound_past_it(self):
        """Where the drums rest (a roll's fill, a drum drop's last bar, a line's own fill), nothing on the drums'
        channel sounds past the rest: FUT04's 2-bar rumble notes stop with the kick under the stutter fill and the
        drop bar."""
        roll = dict(lane='snare', bars=1, every=((1, 1),))
        lines = (Mi.line(DRUMS_RUMBLE_BASS, roll=roll, end='fill'), Mi.line(DRUMS_RUMBLE_BASS, end='drop'),
                 Mi.line(DRUMS_RUMBLE_BASS, fill='AccentNoiseSweepE', end='fill'), Mi.line(Mi.GROOVE))
        _, tracks, _ = self.mix('FUT04', lines, 'vox')
        for line, rest in ((0, 7 * BAR), (1, 7 * BAR), (2, 7 * BAR)):
            with self.subTest(line=line):
                drums = [n for name in ('Kick', 'Rumble', 'Snare', 'Hats', 'Perc')
                         for n in self.notes(tracks, name, line)]
                self.assertTrue([n for n in self.notes(tracks, 'Rumble', line) if n[0] < rest < n[0] + 32 * 240])
                self.assertFalse([n for n in drums if n[1] > rest])

    def test_a_phrase_in_place_of_a_part_and_the_hats_by_block(self):
        """A phrase on the lead's channel (FUT03's keys arp) plays in place of the lead: regions.json reports no lead
        mode and the README names the phrase; a per-block hats pair reads as one ('hats, busy in its second half');
        regions.json no longer lists the basslines' roles (nothing reads them, and a form names its own)."""
        lines = (Mi.line(Mi.GROOVE + ' lead', phrases=('Keys arp',), hats=('full', 'busy')),
                 Mi.line(Mi.GROOVE, hats=('thin', '16ths')), Mi.line(Mi.GROOVE), Mi.line(Mi.GROOVE))
        R, _, text = self.mix('FUT03', lines, 'vox')
        info = R['sections'][0]['lines'][0]
        self.assertEqual(['Keys arp'], info['phrases'])
        self.assertNotIn('lead', info)
        self.assertIn('lead (the keys arp)', text)
        self.assertIn('hats, busy in its second half', text)
        self.assertIn('off-beat hats, 16ths in its second half', text)
        self.assertNotIn('basslines', R)
        for kit in KITS:
            for v in VARIANTS:
                self.assertNotIn('basslines', ext(kit, v)[1])

    @staticmethod
    def said(text, line):
        """The README's words for the scratch form's nth line."""
        return next(row.split(': ', 1)[1] for row in text.splitlines()
                    if row.startswith(f'- {1 + 8 * line}-{8 + 8 * line}: '))

    def test_a_lines_parts_are_its_own_not_its_lanes(self):
        """A part's mode (regions.json and the README) is keyed on the part being in the line, not on its lane having
        notes: a phrase on an extra's channel reads '<lane> (<phrase>)' where it plays alone on its lane (FUT01's organ
        riff on channel 12 is not the pluck, FUT02's laser stab riff on 12 not its pad chords, FUT04's rave stab riff
        on 13 not its accents), '<part> with <phrase>' beside the lane's own part."""
        R, _, text = self.mix('FUT01', (Mi.line(Mi.GROOVE, phrases=('Organ riff',)),
                                        Mi.line(Mi.GROOVE + ' lead', lead='call', phrases=('Organ riff',)),
                                        Mi.line(Mi.GROOVE), Mi.line(Mi.GROOVE)), 'vox')
        info = R['sections'][0]['lines']
        self.assertIn('lead', info[0]['lanes'])
        self.assertNotIn('lead', info[0])
        self.assertEqual('call', info[1]['lead'])
        self.assertIn('lead (the organ riff)', self.said(text, 0))
        self.assertEqual(1, self.said(text, 0).count('lead'))
        self.assertIn("the lead's call with the organ riff", self.said(text, 1))
        k = Mi.kit_of('FUT02')
        alt = {'alt': dict(trigger=k.block('chords', 'groove'), voicing=k.block('voicing', 'groove'))}
        R, _, text = self.mix('FUT02', (Mi.line(Mi.DRUMS_BASS, chords='alt', phrases=(self.STAB,)),
                                        Mi.line(Mi.GROOVE, chords='alt', phrases=(self.STAB,)),
                                        Mi.line(Mi.GROOVE), Mi.line(Mi.GROOVE)), 'vox',
                               chords={**k.P['chords'], 'blocks': alt})
        info = R['sections'][0]['lines']
        self.assertIn('chords', info[0]['lanes'])
        self.assertNotIn('chords', info[0])
        self.assertEqual('alt', info[1]['chords'])
        self.assertIn('chords (the laser stab riff)', self.said(text, 0))
        self.assertEqual(1, self.said(text, 0).count('chords'))
        self.assertIn('chords (alt) with the laser stab riff', self.said(text, 1))
        lines = (Mi.line(DRUMS_RUMBLE_BASS, phrases=('Rave stab riff',)),
                 Mi.line(DRUMS_RUMBLE_BASS, accents='loop', phrases=('Rave stab riff',)),
                 Mi.line(Mi.GROOVE), Mi.line(Mi.GROOVE))
        _, _, text = self.mix('FUT04', lines, 'vox')
        self.assertIn('accents (the rave stab riff)', self.said(text, 0))
        self.assertIn('accents with the rave stab riff', self.said(text, 1))

    def test_the_pad_and_a_thrown_vox_note_are_named(self):
        """The pad is named as the pad, not as the chords whose lane it plays on (FUT03's Pad Warm), and regions.json
        records the chord block it plays where that is its own ('pad'); a thrown vox note is recorded (throw: 1) and
        reads "the vox's last note thrown" on a line with no vox mode (FUT04's groove throws its D4 into the break),
        "vox (call), thrown" beside one, and neither in the no-vox mix."""
        k = Mi.kit_of('FUT03')
        pad = {**k.P['pad'], 'blocks': {**k.P['pad']['blocks'], 'alt': [(0, 60, 64, 1.0, False)]}}
        R, _, text = self.mix('FUT03', (Mi.line('kick pad'), Mi.line('kick pad chords'),
                                        Mi.line('kick pad', chords='alt'), Mi.line(Mi.GROOVE)), 'vox', pad=pad)
        info = R['sections'][0]['lines']
        self.assertEqual(('kick, pad', 'kick, chords, pad', 'kick, pad (alt)'),
                         tuple(self.said(text, line) for line in range(3)))
        self.assertEqual((None, None, 'alt', None), tuple(info[line].get('pad') for line in range(4)))
        self.assertNotIn('chords', info[2])
        lines = (Mi.line(DRUMS_RUMBLE_BASS, throw=True), Mi.line(DRUMS_RUMBLE_BASS, vox='call', throw=True),
                 Mi.line(DRUMS_RUMBLE_BASS, vox='call'), Mi.line(Mi.GROOVE))
        R, _, text = self.mix('FUT04', lines, 'vox')
        info = R['sections'][0]['lines']
        self.assertEqual([(1, None), (1, 'call'), (None, 'call')],
                         [(info[line].get('throw'), info[line].get('vox')) for line in range(3)])
        self.assertEqual({int}, {type(info[line]['throw']) for line in range(2)})      # 1, not true
        self.assertIn("the vox's last note thrown", self.said(text, 0))
        self.assertNotIn('vox (', self.said(text, 0))
        self.assertIn('vox (call), thrown', self.said(text, 1))
        self.assertNotIn('thrown', self.said(text, 2))
        R, _, text = self.mix('FUT04', lines, 'novox')
        self.assertFalse([ln for ln in R['sections'][0]['lines'] if {'throw', 'vox'} & set(ln)])
        self.assertNotIn('thrown', text)


    def test_a_roll_counts_the_bars_it_plays(self):
        """regions.json's roll_from and roll_bars and the README give the bars the roll plays: up to the fill's start,
        so not the fill bar where the fill starts on its downbeat (FUT04's stutter fill, FUT02's laser fill), but where
        it starts later in the bar (FUT01's glitch fill from 16th 12), and the last bar on a line with no fill."""
        roll = dict(lane='snare', bars=2, every=((2, 2),), last=1)
        for kit, lines, want in (
                ('FUT04', (Mi.line(DRUMS_RUMBLE_BASS, end='fill', roll=roll),), (6, 7, 2, 'bars 6-7')),
                ('FUT01', (Mi.line(Mi.GROOVE, end='fill', roll=roll),), (6, 8, 3, 'bars 6-8')),
                ('FUT02', (Mi.line(Mi.GROOVE, roll=dict(roll, bars=0, every=())),), (8, 8, 1, 'bar 8'))):
            R, tracks, text = self.mix(kit, lines + (Mi.line(Mi.GROOVE),) * 3, 'vox')
            info = R['sections'][0]['lines'][0]
            hits = {on // BAR + 1 for on, *_ in lane_ons(kit, tracks, 'snare', 0, 8 * BAR)
                    if on >= (want[0] - 1) * BAR}
            with self.subTest(kit=kit):
                self.assertEqual(want[:3], (info['roll_from'], hits and max(hits), info['roll_bars']))
                self.assertEqual(set(range(want[0], want[1] + 1)), hits)
                self.assertIn(f"rolls over {want[3]}", self.said(text, 0))

    def test_a_rise_takes_its_lanes_tracks(self):
        """rise=(lane, from, to): a lane name rises every track the lane's part plays on ('chords': Chords and Chord
        voicing; 'bass': the bass line's track), an exact track name that track ('Chords' with its voicing too); a name
        that is neither is refused, and so is a rise on the vox where its velocity picks the vowel (FUT02, FUT04)."""
        k = Mi.kit_of('FUT01')
        rest = (Mi.line(Mi.GROOVE),) * 3
        for rise, risen in ((('chords', 0.4, 1.0), {'Chords', 'Chord voicing'}),
                            (('Chords', 0.4, 1.0), {'Chords', 'Chord voicing'}),
                            (('bass', 0.4, 1.0), {'Bass'}), (('hats', 0.4, 1.0), {'Hats'})):
            _, flat, _ = self.mix('FUT01', (Mi.line(Mi.GROOVE),) + rest, 'vox')
            _, tracks, _ = self.mix('FUT01', (Mi.line(Mi.GROOVE, rise=rise),) + rest, 'vox')
            with self.subTest(rise=rise[0]):
                changed = {name for name in tracks if self.notes(tracks, name, 0) != self.notes(flat, name, 0)}
                self.assertEqual(risen, changed)
                for name in risen:          # bar 1 at 0.4 of its velocity, the last bar as written
                    first = [n for n in self.notes(tracks, name, 0) if n[0] < BAR]
                    was = [n for n in self.notes(flat, name, 0) if n[0] < BAR]
                    self.assertTrue(all(a[4] < b[4] for a, b in zip(first, was) if b[4] > 3))
        for kit, rise in (('FUT01', ('nonsense', 0.4, 1.0)), ('FUT02', ('vox', 0.4, 1.0)),
                          ('FUT04', ('Vox', 0.4, 1.0))):
            with self.subTest(kit=kit, rise=rise[0]), self.assertRaises(ValueError):
                self.mix(kit, (Mi.line(Mi.EVERYTHING, vox='loop', rise=rise),) + rest, 'vox')

    def test_a_chords_phrase_writes_its_voicing(self):
        """A phrase on Chords (FUT01's e-piano stab rhythm) with a voicing (PARTS['phrases'][i]['voicing']) plays it on
        Chord voicing (channel 3) wherever it plays, repeated every period; the no-vox accents' stab_drop takes both
        its trigger and its voicing on their 16ths (FUT01's 16th 30 of the accent blocks), and a rise on the chords
        reaches the voicing. The phrase's own file stays the trigger alone; with no voicing, Chord voicing rests."""
        k = Mi.kit_of('FUT01')
        stab = next(ph for ph in k.phrases if ph['name'] == 'E-piano stab rhythm')
        shape = (0, 3, 7, 10, 14)                   # m9 over the stab's zone, as the organ stabs are voiced
        voicing = [(n.step, n.key + d, n.length, n.vel, False) for n in Mi.as_notes(stab['notes']) for d in shape]
        voiced = dict(stab, voicing=voicing)
        phrases = [voiced if ph is stab else ph for ph in k.phrases]
        lines = (Mi.line(Mi.DRUMS_BASS, phrases=(stab['name'],), accents='loop'),
                 Mi.line(Mi.DRUMS_BASS, phrases=(stab['name'],), rise=('chords', 0.5, 1.0)),
                 Mi.line(Mi.GROOVE), Mi.line(Mi.GROOVE))
        period = stab.get('bars', 2) * 16
        for v in VARIANTS:
            drop = set(k.P['loop']['stab_drop'].get(v, ()))
            _, tracks, _ = self.mix('FUT01', lines, v, phrases=phrases)
            want = sorted({(at('FUT01', n[0] + period * r), n[1]) for r in range(8 * 16 // period) for n in voicing
                           if (n[0] + period * r) % 64 not in drop})
            got = self.notes(tracks, 'Chord voicing', 0)
            trig = self.notes(tracks, 'Chords', 0)
            with self.subTest(variant=v):
                self.assertEqual(want, sorted({(on, key) for on, _, ch, key, _ in got}))
                self.assertEqual({3}, {ch for _, _, ch, _, _ in got})
                self.assertEqual({on for on, *_ in got}, {on for on, *_ in trig})
                risen = self.notes(tracks, 'Chord voicing', 1)
                self.assertLess(min(n[4] for n in risen if n[0] < BAR), max(n[4] for n in risen if n[0] >= 7 * BAR))
        _, tracks, _ = self.mix('FUT01', lines, 'vox')
        self.assertFalse(self.notes(tracks, 'Chord voicing', 0))
        files = Mi.files_of(Mi.Kit('FUT01', k.K, {**k.P, 'phrases': phrases}, items=k.items))
        self.assertEqual(kit_files('FUT01')[Mi.phrase_file('FUT01', stab)], files[Mi.phrase_file('FUT01', stab)])


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
        """round(v x 127), unless it would play another layer than v: 0.751 is nearer the 1.0 layer of (0.5, 1.0) but
        95 is nearer 64 than 127, so it is written 96; FUT01's 0.9 kicks, halfway between 0.8 and 1.0 (the recipe plays
        the louder), 115; the 0.3 ghost claps, which the recipe plays on 0.2, 37 (38 is as near 25 as 51, and the
        engine takes the louder)."""
        k = self.kit()
        self.assertEqual(96, k.velocity(10, 36, 0.751))
        self.assertEqual(64, k.velocity(10, 36, 0.5))
        self.assertEqual(127, k.velocity(10, 36, 1.0))
        self.assertEqual(115, Mi.written_velocity((0.2, 0.4, 0.6, 0.8, 1.0), 0.9))
        self.assertEqual(37, Mi.written_velocity((0.2, 0.4, 0.6, 0.8, 1.0), 0.3))   # 38 ties 25 and 51: 0.4's
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
