"""Defect fixtures: small renders that reproduce what the S1 prototypes got wrong, each with a clean control.

The defects (plan section 5.2): a truncated tail, a click at the start, an anti-correlated low end, DC from drive (twice:
a biased curve, and an odd curve on a skewed wave), an anti-correlated freeze bed, and the muddy kick the user heard in
round 0 (distortion on a 300 ms body). Controls are finished like a master (D.finish, then the true-peak ceiling).
Below them, boundary pairs: sounds built to read a set value just inside or just outside one QA limit.
"""
import numpy as np
from scipy import signal

from tools.sound import dsp48 as D

SR = D.SR
F_KICK = float(D.midi_hz(30))  # F#1, 46.25 Hz


def _t(n):
    return np.arange(n) / SR


def master(x):
    """Finish a one-shot like render.py does for a single file: trim, fades, true peak at the ceiling."""
    return D.to_ceiling(D.finish(x))


# ------------------------------------------------------------------ tails and edges
def ping(dur):
    """An 880 Hz ping with a 1 ms attack and a 150 ms decay, rendered for dur seconds."""
    n = int(dur * SR)
    return np.sin(2 * np.pi * 880 * _t(n)) * D.env_exp(n, 0.15, attack=0.001)


def truncated_tail():
    """Rendered too short: the 5 ms fade cuts the decay while it's still loud."""
    return D.to_ceiling(D.fade(ping(0.12), 0.00025, 0.005))


def full_tail():
    return master(ping(3.0))


def _bell(n):
    """A tone that is already at half its peak on sample 0 (a cosine part) and swells to the peak by 5 ms."""
    t = _t(n)
    return (0.5 * np.cos(2 * np.pi * 440 * t) + 0.5 * np.sin(2 * np.pi * 660 * t) * D.env_exp(n, 1.0, attack=0.005)) \
        * D.env_exp(n, 0.2)


def click_start():
    """Starts on sample 0 without a fade-in: the click."""
    n = int(2.5 * SR)
    return D.to_ceiling(D.fade(_bell(n), 0.0, 0.005))


def soft_start():
    return master(_bell(int(2.5 * SR)))


# ------------------------------------------------------------------ stereo low end
def _hiss(n, r):
    """Partly correlated band noise (2-8 kHz) for the top of a wide sound."""
    a, b, c = (D.bfilt(r.uniform(-1, 1, n), (2000, 8000), 'bandpass') for _ in range(3))
    return np.stack([a + b, a + c], 1)


def wide_low_raw():
    """A sub voiced as two detuned sines, one per side and 150 degrees apart (the prototype's wide stacks): the low end
    starts out of phase between L and R and drifts."""
    n = int(4.0 * SR)
    t = _t(n)
    w = 2 * np.pi * F_KICK * t
    low = np.stack([np.sin(0.995 * w), np.sin(1.005 * w + np.radians(150))], 1)
    x = (low + 0.3 * _hiss(n, D.rng('fixture', 'wide'))) * D.env_exp(n, 0.4, attack=0.002)[:, None]
    return D.finish(D.dc_block(x))


def wide_low():
    return D.to_ceiling(wide_low_raw())


def wide_low_fixed():
    return D.to_ceiling(D.mono_below(wide_low_raw(), 150))


def _freeze(src, r, dur, shared):
    """Spectral freeze of one STFT frame. shared=False gives each channel its own random phases (the prototype bug);
    shared=True gives both channels one phase field plus a smaller side, mono under 150 Hz (the plan's fix)."""
    nper, hop = 4096, 1024
    _, _, X = signal.stft(src, SR, nperseg=nper, noverlap=nper - hop)
    mag = np.abs(X[:, X.shape[1] // 2])
    frames = int(dur * SR / hop)

    def one(ph):
        return signal.istft(mag[:, None] * np.exp(1j * ph), SR, nperseg=nper, noverlap=nper - hop)[1][:int(dur * SR)]

    if not shared:
        return np.stack([one(r.uniform(0, 2 * np.pi, (len(mag), frames))) for _ in range(2)], 1)
    M = one(r.uniform(0, 2 * np.pi, (len(mag), frames)))
    S = 0.4 * one(r.uniform(0, 2 * np.pi, (len(mag), frames)))
    return D.mono_below(np.stack([M + S, M - S], 1), 150)


def _chord(dur):
    """F#m7 saws (F#2 A2 C#3 E3), darkened: the freeze source."""
    n = int(dur * SR)
    x = sum(D.polyblep_saw(float(D.midi_hz(m)), n) for m in (42, 45, 49, 52))
    return D.bfilt(x, 3000, 'lowpass')


def freeze_bed(shared=False, level=-20.0):
    """A 6 s frozen bed at a LUFS level (beds are -20 LUFS); shared=False is the anti-correlated prototype, high-passed
    at 100 Hz (2nd order) as it was. The fix also high-passes its F#2 root out of the way: 120 Hz, 8th order."""
    from tools.sound import qa
    y = _freeze(_chord(2.0), D.rng('fixture', 'freeze'), 6.0, shared)
    y = D.bfilt(y, 100, 'highpass')
    if shared:
        y = D.lr4(D.lr4(y, 120, 'highpass'), 120, 'highpass')
    y = D.fade(y, 0.5, 1.0)
    return y * D.undb(level - qa.lufs(y))


# ------------------------------------------------------------------ DC from drive
def _swell(dur):
    n = int(dur * SR)
    return n, D.env_exp(n, 0.25, 0.005)


def dc_bias():
    """A biased (asymmetric) tanh on a 110 Hz sine, no DC blocker afterwards."""
    n, e = _swell(3.0)
    return master(D.tanh_adaa(np.sin(2 * np.pi * 110 * _t(n)) * e, 4.0, bias=0.6))


def dc_bias_fixed():
    n, e = _swell(3.0)
    return master(D.dc_block(D.tanh_adaa(np.sin(2 * np.pi * 110 * _t(n)) * e, 4.0, bias=0.6)))


def _skewed(n):
    """A zero-mean, band-limited 20% pulse at 82 Hz: short tall peaks one way, long shallow ones the other."""
    f0 = 82.0
    ph = 2 * np.pi * D.phase(f0, n)
    p = sum(np.sin(k * ph) * np.sin(np.pi * k * 0.2) / k for k in range(1, int(20000 // f0)))
    return p / np.abs(p).max()


def dc_skew():
    """An odd (symmetric) tanh still makes DC on a skewed wave."""
    n, e = _swell(3.0)
    return master(D.tanh_adaa(_skewed(n) * e, 6.0))


def dc_skew_fixed():
    n, e = _swell(3.0)
    return master(D.dc_block(D.tanh_adaa(_skewed(n) * e, 6.0)))


# ------------------------------------------------------------------ kicks
def _sweep(n, f_hi, tau1, tau2, w2, cents=0.0):
    f_end = F_KICK * 2 ** (cents / 1200)
    t = _t(n)
    f = f_end + (f_hi - f_end) * ((1 - w2) * np.exp(-t / tau1) + w2 * np.exp(-t / tau2))
    return np.sin(2 * np.pi * D.phase(f, n))


def muddy_kick(vel=1.0, dur=4.0):
    """The round-0 kick: slow sweep, a 300 ms body through tanh -> fold -> clip, clean only under 90 Hz."""
    n = int(dur * SR)
    s = _sweep(n, 260 + 340 * vel, 0.009, 0.045, 0.3)
    chain = lambda u: D.hardclip_adaa(D.sinefold_adaa(D.tanh_adaa(u, 2 + 6 * vel, 0.25), 1 + 0.6 * vel), 1.4)
    d = D.oversampled(chain, s * D.env_exp(n, 0.30), 4)
    y = D.lr4(d, 90, 'highpass') + D.lr4(0.9 * s * D.env_exp(n, 0.32), 90, 'lowpass')
    return D.dc_block(D.bfilt(y, 9000, 'lowpass')) * vel


def clean_kick(vel=1.0, rr=0, cents=0.0, dur=1.0):
    """A split kick: clean sub that settles on F#1 by ~60 ms, a short driven knock above 110 Hz with a 255 Hz dip, and a
    noise click. Velocity sets level, knock drive and click brightness."""
    n = int(dur * SR)
    t = _t(n)
    r = D.rng('fixture', 'kick', vel, rr)
    s = _sweep(n, 300 + 300 * vel, 0.004, 0.014, 0.2, cents)
    sub = D.lr4(s * np.exp(-(t / 0.17) ** 1.8) * D.taper(n, 0.30, 0.12), 110, 'lowpass')
    knock = D.tanh_adaa(s * np.exp(-t / 0.03), 2 + 3 * vel) * D.taper(n, 0.03, 0.04)
    knock = D.peaking(D.lr4(knock, 110, 'highpass'), 255, -10, 0.75)
    click = D.bfilt(r.uniform(-1, 1, n), 3000, 'highpass') * D.env_exp(n, 0.0008 + 0.0008 * vel)
    return D.dc_block(sub + (0.4 + 0.3 * vel) * knock + (0.1 + 0.3 * vel) * click) * vel


def shared_layers(fn, vels, **kw):
    """Render layers and give them one shared gain (loudest at the ceiling), as render.py does."""
    raw = {v: D.finish(fn(v, **kw)) for v in vels}
    g = D.undb(D.TP_TARGET - max(D.true_peak_db(x) for x in raw.values()))
    return {v: x * g for v, x in raw.items()}


# ------------------------------------------------------------------ peaks, seams, splices, aliasing, tuning, loudness
def intersample_peak():
    """A sine at SR/4, 45 degrees off the sample grid: samples peak 3 dB under the true peak. Sample peak -1.5 dBFS."""
    n = SR
    x = np.sin(2 * np.pi * (SR / 4) * _t(n) + np.pi / 4)
    return D.fade(x / np.abs(x).max() * D.undb(-1.5), 0.01, 0.01)


def sine_loop(f, seconds=2.0):
    """A looped sustain; the loop is clean only when it holds a whole number of periods."""
    n = int(seconds * SR)
    return 0.5 * np.sin(2 * np.pi * f * _t(n))


def gain_step(crossfade):
    """A sustained tone with a hard (or 5 ms crossfaded) 6 dB drop at a logged splice, at 1 s."""
    n = 2 * SR
    t = _t(n)
    x = 0.5 * (np.sin(2 * np.pi * 330 * t) + 0.5 * np.sin(2 * np.pi * 495 * t + 1.0))
    g = np.where(np.arange(n) < SR, 1.0, 0.5)
    if crossfade:
        g = np.interp(np.arange(n), [SR - 120, SR + 120], [1.0, 0.5])
    return x * g, (SR,)


def naive_saw(f, dur=1.0):
    """A saw made from the raw phase ramp: every harmonic above Nyquist folds back."""
    p = D.phase(f, int(dur * SR)) % 1.0
    return 0.5 * (2 * p - 1)


def clean_saw(f, dur=1.0):
    return 0.5 * D.additive_saw(f, int(dur * SR))


def tone(f, dur=1.0, amp=0.5):
    return amp * np.sin(2 * np.pi * f * _t(int(dur * SR)))


def riser(rising=True, level=-14.0, dur=4.0):
    """Band noise that swells (rising=True) or stays flat, at a LUFS level."""
    from tools.sound import qa
    n = int(dur * SR)
    x = D.bfilt(D.rng('fixture', 'riser').uniform(-1, 1, n), (500, 8000), 'bandpass')
    if rising:
        x *= (np.arange(n) / n) ** 2
    x = D.fade(x, 0.01, 0.003)
    return x * D.undb(level - qa.lufs(x))


def low_pad(highpassed):
    """A pad chord with a 46 Hz root in it (competes with the kick), optionally high-passed at 100 Hz (two LR4s, 8th order)."""
    n = int(2.0 * SR)
    x = sum(D.polyblep_saw(float(D.midi_hz(m)), n) for m in (30, 54, 57, 61))
    x = D.bfilt(x, 4000, 'lowpass')
    if highpassed:
        x = D.lr4(D.lr4(x, 100, 'highpass'), 100, 'highpass')
    return master(x)


# ------------------------------------------------------------------ boundary pairs: just inside and outside a limit
def steady(n, f, t0, t1, amp, edge=0.005):
    """A tone at f Hz from t0 to t1 seconds with raised-cosine edges: a controlled ring in one band."""
    t = _t(n)
    a = np.clip((t - t0) / edge, 0, 1) * np.clip((t1 - t) / edge, 0, 1)
    return amp * (0.5 - 0.5 * np.cos(np.pi * a)) * np.sin(2 * np.pi * f * t)


def ringing_kick(t1, f=250.0, amp=0.03, t0=0.0):
    """clean_kick (v1.0) with a steady ring at f Hz from t0 to t1 seconds, finished like a master."""
    x = clean_kick()
    return master(x + steady(len(x), f, t0, t1, amp))


def ringing_layers(t1, amp, f=250.0, t0=0.08, vels=(0.4, 0.7, 1.0)):
    """clean_kick layers, each with the same ring under it, sharing one gain: for the 2-step loop's gaps."""
    raw = {}
    for v in vels:
        x = clean_kick(v)
        raw[v] = D.finish(x + v * steady(len(x), f, t0, t1, amp))
    g = D.undb(D.TP_TARGET - max(D.true_peak_db(x) for x in raw.values()))
    return {v: x * g for v, x in raw.items()}


def tail_at(level_db):
    """full_tail() with a 1 kHz tone at level_db dBFS over its last 20 ms: the tail check reads level_db."""
    x = full_tail()
    return x + steady(len(x), 1000.0, (len(x) - int(0.02 * SR)) / SR, len(x) / SR, D.undb(level_db), edge=1e-9)


def first_at(level_db):
    """soft_start() with sample 0 at level_db dBFS."""
    x = soft_start()
    x[0] = D.undb(level_db)
    return x


def seam_at(excess_db, f=240.0):
    """sine_loop(f), a clean loop, with its last sample moved so the jump into the start is excess_db over the
    natural step at the seam (the largest step of the sine)."""
    x = sine_loop(f)
    step = abs(x[0] - x[-1])
    x[-1] = x[0] - np.sign(x[0] - x[-1]) * step * D.undb(excess_db)
    return x


def hf_burst(excess_db):
    """A 330 Hz tone over steady noise above 6 kHz, with a 1 ms burst of the same noise at a logged splice (at 1 s)
    that lifts the band by about excess_db. Returns (audio, splices)."""
    n = 2 * SR
    r = D.rng('fixture', 'splice')
    hiss = D.bfilt(r.standard_normal(n), 7000, 'highpass', order=8)
    w = int(0.001 * SR)
    burst = np.zeros(n)
    burst[SR - w // 2:SR + w // 2 + 1] = np.sqrt(D.undb(excess_db) ** 2 - 1)
    x = 0.4 * np.sin(2 * np.pi * 330 * _t(n)) + 0.01 * hiss * (1 + burst)
    return x, (SR,)


def with_alias(level_db, f0=1000.0, dur=1.0):
    """A sine at f0 with an inharmonic tone at 1.5 f0, level_db under it: the alias check reads about level_db."""
    return tone(f0, dur) + tone(1.5 * f0, dur, 0.5 * D.undb(level_db))


def low_side_at(level_db):
    """A 50 Hz sub with a correlated top, its low end level_db wider (side against mid under 120 Hz)."""
    n = 2 * SR
    t = _t(n)
    low = 0.5 * np.sin(2 * np.pi * 50 * t)
    top = 0.2 * D.bfilt(D.rng('fixture', 'side').uniform(-1, 1, n), (2000, 8000), 'bandpass')
    s = D.undb(level_db) * low
    return D.fade(np.stack([low + top + s, low + top - s], 1), 0.01, 0.01)


def low_share_at(share_db):
    """A 1 kHz tone with a 50 Hz tone under it holding share_db of the energy (what the low cut measures)."""
    r = D.undb(share_db) ** 2
    return D.fade(tone(1000.0, 2.0) + tone(50.0, 2.0, 0.5 * np.sqrt(r / (1 - r))), 0.01, 0.01)


def stab(hp_hz=None):
    """A saw stab on F#2 A2 C#3, root 92.5 Hz (the F#1 sub's second harmonic), optionally high-passed at hp_hz (two
    LR4s, 8th order). A 100 Hz high-pass leaves the 92.5 Hz root only about 15 dB down: a stab voiced on F#2 needs it
    a little higher."""
    n = int(1.0 * SR)
    x = sum(D.polyblep_saw(float(D.midi_hz(m)), n) for m in (42, 45, 49)) * D.env_exp(n, 0.25, attack=0.002)
    x = D.bfilt(x, 5000, 'lowpass')
    if hp_hz:
        x = D.lr4(D.lr4(x, hp_hz, 'highpass'), hp_hz, 'highpass')
    return master(x)


def vocal_band(top_db, dur=1.0):
    """Noise shaped like a vocal master's top: flat from 300 Hz to 11 kHz, then a band at 14-20 kHz holding top_db of
    the 6-11 kHz band's energy (hf_band reads top_db); top_db None leaves 11-24 kHz empty."""
    n = int(dur * SR)
    X = np.fft.rfft(D.rng('fixture', 'vocal').standard_normal(n))
    f = np.fft.rfftfreq(n, 1 / SR)
    ref, top = (f >= 6000) & (f < 11000), (f >= 14000) & (f < 20000)
    g = ((f >= 300) & (f < 11000)).astype(float)
    if top_db is not None:
        g[top] = np.sqrt(D.undb(top_db) ** 2 * ref.sum() / top.sum())
    return to_ceiling_fade(np.fft.irfft(X * g, n))


def to_ceiling_fade(x):
    """10 ms fades, then the true peak at the ceiling."""
    return D.to_ceiling(D.fade(x, 0.01, 0.01))


def resampled_vocal(dur=1.0):
    """What D4 bans: a 24 kHz take (here band-limited noise, as Kokoro's output is) resampled 2x to 48 kHz."""
    n = int(dur * 24000)
    sos = signal.butter(8, (300, 11000), 'bandpass', fs=24000, output='sos')
    x24 = signal.sosfilt(sos, D.rng('fixture', 'tts').standard_normal(n))
    return to_ceiling_fade(signal.resample_poly(x24, 2, 1))
