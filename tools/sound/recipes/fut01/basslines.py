"""FUT01 basslines for listening round 2: three original 2-bar lines in F# minor that groove against the 2-step kick,
each a loop of kick A (KickSplit), hat A (ClosedHatFM) and the rebuilt reese (bass.py), in both characters: A, clean
(ReeseClean, the strict mud gate) and B, warm (ReeseWarm, the proposed bass-body rule, ADR 0011).

Round 1's context loop started a reese note on every kick, mostly on F#1, and "doesnt make me want to dance". These
lines do the opposite:
- Room on the kick. No note sounds over a kick's first 80 ms, and every note-off comes 85 ms before the next kick
  (its 30 ms release then ends 55 ms before it), so the kick's knock and sub have the low end to themselves and the
  bass answers in the gaps. A ducker on the bass bus (6 dB, held over each kick's first 80 ms) catches whatever still
  rings. And on a loud kick's tail (its first 200 ms, TAIL_S) only the kick's own F#, or a ghost, may start: a plain
  note a third away there swells against the kick's sub (A1 on F#1 beats at 8.75 Hz) and outweighs it.
- A melodic contour in the key: root, fifth, minor seventh and octave, with A, B and G# as approach notes.
- One motif per line that comes back in bar 2, varied where bar 2's kicks (16, the skip kick on 23, 26) leave other
  gaps than bar 1's (0, 10).
- Short and held notes, ghost notes (the soft layer, 9 dB under a plain note) and one or two slides (legato: the
  voice keeps playing and glides 70 ms in pitch, no new attack).
- Every lane on the kit's grid: phrase.Grid(K, 32), odd 16ths on K.SWING (0.57), so the bass's late 16ths land with
  the hat's.

The kick pattern is the context loop's bars 1-2: 1 and the and of 3 in bar 1; 1, the skip kick on the last 16th of
beat 2 and the and of 3 in bar 2. The hat is the context loop's: off-beats with ghosts.

The bass lane is a stand-in of the engine's sampler playing the multisamples (sampler.sampler_voice, with pitch): each
note plays the zone at or below it (F#1, A1, C#2), repitched up by 0 to 5 semitones through 4-point Hermite
interpolation, its loop running while the note is held; a velocity picks the nearer layer (the louder on a tie) and
scales it by (velocity / layer)^1.5. Every new note restarts its file, so each one opens the reese's low-pass on its
own attack. Then the bus ducker, then a fader that sets the bass lane's loudness to BASS_LUFS, the same for A and B, so
you compare the characters, not their levels. No lane filter: the stand-in (sampler.lane_filter) is round 1's talk
filter, whose formant band-passes and drive coloured the reese round 1 rated 2. So the line's movement is each note's
own filter opening on its velocity, plus its pitch, its ghosts and its slides. The Walk's notes are 0.11-0.38 s long
and Deep's longest root 0.7 s, so they play the files' attack (0.5 s) and at most the first 0.2 s of their loop: the
reese's drift and vowel turn (a 1.8 s cycle) are heard on the multisamples held on their own, not in these lines.

Each loop declares its kicks (Out.hits) and carries them (tag with_kick): A is gated in every gap between kicks, B gets
the bass-body gap checks. The body_duck check can't run on a file that holds its kicks, so each file's metadata also
carries the bass lane alone measured against those kicks (qa.loop_mud): its worst duck, gap and before-kick numbers.

The context loop's designer pick is PICK; lane() renders any line for another loop (the context loop's 4 bars, other
kicks).
"""
import functools

import numpy as np

from tools.sound import dsp48 as D
from tools.sound import parts as P
from tools.sound import phrase as Ph
from tools.sound import qa
from tools.sound.item import Item, Out
from tools.sound.kits import fut01 as K
from tools.sound.recipes.fut01 import bass

SR = D.SR
STEPS = bass.LOOP_STEPS             # 2 bars of 16ths
G = Ph.Grid(K, STEPS)

# ------------------------------------------------------------------ the parts: (16th, velocity)
KICK = ((0, 1.0), (10, 0.8), (16, 1.0), (23, 0.4), (26, 0.8))
HAT_BAR = ((2, 0.8), (3, 0.2), (6, 1.0), (7, 0.4), (10, 0.8), (11, 0.2), (14, 1.0), (15, 0.4))
HAT = tuple((16 * b + s, v) for b in range(2) for s, v in HAT_BAR)

# ------------------------------------------------------------------ the lines
# (16th, length in 16ths, MIDI note, velocity, slide): velocity 1.0 is an accent (the hard layer), 0.7 a plain note,
# 0.35 a ghost; slide: legato from the note before, which must run up to this one. F#1 30, G#1 32, A1 33, B1 35,
# C#2 37, E2 40, F#2 42.
F1, GS1, A1, B1, C2, E2, F2 = 30, 32, 33, 35, 37, 40, 42
ACC, NOTE, GHOST = 1.0, 0.7, 0.35
LINES = {
    # Bounce: the octave answers the kick. Bar 1: F#1 on the and of 1, its octave as a swung ghost, the fifth held
    # over beat 2, a seventh ghost whose release runs into the octave popped on 3 (a shorter ghost leaves the accent's
    # attack over an empty sub); F#1 again after the and-of-3 kick. Bar 2 brings the root-octave pair back and varies
    # it: the seventh instead of the fifth, C#2 squeezed between the skip kick and the and of 3, then A1 slides up to
    # C#2 into the downbeat.
    'A': ((2, 1.0, F1, NOTE, False), (3, 0.5, F2, GHOST, False), (5, 2.0, C2, NOTE, False),
          (7, 0.6, E2, GHOST, False), (8, 1.0, F2, ACC, False), (12, 1.25, F1, ACC, False),
          (18, 1.0, F1, NOTE, False), (19, 0.5, F2, GHOST, False), (21, 1.0, E2, NOTE, False),
          (24, 1.0, C2, ACC, False), (28, 1.0, A1, NOTE, False), (29, 1.75, C2, NOTE, True)),
    # Walk: the root pushed onto the swung 16th before beat 2 and held across it, then a climb, C#2 sliding up to E2
    # and F#2 popped on 3, the seventh hanging after the and-of-3 kick. Bar 2 starts the same and walks down instead,
    # C#2, B1, then two ghosts, A1 on the and-of-3 kick's tail (a ghost, so the kick's F#1 keeps the low end there) and
    # B1, a pickup that slides and swells up to the fifth, held into the loop's downbeat.
    'B': ((3, 2.0, F1, ACC, False), (6, 1.0, C2, NOTE, False), (7, 0.75, E2, NOTE, True),
          (8, 0.75, F2, ACC, False), (12, 1.25, E2, NOTE, False),
          (19, 2.0, F1, ACC, False), (21, 1.0, C2, NOTE, False), (24, 1.0, B1, NOTE, False),
          (27, 0.75, A1, GHOST, False), (28, 1.0, B1, GHOST, False), (29, 2.0, C2, NOTE, True)),
    # Deep: one long F#1 fills the first gap like a sub swell, a seventh ghost lifts out of it, the fifth answers late
    # after the and-of-3 kick. Bar 2 holds a shorter root, a G#1 ghost after the skip kick, then the seventh, two 16ths
    # after the and-of-3 kick (clear of its tail), and a slide up to the octave.
    'C': ((2, 6.0, F1, ACC, False), (8, 0.5, E2, GHOST, False), (13, 1.5, C2, NOTE, False),
          (18, 4.0, F1, ACC, False), (24, 1.0, GS1, GHOST, False), (28, 1.0, E2, ACC, False),
          (29, 1.5, F2, ACC, True)),
}
TAIL_S = 0.200                      # a note of another pitch than the kick's starts at least this long after a kick of
                                    # KICK_TAIL_VEL or more (ghosts excepted): before then the kick's F#1 sub is still
                                    # within about 5 dB of its peak, and a note a third away beats against it
KICK_TAIL_VEL = 0.8
NAMES = {'A': 'Bounce', 'B': 'Walk', 'C': 'Deep'}
WHY = {
    'A': 'it bounces: every gap gets a root-octave answer a 16th or two after the kick, the swung ghosts push against '
         'the straight kicks, and the A1-C#2 slide pulls into the downbeat',
    'B': 'it pushes: the root lands late, on the swung 16th before beat 2, and holds across it, so the bass leans into '
         'the beat instead of sitting on it; bar 1 climbs and bar 2 walks back down, so the two bars answer',
    'C': 'it breathes: a long root holds the first gap like a sub swell and the fifth and seventh answer late, so the '
         'kick and the hats carry the rhythm and the bass carries the weight',
}
PICK = 'B'                          # the designer's pick for the context loop
PICK_WHY = ("the Walk's notes land with the pluck phrase's 3-3-2 (16ths 3, 6 and 8, then 19, 24 and 27), so bass "
            'and hook push the same off-beats (in the context loop the bass\'s top ducks under every pluck attack, so '
            'the hook keeps its transients); it walks under the held F#m9 pad instead of repeating the root; and it '
            "leaves room for the context loop's extra kick in bar 3 (the and of 4), which Deep runs into")

# ------------------------------------------------------------------ the sampler stand-in, with pitch
ZONES = bass.REESE_NOTES
LAYERS = bass.REESE_LAYERS
GLIDE = 0.070                       # a slide's glide, seconds
ATTACK = 0.001                      # the voice's attack: the files carry their own onset (bass.SUB_ONSET, MID_ONSET)
RELEASE = 0.030                     # the voice's release after its note-off (20 ms splatters an F#2 sub into 150 Hz)
ROOM = 0.085                        # every note-off at least this far before the next kick: its release ends 55 ms
                                    # before it, clear of the before-kick window (40-15 ms)
OWN = qa.KICK_OWN_S                 # and no note over a kick's first 80 ms
DUCK = dict(depth_db=6.0, ramp=0.002, hold=OWN - 0.002, release=0.060)   # the bus ducker on each kick
BASS_LUFS = -17.0                   # the bass lane's loudness in the loop, A and B alike (before the loop's gain): its
                                    # peaks then sit about level with the kick's
FADERS = {'kick': -1.0, 'hat': -3.7}   # the context loop's
HAT_PAN = 0.2


def zone_of(note):
    """The zone a note plays: the highest at or below it (repitched up only, so A's 450 Hz wall never drops into the
    low mids)."""
    below = [z for z in ZONES if z <= note]
    if not below:
        raise ValueError(f'note {note} is under the lowest zone {ZONES[0]}')
    return below[-1]


def layer_of(vel):
    """The nearer layer (1-based), the louder on a tie, and the gain within it."""
    i = min(range(len(LAYERS)), key=lambda k: (abs(LAYERS[k] - vel), -k))
    return i + 1, (vel / LAYERS[i]) ** 1.5


def hermite(x, pos, loop):
    """x (n, 2) at fractional positions pos (samples), looping between loop = (a, b): 4-point cubic Hermite. At whole
    positions it returns the samples exactly, so an unrepitched note plays its file as written."""
    a, b = loop
    i = np.floor(pos).astype(np.int64)
    u = (pos - i)[:, None]

    def at(k):
        k = np.maximum(k, 0)
        return x[np.where(k < b, k, a + (k - a) % (b - a))]

    xm, x0, x1, x2 = at(i - 1), at(i), at(i + 1), at(i + 2)
    c1 = 0.5 * (x1 - xm)
    c2 = xm - 2.5 * x0 + 2.0 * x1 - 0.5 * x2
    c3 = 0.5 * (x2 - xm) + 1.5 * (x0 - x1)
    return x0 + u * (c1 + u * (c2 + u * c3))


def voices(line, grid):
    """The line as voices: each a run of notes joined by slides, as (onset sample, [(offset from the onset, note,
    velocity), ...], note-off offset)."""
    out = []
    for step, length, note, vel, slide in line:
        on = grid.at(step)
        if slide:
            start, notes, _ = out[-1]
            notes.append(((on - start) % grid.length, note, vel))
            out[-1] = (start, notes, (on - start) % grid.length + int(round(length * grid.s16)))
        else:
            out.append((on, [(0, note, vel)], int(round(length * grid.s16))))
    return out


def voice(files, notes, n_on):
    """One voice on the kit's files: the first note's zone and layer, its pitch and gain gliding to each slid note's
    over GLIDE, held for n_on samples, then released. files: {(zone, layer): Out}."""
    _, first, vel = notes[0]
    zone = zone_of(first)
    layer, _ = layer_of(vel)
    src = files[(zone, layer)]
    m = n_on + int(RELEASE * SR)
    t = np.arange(m)
    semis, gain = np.zeros(m), np.zeros(m)
    for k, (off, note, v) in enumerate(notes):
        s, g = note - zone, (v / LAYERS[layer - 1]) ** 1.5
        if k == 0:
            semis[:], gain[:] = s, g
            s0, g0 = s, g
            continue
        w = 0.5 - 0.5 * np.cos(np.pi * np.clip((t - off) / (GLIDE * SR), 0.0, 1.0))
        semis = np.where(t >= off, s0 + (s - s0) * w, semis)
        gain = np.where(t >= off, g0 + (g - g0) * w, gain)
        s0, g0 = s, g
    ratio = 2.0 ** (semis / 12.0)
    y = hermite(src.audio, np.concatenate([[0.0], np.cumsum(ratio[:-1])]), src.loop)
    return y * (gain * P.raised(m, ATTACK) * D.taper(m, n_on / SR, RELEASE))[:, None]


def kit_reese(char):
    """The reese's files as written, {(zone, layer): Out}, for the character ('clean' A, 'warm' B)."""
    _, got, _ = Ph.kit_files(K, 'ReeseClean' if char == 'clean' else 'ReeseWarm')
    return {(note, layer): o for (note, layer, _), o in got.items()}


def lane(line, char, kicks, grid):
    """A line's bass lane on a grid (the line repeats every STEPS 16ths), through the bus ducker on the kicks, before
    the fader. line: a key of LINES or a note list; kicks: (16th, velocity) pairs on the same grid."""
    notes = LINES[line] if isinstance(line, str) else line
    reps = grid.steps // STEPS
    files = kit_reese(char)
    buf = np.zeros((grid.length, 2))
    for r in range(reps):
        moved = tuple((s + r * STEPS, *rest) for s, *rest in notes)
        for on, run, n_on in voices(moved, grid):
            Ph.place(buf, voice(files, run, n_on), on)
    duck = D.undb(Ph.pump_db([grid.at(s) for s, _ in kicks], grid.length, **DUCK))
    return D.mono_below(buf * duck[:, None], 150.0)


# ------------------------------------------------------------------ the loops
@functools.lru_cache(maxsize=None)
def drums():
    hits = lambda name, part: Ph.hits_lane(K, name, part, G)
    return (hits('KickSplit', KICK) * D.undb(FADERS['kick']),
            Ph.pan(hits('ClosedHatFM', HAT), HAT_PAN) * D.undb(FADERS['hat']))


def bass_only_rows(y, kicks):
    """The bass lane alone against the loop's kicks (qa.loop_mud): the duck, gap and before-kick numbers the file
    can't show, since it holds its kicks."""
    rows = qa.loop_mud(y, kicks)
    return {'duck_db': round(max(r['duck_db'] for r in rows), 2),
            'gap_vs_sub_db': round(max(r['vs_sub_db'] for r in rows), 2),
            'before_kick_db': round(max(r['before_hit_db'] for r in rows), 2)}


def bassline(line, char):
    kick, hat = drums()
    raw = lane(line, char, KICK, G)
    fader = BASS_LUFS - qa.lufs(raw)
    y = raw * D.undb(fader)
    hits = tuple(G.at(s) for s, _ in KICK)
    meta = {'line': NAMES[line], 'character': 'A, clean' if char == 'clean' else 'B, warm',
            'pick_for_context_loop': line == PICK, 'why_it_grooves': WHY[line],
            'notes': [{'step': s, 'length_16ths': n, 'note': D.note_name(p), 'velocity': v, 'slide': sl}
                      for s, n, p, v, sl in LINES[line]],
            'grid': {'bpm': K.BPM, 'steps': STEPS, 'swing': K.SWING, 'kick_steps': [s for s, _ in KICK]},
            'faders_db': {**FADERS, 'bass': round(fader, 2)}, 'bass_lufs': BASS_LUFS, 'duck': DUCK,
            'bass_alone_vs_kicks': bass_only_rows(y, hits)}
    if line == PICK:
        meta['pick_why'] = PICK_WHY
    return Out(kick + hat + y, hits=hits, meta=meta)


def bassline_a_clean(take):
    return bassline('A', 'clean')


def bassline_a_warm(take):
    return bassline('A', 'warm')


def bassline_b_clean(take):
    return bassline('B', 'clean')


def bassline_b_warm(take):
    return bassline('B', 'warm')


def bassline_c_clean(take):
    return bassline('C', 'clean')


def bassline_c_warm(take):
    return bassline('C', 'warm')


RENDERS = {('A', 'clean'): bassline_a_clean, ('A', 'warm'): bassline_a_warm,
           ('B', 'clean'): bassline_b_clean, ('B', 'warm'): bassline_b_warm,
           ('C', 'clean'): bassline_c_clean, ('C', 'warm'): bassline_c_warm}

CLEAN = {'mud': 'gate', 'why': 'A, clean: the strict mud gate, in every gap between the kicks the loop holds'}
WARM = {'mud': 'bass-body', 'why': 'B, warm: the proposed bass-body rule (ADR 0011) on ReeseWarm, in every gap between '
                                   'the kicks the loop holds'}
ITEMS = [
    Item('Loops', f'Bassline{line}{char.capitalize()}', 'loop', RENDERS[(line, char)], loop=True, sets=('r2', 'r3'),
         tags=('with_kick',), qa=CLEAN if char == 'clean' else WARM,
         about=f"2-bar bassline {line}, {NAMES[line]}{' (the pick for the context loop)' if line == PICK else ''}: "
               f"{'A, clean (ReeseClean)' if char == 'clean' else 'B, warm (ReeseWarm)'} with kick A and hat A, "
               f'134 BPM, F# minor, swing {K.SWING}')
    for line in LINES for char in ('clean', 'warm')
]
