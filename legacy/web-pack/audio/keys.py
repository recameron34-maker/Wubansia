"""House keys: piano (additive, inharmonic, 3 strings per note), M1-style organ bass, string ensemble."""
import numpy as np
import pedalboard as pb
from dsp import SR, sine, saw, svf, bfilt, board, reverb, fade, midi_hz, noise, exp_env, sat

RNG = np.random.default_rng(42)


def piano_note(m, dur, vel=100, release=None, tail=0.5):
    """One piano note. dur = key-down seconds; the damper closes at `release` (defaults to dur)."""
    f0 = float(midi_hz(m))
    n = int((dur + tail) * SR)
    t = np.arange(n) / SR
    vf = vel / 127.0
    B = 0.00011 * 2 ** ((m - 60) / 20)
    out = np.zeros(n)
    kmax = int(min(36, 11000 / f0))
    ref = (262.0 / f0)
    for k in range(1, kmax + 1):
        fk = k * f0 * np.sqrt(1 + B * k * k)
        if fk > SR * 0.45:
            break
        amp = (1.0 / k ** (1.25 - 0.45 * vf)) * np.exp(-0.03 * k * (1 - vf))
        t1 = 0.30 * ref ** 0.35 / (1 + 0.22 * (k - 1))
        t2 = 2.4 * ref ** 0.45 / (1 + 0.28 * (k - 1))
        ws = 0.4 * np.exp(-(k - 1) / 7.0)
        env = (1 - ws) * np.exp(-t / t1) + ws * np.exp(-t / t2)
        for det, w in ((-0.45, 0.33), (0.0, 0.34), (0.5, 0.33)):
            out += w * amp * env * np.sin(2 * np.pi * fk * 2 ** (det / 1200) * t + RNG.uniform(0, 2 * np.pi))
    ham = bfilt(noise(n), 'lowpass', 1800 + 3500 * vf, 2) * exp_env(n, 0.004, 0.0002) * 0.05
    out = out + ham
    r = dur if release is None else release
    i = int(r * SR)
    if i < n:
        out[i:] *= np.exp(-(t[i:] - t[i]) / 0.085)
    a = int(0.0015 * SR)
    out[:a] *= np.linspace(0, 1, a)
    return out * (0.35 + 0.65 * vf)


def piano_post(x, room=0.32, wet=0.14):
    """M1-ish house piano colour: bright, chorused, compressed."""
    y = board(x if x.ndim == 2 else x, [pb.HighpassFilter(70), pb.LowShelfFilter(250, -2.0), pb.HighShelfFilter(3800, 4.5),
                                       pb.Compressor(threshold_db=-18, ratio=3, attack_ms=4, release_ms=120)])
    from dsp import to_stereo
    y = to_stereo(y)
    y = board(y, [pb.Chorus(rate_hz=0.9, depth=0.18, mix=0.32, centre_delay_ms=6)])
    return reverb(y, room, wet, 0.5, 1.0, 1.0)


def organ_bass_note(m, dur, gate=None, vel=110):
    """M1 'Organ 2'-style bass: drawbar harmonics, percussive decay, key click."""
    f0 = float(midi_hz(m))
    n = int((dur + 0.12) * SR)
    t = np.arange(n) / SR
    g = dur if gate is None else gate
    x = np.zeros(n)
    for h, a in ((1, 1.0), (2, 0.62), (3, 0.34), (4, 0.2), (5, 0.08), (6, 0.07), (8, 0.03)):
        if f0 * h < SR * 0.45:
            x += a * np.sin(2 * np.pi * f0 * h * t + RNG.uniform(0, 6.28))
    perc = 0.5 * np.sin(2 * np.pi * f0 * 3 * t) * np.exp(-t / 0.06)
    x = x + perc
    env = 0.48 + 0.52 * np.exp(-t / 0.22)
    a = int(0.003 * SR)
    env[:a] *= np.linspace(0, 1, a)
    i = int(g * SR)
    if i < n:
        env[i:] *= np.exp(-(t[i:] - t[i]) / 0.03)
    x *= env
    x += 0.06 * bfilt(noise(n), 'bandpass', (1500, 5000)) * np.exp(-t / 0.004)
    x = bfilt(x, 'lowpass', 2600, 2)
    return sat(x * 0.9, 1.4) * (0.5 + 0.5 * vel / 127)


def string_chord(midis, dur, gate=None, attack=0.22):
    n = int((dur + 0.9) * SR)
    t = np.arange(n) / SR
    g = dur if gate is None else gate
    L = np.zeros(n)
    R = np.zeros(n)
    vib = 1 + 0.0035 * np.clip((t - 0.25) / 0.4, 0, 1) * np.sin(2 * np.pi * 5.1 * t)
    for m in midis:
        f = float(midi_hz(m))
        for i, c in enumerate((-12, -6, -2, 3, 7, 12)):
            v = saw(f * 2 ** (c / 1200) * vib, phase0=RNG.uniform())
            (L if i % 2 == 0 else R)[:] += v
    x = np.stack([L, R], 1) / (len(midis) * 3)
    x = np.stack([svf(x[:, 0], 4200, 0.6, 'lp'), svf(x[:, 1], 4200, 0.6, 'lp')], 1)
    env = np.minimum(1, t / attack) ** 1.5
    i = int(g * SR)
    if i < n:
        env[i:] *= np.exp(-(t[i:] - t[i]) / 0.35)
    x *= env[:, None]
    x = board(x, [pb.HighpassFilter(110), pb.Chorus(rate_hz=0.6, depth=0.25, mix=0.4, centre_delay_ms=9)])
    return reverb(x, 0.6, 0.22, 0.5, 1.0, 1.0)
