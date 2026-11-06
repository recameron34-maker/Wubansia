"""The kits: every kit module declares its DNA (CLAUDE.md: 128-140 BPM, a key and its Camelot code), the four kits
cover the four directions, no two share a space seed or a recipe package, a kit with TODO fields refuses to render, a
filled-in kit renders and listens like FUT01, the example recipe works for any kit, and recipes and shared code stay
apart.

Nothing here depends on whether FUT02-FUT04 are still stubs: the TODO cases blank a kit's fields for the test, so a
designer filling a kit never has to edit this file."""
import ast
import contextlib
import importlib
import io
import json
import re
import tempfile
import types
import unittest
import unittest.mock
from pathlib import Path

from tools.sound import dsp48 as D
from tools.sound import kits
from tools.sound import qa as Q
from tools.sound import render as R
from tools.sound.item import Item, UnknownName
from tools.sound.kits import fut02
from tools.sound.tests import fixtures as FX

SOUND = Path(__file__).resolve().parents[1]
REPO = SOUND.parents[1]
SHARED = sorted(p.stem for p in SOUND.glob('*.py'))         # every module of tools/sound is shared
DESIGNER = ('KEY', 'CAMELOT', 'SCALE', 'ROOT', 'KICK_NOTE', 'RATIOS', 'VOICE', 'SWING')   # what a kit's designer fills


def modules():
    """Every kit module, imported without the readiness check."""
    return {name: kits.module(name) for name in kits.names()}


def main(*argv):
    """render.main, quiet; returns (status, stderr)."""
    err = io.StringIO()
    with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(err):
        return R.main(list(argv)), err.getvalue()


def package_of(path):
    """The package a file in the repo belongs to: tools/sound/recipes/fut02/drums.py is in tools.sound.recipes.fut02."""
    return '.'.join(Path(path).resolve().relative_to(REPO).parent.parts)


def absolute(name, level, package):
    """A relative module name (`level` dots, then `name`) resolved against the package it is written in."""
    parts = package.split('.')
    return '.'.join(parts[:len(parts) - level + 1] + ([name] if name else []))


def called(node, package):
    """The names a call reaches by a string: importlib.import_module or __import__ of a module (relative to its second
    argument, or to the package), kits.load or kits.module of a kit."""
    fn = node.func.attr if isinstance(node.func, ast.Attribute) else getattr(node.func, 'id', None)
    args = [a.value for a in node.args if isinstance(a, ast.Constant) and isinstance(a.value, str)]
    if not args:
        return set()
    if fn in ('import_module', '__import__'):
        name = args[0]
        level = len(name) - len(name.lstrip('.'))
        return {absolute(name[level:], level, args[1] if len(args) > 1 else package) if level else name}
    if fn in ('load', 'module') and kits.KIT_NAME.fullmatch(args[0]):
        return {f'tools.sound.kits.{args[0].lower()}'}
    return set()


def reached(source, package):
    """Every dotted name a module's source reaches: its imports (`from a import b` gives a and a.b; relative ones
    resolved against its package) and the calls that load a module or a kit by name (called)."""
    names = set()
    for node in ast.walk(ast.parse(source)):
        if isinstance(node, ast.Import):
            names |= {a.name for a in node.names}
        elif isinstance(node, ast.ImportFrom):
            base = absolute(node.module, node.level, package) if node.level else node.module
            names |= {base} | {f'{base}.{a.name}' for a in node.names}
        elif isinstance(node, ast.Call):
            names |= called(node, package)
    return names


def imported(path):
    return reached(Path(path).read_text(encoding='utf-8'), package_of(path))


def kit_refs(names):
    """The kits a set of imported names reaches: through their kit module or their recipe package."""
    ids = {k.lower() for k in kits.names()}
    out = set()
    for n in names:
        parts = n.split('.')
        for i, p in enumerate(parts[:-1]):
            if p in ('kits', 'recipes') and parts[i + 1] in ids and parts[:i] == ['tools', 'sound']:
                out.add(parts[i + 1].upper())
    return out


def filled(mod, **over):
    """Patches that fill a kit's designer fields with valid DNA for a test, whether the kit is still a stub or not."""
    dna = dict(KEY='D minor', CAMELOT='7A', SCALE=kits.scale_of('D minor'), ROOT=38, KICK_NOTE=26,
               RATIOS=(1.0, 2.01, 2.98, 4.03), VOICE=(('af_bella', 0.7), ('af_sky', 0.3)), SWING=0.52)
    dna.update(over)
    return unittest.mock.patch.multiple(mod, **dna)


def blanked(mod):
    """Patches that set a kit's designer fields back to TODO for a test: a stub, whatever the kit's designer has done."""
    return unittest.mock.patch.multiple(mod, **{f: kits.TODO for f in DESIGNER})


def kick(take):
    return FX.clean_kick(take.vel, take.rr)


class TestKitDna(unittest.TestCase):
    def test_four_kits_one_per_direction(self):
        """Four kits, each in one of the known directions and no two in the same one. A direction may have no kit:
        round 4 moved FUT03 from hyper-clean future house to garage-house, and the old direction's club targets and
        extended-mix form stay valid (and tested) for a kit that takes it up again."""
        mods = modules()
        self.assertEqual(['FUT01', 'FUT02', 'FUT03', 'FUT04'], kits.names())
        directions = [m.DIRECTION for m in mods.values()]
        self.assertEqual(len(directions), len(set(directions)))
        self.assertLessEqual(set(directions), set(kits.DIRECTIONS))
        self.assertEqual({'FUT01': 134.0, 'FUT02': 130.0, 'FUT03': 128.0, 'FUT04': 136.0},
                         {k: m.BPM for k, m in mods.items()})

    def test_every_kit_declares_its_dna(self):
        """Every field is declared (TODO is allowed while a designer works); what is declared is valid, BPM in
        128-140 and the key and its Camelot code among it (CLAUDE.md content rules)."""
        for name, mod in modules().items():
            with self.subTest(kit=name):
                self.assertEqual([], [f for f in kits.DNA if not hasattr(mod, f)])
                self.assertEqual([], kits.problems(mod))
                self.assertEqual(name, mod.KIT)
                self.assertTrue(kits.BPM_RANGE[0] <= mod.BPM <= kits.BPM_RANGE[1], mod.BPM)
                self.assertEqual((60.0 / mod.BPM, 15.0 / mod.BPM), (mod.BEAT, mod.STEP))
                self.assertEqual(f'tools.sound.recipes.{name.lower()}', mod.RECIPES)
                self.assertIn(mod.GROOVE, Q.GROOVES)
                if mod.KEY is not kits.TODO:
                    self.assertEqual(kits.camelot(mod.KEY), mod.CAMELOT)
                    self.assertEqual(kits.scale_of(mod.KEY), tuple(mod.SCALE))

    def test_fut01_is_complete(self):
        self.assertEqual([], kits.todo(kits.module('FUT01')))
        self.assertEqual(('F# minor', '11A'), (kits.load('FUT01').KEY, kits.load('FUT01').CAMELOT))

    def test_only_a_designer_field_is_ever_todo(self):
        """The stubs come with KIT, TITLE, DIRECTION, BPM, SPACE_SEED, GROOVE and RECIPES set; a kit's designer fills
        the rest, so anything still TODO is one of theirs (while they work and after)."""
        for name, mod in modules().items():
            self.assertLessEqual(set(kits.todo(mod)), set(DESIGNER), name)

    def test_no_two_kits_share_a_space_seed_or_recipes(self):
        mods = list(modules().values())
        for field in ('SPACE_SEED', 'RECIPES', 'KIT'):
            values = [getattr(m, field) for m in mods]
            self.assertEqual(len(values), len(set(values)), field)
        ratios = [tuple(m.RATIOS) for m in mods if m.RATIOS is not kits.TODO]
        self.assertEqual(len(ratios), len(set(ratios)), 'two kits share a ratio set')

    def test_recipe_packages_exist(self):
        for name, mod in modules().items():
            pkg = importlib.import_module(mod.RECIPES)
            self.assertTrue(hasattr(pkg, '__path__'), f'{mod.RECIPES} must be a package')

    def test_camelot_wheel_matches_the_legacy_tables(self):
        """kits.camelot() against legacy core.js's MINOR and MAJOR tables (pitch class, name, Camelot code)."""
        src = (REPO / 'legacy/web-pack/core.js').read_text(encoding='utf-8')
        for mode, table in (('minor', 'MINOR'), ('major', 'MAJOR')):
            block = src[src.index(f'const {table} = {{'):]
            block = block[:block.index('};')]
            rows = re.findall(r"(\d+): \{ n: '([A-G][#b]?)', cam: '(\d+[AB])'", block)
            self.assertEqual(12, len(rows), table)
            for pc, note, cam in rows:
                self.assertEqual(cam, kits.camelot(f'{note} {mode}'), f'{note} {mode}')
                self.assertEqual(int(pc), kits.scale_of(f'{note} {mode}')[0])

    def test_scales(self):
        self.assertEqual((6, 8, 9, 11, 1, 2, 4), kits.scale_of('F# minor'))
        self.assertEqual((2, 4, 5, 7, 9, 10, 0), kits.scale_of('D minor'))
        self.assertEqual(kits.scale_of('Gb minor'), kits.scale_of('F# minor'))
        self.assertIs(kits.TODO, kits.scale_of(kits.TODO))
        fut01 = kits.module('FUT01')
        self.assertTrue(all(kits.in_key(fut01, n) for n in (30, 42, 44, 45, 47, 49, 50, 52)))
        self.assertFalse(kits.in_key(fut01, 43))

    def test_problems_name_each_bad_field(self):
        good = {f: getattr(kits.module('FUT01'), f) for f in kits.DNA + ('BEAT', 'STEP')}
        self.assertEqual([], kits.problems(types.SimpleNamespace(**good), 'fut01'))
        bad = {'BPM': 126.0, 'KEY': 'H minor', 'CAMELOT': '7A', 'SWING': 0.8, 'VOICE': (('bf_emma', 0.9),),
               'RATIOS': (1.5, 2.0, 3.0, 4.0), 'GROOVE': 'shuffle', 'RECIPES': 'tools.sound.recipes.other',
               'DIRECTION': 'trance', 'KICK_NOTE': 31, 'SPACE_SEED': ''}
        for field, value in bad.items():
            with self.subTest(field=field):
                got = kits.problems(types.SimpleNamespace(**{**good, field: value}), 'fut01')
                self.assertTrue(any(p.startswith(field) for p in got), got)
        self.assertTrue(any(p.startswith('VOICE') for p in kits.problems(
            types.SimpleNamespace(**{**good, 'VOICE': (('xx_nobody', 1.0),)}), 'fut01')))

    def test_kit_strings_pass_the_denylists(self):
        strings = [s for m in modules().values() for s in (m.KIT, m.TITLE, m.DIRECTION, m.SPACE_SEED)]
        self.assertEqual([], [c.name for c in Q.failed(Q.check_content(names=strings))])


class TestKitLoading(unittest.TestCase):
    def test_unknown_kits(self):
        for name in ('FUT99', 'fut1', '../qa', ''):
            with self.assertRaises(UnknownName, msg=name):
                kits.load(name)
        status, err = main('--kit', 'FUT99', '--out', 'unused')
        self.assertEqual(2, status)
        self.assertIn('FUT01, FUT02, FUT03, FUT04', err)

    def test_a_kit_with_todo_fields_refuses_to_render(self):
        """Every kit, its designer fields blanked back to TODO for the test: load() refuses it and render.py exits 2,
        naming the file and the fields, before it writes anything."""
        for name in kits.names():
            with self.subTest(kit=name), tempfile.TemporaryDirectory() as out, blanked(kits.module(name)):
                with self.assertRaises(kits.KitNotReady):
                    kits.load(name)
                status, err = main('--kit', name, '--out', out)
                self.assertEqual(2, status)
                self.assertIn(f'tools/sound/kits/{name.lower()}.py', err)
                self.assertIn('still TODO: KEY, CAMELOT, SCALE, ROOT, KICK_NOTE, RATIOS, VOICE, SWING', err)
                self.assertEqual([], list(Path(out).iterdir()))

    def test_a_filled_in_kit_renders_and_listens(self):
        """Once its DNA is set, a new kit renders with the same tools as FUT01: its own file names, its own groove for
        the kick loop, the default --out, and the listening copies."""
        from tools.sound import listen
        from tools.sound.recipes.fut01.example import ping     # a test may borrow a recipe; a kit may not
        items = [Item('Perc', 'Ping', 'perc', ping, layers=(0.5, 1.0), root=78, sets=('r2',)),
                 Item('Drums', 'TestKick', 'kick', kick, layers=(0.4, 0.7, 1.0), rr=2, root=30, sets=('r2',))]
        with tempfile.TemporaryDirectory() as tmp, contextlib.chdir(tmp), filled(fut02), \
                unittest.mock.patch.object(kits, 'collect', lambda package: list(items)):
            self.assertEqual([], kits.todo(kits.load('FUT02')))
            status, err = main('--kit', 'FUT02', '--set', 'r2', '--no-sheets')
            self.assertEqual(0, status, err)
            out = Path(tmp) / 'build' / 'FUT02-r2'
            report = json.loads((out / 'qa_report.json').read_text())
            self.assertEqual(('FUT02', fut02.TITLE), (report['kit'], report['title']))
            files = sorted(p.name for p in out.glob('*.flac'))
            self.assertEqual(8, len(files))
            self.assertTrue(all(f.startswith('FUT02_') for f in files), files)
            self.assertTrue((out / 'qa' / 'FUT02_Drums_TestKick_4x4.flac').exists())
            kick_checks = {c['name'] for it in report['items'] if it['name'] == 'TestKick' for c in it['checks']}
            self.assertIn('loop_mud_vs_sub', kick_checks)
            wavs = listen.build(out)
            self.assertIn('Ping v2.wav', {p.name for p in wavs})

    def test_the_example_recipe_renders_for_any_kit(self):
        """recipes/fut01/example.py copied as README step 2 says, only its kit import changed, renders a filled-in kit
        with no failed check: the template uses no FUT01-only name."""
        path = SOUND / 'recipes' / 'fut01' / 'example.py'
        src, mine = path.read_text(encoding='utf-8'), 'from tools.sound.kits import fut01 as K'
        self.assertEqual(1, src.count(mine))
        copy = types.ModuleType('example_copied_to_fut02')
        with filled(fut02), tempfile.TemporaryDirectory() as tmp:
            exec(compile(src.replace(mine, 'from tools.sound.kits import fut02 as K'), str(path), 'exec'), copy.__dict__)
            for it in copy.ITEMS:
                entry = R.qa_item(R.render_item(it, 'FUT02'), Path(tmp), kits.load('FUT02'))
                self.assertEqual([], [c['name'] for _, c in R.failures(entry)])
                self.assertTrue(all(f['file'].startswith('FUT02_') for f in entry['files']))

    def test_the_groove_sets_the_kick_loop(self):
        two, onsets2 = Q.two_step_loop({1.0: FX.clean_kick()}, 134.0, Q.GROOVES['2step'])
        four, onsets4 = Q.two_step_loop({1.0: FX.clean_kick()}, 130.0, Q.GROOVES['4x4'])
        s16 = 60 / 130 / 4 * D.SR
        self.assertEqual([int(round(4 * k * s16)) for k in range(8)], onsets4)
        self.assertEqual(5, len(onsets2))
        self.assertEqual(round(32 * s16), len(four))


class TestIsolation(unittest.TestCase):
    """Designers fill only their own kit module and recipe package: a kit's recipes reach no other kit, and the shared
    code reaches none."""

    def test_recipes_import_only_their_own_kit(self):
        for name in kits.names():
            pkg = SOUND / 'recipes' / name.lower()
            for path in sorted(pkg.glob('*.py')):
                with self.subTest(file=str(path.relative_to(SOUND))):
                    self.assertLessEqual(kit_refs(imported(path)), {name})

    def test_kit_modules_import_no_other_kit(self):
        for name in kits.names():
            self.assertLessEqual(kit_refs(imported(SOUND / 'kits' / f'{name.lower()}.py')), {name})

    def test_shared_code_imports_no_kit(self):
        for mod in SHARED:
            with self.subTest(module=mod):
                self.assertEqual(set(), kit_refs(imported(SOUND / f'{mod}.py')))
        for path in (SOUND / 'kits' / '__init__.py', SOUND / 'recipes' / '__init__.py'):
            self.assertEqual(set(), kit_refs(imported(path)))

    def test_no_recipe_module_outside_a_kit_package(self):
        self.assertEqual(['__init__.py'], sorted(p.name for p in (SOUND / 'recipes').glob('*.py')))

    def test_the_check_sees_every_way_in(self):
        """Each way a recipe could reach another kit counts, written in a package that isn't a kit (fut05): relative
        imports, import_module and __import__ of a string, kits.load and kits.module of a name. A kit's own stays
        allowed."""
        sneaky = ('from ..fut01 import drums', 'from .. import fut01', 'from ...kits import fut01',
                  "importlib.import_module('tools.sound.recipes.fut01.bass')",
                  "importlib.import_module('.fut01', 'tools.sound.recipes')", "__import__('tools.sound.kits.fut01')",
                  "kits.load('FUT01')", "kits.module('FUT01')", 'import tools.sound.recipes.fut01.drums')
        for line in sneaky:
            with self.subTest(line=line):
                self.assertEqual({'FUT01'}, kit_refs(reached(line, 'tools.sound.recipes.fut05')))
        for line in ('from . import drums', 'from tools.sound.kits import fut01 as K', "kits.load('FUT01')"):
            self.assertLessEqual(kit_refs(reached(line, 'tools.sound.recipes.fut01')), {'FUT01'}, line)
        self.assertEqual(set(), kit_refs(reached('from . import parts\nfrom .kits import TODO', 'tools.sound')))


if __name__ == '__main__':
    unittest.main()
