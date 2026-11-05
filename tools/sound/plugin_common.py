"""What the plugin manifest exporters share (tools/sound/plugin_kit.py, P2; tools/sound/plugin_mix.py, P3).

docs/plugin/manifest.md is the contract: <KIT>_kit.json and <KIT>_mix.json, integers and strings only, every key in
a fixed order, every string clean. This module is its Python side:

- Integer units: iround () rounds half up, floor(x * scale + 0.5); mdb (), pm (), us ()... convert floats to the
  manifests' units.
- Fixed key order: KEYS names every object's keys in the documented order, and INSERT_KEYS every insert type's.
  ordered () and insert () build objects in that order and refuse a missing or unknown key.
- The canonical layout: dumps () writes the text, byte for byte what docs/plugin/manifest.md section 1 describes
  (2-space indent, arrays of scalars on one line, minimal escapes, a final newline). It refuses floats and None.
- The denylist hook: check_strings () runs every string, keys included, through tools/sound/denylist.py's artists
  and slurs lists. render () checks and dumps; write () also writes the file, or with check=True compares it.

The engine's test-side writer (engine/testing/manifest_json.cpp) follows the same rules. A change to the contract
lands in docs/plugin/manifest.md, the C++ structs and here, in one commit, through P0's owner.

    from tools.sound import plugin_common as PC
    kit = PC.ordered('kit', {...})
    PC.write(path, {'kit': kit, ...})           # or PC.write(path, obj, check=True) for --check
"""
import hashlib
import math
from pathlib import Path

from tools.sound import denylist

CONTENT_RATE = 48000                 # every sample the manifests list is 48 kHz
INT64 = (-(1 << 63), (1 << 63) - 1)  # what core's strict JSON reader takes
DENYLISTS = ('artists', 'slurs')     # strings in the manifests are names, never lyrics: profanity isn't checked


# ------------------------------------------------------------------ integer units

def iround(x, scale=1):
    """x * scale as an integer, halves up: floor(x * scale + 0.5). 0.59 * 1000 is 590, -2.5 * 1000 is -2500, and
    -0.0005 * 1000 is 0. Refuses NaN and infinities."""
    v = x * scale
    if isinstance(v, float) and not math.isfinite(v):
        raise ValueError(f'{x!r} x {scale} is not a finite number')
    return int(math.floor(v + 0.5))


def mdb(db):
    """Decibels (or dBFS) to milli-dB: -2.5 is -2500."""
    return iround(db, 1000)


def pm(x):
    """A ratio to per mille: 0.59 is 590, Q 0.707 is 707."""
    return iround(x, 1000)


def ppm(x):
    """A ratio to parts per million: 0.0004 is 400."""
    return iround(x, 1_000_000)


def hz(f):
    """Hertz, rounded."""
    return iround(f)


def mhz(f):
    """Hertz to millihertz: 46.25 is 46250."""
    return iround(f, 1000)


def us(seconds):
    """Seconds to microseconds: 0.03 is 30000."""
    return iround(seconds, 1_000_000)


def ms(seconds):
    """Seconds to milliseconds: 1.1 is 1100."""
    return iround(seconds, 1000)


def frames(seconds, rate=CONTENT_RATE):
    """Seconds to frames at 48 kHz: 0.02 is 960."""
    return iround(seconds, rate)


def mlu(lufs):
    """LUFS (or LU) to milli-LU: -9.73 is -9730."""
    return iround(lufs, 1000)


def mrad(radians):
    """Radians to milliradians: pi is 3142."""
    return iround(radians, 1000)


# ------------------------------------------------------------------ fixed key order (docs/plugin/manifest.md)

LANES = ('kick', 'snare', 'hats', 'perc', 'bass', 'lead', 'chords', 'vox', 'accents', 'fx')  # engine/kit/kit.h
AB_SWITCHES = ('kick', 'bass', 'stab')

KEYS = {
    # <KIT>_kit.json
    'kit.json': ('kit', 'files', 'instruments', 'keymap', 'extras', 'phrases'),
    'kit': ('id', 'title', 'direction', 'bpm', 'key', 'camelot', 'swing_pm', 'content_version', 'lanes', 'ab'),
    'lanes': LANES,
    'ab': AB_SWITCHES,
    'ab_labels': ('a', 'b'),
    'file': ('path', 'frames', 'channels', 'sha256', 'loop_start', 'loop_end', 'preroll_frames'),
    'instrument': ('id', 'name', 'lane', 'mix_channel', 'mode', 'align', 'attack_us', 'release_us', 'gain_mdb',
                   'start_frames', 'velocity_law', 'interp', 'mono', 'glide', 'restrike_continues', 'choke',
                   'duck_key', 'pan_cycle_pm', 'rr_block_16ths', 'layered_with', 'zones'),
    'glide': ('curve', 'us', 'dip_us'),
    'choke': ('group', 'role', 'fade_us', 'curve'),
    'zone': ('root', 'lo', 'hi', 'layers'),
    'layer': ('vel', 'rr'),
    'keymap': ('ch', 'keys', 'a', 'b', 'ab'),
    'extra': ('ch', 'name', 'lane'),
    'phrase': ('lane', 'ch', 'ab', 'mix_channel', 'gain_mdb', 'stems', 'cycle_16ths', 'tolerance_pm', 'fade_us',
               'slices'),
    'stems': ('a', 'b'),
    'slice': ('step', 'key', 'start', 'end', 'fade', 'lead', 'holds'),
    # <KIT>_mix.json
    'mix.json': ('kit', 'content_version', 'channels', 'groups', 'returns', 'bus_eq', 'master', 'reference'),
    'channel': ('id', 'lane', 'group', 'inserts', 'sends_pm'),
    'groups': ('drums', 'music'),
    'group': ('inserts',),
    'returns': ('space', 'echo', 'resonator'),
    'space': ('delays_frames', 'signs', 't60_ms', 't60_low_ms', 'low_hz', 'damp_hz', 'predelay_us', 'send_hp_hz',
              'send_order', 'shimmer_pm', 'shimmer_lo_hz', 'shimmer_hi_hz', 'window_frames', 'mono_hz', 'return_mdb',
              'inserts'),
    'echo': ('steps', 'feedback_pm', 'lp_hz', 'hp_hz', 'drive_pm', 'wow_ppm', 'wow_mhz', 'flutter_ppm', 'flutter_mhz',
             'phases_mrad', 'return_mdb', 'inserts'),
    'resonator': ('tones_mhz', 'tau_ms', 'send_hp_hz', 'return_mdb', 'inserts'),
    'bus_eq': ('vox', 'novox'),
    'master': ('lowcut_hz', 'lowcut_order', 'mono_below_hz', 'glue', 'vox', 'novox', 'clip', 'limiter', 'dc_hz'),
    'glue': ('ratio_pm', 'knee_mdb', 'attack_us', 'release_fast_us', 'release_slow_us'),
    'master_variant': ('glue_threshold_mdb', 'drive_mdb'),
    'master_clip': ('knee_mdb', 'ceiling_mdb'),
    'limiter': ('ceiling_mdb', 'lookahead_us', 'hold_us', 'release_us'),
    'reference': ('vox', 'novox'),
    'reference_variant': ('pre_master_lufs_mlu', 'club_lufs_mlu', 'channel_true_peak_mdb'),
    'band': ('lo_hz', 'hi_hz'),
    'when': ('variant', 'ab', 'is'),
}

# Each insert type's keys after 'type' (and before the optional 'when'). A duck's depend on its mode.
INSERT_KEYS = {
    'gain': ('gain_mdb',),
    'pan': ('pan_pm',),
    'width': ('width_pm', 'freq_hz', 'order'),
    'peak': ('freq_hz', 'gain_mdb', 'q_pm'),
    'low_shelf': ('freq_hz', 'gain_mdb', 'q_pm'),
    'high_shelf': ('freq_hz', 'gain_mdb', 'q_pm'),
    'highpass': ('freq_hz', 'order', 'q_pm', 'stages', 'band'),
    'lowpass': ('freq_hz', 'order', 'q_pm', 'stages', 'band'),
    'cc_filter': ('response', 'cc74_lo_hz', 'cc74_hi_hz', 'cc74_default', 'cc71_lo_pm', 'cc71_hi_pm', 'cc71_default',
                  'band'),
    'mono_below': ('freq_hz', 'order'),
    'clip': ('ceiling_mdb', 'knee_mdb'),
    'drive': ('drive_pm', 'bias_pm', 'output_mdb'),
    'duck': None,  # by mode: DUCK_KEYS
    'comp': ('threshold_mdb', 'ratio_pm', 'knee_mdb', 'attack_us', 'release_fast_us', 'release_slow_us',
             'makeup_mdb'),
    'parallel': ('level_mdb', 'inserts'),
}
DUCK_KEYS = {
    'onset': ('mode', 'key_lanes', 'min_velocity', 'depth_mdb', 'ramp_us', 'hold_us', 'release_us', 'pre_us', 'band',
              'pump'),
    'level': ('mode', 'key_channel', 'depth_mdb', 'range_mdb', 'top_mdb', 'attack_us', 'release_us', 'band', 'pump'),
}


def _keyed(where, keys, mapping):
    missing = [k for k in keys if k not in mapping]
    unknown = [k for k in mapping if k not in keys]
    if missing or unknown:
        parts = []
        if missing:
            parts.append('missing ' + ', '.join(missing))
        if unknown:
            parts.append('unknown ' + ', '.join(str(k) for k in unknown))
        raise KeyError(f'{where}: {"; ".join(parts)} (docs/plugin/manifest.md)')
    return {k: mapping[k] for k in keys}


def ordered(kind, mapping=None, **fields):
    """The object `kind` (a KEYS name) with exactly its documented keys, in their order. Pass a mapping or keyword
    arguments (or both)."""
    if kind not in KEYS:
        raise KeyError(f'no object kind {kind!r}; KEYS has {", ".join(KEYS)}')
    merged = dict(mapping or {})
    merged.update(fields)
    return _keyed(kind, KEYS[kind], merged)


def insert(type_, mapping=None, when=None, **fields):
    """An insert: {'type': type_, its fields in INSERT_KEYS order, then 'when' if given}. when is a dict with the
    'when' keys, e.g. {'variant': 'novox', 'ab': 'none', 'is': 'a'}."""
    if type_ not in INSERT_KEYS:
        raise KeyError(f'no insert type {type_!r}; the closed set is {", ".join(INSERT_KEYS)}')
    merged = dict(mapping or {})
    merged.update(fields)
    keys = INSERT_KEYS[type_]
    if type_ == 'duck':
        mode = merged.get('mode')
        if mode not in DUCK_KEYS:
            raise KeyError(f'insert duck: mode {mode!r} is not one of {", ".join(DUCK_KEYS)}')
        keys = DUCK_KEYS[mode]
    out = {'type': type_}
    out.update(_keyed(f'insert {type_}', keys, merged))
    if when is not None:
        out['when'] = ordered('when', when)
    return out


# ------------------------------------------------------------------ the canonical layout

_SHORT_ESCAPES = {'"': '\\"', '\\': '\\\\', '\b': '\\b', '\f': '\\f', '\n': '\\n', '\r': '\\r', '\t': '\\t'}


def _string(s):
    out = ['"']
    for c in s:
        if c in _SHORT_ESCAPES:
            out.append(_SHORT_ESCAPES[c])
        elif ord(c) < 0x20:
            out.append(f'\\u{ord(c):04x}')
        else:
            out.append(c)
    out.append('"')
    return ''.join(out)


def _scalar(v):
    return isinstance(v, (bool, int, str))


def _check_value(v, path):
    if isinstance(v, bool) or isinstance(v, str):
        return
    if isinstance(v, int):
        if not INT64[0] <= v <= INT64[1]:
            raise ValueError(f'{path or "the root"}: {v} doesn\'t fit 64 bits')
        return
    if isinstance(v, float):
        raise TypeError(f'{path or "the root"}: {v!r} is a float; the manifests are integers only (use iround and the '
                        f'unit helpers)')
    if v is None:
        raise TypeError(f'{path or "the root"}: None; the manifests have no null')
    raise TypeError(f'{path or "the root"}: a {type(v).__name__} is not integer, string, flag, list or object')


def _write(v, depth, path, out):
    if isinstance(v, dict):
        if not v:
            out.append('{}')
            return
        out.append('{\n')
        pad = '  ' * (depth + 1)
        items = list(v.items())
        for i, (k, x) in enumerate(items):
            if not isinstance(k, str):
                raise TypeError(f'{path or "the root"}: the key {k!r} is not a string')
            out.append(pad + _string(k) + ': ')
            _write(x, depth + 1, f'{path}.{k}' if path else k, out)
            out.append(',\n' if i + 1 < len(items) else '\n')
        out.append('  ' * depth + '}')
    elif isinstance(v, (list, tuple)):
        if not v:
            out.append('[]')
            return
        if all(_scalar(x) for x in v):
            for i, x in enumerate(v):
                _check_value(x, f'{path}[{i}]')
            out.append('[' + ', '.join(_scalar_text(x) for x in v) + ']')
            return
        out.append('[\n')
        pad = '  ' * (depth + 1)
        for i, x in enumerate(v):
            out.append(pad)
            _write(x, depth + 1, f'{path}[{i}]', out)
            out.append(',\n' if i + 1 < len(v) else '\n')
        out.append('  ' * depth + ']')
    else:
        _check_value(v, path)
        out.append(_scalar_text(v))


def _scalar_text(v):
    if isinstance(v, bool):
        return 'true' if v else 'false'
    if isinstance(v, str):
        return _string(v)
    return str(v)


def dumps(obj):
    """obj as manifest text in the canonical layout (docs/plugin/manifest.md section 1): 2 spaces per level, each
    object member on its own line, arrays of integers, strings or flags on one line, other arrays one element per
    line, {} and [] when empty, only ", \\ and control characters escaped, other characters as UTF-8, and a final
    newline. Dict order is kept: build objects with ordered () and insert ()."""
    out = []
    _write(obj, 0, '', out)
    out.append('\n')
    return ''.join(out)


# ------------------------------------------------------------------ the denylist hook

def strings(obj, path=''):
    """Every string in obj, keys included, as (path, text)."""
    if isinstance(obj, str):
        yield path, obj
    elif isinstance(obj, dict):
        for k, v in obj.items():
            here = f'{path}.{k}' if path else str(k)
            if isinstance(k, str):
                yield here + ' (key)', k
            yield from strings(v, here)
    elif isinstance(obj, (list, tuple)):
        for i, v in enumerate(obj):
            yield from strings(v, f'{path}[{i}]')


def check_strings(obj, lists=None):
    """Raises ValueError naming the path of every string in obj that the artists or slurs denylist hits. lists:
    {name: digests} in place of the files (for tests)."""
    lists = lists if lists is not None else {name: denylist.load(name) for name in DENYLISTS}
    bad = []
    for path, text in strings(obj):
        for name, digests in lists.items():
            if denylist.hits(text, digests):
                bad.append(f'{path or "the root"}: {text!r} hits the {name} list')
    if bad:
        raise ValueError('denylisted strings (CLAUDE.md content rules):\n  ' + '\n  '.join(bad))


# ------------------------------------------------------------------ files

def render(obj, lists=None):
    """check_strings (obj), then dumps (obj)."""
    check_strings(obj, lists)
    return dumps(obj)


def write(path, obj, check=False, lists=None):
    """Writes obj to path as UTF-8 manifest text (render ()). check=True writes nothing and returns whether the file
    already holds exactly that text (an exporter's --check). Returns True when the file is (or now is) up to date."""
    text = render(obj, lists).encode('utf-8')
    path = Path(path)
    if check:
        return path.is_file() and path.read_bytes() == text
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(text)
    return True


def sha256_file(path):
    """The SHA-256 of a file's bytes, as 64 lowercase hex digits (files[].sha256)."""
    h = hashlib.sha256()
    with open(path, 'rb') as f:
        for block in iter(lambda: f.read(1 << 20), b''):
            h.update(block)
    return h.hexdigest()
