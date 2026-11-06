"""FUT04 lead for listening round 2: the modular blips, the kit's signature melodic element. A few multisample notes and
an original 2-bar sequence, the phrase that goes up front in the context loop.

Round 1's strength was the melodic top ("the high plucks are nice"; FUT01's chrome pluck got Cool 5), so the kit's lead
is short, high and pitched. Here it is a modular voice, the sound techno sequences come from:
- a sine VCO with a short pitch blip on the attack (a third of a semitone, gone in 8 ms);
- FM from a second sine on the kit's iron ratio (RATIOS[1], 1.5719, the anvil's warble near the minor sixth): a
  metal clank on the strike whose index falls away in about 12 ms, so the note itself is a clean tone;
- a soft wavefolder, harder with velocity, at 4x;
- a low-pass gate: a 2-pole low-pass whose cutoff rides the amplitude envelope, so each blip opens bright and closes
  dark as it dies (the "bloop" of a vactrol gate);
- a 5 ms struck tine on the kit's RATIOS, four octaves over the note, over the attack;
- the side: the same voice through an all-pass, above 1 kHz, so the blips are wide and the mono sum keeps every
  harmonic whole (as FUT01's pluck).
It is high-passed at 150 Hz and mono under 150 Hz, out of the kick and the acid.

The sequence: 2 bars of 16ths at 136 BPM in E minor, on the kit's 0.54 swing (its odd 16ths land with the acid's). Bar 1
sets out the motif, E5 then B4 a 16th early into G5 F#5 E5 (3 + 3 + 2), climbs D5 E5 to an accent on B5 and turns on
A5; bar 2 starts the same and answers darker, up to C6 (the minor sixth over E, the anvil's own interval) as a
three-hit ratchet, then B5 G5 F#5 back down to the loop's E5. Ratchets (two or three hits inside one 16th, softer each
time) are the modular sequencer's gesture.

Provenance: written for this kit. The side all-pass is FUT01's pluck's (recipes/fut01/music.py), from parts.py.
"""
import numpy as np

from tools.sound import dsp48 as D
from tools.sound import parts as P
from tools.sound import phrase as Ph
from tools.sound.item import Item, Out
from tools.sound.kits import fut04 as K

SR = D.SR
HP = 150.0
BLIP_NOTES = (70, 73, 76, 79, 82, 85)   # A#4-C#6 every 3 semitones: the zones over the sequence's range, B4 to C6
BLIP_LAYERS = (0.55, 1.0)
DECAY = (0.075, 0.110)              # the gate's fall, soft and hard layer (seconds)
CLANK = (1.2, 3.2)                  # the FM index on the strike, soft and hard layer
CLANK_TAU = 0.004                   # ...falling with this time constant (about 12 ms to vanish)
FOLD = (0.8, 1.4)                   # the wavefolder's drive, soft and hard layer
BLIP_CENTS = 33.0                   # the pitch blip on the attack
BLIP_TAU = 0.0025
GATE_HZ = (1.4, 9.0)                # the low-pass gate's cutoff: x the note at rest, plus x the note at full level
TINE = 16.0                         # the tine: 4 octaves over the note, on the kit's ratios
SIDE = (0.30, 47, 0.6)              # the side: level, all-pass delay (samples, about 1 ms), coefficient
K4 = 4


def lpg(x, env, f0):
    """The low-pass gate: a 2-pole low-pass (TPT SVF) whose cutoff rides the envelope, then the envelope itself."""
    fc = f0 * (GATE_HZ[0] + GATE_HZ[1] * env)
    return D.svf_tpt(x, np.minimum(fc, 18000.0), q=0.8)[0] * env


def voice(f0, vel, n, decay, r, length=None):
    """One blip, mono, n samples: the VCO with its pitch blip and the iron clank (FM at 4x), the fold, the gate, the
    tine. length (seconds): the gate closes 30 ms after it (a phrase note); None lets it ring out (a multisample)."""
    hard = float(np.clip((vel - BLIP_LAYERS[0]) / (BLIP_LAYERS[1] - BLIP_LAYERS[0]), 0.0, 1.0))
    t4 = np.arange(n * K4) / (SR * K4)
    f = f0 * 2.0 ** (BLIP_CENTS / 1200.0 * np.exp(-t4 / BLIP_TAU))
    ph = 2 * np.pi * np.concatenate([[0.0], np.cumsum(f[:-1] / (SR * K4))])
    index = P.lerp(CLANK, hard) * np.exp(-t4 / CLANK_TAU)
    u = np.sin(ph + index * np.sin(K.RATIOS[1] * ph))
    fold = P.lerp(FOLD, hard)
    y = D.down(D.sinefold_adaa(fold * u, 1.0) / np.sin(0.5 * np.pi * min(fold, 1.0)), K4)
    env = D.env_exp(n, decay, attack=0.0008)
    if length is not None:
        env = env * D.taper(n, length, 0.030)
    y = lpg(y, env, f0)
    decays = [0.005 / (1 + 0.3 * k) for k in range(len(K.RATIOS))]
    tine = D.modal_ir(f0 * TINE, K.RATIOS, decays, [1 / (1 + k) for k in range(len(K.RATIOS))], n, r)
    y = y + (0.05 + 0.10 * hard) * tine * P.raised(n, 0.0004)
    g, d, a = SIDE
    side = g * D.bfilt(P.allpass(y, d, a), 1000.0, 'highpass', order=2)
    out = np.stack([y + side, y - side], 1)
    return D.mono_below(D.bfilt(out, HP, 'highpass', order=2), 150.0) * vel ** 0.9


def blip(take):
    """A multisample note: both layers strike the same tine noise, so only velocity tells them apart."""
    hard = take.layer - 1
    return voice(take.f0, take.vel, int(1.4 * SR), DECAY[hard], D.rng(take.kit, take.sound, 'tine'))


# ------------------------------------------------------------------ the sequence
PHRASE_STEPS = 32
# (16th, MIDI, length in 16ths, velocity, ratchet: hits inside the 16th)
PHRASE = ((0, 76, 1.5, 1.0, 1), (3, 71, 0.8, 0.6, 1), (4, 79, 1.5, 0.85, 1), (6, 78, 1.0, 0.7, 1),
          (7, 76, 0.6, 0.5, 1), (10, 74, 0.8, 0.85, 2), (11, 76, 0.8, 0.6, 1), (12, 83, 2.0, 1.0, 1),
          (14, 81, 1.5, 0.7, 1),
          (16, 76, 1.5, 1.0, 1), (19, 71, 0.8, 0.6, 1), (20, 79, 1.5, 0.85, 1), (22, 78, 1.0, 0.7, 1),
          (23, 74, 0.6, 0.5, 1), (26, 84, 1.0, 1.0, 3), (28, 83, 1.5, 0.8, 1), (30, 79, 1.0, 0.7, 1),
          (31, 78, 0.6, 0.45, 1))
RATCHET_DB = -4.0                   # each ratchet hit this much softer than the one before


def hits(phrase, grid):
    """Every strike of the phrase: (sample, MIDI, gate length in seconds, velocity, index of the note)."""
    out = []
    for i, (step, note, length, vel, ratchet) in enumerate(phrase):
        at = grid.at(step)
        for k in range(ratchet):
            gap = grid.s16 / ratchet
            ln = length * K.STEP if ratchet == 1 else 0.7 * gap / SR
            out.append((at + int(round(k * gap)), note, ln, vel * D.undb(RATCHET_DB * k), i))
    return out


def blip_phrase(take):
    """The sequence on the blip voice at its exact pitches, each note damped after its length, on the kit's swing;
    tails wrap, so it loops. Notes sit a little left or right by pitch (low left, high right), as a patch's pan CV."""
    G = Ph.Grid(K, PHRASE_STEPS)
    y = np.zeros((G.length, 2))
    n = int(0.9 * SR)
    for at, note, ln, vel, i in hits(PHRASE, G):
        f0 = float(D.midi_hz(note))
        hard = float(np.clip((vel - BLIP_LAYERS[0]) / (BLIP_LAYERS[1] - BLIP_LAYERS[0]), 0.0, 1.0))
        x = voice(f0, vel, n, P.lerp(DECAY, hard), take.rng('note', i), length=ln)
        Ph.place(y, Ph.pan(x, 0.35 * (note - 77) / 7.0), at)
    meta = {'bpm': K.BPM, 'swing': K.SWING, 'steps': PHRASE_STEPS,
            'notes': [{'step': s, 'note': D.note_name(m), 'length16': ln, 'vel': v, 'ratchet': r}
                      for s, m, ln, v, r in PHRASE]}
    return Out(D.mono_below(y, 150.0), meta=meta)


ITEMS = [
    Item('Lead', 'ModBlip', 'lead', blip, layers=BLIP_LAYERS, notes=BLIP_NOTES, sets=('r2', 'r3', 'r5'),
         about='modular blip: a sine VCO with a pitch blip and an FM clank on the iron ratio 1.5719, a soft fold, a '
               'low-pass gate that closes as it dies, an iron tine on the strike; A#4-C#6 every 3 semitones'),
    Item('Loops', 'ModBlipPhrase', 'loop', blip_phrase, loop=True, sets=('r2', 'r3', 'r5'),
         qa={'mud': 'lowcut', 'why': 'a lead loop: the low cut, like the lead'},
         about='2-bar modular blip sequence in E minor at 136 BPM on the kit swing: E5 B4 G5 F#5 E5, D5 E5 up to an '
               'accent on B5; bar 2 answers up to a C6 ratchet and falls B5 G5 F#5 home'),
]
