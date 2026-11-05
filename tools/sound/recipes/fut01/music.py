"""FUT01 music parts for the first listen: the air pad on F#m9, the chrome pluck (multisamples) and a 2-bar pluck
phrase.

Both stay out of the kick and sub: the pad is high-passed at 100 Hz and mono under 150 Hz, the pluck high-passed at
150 Hz and mono under 150 Hz.

The pad loop is exactly periodic, like the reese (bass.py): every saw sits on a grid of whole cycles per 4 s loop, and
its breathing repeats a whole number of times per loop, so the crossfaded loop points join cleanly. The pluck is a
stiff Karplus-Strong string; dsp48.karplus tunes its delay from the loop filters' delay at DC, which with this stiffness
leaves the string 0.3 cents sharp at C6 and 2 cents at C7 (the core set's top zone), so parts.ks_tuned() solves for
the pitch to ask for from the filters' actual phase delay at the note.

Provenance: the pad and the pluck grow out of the S1 research prototype (recipes.py: pad, ks_pluck, 2026-10-29), with
the pad's low end fixed (the prototype voiced its root on F#2 and measured side under 120 Hz at -7.3 dB) and the pluck
tuned.
The phrase is written for this kit.
"""
import numpy as np

from tools.sound import dsp48 as D
from tools.sound import parts as P
from tools.sound import phrase as Ph
from tools.sound.item import Item, Out
from tools.sound.kits import fut01 as K

SR = D.SR


# ------------------------------------------------------------------ air pad
PAD_CHORD = (54, 64, 69, 73, 80)    # F#m9, open: F#3 E4 A4 C#5 G#5 (root, 7, 3, 5, 9)
PAD_LOOP_S = 4.0
PAD_ATTACK_S = 1.5
PAD_DETUNE_CENTS = 7.0


def breath(n, cycles, L, depth, ph):
    """Slow amplitude motion that repeats `cycles` times per loop of L samples."""
    return 1.0 + depth * np.sin(2 * np.pi * cycles * np.arange(n) / L + ph)


def air_pad(take):
    """Three detuned saws a side per chord tone, on the loop's grid. The body is low-passed soft with a dip in the low
    mids; the air is the same saws' 6-13 kHz band, breathing deeper and at other rates, so the top moves across the
    stereo field while the body holds."""
    L = int(PAD_LOOP_S * SR)
    grid = SR / L
    A0 = int(PAD_ATTACK_S * SR)
    n = A0 + int(0.02 * SR) + L
    r = take.rng('voices')
    f0s, body, air = [], np.zeros((n, 2)), np.zeros((n, 2))
    for i, note in enumerate(PAD_CHORD):
        f = round(float(D.midi_hz(note)) / grid) * grid      # on the grid: at most 1.2 cents off
        s = max(1, round(f * (2 ** (PAD_DETUNE_CENTS / 1200) - 1) / grid))
        steps = {'L': (-s, 0, s // 2), 'R': (-(s // 2), 0, s)}
        voice = {d: D.down(D.polyblep_saw(f + d * grid, n, r.uniform(), k=4), 4)
                 for d in sorted({*steps['L'], *steps['R']})}
        f0s += [f + d * grid for d in voice]
        for c, side in enumerate('LR'):
            v = sum(voice[d] for d in steps[side]) / 3
            body[:, c] += v * breath(n, 1 + i % 3, L, 0.12, r.uniform(0, 2 * np.pi))
            air[:, c] += v * breath(n, 2 + i % 2, L, 0.5, r.uniform(0, 2 * np.pi))
    body = D.peaking(D.bfilt(body, 1900.0, 'lowpass', order=4), 280.0, -5.0, 0.8)
    air = D.bfilt(air, (6000.0, 13000.0), 'bandpass', order=2)
    y = D.bfilt((body + 0.5 * air) / len(PAD_CHORD), 100.0, 'highpass', order=4)
    y = D.mono_below(y, 150.0) * (np.sin(0.5 * np.pi * np.clip(P.times(n) / 1.1, 0, 1)) ** 2)[:, None]
    y, loop = P.close_loop(y, A0, L, xfade=0.05)
    return Out(y, loop=loop, f0s=tuple(sorted(f0s)))


# ------------------------------------------------------------------ chrome pluck
PLUCK_NOTES = (60, 63, 66, 69, 72, 75, 78, 81, 84)   # C4-C6 every 3 semitones
PLUCK_LAYERS = (0.55, 1.0)
DISP = -0.3                                           # string stiffness: the partials stretch, the "chrome"


PLUCK_SIDE = (0.35, 53, 0.6)                          # the side: level, and the all-pass that decorrelates it
                                                      # (delay in samples, about 1.1 ms; coefficient)


def pluck_note(f0, vel, n, r):
    """One chrome pluck: a band-limited saw-and-noise burst, brighter with velocity and comb-filtered at the pick
    position, into a stiff string, and a short tine on the kit's alloy ratios over the attack. The string is the mid;
    the side is the same string through an all-pass, above 1 kHz, so the mono sum is the string whole and every
    harmonic keeps its level in mono (two independent strings, one a side, cancelled single harmonics by up to 34
    dB)."""
    m = int(SR / f0)
    S = 0.45 - 0.3 * vel
    ex = 0.6 * D.additive_saw(f0, m) + 0.4 * r.uniform(-1, 1, m)
    ex = D.bfilt(ex, 1500.0 + 9000.0 * vel, 'lowpass', order=1)
    p = max(1, int(m * 0.13))
    ex[p:] -= 0.8 * ex[:-p]
    ex[:12] *= 0.5 - 0.5 * np.cos(np.pi * np.arange(12) / 12)
    string = D.karplus(ex, P.ks_tuned(f0, S, DISP), n, decay_s=1.0 + 0.4 * vel, S=S, disp=DISP)
    g, d, a = PLUCK_SIDE
    side = g * D.bfilt(P.allpass(string, d, a), 1000.0, 'highpass', order=2)
    y = np.stack([string + side, string - side], 1)
    decays = [0.006 / (1 + 0.3 * k) for k in range(len(K.ALLOY))]
    tine = D.modal_ir(f0 * 4, K.ALLOY, decays, [1 / (1 + k) for k in range(len(K.ALLOY))], n, r) * P.raised(n, 0.0004)
    y += (0.06 + 0.12 * vel) * D.st(tine)
    return D.mono_below(D.bfilt(y, 150.0, 'highpass', order=2), 150.0) * vel ** 0.9


def chrome_pluck(take):
    """Both layers of a zone pluck the same noise, so only velocity tells them apart (brighter, never by chance)."""
    return pluck_note(take.f0, take.vel, int(2.6 * SR), D.rng(take.kit, take.sound, 'pluck'))


# ------------------------------------------------------------------ the pluck phrase
# 2 bars of 16ths in F# minor, on the pad's chord tones plus B: (16th, MIDI note, length in 16ths, velocity). Bar 1
# leaps F#-C#-A-E on a 3-3-2 rhythm and settles on B; bar 2 answers from A up to G# and ends on C#, leading back to F#.
PHRASE_STEPS = 32
PHRASE = ((0, 66, 2, 1.0), (3, 73, 2, 0.7), (6, 69, 2, 0.8), (8, 76, 3, 0.9), (11, 73, 2, 0.6), (14, 71, 2, 0.75),
          (16, 69, 2, 0.95), (19, 76, 2, 0.7), (22, 73, 2, 0.8), (24, 80, 3, 0.9), (27, 78, 2, 0.65), (30, 73, 2, 0.7))


def pluck_phrase(take):
    """The phrase played on the pluck at its exact pitches, each note damped 120 ms after its length, on the kit's
    swing (its odd 16ths land with the hats'); tails wrap, so it loops."""
    L = Ph.loop_len(K, PHRASE_STEPS)
    y = np.zeros((L, 2))
    n = int(1.6 * SR)
    for i, (step, note, length, vel) in enumerate(PHRASE):
        x = pluck_note(float(D.midi_hz(note)), vel, n, take.rng('note', i)) * D.taper(n, length * K.STEP, 0.12)[:, None]
        at = step + (2 * K.SWING - 1 if step % 2 else 0)
        np.add.at(y, (int(round(at * K.STEP * SR)) + np.arange(n)) % L, x)
    return Out(y)


ITEMS = [
    Item('Pad', 'AirPadFsm9', 'pad', air_pad, root=54, loop=True, sets=('s1a', 'r2', 'r3'),
         about='air pad, F#m9 (F#3 E4 A4 C#5 G#5): detuned saws with a breathing 6-13 kHz air band, high-passed at '
               '100 Hz, mono under 150 Hz; a 4 s loop'),
    Item('Lead', 'ChromePluck', 'lead', chrome_pluck, layers=PLUCK_LAYERS, notes=PLUCK_NOTES, sets=('s1a', 'r2', 'r3'),
         about='chrome pluck: a stiff Karplus-Strong string with an alloy-ratio tine, C4-C6 every 3 semitones'),
    Item('Loops', 'ChromePluckPhrase', 'loop', pluck_phrase, loop=True, sets=('s1a', 'r2', 'r3'),
         qa={'mud': 'lowcut', 'why': 'a lead loop: the low cut, like the lead'},
         about='2-bar chrome pluck phrase in F# minor at 134 BPM'),
]
