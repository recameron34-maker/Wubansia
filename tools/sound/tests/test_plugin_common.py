"""tools/sound/plugin_common.py: the manifests' integer units, key order, canonical layout and denylist hook
(docs/plugin/manifest.md)."""
import json
import re
import tempfile
import unittest
from pathlib import Path

from tools.sound import denylist
from tools.sound import plugin_common as PC

REPO = Path(__file__).resolve().parents[3]
CONTRACT = REPO / 'docs' / 'plugin' / 'manifest.md'


class TestIntegerUnits(unittest.TestCase):
    def test_iround_rounds_halves_up(self):
        self.assertEqual(PC.iround(2.5), 3)
        self.assertEqual(PC.iround(-2.5), -2)
        self.assertEqual(PC.iround(-2.6), -3)
        self.assertEqual(PC.iround(0.59, 1000), 590)
        self.assertEqual(PC.iround(-0.0005, 1000), 0)
        self.assertEqual(PC.iround(7), 7)
        self.assertIsInstance(PC.iround(1.2), int)

    def test_iround_refuses_nan_and_infinities(self):
        for bad in (float('nan'), float('inf'), float('-inf')):
            with self.assertRaises(ValueError):
                PC.iround(bad)

    def test_the_units(self):
        self.assertEqual(PC.mdb(-2.5), -2500)
        self.assertEqual(PC.mdb(-1.02), -1020)
        self.assertEqual(PC.pm(0.707), 707)
        self.assertEqual(PC.ppm(0.0004), 400)
        self.assertEqual(PC.hz(449.6), 450)
        self.assertEqual(PC.mhz(46.25), 46250)
        self.assertEqual(PC.us(0.03), 30000)
        self.assertEqual(PC.ms(1.1), 1100)
        self.assertEqual(PC.frames(0.02), 960)
        self.assertEqual(PC.frames(1.0, 44100), 44100)
        self.assertEqual(PC.mlu(-9.73), -9730)
        self.assertEqual(PC.mrad(3.141592653589793), 3142)


class TestKeyOrder(unittest.TestCase):
    def test_ordered_puts_the_documented_keys_in_order(self):
        got = PC.ordered('glide', {'dip_us': 30000, 'curve': 'raised_cosine', 'us': 70000})
        self.assertEqual(list(got), ['curve', 'us', 'dip_us'])
        self.assertEqual(PC.ordered('band', lo_hz=0, hi_hz=120), {'lo_hz': 0, 'hi_hz': 120})

    def test_ordered_refuses_missing_and_unknown_keys(self):
        with self.assertRaisesRegex(KeyError, 'missing dip_us'):
            PC.ordered('glide', curve='none', us=0)
        with self.assertRaisesRegex(KeyError, 'unknown size'):
            PC.ordered('glide', curve='none', us=0, dip_us=0, size=1)
        with self.assertRaisesRegex(KeyError, 'no object kind'):
            PC.ordered('glider', {})

    def test_a_zones_voicing_is_optional_and_sits_before_its_layers(self):
        self.assertEqual(['root', 'lo', 'hi', 'layers'], list(PC.ordered('zone', layers=[], hi=58, lo=0, root=54)))
        chord = PC.ordered('zone', layers=[], voicing=[0, 7, 10, 14, 15], hi=58, lo=0, root=54)
        self.assertEqual(['root', 'lo', 'hi', 'voicing', 'layers'], list(chord))
        with self.assertRaisesRegex(KeyError, 'missing layers'):
            PC.ordered('zone', root=54, lo=0, hi=58, voicing=[0, 7])
        self.assertEqual({'zone': ('voicing',)}, PC.OPTIONAL)

    def test_an_insert_starts_with_its_type_and_ends_with_its_when(self):
        got = PC.insert('peak', q_pm=900, freq_hz=900, gain_mdb=-3500, when={'variant': 'novox', 'ab': 'none', 'is': 'a'})
        self.assertEqual(list(got), ['type', 'freq_hz', 'gain_mdb', 'q_pm', 'when'])
        self.assertEqual(list(got['when']), ['variant', 'ab', 'is'])
        self.assertNotIn('when', PC.insert('gain', gain_mdb=-2000))

    def test_a_duck_takes_its_modes_keys(self):
        band = PC.ordered('band', lo_hz=0, hi_hz=0)
        onset = PC.insert('duck', mode='onset', key_lanes=['kick'], min_velocity=1, depth_mdb=6000, ramp_us=2000,
                          hold_us=78000, release_us=60000, pre_us=0, band=band, pump=True)
        self.assertEqual(list(onset)[1:], list(PC.DUCK_KEYS['onset']))
        with self.assertRaisesRegex(KeyError, 'unknown key_lanes'):
            PC.insert('duck', mode='level', key_lanes=['kick'], key_channel='vox', depth_mdb=6000, range_mdb=18000,
                      top_mdb=-12000, attack_us=10000, release_us=300000, band=band, pump=False)
        with self.assertRaisesRegex(KeyError, 'mode'):
            PC.insert('duck', mode='sidechain')

    def test_the_insert_set_is_closed(self):
        self.assertEqual(set(PC.INSERT_KEYS), {'gain', 'pan', 'width', 'peak', 'low_shelf', 'high_shelf', 'highpass',
                                               'lowpass', 'cc_filter', 'mono_below', 'clip', 'drive', 'duck', 'comp',
                                               'parallel'})
        with self.assertRaisesRegex(KeyError, 'no insert type'):
            PC.insert('chorus', depth_pm=500)

    def test_every_key_is_in_the_contract(self):
        # docs/plugin/manifest.md names every key, in backticks or quoted in an object; a key here that it doesn't
        # name has drifted.
        text = CONTRACT.read_text(encoding='utf-8')
        documented = set(re.findall(r'`([a-z0-9_]+)`', text)) | set(re.findall(r'"([a-z0-9_]+)"', text))
        keys = {k for ks in PC.KEYS.values() for k in ks}
        keys |= {k for ks in PC.INSERT_KEYS.values() if ks for k in ks}
        keys |= {k for ks in PC.DUCK_KEYS.values() for k in ks}
        keys -= set(PC.LANES)  # the lanes table lists them as words
        self.assertEqual(set(), keys - documented)

    def test_the_lanes_keep_their_permanent_order(self):
        kit_h = (REPO / 'engine' / 'kit' / 'kit.h').read_text(encoding='utf-8')
        ids = re.search(r'kLaneIds \{([^}]*)\}', kit_h).group(1)
        self.assertEqual(tuple(re.findall(r'"([a-z]+)"', ids)), PC.LANES)


class TestDumps(unittest.TestCase):
    def test_the_canonical_layout(self):
        obj = {
            'kit': 'FUT01',
            'n': -3,
            'flag': True,
            'empty_list': [],
            'empty_obj': {},
            'ints': [1, -2, 3],
            'names': ['a', 'b'],
            'rows': [{'x': 1}, {'y': [4, 5]}],
            'nested': {'deep': {'v': 0}},
        }
        self.assertEqual(PC.dumps(obj), (
            '{\n'
            '  "kit": "FUT01",\n'
            '  "n": -3,\n'
            '  "flag": true,\n'
            '  "empty_list": [],\n'
            '  "empty_obj": {},\n'
            '  "ints": [1, -2, 3],\n'
            '  "names": ["a", "b"],\n'
            '  "rows": [\n'
            '    {\n'
            '      "x": 1\n'
            '    },\n'
            '    {\n'
            '      "y": [4, 5]\n'
            '    }\n'
            '  ],\n'
            '  "nested": {\n'
            '    "deep": {\n'
            '      "v": 0\n'
            '    }\n'
            '  }\n'
            '}\n'))

    def test_minimal_escapes_and_utf8(self):
        text = PC.dumps({'s': 'F# minor "x" \\ \t\n\x01 é ♯'})
        self.assertIn(r'"F# minor \"x\" \\ \t\n\u0001 é ♯"', text)
        self.assertEqual(json.loads(text), {'s': 'F# minor "x" \\ \t\n\x01 é ♯'})

    def test_a_mixed_array_goes_one_element_per_line(self):
        self.assertEqual(PC.dumps([1, [2]]), '[\n  1,\n  [2]\n]\n')

    def test_it_refuses_floats_none_and_other_types(self):
        for bad in ({'x': 1.0}, {'x': [1, 2.5]}, {'x': None}, {'x': {1, 2}}, {1: 'non-string key'}, {'x': 1 << 63}):
            with self.subTest(bad=repr(bad)):
                with self.assertRaises((TypeError, ValueError)):
                    PC.dumps(bad)

    def test_the_text_parses_back_and_is_stable(self):
        obj = PC.ordered('glide', curve='raised_cosine', us=70000, dip_us=30000)
        self.assertEqual(json.loads(PC.dumps(obj)), obj)
        self.assertEqual(PC.dumps(json.loads(PC.dumps(obj))), PC.dumps(obj))


class TestDenylistHook(unittest.TestCase):
    def fake_lists(self, *names):
        return {'artists': frozenset(d for n in names for d in denylist.entry_digests(n)), 'slurs': frozenset()}

    def test_clean_strings_pass(self):
        PC.check_strings({'title': 'Alloy Signal', 'files': [{'path': 'Drums/Kick Garage v1 rr1.wav'}]})

    def test_a_listed_name_fails_with_its_path(self):
        lists = self.fake_lists('Some Name')
        obj = {'instruments': [{'name': 'Kick'}, {'name': 'SomeName Stab'}]}
        with self.assertRaisesRegex(ValueError, r'instruments\[1\]\.name'):
            PC.check_strings(obj, lists)
        with self.assertRaisesRegex(ValueError, r'some_name \(key\)'):
            PC.check_strings({'some_name': 1}, lists)

    def test_the_real_lists_load(self):
        for name in PC.DENYLISTS:
            self.assertTrue(denylist.load(name), name)


class TestFiles(unittest.TestCase):
    def test_write_then_check(self):
        obj = {'kit': 'SYN01', 'content_version': 1}
        with tempfile.TemporaryDirectory() as d:
            path = Path(d) / 'syn01' / 'SYN01_mix.json'
            self.assertFalse(PC.write(path, obj, check=True))
            self.assertTrue(PC.write(path, obj))
            self.assertEqual(path.read_bytes(), PC.dumps(obj).encode('utf-8'))
            self.assertTrue(PC.write(path, obj, check=True))
            self.assertFalse(PC.write(path, {'kit': 'SYN01', 'content_version': 2}, check=True))
            self.assertEqual(PC.sha256_file(path), __import__('hashlib').sha256(path.read_bytes()).hexdigest())

    def test_write_runs_the_denylist_first(self):
        lists = {'artists': frozenset(denylist.entry_digests('Some Name')), 'slurs': frozenset()}
        with tempfile.TemporaryDirectory() as d:
            path = Path(d) / 'x.json'
            with self.assertRaises(ValueError):
                PC.write(path, {'title': 'Some Name'}, lists=lists)
            self.assertFalse(path.exists())


if __name__ == '__main__':
    unittest.main()
