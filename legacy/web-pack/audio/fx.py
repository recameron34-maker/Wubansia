"""Fills, risers, impacts and transitions. Everything is rendered at a stated BPM and (when pitched) key."""
import numpy as np
from dsp import *
import hits as H

LANE_HIT = {
    'ukg': {'K': 'kick_ukg', 'SN': 'snare_ukg', 'CL': 'clap_ukg', 'RIM': 'rim_ukg', 'CH': 'chat_ukg', 'OH': 'ohat_ukg'},
    'tech': {'K': 'kick_tech', 'SN': 'snare_tech', 'CL': 'clap_tech', 'RIM': 'rim_tech', 'CH': 'chat_tech', 'OH': 'ohat_tech'},
    'sg': {'K': 'kick_sg', 'SN': 'snare_ukg', 'CL': 'clap_ukg', 'RIM': 'rim_ukg', 'CH': 'chat_ukg', 'OH': 'ohat_ukg'},
    'house': {'K': 'kick_house', 'SN': 'snare_house', 'CL': 'clap_house', 'RIM': 'rim_tech', 'CH': 'chat_house', 'OH': 'ohat_house'},
}
SHARED = {'SH': 'shaker', 'TB': 'tamb', 'COH': 'conga_hi', 'CLO': 'conga_lo', 'BH': 'bongo_hi', 'BL': 'bongo_lo',
          'CLV': 'clave', 'WB': 'click', 'RD': 'ride', 'CR': 'crash', 'LT': 'tom_lo', 'MT': 'tom_mid', 'HT': 'tom_hi', 'CB': 'cowbell'}
MIX = {'K': (1.0, 0), 'SN': (0.55, 0), 'CL': (0.62, 0), 'RIM': (0.36, -0.2), 'CH': (0.26, 0.25), 'OH': (0.3, 0.3),
       'SH': (0.22, -0.3), 'TB': (0.22, 0.4), 'COH': (0.42, -0.35), 'CLO': (0.46, -0.2), 'BH': (0.38, 0.3),
       'BL': (0.4, 0.15), 'CLV': (0.3, 0.35), 'WB': (0.34, -0.4), 'RD': (0.2, 0.3), 'CR': (0.35, 0.0),
       'LT': (0.62, -0.3), 'MT': (0.6, 0.0), 'HT': (0.58, 0.3), 'CB': (0.3, 0.25)}

HITS = None


def hits():
    global HITS
    if HITS is None:
        HITS = {k: normalize(v, -1.0) for k, v in H.render_all().items()}
        HITS.update({k: normalize(v, -1.0) for k, v in H.render_more().items()})
    return HITS


def drum_events(pattern, bars, bpm, swing):
    """pattern: lane -> list of bar strings -> list of (time_sec, lane, gain_digit)."""
    st = step_seconds(bpm)
    ev = []
    for lane, bar_strs in pattern.items():
        for b in range(bars):
            s = bar_strs[b % len(bar_strs)].replace(' ', '')
            res = len(s)
            for j, c in enumerate(s):
                if c == '.':
                    continue
                p16 = b * 16 + j * 16 / res
                ev.append((float(warp16(np.array(p16), swing)) * st, lane, int(c)))
    ev.sort()
    return ev


def render_drums(pattern, bars, bpm, swing, kit='ukg', lane_fx=None, tail=0.0, gains=None):
    hs = hits()
    st = step_seconds(bpm)
    length = n_samples(bars * 16 * st + tail)
    lanes = {}
    ev = drum_events(pattern, bars, bpm, swing)
    ch_times = [t for t, l, _ in ev if l == 'CH']
    for t, lane, d in ev:
        name = LANE_HIT[kit].get(lane) or SHARED[lane]
        x = hs[name].copy()
        if lane == 'OH':  # choke on next closed hat
            nxt = [c for c in ch_times if c > t + 1e-4]
            if nxt:
                cut = n_samples(nxt[0] - t)
                if cut < len(x):
                    x = x[:cut + 1]
                    f = min(len(x), n_samples(0.006))
                    x[-f:] *= np.linspace(1, 0, f)
        buf = lanes.setdefault(lane, np.zeros(length))
        g = (gains or {}).get(lane, MIX[lane][0])
        place(buf, x, n_samples(t), g * vel_gain(d))
    out = np.zeros((length, 2))
    for lane, buf in lanes.items():
        y = to_stereo(buf, MIX[lane][1])
        if lane_fx and lane in lane_fx:
            y = trim_to(lane_fx[lane](y), length)
        out += y
    return out


# ---------------- tonal helpers ----------------
def organ(midi_notes, dur, gate=None, bright=1.0):
    n = n_samples(dur)
    gate = gate if gate is not None else dur * 0.6
    x = np.zeros(n)
    for m in midi_notes:
        f = float(midi_hz(m))
        for h, a in [(1, 1.0), (2, 0.7), (3, 0.45 * bright), (4, 0.3 * bright), (6, 0.12 * bright)]:
            if f * h < SR * 0.45:
                x += a * sine(np.full(n, f * h), phase0=np.random.rand())
    x /= max(1, len(midi_notes)) * 2.0
    env = adsr(n, 0.002, 0.18, 0.45, 0.06, gate)
    x = x * env
    x += 0.08 * bfilt(noise(n), 'bandpass', (2000, 6000)) * exp_env(n, 0.004)  # key click
    return x


def saw_chord(midi_notes, dur, gate, cutoff=(4000, 500), q=1.2, detune=0.08):
    n = n_samples(dur)
    x = np.zeros(n)
    for m in midi_notes:
        for d in (-detune, detune):
            x += saw(np.full(n, float(midi_hz(m + d))), phase0=np.random.rand())
    x /= len(midi_notes) * 2
    t = np.arange(n) / SR
    fc = cutoff[1] + (cutoff[0] - cutoff[1]) * np.exp(-t / 0.08)
    x = svf(x, fc, q, 'lp')
    return x * adsr(n, 0.002, 0.2, 0.3, 0.08, gate)


def pluck_bass(m, dur, gate):
    n = n_samples(dur)
    f = float(midi_hz(m))
    x = 0.6 * saw(np.full(n, f)) + 0.6 * sine(np.full(n, f))
    t = np.arange(n) / SR
    x = svf(x, 180 + 1600 * np.exp(-t / 0.06), 1.0, 'lp')
    return sat(x * adsr(n, 0.002, 0.25, 0.6, 0.04, gate), 1.5)


def sub_bass(m, dur, gate):
    n = n_samples(dur)
    f = float(midi_hz(m))
    x = sine(np.full(n, f)) + 0.15 * sine(np.full(n, 2 * f))
    return sat(x * adsr(n, 0.004, 0.3, 0.8, 0.05, gate), 1.3)


def supersaw(freq, n, voices=7, spread=0.18):
    x = np.zeros(n)
    for i in range(voices):
        cents = (i - (voices - 1) / 2) / ((voices - 1) / 2) * spread * 100
        x += saw(freq * 2 ** (cents / 1200), phase0=np.random.rand())
    return x / voices


# ---------------- playback-rate effects ----------------
def varispeed(x, rate, start=0.0):
    """Read stereo x with a per-output-sample rate curve (can go negative). start = initial read index."""
    pos = start + np.concatenate([[0], np.cumsum(rate[:-1])])
    pos = np.clip(pos, 0, len(x) - 2)
    i = np.floor(pos).astype(int)
    fr = (pos - i)[:, None]
    return x[i] * (1 - fr) + x[i + 1] * fr


def crackle(n, density=40, amp=0.12):
    y = np.zeros(n)
    k = int(n / SR * density)
    idx = RNG.integers(0, n, k)
    y[idx] = RNG.uniform(-1, 1, k)
    y = bfilt(y, 'bandpass', (1500, 9000), 1)
    return amp * y / (np.max(np.abs(y)) + 1e-9) + 0.01 * bfilt(noise(n), 'bandpass', (300, 3000), 1)


# ---------------- grooves used inside transition FX ----------------
def groove_ukg01(bars=1, bpm=134):
    """Pirate Signal drums + organ stabs + sub, used for the rewind."""
    sw = 0.60
    st = step_seconds(bpm)
    pat = {'K': ['9.........8.....'], 'CL': ['....9.......9...'], 'RIM': ['...5..4....5..4.'], 'CH': ['5374537453745374']}
    d = render_drums(pat, bars, bpm, sw, 'ukg')
    L = len(d)
    mus = np.zeros(L)
    stab = [56, 60, 63, 67]  # Ab3 C4 Eb4 G4 (Fm9 rootless)
    for b in range(bars):
        for s, ln in [(0, 2), (3, 1), (6, 2), (10, 1), (13, 2)]:
            t = float(warp16(np.array(b * 16 + s), sw)) * st
            place(mus, organ(stab, ln * st + 0.15, ln * st * 0.9) * 0.55, n_samples(t))
        for s, ln, m in [(0, 3, 29), (6, 1, 41), (10, 3, 29), (14, 1, 32)]:
            t = float(warp16(np.array(b * 16 + s), sw)) * st
            place(mus, sub_bass(m, ln * st + 0.05, ln * st * 0.9) * 0.9, n_samples(t))
    return d + to_stereo(mus)


def groove_simple(kit, pat, chord, chord_fn, bass_steps, bpm, swing, bars=1):
    st = step_seconds(bpm)
    d = render_drums(pat, bars, bpm, swing, kit)
    mus = np.zeros(len(d))
    for b in range(bars):
        if chord:
            for s, ln in chord[1]:
                t = float(warp16(np.array(b * 16 + s), swing)) * st
                place(mus, chord_fn(chord[0], ln * st + 0.15, ln * st * 0.9) * 0.5, n_samples(t))
        for s, ln, m in bass_steps:
            t = float(warp16(np.array(b * 16 + s), swing)) * st
            place(mus, pluck_bass(m, ln * st + 0.05, ln * st * 0.85) * 0.8, n_samples(t))
    return d + to_stereo(mus)


# ---------------- individual FX ----------------
def noise_riser(bars, bpm, f0=300, f1=9500, q=1.6):
    n = n_samples(bars * 4 * 60 / bpm)
    t = np.linspace(0, 1, n)
    fc = f0 * (f1 / f0) ** (t ** 1.6)
    nz = noise(n)
    a = svf(nz, fc, q, 'bp')
    b = svf(noise(n), fc * 1.07, q, 'bp')
    amp = (t ** 1.8)[:, None]
    x = np.stack([a, b], 1) * amp
    x = reverb(x, 0.55, 0.25, 0.4, 1.0, 1.0)[:n]
    return fade(x, 0.01, 0.004)


def pitch_riser(bars, bpm, root_midi, octaves=2):
    n = n_samples(bars * 4 * 60 / bpm)
    t = np.linspace(0, 1, n)
    semis = 12 * octaves * (t ** 1.4)
    f = midi_hz(root_midi + semis)
    l = supersaw(f, n, 7, 0.22)
    r = supersaw(f * 1.003, n, 7, 0.22)
    x = np.stack([l, r], 1)
    x = svf_stereo(x, 400 + 9000 * t ** 1.3, 0.9, 'lp')
    nz = noise(n) * 0.25
    nz = svf(nz, 500 + 9000 * t ** 1.5, 1.2, 'bp')
    x = (x + to_stereo(nz)) * (0.15 + 0.85 * t ** 1.6)[:, None]
    x = reverb(x, 0.6, 0.2, 0.5, 1.0, 1.0)[:n]
    return fade(x, 0.01, 0.004)


def downsweep(bars, bpm):
    n = n_samples(bars * 4 * 60 / bpm)
    t = np.linspace(0, 1, n)
    fc = 12000 * (200 / 12000) ** (t ** 0.7)
    x = np.stack([svf(noise(n), fc, 1.4, 'bp'), svf(noise(n), fc * 0.95, 1.4, 'bp')], 1)
    whoosh = sine(midi_hz(84 - 48 * t ** 0.6)) * 0.25
    x = (x + to_stereo(whoosh)) * ((1 - t) ** 1.3)[:, None] * np.minimum(1, t * 40)[:, None]
    return fade(reverb(x, 0.7, 0.3, 0.5, 1.0, 1.0)[:n], 0.002, 0.05)


def sub_drop(bpm, root_midi, bars=1):
    n = n_samples(bars * 4 * 60 / bpm)
    t = np.linspace(0, 1, n)
    f = midi_hz(root_midi) * (0.25 ** (t ** 0.8))
    x = sine(f) * (1 - t) ** 0.6
    x = sat(x * 1.2, 1.6)
    hit = hits()['kick_ukg_boom'][:n] * 0.6
    x[:len(hit)] += hit
    return fade(x, 0, 0.03)


def impact(bpm, root_midi, bars=2):
    n = n_samples(bars * 4 * 60 / bpm)
    boom = H.sub_boom(float(midi_hz(root_midi)), float(midi_hz(root_midi + 12)), n / SR, 0.9)
    nz = bfilt(noise(n), 'lowpass', 6000) * exp_env(n, 0.25, 0.001) * 0.5
    cr = trim_to(hits()['crash'], n) * 0.45
    x = to_stereo(boom * 0.9) + reverb(nz + cr, 0.75, 0.35, 0.4, 1.0, 1.0)[:n]
    return fade(x, 0, 0.1)


def reverse_crash(bpm, bars=1):
    n = n_samples(bars * 4 * 60 / bpm)
    cr = reverb(hits()['crash'], 0.8, 0.5, 0.3, 1.0, 1.0)
    x = trim_to(cr, n)[::-1].copy()
    t = np.linspace(0, 1, n)
    x *= (t ** 1.2)[:, None]
    return fade(x, 0.02, 0.004)


def reverse_snare_swell(bpm, kit='ukg'):
    st = step_seconds(bpm)
    n = n_samples(16 * st)
    sn = hits()['snare_ukg' if kit == 'ukg' else 'snare_tech']
    wet = reverb(np.concatenate([sn, np.zeros(n_samples(1.5))]), 0.85, 0.9, 0.25, 1.0, 0.3)
    sw = wet[::-1][-n_samples(8 * st):]
    pat = {'K': ['9.........8.....'], 'CL': ['....9...........'], 'CH': ['5374537453......'], 'RIM': ['...5..4.........']}
    d = render_drums(pat, 1, bpm, 0.59, kit)
    out = d.copy()
    out[n - len(sw):] += sw * 0.9
    return fade(out, 0, 0.004)


def rewind_fx(bpm):
    st = step_seconds(bpm)
    g = groove_ukg01(2, bpm)
    n = n_samples(16 * st)
    half = n_samples(8 * st)
    rate = np.ones(n)
    tt = np.linspace(0, 1, n - half)
    rate[half:] = 1 - 4.2 * np.sin(np.pi / 2 * tt) ** 0.7  # pull back hard
    y = varispeed(g, rate, start=0)
    tail = np.linspace(1, 0, n - half) ** 0.5
    y[half:] *= tail[:, None]
    y[half:] = svf_stereo(y[half:], 9000 * (0.15 ** tt) + 400, 0.8, 'lp')
    scr = bfilt(noise(n - half), 'bandpass', (800, 4000)) * np.abs(np.sin(2 * np.pi * 9 * tt)) * 0.2 * tail
    y[half:] += to_stereo(scr)
    return fade(y, 0, 0.004)


def stop_fx(groove, bpm, crackle_amt=0.0, sharp=2.2):
    st = step_seconds(bpm)
    n = n_samples(16 * st)
    half = n_samples(8 * st)
    rate = np.ones(n)
    tt = np.linspace(0, 1, n - half)
    rate[half:] = np.maximum(0, (1 - tt) ** sharp)
    y = varispeed(groove, rate)
    y[half:] = svf_stereo(y[half:], 12000 * (0.06 ** tt) + 150, 0.7, 'lp')
    y[half:] *= (1 - tt ** 3)[:, None]
    if crackle_amt:
        y += to_stereo(crackle(n, 35, crackle_amt))
    return fade(y, 0, 0.004)


def dub_siren(bpm, base_midi, bars=2):
    st = step_seconds(bpm)
    n = n_samples(bars * 16 * st)
    t = np.arange(n) / SR
    cycle = 2 * st  # one 8th note per whoop
    ph = (t / cycle) % 1.0
    semis = 12 * ph ** 0.5
    env_on = (t < n / SR * 0.62).astype(float)
    f = midi_hz(base_midi + semis)
    x = 0.6 * square(f, pw=0.5) + 0.4 * saw(f * 1.004)
    x = svf(x, 2600, 1.0, 'lp') * env_on * 0.6
    x = pingpong(x, 3 * st, 0.45, 0.45, 3500, 10)
    return fade(reverb(x, 0.5, 0.18, 0.5, 1.0, 1.0)[:n], 0.005, 0.05)


def laser_zaps(bpm, end_midi, swing=0.6):
    st = step_seconds(bpm)
    n = n_samples(16 * st)
    out = np.zeros(n)
    pattern = '9..6..9..6.9.6..'
    for j, c in enumerate(pattern):
        if c == '.':
            continue
        zn = n_samples(0.11)
        tz = np.arange(zn) / SR
        f = float(midi_hz(end_midi)) + (2600 - float(midi_hz(end_midi))) * np.exp(-tz / 0.018)
        z = square(f, pw=0.35) * exp_env(zn, 0.045)
        z = bfilt(z, 'lowpass', 7000, 1)
        place(out, z, n_samples(float(warp16(np.array(j), swing)) * st), vel_gain(int(c)))
    x = pingpong(out, 3 * st, 0.35, 0.3, 4000, 6)
    return fade(trim_to(reverb(x, 0.4, 0.12, 0.5, 1.0, 1.0), n), 0, 0.01)


def stab_echo(bpm, notes, kind='organ', bars=2, fb=0.5):
    st = step_seconds(bpm)
    n = n_samples(bars * 16 * st)
    if kind == 'organ':
        s = organ(notes, 2.5 * st, 1.8 * st)
    else:
        s = saw_chord(notes, 3 * st, 1.5 * st, (3200, 400), 1.6)
    x = np.zeros(n)
    place(x, s, 0, 1.0)
    y = pingpong(x, 3 * st, fb, 0.55, 2400, 12)
    return fade(trim_to(reverb(y, 0.6, 0.2, 0.5, 1.0, 1.0), n), 0, 0.05)


def glitch_stutter(groove, bpm):
    st = step_seconds(bpm)
    n = n_samples(16 * st)
    beat = n_samples(4 * st)
    seg = groove[:beat]
    out = np.zeros((n, 2))
    pos = 0
    plan = [(4, 1), (2, 2), (1, 4), (0.5, 8)]  # (slice length in 16ths, repeats) -> totals 4+4+4+4 = 16
    for k, (ln, reps) in enumerate(plan):
        L = n_samples(ln * st)
        sl = seg[:L].copy()
        f = min(L // 4, n_samples(0.003))
        if f > 0:
            sl[:f] *= np.linspace(0, 1, f)[:, None]
            sl[-f:] *= np.linspace(1, 0, f)[:, None]
        for r in range(reps):
            # pitch up a little as it accelerates
            semis = k * 1.5 + r * 0.25
            rate = 2 ** (semis / 12)
            idx = np.clip((np.arange(L) * rate).astype(int), 0, L - 1)
            place(out, sl[idx], pos, 1.0)
            pos += L
    return fade(out, 0, 0.004)
