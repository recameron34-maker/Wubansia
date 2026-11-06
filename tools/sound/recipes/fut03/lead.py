"""FUT03 "Glass Current" lead for listening round 2: the glass pluck, bright and clean, as multisamples, and its
signature phrase, an original 2-bar line in G minor at 128 BPM.

**The glass pluck.** Phase-locked layers, every one started on the same phase on every note, so each attack is the
same crisp edge:
- the body, an exactly band-limited saw through a low-pass that snaps from bright (7-15 kHz, with velocity) down to a
  few harmonics in about 90 ms: the house pluck;
- the glass, two sine partials an octave and two octaves over the note, ringing on a little longer than the body: the
  clean shimmer;
- the glint, the kit's tuned-bell partials (K.RATIOS) an octave over the note, gone within 10 ms: the "ting" of glass on
  every attack.
The level falls on one time constant (longer with velocity) and is damped from 1.15 s, as a released key would be.
The side is the same pluck through an all-pass above 1 kHz, so the mono sum is the pluck whole. High-passed at 150 Hz
and mono below 150 Hz.

**The phrase** (GlassPluckPhrase), on the kit's swing (odd 16ths 7 ms late, as on every lane), notes damped 60 ms
after their length:
- bar 1, the call: D5 on the one, up a minor third to F5 on the swung 16th, D5 C5 stepping down to Bb4 on beat 3,
  C5 D5 back up, and G4, the root, under it;
- bar 2, the answer: the same opening reaching a fourth higher (D5 to G5), then F5 Eb5 (the minor sixth as a passing
  colour) down to D5 on beat 3, C5 Bb4, and A4, left open so the loop leans back into D5.

Provenance: new for FUT03 (the stereo all-pass trick is FUT01's chrome pluck's idea, written here for this voice).
"""
import numpy as np

from tools.sound import dsp48 as D
from tools.sound import parts as P
from tools.sound import phrase as Ph
from tools.sound.item import Item, Out
from tools.sound.kits import fut03 as K

SR = D.SR
PLUCK_NOTES = (62, 67, 70, 74, 79, 82)   # D4 G4 Bb4 D5 G5 Bb5: the phrase's range and a little either side
PLUCK_LAYERS = (0.55, 1.0)
NOTE_S = 2.0                        # each multisample note, rendered long; it is damped from DAMP_AT
FC_PEAK = (7000.0, 15000.0)         # the body's low-pass on the attack, soft to hard...
FC_REST = 3.0                       # ...falling to this many times the note
FC_TAU = 0.09
AMP_TAU = (0.20, 0.32)              # the level's time constant, soft to hard
DAMP_AT, DAMP_TAU = 1.15, 0.08      # the damper: a second, faster fall from 1.15 s (31 dB down by then)
GLASS = ((2, 0.20, 0.45), (4, 0.07, 0.30))   # (harmonic, level, time constant): the glass partials
GLINT = 2.0                         # the glint sits an octave over the note, on the kit's bell partials
GLINT_LEVEL = (0.05, 0.12)
SIDE = (0.30, 37, 0.6)              # the side: level, and the all-pass that decorrelates it (samples, coefficient)
HP = 150.0


def pluck(f0, vel, n):
    """One glass pluck, (n, 2): see the module's docstring. Deterministic: the glint's phases come from the note."""
    t = P.times(n)
    body = D.additive_saw(f0, n, 0.0)
    fc = f0 * FC_REST + (P.lerp(FC_PEAK, vel) - f0 * FC_REST) * np.exp(-t / FC_TAU)
    body = D.svf_tpt(body, np.minimum(fc, 0.45 * SR), q=0.8)[0]
    ph = 2 * np.pi * f0 * t
    glass = sum(a * np.sin(h * ph) * np.exp(-t / tau) for h, a, tau in GLASS)
    decays = [0.004 / (1 + 0.3 * k) for k in range(len(K.RATIOS))]
    amps = [1.0 / (1 + k) for k in range(len(K.RATIOS))]
    glint = D.modal_ir(f0 * GLINT, K.RATIOS, decays, amps, n, D.rng(K.KIT, 'GlassPluck', round(f0, 3), 'glint'))
    env = np.exp(-t / P.lerp(AMP_TAU, vel)) * np.exp(-np.maximum(t - DAMP_AT, 0.0) / DAMP_TAU)
    x = (0.6 * body + glass) * env * P.raised(n, 0.001) + P.lerp(GLINT_LEVEL, vel) * glint * P.raised(n, 0.0003)
    g, d, a = SIDE
    side = g * D.bfilt(P.allpass(x, d, a), 1000.0, 'highpass', order=2)
    y = np.stack([x + side, x - side], 1)
    return D.mono_below(D.bfilt(y, HP, 'highpass', order=2), 150.0) * vel ** 0.9


def glass_pluck(take):
    return pluck(take.f0, take.vel, int(NOTE_S * SR))


# ------------------------------------------------------------------ the phrase
PHRASE_STEPS = 32
# (16th, MIDI note, length in 16ths, velocity): G4 67, A4 69, Bb4 70, C5 72, D5 74, Eb5 75, F5 77, G5 79
PHRASE = (
    (0, 74, 2, 1.0), (3, 77, 2, 0.75), (6, 74, 1, 0.8), (7, 72, 1, 0.6),          # bar 1, the call
    (8, 70, 3, 0.95), (11, 72, 1, 0.6), (12, 74, 2, 0.85), (14, 67, 2, 0.7),
    (16, 74, 2, 1.0), (19, 79, 2, 0.8), (22, 77, 1, 0.75), (23, 75, 1, 0.6),      # bar 2, the answer
    (24, 74, 3, 0.95), (27, 72, 1, 0.6), (28, 70, 2, 0.85), (30, 69, 2, 0.7),
)
DAMP_AFTER = (0.06, 0.10)           # a phrase note is damped this long after its length, over this long


def pluck_phrase(take):
    """The phrase on the pluck at its exact pitches, on the kit's grid and swing; tails wrap, so it loops."""
    G = Ph.Grid(K, PHRASE_STEPS)
    buf = np.zeros((G.length, 2))
    n = int(1.2 * SR)
    for step, note, length, vel in PHRASE:
        x = pluck(float(D.midi_hz(note)), vel, n)
        x = x * D.taper(n, length * G.s16 / SR + DAMP_AFTER[0], DAMP_AFTER[1])[:, None]
        Ph.place(buf, x, G.at(step))
    meta = {'bpm': K.BPM, 'swing': K.SWING, 'steps': PHRASE_STEPS,
            'notes': [{'step': s, 'note': D.note_name(m), 'len16': ln, 'vel': v} for s, m, ln, v in PHRASE]}
    return Out(buf, meta=meta)


ITEMS = [
    Item('Lead', 'GlassPluck', 'lead', glass_pluck, layers=PLUCK_LAYERS, notes=PLUCK_NOTES, sets=('r2', 'r3'),
         about='glass pluck: a band-limited saw through a snapping low-pass, octave and double-octave sine partials '
               'and a glint on the kit bell partials, phase-locked; D4 to Bb5, high-passed at 150 Hz'),
    Item('Loops', 'GlassPluckPhrase', 'loop', pluck_phrase, loop=True, sets=('r2', 'r3'),
         qa={'mud': 'lowcut', 'why': 'a lead loop: the low cut, like the lead'},
         about='the signature phrase: 2 bars of the glass pluck in G minor at 128 BPM on the kit swing; D5 up to F5, '
               'down to Bb4 and G4, then the answer reaching G5 and falling through Eb5 to an open A4'),
]
