"""FUT01 effects for the first listen: a one-beat glitch fill and an 8-bar frequency-shifted barber-pole riser.

Both are built from kit-like material made here on the kit DNA (alloy-ratio strikes on F#, a metal snare, data ticks,
a laser zap), not from other instruments' files.

- GlitchFill: one beat at 134 BPM, one 16th at a time: the hit; a 32nd stutter, its repeat shifted 35 Hz down; four
  64ths of the strike, repitched up and crushed harder each time (4x oversampled sample-and-hold, so the aliasing is
  the planned kind); eight 128ths of tick and zap whose frequency shift climbs to +900 Hz, the last left to ring.
  Every slice edge has a 1 ms raised-cosine fade and is logged as a splice.
- ShiftRiser: 8 bars, end-aligned: the file ends on the downbeat and stays loud to the end (cut_end; the renderer's
  5 ms fade is the only edge). An alloy chord on F#3 and F#4 plus air goes through four frequency shifters whose shift
  ramps 0-600 Hz and wraps, each faded out at its wrap (sin^2 windows a quarter apart sum to a constant): a barber pole
  whose partials glide up for ever. The pole speeds up, a common shift climbs to +1.5 kHz in the last bars, the
  band opens, a little saturation thickens it and the level rises 24 dB.

Both are high-passed well above the kick and sub and mono under 150 Hz.
"""
import numpy as np

from tools.sound import dsp48 as D
from tools.sound.item import Item, Out
from tools.sound.kits import fut01 as K

SR = D.SR
ALLOY = np.array(K.ALLOY)
F_ALLOY = float(D.midi_hz(K.ROOT + 31))   # C#5, the fifth: the strike in the hit (the ping is a fourth up, F#5)
S16 = K.STEP * SR                         # samples per 16th (not an integer at 134 BPM)


def at(steps):
    """A position in 16ths as a sample index."""
    return int(np.floor(steps * S16 + 0.5))


# ------------------------------------------------------------------ kit-like material
def strike(f0, n, r, bright=1.0):
    """A struck alloy plate: modes on the kit's ratios, the upper ones shorter, a 1 ms click of filtered noise."""
    decays = [0.22 / (1 + 0.9 * k) for k in range(len(ALLOY))]
    amps = [bright ** (0.5 * k) / (1 + 0.6 * k) for k in range(len(ALLOY))]
    y = D.modal_ir(f0, ALLOY, decays, amps, n, r) * D.env_exp(n, 1.0, attack=0.0005)
    click = D.bfilt(r.standard_normal(int(0.001 * SR)), 4000, 'highpass') * np.hanning(int(0.001 * SR))
    y[:len(click)] += 0.6 * bright * click
    return y


def metal_snare(n, r):
    """Noise from 1.5 to 9 kHz on a fast decay over a short alloy body on F#3."""
    t = np.arange(n) / SR
    noise = D.bfilt(r.standard_normal(n), (1500, 9000), 'bandpass') * np.exp(-t / 0.07)
    body = D.modal_ir(float(D.midi_hz(K.ROOT + 12)), ALLOY[:4], [0.05, 0.035, 0.025, 0.02], [1, 0.6, 0.4, 0.3], n, r)
    return 0.8 * noise / (np.abs(noise).max() + 1e-12) + 0.6 * body * D.env_exp(n, 1.0, attack=0.0008)


def tick(n, r):
    """A data tick: a 2 ms burst above 6 kHz and a 4 ms blip at the alloy ratio 3.011 over F#4."""
    y = np.zeros(n)
    b = int(0.002 * SR)
    y[:b] = D.bfilt(r.standard_normal(b), 6000, 'highpass') * np.hanning(b)
    m = int(0.004 * SR)
    f = float(D.midi_hz(K.ROOT + 24)) * ALLOY[4]
    y[:m] += 0.5 * np.sin(2 * np.pi * f * np.arange(m) / SR) * np.hanning(m)
    return y


def zap(n, f_hi=4200.0, f_lo=380.0, tau=0.02):
    """A laser zap: a sine and its alloy partial falling exponentially from f_hi to f_lo."""
    t = np.arange(n) / SR
    f = f_lo + (f_hi - f_lo) * np.exp(-t / tau)
    ph = 2 * np.pi * D.phase(f, n)
    return (np.sin(ph) + 0.4 * np.sin(ALLOY[1] * ph)) * np.exp(-t / 0.06)


# ------------------------------------------------------------------ glitch tools
def edge(x, ms=1.0):
    """A slice with a raised-cosine fade at both ends, so no slice edge clicks."""
    return D.fade(x, ms / 1000, ms / 1000)


def crush(x, rate_hz, bits, k=4):
    """Sample-and-hold at rate_hz and quantise to `bits`, run at k x so only the hold's own images fold back, then a
    low-pass at 18 kHz."""
    u = D.up(x, k)
    hold = np.floor(np.arange(len(u)) * rate_hz / (SR * k)).astype(int)
    first = np.concatenate([[0], np.flatnonzero(np.diff(hold)) + 1])
    u = u[first[np.searchsorted(first, np.arange(len(u)), side='right') - 1]]
    q = 2 ** (bits - 1)
    return D.bfilt(D.down(np.round(u * q) / q, k), 18000, 'lowpass', order=4)


def repitch(x, semitones):
    """Play a slice of synthesised material 2^(st/12) faster, by linear interpolation: a glitch, crushed next."""
    ratio = 2 ** (semitones / 12)
    pos = np.arange(int(len(x) / ratio)) * ratio
    return np.interp(pos, np.arange(len(x)), x)


def pan(x, p):
    """Equal-power pan, p in -1..1."""
    a = (p + 1) * np.pi / 4
    return np.stack([x * np.cos(a), x * np.sin(a)], 1) * np.sqrt(2)


# ------------------------------------------------------------------ the glitch fill
def glitch_fill(take):
    r = take.rng('material')
    n = at(4)
    long_n = int(0.4 * SR)
    hit = metal_snare(long_n, r) + 0.8 * strike(F_ALLOY, long_n, r)
    ping = strike(F_ALLOY * 2 ** (5 / 12), long_n, r, bright=1.3)          # F#5
    ring_n = int(0.5 * SR)                                                 # the last 128th rings out
    roll = tick(ring_n, r) + 0.7 * zap(ring_n)
    out = np.zeros((n + ring_n, 2))
    cuts = []

    def put(pos, x, p):
        out[pos:pos + len(x)] += pan(x, p)
        cuts.append(pos)

    put(0, edge(hit[:at(1)]), 0.0)                                         # 16th 1: the hit
    s32 = at(0.5)
    put(at(1), edge(hit[:s32]), -0.3)                                      # 16th 2: a 32nd stutter
    put(at(1.5), edge(D.freq_shift(hit[:s32], -35.0)), 0.3)
    for k in range(4):                                                     # 16th 3: 64ths, up and crushed
        a, b = at(2 + 0.25 * k), at(2 + 0.25 * (k + 1))
        sl = repitch(ping[:2 * (b - a)], 2.0 * (k + 1))[:b - a]
        put(a, edge(crush(sl, 9000.0 / (k + 1), 8 - k)), 0.25 * (-1) ** k)
    for k in range(8):                                                     # 16th 4: 128ths, shifted up
        a, b = at(3 + 0.125 * k), at(3 + 0.125 * (k + 1))
        last = k == 7
        sl = roll[:(ring_n if last else b - a)]
        shift = 900.0 * np.minimum((k + np.arange(len(sl)) / (b - a)) / 8, 1.0) ** 1.5   # holds +900 Hz past the beat
        sl = D.freq_shift(sl, shift) * D.undb(-1.5 * (7 - k))
        if last:                                                           # rings out, gone 0.4 s after it starts
            sl = D.fade(sl, 0.001, 0.0) * D.taper(len(sl), 0.25, 0.15)
        put(a, sl if last else edge(sl), 0.35 * (-1) ** k)
    y = D.bfilt(out, 150.0, 'highpass', order=4)
    return Out(D.mono_below(y, 150.0), splices=tuple(c for c in cuts if c > 0))


# ------------------------------------------------------------------ the frequency-shifted riser
RISER_BARS = 8
RISER_N = int(np.floor(RISER_BARS * 4 * K.BEAT * SR + 0.5))


def svf_sweep(x, fc, kind):
    """A time-varying 2-pole filter per channel (Zavalishin TPT SVF). kind: 'lp' or 'hp'."""
    i = 0 if kind == 'lp' else 2
    return np.stack([D.svf_tpt(x[:, c], fc, q=0.6)[i] for c in range(x.shape[1])], 1)


def shift_riser(take):
    n = RISER_N
    t = np.arange(n) / SR
    u = t / t[-1]
    r = take.rng('air')
    tone = np.zeros(n)
    for k, rt in enumerate(ALLOY):                                         # an alloy chord on F#3 and F#4
        for f0, a in ((float(D.midi_hz(K.ROOT + 12)), 1.0), (float(D.midi_hz(K.ROOT + 24)), 0.5)):
            tone += a / (1 + 0.7 * k) * np.sin(2 * np.pi * f0 * rt * t + r.uniform(0, 2 * np.pi))
    air = D.bfilt(r.standard_normal(n), (800, 12000), 'bandpass')
    x = 0.25 * tone / np.std(tone) + (0.1 + 0.4 * u ** 2) * air / np.std(air)   # RMS: the air grows under the chord
    x = D.bfilt(x, 14000, 'lowpass', order=6)
    rate = 0.15 + 1.6 * u ** 2                                             # pole cycles per second, speeding up
    pole = np.cumsum(rate) / SR
    common = 1500.0 * u ** 3
    y = np.zeros((n, 2))
    for i in range(4):
        pos = (pole + i / 4) % 1.0
        sh = D.freq_shift(x, common + 600.0 * pos) * np.sin(np.pi * pos) ** 2
        y += pan(sh, 0.45 * (-1) ** i)
    y /= 2.0
    y = svf_sweep(y, 3000.0 * (16000.0 / 3000.0) ** u, 'lp')
    y = svf_sweep(y, 200.0 * 6.0 ** u ** 2, 'hp')
    y = y / np.abs(y).max()
    y = 0.5 * y + 0.5 * D.oversampled(lambda v: D.tanh_adaa(v, 2.0), y)   # density: the peaks closer to the loudness
    y *= D.undb(-24.0 + 24.0 * u ** 1.4)[:, None]
    return D.mono_below(D.bfilt(y, 150.0, 'highpass', order=4), 150.0)


ITEMS = [
    Item('FX', 'GlitchFill', 'fx', glitch_fill, sets=('s1a',), flags={'deliberate_alias'}, tags=('fill',),
         qa={'why': 'the sample-and-hold crush aliases on purpose (run at 4x, so only its own images fold back)'},
         about='one-beat glitch fill: hit, 32nd stutter shifted down, crushed 64ths rising, 128ths shifted to +900 Hz'),
    Item('FX', 'ShiftRiser', 'fx', shift_riser, level=-14.0, sets=('s1a',), flags={'cut_end'}, tags=('riser',),
         qa={'why': 'end-aligned: it stays loud to the downbeat, where the next section takes over'},
         about='8-bar barber-pole frequency-shifted riser on an alloy chord, end-aligned: it ends on the downbeat'),
]
