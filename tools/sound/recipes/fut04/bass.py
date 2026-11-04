"""FUT04 bass for listening round 2: the acid bass as A, clean and B, warm (multisamples and two written 2-bar acid
lines in both characters), and the kick-fed rumble.

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
that kick's tail (RUMBLE_PHASE: it had sat against it and taken weight away).

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
                                           # 245 Hz is E1's 6th, G1's 5th and B1's 4th, stay under a saw's roll-off)
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


def saw(freq, k=4):
    """An exactly band-limited saw at k x on `freq` (Hz per sample, at the output rate), k x as many samples: every note
    of the track has its harmonics up to SAW_TOP, the highest under k x's Nyquist. A PolyBLEP saw's faint aliases
    landed next to the ladder's resonance on E1 and E2, which lifted them: -58 dB between the harmonics against -92."""
    f = np.asarray(freq, dtype=float)
    top = min(SAW_TOP * f.max() / f.min(), 0.45 * SR * k)
    return D.additive_saw(np.repeat(f, k) / k, len(f) * k, 0.0, fmax=top / k)


def acid(freq, fc, drive=DRIVE, k=4):
    """The acid voice, mono: the saw on `freq` (Hz per sample), the ladder on `fc` (Hz per sample, at the output rate)
    and the grit, all at k x."""
    y = D.ladder_tanh(saw(freq, k), np.repeat(fc, k), RES, SR * k, drive=drive)
    return D.down(D.tanh_adaa(2.5 * y, GRIT), k)


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
    it, it rides the sub's envelope (gate_b) at the character's level (LOW_LEVEL), above it, its own (gate_v) at the
    squelch's level, the same in both; then the high-pass where the character starts it. Everything runs circularly, so
    a phrase's loop joins. A gates before its high-pass, which also clears the gates' splatter out of 150-400 Hz; B
    high-passes first, so the 120 Hz filter's ringing never outlasts a note's sub."""
    circ = lambda fn, x: Ph.circular(fn, x)  # noqa: E731
    hp = lambda x: circ(lambda u: P.hp_steep(u, MID_HP[char]), x)  # noqa: E731
    split = lambda x: (LOW_LEVEL[char] * circ(lambda u: D.lr4(u, SPLIT, 'lowpass'), x) * gate_b +  # noqa: E731
                       SQUELCH_LEVEL * circ(lambda u: D.lr4(u, SPLIT, 'highpass'), x) * gate_v)
    mids = hp(split(voice)) if char == 'clean' else split(hp(voice))
    return SUB_LEVEL * vel ** 0.6 * sub + vel ** 1.5 * mids


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


@functools.lru_cache(maxsize=None)
def line_parts(name):
    """The line played on the voice (Parts). Under a pedal (SUB_PEDAL_S) the sub holds the kick's note from its gate's
    onset to the end of its release; every sub on the kick's note (a pedal, or an E's) runs from PEDAL_PHASE at the
    last kick, so it adds to the kick's own tail."""
    line = LINE_BY_NAME[name]
    sp = spans(line)
    n = L + TAIL
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
                low.append([on, end, pedal(on)])
        octave[on:] = 0.5 if note >= SUB_OCTAVE_FROM else 1.0
        level[on:] = vel * D.undb(ACCENT_DB if kind(vel) == 'accent' else 0.0)
    level = np.convolve(level, np.ones(240) / 240, mode='same')     # 5 ms: no step in the level
    fc = np.minimum(knob(line, n) * 2.0 ** (amount * env), 12000.0)
    sub_freq = freq * octave
    cycles = D.phase(sub_freq, n)
    for on, end, ped in low:
        span = slice(on, end + int(np.ceil(NOTE_END * SR)))
        if ped:
            sub_freq[span] = F_KICK
        if np.all(np.abs(sub_freq[span] - F_KICK) < 1e-9):   # every sub on the kick's note, the pedal's or an E's
            cycles[span] = F_KICK * (t[span] - last_kick(on)) / SR + PEDAL_PHASE / 360.0
    return Parts(freq, sub_freq, cycles, tuple(map(tuple, groups)), tuple(map(tuple, low)), level,
                 acid(freq, fc)[:L])


def fold(x):
    """A part of L + TAIL samples as one loop: its tail added to the start."""
    return np.concatenate([x[:TAIL] + x[L:], x[TAIL:L]])


def gates(groups, level, env, weight=None):
    """Every gate's envelope (VOICE_ENV or SUB_ENV) times the level, L + TAIL samples; weight(seconds) scales a gate by
    its length. groups: (onset, end, ...) each."""
    g = np.zeros(L + TAIL)
    for on, end, *_ in groups:
        e = envelope(end - on, env) * (1.0 if weight is None else weight((end - on) / SR))
        g[on:on + len(e)] += e
    return g * level


def body_weight(seconds):
    """B's body on a note of this length: a blip (the 33 ms ones before a kick) keeps its snap and 20% of its body, a
    note from 90 ms on all of it. A short note's sine is a few cycles that the gate's 30-120 Hz band spreads thin,
    while its low mids stay put, so a blip with full body reads as low mids without a sub (+2 to +4 dB)."""
    return float(np.clip((seconds - 0.030) / 0.060, 0.2, 1.0))


def seam_silence(hold=0.020, ramp=0.010):
    """1 everywhere but round the loop's seam, the first kick, where the bass rests: down to 0 over `ramp`, `hold`
    before the seam, and back up `hold` after it. It only touches the sub-sonic filter's tails (about -100 dB there),
    and turns them into true silence, so the 24-bit seam isn't a lone LSB step."""
    t = np.minimum(np.arange(L), L - np.arange(L)) / SR
    u = np.clip((t - hold) / ramp, 0.0, 1.0)
    return 0.5 - 0.5 * np.cos(np.pi * u)


SUB_RULE = (f'on {D.note_name(K.KICK_NOTE)}, the kick\'s note, for a note starting within {SUB_PEDAL_S:g} s of a '
            f'kick (a pedal, from {PEDAL_PHASE:g} degrees at each kick, in phase with the top kick layer\'s tail); '
            f'after that on the note, an octave down from {D.note_name(SUB_OCTAVE_FROM)} up')


def acid_line(take, char):
    """One line in one character: the sub and the voice at the character's balance, with the kicks it plays against;
    the sub and the voice under SPLIT run under the duck, the squelch above it (all a tick plays) doesn't. Both
    characters share the sub (SUB_RULE) and the squelch; they differ under SPLIT."""
    name = take.item.name.replace('Clean', '').replace('Warm', '')
    p = line_parts(name)
    duck = D.undb(Ph.pump_db(KICKS, L, *DUCK))
    gate_s = gates(p.low, p.level, SUB_ENV[char])
    sub = fold(sub_wave(p.sub_cycles, char, p.sub_freq) * gate_s) * duck
    gate_b = fold(gates(p.low, p.level, BODY_ENV[char], body_weight if char == 'warm' else None)) * duck
    y = character(sub, p.voice, fold(gates(p.groups, p.level, VOICE_ENV)), gate_b, char)
    y = Ph.circular(lambda u: D.bfilt(u, SUBSONIC, 'highpass', order=2, zero_phase=True), y) * seam_silence()
    line = LINE_BY_NAME[name]
    meta = {'bpm': K.BPM, 'swing': K.SWING, 'steps': LOOP_STEPS, 'pick': name == PICK,
            'notes': [{'step': s, 'note': D.note_name(nt), 'length16': ln, 'vel': v, 'slide': sl}
                      for s, nt, ln, v, sl in line.notes],
            'ticks': [{'step': s, 'note': D.note_name(nt), 'length16': ln, 'vel': v} for s, nt, ln, v in line.ticks],
            'gates_s': [[round(a / SR, 4), round(b / SR, 4)] for a, b, *_ in spans(line)],
            'sub': SUB_RULE, 'pedal_s': SUB_PEDAL_S, 'pedal_phase_deg': PEDAL_PHASE,
            'kicks': list(KICKS), 'duck': dict(zip(('db', 'ramp_s', 'hold_s', 'release_s'), DUCK))}
    return Out(y, hits=KICKS, meta=meta)


def acid_line_clean(take):
    return acid_line(take, 'clean')


def acid_line_warm(take):
    return acid_line(take, 'warm')


# ------------------------------------------------------------------ the rumble
RUMBLE_SUB_HZ = 75.0                # the sub band's top: 150-400 Hz reads it about 48 dB down
RUMBLE_GRIT = (600.0, 2400.0)       # the grit band: above the low mids and their skirt
# the pump: (seconds after the kick, dB), joined by raised cosines: held down under the kick, swelling into the gap,
# gone 40 dB before the next kick (441 ms at 136 BPM)
PUMP = ((0.0, -40.0), (0.045, -40.0), (0.150, 0.0), (0.240, 0.0), (0.375, -40.0))
RUMBLE_MIX = (1.0, 0.6, 2.0)        # the low wash, the E1 resonance, the grit (about 13 dB under the sub)
ROLL = (8.0, 0.040)                 # the grit rolls on the swung 16ths: up to 8 dB, each 16th falling over 40 ms
RUMBLE_PHASE = 135.0                # degrees: the sub's phase turned by this (every frequency, round the loop), so its
                                    # E1 swells in phase with the drums' kick tail. Fed by its own sine kick, it sat
                                    # against it: under the kick at the context loop's faders, 30-120 Hz per beat read
                                    # 0.0 and -0.8 dB against the kick alone (it took weight away); turned 180 degrees,
                                    # +1.6 and +2.1; 135, the best of a 15-degree sweep, +2.3 and +2.3


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
    """The kick on every beat into the kit's space (FDN, dark), driven at 4x, then two bands: the sub under 75 Hz with a
    resonance on E1 (the kick's note), its phase turned RUMBLE_PHASE so it adds to the drums' kick tail, and the grit
    above 600 Hz, wide, rolling on the swung 16ths; both pumped by the kick. Every stage runs circularly, so the loop
    joins."""
    n = int(0.5 * SR)
    feed = np.zeros(L)
    for p, (_, v) in zip(KICKS, qa.GROOVES[K.GROOVE]):
        np.add.at(feed, (p + np.arange(n)) % L, feed_kick(n, v))
    room = Ph.circular(lambda x: D.fdn_reverb(x, K.space_rng(), t60=2.2, size=1.3, damp_hz=1800.0), feed)
    hot = Ph.circular(lambda x: D.oversampled(lambda u: D.tanh_adaa(u, 4.0), 6.0 * x, 4), room)
    mono = D.mono(hot)
    low = Ph.circular(lambda x: lp_steep(x, RUMBLE_SUB_HZ), mono)
    tone = Ph.circular(lambda x: D.svf_tpt(x, F_KICK, q=4.0)[1] / 4.0, low)
    grit = Ph.circular(lambda x: D.bfilt(P.hp_steep(x, RUMBLE_GRIT[0]), RUMBLE_GRIT[1], 'lowpass', order=4), hot)
    g = pump(L)
    wash, focus, edge = RUMBLE_MIX
    y = D.st(rotate(wash * low + focus * tone, RUMBLE_PHASE) * g) + edge * grit * (g * roll(L))[:, None]
    y = D.mono_below(Ph.circular(lambda x: D.bfilt(x, 25.0, 'highpass', order=2), y), 150.0)
    return Out(y, hits=KICKS, meta={'bpm': K.BPM, 'kicks': list(KICKS), 'feed': f'a sine kick swept onto '
                                    f'{D.note_name(K.KICK_NOTE)}', 'pump_db': [list(p) for p in PUMP],
                                    'sub_phase_deg': RUMBLE_PHASE})


# ------------------------------------------------------------------ the items
WARM = {'mud': 'bass-body', 'why': 'B, warm: the proposed bass-body rule (ADR 0011); the voice from 120 Hz over a mono '
                                   'sine sub'}
HELD = (ATTACK_S + 0.05, ATTACK_S + 1.15)   # the alias check's window on a held note: the loop, after the attack
HELD_WHY = ('the alias check runs on the held loop (0.55-1.65 s): the attack sweeps the resonance across the '
            'harmonics, a moving tone that spreads them (-35 to -54 dB from 0.10 s, the same at 8x oversampling, so '
            'nothing folds back); the held loop reads -83 dB or lower')
ABOUT = ('acid bass: a saw into a resonant 4-pole ladder (resonance 0.9) whose cutoff jumps on each note and falls '
         'back, a drive on top, over a mono sine sub')
ITEMS = [
    Item('Bass', 'AcidClean', 'bass', acid_clean, layers=ACID_LAYERS, notes=ACID_NOTES, loop=True, sets=('r2',),
         qa={'sustain': HELD, 'why': f'A, clean: the strict mud gate; {HELD_WHY}'},
         about=f'A, clean: {ABOUT}; the voice from 500 Hz (the strict mud gate); held notes, the cutoff breathing '
               f'once a bar, 1-bar loops'),
    Item('Bass', 'AcidWarm', 'bass', acid_warm, layers=ACID_LAYERS, notes=ACID_NOTES, loop=True, sets=('r2',),
         qa={'mud': 'bass-body', 'sustain': HELD, 'why': f"{WARM['why']}; {HELD_WHY}"},
         about=f'B, warm: {ABOUT}; the voice from 120 Hz (bass-body); held notes, 1-bar loops'),
] + [
    Item('Loops', f'{ln.name}{char.title()}', 'loop', fn, loop=True, sets=('r2',), hp_hz=None,
         qa=WARM if char == 'warm' else {'mud': 'gate', 'why': 'A, clean: a bass phrase, gated like the bass, in '
                                                               'every gap between its kicks'},
         about=f'{"B, warm" if char == "warm" else "A, clean"}: line {ln.name[-1]}'
               f'{" (the pick for the context loop)" if ln.name == PICK else ""}, 2 bars at 136 BPM in E minor on '
               f'the kit swing against four on the floor: {ln.about}')
    for ln in LINES for char, fn in (('clean', acid_line_clean), ('warm', acid_line_warm))
] + [
    Item('Bass', 'Rumble', 'loop', rumble, loop=True, sets=('r2',),
         qa={'mud': 'gate', 'why': 'a low-end loop: gated in every gap between the kicks it plays against'},
         about='kick-fed rumble: a four-on-the-floor kick on E1 into the dark concrete space, driven, a sub tuned to '
               'E1 plus a grit band above 600 Hz rolling on the swung 16ths, pumped so it swells after each kick and '
               'is gone before the next; '
               '2 bars at 136 BPM'),
]
