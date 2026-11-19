"""Drum kits (ADR 0030): the committed SFZ texts (plugin/tests/data/drumkit/) through sfzlint and the real sfizz engine.

Test time only: sfzlint and pysfizz from tools/content/requirements-drumkit.txt, numpy from tools/sound/requirements.txt.
Without them these tests skip, unless WUBANSIA_SFZ_TOOLS=1, where a missing module fails them. CI's sound job installs
the requirements (wheels only) into its venv and sets WUBANSIA_SFZ_TOOLS=1 in its "Content tools" step; the macOS job runs
this folder with the runner's bare python3, no packages, and skips them (GitHub sets CI=true on both, so CI isn't the
switch).

- sfzlint: each SFZ in a temp folder beside empty WAVs under the names its sample= lines give. Any output line fails
  (sfzlint exits 0 even when it finds errors).
- sfizz: each named WAV becomes a 48 kHz / 24-bit mono tone at its own frequency (file i at 200 + 150 i Hz, under 20 kHz);
  sfizz loads the SFZ (every region), then plays every one-shot pad at velocities 1 and 127 and each layer's vel and
  vel - 1, two rounds of round robins from a fresh synth, at a 64-frame block. The loudest frequency after the first
  block names the file it played, which must be the one kit.json gives by Wubansia's rules (the zone holding the key,
  the nearest layer, the louder on a tie, round robin hit % count). Then FUT01's hat choke (the open hat 60 dB down
  12 ms after the closed hat) and FUT04's rumble (it loops past its file's end while held, silent 20 ms after release).

From the repo root: python3 -m unittest discover -s tools/content/tests -t .
"""
import importlib.util
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[3]
DATA = REPO / 'plugin' / 'tests' / 'data' / 'drumkit'
KITS = ('FUT01', 'FUT02', 'FUT03', 'FUT04')
MODULES = ('sfzlint', 'pysfizz', 'numpy')
SWITCH = 'WUBANSIA_SFZ_TOOLS'
NOTE_NAMES = ('C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B')
SR, BLOCK = 48000, 64


def tools_gate(missing, environ):
    """What to do about the SFZ tools: ('run', ''), ('skip', why) or ('fail', why)."""
    if not missing:
        return 'run', ''
    names = ', '.join(missing)
    if environ.get(SWITCH) == '1':
        return 'fail', f'{SWITCH}=1 but {names} is missing: pip install -r tools/content/requirements-drumkit.txt'
    return 'skip', (f'the SFZ tools aren\'t installed ({names}): pip install -r tools/content/requirements-drumkit.txt '
                    f'-r tools/sound/requirements.txt, and set {SWITCH}=1 to make their absence a failure')


def missing_modules():
    return [m for m in MODULES if importlib.util.find_spec(m) is None]


def require_tools(test):
    action, why = tools_gate(missing_modules(), os.environ)
    if action == 'skip':
        raise unittest.SkipTest(why)
    if action == 'fail':
        test.fail(why)


def note_name(key):
    return f'{NOTE_NAMES[key % 12]}{key // 12 - 2}'


def load_kit(kit_id):
    return json.loads((REPO / 'content' / 'plugin' / kit_id.lower() / f'{kit_id}_kit.json').read_text(encoding='utf-8'))


def kit_folder(kit_id):
    folders = sorted(DATA.glob(f'{kit_id} * Drum Kit*'))
    return folders[0] if len(folders) == 1 else None


def sfz_files(kit_id):
    folder = kit_folder(kit_id)
    return sorted(folder.glob('*.sfz')) if folder else []


def sample_names(text):
    return sorted(set(re.findall(r'sample=(.*)$', text, re.M)))


def pick_layer(layers, vel):
    """engine::pickLayer: the nearest layer velocity, the louder on a tie."""
    best, best_d = -1, 0
    for i, layer in enumerate(layers):
        d = abs(layer['vel'] - vel)
        if best < 0 or d <= best_d:
            best, best_d = i, d
    return best


def side_of(kit, sfz_path):
    """'a' for the kick switch's A side's SFZ, else 'b' (Wubansia's default, and the only SFZ of a kit without one)."""
    a = kit['kit']['ab'].get('kick', {}).get('a', '')
    return 'a' if a and sfz_path.name.endswith(f' - {a}.sfz') else 'b'


def instrument_on(kit, key, side):
    for e in kit['keymap']:
        if e['ch'] == 10 and e['keys'][0] <= key <= e['keys'][1]:
            return e[side] if e.get('b') else e['a']
    return None


class ToolsGate(unittest.TestCase):
    """Runs everywhere: when the tests below skip, and when they must fail instead."""

    def test_without_the_modules_it_skips_naming_the_switch(self):
        action, why = tools_gate(['pysfizz'], {})
        self.assertEqual(action, 'skip')
        self.assertIn(SWITCH, why)
        self.assertIn('requirements-drumkit.txt', why)
        self.assertEqual(tools_gate(['pysfizz'], {'CI': 'true'})[0], 'skip')  # the macOS job's bare python3

    def test_with_the_switch_a_missing_module_fails(self):
        action, why = tools_gate(['sfzlint', 'numpy'], {SWITCH: '1'})
        self.assertEqual(action, 'fail')
        self.assertIn('sfzlint, numpy', why)

    def test_with_the_modules_it_runs(self):
        self.assertEqual(tools_gate([], {SWITCH: '1'}), ('run', ''))
        self.assertEqual(tools_gate([], {}), ('run', ''))

    def test_every_kit_has_its_texts(self):
        for kit_id in KITS:
            with self.subTest(kit=kit_id):
                self.assertIsNotNone(kit_folder(kit_id), f'no {kit_id} drum kit folder in {DATA}')
                self.assertTrue(sfz_files(kit_id))


class Sfzlint(unittest.TestCase):
    def test_sfzlint_finds_nothing(self):
        require_tools(self)
        sfzlint = shutil.which('sfzlint', path=os.path.dirname(sys.executable)) or shutil.which('sfzlint')
        self.assertIsNotNone(sfzlint, 'no sfzlint command beside this Python')
        for kit_id in KITS:
            for sfz in sfz_files(kit_id):
                with self.subTest(sfz=sfz.name), tempfile.TemporaryDirectory() as work:
                    text = sfz.read_text(encoding='utf-8')
                    (Path(work) / 'Samples').mkdir()
                    for name in sample_names(text):
                        (Path(work) / 'Samples' / name).write_bytes(b'')
                    shutil.copy(sfz, work)
                    run = subprocess.run([sfzlint, str(Path(work) / sfz.name)], capture_output=True, text=True, timeout=120)
                    self.assertEqual(run.returncode, 0, run.stderr)
                    self.assertEqual((run.stdout + run.stderr).strip(), '', f'sfzlint on {sfz.name}')


class Sfizz(unittest.TestCase):
    def setUp(self):
        require_tools(self)
        import numpy  # noqa: F401 (after the gate)
        import pysfizz  # noqa: F401
        self.np = sys.modules['numpy']
        self.pysfizz = sys.modules['pysfizz']

    def write_tone(self, path, freq, frames, silent=False):
        np = self.np
        t = np.arange(frames) / SR
        x = np.zeros(frames) if silent else 0.5 * np.sin(2 * np.pi * freq * t)
        v = np.round(x * (1 << 23)).astype(np.int64).clip(-(1 << 23), (1 << 23) - 1)
        b = (v & 0xFFFFFF).astype('<u4').view(np.uint8).reshape(-1, 4)[:, :3].tobytes()
        head = b'RIFF' + (36 + len(b) + (len(b) & 1)).to_bytes(4, 'little') + b'WAVEfmt ' + (16).to_bytes(4, 'little')
        head += (1).to_bytes(2, 'little') + (1).to_bytes(2, 'little') + SR.to_bytes(4, 'little') + (SR * 3).to_bytes(4, 'little')
        head += (3).to_bytes(2, 'little') + (24).to_bytes(2, 'little') + b'data' + len(b).to_bytes(4, 'little')
        path.write_bytes(head + b + (b'\0' if len(b) & 1 else b''))

    def stage(self, work, sfz, frames_of=lambda name: 9600, silent=lambda name: False):
        """The SFZ in work, beside its tone files; {sample name: frequency}."""
        text = sfz.read_text(encoding='utf-8')
        (work / 'Samples').mkdir(exist_ok=True)
        shutil.copy(sfz, work)
        names = sample_names(text)
        freqs = {n: 200.0 + 150.0 * i for i, n in enumerate(names)}
        for n in names:
            self.write_tone(work / 'Samples' / n, freqs[n], frames_of(n), silent(n))
        return text, freqs

    def synth(self, path):
        s = self.pysfizz.Synth(sample_rate=SR, block_size=BLOCK)
        self.assertTrue(s.load_sfz_file(str(path), quiet=True))
        return s

    def render(self, s, frames):
        np = self.np
        out = []
        for _ in range(frames // BLOCK):
            left, _right = s._synth.render_block()
            out.append(np.asarray(left, dtype=np.float64).copy())
        return np.concatenate(out) if out else np.zeros(0)

    def test_sfizz_plays_the_file_wubansia_would(self):
        np = self.np
        notes = 0
        for kit_id in KITS:
            kit = load_kit(kit_id)
            instruments = {i['id']: i for i in kit['instruments']}
            for sfz in sfz_files(kit_id):
                with tempfile.TemporaryDirectory() as tmp:
                    work = Path(tmp)
                    text, freqs = self.stage(work, sfz, frames_of=lambda n: 169412 if 'Rumble' in n else 9600)
                    s = self.synth(work / sfz.name)
                    self.assertEqual(s._synth.get_num_regions(), text.count('<region>'), sfz.name)
                    side = side_of(kit, sfz)
                    for e in kit['keymap']:
                        if e['ch'] != 10:
                            continue
                        for key in range(e['keys'][0], e['keys'][1] + 1):
                            inst = instruments[instrument_on(kit, key, side)]
                            if inst['mode'] != 'one_shot':
                                continue
                            zone = next(z for z in inst['zones'] if z['lo'] <= key <= z['hi'])
                            vels = sorted({1, 127} | {la['vel'] for la in zone['layers']} | {la['vel'] - 1 for la in zone['layers'] if la['vel'] > 1})
                            for vel in vels:
                                s = self.synth(work / sfz.name)
                                layer = zone['layers'][pick_layer(zone['layers'], vel)]
                                for hit in range(2 * len(layer['rr'])):
                                    file = kit['files'][layer['rr'][hit % len(layer['rr'])]]['path']
                                    want = f"{key} {note_name(key)} {file.rsplit('/', 1)[-1][:-4]} - {kit_id}.wav"
                                    s._synth.note_on(0, key, vel)
                                    x = self.render(s, 9600)[BLOCK:8192]
                                    self.render(s, 4800)
                                    spectrum = np.abs(np.fft.rfft(x * np.hanning(len(x))))
                                    f = np.argmax(spectrum) * SR / len(x)
                                    got = min(freqs, key=lambda n: abs(freqs[n] - f))
                                    notes += 1
                                    self.assertEqual(got, want, f'{sfz.name}: key {key}, velocity {vel}, hit {hit + 1}')
        self.assertGreater(notes, 1000)

    def test_fut01s_closed_hat_cuts_the_open_hat(self):
        np = self.np
        sfz = next(p for p in sfz_files('FUT01') if p.name.endswith('Kick Garage.sfz'))
        with tempfile.TemporaryDirectory() as tmp:
            work = Path(tmp)
            # The open hat a long tone; the closed hat silent, so what sounds after its note is what's left of the open one.
            self.stage(work, sfz, frames_of=lambda n: 48000, silent=lambda n: n.startswith('42 '))
            s = self.synth(work / sfz.name)
            s._synth.note_on(0, 46, 127)
            before = self.render(s, 4800)[BLOCK:]
            s._synth.note_on(0, 42, 127)
            after = self.render(s, 9600)
            level = np.sqrt(np.mean(before ** 2))
            rest = np.sqrt(np.mean(after[int(0.012 * SR):] ** 2))
            self.assertGreater(level, 1e-4)
            self.assertLessEqual(rest, level * 1e-3)  # 60 dB down from 12 ms

    def test_fut04s_rumble_loops_while_held_and_stops_after_release(self):
        np = self.np
        sfz = sfz_files('FUT04')[0]
        with tempfile.TemporaryDirectory() as tmp:
            work = Path(tmp)
            self.stage(work, sfz, frames_of=lambda n: 169412 if 'Rumble' in n else 9600)
            s = self.synth(work / sfz.name)
            s._synth.note_on(0, 35, 127)
            held = self.render(s, 169412 + 48000)  # a second past the file's end
            tail = held[-4800:]
            s._synth.note_off(0, 35, 0)
            released = self.render(s, 9600)
            self.assertGreater(np.sqrt(np.mean(tail ** 2)), 1e-4)  # still sounding: it loops
            self.assertLessEqual(np.sqrt(np.mean(released[int(0.020 * SR):] ** 2)), np.sqrt(np.mean(tail ** 2)) * 1e-3)


if __name__ == '__main__':
    unittest.main()
