"""The kit manifests (tools/sound/plugin_kit.py): content/plugin/<kit>/<KIT>_kit.json, the contract in
docs/plugin/manifest.md section 2, exported from the r5 Items, build/<kit>-r5/qa_report.json, the WAVs, the MIDI v2
PARTS (tools/sound/midi.py) and the phrase tables (phrase_tables.py).

The committed manifests are the source of truth once exported: build/ isn't in git and CI has no samples, so the
exporter takes each file's facts (frames, channels, SHA-256, loop points) from the WAVs and the round 5 report where
they are here, and from the committed manifest where they aren't. The tests that read the WAVs or the report skip
without them: $WUBANSIA_CONTENT_DIR (a folder of <kit>/ folders, or the checkout's content/samples with git lfs
pulled) and $WUBANSIA_BUILD_DIR (default: the checkout's build/).
"""
import json
import os
import unittest
from pathlib import Path

from tools.sound import channels as CH
from tools.sound import kits, listen
from tools.sound import midi as Mi
from tools.sound import phrase_tables as PT
from tools.sound import plugin_common as PC
from tools.sound import plugin_kit as PK

REPO = Path(__file__).resolve().parents[3]
KITS = ('FUT01', 'FUT02', 'FUT03', 'FUT04')
LANE_IDS = list(CH.LANES)


def strict(text):
    """JSON as core's gen::json reads kit.json: integers only, no duplicate keys, no null."""
    def pairs(items):
        keys = [k for k, _ in items]
        if len(keys) != len(set(keys)):
            raise ValueError(f'a duplicate key in {keys}')
        return dict(items)

    def no_float(s):
        raise ValueError(f'a float: {s}')

    def no_constant(s):
        raise ValueError(f'a constant: {s}')

    out = json.loads(text, object_pairs_hook=pairs, parse_float=no_float, parse_constant=no_constant)

    def walk(x, path):
        if x is None:
            raise ValueError(f'null at {path}')
        if isinstance(x, dict):
            for k, v in x.items():
                walk(v, f'{path}.{k}')
        elif isinstance(x, list):
            for i, v in enumerate(x):
                walk(v, f'{path}[{i}]')
    walk(out, '')
    return out


def committed(kit):
    p = PK.manifest_path(kit)
    return strict(p.read_text(encoding='utf-8')) if p.exists() else None


def manifest(kit):
    m = committed(kit)
    if m is None:
        raise unittest.SkipTest(f'{PK.manifest_path(kit)} is not exported yet')
    return m


def r5_items(kit):
    return {it.name: it for it in kits.items(kits.load(kit), sets=['r5'])}


def instrument(m, iid):
    return next(i for i in m['instruments'] if i['id'] == iid)


def resolve(m, ch, key, ab=None):
    """The instrument (ch, key) plays, on the A side or the B side of its switch: (instrument, zone) or None."""
    for e in m['keymap']:
        if e['ch'] == ch and e['keys'][0] <= key <= e['keys'][1]:
            iid = e['b'] if ab == 'b' and e['b'] else e['a']
            ins = instrument(m, iid)
            zone = next((z for z in ins['zones'] if z['lo'] <= key <= z['hi']), None)
            return ins, zone
    return None


class TestContract(unittest.TestCase):
    """docs/plugin/manifest.md, field by field."""

    def test_top_level_and_kit(self):
        for kit in KITS:
            m = manifest(kit)
            K = kits.load(kit)
            with self.subTest(kit=kit):
                self.assertEqual(['kit', 'files', 'instruments', 'keymap', 'extras', 'phrases'], list(m))
                k = m['kit']
                self.assertEqual(['id', 'title', 'direction', 'bpm', 'key', 'camelot', 'swing_pm', 'content_version',
                                  'lanes', 'ab'], list(k))
                self.assertEqual((kit, K.TITLE, K.DIRECTION, int(K.BPM), K.KEY, K.CAMELOT),
                                 (k['id'], k['title'], k['direction'], k['bpm'], k['key'], k['camelot']))
                self.assertEqual(PK.iround(K.SWING * 1000), k['swing_pm'])
                self.assertEqual(PK.CONTENT_VERSION, k['content_version'])
                self.assertEqual(LANE_IDS, list(k['lanes']))
                self.assertEqual(['kick', 'bass', 'stab'], list(k['ab']))
                for sw in k['ab'].values():
                    self.assertEqual(['a', 'b'], list(sw))
                    self.assertEqual(bool(sw['a']), bool(sw['b']))

    def test_files(self):
        for kit in KITS:
            m = manifest(kit)
            paths = [f['path'] for f in m['files']]
            with self.subTest(kit=kit):
                self.assertEqual(len(paths), len(set(paths)))
                for f in m['files']:
                    self.assertEqual(['path', 'frames', 'channels', 'sha256', 'loop_start', 'loop_end',
                                      'preroll_frames'], list(f))
                    self.assertRegex(f['path'], r'^(Drums|Bass|Music|Vox|FX|Loops|Stems)/[^/]+\.wav$')
                    self.assertTrue(1 <= f['frames'] <= 28_800_000)
                    self.assertIn(f['channels'], (1, 2))
                    self.assertRegex(f['sha256'], r'^[0-9a-f]{64}$')
                    if f['loop_end']:
                        self.assertTrue(0 <= f['loop_start'] < f['loop_end'] <= f['frames'])
                    else:
                        self.assertEqual(0, f['loop_start'])
                    self.assertTrue(0 <= f['preroll_frames'] <= 48000)

    def test_every_file_is_played_and_every_reference_resolves(self):
        for kit in KITS:
            m = manifest(kit)
            n = len(m['files'])
            used = set()
            for ins in m['instruments']:
                for z in ins['zones']:
                    for layer in z['layers']:
                        for f in layer['rr']:
                            self.assertTrue(0 <= f < n)
                            used.add(f)
            for ph in m['phrases']:
                for f in (ph['stems']['a'], ph['stems']['b']):
                    if f >= 0:
                        used.add(f)
            self.assertEqual(set(range(n)), used, kit)

    def test_instruments(self):
        keys = ['id', 'name', 'lane', 'mix_channel', 'mode', 'align', 'attack_us', 'release_us', 'gain_mdb',
                'start_frames', 'velocity_law', 'interp', 'mono', 'glide', 'restrike_continues', 'choke', 'duck_key',
                'pan_cycle_pm', 'rr_block_16ths', 'layered_with', 'zones']
        for kit in KITS:
            m = manifest(kit)
            ids = [i['id'] for i in m['instruments']]
            with self.subTest(kit=kit):
                self.assertEqual(len(ids), len(set(ids)))
            for ins in m['instruments']:
                with self.subTest(kit=kit, instrument=ins['id']):
                    self.assertEqual(keys, list(ins))
                    self.assertRegex(ins['id'], r'^[a-z0-9_]{1,32}$')
                    self.assertIn(ins['lane'], LANE_IDS)
                    self.assertEqual(PK.mix_channels(kit)[ins['mix_channel']], ins['lane'])
                    self.assertIn(ins['mode'], ('one_shot', 'gated', 'sustain_loop'))
                    self.assertIn(ins['align'], ('start', 'end'))
                    self.assertIn(ins['velocity_law'], ('layer_only', 'pow15'))
                    self.assertIn(ins['interp'], ('sinc', 'hermite'))
                    self.assertEqual(['curve', 'us', 'dip_us'], list(ins['glide']))
                    self.assertEqual(['group', 'role', 'fade_us', 'curve'], list(ins['choke']))
                    self.assertTrue(ins['glide']['curve'] == 'none' or ins['mono'])
                    self.assertEqual(ins['choke']['group'] == '', ins['choke']['role'] == 'none')
                    self.assertTrue(1 <= len(ins['zones']) <= 32)
                    roots = [z['root'] for z in ins['zones']]
                    self.assertEqual(sorted(roots), roots)
                    for a, b in zip(ins['zones'], ins['zones'][1:]):
                        self.assertLess(a['hi'], b['lo'])
                    for z in ins['zones']:
                        self.assertEqual(['root', 'lo', 'hi', 'layers'], list(z))
                        self.assertTrue(0 <= z['lo'] <= z['hi'] <= 127)
                        vels = [layer['vel'] for layer in z['layers']]
                        self.assertEqual(sorted(set(vels)), vels)
                        for layer in z['layers']:
                            self.assertEqual(['vel', 'rr'], list(layer))
                            self.assertTrue(1 <= len(layer['rr']) <= 8)
                        chans = {m['files'][f]['channels'] for layer in z['layers'] for f in layer['rr']}
                        self.assertEqual(1, len(chans))
                        prerolls = {m['files'][f]['preroll_frames'] for layer in z['layers'] for f in layer['rr']}
                        self.assertEqual(1, len(prerolls))
                    if ins['mode'] == 'sustain_loop':
                        for z in ins['zones']:
                            for layer in z['layers']:
                                for f in layer['rr']:
                                    self.assertGreater(m['files'][f]['loop_end'], 0)
                    if ins['layered_with']:
                        other = instrument(m, ins['layered_with'])
                        self.assertNotEqual(ins['id'], other['id'])
                        self.assertEqual('', other['layered_with'])
                    self.assertTrue(len(ins['pan_cycle_pm']) <= 16)
                    self.assertTrue(all(-1000 <= p <= 1000 for p in ins['pan_cycle_pm']))

    def test_keymap(self):
        """Ranges on one channel don't overlap; no channel 3 or 9; drums within 35-59; every instrument is reached;
        ab is set exactly where there is a B."""
        for kit in KITS:
            m = manifest(kit)
            ids = {i['id'] for i in m['instruments']}
            reached = set()
            by_ch = {}
            for e in m['keymap']:
                with self.subTest(kit=kit, entry=e):
                    self.assertEqual(['ch', 'keys', 'a', 'b', 'ab'], list(e))
                    self.assertNotIn(e['ch'], (3, 9))
                    lo, hi = e['keys']
                    self.assertTrue(0 <= lo <= hi <= 127)
                    if e['ch'] == 10:
                        self.assertTrue(35 <= lo and hi <= 59)
                    self.assertIn(e['a'], ids)
                    self.assertEqual(e['b'] == '', e['ab'] == '')
                    self.assertIn(e['ab'], ('', 'kick', 'bass', 'stab'))
                    reached |= {e['a'], e['b']} - {''}
                    by_ch.setdefault(e['ch'], []).append((lo, hi))
            for ch, ranges in by_ch.items():
                ranges.sort()
                for a, b in zip(ranges, ranges[1:]):
                    self.assertLess(a[1], b[0], (kit, ch))
            layered = {i['layered_with'] for i in m['instruments']} - {''}
            self.assertEqual(ids, reached | layered, kit)

    def test_extras(self):
        for kit in KITS:
            m = manifest(kit)
            self.assertEqual([{'ch': ch, 'name': n, 'lane': lane} for ch, n, lane in CH.extras(kit)], m['extras'])
            for e in m['keymap']:
                if e['ch'] >= 11:
                    lane = next(x['lane'] for x in m['extras'] if x['ch'] == e['ch'])
                    for iid in (e['a'], e['b']):
                        if iid:
                            self.assertEqual(lane, instrument(m, iid)['lane'], (kit, e))

    def test_phrases_are_the_phrase_tables(self):
        """kit.json's phrases are phrase_tables' tables, their stems indexed into files (a Loops/ stem its cycle
        long, a derived one its own length), their mix channels on their lanes."""
        for kit in KITS:
            m = manifest(kit)
            K = kits.module(kit)
            index = {f['path']: i for i, f in enumerate(m['files'])}
            frames = {f['path']: f['frames'] for f in m['files']}
            self.assertEqual([PT.table(s, index, frames) for s in PT.specs(kit)], m['phrases'], kit)
            for s, ph in zip(PT.specs(kit), m['phrases']):
                self.assertEqual(PK.mix_channels(kit)[ph['mix_channel']], ph['lane'])
                for f in (ph['stems']['a'], ph['stems']['b']):
                    if f >= 0:
                        self.assertEqual(s.frames if s.derived else PT.cycle_frames(K, s.cycle),
                                         m['files'][f]['frames'], (kit, s.line))

    def test_lanes_and_switches(self):
        """Every lane a kit plays has a label; A/B pairs exist exactly where they should: kick and stab on FUT01
        only, bass on all four kits."""
        for kit in KITS:
            m = manifest(kit)
            played = {i['lane'] for i in m['instruments']}
            for lane, label in m['kit']['lanes'].items():
                self.assertEqual(lane in played, bool(label), (kit, lane))
            ab = {k: bool(v['a']) for k, v in m['kit']['ab'].items()}
            self.assertEqual({'kick': kit == 'FUT01', 'bass': True, 'stab': kit == 'FUT01'}, ab, kit)
            switches = {e['ab'] for e in m['keymap']} - {''}
            self.assertEqual({k for k, v in ab.items() if v}, switches, kit)
        m = manifest('FUT01')
        self.assertEqual({'a': 'Kick Split', 'b': 'Kick Garage'}, m['kit']['ab']['kick'])
        self.assertEqual({'a': 'Organ Stab', 'b': 'E Piano Stab'}, m['kit']['ab']['stab'])


class TestItems(unittest.TestCase):
    """The instruments are the r5 Items as the recipes play them."""

    def test_layers_round_robins_and_zones_equal_the_items(self):
        """Layer velocities round(layer x 127), round-robin counts and zone roots equal the r5 Items; a pitched
        instrument's zones tile 0-127 (each key the highest root at or below it, the lowest zone extending down); a
        one-key instrument's zones sit on their keys."""
        for kit in KITS:
            m = manifest(kit)
            items = r5_items(kit)
            for ins in m['instruments']:
                it = items[PK.item_of(kit, ins['id'])]
                with self.subTest(kit=kit, instrument=ins['id']):
                    for z in ins['zones']:
                        self.assertEqual([PK.iround(v * 127) for v in it.layers],
                                         [layer['vel'] for layer in z['layers']])
                        self.assertEqual({it.rr}, {len(layer['rr']) for layer in z['layers']})
                    keyed = PK.one_key(kit, ins['id'])
                    if keyed:
                        self.assertEqual([(k, k, k) for k in sorted(keyed)],
                                         [(z['root'], z['lo'], z['hi']) for z in ins['zones']])
                    else:
                        want = list(it.notes) or [it.root if it.root is not None else 60]
                        self.assertEqual(want, [z['root'] for z in ins['zones']])
                        self.assertEqual(0, ins['zones'][0]['lo'])
                        self.assertEqual(127, ins['zones'][-1]['hi'])
                        for z in ins['zones'][1:]:
                            self.assertEqual(z['root'], z['lo'])
                        for a, b in zip(ins['zones'], ins['zones'][1:]):
                            self.assertEqual(a['hi'] + 1, b['lo'])

    def test_chord_instruments_carry_their_recipe_voicing(self):
        """Chords as chords (ADR 0027, amended): each chord sample's zone carries the chord its recipe voices, as
        semitones over the zone's root (the root first, 0), so For my synths can write a note as its chord. Exactly these
        ten instruments, thirteen zones; no other zone has a voicing."""
        from tools.sound.recipes.fut01 import keys as K1, music as M1
        from tools.sound.recipes.fut02 import accents as A2, pad as P2
        from tools.sound.recipes.fut03 import chords as C3
        from tools.sound.recipes.fut04 import accents as A4, pad as P4
        recipes = {   # (kit, instrument): {zone root: the chord's keys}, the constants each recipe renders
            ('FUT01', 'organ_stab'): {z: K1.chord(z) for z in K1.ZONES},          # keys.SHAPE over F#3 and B3
            ('FUT01', 'e_piano_stab'): {z: K1.chord(z) for z in K1.ZONES},
            ('FUT01', 'air_pad_fsm9'): {M1.PAD_CHORD[0]: M1.PAD_CHORD},
            ('FUT02', 'starfield_pad_dm9'): {P2.CHORD[0]: P2.CHORD},
            ('FUT02', 'laser_chord_stab'): dict(A2.STAB_CHORDS),                  # B-flat maj9 on A#3, Dm9 on D4
            ('FUT03', 'dub_stab_gm9'): {C3.CHORD[0]: C3.CHORD},
            ('FUT03', 'pad_warm'): {C3.CHORD[0]: C3.CHORD},
            ('FUT04', 'drone_em_add_9'): {P4.CHORD[0]: P4.CHORD},
            ('FUT04', 'accent_metal_chord_em'): {A4.EM[0]: A4.EM},
            ('FUT04', 'accent_rave_stab_em'): {A4.EM[0]: A4.EM},
        }
        want = {key: {root: [k - root for k in sorted(chord)] for root, chord in zones.items()}
                for key, zones in recipes.items()}
        found = {}
        for kit in KITS:
            for ins in manifest(kit)['instruments']:
                for z in ins['zones']:
                    if 'voicing' in z:
                        found.setdefault((kit, ins['id']), {})[z['root']] = z['voicing']
        self.assertEqual(want, found)
        self.assertEqual(10, len(found))
        self.assertEqual(13, sum(len(zones) for zones in found.values()))
        for zones in want.values():
            for voicing in zones.values():
                self.assertEqual(0, voicing[0])                              # the zone's root plays the chord's root
                self.assertEqual(sorted(set(voicing)), voicing)
                self.assertTrue(len(voicing) <= 8 and voicing[-1] <= 48)     # what the engine's parser takes

    def test_files_are_the_items_takes(self):
        """Each zone's layers and round robins name the WAVs of the Item's takes (listen.plain), by note, layer and
        round robin; one-key zones name their zone's (the drum zone or the key's)."""
        for kit in KITS:
            m = manifest(kit)
            items = r5_items(kit)
            for ins in m['instruments']:
                it = items[PK.item_of(kit, ins['id'])]
                lay, rrs = len(it.layers) > 1, it.rr > 1
                notes = PK.zone_notes(kit, ins['id'])
                for z, note in zip(ins['zones'], notes):
                    for li, layer in enumerate(z['layers']):
                        want = []
                        for r in range(1, it.rr + 1):
                            take = next(t for t in it.takes(kit) if (t.note, t.layer, t.rr) == (note, li + 1, r))
                            want.append(f'{listen.FOLDERS[it.group]}/{listen.plain(take.file, lay, rrs)}')
                        self.assertEqual(want, [m['files'][f]['path'] for f in layer['rr']], (kit, ins['id']))

    def test_loop_points_equal_the_report(self):
        """Every file of a sustain-loop instrument loops where qa_report.json says; every other file doesn't."""
        for kit in KITS:
            report = PK.qa_report(kit)
            if report is None:
                self.skipTest(f'build/{kit.lower()}-r5/qa_report.json is not here (set WUBANSIA_BUILD_DIR)')
            loops = PK.report_loops(report)
            m = manifest(kit)
            looped = {f for ins in m['instruments'] if ins['mode'] == 'sustain_loop'
                      for z in ins['zones'] for layer in z['layers'] for f in layer['rr']}
            for i, f in enumerate(m['files']):
                with self.subTest(kit=kit, file=f['path']):
                    if i in looped:
                        self.assertEqual(list(loops[f['path']]), [f['loop_start'], f['loop_end']])
                    else:
                        self.assertEqual([0, 0], [f['loop_start'], f['loop_end']])

    def test_the_samples_are_as_listed(self):
        """With the samples pulled, every listed file exists at 48 kHz with the listed frames, channels and SHA-256.
        An LFS pointer file skips the check and never passes it."""
        import hashlib
        import soundfile as sf
        checked = 0
        for kit in KITS:
            m = manifest(kit)
            for f in m['files']:
                p = PK.find_sample(kit, f['path'])
                if p is None:
                    continue
                with self.subTest(kit=kit, file=f['path']):
                    self.assertFalse(PK.is_lfs_pointer(p))
                    info = sf.info(p)
                    self.assertEqual((48000, f['frames'], f['channels']), (info.samplerate, info.frames, info.channels))
                    self.assertEqual(f['sha256'], hashlib.sha256(p.read_bytes()).hexdigest())
                    checked += 1
        if not checked:
            self.skipTest('no samples here (set WUBANSIA_CONTENT_DIR or git lfs pull)')

    def test_lfs_pointers_never_pass(self):
        import tempfile
        with tempfile.TemporaryDirectory() as d:
            p = Path(d) / 'x.wav'
            p.write_text('version https://git-lfs.github.com/spec/v1\noid sha256:' + '0' * 64 + '\nsize 1\n')
            self.assertTrue(PK.is_lfs_pointer(p))
            self.assertIsNone(PK.real_wav(p))


class TestKeymap(unittest.TestCase):
    """The keymap against MIDI v2's PARTS and the committed MIDI files."""

    def test_every_note_in_every_midi_file_resolves(self):
        """Every note of every embedded content/midi file (every kit-prefixed .mid) resolves through the keymap,
        channel 3 excepted, on both sides of every switch."""
        for kit in KITS:
            m = manifest(kit)
            folder = REPO / 'content' / 'midi' / kit.lower()
            files = sorted(p for p in folder.rglob(f'{kit}_*.mid'))
            self.assertTrue(files, kit)
            for path in files:
                for tick, ch, key, vel in PT.smf_notes(path.read_bytes()):
                    if ch == 3:
                        continue
                    for side in ('a', 'b'):
                        got = resolve(m, ch, key, side)
                        with self.subTest(kit=kit, file=path.name, ch=ch, key=key):
                            self.assertIsNotNone(got)
                            self.assertIsNotNone(got[1])

    def test_the_keymap_is_the_parts(self):
        """Channel by channel, the instruments are MIDI v2's: the pitched channels' Items (A first), the drum map and
        its zones, one key per sound from 48 on channels 6-8."""
        for kit in KITS:
            m = manifest(kit)
            k = Mi.kit_of(kit)
            for ch, names in k.channel_items().items():
                got = resolve(m, ch, k.zones(ch)[0], 'a')[0]
                self.assertEqual(names[0], PK.item_of(kit, got['id']), (kit, ch))
                if len(names) > 1:
                    self.assertEqual(names[1], PK.item_of(kit, resolve(m, ch, k.zones(ch)[0], 'b')[0]['id']))
            for note, name in k.drum_map.items():
                ins, zone = resolve(m, 10, note, 'b')
                self.assertEqual(name, PK.item_of(kit, ins['id']), (kit, note))
            for role, ch in (('accents', 6), ('fx', 7), ('chops', 8)):
                for key, sound in k.keymap(role).items():
                    ins, zone = resolve(m, ch, key)
                    self.assertEqual(sound.item, PK.item_of(kit, ins['id']), (kit, role, key))
                    self.assertEqual(sound.start == 0, ins['start_frames'] == 0, (kit, role, key))

    def test_named_cases(self):
        """FUT03's F1 (29) plays the G1 zone pitched down; FUT01's Low line plays Organ Bass on channel 11; FUT02's
        Drop Glide follows the bass switch; FUT04's rumble is drum note 35 on the kick lane, no duck key; FUT01's kick
        36 is A Split, B Garage; FUT03's congas and bongos play their zones unpitched on their drum notes."""
        m = manifest('FUT03')
        ins, zone = resolve(m, 1, 29, 'a')
        self.assertEqual((31, 0), (zone['root'], zone['lo']))
        self.assertEqual('RubberBassClean', PK.item_of('FUT03', ins['id']))
        for note, root in ((41, 41), (43, 43), (45, 45), (47, 47), (48, 48)):
            ins, zone = resolve(m, 10, note)
            self.assertEqual((root, root, root), (zone['root'], zone['lo'], zone['hi']))
        m = manifest('FUT01')
        low = [n.key for n in Mi.as_notes(Mi.kit_of('FUT01').P['bass']['lines']['Low'])]
        for key in low:
            ins, zone = resolve(m, 11, key)
            self.assertEqual('OrganBass', PK.item_of('FUT01', ins['id']))
            self.assertEqual('bass', ins['lane'])
        e = next(e for e in m['keymap'] if e['ch'] == 10 and e['keys'] == [36, 36])
        self.assertEqual(('kick_split', 'kick_garage', 'kick'), (e['a'], e['b'], e['ab']))
        m = manifest('FUT02')
        e = next(e for e in m['keymap'] if e['ch'] == 11)
        self.assertEqual(('drop_glide_clean', 'drop_glide_warm', 'bass'), (e['a'], e['b'], e['ab']))
        m = manifest('FUT04')
        ins, _ = resolve(m, 10, 35)
        self.assertEqual(('rumble', 'kick', False, 'sustain_loop'),
                         (ins['id'], ins['lane'], ins['duck_key'], ins['mode']))

    def test_the_multisamples_play_at_their_stems_level(self):
        """Off the kit's tempo, with phrases off and from MIDI in, a phrase channel's notes play its multisample: each
        of those instruments is set to its stem's level (gain_mdb), and plays on a channel with a corrective EQ in
        front (P3's <channel>_ms: the stem's channel with the EQ) where the multisample's spectrum differs from the
        stem's. Measured with groovecore render, phrases off against phrases auto, pass 2 (content/plugin/README.md).
        FUT03's Rubber Basses play alone: Sub Deep under them put 2 dB too much under 120 Hz and 3 dB too little over
        it against their lines."""
        for kit, want in MULTISAMPLES.items():
            m = manifest(kit)
            channels = {p['ch'] for p in m['phrases']}
            got = {}
            for e in m['keymap']:
                for side in ('a', 'b'):
                    if e['ch'] in channels and e[side]:
                        ins = instrument(m, e[side])
                        got[ins['id']] = (ins['gain_mdb'], ins['mix_channel'])
            self.assertEqual(want, got, kit)
            for ins in m['instruments']:
                self.assertEqual(PK.mix_channels(kit)[ins['mix_channel']], ins['lane'], (kit, ins['id']))
        m = manifest('FUT03')
        self.assertNotIn('sub_deep', {i['id'] for i in m['instruments']})
        self.assertEqual({''}, {i['layered_with'] for i in m['instruments']})

    def test_the_duck_keys(self):
        """The notes that key the onset ducks (mix.json): the kicks and claps (not FUT04's rumble), and FUT02's vox
        synth on channel 5, whose riff keys the arp's pocket (context5.ARP_POCKET: two onset ducks on the vox lane,
        100 ms early); its vox chops, on the same lane, don't. The timeline builds the duck keys from the notes before
        it matches phrases (engine/sched/timeline.h), so the riff keys the pocket whether its notes play the phrase
        lane's stem or the multisample: a phrase has no key of its own."""
        for kit in KITS:
            m = manifest(kit)
            drums = {i['id'] for i in m['instruments'] if any(e['ch'] == 10 and i['id'] in (e['a'], e['b'])
                                                               for e in m['keymap'])}
            for ins in m['instruments']:
                if ins['id'] in drums:
                    want = ins['lane'] in ('kick', 'snare') and (kit, ins['id']) != ('FUT04', 'rumble')
                else:
                    want = (kit, ins['id']) == ('FUT02', 'vox_synth')
                self.assertEqual(want, ins['duck_key'], (kit, ins['id']))
        ins, _ = resolve(manifest('FUT02'), 5, 60)
        self.assertEqual(('vox_synth', 'vox', True), (ins['id'], ins['lane'], ins['duck_key']))
        self.assertNotIn('duck_key', PC.KEYS['phrase'])

    def test_how_the_recipes_play(self):
        """The playing rules the loops were rendered with: chokes (FUT01 5 ms linear, FUT02-04 4 ms cos2, FUT04's ride
        outside), glides (FUT01 raised cosine 70/30 ms, FUT02 exponential 18 ms, FUT03 and FUT04 raised cosine 50 ms:
        FUT03's in-note glides, which MIDI v2 writes as legato notes, take 40-70 ms in rubber.LINES), Hermite
        on FUT01's bass and leads, FUT02's zap pans, end-aligned cut_end sounds, FUT01's glitch fill from its beat
        2, the vox pre-rolls."""
        want_choke = {'FUT01': (5000, 'linear'), 'FUT02': (4000, 'cos2'), 'FUT03': (4000, 'cos2'),
                      'FUT04': (4000, 'cos2')}
        for kit in KITS:
            m = manifest(kit)
            items = r5_items(kit)
            closed, opened = resolve(m, 10, 42)[0], resolve(m, 10, 46)[0]
            self.assertEqual(('hats', 'chokes') + want_choke[kit],
                             tuple(closed['choke'][k] for k in ('group', 'role', 'fade_us', 'curve')))
            self.assertEqual(('hats', 'choked') + want_choke[kit],
                             tuple(opened['choke'][k] for k in ('group', 'role', 'fade_us', 'curve')))
            for ins in m['instruments']:
                it = items[PK.item_of(kit, ins['id'])]
                self.assertEqual('end' if 'cut_end' in it.flags else 'start', ins['align'], (kit, ins['id']))
        self.assertEqual('none', resolve(manifest('FUT04'), 10, 51)[0]['choke']['role'])
        glides = {'FUT01': ('raised_cosine', 70000, 30000), 'FUT02': ('exponential', 18000, 0),
                  'FUT03': ('raised_cosine', 50000, 0), 'FUT04': ('raised_cosine', 50000, 0)}
        for kit, g in glides.items():
            bass = resolve(manifest(kit), 1, 36, 'a')[0]
            self.assertEqual(g, tuple(bass['glide'].values()), kit)
            self.assertTrue(bass['mono'])
            self.assertEqual('sustain_loop', bass['mode'])
        m = manifest('FUT01')
        for ch in (1, 2, 11, 12):
            self.assertEqual('hermite', resolve(m, ch, 60 if ch in (2, 12) else 36)[0]['interp'], ch)
        zap = resolve(manifest('FUT02'), 10, 47)[0]
        self.assertEqual([-550, 550, -550, 350], zap['pan_cycle_pm'])
        fill = resolve(m, 7, Mi.kit_of('FUT01').key_of('fx', 'GlitchFillLift', None, 4))[0]
        self.assertEqual((21493, 2000), (fill['start_frames'], fill['attack_us']))
        vox = {'FUT01': 288, 'FUT02': 960, 'FUT03': 1200, 'FUT04': 0}
        for kit, pre in vox.items():
            m = manifest(kit)
            ins, zone = resolve(m, 5, zone_root(m, 5))
            self.assertEqual(pre, m['files'][zone['layers'][0]['rr'][0]]['preroll_frames'], kit)


# Each phrase channel's multisample instruments: (gain_mdb, mix_channel). The _ms channels carry P3's corrective EQ.
MULTISAMPLES = {
    'FUT01': {'chrome_pluck': (600, 'pluck'), 'vox_synth': (7550, 'vsynth_ms')},
    'FUT02': {'roll_bass_clean': (-900, 'bass_ms'), 'roll_bass_warm': (4160, 'bass_ms'), 'chrome_arp': (130, 'arp'),
              'vox_synth': (1850, 'vox')},
    'FUT03': {'rubber_bass_clean': (1730, 'bass_ms'), 'rubber_bass_warm': (2240, 'bass_ms'),
              'vox_synth': (6120, 'vox_ms')},
    'FUT04': {'acid_clean': (0, 'bass'), 'acid_warm': (0, 'bass'), 'mod_blip': (-470, 'lead')},
}


def zone_root(m, ch):
    e = next(e for e in m['keymap'] if e['ch'] == ch)
    return instrument(m, e['a'])['zones'][0]['root']


class TestReadme(unittest.TestCase):
    def test_the_readme_lists_the_multisample_cycles_and_the_derived_stems(self):
        """content/plugin/README.md lists each kit's manifest (its size and counts), the extended mixes' cycles that
        play the multisample on a phrase lane (as plugin_kit.py --report prints them), and every derived stem with its
        size."""
        text = (REPO / 'content' / 'plugin' / 'README.md').read_text(encoding='utf-8')
        for kit in KITS:
            self.assertIn(PK.summary_row(kit), text)
            for line in PK.report(kit):
                self.assertIn(line, text)
            m = manifest(kit)
            for f in m['files']:
                if f['path'].startswith('Stems/'):
                    size = 44 + f['frames'] * f['channels'] * 3
                    self.assertIn(f"`{kit.lower()}/{f['path']}`", text)
                    self.assertIn(f'{size:,} bytes', text)


class TestExport(unittest.TestCase):
    def test_integers_and_strings_only(self):
        for kit in KITS:
            p = PK.manifest_path(kit)
            if not p.exists():
                self.skipTest('not exported yet')
            text = p.read_text(encoding='utf-8')
            strict(text)
            self.assertFalse(text.startswith('﻿'))
            self.assertTrue(text.endswith('\n') and '\r' not in text)
            self.assertEqual(text, PC.dumps(strict(text)), 'the canonical layout (manifest.md section 1)')

    def test_a_second_export_is_byte_identical_and_equals_the_committed_one(self):
        """plugin_common writes it (render (): the denylist check, then the canonical layout)."""
        for kit in KITS:
            a, b = PC.render(PK.export(kit)), PC.render(PK.export(kit))
            self.assertEqual(a, b, kit)
            p = PK.manifest_path(kit)
            if p.exists():
                self.assertEqual(p.read_text(encoding='utf-8'), a, f'{p} is stale: run plugin_kit.py --kit {kit}')
                self.assertTrue(PC.write(p, PK.export(kit), check=True), kit)

    def test_every_string_passes_the_denylist(self):
        """Every string, keys included, past the artists and slurs lists."""
        for kit in KITS:
            PC.check_strings(PK.export(kit))

    def test_every_object_has_the_contracts_keys_in_order(self):
        """Every object of every manifest has exactly its kind's keys, in the contract's order (plugin_common.KEYS)."""
        for kit in KITS:
            m = manifest(kit)

            def check(kind, obj):
                self.assertEqual(list(PC.KEYS[kind]), list(obj), (kit, kind))

            check('kit.json', m)
            check('kit', m['kit'])
            check('lanes', m['kit']['lanes'])
            check('ab', m['kit']['ab'])
            for labels in m['kit']['ab'].values():
                check('ab_labels', labels)
            for f in m['files']:
                check('file', f)
            for ins in m['instruments']:
                check('instrument', ins)
                check('glide', ins['glide'])
                check('choke', ins['choke'])
                for z in ins['zones']:
                    check('zone', z)
                    for layer in z['layers']:
                        check('layer', layer)
            for e in m['keymap']:
                check('keymap', e)
            for e in m['extras']:
                check('extra', e)
            for ph in m['phrases']:
                check('phrase', ph)
                check('stems', ph['stems'])
                check('ride', ph['ride'])
                for s in ph['slices']:
                    check('slice', s)

    def test_integer_units_round_half_up(self):
        self.assertIs(PC.iround, PK.iround)
        self.assertEqual((3, -2, -2500), (PK.iround(2.5), PK.iround(-2.5), PK.iround(-2.5, 1000)))


if __name__ == '__main__':
    unittest.main()
