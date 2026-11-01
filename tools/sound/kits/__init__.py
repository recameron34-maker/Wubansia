"""The kits. Each kit is one module here, kits/<kit>.py, holding its DNA, and one recipe package of its own,
recipes/<kit>/, whose modules each expose ITEMS = [Item(...), ...]. A kit's designer edits only those two: everything
else in tools/sound is shared, and never imports a kit.

The DNA (plan section 2.1, ADR 0008) every kit module declares, in these names:

    KIT         'FUT02': the file prefix; the module is kits/fut02.py
    TITLE       the kit's name, plain words, never an artist's
    DIRECTION   one of DIRECTIONS
    BPM         the authored tempo, 128-140 (CLAUDE.md)
    KEY         'D minor', 'F# minor', 'Bb major'...
    CAMELOT     the key's Camelot code, camelot(KEY): '7A'
    SCALE       the key's pitch classes, root first: scale_of(KEY)
    ROOT        the key's root in the bass register (MIDI, C4 = 60)
    KICK_NOTE   the kick's and the sub's note, in the key, MIDI 24-40 (its fundamental inside 30-120 Hz)
    RATIOS      the kit's inharmonic ratio set: 1.0 first, ascending (FUT01 also names it ALLOY)
    VOICE       the one Kokoro voice blend: ((voice, weight), ...), weights summing to 1
    SPACE_SEED  seeds every room and FDN of the kit: '<KIT>/space'
    SWING       the one swing of every written lane: where the second 16th of each 8th lands, as a share of the 8th
                (0.5 straight)
    GROOVE      the kick pattern QA loops the kicks and bass shots in: '2step' or '4x4' (qa.GROOVES)
    RECIPES     'tools.sound.recipes.<kit>': the kit's recipe package
    BEAT, STEP  60 / BPM and a 16th, in seconds

and space_rng(*extra) = dsp48.rng(SPACE_SEED, *extra), the kit's room. A value not decided yet is TODO: a kit with a
TODO field refuses to render (render.py exits 2 and names the fields).
"""
import importlib
import pkgutil
import re

from tools.sound.item import UnknownName


class Todo:
    """A DNA value the kit's designer hasn't set yet."""

    def __repr__(self):
        return 'TODO'


TODO = Todo()
DNA = ('KIT', 'TITLE', 'DIRECTION', 'BPM', 'KEY', 'CAMELOT', 'SCALE', 'ROOT', 'KICK_NOTE', 'RATIOS', 'VOICE',
       'SPACE_SEED', 'SWING', 'GROOVE', 'RECIPES')
DIRECTIONS = ('sci-fi club', 'future garage / 2-step', 'hyper-clean future house', 'dark industrial / techno')
BPM_RANGE = (128.0, 140.0)
SWING_RANGE = (0.5, 0.75)
KICK_RANGE = (24, 40)        # C1 (32.7 Hz) to E2 (82.4 Hz)
ROOT_RANGE = (24, 52)
KIT_NAME = re.compile(r'[A-Z]{3}\d{2}')

PITCH = {'C': 0, 'D': 2, 'E': 4, 'F': 5, 'G': 7, 'A': 9, 'B': 11}
MODES = {'minor': (0, 2, 3, 5, 7, 8, 10), 'major': (0, 2, 4, 5, 7, 9, 11)}
# the Camelot wheel by pitch class: legacy/web-pack/core.js's MINOR and MAJOR tables (tests/test_kits.py checks them)
CAMELOT_WHEEL = {'minor': ('5A', '12A', '7A', '2A', '9A', '4A', '11A', '6A', '1A', '8A', '3A', '10A'),
                 'major': ('8B', '3B', '10B', '5B', '12B', '7B', '2B', '9B', '4B', '11B', '6B', '1B')}


class KitNotReady(Exception):
    """A kit whose DNA still has TODO fields or invalid values: a usage error, render.py exits 2."""


# ------------------------------------------------------------------ keys
def parse_key(key):
    """'F# minor' -> (6, 'minor'); None if it isn't a key."""
    m = re.fullmatch(r'([A-G])([#b]?) (minor|major)', key) if isinstance(key, str) else None
    if not m:
        return None
    return (PITCH[m[1]] + {'#': 1, 'b': -1, '': 0}[m[2]]) % 12, m[3]


def camelot(key):
    pk = parse_key(key)
    return None if pk is None else CAMELOT_WHEEL[pk[1]][pk[0]]


def scale_of(key):
    """The key's pitch classes, root first; TODO while the key is TODO."""
    if key is TODO:
        return TODO
    pk = parse_key(key)
    if pk is None:
        raise ValueError(f'not a key: {key!r} (write it like "D minor" or "F# minor")')
    return tuple((pk[0] + s) % 12 for s in MODES[pk[1]])


def in_key(kit, note):
    return int(note) % 12 in kit.SCALE


# ------------------------------------------------------------------ finding and checking kits
def names():
    """Every kit, from the modules in this package: ['FUT01', 'FUT02', ...]."""
    return sorted(m.name.upper() for m in pkgutil.iter_modules(__path__)
                  if not m.name.startswith('_') and KIT_NAME.fullmatch(m.name.upper()))


def module(kit):
    """A kit's module, imported as it stands (TODO fields and all); UnknownName if there is no such kit."""
    if kit not in names():
        raise UnknownName(f'unknown kit {kit!r}; known: {", ".join(names())}')
    return importlib.import_module(f'{__name__}.{kit.lower()}')


def load(kit):
    """A kit's module, ready to render: KitNotReady while a DNA field is TODO or invalid."""
    mod = module(kit)
    pending, bad = todo(mod), problems(mod)
    if pending or bad:
        msg = [f'{kit} is not ready to render: fill its DNA in tools/sound/kits/{kit.lower()}.py']
        if pending:
            msg.append(f'still TODO: {", ".join(pending)}')
        raise KitNotReady('; '.join(msg + bad))
    return mod


def todo(mod):
    """The DNA fields still TODO."""
    return [f for f in DNA if getattr(mod, f, None) is TODO]


def problems(mod, name=None):
    """What is wrong with the declared (not TODO) DNA, one line each, starting with the field's name. name: the
    module's own name (default: its __name__)."""
    from tools.sound import models, qa
    name = name or mod.__name__.rsplit('.', 1)[-1]
    out = [f'{f} is missing' for f in DNA if not hasattr(mod, f)]
    dna = {f: getattr(mod, f) for f in DNA if hasattr(mod, f) and getattr(mod, f) is not TODO}

    def bad(field, ok, why):
        if field in dna and not ok(dna[field]):
            out.append(f'{field} {why} (got {dna[field]!r})')

    num = (int, float)
    bad('KIT', lambda v: v == name.upper(), f'must be {name.upper()!r}, the module name in capitals')
    bad('TITLE', lambda v: isinstance(v, str) and v.strip(), 'must be a name')
    bad('DIRECTION', lambda v: v in DIRECTIONS, f'must be one of {DIRECTIONS}')
    bad('BPM', lambda v: isinstance(v, num) and BPM_RANGE[0] <= v <= BPM_RANGE[1], 'must be 128-140 (CLAUDE.md)')
    bad('KEY', lambda v: parse_key(v) is not None, 'must be a key like "D minor"')
    key = parse_key(dna.get('KEY'))
    if key:
        bad('CAMELOT', lambda v: v == camelot(dna['KEY']), f"must be {camelot(dna['KEY'])}, the key's code")
        bad('SCALE', lambda v: tuple(v) == scale_of(dna['KEY']), f"must be {scale_of(dna['KEY'])}, the key's")
        bad('ROOT', lambda v: isinstance(v, int) and v % 12 == key[0] and ROOT_RANGE[0] <= v <= ROOT_RANGE[1],
            f"must be the key's root, MIDI {ROOT_RANGE[0]}-{ROOT_RANGE[1]}")
        bad('KICK_NOTE', lambda v: isinstance(v, int) and v % 12 in scale_of(dna['KEY'])
            and KICK_RANGE[0] <= v <= KICK_RANGE[1], f'must be in the key, MIDI {KICK_RANGE[0]}-{KICK_RANGE[1]}')
    bad('RATIOS', lambda v: len(v) >= 4 and v[0] == 1.0 and all(b > a for a, b in zip(v, v[1:])),
        'must hold 4 or more ratios, 1.0 first, ascending')
    bad('VOICE', lambda v: len(v) >= 1 and all(s in models.SPEAKERS and w > 0 for s, w in v)
        and abs(sum(w for _, w in v) - 1.0) < 1e-9, 'must be ((Kokoro voice, weight), ...) with weights summing to 1')
    bad('SPACE_SEED', lambda v: isinstance(v, str) and v.strip(), 'must be a seed string, e.g. "FUT02/space"')
    bad('SWING', lambda v: isinstance(v, num) and SWING_RANGE[0] <= v < SWING_RANGE[1], 'must be 0.5 to under 0.75')
    bad('GROOVE', lambda v: v in qa.GROOVES, f'must be one of {tuple(qa.GROOVES)}')
    bad('RECIPES', lambda v: v == f'tools.sound.recipes.{name.lower()}', f'must be tools.sound.recipes.{name.lower()}')
    if isinstance(dna.get('BPM'), num):
        for f, want in (('BEAT', 60.0 / dna['BPM']), ('STEP', 15.0 / dna['BPM'])):
            if getattr(mod, f, None) != want:
                out.append(f'{f} must be {want!r}: 60 / BPM, and a quarter of it')
    return out


# ------------------------------------------------------------------ a kit's items
def check_unique(found):
    """found: (module, item) pairs. Names are the files' <Sound>, so two modules may never share one."""
    seen = {}
    for mod, it in found:
        if it.name in seen:
            raise ValueError(f'item name {it.name!r} is in both {seen[it.name]} and {mod}')
        seen[it.name] = mod


def collect(package):
    """Every ITEMS entry of every module of a recipe package, in module-name order then declaration order."""
    pkg = importlib.import_module(package)
    found = []
    for m in sorted(info.name for info in pkgutil.iter_modules(pkg.__path__) if not info.name.startswith('_')):
        mod = importlib.import_module(f'{package}.{m}')
        found += [(mod.__name__, it) for it in getattr(mod, 'ITEMS', ())]
    check_unique(found)
    return [it for _, it in found]


def items(kit, sets=None, only=None):
    """A kit's items (kit: its module), filtered to the render sets in `sets` and to the names in `only` (UnknownName
    if one isn't in the kit)."""
    every = collect(kit.RECIPES)
    if only:
        missing = sorted(set(only) - {it.name for it in every})
        if missing:
            raise UnknownName(f'no item named {", ".join(missing)} in {kit.KIT}')
        every = [it for it in every if it.name in set(only)]
    if sets:
        every = [it for it in every if set(it.sets) & set(sets)]
    return every
