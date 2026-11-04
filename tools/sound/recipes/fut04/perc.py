"""FUT04's signature perc for round 2: the anvil, a pitched clank on the kit's iron ratios, on E5.

- Anvil: the seven modes of the kit's anvil slab (kits/fut04.py: RATIOS) struck on E5 (659.26 Hz, the tonic three
  octaves over the bass root). The two bending-and-twist pairs beat: 1.5719 against 1.6124 (C6 a few cents flat, the
  minor sixth over E, 44 cents apart, about 27 Hz of warble) and 3.2247 against 3.3671 (75 cents apart, a rougher
  roll). A hammer strike rides on the front: a 2 ms click of noise over 3 kHz and the first 40 ms of the modes
  driven through a biased tanh at 4x (the clank), high-passed at 900 Hz. A little of the kit's dark concrete room
  sits behind it. Harder hits strike shorter (a Hann pulse of 1.0 to 0.25 ms, so more upper modes ring), bend the
  pitch further (0.2 to 1.2 % sharp, settling in 8 ms), drive the clank harder and ring a little longer.

Every take strikes the same mode phases (D.rng of the kit and item), so the round robins differ by their click, their
1.5 cents of pitch and their scrape only. It is high-passed at 250 Hz (4th order) and mono under 150 Hz.
"""
import numpy as np

from tools.sound import dsp48 as D
from tools.sound import parts as P
from tools.sound.item import Item
from tools.sound.kits import fut04 as K

SR = D.SR
LAYERS = (0.2, 0.4, 0.6, 0.8, 1.0)
RATIOS = np.array(K.RATIOS)
ANVIL_NOTE = K.ROOT + 36     # E5, 659.26 Hz
MODE_AMPS = (1.0, 0.75, 0.75, 0.55, 0.45, 0.45, 0.35)   # each pair at one level, so it beats
MODE_DECAYS = (0.45, 0.26, 0.24, 0.14, 0.10, 0.09, 0.06)   # seconds, at velocity 1 (x 0.8 at the softest)
STRIKE = (0.0010, 0.00025)   # the Hann strike's width (s), softest to hardest
CLANK = (2.0, 6.0, 0.50)     # the clank's drive, softest and hardest, and its level
ROOM = dict(t60=1.4, size=0.5, damp_hz=5000.0, send_hp=800.0, level=0.18)
FADE = (1.1, 0.5)            # the tail's raised-cosine fade (start, length), s: about 40 dB down by then


def anvil(take):
    v = take.vel
    r = take.rng('anvil')
    n = int(1.8 * SR)
    t = P.times(n)
    f0 = take.f0 * 2 ** (r.uniform(-1.5, 1.5) / 1200)

    # the modes: a Hann strike sets how far up they ring; each side 0.03 % apart (the same phases on both sides and
    # on every take), so the ring widens as it decays
    fk = f0 * RATIOS
    amps = P.hann_pulse_gain(fk, P.lerp(STRIKE, v)) * np.array(MODE_AMPS)
    decays = [d * (0.8 + 0.2 * v) for d in MODE_DECAYS]
    bend = (0.002 + 0.010 * v) * np.exp(-t / 0.008)
    sides = [P.modes(n, f0 * c, RATIOS, decays, amps, bend, D.rng(take.kit, take.item.name, 'phases'))
             for c in (0.9997, 1.0003)]
    body = np.stack(sides, 1) * D.env_exp(n, 1.0, 0.0003)[:, None]

    # the strike: a click of noise over 3 kHz, and the first 40 ms of the modes driven (the clank), over 900 Hz
    click = P.hp_noise(r, n, 3000.0, order=2) * D.env_exp(n, 0.0020, 0.0002) * (0.15 + 0.50 * v)
    lo, hi, g_clank = CLANK
    drive = P.lerp((lo, hi), v)
    head = D.mono(body) * P.gate(t, 0.015, 0.015) * D.taper(n, 0.040, 0.020)
    clank = D.oversampled(lambda u: D.tanh_adaa(u, drive, bias=0.2), head, 4)
    clank = D.bfilt(D.dc_block(clank), 900.0, 'highpass', order=4) * g_clank * (0.4 + 0.6 * v)
    scrape = (0.02 + 0.06 * v) * P.hp_noise(take.rng('anvil', 'scrape'), n, 6000.0, order=2) * D.env_exp(n, 0.012)
    hit = body + D.st(click + clank + scrape)

    # the kit's dark concrete room, fed above 800 Hz
    send = D.bfilt(D.mono(hit), ROOM['send_hp'], 'highpass', order=4)
    room = D.fdn_reverb(send, K.space_rng('anvil room'), t60=ROOM['t60'], size=ROOM['size'],
                        damp_hz=ROOM['damp_hz'])[:n]
    rm, rs = D.ms(room[:, 0], room[:, 1])
    room = np.stack([rm + 0.6 * rs, rm - 0.6 * rs], 1)
    y = hit + ROOM['level'] * room
    y = D.bfilt(y, 250.0, 'highpass', order=4) * D.taper(n, *FADE)[:, None]
    return D.mono_below(y, 150.0) * P.vel_gain(v, 12.0)


ITEMS = [
    Item('Perc', 'Anvil', 'perc', anvil, layers=LAYERS, rr=2, root=ANVIL_NOTE, sets=('r2',),
         about='anvil on E5: the seven modes of the kit\'s iron slab, two beating pairs, a driven clank and a click on '
               'the strike, a little of the dark concrete room'),
]
