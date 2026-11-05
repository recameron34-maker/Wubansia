"""The sampler's test vectors (P5): what the recipes' own functions give on small synthetic inputs, as float32 files
the engine's C++ tests read (engine/tests/test_sinc.cpp, test_selection.cpp, test_voice.cpp). CI has no samples,
so the inputs are made here, and stored with the outputs: the C++ reads the same floats the recipes read.

    python3 tools/sound/export_sampler_vectors.py            # writes engine/tests/vectors/sampler/
    python3 tools/sound/export_sampler_vectors.py --check    # exits 1 if a committed file differs

Every file is raw little-endian float32, no header; its layout is below and in the C++ tests. vectors.sha256 lists
each file's SHA-256 (sha256sum's format), which the C++ tests check before they trust a file.

- source_a.f32, source_b.f32: two mono test sources, SOURCE_FRAMES frames each, every value a 24-bit step (k / 2^23,
  so float32 holds it exactly, as it holds a 24-bit WAV): a few partials and a little noise, the loop seams not
  smoothed (the loops wrap anyway). Their loops are LOOP_A and LOOP_B.
- hermite_up4.f32: fut01/basslines.hermite on source_a, LOOP_A, read 4 semitones up: positions 0, r, 2r... summed
  one step at a time (bass_voice's cumsum), r = 2 ** (4 / 12). HERMITE_UP4_N values.
- hermite_down.f32: the same at r = 0.5 * 2 ** (-3 / 12) (a 96 kHz host, 3 semitones down): reads between frames
  and the recipe's edge (index -1 reads frame 0). HERMITE_DOWN_N values.
- hermite_sweep.f32: source_b, LOOP_B, the pitch rising linearly from 0 to 7 semitones over HERMITE_SWEEP_N samples
  (semitone j = 7 j / (N - 1)), positions the running sum of 2 ** (semitone / 12). HERMITE_SWEEP_N values.
- glide_dip.f32, glide_slide.f32: fut01/context4.bass_voice, FUT01's warp bass voice: Hermite, a 1 ms raised-cosine
  attack, a 30 ms raised-cosine release, the first note's zone and layer for the whole voice, and each legato note's
  pitch (in semitones) and gain gliding on a raised cosine, 30 ms for a dip and 70 ms for a slide (GLIDES). The notes
  are GLIDE_DIP and GLIDE_SLIDE, with zones basslines.ZONES and two layers. Velocities are MIDI's (v / 127) and the
  layers the manifest's (round(layer x 127) / 127), so the recipe's (velocity / layer) ^ 1.5 is the engine's gain to
  the last bit: the engine only ever sees those integers. Each holds n_on + 30 ms of samples (bass_voice's length).
- envelopes.f32: parts.raised (100 samples, 1 ms: the attack), dsp48.taper (1500 samples from 0, 30 ms: the
  release), and the FUT02-04 choke's gain from its closed hat on (fut02/context.hat_lane: cos^2 over 4 ms, then 0;
  300 samples), one after the other.
- layers.f32: every multi-layer velocity set of the four kits' items, sorted: the count of sets, then per set its
  layer count n, its n layer velocities round(layer x 127), the layer (0-based) the engine's rule picks for each
  velocity 1-127 (the nearest layer velocity, the louder on a tie) and the one the recipes' rule picks
  (phrase.hits_lane: the nearest layer to velocity / 127, the louder on a tie, in floating point).
"""
import argparse
import functools
import hashlib
import importlib
import math
import sys
from pathlib import Path

if __package__ in (None, ''):  # run as a script: import the tools.sound package from the repo root
    sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

import tools.sound  # noqa: E402,F401  first: it pins numpy's SIMD level before numpy loads (ADR 0007)

import numpy as np  # noqa: E402

from tools.sound import dsp48 as D  # noqa: E402
from tools.sound import kits  # noqa: E402
from tools.sound import parts as P  # noqa: E402
from tools.sound.item import Out  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'engine' / 'tests' / 'vectors' / 'sampler'
SR = D.SR
VOICE_KIT = 'FUT01'                          # the kit whose recipes define the sampler voice: basslines, context4


@functools.lru_cache(maxsize=None)
def recipe(module):
    """VOICE_KIT's recipe module, read through the kit's RECIPES (shared code never imports a kit, ADR 0008)."""
    return importlib.import_module(f'{kits.module(VOICE_KIT).RECIPES}.{module}')

SOURCE_FRAMES = 4096
LOOP_A = (1024, 3584)
LOOP_B = (512, 3000)
HERMITE_UP4_N = 3000
HERMITE_DOWN_N = 2000
HERMITE_SWEEP_N = 3000
LAYER_VELS = (76, 127)                       # round(0.6 x 127), round(1.0 x 127): FUT01's reese layers (basslines.LAYERS)
V89 = 89 / 127                               # round(0.7 x 127): a plain note (basslines.NOTE)
V127 = 127 / 127
# (offset from the voice's onset, MIDI note, velocity, glide): F#2 dips to C#2 and back (the warp line's dip pair,
# context4.WARP_LINE steps 5-7, at one velocity), held to N_ON; A1 slides up to C#2 and accents (gain glides). The
# glide is context4.GLIDES' key: 'dip' (30 ms) or 'slide' (70 ms).
GLIDE_DIP = ((0, 42, V89, None), (3600, 37, V89, 'dip'), (5200, 42, V89, 'dip'))
GLIDE_DIP_N_ON = 7000
GLIDE_SLIDE = ((0, 33, V89, None), (2000, 37, V127, 'slide'))
GLIDE_SLIDE_N_ON = 6000
CHOKE_S = 0.004                              # FUT02-04's choke (fut02/drums.CHOKE['fade_s'])


def source(seed, partials):
    """A mono test source, SOURCE_FRAMES frames of 24-bit steps."""
    t = np.arange(SOURCE_FRAMES) / SR
    x = sum(a * np.sin(2 * np.pi * f * t + ph) for f, a, ph in partials)
    x = x + 0.02 * np.random.default_rng(seed).standard_normal(SOURCE_FRAMES)
    return np.round(x * 2.0 ** 23) / 2.0 ** 23


def sources():
    a = source(1, ((61.7, 0.45, 0.3), (185.0, 0.2, 1.1), (1250.0, 0.08, 2.0), (7300.0, 0.03, 0.5)))
    b = source(2, ((55.0, 0.4, 0.0), (110.0, 0.25, 0.7), (880.0, 0.1, 1.9), (11000.0, 0.02, 2.6)))
    return a, b


def running(ratios):
    """Positions as bass_voice makes them: 0, then the running sum of the ratios, one step at a time."""
    return np.concatenate([[0.0], np.cumsum(ratios[:-1])])


def hermite_vectors(a, b):
    B = recipe('basslines')
    up = 2.0 ** (4 / 12)
    down = 0.5 * 2.0 ** (-3 / 12)
    semis = 7.0 * np.arange(HERMITE_SWEEP_N) / (HERMITE_SWEEP_N - 1)
    return {
        'hermite_up4': B.hermite(a[:, None], running(np.full(HERMITE_UP4_N, up)), LOOP_A)[:, 0],
        'hermite_down': B.hermite(a[:, None], running(np.full(HERMITE_DOWN_N, down)), LOOP_A)[:, 0],
        'hermite_sweep': B.hermite(b[:, None], running(2.0 ** (semis / 12.0)), LOOP_B)[:, 0],
    }


def glide_vectors(a, b):
    """context4.bass_voice on the two sources, with the manifest's integer layers (see the module's note)."""
    B, C4 = recipe('basslines'), recipe('context4')
    files = {(zone, layer): Out(a[:, None], loop=LOOP_A) for zone in B.ZONES for layer in (1, 2)}
    files[(33, 1)] = Out(b[:, None], loop=LOOP_B)
    notes = lambda run: [(off, note, vel, C4.GLIDES[g] if g else 0.0) for off, note, vel, g in run]
    saved = B.LAYERS
    B.LAYERS = tuple(v / 127 for v in LAYER_VELS)
    try:
        dip = C4.bass_voice(files, notes(GLIDE_DIP), GLIDE_DIP_N_ON)[:, 0]
        slide = C4.bass_voice(files, notes(GLIDE_SLIDE), GLIDE_SLIDE_N_ON)[:, 0]
    finally:
        B.LAYERS = saved
    return {'glide_dip': dip, 'glide_slide': slide}


def envelope_vector():
    B = recipe('basslines')
    attack = P.raised(100, B.ATTACK)
    release = D.taper(1500, 0.0, B.RELEASE)
    n = int(CHOKE_S * SR)
    choke = np.ones(300)                     # fut02/context.hat_lane, its closed hat at sample 0 of the open hat
    choke[0:n] = np.cos(0.5 * np.pi * np.arange(n) / n) ** 2
    choke[n:] = 0.0
    return np.concatenate([attack, release, choke])


def layer_sets():
    """Every multi-layer velocity set of the kits' items."""
    found = set()
    for kit in kits.names():
        for it in kits.items(kits.module(kit)):
            if len(it.layers) > 1:
                found.add(tuple(it.layers))
    return sorted(found)


def layer_vector():
    sets = layer_sets()
    out = [float(len(sets))]
    for layers in sets:
        vels = [int(math.floor(l * 127 + 0.5)) for l in layers]
        engine = [min(range(len(vels)), key=lambda i: (abs(vels[i] - v), -i)) for v in range(1, 128)]
        recipe = [min(range(len(layers)), key=lambda i: (abs(layers[i] - v / 127), -i)) for v in range(1, 128)]
        out += [float(len(vels))] + [float(v) for v in vels] + [float(p) for p in engine] + [float(p) for p in recipe]
    return np.array(out)


def vectors():
    a, b = sources()
    out = {'source_a': a, 'source_b': b}
    out.update(hermite_vectors(a, b))
    out.update(glide_vectors(a, b))
    out['envelopes'] = envelope_vector()
    out['layers'] = layer_vector()
    return {f'{name}.f32': np.asarray(x, dtype='<f4').tobytes() for name, x in sorted(out.items())}


def manifest(files):
    return ''.join(f'{hashlib.sha256(data).hexdigest()}  {name}\n' for name, data in sorted(files.items()))


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    ap.add_argument('--check', action='store_true', help='compare with the committed files instead of writing')
    args = ap.parse_args(argv)
    files = vectors()
    files_and_sums = dict(files)
    files_and_sums['vectors.sha256'] = manifest(files).encode()
    if args.check:
        bad = [name for name, data in sorted(files_and_sums.items())
               if not (OUT / name).is_file() or (OUT / name).read_bytes() != data]
        extra = sorted(p.name for p in OUT.glob('*.f32') if p.name not in files) if OUT.is_dir() else []
        for name in bad:
            print(f'{name}: differs from what the recipes give now (re-export it)', file=sys.stderr)
        for name in extra:
            print(f'{name}: committed, but no longer exported', file=sys.stderr)
        return 1 if bad or extra else 0
    OUT.mkdir(parents=True, exist_ok=True)
    for name, data in files_and_sums.items():
        (OUT / name).write_bytes(data)
    total = sum(len(d) for d in files.values())
    print(f'wrote {len(files)} vectors ({total} bytes) and vectors.sha256 to {OUT.relative_to(ROOT)}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
