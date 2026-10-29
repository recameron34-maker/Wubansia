"""FUT01 "Alloy Signal": sci-fi club x future garage / 2-step, 134 BPM, F# minor (Camelot 11A).

The kit DNA every recipe draws on, and the registry of the kit's sounds. Each module in tools/sound/recipes/ exposes
ITEMS = [Item(...), ...]; items() collects them in module-name order, so designers add sounds in their own files and
never edit the same one.
"""
import importlib
import pkgutil

from tools.sound import dsp48 as D
from tools.sound.item import UnknownName

KIT = 'FUT01'
TITLE = 'Alloy Signal'
BPM = 134.0
KEY = 'F# minor'
CAMELOT = '11A'
ROOT = 42                    # F#2 (MIDI, C4 = 60): the key's root in the bass register
KICK_NOTE = 30               # F#1, 46.25 Hz: the kick and the sub
SCALE = (6, 8, 9, 11, 1, 2, 4)  # F# natural minor pitch classes: F# G# A B C# D E

# the kit DNA (plan section 2.1)
ALLOY = (1.0, 1.483, 1.932, 2.546, 3.011, 3.715, 4.468)  # inharmonic ratios: perc, stabs, impacts, the resonator
VOICE = (('bf_emma', 0.6), ('af_nicole', 0.4))           # the one Kokoro voice blend
SPACE_SEED = 'FUT01/space'                                # seeds every room and FDN in the kit

BEAT = 60.0 / BPM            # seconds
STEP = BEAT / 4              # one 16th
SWING = 0.57                 # the groove's base swing for the kit's written phrases: where the second 16th of each 8th
                             # lands, as a share of the 8th (0.5 straight; 15.7 ms late at 134 BPM). One value for every
                             # lane, so hits that share a step land together (plan section 2.3)
RECIPES = 'tools.sound.recipes'


def space_rng(*extra):
    """The kit's space: pass it to fdn_reverb so every sound sits in the same room."""
    return D.rng(SPACE_SEED, *extra)


def in_key(note):
    return int(note) % 12 in SCALE


def check_unique(found):
    """found: (module, item) pairs. Names are the files' <Sound>, so two modules may never share one."""
    seen = {}
    for mod, it in found:
        if it.name in seen:
            raise ValueError(f'item name {it.name!r} is in both {seen[it.name]} and {mod}')
        seen[it.name] = mod


def collect(package=RECIPES):
    """Every ITEMS entry of every recipe module, in module-name order then declaration order."""
    pkg = importlib.import_module(package)
    found = []
    for m in sorted(info.name for info in pkgutil.iter_modules(pkg.__path__) if not info.name.startswith('_')):
        mod = importlib.import_module(f'{package}.{m}')
        found += [(mod.__name__, it) for it in getattr(mod, 'ITEMS', ())]
    check_unique(found)
    return [it for _, it in found]


def items(sets=None, only=None):
    """The kit's items, filtered to the render sets in `sets` and to the names in `only` (UnknownName if one isn't
    in the kit)."""
    every = collect()
    if only:
        names = {it.name for it in every}
        missing = sorted(set(only) - names)
        if missing:
            raise UnknownName(f'no item named {", ".join(missing)} in {KIT}')
        every = [it for it in every if it.name in set(only)]
    if sets:
        every = [it for it in every if set(it.sets) & set(sets)]
    return every
