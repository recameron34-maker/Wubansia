"""What pack.py and install.py share: a kit's file list, the content folder's layout, and the checks the plugin makes
before it plays a kit (ADR 0022). Standard library only, Python 3.9 or later, so a Mac's own python3 runs it.

A kit's files are what its kit.json lists (docs/plugin/manifest.md, section 2.2): path, frames, channels, sha256.
In a content folder they live at <root>/<kit>/v<N>/<path>, or at <root>/<kit>/<path> in the repo's layout; the plugin
tries v<N>/ first. A content zip holds 'Wubansia Content/<kit>/v<N>/': the files and SHA256SUMS, plus README.txt at
the top.
"""
import hashlib
import json
import platform
import re
import struct
from pathlib import Path
from typing import NamedTuple, Tuple

REPO = Path(__file__).resolve().parents[2]
MANIFESTS = REPO / 'content' / 'plugin'   # <kit>/<KIT>_kit.json, as the plugin embeds them
SAMPLES = REPO / 'content' / 'samples'    # <kit>/<path>: the repo's layout (Git LFS)
README = REPO / 'dist' / 'README.txt'     # the content zips' readme

ZIP_ROOT = 'Wubansia Content'  # the folder at the top of every content zip
SUMS = 'SHA256SUMS'            # '<sha256>  <path>' per file, in each <kit>/v<N>/ folder
RATE = 48000                   # every sample the manifests list is 48 kHz
MAX_FRAMES = 28_800_000        # docs/plugin/manifest.md: files[].frames is 1 to this

KIT_ID = re.compile(r'[A-Z]{3}[0-9]{2}\Z')
SHA256_HEX = re.compile(r'[0-9a-f]{64}\Z')
SUMS_LINE = re.compile(r'([0-9a-f]{64})  (.+)\Z', re.DOTALL)
LFS_PREFIXES = (b'version https://git-lfs.github.com/spec/', b'version https://hawser.github.com/spec/')
LFS_POINTER_MAX = 1024         # Git LFS pointer files are smaller than this

# How to get the samples into a clone: what an LFS pointer's message points at.
LFS_HINT = "in the clone, run: git lfs install && git lfs pull --include 'content/samples/**'"


class ManifestError(ValueError):
    """A kit.json that these tools can't use."""


class WavError(ValueError):
    """Bytes the plugin's WAV reader would refuse."""


class SampleFile(NamedTuple):
    path: str       # relative to the kit's folder, '/'-separated
    frames: int     # at 48 kHz
    channels: int   # 1 or 2
    sha256: str     # of the whole file, 64 lowercase hex digits


class Kit(NamedTuple):
    id: str                        # 'FUT01'
    version: int                   # kit.content_version
    files: Tuple[SampleFile, ...]  # in kit.json's order
    manifest: Path

    @property
    def folder(self):
        """The kit's folder name: its id in lowercase (fut01)."""
        return self.id.lower()

    @property
    def label(self):
        return f'{self.id} v{self.version}'


class WavInfo(NamedTuple):
    rate: int
    channels: int
    frames: int
    bits: int
    format: int  # 1 PCM or 3 float (WAVE_FORMAT_EXTENSIBLE reads as its sub-format)


# ------------------------------------------------------------------ paths

def check_path(path):
    """None when path is a kit file path these tools take (docs/plugin/manifest.md 2.2, and what a SHA256SUMS line can
    carry), else why not."""
    if not isinstance(path, str) or not path:
        return 'no path'
    if any(ord(c) < 0x20 or ord(c) == 0x7f for c in path):
        return f'{path!r} has a control character'
    if '\\' in path:
        return f'{path!r} has a backslash'
    if path.startswith('/'):
        return f'{path!r} starts with /'
    if any(part in ('', '.', '..') for part in path.split('/')):
        return f'{path!r} has an empty, "." or ".." part'
    if not path.endswith('.wav'):
        return f'{path!r} is not a .wav file'
    return None


def ordered(files):
    """files (SampleFile) sorted folder by folder: the order of the zips' entries and of SHA256SUMS."""
    return sorted(files, key=lambda f: f.path.split('/'))


def zip_name(kit):
    return f'Wubansia-Content-{kit.id}-v{kit.version}.zip'


def default_content_dir(system=None, home=None):
    """The content folder the plugin looks in after $WUBANSIA_CONTENT_DIR and the folder chosen in its editor (ADR
    0022), or None where that isn't decided yet (Windows)."""
    system = system or platform.system()
    home = Path(home) if home is not None else Path.home()
    if system == 'Darwin':
        return home / 'Library' / 'Application Support' / 'Wubansia Audio' / 'Wubansia' / 'Content'
    if system == 'Linux':
        return home / '.local' / 'share' / 'Wubansia Audio' / 'Wubansia' / 'Content'
    return None


def kit_source(root, kit):
    """Where kit's files are under root: <root>/<kit>/v<N>/ if that folder exists, else <root>/<kit>/ (the plugin's
    order)."""
    root = Path(root)
    versioned = root / kit.folder / f'v{kit.version}'
    return versioned if versioned.is_dir() else root / kit.folder


# ------------------------------------------------------------------ manifests

def manifest_paths(manifests, ids=None):
    """The kit.json of each kit in ids (any case), or of every kit under manifests (<kit>/<KIT>_kit.json), sorted.
    Raises ManifestError for a kit that has none."""
    manifests = Path(manifests)
    if ids:
        out = []
        for kit_id in ids:
            up = kit_id.upper()
            path = manifests / up.lower() / f'{up}_kit.json'
            if not KIT_ID.match(up) or not path.is_file():
                raise ManifestError(f'no kit {kit_id!r}: {path} does not exist')
            if path not in out:
                out.append(path)
        return out
    found = sorted(p for p in manifests.glob('*/*_kit.json')
                   if KIT_ID.match(p.name[:-len('_kit.json')]) and p.parent.name == p.name[:-len('_kit.json')].lower())
    if not found:
        raise ManifestError(f'no kit manifests (<kit>/<KIT>_kit.json) in {manifests}')
    return found


def _int(value, low, high):
    return isinstance(value, int) and not isinstance(value, bool) and low <= value <= high


def load_kit(path):
    """The kit's id, content version and file list from its kit.json. Checks what these tools rely on and nothing
    else; raises ManifestError naming the JSON path."""
    path = Path(path)
    where = str(path)
    try:
        obj = json.loads(path.read_text(encoding='utf-8'))
    except (OSError, ValueError) as e:
        raise ManifestError(f'{where}: {e}') from None
    kit = obj.get('kit') if isinstance(obj, dict) else None
    if not isinstance(kit, dict):
        raise ManifestError(f'{where}: no "kit" object')
    kit_id, version = kit.get('id'), kit.get('content_version')
    if not isinstance(kit_id, str) or not KIT_ID.match(kit_id):
        raise ManifestError(f'{where}: kit.id {kit_id!r} is not like FUT01')
    if not _int(version, 1, 999):
        raise ManifestError(f'{where}: kit.content_version {version!r} is not 1-999')
    files = obj.get('files')
    if not isinstance(files, list) or not files:
        raise ManifestError(f'{where}: "files" is not a list of files')
    out, seen = [], {}
    for i, f in enumerate(files):
        at = f'{where}: files[{i}]'
        if not isinstance(f, dict):
            raise ManifestError(f'{at} is not an object')
        p = f.get('path')
        problem = check_path(p)
        if problem:
            raise ManifestError(f'{at}.path: {problem}')
        if not _int(f.get('frames'), 1, MAX_FRAMES):
            raise ManifestError(f'{at}.frames: {f.get("frames")!r} is not 1-{MAX_FRAMES}')
        if f.get('channels') not in (1, 2) or isinstance(f.get('channels'), bool):
            raise ManifestError(f'{at}.channels: {f.get("channels")!r} is not 1 or 2')
        if not isinstance(f.get('sha256'), str) or not SHA256_HEX.match(f['sha256']):
            raise ManifestError(f'{at}.sha256: {f.get("sha256")!r} is not 64 lowercase hex digits')
        key = p.casefold()
        if key in seen:  # one file on a case-insensitive disk (macOS's default)
            raise ManifestError(f'{at}.path: {p!r} is files[{seen[key]}].path apart from letter case')
        seen[key] = i
        out.append(SampleFile(p, f['frames'], f['channels'], f['sha256']))
    return Kit(kit_id, version, tuple(out), path)


# ------------------------------------------------------------------ SHA256SUMS

def sums_text(files):
    """SHA256SUMS for files (SampleFile): '<sha256>  <path>' a line, folder by folder. shasum -a 256 -c and
    sha256sum -c read it."""
    return ''.join(f'{f.sha256}  {f.path}\n' for f in ordered(files))


def parse_sums(text):
    """[(sha256, path)] from SHA256SUMS text, in its order. Raises ValueError for an empty file, a line that isn't
    '<sha256>  <path>', a path check_path refuses, or a path listed twice."""
    lines = text.split('\n')
    if lines and lines[-1] == '':
        lines.pop()
    if not lines:
        raise ValueError(f'{SUMS} is empty')
    out, seen = [], set()
    for n, line in enumerate(lines, 1):
        m = SUMS_LINE.match(line)
        if not m:
            raise ValueError(f'{SUMS} line {n} is not "<sha256>  <path>": {line[:80]!r}')
        problem = check_path(m.group(2))
        if problem:
            raise ValueError(f'{SUMS} line {n}: {problem}')
        key = m.group(2).casefold()
        if key in seen:
            raise ValueError(f'{SUMS} line {n}: {m.group(2)!r} is listed twice')
        seen.add(key)
        out.append((m.group(1), m.group(2)))
    return out


# ------------------------------------------------------------------ the files themselves

def sha256_bytes(data):
    return hashlib.sha256(data).hexdigest()


def sha256_file(path):
    h = hashlib.sha256()
    with open(path, 'rb') as f:
        for block in iter(lambda: f.read(1 << 20), b''):
            h.update(block)
    return h.hexdigest()


def is_lfs_pointer_bytes(data):
    return len(data) <= LFS_POINTER_MAX and data.startswith(LFS_PREFIXES)


def is_lfs_pointer(path):
    """Whether the file at path is a Git LFS pointer: what a clone holds where git-lfs didn't fetch the sample."""
    try:
        with open(path, 'rb') as f:
            head = f.read(LFS_POINTER_MAX + 1)
    except OSError:
        return False
    return is_lfs_pointer_bytes(head)


def wav_info_bytes(data):
    """The header facts of a RIFF WAVE file in data: 16/24-bit PCM or 32-bit float, WAVE_FORMAT_EXTENSIBLE too.
    Raises WavError for anything else, or a data chunk the file doesn't hold."""
    if len(data) < 12 or data[:4] != b'RIFF' or data[8:12] != b'WAVE':
        raise WavError('not a RIFF WAVE file')
    pos, fmt = 12, None
    while pos + 8 <= len(data):
        cid, size = data[pos:pos + 4], struct.unpack_from('<I', data, pos + 4)[0]
        body = pos + 8
        if cid == b'fmt ':
            if size < 16 or body + size > len(data):
                raise WavError('a short fmt chunk')
            tag, channels, rate, _, block, bits = struct.unpack_from('<HHIIHH', data, body)
            if tag == 0xFFFE and size >= 40:
                tag = struct.unpack_from('<H', data, body + 24)[0]  # the sub-format GUID's first two bytes
            if tag not in (1, 3) or channels < 1 or rate < 1 or bits % 8 or block != channels * bits // 8:
                raise WavError(f'format {tag}, {channels} channels, {bits} bits: not PCM or float the plugin reads')
            fmt = (tag, channels, rate, bits, block)
        elif cid == b'data':
            if fmt is None:
                raise WavError('a data chunk before the fmt chunk')
            if body + size > len(data):
                raise WavError(f'the data chunk says {size} bytes, the file has {len(data) - body}')
            if size % fmt[4]:
                raise WavError(f'{size} bytes of data is not a whole number of frames')
            tag, channels, rate, bits, block = fmt
            return WavInfo(rate, channels, size // block, bits, tag)
        pos = body + size + (size & 1)
    raise WavError('no data chunk')


def wav_info(path):
    with open(path, 'rb') as f:
        return wav_info_bytes(f.read())


def check_bytes(data, f):
    """The first reason data isn't the file f (SampleFile) as kit.json lists it, or None: a Git LFS pointer, a WAV the
    plugin won't read, the wrong rate, channels or frames, then the SHA-256."""
    if is_lfs_pointer_bytes(data):
        return 'a Git LFS pointer, not the sample'
    try:
        info = wav_info_bytes(data)
    except WavError as e:
        return f'not a WAV the plugin reads ({e})'
    if info.rate != RATE:
        return f'{info.rate} Hz, not {RATE}'
    if info.channels != f.channels:
        return f'{info.channels} channels, kit.json says {f.channels}'
    if info.frames != f.frames:
        return f'{info.frames} frames, kit.json says {f.frames}'
    got = sha256_bytes(data)
    if got != f.sha256:
        return f'SHA-256 {got}, kit.json says {f.sha256}'
    return None


def read_checked(path, f):
    """(bytes, None) when the file at path is f, else (None, why not)."""
    path = Path(path)
    if not path.exists():
        return None, 'missing'
    if not path.is_file():
        return None, 'not a file'
    try:
        data = path.read_bytes()
    except OSError as e:
        return None, f'unreadable ({e.strerror})'
    problem = check_bytes(data, f)
    return (None, problem) if problem else (data, None)


def summarize(problems, limit=10):
    """The first `limit` problems, then how many more."""
    out = list(problems[:limit])
    if len(problems) > limit:
        out.append(f'... and {len(problems) - limit} more')
    return out


def size_text(n):
    return f'{n / 1e6:.1f} MB' if n >= 100_000 else f'{n:,} bytes'
