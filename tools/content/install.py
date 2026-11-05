#!/usr/bin/env python3
"""Installs kits' samples where the plugin looks for them: <dest>/<kit>/v<N>/ (ADR 0022).

    python3 tools/content/install.py --from-repo [PATH] [--kit FUT01] [--dest DIR] [--dry-run]
    python3 tools/content/install.py --from-zip ZIP [ZIP ...] [--kit FUT01] [--dest DIR] [--dry-run]

--from-repo copies, from a clone's content/samples (this checkout's by default; PATH may be a clone or a folder of
<kit>/ folders), exactly the files each kit's kit.json (content/plugin/<kit>/<KIT>_kit.json) lists, after checking
each as the plugin does (a 48 kHz WAV with the listed frames and channels, not a Git LFS pointer) and its SHA-256.
--from-zip installs the content zips pack.py makes, checking every file against the zip's SHA256SUMS, and against
kit.json as well when this checkout has that kit at that content version.

The default --dest is the plugin's content folder:
    macOS  ~/Library/Application Support/Wubansia Audio/Wubansia/Content
    Linux  ~/.local/share/Wubansia Audio/Wubansia/Content
(Windows has none yet: pass --dest.) Each kit also gets its SHA256SUMS, so a folder installed from the repo and
one installed from a zip are the same.

It never replaces a sample that is already there with other bytes: that is another build of the same content
version (a different version has its own v<N> folder). Every file is checked before anything is written, and a kit
with any problem or any such sample is left exactly as it was. Samples already there with the right bytes stay, so a
second run changes nothing, and a build that only adds files to a version adds them; SHA256SUMS then describes the
latest one. Each file is written under a temporary name and renamed into place. --dry-run checks everything and
lists every file it would copy, and writes nothing. Exit status: 0 done, 1 a kit was refused, 2 a
usage error. Standard library only (Python 3.9+).
"""
import argparse
import os
import re
import sys
import zipfile
from pathlib import Path

try:
    from . import kitfiles as KF  # imported as tools.content.install (the tests)
except ImportError:
    import kitfiles as KF  # run as a script

ZIP_SUMS = re.compile(re.escape(KF.ZIP_ROOT) + r'/([a-z]{3}[0-9]{2})/v([0-9]{1,3})/' + re.escape(KF.SUMS) + r'\Z')


class Source:
    """One kit at one content version from one place: its files [(path, sha256)] in SHA256SUMS's order, the
    SHA256SUMS bytes to install with them, how to read a file, and how to check its bytes."""

    def __init__(self, kit_id, version, where, items, sums, read, check):
        self.kit_id = kit_id
        self.version = version
        self.where = where      # for messages: a folder or a zip
        self.items = items
        self.sums = sums
        self.read = read        # path -> bytes
        self.check = check      # (path, sha256, bytes) -> a problem or None
        self.problems = []      # found before any file is read

    @property
    def label(self):
        return f'{self.kit_id} v{self.version}'


def repo_samples(path):
    """The samples folder --from-repo reads: this checkout's content/samples, PATH/content/samples for a clone, else
    PATH."""
    if path is None:
        return KF.SAMPLES
    p = Path(path)
    return p / 'content' / 'samples' if (p / 'content' / 'samples').is_dir() else p


def repo_source(kit, samples):
    src = KF.kit_source(samples, kit)
    files = {f.path: f for f in kit.files}
    source = Source(kit.id, kit.version, str(src), [(f.path, f.sha256) for f in KF.ordered(kit.files)],
                    KF.sums_text(kit.files).encode('utf-8'), lambda p: (src / p).read_bytes(),
                    lambda p, sha, data: KF.check_bytes(data, files[p]))
    if not src.is_dir():
        source.problems.append(f'no folder {src}')
    return source


def zip_check(listed):
    """The check for a zip's file: against kit.json when this checkout lists it, else against SHA256SUMS and the
    plugin's rate."""
    def check(path, sha, data):
        if path in listed:
            return KF.check_bytes(data, listed[path])
        got = KF.sha256_bytes(data)
        if got != sha:
            return f'SHA-256 {got}, {KF.SUMS} says {sha}'
        try:
            info = KF.wav_info_bytes(data)
        except KF.WavError as e:
            return f'not a WAV the plugin reads ({e})'
        return None if info.rate == KF.RATE else f'{info.rate} Hz, not {KF.RATE}'
    return check


def manifest_differences(items, kit):
    """How a zip's SHA256SUMS ([(path, sha256)]) differs from this checkout's kit.json for the same kit and
    version."""
    zipped = dict(items)
    listed = {f.path: f.sha256 for f in kit.files}
    out = []
    for p in sorted(set(zipped) | set(listed), key=lambda p: p.split('/')):
        if p not in listed:
            out.append(f'{p}: in the zip, not in {kit.manifest.name}')
        elif p not in zipped:
            out.append(f'{p}: in {kit.manifest.name}, not in the zip')
        elif zipped[p] != listed[p]:
            out.append(f'{p}: the zip has SHA-256 {zipped[p]}, {kit.manifest.name} says {listed[p]}')
    return out


def zip_sources(z, where, manifests):
    """A Source for each kit in an open content zip."""
    out = []
    for name in sorted(z.namelist()):
        m = ZIP_SUMS.match(name)
        if not m:
            continue
        kit_id, version, kit_dir = m.group(1).upper(), int(m.group(2)), name[:-len(KF.SUMS)]
        sums = z.read(name)
        listed = {}
        source = Source(kit_id, version, where, [], sums, lambda p, d=kit_dir: z.read(d + p), zip_check(listed))
        try:
            source.items = [(p, s) for s, p in KF.parse_sums(sums.decode('utf-8'))]
        except (UnicodeDecodeError, ValueError) as e:
            source.problems.append(str(e))
        try:
            kit = KF.load_kit(KF.manifest_paths(manifests, [kit_id])[0])
        except KF.ManifestError:
            kit = None
        if kit is not None and kit.version == version and source.items:
            source.problems += manifest_differences(source.items, kit)
            listed.update({f.path: f for f in kit.files})
        out.append(source)
    return out


def _state(target, sha):
    """'missing', 'same' or 'differs': what's at target against a file with this SHA-256."""
    if not os.path.lexists(target):
        return 'missing'
    if not target.is_file():
        return 'differs'
    return 'same' if KF.sha256_file(target) == sha else 'differs'


def _sums_state(target, items):
    if not os.path.lexists(target):
        return 'missing'
    try:
        there = KF.parse_sums(target.read_text(encoding='utf-8'))
    except (OSError, UnicodeDecodeError, ValueError):
        return 'differs'
    return 'same' if sorted((p, s) for s, p in there) == sorted(items) else 'differs'


def _write(target, data):
    """data into target, through a temporary file in the same folder and a rename."""
    target.parent.mkdir(parents=True, exist_ok=True)
    tmp = target.with_name(f'.{target.name}.part-{os.getpid()}')
    try:
        with open(tmp, 'wb') as f:
            f.write(data)
        os.replace(tmp, target)
    except BaseException:
        tmp.unlink(missing_ok=True)
        raise


def _read(source, path):
    """(bytes, None) or (None, why it can't be read)."""
    try:
        return source.read(path), None
    except FileNotFoundError:
        return None, 'missing'
    except IsADirectoryError:
        return None, 'not a file'
    except KeyError:
        return None, 'missing from the zip'
    except OSError as e:
        return None, f'unreadable ({e.strerror or e})'
    except (zipfile.BadZipFile, EOFError) as e:  # a damaged entry: Bad CRC-32, a short read
        return None, f'damaged in the zip ({e})'


def install(source, dest_root, dry_run, out=print, err=None):
    """Checks source's files, then (unless dry_run) installs them into dest_root/<kit>/v<N>/. Returns True when the
    kit is (or would be) installed."""
    err = err or (lambda line: print(line, file=sys.stderr))
    dest = Path(dest_root) / source.kit_id.lower() / f'v{source.version}'
    out(f'{source.label} from {source.where}')
    problems, pointers, new, there, size = list(source.problems), 0, [], 0, 0
    conflicts = []
    if not problems:
        for path, sha in source.items:
            data, problem = _read(source, path)
            if problem is None:
                problem = source.check(path, sha, data)
            if problem:
                problems.append(f'{path}: {problem}')
                pointers += problem.startswith('a Git LFS pointer')
                continue
            state = _state(dest / path, sha)
            if state == 'differs':
                conflicts.append(path)
            elif state == 'same':
                there += 1
            else:
                new.append(path)
                size += len(data)
    if problems:
        err(f'{source.label}: not installed: {KF.plural(len(problems), "problem")} in {source.where}:')
        for line in KF.summarize(problems):
            err(f'  {line}')
        if pointers:
            err(f'  {pointers} of them are Git LFS pointers: {KF.LFS_HINT}')
        return False
    sums_state = _sums_state(dest / KF.SUMS, source.items)
    if conflicts:
        err(f'{source.label}: not installed: {dest} holds another build of {source.label}:')
        for line in KF.summarize([f'{path} differs from the one in {source.where}' for path in conflicts]):
            err(f'  {line}')
        err(f'  Nothing there was changed. Move {dest} aside (or delete it), then run this again.')
        return False
    if not new and sums_state == 'same':
        out(f'{source.label}: already installed in {dest} ({KF.plural(there, "file")})')
        return True
    if dry_run:
        for path in new:
            out(f'  would copy {path}')
        if sums_state != 'same':
            out(f'  would {"write" if sums_state == "missing" else "update"} {KF.SUMS}')
        out(f'{source.label}: would install {KF.plural(len(new), "file")} ({KF.size_text(size)}) into {dest} '
            f'({there} {"was" if there == 1 else "were"} already there)')
        return True
    hashes = dict(source.items)
    try:
        for path in new:
            data, problem = _read(source, path)
            if problem is None and KF.sha256_bytes(data) != hashes[path]:
                problem = 'changed while installing'
            if problem:
                raise OSError(f'{path}: {problem}')
            _write(dest / path, data)
        if sums_state != 'same':
            _write(dest / KF.SUMS, source.sums)
    except OSError as e:
        err(f'{source.label}: stopped: {e}. Files already copied are complete; run this again to finish.')
        return False
    out(f'{source.label}: installed {KF.plural(len(new), "file")} ({KF.size_text(size)}) into {dest} '
        f'({there} {"was" if there == 1 else "were"} already there)')
    return True


def main(argv=None):
    ap = argparse.ArgumentParser(prog='install.py', description=__doc__.split('\n\n')[0],
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    src = ap.add_mutually_exclusive_group(required=True)
    src.add_argument('--from-repo', nargs='?', const='', metavar='PATH',
                     help="a clone, or a folder of <kit>/ folders; default: this checkout's content/samples")
    src.add_argument('--from-zip', nargs='+', type=Path, metavar='ZIP', help='content zips made by pack.py')
    ap.add_argument('--kit', action='append', metavar='KIT', help='only this kit, like FUT01 (repeat for more)')
    ap.add_argument('--dest', type=Path, metavar='DIR', help="the content folder; default: the plugin's")
    ap.add_argument('--dry-run', action='store_true', help='check everything and list the actions; write nothing')
    ap.add_argument('--manifests', type=Path, default=KF.MANIFESTS, metavar='DIR',
                    help='a folder of <kit>/<KIT>_kit.json; default: content/plugin')
    args = ap.parse_args(argv)

    dest = args.dest or KF.default_content_dir()
    if dest is None:
        print('install.py: there is no default content folder on this system yet: pass --dest', file=sys.stderr)
        return 2
    if args.dry_run:
        print(f'Dry run: checking everything, writing nothing. Content folder: {dest}')

    zips = []
    try:
        if args.from_zip is None:
            samples = repo_samples(args.from_repo or None)
            try:
                kits = [KF.load_kit(p) for p in KF.manifest_paths(args.manifests, args.kit)]
            except KF.ManifestError as e:
                print(f'install.py: {e}', file=sys.stderr)
                return 2
            sources = [repo_source(kit, samples) for kit in kits]
        else:
            sources = []
            for path in args.from_zip:
                try:
                    z = zipfile.ZipFile(path)
                except (OSError, zipfile.BadZipFile) as e:
                    print(f'install.py: {path}: not a zip I can read ({e})', file=sys.stderr)
                    return 2
                zips.append(z)
                found = zip_sources(z, str(path), args.manifests)
                if not found:
                    print(f'install.py: {path} holds no {KF.ZIP_ROOT}/<kit>/v<N>/{KF.SUMS}: not a content zip',
                          file=sys.stderr)
                    return 2
                sources += found
            if args.kit:
                wanted = {k.upper() for k in args.kit}
                missing = wanted - {s.kit_id for s in sources}
                if missing:
                    print(f'install.py: no {", ".join(sorted(missing))} in {", ".join(map(str, args.from_zip))}',
                          file=sys.stderr)
                    return 2
                sources = [s for s in sources if s.kit_id in wanted]
        ok = [install(s, dest, args.dry_run) for s in sources]
    finally:
        for z in zips:
            z.close()
    if args.dry_run:
        print('Dry run: nothing was written.')
    return 0 if all(ok) else 1


if __name__ == '__main__':
    sys.exit(main())
