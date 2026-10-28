"""Drum one-shot synthesis. Each function returns a mono float array."""
import numpy as np
from dsp import *


def kick(f_end, f_start=170.0, tau_p=0.035, tau_a=0.16, dur=0.42, click=0.4, drive=1.6, harm=0.0):
    n = n_samples(dur)
    t = np.arange(n) / SR
    f = f_end + (f_start - f_end) * np.exp(-t / tau_p)
    body = sine(f) * exp_env(n, tau_a, attack=0.0008)
    if harm:
        body += harm * sine(2 * f) * exp_env(n, tau_a * 0.35, attack=0.0008)
    clk = bfilt(noise(n), 'highpass', 1500) * exp_env(n, 0.0022)
    tone_clk = sine(np.full(n, 1700.0)) * exp_env(n, 0.003)
    x = body + click * (0.6 * clk + 0.4 * tone_clk)
    x = sat(x, drive)
    return fade(x, 0, 0.012)


def clap(spread=(0, 0.008, 0.017, 0.026), tail_tau=0.075, band=(900, 2600), room=0.18, dur=0.6):
    n = n_samples(dur)
    nz = bfilt(noise(n), 'bandpass', band, 2)
    out = np.zeros(n)
    for k, o in enumerate(spread):
        i = int(o * SR)
        e = np.zeros(n)
        e[i:] = np.exp(-np.arange(n - i) / SR / 0.0045) * (0.8 if k < len(spread) - 1 else 1.0)
        out += nz * e
    i = int(spread[-1] * SR)
    tail = np.zeros(n)
    tail[i:] = np.exp(-np.arange(n - i) / SR / tail_tau)
    x = out + 0.65 * nz * tail
    x = bfilt(x, 'highpass', 450, 2)
    if room:
        x = board(x, [pb.Reverb(room_size=room, damping=0.6, wet_level=0.18, dry_level=1.0, width=0.0)])
    return fade(x, 0, 0.02)


def snare(t1=185.0, t2=330.0, tone_tau=0.055, noise_tau=0.09, hp=1800, lp=11000, tone_amt=0.55, noise_amt=0.75, dur=0.35, drive=1.3):
    n = n_samples(dur)
    t = np.arange(n) / SR
    tone = (0.62 * sine(t1 * (1 + 0.25 * np.exp(-t / 0.01))) + 0.38 * sine(t2 * (1 + 0.1 * np.exp(-t / 0.008)))) * exp_env(n, tone_tau)
    nz = bfilt(bfilt(noise(n), 'highpass', hp, 2), 'lowpass', lp, 1) * exp_env(n, noise_tau)
    x = tone_amt * tone + noise_amt * nz
    return fade(sat(x, drive), 0, 0.015)


def rim(f1=1720.0, f2=480.0, tau=0.0075, dur=0.12, drive=2.0):
    n = n_samples(dur)
    x = (0.65 * sine(np.full(n, f1)) + 0.45 * sine(np.full(n, f2))) * exp_env(n, tau)
    x += 0.3 * bfilt(noise(n), 'highpass', 3000) * exp_env(n, 0.002)
    x = bfilt(x, 'highpass', 250)
    return fade(sat(x, drive), 0, 0.01)


METAL = np.array([205.3, 304.4, 369.6, 522.7, 540.0, 800.0])


def hat(tau, dur, scale=1.0, metal_amt=0.7, noise_amt=0.5, band=(6500, 12500), hp=7000):
    n = n_samples(dur)
    metal = sum(square(np.full(n, f * scale)) for f in METAL) / 6.0
    metal = bfilt(metal, 'bandpass', band, 2)
    nz = bfilt(bfilt(noise(n), 'highpass', hp, 2), 'lowpass', 15500, 1)
    x = (metal_amt * metal + noise_amt * nz) * exp_env(n, tau, attack=0.0003)
    return fade(x, 0, 0.01)


def shaker(dur=0.15, attack=0.012, tau=0.03):
    n = n_samples(dur)
    e = exp_env(n, tau, attack=0.0001)
    a = int(attack * SR)
    ramp = np.ones(n)
    ramp[:a] = np.linspace(0.15, 1, a) ** 2
    e = np.concatenate([ramp[:a], np.exp(-np.arange(n - a) / SR / tau)])
    x = bfilt(noise(n), 'bandpass', (4500, 11000), 2) * e
    return fade(x, 0, 0.01)


def tamb(dur=0.35):
    n = n_samples(dur)
    x = np.zeros(n)
    rng = np.random.default_rng(11)
    for _ in range(14):
        f = rng.uniform(5200, 10500)
        x += sine(np.full(n, f), phase0=rng.uniform()) * exp_env(n, rng.uniform(0.04, 0.11)) * rng.uniform(0.4, 1.0)
    x = x / 14 + 0.6 * bfilt(noise(n), 'bandpass', (6000, 12500), 2) * exp_env(n, 0.07)
    a = int(0.003 * SR)
    x[:a] *= np.linspace(0, 1, a)
    return fade(x, 0, 0.02)


def drum_tone(fc, tau=0.09, bend=0.6, bend_tau=0.008, dur=0.4, slap=0.3, partial=0.2):
    n = n_samples(dur)
    t = np.arange(n) / SR
    f = fc * (1 + bend * np.exp(-t / bend_tau))
    x = sine(f) * exp_env(n, tau, attack=0.0005)
    x += partial * sine(1.5 * f) * exp_env(n, tau * 0.33)
    x += slap * bfilt(noise(n), 'bandpass', (1500, 5000)) * exp_env(n, 0.004)
    return fade(sat(x, 1.2), 0, 0.02)


def clave(dur=0.1):
    n = n_samples(dur)
    x = sine(np.full(n, 2500.0)) * exp_env(n, 0.018) + 0.3 * sine(np.full(n, 5100.0)) * exp_env(n, 0.009)
    return fade(x, 0, 0.01)


def click(dur=0.08):
    n = n_samples(dur)
    t = np.arange(n) / SR
    x = sine(1250 * (1 + 0.5 * np.exp(-t / 0.004))) * exp_env(n, 0.02)
    x += 0.4 * bfilt(noise(n), 'highpass', 4000) * exp_env(n, 0.0015)
    x = bfilt(x, 'bandpass', (700, 7000), 1)
    return fade(x, 0, 0.008)


def ride(dur=1.6):
    n = n_samples(dur)
    freqs = np.array([372, 495, 617, 788, 931, 1153]) * 2.2
    metal = sum(square(np.full(n, f)) for f in freqs) / 6.0
    metal = bfilt(metal, 'bandpass', (3000, 14000), 2) * exp_env(n, 0.55, attack=0.0003)
    bell = (0.25 * sine(np.full(n, 3350.0)) + 0.15 * sine(np.full(n, 5020.0))) * exp_env(n, 0.25)
    nz = 0.3 * bfilt(noise(n), 'highpass', 6000) * exp_env(n, 0.4)
    return fade(metal + bell + nz, 0, 0.05)


def crash(dur=2.4):
    n = n_samples(dur)
    rng = np.random.default_rng(5)
    metal = sum(square(np.full(n, f)) for f in rng.uniform(300, 1500, 8)) / 8.0
    metal = bfilt(metal, 'bandpass', (3000, 15000), 2)
    nz = bfilt(noise(n), 'highpass', 2500, 2)
    x = (0.5 * metal + 0.8 * nz) * exp_env(n, 0.9, attack=0.002)
    return fade(x, 0, 0.1)


def sub_boom(f_end, f_start=None, dur=2.0, tau=0.7):
    n = n_samples(dur)
    t = np.arange(n) / SR
    f_start = f_start or f_end * 1.6
    f = f_end + (f_start - f_end) * np.exp(-t / 0.25)
    x = sine(f) * exp_env(n, tau, attack=0.002)
    return fade(sat(x, 1.4), 0, 0.05)


# name, function, label, group, key/pitch note (None = unpitched)
ONE_SHOTS = [
    ('kick_ukg', lambda: kick(note_hz('G1'), 175, 0.032, 0.15, 0.42, 0.45, 1.7), 'Kick UKG Punch', 'ukg', 'G'),
    ('kick_ukg_boom', lambda: kick(note_hz('F1'), 140, 0.05, 0.3, 0.75, 0.3, 1.4), 'Kick UKG Boom', 'ukg', 'F'),
    ('clap_ukg', lambda: clap(room=0.22), 'Clap UKG', 'ukg', None),
    ('snare_ukg', lambda: snare(), 'Snare UKG Crisp', 'ukg', None),
    ('rim_ukg', lambda: rim(), 'Rim UKG', 'ukg', None),
    ('chat_ukg', lambda: hat(0.022, 0.12), 'Closed Hat UKG', 'ukg', None),
    ('ohat_ukg', lambda: hat(0.2, 0.6), 'Open Hat UKG', 'ukg', None),
    ('kick_tech', lambda: kick(note_hz('A1'), 260, 0.024, 0.22, 0.55, 0.35, 2.1, harm=0.15), 'Kick Tech 909', 'tech', 'A'),
    ('kick_tech_round', lambda: kick(note_hz('G1'), 200, 0.03, 0.26, 0.6, 0.25, 1.5), 'Kick Tech Round', 'tech', 'G'),
    ('clap_tech', lambda: clap(spread=(0, 0.011, 0.02, 0.03), tail_tau=0.06, band=(1000, 3200), room=0.1), 'Clap Tech Dry', 'tech', None),
    ('snare_tech', lambda: snare(180, 240, 0.07, 0.16, 1200, 8000, 0.7, 0.65, 0.4, 1.5), 'Snare Tech 909', 'tech', None),
    ('rim_tech', lambda: rim(1850, 520, 0.006, 0.1, 2.4), 'Rim Tech', 'tech', None),
    ('chat_tech', lambda: hat(0.03, 0.14, scale=1.3, metal_amt=0.45, noise_amt=0.8, band=(7000, 14000), hp=8000), 'Closed Hat Tech', 'tech', None),
    ('ohat_tech', lambda: hat(0.28, 0.7, scale=1.3, metal_amt=0.45, noise_amt=0.8, band=(7000, 14000), hp=8000), 'Open Hat Tech', 'tech', None),
    ('ride', lambda: ride(), 'Ride', 'perc', None),
    ('crash', lambda: crash(), 'Crash', 'perc', None),
    ('shaker', lambda: shaker(), 'Shaker', 'perc', None),
    ('tamb', lambda: tamb(), 'Tambourine', 'perc', None),
    ('conga_hi', lambda: drum_tone(note_hz('E4'), 0.09), 'Conga High', 'perc', 'E'),
    ('conga_lo', lambda: drum_tone(note_hz('A3'), 0.12), 'Conga Low', 'perc', 'A'),
    ('bongo_hi', lambda: drum_tone(note_hz('C5'), 0.055, 0.4, 0.005, 0.3, 0.35, 0.15), 'Bongo High', 'perc', 'C'),
    ('bongo_lo', lambda: drum_tone(note_hz('G4'), 0.07, 0.45, 0.006, 0.3, 0.3, 0.15), 'Bongo Low', 'perc', 'G'),
    ('clave', lambda: clave(), 'Clave', 'perc', None),
    ('click', lambda: click(), 'Perc Click', 'perc', None),
    ('tom_lo', lambda: drum_tone(note_hz('F2'), 0.2, 0.8, 0.03, 0.6, 0.15, 0.1), 'Tom Low', 'perc', 'F'),
    ('tom_mid', lambda: drum_tone(note_hz('Ab2'), 0.18, 0.8, 0.03, 0.55, 0.15, 0.1), 'Tom Mid', 'perc', 'Ab'),
    ('tom_hi', lambda: drum_tone(note_hz('C3'), 0.16, 0.8, 0.03, 0.5, 0.15, 0.1), 'Tom High', 'perc', 'C'),
]


def render_all():
    return {name: fn() for name, fn, *_ in ONE_SHOTS}


def cowbell(dur=0.45):
    n = n_samples(dur)
    x = 0.6 * square(np.full(n, 540.0)) + 0.6 * square(np.full(n, 800.0))
    x = bfilt(x, 'bandpass', (650, 3200), 2)
    env = 0.65 * exp_env(n, 0.018, 0.0005) + 0.35 * exp_env(n, 0.14, 0.0005)
    return fade(sat(x * env, 1.3), 0, 0.02)


ONE_SHOTS_MORE = [
    ('kick_sg', lambda: kick(note_hz('F1'), 190, 0.03, 0.22, 0.55, 0.5, 2.0, harm=0.12), 'Kick Speed Garage', 'sg', 'F'),
    ('kick_house', lambda: kick(note_hz('A1'), 240, 0.026, 0.3, 0.72, 0.4, 2.2, harm=0.1), 'Kick House 909 Long', 'house', 'A'),
    ('clap_house', lambda: clap(spread=(0, 0.01, 0.021, 0.033), tail_tau=0.11, band=(850, 2800), room=0.3), 'Clap House', 'house', None),
    ('snare_house', lambda: snare(190, 260, 0.06, 0.18, 1500, 10000, 0.6, 0.75, 0.45, 1.6), 'Snare House 909', 'house', None),
    ('chat_house', lambda: hat(0.035, 0.15, scale=1.45, metal_amt=0.35, noise_amt=0.9, band=(7500, 15000), hp=8500), 'Closed Hat House', 'house', None),
    ('ohat_house', lambda: hat(0.32, 0.8, scale=1.45, metal_amt=0.35, noise_amt=0.9, band=(7500, 15000), hp=8500), 'Open Hat House', 'house', None),
    ('cowbell', lambda: cowbell(), 'Cowbell 808', 'perc', None),
]


def render_more():
    return {name: fn() for name, fn, *_ in ONE_SHOTS_MORE}
