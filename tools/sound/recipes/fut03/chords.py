"""FUT03 round 4 chords: the dubby deep-house stab on an electric piano, the warm pad bed under it, and the dub chord
loop that plays the stab through its delay, its reverb and a filter ride.

- DubStabGm9: Gm9 voiced open from G3 (G3 D4 F4 A4 Bb4: the ninth rubbing against the minor third on top) on an FM
  electric piano: per note a carrier with its modulator
  on the same pitch (every partial harmonic), the index falling from bright to mellow in about 0.3 s (the bark of a
  struck tine, harder with velocity), a short tine partial on the 14th harmonic for the "ding", and a soft 4x tanh on each
  note, the warmth of a driven amp (the chord driven as one intermodulates off the key). The chord holds about a dotted 8th and is gone by 0.6 s. 3 layers, 2 round robins (each
  its own small detune and tine phase). High-passed at 110 Hz, mono under 150 Hz.
- PadWarm: the same Gm9 as a warm pad bed, 4 bars long and looped whole: three saws a note (the outer two 6 cents
  either way, across the field), every voice on a whole number of cycles over the loop so it joins without a crossfade,
  through a low-pass that breathes between 0.9 and 1.6 kHz once over the 4 bars. High-passed at 120 Hz, mono under
  150 Hz.
- DubChordLoop: 2 bars of the stab's dub rhythm (off the kicks: mostly the swung "a"s, pushing into the beats) as the
  context loop plays it: a 3/16 echo that darkens as it repeats, a short room, the low-pass opening over the 2 bars, and the kick-keyed
  pump.
"""
import numpy as np

from tools.sound import dsp48 as D
from tools.sound import parts as P
from tools.sound import phrase as Ph
from tools.sound.item import Item, Out
from tools.sound.kits import fut03 as K

SR = D.SR
SETS = ('r4',)
CHORD = (K.ROOT + 12, K.ROOT + 19, K.ROOT + 22, K.ROOT + 26, K.ROOT + 27)   # G3 D4 F4 A4 Bb4: Gm9, open
# (the deep-house voicing: root, fifth, seventh, ninth and the minor third on top, the ninth rubbing against it;
# nothing within a fourth of the root, so the tuning reads G3 alone)
ROOT_NOTE = K.ROOT + 12
STAB_LAYERS = (0.5, 0.75, 1.0)
EP_INDEX = ((1.2, 2.6), (0.35, 0.6), 0.09)   # FM index: attack (soft, hard), rest (soft, hard), fall (s)
TINE = (14, (0.10, 0.22), 0.012)             # the tine: harmonic, level (soft, hard), fall (s)
EP_DECAY = 0.9                               # the piano's own fall (s)
HOLD, RELEASE = 0.26, 0.18                   # the stab's gate, then a raised-cosine release: gone by 0.6 s
EP_DRIVE = 1.3


def ep_note(f, n, v, r):
    """One electric-piano note on f: FM with the modulator on the note (harmonic), its index falling, a tine on the
    14th harmonic, the piano's own decay."""
    t = P.times(n)
    (ia, ir, tau) = (P.lerp(EP_INDEX[0], v), P.lerp(EP_INDEX[1], v), EP_INDEX[2])
    idx = ir + (ia - ir) * np.exp(-t / tau)
    ph = 2 * np.pi * f * t + r.uniform(0, 2 * np.pi)
    x = np.sin(ph + idx * np.sin(ph))
    h, (lo, hi), ttau = TINE
    if f * h < 16000:
        x = x + P.lerp((lo, hi), v) * np.sin(h * ph + r.uniform(0, 2 * np.pi)) * np.exp(-t / ttau)
    return x * np.exp(-t / EP_DECAY)


def dub_stab(take):
    """Gm9 on the electric piano: every note its own small detune (under a cent) and tine phase per round robin, panned
    a little across the field; gated, released, driven softly at 4x."""
    v = take.vel
    r = take.rng('ep')
    n = int(0.62 * SR)
    t = P.times(n)
    y = np.zeros((n, 2))
    f0s = []
    for k, note in enumerate(CHORD):
        f = float(D.midi_hz(note)) * (1 + 0.0004 * r.uniform(-1, 1) if note != ROOT_NOTE else 1.0)
        f0s.append(f)
        x = D.oversampled(lambda w: D.tanh_adaa(w, EP_DRIVE), ep_note(f, n, v, r), 4)   # each note its own amp: a
        p = (-0.5 + k / (len(CHORD) - 1)) * 0.6                                       # chord driven as one would
        y += Ph.pan(x, p) / np.sqrt(2)                                                # intermodulate off the key
    u = np.clip((t - HOLD) / RELEASE, 0.0, 1.0)
    amp = P.raised(n, 0.0015) * (0.5 + 0.5 * np.cos(np.pi * u))
    y = y * amp[:, None] / len(CHORD)
    y = D.mono_below(D.bfilt(y, 110.0, 'highpass', order=4), 150.0)
    return Out(y * P.vel_gain(v, 6.0, STAB_LAYERS[0]), f0s=tuple(sorted(f0s)))


# ------------------------------------------------------------------ the pad bed
PAD_STEPS = 64                               # 4 bars, the context loop's length
PAD_DETUNE = 6.0                             # cents: the outer saws
PAD_FC = (900.0, 1600.0)                     # the low-pass breathes between these, once over the loop
PAD_FMAX = 8000.0


def whole(f, L):
    """f moved to the nearest whole number of cycles over L samples (at most half a cycle per loop off: 0.3 cents on
    G3 over 4 bars), so the voice joins at the loop's seam."""
    T = L / SR
    return max(1, int(np.floor(f * T + 0.5))) / T


def pad_warm(take):
    L = Ph.loop_len(K, PAD_STEPS)
    y = np.zeros((L, 2))
    f0s = []
    r = D.rng(take.kit, take.item.name, 'phases')
    for k, note in enumerate(CHORD):
        for j, d in enumerate((-PAD_DETUNE, 0.0, PAD_DETUNE)):
            f = whole(float(D.midi_hz(note)) * 2 ** (d / 1200), L)
            f0s.append(f)
            x = D.additive_saw(f, L, r.uniform(0, 1), fmax=PAD_FMAX)
            y += Ph.pan(x, 0.7 * d / PAD_DETUNE) * (1.0 if d == 0 else 0.8)
    u = np.arange(L) / L
    fc = PAD_FC[0] + (PAD_FC[1] - PAD_FC[0]) * (0.5 - 0.5 * np.cos(2 * np.pi * u))
    out = np.empty_like(y)
    for c in range(2):
        z = np.tile(y[:, c], 3)
        out[:, c] = D.svf_tpt(D.svf_tpt(z, np.tile(fc, 3), q=0.54)[0], np.tile(fc, 3), q=1.31)[0][L:2 * L]
    out = Ph.circular(lambda w: D.bfilt(w, 120.0, 'highpass', order=4), out)
    out = Ph.circular(lambda w: D.mono_below(w, 150.0), out)
    return Out(out / (3 * len(CHORD)), f0s=tuple(sorted(f0s)))


# ------------------------------------------------------------------ the dub chord loop
LOOP_STEPS = 32
HITS = ((3, 0.85), (6, 0.55), (11, 0.8), (15, 0.6), (19, 0.85), (22, 0.55), (27, 0.8), (29, 0.55))   # off the
# kicks and off the bass's accents: the swung "a"s push into the beats (the pump catches each tail), with an "and"
# between. A first version put the stab on the bass's G1 accent on the "and" of beat 1, and that stack was the
# loop's loudest moment
ECHO_STEPS = 3                               # the dub delay: a dotted 8th
ECHO = dict(feedback=0.48, lp=(5200.0, 0.80), hp=380.0, taps=6, level=0.55)   # each repeat darker by lp[1]
ROOM = dict(t60=1.4, size=0.6, damp_hz=6000.0, level=0.22, hp=400.0)
SWEEP = (700.0, 5200.0)                      # the low-pass ride: closed to open over the loop
PUMP = (7.0, 0.003, 0.020, 0.200)


def dub_echo(x, L, steps=ECHO_STEPS):
    """A ping-pong dub delay round the loop: each repeat `steps` 16ths later (straight), alternating sides, darker and
    softer each time."""
    d = int(np.floor(steps * K.STEP * SR + 0.5))
    m = D.mono(x)
    out = np.zeros((L, 2))
    tap = m
    lp = ECHO['lp'][0]
    for k in range(1, ECHO['taps'] + 1):
        tap = D.bfilt(D.bfilt(tap, lp, 'lowpass'), ECHO['hp'], 'highpass') * (1.0 if k == 1 else ECHO['feedback'])
        lp *= ECHO['lp'][1]
        side = np.array([1.0, 0.35]) if k % 2 else np.array([0.35, 1.0])
        Ph.place(out, tap[:, None] * side, (k * d) % L)
    return out * ECHO['level']


def lp_ride(x, fc):
    """A 2-pole low-pass on a moving cutoff, round the loop (three passes, the middle kept)."""
    L = len(x)
    out = np.empty_like(x)
    for c in range(2):
        out[:, c] = D.svf_tpt(np.tile(x[:, c], 3), np.tile(fc, 3), q=0.9)[0][L:2 * L]
    return out


def dub_loop(take):
    G = Ph.Grid(K, LOOP_STEPS)
    L = G.length
    dry = Ph.hits_lane(K, 'DubStabGm9', HITS, G)
    wet = dub_echo(dry, L)
    send = D.bfilt(D.mono(dry + wet), ROOM['hp'], 'highpass')
    room = Ph.two_pass(lambda u: D.fdn_reverb(u, K.space_rng('dub room'), t60=ROOM['t60'], size=ROOM['size'],
                                              damp_hz=ROOM['damp_hz']), send, 0.02)[:L]
    y = dry + wet + ROOM['level'] * room
    u = np.arange(L) / L
    y = lp_ride(y, SWEEP[0] * (SWEEP[1] / SWEEP[0]) ** (0.5 - 0.5 * np.cos(2 * np.pi * u)))
    beats = [G.at(s) for s in range(0, LOOP_STEPS, 4)]
    y = y * D.undb(Ph.pump_db(beats, L, *PUMP))[:, None]
    y = Ph.circular(lambda w: D.mono_below(D.bfilt(w, 110.0, 'highpass', order=4), 150.0), y)
    return Out(y, meta={'steps': LOOP_STEPS, 'bpm': K.BPM, 'swing': K.SWING, 'hits': [list(h) for h in HITS],
                        'echo': {**ECHO, 'steps': ECHO_STEPS}, 'room': ROOM, 'lowpass_ride_hz': list(SWEEP),
                        'pump': dict(zip(('depth_db', 'ramp_s', 'hold_s', 'release_s'), PUMP))})


ITEMS = [
    Item('Stabs', 'DubStabGm9', 'stab', dub_stab, layers=STAB_LAYERS, rr=2, root=ROOT_NOTE, sets=SETS,
         about='dubby deep-house stab, Gm9 (G3 D4 F4 A4 Bb4, open) on an FM electric piano: the bark of a struck tine '
               'mellowing in 0.3 s, a short tine ding, a soft driven amp; gone by 0.6 s'),
    Item('Pad', 'PadWarm', 'pad', pad_warm, loop=True, root=ROOT_NOTE, level=-20.0, sets=SETS,
         about='warm pad bed, Gm9: three saws a note, every voice on whole cycles over 4 bars, a low-pass breathing '
               'between 0.9 and 1.6 kHz once over the loop'),
    Item('Loops', 'DubChordLoop', 'loop', dub_loop, loop=True, sets=SETS, tags=('preview',),
         qa={'mud': 'lowcut', 'why': 'a chord loop: the low cut, like the stab'},
         about='2-bar dub chord loop: the Gm9 stab off the kicks through a 3/16 ping-pong delay that darkens, a short '
               'room, a low-pass opening over the 2 bars and a kick-keyed pump'),
]
