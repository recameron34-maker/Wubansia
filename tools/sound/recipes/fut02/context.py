"""FUT02 "Orbit Relay" context loop for listening round 2: 4 bars at 130 BPM in D minor, written for this kit and
played on its round 2 files, so each sound is heard where it will live.

- ContextLoop: dry, with bass A (clean). The kit's files exactly as written, played the way the sampler will: hits on
  the 16th grid, every lane on the kit's one swing (K.SWING), a velocity picks the nearest layer, round robins cycle,
  the open hat is choked by the next closed hat, held voices loop while the key is down and release when it lifts. A
  fader and a pan per lane, one insert (a sidechain on the drop glide), the arp's automation (its rides under the
  vocals and its rest in the fill bar), no master chain.
- ContextLoopWarm: the same loop with bass B (warm), the line and the glide level-matched to A's (the same loudness on
  the bass lane), so the A/B is the low end alone.
- ContextLoopWet: the loop with the picked bass (bass.PICK, "Orbit", in its B, warm character: round 1 found the strict
  gate's hollow reese "doesnt make me want to dance", and B is the one with body on small speakers) plus the three
  sends, in their Python versions (tools/sound/sends.py): Space, the 8-line FDN with a little shimmer, ducked under the
  vocals; Echo, the tape ping-pong on a dotted 8th; and the resonator stand-in, 2-pole modes on the pad's chord two
  octaves up times the kit's stiff-wire ratios (K.RATIOS). The last chop is a throw (full send) into the fill bar. The
  target the engine must match (S1e): its settings are in the file's metadata in qa_report.json.
- ContextLoopLow: the kick and bass A lanes alone, as they sit in the loop. The strict mud gate runs on it in every
  gap between its kicks (tag with_kick). ContextLoopLowWarm is the same with bass B, under the bass-body rule. These
  two need no vocal models.

The parts (16ths 0-63, bar b starts at 16 b):
- Kick: four on the floor, every beat on the hard layer (the even pulse of a drum machine: the first render's softer
  2 and 4 were 3.3 LU down, and the clap on them led the pulse), out on beat 4 of bar 4, where the fill dives.
- Clap: 2 and 4 of bars 1-3, three ghosts on the a of a beat, 2.5 dB under its first level, so the kick leads the
  backbeat; in bar 4 the fill's own claps carry the backbeat.
- Hats: closed 16ths around an open hat on every off-beat 8th, each open hat choked by the next closed one; bar 2
  varies the a's. Bar 4 is the fill's own hat crescendo.
- Zap: four lasers on D5: a pickup in bar 1, a turnaround into bar 2, an answer to the robot, a lead-in to the fill.
- Bass: the designers' pick, the "Orbit" line (2 bars, twice). It rests on every kick and answers it, D1 on the e and
  D2 on the and, with ghosts on the a, a slide and a B-flat in bar 2. Bar 4 opens with the drop glide (the second bass
  gesture, once): struck on the kick, D2 diving to D1 over beat 1, held over beat 2, its key lifted before beat 3's
  kick; then the line returns with its B-flat on the and of beat 3 and climbs A1-C2-D2 into the loop: D1, Bb1, A1,
  C2, D2, a cadence into the downbeat.
- Pad: the starfield Dm9 held right through the loop point, its 4-bar sweep closed at bar 1 and open at bar 3.
- Lead, up front: the chrome arp phrase (2 bars, twice), the kit's signature. It makes room for the words, the
  vocal pocket: 4 dB down with its top (1.2-6.5 kHz) carved a further 8-10 dB under the robot and "pull me", its top
  carved 6 dB under "into orbit", so it sits under the vocals where the words are heard (at or under them in 1.4-5.6
  kHz) and keeps its melody. It rests in bar 4, where the fill's lasers take over its 3-3-2 rhythm (playing on, it
  rubbed a major second against the fill's A5 on step 54).
- Vocals, call and response: the robot calls "Relay online" on bar 2's downbeat, the zap answers, and the sung line
  replies across the bar line, "pull me" on beat 4 of bar 2 and "into orbit" from beat 2 of bar 3.
- Fill: the laser fill, all of bar 4, its dive landing on D5 at the loop's downbeat.
Everything loops: hits and tails wrap round the loop, the sends run over two passes, keeping the second.

Rendering needs the vocal models (the chops and the robot): see tools/sound/README.md.
"""
import functools

import numpy as np

from tools.sound import dsp48 as D
from tools.sound import phrase as Ph
from tools.sound import qa
from tools.sound import sends as S
from tools.sound.item import Item, Out
from tools.sound.kits import fut02 as K
from tools.sound.recipes.fut02 import bass, drums, pad, vox

SR = D.SR
STEPS = 64                                # 4 bars of 16ths
L = 2 * Ph.loop_len(K, bass.LINE_STEPS)   # 354462 samples: twice the 2-bar phrases, so the bass line and the arp tile
G = Ph.Grid(K, STEPS, L)                  # exactly; the same as loop_len(K, 64), and the pad's 4-bar loop
SEAM_S = 0.02                             # the sends' last 20 ms blend into what precedes the loop's start
CLEAN, WARM = bass.CLEAN, bass.WARM
WET_BASS = WARM                           # the wet loop's bass: B, warm (see the docstring)

# ------------------------------------------------------------------ the parts: (16th, velocity) unless noted
KICK = tuple((s, 1.0) for s in range(0, STEPS, 4) if s != 60)
CLAP = ((4, 1.0), (12, 1.0), (15, 0.2), (20, 1.0), (28, 1.0), (31, 0.2), (36, 1.0), (42, 0.2), (44, 1.0))
HAT_BARS = 3                              # bars 1-3; bar 4 is the fill's own hats
OPEN_BAR = ((2, 0.8), (6, 0.8), (10, 0.8), (14, 1.0))   # the off-beat 8ths
CLOSED_BAR = ((0, 0.6), (1, 0.4), (3, 0.8), (4, 0.6), (5, 0.2), (8, 0.6), (9, 0.4), (11, 0.8), (12, 0.6), (13, 0.2),
              (15, 0.8))                  # no closed hat on 7: the open hat on 6 rings a whole 8th into beat 3
CLOSED_VARY = {1: ((7, 0.4),), }          # bar 2 adds a ghost on 7 (the 6 open hat choked after a 16th)...
CLOSED_DROP = {1: (15,)}                  # ...and leaves out its last a, so the open hat on 14 rings into bar 3
HAT_OPEN = tuple((16 * b + s, v) for b in range(HAT_BARS) for s, v in OPEN_BAR)
HAT_CLOSED = tuple(sorted((16 * b + s, v) for b in range(HAT_BARS)
                          for s, v in CLOSED_BAR + CLOSED_VARY.get(b, ()) if s not in CLOSED_DROP.get(b, ())))
ZAP = ((6, 0.6), (14, 0.8), (26, 0.8), (47, 1.0))
FILL_AT = 48                              # bar 4: the laser fill, ending on the loop's downbeat
# the bass: the picked line twice, muted where the drop glide plays (from the kick at GLIDE[0] to the kick at
# GLIDE[1]; the line rests on both kicks, so the mute's edges fall in its silence)
GLIDE = (48, 56)
GLIDE_LAYER = 2                           # the hard layer: it is struck
GLIDE_EARLY = 0.060                       # its key lifts this long before the kick at GLIDE[1]...
GLIDE_RELEASE = 0.020                     # ...and it releases over this, silent 40 ms before that kick
GLIDE_DUCK = dict(depth_db=6.0, ramp=0.002, hold=0.020, release=0.120)   # the glide's own sidechain on the kicks
MUTE_EDGE = 0.005
# the vocals: (16th, item, sends) for the chops; the robot's file is placed so its step 0 lands on ROBOT_AT
THROW = (1.0, 1.0)                        # a throw (CC91-style): the chop's full Space and Echo send
ROBOT_AT = 16
CHOPS = ((28, 'ChopPullMe', None), (36, 'ChopIntoOrbit', THROW))
# the arp makes room for the words (the vocal pocket) and rests in the fill bar. A ride: (from 16th, to 16th, dB), at
# full depth from ARP_PRE before its first 16th (the robot's slices start 60 ms early, the chops' onset consonants
# before their notes) until ARP_RAMP before its last, raised-cosine ramps outside that; overlapping rides take the
# deeper. The duck is the whole lane; the carve is its top on top of that, a band (CARVE_HZ) taken out where the
# words are and nowhere else, so its melody stays
ARP_DUCK = ((16, 32, -4.0),)                                    # under the robot (16-26) and "pull me" (28-32)
ARP_CARVE = ((16, 26, -8.0), (26, 32, -10.0), (36, 48, -6.0))   # ...and "into orbit" (36-48)
CARVE_HZ = (1200.0, 6500.0)               # a 4th-order Butterworth band, zero phase: its full depth near 2.8 kHz,
                                          # 2.5-3.6 dB at the edges, 0 dB outside the rides
ARP_RAMP = 0.020
ARP_PRE = 0.080
ARP_REST = (48, 0.010, 0.003)             # resting from this 16th to the loop's end: out over 10 ms before it, back
                                          # over 3 ms on the downbeat (the F5 accent keeps its attack)

# ------------------------------------------------------------------ the mix: fader dB, pan, the three sends
# Faders are set so each lane reads, over the loop (LUFS-I, before the loop's one gain): kick -17, bass -18.5, clap
# -21 (2.5 dB under its first level: the kick leads the backbeat), hats -25, zap -25, pad -24, arp -23.5 (up front:
# where it plays alone it reads -20.5 as before, 3.5 LU under the kick, where FUT01's pluck sat 7.5 under its kick;
# its rides under the vocals and its rest in the fill bar take the other 3 LU), chops -19.5, robot -20.5, fill -19.5.
# The bass fader differs between A and B so both bass lanes read the same. The meta records every lane's reading. The
# arp's sends stay light: a dotted-8th echo on a running 16th line smears it.
LANES = {
    'kick': ('KickSplit', -2.0, 0.0, 0.0, 0.0, 0.0),
    'bass': ('OrbitLine + DropGlide', None, 0.0, 0.0, 0.0, 0.0),
    'clap': ('ElectroClap', -2.5, 0.0, 0.15, 0.0, 0.0),
    'hat': ('ClosedHat, OpenHat', -1.5, 0.15, 0.06, 0.10, 0.15),
    'zap': ('Zap', -6.0, -0.2, 0.50, 0.60, 0.20),
    'pad': ('StarfieldPadDm9', -12.1, 0.0, 0.35, 0.0, 0.0),
    'arp': ('ChromeArpPhrase', -7.6, 0.0, 0.20, 0.15, 0.10),
    'chops': (', '.join(n for _, n, _ in CHOPS), -0.7, 0.0, 0.80, 0.60, 0.30),
    'robot': ('VocoderRelay', -1.5, 0.0, 0.60, 0.50, 0.0),
    'fill': ('LaserFill', -5.7, 0.0, 0.25, 0.25, 0.0),
}
BASS_FADER = {CLEAN: -2.9, WARM: -4.15}     # dB: level-matched (see above)
SENDS = {'space': 3, 'echo': 4, 'resonator': 5}   # column of each send in LANES
VOCALS = ('chops', 'robot')               # the Space return ducks under these
LOW = ('kick', 'bass')
RESONATOR_TONES = tuple(n + 24 for n in (pad.CHORD[0],) + pad.CHORD[2:])   # the pad's Dm9 two octaves up, D5 C6 F6 A6
                                          # E7, times RATIOS[:4]: modes from 0.59 to 10.6 kHz, where the hats, zap,
                                          # arp and chops reach, out of the low mids


def layer_of(item, v):
    """The layer nearest a velocity (the louder on a tie), as phrase.hits_lane picks it."""
    return 1 + min(range(len(item.layers)), key=lambda i: (abs(item.layers[i] - v), -i))


def tile(x):
    """A 2-bar loop file twice: the 4 bars."""
    x = D.st(x)
    if 2 * len(x) != L:
        raise ValueError(f'a 2-bar loop of {len(x)} samples does not tile the {L}-sample context loop')
    return np.concatenate([x, x])


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
            if name == 'OpenHat':
                nxt = min((c - p) % n for c in closed_at if c != p)
                g = np.ones(len(x))
                m = len(g[nxt:nxt + choke_n])
                g[nxt:nxt + m] = np.cos(0.5 * np.pi * np.arange(m) / choke_n) ** 2
                g[nxt + choke_n:] = 0.0
                x *= g[:, None]
            Ph.place(buf, x, p)
    return buf


def line_name(char):
    return f"{bass.PICK}Line{'Clean' if char == CLEAN else 'Warm'}"


def glide_name(char):
    return f"DropGlide{'Clean' if char == CLEAN else 'Warm'}"


def mute_mask():
    """1 except where the glide plays, with 5 ms raised-cosine edges (the line is silent there anyway)."""
    a, b = G.at(GLIDE[0]), G.at(GLIDE[1])
    e = int(MUTE_EDGE * SR)
    g = np.ones(L)
    g[a:b] = 0.0
    w = 0.5 + 0.5 * np.cos(np.pi * np.arange(e) / e)
    g[a - e:a] = w
    g[b:b + e] = w[::-1]
    return g


def glide_lane(char):
    """The drop glide struck on the kick at GLIDE[0], its key lifted GLIDE_EARLY before the kick at GLIDE[1] (a
    raised-cosine release), through its own sidechain on the kicks it plays over."""
    _, got, _ = Ph.kit_files(K, glide_name(char))
    x = got[(None, GLIDE_LAYER, 1)].audio
    n_on = G.at(GLIDE[1]) - G.at(GLIDE[0]) - int(GLIDE_EARLY * SR)
    m = n_on + int(GLIDE_RELEASE * SR)
    x = D.st(x[:m]) * D.taper(m, n_on / SR, GLIDE_RELEASE)[:, None]
    buf = np.zeros((L, 2))
    Ph.place(buf, x, G.at(GLIDE[0]))
    duck = Ph.pump_db([G.at(s) for s, _ in KICK if GLIDE[0] <= s < GLIDE[1]], L, **GLIDE_DUCK)
    return buf * D.undb(duck)[:, None]


def bass_lane(char):
    """The picked line twice, muted under the glide, plus the glide: the bass bus before its fader."""
    _, got, _ = Ph.kit_files(K, line_name(char))
    return tile(got[(None, 1, 1)].audio) * mute_mask()[:, None] + glide_lane(char)


def pad_lane():
    """The pad held: its 4-bar loop region (exactly the loop's length) as the sampler plays it once the key has been
    down a while, read so the sweep's phase follows the loop (its cutoff lowest at bar 1, highest at bar 3)."""
    _, got, _ = Ph.kit_files(K, 'StarfieldPadDm9')
    o = got[(None, 1, 1)]
    a, b = o.loop
    if b - a != L:
        raise ValueError(f'the pad loops every {b - a} samples, not the context loop\'s {L}')
    return D.st(o.audio)[a + (np.arange(L) - a) % L]


def ride_db(rides):
    """A lane's automation in dB per sample over the loop (see ARP_DUCK)."""
    t = np.arange(L)
    r, pre = int(ARP_RAMP * SR), int(ARP_PRE * SR)
    g = np.zeros(L)
    for a, b, depth in rides:
        pa, pb = G.at(a) - pre, G.at(b) if b < STEPS else L
        u = np.clip(np.minimum((t - (pa - r)) / r, (pb - t) / r), 0.0, 1.0)
        g = np.minimum(g, depth * (0.5 - 0.5 * np.cos(np.pi * u)))
    return g


def rest_mask():
    """1 except in the fill bar, where the arp rests (ARP_REST)."""
    a, out, back = G.at(ARP_REST[0]), int(ARP_REST[1] * SR), int(ARP_REST[2] * SR)
    m = np.ones(L)
    m[a - out:a] = 0.5 + 0.5 * np.cos(np.pi * np.arange(out) / out)
    m[a:] = 0.0
    m[:back] = 0.5 - 0.5 * np.cos(np.pi * np.arange(back) / back)
    return m


def arp_lane():
    """The arp phrase twice, ducked and carved under the vocals, resting in the fill bar."""
    _, got, _ = Ph.kit_files(K, 'ChromeArpPhrase')
    x = tile(got[(None, 1, 1)].audio) * D.undb(ride_db(ARP_DUCK))[:, None]
    top = Ph.circular(lambda u: D.bfilt(u, CARVE_HZ, 'bandpass', order=2, zero_phase=True), x)
    x = x - (1 - D.undb(ride_db(ARP_CARVE)))[:, None] * top
    return x * rest_mask()[:, None]


def robot_lane():
    _, got, _ = Ph.kit_files(K, 'VocoderRelay')
    buf = np.zeros((L, 2))
    Ph.place(buf, got[(None, 1, 1)].audio, G.at(ROBOT_AT) - int(np.floor(vox.ROBOT_PRE * SR + 0.5)))
    return buf


def chop_parts():
    """Each chop as its own part of the chops lane (so a throw can take its own sends), its note on its 16th."""
    parts = []
    for step, name, _ in CHOPS:
        _, got, _ = Ph.kit_files(K, name)
        buf = np.zeros((L, 2))
        Ph.place(buf, got[(None, 1, 1)].audio, G.at(step) - int(np.floor(vox.chop_onset(name) * SR + 0.5)))
        parts.append(buf)
    return parts


def fill_lane():
    _, got, _ = Ph.kit_files(K, 'LaserFill')
    x = got[(None, 1, 1)].audio
    if G.at(FILL_AT) + len(x) != L:
        raise ValueError('the laser fill must end exactly on the loop\'s downbeat')
    buf = np.zeros((L, 2))
    Ph.place(buf, x, G.at(FILL_AT))
    return buf


def centred(x, p):
    """phrase.pan, except in the centre, where it is exactly unity: pan(x, 0) scales L and R by cos and sin of 45
    degrees, which differ in their last bit, so a mono lane would come out stereo."""
    return D.st(x) if p == 0 else Ph.pan(x, p)


def fader_db(k, char):
    return BASS_FADER[char] if k == 'bass' else LANES[k][1]


def faded(k, x, char):
    """A lane after its fader and pan (the bass's fader for character `char`)."""
    return centred(x, LANES[k][2]) * D.undb(fader_db(k, char))


def kick_onsets():
    return tuple(G.at(s) for s, _ in KICK)


@functools.lru_cache(maxsize=None)
def kick_raw():
    return Ph.hits_lane(K, 'KickSplit', KICK, G)


@functools.lru_cache(maxsize=None)
def raw_lanes():
    """Every lane but the bass before its fader (the bass has its own, per character: bass_raw); the chops as
    parts."""
    hits = lambda name, part: Ph.hits_lane(K, name, part, G)
    raw = {'kick': kick_raw(), 'clap': hits('ElectroClap', CLAP), 'hat': hat_lane(), 'zap': hits('Zap', ZAP),
           'pad': pad_lane(), 'arp': arp_lane(), 'robot': robot_lane(), 'fill': fill_lane()}
    return raw, tuple(chop_parts())


@functools.lru_cache(maxsize=None)
def bass_raw(char):
    return bass_lane(char)


def low_lanes(char):
    """The kick and bass lanes after their faders: what lanes(char) holds for them, without the other lanes (no
    vocal models needed)."""
    return {'kick': faded('kick', kick_raw(), char), 'bass': faded('bass', bass_raw(char), char)}


@functools.lru_cache(maxsize=None)
def lanes(char):
    """Each lane as the loop hears it, after its fader and pan (the bass in character `char`); the chops' parts, the
    same way; and the kick onsets."""
    raw, parts = raw_lanes()
    parts = tuple(faded('chops', p, char) for p in parts)
    out = {}
    for k in LANES:
        out[k] = sum(parts) if k == 'chops' else faded(k, bass_raw(char) if k == 'bass' else raw[k], char)
    return out, parts, kick_onsets()


def send_bus(mix, parts, send):
    """What a send hears: every lane at its send level, each chop at its own (a throw) or the chops lane's."""
    j = SENDS[send]
    bus = sum(mix[k] * LANES[k][j] for k in LANES if k != 'chops' and LANES[k][j])
    for p, (_, _, own) in zip(parts, CHOPS):
        level = own[j - 3] if own and j < 5 else LANES['chops'][j]
        bus = bus + p * level
    return bus


def lane_meta(mix, char, keys=None):
    return {k: {'source': LANES[k][0], 'fader_db': fader_db(k, char), 'pan': LANES[k][2],
                **{f'{s}_send': LANES[k][j] for s, j in SENDS.items()}, 'lufs_before_gain': round(qa.lufs(mix[k]), 2)}
            for k in (keys or LANES)}


GRID = {'bpm': K.BPM, 'steps': STEPS, 'samples': L, 'swing': K.SWING, 'kick_steps': [s for s, _ in KICK]}


def parts_meta(char):
    return {'bass': {'line': line_name(char), 'pick': bass.PICK, 'glide': glide_name(char),
                     'glide_steps': list(GLIDE), 'glide_layer': GLIDE_LAYER, 'glide_key_up_s_before_kick': GLIDE_EARLY,
                     'glide_release_s': GLIDE_RELEASE},
            'robot_step': ROBOT_AT, 'chops': [{'step': s, 'chop': n} for s, n, _ in CHOPS], 'fill_step': FILL_AT,
            'zap': list(ZAP), 'clap': list(CLAP), 'hats': {'closed': list(HAT_CLOSED), 'open': list(HAT_OPEN),
                                                           'choke_s': 0.004},
            'arp': {'duck': [list(r) for r in ARP_DUCK], 'carve': [list(r) for r in ARP_CARVE],
                    'carve_hz': list(CARVE_HZ), 'ramp_s': ARP_RAMP, 'pre_s': ARP_PRE, 'rest_from_step': ARP_REST[0],
                    'rest_fades_s': list(ARP_REST[1:])}}


INSERT = {'glide_sidechain': {**GLIDE_DUCK, 'on': 'the kicks under the drop glide'},
          'arp_rides': 'the arp lane: volume and a 1.2-6.5 kHz carve automated under the vocals, a rest in the fill '
                       'bar'}
NOT_MODELLED = 'the master chain, the engine\'s voice stealing, any insert but the glide\'s sidechain'


# ------------------------------------------------------------------ the files
def dry(char):
    mix, _, _ = lanes(char)
    meta = {'grid': GRID, 'bass': char, 'lanes': lane_meta(mix, char), 'parts': parts_meta(char), 'insert': INSERT,
            'not_modelled': NOT_MODELLED}
    return Out(sum(mix.values()), meta=meta)


def context_dry(take):
    return dry(CLEAN)


def context_warm(take):
    return dry(WARM)


def resonator_freqs():
    return tuple(float(D.midi_hz(n)) * a for n in RESONATOR_TONES for a in K.RATIOS[:4])


@functools.lru_cache(maxsize=None)
def returns(char):
    """The three returns (after their return gains), each over two passes so its tail wraps."""
    mix, parts, _ = lanes(char)
    two_pass = lambda fn, x: Ph.two_pass(fn, x, SEAM_S)
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
            'bass_why': 'the picked line in B, warm: the body round 1 missed in the hollow reese',
            'lanes': lane_meta(mix, char), 'parts': parts_meta(char), 'insert': INSERT,
            'throws': [{'step': s, 'chop': n, 'space_send': t[0], 'echo_send': t[1]} for s, n, t in CHOPS if t],
            'space': space_meta, 'echo': echo_meta, 'resonator': res_meta,
            'return_lufs': {k: round(qa.lufs(v), 2) for k, v in ret.items()},
            'not_modelled': NOT_MODELLED + ", the resonator's retuning"}
    return Out(sum(mix.values()) + sum(ret.values()), meta=meta)


def low(char):
    mix = low_lanes(char)
    meta = {'grid': GRID, 'bass': char, 'lanes': lane_meta(mix, char, LOW), 'parts': {'bass': parts_meta(char)['bass']},
            'insert': {'glide_sidechain': INSERT['glide_sidechain']}}
    return Out(sum(mix[k] for k in LOW), hits=kick_onsets(), meta=meta)


def context_low(take):
    return low(CLEAN)


def context_low_warm(take):
    return low(WARM)


# The full loops carry vocals, clap, pad and the robot in 150-400 Hz by design, so the low-end rules run on their low
# end alone (ContextLoopLow, ContextLoopLowWarm); every other lane comes from an item that passed its own low cut.
ABOUT = '4-bar context loop, 130 BPM, D minor'
FULL_MIX = {'mud': 'off', 'why': 'the full mix: vocals, clap, pad and robot belong in 150-400 Hz; its low end is gated '
                                 'as ContextLoopLow (A) and ContextLoopLowWarm (B), and every other lane passed its '
                                 'own low cut'}
ITEMS = [
    Item('Loops', 'ContextLoop', 'loop', context_dry, loop=True, sets=('r2',), tags=('dry',), qa=FULL_MIX,
         about=f'{ABOUT}, dry, bass A (clean): the round 2 files as the sampler plays them'),
    Item('Loops', 'ContextLoopWarm', 'loop', context_warm, loop=True, sets=('r2',), tags=('dry',), qa=FULL_MIX,
         about=f'{ABOUT}, dry, bass B (warm), level-matched to A'),
    Item('Loops', 'ContextLoopWet', 'loop', context_wet, loop=True, sets=('r2',), tags=('wet',), qa=FULL_MIX,
         about=f"{ABOUT}, wet, bass B (warm), the picked Orbit line: Space (FDN, a little shimmer), Echo (tape "
               "ping-pong) and the resonator on the kit ratios, a throw on the last chop; the engine's target"),
    Item('Loops', 'ContextLoopLow', 'loop', context_low, loop=True, sets=('r2',), tags=('with_kick',),
         qa={'mud': 'gate', 'why': 'the kick and bass A lanes as mixed: gated in every gap between its kicks'},
         about=f'{ABOUT}: the kick and bass A (clean) lanes alone, mud-gated in every gap between kicks'),
    Item('Loops', 'ContextLoopLowWarm', 'loop', context_low_warm, loop=True, sets=('r2',), tags=('with_kick',),
         qa={'mud': 'bass-body', 'why': f'{bass.WHY_WARM}: the kick and bass B lanes as mixed, in every gap'},
         about=f'{ABOUT}: the kick and bass B (warm) lanes alone, under the bass-body rule in every gap between kicks'),
]
