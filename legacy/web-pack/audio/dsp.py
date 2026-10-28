"""Small synthesis toolkit for the pack: oscillators, envelopes, filters, FX, file IO."""
import numpy as np
import soundfile as sf
from scipy import signal
import pedalboard as pb

SR = 44100
RNG = np.random.default_rng(7)

NOTE_PC = {'C': 0, 'C#': 1, 'Db': 1, 'D': 2, 'D#': 3, 'Eb': 3, 'E': 4, 'F': 5, 'F#': 6, 'Gb': 6,
           'G': 7, 'G#': 8, 'Ab': 8, 'A': 9, 'A#': 10, 'Bb': 10, 'B': 11}


def note_hz(name):
    """'F1' -> Hz (A4 = 440, C4 = 60)."""
    import re
    m = re.match(r'^([A-G][#b]?)(-?\d)$', name)
    midi = 12 * (int(m.group(2)) + 1) + NOTE_PC[m.group(1)]
    return 440.0 * 2 ** ((midi - 69) / 12)


def midi_hz(m):
    return 440.0 * 2 ** ((np.asarray(m, dtype=float) - 69) / 12)


def t_axis(dur):
    return np.arange(int(round(dur * SR))) / SR


def n_samples(dur):
    return int(round(dur * SR))


# ---------------- oscillators ----------------
def phase_from_freq(freq, n=None):
    freq = np.broadcast_to(np.asarray(freq, dtype=float), (n,)) if n is not None else np.asarray(freq, dtype=float)
    ph = np.cumsum(freq / SR)
    return ph - freq / SR  # start at 0


def sine(freq, n=None, phase0=0.0):
    ph = phase_from_freq(freq, n)
    return np.sin(2 * np.pi * (ph + phase0))


def _polyblep(t, dt):
    out = np.zeros_like(t)
    m1 = t < dt
    x = t[m1] / dt[m1]
    out[m1] = x + x - x * x - 1.0
    m2 = t > 1.0 - dt
    x = (t[m2] - 1.0) / dt[m2]
    out[m2] = x * x + x + x + 1.0
    return out


def saw(freq, n=None, phase0=0.0):
    f = np.broadcast_to(np.asarray(freq, dtype=float), (n,)) if n is not None else np.asarray(freq, dtype=float)
    ph = (phase_from_freq(f) + phase0) % 1.0
    dt = np.clip(f / SR, 1e-9, 0.5)
    return 2.0 * ph - 1.0 - _polyblep(ph, dt)


def square(freq, n=None, phase0=0.0, pw=0.5):
    f = np.broadcast_to(np.asarray(freq, dtype=float), (n,)) if n is not None else np.asarray(freq, dtype=float)
    ph = (phase_from_freq(f) + phase0) % 1.0
    dt = np.clip(f / SR, 1e-9, 0.5)
    sq = np.where(ph < pw, 1.0, -1.0)
    sq = sq + _polyblep(ph, dt) - _polyblep((ph + 1 - pw) % 1.0, dt)
    return sq


def noise(n):
    return RNG.uniform(-1, 1, n)


# ---------------- envelopes ----------------
def exp_env(n, tau, attack=0.0005):
    t = np.arange(n) / SR
    e = np.exp(-t / tau)
    a = max(1, int(attack * SR))
    e[:a] *= np.linspace(0, 1, a)
    return e


def adsr(n, a, d, s, r, gate):
    """gate = seconds the note is held."""
    t = np.arange(n) / SR
    e = np.zeros(n)
    a = max(a, 1e-4)
    e = np.where(t < a, t / a, s + (1 - s) * np.exp(-(t - a) / max(d, 1e-4)))
    rel = t > gate
    if rel.any():
        g_level = e[min(n - 1, int(gate * SR))]
        e[rel] = g_level * np.exp(-(t[rel] - gate) / max(r, 1e-4))
    return e


def fade(x, fin=0.001, fout=0.005):
    x = x.copy()
    a, b = int(fin * SR), int(fout * SR)
    if a > 0:
        x[:a] *= np.linspace(0, 1, a)[:, None] if x.ndim == 2 else np.linspace(0, 1, a)
    if b > 0:
        x[-b:] *= np.linspace(1, 0, b)[:, None] if x.ndim == 2 else np.linspace(1, 0, b)
    return x


# ---------------- filters ----------------
def bfilt(x, kind, freq, order=2):
    nyq = SR / 2
    if kind in ('bandpass', 'bandstop'):
        wn = [max(10, freq[0]) / nyq, min(freq[1], nyq * 0.95) / nyq]
    else:
        wn = min(max(freq, 10), nyq * 0.95) / nyq
    sos = signal.butter(order, wn, btype=kind, output='sos')
    return signal.sosfilt(sos, x, axis=0)


def svf(x, cutoff, q=0.7, mode='lp'):
    """Time-varying state-variable filter (Chamberlin, 2x oversampled-ish via tan prewarp)."""
    x = np.asarray(x, dtype=float)
    n = len(x)
    fc = np.broadcast_to(np.asarray(cutoff, dtype=float), (n,))
    g = np.tan(np.pi * np.clip(fc, 20, SR * 0.45) / SR)
    k = 1.0 / q
    a1 = 1.0 / (1.0 + g * (g + k))
    a2 = g * a1
    a3 = g * a2
    ic1 = ic2 = 0.0
    out = np.empty(n)
    for i in range(n):
        v3 = x[i] - ic2
        v1 = a1[i] * ic1 + a2[i] * v3
        v2 = ic2 + a2[i] * ic1 + a3[i] * v3
        ic1 = 2 * v1 - ic1
        ic2 = 2 * v2 - ic2
        if mode == 'lp':
            out[i] = v2
        elif mode == 'bp':
            out[i] = v1
        else:  # hp
            out[i] = x[i] - k * v1 - v2
    return out


def svf_stereo(x, cutoff, q=0.7, mode='lp'):
    if x.ndim == 1:
        return svf(x, cutoff, q, mode)
    return np.stack([svf(x[:, c], cutoff, q, mode) for c in range(x.shape[1])], axis=1)


# ---------------- dynamics / colour ----------------
def sat(x, drive=1.5):
    return np.tanh(drive * x) / np.tanh(drive)


def to_stereo(x, pan=0.0, width=0.0):
    if x.ndim == 2:
        return x
    l = np.cos((pan + 1) * np.pi / 4)
    r = np.sin((pan + 1) * np.pi / 4)
    return np.stack([x * l * np.sqrt(2), x * r * np.sqrt(2)], axis=1)


def board(x, plugins):
    """Run a pedalboard chain on mono (n,) or stereo (n,2)."""
    arr = x.T.astype(np.float32) if x.ndim == 2 else x[None, :].astype(np.float32)
    y = pb.Pedalboard(plugins)(arr, SR)
    return y.T.astype(float) if x.ndim == 2 else y[0].astype(float)


def reverb(x, size=0.4, wet=0.2, damp=0.5, width=1.0, dry=1.0):
    x = to_stereo(x)
    return board(x, [pb.Reverb(room_size=size, damping=damp, wet_level=wet, dry_level=dry, width=width)])


def delay(x, secs, fb=0.35, mix=0.25):
    return board(x, [pb.Delay(delay_seconds=secs, feedback=fb, mix=mix)])


def pingpong(x, secs, fb=0.4, mix=0.3, lp=4000.0, taps=8):
    """Stereo ping-pong delay with a darkening feedback path."""
    x = to_stereo(x)
    n = len(x)
    d = int(secs * SR)
    total = n + d * taps
    out = np.zeros((total, 2))
    out[:n] += x * (1 - mix * 0.5)
    mono = x.mean(axis=1)
    tap = mono.copy()
    for k in range(1, taps + 1):
        tap = bfilt(tap, 'lowpass', lp, 1) * fb if k > 1 else bfilt(tap, 'lowpass', lp, 1) * mix
        ch = (k - 1) % 2
        out[k * d:k * d + n, ch] += tap
    return out


def normalize(x, peak_db=-1.0):
    p = np.max(np.abs(x)) + 1e-12
    return x * (10 ** (peak_db / 20) / p)


def trim_to(x, n):
    if len(x) >= n:
        return x[:n]
    pad = np.zeros((n - len(x),) + x.shape[1:])
    return np.concatenate([x, pad])


def place(buf, x, at, gain=1.0):
    """Mix x into buf starting at sample index at (clipped)."""
    at = int(at)
    if at >= len(buf):
        return
    end = min(len(buf), at + len(x))
    if x.ndim == 1 and buf.ndim == 2:
        x = to_stereo(x)
    buf[at:end] += x[:end - at] * gain


def write(path, x, peak_db=-1.0, subtype='PCM_16'):
    x = normalize(x, peak_db) if peak_db is not None else x
    x = np.clip(x, -1, 1)
    sf.write(path, x.astype(np.float32), SR, subtype=subtype)
    return x


def peaks(x, n=64):
    """Waveform overview: n peak values 0-1 (mono-summed)."""
    m = np.abs(x).max(axis=1) if x.ndim == 2 else np.abs(x)
    chunks = np.array_split(m, n)
    p = np.array([c.max() if len(c) else 0 for c in chunks])
    top = p.max() + 1e-12
    return [round(float(v / top), 3) for v in p]


# ---------------- swing / sequencing ----------------
def warp16(p, swing):
    base = np.floor(p / 2) * 2
    f = p - base
    return base + np.where(f <= 1, f * 2 * swing, 2 * swing + (f - 1) * (2 - 2 * swing))


def step_seconds(bpm):
    return 60.0 / bpm / 4.0  # a 16th note


def vel_gain(d):
    v = min(127, round(13 + d * 12.7)) / 127.0
    return v ** 1.6
