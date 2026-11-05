#!/usr/bin/env python3
"""Packs each kit's samples into its content zip: exactly the files its kit.json lists, each checked first, in a
fixed order, with SHA256SUMS and README.txt (ADR 0022). Two runs give the same bytes.

    python3 tools/content/pack.py --kit FUT01 --out DIR      # DIR/Wubansia-Content-FUT01-v1.zip
    python3 tools/content/pack.py --out DIR                  # every kit in content/plugin
    python3 tools/content/pack.py --kit FUT01 --list         # the listed files' paths, for git lfs pull --include
    python3 tools/content/pack.py --check                    # every listed file is here, as kit.json says

Each zip holds:
    Wubansia Content/README.txt                  dist/README.txt, the same in every zip
    Wubansia Content/<kit>/v<N>/SHA256SUMS       '<sha256>  <path>' a line
    Wubansia Content/<kit>/v<N>/<path>           every file kit.json lists, and nothing else

Entries go folder by folder, each folder before its files, all dated 1980-01-01 00:00 with modes 644 and 755, the
files deflated: the bytes depend only on the files, the kit's id, content version and file list, and README.txt.

The samples come from --samples, a folder of <kit>/ folders (default content/samples, the repo's layout, through Git
LFS); a <kit>/v<N>/ folder there is used first, as the plugin does, so an installed content folder packs too. Each
file is checked as the plugin checks it (a 48 kHz WAV with kit.json's frames and channels, not an LFS pointer) and
against kit.json's SHA-256, and the zip holds the bytes that were checked. Any problem stops that kit's zip; nothing
partial is left. Standard library only (Python 3.9+).
"""
import argparse
import stat
import sys
import zipfile
from pathlib import Path, PurePosixPath

try:
    from . import kitfiles as KF  # imported as tools.content.pack (the tests)
except ImportError:
    import kitfiles as KF  # run as a script

DATE = (1980, 1, 1, 0, 0, 0)          # DOS time's first day: nothing about when it was packed
FILE_MODE = stat.S_IFREG | 0o644
DIR_MODE = stat.S_IFDIR | 0o755
LEVEL = 9                             # deflate level; WAV saves about 12 %
UNIX = 3                              # ZipInfo.create_system: the modes above are honoured on unzip


class PackError(Exception):
    def __init__(self, kit, problems, hint=None):
        super().__init__(f'{kit.label}: {KF.plural(len(problems), "problem")}')
        self.kit = kit
        self.problems = problems
        self.hint = hint


def _entry(name, mode, compress):
    info = zipfile.ZipInfo(name, date_time=DATE)
    info.create_system = UNIX
    info.external_attr = mode << 16
    if name.endswith('/'):
        info.external_attr |= 0x10  # MS-DOS directory flag
    info.compress_type = compress
    return info


def read_kit(kit, samples):
    """{path: bytes} for every file kit.json lists, each checked. Raises PackError with every problem."""
    src = KF.kit_source(samples, kit)
    if not src.is_dir():
        raise PackError(kit, [f'no folder {src}'])
    data, problems, pointers = {}, [], 0
    for f in kit.files:
        got, problem = KF.read_checked(src / f.path, f)
        if problem:
            problems.append(f'{f.path}: {problem}')
            pointers += problem.startswith('a Git LFS pointer')
        else:
            data[f.path] = got
    if problems:
        hint = f'{pointers} of the files are Git LFS pointers: {KF.LFS_HINT}' if pointers else None
        raise PackError(kit, problems, hint)
    return data


def write_zip(kit, data, out_dir, readme):
    """Writes out_dir/Wubansia-Content-<KIT>-v<N>.zip from data ({path: bytes}, every listed file) and returns its
    path. It is written under a temporary name and renamed when complete."""
    out_dir = Path(out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    final = out_dir / KF.zip_name(kit)
    partial = out_dir / (final.name + '.partial')
    top = KF.ZIP_ROOT + '/'
    kit_dir = f'{top}{kit.folder}/v{kit.version}/'
    try:
        with zipfile.ZipFile(partial, 'w') as z:
            def folder(name):
                z.writestr(_entry(name, DIR_MODE, zipfile.ZIP_STORED), b'')

            def file(name, content):
                z.writestr(_entry(name, FILE_MODE, zipfile.ZIP_DEFLATED), content, compresslevel=LEVEL)

            folder(top)
            file(top + 'README.txt', readme)
            folder(f'{top}{kit.folder}/')
            folder(kit_dir)
            file(kit_dir + KF.SUMS, KF.sums_text(kit.files).encode('utf-8'))
            made = set()
            for f in KF.ordered(kit.files):
                parts = f.path.split('/')
                for i in range(1, len(parts)):
                    name = kit_dir + '/'.join(parts[:i]) + '/'
                    if name not in made:
                        made.add(name)
                        folder(name)
                file(kit_dir + f.path, data[f.path])
        partial.replace(final)
    except BaseException:
        if partial.exists():
            partial.unlink()
        raise
    return final


def pack(kit, samples, out_dir, readme):
    """Checks kit's files under samples and writes its zip into out_dir; returns the zip's path."""
    return write_zip(kit, read_kit(kit, samples), out_dir, readme)


def check_files(kit, samples):
    """(how many listed files are LFS pointers, how many are samples, problems): each listed file must be in the
    kit's folder, as a Git LFS pointer whose oid is kit.json's SHA-256 (a checkout without LFS, as CI's) or as the
    sample itself, checked like pack(). Reads only the pointers' first bytes, so it costs no LFS download."""
    src = KF.kit_source(samples, kit)
    pointers, samples_found, problems = 0, 0, []
    for f in kit.files:
        path = src / f.path
        if not path.is_file():
            problems.append(f'{f.path}: missing from {src}')
            continue
        if KF.is_lfs_pointer(path):
            with open(path, 'rb') as fh:
                oid = KF.lfs_oid(fh.read(KF.LFS_POINTER_MAX + 1))
            if oid is None:
                problems.append(f'{f.path}: an LFS pointer without a sha256 oid')
            elif oid != f.sha256:
                problems.append(f"{f.path}: its LFS pointer's oid is {oid}, kit.json says {f.sha256}")
            else:
                pointers += 1
            continue
        _, problem = KF.read_checked(path, f)
        if problem:
            problems.append(f'{f.path}: {problem}')
        else:
            samples_found += 1
    return pointers, samples_found, problems


def listed_paths(kit, samples, shown):
    """The listed files' paths under samples, spelled from `shown` (how that folder was named), in kit.json's order."""
    sub = KF.kit_source(samples, kit).relative_to(samples).as_posix()
    return [str(PurePosixPath(shown) / sub / f.path) for f in kit.files]


def main(argv=None):
    ap = argparse.ArgumentParser(prog='pack.py', description=__doc__.split('\n\n')[0],
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--kit', action='append', metavar='KIT',
                    help='a kit id, like FUT01 (repeat for more); default: every kit in --manifests')
    what = ap.add_mutually_exclusive_group(required=True)
    what.add_argument('--out', type=Path, metavar='DIR', help='where to write the zips')
    what.add_argument('--list', action='store_true',
                      help="print each listed file's path (repo-relative by default) and pack nothing")
    what.add_argument('--check', action='store_true',
                      help='check every listed file is here, as an LFS pointer with its SHA-256 or as the sample, '
                           'and pack nothing (no LFS download)')
    ap.add_argument('--samples', metavar='DIR',
                    help='a folder of <kit>/ (or <kit>/v<N>/) folders; default: content/samples')
    ap.add_argument('--manifests', type=Path, default=KF.MANIFESTS, metavar='DIR',
                    help='a folder of <kit>/<KIT>_kit.json; default: content/plugin')
    args = ap.parse_args(argv)

    try:
        kits = [KF.load_kit(p) for p in KF.manifest_paths(args.manifests, args.kit)]
    except KF.ManifestError as e:
        print(f'pack.py: {e}', file=sys.stderr)
        return 2
    samples = Path(args.samples) if args.samples else KF.SAMPLES

    if args.check:
        status = 0
        for kit in kits:
            pointers, found, problems = check_files(kit, samples)
            src = KF.kit_source(samples, kit)
            if problems:
                print(f'pack.py: {kit.label}: {KF.plural(len(problems), "problem")} in {src}:', file=sys.stderr)
                for line in KF.summarize(problems):
                    print(f'  {line}', file=sys.stderr)
                status = 1
            else:
                print(f'{kit.label}: all {len(kit.files)} files kit.json lists are in {src} ({pointers} as LFS '
                      f'pointers, {found} as samples), with its SHA-256')
        return status

    if args.list:
        shown = args.samples if args.samples else 'content/samples'
        for kit in kits:
            for path in listed_paths(kit, samples, shown):
                if ',' in path:  # git lfs pull --include splits on commas
                    print(f'pack.py: {path!r} has a comma, which git lfs --include can\'t take', file=sys.stderr)
                    return 1
                print(path)
        return 0

    try:
        readme = KF.README.read_bytes()
    except OSError as e:
        print(f'pack.py: can\'t read {KF.README}: {e.strerror}', file=sys.stderr)
        return 2
    status = 0
    for kit in kits:
        try:
            path = pack(kit, samples, args.out, readme)
        except PackError as e:
            print(f'pack.py: {e}; no zip written:', file=sys.stderr)
            for line in KF.summarize(e.problems):
                print(f'  {line}', file=sys.stderr)
            if e.hint:
                print(f'  {e.hint}', file=sys.stderr)
            status = 1
            continue
        size = path.stat().st_size
        print(f'{path}  {size:,} bytes ({KF.size_text(size)})  {kit.label}, {len(kit.files)} files  '
              f'sha256 {KF.sha256_file(path)}')
    return status


if __name__ == '__main__':
    sys.exit(main())
