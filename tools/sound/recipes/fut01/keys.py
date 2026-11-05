"""FUT01 round 4 chords (set r4): classic house organ stabs and a warm electric-piano stab, minor 9 chords in two zones,
F#m9 on F#3 and Bm9 on B3 (the key's i and iv), so a line of stabs plays each chord from its own file.

- OrganStab: an additive organ (drawbar harmonics 1, 2, 3, 4, 6 and 8), a percussion partial and a key click on the
  attack, a short gate (held about 90-120 ms, then gone in about 60 ms): short and punchy. A slow stereo spread (each
  side's partials 2 cents apart) gives it width; the mono sum keeps every note.
- EPianoStab: a two-operator FM tine piano per note (modulator on the note, its index falling with the strike) with a
  bell partial (a ratio-14 modulator that dies in 20 ms), a warm low-pass and a stereo tremolo at 4.5 Hz; held about
  0.3 s, released over 0.12 s.

Voicings: root, fifth, minor seventh, ninth and minor third, F#3 C#4 E4 G#4 A4 and B3 F#4 A4 C#5 D5: the ninth rubs
against the third, the deep-house colour. The root sits alone in its own band (no other tone within 30 % of it), so
the tuning check reads the root. Both high-passed at 150 Hz and mono under 150 Hz: body from 185 Hz up, nothing in the
kick's or the sub's band.
"""
import numpy as np

from tools.sound import dsp48 as D
from tools.sound import parts as P
from tools.sound.item import Item, Out

SR = D.SR
SHAPE = (0, 7, 10, 14, 15)          # root, 5, m7, 9, m3 (an octave up): the m9 voicing over its zone's root
ZONES = (54, 59)                    # F#3 (F#m9) and B3 (Bm9): the key's i and iv
LAYERS = (0.6, 1.0)
DRAWBARS = ((1, 1.0), (2, 0.65), (3, 0.40), (4, 0.30), (6, 0.16), (8, 0.12))
ORGAN_PERC = (3, 0.45, 0.05)        # harmonic, level, decay (s)
ORGAN_GATE = dict(hold=(0.09, 0.12), tau=0.035, fade=(0.30, 0.06))   # held soft..hard, then a fast fall
SPREAD_CENTS = 2.0                  # each side's partials this far apart (left flat, right sharp)


def chord(zone):
    """The chord's MIDI notes over its zone's root."""
    return tuple(zone + s for s in SHAPE)


def organ_stab(take):
    """One stab: every chord tone as drawbar harmonics (the hard layer brighter), the percussion partial and a click on
    the attack, a short gate; the sides 2 cents apart. Returns the chord's fundamentals for the alias check."""
    v = take.vel
    n = int(0.5 * SR)
    t = P.times(n)
    up = (v - LAYERS[0]) / (LAYERS[1] - LAYERS[0])
    f0s, sides = [], []
    for c, cents in enumerate((-SPREAD_CENTS / 2, SPREAD_CENTS / 2)):
        y = np.zeros(n)
        for note in chord(take.note):
            f = float(D.midi_hz(note)) * 2 ** (cents / 1200)
            f0s.append(f)
            ph = 2 * np.pi * f * t + D.rng(take.kit, take.item.name, note).uniform(0, 2 * np.pi)   # one phase
                                                                                      # a note
            for h, a in DRAWBARS:
                if h * f < 16000:
                    y += a * (0.7 + 0.3 * up) ** (h - 1) * np.sin(h * ph)
            hp, ap, dp = ORGAN_PERC
            y += ap * (0.5 + 0.5 * up) * np.sin(hp * ph) * np.exp(-t / dp)
        sides.append(y / len(SHAPE))
    x = np.stack(sides, 1)
    click = P.hp_noise(take.rng('click'), n, 3000.0, order=2) * D.env_exp(n, 0.002, 0.0002)
    x = x + 0.05 * v * D.st(click)
    g = ORGAN_GATE
    env = P.raised(n, 0.002) * P.gate(t, P.lerp(g['hold'], v), g['tau']) * D.taper(n, *g['fade'])
    y = x * env[:, None]
    y = D.mono_below(D.bfilt(y, 150.0, 'highpass', order=4), 150.0)
    return Out(y * (0.55 + 0.45 * v), f0s=tuple(sorted(f0s)))


EP_INDEX = (1.2, 2.6)               # the tine's FM index at the strike, soft..hard; it falls over EP_INDEX_TAU
EP_INDEX_TAU = 0.25
EP_BELL = (0.6, 0.020)              # the bell modulator's index (x velocity) and its decay (s)
EP_GATE = dict(hold=0.30, release=0.12, decay=1.1)
EP_TREMOLO = (4.5, 0.25)            # Hz, depth: the stereo tremolo


def epiano_stab(take):
    """One stab on an FM tine piano: per note a carrier on the note, a modulator on the note (index falling from the
    strike) and a bell modulator at 14x that dies in 20 ms; a warm low-pass that opens with velocity; a stereo
    tremolo; held 0.3 s, then released."""
    v = take.vel
    n = int(0.9 * SR)
    t = P.times(n)
    f0s = []
    y = np.zeros(n)
    for note in chord(take.note):
        f = float(D.midi_hz(note))
        f0s.append(f)
        ph0 = D.rng(take.kit, take.item.name, note).uniform(0, 2 * np.pi)
        idx = P.lerp(EP_INDEX, v) * np.exp(-t / EP_INDEX_TAU) + 0.15
        bell = EP_BELL[0] * v * np.exp(-t / EP_BELL[1])
        mod = idx * np.sin(2 * np.pi * f * t) + bell * np.sin(2 * np.pi * 14 * f * t)
        level = 1.0 / (1 + 0.12 * (note - take.note))           # the upper notes a little softer
        y += level * np.sin(2 * np.pi * f * t + ph0 + mod) * np.exp(-t / (EP_GATE['decay'] * (0.8 + 0.2 * v)))
    y = D.bfilt(y / len(SHAPE), 3500.0 + 3500.0 * v, 'lowpass', order=2)
    rate, depth = EP_TREMOLO
    trem = depth * np.sin(2 * np.pi * rate * t)
    x = np.stack([y * (1 - trem), y * (1 + trem)], 1)
    env = P.raised(n, 0.002) * D.taper(n, EP_GATE['hold'], EP_GATE['release'])
    x = x * env[:, None]
    x = D.mono_below(D.bfilt(x, 150.0, 'highpass', order=4), 150.0)
    return Out(x * (0.6 + 0.4 * v), f0s=tuple(sorted(f0s)))


R4 = ('r4', 'r5')                  # round 5 carries every one on, unchanged
ITEMS = [
    Item('Stabs', 'OrganStab', 'stab', organ_stab, layers=LAYERS, notes=ZONES, sets=R4,
         about='house organ stab, minor 9: F#m9 on F#3 and Bm9 on B3 (root, 5, m7, 9, m3), drawbar harmonics, a '
               'percussion partial and a click, gated short; the sides 2 cents apart'),
    Item('Stabs', 'EPianoStab', 'stab', epiano_stab, layers=LAYERS, notes=ZONES, sets=R4,
         about='warm electric-piano stab, minor 9: F#m9 on F#3 and Bm9 on B3, an FM tine with a bell on the strike, a '
               'warm low-pass and a stereo tremolo, held 0.3 s'),
]
