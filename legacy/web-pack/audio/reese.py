"""Reese bass engine.

Signal flow per voice:  6 detuned polyBLEP saws (3 per side, different detune sets per side + a shared centre pair)
  -> 24 dB low-pass (ladder for static patches, 2x SVF for moving ones) -> drive -> phaser for the slow 'Reese' motion
  -> high-pass at 140 Hz so the mids never fight the sub -> two-band compression glue
  + a pure mono sine sub on the fundamental, low-passed at 120 Hz
  -> soft clip, -1 dBFS.
A note list is turned into one continuous pitch track (mono synth), with exponential glides into legato notes.
"""
import numpy as np
import pedalboard as pb
from dsp import SR, saw, sine, svf, bfilt, board, sat, fade, normalize

RNG_PH = np.random.default_rng(1977)

PATCHES = {
    # name: cutoff Hz, resonance, drive, phaser mix, glide s, env amount (Hz), lfo (cycles per beat, depth octaves, base Hz)
    'classic': dict(cut=1500, res=0.28, drive=2.4, phaser=0.28, glide=0.055, env=900, lfo=None, sub=0.85, width=1.0),
    'dark': dict(cut=720, res=0.22, drive=1.9, phaser=0.22, glide=0.07, env=420, lfo=None, sub=1.0, width=0.8),
    'wobble': dict(cut=None, res=0.45, drive=3.0, phaser=0.15, glide=0.05, env=0, lfo=(2.0, 3.4, 230), sub=0.85, width=1.0),
    'growl': dict(cut=None, res=0.6, drive=4.2, phaser=0.2, glide=0.05, env=0, lfo=(1.0, 2.6, 420), sub=0.8, width=1.0),
    'stab': dict(cut=2300, res=0.35, drive=2.8, phaser=0.18, glide=0.03, env=2200, lfo=None, sub=0.8, width=1.0),
}
DETUNE_L = (-17.0, -6.0, 4.5)
DETUNE_R = (-4.5, 6.0, 17.0)
DETUNE_C = (-9.0, 9.0)


def pitch_track(notes, n, glide):
    """notes: (start_s, dur_s, midi, vel, legato_into_next). Returns per-sample midi pitch, gate and velocity arrays."""
    midi = np.zeros(n)
    gate = np.zeros(n)
    vel = np.zeros(n)
    notes = sorted(notes)
    cur = notes[0][2] if notes else 36
    prev_end = -1.0
    prev_leg = False
    for i, (st, du, m, v, leg) in enumerate(notes):
        a = int(st * SR)
        b = min(n, int((st + du) * SR))
        if a >= n:
            break
        slide = prev_leg and (st - prev_end) < 0.03
        seg = np.full(b - a, float(m))
        if slide:
            g = min(b - a, int(glide * SR))
            k = np.arange(g) / max(1, g)
            seg[:g] = cur + (m - cur) * (1 - np.exp(-5 * k)) / (1 - np.exp(-5))
        midi[a:b] = seg
        gate[a:b] = 1.0
        vel[a:b] = v / 127.0
        # hold pitch through the gap so the release keeps its note
        nxt = notes[i + 1][0] if i + 1 < len(notes) else n / SR
        c = min(n, int(nxt * SR))
        if c > b:
            midi[b:c] = m
            vel[b:c] = v / 127.0
        cur = m
        prev_end = st + du
        prev_leg = leg
    first = int(notes[0][0] * SR) if notes else 0
    midi[:first] = notes[0][2] if notes else 36
    return midi, gate, vel


def amp_env(gate, attack=0.004, release=0.06):
    """One-pole attack/release follower on the gate, so legato notes don't retrigger."""
    out = np.empty_like(gate)
    a = 1 - np.exp(-1 / (attack * SR))
    r = 1 - np.exp(-1 / (release * SR))
    y = 0.0
    # vectorise in chunks of constant gate
    idx = np.flatnonzero(np.diff(gate)) + 1
    bounds = np.concatenate([[0], idx, [len(gate)]])
    for s, e in zip(bounds[:-1], bounds[1:]):
        target = gate[s]
        coef = a if target > y else r
        k = np.arange(1, e - s + 1)
        seg = target + (y - target) * (1 - coef) ** k
        out[s:e] = seg
        y = seg[-1] if len(seg) else y
    return out


def note_on_env(gate, decay=0.15):
    """Rises to 1 at every new gate-on, decays; used for the filter 'pluck'."""
    n = len(gate)
    on = np.flatnonzero((gate[1:] > 0) & (gate[:-1] == 0)) + 1
    if gate[0] > 0:
        on = np.concatenate([[0], on])
    env = np.zeros(n)
    t = np.arange(n) / SR
    for i, o in enumerate(on):
        end = on[i + 1] if i + 1 < len(on) else n
        env[o:end] = np.exp(-(t[o:end] - t[o]) / decay)
    return env


def stack(freq, detunes, n):
    x = np.zeros(n)
    for d in detunes:
        x += saw(freq * 2 ** (d / 1200.0), phase0=RNG_PH.uniform())
    return x / len(detunes)


def two_band_glue(x):
    lo = bfilt(x, 'lowpass', 1500, 2)
    hi = x - lo
    lo = board(lo, [pb.Compressor(threshold_db=-22, ratio=4, attack_ms=6, release_ms=90), pb.Gain(5)])
    hi = board(hi, [pb.Compressor(threshold_db=-26, ratio=5, attack_ms=3, release_ms=60), pb.Gain(4)])
    return lo + hi


def render(notes, dur, patch='classic', bpm=128.0, dive=None):
    """notes: (start_s, dur_s, midi, vel, legato). Returns stereo float array."""
    P = PATCHES[patch]
    n = int(round(dur * SR))
    midi, gate, vel = pitch_track(notes, n, P['glide'])
    if dive:  # (start_s, semitones, seconds): exponential pitch dive (speed-garage drop)
        s0 = int(dive[0] * SR)
        k = np.arange(n - s0) / SR
        midi[s0:] += dive[1] * (1 - np.exp(-k / (dive[2] / 3)))
    f = 440.0 * 2 ** ((midi - 69) / 12)
    env = amp_env(gate) * (0.55 + 0.45 * vel)
    t = np.arange(n) / SR
    # --- mids: two sides + shared centre
    c = stack(f, DETUNE_C, n)
    L = 0.75 * stack(f, DETUNE_L, n) + 0.25 * c
    R = 0.75 * stack(f, DETUNE_R, n) + 0.25 * c
    L, R = np.tanh(1.6 * L), np.tanh(1.6 * R)  # pre-drive feeds the filter extra harmonics
    if P['lfo'] is None:
        cut = P['cut']
        pl = note_on_env(gate, 0.12)
        mids = []
        for ch in (L, R):
            fc = cut + P['env'] * pl
            y = svf(svf(ch, fc, 0.7 + P['res'], 'lp'), fc, 0.7, 'lp')
            mids.append(y)
    else:
        cyc, depth, base = P['lfo']
        rate = bpm / 60.0 * cyc
        lfo = 0.5 - 0.5 * np.cos(2 * np.pi * rate * t)
        fc = base * 2 ** (depth * lfo)
        mids = [svf(svf(ch, fc, 0.7 + P['res'], 'lp'), fc, 0.7, 'lp') for ch in (L, R)]
        if patch == 'growl':  # vowel-ish formant sweep for the growl
            form = 500 + 700 * lfo
            mids = [0.6 * m + 0.8 * svf(m, form, 3.0, 'bp') for m in mids]
    mids = np.stack(mids, 1)
    mids = np.tanh(P['drive'] * mids) / np.tanh(P['drive'])
    mids = board(mids, [pb.Phaser(rate_hz=0.35, depth=0.6, centre_frequency_hz=900, feedback=0.25, mix=P['phaser'])])
    mids = bfilt(mids, 'highpass', 140, 2)
    mids = two_band_glue(mids)
    # widen above ~300 Hz only (M/S), keeping the low-mids centred
    M, S = mids.mean(1), (mids[:, 0] - mids[:, 1]) / 2
    S = bfilt(S, 'highpass', 300, 2) * (1.0 + 1.4 * P['width'])
    mids = np.stack([M + S, M - S], 1)
    mids *= env[:, None]
    # --- sub: clean mono sine on the fundamental
    ph = np.cumsum(f / SR)
    sub = np.sin(2 * np.pi * ph) * env * P['sub']
    sub = bfilt(sub, 'lowpass', 120, 2)
    out = 0.85 * mids + sub[:, None] * 0.72
    out = np.tanh(1.15 * out) / np.tanh(1.15)
    M, S = out.mean(1), (out[:, 0] - out[:, 1]) / 2
    S = bfilt(S, 'highpass', 160, 3)  # everything under ~160 Hz stays mono
    out = np.stack([M + S, M - S], 1)
    return fade(out, 0.002, 0.02)


def one_shot(midi, patch='classic', dur=2.0, hold=1.75, bpm=132.0, dive=None):
    return render([(0.0, hold, midi, 112, False)], dur, patch, bpm, dive)
