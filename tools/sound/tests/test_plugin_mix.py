"""The mix manifests (content/plugin/<kit>/<KIT>_mix.json) against the contract and the recipes, and the trace of the
recipes' mixes (mix_trace.py).

The contract and recipe-constant tests read the committed manifests and run everywhere. The trace and export tests
render each kit's recipe (about 30 s a variant) and the per-call checks read the round 5 reports: they run where the
round 5 renders are (WUBANSIA_R5_BUILD, or the repo's build/<kit>-r5) and skip otherwise, as test_engine_ref's gates
do.
"""
import json
import math
import re
import unittest

import numpy as np

from tools.sound import plugin_mix as PM

KITS = PM.KITS


def heavy(kit='FUT01'):
    from tools.sound import engine_ref as E
    return E.r5_dir(kit) is not None


HEAVY_WHY = 'the round 5 renders are not here (set WUBANSIA_R5_BUILD to the folder holding <kit>-r5/)'

# each recipe lane -> the channels that carry it (the accents of FUT02 split by sound, FUT03's perc by instrument,
# FUT02's bass into the line and the drop glide); every channel listed here plays in the loops
LANE_CHANNELS = {
    'FUT01': {'kick': ['kick'], 'bass': ['bass'], 'clap': ['clap'], 'hat_c': ['hat_c'], 'hat_o': ['hat_o'],
              'tamb': ['tamb'], 'rim': ['rim'], 'perc': ['perc'], 'stab': ['stab'], 'pluck': ['pluck'],
              'fill': ['fill'], 'vsynth': ['vsynth'], 'acc_rev': ['acc_rev'], 'acc_up': ['acc_up'],
              'acc_hit': ['acc_hit'], 'acc_warp': ['acc_warp']},
    'FUT02': {'kick': ['kick'], 'clap': ['clap'], 'hat': ['hat'], 'zap': ['zap'], 'bass': ['bass', 'glide'],
              'pad': ['pad'], 'arp': ['arp'], 'fill': ['fill'], 'vox': ['vox'],
              'accents': ['acc_laser', 'acc_glass', 'acc_sweep', 'acc_revchord', 'acc_arpstab']},
    'FUT03': {'kick': ['kick'], 'clap': ['clap'], 'hat': ['hat'], 'shaker': ['shaker'], 'perc': ['perc', 'rim'],
              'stab': ['stab'], 'pad': ['pad'], 'bass': ['bass'], 'vox': ['vox'], 'accent': ['accent']},
    'FUT04': {'kick': ['kick'], 'bass': ['bass_stem'], 'rumble': ['rumble'], 'snare': ['snare'], 'hat': ['hat'],
              'ride': ['ride'], 'perc': ['perc'], 'pad': ['pad'], 'lead': ['lead'], 'fill': ['fill'], 'vox': ['vox'],
              'accent': ['accent']},
}
# channels for instruments the loops don't play (the extended mix's and MIDI in's)
EXTRA_CHANNELS = {'FUT01': ['bass_organ', 'pad'], 'FUT02': [], 'FUT03': ['lead', 'fx'], 'FUT04': ['bass']}
LANE_OF = {  # each channel's plugin lane (channels.py: drum notes 37-40 snare, 42/46/51 hats, others perc)
    'FUT01': {'kick': 'kick', 'bass': 'bass', 'bass_organ': 'bass', 'clap': 'snare', 'hat_c': 'hats', 'hat_o': 'hats',
              'tamb': 'perc', 'rim': 'perc', 'perc': 'perc', 'stab': 'chords', 'pluck': 'lead', 'fill': 'fx',
              'pad': 'chords', 'vsynth': 'vox', 'acc_rev': 'accents', 'acc_up': 'accents', 'acc_hit': 'accents',
              'acc_warp': 'accents'},
    'FUT02': {'kick': 'kick', 'clap': 'snare', 'hat': 'hats', 'zap': 'perc', 'bass': 'bass', 'glide': 'bass',
              'pad': 'chords', 'arp': 'lead', 'fill': 'fx', 'vox': 'vox', 'acc_laser': 'accents',
              'acc_glass': 'accents', 'acc_sweep': 'accents', 'acc_revchord': 'accents', 'acc_arpstab': 'accents'},
    'FUT03': {'kick': 'kick', 'clap': 'snare', 'rim': 'perc', 'hat': 'hats', 'shaker': 'perc', 'perc': 'perc',
              'bass': 'bass', 'stab': 'chords', 'pad': 'chords', 'lead': 'lead', 'fx': 'fx', 'vox': 'vox',
              'accent': 'accents'},
    'FUT04': {'kick': 'kick', 'rumble': 'kick', 'bass': 'bass', 'bass_stem': 'bass', 'snare': 'snare', 'hat': 'hats',
              'ride': 'hats', 'perc': 'perc', 'fill': 'fx', 'pad': 'chords', 'lead': 'lead', 'vox': 'vox',
              'accent': 'accents'},
}


def tag(ins):
    """An insert's type (a duck's with its mode), with its condition: 'gain:bass=a', 'clip:novox'."""
    t = ins['type'] if ins['type'] != 'duck' else f"duck_{ins['mode']}"
    w = ins.get('when')
    if not w:
        return t
    cond = ([w['variant']] if w['variant'] != 'any' else []) + ([f"{w['ab']}={w['is']}"] if w['ab'] != 'none' else [])
    return f"{t}:{','.join(cond)}"


# each channel's (and group's, return's, bus EQ's) inserts in the recipe's call order, checked against the recipes
# by hand: context4/context5 (FUT01), context/context5 (FUT02-04); the per-side and per-variant clips are one clip
# between gains (plugin_mix.clip_per)
INSERTS = {
    'FUT01': {
        'kick': ['clip', 'peak', 'gain:kick=a', 'clip', 'gain:kick=a'],
        'bass': ['highpass:bass=a', 'cc_filter', 'duck_onset', 'duck_onset', 'width', 'mono_below', 'peak', 'peak',
                 'gain:bass=b', 'gain:bass=a', 'gain:bass=a', 'clip', 'gain:bass=a'],
        'bass_organ': ['duck_onset', 'duck_onset', 'width', 'mono_below', 'peak', 'peak', 'gain'],
        'clap': ['peak', 'peak', 'width', 'gain', 'clip'],
        'hat_c': ['peak', 'width', 'pan', 'gain'],
        'hat_o': ['peak', 'width', 'pan', 'gain'],
        'tamb': ['peak', 'width', 'pan', 'gain'],
        'rim': ['pan', 'gain'],
        'perc': ['clip', 'pan', 'gain'],
        'stab': ['peak', 'peak', 'clip', 'gain', 'duck_onset', 'gain:novox'],
        'pluck': ['gain', 'duck_onset', 'gain:novox'],
        'fill': ['gain', 'gain:vox'],
        'pad': ['mono_below', 'peak', 'peak', 'gain'],
        'vsynth': ['peak', 'peak', 'peak', 'clip', 'duck_onset', 'gain'],
        'acc_rev': ['duck_onset', 'duck_onset', 'gain', 'gain:novox'],
        'acc_up': ['duck_onset', 'duck_onset', 'gain', 'gain:novox'],
        'acc_hit': ['clip', 'duck_onset', 'duck_onset', 'gain', 'gain:novox'],
        'acc_warp': ['clip', 'duck_onset', 'duck_onset', 'pan', 'gain', 'gain:novox'],
        'groups.drums': [],
        'groups.music': [],
        'returns.space': ['duck_level'],
        'returns.echo': [],
        'returns.resonator': [],
        'bus_eq.vox': ['peak', 'highpass'],
        'bus_eq.novox': ['peak', 'peak', 'peak', 'peak', 'peak', 'highpass'],
    },
    'FUT02': {
        'kick': ['gain'],
        'clap': ['gain', 'duck_onset'],
        'hat': ['pan', 'gain'],
        'zap': ['gain', 'gain', 'duck_onset'],
        'bass': ['gain:bass=b', 'gain:bass=a', 'gain'],
        'glide': ['duck_onset', 'gain:bass=b', 'gain:bass=a', 'gain'],
        'pad': ['gain', 'duck_onset'],
        'arp': ['cc_filter', 'duck_onset', 'duck_onset', 'gain', 'duck_onset'],
        'fill': ['gain', 'duck_onset'],
        'vox': ['clip', 'gain', 'duck_onset'],
        'acc_laser': ['gain', 'duck_onset'],
        'acc_glass': ['pan', 'gain', 'duck_onset'],
        'acc_sweep': ['gain', 'duck_onset'],
        'acc_revchord': ['gain', 'duck_onset'],
        'acc_arpstab': ['pan', 'gain', 'duck_onset'],
        'groups.drums': ['clip'],
        'groups.music': ['peak', 'peak'],
        'returns.space': ['duck_level', 'duck_onset', 'peak', 'peak'],
        'returns.echo': ['duck_onset', 'peak', 'peak'],
        'returns.resonator': ['duck_onset', 'peak', 'peak'],
        'bus_eq.vox': [],
        'bus_eq.novox': [],
    },
    'FUT03': {
        'kick': [],
        'clap': ['gain'],
        'rim': ['gain'],
        'hat': ['pan', 'gain'],
        'shaker': ['pan', 'gain'],
        'perc': ['gain'],
        'bass': ['gain:bass=a'],
        'stab': ['cc_filter', 'duck_onset', 'gain'],
        'pad': ['duck_onset', 'gain'],
        'lead': ['gain'],
        'fx': ['gain'],
        'vox': ['gain', 'comp', 'duck_onset', 'peak', 'peak', 'peak'],
        'accent': ['highpass', 'peak', 'gain', 'duck_onset'],
        'groups.drums': ['clip'],
        'groups.music': ['peak', 'peak', 'peak'],
        'returns.space': ['duck_level', 'duck_onset', 'peak', 'peak', 'peak'],
        'returns.echo': ['duck_onset', 'peak', 'peak', 'peak'],
        'returns.resonator': [],
        'bus_eq.vox': [],
        'bus_eq.novox': [],
    },
    'FUT04': {
        'kick': ['gain'],
        'rumble': ['gain'],
        'bass': ['cc_filter', 'gain:bass=b', 'gain:bass=a', 'duck_onset', 'duck_level'],
        'bass_stem': ['gain:bass=b', 'gain:bass=a', 'duck_onset', 'duck_level'],
        'snare': ['gain'],
        'hat': ['pan', 'gain'],
        'ride': ['gain'],
        'perc': ['pan', 'gain'],
        'fill': ['gain', 'duck_level'],
        'pad': ['gain', 'duck_onset'],
        'lead': ['gain'],
        'vox': ['gain'],
        'accent': [],
        'groups.drums': ['parallel', 'clip'],
        'groups.music': ['peak', 'peak', 'peak'],
        'returns.space': ['duck_level', 'duck_onset', 'peak', 'peak', 'peak'],
        'returns.echo': ['duck_onset', 'peak', 'peak', 'peak'],
        'returns.resonator': ['duck_onset', 'peak', 'peak', 'peak'],
        'bus_eq.vox': [],
        'bus_eq.novox': [],
    },
}

# the FDN's delays and input signs and the echo's wow and flutter phases: the numpy PCG64 draws of each kit's space
# seed (scratchpad scan/sendseeds.py, run read-only on the round 5 tree)
DRAWS = {
    'FUT01': ([515, 583, 682, 799, 885, 1029, 1167, 1361], [-1, -1, -1, 1, 1, -1, -1, -1], (3.053577, 0.182796)),
    'FUT02': ([1147, 1303, 1528, 1737, 1970, 2260, 2571, 3031], [-1, 1, 1, 1, 1, 1, -1, -1], (3.015247, 3.179961)),
    'FUT03': ([680, 809, 920, 1027, 1196, 1361, 1564, 1794], [1, -1, -1, 1, 1, -1, -1, 1], (4.812487, 1.378363)),
    'FUT04': ([1164, 1303, 1562, 1756, 1945, 2271, 2583, 2927], [-1, 1, 1, 1, 1, 1, -1, -1], (2.122682, 3.892756)),
}


def _inserts(m):
    """Every insert of a manifest, parallel chains included."""
    def walk(lst):
        for i in lst:
            yield i
            if i['type'] == 'parallel':
                yield from walk(i['inserts'])
    for c in m['channels']:
        yield from walk(c['inserts'])
    for g in m['groups'].values():
        yield from walk(g['inserts'])
    for r in m['returns'].values():
        yield from walk(r['inserts'])
    for lst in m['bus_eq'].values():
        yield from walk(lst)


def manifests():
    return {k: PM.load(k) for k in KITS}


class Contract(unittest.TestCase):
    """The committed manifests keep docs/plugin/manifest.md's rules."""

    @classmethod
    def setUpClass(cls):
        cls.m = manifests()

    def test_integer_only_and_denylist_clean(self):
        for kit, m in self.m.items():
            PM.validate(m)        # floats, ids, references, pre_us, peaks per channel, latency, cycles, denylists

    def test_canonical_layout(self):
        for kit, m in self.m.items():
            text = PM.path(kit).read_text(encoding='utf-8')
            self.assertEqual(text, PM.dumps(m), kit)
            self.assertTrue(text.endswith('\n') and not text.startswith('﻿') and '\r' not in text)
            self.assertIsNone(re.search(r'\d\.\d|\de-?\d', text.replace('"', '')), f'{kit}: a non-integer number')

    def test_top_level_keys(self):
        for kit, m in self.m.items():
            self.assertEqual(['kit', 'content_version', 'channels', 'groups', 'returns', 'bus_eq', 'master',
                              'reference'], list(m))
            self.assertEqual(kit, m['kit'])
            self.assertEqual(PM.CONTENT_VERSION, m['content_version'])
            for c in m['channels']:
                self.assertEqual(['id', 'lane', 'group', 'inserts', 'sends_pm'], list(c))
                self.assertIn(c['group'], ('none', 'drums', 'music'))
                self.assertEqual(3, len(c['sends_pm']))
                for ins in c['inserts']:
                    self.assertEqual('type', next(iter(ins)))

    def test_channels_and_lanes(self):
        for kit, m in self.m.items():
            ids = [c['id'] for c in m['channels']]
            want = sorted({ch for chans in LANE_CHANNELS[kit].values() for ch in chans} | set(EXTRA_CHANNELS[kit]))
            self.assertEqual(want, sorted(ids), kit)
            self.assertEqual(len(ids), len(set(ids)))
            self.assertLessEqual(len(ids), PM.MAX_CHANNELS)
            for c in m['channels']:
                self.assertEqual(LANE_OF[kit][c['id']], c['lane'], f'{kit} {c["id"]}')
            for lane, chans in LANE_CHANNELS[kit].items():      # every recipe lane maps to its channels, once each
                for ch in chans:
                    self.assertEqual(1, ids.count(ch))

    def test_inserts_in_the_recipes_order(self):
        """Each channel's, group's, return's and bus EQ's inserts, in the recipe's call order (INSERTS: the types, a
        duck's mode, and the condition of a when-switched insert)."""
        for kit, m in self.m.items():
            got = {c['id']: [tag(i) for i in c['inserts']] for c in m['channels']}
            got.update({f'groups.{g}': [tag(i) for i in m['groups'][g]['inserts']] for g in ('drums', 'music')})
            got.update({f'returns.{r}': [tag(i) for i in m['returns'][r]['inserts']]
                        for r in ('space', 'echo', 'resonator')})
            got.update({f'bus_eq.{v}': [tag(i) for i in m['bus_eq'][v]] for v in ('vox', 'novox')})
            self.assertEqual(INSERTS[kit], got, kit)

    def test_path_latency_budget(self):
        for kit, m in self.m.items():
            lat = PM.path_latencies(m)
            self.assertLessEqual(max(lat.values()), PM.MAX_PATH_LATENCY, kit)

    def test_no_level_duck_cycles(self):
        for kit, m in self.m.items():
            self.assertEqual([], PM.level_duck_cycles(m), kit)
        a = {'channels': [{'id': 'a', 'inserts': [{'type': 'duck', 'mode': 'level', 'key_channel': 'b'}]},
                          {'id': 'b', 'inserts': [{'type': 'duck', 'mode': 'level', 'key_channel': 'a'}]}]}
        self.assertEqual([['a', 'b', 'a']], PM.level_duck_cycles(a))

    def test_validate_refuses(self):
        m = json.loads(json.dumps(self.m['FUT01']))
        m['channels'][0]['inserts'].append({'type': 'gain', 'gain_mdb': 1.5})
        with self.assertRaisesRegex(ValueError, 'float'):
            PM.validate(m)
        m = json.loads(json.dumps(self.m['FUT01']))
        m['channels'][0]['inserts'] += [PM.clip(-1.0), PM.clip(-1.0)]
        with self.assertRaisesRegex(ValueError, 'insert latency'):
            PM.validate(m)

    def test_reference_one_peak_per_channel(self):
        for kit, m in self.m.items():
            for var in ('vox', 'novox'):
                r = m['reference'][var]
                self.assertEqual(len(m['channels']), len(r['channel_true_peak_mdb']))
                self.assertLess(r['pre_master_lufs_mlu'], r['club_lufs_mlu'])

    def test_bus_eq_per_variant(self):
        for kit, m in self.m.items():
            self.assertEqual(['vox', 'novox'], list(m['bus_eq']))


class RecipeConstants(unittest.TestCase):
    """Values the manifests copy from the recipes and the kits' seeds (cheap: no render)."""

    @classmethod
    def setUpClass(cls):
        cls.m = manifests()

    def test_fdn_delays_signs_and_echo_phases_are_the_pcg64_draws(self):
        for kit, (delays, signs, phases) in DRAWS.items():
            r = self.m[kit]['returns']
            self.assertEqual(delays, r['space']['delays_frames'], kit)
            self.assertEqual(signs, r['space']['signs'], kit)
            self.assertEqual([PM.iround(p, 1000) for p in phases], r['echo']['phases_mrad'], kit)

    def test_returns_follow_the_kit_settings(self):
        from tools.sound.recipes.fut01 import context4 as C1
        from tools.sound.recipes.fut02 import context as C2
        from tools.sound.recipes.fut03 import context as C3
        from tools.sound.recipes.fut04 import context as C4
        for kit, space, echo in (('FUT01', C1.PLATE, C1.ECHO), ('FUT02', C2.SPACE, C2.ECHO),
                                 ('FUT03', C3.SPACE, C3.ECHO), ('FUT04', C4.SPACE, C4.ECHO)):
            r = self.m[kit]['returns']
            self.assertEqual(PM.iround(space['t60'], 1000), r['space']['t60_ms'])
            self.assertEqual(PM.iround(space['predelay'], 1e6), r['space']['predelay_us'])
            self.assertEqual(PM.iround(space['damp_hz']), r['space']['damp_hz'])
            self.assertEqual(PM.pm(space['shimmer']), r['space']['shimmer_pm'])
            self.assertEqual(PM.iround(space['send_hp']), r['space']['send_hp_hz'])
            self.assertEqual(PM.pm(echo['feedback']), r['echo']['feedback_pm'])
            self.assertEqual((PM.iround(echo['lp_hz']), PM.iround(echo['hp_hz'])), (r['echo']['lp_hz'],
                                                                                     r['echo']['hp_hz']))
            # the predelay in frames survives the microseconds
            self.assertEqual(int(round(space['predelay'] * 48000)), int(round(r['space']['predelay_us'] * 0.048)))

    def test_resonators(self):
        from tools.sound.recipes.fut01 import context4 as C1
        from tools.sound.recipes.fut02 import context as C2
        from tools.sound.recipes.fut04 import context as C4
        for kit, freqs in (('FUT01', C1.resonator_freqs()), ('FUT02', C2.resonator_freqs()),
                           ('FUT04', C4.resonator_freqs())):
            self.assertEqual([PM.iround(f, 1000) for f in sorted(freqs)], self.m[kit]['returns']['resonator'][
                'tones_mhz'])
        self.assertEqual([], self.m['FUT03']['returns']['resonator']['tones_mhz'])

    def test_master(self):
        from tools.sound import master as MS
        for kit, m in self.m.items():
            s = MS.settings(__import__(f'tools.sound.kits.{kit.lower()}', fromlist=['KIT']))
            ms = m['master']
            self.assertEqual((25, 4, 150), (ms['lowcut_hz'], ms['lowcut_order'], ms['mono_below_hz']))
            self.assertEqual({'ratio_pm': 2000, 'knee_mdb': 6000, 'attack_us': 30000, 'release_fast_us': 150000,
                              'release_slow_us': 1500000}, ms['glue'])
            self.assertEqual({'knee_mdb': 1500, 'ceiling_mdb': PM.mdb(s.ceiling_dbtp + s.clip_db)}, ms['clip'])
            self.assertEqual({'ceiling_mdb': -1020, 'lookahead_us': 5000, 'hold_us': 60000, 'release_us': 80000},
                             ms['limiter'])

    def test_ride_points(self):
        for kit in KITS:
            rides = PM.ride_points(kit)
            m = self.m[kit]
            filt = {c['id'] for c in m['channels'] if any(i['type'] == 'cc_filter' for i in c['inserts'])}
            self.assertEqual(filt, set(rides), kit)
            for ch, cc in rides.items():
                for num, pts in cc.items():
                    ticks = [t for t, _ in pts]
                    self.assertEqual(0, ticks[0])
                    self.assertEqual(sorted(ticks), ticks)
                    self.assertTrue(all(t % PM.CC_STEP == 0 and 0 <= t < PM.TICKS for t in ticks))
                    self.assertTrue(all(0 <= v <= 127 for _, v in pts))
                    self.assertTrue(all(a[1] != b[1] for a, b in zip(pts, pts[1:])), 'deduped')

    def test_fut04_acid_ride_follows_bass_rides(self):
        from tools.sound.recipes.fut04 import bass as B
        r = PM.ride_points('FUT04')['bass']
        self.assertEqual(0, r[74][0][1])
        self.assertEqual(127, max(v for _, v in r[74]))         # the widest, x2.6, on the fill bar's third beat
        self.assertEqual(127, max(v for _, v in r[71]))         # the resonance at 0.96
        self.assertAlmostEqual(2.6, max(v for _, v in B.RIDES[B.PICK]['cutoff']))


@unittest.skipUnless(heavy(), HEAVY_WHY)
class Trace(unittest.TestCase):
    """Every DSP call each kit's context5 mix makes, in both variants, is classified, and the manifest holds what the
    calls measured and solved."""

    def test_every_call_is_classified(self):
        from tools.sound import mix_trace as T
        for kit in KITS:
            for v in ('vox', 'novox'):
                calls, _ = T.run(kit, v)            # classify() raises on any call no rule matches
                rep = T.report(calls)
                self.assertTrue(all(c.cls in T.CLASSES and c.why for c in calls))
                self.assertGreater(rep['steps']['live'], 10)
                for d in rep['dropped']:
                    self.assertTrue(d['why'], d)

    def test_every_traced_peak_and_duck_is_in_the_manifest(self):
        """Each RBJ peak the recipe runs (Hz, dB, Q) and each phrase.pump_db envelope (depth, ramp, hold, release) is
        an insert of the manifest, to the contract's units."""
        from tools.sound import mix_trace as T
        for kit in KITS:
            m = PM.load(kit)
            found = list(_inserts(m))
            peaks = {(i['freq_hz'], i['gain_mdb'], i['q_pm']) for i in found if i['type'] == 'peak'}
            ducks = {(i['depth_mdb'], i['ramp_us'], i['hold_us'], i['release_us']) for i in found
                     if i['type'] == 'duck' and i['mode'] == 'onset'}
            for v in ('vox', 'novox'):
                calls, _ = T.run(kit, v)
                for c in calls:
                    if c.cls not in T.MIX_CLASSES:
                        continue
                    a = c.args
                    if c.fn == 'dsp48.peaking':
                        self.assertIn((PM.iround(a['fc']), PM.mdb(a['gain_db']), PM.pm(a['q'])), peaks, f'{kit} {v}')
                    if c.fn == 'phrase.pump_db':
                        self.assertIn((PM.mdb(a['depth_db']), PM.us(a['ramp']), PM.us(a['hold']), PM.us(a['release'])),
                                      ducks, f'{kit} {v}')

    def test_clip_ceilings_are_the_lanes_true_peaks(self):
        """Each lane clip's ceiling is the lane's true peak in the reference loop, less its clip dB, plus 0.5 (FUT01);
        the drum buses' the kick lane's own (FUT02-04); the manifest holds each to the mdB."""
        from tools.sound import dsp48 as D
        from tools.sound import mix_trace as T
        from tools.sound.recipes.fut01 import context4 as C4, context5 as C5
        cap = T.run('FUT01', 'vox')[1]
        under = {'kick': C4.KICK_CLIP_DB, 'perc': C4.CLIP_DB['perc'], 'stab': C4.CLIP_DB['stab'],
                 'clap': C5.CLAP_CLIP_DB, 'vsynth': C5.CLIP_DB['vsynth'], 'acc_hit': C5.CLIP_DB['acc_hit']}
        lanes = {'kick': cap.sides[('kick', 'b')]}
        x = lanes['kick'] * 1.0
        self.assertAlmostEqual(D.true_peak_db(x) - under['kick'] + 0.5, cap.values[('clip', 'kick', 0, 'b')], 9)
        for kit in ('FUT02', 'FUT03', 'FUT04'):
            c = T.run(kit, 'vox')[1]
            from tools.sound import mixmeter as MM
            want = MM.true_peak_db(c.lanes['kick'])
            self.assertAlmostEqual(want, c.values[('clip', 'drums', 0, '')], 9, kit)
            m = PM.load(kit)
            clips = [i for i in m['groups']['drums']['inserts'] if i['type'] == 'clip']
            self.assertEqual(PM.mdb(want), clips[0]['ceiling_mdb'], kit)

    def test_fut04_faders_are_the_recipes(self):
        from tools.sound.recipes.fut04 import context as C
        m = PM.load('FUT04')
        ch = {c['id']: c for c in m['channels']}
        for k in ('kick', 'rumble', 'snare', 'hat', 'ride', 'perc', 'pad', 'lead', 'fill'):
            gains = [i['gain_mdb'] for i in ch[k]['inserts'] if i['type'] == 'gain']
            self.assertEqual([PM.mdb(C.fader_db(k, C.WARM))], gains, k)
        for side, char in (('b', C.WARM), ('a', C.CLEAN)):
            g = [i['gain_mdb'] for i in ch['bass_stem']['inserts']
                 if i['type'] == 'gain' and i.get('when', {}).get('is') == side]
            self.assertEqual([PM.mdb(C.fader_db('bass', char))], g, side)

    def test_glue_thresholds_and_drives_are_the_club_loops(self):
        from tools.sound import engine_ref as E
        for kit in KITS:
            d = E.r5_dir(kit)
            rep = {i['name']: i for i in json.loads((d / 'qa_report.json').read_text())['items']}
            m = PM.load(kit)
            for v in ('vox', 'novox'):
                meta = rep[E.items(kit, v)[1]]['files'][0]['meta']['master']
                self.assertEqual(PM.mdb(meta['glue']['threshold_dbfs']), m['master'][v]['glue_threshold_mdb'], kit)
                self.assertEqual(PM.mdb(meta['drive_db']), m['master'][v]['drive_mdb'], kit)
                self.assertEqual(PM.mdb(meta['input']['lufs_i']), m['reference'][v]['pre_master_lufs_mlu'], kit)
                self.assertEqual(PM.mdb(meta['output']['lufs_i']), m['reference'][v]['club_lufs_mlu'], kit)

    def test_export_is_deterministic(self):
        for kit in KITS:
            self.assertTrue(PM.export(kit, check=True), f'{kit}: the committed manifest differs from a fresh export')


if __name__ == '__main__':
    unittest.main()
