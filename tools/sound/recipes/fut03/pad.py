"""FUT03 chord stab for round 2: the supersaw Gm9 stab, short, and its 2-bar sidechained preview.

- StabGm9: Gm9 in stacked thirds, G3 Bb3 D4 F4 A4 (root, minor third, fifth, minor seventh, ninth on top), each
  note a seven-voice supersaw (exactly band-limited saws, spread about +-9 cents and across the stereo field). No two
  voices closer than a minor third: the first voicing put the ninth a semitone under the third (A4 under Bb4, 26 Hz
  apart, inside one critical band), a rough cluster on every hit of a kit meant to be hyper-clean. The top stays on
  A4, under most of the pluck's phrase (Bb4-G5). A 4-pole low-pass snaps down from 6-14 kHz to 2 kHz in 60-90 ms,
  so it speaks like a pluck; the chord holds a dotted 16th and is gone by 0.28 s. Harder hits open the filter further
  and keep it open a little longer. 3 velocity layers, 2 round robins (each its own saw phases). High-passed at
  110 Hz, mono under 150 Hz: the root, G3 at 196 Hz, is the lowest note.
- StabPump: a 2-bar preview of the stab on the kit's grid and swing, played on its files as written: a 3-3-4-3-3 /
  3-3-2-3-3-2 rhythm that pushes against the four on the floor, under a sidechain pump keyed to every beat (9 dB
  down in 3 ms, held 20 ms, back over 200 ms): the off-beat hits bounce, and a hit on the beat stays ducked (it is
  gone by 0.28 s, before the pump is back), the kick's shadow. No kick in the file. The context loop plays only the
  off-beat hits (recipes/fut03/context.py STAB).
"""
import numpy as np

from tools.sound import dsp48 as D
from tools.sound import parts as P
from tools.sound import phrase as Ph
from tools.sound.item import Item, Out
from tools.sound.kits import fut03 as K

SR = D.SR
STAB_CHORD = (K.ROOT + 12, K.ROOT + 15, K.ROOT + 19, K.ROOT + 22, K.ROOT + 26)   # G3 Bb3 D4 F4 A4
STAB_LAYERS = (0.5, 0.75, 1.0)
DETUNE, MIX = 0.25, 0.5      # supersaw: detune curve value (about +-9 cents outer voices) and the side voices' mix
HOLD = 0.176                 # seconds: a dotted 16th at 128 BPM is 0.176 s
RELEASE = 0.10               # the release, a raised-cosine after the hold: gone by 0.28 s
CUTOFF = (2000.0, (6000.0, 14000.0), (0.060, 0.090))   # the low-pass: rest, peak (soft to hard), fall time (s)


def svf_lp4(x, fc):
    """Two 2-pole TPT low-passes in a row (a 4-pole low-pass, q 0.54 and 1.31: Butterworth), per channel, the cutoff one
    value per sample."""
    out = np.empty_like(x)
    for c in range(x.shape[1]):
        lp1 = D.svf_tpt(x[:, c], fc, q=0.5412)[0]
        out[:, c] = D.svf_tpt(lp1, fc, q=1.3066)[0]
    return out


PANS = np.array([-1, -0.66, -0.33, 0, 0.33, 0.66, 1.0]) * 0.9   # the voices across the field, lowest pitch left


def locked_supersaw(f, n, spread):
    """Seven exactly band-limited saws on the Szabo detune and mix curves (dsp48.supersaw's voices and pans), with phases
    locked in mirror pairs: the voices a fraction d sharp and flat start at +p and -p (spread[k], cycles) round the
    centre voice at 0, so each pair sums to a real envelope times the centre's own carrier. The stack's pitch is then
    exactly the note at every moment (random phases let one detuned voice lead, up to 6 cents off on G3, and moved the
    level 0.6 dB between round robins), and the stack still beats and spreads across the stereo field."""
    dfac = D.sz_detune(DETUNE)
    cg, sg = D.sz_mix(MIX)
    out = np.zeros((n, 2))
    ph0 = np.concatenate([-np.asarray(spread)[::-1], [0.0], np.asarray(spread)])
    for off, p, ph in zip(D.SZ_OFF, PANS, ph0):
        osc = D.additive_saw(f * (1 + dfac * off), n, ph)
        g = cg if off == 0 else sg
        out[:, 0] += g * osc * np.cos((p + 1) * np.pi / 4)
        out[:, 1] += g * osc * np.sin((p + 1) * np.pi / 4)
    return D.bfilt(out, f, 'highpass', order=1) / 3, [f * (1 + dfac * off) for off in D.SZ_OFF]


def stab(take):
    """Each note's stack starts its mirror pairs at their own phases (from the note and the round robin, the same on
    every layer), so the round robins differ in how the chord beats, never in its pitch."""
    v = take.vel
    n = int(0.36 * SR)
    t = P.times(n)
    y = np.zeros((n, 2))
    f0s = []
    for note in STAB_CHORD:
        spread = D.rng(take.kit, take.item.name, note, take.rr).uniform(0.02, 0.12, 3)
        x, fs = locked_supersaw(float(D.midi_hz(note)), n, spread)
        y += x
        f0s += fs
    rest, peak, fall = CUTOFF
    fc = rest + (P.lerp(peak, v) - rest) * np.exp(-t / P.lerp(fall, v))
    y = svf_lp4(y, fc)
    u = np.clip((t - HOLD) / RELEASE, 0.0, 1.0)
    amp = P.raised(n, 0.002) * (0.5 + 0.5 * np.cos(np.pi * u))
    y = D.bfilt(y * amp[:, None], 110.0, 'highpass', order=4)
    return Out(D.mono_below(y, 150.0) / len(STAB_CHORD) * P.vel_gain(v, 6.0, STAB_LAYERS[0]),
               f0s=tuple(sorted(f0s)))


# ------------------------------------------------------------------ the sidechained preview
PUMP_STEPS = 32
PUMP_HITS = ((0, 1.0), (3, 0.75), (6, 0.9), (10, 0.75), (13, 0.7),
             (16, 1.0), (19, 0.75), (22, 0.9), (24, 0.5), (27, 0.75), (30, 0.8))
PUMP = (9.0, 0.003, 0.020, 0.200)    # sidechain: depth (dB), ramp, hold, release (s), keyed to every beat


def stab_pump(take):
    """The rhythm on the stab's files as written (layer nearest the velocity, round robins in turn), tails wrapping
    round the loop, then the kick-keyed pump over the whole loop."""
    G = Ph.Grid(K, PUMP_STEPS)
    y = Ph.hits_lane(K, 'StabGm9', PUMP_HITS, G)
    beats = [G.at(s) for s in range(0, PUMP_STEPS, 4)]
    depth, ramp, hold, release = PUMP
    y = y * D.undb(Ph.pump_db(beats, G.length, depth, ramp, hold, release))[:, None]
    return Out(y, meta={'steps': PUMP_STEPS, 'bpm': K.BPM, 'swing': K.SWING, 'hits': [list(h) for h in PUMP_HITS],
                        'pump': dict(zip(('depth_db', 'ramp_s', 'hold_s', 'release_s'), PUMP)), 'keyed_to': 'beats'})


ITEMS = [
    Item('Stabs', 'StabGm9', 'stab', stab, layers=STAB_LAYERS, rr=2, root=STAB_CHORD[0], sets=('r2',),
         about='supersaw stab, Gm9 in stacked thirds (G3 Bb3 D4 F4 A4): seven saws a note, a 4-pole low-pass '
               'snapping shut, a dotted 16th long; high-passed at 110 Hz, mono under 150 Hz'),
    Item('Loops', 'StabPump', 'loop', stab_pump, loop=True, sets=('r2',), tags=('preview',),
         qa={'mud': 'lowcut', 'why': 'a stab loop: the low cut, like the stab'},
         about='2-bar preview of the Gm9 stab at 128 BPM on the kit swing, a 3-3-4-3-3 push under a 9 dB '
               'sidechain pump keyed to every beat'),
]
