"""FUT03's context loop, round 4: 4 bars of garage-house and rolling deep tech house at 128 BPM in G minor, written for
the kit and played on its sounds, so each sound is heard where it will live. Mixed on buses and mastered through the
shared club master (tools/sound/master.py, ADR 0014) at the genre's settings: -8.5 LUFS-I, a PLR of at least 7 dB,
with the kit's cap on peak reduction, 4.5 dB (the standard's 4, raised and declared: MASTER_REDUCTION_DB).

- ContextLoopClub: the wet loop through the club master: the loop as a club record plays it (tag club: QA checks it
  against the genre's targets, qa.CLUB). Its metadata holds the master's report.
- ContextLoop: dry, with bass A (clean, the strict gate): the kit's files exactly as written, played as the sampler
  will (hits on the 16th grid on the kit's one swing, K.SWING 0.56; the nearest velocity layer; round robins in turn;
  the closed hat choking the open one), a fader and a pan per lane, the inserts and the buses; no sends, no master.
- ContextLoopWarm: the same loop with bass B (warm, the proposed bass-body rule, ADR 0011); dry A plays at its
  loudness (DRY_A_LUFS), so the A/B is the character alone.
- ContextLoopWet: bass B (the designer's pick) plus the kit's sends: Space, a 1.8 s plate, ducked under the vocals;
  Echo, a 3/16 dub ping-pong (the stab's dub delay); both returns pumped with the music bus.
- ContextLoopLow / ContextLoopLowWarm: the kick and the bass (A, or B) alone as the loop plays them, mud-gated (A) or
  under bass-body (B) in every gap between the kicks (tag with_kick).
- DrumLoop: 2 bars of the drums and perc through the drum bus. FillPreview: a bar of the groove, then the build fill
  under the drums on beats 1-3 into the downbeat.

The arrangement (16ths 0-63; odd 16ths on the swing). Rolling and hypnotic: the same two bars twice, the vocal answering
in the second half.
- Kick on every beat, every hit on the top layer, the round robins alternating.
- ClapSnare on 2 and 4; a ghost of it on the last "a" of bars 2 and 4.
- Hats: per beat a closed hat on the beat (soft) and on the swung "e", the open hat on the offbeat "and" choked by the
  closed one on the "a"; beat 4 of each bar lets the open hat ring. The shaker on every 16th, the swung "e" and "a"
  leading.
- Perc: the congas, bongos and rims of groove.PERC (2 bars) twice, ghosts on the swung 16ths.
- Bass: the hook, LineRubber (B in the wet loop), twice.
- The dub stab: DubStabGm9 off the kicks and the bass's accents (chords.HITS), twice, pumped 7 dB, through a low-pass
  that opens over the 4 bars and closes again, the dub echo on it.
- The pad bed: PadWarm, the whole 4 bars, pumped 4 dB: the density under the short parts.
- The vocal hook (VoxHook) in bars 3-4; ear candy in bars 1-2: "ride" a fifth up on the "and" of beat 4 of bar 1, the
  "cur-" stutter on beat 4 of bar 2 into the hook, the reversed "me" swelling into the loop's downbeat.
Everything loops: hits and tails wrap, the pump is circular and the sends run over two passes, keeping the second.

Rendering needs the vocal models (the hook and the candy): see tools/sound/README.md. Round 5's loops, without them,
are recipes/fut03/context5.py; the low-end loops, the drum loop and the fill preview carry on into round 5 as they are.
"""
import functools

import numpy as np

from tools.sound import dsp48 as D
from tools.sound import master as MS
from tools.sound import mixmeter as MM
from tools.sound import phrase as Ph
from tools.sound import qa
from tools.sound import sends as S
from tools.sound.item import Item, Out
from tools.sound.kits import fut03 as K
from tools.sound.recipes.fut03 import chords, groove, hook, rubber

SR = D.SR
STEPS = 64
L = Ph.loop_len(K, STEPS)           # 360000 samples, 7.5 s
G = Ph.Grid(K, STEPS)
SEAM_S = 0.02
DC_HZ = 10.0
BAR = 16
SETS = ('r2', 'r3', 'r4')           # the loops' names since round 2; round 4 rebuilt what they play

# ------------------------------------------------------------------ the parts: (16th, velocity) unless noted
KICK = tuple((s, 1.0) for s in range(0, STEPS, 4))
CLAP = ((4, 1.0), (12, 1.0), (20, 1.0), (28, 1.0), (31, 0.3), (36, 1.0), (44, 1.0), (52, 1.0), (60, 1.0),
        (63, 0.3))
LET_RING = 3


def hat_hits(bars=4):
    """(16th, 'closed' or 'open', velocity): per beat a soft closed hat on the beat and a louder one on the swung "e",
    the open hat on the "and", a closed hat on the "a" that chokes it, except on beat LET_RING of each bar."""
    hits = []
    for b in range(4 * bars):
        s = 4 * b
        hits += [(s, 'closed', 0.35), (s + 1, 'closed', 0.6 if b % 2 else 0.5), (s + 2, 'open', 0.8)]
        if b % 4 != LET_RING:
            hits.append((s + 3, 'closed', 0.45))
    return tuple(hits)


HATS = hat_hits()
SHAKER_BEAT = (0.4, 0.8, 0.6, 1.0)


def shaker_hits(bars=4):
    return tuple((s, SHAKER_BEAT[s % 4]) for s in range(16 * bars))


SHAKER = shaker_hits()
PERC = tuple(groove.PERC) + tuple((s + 32, n, z, v) for s, n, z, v in groove.PERC)
STAB = tuple((s + 32 * p, v) for p in range(2) for s, v in chords.HITS)
HOOK_AT = 32                        # the hook's 2 bars: bars 3-4
CANDY = ((14, 'CandyRideFifth'), (28, 'CandyCurStutter'))   # (16th, item): placed on their notes
REVERSE = 'CandyMeReverse'          # ends on the loop's downbeat
CANDY_LEAD = {'CandyRideFifth': ('ChopRide', 7.0), 'CandyCurStutter': ('ChopCurrent', 0.0)}

# ------------------------------------------------------------------ the mix: source, fader dB, pan, space, echo sends
LINE = {'clean': f'Line{rubber.PICK}Clean', 'warm': f'Line{rubber.PICK}Warm'}
BASS_PICK = 'warm'                  # the designer's pick for the wet loop and the club master: B's warm middle
LANES = {
    'kick': ('KickDeep', 0.0, 0.0, 0.0, 0.0),
    'bass': (None, None, 0.0, 0.0, 0.0),
    'clap': ('ClapSnare', -1.0, 0.0, 0.14, 0.0),
    'hat': ('ClosedHatDeep, OpenHatDeep', -5.0, 0.15, 0.04, 0.0),
    'shaker': ('ShakerDeep', -9.0, -0.2, 0.04, 0.0),
    'perc': ('Conga, Bongo, Rimshot', -5.0, 0.0, 0.10, 0.06),
    'stab': ('DubStabGm9', -3.0, 0.0, 0.30, 0.50),
    'pad': ('PadWarm', 2.0, 0.0, 0.10, 0.0),
    'hook': ('VoxHook', -1.0, 0.0, 0.30, 0.18),
    'candy': ('CandyRideFifth, CandyCurStutter, CandyMeReverse', -4.0, 0.0, 0.25, 0.30),
}
BASS_FADER = {'clean': 0.47, 'warm': 0.0}   # dB: the two lines level-matched in the loop. At their own peaks B reads
                                            # 0.47 LU over A (-15.39 against -15.86 LUFS-I), so A comes up to B and
                                            # B, the wet loop's and the club master's, plays as written
SENDS = {'space': 3, 'echo': 4}
LOW = ('kick', 'bass')
VOCALS = ('hook', 'candy')
PUMP = {'stab': dict(depth_db=7.0, ramp=0.003, hold=0.020, release=0.200),
        'pad': dict(depth_db=4.0, ramp=0.005, hold=0.020, release=0.250)}
RETURN_PUMP = dict(depth_db=4.0, ramp=0.005, hold=0.020, release=0.250)
STAB_RIDE = (900.0, 5000.0)         # the stab's low-pass: closed at the loop's start, open at its middle

# ------------------------------------------------------------------ the kit's sends
SPACE = {**S.SPACE, 't60': 1.8, 'size': 0.6, 'damp_hz': 9000.0, 'predelay': K.STEP / 4, 'send_hp': 500.0,
         'low_hz': 600.0, 't60_low': 0.6, 'shimmer': 0.0}
ECHO_STEPS = 3                      # the dub delay: a dotted 8th
ECHO = {**S.ECHO, 'feedback': 0.45, 'hp_hz': 400.0, 'lp_hz': 4200.0}

# ------------------------------------------------------------------ the buses
DRUM_BUS = ('kick', 'clap', 'hat', 'shaker', 'perc')
BASS_BUS = ('bass',)
DRUM_CLIP_DB = 0.0
DRUM_KNEE_DB = 1.5
MUSIC_EQ = ((330.0, -2.0, 1.0), (950.0, -2.5, 1.0), (2800.0, 4.5, 0.8))   # (Hz, dB, Q): out of the box band, under
VOCAL_EQ = MUSIC_EQ                 # 1 kHz (where the stab, the pad and the hook all sit) and presence: measured on the
                                    # club master against the genre's octave windows (2 and 4 kHz were 1-1.1 dB under)
VOX_COMP = dict(gr_db=5.0, ratio=4.0, knee_db=6.0, attack_s=0.005, release_s=(0.080, 0.500))
VOX_MAKEUP_DB = {'hook': 2.5, 'candy': 2.0}
KICK_ROOM = dict(depth_db=3.0, ramp=0.002, hold=0.015, release=0.100)
ROOMED = ('hook', 'candy', 'perc')


# ------------------------------------------------------------------ playing the kit's files
def layer_of(item, v):
    return 1 + min(range(len(item.layers)), key=lambda i: (abs(item.layers[i] - v), -i))


def kicks(grid=G, hits=KICK):
    return tuple(grid.at(s) for s, _ in hits)


def hat_lane(grid=G, hits=HATS):
    """The hats on the grid, each open hat faded out over the choke's 4 ms at the next closed hat."""
    y = np.zeros((grid.length, 2))
    fade_n = int(groove.CHOKE['fade_s'] * SR)
    closed_at = sorted(grid.at(s) for s, kind, _ in hits if kind == 'closed')
    count = {'closed': 0, 'open': 0}
    for step, kind, vel in hits:
        item, files, _ = Ph.kit_files(K, 'ClosedHatDeep' if kind == 'closed' else 'OpenHatDeep')
        x = np.array(D.st(files[(None, layer_of(item, vel), 1 + count[kind] % item.rr)].audio))
        count[kind] += 1
        pos = grid.at(step)
        if kind == 'open':
            nxt = min((c for c in closed_at if c > pos), default=closed_at[0] + grid.length) - pos
            g = np.ones(len(x))
            k = len(g[nxt:nxt + fade_n])
            g[nxt:nxt + k] = np.cos(0.5 * np.pi * np.arange(k) / fade_n) ** 2
            g[nxt + fade_n:] = 0.0
            x = x * g[:, None]
        Ph.place(y, x, pos)
    return y


def file_lane(name, at):
    """A one-file item (a loop, or a one-shot) placed at a sample position, its tail wrapping round the loop."""
    _, files, _ = Ph.kit_files(K, name)
    y = np.zeros((L, 2))
    Ph.place(y, D.st(files[(None, 1, 1)].audio), at)
    return y


def bass_lane(char):
    _, files, _ = Ph.kit_files(K, LINE[char])
    x = D.st(files[(None, 1, 1)].audio)
    return np.concatenate([x, x])


def pump(**kw):
    return D.undb(Ph.pump_db(kicks(), L, **kw))[:, None]


def stab_lane():
    """The stab off the kicks, through its low-pass ride (round the loop) and the pump."""
    y = Ph.hits_lane(K, 'DubStabGm9', STAB, G)
    u = np.arange(L) / L
    fc = STAB_RIDE[0] * (STAB_RIDE[1] / STAB_RIDE[0]) ** (0.5 - 0.5 * np.cos(2 * np.pi * u))
    return chords.lp_ride(y, fc) * pump(**PUMP['stab'])


def candy_lane():
    """The ear candy: each chop on its 16th, placed by its note; the reversed swell ending on the loop's downbeat."""
    y = np.zeros((L, 2))
    for step, name in CANDY:
        _, files, _ = Ph.kit_files(K, name)
        chop, semis = CANDY_LEAD[name]
        onset = hook.chop_audio(chop, semis)[1]
        lead = onset if name == 'CandyRideFifth' else min(onset, 0.02)   # the stutter's file starts 20 ms before
        Ph.place(y, D.st(files[(None, 1, 1)].audio), G.at(step) - int(np.floor(lead * SR + 0.5)))
    _, files, _ = Ph.kit_files(K, REVERSE)
    x = D.st(files[(None, 1, 1)].audio)
    Ph.place(y, x, (L - len(x)) % L)
    return y


RAW = {
    'kick': lambda: Ph.hits_lane(K, 'KickDeep', KICK, G),
    'clap': lambda: Ph.hits_lane(K, 'ClapSnare', CLAP, G),
    'hat': hat_lane,
    'shaker': lambda: Ph.hits_lane(K, 'ShakerDeep', SHAKER, G),
    'perc': lambda: groove.perc_hits(PERC, G, L),
    'stab': stab_lane,
    'pad': lambda: file_lane('PadWarm', 0) * pump(**PUMP['pad']),
    'hook': lambda: file_lane('VoxHook', G.at(HOOK_AT)),
    'candy': candy_lane,
}


def fader_db(key, char=None):
    return BASS_FADER[char] if key == 'bass' else LANES[key][1]


def panned(x, p):
    return D.st(x) if p == 0 else Ph.pan(x, p)


@functools.lru_cache(maxsize=None)
def lane(key, char=None):
    """One lane as the loop hears it, after its fader and pan; the vocals through the vocal bus's compressor."""
    x = bass_lane(char) if key == 'bass' else RAW[key]()
    y = panned(x, LANES[key][2]) * D.undb(fader_db(key, char))
    if key in VOCALS:
        g, _ = MS.glue(y, **VOX_COMP)
        y = g * D.undb(VOX_MAKEUP_DB[key])
    return y


def dc_clean(y):
    return Ph.circular(lambda u: D.bfilt(u, DC_HZ, 'highpass', order=4, zero_phase=True), y)


def drum_ceiling_db(lanes):
    return MM.true_peak_db(lanes['kick']) + DRUM_CLIP_DB


def eq(x, bands):
    def run(u):
        for band in bands:
            u = D.peaking(u, *band)
        return u
    return Ph.circular(run, x) if bands else x


def mixdown(lanes, ret=None):
    """The lanes through the buses, plus the returns when given, DC cleaned: the loop."""
    drums, _ = MS.soft_clip(sum(lanes[k] for k in DRUM_BUS if k in lanes), drum_ceiling_db(lanes), DRUM_KNEE_DB, 4)
    room = pump(**KICK_ROOM)
    roomed = lambda k: lanes[k] * (room if k in ROOMED else 1.0)  # noqa: E731
    music = sum((roomed(k) for k in lanes if k not in DRUM_BUS + BASS_BUS + VOCALS), np.zeros((L, 2)))
    if ret is not None:
        music = music + sum(ret.values())
    vocal = sum((roomed(k) for k in VOCALS if k in lanes), np.zeros((L, 2)))
    bass = sum((lanes[k] for k in BASS_BUS if k in lanes), np.zeros((L, 2)))
    return dc_clean(drums + bass + eq(music, MUSIC_EQ) + eq(vocal, VOCAL_EQ))


def mix(char, keys=tuple(LANES)):
    return {k: lane(k, char if k == 'bass' else None) for k in keys}


def send_bus(lanes, send):
    j = SENDS[send]
    return sum(lanes[k] * LANES[k][j] for k in LANES if LANES[k][j] and k in lanes)


def lane_meta(lanes, char=None):
    out = {}
    for k, x in lanes.items():
        src = LINE[char] if k == 'bass' else LANES[k][0]
        out[k] = {'source': src, 'fader_db': fader_db(k, char), 'pan': LANES[k][2],
                  **{f'{s}_send': LANES[k][j] for s, j in SENDS.items()}, 'lufs_before_gain': round(qa.lufs(x), 2)}
    return out


GRID = {'bpm': K.BPM, 'steps': STEPS, 'samples': L, 'swing': K.SWING, 'kick_steps': [s for s, _ in KICK]}
ARRANGEMENT = {'bass_line': f'Line{rubber.PICK}', 'hook': {'step': HOOK_AT, 'item': 'VoxHook'},
               'candy': [[s, n] for s, n in CANDY] + [['end', REVERSE]], 'stab_steps': [s for s, _ in STAB],
               'perc': [[s, n, D.note_name(z) if z else None, v] for s, n, z, v in PERC],
               'shaker': [[s, v] for s, v in SHAKER], 'clap': [list(h) for h in CLAP]}
INSERT = {'music_bus_pump': PUMP, 'keyed_to': 'kicks', 'stab_lowpass_ride_hz': list(STAB_RIDE),
          'dc': f'{DC_HZ:g} Hz zero-phase high-pass on the mix (circular); no renderer high-pass'}


def bus_meta(lanes):
    return {'drum_bus': {'lanes': list(DRUM_BUS), 'clip': 'master.soft_clip, 4x', 'ceiling_over_kick_db': DRUM_CLIP_DB,
                         'knee_db': DRUM_KNEE_DB, 'ceiling_dbfs': round(drum_ceiling_db(lanes), 3)},
            'vocal_bus': {'lanes': list(VOCALS), 'compressor': 'master.glue', **VOX_COMP, 'makeup_db': VOX_MAKEUP_DB,
                          'eq': [{'hz': f, 'db': g, 'q': q} for f, g, q in VOCAL_EQ]},
            'bass_bus': {'lanes': list(BASS_BUS)},
            'music_bus': {'lanes': [k for k in LANES if k not in DRUM_BUS + BASS_BUS + VOCALS],
                          'returns': 'in the wet loop', 'eq': [{'hz': f, 'db': g, 'q': q} for f, g, q in MUSIC_EQ]},
            'kick_room': {'lanes': list(ROOMED), **KICK_ROOM}}


def meta(lanes, char, **extra):
    return {'grid': GRID, 'bass': {'character': char, 'item': LINE[char]}, 'arrangement': ARRANGEMENT,
            'lanes': lane_meta(lanes, char), 'insert': INSERT, 'buses': bus_meta(lanes), **extra,
            'not_modelled': "the master chain (ContextLoopClub has it), the engine's other inserts"}


# ------------------------------------------------------------------ the files
def context_dry(take, char):
    lanes = mix(char)
    return Out(mixdown(lanes), meta=meta(lanes, char))


def context_clean(take):
    return context_dry(take, 'clean')


def context_warm(take):
    return context_dry(take, 'warm')


@functools.lru_cache(maxsize=None)
def returns(char):
    """The two returns, each over two passes so its tail wraps, both riding the music bus's pump."""
    lanes = mix(char)
    two = lambda fn, x: Ph.two_pass(fn, x, SEAM_S)  # noqa: E731
    space = two(lambda u: S.space(u, K.space_rng('send'), **SPACE), D.mono(send_bus(lanes, 'space')))
    key = sum(lanes[k] for k in VOCALS)
    space = two(lambda u: S.duck(u, np.concatenate([key, key]), **S.DUCK)[0], space)
    echo = two(lambda u: S.echo(u, K.space_rng('echo'), ECHO_STEPS * K.STEP, **ECHO), D.mono(send_bus(lanes, 'echo')))
    p = pump(**RETURN_PUMP)
    return {'space': space * D.undb(S.SPACE_RETURN_DB) * p, 'echo': echo * D.undb(S.ECHO_RETURN_DB) * p}


def context_wet(take):
    char = BASS_PICK
    lanes = mix(char)
    ret = returns(char)
    m = meta(lanes, char, target='the target the engine must match', bass_pick=BASS_PICK,
             space={**SPACE, 'return_db': S.SPACE_RETURN_DB, 'seed': [K.SPACE_SEED, 'send'],
                    'duck_under': list(VOCALS), 'pump': RETURN_PUMP},
             echo={**ECHO, 'delay_steps': ECHO_STEPS, 'delay_s': round(ECHO_STEPS * K.STEP, 6),
                   'return_db': S.ECHO_RETURN_DB, 'seed': [K.SPACE_SEED, 'echo'], 'pump': RETURN_PUMP},
             return_lufs={k: round(qa.lufs(v), 2) for k, v in ret.items()})
    return Out(mixdown(lanes, ret), meta=m)


# The club master's cap: the standard's 4 dB stops this loop at -9.4 to -9.6 LUFS-I, 0.1 LU inside the window's edge
# (-9.5) at best, whatever the balance (measured over 20 balances: the stab moved off the bass's accents, the pad 6 dB
# up, more glue, a peak limiter on the hook or on the bass each moved it by a tenth or two). The loop's PLR is its
# arrangement's: a kick on every beat and short, plucked parts. 4.5 dB reaches about -9.1, with the kicks and claps
# losing about 1.2 dB of crest (the standard's limit 1.5). Declared on the item; the engine's master must match it.
MASTER_REDUCTION_DB = 4.5


def club_hits():
    return {'kick': list(kicks()), 'clap': list(kicks(hits=tuple(h for h in CLAP if h[1] >= 0.8)))}


def context_club(take):
    """The wet loop through the club master at the genre's settings (MS.settings(K): -8.5 LUFS-I, a PLR of at least
    7 dB) with the kit's cap, MASTER_REDUCTION_DB, declared on the item."""
    wet = context_wet(take)
    hits = club_hits()
    z, report = MS.master(wet.audio, MS.settings(K, max_reduction_db=MASTER_REDUCTION_DB), hits=hits)
    return Out(z, top_hits=hits['kick'],
               meta={**wet.meta, 'master': report, 'target': 'the target the engine must match, through its master'})


def context_low(take, char):
    lanes = mix(char, LOW)
    return Out(dc_clean(sum(lanes.values())), hits=kicks(), top_hits=club_hits()['kick'], meta=meta(lanes, char))


def context_low_clean(take):
    return context_low(take, 'clean')


def context_low_warm(take):
    return context_low(take, 'warm')


# ------------------------------------------------------------------ the 2-bar previews
DRUM_STEPS = 32
DG = Ph.Grid(K, DRUM_STEPS)
DRUM_KICK = tuple(h for h in KICK if h[0] < DRUM_STEPS)
DRUM_CLAP = tuple(h for h in CLAP if h[0] < DRUM_STEPS)
DRUM_HATS = hat_hits(bars=2)
DRUM_SHAKER = shaker_hits(bars=2)


def drums_2bar(kick, clap, hats, shaker, perc, fill_at=None):
    fader = lambda k, x: panned(x, LANES[k][2]) * D.undb(LANES[k][1])  # noqa: E731
    parts = {'kick': fader('kick', Ph.hits_lane(K, 'KickDeep', kick, DG)),
             'clap': fader('clap', Ph.hits_lane(K, 'ClapSnare', clap, DG)), 'hat': fader('hat', hat_lane(DG, hats)),
             'shaker': fader('shaker', Ph.hits_lane(K, 'ShakerDeep', shaker, DG)),
             'perc': fader('perc', groove.perc_hits(perc, DG, DG.length))}
    y, _ = MS.soft_clip(sum(parts.values()), drum_ceiling_db(parts), DRUM_KNEE_DB, 4)
    meta_ = {'steps': DRUM_STEPS, 'bpm': K.BPM, 'swing': K.SWING, 'kick': [list(h) for h in kick],
             'clap': [list(h) for h in clap], 'hats': [list(h) for h in hats], 'shaker': [list(h) for h in shaker],
             'perc': [list(h) for h in perc],
             'drum_bus': {'clip': 'master.soft_clip, 4x', 'ceiling_over_kick_db': DRUM_CLIP_DB, 'knee_db': DRUM_KNEE_DB}}
    if fill_at is not None:
        _, files, _ = Ph.kit_files(K, 'BuildFill')
        f = np.zeros((DG.length, 2))
        Ph.place(f, files[(None, 1, 1)].audio, DG.at(fill_at))
        y = y + D.undb(FILL_FADER_DB) * f
        meta_['fill'] = {'step': fill_at, 'item': 'BuildFill', 'fader_db': FILL_FADER_DB}
    return dc_clean(y), meta_


def drum_loop(take):
    y, m = drums_2bar(DRUM_KICK, DRUM_CLAP, DRUM_HATS, DRUM_SHAKER, groove.PERC)
    return Out(y, meta=m)


FILL_FADER_DB = -4.0
FILL_KICK = tuple(h for h in KICK if h[0] < BAR) + tuple((s - 32, v) for s, v in KICK if 48 <= s < 60)
FILL_CLAP = tuple(h for h in CLAP if h[0] < BAR) + ((20, 1.0),)
FILL_HATS = tuple(h for h in HATS if h[0] < BAR) + tuple((s - 32, k, v) for s, k, v in HATS if 48 <= s < 60)
FILL_SHAKER = tuple(h for h in SHAKER if h[0] < BAR) + tuple((s - 32, v) for s, v in SHAKER if 48 <= s < 60)
FILL_PERC = tuple(h for h in groove.PERC if h[0] < BAR)


def fill_preview(take):
    y, m = drums_2bar(FILL_KICK, FILL_CLAP, FILL_HATS, FILL_SHAKER, FILL_PERC, fill_at=BAR)
    return Out(y, meta=m)


ABOUT = '4-bar context loop, 128 BPM, G minor, garage-house / deep tech house'
FULL_MIX = {'mud': 'off', 'why': 'the full mix: the clap, the congas, the stab, the pad and the vocals belong in '
                                 '150-400 Hz; its low end is gated as ContextLoopLow (A) and ContextLoopLowWarm (B), and '
                                 'every other lane passed its own low cut'}
LOOP = dict(loop=True, hp_hz=None, sets=SETS)
LOOP5 = dict(LOOP, sets=SETS + ('r5',))   # round 5 keeps these: no vocals in them (recipes/fut03/context5.py)
CLUB = dict(loop=True, hp_hz=None, sets=('r3', 'r4'))
DRY_A_LUFS = -13.05                 # dry A and B at one loudness, so the A/B is the character alone: at their own
                                    # peaks A read 0.57 LU over B (-12.48 against -13.05 LUFS-I), so A comes down to
                                    # B, which keeps its peak
CLUB_QA = {'mud': 'off', 'club_max_reduction_db': MASTER_REDUCTION_DB,
           'why': f"{FULL_MIX['why']}. The master's cap is {MASTER_REDUCTION_DB:g} dB, over the standard's 4 (at 4 dB the loop "
                  'stops 0.1 LU inside the -9.5 edge of its window): the arrangement\'s own PLR, for your decision by ear'}
ITEMS = [
    Item('Loops', 'ContextLoopClub', 'loop', context_club, tags=('wet', 'club'), qa=CLUB_QA, **CLUB,
         about=f'{ABOUT}, the wet loop through the club master (-8.5 LUFS-I, a PLR of at least 7 dB, at most '
               f'{MASTER_REDUCTION_DB:g} dB of peak reduction): the loop as a club record plays it'),
    Item('Loops', 'ContextLoop', 'loop', context_clean, tags=('dry',), qa=FULL_MIX, level=DRY_A_LUFS, **LOOP,
         about=f'{ABOUT}, dry, bass A (clean): the kit files as the sampler plays them, mixed on the buses, at dry '
               f'B\'s loudness'),
    Item('Loops', 'ContextLoopWarm', 'loop', context_warm, tags=('dry',), qa=FULL_MIX, **LOOP,
         about=f'{ABOUT}, dry, bass B (warm): the same loop with the warm line'),
    Item('Loops', 'ContextLoopWet', 'loop', context_wet, tags=('wet',), qa=FULL_MIX, **LOOP,
         about=f'{ABOUT}, wet, bass B (the pick): a 1.8 s plate and a 3/16 dub ping-pong, unmastered; the engine\'s '
               f'target'),
    Item('Loops', 'ContextLoopLow', 'loop', context_low_clean, tags=('with_kick',), **LOOP5,
         qa={'mud': 'gate', 'why': 'the kick and bass A as mixed: gated over the loop and in every gap'},
         about=f'{ABOUT}: the kick and bass A (clean) alone, mud-gated in every gap between kicks'),
    Item('Loops', 'ContextLoopLowWarm', 'loop', context_low_warm, tags=('with_kick',), **LOOP5,
         qa={'mud': 'bass-body', 'why': "B, warm: the wet loop's low end (the kick and bass B) under the proposed "
                                        'bass-body rule (ADR 0011), in every gap'},
         about=f'{ABOUT}: the kick and bass B (warm) alone, under the bass-body rule in every gap'),
    Item('Loops', 'DrumLoop', 'loop', drum_loop, tags=('preview',), **LOOP5,
         qa={'mud': 'off', 'why': "a drum preview: the clap's body and the congas belong in 150-400 Hz; the kick "
                                  'passed its own gate and groove loop, the rest their low cut'},
         about='2-bar drum preview at 128 BPM on the kit swing, through the drum bus: the deep kick, the clap and '
               'snare on 2 and 4, shuffled hats round the open offbeat hat, the shaker on the 16ths, congas and rims'),
    Item('Loops', 'FillPreview', 'loop', fill_preview, tags=('preview',), **LOOP5,
         qa={'mud': 'off', 'why': "a fill preview: the clap's body and the toms belong in 150-400 Hz; the kick passed "
                                  'its own gate, the fill, hats and shaker their low cut'},
         about='2-bar fill preview at 128 BPM: a bar of the groove, then the build fill under the drums on beats 1-3'),
]
