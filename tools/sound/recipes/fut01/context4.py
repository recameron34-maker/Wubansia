"""FUT01's round 4 context loops (set r4): 4 bars of modern UK garage at 134 BPM in F# minor, played on the kit's round
4 files exactly as written, mixed and mastered against the standard's UK garage targets (docs/plans/S1-modern-sound.md).

The groove:
- Drums: the 2-step kick (KickGarage: 1 and the and of 3, a skip kick on the last 16th of beat 2 in bars 2 and 4), the
  garage clap on 2 and 4 with two ghosts, closed hats on every swung 16th (the shuffle), open hats on the off-beat 8ths
  choked by the next closed hat, the tambourine on every 16th (loudest on each beat's last, swung, 16th), rim ghosts,
  and one alloy hit in bar 3 (the hint of the kit's colour). The drums stop for the glitch fill's roll on beat 4 of
  bar 4, as in round 3.
- Bass: the warp line (WARP_LINE), 2 bars over F#m9 then Bm9, bouncing in the gaps of the 2-step kick with octave pops,
  two slides and two pitch dips (fast glides down and back, as the engine's pitch bend will play them), on the sampler
  stand-in; a lane filter that wobbles once an 8th over its top (above 450 Hz), deeper in bars 2 and 4; ducked 6 dB
  under every kick and 3 dB under every clap. Bass A (WarpClean) is walled again at 450 Hz after the dips, so it keeps
  the strict gate; bass B (WarpWarm) keeps its body.
- Chords: the organ stab on the off-beats, F#m9 then Bm9.
- The chrome pluck phrase (round 1's Cool 5), the melodic top.
- Vocals: the off-beat riff of re-pitched hooks (hooks.ChopRiff) in bars 1-2, the call (the whole line a fourth up) in
  bars 3-4, "floor" thrown into the fill.

Files:
- GarageContext: dry, bass A (clean); GarageContextWarm: dry, bass B (warm); both bass lanes at one loudness.
- GarageContextWet: bass B with the sends: the garage plate, the echo, and a little resonator on the alloy hit.
- GarageContextClub: the wet loop through the shared club master (master.py) at the genre's settings: -9 LUFS.
- GarageContextLow, GarageContextLowWarm: the kick and bass lanes alone, gated in every gap.
- SpeedGarageLoop: the 4x4 speed garage variant, 2 bars: the kick on every beat, the warp line between the kicks on
  the off-beats (bass B), the same tops and stabs; SpeedGarageLow: its kick and bass A, gated in every gap.
- Previews: DrumGroove4Garage and DrumGroove4Split (the kick A/B in the loop's drums), DrumGroove4Swing57 (the old
  swing, against the kit's new one), WarpLineClean, WarpLineWarm and OrganLine (the bass on its own with the kick).

Everything loops: hits and tails wrap; the filters and sends run round the loop.
"""
import functools

import numpy as np

from tools.sound import dsp48 as D
from tools.sound import master as MS
from tools.sound import parts as P
from tools.sound import phrase as Ph
from tools.sound import qa
from tools.sound import sends as S
from tools.sound.item import Item, Out
from tools.sound.kits import fut01 as K
from tools.sound.recipes.fut01 import basslines as B
from tools.sound.recipes.fut01 import fx, hooks as H

SR = D.SR
STEPS = 64
L = 2 * Ph.loop_len(K, 32)          # 4 bars: twice the 2-bar phrases, so the line, the riff and the pluck tile
G = Ph.Grid(K, STEPS, L)
SEAM_S = 0.02
FILL_AT, FILL_FROM, FILL_FADE_S = 60, 4, 0.002


def bars(bar, n=4, stop=FILL_AT, length=16):
    """A one-bar part (16th, velocity) repeated over n bars, stopping at `stop`."""
    return tuple((length * b + s, v) for b in range(n) for s, v in bar if length * b + s < stop)


# ------------------------------------------------------------------ the parts: (16th, velocity)
KICK = ((0, 1.0), (10, 0.9), (16, 1.0), (23, 0.55), (26, 0.9), (32, 1.0), (42, 0.9), (48, 1.0), (55, 0.55),
        (58, 0.9))
CLAP = ((4, 1.0), (12, 1.0), (20, 1.0), (28, 1.0), (31, 0.3), (36, 1.0), (44, 1.0), (47, 0.25), (52, 1.0))
HAT_C_BAR = ((1, 0.35), (3, 0.75), (5, 0.4), (7, 0.8), (9, 0.35), (11, 0.75), (13, 0.45), (15, 0.85))
HAT_O_BAR = ((2, 0.75), (6, 0.85), (10, 0.75), (14, 1.0))
TAMB_BEAT = (0.4, 0.3, 0.6, 0.9)    # on the beat, the "e", the "and", the "a" (swung late, pushing): loudest
TAMB_BAR = tuple((s, TAMB_BEAT[s % 4]) for s in range(16))
RIM = ((3, 0.35), (13, 0.5), (19, 0.35), (25, 0.45), (29, 0.6), (35, 0.35), (45, 0.5), (51, 0.35), (57, 0.45))
PERC = ((38, 0.75),)
HAT_C, HAT_O, TAMB = bars(HAT_C_BAR), bars(HAT_O_BAR), bars(TAMB_BAR)
# the organ stabs, 2 bars: (16th, zone, velocity): F#m9 on the off-beats, then Bm9
STABS_2 = ((2, 54, 0.9), (6, 54, 1.0), (9, 54, 0.6), (14, 54, 0.8), (18, 59, 0.9), (22, 59, 1.0), (25, 59, 0.6),
           (30, 59, 0.8))
STABS = tuple((s + 32 * r, z, v) for r in range(2) for s, z, v in STABS_2 if s + 32 * r < FILL_AT)

# ------------------------------------------------------------------ the warp line: (16th, length, note, vel, slide)
# slide: False (a new note), 'slide' (legato, a 70 ms glide) or 'dip' (legato, a 30 ms glide: a pitch-bend dip).
F1, GS1, A1, B1, C2, D2, E2, F2 = 30, 32, 33, 35, 37, 38, 40, 42
ACC, NOTE, GHOST = 1.0, 0.7, 0.35
GLIDES = {'slide': 0.070, 'dip': 0.030}
WARP_LINE = (
    # bar 1, F#m9 (kicks on 0 and 10): F#1 on the and of 1, its octave popped, a dip from F#2 to C#2 and back (at
    # one velocity: a dip bends the note, it doesn't accent it), then
    # F#1 again after the and-of-3 kick, the octave ghost and a slide up to the fifth into bar 2
    (2, 1.0, F1, ACC, False), (3, 0.75, F2, GHOST, False), (5, 1.0, F2, NOTE, False), (6, 0.5, C2, NOTE, 'dip'),
    (7, 1.0, F2, NOTE, 'dip'), (11, 1.0, F1, NOTE, False), (12, 0.5, F2, GHOST, False), (13, 1.0, A1, NOTE, False),
    (14, 1.0, C2, NOTE, 'slide'),
    # bar 2, Bm9 (kicks on 16, the skip on 23, 26): B1 on the and of 1, its fifth, a dip from F#2 down to D2 and back,
    # a ghost on the skip kick's own F#, B1 between the kicks, then C#2 and a slide up to D2 into the downbeat
    (18, 1.0, B1, ACC, False), (19, 0.75, F2, GHOST, False), (20, 1.0, F2, NOTE, False), (21, 0.5, D2, NOTE, 'dip'),
    (22, 0.6, F2, NOTE, 'dip'), (24, 1.0, B1, NOTE, False), (27, 0.5, F2, GHOST, False), (28, 1.0, C2, NOTE, False),
    (29, 1.75, D2, NOTE, 'slide'),
)
LINE_STEPS = 32
# the organ bass's line: the same bounce in the low octave (F#1 to C#2), so its drawbar body (2nd to 4th harmonics)
# stays under its sub: the warp line's F#2 pops, repitched up from the C#2 zone, put the organ's 8' at 185 Hz
ORGAN_LINE = (
    (2, 1.5, F1, ACC, False), (5, 0.75, F1, GHOST, False), (7, 1.5, A1, NOTE, False), (11, 1.0, F1, NOTE, False),
    (13, 1.0, A1, NOTE, False), (14, 1.0, C2, NOTE, 'slide'),
    (18, 1.5, B1, ACC, False), (21, 0.75, B1, GHOST, False), (24, 1.0, A1, NOTE, False), (27, 0.5, F1, GHOST, False),
    (28, 1.0, GS1, NOTE, False), (29, 1.75, C2, NOTE, 'slide'),
)
# the 4x4 line, 2 bars: the kick on every beat, the bass on the off-beat 8ths between them, with pops and a dip
KICK_4X4 = tuple((4 * k, 1.0 if k % 2 == 0 else 0.9) for k in range(8))
SPEED_LINE = (
    (2, 1.25, F1, ACC, False), (6, 1.25, F1, NOTE, False), (7, 0.5, F2, GHOST, False), (10, 1.25, F2, ACC, False),
    (11, 0.5, C2, NOTE, 'dip'), (14, 1.25, A1, NOTE, False),
    (18, 1.25, B1, ACC, False), (22, 1.25, B1, NOTE, False), (23, 0.5, F2, GHOST, False), (26, 1.25, F2, ACC, False),
    (27, 0.5, D2, NOTE, 'dip'), (30, 1.25, C2, NOTE, False),
)
CLAP_4X4 = ((4, 1.0), (12, 1.0), (20, 1.0), (28, 1.0))

# ------------------------------------------------------------------ the mix
LANES = {  # fader dB, pan, space send, echo send, resonator send
    'kick': (0.0, 0.0, 0.0, 0.0, 0.0),
    'bass': (0.0, 0.0, 0.0, 0.0, 0.0),
    'clap': (-2.0, 0.0, 0.22, 0.0, 0.0),
    'hat_c': (-9.5, 0.22, 0.04, 0.0, 0.0),
    'hat_o': (-12.5, -0.15, 0.08, 0.0, 0.0),
    'tamb': (-12.5, -0.3, 0.05, 0.0, 0.0),
    'rim': (-11.0, 0.3, 0.12, 0.08, 0.0),
    'perc': (-8.0, -0.25, 0.4, 0.3, 0.5),
    'stab': (-8.0, 0.0, 0.25, 0.0, 0.0),
    'pluck': (-11.0, 0.0, 0.30, 0.25, 0.0),
    'vox': (-1.5, 0.0, 0.35, 0.25, 0.0),
    'fill': (-3.0, 0.0, 0.25, 0.25, 0.0),
}
SENDS = {'space': 2, 'echo': 3, 'resonator': 4}
FILES = {'kick': 'KickGarage', 'clap': 'ClapGarage', 'hat_c': 'HatClosed', 'hat_o': 'HatOpen', 'tamb': 'Tambourine',
         'rim': 'RimShot', 'perc': 'AlloyHit', 'stab': 'OrganStab', 'pluck': 'ChromePluckPhrase'}
WARP = {'clean': 'WarpClean', 'warm': 'WarpWarm', 'organ': 'OrganBass'}
BASS_LUFS = -16.5                   # the bass lane's loudness, A and B alike (before the loop's gain): 0.3 LU under
                                    # the kick lane, so the kick leads
LOOP_LUFS = -16.3                   # the dry and wet loops at one loudness
WET_BASS = 'warm'
KICK_CLIP_DB = 1.5                  # the kick lane soft-clipped this far under its own true peak (master.soft_clip)
DUCK = dict(depth_db=6.0, ramp=0.002, hold=qa.KICK_OWN_S - 0.002, release=0.060)   # the bass under every kick
CLAP_DUCK = dict(depth_db=3.0, ramp=0.002, hold=0.030, release=0.060)              # and under every clap
WOBBLE = dict(xover=450.0, base=650.0, depth=(2.2, 3.0, 2.2, 3.4), q=1.6)   # the lane filter: open up to base x
                                    # depth (per bar) at the middle of each 8th, closed on its edges
WALL_SPLIT = 250.0                  # bass A, after the dips: everything over this walled again at bass.MID_HP
MONO_BELOW = 450.0
BASS_WIDTH = 0.6
PLATE = dict(S.SPACE, t60=1.1, size=0.45, damp_hz=9000.0, predelay=round(K.STEP / 4, 6), send_hp=500.0, t60_low=0.4,
             shimmer=0.03)
ECHO = dict(S.ECHO, hp_hz=600.0)
RESONATOR_TONES = tuple(n + 24 for n in (54, 61, 64, 68, 69))
# the lane inserts, run round the loop: (Hz, dB, Q) peaking filters, then a soft clip this far under the lane's own
# true peak (master.soft_clip, 4x): the measured mix put the 1 kHz octave over everything (the bass's top and the vox),
# the top over its windows (the clap's and the open hat's air) and the low mids under theirs; and the vox, the bass and
# the alloy hit set the peaks away from the kicks, which held the master short of its target
EQ = {
    'bass': ((200.0, 1.0, 1.0), (1000.0, -4.5, 0.8)),
    'kick': ((125.0, 2.0, 1.0),),
    'vox': ((500.0, -2.5, 1.0), (1000.0, -2.0, 1.0), (3200.0, 2.0, 1.0)),
    'stab': ((250.0, 7.5, 0.8), (1000.0, -2.0, 0.9)),
    'clap': ((220.0, 6.0, 1.0), (9000.0, -5.5, 0.7)),
    'hat_c': ((14000.0, -7.0, 0.7),),
    'hat_o': ((14000.0, -8.0, 0.7),),
    'tamb': ((9000.0, -3.5, 0.8),),
}
CLIP_DB = {'vox': 3.0, 'perc': 4.0, 'stab': 1.5}
# the tops' width: each lane's side over this frequency scaled up (its mid, and so its mono sum, untouched). The first
# mix read 13-14 dB of side under mid at 4-16 kHz, narrower than the standard's windows (-10..-4, -8..-3): the clap and
# the hats carry the top, and their own side sits about 11 dB under their mid
WIDEN = {'clap': (1500.0, 3.4), 'hat_c': (3000.0, 2.2), 'hat_o': (3000.0, 2.2), 'tamb': (3000.0, 2.2)}


def circ(fn, x):
    return Ph.circular(fn, x)


def fader(k, x):
    return Ph.pan(x, LANES[k][1]) * D.undb(LANES[k][0])


def inserts(k, x):
    """A lane's inserts, in order: its EQ (round the loop), its soft clip, then its top's width."""
    for fc, gain, q in EQ.get(k, ()):
        x = circ(lambda u, fc=fc, gain=gain, q=q: D.peaking(u, fc, gain, q), x)
    if k in CLIP_DB:
        x = MS.soft_clip(x, D.true_peak_db(x) - CLIP_DB[k] + 0.5)[0]
    if k in WIDEN:
        x = widen(x, *WIDEN[k])
    return x


def widen(x, fc, amount):
    """The side over fc (a 4th-order high-pass, round the loop) scaled by amount; the mid as it is."""
    m, s = 0.5 * (x[:, 0] + x[:, 1]), 0.5 * (x[:, 0] - x[:, 1])
    s = s + (amount - 1.0) * circ(lambda u: D.bfilt(u, fc, 'highpass', order=4), s)
    return np.stack([m + s, m - s], 1)


def kick_clip(x):
    """The kick lane's soft clip (master.soft_clip, round the loop): its ceiling KICK_CLIP_DB under the lane's true
    peak plus the clipper's 0.5 dB over."""
    if not KICK_CLIP_DB:
        return x
    return MS.soft_clip(x, D.true_peak_db(x) - KICK_CLIP_DB + 0.5)[0]


# ------------------------------------------------------------------ the lanes
def choked_lane(name, hits, chokes, grid, fade_s=0.005):
    """One-shots on the grid like phrase.hits_lane, each cut where the next choke hit starts (a 5 ms fade): an open hat
    in a hat group, choked by the closed hat."""
    item, got, _ = Ph.kit_files(K, name)
    buf = np.zeros((grid.length, 2))
    at = sorted(grid.at(s) for s, _ in chokes)
    for k, (step, v) in enumerate(hits):
        layer = 1 + min(range(len(item.layers)), key=lambda i: (abs(item.layers[i] - v), -i))
        x = np.array(got[(None, layer, 1 + k % item.rr)].audio)
        p = grid.at(step)
        nxt = min(((a - p) % grid.length for a in at if (a - p) % grid.length > 0), default=len(x))
        if nxt < len(x):
            f = int(fade_s * SR)
            x = x[:nxt]
            x[-f:] *= np.linspace(1.0, 0.0, f)[:, None]
        Ph.place(buf, x, p)
    return buf


def zoned_lane(name, hits, grid):
    """Zoned one-shots on the grid: (16th, zone note, velocity), the layer nearest the velocity, scaled by (velocity /
    layer)^1.5 as the sampler stand-in does."""
    item, got, _ = Ph.kit_files(K, name)
    buf = np.zeros((grid.length, 2))
    for step, zone, v in hits:
        i = min(range(len(item.layers)), key=lambda j: (abs(item.layers[j] - v), -j))
        Ph.place(buf, got[(zone, i + 1, 1)].audio * (v / item.layers[i]) ** 1.5, grid.at(step))
    return buf


def line_voices(line, grid, reps):
    """A line as voices, (onset, [(offset, note, velocity, glide s)], note-off offset), repeated reps times."""
    out = []
    for r in range(reps):
        for step, length, note, vel, slide in line:
            step = step + r * LINE_STEPS
            on = grid.at(step)
            if slide:
                start, notes, _ = out[-1]
                off = (on - start) % grid.length
                notes.append((off, note, vel, GLIDES[slide]))
                out[-1] = (start, notes, off + int(round(length * grid.s16)))
            else:
                out.append((on, [(0, note, vel, 0.0)], int(round(length * grid.s16))))
    return out


def bass_voice(files, notes, n_on):
    """One sampler voice on the bass files: the first note's zone and layer, its pitch and gain gliding to each slid or
    dipped note over that note's glide, held for n_on samples, then released (basslines.voice, with a glide per
    note)."""
    _, first, vel, _ = notes[0]
    zone = B.zone_of(first)
    layer, _ = B.layer_of(vel)
    src = files[(zone, layer)]
    m = n_on + int(B.RELEASE * SR)
    t = np.arange(m)
    semis, gain = np.zeros(m), np.zeros(m)
    for k, (off, note, v, glide) in enumerate(notes):
        s, g = note - zone, (v / B.LAYERS[layer - 1]) ** 1.5
        if k == 0:
            semis[:], gain[:] = s, g
            s0, g0 = s, g
            continue
        w = 0.5 - 0.5 * np.cos(np.pi * np.clip((t - off) / (glide * SR), 0.0, 1.0))
        semis = np.where(t >= off, s0 + (s - s0) * w, semis)
        gain = np.where(t >= off, g0 + (g - g0) * w, gain)
        s0, g0 = s, g
    y = B.hermite(src.audio, np.concatenate([[0.0], np.cumsum(2.0 ** (semis[:-1] / 12.0))]), src.loop)
    return y * (gain * P.raised(m, B.ATTACK) * D.taper(m, n_on / SR, B.RELEASE))[:, None]


def bass_files(char):
    _, got, _ = Ph.kit_files(K, WARP[char])
    return {(note, layer): o for (note, layer, _), o in got.items()}


def wobble_cutoff(grid):
    """The lane filter's cutoff, Hz per sample: base x depth^w, w rising from 0 to 1 and back once an 8th (straight
    8ths, on the bar), its depth per bar."""
    t = np.arange(grid.length)
    eighth = 2 * grid.s16
    w = 0.5 - 0.5 * np.cos(2 * np.pi * t / eighth)
    bar = (t // (16 * grid.s16)).astype(int) % len(WOBBLE['depth'])
    return WOBBLE['base'] * np.array(WOBBLE['depth'])[bar] ** w


def wobble(x, grid):
    """The bass's top through the wobbling low-pass: over WOBBLE['xover'] (LR4, so the bands sum flat) a TPT
    state-variable low-pass, run over three passes of the loop so its state joins at the seam."""
    lo = circ(lambda u: D.lr4(u, WOBBLE['xover'], 'lowpass'), x)
    hi = circ(lambda u: D.lr4(u, WOBBLE['xover'], 'highpass'), x)
    fc = np.tile(wobble_cutoff(grid), 3)
    hh = np.tile(hi, (3, 1))
    top = np.stack([D.svf_tpt(hh[:, c], fc, q=WOBBLE['q'])[0] for c in range(2)], 1)[grid.length:2 * grid.length]
    return lo + top


def wall(x, char):
    """Bass A after its dips: everything over WALL_SPLIT walled again at the reese's 450 Hz (a dip moves the file's
    wall down with the pitch), so the lane keeps the strict gate. Bass B and the organ keep their body."""
    if char != 'clean':
        return x
    lo = circ(lambda u: D.lr4(u, WALL_SPLIT, 'lowpass'), x)
    hi = circ(lambda u: D.lr4(u, WALL_SPLIT, 'highpass'), x)
    return lo + circ(lambda u: P.hp_steep(u, B.bass.MID_HP), hi)


def width(x, amount):
    m, s = 0.5 * (x[:, 0] + x[:, 1]), 0.5 * (x[:, 0] - x[:, 1]) * amount
    return np.stack([m + s, m - s], 1)


@functools.lru_cache(maxsize=None)
def bass_lane(char, line='garage'):
    """The warp line on its grid for the character ('clean', 'warm' or 'organ'): the sampler voice with slides and
    dips, the wall (bass A), the wobble, the kick and clap duckers, mono under 450 Hz and narrower over it, then a
    fader to BASS_LUFS. Returns (lane, fader dB)."""
    grid, notes, kicks, claps = (G, WARP_LINE, KICK, CLAP) if line == 'garage' else (G2, SPEED_LINE, KICK_4X4, CLAP_4X4)
    if char == 'organ':
        notes = ORGAN_LINE
    files = bass_files(char)
    buf = np.zeros((grid.length, 2))
    for on, run, n_on in line_voices(notes, grid, grid.steps // LINE_STEPS):
        Ph.place(buf, bass_voice(files, run, n_on), on)
    x = wobble(wall(buf, char), grid) if char != 'organ' else wall(buf, char)
    duck = Ph.pump_db([grid.at(s) for s, _ in kicks], grid.length, **DUCK)
    duck = np.minimum(duck, Ph.pump_db([grid.at(s) for s, v in claps if v >= 0.8], grid.length, **CLAP_DUCK))
    x = x * D.undb(duck)[:, None]
    x = inserts('bass', circ(lambda u: D.mono_below(u, MONO_BELOW), width(x, BASS_WIDTH)))
    fader_db = BASS_LUFS - qa.lufs(x)
    return x * D.undb(fader_db), float(fader_db)


@functools.lru_cache(maxsize=None)
def kick_lane(name='KickGarage', line='garage'):
    grid, kicks = (G, KICK) if line == 'garage' else (G2, KICK_4X4)
    return inserts('kick', kick_clip(fader('kick', Ph.hits_lane(K, name, kicks, grid))))


def pluck_lane(grid):
    _, got, _ = Ph.kit_files(K, 'ChromePluckPhrase')
    buf = np.zeros((grid.length, 2))
    for step in range(0, grid.steps, 32):
        Ph.place(buf, got[(None, 1, 1)].audio, grid.at(step))
    return buf


def vox_parts():
    """The vox lane's parts: the riff's hooks (hooks.RIFF) in bars 1-2, each its own file so its tail rings on, the
    call from bar 3 (its first note on 32), the throw on 58."""
    parts = []
    for step, name, level in H.RIFF + ((32, 'CallLineA4', -2.0), (58, 'ThrowFloorE5', -1.0)):
        _, got, _ = Ph.kit_files(K, name)
        b = np.zeros((L, 2))
        Ph.place(b, got[(None, 1, 1)].audio * D.undb(level), G.at(step) - int(np.floor(onset(name) * SR + 0.5)))
        parts.append((name, b))
    return parts


def onset(name):
    return H.hook_onset(H.THROW_BY_NAME[name][1] if name in H.THROW_BY_NAME else name)


def fill_lane():
    _, got, _ = Ph.kit_files(K, 'GlitchFillLift')
    buf = np.zeros((L, 2))
    Ph.place(buf, D.fade(got[(None, 1, 1)].audio[fx.swung(FILL_FROM):], FILL_FADE_S, 0.0), G.at(FILL_AT))
    return buf


@functools.lru_cache(maxsize=1)
def other_lanes():
    hits = lambda k, part: Ph.hits_lane(K, FILES[k], part, G)
    raw = {'clap': hits('clap', CLAP), 'hat_c': hits('hat_c', HAT_C),
           'hat_o': choked_lane(FILES['hat_o'], HAT_O, HAT_C, G), 'tamb': hits('tamb', TAMB),
           'rim': hits('rim', RIM), 'perc': hits('perc', PERC), 'stab': zoned_lane(FILES['stab'], STABS, G),
           'pluck': pluck_lane(G), 'vox': sum(p for _, p in vox_parts()), 'fill': fill_lane()}
    return {k: fader(k, inserts(k, x)) for k, x in raw.items()}


def lanes(char):
    return {'kick': kick_lane(), 'bass': bass_lane(char)[0], **other_lanes()}


def kick_onsets(kicks=KICK, grid=None):
    g = grid or G
    return tuple(g.at(s) for s, _ in kicks)


def full_kicks(kicks=KICK, grid=None):
    g = grid or G
    return [g.at(s) for s, v in kicks if v == 1.0]


GRID = {'bpm': K.BPM, 'steps': STEPS, 'samples': L, 'swing': K.SWING, 'kick_steps': [s for s, _ in KICK],
        'clap_steps': [s for s, _ in CLAP], 'fill_step': FILL_AT}


def line_meta(line, char):
    return {'line': [{'step': s, 'length_16ths': n, 'note': D.note_name(p), 'velocity': v, 'slide': sl or None}
                     for s, n, p, v, sl in line], 'source': WARP[char], 'bass_lufs': BASS_LUFS,
            'wobble': {**WOBBLE, 'depth': list(WOBBLE['depth'])}, 'kick_duck': DUCK, 'clap_duck': CLAP_DUCK,
            'wall_after_dips': char == 'clean'}


def mix_meta(mix, char):
    return {'grid': GRID, 'lanes': {k: {'source': FILES.get(k, WARP.get(char) if k == 'bass' else k),
                                        'fader_db': round(bass_lane(char)[1], 2) if k == 'bass' else LANES[k][0],
                                        'pan': LANES[k][1], 'lufs': round(qa.lufs(v), 2)} for k, v in mix.items()},
            'bass': line_meta(WARP_LINE, char), 'kick_clip_db': KICK_CLIP_DB}


def context_dry(char):
    mix = lanes(char)
    return Out(sum(mix.values()), meta=mix_meta(mix, char))


def context_dry_clean(take):
    return context_dry('clean')


def context_dry_warm(take):
    return context_dry('warm')


def send_bus(mix, send):
    j = SENDS[send]
    return sum(mix[k] * LANES[k][j] for k in mix if k in LANES and LANES[k][j])


def resonator_freqs():
    return tuple(float(D.midi_hz(n)) * a for n in RESONATOR_TONES for a in K.RATIOS[:4])


@functools.lru_cache(maxsize=1)
def returns():
    mix = lanes(WET_BASS)
    two = lambda fn, x: Ph.two_pass(fn, x, SEAM_S)
    space = two(lambda u: S.space(u, K.space_rng('send r4'), **PLATE), D.mono(send_bus(mix, 'space')))
    key = np.concatenate([mix['vox'], mix['vox']])
    space = two(lambda u: S.duck(u, key, **S.DUCK)[0], space)
    echo = two(lambda u: S.echo(u, K.space_rng('echo r4'), S.ECHO_STEPS * K.STEP, **ECHO),
               D.mono(send_bus(mix, 'echo')))
    res = two(lambda u: S.resonator(u, resonator_freqs(), **S.RESONATOR), D.mono(send_bus(mix, 'resonator')))
    return {'space': space * D.undb(S.SPACE_RETURN_DB), 'echo': echo * D.undb(S.ECHO_RETURN_DB),
            'resonator': res * D.undb(S.RESONATOR_RETURN_DB)}


@functools.lru_cache(maxsize=1)
def wet_mix():
    mix = lanes(WET_BASS)
    meta = {**mix_meta(mix, WET_BASS), 'space': dict(PLATE), 'echo': dict(ECHO),
            'return_lufs': {k: round(qa.lufs(v), 2) for k, v in returns().items()},
            'target': 'the target the engine must match'}
    return sum(mix.values()) + sum(returns().values()), meta


def context_wet(take):
    y, meta = wet_mix()
    return Out(y, meta=meta)


def club_hits():
    return {'kick': full_kicks(), 'clap': [G.at(s) for s, v in CLAP if v >= 0.8]}


def context_club(take):
    """The wet loop as its file is (the renderer's 20 Hz high-pass, round the loop), through the club master at the
    genre's settings (master.settings(K): -9 LUFS, PLR >= 7.5 dB, at most 4 dB of peak reduction)."""
    y, meta = wet_mix()
    wet = circ(lambda u: D.dc_block(u, 20.0), y)
    hits = club_hits()
    z, report = MS.master(wet, MS.settings(K), hits=hits)
    return Out(z, top_hits=hits['kick'], meta={**meta, 'target': 'the club master (ADR 0014)', 'master': report})


def context_low(char):
    kick, bass = kick_lane(), bass_lane(char)[0]
    return Out(kick + bass, hits=kick_onsets(), top_hits=full_kicks(),
               meta={'grid': GRID, 'bass': line_meta(WARP_LINE, char), 'kick_clip_db': KICK_CLIP_DB})


def context_low_clean(take):
    return context_low('clean')


def context_low_warm(take):
    return context_low('warm')


# ------------------------------------------------------------------ the 4x4 variant, 2 bars
G2 = Ph.Grid(K, 32)


def speed_lanes(char):
    hits = lambda k, part: Ph.hits_lane(K, FILES[k], part, G2)
    two = lambda part: tuple((s, v) for s, v in part if s < 32)
    hat_c = two(bars(HAT_C_BAR, 2, 32))
    raw = {'clap': hits('clap', CLAP_4X4), 'hat_c': hits('hat_c', hat_c),
           'hat_o': choked_lane(FILES['hat_o'], two(bars(HAT_O_BAR, 2, 32)), hat_c, G2),
           'tamb': hits('tamb', two(bars(TAMB_BAR, 2, 32))), 'rim': hits('rim', two(RIM)),
           'stab': zoned_lane(FILES['stab'], STABS_2, G2)}
    out = {k: fader(k, inserts(k, x)) for k, x in raw.items()}
    return {'kick': kick_lane('KickGarage', 'speed'), 'bass': bass_lane(char, 'speed')[0], **out}


def speed_loop(take):
    mix = speed_lanes(WET_BASS)
    return Out(sum(mix.values()), hits=kick_onsets(KICK_4X4, G2), top_hits=full_kicks(KICK_4X4, G2),
               meta={'grid': {'bpm': K.BPM, 'steps': 32, 'swing': K.SWING, 'kick_steps': [s for s, _ in KICK_4X4]},
                     'bass': line_meta(SPEED_LINE, WET_BASS)})


def speed_low(take):
    kick, bass = kick_lane('KickGarage', 'speed'), bass_lane('clean', 'speed')[0]
    return Out(kick + bass, hits=kick_onsets(KICK_4X4, G2), top_hits=full_kicks(KICK_4X4, G2),
               meta={'bass': line_meta(SPEED_LINE, 'clean')})


# ------------------------------------------------------------------ previews
DRUM_LUFS = -16.5
GROOVE_LANES = ('clap', 'hat_c', 'hat_o', 'tamb', 'rim')


def drum_groove(kick, swing=None):
    """Bars 1 and 2 of the loop's drums with this kick (its lane through the loop's kick clip and EQ), the other drums
    at the loop's faders, pans and inserts; on the kit's swing unless given."""
    g = Ph.Grid(K, 32)
    if swing is not None:
        g.swing = swing
    two = lambda part: tuple((s, v) for s, v in part if s < 32)
    buf = inserts('kick', kick_clip(fader('kick', Ph.hits_lane(K, kick, two(KICK), g))))
    parts = {'clap': two(CLAP), 'hat_c': two(HAT_C), 'tamb': two(TAMB), 'rim': two(RIM)}
    for k, part in parts.items():
        buf = buf + fader(k, inserts(k, Ph.hits_lane(K, FILES[k], part, g)))
    buf = buf + fader('hat_o', inserts('hat_o', choked_lane(FILES['hat_o'], two(HAT_O), two(HAT_C), g)))
    return Out(buf, top_hits=full_kicks(two(KICK), g),
               meta={'steps': 32, 'bpm': K.BPM, 'swing': g.swing, 'kick': kick, 'kick_clip_db': KICK_CLIP_DB})


def groove_garage(take):
    return drum_groove('KickGarage')


def groove_split(take):
    return drum_groove('KickSplit')


def groove_swing57(take):
    return drum_groove('KickGarage', 0.57)


def warp_line(char):
    """The warp line alone with its kick (KickGarage, the loop's 2-step kick part) and the closed hats, 2 bars."""
    g = Ph.Grid(K, 32)
    two = lambda part: tuple((s, v) for s, v in part if s < 32)
    kick = inserts('kick', kick_clip(fader('kick', Ph.hits_lane(K, 'KickGarage', two(KICK), g))))
    hats = fader('hat_c', inserts('hat_c', Ph.hits_lane(K, FILES['hat_c'], two(HAT_C), g)))
    bass = bass_lane(char)[0][:g.length]
    return Out(kick + hats + bass, hits=kick_onsets(two(KICK), g),
               meta=line_meta(ORGAN_LINE if char == 'organ' else WARP_LINE, char))


def warp_line_clean(take):
    return warp_line('clean')


def warp_line_warm(take):
    return warp_line('warm')


def organ_line(take):
    return warp_line('organ')


R4 = ('r4',)
R45 = ('r4', 'r5')                  # round 5 carries these on, unchanged (its own loops are context5.py's)
ABOUT = '4-bar UK garage context loop, 134 BPM, F# minor: the 2-step kick, the garage clap, swung hats and tambourine'
FULL_MIX = {'mud': 'off', 'why': 'the full mix: vocals, the clap, the stabs and the pluck belong in 150-400 Hz; its '
                                 'low '
                                 'end is checked as GarageContextLow (the gate) and GarageContextLowWarm (bass-body), '
                                 'and every other lane passed its own low cut'}
CLUB = {'mud': 'off', 'why': FULL_MIX['why']}
CLEAN = {'mud': 'gate', 'why': 'A, clean: the strict mud gate in every gap between the kicks the loop holds'}
WARM = {'mud': 'bass-body', 'why': 'B, warm: the proposed bass-body rule (ADR 0011) in every gap between the kicks the '
                                   'loop holds'}
ORGAN = {'mud': 'bass-body', 'why': 'the organ bass keeps its drawbar body: the proposed bass-body rule (ADR 0011) in '
                                    'every gap between the kicks the loop holds'}
DRUMS = {'mud': 'off', 'why': 'a preview: the drums as the loop plays them; the kicks pass the mud gate, the rest the '
                              'low cut, each on its own'}
ITEMS = [
    Item('Loops', 'GarageContext', 'loop', context_dry_clean, loop=True, level=LOOP_LUFS, sets=R4, tags=('dry',),
         qa=FULL_MIX, about=f'{ABOUT}, dry, warp bass A (clean): no sends, no master'),
    Item('Loops', 'GarageContextWarm', 'loop', context_dry_warm, loop=True, level=LOOP_LUFS, sets=R4, tags=('dry',),
         qa=FULL_MIX, about=f'{ABOUT}, dry, warp bass B (warm), its lane at the same loudness'),
    Item('Loops', 'GarageContextWet', 'loop', context_wet, loop=True, level=LOOP_LUFS, sets=R4, tags=('wet',),
         qa=FULL_MIX,
         about=f'{ABOUT}, wet, bass B: the garage plate, the echo, a little resonator; the engine\'s target'),
    Item('Loops', 'GarageContextClub', 'loop', context_club, loop=True, hp_hz=None, sets=R4, tags=('wet', 'club'),
         qa=CLUB, about=f'{ABOUT}: GarageContextWet through the club master, aimed at the UK garage target, -9 LUFS'),
    Item('Loops', 'GarageContextLow', 'loop', context_low_clean, loop=True, sets=R45, tags=('with_kick',), qa=CLEAN,
         about=f'{ABOUT}: the kick and warp bass A lanes alone, mud-gated in every gap between kicks'),
    Item('Loops', 'GarageContextLowWarm', 'loop', context_low_warm, loop=True, sets=R45, tags=('with_kick',), qa=WARM,
         about=f'{ABOUT}: the kick and warp bass B lanes alone, under the bass-body rule in every gap'),
    Item('Loops', 'SpeedGarageLoop', 'loop', speed_loop, loop=True, level=LOOP_LUFS, sets=R45, tags=('dry',),
         qa={'mud': 'off', 'why': 'the full mix: the clap and the stabs belong in 150-400 Hz; its low end is checked '
                                  'as SpeedGarageLow (the gate), and every other lane passed its own low cut'},
         about='2-bar speed garage variant, 134 BPM, F# minor: the kick on every beat, the warp line (B) on the '
               'off-beats between the kicks, the clap, swung hats, tambourine and organ stabs'),
    Item('Loops', 'SpeedGarageLow', 'loop', speed_low, loop=True, sets=R45, tags=('with_kick',), qa=CLEAN,
         about='the speed garage variant\'s kick and warp bass A alone, mud-gated in every gap between kicks'),
    Item('Loops', 'DrumGroove4Garage', 'loop', groove_garage, loop=True, level=DRUM_LUFS, sets=R45,
         tags=('preview', 'with_kick'), qa=DRUMS,
         about='preview: kick B (Garage) in bars 1-2 of the round 4 drums (clap, closed and open hats, tambourine, '
               'rim), 134 BPM'),
    Item('Loops', 'DrumGroove4Split', 'loop', groove_split, loop=True, level=DRUM_LUFS, sets=R45,
         tags=('preview', 'with_kick'), qa=DRUMS,
         about='preview: kick A (Split) in bars 1-2 of the round 4 drums, 134 BPM'),
    Item('Loops', 'DrumGroove4Swing57', 'loop', groove_swing57, loop=True, level=DRUM_LUFS, sets=R45,
         tags=('preview', 'with_kick'), qa=DRUMS,
         about='preview: DrumGroove4Garage on round 3\'s swing, 0.57, against the kit\'s new one'),
    Item('Loops', 'WarpLineClean', 'loop', warp_line_clean, loop=True, sets=R45, tags=('with_kick',), qa=CLEAN,
         about='2-bar warp line, A clean (WarpClean), with the 2-step kick and the closed hats, 134 BPM, F# minor'),
    Item('Loops', 'WarpLineWarm', 'loop', warp_line_warm, loop=True, sets=R45, tags=('with_kick',), qa=WARM,
         about='2-bar warp line, B warm (WarpWarm), with the 2-step kick and the closed hats, 134 BPM, F# minor'),
    Item('Loops', 'OrganLine', 'loop', organ_line, loop=True, sets=R45, tags=('with_kick',), qa=ORGAN,
         about='2-bar line on the organ bass, with the 2-step kick and the closed hats, 134 BPM, F# minor'),
]
