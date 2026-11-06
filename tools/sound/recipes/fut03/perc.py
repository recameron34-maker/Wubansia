"""FUT03 perc for round 2: the metal pling, the kit's signature perc.

- MetalPling: a struck glass bell on the kit's ratios (kits/fut03.py: prime, tierce, quint and nominal tuned like a
  minor-third bell, then three upper modes stretched sharp like a stiff glass shell). Struck on G, C or D, its four
  low modes ring a minor chord inside G minor (G-Bb-D, C-Eb-G, D-F-A), so the pling can play a little line and stay
  in the key. Zones G4, C5, D5 and G5; 5 velocity layers, 2 round robins.
  The strike is three layers locked to one onset: the bell's modes, an FM "pl" on its top mode (the modulator on the
  nominal, the index falling in 2-5 ms) and a tick of noise. The modes start sharp and settle (the pitch envelope).
  Harder hits strike shorter, so more of the upper modes light up; they also get more FM, a deeper pitch drop and a
  brighter tick. A round robin strikes the bell at another point (a slightly different balance of its modes) and
  sits up to 0.8 cents off.
  The width is the upper modes only: the side is those modes with their own phases, so the mono sum is the bell whole
  and the prime, which carries the pitch, is dead centre. High-passed at 150 Hz, mono under 150 Hz.
"""
import numpy as np

from tools.sound import dsp48 as D
from tools.sound import parts as P
from tools.sound.item import Item
from tools.sound.kits import fut03 as K

SR = D.SR
RATIOS = np.array(K.RATIOS)
PLING_NOTES = (K.ROOT + 24, K.ROOT + 29, K.ROOT + 31, K.ROOT + 36)   # G4, C5, D5, G5: strikes whose triad is in key
PLING_LAYERS = (0.2, 0.4, 0.6, 0.8, 1.0)
# per mode: decay at G4 (s) and level. The prime is the loudest and the longest, so it carries the pitch; the tierce
# (1.1892, inside the tuning band) stays under it from the first ms
MODE_DECAY = (0.60, 0.48, 0.40, 0.45, 0.22, 0.16, 0.09)
MODE_LEVEL = (1.00, 0.55, 0.45, 0.60, 0.40, 0.32, 0.26)
FADE = (1.3, 0.7)            # a raised-cosine fade: gone by 2.0 s, whatever the zone
SIDE_MODES = 4               # modes from this index on (the stretched upper three) also make the side
SIDE_GAIN = 0.7              # the side against those modes in the mid


def pling(take):
    v = take.vel
    r = take.rng('pling')
    f0 = take.f0 * 2 ** (r.uniform(-0.8, 0.8) / 1200)
    n = int(2.1 * SR)
    t = P.times(n)
    scale = (float(D.midi_hz(PLING_NOTES[0])) / f0) ** 0.3        # higher zones ring a little shorter
    decays = [d * scale for d in MODE_DECAY]
    # the strike: shorter on harder hits (more upper modes), at a point the round robin picks
    pos = r.uniform(0.15, 0.35)
    k = np.arange(len(RATIOS))
    where = 1.0 + 0.18 * np.cos(np.pi * pos * (k + 1))
    amps = np.array(MODE_LEVEL) * where * P.hann_pulse_gain(f0 * RATIOS, P.lerp((0.0016, 0.0005), v)) \
        / P.hann_pulse_gain(f0, P.lerp((0.0016, 0.0005), v))
    bend = (0.003 + 0.012 * v) * np.exp(-t / 0.008)
    ph = D.rng(take.kit, take.sound, 'modes')                       # the same mode phases on every take of a zone
    body = P.modes(n, f0, RATIOS, decays, amps, bend, ph)
    upper = np.zeros(len(RATIOS))
    upper[SIDE_MODES:] = amps[SIDE_MODES:]
    side = P.modes(n, f0, RATIOS, decays, upper, bend, D.rng(take.kit, take.sound, 'side phases'))

    # FM "pl": carrier on the top mode, modulator on the nominal, its index falling fast; capped under 20 kHz
    fc, fm = f0 * RATIOS[-1], f0 * RATIOS[3]
    idx = min((20000.0 - fc) / fm - 1.0, 0.8 + 2.4 * v) * np.exp(-t / (0.002 + 0.003 * v))
    ph_fm = 2 * np.pi * D.phase(fc * (1 + bend), n)
    strike = np.sin(ph_fm + idx * np.sin(2 * np.pi * D.phase(fm * (1 + bend), n))) * D.env_exp(n, 0.010 + 0.010 * v)
    tick = P.hp_noise(r, n, 5000.0, order=2) * D.env_exp(n, 0.0006 + 0.0006 * v, 0.0001)

    mid = body + (0.10 + 0.25 * v) * strike + (0.03 + 0.10 * v) * tick
    y = np.stack([mid + SIDE_GAIN * side, mid - SIDE_GAIN * side], 1) * P.raised(n, 0.0002)[:, None]
    y = D.mono_below(D.bfilt(y, 150.0, 'highpass', order=4), 150.0)
    return y * D.taper(n, *FADE)[:, None] * P.vel_gain(v, 14.0)


ITEMS = [
    Item('Perc', 'MetalPling', 'perc', pling, layers=PLING_LAYERS, rr=2, notes=PLING_NOTES, sets=('r2', 'r3'),
         about='metal pling: a struck glass bell on the kit ratios (its low modes a minor triad), an FM "pl" and a '
               'tick; zones G4 C5 D5 G5, each ringing a chord in G minor'),
]
