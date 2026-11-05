"""FUT04's drone for rounds 2 and 3: a dark Em(add9) bed with an iron hum and a slow, resonant filter over 4 bars.

One chord, E3 B3 E4 G4 F#5 (root, 5, octave, b3, 9: the 9 sits a major seventh over the third, dark but open), in
two layers:
- the body: detuned saws per chord tone (the root: one steady saw in the middle with a quieter detuned pair, so the
  chord never loses its E), through a resonant low-pass that rises from 450 Hz to 2 kHz and back once every 4 bars
  at 136 BPM (7.06 s), the two sides a little apart, and a broad dip at 230 Hz (round 2: 8 dB, so the bed stayed out of
  the low mids; round 3: 3 dB, so its root and fifth fill the kit's low-mid hole, the deepest of the four kits: the
  modern-sound standard's FUT04 change 3), kept in the middle up to 450 Hz, where a wide low mid would smear;
- the iron: the kit's anvil modes (RATIOS) on E4, seven sines breathing at their own slow rates (1 to 4 times per
  loop), so the slab's beating pairs (1.5719 and 1.6124 near C5, 3.2247 and 3.3671) hum through the chord like a
  bowed plate in the furnace.

The loop is exactly periodic: every oscillator sits on a grid of whole cycles per loop (4 bars), the sweep and the
breathing repeat a whole number of times per loop, and the crossfaded loop points join cleanly. It fades in over
1.1 s and loops from its attack's end. High-passed at 100 Hz (the root is E3, 164.8 Hz; 4th order) and mono under
450 Hz (round 2: 150). No noise and no shaper: every component is a declared fundamental (f0s), so the alias check
reads it whole.
"""
import numpy as np

from tools.sound import dsp48 as D
from tools.sound import parts as P
from tools.sound import phrase as Ph
from tools.sound.item import Item, Out
from tools.sound.kits import fut04 as K

SR = D.SR
RATIOS = np.array(K.RATIOS)
CHORD = (K.ROOT + 12, K.ROOT + 19, K.ROOT + 24, K.ROOT + 27, K.ROOT + 38)   # E3 B3 E4 G4 F#5
IRON_NOTE = K.ROOT + 24      # E4: the anvil modes' fundamental in the hum (330-1388 Hz)
LOOP_STEPS = 64              # 4 bars of 16ths: one sweep per loop
ATTACK_S = 1.5               # the attack before the loop starts
FADE_IN_S = 1.1
DETUNE_CENTS = 6.0
ROOT_PAIR = 0.3              # the root's detuned pair against its steady centre saw (it never cancels; at 0.4 its
                             # beating pulled the tuning read in the fade-in to -2.1 cents, at 0.3 -1.7)
ROOT_NORM = 1.75             # the root's sum over this: about the level of a three-saw stack
SWEEP = (450.0, 2000.0, 1.8, 0.5)   # the low-pass: lowest and highest cutoff (Hz), q, the sides' phase offset (rad)
IRON_AMPS = (1.0, 0.6, 0.6, 0.35, 0.25, 0.25, 0.15)   # each pair at one level, so it beats
IRON_RATES = (1, 2, 3, 2, 4, 3, 4)   # breaths per loop, per mode
MIX = (1.0, 0.20)            # body, iron
LOW_MID_DIP = (230.0, 0.0, 0.6)    # the body's low-mid dip: centre (Hz), gain (dB), q (round 2: -8 dB; round 3: none)
MONO_HZ = 450.0              # mono under this (round 2: 150 Hz): its low mids stay in the middle (the whole 250 Hz
                             # octave, 177-354 Hz)


def sweep_lp(x, L, phase):
    """The body's low-pass, one channel: a TPT SVF whose cutoff rises from SWEEP's low to its high and back once per
    loop of L samples (log frequency, raised cosine), starting at `phase` radians into the cycle."""
    lo, hi, q, _ = SWEEP
    u = 0.5 - 0.5 * np.cos(2 * np.pi * np.arange(len(x)) / L + phase)
    return D.svf_tpt(x, lo * (hi / lo) ** u, q=q)[0]


def drone(take):
    L = Ph.loop_len(K, LOOP_STEPS)
    grid = SR / L
    A0 = int(ATTACK_S * SR)
    n = A0 + int(0.05 * SR) + L
    t = np.arange(n)
    r = take.rng('voices')
    on_grid = lambda f: round(f / grid) * grid           # at most 0.07 Hz off: under 0.8 cents at E3
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
    body = D.peaking(body, *LOW_MID_DIP) / len(CHORD)

    # the iron: the anvil's modes on E4, each on the grid and breathing at its own rate, the same on both sides
    iron = np.zeros(n)
    ri = take.rng('iron')
    for rt, a, rate in zip(RATIOS, IRON_AMPS, IRON_RATES):
        f = on_grid(float(D.midi_hz(IRON_NOTE)) * rt)
        breath = (0.55 + 0.45 * np.sin(2 * np.pi * rate * t / L + ri.uniform(0, 2 * np.pi))) ** 2
        iron += a * breath * np.sin(2 * np.pi * f * t / SR + ri.uniform(0, 2 * np.pi))
        f0s.append(f)
    iron /= sum(IRON_AMPS)

    g_body, g_iron = MIX
    y = g_body * body + g_iron * D.st(iron)
    y = D.bfilt(y, 100.0, 'highpass', order=4)
    y = D.mono_below(y, MONO_HZ) * (np.sin(0.5 * np.pi * np.clip(P.times(n) / FADE_IN_S, 0, 1)) ** 2)[:, None]
    y, loop = P.close_loop(y, A0, L, xfade=0.05)
    return Out(y, loop=loop, f0s=tuple(sorted(f0s)))


ITEMS = [
    Item('Pad', 'DroneEmAdd9', 'pad', drone, root=CHORD[0], loop=True, sets=('r2', 'r3'),
         about='dark drone, Em(add9) (E3 B3 E4 G4 F#5): detuned saws under a resonant 4-bar filter rise, the anvil '
               'modes humming on E4; high-passed at 100 Hz, mono under 450 Hz; a 4-bar loop'),
]
