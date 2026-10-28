"""Wave 8: groove house (in the spirit of Chris Stussy) and tech house (in the spirit of Chris Lake and Riordan).
Original synthesis only, nothing sampled from anyone's records.

Groove house: a bouncy, warm kit with a clap drowned in reverb, vintage swung hats and glitchy ticks; a two-operator
FM "rubber" bass, a glowing saw chord voice with a phaser and a long release, and a Juno-style lead that plays in fifths.
Tech house: a fat punchy kick with a resonant bump, a hard distorted snare, a layered clap, driving hats, offbeat toms; a
plucky bounce bass (left bright and sustained so the player's resonant filter envelope does the plucking), a Moog-style
fifth bass, a rave stab (minor chord memory) and a crushed Reese.

Writes WAVs into lib/Samples/{GrooveHouse,TechHouse}/... and a sidecar inst/w8_meta.json (roots, loop points, notes)."""
import os as _os  # noqa: E402
PACK_ROOT = _os.environ.get('PACK_ROOT') or _os.path.abspath(_os.path.join(_os.path.dirname(_os.path.abspath(__file__)), '..'))  # the legacy web-pack folder
import json, os, sys
import numpy as np
import pedalboard as pb

sys.path.insert(0, PACK_ROOT + '/audio')
sys.path.insert(0, PACK_ROOT + '/inst')
from dsp import SR, bfilt, noise, exp_env, n_samples, sat, saw, square, sine, board  # noqa: E402
import hits as H  # noqa: E402
from render_inst import per_phase, polyblep, saw_per, circ_lp, loop_len, write_wav, rms, seam_db, nname, m2f  # noqa: E402

LIB = PACK_ROOT + '/inst/lib'
META = {}
rng = np.random.default_rng(20261027)


def save(sid, rel, x, root=None, loop=None, **extra):
    write_wav(os.path.join(LIB, rel), x, root, loop)
    META[sid] = dict(path=rel, root=root, frames=int(x.shape[0]), ch=1 if x.ndim == 1 else 2, loop=loop, peak=round(float(np.abs(x).max()), 3), **extra)
    if loop:
        xs = x if x.ndim == 1 else x[:, 0]
        META[sid]['seam'] = round(seam_db(xs, loop[0], loop[1]), 2)


def norm(x, peak=0.89):
    return x / (np.abs(x).max() + 1e-12) * peak


def fade_out(x, sec):
    n = int(sec * SR)
    x = x.copy()
    ramp = np.linspace(1, 0, n) ** 2
    x[-n:] *= ramp[:, None] if x.ndim == 2 else ramp
    return x


def fade_in(x, sec):
    n = max(1, int(sec * SR))
    x = x.copy()
    ramp = np.linspace(0, 1, n)
    x[:n] *= ramp[:, None] if x.ndim == 2 else ramp
    return x


def stereo(x):
    return np.stack([x, x], 1) if x.ndim == 1 else x


def ladder(x, cutoff, res=0.3, drive=1.0, mode='lp24', block=32):
    """Moog-style ladder filter (pedalboard) with a time-varying cutoff, updated every `block` samples.
    cutoff: a function of time in seconds (vectorised), or a number."""
    M = {'lp24': pb.LadderFilter.Mode.LPF24, 'lp12': pb.LadderFilter.Mode.LPF12, 'bp12': pb.LadderFilter.Mode.BPF12, 'hp12': pb.LadderFilter.Mode.HPF12}[mode]
    mono = x.ndim == 1
    X = (x[None, :] if mono else x.T).astype(np.float32)
    lf = pb.LadderFilter(mode=M, cutoff_hz=1000.0, resonance=res, drive=drive)
    n = X.shape[1]
    out = []
    cf = cutoff if callable(cutoff) else (lambda t, c=cutoff: np.full_like(t, c))
    cuts = cf(np.arange(0, n, block) / SR)
    for j, i in enumerate(range(0, n, block)):
        lf.cutoff_hz = float(np.clip(cuts[j], 20, 20000))
        out.append(lf.process(X[:, i:i + block], SR, reset=(i == 0)))
    y = np.concatenate(out, 1).astype(float)
    return y[0] if mono else y.T


def mod_delay(x, base_ms, depth_ms, rate, phase=0.0):
    """Delay line with a triangle-modulated delay time (a BBD chorus voice), linear interpolation."""
    n = len(x)
    t = np.arange(n) / SR
    tri = 2 * np.abs(2 * ((rate * t + phase) % 1.0) - 1) - 1
    d = (base_ms + depth_ms * tri) * SR / 1000
    idx = np.arange(n) - d
    i0 = np.floor(idx).astype(int)
    fr = idx - i0
    i0c = np.clip(i0, 0, n - 1)
    i1c = np.clip(i0 + 1, 0, n - 1)
    y = x[i0c] * (1 - fr) + x[i1c] * fr
    y[i0 < 0] = 0
    return y


def juno_chorus(x, mix=0.5, rate=0.52, depth=1.6, base=3.4):
    """Juno-60 style chorus: one BBD line per side, modulated in opposite phase."""
    L = (1 - mix) * x + mix * mod_delay(x, base, depth, rate, 0.0)
    R = (1 - mix) * x + mix * mod_delay(x, base, depth, rate, 0.5)
    return np.stack([L, R], 1)


def room(x, size=0.25, wet=0.18, damp=0.5, width=0.7, dry=1.0):
    X = stereo(x).T.astype(np.float32)
    y = pb.Pedalboard([pb.Reverb(room_size=size, damping=damp, wet_level=wet, dry_level=dry, width=width)])(X, SR)
    return y.T.astype(float)


def phase_of(freq):
    """Phase (radians) of an oscillator following a frequency curve."""
    return 2 * np.pi * np.cumsum(freq) / SR


def pulse(f, n, pw, phase0=0.0):
    """Band-limited pulse from two polyBLEP saws; pw can vary per sample."""
    inc = np.broadcast_to(np.asarray(f, float), (n,)) / SR
    ph = np.cumsum(inc) + phase0
    p1 = ph % 1.0
    p2 = (ph + pw) % 1.0
    s1 = 2 * p1 - 1 - polyblep(p1, inc)
    s2 = 2 * p2 - 1 - polyblep(p2, inc)
    return s1 - s2


def sawv(f, n, phase0=0.0):
    inc = np.broadcast_to(np.asarray(f, float), (n,)) / SR
    p = (np.cumsum(inc) + phase0) % 1.0
    return 2 * p - 1 - polyblep(p, inc)


# ====================================================================== GROOVE HOUSE (Stussy-inspired) kit
def gv_kit():
    hits = {}
    n = n_samples(0.42)
    t = np.arange(n) / SR
    k = H.kick(49.0, f_start=185, tau_p=0.03, tau_a=0.125, dur=0.42, click=0.24, drive=1.5, harm=0.12)
    thump = np.sin(phase_of(118 * (1 + 0.45 * np.exp(-t / 0.01)))) * np.exp(-t / 0.04) * 0.3    # the bounce: a quick low-mid knock
    x = sat((k + thump) * 0.95, 1.15)
    x = board(x, [pb.LowpassFilter(9500)])
    hits['kick_gv'] = ('Kick-GrooveHouse-Bouncy_G', 'Kick Groove House Bouncy', 'G', 'warm, bouncy kick with a low-mid knock, tuned to G', fade_out(norm(x, 0.92), 0.02))

    c = H.clap(spread=(0, 0.007, 0.016, 0.027), tail_tau=0.06, band=(950, 3000), room=0.0, dur=1.9)
    pre = np.concatenate([np.zeros(int(0.018 * SR)), c])[:len(c)]
    wet = board(stereo(pre), [pb.Reverb(room_size=0.84, damping=0.62, wet_level=1.0, dry_level=0.0, width=0.95),
                              pb.HighpassFilter(380), pb.LowpassFilter(7200)])
    x = stereo(c) + 0.5 * wet
    hits['clap_gv'] = ('Clap-GrooveHouse-BigVerb', 'Clap Groove House Big Reverb', None, 'four-layer clap into a long, dark hall', fade_out(norm(x, 0.86), 0.25))

    s = H.snare(t1=210, t2=360, tone_tau=0.04, noise_tau=0.07, hp=2200, lp=12000, tone_amt=0.45, noise_amt=0.85, dur=0.3, drive=1.6)
    x = room(s, 0.2, 0.14, 0.5, 0.5)
    hits['snare_gv'] = ('Snare-GrooveHouse-Snappy', 'Snare Groove House Snappy', None, 'snappy snare with a short plate', fade_out(norm(x, 0.84), 0.03))

    r = H.rim(f1=1350.0, f2=440.0, tau=0.011, dur=0.14, drive=1.6)
    r = board(r, [pb.LowpassFilter(9000)])
    hits['rim_gv'] = ('Rim-GrooveHouse-Wood', 'Rim Groove House Wood', None, 'woody side-stick', fade_out(norm(r, 0.8), 0.01))

    def vintage(h):     # a little grit and 12-bit edge, like an old drum machine through a mixer
        h = 0.8 * h + 0.2 * board(h, [pb.Bitcrush(bit_depth=10)])
        return sat(h * 1.3, 1.2)
    ch = vintage(H.hat(0.03, 0.14, scale=1.05, metal_amt=0.55, noise_amt=0.7, band=(7000, 13500), hp=7200))
    hits['chat_gv'] = ('ClosedHat-GrooveHouse-Vintage', 'Closed Hat Groove House Vintage', None, 'crisp closed hat with a vintage edge', fade_out(norm(ch, 0.74), 0.01))
    oh = vintage(H.hat(0.17, 0.5, scale=1.05, metal_amt=0.55, noise_amt=0.65, band=(6800, 13000), hp=7000))
    hits['ohat_gv'] = ('OpenHat-GrooveHouse-Vintage', 'Open Hat Groove House Vintage', None, 'short open hat, same machine', fade_out(norm(oh, 0.74), 0.03))
    sh = board(H.shaker(dur=0.16, attack=0.01, tau=0.032), [pb.LowpassFilter(11000)])
    hits['shaker_gv'] = ('Shaker-GrooveHouse', 'Shaker Groove House', None, 'soft shaker for swung 16ths', fade_out(norm(sh, 0.7), 0.01))

    n = n_samples(0.12)
    t = np.arange(n) / SR
    fc = 1650 * (1 + 0.35 * np.exp(-t / 0.006))
    I = 3.0 * np.exp(-t / 0.012)
    tick = np.sin(phase_of(fc) + I * np.sin(phase_of(fc * 1.48))) * np.exp(-t / 0.025)
    tick += 0.25 * bfilt(noise(n), 'highpass', 5000) * np.exp(-t / 0.0015)
    hits['perc_gv'] = ('Perc-GrooveHouse-GlitchTick', 'Perc Groove House Glitch Tick', None, 'FM tick for minimal percussion', fade_out(norm(tick, 0.75), 0.01))
    for key, (fname, title, k_, note, x) in hits.items():
        save(key, f'Samples/Drums/GrooveHouse/GV_{fname}.wav', x, None, None, title=title, key=k_, note=note)
    return {k: v[4] for k, v in hits.items()}


# ====================================================================== TECH HOUSE (Lake / Riordan-inspired) kit
def th_kit():
    hits = {}
    k = H.kick(52.0, f_start=230, tau_p=0.024, tau_a=0.13, dur=0.38, click=0.42, drive=2.8, harm=0.18)
    y = board(k, [pb.HighpassFilter(28), pb.PeakFilter(105, 4.5, 1.1), pb.PeakFilter(3200, 2.0, 0.8)])
    y = 0.95 * y + 0.05 * sat(y * 4, 3)          # a touch of grit, mixed low
    hits['kick_th'] = ('Kick-TechHouse-Punch_Ab', 'Kick Tech House Punch', 'Ab', 'fat, punchy kick with a resonant bump and a little grit; short sub, so the bass owns the low end', fade_out(norm(y, 0.93), 0.015))

    s = H.snare(t1=205, t2=355, tone_tau=0.05, noise_tau=0.085, hp=1600, lp=13000, tone_amt=0.6, noise_amt=0.9, dur=0.34, drive=2.2)
    body = H.drum_tone(230, tau=0.05, bend=0.3, dur=0.34, slap=0.1) * 0.25
    x = s + body[:len(s)]
    x = np.tanh(3 * (x + 0.15 * x ** 2)) / np.tanh(3)                    # trashed, asymmetric
    a = int(0.008 * SR)
    x[:a] *= np.linspace(1.35, 1.0, a)                                    # transient lift
    x = board(x, [pb.HighpassFilter(180), pb.PeakFilter(420, 3.0, 1.0), pb.HighShelfFilter(7000, 3.0)])
    x = room(x, 0.24, 0.17, 0.4, 0.6)
    hits['snare_th'] = ('Snare-TechHouse-Hard', 'Snare Tech House Hard', None, 'tight, hard snare, distorted, with a short room', fade_out(norm(x, 0.88), 0.03))

    c1 = H.clap(spread=(0, 0.006, 0.013, 0.021), tail_tau=0.05, band=(1100, 3200), room=0.0, dur=0.6)
    c2 = H.clap(spread=(0, 0.009, 0.02, 0.031), tail_tau=0.07, band=(800, 2400), room=0.0, dur=0.6)
    c2 = np.concatenate([np.zeros(int(0.002 * SR)), c2])[:len(c1)]
    x = np.stack([0.62 * c1 + 0.45 * c2, 0.45 * c1 + 0.62 * c2], 1)
    x = room(x, 0.35, 0.2, 0.5, 0.8)
    hits['clap_th'] = ('Clap-TechHouse-Layered', 'Clap Tech House Layered', None, 'two claps layered wide, short room', fade_out(norm(x, 0.86), 0.03))

    r = H.rim(f1=1950.0, f2=560.0, tau=0.006, dur=0.1, drive=2.4)
    hits['rim_th'] = ('Rim-TechHouse-Bright', 'Rim Tech House Bright', None, 'bright rim for offbeat top loops', fade_out(norm(r, 0.8), 0.01))

    n = n_samples(0.12)
    t = np.arange(n) / SR
    stick = bfilt(noise(n), 'bandpass', (2800, 6500), 2) * np.exp(-t / 0.0025) * 0.6
    ch = H.hat(0.022, 0.12, scale=1.1, metal_amt=0.45, noise_amt=0.9, band=(7500, 14500), hp=7800) + stick
    hits['chat_th'] = ('ClosedHat-TechHouse-Driving', 'Closed Hat Tech House Driving', None, 'tight, noisy closed hat with a stick click', fade_out(norm(ch, 0.76), 0.01))
    oh = H.hat(0.12, 0.36, scale=1.1, metal_amt=0.5, noise_amt=0.85, band=(7000, 14000), hp=7400)
    hits['ohat_th'] = ('OpenHat-TechHouse-Short', 'Open Hat Tech House Short', None, 'short open hat', fade_out(norm(oh, 0.76), 0.02))

    n = n_samples(0.2)
    t = np.arange(n) / SR
    g1 = np.exp(-t / 0.018) * np.clip(t / 0.006, 0, 1)
    t2 = np.clip(t - 0.034, 0, None)
    g2 = np.where(t > 0.034, np.exp(-t2 / 0.024) * np.clip(t2 / 0.004, 0, 1), 0) * 0.8
    sh = bfilt(noise(n), 'bandpass', (3500, 9500), 2) * (g1 + g2)
    hits['shaker_th'] = ('Shaker-TechHouse-Acoustic', 'Shaker Tech House Acoustic', None, 'two-grain shaker (forward and back)', fade_out(norm(sh, 0.7), 0.01))

    for key, fc, nm in (('tom_th_lo', 95, 'Low_Gb'), ('tom_th_mid', 135, 'Mid_Db'), ('tom_th_hi', 182, 'High_Gb')):
        tm = H.drum_tone(fc, tau=0.085, bend=0.5, bend_tau=0.018, dur=0.42, slap=0.3, partial=0.25)
        tm = room(sat(tm * 1.1, 1.2), 0.22, 0.12, 0.5, 0.5)
        hits[key] = (f'Tom-TechHouse-{nm}', 'Tom Tech House ' + nm.split('_')[0], nm.split('_')[1], 'tuned tom with a bit of drive, for offbeat tom grooves', fade_out(norm(tm, 0.82), 0.03))

    n = n_samples(0.18)
    t = np.arange(n) / SR
    ping = np.sin(phase_of(np.full(n, 1180.0)) + 2.2 * np.exp(-t / 0.02) * np.sin(phase_of(np.full(n, 1180.0 * 3.17)))) * np.exp(-t / 0.045)
    ping += 0.2 * bfilt(noise(n), 'highpass', 4000) * np.exp(-t / 0.002)
    hits['perc_th'] = ('Perc-TechHouse-MetalPing', 'Perc Tech House Metal Ping', None, 'short metallic FM ping', fade_out(norm(ping, 0.75), 0.01))
    for key, (fname, title, k_, note, x) in hits.items():
        save(key, f'Samples/Drums/TechHouse/TH_{fname}.wav', x, None, None, title=title, key=k_, note=note)
    return {k: v[4] for k, v in hits.items()}


# ====================================================================== instruments
def fm_bass(m, dur=1.4):
    """Two-operator FM 'rubber' bass, after the 90s FM house basses: a sine carrier, a self-modulating modulator at 1:1 whose
    index falls fast (the pluck), a 25-cent pitch drop at the start, warm asymmetric saturation."""
    f = m2f(m)
    n = n_samples(dur)
    t = np.arange(n) / SR
    fp = f * 2 ** ((25 / 1200) * np.exp(-t / 0.025))
    pc = phase_of(fp)
    I0 = 3.2 * (1 + max(0, 48 - m) / 24)                 # low notes get a bigger index, so they still have bite
    I = 0.55 + I0 * np.exp(-t / 0.075)
    mod = np.sin(pc + 0.7 * np.sin(pc))                   # one-step feedback: a saw-ish modulator
    x = np.sin(pc + I * mod)
    x += 0.16 * np.sin(2 * pc + 0.5 * I * mod) * np.exp(-t / 0.05)
    amp = (0.52 + 0.48 * np.exp(-t / 0.11)) * np.exp(-t / 1.9)
    a = int(0.0015 * SR)
    amp[:a] *= np.linspace(0, 1, a)
    x = x * amp
    x = np.tanh(1.7 * (x + 0.12 * x ** 2)) / np.tanh(1.7)
    x = x - bfilt(x, 'lowpass', 12, 1)                   # no DC from the asymmetry
    x = bfilt(x, 'lowpass', 4800, 2)
    return fade_out(x, 0.09)


def glow(m, dur=2.6):
    """Glow Saw: two saws three cents apart, a breath of noise, a ladder low-pass that closes after the hit, a long
    release, and a slow phaser that runs at a different rate on each side."""
    f = m2f(m)
    n = n_samples(dur)
    t = np.arange(n) / SR
    s = sawv(f, n, rng.uniform()) + 0.8 * sawv(f * 2 ** (3 / 1200), n, rng.uniform())
    s += 0.05 * bfilt(noise(n), 'bandpass', (2000, 8000), 2)
    amp = np.where(t < 0.08, 1.0, 0.0) + np.where(t >= 0.08, 0.38 + 0.62 * np.exp(-(t - 0.08) / 0.3), 0.0)
    amp *= np.where(t < 0.55, 1.0, np.exp(-(t - 0.55) / 0.5))
    amp *= np.clip(t / 0.004, 0, 1)
    top = min(1.0, f / 180.0)
    x = ladder(s * amp * 0.5, lambda tt: 700 + 5200 * top * np.exp(-tt / 0.2) + 900 * top, res=0.18, drive=1.1, mode='lp24')
    L = board(x, [pb.Phaser(rate_hz=0.35, depth=0.6, centre_frequency_hz=900, feedback=0.25, mix=0.35)])
    R = board(x, [pb.Phaser(rate_hz=0.43, depth=0.6, centre_frequency_hz=1100, feedback=0.25, mix=0.35)])
    return fade_out(np.stack([L, R], 1), 0.15)


def fifth_lead(m, dur=2.2):
    """Juno-style lead that sounds the note and its fifth: a PWM pulse and a saw per voice, each voice slowly drifting a
    few cents, a plucky ladder low-pass, and a Juno chorus (one BBD line per side, opposite phase)."""
    f = m2f(m)
    n = n_samples(dur)
    t = np.arange(n) / SR
    out = np.zeros(n)
    for ratio, amp_, rate, ph in ((1.0, 1.0, 0.23, 0.0), (2 ** (7 / 12), 0.8, 0.31, 0.4)):
        drift = 2 ** ((5 / 1200) * np.sin(2 * np.pi * rate * t + ph * 6.28))
        fv = f * ratio * drift
        pw = 0.5 + 0.13 * np.sin(2 * np.pi * 0.8 * t + ph * 3)
        out += amp_ * (0.6 * pulse(fv, n, pw, rng.uniform()) + 0.4 * sawv(fv, n, rng.uniform()))
    env = 0.7 + 0.3 * np.exp(-t / 0.3)
    env *= np.where(t < 1.6, 1.0, np.exp(-(t - 1.6) / 0.22))
    env *= np.clip(t / 0.006, 0, 1)
    x = ladder(out * env * 0.35, lambda tt: 2600 + 3600 * np.exp(-tt / 0.12), res=0.22, drive=1.2)
    return fade_out(juno_chorus(x, 0.32, rate=0.5, depth=1.1, base=3.6), 0.12)


def bounce_bass(m, T=1.0):
    """Bounce Bass, left bright and sustaining (an exact loop): a saw and a square one cycle apart per loop, a sine sub,
    asymmetric distortion and a little 6-bit grit for upper harmonics. The player's resonant low-pass envelope (deep and
    plucky, opening further on hard-hit notes) does the bouncing."""
    f = m2f(m)
    N, L = loop_len(f, T)
    k = np.arange(L)
    sw = saw_per(N, L, L, 0.0)
    ph, inc = per_phase(N + 1, L, L, 0.37)
    p = ph % 1.0
    sq = np.where(p < 0.5, 1.0, -1.0) + polyblep(p, inc) - polyblep((p + 0.5) % 1.0, inc)
    sub = np.sin(2 * np.pi * N * k / L)
    x = 0.7 * sw + 0.42 * sq + 0.75 * sub
    x = np.tanh(2.0 * (x + 0.25 * x ** 2)) / np.tanh(2.0)
    q = np.round(x * 32) / 32
    x = 0.86 * x + 0.14 * q
    x = circ_lp(x - x.mean(), 7500, 2)
    return x, L


def fifth_bass(m, dur=0.95):
    """Moog-style fifth bass: saws on the note and its fifth over a square an octave down, a resonant ladder low-pass
    with a short envelope, a slow detune drift, and a touch of bit-crush."""
    f = m2f(m)
    n = n_samples(dur)
    t = np.arange(n) / SR
    d1 = 2 ** ((4 / 1200) * np.sin(2 * np.pi * 0.9 * t))
    x = sawv(f * d1, n, rng.uniform()) + 0.55 * sawv(f * 2 ** (7 / 12) / d1, n, rng.uniform())
    x += 0.5 * pulse(np.full(n, f / 2 if f / 2 >= 30 else f), n, 0.5, rng.uniform())   # the octave below, unless that's subsonic
    amp = (0.22 + 0.78 * np.exp(-t / 0.3)) * np.clip(t / 0.002, 0, 1)
    y = ladder(x * amp * 0.4, lambda tt: 170 + 3300 * np.exp(-tt / 0.065), res=0.62, drive=1.6)
    y = 0.75 * y + 0.25 * board(y, [pb.Bitcrush(bit_depth=9)])
    y = y - bfilt(y, 'lowpass', 15, 1)
    return fade_out(y, 0.08)


def rave_stab(m, dur=0.9):
    """Rave stab: a minor chord with the octave (chord memory: every key plays a minor chord), three detuned saws per
    tone panned across the field, a square sub, a noise click, a snapping ladder envelope and a small room."""
    f = m2f(m)
    n = n_samples(dur)
    t = np.arange(n) / SR
    Lc, Rc = np.zeros(n), np.zeros(n)
    for semi, a in ((0, 1.0), (3, 0.8), (7, 0.85), (12, 0.6)):
        for det, pan in ((-12, -0.7), (0, 0.0), (12, 0.7)):
            v = a * sawv(f * 2 ** ((semi * 100 + det) / 1200), n, rng.uniform())
            Lc += v * np.sqrt(0.5 - 0.5 * pan)
            Rc += v * np.sqrt(0.5 + 0.5 * pan)
    sub = 0.5 * pulse(np.full(n, f / 2), n, 0.5, 0.0)
    amp = (0.25 * np.exp(-t / 0.25) + 0.75 * np.exp(-t / 0.11)) * np.clip(t / 0.0012, 0, 1)
    X = np.stack([(Lc + sub) * amp, (Rc + sub) * amp], 1) * 0.12
    X[:, 0] += 0.15 * bfilt(noise(n), 'bandpass', (2000, 7000), 2) * np.exp(-t / 0.01)
    X[:, 1] += 0.15 * bfilt(noise(n), 'bandpass', (2000, 7000), 2) * np.exp(-t / 0.01)
    Y = ladder(X, lambda tt: 650 + 7200 * np.exp(-tt / 0.085), res=0.35, drive=1.3)
    Y = np.tanh(1.4 * Y) / np.tanh(1.4)
    Y = room(Y, 0.3, 0.16, 0.5, 0.9)
    return fade_out(Y, 0.1)


def crushed_reese(m, T=2.0):
    """Crushed Reese (an exact loop): two saws a cycle apart either side of the note, a sine a fifth up, a little noise;
    driven, band-pass overdrive blended in, 6-bit quantising for grit."""
    f = m2f(m)
    N, L = loop_len(f, T, even=True)
    k = np.arange(L)
    x = saw_per(N - 1, L, L, 0.1) + saw_per(N + 1, L, L, 0.6)
    x += 0.35 * np.sin(2 * np.pi * (N * 3 // 2) * k / L)
    nz = rng.standard_normal(L)
    x += 0.04 * circ_lp(nz, 3000, 2)
    x = np.tanh(2.2 * x / 2) / np.tanh(2.2)
    X = np.fft.rfft(x)
    fr = np.fft.rfftfreq(L, 1 / SR)
    bp = np.fft.irfft(X * ((fr > 220) & (fr < 2600)), L)
    x = 0.8 * x + 0.35 * np.tanh(3 * bp) / np.tanh(3)
    q = np.round(x * 24) / 24
    x = 0.78 * x + 0.22 * q
    x = circ_lp(x - x.mean(), 4200, 2)
    return x, L


INSTR = {   # key: (folder, file stem, notes, builder, kind)
    'gvfm': ('GrooveHouse/FMRubberBass', 'FMRubberBass', range(24, 61, 3), fm_bass, 'oneshot'),
    'gvglow': ('GrooveHouse/GlowSaw', 'GlowSaw', range(36, 85, 3), glow, 'oneshot'),
    'gvfifth': ('GrooveHouse/FifthsLead', 'FifthsLead', range(48, 85, 3), fifth_lead, 'oneshot'),
    'thbounce': ('TechHouse/BounceBass', 'BounceBass', range(24, 61, 3), bounce_bass, 'loop'),
    'thfifth': ('TechHouse/FifthBass', 'FifthBass', range(24, 61, 3), fifth_bass, 'oneshot'),
    'thrave': ('TechHouse/RaveStab', 'RaveStab', range(36, 73, 3), rave_stab, 'oneshot'),
    'threese': ('TechHouse/CrushedReese', 'CrushedReese', range(24, 61, 3), crushed_reese, 'loop'),
}


def instruments(only=None):
    for key, (folder, stem, notes, fn, kind) in INSTR.items():
        if only and key not in only:
            continue
        xs = {}
        for m in notes:
            r = fn(m)
            xs[m] = r if kind == 'oneshot' else r
        if kind == 'oneshot':
            g = 0.89 / max(np.abs(x).max() for x in xs.values())
            for m, x in xs.items():
                save(f'{key}_{m}', f'Samples/{folder}/{stem}_{nname(m)}.wav', x * g, m, None, inst=key)
        else:
            g = 0.8 / max(np.abs(x).max() for x, L in xs.values())
            for m, (x, L) in xs.items():
                save(f'{key}_{m}', f'Samples/{folder}/{stem}_{nname(m)}.wav', x * g, m, (0, L - 1), inst=key)


if __name__ == '__main__':
    what = sys.argv[1:] or ['kits']
    if 'kits' in what:
        gv_kit(); th_kit()
    if 'inst' in what:
        instruments([w for w in what if w in INSTR] or None)
    path = PACK_ROOT + '/inst/w8_meta.json'
    allm = json.load(open(path)) if os.path.exists(path) else {}
    allm.update(META)
    json.dump(allm, open(path, 'w'), indent=1)
    print(len(META), 'samples rendered;', len(allm), 'in w8_meta.json')
