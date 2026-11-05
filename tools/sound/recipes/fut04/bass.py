"""FUT04 bass for listening rounds 2 and 3: the acid bass as A, clean and B, warm (multisamples and two written 2-bar
acid lines in both characters, and the pick's 4-bar ride, round 3), and the kick-fed rumble.

Round 1 gave FUT01's reese Cool 2 ("Reese doesnt sound that musical and doesnt make me want to dance"). So this bass is
built around the line first, and the voice is the one techno dances to: a single saw through a resonant 4-pole ladder
whose cutoff jumps on every note and falls back (the acid "squelch"), accents that open it further and play louder,
slides that glide the pitch, and a short drive on top for the furnace grit.

- The voice: an exactly band-limited saw (additive, 4x) into the ladder (dsp48.ladder_tanh at 4x, resonance 0.9: the
  resonance stands about 15 dB over the saw's harmonics, just under the cutoff), then a tanh drive at 4x. It runs
  continuously, a monophonic voice, and a VCA gates each note.
- The sub: a mono sine under the whole voice, so the low end is one clean note even while the filter squelches. On a
  held multisample it plays the note; in the lines it follows SUB_RULE (below).
- A, AcidClean (the strict mud gate, the category default): the voice starts at 500 Hz, about 96 dB per octave, so
  150-400 Hz stays empty; the squelch sits above it. The resting cutoff is 900 Hz, so the resonance stays clear of the
  edge, whose skirt the gate's 150-400 Hz band still reads.
- B, AcidWarm (the proposed bass-body rule, ADR 0011): A's sub and squelch, plus the voice from 120 Hz under SPLIT, so
  its own harmonics fill 150-400 Hz under a saw's level (LOW_LEVEL), and a quiet second harmonic on the sine of its low
  notes. The squelch above SPLIT is A's level in both (SQUELCH_LEVEL): the first version scaled B's whole voice down
  with its body, which left its squelch 7 dB under A's in the level-matched loops.

The multisamples (E1, A1, E2, two layers) hold a note: the filter envelope falls from the attack onto the resting
cutoff, which breathes once per bar, and the sustain loops over one bar of whole cycles. The two lines are written for
this groove (four on the floor at 136 BPM, the kit's 0.54 swing, E minor) and rendered on the voice itself, so the
slides glide and the filter envelope restarts as on the hardware the sound comes from:
- every note leaves the kick room: no note sounds on a beat, and each is silent 50 ms before the next kick (the gate's
  before-hit rule, and the "room on the kick" of round 1's lesson). A line may put a squelch-only tick on a kick's 16th
  (Line.ticks): the voice above SPLIT alone, so the acid keeps its 16ths while the low end rests;
- one sub pitch under each beat's first 250 ms: the kick's own E1 tail rings that long (-10 dB at 254 ms), and a sine on
  another note beat against it (G1 against E1 swung 8 Hz, D1 4.5 Hz) and cancelled it at times, 20-33 dB deep on the
  leaps. So a note starting within SUB_PEDAL_S of a kick plays its sine on E1, a pedal, while the line's pitch stays in
  the voice; every E1 sine starts at each kick on PEDAL_PHASE, the kick tail's own phase, so the two add (per beat,
  kick and bass read 1.1 to 2.1 dB over the kick alone in 30-120 Hz, and no note falls under their power sum). Later
  notes, both characters alike, play their sine on the note, an octave down from D2 up;
- each phrase declares the eight kicks it plays against (Out.hits), and its low part (the sub and the voice under
  SPLIT) runs under a 6 dB duck held over each kick's first 80 ms (a bus sidechain's stand-in on the bass's low band;
  with every note resting under the kick it changes nothing, by design, and a tick's squelch, above it, speaks);
- a short sine note splatters into 150-400 Hz at its edges, the more the higher it sits, so the sub's edges are smooth
  (squared raised cosines, 16-24 ms), the voice's low part (under 800 Hz: B's body) rides inside the sub's envelope
  while the squelch above snaps in 3 ms, the sine stays in the kick's octave from D2 up (the leaps are in the voice),
  and B's 33 ms blips keep their snap and a fifth of their body;
- the lines take their sub-sonic high-pass zero-phase (the renderer's causal one delays the sub behind the voice).

The rumble is the kit's second bass gesture: the four-on-the-floor kick fed into the kit's dark concrete space, driven,
split into a sub tuned to E1 and a grit band above 600 Hz, and pumped so it swells after each kick and is gone before
the next. Its feed is a kick of the kit's design on E1 (a sine swept onto KICK_NOTE), not the drums' file, so the rumble
doesn't move when the drums change; the context loop plays it under the real kick, so its sub is turned in phase with
that kick's tail (RUMBLE_PHASE: it had sat against it and taken weight away), and since the fix pass it swells in only
as that tail fades (PUMP), so the two never stack into a swell louder than the kick.

Round 3 (the modern-sound standard, docs/plans/S1-modern-sound.md, FUT04 changes 4 and 8):
- the edge: the squelch (the voice above SPLIT) runs into a distortion after the VCA, as a 303 runs into a pedal: a
  presence lift at 3.5 kHz into an asymmetric tanh at 4x, a dip at 1 kHz after it, a low-pass at 12 kHz and a steep
  high-pass at 700 Hz that keeps its difference tones out of the body (EDGE). Accents drive it hardest. The lines read
  3.2-5.9 dB less peaky (PLR) and 1-6 dB brighter at 2-8 kHz against 1 kHz; A and B share the one squelch, so the A/B
  is still the body alone;
- the ride (AcidLine1RideClean, AcidLine1RideWarm): the pick played twice, as the context loop plays it, with its
  resting cutoff and resonance ridden over the 4 bars, as on the hardware's knobs: bars 1-2 as written, opening through
  bar 3, wide open (2.6 times the cutoff, resonance 0.96) on the fill bar's third beat, closed again by the downbeat
  (RIDES): over 600 Hz its centroid rises up to a third (bar 4's first half, against bar 2). The 2-bar lines don't
  ride: they are the line as written;
- the rumble's sub band reaches 100 Hz (round 2: 75) and carries the kick's octave, E2, so it fills the 63 Hz octave
  under the kick's weight; it plays all 4 bars of the context loop. The fix pass moved its swell later, after the
  kick's tail (PUMP), so the kick leads its own sub.

Provenance: written for this kit. The sine-sub split and the two characters (a steep high-pass on the voice, 500 or
120 Hz here) follow FUT01's round 2 reese (recipes/fut01/bass.py), the loop pieces are parts.py's.
"""
import functools
from dataclasses import dataclass

import numpy as np

from tools.sound import dsp48 as D
from tools.sound import parts as P
from tools.sound import phrase as Ph
from tools.sound import qa
from tools.sound.item import Item, Out
from tools.sound.kits import fut04 as K

SR = D.SR
LOOP_STEPS = 32                     # the lines and the rumble: 2 bars of 16ths
L = Ph.loop_len(K, LOOP_STEPS)      # 169412 samples
G = Ph.Grid(K, LOOP_STEPS)          # the 16th grid on the kit's one swing
KICKS = tuple(G.at(s) for s, _ in qa.GROOVES[K.GROOVE])   # four on the floor: the hits every phrase plays against
F_KICK = float(D.midi_hz(K.KICK_NOTE))   # E1: the kick, the lines' pedal and the rumble
MID_HP = {'clean': 500.0, 'warm': 120.0}   # where the voice starts: A, clean keeps 150-400 Hz empty (from 450 Hz, the
                                           # gate's 150-400 Hz band still read the edge's skirt at the end of the lines'
                                           # notes, -15 dB against -26 from 500); B, warm keeps its own harmonics down
                                           # to the sub
SUB_LEVEL = 0.5
SQUELCH_LEVEL = 2.2                 # the voice above SPLIT over the sub, the same in both characters: B is A's squelch
                                    # plus a body, so the A/B differs only under SPLIT
LOW_LEVEL = {'clean': 2.2, 'warm': 0.65}   # the voice under SPLIT over the sub: A's 500-800 Hz, at its squelch's level;
                                           # B's body, 1.6 dB under the first level (0.78), which left line 1 0.7 dB
                                           # inside bass-body's window limit even with the sub pedal; at 0.65 every
                                           # bass-body reading, the lines' and the context loop's with the bass 5 ms
                                           # early or late, keeps 2 dB or more (and a line's coinciding harmonics,
                                           # 245 Hz is E1's 6th, G1's 5th and B1's 4th, stay under a saw's roll-off).
                                           # Round 3 tried 0.75 for the kit's low-mid hole: line 1 kept only 1.3 dB, for
                                           # about 0.4 dB more in the loop's 250 Hz octave, so B stays at 0.65
WARM_H2 = 0.2                       # B's sub: a mono second harmonic, 14 dB down
SPLIT = 800.0                       # the voice under this rides the sub's envelope, the squelch above snaps with the
                                    # voice's own: at a note's edges the low mids (B's body; A's 500-800 Hz, which the
                                    # gate's 150-400 Hz band still reads on its skirt) keep their balance with the sub
RES = 0.9                           # the ladder's resonance
REST_HZ = 900.0                     # the resting cutoff
DRIVE = 1.6                         # the ladder's input drive (the 303-style growl)
GRIT = 2.2                          # the drive after the filter, at 4x: the furnace grit
# A note's envelopes: (attack, release, lead, power, delay): a raised cosine up over `attack` from `delay` after the
# onset, and down over `release` from `lead` before the gate's end, the whole raised to `power` (2: smoother edges, less
# splatter). A sine note's own edges splatter into 150-400 Hz, the more the higher it sits: a 120 ms note with 10 ms
# raised-cosine edges reads -30 dB on E1 in the gate's windows but -7 on E2 and -5 on G2; squared over 24 ms, -55 on
# E1, -41 on B1, -29 on E2.
VOICE_ENV = (0.003, 0.010, 0.0, 1, 0.0)    # the voice's snap (above SPLIT); A's high-pass comes after it
SUB_ENV = {'clean': (0.024, 0.024, 0.006, 2, 0.0), 'warm': (0.016, 0.020, 0.006, 2, 0.0)}
# the voice under SPLIT (B's body): inside the sub's envelope, 6 ms later in and 12-16 ms earlier out. The gate's
# 30-120 Hz band smears a note's edges thinner than its 150-400 Hz band does, so a body that rides the sub exactly still
# reads over it in a note's first and last window (B, a D2 note's end: -0.3 dB)
BODY_ENV = {'clean': (0.024, 0.016, 0.014, 2, 0.006), 'warm': (0.016, 0.016, 0.014, 2, 0.006)}
NOTE_END = max(e[1] - e[2] for e in (VOICE_ENV, *SUB_ENV.values(), *BODY_ENV.values()))   # silent this long after
                                                                                           # the gate
SUB_OCTAVE_FROM = 38                # the lines, both characters: from D2 up the sine plays an octave down, in the
                                    # kick's octave (D1-C2), so no short sine sits near 150 Hz; the leaps are in the
                                    # voice
SUB_PEDAL_S = 0.25                  # the lines, both characters: a note (or slide group) that starts this soon after a
                                    # kick plays its sine on the kick's note, E1, while the kick's own E1 tail still
                                    # rings (-10 dB at 254 ms, -20 dB at 340 ms): one sub pitch under each beat's first
                                    # 250 ms, so the low end never beats (a G1 against the tail's E1 swung 8 Hz, a D1
                                    # 4.5 Hz). The line's pitch is in the voice: A's squelch, B's body from 120 Hz
PEDAL_PHASE = 120.0                 # degrees: the pedal's sine, sin(2 pi E1 t + phase) from each kick, starts on the
                                    # phase of the top kick layer's E1 tail (KickSplit v5 as written: rr1 116, rr2 129,
                                    # steady within 3 degrees from 100 to 380 ms), so the two add instead of cancelling:
                                    # per beat, kick and bass read 30-120 Hz 1.1 to 2.1 dB over the kick alone, and the
                                    # context loop's warm low end holds -4.9 dB or lower (limit -2) with the bass up to
                                    # 5 ms early or late (a 74-degree swing). The softer kick layers sit elsewhere (v4
                                    # near 0), so the context loop plays every kick on the top layer
SAW_TOP = 20000.0                   # every note's saw carries its harmonics up to here
# round 3, the edge: the squelch after the VCA into a pedal (the distortion after the filter, the standard's change 8):
# pre: a presence lift (Hz, dB, Q) into the drive; drive and bias: an asymmetric tanh at 4x around ref (the squelch's
# nominal level times 0.6, so normal notes saturate a little and accents a lot); post: a dip after it (Hz, dB, Q); lp:
# a 2nd-order low-pass on the fizz; hp: a steep high-pass (96 dB per octave) under it, which takes the bias's DC and the
# drive's difference tones out of the body and away from the gate's 150-400 Hz band. Measured on the lines (round 2 to
# 3): PLR 17.2 to 14.0 dB (A, line 1), 19.8 to 15.1 (B, line 1), 20.9 to 17.7 and 23.4 to 17.5 (line 2); 2 / 4 / 8 kHz
# re 1 kHz -3.0 / -9.3 / -17.4 to +2.6 / -4.5 / -11.9 dB (A, line 1). A brighter voicing (+8 dB into the drive) put a
# held E1's skirt over the strict gate (-19.7 dB)
EDGE = dict(pre=(3500.0, 6.0, 0.6), drive=3.0, bias=0.2, post=(1000.0, -3.0, 0.7), lp=12000.0, hp=700.0)
EDGE_REF = 0.6                      # the drive's reference level, re SQUELCH_LEVEL (defined below)


def saw(freq, k=4):
    """An exactly band-limited saw at k x on `freq` (Hz per sample, at the output rate), k x as many samples: every note
    of the track has its harmonics up to SAW_TOP, the highest under k x's Nyquist. A PolyBLEP saw's faint aliases
    landed next to the ladder's resonance on E1 and E2, which lifted them: -58 dB between the harmonics against -92."""
    f = np.asarray(freq, dtype=float)
    top = min(SAW_TOP * f.max() / f.min(), 0.45 * SR * k)
    return D.additive_saw(np.repeat(f, k) / k, len(f) * k, 0.0, fmax=top / k)


def ladder(x, fc, res, fs, drive=1.0):
    """dsp48.ladder_tanh with the resonance per sample (the 4-bar ride's knob); the same arithmetic, so a constant
    resonance gives its bytes exactly."""
    n = len(x)
    g = 1.0 - np.exp(-2 * np.pi * np.clip(np.broadcast_to(np.asarray(fc, dtype=float), (n,)), 10, fs * 0.45) / fs)
    r4 = (4.0 * np.broadcast_to(np.asarray(res, dtype=float), (n,))).tolist()
    g, xs = g.tolist(), x.tolist()
    s0 = s1 = s2 = s3 = 0.0
    th = np.tanh
    y = [0.0] * n
    for i in range(n):
        gi = g[i]
        u = th(drive * xs[i] - r4[i] * s3)
        s0 += gi * (u - th(s0))
        s1 += gi * (th(s0) - th(s1))
        s2 += gi * (th(s1) - th(s2))
        s3 += gi * (th(s2) - th(s3))
        y[i] = s3
    return np.array(y)


def acid(freq, fc, drive=DRIVE, k=4, res=None):
    """The acid voice, mono: the saw on `freq` (Hz per sample), the ladder on `fc` (Hz per sample, at the output rate)
    and the grit, all at k x. res: the resonance per sample (the ride), RES when None."""
    if res is None:
        y = D.ladder_tanh(saw(freq, k), np.repeat(fc, k), RES, SR * k, drive=drive)
    else:
        y = ladder(saw(freq, k), np.repeat(fc, k), np.repeat(res, k), SR * k, drive=drive)
    return D.down(D.tanh_adaa(2.5 * y, GRIT), k)


def edge(x):
    """The squelch's pedal (EDGE), round the loop: the presence lift, the asymmetric tanh at 4x around its reference,
    the dip, the low-pass and the steep high-pass."""
    circ = Ph.circular
    pre_hz, pre_db, pre_q = EDGE['pre']
    ref = EDGE_REF * SQUELCH_LEVEL
    u = circ(lambda v: D.peaking(v, pre_hz, pre_db, pre_q), x)
    u = D.oversampled(lambda v: D.tanh_adaa(v / ref, EDGE['drive'], bias=EDGE['bias']) * ref, u, 4)
    u = circ(lambda v: D.peaking(v, *EDGE['post']), u)
    u = circ(lambda v: D.bfilt(v, EDGE['lp'], 'lowpass', order=2), u)
    return circ(lambda v: P.hp_steep(v, EDGE['hp']), u)


def rise(n, env):
    """An envelope's attack alone, for a held note: n samples."""
    a, _, _, p, delay = env
    u = np.clip((np.arange(n) / SR - delay) / a, 0.0, 1.0)
    return (0.5 - 0.5 * np.cos(np.pi * u)) ** p


def envelope(m, env):
    """A note's envelope (VOICE_ENV, SUB_ENV, BODY_ENV) over its gate of m samples, as long as it sounds."""
    a, r, lead, p, delay = env
    t = np.arange(m - int(lead * SR) + int(r * SR)) / SR
    up = np.clip((t - delay) / a, 0.0, 1.0)
    down = np.clip((t - (m / SR - lead)) / r, 0.0, 1.0)
    return ((0.5 - 0.5 * np.cos(np.pi * up)) * (0.5 + 0.5 * np.cos(np.pi * down))) ** p


def h2_weight(freq):
    """B's second harmonic, where it stays in the sub band: full on E1 (82 Hz), gone from B1 up (2f over 120 Hz would
    add to the low mids instead)."""
    return WARM_H2 * np.clip((125.0 - 2.0 * np.asarray(freq, dtype=float)) / 40.0, 0.0, 1.0)


def sub_wave(cycles, char, freq):
    """The mono sine sub from its running phase in cycles; B adds a quiet second harmonic (h2_weight)."""
    ph = 2 * np.pi * cycles
    return np.sin(ph) + (h2_weight(freq) * np.sin(2 * ph) if char == 'warm' else 0.0)


def character(sub, voice, gate_v, gate_b, char, vel=1.0):
    """Sub and voice at the character's balance; the soft layer leans on the sub. Mono. The voice splits at SPLIT: under
    it, it rides the sub's envelope (gate_b) at the character's level (LOW_LEVEL), then the high-pass where the
    character starts it; above it, its own (gate_v) at the squelch's level through the edge (round 3: after the VCA, as
    a pedal after the hardware), one squelch for both characters, so the A/B differs only under SPLIT. Everything runs
    circularly, so a phrase's loop joins. A gates before its high-pass, which also clears the gates' splatter out of
    150-400 Hz; B high-passes first, so the 120 Hz filter's ringing never outlasts a note's sub; the edge's own steep
    high-pass at 700 Hz clears the squelch's."""
    circ = lambda fn, x: Ph.circular(fn, x)  # noqa: E731
    hp = lambda x: circ(lambda u: P.hp_steep(u, MID_HP[char]), x)  # noqa: E731
    low = lambda x: LOW_LEVEL[char] * circ(lambda u: D.lr4(u, SPLIT, 'lowpass'), x) * gate_b  # noqa: E731
    squelch = edge(SQUELCH_LEVEL * circ(lambda u: D.lr4(u, SPLIT, 'highpass'), voice) * gate_v)
    body = hp(low(voice)) if char == 'clean' else low(hp(voice))
    return SUB_LEVEL * vel ** 0.6 * sub + vel ** 1.5 * (body + squelch)


# ------------------------------------------------------------------ the multisamples
ACID_NOTES = (28, 33, 40)           # E1, A1, E2: zones over the lines' range, E1 to G2
ACID_LAYERS = (0.6, 1.0)
LOOP_S = 16 * K.STEP                # one bar: the resting cutoff breathes once per loop
ATTACK_S = 0.5                      # the filter envelope's fall onto the resting cutoff, before the loop
ENV_OCT = (1.6, 2.6)                # how far the attack opens the filter, soft and hard layer (octaves)
ENV_TAU = 0.14
REST = (0.8, 1.0)                   # the resting cutoff, soft and hard layer, re REST_HZ
BREATH = 0.10                       # the cutoff breathes this much over the bar (35% sweeps the resonance across
                                    # harmonics fast enough to spread them: the alias check read -44 dB)


def acid_note(take, char):
    """One zone and layer, held: the attack opens the filter, its envelope falls onto the resting cutoff by the loop
    start (exactly, so the loop is the voice's own steady state), then one bar of the cutoff breathing, looped."""
    Lp, grid, f = P.loop_grid(take.f0, LOOP_S)
    A0 = int(ATTACK_S * SR)
    n = A0 + int(0.02 * SR) + Lp
    t = np.arange(n)
    hard = take.layer - 1
    fall = np.clip(t / A0, 0.0, 1.0)
    env = np.exp(-t / SR / ENV_TAU) * (1.0 - fall * fall * (3 - 2 * fall))
    breath = 0.5 - 0.5 * np.cos(2 * np.pi * (t - A0) / Lp)
    fc = REST_HZ * REST[hard] * (1.0 + BREATH * breath) * 2.0 ** (ENV_OCT[hard] * env)
    voice = acid(np.full(n, f), fc)
    sub = sub_wave(D.phase(f, n), char, f) * rise(n, SUB_ENV[char])
    y = character(sub, voice, rise(n, VOICE_ENV), rise(n, BODY_ENV[char]), char, take.vel)
    y, loop = P.close_loop(D.st(y), A0, Lp)
    return Out(y[:, 0], loop=loop, f0s=(f,))


def acid_clean(take):
    return acid_note(take, 'clean')


def acid_warm(take):
    return acid_note(take, 'warm')


# ------------------------------------------------------------------ the lines
@dataclass(frozen=True)
class Line:
    """A 2-bar acid line. notes: (16th, MIDI, length in 16ths, velocity, slide): velocity 1.0 is an accent (the
    filter opens further, 3 dB louder), under 0.5 a ghost; slide glides into the note from the one before, which holds
    into it (no new filter envelope). knob: (16th, Hz) the resting cutoff, a hand on the knob, joined in log frequency
    round the loop. ticks: (16th, MIDI, length in 16ths, velocity) squelch-only notes on a kick's 16th: the voice above
    SPLIT alone, no sub and no body, so the low end still rests under the kick while the acid keeps its 16ths."""
    name: str
    notes: tuple
    knob: tuple
    about: str
    why: str
    ticks: tuple = ()


LINES = (
    Line('AcidLine1',
         ((1, 28, 1.0, 0.8, False), (2, 40, 1.4, 1.0, False),
          (5, 31, 0.9, 0.75, False), (6, 33, 1.4, 0.85, True),
          (9, 28, 1.0, 0.8, False), (10, 38, 1.4, 1.0, False),
          (13, 35, 2.3, 0.8, False),
          (17, 28, 1.0, 0.8, False), (18, 40, 0.9, 1.0, False), (19, 38, 0.35, 0.55, True),
          (21, 36, 2.3, 0.85, False),
          (25, 28, 1.0, 0.8, False), (26, 43, 0.9, 1.0, False), (27, 40, 0.35, 0.55, True),
          (29, 35, 0.9, 0.75, False), (30, 38, 0.9, 0.85, False), (31, 40, 0.35, 0.45, False)),
         ((0, 850.0), (10, 1200.0), (16, 900.0), (26, 2000.0), (32, 850.0)),
         'the rolling hook: kick, root on the 16th after it, an accented leap on the off-beat 8th (E2, G1 sliding to '
         'A1, D2), a squelch tick on beat 3\'s kick, and bar 1 ends on a held B1; bar 2 brings the hook back varied '
         '(E2 sliding down to D2), holds a dark C2 for a beat, peaks on G2 sliding to E2 and climbs B1 D2 E2 home',
         'the bass answers the kick on the 16th after it, so the body locks to it, and the leap\'s accent on the '
         'off-beat 8th is the bounce; the hook comes back with a new target each time (octave, fourth, seventh, then '
         'the top G2), while the two held notes (the fifth, closing bar 1, and the minor sixth) and the ticks on the '
         'kick break the cell, so the ear follows the line over the bar line',
         ticks=((8, 40, 0.3, 0.45), (24, 40, 0.3, 0.45))),
    Line('AcidLine2',
         ((2, 28, 1.45, 0.85, False), (6, 28, 1.0, 0.8, False), (7, 40, 0.35, 1.0, False),
          (10, 31, 1.45, 0.85, False), (13, 33, 0.5, 0.45, False), (14, 28, 1.0, 0.8, False),
          (15, 38, 0.35, 1.0, False),
          (17, 28, 0.6, 0.5, False), (18, 28, 1.0, 0.85, False), (19, 40, 0.35, 1.0, False),
          (21, 31, 0.6, 0.5, False), (22, 31, 1.0, 0.85, False), (23, 43, 0.35, 1.0, False),
          (25, 33, 0.6, 0.5, False), (26, 35, 1.4, 0.9, True), (29, 36, 0.9, 0.8, False),
          (30, 35, 0.9, 0.85, True), (31, 40, 0.35, 0.45, False)),
         ((0, 850.0), (12, 950.0), (16, 1000.0), (28, 2400.0), (32, 850.0)),
         'the off-beat pulse: bar 1 holds the root on the off-beat 8ths with an accented octave blip on the last 16th '
         '(dum, ba-DIT), then G1 and D2 for the blip; bar 2 doubles it into a 16th roll that climbs E, G, A to B1 '
         '(a slide), C2, and falls back to B1 as the filter opens',
         'the held off-beats push against the kick like a classic techno off-beat bass, the blip on the 16th before '
         'each kick pulls into it, and the motif (off-beat note plus blip) comes back as the bar 2 roll, two 16ths '
         'and a blip per beat, rising, so bar 2 lifts and lands on the loop\'s first kick'),
)
LINE_BY_NAME = {ln.name: ln for ln in LINES}
PICK = 'AcidLine1'                  # the designer's pick for the context loop
ENV = {'accent': (2.8, 0.07), 'normal': (1.7, 0.10), 'ghost': (0.9, 0.05)}   # filter envelope: octaves, decay (s)
ACCENT_DB = 3.0
SLIDE_S = 0.050                     # the glide into a slid note
ROOM = 0.050                        # every note is silent this long before the next kick
GAP = NOTE_END + 0.002              # a gap between notes that don't slide (each restarts the filter envelope): a
                                    # note is silent before the next starts, so no pitch moves under a sounding tail
DUCK = (6.0, 0.002, 0.080, 0.030)   # the duck under each kick: dB, ramp, hold, release (Ph.pump_db)
TAIL = int(0.3 * SR)
SUBSONIC = 20.0                     # the lines' own sub-sonic high-pass, zero-phase (the items skip the renderer's):
                                    # the renderer's causal 4th-order one delays the sub about 10 ms behind the voice,
                                    # which the gate's windows read at every note's onset (A, line 1: -16 dB became -7)


def kind(vel):
    return 'accent' if vel >= 0.95 else ('ghost' if vel < 0.5 else 'normal')


def next_kick(pos):
    """The sample of the first kick after pos (the loop's next start counts)."""
    return next((k for k in KICKS if k > pos), KICKS[0] + L)


def events(line):
    """The line's notes and ticks in time order: (16th, MIDI, length in 16ths, velocity, slide, tick)."""
    ev = [(*n, False) for n in line.notes] + [(s, m, ln, v, False, True) for s, m, ln, v in line.ticks]
    return sorted(ev, key=lambda e: e[0])


def spans(line):
    """Per note and tick: onset sample, gate end sample (its written length, before the next event that doesn't slide,
    and ROOM + NOTE_END before the next kick), MIDI, velocity, whether it slides in, whether it is a tick."""
    ev = events(line)
    on = [G.at(e[0]) for e in ev]
    out = []
    for i, (step, note, length, vel, slide, tick) in enumerate(ev):
        end = on[i] + int(round(length * G.s16))
        if i + 1 < len(on) and not ev[i + 1][4]:
            end = min(end, on[i + 1] - int(GAP * SR))
        end = min(end, next_kick(on[i]) - int((ROOM + NOTE_END) * SR))
        out.append((on[i], end, note, vel, slide, tick))
    return out


def note_spans(line):
    """spans() of the notes alone (no ticks): onset, gate end, MIDI, velocity, slide."""
    return [s[:5] for s in spans(line) if not s[5]]


def last_kick(pos):
    """The sample of the last kick at or before pos."""
    return max(k for k in KICKS if k <= pos)


def pedal(on):
    """Whether a note (or slide group) starting at sample `on` plays its sub on the kick's note (SUB_PEDAL_S)."""
    return on - last_kick(on) < int(SUB_PEDAL_S * SR)


def knob(line, n):
    """The resting cutoff per sample: the line's breakpoints joined in log frequency, round the loop."""
    pts = [(G.at(s) if s < LOOP_STEPS else L + G.at(s - LOOP_STEPS), np.log2(hz)) for s, hz in line.knob]
    x = np.arange(n) % L
    return 2.0 ** np.interp(x, [p for p, _ in pts], [v for _, v in pts])


@dataclass(frozen=True, eq=False)
class Parts:
    """A line played on the voice, before its envelopes (L + TAIL samples unless noted)."""
    freq: np.ndarray          # the voice's pitch track, Hz
    sub_freq: np.ndarray      # the sub's pitch: the kick's note under a pedal, else the note's (an octave down from D2
                              # up)
    sub_cycles: np.ndarray    # the sub's phase in cycles: from PEDAL_PHASE at each kick under a pedal, else running
    groups: tuple             # (onset, end) of every gate the voice plays: notes, slides joined to the note before (one
                              # gate, the pitch gliding, no new filter envelope), and ticks
    low: tuple                # (onset, end, pedal) of the gates the sub and the voice under SPLIT play: no ticks
    level: np.ndarray         # per sample, accents up
    voice: np.ndarray         # the voice over one loop (L samples), ungated: it runs on like the hardware's oscillator,
                              # and every gate is shut over the seam


# round 3, the ride: the pick played twice, as the context loop plays it, with the line's resting cutoff (its knob)
# times a 4-bar ride and the ladder's resonance ridden with it, as a hand on the hardware's two knobs: (16th of the 4
# bars, value), joined in log frequency (cutoff) and linearly (resonance). Bars 1-2 play the line as written (x1, RES);
# it opens through bar 3, is widest on the fill bar's third beat (x2.6, 0.96) and closes by the loop's downbeat
RIDES = {'AcidLine1': dict(cutoff=((0, 1.0), (32, 1.0), (48, 1.8), (56, 2.6), (64, 1.0)),
                           res=((0, RES), (32, RES), (48, 0.94), (56, 0.96), (64, RES)))}
RIDE_PASSES = 2                     # the ride's length in lines: 4 bars
RIDE_GRID = Ph.Grid(K, RIDE_PASSES * LOOP_STEPS, RIDE_PASSES * L)


def ride_curve(points, n, log):
    """A ride's breakpoints per sample over its 4 bars (n samples, wrapping), log2-joined (cutoff) or linear."""
    xs = [RIDE_GRID.at(st) if st < RIDE_GRID.steps else RIDE_GRID.length for st, _ in points]
    vs = [np.log2(v) if log else v for _, v in points]
    y = np.interp(np.arange(n) % RIDE_GRID.length, xs, vs)
    return 2.0 ** y if log else y


@functools.lru_cache(maxsize=None)
def line_parts(name, ride=False):
    """The line played on the voice (Parts); ride: twice, as the 4-bar ride (RIDES), its spans and kicks repeated a
    line later. Under a pedal (SUB_PEDAL_S) the sub holds the kick's note from its gate's onset to the end of its
    release; every sub on the kick's note (a pedal, or an E's) runs from PEDAL_PHASE at the last kick, so it adds to the
    kick's own tail."""
    line = LINE_BY_NAME[name]
    passes = RIDE_PASSES if ride else 1
    LL = passes * L
    sp = [(on + p * L, end + p * L, *rest) for p in range(passes) for on, end, *rest in spans(line)]
    n = LL + TAIL
    t = np.arange(n)
    freq = np.full(n, float(D.midi_hz(sp[0][2])))
    octave = np.ones(n)
    env = np.zeros(n)
    amount = np.zeros(n)
    level = np.zeros(n)
    groups, low = [], []
    for on, end, note, vel, slide, tick in sp:
        f = float(D.midi_hz(note))
        if slide and groups:
            if not low or low[-1][0] != groups[-1][0]:
                raise ValueError(f'{name}: a note slides from a tick at sample {groups[-1][0]}')
            prev = freq[on - 1]
            u = np.clip((t[on:] - on) / (SLIDE_S * SR), 0.0, 1.0)
            freq[on:] = 2.0 ** (np.log2(prev) + (np.log2(f) - np.log2(prev)) * (0.5 - 0.5 * np.cos(np.pi * u)))
            groups[-1][1] = end
            low[-1][1] = end
        else:
            freq[on:] = f
            octs, tau = ENV[kind(vel)]
            env[on:] = np.exp(-(t[on:] - on) / SR / tau)
            amount[on:] = octs
            groups.append([on, end])
            if not tick:
                low.append([on, end, pedal(on % L)])
        octave[on:] = 0.5 if note >= SUB_OCTAVE_FROM else 1.0
        level[on:] = vel * D.undb(ACCENT_DB if kind(vel) == 'accent' else 0.0)
    level = np.convolve(level, np.ones(240) / 240, mode='same')     # 5 ms: no step in the level
    rest = knob(line, n)
    res = None
    if ride:
        rest = rest * ride_curve(RIDES[name]['cutoff'], n, True)
        res = ride_curve(RIDES[name]['res'], n, False)
    fc = np.minimum(rest * 2.0 ** (amount * env), 12000.0)
    sub_freq = freq * octave
    cycles = D.phase(sub_freq, n)
    for on, end, ped in low:
        span = slice(on, end + int(np.ceil(NOTE_END * SR)))
        if ped:
            sub_freq[span] = F_KICK
        if np.all(np.abs(sub_freq[span] - F_KICK) < 1e-9):   # every sub on the kick's note, the pedal's or an E's
            cycles[span] = F_KICK * (t[span] - (last_kick(on % L) + on - on % L)) / SR + PEDAL_PHASE / 360.0
    return Parts(freq, sub_freq, cycles, tuple(map(tuple, groups)), tuple(map(tuple, low)), level,
                 acid(freq, fc, res=res)[:LL])


def fold(x, LL=L):
    """A part of LL + TAIL samples as one loop of LL: its tail added to the start."""
    return np.concatenate([x[:TAIL] + x[LL:], x[TAIL:LL]])


def gates(groups, level, env, weight=None):
    """Every gate's envelope (VOICE_ENV or SUB_ENV) times the level, as long as the level (a loop plus TAIL);
    weight(seconds) scales a gate by its length. groups: (onset, end, ...) each."""
    g = np.zeros(len(level))
    for on, end, *_ in groups:
        e = envelope(end - on, env) * (1.0 if weight is None else weight((end - on) / SR))
        g[on:on + len(e)] += e
    return g * level


def body_weight(seconds):
    """B's body on a note of this length: a blip (the 33 ms ones before a kick) keeps its snap and 20% of its body, a
    note from 90 ms on all of it. A short note's sine is a few cycles that the gate's 30-120 Hz band spreads thin,
    while its low mids stay put, so a blip with full body reads as low mids without a sub (+2 to +4 dB)."""
    return float(np.clip((seconds - 0.030) / 0.060, 0.2, 1.0))


def seam_silence(hold=0.020, ramp=0.010, LL=L):
    """1 everywhere but round the loop's seam (a loop of LL samples), the first kick, where the bass rests: down to 0
    over `ramp`, `hold` before the seam, and back up `hold` after it. It only touches the sub-sonic filter's tails
    (about -100 dB there), and turns them into true silence, so the 24-bit seam isn't a lone LSB step."""
    t = np.minimum(np.arange(LL), LL - np.arange(LL)) / SR
    u = np.clip((t - hold) / ramp, 0.0, 1.0)
    return 0.5 - 0.5 * np.cos(np.pi * u)


SUB_RULE = (f'on {D.note_name(K.KICK_NOTE)}, the kick\'s note, for a note starting within {SUB_PEDAL_S:g} s of a '
            f'kick (a pedal, from {PEDAL_PHASE:g} degrees at each kick, in phase with the top kick layer\'s tail); '
            f'after that on the note, an octave down from {D.note_name(SUB_OCTAVE_FROM)} up')


def ride_kicks():
    """The four-on-the-floor kicks over the ride's 4 bars."""
    return tuple(k + p * L for p in range(RIDE_PASSES) for k in KICKS)


def play_line(name, char, ride=False):
    """A line (or its 4-bar ride) in one character: the sub and the voice at the character's balance; the sub and the
    voice under SPLIT run under the duck, the squelch above it (all a tick plays) doesn't. Both characters share the
    sub (SUB_RULE) and the squelch; they differ under SPLIT. Returns (audio, the kicks it plays against)."""
    p = line_parts(name, ride)
    LL = len(p.voice)
    kicks = ride_kicks() if ride else KICKS
    duck = D.undb(Ph.pump_db(kicks, LL, *DUCK))
    gate_s = gates(p.low, p.level, SUB_ENV[char])
    sub = fold(sub_wave(p.sub_cycles, char, p.sub_freq) * gate_s, LL) * duck
    gate_b = fold(gates(p.low, p.level, BODY_ENV[char], body_weight if char == 'warm' else None), LL) * duck
    y = character(sub, p.voice, fold(gates(p.groups, p.level, VOICE_ENV), LL), gate_b, char)
    y = Ph.circular(lambda u: D.bfilt(u, SUBSONIC, 'highpass', order=2, zero_phase=True), y) * seam_silence(LL=LL)
    return y, kicks


def line_meta(name, kicks, steps):
    line = LINE_BY_NAME[name]
    return {'bpm': K.BPM, 'swing': K.SWING, 'steps': steps, 'pick': name == PICK,
            'notes': [{'step': s, 'note': D.note_name(nt), 'length16': ln, 'vel': v, 'slide': sl}
                      for s, nt, ln, v, sl in line.notes],
            'ticks': [{'step': s, 'note': D.note_name(nt), 'length16': ln, 'vel': v} for s, nt, ln, v in line.ticks],
            'gates_s': [[round(a / SR, 4), round(b / SR, 4)] for a, b, *_ in spans(line)],
            'sub': SUB_RULE, 'pedal_s': SUB_PEDAL_S, 'pedal_phase_deg': PEDAL_PHASE, 'edge': EDGE,
            'edge_ref': EDGE_REF, 'kicks': list(kicks),
            'duck': dict(zip(('db', 'ramp_s', 'hold_s', 'release_s'), DUCK))}


def acid_line(take, char):
    """One 2-bar line in one character, with the kicks it plays against."""
    name = take.item.name.replace('Clean', '').replace('Warm', '')
    y, kicks = play_line(name, char)
    return Out(y, hits=kicks, meta=line_meta(name, kicks, LOOP_STEPS))


def acid_ride(take, char):
    """The pick's 4-bar ride in one character (RIDES): the line twice, its cutoff and resonance ridden."""
    name = take.item.name.replace('Ride', '').replace('Clean', '').replace('Warm', '')
    y, kicks = play_line(name, char, ride=True)
    meta = {**line_meta(name, kicks, RIDE_PASSES * LOOP_STEPS), 'passes': RIDE_PASSES,
            'ride': {k: [list(pt) for pt in v] for k, v in RIDES[name].items()}}
    return Out(y, hits=kicks, meta=meta)


def acid_line_clean(take):
    return acid_line(take, 'clean')


def acid_line_warm(take):
    return acid_line(take, 'warm')


def acid_ride_clean(take):
    return acid_ride(take, 'clean')


def acid_ride_warm(take):
    return acid_ride(take, 'warm')


# ------------------------------------------------------------------ the rumble
RUMBLE_SUB_HZ = 100.0               # the sub band's top (round 3; round 2: 75 Hz): it reaches the kick's octave, E2,
                                    # so the rumble fills the 63 Hz octave under the kick; 150-400 Hz still reads it
                                    # far down (the gate's gaps -56 dB)
RUMBLE_GRIT = (600.0, 2400.0)       # the grit band: above the low mids and their skirt
# the pump: (seconds after the kick, dB), joined by raised cosines: held down under the kick, swelling into the gap,
# gone 40 dB before the next kick (441 ms at 136 BPM). The fix pass (the A&R review: the low end peaked 140-200 ms after
# each kick, 3.6 dB over the kick's own 40-100 ms) holds it down to 90 ms and lets it swell in as the kick's E1 tail
# fades: -3 dB at 210 ms, full from 270 to 300 ms, 40 dB down by 380 ms (round 3: full from 150 to 240 ms, down by
# 375). In phase with that tail it takes the E1 over instead of stacking on it, its swell peaks 270-300 ms after the
# kick, under the kick's own peak, and it is gone 60 ms before the next kick, so the kick's jump keeps its contrast (a
# swell held to 340 ms took 4 dB off the jump in 40-120 Hz)
PUMP = ((0.0, -40.0), (0.090, -40.0), (0.210, -3.0), (0.270, 0.0), (0.300, 0.0), (0.380, -40.0))
RUMBLE_MIX = (1.0, 0.6, 2.0, 0.8)   # the low wash, the E1 resonance, the grit (about 13 dB under the sub) and (round 3)
                                    # the E2 resonance, the kick's octave: the growl over the sub
ROLL = (8.0, 0.040)                 # the grit rolls on the swung 16ths: up to 8 dB, each 16th falling over 40 ms
RUMBLE_PHASE = 45.0                 # degrees: the sub's phase turned by this (every frequency, round the loop), so its
                                    # E1 swells in phase with the drums' kick tail. Fed by its own sine kick, it sat
                                    # against it: round 2 (the band to 75 Hz) read 0.0 and -0.8 dB per beat in 30-120
                                    # Hz against the kick alone, unturned, and +2.3 at 135 degrees. Round 3's band to
                                    # 100 Hz shifts its phase at E1: at 135 it read +1.0, at 180 -0.7 (taking weight
                                    # away); 45, the best of a 15-degree sweep with the E2 resonance in. The fix pass
                                    # A/B'd 135 again, against the swell over the kick: there the kick led, but the
                                    # rumble drifted against the kick's tail (a 40 ms window 1.6 dB under the kick
                                    # alone) and sat in quadrature with the bass's pedal, which follows the kick's tail
                                    # (PEDAL_PHASE): with the bass 3-5 ms late the two cancelled on the off-beats,
                                    # and the warm low end's bass-body reading went from -6.2 to +3.0 dB. In phase with
                                    # both, nothing cancels at any offset, so 45 stays and the swell's timing (PUMP),
                                    # its level and the bass's sidechain (context.py) let the kick lead


def rotate(x, deg):
    """x with the phase of every frequency advanced by `deg` degrees: a constant-phase all-pass, circular, so the loop
    still joins (sin becomes sin(. + deg))."""
    X = np.fft.rfft(x)
    X[1:] = X[1:] * np.exp(1j * np.radians(deg))
    return np.fft.irfft(X, n=len(x))


def feed_kick(n, vel):
    """The rumble's feed: a kick of the kit's design, a sine swept onto E1 with a short knock, mono."""
    t = P.times(n)
    ph = P.sweep(n, 150.0 + 60.0 * vel, 0.006, 0.030, 0.30, F_KICK)
    return vel * np.sin(ph) * P.gate(t, 0.04, 0.11) * P.raised(n, 0.001)


def pump(n):
    """The pump's gain per sample over the loop: each kick's shape (PUMP) from its onset to the next kick."""
    g = np.zeros(n)
    times = [p for p, _ in PUMP]
    for i, k in enumerate(KICKS):
        nxt = KICKS[i + 1] if i + 1 < len(KICKS) else L
        tau = np.arange(nxt - k) / SR
        seg = np.searchsorted(times, tau, side='right') - 1
        seg = np.clip(seg, 0, len(PUMP) - 1)
        a = np.array([PUMP[j][1] for j in seg])
        b = np.array([PUMP[min(j + 1, len(PUMP) - 1)][1] for j in seg])
        t0 = np.array([PUMP[j][0] for j in seg])
        t1 = np.array([PUMP[min(j + 1, len(PUMP) - 1)][0] for j in seg])
        u = np.where(t1 > t0, np.clip((tau - t0) / np.maximum(t1 - t0, 1e-9), 0.0, 1.0), 0.0)
        g[k:nxt] = a + (b - a) * (0.5 - 0.5 * np.cos(np.pi * u))
    return D.undb(g)


def roll(n):
    """The grit's 16th roll, a gain per sample: each 16th of the kit's grid (swung, as every lane) lifts it over 3 ms,
    and it falls back ROLL[0] dB over ROLL[1] seconds, round the loop."""
    depth, tau = ROLL
    up = np.zeros(n)
    t = np.arange(n)
    for s in range(LOOP_STEPS):
        age = ((t - G.at(s)) % L) / SR
        up = np.maximum(up, (1.0 - np.exp(-age / 0.003)) * np.exp(-age / tau))
    return D.undb(-depth * (1.0 - up))


def lp_steep(x, fc):
    """Two 8th-order Butterworth low-passes in a row: about 96 dB per octave over fc."""
    return D.bfilt(D.bfilt(x, fc, 'lowpass', order=8), fc, 'lowpass', order=8)


def rumble(take):
    """The kick on every beat into the kit's space (FDN, dark), driven at 4x, then two bands: the sub under 100 Hz with
    resonances on E1 (the kick's note) and E2, its phase turned RUMBLE_PHASE so it adds to the drums' kick tail, and the
    grit above 600 Hz, wide, rolling on the swung 16ths; both pumped by the kick, the swell coming in as the kick's tail
    fades. Every stage runs circularly, so the loop joins."""
    n = int(0.5 * SR)
    feed = np.zeros(L)
    for p, (_, v) in zip(KICKS, qa.GROOVES[K.GROOVE]):
        np.add.at(feed, (p + np.arange(n)) % L, feed_kick(n, v))
    room = Ph.circular(lambda x: D.fdn_reverb(x, K.space_rng(), t60=2.2, size=1.3, damp_hz=1800.0), feed)
    hot = Ph.circular(lambda x: D.oversampled(lambda u: D.tanh_adaa(u, 4.0), 6.0 * x, 4), room)
    mono = D.mono(hot)
    low = Ph.circular(lambda x: lp_steep(x, RUMBLE_SUB_HZ), mono)
    tone = Ph.circular(lambda x: D.svf_tpt(x, F_KICK, q=4.0)[1] / 4.0, low)
    octave = Ph.circular(lambda x: D.svf_tpt(x, 2.0 * F_KICK, q=4.0)[1] / 4.0, low)
    grit = Ph.circular(lambda x: D.bfilt(P.hp_steep(x, RUMBLE_GRIT[0]), RUMBLE_GRIT[1], 'lowpass', order=4), hot)
    g = pump(L)
    wash, focus, grit_level, focus2 = RUMBLE_MIX
    y = (D.st(rotate(wash * low + focus * tone + focus2 * octave, RUMBLE_PHASE) * g)
         + grit_level * grit * (g * roll(L))[:, None])
    y = Ph.circular(lambda x: D.mono_below(D.bfilt(x, 25.0, 'highpass', order=2), 150.0), y)
    return Out(y, hits=KICKS, meta={'bpm': K.BPM, 'kicks': list(KICKS), 'feed': f'a sine kick swept onto '
                                    f'{D.note_name(K.KICK_NOTE)}', 'pump_db': [list(p) for p in PUMP],
                                    'sub_phase_deg': RUMBLE_PHASE})


# ------------------------------------------------------------------ the items
R23 = ('r2', 'r3')                  # round 3 changes these sounds and keeps them in both sets
WARM = {'mud': 'bass-body', 'why': 'B, warm: the proposed bass-body rule (ADR 0011); the voice from 120 Hz over a mono '
                                   'sine sub'}
HELD = (ATTACK_S + 0.05, ATTACK_S + 1.15)   # the alias check's window on a held note: the loop, after the attack
HELD_WHY = ('the alias check runs on the held loop (0.55-1.65 s): the attack sweeps the resonance across the '
            'harmonics, a moving tone that spreads them (-35 to -54 dB from 0.10 s, the same at 8x oversampling, so '
            'nothing folds back); the held loop reads -83 dB or lower')
ABOUT = ('acid bass: a saw into a resonant 4-pole ladder (resonance 0.9) whose cutoff jumps on each note and falls '
         'back, a drive on top and a distortion after the filter, over a mono sine sub')
# Each line's A and B (and the ride's) at one loudness, so the A/B is the character alone: at their own peaks A read
# 1.18 LU over B on line 1 (-14.92 against -16.09 LUFS-I), 0.96 on the ride (-16.18 against -17.14) and B 0.22 over A
# on line 2 (-18.57 against -18.79). The louder side comes down to the quieter's loudness; the quieter keeps its peak
# (the warm ride, the wet and club loops' bass, keeps its bytes).
LINE_LUFS = {'AcidLine1Clean': -16.09, 'AcidLine2Warm': -18.79}
RIDE_LUFS = {'AcidLine1RideClean': -17.14}
ITEMS = [
    Item('Bass', 'AcidClean', 'bass', acid_clean, layers=ACID_LAYERS, notes=ACID_NOTES, loop=True, sets=R23,
         qa={'sustain': HELD, 'why': f'A, clean: the strict mud gate; {HELD_WHY}'},
         about=f'A, clean: {ABOUT}; the voice from 500 Hz (the strict mud gate); held notes, the cutoff breathing '
               f'once a bar, 1-bar loops'),
    Item('Bass', 'AcidWarm', 'bass', acid_warm, layers=ACID_LAYERS, notes=ACID_NOTES, loop=True, sets=R23,
         qa={'mud': 'bass-body', 'sustain': HELD, 'why': f"{WARM['why']}; {HELD_WHY}"},
         about=f'B, warm: {ABOUT}; the voice from 120 Hz (bass-body); held notes, 1-bar loops'),
] + [
    Item('Loops', f'{ln.name}{char.title()}', 'loop', fn, loop=True, sets=R23, hp_hz=None,
         level=LINE_LUFS.get(f'{ln.name}{char.title()}'),
         qa=WARM if char == 'warm' else {'mud': 'gate', 'why': 'A, clean: a bass phrase, gated like the bass, in '
                                                               'every gap between its kicks'},
         about=f'{"B, warm" if char == "warm" else "A, clean"}: line {ln.name[-1]}'
               f'{" (the pick for the context loop)" if ln.name == PICK else ""}, 2 bars at 136 BPM in E minor on '
               f'the kit swing against four on the floor: {ln.about}')
    for ln in LINES for char, fn in (('clean', acid_line_clean), ('warm', acid_line_warm))
] + [
    Item('Loops', f'{name}Ride{char.title()}', 'loop', fn, loop=True, sets=('r3',), hp_hz=None,
         level=RIDE_LUFS.get(f'{name}Ride{char.title()}'),
         qa=WARM if char == 'warm' else {'mud': 'gate', 'why': 'A, clean: a bass phrase, gated like the bass, in '
                                                               'every gap between its kicks'},
         about=f'{"B, warm" if char == "warm" else "A, clean"}: line {name[-1]}, the pick, twice, as the context loop '
               f'plays it: 4 bars at 136 BPM, its cutoff and resonance ridden open through bar 3, widest on bar 4, '
               f'closed by the downbeat')
    for name in RIDES for char, fn in (('clean', acid_ride_clean), ('warm', acid_ride_warm))
] + [
    Item('Bass', 'Rumble', 'loop', rumble, loop=True, sets=R23,
         qa={'mud': 'gate', 'why': 'a low-end loop: gated in every gap between the kicks it plays against'},
         about='kick-fed rumble: a four-on-the-floor kick on E1 into the dark concrete space, driven, a sub tuned to '
               'E1 with an E2 growl, plus a grit band above 600 Hz rolling on the swung 16ths, pumped so it swells in '
               'as each kick fades, peaking 270-300 ms after it, and is gone before the next; 2 bars at 136 BPM'),
]
