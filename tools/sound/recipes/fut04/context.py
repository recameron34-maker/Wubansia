"""FUT04 "Furnace Code" context loop for listening round 2: 4 bars at 136 BPM in E minor, written for this kit and
played on its round 2 files, so each sound is heard where it will live.

Round 1 rated FUT01's loop Cool 4 with one fix: "Reese doesnt sound that musical and doesnt make me want to dance. The
high plucks are nice." So this loop is built on a bassline that dances with the kick and keeps the kit's melodic top,
the modular blips, up front.

- ContextLoop: dry, with bass A (clean). The kit's files exactly as written, played the way the sampler will: hits on
  the 16th grid, every lane on the kit's one swing (K.SWING, 0.54), a velocity picks the nearest layer, round robins
  cycle, the open hat is choked by the next closed hat (drums.CHOKE), the drone is held right through the loop point.
  A fader and a pan per lane, and one ducker: the bass lane's band over 500 Hz dips about 6 dB under the vocals
  (BASS_DUCK, sends.duck, as the Space return does), so the words read over the acid squelch, and the fill's band over
  2.5 kHz the same way, so its hiss leaves "the furnace" its consonants (DIPS). No other insert, no master chain. The
  three full loops sit at one loudness (LOOP_LUFS).
- ContextLoopWarm: the same loop with bass B (warm). Both bass lanes are set to one loudness (LANE_LUFS['bass']) before
  the dip, so the two dry loops differ only in the bass's character: B is A's sub and squelch plus a body from 120 Hz
  (bass.character).
- ContextLoopWet: the loop with the picked line in B, warm (WET_BASS: the integrator's pick, since the bass designer
  named a line, AcidLine1, but no character; B is round 1's answer to the hollow reese, body on small speakers, and the
  bass sends nothing, so the A/B in the dry loops is untouched), plus the three shared sends in their Python versions
  (tools/sound/sends.py): Space, the 8-line FDN with a little octave-up shimmer, seeded by the kit's dark concrete
  space and ducked under the vocals; Echo, the tape ping-pong on a dotted 8th; and the resonator stand-in, 2-pole modes
  on the drone's chord two octaves up times the kit's iron ratios (K.RATIOS[:4]). The last chop is a throw (full Space
  and Echo send) into the fill bar. The target the engine must match (S1e): its settings are in the file's metadata in
  qa_report.json.
- ContextLoopLow: the kick and bass A lanes alone (the acid line and the rumble), as they sit in the loop but without
  the bass's dip under the vocals (over 500 Hz, which the low-end rules don't read), mud-gated over the loop and in
  every gap between its kicks (tag with_kick). ContextLoopLowWarm: the same with bass B, under the bass-body rule
  (ADR 0011).

The parts (16ths 0-63, bar b starts at 16 b):
- Kick: four on the floor, every kick on the top layer, machine-hard as industrial techno plays it, out on beat 4 of
  bar 4 (KICK_OUT) for the fill's last beat, so the downbeat lands. The top layer also carries the phase the bass's sub
  pedal and the rumble are turned to (bass.PEDAL_PHASE, bass.RUMBLE_PHASE): its E1 tail rings at 116-129 degrees,
  while the softer layers ring elsewhere (v4 near 0), so a softer kick on 2 and 4 would fight the pedal instead of
  adding to it.
- Snare: the gated iron snare on 2 and 4 all through, bar 4 too, with ghosts on swung 16ths (a drag into bar 2's
  beat 3 and bar 3, a pickup into the fill bar). On beat 4 of bar 4 it is the only drum left.
- Hats: the hat loop's part (drums.HAT_CLOSED, HAT_OPEN): closed 16ths driving round an open hat on every off-beat,
  each open hat choked by the next closed one, rung out into the bar line on beat 4. They stop after beat 1 of bar 4,
  where the stutter fill takes the top.
- Bass: the designers' pick, AcidLine1, "the rolling hook" (bass.PICK), twice. It rests on every kick and answers in
  the gap: root on the 16th after the kick, an accented leap on the off-beat 8th whose target changes (E2, G1 sliding
  to A1, D2), a squelch-only tick on beat 3's kick, and bar 1 closes on a held B1; bar 2 brings the hook back varied
  (E2 sliding to D2), holds a dark C2 for a beat, peaks on G2 and climbs B1 D2 E2 home into the downbeat. Its slides,
  accents, ghosts and ticks are the acid voice's own (bass.LINES). Under each beat's first 250 ms its sub holds E1 in
  phase with the kick's tail (bass.SUB_PEDAL_S), so the low end adds up and never beats.
- Rumble, the second bass gesture, once: its 2 bars under bars 3 and 4, the kick-fed sub swelling after each kick in
  phase with the kick's tail (bass.RUMBLE_PHASE) and gone before the next; it drops out with the kick on beat 4 of
  bar 4.
- Drone: Em(add9), its 4-bar loop region as the sampler holds it, read so the filter is darkest at bar 1 and most open
  at bar 3 (its sweep runs once per loop).
- Lead, up front: the modular blip sequence (2 bars, twice), the kit's signature. Call and response: the anvil answers
  in the sequence's gaps, on the swung 16th after its motif in bars 1-3, and round bar 2's C6 ratchet (before it,
  and a ghost after it).
- Vocals, call and response: the robot calls "Fill the furnace" on bar 2's downbeat; the sung line answers, "come
  down" on bar 3's downbeat, then "the furnace" with "fur" on bar 4's downbeat, thrown into the fill (wet loop).
- Fill: the stutter fill, all of bar 4 over the kick on beats 1-3 and the snare, its 64th roll landing on the loop's
  downbeat.
Everything loops: hits and tails wrap round the loop, the sends run over two passes, keeping the second.

The three full loops need the vocal models (the chops and the robot: see tools/sound/README.md). The low-end loops
and the previews don't: each lane renders only its own files (raw_lane).
"""
import functools

import numpy as np

from tools.sound import dsp48 as D
from tools.sound import phrase as Ph
from tools.sound import qa
from tools.sound import sends as S
from tools.sound.item import Item, Out
from tools.sound.kits import fut04 as K
from tools.sound.recipes.fut04 import bass, drums, pad, vox

SR = D.SR
STEPS = 64                                # 4 bars of 16ths
L = 2 * Ph.loop_len(K, bass.LOOP_STEPS)   # 338824 samples: twice the 2-bar phrases, so the bass line and the blip
G = Ph.Grid(K, STEPS, L)                  # sequence tile exactly; the same as loop_len(K, 64) and the drone's loop
SEAM_S = 0.02                             # the sends' last 20 ms blend into what precedes the loop's start
CLEAN, WARM = 'clean', 'warm'
WET_BASS = WARM                           # the wet loop's bass (see the docstring)

# ------------------------------------------------------------------ the parts: (16th, velocity) unless noted
FILL_AT = 48                              # bar 4: the stutter fill, ending exactly on the loop's downbeat
KICK_OUT = 60                             # beat 4 of bar 4: no kick (and no rumble) under the fill's 64th roll
KICK = tuple((s, 1.0) for s in range(0, STEPS, 4) if s != KICK_OUT)   # every kick full, machine-hard
SNARE = ((4, 1.0), (12, 0.8), (20, 1.0), (23, 0.2), (28, 0.8), (31, 0.2), (36, 1.0), (44, 0.8), (47, 0.4), (52, 1.0),
         (60, 0.8))
HAT_END = 52                              # the hats stop after beat 1 of bar 4: the fill takes the top from there
HAT_CLOSED = tuple((s + 32 * p, v) for p in (0, 1) for s, v in drums.HAT_CLOSED if s + 32 * p < HAT_END)
HAT_OPEN = tuple((s + 32 * p, v) for p in (0, 1) for s, v in drums.HAT_OPEN if s + 32 * p < HAT_END)
ANVIL = ((9, 0.8), (25, 1.0), (27, 0.4), (41, 0.8))   # in the blip sequence's gaps, on swung 16ths: round the C6
                                                      # ratchet in bar 2, before it and a ghost after it
LINE = bass.PICK                          # the bass designer's pick of the two lines: AcidLine1, the rolling hook
LEAD_AT = (0, 32)                         # the 2-bar blip sequence, twice
RUMBLE_AT = 32                            # the rumble's 2 bars, once: bars 3 and 4
ROBOT_AT = 16                             # "Fill the furnace": its step 0 ("fill") on bar 2's downbeat
THROW = (1.0, 1.0)                        # a throw (CC91-style): the chop's full Space and Echo send
CHOPS = ((32, 'ChopComeDown', None), (46, 'ChopTheFurnace', THROW))   # the chop's first note lands on the 16th (even
                                          # 16ths, so the chop's own swung timing lines up); its sends (None: the
                                          # lane's)
MUTE_EDGE = 0.005                         # the rumble's mute under KICK_OUT: a 5 ms raised cosine, in its own silence

# ------------------------------------------------------------------ the mix: source, pan, the three sends
# Each lane's fader is set so the lane reads LANE_LUFS over the loop (LUFS-I, before the loop's one gain and the bass's
# dip under the vocals): the kick leads, the chops level with it and the robot 0.5 LU under (3 LU up from the first
# mix, where in their loudest 50 ms frames the chops sat 13 dB and the robot 6 dB under the rest of the mix at 1-4 kHz;
# now 7 dB and 0 dB), the acid line 1.5 LU under the kick, the blips 2.5 LU under it (up front: FUT01's plucks sat
# 7.5 LU under their kick, and round 1 still heard them), the drone and the hats well back, the rumble felt more than
# heard. Both bass characters read the same, so the A/B is the sound alone. The meta records every fader.
LANES = {
    'kick': ('KickSplit', 0.0, 0.0, 0.0, 0.0),
    'bass': (None, 0.0, 0.0, 0.0, 0.0),             # its file per character: line_name()
    'rumble': ('Rumble', 0.0, 0.0, 0.0, 0.0),
    'snare': ('SnareGated', 0.0, 0.15, 0.0, 0.0),
    'hat': ('ClosedHat, OpenHat', 0.15, 0.08, 0.10, 0.10),
    'perc': ('Anvil', -0.25, 0.60, 0.50, 0.10),
    'pad': ('DroneEmAdd9', 0.0, 0.45, 0.0, 0.0),
    'lead': ('ModBlipPhrase', 0.0, 0.25, 0.20, 0.05),
    'chops': (', '.join(n for _, n, _ in CHOPS), 0.0, 0.80, 0.60, 0.20),
    'robot': (vox.ROBOT_ITEM, 0.0, 0.60, 0.40, 0.0),
    'fill': ('StutterFill', 0.0, 0.25, 0.25, 0.0),
}
LANE_LUFS = {'kick': -17.0, 'bass': -18.5, 'rumble': -25.0, 'snare': -19.5, 'hat': -25.0, 'perc': -24.0, 'pad': -25.0,
             'lead': -19.5, 'chops': -17.0, 'robot': -17.5, 'fill': -20.5}
LOOP_LUFS = -17.0                         # the three full loops at one loudness, so dry A, dry B and wet compare by
                                          # sound: just under the quietest one's level with its peak at the ceiling
                                          # (dry B, -16.9; the wet loop -16.6, dry A -15.4)
SENDS = {'space': 2, 'echo': 3, 'resonator': 4}   # column of each send in LANES
VOCALS = ('chops', 'robot')               # the Space return and the lanes' top bands (DIPS) duck under these
BASS_DUCK_HZ = 500.0                      # the bass lane's band over this dips under the vocals, so the words read
FILL_DUCK_HZ = 2500.0                     # ...and the fill's (its 2.5-9 kHz hiss): in the fill bar it covered the
                                          # consonants of "the furnace" at 2.4-4.8 kHz
DIPS = {'bass': BASS_DUCK_HZ, 'fill': FILL_DUCK_HZ}
BASS_DUCK = dict(depth_db=9.0, range_db=30.0, attack=0.010, release=0.150)   # sends.duck (the engine's ducker, as on
                                          # the Space return), for every dip: about 6 dB while a word sounds. The acid
                                          # squelch covered the chops' 1-4 kHz by 8 dB in their loudest frames; with the
                                          # dip and the vocal lanes 3 dB up it sits level with them, 6 dB under the robot
LOW = ('kick', 'bass', 'rumble')
RESONATOR_TONES = tuple(n + 24 for n in pad.CHORD)   # the drone's Em(add9) two octaves up, E5 B5 E6 G6 F#7, times
                                          # RATIOS[:4] (the anvil's first bending mode, its warbling pair and the iron
                                          # third): modes from 0.66 to 7.2 kHz, where the hats, anvil, blips and chops
                                          # reach, out of the low mids


def layer_of(item, v):
    """The layer nearest a velocity (the louder on a tie), as phrase.hits_lane picks it."""
    return 1 + min(range(len(item.layers)), key=lambda i: (abs(item.layers[i] - v), -i))


def tile(x):
    """A 2-bar loop file twice: the 4 bars."""
    x = D.st(x)
    if 2 * len(x) != L:
        raise ValueError(f'a 2-bar loop of {len(x)} samples does not tile the {L}-sample context loop')
    return np.concatenate([x, x])


def centred(x, p):
    """phrase.pan, except in the centre, where it is exactly unity (pan(x, 0) scales L and R by cos and sin of 45
    degrees, which differ in their last bit, so a mono lane would come out stereo)."""
    return D.st(x) if p == 0 else Ph.pan(x, p)


def kit_file(name, note=None, layer=1, rr=1):
    return Ph.kit_files(K, name)[1][(note, layer, rr)]


# ------------------------------------------------------------------ the lanes, on the kit's files as written
def hat_lane(grid=G, closed=HAT_CLOSED, opened=HAT_OPEN):
    """The closed and open hats on a grid, round robins in turn; each open hat fades out over the choke's 4 ms from
    the next closed hat (the sampler's choke group, drums.CHOKE)."""
    n = grid.length
    buf = np.zeros((n, 2))
    closed_at = sorted(grid.at(s) for s, _ in closed)
    choke_n = int(drums.CHOKE['fade_s'] * SR)
    for name, hits in (('ClosedHat', closed), ('OpenHat', opened)):
        item, got, _ = Ph.kit_files(K, name)
        for k, (step, vel) in enumerate(hits):
            x = np.array(D.st(got[(None, layer_of(item, vel), 1 + k % item.rr)].audio))
            p = grid.at(step)
            if name == 'OpenHat' and closed_at:
                nxt = min((c - p) % n for c in closed_at if c != p)
                g = np.ones(len(x))
                m = len(g[nxt:nxt + choke_n])
                g[nxt:nxt + m] = np.cos(0.5 * np.pi * np.arange(m) / choke_n) ** 2
                g[nxt + choke_n:] = 0.0
                x *= g[:, None]
            Ph.place(buf, x, p)
    return buf


def line_name(char):
    return f"{LINE}{'Clean' if char == CLEAN else 'Warm'}"


def bass_raw(char):
    """The picked line twice, in the character: the bass lane before its fader."""
    return tile(kit_file(line_name(char)).audio)


def mute_mask(at, edge=MUTE_EDGE):
    """1 up to the 16th `at`, then a raised cosine down to 0 over `edge`, and 0 to the loop's end."""
    a, e = G.at(at), int(edge * SR)
    g = np.ones(L)
    g[a:a + e] = 0.5 + 0.5 * np.cos(np.pi * np.arange(e) / e)
    g[a + e:] = 0.0
    return g


def rumble_lane():
    """The rumble's 2 bars once, from RUMBLE_AT to the loop's end, silent from KICK_OUT: it swells after each kick, so
    with no kick there it rests too. Its pump holds it 40 dB down from 375 ms after each kick, so the mute and both
    ends fall in its own silence."""
    x = kit_file('Rumble').audio
    if G.at(RUMBLE_AT) + len(x) != L:
        raise ValueError('the rumble must end exactly on the loop\'s downbeat')
    buf = np.zeros((L, 2))
    Ph.place(buf, x, G.at(RUMBLE_AT))
    return buf * mute_mask(KICK_OUT)[:, None]


def pad_lane():
    """The drone held: its 4-bar loop region (exactly the loop's length) as the sampler plays it once the key has been
    down a while, read so the sweep's phase follows the loop (its cutoff lowest at bar 1, highest at bar 3)."""
    o = kit_file('DroneEmAdd9')
    a, b = o.loop
    if b - a != L:
        raise ValueError(f'the drone loops every {b - a} samples, not the context loop\'s {L}')
    return D.st(o.audio)[a + (np.arange(L) - a) % L]


def lead_lane():
    return tile(kit_file('ModBlipPhrase').audio)


def robot_lane():
    """The dry robot, its step 0 ("fill") on ROBOT_AT: the file starts vox.ROBOT_PRE before it."""
    buf = np.zeros((L, 2))
    Ph.place(buf, kit_file(vox.ROBOT_ITEM).audio, G.at(ROBOT_AT) - int(np.floor(vox.ROBOT_PRE * SR + 0.5)))
    return buf


@functools.lru_cache(maxsize=1)
def chop_parts():
    """Each chop as its own part of the chops lane (so a throw can take its own sends), its first note on its 16th,
    panned as the lane."""
    parts = []
    for step, name, _ in CHOPS:
        buf = np.zeros((L, 2))
        Ph.place(buf, kit_file(name).audio, G.at(step) - int(np.floor(vox.chop_onset(name) * SR + 0.5)))
        parts.append(centred(buf, LANES['chops'][1]))
    return tuple(parts)


def fill_lane():
    x = kit_file('StutterFill').audio
    if G.at(FILL_AT) + len(x) != L:
        raise ValueError('the stutter fill must end exactly on the loop\'s downbeat')
    buf = np.zeros((L, 2))
    Ph.place(buf, x, G.at(FILL_AT))
    return buf


LANE_SOURCES = {
    'kick': lambda: Ph.hits_lane(K, 'KickSplit', KICK, G),
    'rumble': rumble_lane,
    'snare': lambda: Ph.hits_lane(K, 'SnareGated', SNARE, G),
    'hat': hat_lane,
    'perc': lambda: Ph.hits_lane(K, 'Anvil', ANVIL, G),
    'pad': pad_lane,
    'lead': lead_lane,
    'robot': robot_lane,
    'fill': fill_lane,
}


@functools.lru_cache(maxsize=None)
def raw_lane(k, char=WET_BASS):
    """One lane, panned, before its fader (the bass in the character; the chops as the sum of their parts). Each lane
    renders only its own files, so the low-end loops and the previews need no vocal models."""
    if k == 'bass':
        return centred(bass_raw(char), LANES['bass'][1])
    if k == 'chops':
        return sum(chop_parts())
    return centred(LANE_SOURCES[k](), LANES[k][1])


@functools.lru_cache(maxsize=None)
def fader_db(k, char=WET_BASS):
    """The lane's fader: the dB that sets it to LANE_LUFS over the loop (the bass per character)."""
    return float(LANE_LUFS[k] - qa.lufs(raw_lane(k, char)))


def mixed(k, char=WET_BASS):
    """One lane after its fader and pan."""
    return raw_lane(k, char) * D.undb(fader_db(k, char))


def dipped(x, key, hz=BASS_DUCK_HZ):
    """A lane with its band over hz dipped under the vocals (sends.duck on the vocal lanes, BASS_DUCK, round the loop):
    x plus its high band (zero-phase, so in phase with x and the dip is the dip) times the gain less one, so outside the
    dips it is x exactly; the bass's dip moves nothing under about 250 Hz (the low-end loops leave it out)."""
    hi = Ph.circular(lambda u: D.bfilt(u, hz, 'highpass', order=4, zero_phase=True), x)
    g = S.duck(np.ones(3 * L), np.tile(D.mono(key), 3), **BASS_DUCK)[1][L:2 * L]
    return x + hi * (D.undb(g) - 1.0)[:, None]


@functools.lru_cache(maxsize=None)
def lanes(char):
    """Each lane as the loop hears it, after its fader and pan, the bass in the character, and the bass and the fill
    with their top bands dipped under the vocals (DIPS); the chops' parts, the same way; and the kick onsets."""
    mix = {k: mixed(k, char) for k in LANES}
    key = sum(mix[k] for k in VOCALS)
    for k, hz in DIPS.items():
        mix[k] = dipped(mix[k], key, hz)
    parts = tuple(p * D.undb(fader_db('chops', char)) for p in chop_parts())
    return mix, parts, tuple(G.at(s) for s, _ in KICK)


def send_bus(mix, parts, send):
    """What a send hears: every lane at its send level, each chop at its own (a throw) or the chops lane's."""
    j = SENDS[send]
    bus = sum(mix[k] * LANES[k][j] for k in LANES if k != 'chops' and LANES[k][j])
    for p, (_, _, own) in zip(parts, CHOPS):
        level = own[j - 2] if own and j < 4 else LANES['chops'][j]
        bus = bus + p * level
    return bus


def lane_meta(mix, char, keys=tuple(LANES)):
    meta = {}
    for k in keys:
        meta[k] = {'source': line_name(char) if k == 'bass' else LANES[k][0], 'fader_db': round(fader_db(k, char), 2),
                   'pan': LANES[k][1], **{f'{s}_send': LANES[k][j] for s, j in SENDS.items()},
                   'lufs_before_gain': round(qa.lufs(mix[k]), 2)}
    return meta


GRID = {'bpm': K.BPM, 'steps': STEPS, 'samples': L, 'swing': K.SWING, 'kick_steps': [s for s, _ in KICK]}


def parts_meta(char):
    return {'bass': {'line': line_name(char), 'pick': LINE, 'character': 'A, clean' if char == CLEAN else 'B, warm',
                     'pick_about': bass.LINE_BY_NAME[LINE].about, 'pick_why': bass.LINE_BY_NAME[LINE].why},
            'rumble': {'step': RUMBLE_AT, 'bars': 2, 'muted_from_step': KICK_OUT},
            'snare': [list(h) for h in SNARE], 'hats': {'closed': [list(h) for h in HAT_CLOSED],
                                                        'open': [list(h) for h in HAT_OPEN], 'choke': drums.CHOKE},
            'anvil': [list(h) for h in ANVIL], 'lead_steps': list(LEAD_AT), 'robot_step': ROBOT_AT,
            'chops': [{'step': s, 'chop': n} for s, n, _ in CHOPS], 'fill_step': FILL_AT}


NOT_MODELLED = ('the master chain, the engine\'s voice stealing, any insert but the bass and fill lanes\' dips under '
                'the vocals')
DIP_META = {k: {'over_hz': hz, 'under': list(VOCALS), **BASS_DUCK} for k, hz in DIPS.items()}


# ------------------------------------------------------------------ the files
def dry(char):
    mix, _, _ = lanes(char)
    meta = {'grid': GRID, 'bass': char, 'lanes': lane_meta(mix, char), 'parts': parts_meta(char),
            'dips': DIP_META, 'not_modelled': NOT_MODELLED}
    return Out(sum(mix.values()), meta=meta)


def context_dry(take):
    return dry(CLEAN)


def context_warm(take):
    return dry(WARM)


def resonator_freqs():
    return tuple(float(D.midi_hz(n)) * a for n in RESONATOR_TONES for a in K.RATIOS[:4])


@functools.lru_cache(maxsize=None)
def returns(char):
    """The three returns (after their return gains), each over two passes so its tail wraps. The bass lanes send
    nothing, so they are the same under either bass."""
    mix, parts, _ = lanes(char)
    two_pass = lambda fn, x: Ph.two_pass(fn, x, SEAM_S)  # noqa: E731
    space = two_pass(lambda u: S.space(u, K.space_rng('send'), **S.SPACE), D.mono(send_bus(mix, parts, 'space')))
    key = sum(mix[k] for k in VOCALS)
    key = np.concatenate([key, key])
    space = two_pass(lambda u: S.duck(u, key, **S.DUCK)[0], space)
    delay = S.ECHO_STEPS * K.STEP
    echo = two_pass(lambda u: S.echo(u, K.space_rng('echo'), delay, **S.ECHO), D.mono(send_bus(mix, parts, 'echo')))
    res = two_pass(lambda u: S.resonator(u, resonator_freqs(), **S.RESONATOR),
                   D.mono(send_bus(mix, parts, 'resonator')))
    return {'space': space * D.undb(S.SPACE_RETURN_DB), 'echo': echo * D.undb(S.ECHO_RETURN_DB),
            'resonator': res * D.undb(S.RESONATOR_RETURN_DB)}


def context_wet(take):
    char = WET_BASS
    mix, _, _ = lanes(char)
    ret = returns(char)
    delay = S.ECHO_STEPS * K.STEP
    space_meta = {**S.SPACE, 'return_db': S.SPACE_RETURN_DB, 'seed': [K.SPACE_SEED, 'send'], 'duck_under': 'vocals',
                  **{f'duck_{k}': v for k, v in S.DUCK.items()}}
    echo_meta = {**S.ECHO, 'delay_steps': S.ECHO_STEPS, 'delay_s': round(delay, 6), 'return_db': S.ECHO_RETURN_DB,
                 'seed': [K.SPACE_SEED, 'echo']}
    res_meta = {**S.RESONATOR, 'return_db': S.RESONATOR_RETURN_DB, 'tones_midi': list(RESONATOR_TONES),
                'ratios': list(K.RATIOS[:4]), 'stand_in': 'a 2-pole bank on one chord; the engine retunes per chord'}
    meta = {'target': 'the target the engine must match', 'grid': GRID, 'bass': char,
            'bass_why': "the designers' pick, AcidLine1 (the rolling hook), in B, warm: the integrator's pick of the "
                        'character (the bass designer named the line, not the character), the body round 1 missed in '
                        'the hollow reese; the A/B is the listener\'s call',
            'lanes': lane_meta(mix, char), 'parts': parts_meta(char), 'dips': DIP_META,
            'throws': [{'step': s, 'chop': n, 'space_send': t[0], 'echo_send': t[1]} for s, n, t in CHOPS if t],
            'space': space_meta, 'echo': echo_meta, 'resonator': res_meta,
            'return_lufs': {k: round(qa.lufs(v), 2) for k, v in ret.items()},
            'not_modelled': NOT_MODELLED + ", the resonator's retuning"}
    return Out(sum(mix.values()) + sum(ret.values()), meta=meta)


def low(char):
    """The kick, bass and rumble lanes as the full loops mix them, but without the bass's dip under the vocals (over
    500 Hz, which the low-end rules don't read): so the low-end loops need no vocal models."""
    mix = {k: mixed(k, char) for k in LOW}
    meta = {'grid': GRID, 'bass': char, 'lanes': lane_meta(mix, char, LOW),
            'parts': {k: parts_meta(char)[k] for k in ('bass', 'rumble')},
            'not_modelled': "the bass lane's dip under the vocals (over 500 Hz), which the full loops have"}
    return Out(sum(mix[k] for k in LOW), hits=tuple(G.at(s) for s, _ in KICK), meta=meta)


def context_low(take):
    return low(CLEAN)


def context_low_warm(take):
    return low(WARM)


# The full loops carry vocals, snare, drone and anvil in 150-400 Hz by design, so the low-end rules run on their low end
# alone (ContextLoopLow, ContextLoopLowWarm); every other lane comes from an item that passed its own low cut.
ABOUT = '4-bar context loop, 136 BPM, E minor, bassline AcidLine1 (the rolling hook)'
FULL_MIX = {'mud': 'off', 'why': 'the full mix: vocals, snare, drone and anvil belong in 150-400 Hz; its low end is '
                                 'checked as ContextLoopLow (the gate) and ContextLoopLowWarm (bass-body), and every '
                                 'other lane passed its own low cut'}
ITEMS = [
    Item('Loops', 'ContextLoop', 'loop', context_dry, loop=True, level=LOOP_LUFS, sets=('r2',), tags=('dry',),
         qa=FULL_MIX, about=f'{ABOUT}, dry, bass A (clean): the round 2 files as the sampler plays them'),
    Item('Loops', 'ContextLoopWarm', 'loop', context_warm, loop=True, level=LOOP_LUFS, sets=('r2',), tags=('dry',),
         qa=FULL_MIX, about=f'{ABOUT}, dry, bass B (warm): the same loop, its bass lane at the same loudness'),
    Item('Loops', 'ContextLoopWet', 'loop', context_wet, loop=True, level=LOOP_LUFS, sets=('r2',), tags=('wet',),
         qa=FULL_MIX,
         about=f'{ABOUT}, wet, bass B (warm, the integrator\'s pick): Space (FDN, a little shimmer, the dark concrete '
               'seed), Echo (tape ping-pong) and the resonator on the iron ratios, a throw on the last chop; the '
               'engine\'s target'),
    Item('Loops', 'ContextLoopLow', 'loop', context_low, loop=True, sets=('r2',), tags=('with_kick',),
         qa={'mud': 'gate', 'why': 'the kick and bass A lanes (acid line and rumble) as mixed: gated over the loop and '
                                   'in every gap between its kicks'},
         about=f'{ABOUT}: the kick and bass A (clean) lanes alone, with the rumble, mud-gated in every gap between '
               'kicks'),
    Item('Loops', 'ContextLoopLowWarm', 'loop', context_low_warm, loop=True, sets=('r2',), tags=('with_kick',),
         qa={'mud': 'bass-body', 'why': 'B, warm: the proposed bass-body rule (ADR 0011) on the kick and bass B lanes '
                                       '(acid line and rumble) as mixed, in every gap between the kicks the loop '
                                       'holds'},
         about=f'{ABOUT}: the kick and bass B (warm) lanes alone, with the rumble, under the bass-body rule in every '
               'gap'),
]
