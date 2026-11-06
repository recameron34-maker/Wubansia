"""FUT01 round 4 synth accents (set r4): one-shots to drop into a garage track, each in F# minor on a stated root.

- OrganHit (F#3): a big house organ chord hit, F#m9 over two octaves, drawbars with a percussion partial, a rotary
  shimmer (the sides' level moving at 6 Hz) and a 0.8 s release.
- WarpStab (F#3): the warp bass's voice up in the stab register: the detuned saw pair over a square, driven, scooping
  up a whole tone into F#3 over 40 ms, its resonant filter opening and closing; F#3 and F#4.
- RaveStab (F#3): an original rave-style stab: F#3 F#4 A4 C#5 on three detuned saws each, dropping a semitone into
  pitch over 25 ms, a bright filter blip and a short tail.
- ChordSweep (F#3): F#m9 on detuned saws through a resonant low-pass that sweeps down from 9 kHz to 400 Hz over a
  second: a chord hit with a filter sweep.
- ReverseStab (F#3): the organ hit with its own tail and the kit's plate, reversed: it swells into the beat and ends
  on it (cut_end).
- SubDrop (F#2): a clean sine on F#2 that holds 120 ms, then drops two octaves over 1.4 s, a soft thump on its
  onset: the drop into a section.
- AlloyBell (C#6): the kit's alloy hit, struck on the fifth two octaves up, its modes ringing three times longer: the
  metallic accent in the kit's own colour.
- VoxSynthStab (F#3): a sung-vowel synth chord, F#m7 on saws through "oo" opening to "ah" formants (F1 300 to 750,
  F2 870 to 1220, F3 2600 Hz) over 70 ms: a pitched vocal-synth stab, no voice and no words.
- NoiseUplifter (F#): 2 bars of noise through a comb tuned to F# (its resonances on F#'s harmonics) and a band-pass
  rising from 400 Hz to 9 kHz, rising 18 dB, ending on the downbeat (cut_end).

Every tonal accent declares its fundamentals (Out.f0s) for the alias check and voices its root alone in its own band,
so the tuning check reads the root; everything but the sub drop is high-passed far above the kick and mono under 150 Hz.
"""
import numpy as np

from tools.sound import dsp48 as D
from tools.sound import parts as P
from tools.sound import sends as S
from tools.sound.item import Item, Out
from tools.sound.kits import fut01 as K
from tools.sound.recipes.fut01 import bass as Rb, drums as Dr, keys as Ky

SR = D.SR
ROOT3 = 54                          # F#3
LAYERS = (0.6, 1.0)


def hz(note):
    return float(D.midi_hz(note))


def finish(y, hp=150.0):
    """High-passed (4th order) and mono under 150 Hz."""
    return D.mono_below(D.bfilt(D.st(y), hp, 'highpass', order=4), 150.0)


# ------------------------------------------------------------------ the organ hit, and its reverse
ORGAN_HIT = (54, 61, 64, 69, 73, 80)    # F#3 C#4 E4 A4 C#5 G#5: F#m9 over two octaves


def organ_chord(take, n, release):
    """The organ hit before its finish: (n, 2), and its fundamentals."""
    v = take.vel
    t = P.times(n)
    up = (v - LAYERS[0]) / (LAYERS[1] - LAYERS[0])
    y = np.zeros(n)
    f0s = []
    for note in ORGAN_HIT:
        f = hz(note)
        f0s.append(f)
        ph = 2 * np.pi * f * t + D.rng(take.kit, take.item.name, note).uniform(0, 2 * np.pi)
        for h, a in Ky.DRAWBARS:
            if h * f < 16000:
                y += a * (0.7 + 0.3 * up) ** (h - 1) * np.sin(h * ph)
        y += 0.4 * v * np.sin(3 * ph) * np.exp(-t / 0.06)
    y /= len(ORGAN_HIT)
    env = P.raised(n, 0.002) * P.gate(t, 0.12, release)
    rot = 0.18 * np.sin(2 * np.pi * 6.0 * t)                  # the rotary shimmer: the sides' level at 6 Hz
    return np.stack([y * (1 + rot), y * (1 - rot)], 1) * env[:, None], f0s


def organ_hit(take):
    n = int(1.6 * SR)
    y, f0s = organ_chord(take, n, 0.25)
    y = y * D.taper(n, 1.2, 0.35)[:, None]
    return Out(finish(y) * (0.45 + 0.55 * take.vel), f0s=tuple(f0s))


def reverse_stab(take):
    """The organ hit with a long release and the kit's plate (one second of it), reversed: it swells into its end, the
    hit's own attack last, so it lands on the beat where the file ends."""
    n = int(1.3 * SR)
    y, _ = organ_chord(take, n, 0.35)
    plate = S.space(D.mono(y), K.space_rng('reverse'), t60=1.4, size=0.6, damp_hz=8000.0, predelay=0.01,
                    send_hp=300.0)[:n]
    y = (y + 0.5 * plate)[::-1]
    y = y * P.raised(n, 0.25)[:, None]
    return finish(y) * (0.45 + 0.55 * take.vel)


# ------------------------------------------------------------------ the warp stab
def warp_stab(take):
    """The warp voice up in the stab register: F#3 and F#4, each a saw pair (+-9 cents) over a square, driven at 4x,
    scooping up a whole tone over 40 ms; a resonant low-pass that opens with the hit and closes over 120 ms; held
    0.25 s."""
    v = take.vel
    n = int(0.8 * SR)
    t = P.times(n)
    scoop = 2.0 ** (-2.0 / 12.0 * np.exp(-t / 0.012))          # a whole tone under, rising into pitch
    r = D.rng(take.kit, take.item.name, 'phases')
    y = np.zeros((n, 2))
    f0s = []
    for note, g in ((ROOT3, 1.0), (ROOT3 + 12, 0.35)):
        f = hz(note)
        d = f * (2 ** (9.0 / 1200) - 1)
        f0s += [f - d, f, f + d]
        drive = 1.5 + 1.5 * v
        saws = [Rb.saw((f + s * d) * scoop, n, r.uniform(), drive, np.ones(n)) for s in (-1, 1)]
        ph = r.uniform()
        sq = D.down(D.tanh_adaa(0.5 * (D.polyblep_saw(f * scoop, n, ph, k=4)
                                       - D.polyblep_saw(f * scoop, n, (ph + 0.5) % 1.0, k=4)), drive), 4)
        mix = lambda a, b: 0.55 * (0.7 * a + 0.3 * b) + 0.45 * sq
        y += g * np.stack([mix(saws[0], saws[1]), mix(saws[1], saws[0])], 1)
    fc = np.minimum((700.0 + 500.0 * v) * (1.0 + (3.0 + 3.0 * v) * np.exp(-t / 0.12)), 16000.0)
    y = np.stack([D.svf_tpt(y[:, c], fc, q=2.0)[0] for c in range(2)], 1)
    y = y + 0.8 * D.st(np.sin(2 * np.pi * D.phase(hz(ROOT3) * scoop, n)))   # the root's sine, as a stab patch's sub
                                                                            # oscillator: the note reads on its root
    y = y * (P.raised(n, 0.002) * D.taper(n, 0.25, 0.12))[:, None]
    return Out(finish(y, 120.0) * (0.2 + 0.4 * v), f0s=tuple(sorted(set(f0s))))


# ------------------------------------------------------------------ the rave stab
RAVE = (54, 66, 69, 73)             # F#3 F#4 A4 C#5: the root alone under its octave, minor tenth and twelfth
RAVE_DETUNE = (-8.0, 0.0, 8.0)      # cents: three saws a note


def rave_stab(take):
    """Three exactly band-limited saws a note (-8, 0, +8 cents, the centre one leading), the whole chord dropping a
    semitone into pitch over 25 ms; a filter blip (open at 9-13 kHz, down to 2 kHz in 120 ms); about 0.2 s, then a
    smooth release."""
    v = take.vel
    n = int(1.0 * SR)
    t = P.times(n)
    drop = 2.0 ** (1.0 / 12.0 * np.exp(-t / 0.008))
    r = D.rng(take.kit, take.item.name, 'phases')
    sides = np.zeros((n, 2))
    f0s = []
    for note in RAVE:
        for c, cents in enumerate(RAVE_DETUNE):
            if note == RAVE[0] and c != 1:
                continue                                        # the root one saw, undetuned: it reads on its pitch
            f = hz(note) * 2 ** (cents / 1200)
            f0s.append(f)
            osc = D.additive_saw(f * drop, n, r.uniform(), fmax=16000.0)
            pan = (0.7, 0.7) if c == 1 else ((0.35, 0.15) if c == 0 else (0.15, 0.35))   # the centre saw leads
            sides += np.outer(osc, pan)
    sides /= len(RAVE) * 1.5
    fc = 2000.0 + (7000.0 + 4000.0 * v) * np.exp(-t / 0.12)
    y = np.stack([D.svf_tpt(sides[:, c], fc, q=0.75)[0] for c in range(2)], 1)
    y = y * (P.raised(n, 0.001) / (1.0 + (t / 0.2) ** 8) * D.taper(n, 0.6, 0.3))[:, None]   # a smooth release
    return Out(finish(y) * (0.45 + 0.55 * v), f0s=tuple(f0s))


# ------------------------------------------------------------------ the chord sweep
SWEEP_CHORD = (54, 66, 69, 73, 76, 80)   # F#m9 spread: F#3 alone under F#4 A4 C#5 E5 G#5


def chord_sweep(take):
    """F#m9 on two band-limited saws a note (+-7 cents), through a resonant low-pass sweeping down from 9 kHz to 400 Hz
    over a second (exponential), the level falling with it."""
    v = take.vel
    n = int(1.6 * SR)
    t = P.times(n)
    r = D.rng(take.kit, take.item.name, 'phases')
    y = np.zeros((n, 2))
    f0s = []
    for note in SWEEP_CHORD:
        for c, cents in enumerate((-7.0, 7.0)):
            f = hz(note) * 2 ** (cents / 1200)
            f0s.append(f)
            osc = D.additive_saw(f, n, r.uniform(), fmax=16000.0)
            y[:, c] += 0.7 * osc
            y[:, 1 - c] += 0.3 * osc
    y /= len(SWEEP_CHORD)
    fc = 400.0 * (9000.0 / 400.0) ** np.exp(-t / (0.35 + 0.15 * v))
    y = np.stack([D.svf_tpt(y[:, c], fc, q=1.4)[0] for c in range(2)], 1)
    y = y + 0.25 * D.st(np.sin(2 * np.pi * hz(SWEEP_CHORD[0]) * t))      # the root's sine
    y = y * (P.raised(n, 0.002) * np.exp(-t / 0.9) * D.taper(n, 1.25, 0.3))[:, None]
    return Out(finish(y) * (0.45 + 0.55 * v), f0s=tuple(f0s))


# ------------------------------------------------------------------ the sub drop
SUB_DROP = dict(note=42, hold=0.12, octaves=2.0, tau=0.45, length=1.8)


def sub_drop(take):
    """A clean sine on F#2 held 120 ms, then dropping two octaves (exponentially, most of it in the first half
    second), its level falling slowly: a pure sub with nothing in the low mids."""
    s = SUB_DROP
    n = int((s['length'] + 0.6) * SR)
    t = P.times(n)
    f = hz(s['note']) * 2.0 ** (-s['octaves'] * (1 - np.exp(-np.maximum(t - s['hold'], 0) / s['tau'])))
    y = np.sin(2 * np.pi * D.phase(f, n)) * P.raised(n, 0.012) * np.exp(-np.maximum(t - 0.3, 0) / 0.8)
    y = y * D.taper(n, s['length'] - 0.5, 0.45)
    thump = P.band_noise(take.rng('thump'), n, 1000.0, 4000.0) * D.env_exp(n, 0.003, 0.0003)   # a soft thump on
    return D.st(0.7 * y + 0.03 * thump)                                                       # the onset


# ------------------------------------------------------------------ the alloy bell
def alloy_bell(take):
    return Dr.alloy(take, 0.45, (1.9, 0.9), 3.0)


# ------------------------------------------------------------------ the vocal-synth stab
VOWEL = dict(oo=((300.0, 870.0, 2240.0), (1.0, 0.5, 0.15)), ah=((750.0, 1220.0, 2600.0), (1.0, 0.7, 0.3)))
VOX_CHORD = (54, 66, 69, 73, 76)    # F#m7 spread: F#3 alone under F#4 A4 C#5 E5


def vox_synth_stab(take):
    """F#m7 on band-limited saws (+-5 cents a side) through three formant band-passes moving from "oo" to "ah" over
    70 ms; held 0.3 s, released over 0.15 s. A synth vowel: no voice, no words."""
    v = take.vel
    n = int(0.9 * SR)
    t = P.times(n)
    r = D.rng(take.kit, take.item.name, 'phases')
    x = np.zeros((n, 2))
    f0s = []
    for note in VOX_CHORD:
        for c, cents in enumerate((-5.0, 5.0)):
            f = hz(note) * 2 ** (cents / 1200)
            f0s.append(f)
            osc = D.additive_saw(f, n, r.uniform(), fmax=12000.0)
            x[:, c] += 0.7 * osc
            x[:, 1 - c] += 0.3 * osc
    x /= len(VOX_CHORD)
    u = np.clip(t / 0.07, 0.0, 1.0)
    u = 0.5 - 0.5 * np.cos(np.pi * u)
    y = np.zeros((n, 2))
    for k in range(3):
        fk = VOWEL['oo'][0][k] * (VOWEL['ah'][0][k] / VOWEL['oo'][0][k]) ** u
        gk = VOWEL['oo'][1][k] + (VOWEL['ah'][1][k] - VOWEL['oo'][1][k]) * u
        q = 6.0
        for c in range(2):
            y[:, c] += gk * D.svf_tpt(x[:, c], fk, q=q)[1] / q * (1.0 + 0.6 * v * (k == 2))
    y = y + 0.12 * D.st(np.sin(2 * np.pi * hz(VOX_CHORD[0]) * t))      # the root's sine
    y = y * (P.raised(n, 0.004) * D.taper(n, 0.30, 0.15))[:, None]
    return Out(finish(y) * (0.6 + 0.9 * v), f0s=tuple(f0s))


# ------------------------------------------------------------------ the noise uplifter
UPLIFT_BARS = 2


def noise_uplifter(take):
    """Two bars of noise through a comb tuned to F#3 (feedback 0.55: resonances on F#'s harmonics) and a band-pass
    whose centre rises from 400 Hz to 9 kHz, its level rising 18 dB to the last sample, which is the downbeat after
    the two bars (cut_end)."""
    n = int(round(UPLIFT_BARS * 16 * K.STEP * SR))
    t = P.times(n)
    u = t / t[-1]
    x = take.rng('noise').standard_normal(n)
    d = SR / hz(ROOT3)
    y = np.zeros(n)
    i, fr = int(np.floor(d)), d - np.floor(d)
    for k in range(n):                                          # the comb: y[k] = x[k] + 0.55 y[k - d]
        a = y[k - i] if k >= i else 0.0
        b = y[k - i - 1] if k >= i + 1 else 0.0
        y[k] = x[k] + 0.55 * ((1 - fr) * a + fr * b)
    fc = 400.0 * (9000.0 / 400.0) ** (u ** 1.5)
    band = D.svf_tpt(y, fc, q=1.2)[1]
    band = band * D.undb(-18.0 * (1 - u) ** 1.3) * P.raised(n, 0.5)
    side = D.bfilt(D.svf_tpt(take.rng('side').standard_normal(n), fc, q=1.2)[1], 1500.0, 'highpass') * u
    out = np.stack([band + 0.25 * side, band - 0.25 * side], 1)
    return D.mono_below(D.bfilt(out, 200.0, 'highpass', order=4), 150.0)


R4 = ('r4', 'r5')                  # round 5 carries every one on, unchanged
ITEMS = [
    Item('Stabs', 'OrganHit', 'stab', organ_hit, layers=LAYERS, root=ROOT3, sets=R4,
         about='accent, organ hit on F#3: F#m9 over two octaves (F#3 C#4 E4 A4 C#5 G#5), drawbars, a percussion '
               'partial, a 6 Hz rotary shimmer, a 0.8 s release'),
    Item('Stabs', 'WarpStab', 'stab', warp_stab, layers=LAYERS, root=ROOT3, sets=R4,
         about='accent, warp stab on F#3: the warp voice (saw pair over a square) on F#3 and F#4, scooping up a whole '
               'tone, its resonant filter opening and closing'),
    Item('Stabs', 'RaveStab', 'stab', rave_stab, layers=LAYERS, root=ROOT3, sets=R4,
         qa={'sustain': (0.10, 0.32), 'why': "the alias check reads the stab's body: from 0.2 s its fast release "
                                            "spreads every one of its 120-odd partials past the check's 8 Hz (exactly "
                                            "band-limited saws, so nothing in it can alias)"},
         about='accent, rave stab on F#3: F#3 F#4 A4 C#5 on three detuned saws each, dropping a semitone into pitch, '
               'a bright filter blip, a short tail'),
    Item('Stabs', 'ChordSweep', 'stab', chord_sweep, layers=LAYERS, root=ROOT3, sets=R4,
         about='accent, chord hit with a filter sweep on F#3: F#m9 on detuned saws, a resonant low-pass sweeping from '
               '9 kHz to 400 Hz over a second'),
    Item('FX', 'ReverseStab', 'fx', reverse_stab, layers=LAYERS, root=ROOT3, sets=R4, flags={'cut_end'},
         about='accent, reverse stab on F#3: the organ hit and its plate reversed, swelling into the beat it ends on'),
    Item('Bass', 'SubDrop', 'bass', sub_drop, root=42, sets=R4,
         qa={'tonal': False, 'tune_window': (0.02, 0.11), 'loop_check': False,
             'why': 'a pitch drop: on F#2 for its first 120 ms (the tuning window), then gliding, so no steady tone '
                    'for '
                    'the alias check; an accent for a section change, not a groove bass shot'},
         about='accent, sub drop from F#2: a clean sine held 120 ms, then dropping two octaves over 1.4 s'),
    Item('Perc', 'AlloyBell', 'perc', alloy_bell, layers=Dr.ALLOY_LAYERS, rr=2, root=85, sets=R4,
         about="accent, alloy bell on C#6: the kit's alloy hit struck on the fifth, its modes ringing three times "
               'longer, +-6-36 Hz shifted sides'),
    Item('Stabs', 'VoxSynthStab', 'stab', vox_synth_stab, layers=LAYERS, root=ROOT3, sets=R4,
         about='accent, vocal-synth stab on F#3: F#m7 on saws through formants moving from "oo" to "ah" over 70 ms'),
    Item('FX', 'NoiseUplifter', 'fx', noise_uplifter, root=ROOT3, level=-14.0, sets=R4, flags={'cut_end'},
         tags=('riser',),
         about='accent, noise uplifter on F#: 2 bars of noise through a comb tuned to F#3 and a band-pass rising from '
               '400 Hz to 9 kHz, rising 18 dB to the downbeat'),
]
