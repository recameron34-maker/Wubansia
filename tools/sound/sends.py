"""Python versions of the engine's planned sends (plan section 2.4), as references for S1e.

- space(): send A, "Space". An 8-line FDN with an octave-up shimmer in its feedback, behind a pre-delay and a
  high-passed send. duck() lowers its return while the Vox bus plays.
- echo(): send B, "Echo". A tape ping-pong on a dotted 8th, with LP 2.6 kHz and HP 300 Hz in the feedback, a soft
  saturation and a little wow and flutter.
- resonator(): a stand-in for send C, the "Alloy resonator": a bank of 2-pole resonators on the chord's tones times the
  kit's alloy ratios. The engine's version retunes on the timeline's chords; the context loop holds one chord.

Both run the way the engine will: fixed feedback loops processed in blocks no longer than their shortest delay, so a
block only reads samples already written. Mono in, stereo out, the return only (no dry signal). The context loop's
wet version (recipes/fut01/context.py) renders through them, and the engine's sends must match it. Nothing here
is tied to a kit: the caller passes the kit's space_rng() and its tempo's delay.

Provenance: space() extends dsp48.fdn_reverb (the S1 research prototype's FDN, 2026-10-29) with the shimmer path. The
echo's voicing (feedback 0.38, LP 2.6 kHz / HP 300 Hz in the loop) is the legacy web pack's
(legacy/web-pack/app.js:225-229); its return levels (0.32 and 0.42) are not, because its reverb was a normalised
convolver: here the returns sit at +4 dB and the sends carry the mix (at unity they left the wet loop 19 LU under the
dry one, so nearly dry).
"""
import numpy as np
from scipy import signal
from scipy.linalg import hadamard

from tools.sound import dsp48 as D

SR = D.SR
FDN_DELAYS = (1153, 1327, 1523, 1741, 1999, 2297, 2593, 2953)   # samples at size 1.0, as dsp48.fdn_reverb
OUT_L = np.array([1, -1, 1, 1, -1, 1, -1, -1])
OUT_R = np.array([1, 1, -1, -1, 1, 1, -1, -1])   # orthogonal to OUT_L: an uncorrelated pair

# the defaults the context loop uses: the starting values for the engine's sends. The send is high-passed at 450 Hz
# (4th order) and the low mids decay in 0.8 s, so louder tails stay out of the 150-400 Hz band
SPACE = dict(t60=2.6, size=1.0, damp_hz=6500.0, predelay=0.020, send_hp=450.0, send_order=4, low_hz=500.0,
             t60_low=0.8, shimmer=0.06, shimmer_hp=300.0, shimmer_lp=7000.0, window=2048, mono_hz=150.0)
SPACE_RETURN_DB = 3.0
DUCK = dict(depth_db=6.0, range_db=18.0, attack=0.010, release=0.300)
ECHO_STEPS = 3                # a dotted 8th, in 16ths
ECHO = dict(feedback=0.38, lp_hz=2600.0, hp_hz=300.0, drive=1.5, wow=0.0004, wow_hz=0.5, flutter=0.00015,
            flutter_hz=4.7)
ECHO_RETURN_DB = 3.0
RESONATOR = dict(tau=0.4, send_hp=600.0)
RESONATOR_RETURN_DB = -21.0


# ------------------------------------------------------------------ Space
def octave_taps(t, window):
    """The two read taps of a delay-line octave shifter at absolute sample times t: (delay in samples, weight) each.
    The delays fall by one sample per sample, so the taps read at twice the speed (an octave up), and each jumps back
    to `window` where its sin^2 weight is zero; the two weights, half a window apart, always sum to 1."""
    taps = []
    for off in (0, window // 2):
        d = window - (t + off) % window
        taps.append((d, np.sin(np.pi * d / window) ** 2))
    return taps


def space(x, r, t60=2.6, size=1.0, damp_hz=6500.0, predelay=0.020, send_hp=250.0, send_order=2, low_hz=500.0,
          t60_low=None, shimmer=0.06, shimmer_hp=300.0, shimmer_lp=7000.0, window=2048, mono_hz=150.0, n_tail=0):
    """Send A: the send high-passed at send_hp (no reverb in the low end), a pre-delay, then an 8-line FDN (Hadamard
    matrix, one-pole damping per line, line gains for t60 seconds). t60_low: the band under low_hz (a one-pole split in
    each line) decays in t60_low seconds instead, so low mids don't hang in the tail. The shimmer: each line's damped
    output, band-passed
    to shimmer_hp-shimmer_lp, goes through an octave-up shifter (octave_taps); the line feeds back sqrt(1 - shimmer^2)
    of itself plus `shimmer` of its octave, which keeps the loop's power (the two are uncorrelated), so every trip
    round the loop lifts a little of the tail an octave without lengthening it. The band keeps the loop stable: the
    shifter's crossfade spreads its output about +-2 SR/window around 2f, so under ~50 Hz a tone would feed itself
    (it ran away at shimmer 0.3 without the high-pass); above shimmer_hp energy only moves up, to be damped, and under
    shimmer_lp the octave can't fold back. Under mono_hz the return is mono. r sets the delay jitter
    and the input signs: pass the kit's space seed."""
    x = D.bfilt(D.mono(x), send_hp, 'highpass', order=send_order)
    pd = int(round(predelay * SR))
    n = len(x) + n_tail
    xin = np.zeros(n)
    xin[pd:min(n, pd + len(x))] = x[:max(0, n - pd)]
    d = np.round(np.array(FDN_DELAYS) * size * (1 + 0.03 * r.uniform(-1, 1, 8))).astype(int)
    inj = r.choice([-1, 1], 8) / np.sqrt(8)
    Hm = hadamard(8) / np.sqrt(8)
    g = 10 ** (-3 * d / (t60 * SR))
    g_low = 10 ** (-3 * d / (t60_low * SR)) if t60_low else g
    a = np.exp(-2 * np.pi * damp_hz / SR)
    a_low = np.exp(-2 * np.pi * low_hz / SR)
    z_low = np.zeros((8, 1))
    dmax, B = int(d.max()), int(d.min())
    lines = np.zeros((8, n + dmax))
    z_damp = np.zeros((8, 1))
    sos = signal.butter(4, (shimmer_hp, shimmer_lp), 'bandpass', fs=SR, output='sos')
    z_lp = np.zeros((sos.shape[0], 8, 2))
    hist = np.zeros((8, n + window))       # the shifter's input, with `window` samples of silence before the start
    for s in range(0, n, B):
        e = min(s + B, n)
        taps = np.stack([lines[i, dmax + s - d[i]:dmax + e - d[i]] for i in range(8)])
        taps, z_damp = signal.lfilter([1 - a], [1, -a], taps, axis=1, zi=z_damp)
        if t60_low:
            lo, z_low = signal.lfilter([1 - a_low], [1, -a_low], taps, axis=1, zi=z_low)
            taps = taps + (g_low / g - 1)[:, None] * lo
        fb = taps
        if shimmer:
            h, z_lp = signal.sosfilt(sos, taps, axis=1, zi=z_lp)
            hist[:, window + s:window + e] = h
            t = np.arange(s, e)
            up = sum(w * hist[:, window + t - dk] for dk, w in octave_taps(t, window))
            fb = np.sqrt(1 - shimmer ** 2) * taps + shimmer * up
        lines[:, dmax + s:dmax + e] = Hm @ (fb * g[:, None]) + inj[:, None] * xin[s:e][None, :]
    out = lines[:, dmax:]
    y = np.stack([OUT_L @ out, OUT_R @ out], 1) / np.sqrt(8)
    return D.mono_below(y, mono_hz) if mono_hz else y


def duck(x, key, depth_db=6.0, range_db=18.0, attack=0.010, release=0.300, block=64):
    """A return that ducks under a bus: the key's level (block RMS, then attack/release in dB) from range_db under the
    key's own loudest block up to that block maps to 0..depth_db of gain reduction. Returns (x ducked, gain in dB per
    sample)."""
    k = D.mono(key)
    nb = -(-len(k) // block)
    p = np.concatenate([k ** 2, np.zeros(nb * block - len(k))]).reshape(nb, block).mean(1)
    lv = 10 * np.log10(p + 1e-20)
    a_att, a_rel = np.exp(-block / (attack * SR)), np.exp(-block / (release * SR))
    s, sm = lv.min(), np.empty(nb)
    for i, v in enumerate(lv.tolist()):
        c = a_att if v > s else a_rel
        s = c * s + (1 - c) * v
        sm[i] = s
    top = lv.max()
    gr = -depth_db * np.clip((sm - (top - range_db)) / range_db, 0.0, 1.0)
    g = np.interp(np.arange(len(k)), (np.arange(nb) + 0.5) * block - 0.5, gr)
    return x * (D.undb(g)[:, None] if np.ndim(x) == 2 else D.undb(g)), g


# ------------------------------------------------------------------ Alloy resonator (stand-in)
def resonator(x, freqs, tau=0.4, send_hp=600.0):
    """Send C, a stand-in: the send high-passed at send_hp, into one 2-pole resonator per frequency (amplitude falls
    by e in tau seconds), each scaled so white noise comes out at the level it went in; modes alternate left and right
    of centre. Mono in, stereo out, the return only."""
    x = D.bfilt(D.mono(x), send_hp, 'highpass', order=2)
    rad = np.exp(-1.0 / (tau * SR))
    out = np.zeros((len(x), 2))
    for i, f in enumerate(sorted(freqs)):
        w = 2 * np.pi * f / SR
        a = [1.0, -2 * rad * np.cos(w), rad * rad]
        y = signal.lfilter([1.0], a, x) * np.sqrt(2 * (1 - rad * rad)) * np.sin(w)   # unity power gain on noise
        p = 0.3 if i % 2 else -0.3
        out += y[:, None] * np.array([np.sqrt(0.5 - 0.5 * p), np.sqrt(0.5 + 0.5 * p)]) * np.sqrt(2)
    return out


# ------------------------------------------------------------------ Echo
def _read(v, pos):
    """v at fractional positions (linear interpolation); silence before sample 0."""
    i = np.floor(pos).astype(int)
    fr = pos - i
    ok = i >= 0
    i = np.where(ok, i, 0)
    return np.where(ok, (1 - fr) * v[i] + fr * v[i + 1], 0.0)


def echo(x, r, delay_s, feedback=0.38, lp_hz=2600.0, hp_hz=300.0, drive=1.5, wow=0.0004, wow_hz=0.5,
         flutter=0.00015, flutter_hz=4.7, n_tail=0):
    """Send B: a tape ping-pong. Two delay lines of delay_s, cross-fed: the input enters the left line; each repeat
    reads its line (the delay wobbles by wow and flutter, as a fraction of the time), soft-saturates (tanh, drive),
    passes LP lp_hz and HP hp_hz (2nd-order Butterworth), sounds on its side and feeds the other line at `feedback`.
    So repeat k sounds on the left for odd k and the right for even k, at feedback^(k-1), filtered k times: darker and
    thinner each time. r sets the wow and flutter phases. Returns the stereo return."""
    x = D.mono(x)
    n = len(x) + n_tail
    xin = np.zeros(n)
    xin[:len(x)] = x[:n]
    t = np.arange(n) / SR
    p1, p2 = r.uniform(0, 2 * np.pi, 2)
    wobble = wow * np.sin(2 * np.pi * wow_hz * t + p1) + flutter * np.sin(2 * np.pi * flutter_hz * t + p2)
    dly = delay_s * SR * (1 + wobble)
    B = int(np.floor(dly.min())) - 1
    sos = np.vstack([signal.butter(2, lp_hz, 'lowpass', fs=SR, output='sos'),
                     signal.butter(2, hp_hz, 'highpass', fs=SR, output='sos')])
    za, zb = np.zeros((2, 2)), np.zeros((2, 2))
    a, b, out = np.zeros(n + 1), np.zeros(n + 1), np.zeros((n, 2))
    sat = lambda u: np.tanh(drive * u) / drive
    for s in range(0, n, B):
        e = min(s + B, n)
        pos = np.arange(s, e) - dly[s:e]
        ta, za = signal.sosfilt(sos, sat(_read(a, pos)), zi=za)
        tb, zb = signal.sosfilt(sos, sat(_read(b, pos)), zi=zb)
        out[s:e, 0], out[s:e, 1] = ta, tb
        b[s:e] = feedback * ta
        a[s:e] = xin[s:e] + feedback * tb
    return out
