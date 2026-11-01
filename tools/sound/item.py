"""What a recipe declares: an Item per instrument, rendered as one Take per note x velocity layer x round robin.

    Item('Drums', 'Kick', 'kick', render_kick, layers=(0.4, 0.55, 0.7, 0.85, 1.0), rr=2, root=30, sets=('s1a',))

render(take) returns float audio at 48 kHz, (n,) or (n, 2), or an Out when it also knows its splices, loop points or
chord tones. Everything random comes from take.rng(), so a take renders the same bytes whatever else ran first.
"""
import re
from dataclasses import dataclass, field

import numpy as np

from tools.sound import dsp48 as D
from tools.sound import qa

GROUPS = ('Drums', 'Perc', 'Bass', 'Stabs', 'Pad', 'Lead', 'Vox', 'FX', 'Textures', 'Loops')
QA_KEYS = ('mud', 'tonal', 'tune_cents', 'tune_window', 'sustain', 'harm_tol', 'loop_check', 'crest', 'why')
NAME = re.compile(r'[A-Z][A-Za-z0-9]*')
LOW_END_GROUPS = ('Bass', 'Loops')   # their mud rule may be 'off' only with a reason (qa['why'])


class UnknownName(KeyError):
    """A kit or item name that doesn't exist: a usage error, not a bug in a recipe."""


@dataclass
class Out:
    """A render with what QA needs to know about it."""
    audio: np.ndarray
    splices: tuple = ()      # sample positions the recipe cut or joined (stutters, chop edges)
    loop: tuple = None       # (start, end) samples, end exclusive, for loop items; default the whole file
    f0s: tuple = ()          # fundamentals present (a chord's notes), Hz, for the alias check
    hits: tuple = ()         # kick onsets (samples) in a loop: the mud gate then also runs per gap between them; in a
                             # bass-body phrase, the kicks it plays against (tag 'with_kick' if they're in the file)
    meta: dict = None        # facts about the file for the report (and later index.json): slices, a target's settings


@dataclass(frozen=True, eq=False)
class Item:
    group: str               # file group, one of GROUPS
    name: str                # CamelCase, unique in the kit: the <Sound> of the file name
    category: str            # one of qa.CATEGORIES: picks the QA rules (qa.CATEGORY_RULES)
    render: object           # render(take) -> audio or Out; a module-level function
    layers: tuple = (1.0,)   # velocities 0..1, softest first: files v1..vN
    rr: int = 1              # round robins: files rr1..rrN
    notes: tuple = ()        # MIDI notes of the zones (C4 = 60), ascending; empty = one unpitched sound
    root: int = None         # MIDI note an unzoned sound is tuned to (the kick on F#1 = 30)
    loop: bool = False       # a sustain or bed that loops: no tail trim or fade-out, seam checked instead
    level: float = None      # LUFS-I target for the loudest take; None = loudest take's true peak at the ceiling
    hp_hz: float = 20.0      # sub-sonic high-pass the renderer applies to every take; None for none
    flags: frozenset = frozenset()  # qa.FLAGS: deliberate_alias, explicit, hf_synthetic, cut_end
    tags: tuple = ()         # free tags; QA reads riser, wet, dry, clean, with_kick
    sets: tuple = ()         # render sets this item belongs to, e.g. ('s1a',)
    script: str = None       # the words, for vocals: ASR and content checks
    about: str = ''          # one line for the report
    qa: dict = field(default_factory=dict)  # rule overrides, QA_KEYS: e.g. {'mud': 'bass-body', 'why': 'B, warm'};
                                            # 'why' is one line of reason, written into the report's exceptions

    def __post_init__(self):
        object.__setattr__(self, 'flags', frozenset(self.flags))
        object.__setattr__(self, 'layers', tuple(float(v) for v in self.layers))
        object.__setattr__(self, 'notes', tuple(int(n) for n in self.notes))
        object.__setattr__(self, 'tags', tuple(self.tags))
        object.__setattr__(self, 'sets', tuple(self.sets))
        bad = []
        if self.group not in GROUPS:
            bad.append(f'group {self.group!r} is not one of {GROUPS}')
        if not NAME.fullmatch(self.name or ''):
            bad.append(f'name {self.name!r} must be CamelCase letters and digits')
        if self.category not in qa.CATEGORIES:
            bad.append(f'category {self.category!r} is not one of {qa.CATEGORIES}')
        if not self.layers or any(not 0 < v <= 1 for v in self.layers) or list(self.layers) != sorted(set(self.layers)):
            bad.append('layers must be distinct velocities in (0, 1], softest first')
        if self.rr < 1:
            bad.append('rr must be >= 1')
        if list(self.notes) != sorted(set(self.notes)) or any(not 0 <= n <= 127 for n in self.notes):
            bad.append('notes must be distinct MIDI notes, ascending')
        if self.flags - set(qa.FLAGS):
            bad.append(f'unknown flags {sorted(self.flags - set(qa.FLAGS))}; known: {qa.FLAGS}')
        if set(self.qa) - set(QA_KEYS):
            bad.append(f'unknown qa keys {sorted(set(self.qa) - set(QA_KEYS))}; known: {QA_KEYS}')
        if self.level is not None and self.level >= 0:
            bad.append('level is a LUFS target, below 0')
        if 'mud' in self.qa and self.qa['mud'] not in qa.MUD_MODES:
            bad.append(f"qa mud {self.qa['mud']!r} is not one of {qa.MUD_MODES}")
        if self.category in qa.STRICT and self.qa.get('mud', 'gate') != 'gate':
            bad.append(f"a {self.category} keeps the strict mud gate (ADR 0011): no qa mud {self.qa['mud']!r}")
        elif self.qa.get('mud') == 'bass-body' and self.category not in qa.BODY_CATEGORIES:
            bad.append(f"bass-body is for the categories {qa.BODY_CATEGORIES}, not {self.category!r} (ADR 0011)")
        elif self.qa.get('mud') == 'bass-body' and not self.qa.get('why'):
            bad.append("bass-body is an exception (ADR 0011): it needs qa['why']")
        if self.category in qa.CATEGORIES and self.group in LOW_END_GROUPS and not self.qa.get('why') \
                and self.qa.get('mud', qa.CATEGORY_RULES[self.category]['mud']) == 'off':
            bad.append(f"a {self.group} item needs a low-end rule: qa={{'mud': 'gate'}}, 'bass-body' or 'lowcut', or "
                       f"'off' with qa['why']")
        if self.category in qa.CATEGORIES and qa.CATEGORY_RULES[self.category]['crest'] \
                and self.qa.get('crest') is False and not self.qa.get('why'):
            bad.append("turning the kick crest floor off needs qa['why']")
        if bad:
            raise ValueError(f'Item {self.name!r}: ' + '; '.join(bad))

    def takes(self, kit):
        """Every file of the instrument, ordered note, layer, round robin."""
        return [Take(kit, self, layer, rr, note) for note in (self.notes or (None,))
                for layer in range(1, len(self.layers) + 1) for rr in range(1, self.rr + 1)]


@dataclass(frozen=True, eq=False)
class Take:
    kit: str
    item: Item
    layer: int               # 1..len(item.layers), softest first
    rr: int                  # 1..item.rr
    note: int = None         # the zone's MIDI note, or None

    @property
    def vel(self):
        return self.item.layers[self.layer - 1]

    @property
    def pitch(self):
        """The MIDI note this take must sound at: its zone, else the item's root."""
        return self.note if self.note is not None else self.item.root

    @property
    def f0(self):
        return None if self.pitch is None else float(D.midi_hz(self.pitch))

    @property
    def sound(self):
        return self.item.name if self.note is None else f'{self.item.name}-{D.note_name(self.note)}'

    @property
    def file(self):
        return f'{self.kit}_{self.item.group}_{self.sound}_v{self.layer}_rr{self.rr}.flac'

    def rng(self, *extra):
        """rng(kit, sound, layer, rr, *extra): this take's own noise; extra keys give a recipe independent streams."""
        return D.rng(self.kit, self.sound, self.layer, self.rr, *extra)

    def spec(self, out):
        """The QA spec of this take as rendered."""
        it = self.item
        rules = {k: v for k, v in it.qa.items() if k not in ('loop_check', 'why')}
        return qa.Spec(it.category, oneshot=not it.loop, f0=self.f0, f0s=tuple(out.f0s), flags=it.flags, tags=it.tags,
                       splices=tuple(out.splices), loop=out.loop if it.loop else None, hits=tuple(out.hits),
                       top=self.layer == len(it.layers), **rules)
