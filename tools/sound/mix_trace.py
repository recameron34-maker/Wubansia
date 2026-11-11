"""Trace every DSP call a kit's round 5 context mix makes, in both variants, and classify it.

    python3 tools/sound/mix_trace.py --kit FUT01 [--variant vox] [--list] [--json out.json]

run(kit, variant) renders the kit's own club loop for the variant (context5's club(), which renders the wet loop) with
every module-level function of the DSP library (dsp48, master, phrase, sends, mixmeter, qa, parts, sampler) and of the
kit's recipe modules wrapped, and records each call made from recipe code (and the club master's own stages, made from
master.master). Every recorded call is then classified by the kit's table (rules() in the kit's own mix module,
<RECIPES>.plugin_mix, after COMMON), the first rule that matches:

- live: an exported live insert, return or master stage (its manifest path);
- causal: a causal equivalent of a zero-phase or two-sided step (what replaces it);
- fixed: a solved or measured value exported as a fixed one (or a measurement feeding one);
- dropped: dropped, with the reason;
- dry: the dry lane, what the voices play (kit.json's sampler and phrase lanes, P2): engine_ref's input;
- structure: the recipe composing lanes, buses and files (no DSP of its own, or DSP its parts already account for).

A call no rule matches is unclassified, and classify() raises. While it runs, the trace also captures what
engine_ref.py needs from the recipe (the kit module's Capture): each channel's dry lane, the thrown parts, and the
values the manifest fixes (clip ceilings from the lanes' true peaks, solved faders, the club master's solved glue
threshold and drive, the level ducks' tops), so the manifest and the render come from the recipe's own calls.
"""
import argparse
import collections
import dataclasses
import functools
import importlib
import inspect
import json
import re
import sys
import time
from pathlib import Path

if __package__ in (None, ''):  # run as a script: import the tools.sound package from the repo root
    sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

import tools.sound  # noqa: E402,F401  first: it pins numpy's SIMD level before numpy loads

import numpy as np  # noqa: E402

LIB = ('dsp48', 'master', 'phrase', 'sends', 'mixmeter', 'qa', 'parts', 'sampler')
CLASSES = ('live', 'causal', 'fixed', 'dropped', 'dry', 'structure')
MIX_CLASSES = CLASSES[:4]          # the mix's DSP; dry and structure are the voices' and the recipe's composition
MASTER_FRAMES = {'master', 'master.<locals>.<lambda>', '_chain'}   # master.py frames whose calls are its stages
ALWAYS = {'phrase.kit_files'}      # recorded wherever they are called from: the kit's files, rendered (dry)


# ------------------------------------------------------------------ records
@dataclasses.dataclass
class Call:
    seq: int
    fn: str                   # 'dsp48.peaking', 'context4.inserts', ...
    args: dict                # the call's arguments by name, summarised (summary())
    parents: tuple            # the enclosing recorded calls, outermost first: (fn, args) each
    out: object = None        # the result, summarised
    cls: str = None
    why: str = None           # the target (a manifest path or what replaces it) or the reason
    parent_seq: int = -1      # the innermost enclosing recorded call
    nth: int = 0              # this call's index among its parent's calls to the same function

    def under(self, fn, **match):
        """The nearest enclosing call to fn whose args match (None when there is none)."""
        for f, a in reversed(self.parents):
            if f == fn and all(a.get(k) == v for k, v in match.items()):
                return a
        return None

    def parent(self):
        return self.parents[-1][0] if self.parents else None

    def step(self):
        """What makes two calls the same step: the function, its parent and its scalar arguments."""
        scal = tuple(sorted((k, v) for k, v in self.args.items() if isinstance(v, (int, float, str, bool, type(None)))))
        return (self.fn, self.parent(), scal)


def summary(v, depth=0):
    """A JSON-ready summary of an argument or a result: scalars as they are, small tuples of scalars, arrays by
    shape, callables by name, everything else by type."""
    if v is None or isinstance(v, (bool, str)):
        return v
    if isinstance(v, (int, np.integer)):
        return int(v)
    if isinstance(v, (float, np.floating)):
        return round(float(v), 9)
    if isinstance(v, np.ndarray):
        if v.ndim == 0:
            return summary(v.item())
        return ('array', tuple(v.shape))
    if isinstance(v, (tuple, list)):
        if depth < 2 and len(v) <= 64:
            return tuple(summary(u, depth + 1) for u in v)
        return ('seq', len(v))
    if isinstance(v, dict):
        if depth < 2 and len(v) <= 32:
            return {str(k): summary(u, depth + 1) for k, u in v.items()}
        return ('dict', len(v))
    if callable(v):
        return ('fn', getattr(v, '__qualname__', type(v).__name__))
    if hasattr(v, 'name') and isinstance(getattr(v, 'name'), str):
        return (type(v).__name__, v.name)
    if hasattr(v, 'KIT'):
        return ('kit', v.KIT)
    return (type(v).__name__,)


# ------------------------------------------------------------------ the tracer
class Tracer:
    """Wraps the library's and the kit's recipe modules' functions while active; records the calls recipe code (and
    master.master) makes; lets a capture hook keep the arrays engine_ref needs."""

    def __init__(self, kit, hook=None):
        from tools.sound import kits
        self.kit = kit
        self.prefix = kits.load(kit).RECIPES
        self.own = f'{self.prefix}.plugin_mix'     # the kit's mix module: its render() asks the recipe
        self.hook = hook
        self.calls = []
        self.stack = []
        self.on = False
        self._saved = []

    def modules(self):
        importlib.import_module(self.prefix + '.context5')     # the kit's mix and every recipe module it uses
        mods = [importlib.import_module(f'tools.sound.{m}') for m in LIB]
        mods += [m for name, m in sorted(sys.modules.items())
                 if name.startswith(self.prefix + '.') and m is not None and name != self.own]
        return mods

    def _recorded(self, frame):
        g = frame.f_globals.get('__name__', '')
        if g == self.own:
            return frame.f_code.co_name == 'render'       # what the kit's mix module asks of the recipe, not its hooks
        if g.startswith(self.prefix):
            return True
        return g == 'tools.sound.master' and frame.f_code.co_qualname in MASTER_FRAMES

    def _wrap(self, mod, name, fn):
        short = mod.__name__.rsplit('.', 1)[-1]
        qual = f'{short}.{name}'
        try:
            sig = inspect.signature(fn)
        except (TypeError, ValueError):
            sig = None
        tracer = self

        @functools.wraps(fn)
        def wrapper(*a, **k):
            if not tracer.on or not (qual in ALWAYS or tracer._recorded(sys._getframe(1))):
                return fn(*a, **k)
            try:
                bound = sig.bind(*a, **k) if sig else None
                if bound:
                    bound.apply_defaults()
                args = dict(bound.arguments) if bound else {'args': a, **k}
            except TypeError:
                args = {'args': a, **k}
            rec = Call(len(tracer.calls), qual, {n: summary(v) for n, v in args.items()},
                       tuple((c.fn, c.args) for c in tracer.stack),
                       parent_seq=tracer.stack[-1].seq if tracer.stack else -1)
            tracer.calls.append(rec)
            tracer.stack.append(rec)
            try:
                out = fn(*a, **k)
            finally:
                tracer.stack.pop()
            rec.out = summary(out)
            if tracer.hook:
                tracer.hook(rec, args, out)
            return out
        return wrapper

    def __enter__(self):
        for mod in self.modules():
            for name, fn in list(vars(mod).items()):
                if hasattr(fn, 'cache_clear') and mod.__name__.startswith(self.prefix):
                    fn.cache_clear()         # recipe caches: every lane renders again, under the trace
                if not callable(fn) or isinstance(fn, type) or name.startswith('__'):
                    continue
                target = getattr(fn, '__wrapped__', fn)
                if getattr(target, '__module__', None) != mod.__name__:
                    continue                 # imported into the module, not defined there
                self._saved.append((mod, name, fn))
                setattr(mod, name, self._wrap(mod, name, fn))
        self.on = True
        return self

    def __exit__(self, *exc):
        self.on = False
        for mod, name, fn in reversed(self._saved):
            setattr(mod, name, fn)
            if hasattr(fn, 'cache_clear') and mod.__name__.startswith(self.prefix):
                fn.cache_clear()
        self._saved = []
        return False


# ------------------------------------------------------------------ classification
@dataclasses.dataclass(frozen=True)
class Rule:
    """A call to fn (a name or a regex) that is under `under` (an enclosing call's fn, optional) and passes `when`
    (a predicate on the Call, optional) is `cls`, with `why` (a string, or a function of the Call)."""
    fn: str
    cls: str
    why: object
    under: str = None
    when: object = None

    def matches(self, c):
        if not (c.fn == self.fn or (any(ch in self.fn for ch in '.*[]|^$') and re.fullmatch(self.fn, c.fn))):
            return False
        if self.under and not any(f == self.under for f, _ in c.parents):
            return False
        return self.when is None or bool(self.when(c))


def classify(calls, rules):
    """Each call's class and reason, by the first rule that matches; ValueError listing the calls none matches."""
    count = collections.Counter()
    for c in calls:
        c.nth = count[(c.parent_seq, c.fn)]
        count[(c.parent_seq, c.fn)] += 1
    missing = []
    for c in calls:
        for r in rules:
            if r.matches(c):
                c.cls = r.cls
                c.why = r.why(c) if callable(r.why) else r.why
                break
        else:
            missing.append(c)
    if missing:
        rows = sorted({(c.fn, c.parent()) for c in missing})
        raise ValueError(f'{len(missing)} unclassified calls: ' + '; '.join(f'{f} under {p}' for f, p in rows))
    return calls


def report(calls):
    """Counts by class (calls and distinct steps), and every dropped step with its reason."""
    by = collections.Counter(c.cls for c in calls)
    steps = {}
    for c in calls:
        steps.setdefault((c.cls, c.why, c.fn), c)
    distinct = collections.Counter(cls for cls, _, _ in steps)
    dropped = sorted({(c.fn, c.why) for c in calls if c.cls == 'dropped'})
    return {'calls': {k: by.get(k, 0) for k in CLASSES}, 'steps': {k: distinct.get(k, 0) for k in CLASSES},
            'dropped': [{'fn': f, 'why': w} for f, w in dropped]}


# ------------------------------------------------------------------ running a kit
def kit_module(kit):
    """The kit's own mix module, <RECIPES>.plugin_mix (render, Capture, rules, build, keys, loop_info, ride_points,
    R5_LOOPS, VARIANTS): shared code reaches a kit only through its RECIPES (ADR 0008)."""
    from tools.sound import kits
    return importlib.import_module(f'{kits.load(kit).RECIPES}.plugin_mix')


@functools.lru_cache(maxsize=None)
def run(kit, variant):
    """Render the kit's club loop for the variant ('vox' or 'novox') under the trace: (calls classified, capture)."""
    mod = kit_module(kit)
    cap = mod.Capture(kit, variant)
    t0 = time.time()
    with Tracer(kit, hook=cap.hook) as tr:
        mod.render(mod.VARIANTS[variant])
    cap.finish()
    calls = classify(tr.calls, mod.rules(mod.VARIANTS[variant]))
    cap.seconds = round(time.time() - t0, 1)
    return calls, cap


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    ap.add_argument('--kit', required=True)
    ap.add_argument('--variant', choices=('vox', 'novox'), action='append')
    ap.add_argument('--list', action='store_true', help='print every distinct step with its class')
    ap.add_argument('--json', help='write the classified trace here')
    a = ap.parse_args(argv)
    out = {}
    for v in a.variant or ('vox', 'novox'):
        calls, cap = run(a.kit, v)
        out[v] = {'report': report(calls), 'seconds': cap.seconds}
        if a.list:
            seen = set()
            for c in calls:
                key = (c.cls, c.why, c.fn, c.parent())
                if key not in seen:
                    seen.add(key)
                    print(f'{v:6} {c.cls:9} {c.fn:28} under {str(c.parent()):28} {c.why}')
        print(json.dumps({v: out[v]['report']}, indent=1))
        if a.json:
            out[v]['calls'] = [dataclasses.asdict(c) for c in calls]
    if a.json:
        Path(a.json).write_text(json.dumps(out, indent=1, default=str) + '\n')
    return 0


# ------------------------------------------------------------------ captures
class Capture:
    """What a traced render keeps for engine_ref and the manifest (filled by the kit's hook):
    - dry: {channel: (L, 2)}: what the voices play into each channel over the loop, tails wrapped, on the sides the
      loops play (kick B on FUT01, bass B);
    - sides: {(channel, side): (L, 2)}: a dry lane on one side of an A/B switch ('a', 'b');
    - throws: {channel: [((L, 2), cc91, cc94)]}: the thrown voices among a channel's dry lane;
    - values: {key: float}: measured and solved values (clip ceilings, faders, tops, the master's);
    - lanes: {recipe lane: (L, 2)}: each lane as the recipe mixes it (the reference's channel peaks);
    - wet, club: the recipe's wet loop (the club master's input) and club loop; master: the master's report."""

    def __init__(self, kit, variant):
        self.kit, self.variant = kit, variant
        self.dry, self.sides, self.throws, self.values, self.lanes = {}, {}, {}, {}, {}
        self.wet = self.club = self.club_input = self.master = None
        self.seconds = None

    @staticmethod
    def copy(x):
        return np.array(D_st(x), dtype=float, copy=True)

    def put(self, channel, x, side=None, loops=True):
        """A dry lane: the loops' own when loops (dry), and its A/B side when given (sides)."""
        x = self.copy(x)
        if side:
            self.sides[(channel, side)] = x
        if loops:
            self.dry[channel] = x

    def value(self, key, v):
        self.values[key] = float(v)

    def hook(self, rec, a, out):
        pass

    def master_hook(self, rec, a, out):
        """The club master's report and loops (master.master called from the recipe)."""
        if rec.fn == 'master.master':
            y, rep = out
            self.club, self.club_input, self.master = self.copy(y), self.copy(a['x']), rep
            self.value('glue_threshold_db', rep['glue']['threshold_dbfs'])
            self.value('drive_db', rep['drive_db'])
            self.value('pre_master_lufs', rep['input']['lufs_i'])
            self.value('club_lufs', rep['output']['lufs_i'])
            self.value('trim_db', rep['trim_db'])

    def finish(self):
        pass


def D_st(x):
    from tools.sound import dsp48 as D
    return D.st(x)


# a rule's predicates
def arg(name, *values):
    return lambda c: c.args.get(name) in values


def anc(fn, **match):
    return lambda c: c.under(fn, **match) is not None


def scalar_arg(name):
    return lambda c: isinstance(c.args.get(name), (int, float))


def array_arg(name):
    return lambda c: isinstance(c.args.get(name), tuple) and c.args[name][:1] == ('array',)


def nth(k):
    return lambda c: c.nth == k


def lane_of(fn, key='k'):
    """The lane an enclosing call names (its argument `key`)."""
    def get(c):
        a = c.under(fn)
        return a.get(key) if a else None
    return get


COMMON = (
    Rule('phrase.kit_files', 'dry', "the kit's files as render.py writes them: kit.json's samples"),
    Rule('context5?.club', 'structure', 'the club loop: the wet loop through the club master'),
    Rule(r'[a-z0-9_]+\.[A-Za-z0-9_]+', 'dry', "inside an item's render: the samples themselves",
         under='phrase.kit_files'),
    Rule('phrase.hits_lane', 'dry', 'one-shots on the grid: the sampler (the layer nearest the velocity, round '
                                    'robins in turn)'),
    Rule(r'[a-z0-9_]+\.[A-Za-z0-9_]+', 'dry', 'one-shots on the grid: the sampler', under='phrase.hits_lane'),
    Rule('phrase.circular', 'structure', 'round the loop: the steady state a stream reaches on its second pass'),
    Rule('phrase.two_pass', 'dropped', "the seam: two_pass crossfades its last 20 ms into the pass before so the "
                                       "file loops; a stream's tail joins by itself (the return inside is live)"),
    Rule('phrase.second_pass', 'dropped', 'the seam crossfade (two_pass)'),
    Rule('master.settings', 'structure', "the club master's settings: the solver's target, PLR floor and cap"),
    Rule('master.master', 'structure', 'the club master: its stages below'),
    Rule('master.lowcut', 'live', 'master.lowcut_hz, lowcut_order', under='master.master'),
    Rule('master.mono_below', 'live', 'master.mono_below_hz', under='master.master'),
    Rule('master.glue', 'fixed', 'master.<variant>.glue_threshold_mdb: the threshold solved for 2.5 dB of reduction '
                                 'at the loudest moment, fixed (rt_ref.glue_db)', under='master.master'),
    Rule('master._chain', 'structure', "the master's stages 4-8 at a drive (the solve runs it per try)"),
    Rule('dsp48.undb', 'fixed', 'master.<variant>.drive_mdb: the drive solved toward the target under the cap, '
                                'fixed', under='master._chain', when=nth(0)),
    Rule('dsp48.undb', 'dropped', "the trim (about -0.003 dB): the true peak a few mdB over the ceiling, which "
                                  "a live master can't read ahead of time", under='master._chain', when=nth(1)),
    Rule('master.soft_clip', 'live', 'master.clip (os_fir(4) at 4x, ADAA)', under='master._chain'),
    Rule('master.dc_clean', 'causal', 'master.dc_hz: a causal DC blocker before the limiter (rt_ref.dc_block) in '
                                      'place of the zero-phase 10 Hz clean', under='master._chain', when=nth(0)),
    Rule('master.dc_clean', 'dropped', "the second DC clean, after the limiter: live, a filter there moves the peaks "
                                       "over the ceiling, and the blocker before the limiter takes the ripple",
         under='master._chain', when=nth(1)),
    Rule('master.limit', 'causal', "master.limiter: its 60 ms hold looks back over the two-sided window's length "
                                   "(120 ms), reaching forward only over the true peak's FIR (rt_ref.limit_db, "
                                   'latency 289 against 3145); the two-sided hold stays for renders',
         under='master._chain'),
    Rule('mixmeter.true_peak_db', 'dropped', "the trim's reading", under='master._chain'),
    Rule('mixmeter.(true_peak_db|lufs_i)', 'fixed', "the drive solve's readings (the drive is exported)",
         under='master.master'),
    Rule('master.(_loud|limit_latency|punch|peaks)', 'structure', "the master's report", under='master.master'),
    Rule('dsp48.(st|undb)', 'structure', "the master's input as stereo; the report's driven loop",
         under='master.master'),
    Rule('sends.space', 'live', 'returns.space: the FDN at the exported delays and signs (the PCG64 draws)'),
    Rule('sends.echo', 'live', 'returns.echo: the ping-pong at the exported wow and flutter phases'),
    Rule('sends.resonator', 'live', 'returns.resonator: tones_mhz, tau_ms, send_hp_hz'),
    Rule('sends.duck', 'causal', 'returns.space.inserts: a level duck keyed on the vox channel, its top fixed at '
                                 "the key's loudest block in the reference loop, its gain 96 samples late "
                                 '(rt_ref.level_duck_db)'),
)


if __name__ == '__main__':
    sys.exit(main())
