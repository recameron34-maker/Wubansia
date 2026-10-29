"""Python stand-ins of the engine's sampler voice and bass lane filter, for written phrases and context loops: what the
engine does with a held note (play, loop while the key is down, release), a CC lane as the engine receives it, and
the lane filter (an SVF low-pass on CC74, a formant pair on CC1, the lane's drive) over a sub that passes untouched.

They came out of FUT01's recipes (bass.py) unchanged, so FUT01 renders the same bytes; the lane filter's mid cut has no
default, since it decides which low-end rule the bass can pass. Like sends.py, they are targets the engine must match
(S1e).
"""
import numpy as np
from scipy import signal

from tools.sound import dsp48 as D
from tools.sound.parts import hp_steep, raised

SR = D.SR
# the lane's vowel path: (CC1, F1, F2), Hz
VOWELS = ((0, 520.0, 900.0), (42, 650.0, 1080.0), (85, 760.0, 1500.0), (127, 560.0, 2200.0))


def sampler_voice(out, n_on, attack=0.002, release=0.02):
    """What the engine's sampler does with one held note: play from the start, loop while the key is down (n_on
    samples), then a raised-cosine release. out: an Out with its loop points."""
    a, b = out.loop
    m = n_on + int(release * SR)
    i = np.arange(m)
    y = out.audio[np.where(i < b, i, a + (i - a) % (b - a))]
    return y * (raised(m, attack) * D.taper(m, n_on / SR, release))[:, None]


def cc_curve(points, L, step, smooth=0.008):
    """A CC lane as the engine gets it: breakpoints (16th, value) joined linearly, sent as 7-bit values, then the
    parameter smoother (one-pole, `smooth` seconds). step: the kit's 16th, in seconds. Circular over the loop of L
    samples, so the curve joins at the seam."""
    steps = np.array([p[0] for p in points], dtype=float) * step * SR
    raw = np.round(np.interp(np.arange(L), steps, [p[1] for p in points]))
    a = np.exp(-1.0 / (smooth * SR))
    return signal.lfilter([1 - a], [1, -a], np.tile(raw, 2), zi=[a * raw[-1]])[0][L:]


def formants(cc1, path=VOWELS):
    """F1 and F2 per sample along a vowel path, interpolated in log frequency."""
    c = [p[0] for p in path]
    return tuple(2 ** np.interp(cc1, c, np.log2([p[j] for p in path])) for j in (1, 2))


def lane_filter(x, cc74, cc1, *, mids_hp, drive=1.8, path=VOWELS, xover=120.0):
    """The engine's bass lane filter, a stand-in. Under `xover` the sub passes untouched (LR4 crossover, so the bands
    sum flat). Above it: a TPT SVF low-pass on CC74 (300 Hz to 19 kHz), a pair of formant band-passes moved along the
    vowel path by CC1, and the lane's drive at 4x. mids_hp, required: the steep high-pass after the drive. 450 Hz
    (FUT01's A, clean) keeps 150-400 Hz empty for the strict mud gate, since the drive's difference tones would land in
    the low mids, and leaves the bass hollow; None keeps the mids' own low end, for a B, warm bass under the bass-body
    rule. x: (n, 2); cc74 and cc1 one value per sample."""
    lo = D.lr4(x, xover, 'lowpass')
    hi = D.lr4(x, xover, 'highpass')
    fc = 300.0 * 2 ** (6.0 * cc74 / 127.0)
    f1, f2 = formants(cc1, path)
    out = []
    for ch in range(hi.shape[1]):
        lp = D.svf_tpt(hi[:, ch], fc, q=1.6)[0]
        b1 = D.svf_tpt(lp, f1, q=5.0)[1] / 5.0     # SVF band-pass peaks at q: scaled to unity
        b2 = D.svf_tpt(lp, f2, q=8.0)[1] / 8.0
        out.append(0.12 * lp + 1.2 * b1 + 0.9 * b2)
    hi = D.oversampled(lambda u: D.tanh_adaa(u, drive), np.stack(out, 1), 4)
    return lo + (hp_steep(hi, mids_hp) if mids_hp else hi)
