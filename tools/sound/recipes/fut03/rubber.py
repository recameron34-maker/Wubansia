"""FUT03 round 4 bass: the rubbery, rolling deep-house bass (multisamples, A clean and B warm), the deep sub under it,
and two original 2-bar lines played on it against the four-on-the-floor kick (each in both characters). The
LineRubber line is the track's hook.

**The voice ("rubber bass").** Phase-locked and harmonic, so nothing in it beats:
- the source: a saw and a square on the sub's own phase (the square two saws half a cycle apart), soft-driven at 4x;
- the "boing": a resonant low-pass (q 4.5) that snaps down from 2.4-3.6 kHz to its rest (380-520 Hz on G1, tracking the
  note) in 40-65 ms, so every accented note goes "bwoing"; a ghost gets a smaller, faster one;
- pitch glides: a line's note can slide into the next pitch inside its gate (a rubbery drop or lift), and every
  accented note scoops up 1.5 semitones into its pitch over 25 ms;
- under it, a mono sine sub on the written pitch.

Two low ends, the same sub and voice (ADR 0011):
- A, clean (the strict mud gate): the voice from 480 Hz, about 96 dB per octave; the sine sub carries the note.
- B, warm (the proposed bass-body rule): A plus the body, the voice at rest from 120 to 480 Hz (a darker low-pass, no
  resonance, no snap) on the sub's envelope, so the rubber has its warm middle on small speakers and never flares.

**The lines** (2 bars at 128 BPM, kicks on every beat), played as a producer layers a house bass: one phase-continuous
sine sub per gap between two kicks (it swells in from 15 ms before the gap's first note, glides 15 ms into each held
note, holds under the pops and is released 25 ms after the gap's last note), the voice note by note over it; every note
released at least 45 ms before the next kick, then a 6 dB duck held over each kick's first 80 ms. The bass rests on every
kick and answers in the gaps, on the kit's one swing (K.SWING 0.56).
- LineRubber (the hook, the pick): bar 1, G1 bwoing on the "and" with an octave pop on the swung "a", Bb1-C2, the fifth
  held on beat 3, F2, then G2 dropping an octave to G1 inside one note (the rubber glide). Bar 2 brings the motif back
  varied: a ghost into G1 and its pop, Bb1-A1, F1 held from the "e" sliding up to G1, and C2 lifting to D2 into the bar.
- LineDeep: a rolling, hypnotic e-and-a line on G1 that walks Bb1 C2 D2 under it, a held D2 sliding to C2, and in bar 2
  F2 sliding to D2, C2, and Bb1 sliding down to G1: the deep tech roll.

**The deep sub** (SubDeep, G1 D2 G2): the line's sine as a held multisample, with a whisper of 2nd and 3rd harmonics
for small speakers, looped over 1 s of whole cycles, under the strict gate.
"""
import numpy as np

from tools.sound import dsp48 as D
from tools.sound import parts as P
from tools.sound import phrase as Ph
from tools.sound.item import Item, Out
from tools.sound.kits import fut03 as K

SR = D.SR
SETS = ('r4',)
CLEAN, WARM = 'clean', 'warm'
MID_HP = 480.0                      # A: the voice starts here, steep
WARM_XO = 120.0                     # B: the body from here
LOOP_STEPS = 32
G = Ph.Grid(K, LOOP_STEPS)
KICKS = tuple(range(0, LOOP_STEPS, 4))
F_KICK = float(D.midi_hz(K.KICK_NOTE))

# ------------------------------------------------------------------ the voice
SETTLE = 0.30                       # every envelope reaches rest exactly here (the multisamples loop after it)
FC_PEAK = (2400.0, 3600.0)          # the boing starts here, soft to hard...
FC_REST = (380.0, 520.0)            # ...and rests here on G1 (tracking the note, FC_TRACK)
FC_TRACK = 0.5
FC_TAU = (0.040, 0.065)             # how fast it falls, soft to hard
FC_Q = 4.5                          # the resonance: the "rubber"
GHOST_VEL = 0.7                     # notes under this are ghosts: a smaller, faster boing, no scoop
SQUARE_MIX = 0.45                   # the square under the saw
DRIVE = 1.5
SCOOP = (1.5, 0.025)                # every accented note scoops up into its pitch: semitones, seconds
BODY_FC = 560.0                     # B's body: the voice at rest, darker, no resonance
BODY_Q = 0.8
BODY_TRACK = 0.8                    # B's body falls with the note, (G1 / note) ** this: on G2 its 2nd and 3rd harmonics
                                    # sit inside 150-400 Hz, where a full body reads past bass-body's +5 dB over a saw
SUB_LEVEL = 0.62
VOICE_LEVEL = 1.25
BODY_LEVEL = 0.80
LINE_BODY_LEVEL = 0.95
WARM_H2 = 0.16                      # B's sub: a quiet second harmonic
SUB_ATTACK, SUB_RELEASE = 0.012, 0.025
MID_ATTACK, BITE_LAG, RELEASE = 0.008, 0.005, 0.020


def settle(t, tau, end=SETTLE):
    """An exponential fall from 1 that reaches exactly 0 at `end` seconds."""
    floor = np.exp(-end / tau)
    return np.clip((np.exp(-t / tau) - floor) / (1.0 - floor), 0.0, 1.0)


def smoothstep(u):
    u = np.clip(u, 0.0, 1.0)
    return u * u * (3 - 2 * u)


def source(freq, n, ph):
    """A saw and a square on the sub's phase, exactly band-limited (additive, every harmonic up to 16 kHz): the square
    is the saw minus itself half a cycle on."""
    saw = D.additive_saw(freq, n, 0.0, fmax=16000.0)
    saw2 = D.additive_saw(freq, n, 0.5, fmax=16000.0)
    return 0.6 * saw + SQUARE_MIX * (saw - saw2)


def rubber(freq, n, vel, note_hz, ph, boing=True):
    """The voice before its low end: the source through the resonant low-pass (the boing, or at rest for B's body),
    soft-driven at 4x."""
    t = P.times(n)
    x = source(freq, n, ph)
    rest = P.lerp(FC_REST, vel) * (note_hz / F_KICK) ** FC_TRACK
    if boing:
        ghost = vel < GHOST_VEL
        peak = P.lerp(FC_PEAK, vel) * (0.6 if ghost else 1.0)
        tau = P.lerp(FC_TAU, vel) * (0.6 if ghost else 1.0)
        fc = rest + (peak - rest) * settle(t, tau)
        x = D.svf_tpt(x, fc, q=FC_Q)[0]
    else:
        x = D.svf_tpt(x, np.full(n, BODY_FC * (note_hz / F_KICK) ** 0.3), q=BODY_Q)[0]
    return D.oversampled(lambda u: D.tanh_adaa(u, DRIVE), x, 4)


def voice(freq, n, vel, char, note_hz):
    """One held note (the multisamples): the sub on the pitch curve, the voice from MID_HP, and in B the body."""
    ph = D.phase(freq, n)
    low = np.sin(2 * np.pi * ph) + (WARM_H2 * np.sin(4 * np.pi * ph) if char == WARM else 0.0)
    sub_env = P.raised(n, SUB_ATTACK)
    lag = int(BITE_LAG * SR)
    env = np.concatenate([np.zeros(lag), P.raised(n - lag, MID_ATTACK)])
    y = SUB_LEVEL * vel ** 0.6 * low * sub_env + VOICE_LEVEL * vel ** 1.3 * P.hp_steep(
        rubber(freq, n, vel, note_hz, ph) * env, MID_HP)
    if char == WARM:
        body = D.lr4(D.lr4(rubber(freq, n, vel, note_hz, ph, boing=False), WARM_XO, 'highpass'), MID_HP, 'lowpass')
        y = y + BODY_LEVEL * vel * (F_KICK / note_hz) ** BODY_TRACK * body * sub_env
    return y


# ------------------------------------------------------------------ the multisamples
BASS_NOTES = (31, 38, 43)           # G1, D2, G2
BASS_LAYERS = (0.6, 1.0)
ATTACK_S, LOOP_S, RELEASE_S = 0.40, 1.0, 0.08


def scoop(n, vel):
    """Semitones per sample (<= 0): an accented note starts SCOOP[0] under its pitch and glides up over SCOOP[1]."""
    if vel < GHOST_VEL:
        return np.zeros(n)
    return -SCOOP[0] * (1.0 - smoothstep(P.times(n) / SCOOP[1]))


def with_release(y, loop):
    a, _ = loop
    m = int(RELEASE_S * SR)
    return np.concatenate([y, y[a:a + m] * (0.5 + 0.5 * np.cos(np.pi * np.arange(m) / m))[:, None]])


def rubber_bass(take, char):
    """One zone and layer: the note with its scoop and boing, then 1 s of the steady note, looped."""
    L, _, f = P.loop_grid(take.f0, LOOP_S)
    A0 = int(ATTACK_S * SR)
    n = A0 + int(0.02 * SR) + L
    freq = f * 2.0 ** (scoop(n, take.vel) / 12.0)
    y, loop = P.close_loop(D.st(voice(freq, n, take.vel, char, f)), A0, L)
    return Out(with_release(y, loop), loop=loop, f0s=(f,))


def rubber_clean(take):
    return rubber_bass(take, CLEAN)


def rubber_warm(take):
    return rubber_bass(take, WARM)


SUB_HARM = (0.0, D.undb(-26.0), D.undb(-32.0))   # the deep sub's whisper of 2nd and 3rd harmonics


def sub_deep(take):
    """The deep sub: a sine on the zone's note (whole cycles over the loop), a whisper of its 2nd and 3rd harmonics,
    a 12 ms raised-cosine attack; 1 s looped after 0.4 s, then an 80 ms release."""
    L, _, f = P.loop_grid(take.f0, LOOP_S)
    A0 = int(ATTACK_S * SR)
    n = A0 + int(0.02 * SR) + L
    ph = f * P.times(n)
    y = np.sin(2 * np.pi * ph) + sum(a * np.sin(2 * np.pi * (k + 1) * ph) for k, a in enumerate(SUB_HARM) if a)
    y = y * P.raised(n, SUB_ATTACK) * take.vel ** 0.8
    y, loop = P.close_loop(D.st(y), A0, L)
    return Out(with_release(y, loop), loop=loop, f0s=(f,))


# ------------------------------------------------------------------ the lines
# (16th, length in 16ths, MIDI note, velocity, slide): slide = (target, where it starts as a share of the note's length,
# glide seconds) or None. F1 29, G1 31, A1 33, Bb1 34, C2 36, D2 38, F2 41, G2 43. A note on a swung "a" before a kick is
# at most 0.28 of a 16th, so it is released 45 ms before the kick
LINES = {
    'Rubber': (
        (2, 0.9, 31, 1.0, None), (3, 0.28, 43, 0.6, None),           # bar 1: G1 bwoing, the octave pop
        (6, 0.9, 34, 0.9, None), (7, 0.28, 36, 0.6, None),           # Bb1, C2
        (9, 0.5, 38, 0.5, None), (10, 1.3, 38, 1.0, None),           # a ghost into the fifth, held
        (13, 0.5, 41, 0.6, None), (14, 1.3, 43, 0.95, (31, 0.3, 0.07)),   # F2, then G2 dropping to G1: the rubber
        (17, 0.45, 31, 0.5, None), (18, 0.9, 31, 1.0, None), (19, 0.28, 43, 0.6, None),   # bar 2: the motif again...
        (22, 0.9, 34, 0.9, None), (23, 0.28, 33, 0.6, None),         # ...varied: Bb1 A1
        (25, 2.2, 29, 1.0, (31, 0.45, 0.06)),                        # F1 held from the e, sliding up to G1
        (30, 0.9, 36, 0.85, (38, 0.4, 0.05)),                        # C2 lifting to D2, back to the top
    ),
    'Deep': (
        (1, 0.5, 31, 0.5, None), (2, 0.8, 31, 1.0, None), (3, 0.28, 31, 0.6, None),     # bar 1: the roll on G1...
        (5, 0.5, 31, 0.5, None), (6, 0.8, 34, 0.9, None), (7, 0.28, 31, 0.6, None),     # ...walking Bb1
        (9, 0.5, 31, 0.5, None), (10, 0.8, 36, 0.95, None), (11, 0.28, 31, 0.6, None),  # C2
        (13, 0.5, 31, 0.5, None), (14, 1.3, 38, 1.0, (36, 0.5, 0.05)),                  # D2 held, sliding to C2
        (17, 0.5, 31, 0.5, None), (18, 0.8, 31, 1.0, None), (19, 0.28, 31, 0.6, None),  # bar 2: the roll again
        (21, 0.5, 31, 0.5, None), (22, 0.9, 41, 0.9, (38, 0.5, 0.04)),                  # F2 sliding to D2
        (25, 0.5, 31, 0.5, None), (26, 0.8, 36, 0.95, None), (27, 0.28, 31, 0.6, None),  # C2
        (29, 0.5, 31, 0.5, None), (30, 1.3, 34, 1.0, (31, 0.5, 0.05)),                  # Bb1 sliding down to G1
    ),
}
# a phrase on the same machinery (recipes/fut03/accents.py plays it): not one of the kit's basslines
PHRASES = {
    'Riff': (
        (2, 0.6, 31, 1.0, None), (3, 0.28, 43, 0.6, None),           # G1 and its pop
        (5, 0.45, 31, 0.5, None), (6, 0.9, 41, 0.9, (38, 0.5, 0.04)),     # a ghost, F2 sliding down to D2
        (10, 0.9, 36, 1.0, None), (11, 0.28, 34, 0.6, None),         # C2, Bb1
        (13, 0.45, 31, 0.5, None), (14, 1.3, 34, 0.95, (31, 0.4, 0.06)),  # Bb1 sliding down to G1
        (18, 0.6, 31, 1.0, None), (19, 0.28, 43, 0.6, None),         # bar 2: G1 and its pop
        (21, 0.45, 31, 0.5, None), (22, 1.2, 38, 0.9, None),          # a ghost, D2
        (26, 0.9, 36, 1.0, (38, 0.5, 0.04)),                         # C2 lifting to D2
        (29, 0.45, 38, 0.55, None), (30, 1.3, 36, 0.9, (31, 0.4, 0.06)),  # D2, C2 sliding down to G1
    ),
}
NOTES = {**LINES, **PHRASES}
PICK = 'Rubber'
LINE_ABOUT = {
    'Rubber': 'the hook: G1 bwoing with an octave pop, Bb1 C2, the fifth held, F2, G2 dropping to G1 inside one note; '
              'bar 2 the motif varied (Bb1 A1, F1 sliding up to G1, C2 lifting to D2)',
    'Deep': 'a rolling e-and-a line on G1 walking Bb1 C2 D2, a held D2 sliding to C2; bar 2 F2 sliding to D2, C2, and '
            'Bb1 sliding down to G1',
}
DUCK = dict(depth_db=6.0, ramp=0.002, hold=0.080, release=0.040)
CLEAR_S = 0.045
POP_LEN = 0.5
LINE_SUB_ATTACK, SUB_LEAD, SUB_GLIDE = 0.030, 0.015, 0.015
BODY_LAG = 0.010


def kick_onsets():
    return tuple(G.at(s) for s in KICKS)


def slide_curve(n, gate_s, note, slide):
    if slide is None:
        return np.zeros(n)
    target, start, glide = slide
    return (target - note) * smoothstep((P.times(n) - start * gate_s) / glide)


def is_pop(note):
    return note[1] < POP_LEN


def gap_notes(name):
    out = []
    for k in KICKS:
        notes = [n for n in NOTES[name] if max(s for s in KICKS if s <= n[0]) == k]
        if notes:
            out.append((k, notes))
    return out


def sub_gaps(name):
    """The sub of each gap: (start sample, MIDI pitch per sample, level per sample, envelope)."""
    out = []
    for _, notes in gap_notes(name):
        held = [n for n in notes if not is_pop(n)] or notes[:1]
        a = G.at(notes[0][0]) - int(SUB_LEAD * SR)
        end_s = max((G.at(s) - a) / SR + ln * G.s16 / SR for s, ln, *_ in notes)
        n = int((end_s + SUB_RELEASE + 0.01) * SR)
        t = P.times(n)
        cur, lv = float(held[0][2]), SUB_LEVEL * held[0][3] ** 0.6
        pitch, level = np.full(n, cur), np.full(n, lv)
        for j, (s, ln, note, vel, slide) in enumerate(held):
            tj = (G.at(s) - a) / SR
            if j:
                pitch += (note - cur) * smoothstep((t - tj) / SUB_GLIDE)
                level += (SUB_LEVEL * vel ** 0.6 - lv) * smoothstep((t - tj) / SUB_GLIDE)
                cur, lv = float(note), SUB_LEVEL * vel ** 0.6
            if slide:
                target, start, glide = slide
                pitch += (target - cur) * smoothstep((t - tj - start * ln * G.s16 / SR) / glide)
                cur = float(target)
        out.append((a, pitch, level, P.raised(n, LINE_SUB_ATTACK) * D.taper(n, end_s, SUB_RELEASE)))
    return out


def sub_lane(name, char):
    y, where = np.zeros((G.length, 2)), np.zeros((G.length, 2))
    for a, pitch, level, env in sub_gaps(name):
        ph = D.phase(D.midi_hz(pitch), len(pitch))
        low = np.sin(2 * np.pi * ph) + (WARM_H2 * np.sin(4 * np.pi * ph) if char == WARM else 0.0)
        Ph.place(y, level * low * env, a)
        Ph.place(where, env, a)
    return y, where


def voice_lane(name, char):
    """The voice of every note (and in B its body): its pitch with the scoop and any slide, gated and released."""
    bite, body = np.zeros((G.length, 2)), np.zeros((G.length, 2))
    lag, blag = int(BITE_LAG * SR), int(BODY_LAG * SR)
    for step, length, note, vel, slide in NOTES[name]:
        gate = length * G.s16 / SR
        n = int((gate + SUB_RELEASE + 0.01) * SR)
        f = float(D.midi_hz(note))
        freq = f * 2.0 ** ((scoop(n, vel) + slide_curve(n, gate, note, slide)) / 12.0)
        ph = D.phase(freq, n)
        env = np.concatenate([np.zeros(lag), P.raised(n - lag, MID_ATTACK)])
        x = rubber(freq, n, vel, f, ph) * env
        Ph.place(bite, VOICE_LEVEL * vel ** 1.3 * P.hp_steep(x, MID_HP) * D.taper(n, gate, RELEASE), G.at(step))
        if char == WARM:
            env = np.concatenate([np.zeros(blag), P.raised(n - blag, SUB_ATTACK)]) * D.taper(n, gate, SUB_RELEASE)
            b = D.lr4(D.lr4(rubber(freq, n, vel, f, ph, boing=False), WARM_XO, 'highpass'), MID_HP, 'lowpass')
            Ph.place(body, LINE_BODY_LEVEL * vel * (F_KICK / f) ** BODY_TRACK * b * env, G.at(step))
    return bite, body


def line_lane(name, char):
    sub, where = sub_lane(name, char)
    bite, body = voice_lane(name, char)
    y = sub + bite + body * where
    return y * D.undb(Ph.pump_db(kick_onsets(), G.length, **DUCK))[:, None]


def line_out(name, char):
    meta = {'bpm': K.BPM, 'swing': K.SWING, 'steps': LOOP_STEPS, 'kick_steps': list(KICKS), 'character': char,
            'pick': name == PICK, 'duck': DUCK,
            'voice': {'boing_hz': [list(FC_PEAK), list(FC_REST)], 'q': FC_Q, 'scoop_st': SCOOP[0]},
            'notes': [{'step': s, 'len16': ln, 'note': D.note_name(m), 'vel': v,
                       'slide_to': D.note_name(sl[0]) if sl else None} for s, ln, m, v, sl in NOTES[name]]}
    return Out(line_lane(name, char), hits=kick_onsets(), meta=meta)


def line_rubber_clean(take):
    return line_out('Rubber', CLEAN)


def line_rubber_warm(take):
    return line_out('Rubber', WARM)


def line_deep_clean(take):
    return line_out('Deep', CLEAN)


def line_deep_warm(take):
    return line_out('Deep', WARM)

SUSTAIN = (0.35, 1.35)              # the multisamples' steady note: the boing's resonant sweep settles by 0.30 s
SUSTAIN_WHY = ("the alias check reads the steady note (0.35-1.35 s, the loop): before 0.30 s the boing's resonant "
               "low-pass is still sweeping, and its ringing at a moving cutoff is not a harmonic of the note")
CLEAN_QA = {'sustain': SUSTAIN, 'why': SUSTAIN_WHY}
LINE_LEVEL = {('Deep', WARM): -15.70}   # the Deep line's A and B at one loudness: at their own peaks B read 1.24 LU over
                                        # A (-14.46 against -15.70 LUFS-I), so B comes down to A. The Rubber line keeps
                                        # its peaks (B 0.47 LU over A): the loops play its B as written and match A to it
WARM_QA = {'mud': 'bass-body', 'sustain': SUSTAIN,
           'why': 'B, warm: the proposed bass-body rule (ADR 0011); the resting voice from 120 Hz over a mono sine sub; '
                  + SUSTAIN_WHY}
CLEAN_LINE_QA = {'mud': 'gate', 'why': 'A, clean: a bass phrase under the strict gate, like the bass'}
WARM_LINE_QA = {'mud': 'bass-body', 'why': 'B, warm: a bass phrase under the proposed bass-body rule (ADR 0011), '
                                           'resting on every kick'}
VOICE_ABOUT = ('rubbery deep-house bass: a phase-locked saw and square through a resonant low-pass that boings down '
               'from 2.4-3.6 kHz on every note, a scoop into the pitch, a mono sine sub; G1 D2 G2, 1 s loops')
ITEMS = [
    Item('Bass', 'RubberBassClean', 'bass', rubber_clean, layers=BASS_LAYERS, notes=BASS_NOTES, loop=True, sets=SETS,
         qa=CLEAN_QA, about=f'A, clean: {VOICE_ABOUT}; the voice from 480 Hz (the strict mud gate)'),
    Item('Bass', 'RubberBassWarm', 'bass', rubber_warm, layers=BASS_LAYERS, notes=BASS_NOTES, loop=True, sets=SETS,
         qa=WARM_QA, about=f'B, warm: {VOICE_ABOUT}; plus its body, the resting voice from 120 Hz (bass-body)'),
    Item('Bass', 'SubDeep', 'bass', sub_deep, layers=(1.0,), notes=BASS_NOTES, loop=True, sets=SETS,
         about='deep sub: a sine with a whisper of its 2nd and 3rd harmonics, G1 D2 G2, 1 s loops (the strict gate)'),
] + [
    Item('Loops', f'Line{name}{char.title()}', 'loop', fn, loop=True, sets=SETS,
         qa=CLEAN_LINE_QA if char == CLEAN else WARM_LINE_QA, level=LINE_LEVEL.get((name, char)),
         about=f'bassline {name}{" (the hook, the pick for the context loop)" if name == PICK else ""}, '
               f'{"A, clean" if char == CLEAN else "B, warm"}: 2 bars at 128 BPM against four-on-the-floor kicks '
               f'(declared), on the kit swing; {LINE_ABOUT[name]}')
    for name, char, fn in (('Rubber', CLEAN, line_rubber_clean), ('Rubber', WARM, line_rubber_warm),
                           ('Deep', CLEAN, line_deep_clean), ('Deep', WARM, line_deep_warm))
]
