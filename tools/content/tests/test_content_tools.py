"""tools/content/pack.py, tools/content/install.py and dist/install-macos.sh (ADR 0022), on a synthetic kit.

From the repo root: python3 -m unittest discover -s tools/content/tests -t .

The synthetic kit mirrors engine/testing/synthetic_kit.cpp's SYN01 file table (its paths and channels, at a tenth of
its frames) plus one Stems/ file, which SYN01 doesn't have and the real kits do. Its kit.json holds the two sections
these tools read (kit and files, docs/plugin/manifest.md); the others are empty. Nothing here needs numpy, the
samples or a C++ build, so CI runs it on Linux and on the macOS runner (there on the real bash 3.2 and BSD tools).

install-macos.sh runs for real only on macOS. On Linux these tests run its --dry-run against a fake home folder, and a
real run with stand-ins for the macOS-only commands on PATH (uname says Darwin; xattr, codesign and killall log what
they're asked); the plugin half of that runs only on Linux, because on a Mac it would write /Applications.
"""
import hashlib
import json
import os
import random
import re
import shutil
import stat
import struct
import subprocess
import sys
import tempfile
import unittest
import zipfile
from pathlib import Path

from tools.content import install as INST
from tools.content import kitfiles as KF

REPO = Path(__file__).resolve().parents[3]
PACK_PY = REPO / 'tools' / 'content' / 'pack.py'
INSTALL_PY = REPO / 'tools' / 'content' / 'install.py'
SCRIPT = REPO / 'dist' / 'install-macos.sh'
README_TXT = REPO / 'dist' / 'README.txt'
INSTALL_MD = REPO / 'dist' / 'INSTALL.md'

ON_MAC = sys.platform == 'darwin'
ON_LINUX = sys.platform.startswith('linux')
BASH = shutil.which('bash')
UNZIP = shutil.which('unzip')

# engine/testing/synthetic_kit.cpp's kFiles (path, channels, frames / 10), then a Stems/ file SYN01 doesn't have.
SYN_FILES = (
    ('Drums/Kick B v1 rr1.wav', 1, 1440),
    ('Drums/Kick B v1 rr2.wav', 1, 1440),
    ('Drums/Kick B v2 rr1.wav', 1, 1440),
    ('Drums/Kick B v2 rr2.wav', 1, 1440),
    ('Drums/Kick A.wav', 1, 1200),
    ('Drums/Snare v1 rr1.wav', 2, 960),
    ('Drums/Snare v1 rr2.wav', 2, 960),
    ('Drums/Snare v2 rr1.wav', 2, 960),
    ('Drums/Snare v2 rr2.wav', 2, 960),
    ('Drums/Closed Hat rr1.wav', 2, 480),
    ('Drums/Closed Hat rr2.wav', 2, 480),
    ('Drums/Open Hat.wav', 2, 3840),
    ('Bass/Bass A G1.wav', 1, 2400),
    ('Bass/Bass A D2.wav', 1, 2400),
    ('Bass/Bass A G2.wav', 1, 2400),
    ('Bass/Bass B G1 v1.wav', 1, 2400),
    ('Bass/Bass B G1 v2.wav', 1, 2400),
    ('Bass/Bass B D2 v1.wav', 1, 2400),
    ('Bass/Bass B D2 v2.wav', 1, 2400),
    ('Bass/Bass B G2 v1.wav', 1, 2400),
    ('Bass/Bass B G2 v2.wav', 1, 2400),
    ('Music/Lead C5.wav', 2, 2400),
    ('Music/Stab G3.wav', 2, 1920),
    ('Music/Stab D4.wav', 2, 1920),
    ('Music/Pad G3.wav', 2, 9600),
    ('Vox/Vox G4 v1.wav', 2, 2496),
    ('Vox/Vox G4 v2.wav', 2, 2496),
    ('Vox/Vox G4 v3.wav', 2, 2496),
    ('FX/Riser.wav', 2, 9000),
    ('Loops/Lead Phrase.wav', 2, 18000),
    ('Stems/Lead Phrase F#4 4 Bars.wav', 2, 36000),
)
# In the kit's folder but not in its kit.json: what the repo's sample folders also hold (a reference loop, notes).
UNLISTED = ('Loops/Lead Phrase Club.wav', 'LISTEN.md')


def wav_bytes(channels, frames, seed, rate=48000, bits=24, fmt_tag=1):
    """A WAV file: RIFF, 'fmt ' and 'data', with deterministic noise as its samples."""
    block = channels * bits // 8
    data = random.Random(seed).randbytes(frames * block)
    fmt = struct.pack('<HHIIHH', fmt_tag, channels, rate, rate * block, block, bits)
    if fmt_tag == 0xFFFE:  # WAVE_FORMAT_EXTENSIBLE: cbSize, valid bits, channel mask, the PCM sub-format GUID
        fmt += struct.pack('<HHI', 22, bits, 3) + bytes.fromhex('0100000000001000800000aa00389b71')
    body = b'WAVE' + b'fmt ' + struct.pack('<I', len(fmt)) + fmt + b'data' + struct.pack('<I', len(data)) + data
    if len(data) % 2:
        body += b'\0'
    return b'RIFF' + struct.pack('<I', len(body)) + body


def lfs_pointer(data):
    """What a Git LFS checkout without the objects holds in place of data."""
    return (f'version https://git-lfs.github.com/spec/v1\noid sha256:{hashlib.sha256(data).hexdigest()}\n'
            f'size {len(data)}\n').encode()


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def parts(path):
    return path.split('/')


class SynKit:
    """A synthetic kit on disk: <root>/samples/<kit>/<path> (the repo's layout) and
    <root>/manifests/<kit>/<KIT>_kit.json."""

    def __init__(self, root, kit_id='SYN01', version=1, files=SYN_FILES, seed=''):
        self.root = Path(root)
        self.id = kit_id
        self.folder = kit_id.lower()
        self.version = version
        self.samples = self.root / 'samples'
        self.manifests = self.root / 'manifests'
        self.data = {}
        entries = []
        for path, channels, frames in files:
            data = wav_bytes(channels, frames, f'{kit_id}/{path}{seed}')
            self.data[path] = data
            entries.append({'path': path, 'frames': frames, 'channels': channels, 'sha256': sha256(data),
                            'loop_start': 0, 'loop_end': 0, 'preroll_frames': 0})
        for path, data in self.data.items():
            self.write_sample(path, data)
        self.write_sample(UNLISTED[0], wav_bytes(2, 4800, 'club'))
        self.write_sample(UNLISTED[1], b'# Notes\n')
        self.entries = entries
        self.write_manifest()

    @property
    def paths(self):
        return [e['path'] for e in self.entries]

    @property
    def manifest(self):
        return self.manifests / self.folder / f'{self.id}_kit.json'

    @property
    def zip_name(self):
        return f'Wubansia-Content-{self.id}-v{self.version}.zip'

    def sample(self, path):
        return self.samples / self.folder / path

    def write_sample(self, path, data):
        p = self.sample(path)
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_bytes(data)

    def write_manifest(self, entries=None):
        obj = {
            'kit': {'id': self.id, 'title': 'Synthetic', 'direction': 'test', 'bpm': 128, 'key': 'G minor',
                    'camelot': '6A', 'swing_pm': 560, 'content_version': self.version,
                    'lanes': {lane: '' for lane in ('kick', 'snare', 'hats', 'perc', 'bass', 'lead', 'chords', 'vox',
                                                    'accents', 'fx')},
                    'ab': {s: {'a': '', 'b': ''} for s in ('kick', 'bass', 'stab')}},
            'files': entries if entries is not None else self.entries,
            'instruments': [], 'keymap': [], 'extras': [], 'phrases': [],
        }
        self.manifest.parent.mkdir(parents=True, exist_ok=True)
        self.manifest.write_text(json.dumps(obj, indent=2, ensure_ascii=False) + '\n', encoding='utf-8')

    def sums_text(self):
        return ''.join(f"{e['sha256']}  {e['path']}\n" for e in sorted(self.entries, key=lambda e: parts(e['path'])))


def run(args, env=None, cwd=None, timeout=120):
    full = dict(os.environ)
    full.update(env or {})
    return subprocess.run([str(a) for a in args], capture_output=True, text=True, env=full, cwd=cwd, timeout=timeout)


def run_pack(kit, *args, samples=None):
    return run([sys.executable, PACK_PY, '--kit', kit.id, '--manifests', kit.manifests,
                '--samples', samples or kit.samples, *args])


def run_install(kit, *args):
    return run([sys.executable, INSTALL_PY, '--manifests', kit.manifests, *args])


def tree(root):
    """Every file under root, relative, with its bytes' SHA-256."""
    root = Path(root)
    if not root.exists():
        return {}
    return {p.relative_to(root).as_posix(): sha256(p.read_bytes()) for p in sorted(root.rglob('*')) if p.is_file()}


class Case(unittest.TestCase):
    def setUp(self):
        self.tmp = Path(tempfile.mkdtemp(prefix='wubansia content '))  # a space, as in "Application Support"
        self.addCleanup(shutil.rmtree, self.tmp, ignore_errors=True)

    def assertOk(self, proc, msg=''):
        self.assertEqual(proc.returncode, 0, f'{msg}\nstdout:\n{proc.stdout}\nstderr:\n{proc.stderr}')

    def assertFails(self, proc, code=1):
        self.assertEqual(proc.returncode, code, f'stdout:\n{proc.stdout}\nstderr:\n{proc.stderr}')

    def packed(self, kit, out=None):
        out = out or self.tmp / 'out'
        proc = run_pack(kit, '--out', out)
        self.assertOk(proc)
        return out / kit.zip_name


# ------------------------------------------------------------------ the shared rules (kitfiles.py)

class TestKitFiles(Case):
    def test_load_kit_reads_the_id_version_and_files(self):
        kit = SynKit(self.tmp, version=3)
        k = KF.load_kit(kit.manifest)
        self.assertEqual((k.id, k.folder, k.version), ('SYN01', 'syn01', 3))
        self.assertEqual([f.path for f in k.files], kit.paths)
        self.assertEqual(k.files[0].sha256, kit.entries[0]['sha256'])
        self.assertEqual((k.files[0].frames, k.files[0].channels), (1440, 1))

    def test_load_kit_refuses_what_breaks_the_rules(self):
        kit = SynKit(self.tmp)
        bad = {
            'a parent folder': '../x.wav',
            'an absolute path': '/x.wav',
            'an empty part': 'Drums//x.wav',
            'a backslash': 'Drums\\x.wav',
            'a newline': 'Drums/x\n.wav',
            'not a WAV': 'Drums/x.aif',
        }
        for why, path in bad.items():
            with self.subTest(why):
                entries = [dict(kit.entries[0], path=path)]
                kit.write_manifest(entries)
                with self.assertRaises(KF.ManifestError):
                    KF.load_kit(kit.manifest)
        with self.subTest('two paths that differ only in case (one file on a Mac)'):
            kit.write_manifest([kit.entries[0], dict(kit.entries[1], path=kit.entries[0]['path'].upper())])
            with self.assertRaises(KF.ManifestError):
                KF.load_kit(kit.manifest)
        with self.subTest('a bad SHA-256'):
            kit.write_manifest([dict(kit.entries[0], sha256='ABC')])
            with self.assertRaises(KF.ManifestError):
                KF.load_kit(kit.manifest)

    def test_lfs_pointers_are_recognised(self):
        p = self.tmp / 'x.wav'
        p.write_bytes(lfs_pointer(b'RIFF'))
        self.assertTrue(KF.is_lfs_pointer(p))
        p.write_bytes(wav_bytes(1, 10, 'x'))
        self.assertFalse(KF.is_lfs_pointer(p))

    def test_wav_info_reads_pcm_float_and_extensible_headers(self):
        p = self.tmp / 'x.wav'
        for tag, bits in ((1, 16), (1, 24), (3, 32), (0xFFFE, 24)):
            with self.subTest(tag=tag, bits=bits):
                p.write_bytes(wav_bytes(2, 333, 'w', bits=bits, fmt_tag=tag))
                info = KF.wav_info(p)
                self.assertEqual((info.rate, info.channels, info.frames), (48000, 2, 333))
        p.write_bytes(b'not a wav at all')
        with self.assertRaises(KF.WavError):
            KF.wav_info(p)

    def test_sums_are_sorted_by_folder_and_parse_back(self):
        kit = SynKit(self.tmp)
        k = KF.load_kit(kit.manifest)
        text = KF.sums_text(k.files)
        self.assertEqual(text, kit.sums_text())
        self.assertEqual(KF.parse_sums(text), [(e['sha256'], e['path'])
                                               for e in sorted(kit.entries, key=lambda e: parts(e['path']))])
        for bad in ('abc  x.wav\n', f'{"0" * 64} x.wav\n', f'{"0" * 64}  ../x.wav\n', ''):
            with self.subTest(bad=bad):
                with self.assertRaises(ValueError):
                    KF.parse_sums(bad)

    def test_the_default_content_folder_is_where_the_plugin_looks(self):
        home = Path('/Users/someone')
        self.assertEqual(KF.default_content_dir('Darwin', home),
                         home / 'Library' / 'Application Support' / 'Wubansia Audio' / 'Wubansia' / 'Content')
        self.assertEqual(KF.default_content_dir('Linux', home),
                         home / '.local' / 'share' / 'Wubansia Audio' / 'Wubansia' / 'Content')
        self.assertIsNone(KF.default_content_dir('Windows', home))  # not decided yet: --dest is required there


# ------------------------------------------------------------------ pack.py

class TestPack(Case):
    def expected_names(self, kit):
        top = 'Wubansia Content/'
        kitdir = f'{top}{kit.folder}/v{kit.version}/'
        names = [top, top + 'README.txt', f'{top}{kit.folder}/', kitdir, kitdir + 'SHA256SUMS']
        for path in sorted(kit.paths, key=parts):
            p = parts(path)
            for i in range(1, len(p)):
                d = kitdir + '/'.join(p[:i]) + '/'
                if d not in names:
                    names.append(d)
            names.append(kitdir + path)
        return names

    def test_the_zip_holds_exactly_the_listed_files_with_sha256sums_and_the_readme(self):
        kit = SynKit(self.tmp)
        zpath = self.packed(kit)
        with zipfile.ZipFile(zpath) as z:
            self.assertEqual(z.namelist(), self.expected_names(kit))
            kitdir = f'Wubansia Content/{kit.folder}/v{kit.version}/'
            for path, data in kit.data.items():
                self.assertEqual(z.read(kitdir + path), data, path)
            self.assertEqual(z.read(kitdir + 'SHA256SUMS').decode(), kit.sums_text())
            self.assertEqual(z.read('Wubansia Content/README.txt'), README_TXT.read_bytes())
            self.assertIsNone(z.testzip())
        for name in UNLISTED:
            self.assertNotIn(name, ' '.join(zipfile.ZipFile(zpath).namelist()))

    def test_two_runs_give_the_same_bytes(self):
        kit = SynKit(self.tmp)
        a = self.packed(kit, self.tmp / 'a')
        for p in kit.samples.rglob('*.wav'):  # other modification times, and the manifest in another order
            os.utime(p, (1_700_000_000, 1_700_000_000))
        kit.write_manifest(list(reversed(kit.entries)))
        b = self.packed(kit, self.tmp / 'b')
        self.assertEqual(a.read_bytes(), b.read_bytes())

    def test_entries_have_fixed_dates_and_modes(self):
        zpath = self.packed(SynKit(self.tmp))
        with zipfile.ZipFile(zpath) as z:
            for info in z.infolist():
                with self.subTest(info.filename):
                    self.assertEqual(info.date_time, (1980, 1, 1, 0, 0, 0))
                    self.assertEqual(info.create_system, 3)  # Unix: the modes below are honoured
                    mode = info.external_attr >> 16
                    if info.filename.endswith('/'):
                        self.assertEqual(mode, stat.S_IFDIR | 0o755)
                    else:
                        self.assertEqual(mode, stat.S_IFREG | 0o644)
                        self.assertEqual(info.compress_type, zipfile.ZIP_DEFLATED)

    def test_it_refuses_what_the_plugin_would_refuse(self):
        def broken(name, change):
            kit = SynKit(self.tmp / name)
            change(kit)
            return kit, run_pack(kit, '--out', self.tmp / name / 'out')

        path = 'Bass/Bass A D2.wav'
        cases = {
            'an LFS pointer': (lambda k: k.write_sample(path, lfs_pointer(k.data[path])), 'LFS pointer'),
            'a missing file': (lambda k: k.sample(path).unlink(), 'missing'),
            'other bytes': (lambda k: k.write_sample(path, wav_bytes(1, 2400, 'other')), 'SHA-256'),
            'a 44.1 kHz file': (lambda k: k.write_sample(path, wav_bytes(1, 2400, 'x', rate=44100)), '44100'),
            'other frames': (lambda k: k.write_sample(path, wav_bytes(1, 2399, 'x')), 'frames'),
        }
        for name, (change, words) in cases.items():
            with self.subTest(name):
                kit, proc = broken(name.replace(' ', '_'), change)
                self.assertFails(proc)
                self.assertIn(path, proc.stderr)
                self.assertIn(words, proc.stderr)
                out = kit.root / 'out'
                self.assertEqual(list(out.iterdir()) if out.exists() else [], [])  # no zip, not even a partial one

    def test_an_lfs_pointer_says_how_to_get_the_samples(self):
        kit = SynKit(self.tmp)
        for path, data in kit.data.items():
            kit.write_sample(path, lfs_pointer(data))
        proc = run_pack(kit, '--out', self.tmp / 'out')
        self.assertFails(proc)
        self.assertIn('git lfs pull', proc.stderr)
        self.assertLess(len(proc.stderr.splitlines()), 20)  # a summary, not 31 lines

    def test_it_packs_from_a_kit_v_n_folder_first(self):
        kit = SynKit(self.tmp)
        installed = self.tmp / 'installed'
        shutil.copytree(kit.samples / kit.folder, installed / kit.folder / 'v1')
        shutil.rmtree(kit.samples)
        self.assertOk(run_pack(kit, '--out', self.tmp / 'out', samples=installed))

    def test_list_prints_the_listed_files_for_git_lfs(self):
        kit = SynKit(self.tmp)
        proc = run_pack(kit, '--list')
        self.assertOk(proc)
        self.assertEqual(proc.stdout.splitlines(), [str(kit.samples / kit.folder / p) for p in kit.paths])

    def test_list_defaults_to_repo_relative_paths(self):
        kit = SynKit(self.tmp)
        proc = run([sys.executable, PACK_PY, '--kit', kit.id, '--manifests', kit.manifests, '--list'], cwd=REPO)
        self.assertOk(proc)
        self.assertEqual(proc.stdout.splitlines()[0], f'content/samples/{kit.folder}/{kit.paths[0]}')

    def test_every_kit_by_default_and_a_line_per_zip(self):
        a = SynKit(self.tmp, 'SYN01')
        b = SynKit(self.tmp, 'SYN02', version=2)
        proc = run([sys.executable, PACK_PY, '--manifests', a.manifests, '--samples', a.samples,
                    '--out', self.tmp / 'out'])
        self.assertOk(proc)
        self.assertEqual(sorted(p.name for p in (self.tmp / 'out').iterdir()), [a.zip_name, b.zip_name])
        for k in (a, b):
            zpath = self.tmp / 'out' / k.zip_name
            self.assertIn(f'{zpath.name}', proc.stdout)
            self.assertIn(sha256(zpath.read_bytes()), proc.stdout)

    def test_the_readme_is_plain_text_without_artist_names(self):
        text = README_TXT.read_text(encoding='utf-8')
        self.assertTrue(text.endswith('\n'))
        self.assertNotIn('\r', text)
        for needed in ('install-macos.sh', 'SHA256SUMS', 'Application Support/Wubansia Audio/Wubansia/Content',
                       'Choose content folder'):
            self.assertIn(needed, text)
        try:
            from tools.sound import denylist
        except ImportError:  # pragma: no cover
            self.skipTest('tools/sound/denylist.py is not importable')
        for name in ('artists', 'slurs'):
            self.assertFalse(denylist.hits(text, denylist.load(name)), name)


# ------------------------------------------------------------------ install.py

class TestInstall(Case):
    def dest_of(self, kit, dest=None):
        return (dest or self.tmp / 'Content') / kit.folder / f'v{kit.version}'

    def expected_tree(self, kit):
        out = {p: sha256(d) for p, d in kit.data.items()}
        out['SHA256SUMS'] = sha256(kit.sums_text().encode())
        return out

    def test_from_repo_copies_only_the_listed_files_into_kit_v_n(self):
        kit = SynKit(self.tmp)
        proc = run_install(kit, '--from-repo', kit.samples, '--dest', self.tmp / 'Content')
        self.assertOk(proc)
        self.assertEqual(tree(self.tmp / 'Content'), {f'{kit.folder}/v1/{p}': h
                                                      for p, h in self.expected_tree(kit).items()})
        self.assertIn('installed 31 files', proc.stdout)

    def test_from_repo_takes_a_clone_too(self):
        kit = SynKit(self.tmp)
        clone = self.tmp / 'clone'
        shutil.copytree(kit.samples, clone / 'content' / 'samples')
        self.assertOk(run_install(kit, '--from-repo', clone, '--dest', self.tmp / 'Content'))
        self.assertEqual(tree(self.dest_of(kit)), self.expected_tree(kit))

    def test_the_default_repo_is_this_checkout(self):
        self.assertEqual(INST.repo_samples(None), REPO / 'content' / 'samples')

    def test_a_second_run_changes_nothing(self):
        kit = SynKit(self.tmp)
        dest = self.tmp / 'Content'
        self.assertOk(run_install(kit, '--from-repo', kit.samples, '--dest', dest))
        before = {p: os.stat(p).st_mtime_ns for p in dest.rglob('*')}
        proc = run_install(kit, '--from-repo', kit.samples, '--dest', dest)
        self.assertOk(proc)
        self.assertIn('already installed', proc.stdout)
        self.assertEqual({p: os.stat(p).st_mtime_ns for p in dest.rglob('*')}, before)

    def test_it_refuses_lfs_pointers_and_mismatches_and_copies_nothing(self):
        path = 'Vox/Vox G4 v2.wav'
        cases = {
            'an LFS pointer': (lambda k: k.write_sample(path, lfs_pointer(k.data[path])), 'LFS pointer'),
            'other bytes': (lambda k: k.write_sample(path, wav_bytes(2, 2496, 'other')), 'SHA-256'),
            'other frames': (lambda k: k.write_sample(path, wav_bytes(2, 2000, 'x')), 'frames'),
            'a missing file': (lambda k: k.sample(path).unlink(), 'missing'),
        }
        for name, (change, words) in cases.items():
            with self.subTest(name):
                kit = SynKit(self.tmp / name.replace(' ', '_'))
                change(kit)
                dest = kit.root / 'Content'
                proc = run_install(kit, '--from-repo', kit.samples, '--dest', dest)
                self.assertFails(proc)
                self.assertIn(path, proc.stdout + proc.stderr)
                self.assertIn(words, proc.stdout + proc.stderr)
                self.assertEqual(tree(dest), {})

    def test_frames_are_checked_even_when_the_hash_matches(self):
        kit = SynKit(self.tmp)
        entries = [dict(e, frames=e['frames'] + 1) if e['path'] == 'FX/Riser.wav' else e for e in kit.entries]
        kit.write_manifest(entries)
        proc = run_install(kit, '--from-repo', kit.samples, '--dest', self.tmp / 'Content')
        self.assertFails(proc)
        self.assertIn('FX/Riser.wav', proc.stdout + proc.stderr)
        self.assertIn('frames', proc.stdout + proc.stderr)

    def test_it_never_overwrites_a_different_version(self):
        kit = SynKit(self.tmp)
        dest = self.tmp / 'Content'
        other = self.dest_of(kit, dest) / 'Drums' / 'Kick A.wav'
        other.parent.mkdir(parents=True)
        other.write_bytes(b'another build of v1')
        v2 = dest / kit.folder / 'v2' / 'Drums' / 'Kick A.wav'
        v2.parent.mkdir(parents=True)
        v2.write_bytes(b'content v2')
        before = tree(dest)
        proc = run_install(kit, '--from-repo', kit.samples, '--dest', dest)
        self.assertFails(proc)
        self.assertIn('Drums/Kick A.wav', proc.stdout + proc.stderr)
        self.assertIn('differs', proc.stdout + proc.stderr)
        self.assertEqual(tree(dest), before)

    def test_a_build_that_adds_files_to_the_same_version_adds_them(self):
        # Every sample already there is identical, so the folder serves both builds: the new files go in, and
        # SHA256SUMS then describes the new build.
        kit = SynKit(self.tmp / 'a', files=SYN_FILES[:-1])
        dest = self.tmp / 'Content'
        self.assertOk(run_install(kit, '--from-repo', kit.samples, '--dest', dest))
        bigger = SynKit(self.tmp / 'b')
        proc = run_install(bigger, '--from-zip', self.packed(bigger), '--dest', dest)
        self.assertOk(proc)
        self.assertIn('installed 1 file (', proc.stdout)
        self.assertEqual(tree(self.dest_of(bigger, dest)), self.expected_tree(bigger))

    def test_it_fills_in_a_half_installed_kit(self):
        kit = SynKit(self.tmp)
        dest = self.tmp / 'Content'
        self.assertOk(run_install(kit, '--from-repo', kit.samples, '--dest', dest))
        for p in ('Drums/Open Hat.wav', 'Stems/Lead Phrase F#4 4 Bars.wav', 'SHA256SUMS'):
            (self.dest_of(kit, dest) / p).unlink()
        proc = run_install(kit, '--from-repo', kit.samples, '--dest', dest)
        self.assertOk(proc)
        self.assertIn('installed 2 files', proc.stdout)  # the samples; SHA256SUMS comes along uncounted
        self.assertIn('29 were already there', proc.stdout)
        self.assertEqual(tree(self.dest_of(kit, dest)), self.expected_tree(kit))

    def test_a_dry_run_lists_its_actions_and_writes_nothing(self):
        kit = SynKit(self.tmp)
        dest = self.tmp / 'Content'
        proc = run_install(kit, '--from-repo', kit.samples, '--dest', dest, '--dry-run')
        self.assertOk(proc)
        self.assertFalse(dest.exists())
        for path in kit.paths:
            self.assertIn(f'would copy {path}', proc.stdout)
        self.assertIn('would write SHA256SUMS', proc.stdout)
        self.assertIn(str(self.dest_of(kit, dest)), proc.stdout)
        self.assertIn('dry run', proc.stdout.lower())

    def test_kit_chooses_among_the_kits(self):
        a = SynKit(self.tmp, 'SYN01')
        SynKit(self.tmp, 'SYN02', version=2)
        dest = self.tmp / 'Content'
        self.assertOk(run_install(a, '--from-repo', a.samples, '--dest', dest, '--kit', 'syn02'))
        self.assertEqual(sorted(p.name for p in dest.iterdir()), ['syn02'])
        self.assertEqual(sorted(p.name for p in (dest / 'syn02').iterdir()), ['v2'])
        proc = run_install(a, '--from-repo', a.samples, '--dest', dest, '--kit', 'SYN09')
        self.assertFails(proc, 2)

    def test_from_zip_installs_a_packed_kit(self):
        kit = SynKit(self.tmp)
        zpath = self.packed(kit)
        dest = self.tmp / 'Content'
        proc = run_install(kit, '--from-zip', zpath, '--dest', dest)
        self.assertOk(proc)
        self.assertEqual(tree(dest), {f'{kit.folder}/v1/{p}': h for p, h in self.expected_tree(kit).items()})
        # The same folder as from the repo: either route gives the plugin the same thing.
        other = self.tmp / 'Content from repo'
        self.assertOk(run_install(kit, '--from-repo', kit.samples, '--dest', other))
        self.assertEqual(tree(dest), tree(other))

    def test_from_zip_needs_no_manifest_and_refuses_one_that_disagrees(self):
        kit = SynKit(self.tmp)
        zpath = self.packed(kit)
        nowhere = self.tmp / 'no manifests'
        proc = run([sys.executable, INSTALL_PY, '--manifests', nowhere, '--from-zip', zpath,
                    '--dest', self.tmp / 'A'])
        self.assertOk(proc)
        self.assertEqual(tree(self.tmp / 'A' / kit.folder / 'v1'), self.expected_tree(kit))
        kit.write_manifest([dict(e, sha256='0' * 64) if e['path'] == 'FX/Riser.wav' else e for e in kit.entries])
        proc = run_install(kit, '--from-zip', zpath, '--dest', self.tmp / 'B')
        self.assertFails(proc)
        self.assertIn('FX/Riser.wav', proc.stdout + proc.stderr)
        self.assertEqual(tree(self.tmp / 'B'), {})

    def test_from_zip_refuses_a_damaged_zip(self):
        kit = SynKit(self.tmp)
        zpath = self.packed(kit)
        bad = self.tmp / 'bad.zip'
        kitdir = f'Wubansia Content/{kit.folder}/v1/'
        with zipfile.ZipFile(zpath) as src, zipfile.ZipFile(bad, 'w') as out:
            for info in src.infolist():
                data = src.read(info)
                if info.filename == kitdir + 'Music/Pad G3.wav':
                    data = data[:-3] + b'\0\0\0'
                out.writestr(info, data)
        proc = run_install(kit, '--from-zip', bad, '--dest', self.tmp / 'Content')
        self.assertFails(proc)
        self.assertIn('Music/Pad G3.wav', proc.stdout + proc.stderr)
        self.assertEqual(tree(self.tmp / 'Content'), {})

    def test_from_zip_dry_run_writes_nothing(self):
        kit = SynKit(self.tmp)
        zpath = self.packed(kit)
        proc = run_install(kit, '--from-zip', zpath, '--dest', self.tmp / 'Content', '--dry-run')
        self.assertOk(proc)
        self.assertFalse((self.tmp / 'Content').exists())
        self.assertIn('would copy Drums/Kick A.wav', proc.stdout)

    def test_a_run_leaves_no_temporary_files(self):
        kit = SynKit(self.tmp)
        dest = self.tmp / 'Content'
        self.assertOk(run_install(kit, '--from-zip', self.packed(kit), '--dest', dest))
        leftovers = [p for p in dest.rglob('*') if p.name.startswith('.')]
        self.assertEqual(leftovers, [])


# ------------------------------------------------------------------ dist/install-macos.sh

STUBS = {
    # macOS's own commands, as stand-ins on Linux. Each logs its arguments to $STUB_LOG.
    'uname': 'case "$1" in -s|"") echo Darwin ;; *) exec {uname} "$@" ;; esac\n',
    'id': 'case "$1" in -u) echo 501 ;; *) exec {id} "$@" ;; esac\n',
    'xattr': 'echo "xattr $*" >> "$STUB_LOG"\ncase "$1" in -p) exit 1 ;; esac\n',
    'codesign': 'echo "codesign $*" >> "$STUB_LOG"\n',
    'killall': 'echo "killall $*" >> "$STUB_LOG"\n',
}

BUNDLES = ('Wubansia.vst3', 'Wubansia.component', 'Wubansia.clap', 'Wubansia.app')


@unittest.skipUnless(BASH and UNZIP, 'needs bash and unzip')
class TestInstallMacos(Case):
    def setUp(self):
        super().setUp()
        self.home = self.tmp / 'home folder'
        self.home.mkdir()
        self.downloads = self.tmp / 'Downloads'
        self.pkg = self.downloads / 'Wubansia'  # the unzipped CI zip
        self.pkg.mkdir(parents=True)
        for b in BUNDLES:
            exe = self.pkg / b / 'Contents' / 'MacOS' / 'Wubansia'
            exe.parent.mkdir(parents=True)
            exe.write_bytes(b'\xcf\xfa\xed\xfe' + b.encode())
            (self.pkg / b / 'Contents' / 'Info.plist').write_text('<plist/>\n')
        for f in (SCRIPT, INSTALL_MD, REPO / 'THIRD_PARTY_NOTICES.md'):
            shutil.copy2(f, self.pkg / f.name)
        self.tmpdir = self.tmp / 'tmp'
        self.tmpdir.mkdir()
        self.log = self.tmp / 'stub.log'

    def content_dir(self):
        return self.home / 'Library' / 'Application Support' / 'Wubansia Audio' / 'Wubansia' / 'Content'

    def plugins_dir(self):
        return self.home / 'Library' / 'Audio' / 'Plug-Ins'

    def stubs(self):
        bin_dir = self.tmp / 'stub bin'
        bin_dir.mkdir(exist_ok=True)
        for name, body in STUBS.items():
            p = bin_dir / name
            p.write_text('#!/bin/sh\n' + body.format(uname=shutil.which('uname'), id=shutil.which('id')))
            p.chmod(0o755)
        return {'PATH': f'{bin_dir}{os.pathsep}{os.environ["PATH"]}', 'STUB_LOG': str(self.log)}

    def script(self, *args, stubs=False, env=None):
        full = {'HOME': str(self.home), 'TMPDIR': str(self.tmpdir)}
        if stubs:
            full.update(self.stubs())
        full.update(env or {})
        return run([BASH, self.pkg / 'install-macos.sh', *args], env=full, cwd=self.tmp)

    def zip_beside(self, kit):
        zpath = self.packed(kit, self.tmp / 'packed')
        target = self.downloads / zpath.name
        shutil.copy2(zpath, target)
        return target

    def assertNothingWritten(self):
        self.assertEqual(list(self.home.rglob('*')), [])
        self.assertEqual(list(self.tmpdir.rglob('*')), [])

    def test_shellcheck_passes(self):
        shellcheck = shutil.which('shellcheck')
        if not shellcheck:
            self.skipTest('shellcheck is not installed')
        self.assertOk(run([shellcheck, '--shell=bash', SCRIPT]))

    def test_the_syntax_parses(self):
        self.assertOk(run([BASH, '-n', SCRIPT]))

    def test_it_uses_nothing_newer_than_bash_3_2(self):
        # macOS's bash is 3.2: no associative arrays, mapfile, case changes, |&, ;& or ;;&, -v tests, namerefs,
        # {fd} redirections or ${x@Q}. No arrays at all: "${a[@]}" on an empty one fails under set -u before 4.4.
        text = SCRIPT.read_text()
        code = '\n'.join(line.split(' #', 1)[0] for line in text.splitlines() if not line.lstrip().startswith('#'))
        for pattern in (r'declare\s+-[aA]', r'\bmapfile\b', r'\breadarray\b', r'\$\{\w+(,,?|\^\^?)\}', r'\|&',
                        r';;&', r';&', r'\[\[\s+-v', r'local\s+-n', r'\{\w+\}[<>]', r'@[QEPAa]\}', r'\[@\]',
                        r'\[\*\]', r'\bcoproc\b', r'\bwait\s+-n', r'\bshopt\s+-s\s+globstar', r'=~'):
            self.assertIsNone(re.search(pattern, code), pattern)
        # "$@" with no arguments counts as unbound under set -u in some old bash versions: ${1+"$@"} is safe.
        self.assertEqual(code.count('"$@"'), code.count('${1+"$@"}'))

    def test_it_is_executable_in_git(self):
        if not (REPO / '.git').exists():
            self.skipTest('not a git checkout')
        proc = run(['git', '-C', REPO, 'ls-files', '-s', 'dist/install-macos.sh'])
        self.assertOk(proc)
        self.assertTrue(proc.stdout.startswith('100755 '), proc.stdout)

    def test_it_refuses_other_shells(self):
        dash = shutil.which('dash')  # macOS has /bin/dash too; its /bin/sh is bash, which the script accepts
        if not dash:
            self.skipTest('dash is not installed')
        proc = run([dash, SCRIPT, '--dry-run'], env={'HOME': str(self.home)})
        self.assertEqual(proc.returncode, 2, proc.stdout + proc.stderr)
        self.assertIn('bash install-macos.sh', proc.stderr)

    def test_bad_options_are_usage_errors(self):
        for args in (('--bogus',), ('--plugins-only', '--content-only')):
            with self.subTest(args=args):
                proc = self.script('--dry-run', *args)
                self.assertFails(proc, 2)
                self.assertIn('usage', proc.stderr.lower())
        self.assertOk(self.script('--help'))

    @unittest.skipIf(ON_MAC, 'on a Mac it would install for real')
    def test_on_linux_it_runs_only_as_a_dry_run(self):
        self.zip_beside(SynKit(self.tmp / 'kit'))
        proc = self.script()
        self.assertFails(proc, 2)
        self.assertIn('--dry-run', proc.stderr)
        self.assertNothingWritten()

    def test_a_dry_run_checks_the_zips_and_prints_every_step(self):
        kit = SynKit(self.tmp / 'kit')
        zpath = self.zip_beside(kit)
        proc = self.script('--dry-run')
        self.assertOk(proc)
        out = proc.stdout
        apps = '/Applications' if Path('/Applications').is_dir() and os.access('/Applications', os.W_OK) \
            else str(self.home / 'Applications')
        for b, folder in zip(BUNDLES, ('VST3', 'Components', 'CLAP')):
            self.assertIn(str(self.plugins_dir() / folder / b), out)
        self.assertIn(f'{apps}/Wubansia.app', out)
        self.assertIn('quarantine', out)
        self.assertIn(str(zpath), out)
        self.assertIn(f'{len(kit.paths)} files', out)
        self.assertIn('SHA-256', out)
        self.assertIn(str(self.content_dir() / kit.folder / 'v1'), out)
        self.assertIn('AudioComponentRegistrar', out)
        self.assertIn('dry run', out.lower())
        self.assertNothingWritten()

    def test_a_dry_run_finds_an_unzipped_content_folder_too(self):
        # Safari unzips downloads: the zip is gone and a "Wubansia Content" folder is there instead.
        kit = SynKit(self.tmp / 'kit')
        zpath = self.packed(kit, self.tmp / 'packed')
        with zipfile.ZipFile(zpath) as z:
            z.extractall(self.downloads)
        (self.downloads / 'Wubansia Content').rename(self.downloads / 'Wubansia Content 2')
        proc = self.script('--dry-run', '--content-only')
        self.assertOk(proc)
        self.assertIn('Wubansia Content 2', proc.stdout)
        self.assertIn(str(self.content_dir() / kit.folder / 'v1'), proc.stdout)
        self.assertNothingWritten()

    def test_a_dry_run_takes_zips_named_on_the_command_line(self):
        kit = SynKit(self.tmp / 'kit')
        zpath = self.packed(kit, self.tmp / 'somewhere else')
        proc = self.script('--dry-run', '--content-only', zpath)
        self.assertOk(proc)
        self.assertIn(str(zpath), proc.stdout)
        self.assertIn(str(self.content_dir() / kit.folder / 'v1'), proc.stdout)

    def test_a_dry_run_refuses_a_damaged_zip(self):
        kit = SynKit(self.tmp / 'kit')
        zpath = self.zip_beside(kit)
        kitdir = f'Wubansia Content/{kit.folder}/v1/'
        damaged = self.tmp / 'damaged.zip'
        with zipfile.ZipFile(zpath) as src, zipfile.ZipFile(damaged, 'w') as out:
            for info in src.infolist():
                data = src.read(info)
                if info.filename == kitdir + 'Drums/Open Hat.wav':
                    data = data[:-1] + bytes([data[-1] ^ 1])
                out.writestr(info, data)
        shutil.move(damaged, zpath)
        proc = self.script('--dry-run', '--content-only')
        self.assertFails(proc)
        self.assertIn('Drums/Open Hat.wav', proc.stdout + proc.stderr)
        self.assertNothingWritten()

    def test_plugins_only_and_content_only(self):
        kit = SynKit(self.tmp / 'kit')
        self.zip_beside(kit)
        plugins = self.script('--dry-run', '--plugins-only')
        self.assertOk(plugins)
        self.assertIn('Wubansia.vst3', plugins.stdout)
        self.assertNotIn(str(self.content_dir()), plugins.stdout)
        content = self.script('--dry-run', '--content-only')
        self.assertOk(content)
        self.assertNotIn('Wubansia.vst3', content.stdout)
        self.assertNotIn('AudioComponentRegistrar', content.stdout)
        self.assertIn(str(self.content_dir()), content.stdout)

    def test_without_content_it_says_how_to_add_it(self):
        proc = self.script('--dry-run')
        self.assertOk(proc)
        self.assertIn('Wubansia-Content-', proc.stdout)
        self.assertIn('--content-only', proc.stdout)

    def test_content_only_with_no_content_fails(self):
        proc = self.script('--dry-run', '--content-only')
        self.assertFails(proc)
        self.assertIn('Wubansia-Content-', proc.stdout + proc.stderr)

    def real_content_run(self, *args):
        # A real run that only writes under the fake home folder: on a Mac for real, on Linux with the stand-ins.
        return self.script('--content-only', *args, stubs=not ON_MAC)

    @unittest.skipUnless(ON_MAC or ON_LINUX, 'macOS, or Linux with stand-ins')
    def test_a_real_content_install_against_a_fake_home(self):
        kit = SynKit(self.tmp / 'kit')
        self.zip_beside(kit)
        proc = self.real_content_run()
        self.assertOk(proc)
        dest = self.content_dir() / kit.folder / 'v1'
        expected = {p: sha256(d) for p, d in kit.data.items()}
        expected['SHA256SUMS'] = sha256(kit.sums_text().encode())
        self.assertEqual(tree(dest), expected)
        self.assertEqual([p.name for p in self.content_dir().iterdir()], [kit.folder])  # no staging left
        self.assertEqual(list(self.tmpdir.iterdir()), [])
        self.assertIn(f'installed {len(kit.paths)} files', proc.stdout)

        again = self.real_content_run()
        self.assertOk(again)
        self.assertIn('already installed', again.stdout)
        self.assertEqual(tree(dest), expected)

        (dest / 'Drums' / 'Kick A.wav').write_bytes(b'another build of v1')
        (dest / 'Music' / 'Pad G3.wav').unlink()
        changed = self.real_content_run()
        self.assertFails(changed)
        self.assertIn('Drums/Kick A.wav', changed.stdout + changed.stderr)
        self.assertIn('differs', changed.stdout + changed.stderr)
        self.assertEqual((dest / 'Drums' / 'Kick A.wav').read_bytes(), b'another build of v1')
        self.assertFalse((dest / 'Music' / 'Pad G3.wav').exists())  # nothing changes when a kit is refused

    @unittest.skipUnless(ON_LINUX, 'stand-ins for the macOS commands; on a Mac it would write /Applications')
    def test_a_real_install_with_stand_ins_for_the_macos_commands(self):
        kit = SynKit(self.tmp / 'kit')
        self.zip_beside(kit)
        old = self.plugins_dir() / 'VST3' / 'Wubansia.vst3'
        (old / 'Contents').mkdir(parents=True)
        (old / 'Contents' / 'stale').write_text('an older build')
        proc = self.script(stubs=True)
        self.assertOk(proc)
        apps = self.home / 'Applications'  # Linux has no /Applications to write to
        installed = [self.plugins_dir() / 'VST3' / BUNDLES[0], self.plugins_dir() / 'Components' / BUNDLES[1],
                     self.plugins_dir() / 'CLAP' / BUNDLES[2], apps / BUNDLES[3]]
        for b, dest in zip(BUNDLES, installed):
            self.assertEqual(tree(dest), tree(self.pkg / b), b)
        self.assertFalse((old / 'Contents' / 'stale').exists())
        self.assertEqual([p.name for p in self.plugins_dir().joinpath('VST3').iterdir()], ['Wubansia.vst3'])
        log = self.log.read_text().splitlines()
        for dest in installed:
            self.assertIn(f'xattr -dr com.apple.quarantine {dest}', log)
            self.assertIn(f'codesign --verify --strict {dest}', log)
        self.assertEqual(log.count('killall -9 AudioComponentRegistrar'), 1)
        self.assertEqual(tree(self.content_dir() / kit.folder / 'v1')['Drums/Kick A.wav'],
                         sha256(kit.data['Drums/Kick A.wav']))
        for line in ('Wubansia.vst3', 'Wubansia.component', 'Wubansia.clap', 'Wubansia.app', 'quarantine',
                     'AudioComponentRegistrar', str(self.content_dir() / kit.folder / 'v1')):
            self.assertIn(line, proc.stdout)

    @unittest.skipUnless(ON_LINUX, 'stand-ins for the macOS commands')
    def test_it_refuses_sudo(self):
        stubs = self.stubs()
        (self.tmp / 'stub bin' / 'id').write_text('#!/bin/sh\necho 0\n')
        proc = run([BASH, self.pkg / 'install-macos.sh'], env={'HOME': str(self.home), **stubs})
        self.assertFails(proc, 2)
        self.assertIn('sudo', proc.stderr)
        self.assertEqual(list(self.home.rglob('*')), [])


WORKFLOW = REPO / '.github' / 'workflows' / 'ci.yml'


def job_block(text, name):
    """A job's lines in ci.yml: from '  <name>:' to the next job (no YAML parser in CI's venv)."""
    m = re.search(rf'^  {name}:\n(.*?)(?=^  [a-z]+:\n|\Z)', text, re.M | re.S)
    if not m:
        raise AssertionError(f'no job {name} in {WORKFLOW}')
    return m.group(1)


class TestWorkflow(unittest.TestCase):
    def setUp(self):
        self.text = WORKFLOW.read_text(encoding='utf-8')

    def test_the_content_job_runs_only_when_dispatched_with_content(self):
        content = job_block(self.text, 'content')
        self.assertIn("if: github.event_name == 'workflow_dispatch' && inputs.platforms == 'content'\n", content)
        self.assertIn('options: [all, linux, macos, windows, content]', self.text)
        for job in ('linux', 'sound', 'macos', 'windows'):
            condition = job_block(self.text, job).split('runs-on:')[0]
            self.assertNotIn('content', condition, job)  # platforms=content runs nothing else; all runs no content

    def test_the_content_job_caches_the_samples_by_the_manifests_hash(self):
        content = job_block(self.text, 'content')
        self.assertIn('uses: actions/cache@', content)
        self.assertIn('path: .git/lfs/objects', content)
        self.assertIn("key: content-lfs-${{ matrix.kit }}-${{ hashFiles(format('content/plugin/{0}/*_kit.json', "
                      "matrix.kit)) }}", content)
        self.assertIn('tools/content/pack.py --kit "$KIT" --list', content)  # only the listed samples come down
        self.assertIn('git lfs pull --include', content)
        self.assertIn('retention-days: 3', content)
        self.assertIsNone(re.search(r'^\s*lfs:\s*true', content, re.M))  # the checkout itself fetches no samples

    def test_the_macos_zip_carries_the_installer_and_is_checked(self):
        macos = job_block(self.text, 'macos')
        self.assertIn('cp dist/INSTALL.md dist/install-macos.sh dist/Wubansia/', macos)
        check = macos.split('name: Check the package')[1].split('- name:')[0]
        for item in ('Wubansia.vst3', 'Wubansia.component', 'Wubansia.clap', 'Wubansia.app',
                     'THIRD_PARTY_NOTICES.md', 'INSTALL.md', 'install-macos.sh'):
            self.assertIn(item, check)
        self.assertIn('codesign --verify --strict', check)
        self.assertIn('install-macos.sh" --dry-run', check)
        self.assertLess(macos.index('name: Check the package'), macos.index('name: Upload'))

    def test_actionlint_passes(self):
        actionlint = os.environ.get('WUBANSIA_ACTIONLINT') or shutil.which('actionlint')
        if not actionlint:
            self.skipTest('actionlint is not installed (set WUBANSIA_ACTIONLINT to its path)')
        proc = run([actionlint, WORKFLOW], cwd=REPO)
        self.assertEqual(proc.returncode, 0, proc.stdout + proc.stderr)


class TestDocs(unittest.TestCase):
    def test_install_md_names_every_step_the_script_takes(self):
        text = INSTALL_MD.read_text(encoding='utf-8')
        for needed in ('bash install-macos.sh', '--dry-run', '--plugins-only', '--content-only',
                       'Library/Audio/Plug-Ins/VST3', 'Library/Audio/Plug-Ins/Components', 'Library/Audio/Plug-Ins/CLAP',
                       '/Applications', 'com.apple.quarantine', 'AudioComponentRegistrar', 'SHA256SUMS',
                       'Application Support/Wubansia Audio/Wubansia/Content', 'Choose content folder'):
            self.assertIn(needed, text)

    def test_the_script_and_the_docs_agree_on_the_folders(self):
        script = SCRIPT.read_text()
        self.assertIn('Library/Application Support/Wubansia Audio/Wubansia/Content', script)
        self.assertIn('Library/Audio/Plug-Ins', script)
        self.assertIn(KF.ZIP_ROOT, script)
        self.assertIn(KF.SUMS, script)


if __name__ == '__main__':
    unittest.main()
