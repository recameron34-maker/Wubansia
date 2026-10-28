"""Wave 3: new instruments and loop builders for the four genres.
Subs, rubber bass, donks, hoovers; garage organ, Rhodes, FM bells; dub chords and bleeps; a 303; brass, orchestra hits,
choir, disco string runs and dark string stabs; airhorns, a bomb drop, whooshes and crackle; a break kit, top and
percussion patterns. Every function returns float audio: mono (n,) or stereo (n, 2) at 44.1 kHz."""
import numpy as np
import pedalboard as pb
from scipy import signal
from dsp import *
import hits as H
import fx as F
import reese as R
import keys as Kz

RNG3 = np.random.default_rng(303)


def m2f(m):
    return float(midi_hz(m))


def wrap_loop(buf, L):
    """Fold everything past the loop end back onto the start so the loop repeats seamlessly."""
    out = buf[:L].copy()
    tail = buf[L:]
    while len(tail):
        k = min(L, len(tail))
        out[:k] += tail[:k]
        tail = tail[k:]
    return out


def glue(x, thresh=-16, ratio=3.0, drive=2.0, lp=None):
    """Drum-bus glue: compression, a little saturation, optional top-end roll-off."""
    chain = [pb.Compressor(threshold_db=thresh, ratio=ratio, attack_ms=8, release_ms=110), pb.Distortion(drive_db=drive), pb.Gain(-drive * 0.6)]
    if lp:
        chain.append(pb.LowpassFilter(lp))
    return board(to_stereo(x), chain)


def asym_sat(x, drive=1.8, bias=0.12):
    """Saturation with a little even-harmonic content (DC removed afterwards by the caller's high-pass)."""
    return (np.tanh(drive * (x + bias)) - np.tanh(drive * bias)) / np.tanh(drive)


# ============================================================== basses
def sub808(m, dur=1.6, tau=0.95, drive=1.9):
    """2-step sub: a sine that drops into pitch, a long 808-style tail, harmonics for small speakers."""
    n = n_samples(dur)
    t = np.arange(n) / SR
    f = m2f(m) * (1 + 0.33 * np.exp(-t / 0.016))
    x = sine(f) * exp_env(n, tau, attack=0.003)
    x = asym_sat(x, drive, 0.14)
    x += bfilt(noise(n), 'lowpass', 2200, 2) * exp_env(n, 0.0022) * 0.1
    x = bfilt(x, 'highpass', 20, 2)
    return fade(x, 0, 0.09)


def sub_note(m, dur, gate, dip=0.0, vel=1.0):
    """A held sine sub; dip = semitones it bends down over the last 200 ms (the 'wub' at the tail)."""
    n = n_samples(dur + 0.08)
    t = np.arange(n) / SR
    semis = np.zeros(n)
    if dip:
        s0 = max(0.0, gate - 0.2)
        semis = -dip * np.clip((t - s0) / 0.2, 0, 1) ** 2
    f = m2f(m) * 2 ** (semis / 12)
    x = sine(f) + 0.12 * sine(2 * f)
    x = x * adsr(n, 0.004, 0.3, 0.85, 0.04, gate) * vel
    return asym_sat(x, 1.5, 0.08)


def rubber(m, dur=0.75, gate=0.35, vel=1.0, bounce=0.0):
    """Minimal 'rubber' bass: round sine body, a resonant square under a closing filter, a blip of pitch at the front."""
    n = n_samples(dur)
    t = np.arange(n) / SR
    f = m2f(m) * (1 + 1.1 * np.exp(-t / 0.011)) * 2 ** ((bounce * np.exp(-t / 0.03)) / 12)
    body = sine(f) + 0.3 * sine(2 * f) * np.exp(-t / 0.05)
    fc = 150 + 2400 * np.exp(-t / 0.045) * (0.6 + 0.4 * vel)
    x = 0.9 * body + 0.45 * svf(square(f), fc, 2.4, 'lp')
    x = x * adsr(n, 0.002, 0.18, 0.55, 0.06, gate) * (0.55 + 0.45 * vel)
    return fade(sat(x * 0.85, 1.9), 0, 0.02)


def donk(m, dur=0.45):
    """The bassline/hard-house donk: an FM blip that falls two octaves into pitch in a few milliseconds."""
    n = n_samples(dur)
    t = np.arange(n) / SR
    f = m2f(m) * (1 + 2.0 * np.exp(-t / 0.006))
    idx = 2.6 * np.exp(-t / 0.03)
    ph = np.cumsum(f / SR)
    car = np.sin(2 * np.pi * ph + idx * np.sin(2 * np.pi * 2.0 * ph))
    x = car * exp_env(n, 0.11, 0.0008) + 0.25 * np.sin(2 * np.pi * ph) * exp_env(n, 0.2)
    x += 0.2 * bfilt(noise(n), 'bandpass', (1500, 6000)) * exp_env(n, 0.002)
    return fade(sat(x, 1.5), 0, 0.03)


def hoover(m, dur=1.5, gate=0.9, swoop=5.0, rise=None, lfo=None):
    """Mentasm-style hoover: PWM saws spread wide, an octave below, a swoop up into pitch, chorus and grit.
    rise = (semitones, curve) bends up over the whole note; lfo = array of tremolo depth per sample."""
    n = n_samples(dur)
    t = np.arange(n) / SR
    semis = -swoop * np.exp(-t / 0.06)
    if rise:
        semis = semis + rise[0] * (t / t[-1]) ** rise[1]
    pitch = m2f(m) * 2 ** (semis / 12) * (1 + 0.004 * np.sin(2 * np.pi * 5.5 * t) * np.clip(t / 0.3, 0, 1))
    L = np.zeros(n)
    Rr = np.zeros(n)
    for i, c in enumerate([-31, -17, -8, 0, 8, 17, 31]):
        pw = 0.5 + 0.33 * np.sin(2 * np.pi * (0.8 + 0.23 * i) * t + i)
        fv = pitch * 2 ** (c / 1200)
        v = 0.6 * saw(fv, phase0=RNG3.uniform()) + 0.4 * square(fv, pw=pw)
        if c == 0:
            L += 0.7 * v
            Rr += 0.7 * v
        elif i % 2 == 0:
            L += v
        else:
            Rr += v
    sub = 0.55 * saw(pitch / 2)
    x = np.stack([L + sub, Rr + sub], 1) / 4.5
    x = svf_stereo(x, 3600, 0.8, 'lp')
    env = adsr(n, 0.004, 0.3, 0.78, 0.18, gate)
    if lfo is not None:
        env = env * lfo
    x = x * env[:, None]
    x = board(x, [pb.Chorus(rate_hz=0.45, depth=0.3, mix=0.35, centre_delay_ms=7), pb.Distortion(drive_db=7), pb.Gain(-7)])
    return reverb(x, 0.35, 0.12, 0.5, 1.0, 1.0)[:n]


def acid303(steps, bpm, swing=0.5, wave='saw', cut=380.0, envmod=3.6, reso=7.0, decay=0.22, dist=2.6, bars=None, cutmul=None):
    """A TB-303 line. steps: one entry per 16th, None for a rest or (midi, accent, slide).
    Slide ties a note into the next one with a 60 ms glide; accents open the filter further and play louder."""
    st = step_seconds(bpm)
    nst = len(steps)
    bars = bars or nst // 16
    n = n_samples(bars * 16 * st) + n_samples(0.3)
    on = lambda i: float(warp16(np.array(i), swing)) * st
    pitch = np.full(n, np.nan)
    gate = np.zeros(n)
    fenv = np.zeros(n)
    accent = np.zeros(n)
    t = np.arange(n) / SR
    prev_slide = False
    last_f = None
    for i, s in enumerate(steps):
        if s is None:
            prev_slide = False
            continue
        m, acc, slide = s
        a = n_samples(on(i))
        nxt = i + 1
        b = min(n, n_samples(on(i) + (st * 0.55 if not slide else (on(nxt) - on(i)) + 0.004)))
        f = m2f(m)
        if prev_slide and last_f is not None:   # glide from the previous pitch
            k = np.arange(b - a) / SR
            pitch[a:b] = f * (last_f / f) ** np.exp(-k / 0.02)
        else:
            pitch[a:b] = f
            k = np.arange(n - a) / SR
            fenv[a:] = np.exp(-k / (decay * (0.6 if acc else 1.0))) * (1.6 if acc else 1.0)   # retrigger the filter envelope
            accent[a:] = 1.0 if acc else 0.0
        gate[a:b] = 1.0
        last_f = f
        prev_slide = bool(slide)
    # hold pitch through rests so the filter has something to chew on
    idx = np.where(~np.isnan(pitch))[0]
    pitch = np.interp(np.arange(n), idx, pitch[idx]) if len(idx) else np.full(n, 110.0)
    osc = saw(pitch) if wave == 'saw' else square(pitch, pw=0.5)
    fc = np.clip(cut * 2 ** (envmod * fenv) * (1 if cutmul is None else np.interp(np.linspace(0, 1, n), np.linspace(0, 1, len(cutmul)), cutmul)), 60, 12000)
    y = svf(svf(osc, fc, 0.7, 'lp'), fc, reso * (1 + 0.25 * accent), 'lp')
    g = signal.lfilter([1 - np.exp(-1 / (0.003 * SR))], [1, -np.exp(-1 / (0.003 * SR))], gate)   # 3 ms gate smoothing
    y = y * g * (0.75 + 0.35 * accent)
    y = np.tanh(dist * y) / np.tanh(dist)
    y = bfilt(y, 'highpass', 35, 2)
    return y


# ============================================================== keys and stabs
def organ_stab(midis, gate=0.22, dur=1.1):
    """Garage M1-style organ stab: drawbars, a percussive third harmonic, key click, fast Leslie shimmer."""
    n = n_samples(dur)
    t = np.arange(n) / SR
    x = np.zeros(n)
    for m in midis:
        f = m2f(m)
        for h, a in [(0.5, 0.3), (1, 1.0), (2, 0.8), (3, 0.5), (4, 0.4), (6, 0.18), (8, 0.12)]:
            if f * h < SR * 0.45:
                x += a * sine(np.full(n, f * h), phase0=RNG3.uniform())
        x += 0.45 * sine(np.full(n, f * 3)) * np.exp(-t / 0.05)
    x /= len(midis) * 2.6
    x = x * adsr(n, 0.002, 0.12, 0.6, 0.05, gate) + 0.05 * bfilt(noise(n), 'bandpass', (2500, 7000)) * exp_env(n, 0.003)
    x = sat(x * 1.3, 1.4)
    y = board(to_stereo(x), [pb.Chorus(rate_hz=6.2, depth=0.14, mix=0.5, centre_delay_ms=3), pb.HighShelfFilter(4000, 2.0)])
    return reverb(y, 0.3, 0.14, 0.5, 1.0, 1.0)[:n]


def rhodes_note(m, dur, vel=96, gate=None):
    """FM electric piano note: bell-ish attack that mellows, a tine ping, decay that is longer for low notes."""
    g = dur if gate is None else gate
    n = n_samples(dur + 0.9)
    t = np.arange(n) / SR
    f = m2f(m)
    v = vel / 127
    idx = (1.9 * v + 0.35) * np.exp(-t / 0.32) + 0.25
    car = np.sin(2 * np.pi * f * t + idx * np.sin(2 * np.pi * f * t) + RNG3.uniform(0, 6.28))
    tine = np.sin(2 * np.pi * f * 13.8 * t) * np.exp(-t / 0.01) * 0.12 * v
    tau = max(0.9, 3.4 - 0.035 * (m - 40))
    x = (car + tine) * np.exp(-t / tau)
    i = n_samples(g)
    if i < n:
        x[i:] *= np.exp(-(t[i:] - t[i]) / 0.13)
    a = n_samples(0.002)
    x[:a] *= np.linspace(0, 1, a)
    return x * (0.4 + 0.6 * v)


def rhodes_post(x, bpm=None, trem=0.32, rate=None):
    """Suitcase tremolo (stereo autopan), a little amp grit, chorus and room."""
    x = x if x.ndim == 1 else x.mean(axis=1)
    n = len(x)
    t = np.arange(n) / SR
    rate = rate or ((bpm / 60) * 2 if bpm else 4.6)
    lfo = np.sin(2 * np.pi * rate * t)
    y = np.stack([x * (1 - trem * (0.5 + 0.5 * lfo)), x * (1 - trem * (0.5 - 0.5 * lfo))], 1)
    y = board(y, [pb.HighpassFilter(30), pb.Distortion(drive_db=3), pb.Gain(-2), pb.Chorus(rate_hz=0.7, depth=0.15, mix=0.25)])
    return reverb(y, 0.35, 0.14, 0.5, 1.0, 1.0)


def fm_bell(m, dur=1.4, vel=100):
    n = n_samples(dur)
    t = np.arange(n) / SR
    f = m2f(m)
    v = vel / 127
    idx = 3.8 * v * np.exp(-t / 0.18)
    x = np.sin(2 * np.pi * f * t + idx * np.sin(2 * np.pi * 3.5 * f * t)) * np.exp(-t / 0.9)
    x += 0.15 * np.sin(2 * np.pi * 2 * f * t) * np.exp(-t / 0.4)
    a = n_samples(0.002)
    x[:a] *= np.linspace(0, 1, a)
    return x * (0.4 + 0.6 * v)


def dub_chord(midis, bpm, dur=2.9):
    """Basic Channel-style dub chord: a filtered saw/pulse stab into a darkening ping-pong delay and a big room."""
    n0 = n_samples(0.45)
    t = np.arange(n0) / SR
    x = np.zeros(n0)
    for m in midis:
        f = m2f(m)
        for d in (-7, 7):
            fv = f * 2 ** (d / 1200)
            x += saw(np.full(n0, fv), phase0=RNG3.uniform()) + 0.5 * square(np.full(n0, fv), pw=0.4)
    x /= len(midis) * 3
    x = svf(x, 380 + 2600 * np.exp(-t / 0.035), 1.6, 'lp')
    x = bfilt(x, 'highpass', 180, 2) * adsr(n0, 0.002, 0.06, 0.25, 0.04, 0.11)
    st = step_seconds(bpm)
    y = pingpong(x, 3 * st, 0.58, 0.5, 2300, 12)
    y = reverb(y, 0.78, 0.3, 0.6, 1.0, 1.0)
    n = n_samples(dur)
    y = trim_to(y, n)
    return fade(y, 0, 0.4)


def bleep(m, dur=0.16, vel=100):
    n = n_samples(dur)
    t = np.arange(n) / SR
    f = m2f(m)
    x = np.sin(2 * np.pi * f * t) + 0.08 * square(np.full(n, 2 * f))
    return x * exp_env(n, 0.045, 0.0006) * (0.5 + 0.5 * vel / 127)


def string_voice(midis, dur, gate, attack=0.012, bright=4200, decay=None):
    """Ensemble strings with a selectable attack; decay= makes a stab that dies away while held."""
    n = n_samples(dur + 0.6)
    t = np.arange(n) / SR
    L = np.zeros(n)
    Rr = np.zeros(n)
    vib = 1 + 0.0035 * np.clip((t - 0.2) / 0.3, 0, 1) * np.sin(2 * np.pi * 5.2 * t)
    for m in midis:
        f = m2f(m)
        for i, c in enumerate((-12, -6, -2, 3, 7, 12)):
            v = saw(f * 2 ** (c / 1200) * vib, phase0=RNG3.uniform())
            (L if i % 2 == 0 else Rr)[:] += v
    x = np.stack([L, Rr], 1) / (len(midis) * 3)
    fc = bright if np.isscalar(bright) else bright
    x = svf_stereo(x, fc, 0.6, 'lp')
    env = np.minimum(1, t / attack) ** 1.5
    if decay:
        env *= 0.25 + 0.75 * np.exp(-t / decay)
    i = n_samples(gate)
    if i < n:
        env[i:] *= np.exp(-(t[i:] - t[i]) / 0.3)
    return x * env[:, None]


def brass(midis, dur=0.9, gate=0.22, fall=0.0, vel=1.0):
    """Disco/house brass section stab: saws with a fast 'blat' filter, vibrato; fall = semitones it drops at the end."""
    n = n_samples(dur)
    t = np.arange(n) / SR
    bend = 2 ** (-fall * np.clip((t - gate * 0.75) / 0.28, 0, 1) ** 1.6 / 12) if fall else np.ones(n)
    vib = 1 + 0.006 * np.sin(2 * np.pi * 5.8 * t) * np.clip((t - 0.12) / 0.2, 0, 1)
    L = np.zeros(n)
    Rr = np.zeros(n)
    for m in midis:
        f = m2f(m) * bend * vib
        for i, c in enumerate((-9, -3, 4, 10)):
            v = saw(f * 2 ** (c / 1200), phase0=RNG3.uniform())
            (L if i % 2 == 0 else Rr)[:] += v
    x = np.stack([L, Rr], 1) / (len(midis) * 2.2)
    fc = 700 + 4300 * (1 - np.exp(-t / 0.016)) * (0.35 + 0.65 * np.exp(-t / 0.3)) * vel
    x = svf_stereo(x, fc, 1.1, 'lp')
    x = x * adsr(n, 0.012, 0.2, 0.72, 0.07, gate)[:, None]
    x = board(x, [pb.Distortion(drive_db=4), pb.Gain(-3), pb.PeakFilter(1400, 3.0, 0.9), pb.HighpassFilter(120)])
    return reverb(x, 0.42, 0.16, 0.5, 1.0, 1.0)[:n]


def orch_hit(midis, root, dur=1.5):
    """Orchestra hit: a bright stacked chord that slams and decays, a timpani thump on the root, a noise burst, big plate."""
    n = n_samples(dur)
    t = np.arange(n) / SR
    L = np.zeros(n)
    Rr = np.zeros(n)
    for m in midis:
        for o in (0, 12):
            f = m2f(m + o)
            for i, c in enumerate((-12, 0, 12)):
                v = saw(np.full(n, f * 2 ** (c / 1200)), phase0=RNG3.uniform())
                (L if (i + o // 12) % 2 == 0 else Rr)[:] += v
    x = np.stack([L, Rr], 1) / (len(midis) * 4)
    x = svf_stereo(x, 1500 + 8000 * np.exp(-t / 0.05), 0.8, 'lp')
    x = x * (np.exp(-t / 0.26) * np.minimum(1, t / 0.002))[:, None]
    ft = m2f(root) * (1 + 0.5 * np.exp(-t / 0.02))
    timp = sine(ft) * exp_env(n, 0.45, 0.002) * 0.8
    burst = bfilt(noise(n), 'bandpass', (1800, 8000), 2) * exp_env(n, 0.035, 0.0005) * 0.5
    y = x + to_stereo(timp + burst)
    y = board(y, [pb.Compressor(threshold_db=-14, ratio=3, attack_ms=1, release_ms=120), pb.Distortion(drive_db=3), pb.Gain(-3)])
    return reverb(y, 0.85, 0.3, 0.45, 1.0, 1.0)[:n]


VOWELS = {'a': [(830, 80, 1.0), (1180, 90, 0.5), (2850, 120, 0.16), (3600, 130, 0.07)],
          'o': [(470, 70, 1.0), (820, 80, 0.45), (2700, 120, 0.08), (3400, 130, 0.04)],
          'e': [(560, 70, 1.0), (1750, 100, 0.35), (2600, 120, 0.2), (3500, 130, 0.08)]}


def formant(x, vowel='a'):
    out = np.zeros_like(x)
    for fc, bw, g in VOWELS[vowel]:
        out += g * bfilt(x, 'bandpass', (fc - bw, fc + bw), 2)
    return out


def choir(midis, dur=2.4, gate=None, vowel='a', attack=0.18, voices=3):
    """Choir 'aah': per note several voices with vibrato and jitter, a glottal-ish source through vowel formants."""
    g = dur * 0.8 if gate is None else gate
    n = n_samples(dur + 0.5)
    t = np.arange(n) / SR
    L = np.zeros(n)
    Rr = np.zeros(n)
    k = 0
    for m in midis:
        f = m2f(m)
        for vv in range(voices):
            det = (vv - (voices - 1) / 2) * 9
            rate = 5.0 + RNG3.uniform(-0.5, 0.5)
            jit = np.cumsum(RNG3.normal(0, 1, n)) / SR
            jit = bfilt(jit - np.linspace(jit[0], jit[-1], n), 'lowpass', 6, 1) * 0.6
            fv = f * 2 ** (det / 1200) * (1 + 0.006 * np.sin(2 * np.pi * rate * t + RNG3.uniform(0, 6)) * np.clip(t / 0.4, 0, 1) + 0.002 * jit)
            src = saw(fv, phase0=RNG3.uniform())
            src = bfilt(src, 'lowpass', 3200, 1) + 0.04 * bfilt(noise(n), 'bandpass', (1500, 6000), 1)
            v = formant(src, vowel) + 0.35 * bfilt(src, 'lowpass', 700, 2)   # formants plus the body below F1
            pan = ((k % 4) - 1.5) / 1.5 * 0.7
            L += v * np.cos((pan + 1) * np.pi / 4)
            Rr += v * np.sin((pan + 1) * np.pi / 4)
            k += 1
    x = np.stack([L, Rr], 1) / (k ** 0.6)
    env = np.minimum(1, t / attack) ** 1.4
    i = n_samples(g)
    if i < n:
        env[i:] *= np.exp(-(t[i:] - t[i]) / 0.35)
    x = x * env[:, None]
    x = board(x, [pb.HighpassFilter(150), pb.Chorus(rate_hz=0.5, depth=0.2, mix=0.3)])
    return reverb(x, 0.7, 0.26, 0.5, 1.0, 1.0)[:n_samples(dur)]


# ============================================================== FX
def airhorn(f0=415.0, blasts=((0.0, 0.13), (0.2, 0.13), (0.4, 0.78)), dur=2.0, spread=0.012):
    """The sound-system airhorn: two buzzy horns beating, a pressure scoop into each blast, a resonant bell, grit."""
    n = n_samples(dur)
    x = np.zeros(n)
    for st_, ln in blasts:
        a, b = n_samples(st_), n_samples(st_ + ln)
        k = np.arange(b - a) / SR
        semis = -1.6 * np.exp(-k / 0.022) + 0.1 * np.sin(2 * np.pi * 7.5 * k)
        p = f0 * 2 ** (semis / 12)
        v = square(p, pw=0.32) + 0.8 * saw(p * (1 + spread)) + 0.5 * saw(p * 2.003)
        e = np.minimum(1, k / 0.006) * np.minimum(1, (k[-1] - k) / 0.025 + 1e-9)
        x[a:b] += v * e
    body = 0.6 * bfilt(x, 'bandpass', (450, 4200), 2) + 0.5 * svf(x, 1250, 4.0, 'bp') + 0.35 * svf(x, 2500, 5.0, 'bp')
    y = bfilt(np.tanh(2.4 * body) / np.tanh(2.4), 'highpass', 120, 2)
    st2 = np.stack([y, np.concatenate([np.zeros(n_samples(0.011)), y])[:n]], 1)
    return reverb(st2, 0.4, 0.14, 0.4, 1.0, 1.0)[:n]


def bomb_drop(bpm):
    """A falling whistle for a bar and a half, then the hit: noise blast, sub boom, crash, long dark tail (2 bars)."""
    bar = 4 * 60 / bpm
    n = n_samples(2 * bar)
    t = np.arange(n) / SR
    fall = 1.5 * bar
    nf = n_samples(fall)
    tf = t[:nf] / fall
    f = 2300 * (310 / 2300) ** (tf ** 1.25) * (1 + 0.012 * np.sin(2 * np.pi * 6 * t[:nf]))
    wh = (sine(f) + 0.05 * bfilt(noise(nf), 'bandpass', (1500, 5000), 1)) * (0.2 + 0.8 * tf) * 0.5
    x = np.zeros((n, 2))
    pan = np.sin(np.pi * tf) * 0.6
    x[:nf, 0] += wh * np.cos((pan + 1) * np.pi / 4) * 1.4
    x[:nf, 1] += wh * np.sin((pan + 1) * np.pi / 4) * 1.4
    hit = n_samples(fall)
    m = n - hit
    th = np.arange(m) / SR
    blast = bfilt(noise(m), 'lowpass', 1400, 2) * exp_env(m, 0.5, 0.002) * 1.4
    blast += bfilt(noise(m), 'bandpass', (1500, 7000), 2) * exp_env(m, 0.08, 0.001) * 0.6
    boom = H.sub_boom(m2f(29), m2f(41), m / SR, 0.9)[:m] * 1.1
    cr = trim_to(F.hits()['crash'], m) * 0.4
    hitx = to_stereo(np.tanh(1.8 * (blast + boom)) * 0.8) + to_stereo(cr)
    hitx = reverb(hitx, 0.9, 0.35, 0.6, 1.0, 1.0)[:m]
    x[hit:] += hitx
    return fade(x, 0.005, 0.25)


def whoosh(bpm, bars=1, up=True):
    n = n_samples(bars * 4 * 60 / bpm)
    t = np.linspace(0, 1, n)
    shape = t ** 1.5 if up else (1 - t) ** 1.2
    fc = 250 * (9000 / 250) ** (t ** 1.2 if up else (1 - t) ** 0.8)
    a = svf(noise(n), fc, 1.8, 'bp')
    b = svf(noise(n), fc * 1.06, 1.8, 'bp')
    pan = np.sin(2 * np.pi * (0.5 if up else 0.75) * t) * 0.8
    x = np.stack([a * np.cos((pan + 1) * np.pi / 4) + 0.3 * b, b * np.sin((pan + 1) * np.pi / 4) + 0.3 * a], 1)
    x = x * (shape * (np.minimum(1, (1 - t) * 60) if up else np.minimum(1, t * 80)))[:, None]
    return reverb(x, 0.6, 0.2, 0.5, 1.0, 1.0)[:n]


def crackle_bed(bpm, bars=2):
    """Vinyl texture: crackle, pops, hiss and a little motor rumble; folded so it loops cleanly."""
    L = n_samples(bars * 4 * 60 / bpm)
    n = L + n_samples(0.5)
    c1 = F.crackle(n, 55, 0.18)
    c2 = F.crackle(n, 45, 0.16)
    pops = np.zeros(n)
    for i in RNG3.integers(0, n - 2000, int(bars * 3)):
        k = np.arange(2000) / SR
        pops[i:i + 2000] += RNG3.uniform(0.08, 0.2) * np.sin(2 * np.pi * RNG3.uniform(180, 420) * k) * np.exp(-k / 0.004)
    hiss = bfilt(noise(n), 'bandpass', (3000, 12000), 1) * 0.012
    rumble = bfilt(noise(n), 'lowpass', 70, 2) * 0.05
    x = np.stack([c1 + 0.5 * pops + hiss + rumble, c2 + 0.5 * pops + hiss * 0.9 + rumble], 1)
    return wrap_loop(x, L)


def noise_pad(n):
    return bfilt(noise(n), 'bandpass', (400, 8000), 1)


# ============================================================== drums: break kit, tops, percussion
_BK = None


def break_kit():
    """An 'acoustic' break kit: rounder kick, ringing snare, ghost, tight and open hats, ride."""
    global _BK
    if _BK is None:
        kick = H.kick(m2f(28) * 1.0, 120, 0.03, 0.17, 0.45, 0.22, 1.3)
        sn = H.snare(205, 345, 0.075, 0.14, 900, 9000, 0.75, 0.85, 0.5, 1.4)
        ring = sine(np.full(n_samples(0.5), 560.0)) * exp_env(n_samples(0.5), 0.09) * 0.12
        sn = sn + ring[:len(sn)]
        ghost = H.snare(205, 345, 0.04, 0.06, 1100, 8000, 0.6, 0.8, 0.25, 1.2)
        ch = H.hat(0.028, 0.14, scale=0.85, metal_amt=0.55, noise_amt=0.6, band=(5500, 11500), hp=6000)
        oh = H.hat(0.22, 0.6, scale=0.85, metal_amt=0.55, noise_amt=0.6, band=(5500, 11500), hp=6000)
        rd = H.ride(1.4)
        _BK = {'K': normalize(kick, -1), 'S': normalize(sn, -1), 'G': normalize(ghost, -1), 'H': normalize(ch, -1), 'O': normalize(oh, -1), 'R': normalize(rd, -1)}
    return _BK


BK_MIX = {'K': (1.0, 0), 'S': (0.8, 0.05), 'G': (0.22, 0.05), 'H': (0.3, 0.3), 'O': (0.32, 0.3), 'R': (0.24, -0.3)}


def render_break(lanes, bars, bpm, swing=0.5, human=0.004, lofi=True):
    """lanes: {lane: [bar strings of 16 chars, digits = velocity]} for the break kit. Humanised, roomy, squashed."""
    kit = break_kit()
    st = step_seconds(bpm)
    L = n_samples(bars * 16 * st)
    buf = {k: np.zeros(L + n_samples(1.6)) for k in kit}
    for lane, bstr in lanes.items():
        for b in range(bars):
            s = bstr[b % len(bstr)]
            for j, c in enumerate(s):
                if c == '.':
                    continue
                tt = float(warp16(np.array(b * 16 + j), swing)) * st + RNG3.normal(0, human)
                x = kit[lane]
                if lane == 'O':   # open hat chokes on the next closed hat
                    nxt = [b2 * 16 + j2 for b2 in range(bars) for j2, c2 in enumerate(lanes.get('H', ['.' * 16])[b2 % len(lanes.get('H', ['.' * 16]))]) if c2 != '.' and b2 * 16 + j2 > b * 16 + j]
                    if nxt:
                        cut = n_samples(float(warp16(np.array(nxt[0]), swing)) * st - tt)
                        x = x[:max(10, cut)].copy()
                        x[-min(len(x), 200):] *= np.linspace(1, 0, min(len(x), 200))
                place(buf[lane], x, max(0, n_samples(tt)), BK_MIX[lane][0] * vel_gain(int(c)) * RNG3.uniform(0.92, 1.05))
    mix = np.zeros((len(buf['K']), 2))
    for lane, b in buf.items():
        mix += to_stereo(b, BK_MIX[lane][1])
    room = board(mix, [pb.Reverb(room_size=0.22, damping=0.5, wet_level=0.16, dry_level=1.0, width=0.8)])
    chain = [pb.Compressor(threshold_db=-20, ratio=4, attack_ms=3, release_ms=80), pb.Distortion(drive_db=5), pb.Gain(-3)]
    if lofi:
        chain += [pb.LowpassFilter(11500), pb.Bitcrush(bit_depth=12)]
    y = board(room, chain)
    return wrap_loop(y, L)


def render_pattern(pattern, bars, bpm, swing, kit='ukg', gains=None, lane_fx=None):
    """Kit-hit patterns (same lane names as the MIDI kits) rendered with tails folded back onto the start."""
    st = step_seconds(bpm)
    L = n_samples(bars * 16 * st)
    x = F.render_drums(pattern, bars, bpm, swing, kit, lane_fx=lane_fx, tail=1.5, gains=gains)
    return wrap_loop(x, L)


def perc_hit(name, semis=0.0):
    x = F.hits()[name]
    if semis:
        r = 2 ** (semis / 12)
        idx = np.arange(0, len(x) - 1, r)
        x = np.interp(idx, np.arange(len(x)), x)
    return x


def render_perc(events, bars, bpm, swing):
    """events: [(step16, hit_name, gain, pan, semis)] -> stereo loop."""
    st = step_seconds(bpm)
    L = n_samples(bars * 16 * st)
    out = np.zeros((L + n_samples(1.0), 2))
    for p, name, g, pan, semis in events:
        tt = float(warp16(np.array(p), swing)) * st + RNG3.normal(0, 0.002)
        place(out, to_stereo(perc_hit(name, semis), pan), max(0, n_samples(tt)), g)
    return wrap_loop(out, L)
