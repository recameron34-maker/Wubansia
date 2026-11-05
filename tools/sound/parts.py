"""Building blocks every kit's recipes share, none tied to a kit: drum pieces (the pitch sweep, gates, struck modes,
noise bands, the soft clip, the low clean-up), loop pieces (raised edges, a fractional delay, the steep high-pass, loops
of whole cycles and their closing) and the tuned Karplus-Strong string.

They came out of FUT01's recipes (drums.py, bass.py, music.py) unchanged, so FUT01 renders the same bytes. A recipe
imports them (`from tools.sound import parts as P`) instead of another kit's recipes.
"""
import numpy as np
from scipy import signal

from tools.sound import dsp48 as D

SR = D.SR


def times(n):
    """The times of n samples, in seconds."""
    return np.arange(n) / SR


def lerp(pair, v):
    return pair[0] + (pair[1] - pair[0]) * v


# ------------------------------------------------------------------ drum pieces
def vel_gain(v, span_db, v_min=0.2):
    """Level of a layer: span_db between the softest layer (v_min) and the hardest (1.0). Synthesis adds its own rise
    on top (brighter, harder hits read louder)."""
    return float(D.undb(span_db * (v - 1.0) / (1.0 - v_min)))


def sweep(n, f_hi, tau1, tau2, w2, f_end):
    """Kick pitch drop from f_hi to exactly f_end: a fast snap (tau1) and a glide (tau2, weight w2). Returns the phase
    in radians."""
    t = times(n)
    f = f_end + (f_hi - f_end) * ((1 - w2) * np.exp(-t / tau1) + w2 * np.exp(-t / tau2))
    return 2 * np.pi * D.phase(f, n)


def gate(t, hold, tau):
    """1 for `hold` seconds, then an exponential fall."""
    return np.exp(-np.maximum(0.0, t - hold) / tau)


def hann_pulse_gain(f, width):
    """Magnitude of a Hann pulse `width` seconds long at f Hz, re DC: a shorter strike reaches higher modes."""
    x = np.asarray(f, dtype=float) * width
    with np.errstate(divide='ignore', invalid='ignore'):
        g = np.sinc(x) / (1.0 - x * x)
    return np.abs(np.where(np.isclose(np.abs(x), 1.0), 0.5, g))


def modes(n, f0, ratios, decays, amps, bend, r):
    """Damped sines at f0 * ratios whose pitch starts `bend` (a fraction, one value per sample) sharp: the pitch
    envelope of a struck plate. Modes above 20 kHz are left out; start phases come from r."""
    t = times(n)
    y = np.zeros(n)
    for rt, d, a in zip(ratios, decays, amps):
        f = f0 * rt
        ph0 = r.uniform(0, 2 * np.pi)
        if f * (1 + bend[0]) < 20000:
            y += a * np.exp(-t / d) * np.sin(2 * np.pi * D.phase(f * (1 + bend), n) + ph0)
    return y


def band_noise(r, n, lo, hi, order=2):
    """Gaussian noise band-passed to lo-hi Hz, scaled by the band's width to about unit RMS (a fixed gain, the same on
    every take)."""
    return D.bfilt(r.standard_normal(n), (lo, hi), 'bandpass', order=order) * np.sqrt(0.5 * SR / (hi - lo))


def hp_noise(r, n, fc, order=4):
    """Gaussian noise high-passed at fc, about unit RMS."""
    return D.bfilt(r.standard_normal(n), fc, 'highpass', order=order) * np.sqrt(0.5 * SR / (0.5 * SR - fc))


def soft_clip(x, drive):
    """tanh(drive x) / drive (ADAA): unity for small signals, so it shaves peaks without moving the level."""
    return D.tanh_adaa(x, drive) * np.tanh(drive) / drive


def low_clean(x, hp=120.0):
    """Off the kick and sub: high-pass, and mono under 150 Hz."""
    return D.mono_below(D.bfilt(D.st(x), hp, 'highpass', order=4), 150.0)


# ------------------------------------------------------------------ loop pieces
def raised(n, length):
    """A raised-cosine rise over `length` seconds, then 1."""
    a = max(1, int(length * SR))
    e = np.ones(n)
    e[:a] = 0.5 - 0.5 * np.cos(np.pi * np.arange(a) / a)
    return e


def frac_delay(x, d):
    """x delayed by d samples (fractional, linear interpolation), silent before the start."""
    i = int(np.floor(d))
    fr = d - i
    y = np.zeros_like(x)
    y[i:] = (1 - fr) * x[:len(x) - i]
    y[i + 1:] += fr * x[:len(x) - i - 1]
    return y


def hp_steep(x, fc):
    """Two 8th-order Butterworth high-passes in a row: about 96 dB per octave under fc."""
    return D.bfilt(D.bfilt(x, fc, 'highpass', order=8), fc, 'highpass', order=8)


def loop_grid(f0, seconds=2.0):
    """A loop of about `seconds` holding a whole number of f0 cycles: (length in samples, the grid step in Hz, the
    pitch on the grid). Every oscillator on the grid repeats exactly once per loop."""
    cycles = round(f0 * seconds)
    L = int(round(cycles * SR / f0))
    return L, SR / L, cycles * SR / L


def close_loop(y, a0, L, xfade=0.02, search=0.02):
    """Loop points for a sound that repeats every L samples from about a0 on: the start is the sample in the next
    `search` seconds with the smallest step into it (no saw edge on the seam), and the loop's last `xfade` seconds fade
    into the samples just before the start, so it also joins when the attack hasn't quite settled. y: (n, 2). Returns
    the file (attack plus one loop) and the loop points."""
    d = np.abs(np.diff(y[a0 - 1:a0 + int(search * SR)], axis=0)).max(1)
    a = a0 + int(np.argmin(d))
    X = int(xfade * SR)
    w = np.linspace(0.0, 1.0, X)[:, None]
    y = np.array(y[:a + L])
    y[a + L - X:] = (1 - w) * y[a + L - X:] + w * y[a - X:a]
    return y, (a, a + L)


def allpass(x, d, a):
    """Schroeder all-pass: a flat magnitude and a phase that wanders with frequency, a decorrelator. Its output holds
    -a times the input at no delay (its first sample is -a x[0]), so a side built from it carries an anti-phase copy
    of the sound: with a = 0.6 it combs left against right and can lean the top to one side (FUT02's arp read 4.4 dB
    louder right above 3 kHz in round 3). Check a wide part's lean per band (mixmeter.lean_db), not only its side
    over mid."""
    b, den = np.zeros(d + 1), np.zeros(d + 1)
    b[0], b[d], den[0], den[d] = -a, 1.0, 1.0, -a
    return signal.lfilter(b, den, x)


# ------------------------------------------------------------------ the tuned string
def ks_loop(f_in, S, disp):
    """dsp48.karplus's loop for a requested pitch: the integer delay N and the loop filter B/A (brightness FIR, tuning
    all-pass, dispersion all-pass), built the same way."""
    period = SR / f_in
    dd = (1 - disp) / (1 + disp) if disp else 0.0
    N = int(np.floor(period - S - dd - 0.1))
    frac = period - N - S - dd
    eta = (1 - frac) / (1 + frac)
    B = np.polymul([1 - S, S], [eta, 1.0])
    A = np.array([1.0, eta])
    if disp:
        B = np.polymul(B, [disp, 1.0])
        A = np.polymul(A, [1.0, disp])
    return N, B, A


def ks_pitch(f_in, S, disp):
    """The string's actual fundamental: the f at which N plus the loop filter's phase delay at f is one period."""
    N, B, A = ks_loop(f_in, S, disp)
    f = f_in
    for _ in range(20):
        w = 2 * np.pi * f / SR
        h = signal.freqz(B, A, worN=[w])[1][0]
        f = SR / (N - np.angle(h) / w)
    return f


def ks_tuned(f0, S, disp):
    """The pitch to ask dsp48.karplus() for so a stiff string (brightness S, dispersion disp) sounds at f0: a few
    fixed-point steps on ks_pitch. karplus tunes its delay from the loop filters' delay at DC, which with stiffness
    leaves the string sharp (0.3 cents at C6, 2 cents at C7 with disp -0.3)."""
    f_in = f0
    for _ in range(30):
        f_in *= f0 / ks_pitch(f_in, S, disp)
    return f_in
