"""FUT02's pad for round 2: the starfield pad, a wide Dm9 with a slow filter sweep over 4 bars.

Three layers on one chord, D3 A3 C4 F4 A4 E5 (root, 5, b7, b3, 5, 9):
- the body: three detuned saws a side per chord tone (the root: one steady saw in the middle with a quieter detuned
  pair, so the chord never loses its D), through a resonant low-pass that sweeps 600 Hz to 2.6 kHz and back once every
  4 bars at 130 BPM (7.38 s), the two sides a little apart, so the sweep moves across the field;
- the chrome: each tone's seven partials on the kit's stiff-wire ratios, stretched +2 to +31 cents, quieter, beating
  slowly against the saws' plain harmonics (the kit's shimmer);
- the stars: the chord two octaves up on the wire's partials 1, 3, 5 and 7, 24 sines up to 19 kHz, each twinkling at
  its own slow rate and placed across the stereo field.

The loop is exactly periodic, like FUT01's air pad: every oscillator sits on a grid of whole cycles per loop (4 bars),
the sweep and the twinkles repeat a whole number of times per loop, and the crossfaded loop points join cleanly. It is
high-passed at 100 Hz (the root is D3, 146.8 Hz) and mono under 150 Hz, the root included: its side is high-passed at
200 Hz (MONO_HZ), so the root's detuned pair, split left and right, and the two sides' sweeps don't widen it.
"""
import numpy as np

from tools.sound import dsp48 as D
from tools.sound import parts as P
from tools.sound import phrase as Ph
from tools.sound.item import Item, Out
from tools.sound.kits import fut02 as K

SR = D.SR
RATIOS = np.array(K.RATIOS)
CHORD = (K.ROOT + 12, K.ROOT + 19, K.ROOT + 22, K.ROOT + 27, K.ROOT + 31, K.ROOT + 38)   # D3 A3 C4 F4 A4 E5
LOOP_STEPS = 64              # 4 bars of 16ths: one sweep per loop
ATTACK_S = 1.5               # the attack before the loop starts
FADE_IN_S = 1.1
DETUNE_CENTS = 7.0
ROOT_PAIR = 0.4              # the root's detuned pair against its steady centre saw: in mono the centre is 2.0 and the
                             # pair 0.8 at most, so the root dips 7.4 dB at worst instead of cancelling (a full stack
                             # of three random-phase saws a side lost D3 by up to 27 dB every few seconds)
ROOT_NORM = 1.9              # the root's sum over this: about the level of a three-saw stack (3 random phases, / 3)
SWEEP = (600.0, 2600.0, 1.3, 0.6)   # the low-pass: lowest and highest cutoff (Hz), q, the sides' phase offset (rad)
MIX = (1.0, 0.22, 0.10)      # body, chrome, stars
STAR_PARTIALS = (0, 2, 4, 6)  # indices into RATIOS: partials 1, 3, 5, 7
STAR_RATES = (2, 3, 4, 5, 6, 7)   # twinkles per loop
MONO_HZ = 200.0              # the side is high-passed here (8th order), so the root D3 (146.8 Hz) is mono too: 21 dB
                             # down on the side; at 150 Hz, just over the root, it was only 3.8 dB down


def sweep_lp(x, L, phase):
    """The body's low-pass, one channel: a TPT SVF whose cutoff rises from SWEEP's low to its high and back once per
    loop of L samples (log frequency, raised cosine), starting at `phase` radians into the cycle."""
    lo, hi, q, _ = SWEEP
    u = 0.5 - 0.5 * np.cos(2 * np.pi * np.arange(len(x)) / L + phase)
    return D.svf_tpt(x, lo * (hi / lo) ** u, q=q)[0]


def starfield_pad(take):
    L = Ph.loop_len(K, LOOP_STEPS)
    grid = SR / L
    A0 = int(ATTACK_S * SR)
    n = A0 + int(0.05 * SR) + L
    t = np.arange(n)
    r = take.rng('voices')
    on_grid = lambda f: round(f / grid) * grid           # at most 0.07 Hz off: under 1 cent at D3
    f0s, body = [], np.zeros((n, 2))
    for i, note in enumerate(CHORD):
        f = on_grid(float(D.midi_hz(note)))
        s = max(1, round(f * (2 ** (DETUNE_CENTS / 1200) - 1) / grid))
        if i == 0:   # the root: one steady saw on both sides, a quieter detuned pair beside it
            steps = {'L': ((-s, ROOT_PAIR), (0, 1.0)), 'R': ((0, 1.0), (s, ROOT_PAIR))}
        else:
            steps = {'L': ((-s, 1.0), (0, 1.0), (s // 2, 1.0)), 'R': ((-(s // 2), 1.0), (0, 1.0), (s, 1.0))}
        norm = ROOT_NORM if i == 0 else 3.0
        voice = {d: D.down(D.polyblep_saw(f + d * grid, n, r.uniform(), k=4), 4)
                 for d in sorted({d for side in steps.values() for d, _ in side})}
        f0s += [f + d * grid for d in voice]
        for c, side in enumerate('LR'):
            body[:, c] += sum(g * voice[d] for d, g in steps[side]) / norm
    off = SWEEP[3]
    body = np.stack([sweep_lp(body[:, 0], L, -off), sweep_lp(body[:, 1], L, off)], 1)
    body = D.peaking(body, 280.0, -4.0, 0.8) / len(CHORD)

    # the chrome: every tone's partials on the wire ratios, each on the grid, the same on both sides
    chrome = np.zeros(n)
    for note in CHORD:
        for k, rt in enumerate(RATIOS):
            f = on_grid(float(D.midi_hz(note)) * rt)
            if f < 18000:
                chrome += np.sin(2 * np.pi * f * t / SR + r.uniform(0, 2 * np.pi)) / (1 + k) ** 1.3
                f0s.append(f)
    chrome = D.bfilt(chrome, 4000.0, 'lowpass', order=2) / len(CHORD)

    # the stars: the chord two octaves up on the wire's odd partials, twinkling, each at its own place in the field
    stars = np.zeros((n, 2))
    rs = take.rng('stars')
    for i, note in enumerate(CHORD):
        for j, k in enumerate(STAR_PARTIALS):
            f = on_grid(float(D.midi_hz(note + 24)) * RATIOS[k])
            if f > 19000:
                continue
            rate = STAR_RATES[(i + 2 * j) % len(STAR_RATES)]
            tw = (0.5 + 0.5 * np.sin(2 * np.pi * rate * t / L + rs.uniform(0, 2 * np.pi))) ** 2
            x = tw * np.sin(2 * np.pi * f * t / SR + rs.uniform(0, 2 * np.pi)) / (1 + 0.5 * j)
            stars += Ph.pan(x, rs.uniform(-0.9, 0.9))
            f0s.append(f)
    stars /= len(CHORD)

    g_body, g_chrome, g_stars = MIX
    y = g_body * body + g_chrome * D.st(chrome) + g_stars * stars
    y = D.bfilt(y, 100.0, 'highpass', order=4)
    y = D.mono_below(y, MONO_HZ) * (np.sin(0.5 * np.pi * np.clip(P.times(n) / FADE_IN_S, 0, 1)) ** 2)[:, None]
    y, loop = P.close_loop(y, A0, L, xfade=0.05)
    return Out(y, loop=loop, f0s=tuple(sorted(f0s)))


ITEMS = [
    Item('Pad', 'StarfieldPadDm9', 'pad', starfield_pad, root=CHORD[0], loop=True, sets=('r2',),
         about='starfield pad, Dm9 (D3 A3 C4 F4 A4 E5): detuned saws under a 4-bar filter sweep, wire-ratio chrome and '
               'twinkling stars; high-passed at 100 Hz, mono under 150 Hz and at its root D3; a 4-bar loop'),
]
