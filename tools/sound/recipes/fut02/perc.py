"""FUT02 perc for rounds 2 and 3: the zap, a pitch-dropping laser tuned to D5, and the laser gesture it shares with the
fill and the riser (fx.py).

The gesture (laser()) is the kit's signature move: a tone on the stiff wire's partials that falls a few octaves onto a
note of the key and lands exactly on it, the fall fast at first and easing in (its pitch curve is (1 - u)^k in
octaves, so it reaches the note with no glide left over). Partials that would pass 19 kHz on the way down fade out
before they get there, so nothing aliases. An FM bite (the modulator on the wire's second partial) and a spark of noise
give it its edge on the front. Its width (widen()) is a side made of the zap itself 1.1 ms later, above 1 kHz: the mono
sum is the zap whole. Round 3's first pass doubled that side through an all-pass instead; the all-pass's direct term
(-0.6 times its input) put an anti-phase copy of the zap on the side, so every zap leaned 4-7 dB to the right above
1.6 kHz (round 2 at its narrower width: 2.7-4.5 dB) under a deep comb in each channel. The width is back to round 2's
(ZAP_WIDTH), and a plain delay has no direct term: above 3 kHz the zap's left and right are within 0.2 dB (a static
side still places single partials a little: the D5's second partial, near 1.2 kHz, reads 2 dB to the left, as in
round 2). In the context loop the four zaps take their place across the field by their pans.
"""
import numpy as np

from tools.sound import dsp48 as D
from tools.sound import parts as P
from tools.sound.item import Item
from tools.sound.kits import fut02 as K

SR = D.SR
RATIOS = np.array(K.RATIOS)
LAYERS = (0.2, 0.4, 0.6, 0.8, 1.0)
ZAP_NOTE = K.ROOT + 36       # D5, 587.33 Hz: the key's root, where the example ping rang
F_TOP = 19000.0              # partials fade out on their way over this (Hz), within a 1.5 kHz band
SIDE_DELAY = 53              # the side's delay, samples (about 1.1 ms)
ZAP_WIDTH = (0.15, 0.25)     # the zap's side above 1 kHz, a + b x velocity: round 2's, the side 8-14 dB under the mid
# the zap is mostly its fall: 67-85% of its energy comes before it lands (the first render fell in 40-64 ms and rang
# on, so 40-62% did, and it read as a chirp on a ping). It lands before 0.10 s, where QA starts tuning it to D5
ZAP_DROP = (0.100, 0.075)    # the fall's time, soft to hard (s): 95 ms at the softest layer, 75 ms at the hardest
ZAP_CURVE = 1.8              # its shape: (1 - u)^curve octaves, so the fall stays audible to the end
ZAP_RING = (0.10, 0.04)      # the decay, tau = a + b x velocity (s): the landed D5 about 20 dB down by 0.23-0.31 s
ZAP_END = (0.30, 0.15)       # then a raised-cosine fade to exactly 0 at 0.45 s


def laser(n, f_to, depth_oct, drop_s, curve=2.2, amps=(1.0, 0.45, 0.28, 0.16), fm=0.0, fm_tau=0.02, k=4):
    """The kit's laser gesture, mono: a tone falling depth_oct octaves onto f_to over drop_s seconds and holding there,
    its partials on the wire's ratios (amps: partials 1, 2, ...), each faded out wherever it would pass F_TOP. fm: an
    FM index on the fundamental (modulator on the wire's second partial) that decays with fm_tau. Rendered at k x, so
    the FM's upper sidebands never fold back. No envelope: the caller shapes it."""
    t = np.arange(n * k) / (SR * k)
    u = np.clip(t / drop_s, 0.0, 1.0)
    f = f_to * 2.0 ** (depth_oct * (1.0 - u) ** curve)
    return wire_tone(f, amps, fm * np.exp(-t / fm_tau) if fm else None, k)


def wire_tone(f, amps, fm=None, k=4):
    """A tone on the wire's ratios following f (Hz, one value per sample at k x SR), mono at SR: partial i at amps[i],
    each faded out wherever it would pass F_TOP; fm (one value per sample at k x, or None): an FM index on the
    fundamental, the modulator on the wire's second partial. The laser falls on it; the riser's laser rises on it."""
    ph = 2 * np.pi * np.concatenate([[0.0], np.cumsum(f[:-1] / (SR * k))])
    mod = fm * np.sin(RATIOS[1] * ph) if fm is not None else 0.0
    y = np.zeros(len(f))
    for i, (a, rt) in enumerate(zip(amps, RATIOS)):
        y += a * np.clip((F_TOP - rt * f) / 1500.0, 0.0, 1.0) * np.sin(rt * ph + (mod if i == 0 else 0.0))
    return D.down(y, k)


def spark(r, n, v):
    """A 2-4 ms spark of noise over 4 kHz: the laser's front edge."""
    return (0.10 + 0.25 * v) * P.hp_noise(r, n, 4000.0, order=2) * D.env_exp(n, 0.0012 + 0.0012 * v, 0.0002)


def widen(y, amount):
    """Mono to stereo: the side is the sound SIDE_DELAY samples later, above 1 kHz, so the mono sum is the sound
    whole. A plain delay's response has a flat magnitude and no direct term, so the sound leans to neither side (an
    all-pass's -a at lag 0 put an anti-phase copy on the side and leaned it right)."""
    late = np.concatenate([np.zeros(SIDE_DELAY), y[:len(y) - SIDE_DELAY]])
    side = amount * D.bfilt(late, 1000.0, 'highpass', order=2)
    return np.stack([y + side, y - side], 1)


def zap(take):
    """A laser falling onto D5: harder hits start higher (1.6 to 3 octaves up), fall faster (95 to 75 ms), bite with
    more FM, light more partials and ring a little longer (a decay of 0.11-0.14 s). The round robin nudges the fall's
    depth and time, and its spark."""
    v = take.vel
    r = take.rng('zap')
    n = int(0.9 * SR)
    t = P.times(n)
    depth = P.lerp((1.6, 3.0), v) + 0.05 * r.uniform(-1, 1)
    drop = P.lerp(ZAP_DROP, v) * (1 + 0.03 * r.uniform(-1, 1))
    amps = (1.0, 0.25 + 0.35 * v, 0.10 + 0.30 * v, 0.03 + 0.22 * v)
    y = laser(n, take.f0, depth, drop, curve=ZAP_CURVE, amps=amps, fm=0.4 + 1.6 * v, fm_tau=0.015 + 0.010 * v)
    y *= D.env_exp(n, ZAP_RING[0] + ZAP_RING[1] * v, 0.0004) * D.taper(n, *ZAP_END)
    y += spark(r, n, v)
    y = D.bfilt(y, 200.0, 'highpass', order=4)
    return D.mono_below(widen(y, ZAP_WIDTH[0] + ZAP_WIDTH[1] * v), 150.0) * P.vel_gain(v, 12.0)


ITEMS = [
    Item('Perc', 'Zap', 'perc', zap, layers=LAYERS, rr=2, root=ZAP_NOTE, sets=('r2', 'r3', 'r5'),
         about="laser zap: wire partials falling 1.6-3 octaves onto D5 in 75-95 ms, most of its energy in the fall, "
               "with an FM bite and a spark; the kit's pitch-drop gesture"),
]
