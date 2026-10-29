"""Items, the kit registry and render.py: names, one shared gain, the ceiling, the reports, identical bytes twice."""
import contextlib
import hashlib
import io
import json
import platform
import re
import subprocess
import sys
import tempfile
import unittest
import unittest.mock
from pathlib import Path

import numpy as np
import soundfile as sf

from tools.sound import dsp48 as D
from tools.sound import qa as Q
from tools.sound import render as R
from tools.sound.item import Item, Out, UnknownName
from tools.sound.kits import fut01
from tools.sound.tests import fixtures as FX

REPO = Path(__file__).resolve().parents[3]
SR = D.SR
MASTERS = Path(__file__).resolve().parent / 'masters.sha256'   # SHA-256 of the Ping and KickSplit masters
RENDER_HOST = sys.platform.startswith('linux') and platform.machine() == 'x86_64'
NAME = re.compile(r'^FUT01_(Drums|Perc|Bass|Stabs|Pad|Lead|Vox|FX|Textures|Loops)_[A-Z][A-Za-z0-9]*(-[A-G]s?-?\d+)?'
                  r'_v\d+_rr\d+\.flac$')


def blip(take):
    """A 1 kHz blip; round robin 2 is 2 dB louder than round robin 1, velocity scales it."""
    n = SR // 2
    x = np.sin(2 * np.pi * 1000 * np.arange(n) / SR) * D.env_exp(n, 0.03, attack=0.001)
    return x * take.vel * (D.undb(2.0) if take.rr == 2 else 1.0)


def wide(take):
    r = take.rng()
    n = SR // 2
    x = D.bfilt(r.uniform(-1, 1, (n, 2)), (2000, 8000), 'bandpass') * D.env_exp(n, 0.03, attack=0.001)[:, None]
    return Out(x)


def bed(take):
    n = 4 * SR
    x = D.bfilt(take.rng().uniform(-1, 1, n), (300, 3000), 'bandpass')
    return Out(D.fade(x, 0.5, 0.5), loop=(SR, 3 * SR))


def bed_with_meta(take):
    out = bed(take)
    out.meta = {'note': 'facts for the report'}
    return out


def clean_kick(take):
    return FX.clean_kick(take.vel, take.rr)


def muddy_kick(take):
    return FX.muddy_kick(take.vel)


def main(*argv):
    """render.main, quiet."""
    with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
        return R.main(list(argv))


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


class TestItems(unittest.TestCase):
    def test_validation(self):
        for bad in (dict(group='Drum'), dict(category='drum'), dict(name='Kick_A'), dict(name='kick'),
                    dict(layers=(1.0, 0.5)), dict(rr=0), dict(flags={'loud'}), dict(notes=(45, 42)),
                    dict(qa={'nope': 1}), dict(level=3.0)):
            with self.assertRaises(ValueError, msg=str(bad)):
                Item(**{**dict(group='Drums', name='Kick', category='kick', render=blip), **bad})

    def test_bass_and_loops_need_a_low_end_rule(self):
        """A bass or loop file may skip the mud gate only with a reason: an S1c bass loop can't go ungated by
        default."""
        for kw in (dict(group='Bass', category='bass', qa={'mud': 'off'}), dict(group='Loops', category='loop')):
            with self.assertRaises(ValueError, msg=str(kw)):
                Item(name='Test', render=blip, **kw)
        Item('Loops', 'Test', 'loop', blip, qa={'mud': 'gate'})
        Item('Loops', 'Test', 'loop', blip, qa={'mud': 'off', 'why': 'vocals belong in the low mids'})
        Item('Textures', 'Test', 'loop', blip)                     # other groups keep the category default

    def test_takes_and_file_names(self):
        it = Item('Bass', 'Test', 'bass', blip, layers=(0.5, 1.0), rr=2, notes=(42, 45))
        takes = it.takes('FUT01')
        self.assertEqual(8, len(takes))
        self.assertEqual('FUT01_Bass_Test-Fs2_v1_rr1.flac', takes[0].file)
        self.assertEqual('FUT01_Bass_Test-A2_v2_rr2.flac', takes[-1].file)
        self.assertEqual([(42, 1, 1), (42, 1, 2), (42, 2, 1)], [(t.note, t.layer, t.rr) for t in takes[:3]])
        self.assertTrue(all(NAME.match(t.file) for t in takes))
        kick = Item('Drums', 'Kick', 'kick', blip, root=30).takes('FUT01')[0]
        self.assertEqual(('FUT01_Drums_Kick_v1_rr1.flac', 30), (kick.file, kick.pitch))
        self.assertAlmostEqual(kick.f0, 46.249, places=3)

    def test_take_noise_is_per_take(self):
        a, b = Item('Perc', 'Test', 'perc', blip, rr=2).takes('FUT01')
        self.assertEqual(a.rng().uniform(), a.rng().uniform())
        self.assertNotEqual(a.rng().uniform(), b.rng().uniform())
        self.assertEqual(a.rng('x').uniform(), D.rng('FUT01', 'Test', 1, 1, 'x').uniform())

    def test_registry(self):
        items = fut01.items()
        names = [i.name for i in items]
        self.assertEqual(len(names), len(set(names)))
        self.assertIn('Ping', names)
        self.assertEqual(['Ping'], [i.name for i in fut01.items(sets=('example',))])
        self.assertEqual(['Ping'], [i.name for i in fut01.items(only=('Ping',))])
        with self.assertRaises(UnknownName):
            fut01.items(only=('NoSuchSound',))

    def test_duplicate_names_are_refused(self):
        a = Item('Perc', 'Same', 'perc', blip)
        with self.assertRaises(ValueError):
            fut01.check_unique([('one', a), ('two', a)])

    def test_kit_dna(self):
        self.assertEqual((1.0, 1.483, 1.932, 2.546, 3.011, 3.715, 4.468), fut01.ALLOY)
        self.assertAlmostEqual(1.0, sum(w for _, w in fut01.VOICE))
        self.assertEqual(('FUT01', 134.0, 'F# minor', '11A', 'FUT01/space'),
                         (fut01.KIT, fut01.BPM, fut01.KEY, fut01.CAMELOT, fut01.SPACE_SEED))
        self.assertTrue(all(fut01.in_key(n) for n in (30, 42, 44, 45, 47, 49, 50, 52)))
        self.assertFalse(fut01.in_key(43))


class TestRender(unittest.TestCase):
    def test_one_gain_over_every_layer_and_round_robin(self):
        it = Item('Perc', 'Blip', 'perc', blip, layers=(0.5, 1.0), rr=2)
        done = R.render_item(it, 'FUT01')
        tps = [D.true_peak_db(a) for a in done.audio]
        self.assertAlmostEqual(max(tps), D.TP_TARGET, places=6)        # the loudest take sets the gain ...
        self.assertTrue(all(tp <= Q.CEILING_DBTP for tp in tps))         # ... so no round robin goes over
        self.assertAlmostEqual(tps[1] - tps[0], 2.0, places=2)           # rr2 stays 2 dB over rr1
        self.assertAlmostEqual(tps[3] - tps[1], 6.02, places=2)          # v2 stays 6 dB over v1

    def test_level_target_and_its_cap(self):
        it = Item('Textures', 'Bed', 'texture', bed, loop=True, level=-20.0)
        done = R.render_item(it, 'FUT01')
        self.assertAlmostEqual(Q.lufs(done.audio[0]), -20.0, places=2)
        self.assertFalse(done.capped)
        hot = R.render_item(Item('Textures', 'Hot', 'texture', bed, loop=True, level=-3.0), 'FUT01')
        self.assertTrue(hot.capped)
        self.assertAlmostEqual(D.true_peak_db(hot.audio[0]), D.TP_TARGET, places=6)

    def test_loop_items_keep_their_length_and_loop(self):
        done = R.render_item(Item('Textures', 'Bed', 'texture', bed, loop=True, level=-20.0), 'FUT01')
        self.assertEqual(4 * SR, len(done.audio[0]))
        self.assertEqual((SR, 3 * SR), done.outs[0].loop)

    def test_files_are_mono_where_l_equals_r(self):
        with tempfile.TemporaryDirectory() as tmp:
            for it, ch in ((Item('Perc', 'Blip', 'perc', blip), 1), (Item('Perc', 'Wide', 'perc', wide), 2)):
                done = R.render_item(it, 'FUT01')
                path = Path(tmp) / done.takes[0].file
                R.write_flac(path, done.audio[0])
                info = sf.info(path)
                got = (info.samplerate, info.subtype, info.format, info.channels)
                self.assertEqual((SR, 'PCM_24', 'FLAC', ch), got)

    def test_kicks_go_through_the_mud_gate_and_the_loop(self):
        with tempfile.TemporaryDirectory() as tmp:
            for render, ok in ((clean_kick, True), (muddy_kick, False)):
                it = Item('Drums', 'TestKick', 'kick', render, layers=(0.4, 0.7, 1.0), rr=2, root=fut01.KICK_NOTE)
                entry = R.qa_item(R.render_item(it, 'FUT01'), Path(tmp), fut01)
                names = {c['name'] for f in entry['files'] for c in f['checks']} | {c['name'] for c in entry['checks']}
                self.assertTrue({'mud_150_400_fall', 'mud_share', 'loop_mud_vs_sub', 'tuning'} <= names)
                failed = {c['name'] for _, c in R.failures(entry)}
                if ok:
                    self.assertEqual(set(), failed)
                else:
                    self.assertTrue({'mud_150_400_fall', 'mud_vs_sub_after_80ms', 'loop_mud_vs_sub'} <= failed, failed)
                self.assertTrue((Path(tmp) / entry['loop_file']).exists())

    def test_bass_shots_get_the_two_step_loop_and_bass_loops_dont(self):
        with tempfile.TemporaryDirectory() as tmp:
            shot = R.qa_item(R.render_item(Item('Bass', 'Shot', 'bass', clean_kick, root=fut01.KICK_NOTE,
                                                qa={'tonal': False}), 'FUT01'), Path(tmp), fut01)
            self.assertIn('loop_mud_vs_sub', {c['name'] for c in shot['checks']})
            self.assertTrue((Path(tmp) / shot['loop_file']).exists())
            held = R.qa_item(R.render_item(Item('Bass', 'Held', 'bass', bed, loop=True), 'FUT01'), Path(tmp), fut01)
            self.assertIsNone(held['loop_file'])

    def test_report_carries_flags_overrides_and_exceptions(self):
        with tempfile.TemporaryDirectory() as tmp:
            it = Item('FX', 'Blip', 'fx', blip, flags={'cut_end'}, tags=('fill',),
                      qa={'tonal': False, 'why': 'it ends on the downbeat'})
            entry = R.qa_item(R.render_item(it, 'FUT01'), Path(tmp), fut01)
            self.assertEqual((['cut_end'], ['fill'], {'tonal': False, 'why': 'it ends on the downbeat'}),
                             (entry['flags'], entry['tags'], entry['qa_overrides']))
            report = {'kit': 'FUT01', 'title': fut01.TITLE, 'set': None, 'environment': R.environment(),
                      'items': [entry], 'summary': {'items': 1, 'files': 1, 'failed_checks': 0, 'failed_items': []}}
            R.write_reports(Path(tmp), report)
            md = (Path(tmp) / 'qa_report.md').read_text()
            self.assertIn('## Exceptions', md)
            self.assertIn('| FX/Blip | cut_end | tonal=False | it ends on the downbeat |', md)
            self.assertIn('numpy', md)

    def test_report_carries_loop_points_and_meta(self):
        with tempfile.TemporaryDirectory() as tmp:
            it = Item('Textures', 'Bed', 'texture', bed_with_meta, loop=True, level=-20.0)
            entry = R.qa_item(R.render_item(it, 'FUT01'), Path(tmp), fut01)
            self.assertEqual([SR, 3 * SR], entry['files'][0]['loop'])
            self.assertEqual({'note': 'facts for the report'}, entry['files'][0]['meta'])
            one = R.qa_item(R.render_item(Item('Perc', 'Blip', 'perc', blip), 'FUT01'), Path(tmp), fut01)
            self.assertNotIn('loop', one['files'][0])

    def test_missing_models_are_a_usage_error(self):
        from tools.sound import models

        def no_models(*a, **kw):
            raise models.ModelsMissing('WUBANSIA_MODELS_DIR is not set.')
        with unittest.mock.patch.object(R, 'run', no_models):
            self.assertEqual(2, main('--kit', 'FUT01', '--out', 'unused'))

    def test_render_twice_gives_identical_bytes(self):
        with tempfile.TemporaryDirectory() as a, tempfile.TemporaryDirectory() as b:
            for out in (a, b):
                self.assertEqual(0, main('--kit', 'FUT01', '--set', 'example', '--out', out, '--no-sheets'))
            files = sorted(p.name for p in Path(a).glob('*.flac'))
            self.assertEqual(4, len(files))
            self.assertTrue(all(NAME.match(f) for f in files), files)
            self.assertEqual([sha(Path(a) / f) for f in files], [sha(Path(b) / f) for f in files])
            self.assertEqual(sha(Path(a) / 'qa_report.json'), sha(Path(b) / 'qa_report.json'))
            report = json.loads((Path(a) / 'qa_report.json').read_text())
            self.assertEqual(0, report['summary']['failed_checks'])
            self.assertEqual(files, sorted(f['file'] for it in report['items'] for f in it['files']))
            self.assertTrue((Path(a) / 'qa_report.md').read_text().startswith('# QA report'))

    def test_repro_flag_re_renders_in_a_fresh_process(self):
        with tempfile.TemporaryDirectory() as out:
            self.assertEqual(0, main('--kit', 'FUT01', '--set', 'example', '--out', out, '--no-sheets', '--repro'))
            report = json.loads((Path(out) / 'qa_report.json').read_text())
            checks = {c['name']: c for c in report['items'][0]['checks']}
            self.assertEqual('pass', checks['reproducible']['status'])
            self.assertIn('fresh process', checks['reproducible']['detail'])
        here = [hashlib.sha256(R.encode(x)).hexdigest() for x in R._render_named(('FUT01', 'Ping')).audio]
        self.assertEqual([here], R.repro_hashes([('FUT01', 'Ping')]))

    def test_out_defaults_to_build_kit_set(self):
        with tempfile.TemporaryDirectory() as tmp, contextlib.chdir(tmp):
            self.assertEqual(0, main('--kit', 'FUT01', '--set', 'example', '--no-sheets'))
            self.assertTrue((Path(tmp) / 'build' / 'FUT01-example' / 'qa_report.json').exists())

    def test_listening_copies_are_the_same_samples(self):
        from tools.sound import listen
        with tempfile.TemporaryDirectory() as out:
            self.assertEqual(0, main('--kit', 'FUT01', '--set', 'example', '--out', out, '--no-sheets'))
            wavs = listen.build(out)
            self.assertEqual(['Ping v1 rr1.wav', 'Ping v1 rr2.wav', 'Ping v2 rr1.wav', 'Ping v2 rr2.wav'],
                             sorted(p.name for p in wavs))
            self.assertEqual({'Drums'}, {p.parent.name for p in wavs})
            wav, sr = sf.read(Path(out) / 'Drums' / 'Ping v2 rr1.wav', dtype='int32')
            flac, _ = sf.read(Path(out) / 'FUT01_Perc_Ping_v2_rr1.flac', dtype='int32')
            self.assertEqual((SR, 'PCM_24'), (sr, sf.info(Path(out) / 'Drums' / 'Ping v2 rr1.wav').subtype))
            self.assertTrue(np.array_equal(wav, flac))
        self.assertEqual('Air Pad F#m9.wav', listen.plain('FUT01_Pad_AirPadFsm9_v1_rr1.flac', False, False))
        self.assertEqual('Closed Hat FM v5 rr3.wav', listen.plain('FUT01_Drums_ClosedHatFM_v5_rr3.flac'))
        self.assertEqual('Reese F#1 v2.wav', listen.plain('FUT01_Bass_Reese-Fs1_v2_rr1.flac', True, False))

    def test_unknown_names_are_an_error(self):
        with tempfile.TemporaryDirectory() as out:
            self.assertEqual(2, main('--kit', 'FUT01', '--only', 'NoSuchSound', '--out', out))
            self.assertEqual(2, main('--kit', 'NOPE', '--out', out))

    def test_a_key_error_inside_a_recipe_keeps_its_traceback(self):
        def broken(*a, **kw):
            return {'a': 1}['decay']
        with unittest.mock.patch.object(R, 'run', broken), self.assertRaises(KeyError):
            main('--kit', 'FUT01', '--out', 'unused')

    def test_cli_with_sheets(self):
        with tempfile.TemporaryDirectory() as out:
            run = subprocess.run([sys.executable, 'tools/sound/render.py', '--kit', 'FUT01', '--only', 'Ping',
                                  '--out', out], cwd=REPO, capture_output=True, text=True)
            self.assertEqual(0, run.returncode, run.stdout + run.stderr)
            self.assertTrue((Path(out) / 'sheets' / 'FUT01_Perc_Ping.png').stat().st_size > 10000)
            self.assertIn('0 failed', run.stdout)
            env = json.loads((Path(out) / 'qa_report.json').read_text())['environment']
            self.assertEqual(np.__version__, env['numpy'])
            if RENDER_HOST:                                   # the pins: baseline numpy kernels, plain OpenBLAS
                self.assertEqual([], env['numpy_simd']['dispatch'])
                self.assertEqual('Prescott', env['OPENBLAS_CORETYPE'])

    @unittest.skipUnless(RENDER_HOST, 'masters are hashed on x86_64 Linux only (ADR 0007)')
    def test_the_fdn_matrix_runs_the_pinned_blas_kernel(self):
        """The FDN's mixing matrix goes through OpenBLAS, whose Haswell and SkylakeX kernels round differently. With the
        pin (tools/sound/__init__.py) its float64 output is the same on any x86-64 CPU. The masters that change with the
        kernel today are the wet loops, which CI can't render (they need the vocal models), so this checks the floats."""
        x = np.zeros(SR)
        x[0] = 1.0
        x[100:4900] = D.rng('FUT01/test', 'blas').standard_normal(4800) * 0.1
        y = D.fdn_reverb(x, D.rng('FUT01/space'), t60=1.0)
        self.assertEqual('4a5098e17514a57d7a384ef94f7e9e7659d94985262ca44ccf0ccea0e2368f0f',
                         hashlib.sha256(np.ascontiguousarray(y, '<f8').tobytes()).hexdigest())

    @unittest.skipUnless(RENDER_HOST, 'masters are hashed on x86_64 Linux only (ADR 0007)')
    def test_masters_match_the_committed_hashes(self):
        """The same pins and SIMD level give the same bytes on another machine: CI renders two instruments and
        compares them with the hashes committed from the S1a render. A deliberate sound change re-renders them in its
        own commit (ADR 0008)."""
        want = dict(reversed(ln.split()) for ln in MASTERS.read_text().splitlines() if ln.strip())
        with tempfile.TemporaryDirectory() as out:
            run = subprocess.run([sys.executable, 'tools/sound/render.py', '--kit', 'FUT01', '--only', 'Ping',
                                  'KickSplit', '--no-sheets', '--out', out], cwd=REPO, capture_output=True, text=True)
            self.assertEqual(0, run.returncode, run.stdout + run.stderr)
            got = {p.name: sha(p) for p in Path(out).glob('*.flac')}
        self.assertEqual(want, got)


if __name__ == '__main__':
    unittest.main()
