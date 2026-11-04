"""FUT04 effects for round 2: the one-bar stutter fill and the 8-bar iron riser, both at 136 BPM on the kit's swing,
pitched to E minor and end-aligned: each file ends on the next downbeat, loud (cut_end), so the drop lands where it
stops.

- StutterFill: one bar of metallic noise: the kit's anvil modes (RATIOS) on a pitch, each a narrow band of noise (a sine
  whose frequency wanders 6 Hz at random, so the fundamental always leads and the note reads), over a little 2.5-9 kHz
  hiss. It is cut into slices that halve every beat and repeat the same grain (the stutter): two 8ths on E4, four 16ths
  on G4 G4 A4 G4, eight 32nds on A4 then B4, sixteen 64ths climbing B4, D5 and E5, so the line goes up the E minor
  pentatonic (E G A B D) and lands on E. Each slice is a hit (a 0.5 ms attack, a decay inside the slice) with 1 ms
  edges, logged as splices. The level rises 10 dB over the bar, the high-pass opens from 150 to 450 Hz (under the notes)
  and the pan swings a little wider on every halving.
- IronRiser: 8 bars. White noise through the anvil modes, a choir of resonances that climbs the E minor scale
  one degree a bar (E3 F#3 G3 A3 B3 C4 D4 E4, a 40 ms glide between), ringing longer as it climbs; over it a band of
  air sweeps from 400 Hz to 7 kHz; in the last bar the whole riser pulses in 16ths on the swing. It rises 15 dB, the
  high-pass opens from 150 to 450 Hz, and it ends on the downbeat.

Both are high-passed far above the kick and sub and mono under 150 Hz.
"""
import numpy as np
from scipy import signal

from tools.sound import dsp48 as D
from tools.sound import parts as P
from tools.sound import phrase as Ph
from tools.sound.item import Item, Out
from tools.sound.kits import fut04 as K

SR = D.SR
RATIOS = np.array(K.RATIOS)
S16 = K.STEP * SR             # samples per 16th (not an integer at 136 BPM)
E4, G4, A4, B4, D5, E5 = 64, 67, 69, 71, 74, 76
MODE_AMPS = (1.0, 0.7, 0.7, 0.5, 0.4, 0.4, 0.3)


def swung(s):
    """A position in 16ths (fractional: 32nds, 64ths) as a sample index on the kit's swing: the first 16th of each 8th
    spans 2 x K.SWING of it and the second the rest, so a slice inside the swung 16th shrinks with it and every odd 16th
    lands where phrase.Grid puts it."""
    base = np.floor(s / 2) * 2
    f = s - base
    u = f * K.SWING if f <= 1 else K.SWING + (f - 1) * (1 - K.SWING)
    return int(np.floor((base + 2 * u) * S16 + 0.5))


def metal_noise(x, f0, bw, amps=MODE_AMPS):
    """Noise rung through the anvil's modes on f0: a 2-pole resonator per mode (bandwidth bw Hz, about unit peak
    gain) at the levels `amps`, every mode under 18 kHz. Linear and time-invariant: one pitch per call."""
    y = np.zeros_like(x)
    for rt, a in zip(RATIOS, amps):
        f = f0 * rt
        if f >= 18000:
            continue
        rad = np.exp(-np.pi * bw / SR)
        w = 2 * np.pi * f / SR
        y += a * signal.lfilter([1 - rad], [1, -2 * rad * np.cos(w), rad * rad], x, axis=0)
    return y


def metal_grain(r, n, f0, wander, amps):
    """Metal noise with an exact balance: each of the anvil's modes on f0 as a sine at its level in `amps`, its
    frequency wandering at random (low-passed noise, `wander` Hz RMS, under 1.5 x wander Hz), so every mode is a narrow
    band of noise with a flat envelope. Noise rung through narrow resonators instead fades in and out at random: over
    one slice, a fundamental landed up to 7 dB under the twist pair, and the note went with it."""
    t = np.arange(n) / SR
    y = np.zeros(n)
    for rt, a in zip(RATIOS, amps):
        if f0 * rt >= 18000:
            continue
        df = D.bfilt(r.standard_normal(n), 1.5 * wander, 'lowpass', order=2)
        df *= wander / (np.std(df) + 1e-12)
        y += a * np.sin(2 * np.pi * (f0 * rt * t + np.cumsum(df) / SR) + r.uniform(0, 2 * np.pi))
    return y


def edge(x, ms=1.0):
    """A slice with a raised-cosine fade at both ends, so no slice edge clicks."""
    return D.fade(x, ms / 1000, ms / 1000)


# ------------------------------------------------------------------ the stutter fill
# one row per slice group: (first 16th, slice length in 16ths, number of slices, notes (one per slice, or cycled),
# dB: the shorter slices decay further inside themselves, so they get a little more level)
FILL = ((0.0, 2.0, 2, (E4,), 0.0),
        (4.0, 1.0, 4, (G4, G4, A4, G4), 0.0),
        (8.0, 0.5, 8, (A4, A4, A4, A4, B4, B4, B4, B4), 1.0),
        (12.0, 0.25, 16, (B4, B4, B4, B4, D5, D5, D5, D5, E5, E5, E5, E5, E5, E5, E5, E5), 3.0))
FILL_MODES = (1.0, 0.45, 0.45, 0.30, 0.20, 0.20, 0.15)   # the fundamental leads, so each slice says its note
FILL_RISE_DB = 10.0
FILL_HP = (150.0, 450.0)     # the high-pass opens over the bar, staying under the notes (B4 is 494 Hz)
FILL_WANDER = 6.0            # each mode's random frequency wander, Hz RMS: noise, but narrow enough that even a 64th
                             # keeps its pitch
FILL_HISS = 0.25             # a band of 2.5-9 kHz noise in every grain, against the modes (unit RMS)


def stutter_fill(take):
    n = swung(16)
    out = np.zeros((n, 2))
    cuts = []
    grains = {}
    for g, (s0, length, count, notes, gain) in enumerate(FILL):
        for k in range(count):
            note = notes[k % len(notes)]
            a, b = swung(s0 + k * length), swung(s0 + (k + 1) * length)
            key = (note, length)
            if key not in grains:   # one grain per pitch and slice length, the same noise: every repeat is the same
                m = swung(s0 + length) - swung(s0)              # the group's first slice, its longest
                x = metal_grain(take.rng('grain', note), m, float(D.midi_hz(note)), FILL_WANDER, FILL_MODES)
                x = x + FILL_HISS * P.band_noise(take.rng('hiss', note), m, 2500.0, 9000.0)
                grains[key] = x / (np.sqrt(np.mean(x ** 2)) + 1e-12)
            hit = grains[key][:b - a] * D.env_exp(b - a, max(0.012, 0.45 * (b - a) / SR), 0.0005)
            hit = edge(hit) * D.undb(gain - FILL_RISE_DB * (1 - a / n))
            p = (0.15 + 0.12 * g) * (-1) ** k
            out[a:b] += Ph.pan(hit, p)
            cuts += [a, b]
    hp = FILL_HP[0] * (FILL_HP[1] / FILL_HP[0]) ** (np.arange(n) / n)
    y = np.stack([D.svf_tpt(out[:, c], hp, q=0.6)[2] for c in range(2)], 1)
    y = D.bfilt(y, 150.0, 'highpass', order=4)
    return Out(D.mono_below(y, 150.0), splices=tuple(sorted({c for c in cuts if 0 < c < n})),
               meta={'bpm': K.BPM, 'swing': K.SWING, 'notes': [D.note_name(m) for m in (E4, G4, A4, B4, D5, E5)]})


# ------------------------------------------------------------------ the iron riser
RISER_BARS = 8
RISER_N = swung(16 * RISER_BARS)
RISER_SCALE = tuple(K.ROOT + 12 + d for d in (0, 2, 3, 5, 7, 8, 10, 12))   # E3 F#3 G3 A3 B3 C4 D4 E4, one a bar
RISER_GLIDE = 0.040          # seconds between degrees
RISER_BW = (16.0, 7.0)       # the choir's bandwidth, Hz, first bar to last: it rings longer as it climbs
AIR = (400.0, 7000.0, 1.2)   # the air band: from, to (Hz), q
RISER_MIX = (1.0, 0.55)      # choir, air (both at unit RMS before the rise)
RISER_RISE_DB = 15.0
RISER_CURVE = 1.0            # the rise in dB against time: a straight line
RISER_SPREAD = (0.8, 0.6)    # the right side's noise: this much of the left's plus its own (correlation 0.8)
RISER_DENSITY = 3.0          # a tanh at 4x, half and half with the clean signal: the peaks closer to the loudness
RISER_HP = (150.0, 450.0)


def iron_riser(take):
    n = RISER_N
    u = np.arange(n) / n
    r = take.rng('air')
    bar = swung(16)

    # the pitch: one degree of the scale a bar, a short glide into each
    f = np.zeros(n)
    for i, note in enumerate(RISER_SCALE):
        f[i * bar:] = float(D.midi_hz(note))
    g = int(RISER_GLIDE * SR)
    f = np.convolve(np.concatenate([np.full(g, f[0]), f]), np.ones(g) / g, mode='valid')[:n]
    bw = RISER_BW[0] * (RISER_BW[1] / RISER_BW[0]) ** u

    # the choir: noise through the modes, per side, each mode a band-pass whose Q follows its pitch over the bandwidth
    x = r.standard_normal(n)
    x = np.stack([x, RISER_SPREAD[0] * x + RISER_SPREAD[1] * take.rng('air', 'side').standard_normal(n)], 1)
    choir = np.zeros((n, 2))
    for rt, a in zip(RATIOS, MODE_AMPS):
        fk = f * rt
        q = np.clip(fk / bw, 0.5, 400.0)
        for c in range(2):
            choir[:, c] += a * _svf_bp(x[:, c], fk, q)
    choir /= np.sqrt(np.mean(choir ** 2)) + 1e-12

    # the air: the same noise through one band sweeping up
    fa = AIR[0] * (AIR[1] / AIR[0]) ** (u ** 1.5)
    air = np.stack([D.svf_tpt(x[:, c], fa, q=AIR[2])[1] for c in range(2)], 1)
    air /= np.sqrt(np.mean(air ** 2)) + 1e-12

    y = RISER_MIX[0] * choir + RISER_MIX[1] * air
    # the last bar pulses in 16ths on the swing: each 16th dips to -12 dB at its end and comes back on the next
    pulse = np.ones(n)
    for k in range(16):
        a, b = swung(16 * (RISER_BARS - 1) + k), swung(16 * (RISER_BARS - 1) + k + 1)
        w = np.linspace(0.0, 1.0, b - a)
        pulse[a:b] = D.undb(-12.0 * w ** 2)
        pulse[a:a + int(0.002 * SR)] = np.minimum(pulse[a:a + int(0.002 * SR)],
                                                  np.linspace(D.undb(-12.0), 1.0, int(0.002 * SR)))
    y *= pulse[:, None]
    y = y / np.abs(y).max()
    y = 0.5 * y + 0.5 * D.oversampled(lambda w: D.tanh_adaa(w, RISER_DENSITY), y)
    hp = RISER_HP[0] * (RISER_HP[1] / RISER_HP[0]) ** u
    y = np.stack([D.svf_tpt(y[:, c], hp, q=0.6)[2] for c in range(2)], 1)
    y *= D.undb(-RISER_RISE_DB + RISER_RISE_DB * u ** RISER_CURVE)[:, None]
    return D.mono_below(D.bfilt(y, 150.0, 'highpass', order=4), 150.0)


def _svf_bp(x, fc, q):
    """A TPT SVF band-pass (Zavalishin, as dsp48.svf_tpt) with a per-sample cutoff and Q, at unit peak gain."""
    n = len(x)
    g = np.tan(np.pi * np.clip(fc, 10, SR * 0.49) / SR)
    k = 1.0 / q
    a1 = 1.0 / (1.0 + g * (g + k))
    out = np.empty(n)
    s1 = s2 = 0.0
    for i in range(n):
        h = (x[i] - (k[i] + g[i]) * s1 - s2) * a1[i]
        v1 = g[i] * h
        b = v1 + s1
        s1 = b + v1
        v2 = g[i] * b
        lo = v2 + s2
        s2 = lo + v2
        out[i] = b * k[i]
    return out

ITEMS = [
    Item('FX', 'StutterFill', 'fx', stutter_fill, root=E4, sets=('r2',), flags={'cut_end'}, tags=('fill',),
         qa={'why': 'it ends on the next downbeat: the last 64th roll on E5 runs into the drop'},
         about='one-bar stutter fill at 136 BPM: metallic noise on the anvil modes in slices that halve every beat, '
               'climbing E4 G4 A4 B4 D5 E5 and landing on the downbeat'),
    Item('FX', 'IronRiser', 'fx', iron_riser, level=-14.0, sets=('r2',), flags={'cut_end'}, tags=('riser',),
         qa={'why': 'end-aligned: it stays loud to the downbeat, where the next section takes over'},
         about='8-bar iron riser at 136 BPM: noise through the anvil modes climbing the E minor scale a degree a bar, '
               'an air band sweeping up, 16th pulses in the last bar; it ends on the downbeat'),
]
