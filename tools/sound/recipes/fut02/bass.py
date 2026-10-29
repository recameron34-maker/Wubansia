"""FUT02 "Orbit Relay" bass for listening round 2: the rolling offbeat synth bass as multisamples (D1, A1, D2), two
original 2-bar basslines played on it against four-on-the-floor kicks, and a drop glide for transitions. Every one
comes twice, so you pick by ear (ADR 0011, round 2):

- A, clean: the strict mud gate, the category default. A mono sine sub under mids that start at 520 Hz behind a steep
  high-pass, so 150-400 Hz stays empty: tight and punchy, a little hollow.
- B, warm: the proposed bass-body rule (ADR 0011). The same voice with its mids kept down to 120 Hz, so its own
  harmonics give it body on small speakers, up to a plain saw's level on the note, over the same mono sine sub (no
  detuned stack: the sub never beats).

The voice is a mono synth: one saw-and-square oscillator (70/30) through a 4-pole low-pass (two TPT state-variable
filters) whose cutoff snaps from bright to dark on every note (the pluck: brighter and faster with velocity), a soft
drive at 4x with its fizz rolled off above 8 kHz, a sine sub on the same pitch, and a short chrome tine on each attack,
four octaves over the note on the kit's stretched partials (K.RATIOS), high-passed at 700 Hz in both characters, so
the kit's shimmer sits on the bass's bite and never in its low mids. Both characters draw the same oscillator phases;
besides the low end, A rests a little brighter, since its mids start at 520 Hz and carry the note on their own.

The lines leave the kick its room: four on the floor at 130 BPM, so a beat is 4 16ths (115 ms each). The bass rests on
every kick and rolls on the e, the and and the a of each beat; the last note of a beat is gone 44 ms before the next
kick (the gate's before-hit rule, ADR 0009). The sub swells in over the 25 ms before each note and, between two notes of
the same beat, dips 12 dB instead of stopping, so the 16ths articulate without the sub ever dropping out under the low
mids' onsets. Over the 2 bars the resting cutoff opens by half, so bar 2 answers bar 1 brighter. Every note sits on the
kit's one swing (phrase.Grid, K.SWING 0.52).

Provenance: new for FUT02. The loop-closing (whole cycles per loop, parts.loop_grid / close_loop) and the steep
high-pass come from the shared parts; the line notation follows FUT01's bass phrases.
"""
import numpy as np

from tools.sound import dsp48 as D
from tools.sound import parts as P
from tools.sound import phrase as Ph
from tools.sound import qa
from tools.sound.item import Item, Out
from tools.sound.kits import fut02 as K

SR = D.SR
CLEAN, WARM = 'clean', 'warm'
MID_HP = {CLEAN: 520.0, WARM: 120.0}   # where the mids start: A behind a 96 dB/oct wall, B an LR4 crossover to the sub
TINE_HP = 700.0                        # the chrome tine: above the low mids in both characters
WHY_WARM = 'B, warm: the proposed bass-body rule (ADR 0011)'
QA_WARM = {'mud': 'bass-body', 'why': WHY_WARM}

# ------------------------------------------------------------------ the voice
SAW_MIX = 0.7                          # saw against square: the square's odd harmonics give the note its hollow push
DRIVE = 2.0
TOP_LP = 8000.0                        # after the drive, a gentle 2-pole low-pass: the fizz off the top
SUB_GAIN = {CLEAN: 0.62, WARM: 0.80}   # the sine sub against the mids
REST = {CLEAN: 1.6, WARM: 1.0}         # the resting cutoff: A rests brighter, so its mids (from 520 Hz) carry the note
MID_GAIN = {CLEAN: 1.35, WARM: 1.10}   # B's low mids reach a plain saw's level on the note (body_over_saw about 0)
TINE_GAIN = 0.16
TINE_UP = 16.0                         # the tine sits four octaves over the note
SUB_ATTACK = 0.008                     # a held note's sub starts over 8 ms: a faster start splatters into 150-400 Hz
SUB_LEAD = 0.025                       # in a line, the sub swells in over the 25 ms before the note...
SUB_RELEASE = 0.020                    # ...and out over 20 ms after the key
BED_DB = -12.0                         # between two notes of one beat the sub dips this far instead of stopping
MID_ATTACK = 0.003
MID_RELEASE = 0.012
GLIDE_TAU = 0.018                      # a slide's glide, seconds (about 50 ms to land)
STEP_S = 0.003                         # an unslid pitch change, under the sub's bed: a 3 ms raised-cosine step


def cutoff(n, vel, char, accent=1.0):
    """The pluck: the low-pass snaps from bright to its resting cutoff, faster and from higher with velocity. Always
    above 450 Hz, so the filter's resonance never sits in the low mids."""
    t = P.times(n)
    peak = (1300.0 + 4200.0 * vel) * accent
    rest = (470.0 + 330.0 * vel) * REST[char]
    tau = 0.055 - 0.02 * vel
    return rest + (np.maximum(peak, rest) - rest) * np.exp(-t / tau)


def osc(f, n, ph0):
    """The saw and the square on the frequency curve f (one value per sample), PolyBLEP at 4x."""
    k = 4
    saw = D.polyblep_saw(f, n, ph0, k=k)
    sq = 0.5 * (saw - D.polyblep_saw(f, n, ph0 + 0.5, k=k))
    return D.down(SAW_MIX * saw + (1 - SAW_MIX) * sq, k)


def lowpass4(x, fc):
    """Two TPT state-variable low-passes in a row: 24 dB per octave, a gentle bump at the cutoff."""
    return D.svf_tpt(D.svf_tpt(x, fc, q=0.62)[0], fc, q=0.95)[0]


TINE_N = int(0.15 * SR)


def tine(f, vel, r):
    """The chrome tine on an attack: the kit's stretched partials (K.RATIOS) four octaves over the note, gone in about
    60 ms. 150 ms long."""
    t = P.times(TINE_N)
    y = np.zeros(TINE_N)
    for k, ratio in enumerate(K.RATIOS):
        fk = TINE_UP * f * ratio
        if fk < 18000:
            y += np.exp(-t / (0.03 / (1 + 0.3 * k))) * np.sin(2 * np.pi * fk * t + r.uniform(0, 2 * np.pi)) / (1 + k)
    return y * P.raised(TINE_N, 0.0005) * (0.4 + 0.6 * vel)


def tine_partials(f):
    return tuple(TINE_UP * f * ratio for ratio in K.RATIOS if TINE_UP * f * ratio < 18000)


def voice(m, sub_amp, mid_amp, fc, char, ph0, tines, r):
    """The mono synth on a pitch curve m (MIDI per sample), with its sub and mids gains and the low-pass cutoff per
    sample, and a tine at each (sample, Hz, velocity) of `tines`. Returns mono audio."""
    n = len(m)
    f = D.midi_hz(m)
    mids = D.oversampled(lambda u: D.tanh_adaa(u, DRIVE), lowpass4(osc(f, n, ph0), fc), 4)
    mids = D.bfilt(mids, TOP_LP, 'lowpass', order=2) * mid_amp
    mids = P.hp_steep(mids, MID_HP[CLEAN]) if char == CLEAN else D.lr4(mids, MID_HP[WARM], 'highpass')
    tn = np.zeros(n + TINE_N)
    for at, ft, v in tines:
        tn[at:at + TINE_N] += tine(ft, v, r)
    tn = P.hp_steep(tn[:n], TINE_HP)
    sub = np.sin(2 * np.pi * D.phase(f, n)) * sub_amp
    return SUB_GAIN[char] * sub + MID_GAIN[char] * mids + TINE_GAIN * tn


# ------------------------------------------------------------------ the multisamples
ROLL_NOTES = (26, 33, 38)           # D1, A1, D2: the lines' root, fifth and octave
ROLL_LAYERS = (0.6, 1.0)
LOOP_S = 2.0                        # one turn of the filter's slow drift
ATTACK_S = 0.5                      # the pluck and its settle before the loop
DRIFT = 0.16                        # the resting cutoff drifts +-16% once per loop: movement while a note is held


def roll(take, char):
    """The bass held: the pluck, then the resting tone, the cutoff drifting once per loop. The oscillators sit on a
    whole number of cycles per loop (parts.loop_grid), and the drift repeats once per loop, so the loop joins by
    itself; the crossfade only absorbs what's left of the attack."""
    L, _, f = P.loop_grid(take.f0, LOOP_S)
    A0 = int(ATTACK_S * SR)
    n = A0 + int(0.02 * SR) + L
    vel = take.vel
    fc = cutoff(n, vel, char) * (1 + DRIFT * np.sin(2 * np.pi * np.arange(n) / L))
    r = D.rng(take.kit, 'RollBass', take.note, 'voice')    # both layers and characters share the phases
    m = np.full(n, 69 + 12 * np.log2(f / 440.0))
    y = voice(m, P.raised(n, SUB_ATTACK), P.raised(n, MID_ATTACK), fc, char, r.uniform(), [(0, f, vel)], r)
    y = y * P.vel_gain(vel, 6.0, v_min=0.6)
    y, loop = P.close_loop(D.st(y), A0, L)
    return Out(y, loop=loop, f0s=(f,) + tine_partials(f))


def roll_clean(take):
    return roll(take, CLEAN)


def roll_warm(take):
    return roll(take, WARM)


# ------------------------------------------------------------------ the lines
LINE_STEPS = 32                     # 2 bars of 16ths
G = Ph.Grid(K, LINE_STEPS)          # the kit's one swing
KICK_STEPS = tuple(s for s, _ in qa.GROOVES[K.GROOVE])   # four on the floor: 0, 4, ..., 28
KICKS = tuple(G.at(s) for s in KICK_STEPS)
# (16th step, MIDI note, length in 16ths, velocity, slide from the note before). Every note rests on the kick, and the
# last of each beat is gone 44 ms before the next one: a note on the a of a beat is at most 0.4 of a 16th.
#
# "Orbit" (the pick for the context loop): the rolling octave bounce. Each beat answers the kick with D1 on the e and
# D2 on the and (the offbeat accent); the a is the one that moves (a D1 ghost, C2, D2), so the motif is one cell whose
# pickup changes. Beat 3 jumps to F2, beat 4 slides A1 up to C2. Bar 2 repeats the cell, then breaks it: beat 3 keeps
# the D1 on the e but drops the and to a B-flat (the dark sixth), and beat 4 climbs A1-C2-D2 back into the loop. The
# B-flat waits for the and, off the kick's tail: with the first render's longer kick, whose A1 sub still rang about
# 4 dB down on the e, a B-flat there rubbed a semitone against it (ContextLoopLowWarm read +3.7 dB in that gap against
# the -2 dB bass-body limit). The kick's sub is now 20 dB down by the e.
ORBIT = (
    (1, 26, 0.6, 0.80, 0), (2, 38, 0.8, 1.00, 0), (3, 26, 0.4, 0.40, 0),
    (5, 26, 0.6, 0.80, 0), (6, 38, 0.8, 1.00, 0), (7, 36, 0.4, 0.60, 0),
    (9, 26, 0.6, 0.80, 0), (10, 41, 0.8, 1.00, 0), (11, 38, 0.4, 0.60, 0),
    (13, 33, 1.0, 0.85, 0), (14, 36, 1.3, 0.95, 1),
    (17, 26, 0.6, 0.80, 0), (18, 38, 0.8, 1.00, 0), (19, 26, 0.4, 0.40, 0),
    (21, 26, 0.6, 0.80, 0), (22, 38, 0.8, 1.00, 0), (23, 36, 0.4, 0.60, 0),
    (25, 26, 0.6, 0.80, 0), (26, 34, 1.3, 0.95, 0),
    (29, 33, 0.6, 0.80, 0), (30, 36, 0.8, 0.90, 0), (31, 38, 0.4, 0.55, 0),
)
# "Relay": the melodic one, sitting on D2. The motif is beats 1-2: D2 short then held over the offbeat, a ghost, up to
# F2 and back (D2 D2 | d F2 D2); beat 3 holds C2, the minor seventh under it, and beat 4 answers A1-C2. Bar 2 brings
# the motif back a step higher (G2-F2), slides A1 up to C2, and climbs F1-G1-A1 from below into the loop's D2.
RELAY = (
    (1, 38, 0.5, 0.60, 0), (2, 38, 1.4, 1.00, 0),
    (5, 38, 0.45, 0.35, 0), (6, 41, 0.9, 0.90, 0), (7, 38, 0.4, 0.60, 0),
    (9, 36, 2.3, 0.95, 0),
    (14, 33, 0.9, 0.85, 0), (15, 36, 0.4, 0.60, 0),
    (17, 38, 0.5, 0.60, 0), (18, 38, 1.4, 1.00, 0),
    (21, 38, 0.45, 0.35, 0), (22, 43, 0.9, 0.90, 0), (23, 41, 0.35, 0.60, 0),
    (25, 33, 1.0, 0.90, 0), (26, 36, 1.2, 0.90, 1),
    (29, 29, 0.45, 0.40, 0), (30, 31, 0.9, 0.85, 0), (31, 33, 0.4, 0.70, 0),
)
LINES = {'Orbit': ORBIT, 'Relay': RELAY}
PICK = 'Orbit'                      # the line the context loop plays
VEL_SPAN_DB = 12.0                  # a ghost (0.35) sits about 10 dB under an accent
SWELL = 0.5                         # the resting cutoff opens by half over the 2 bars: bar 2 answers brighter
LINE_TAIL = int(0.15 * SR)          # rendered past the loop, then wrapped to its start


def voices(line):
    """The line as voices: a slide joins its note to the one before, so one voice plays both (one envelope, a glide).
    Each voice: dict(on, off: samples; notes: [(sample, MIDI, velocity)])."""
    out = []
    for step, note, length, vel, slide in line:
        at = G.at(step)
        end = at + int(round(length * G.s16))
        if slide and out:
            out[-1]['notes'].append((at, note, vel))
            out[-1]['off'] = end
        else:
            out.append(dict(on=at, off=end, notes=[(at, note, vel)]))
    return out


def ramp(n, a, b, up=True):
    """A raised-cosine edge over samples a..b (0 before and 1 after when up, the reverse when down)."""
    u = np.clip((np.arange(n) - a) / max(1, b - a), 0.0, 1.0)
    e = 0.5 - 0.5 * np.cos(np.pi * u)
    return e if up else 1.0 - e


def line_curves(name, char):
    """The line as the mono synth's control curves, over the loop plus its tail: pitch (MIDI), sub and mids gains,
    cutoff, and the tines (sample, Hz, velocity)."""
    vs = voices(LINES[name])
    n = G.length + LINE_TAIL
    lead, rel_sub, rel_mid = int(SUB_LEAD * SR), int(SUB_RELEASE * SR), int(MID_RELEASE * SR)
    att_mid, step_n = int(MID_ATTACK * SR), int(STEP_S * SR)
    m = np.full(n, float(vs[0]['notes'][0][1]))
    sub, mid = np.zeros(n), np.zeros(n)
    fc = np.full(n, 470.0)
    tines = []
    for i, v in enumerate(vs):
        (on, first, vel), off = v['notes'][0], v['off']
        g = P.vel_gain(vel, VEL_SPAN_DB)
        prev = m[on - lead]
        m[on - lead:] = prev + (first - prev) * ramp(n - (on - lead), 0, step_n)
        for (_, a, _), (at, b, _) in zip(v['notes'], v['notes'][1:]):
            m[at:] = b + (a - b) * np.exp(-np.arange(n - at) / (GLIDE_TAU * SR))
        sub = np.maximum(sub, g * ramp(n, on - lead, on) * ramp(n, off, off + rel_sub, up=False))
        mid = np.maximum(mid, g * ramp(n, on, on + att_mid) * ramp(n, off, off + rel_mid, up=False))
        nxt = vs[i + 1] if i + 1 < len(vs) else None
        if nxt and not any(on < k <= nxt['on'] for k in KICKS):     # the next note in the same beat: the sub's bed
            g2 = P.vel_gain(nxt['notes'][0][2], VEL_SPAN_DB)
            sub[off:nxt['on']] = np.maximum(sub[off:nxt['on']], D.undb(BED_DB) * min(g, g2))
        for k, (at, note, nv) in enumerate(v['notes']):
            seg = cutoff(n - at, nv, char, accent=1.0 if k == 0 else 0.6)
            fc[at:] = seg if k == 0 else np.maximum(fc[at:], seg)
            tines.append((at, float(D.midi_hz(note)), nv if k == 0 else 0.5 * nv))
    u = np.clip(np.arange(n) / G.length, 0.0, 1.0)
    fc = fc * (1 + SWELL * (0.5 - 0.5 * np.cos(np.pi * u)))
    return m, sub, mid, fc, tines


def line_lane(name, char):
    """A line on the bass, as the loop hears it: the mono synth played on the grid, its tail wrapped round to the
    start. Mono, one loop of the grid."""
    m, sub, mid, fc, tines = line_curves(name, char)
    r = D.rng(K.KIT, 'line', name)                         # both characters share the phases
    y = voice(m, sub, mid, fc, char, r.uniform(), tines, r)
    L = G.length
    y[:len(y) - L] += y[L:]
    return y[:L]


def line_meta(name):
    return {'line': name, 'pick': name == PICK, 'bpm': K.BPM, 'swing': K.SWING, 'steps': LINE_STEPS,
            'kick_steps': list(KICK_STEPS),
            'notes': [{'step': s, 'note': D.note_name(m), 'length_16ths': ln, 'velocity': v, 'slide': bool(sl)}
                      for s, m, ln, v, sl in LINES[name]]}


def line(take, name, char):
    """The line as a 2-bar loop, with the kicks it plays against declared (not in the file)."""
    return Out(line_lane(name, char), hits=KICKS, meta=line_meta(name))


def orbit_clean(take):
    return line(take, 'Orbit', CLEAN)


def orbit_warm(take):
    return line(take, 'Orbit', WARM)


def relay_clean(take):
    return line(take, 'Relay', CLEAN)


def relay_warm(take):
    return line(take, 'Relay', WARM)


# ------------------------------------------------------------------ the drop glide
DROP_ROOT = 26                      # D1: where it lands
DROP_FROM = 38                      # D2: where it starts, an octave up
DROP_LAYERS = (0.7, 1.0)
DROP_HOLD_BEATS = 2                 # the key held two beats; the glide takes the first
LIFT = (0.06, 0.13, 1400.0)         # A: the mids lift from 520 Hz to 1.4 kHz over 60-130 ms, so 400 Hz-1 kHz falls
                                    # inside the gate while the dive goes on above it
WARM_MIDS = (0.40, 0.24, 0.12)      # B: its mids fade slowly (time constant, s) through the dive and are gone by
                                    # 0.36 s (a raised-cosine fade from 0.24 s over 0.12 s), before the next kick
DROP_MID_GAIN = {CLEAN: 1.35, WARM: 1.0}


def drop(take, char):
    """A transition hit: D2 struck hard, diving an octave to D1 over exactly one beat (a cubic ease-out: fast, then
    settling onto D1), held for a second beat, then released. The cutoff falls with the pitch. A keeps its mids from
    520 Hz for the hit, then lifts them to 1.4 kHz, level-matched, so the dive is heard as a buzz above the low mids; B
    keeps its warm mids down to 120 Hz through most of the dive (8 dB down at 0.2 s) and fades them out by 0.36 s,
    before the next kick, leaving the sine sub to land. (The first B faded its mids with a 0.10 s time constant: 22 dB
    down at 0.2 s, so on small speakers the dive was a bare sine.)"""
    v = take.vel
    n = int((DROP_HOLD_BEATS * K.BEAT + 0.4) * SR)
    n_on = int(DROP_HOLD_BEATS * K.BEAT * SR)
    t = P.times(n)
    u = np.clip(t / K.BEAT, 0.0, 1.0)
    m = DROP_ROOT + (DROP_FROM - DROP_ROOT) * (1 - u) ** 3
    fc = (600.0 + 5200.0 * v) * np.exp(-t / 0.09) + 520.0 * 2 ** ((m - DROP_ROOT) / 12)
    r = D.rng(take.kit, 'DropGlide', 'drop')             # both layers and characters share the phases
    f = D.midi_hz(m)
    gate = P.raised(n, MID_ATTACK) * D.taper(n, n_on / SR, 0.15)
    drive = 1.6 + 1.6 * v
    mids = D.oversampled(lambda w: D.tanh_adaa(w, drive), lowpass4(osc(f, n, r.uniform()), fc), 4)
    mids = D.bfilt(mids, TOP_LP, 'lowpass', order=2) * gate
    if char == CLEAN:
        a, b, hi = LIFT
        lift = D.taper(n, a, b - a)
        lo_band, hi_band = P.hp_steep(mids, MID_HP[CLEAN]), P.hp_steep(mids, hi)
        w = slice(int(a * SR), int(a * SR) + int(0.05 * SR))
        match = np.sqrt(np.sum(lo_band[w] ** 2) / (np.sum(hi_band[w] ** 2) + 1e-30))
        mids = lift * lo_band + (1 - lift) * match * hi_band * D.taper(n, 0.55, 0.25)
    else:
        tau, a, b = WARM_MIDS
        mids = D.lr4(mids * np.exp(-t / tau) * D.taper(n, a, b), MID_HP[WARM], 'highpass')
    tn = np.zeros(n)
    tn[:TINE_N] = tine(float(f[0]), v, r)
    tn = P.hp_steep(tn, TINE_HP)
    sub = np.sin(2 * np.pi * D.phase(f, n)) * P.raised(n, SUB_ATTACK) * D.taper(n, n_on / SR, 0.15)
    y = 0.75 * sub + DROP_MID_GAIN[char] * mids + 1.6 * TINE_GAIN * tn
    return Out(y * P.vel_gain(v, 5.0, v_min=0.7), f0s=(float(D.midi_hz(DROP_ROOT)),),
               meta={'bpm': K.BPM, 'dive_beats': 1, 'hold_beats': DROP_HOLD_BEATS,
                     'from': D.note_name(DROP_FROM), 'to': D.note_name(DROP_ROOT)})


def drop_clean(take):
    return drop(take, CLEAN)


def drop_warm(take):
    return drop(take, WARM)


# ------------------------------------------------------------------ the items
TONE = ('mono synth: saw and square through a 4-pole pluck filter, a soft drive, a chrome tine on the kit ratios, a '
        'mono sine sub')
DROP_QA = {'tune_window': (0.55, 0.85), 'sustain': (0.55, 0.85),
           'why': 'a dive: tuned and alias-checked where it has landed on D1 (0.55-0.85 s)'}
LINE_ABOUT = '2-bar bassline at 130 BPM in D minor against four-on-the-floor kicks (declared, not in the file)'
LINE_GATE = {'mud': 'gate', 'why': 'a bass line: gated like the bass, in every gap between its kicks'}
ITEMS = [
    Item('Bass', 'RollBassClean', 'bass', roll_clean, layers=ROLL_LAYERS, notes=ROLL_NOTES, loop=True, sets=('r2',),
         about=f'A, clean: the rolling bass held, mids from 520 Hz; {TONE}; loops of 2 s'),
    Item('Bass', 'RollBassWarm', 'bass', roll_warm, layers=ROLL_LAYERS, notes=ROLL_NOTES, loop=True, sets=('r2',),
         qa=QA_WARM, about=f'B, warm: the rolling bass held, mids from 120 Hz; {TONE}; loops of 2 s'),
    Item('Loops', 'OrbitLineClean', 'loop', orbit_clean, loop=True, sets=('r2',), qa=LINE_GATE,
         about=f'A, clean: "Orbit", the pick, the rolling octave bounce; {LINE_ABOUT}'),
    Item('Loops', 'OrbitLineWarm', 'loop', orbit_warm, loop=True, sets=('r2',), qa=QA_WARM,
         about=f'B, warm: "Orbit", the pick, the rolling octave bounce; {LINE_ABOUT}'),
    Item('Loops', 'RelayLineClean', 'loop', relay_clean, loop=True, sets=('r2',), qa=LINE_GATE,
         about=f'A, clean: "Relay", the melodic line on D2; {LINE_ABOUT}'),
    Item('Loops', 'RelayLineWarm', 'loop', relay_warm, loop=True, sets=('r2',), qa=QA_WARM,
         about=f'B, warm: "Relay", the melodic line on D2; {LINE_ABOUT}'),
    Item('Bass', 'DropGlideClean', 'bass', drop_clean, layers=DROP_LAYERS, root=DROP_ROOT, sets=('r2',), qa=DROP_QA,
         about='A, clean: a transition hit, D2 diving to D1 over one beat, held a second; the mids lift above the '
               'low mids'),
    Item('Bass', 'DropGlideWarm', 'bass', drop_warm, layers=DROP_LAYERS, root=DROP_ROOT, sets=('r2',),
         qa={**DROP_QA, 'mud': 'bass-body', 'why': f"{WHY_WARM}; {DROP_QA['why']}"},
         about='B, warm: a transition hit, D2 diving to D1 over one beat, held a second; warm mids that clear by the '
               'next kick'),
]
