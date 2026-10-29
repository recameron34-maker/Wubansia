"""FUT01's context loop for the first listen: 4 bars at 134 BPM in F# minor, written for this kit and played on the S1a
sounds, so each sound is heard where it will live.

- ContextLoop: dry. The kit's files exactly as written, played the way the sampler will: hits on the 16th grid, every
  lane on the kit's one swing (K.SWING), a velocity picks the nearest layer, round robins cycle, and the reese plays
  through the stand-in of its lane filter (sampler.lane_filter). A fader and a pan per lane, one insert on the reese (a
  stand-in of the Bass bus: its sub ducked out under each kick's sub, its mids pumped on the kick's ticks), no master
  chain.
- ContextLoopWet: the same loop plus the three sends, in their Python versions (tools/sound/sends.py): Space, the
  8-line FDN with a little shimmer, ducked under the vox; Echo, the tape ping-pong on a dotted 8th; and a stand-in of
  the Alloy resonator on the loop's chord. The "light" chop is a throw (full send, CC91-style) into the drum stop. The
  target the engine must match (S1e): its settings are in the file's metadata in qa_report.json.
- ContextLoopLow: the kick and reese lanes alone, as they sit in the loop. The mud gate runs on it, over the loop and in
  every gap between kicks.

Over one F#m9 bar after bar (the pad, held across the seam), kick A on 1 and the and of 3, varied in bars 2-4, locks
with the reese, which starts a note on every kick and talks through CC74 (filter) and CC1 (vowel); snare A on 2 and 4
with two ghosts; hat A on the off-beats with ghosts; the alloy hit three times; the pluck phrase twice; the chops
"copper" in bar 2 and "light" in bar 4; the glitch fill on beat 4 of bar 4, where the drums stop. Everything loops:
hits and tails wrap, and the reese lane, the pad and the sends run over two passes, keeping the second.

Rendering needs the vocal models (the chops): see tools/sound/README.md.
"""
import functools

import numpy as np

from tools.sound import dsp48 as D
from tools.sound import phrase as Ph
from tools.sound import qa
from tools.sound import sampler as Sm
from tools.sound import sends as S
from tools.sound.item import Item, Out
from tools.sound.kits import fut01 as K
from tools.sound.recipes.fut01 import bass

SR = D.SR
STEPS = 64                          # 4 bars of 16ths
L = 2 * Ph.loop_len(K, bass.LOOP_STEPS)   # 343880 samples: twice the 2-bar phrases, so the pluck phrase tiles exactly
G = Ph.Grid(K, STEPS, L)            # the 16th grid on the kit's one swing (G.s16 samples per 16th)
SEAM_S = 0.02                       # the processed lanes' last 20 ms blend into what precedes the loop's start

# ------------------------------------------------------------------ the parts: (16th, velocity) unless noted
KICK = ((0, 1.0), (10, 0.8), (16, 1.0), (23, 0.4), (26, 0.8), (32, 1.0), (42, 0.8), (46, 0.6), (48, 1.0), (55, 0.4),
        (58, 0.8))
SNARE = ((4, 1.0), (12, 0.8), (20, 1.0), (28, 0.8), (31, 0.2), (36, 1.0), (39, 0.2), (44, 0.8), (52, 1.0))
HAT_BAR = ((2, 0.8), (3, 0.2), (6, 1.0), (7, 0.4), (10, 0.8), (11, 0.2), (14, 1.0), (15, 0.4))
HAT = tuple((16 * b + s, v) for b in range(4) for s, v in HAT_BAR if not (b == 3 and s >= 12)) + ((29, 0.2),)
PERC = ((14, 0.75), (38, 1.0), (45, 0.5))
FILL_AT = 60                        # beat 4 of bar 4: the drums stop for the glitch fill
# the reese: (16th, length in 16ths, MIDI note, accent); a note starts on every kick. F#1, with A1 and C#2 from the
# pad's chord; accents play the hard layer
BASS = ((0, 4, 30, 1), (7, 2, 30, 0), (10, 5, 30, 1),
        (16, 3, 30, 1), (20, 2, 37, 0), (23, 2, 30, 0), (26, 4, 33, 1), (30, 2, 30, 0),
        (32, 4, 30, 1), (39, 2, 30, 0), (42, 3, 30, 1), (46, 2, 37, 0),
        (48, 3, 30, 1), (52, 2, 33, 0), (55, 2, 37, 0), (58, 2, 30, 1))
CC74 = ((0, 44), (1, 110), (4, 52), (7, 88), (9, 48), (10, 120), (14, 60), (16, 40), (17, 104), (20, 70), (23, 96),
        (26, 127), (30, 58), (32, 46), (33, 112), (36, 54), (39, 90), (42, 122), (45, 62), (46, 100), (48, 42),
        (49, 108), (52, 76), (55, 98), (58, 127), (60, 50), (64, 44))
CC1 = ((0, 0), (4, 80), (7, 20), (10, 40), (14, 127), (16, 10), (20, 60), (23, 30), (26, 100), (30, 20), (32, 0),
       (36, 90), (39, 24), (42, 60), (45, 120), (48, 16), (52, 84), (55, 40), (58, 110), (62, 30), (64, 0))
THROW = (1.0, 1.0)                  # a throw (CC91-style): the chop's full Space and Echo send, ringing into the stop
CHOPS = ((24, 'ChopCopper', None), (56, 'ChopLight', THROW))   # the chop's note lands on the 16th; its own sends
PLUCK_AT = (0, 32)                  # the 2-bar phrase, twice

# ------------------------------------------------------------------ the mix: fader dB, pan, the three sends
LANES = {
    'kick': ('KickSplit', -1.0, 0.0, 0.0, 0.0, 0.0),
    'bass': ('Reese', -5.0, 0.0, 0.0, 0.0, 0.0),
    'snare': ('SnareMetal', -1.7, 0.0, 0.15, 0.0, 0.0),
    'hat': ('ClosedHatFM', -3.7, 0.2, 0.06, 0.10, 0.15),
    'perc': ('AlloyHit', -5.0, -0.25, 0.70, 0.60, 0.0),
    'pad': ('AirPadFsm9', -12.0, 0.0, 0.40, 0.0, 0.0),
    'pluck': ('ChromePluckPhrase', -8.0, 0.0, 0.30, 0.25, 0.0),
    'vox': (None, -0.5, 0.0, 1.00, 0.80, 0.30),
    'fill': ('GlitchFill', -4.0, 0.0, 0.25, 0.25, 0.0),
}
SENDS = {'space': 3, 'echo': 4, 'resonator': 5}   # column of each send in LANES
# the insert on the reese, a stand-in for the Bass bus (plan section 2.4), driven by the kick's ticks: down depth_db
# over `ramp`, held, then a raised-cosine back over `release`. Under 120 Hz the reese ducks out for the kick's whole sub
# (0.28 s), so the two F#1 subs never add or cancel; above it, the pump
PUMP = dict(depth_db=10.0, ramp=0.002, hold=0.020, release=0.180)
SUB_DUCK = dict(depth_db=24.0, ramp=0.002, hold=0.280, release=0.080)
SUB_XOVER = 120.0
LOW = ('kick', 'bass')
SEAM_HOLD = 0.25                    # a held voice across the seam: its last 0.25 s crossfade (equal power) into the
                                    # samples before the window, so the pad never drops out at the loop point
RESONATOR_TONES = tuple(n + 24 for n in (54, 64, 69, 73, 80))   # the pad's F#m9 two octaves up: F#5 E6 A6 C#7 G#7, so
                                                                # the modes (x ALLOY[:4], 0.74-8.5 kHz) sit where hats
                                                                # and chops reach, and stay out of the low mids


# ------------------------------------------------------------------ the lanes, on the kit's files as written
def bass_lane():
    """The reese: each note held for its length (the sampler loops it) and released, through the lane stand-in on the
    two CC lanes, then the bus insert: under SUB_XOVER ducked out for each kick's sub, above it pumped. Its drive was
    voiced on the reese before its gain (ReeseTalk), so the files are trimmed back up to that level in front of it;
    the fader comes after."""
    _, got, gain_db = Ph.kit_files(K, 'Reese')
    x = np.zeros((L, 2))
    for step, length, note, accent in BASS:
        v = Sm.sampler_voice(got[(note, 2 if accent else 1, 1)], int(round(length * G.s16)) - int(0.01 * SR),
                             bass.REESE_ATTACK)
        Ph.place(x, v, G.at(step))
    trim = D.undb(-gain_db)
    cc74, cc1 = Sm.cc_curve(CC74, L, K.STEP), Sm.cc_curve(CC1, L, K.STEP)
    lane = lambda u: Sm.lane_filter(u * trim, np.tile(cc74, 2), np.tile(cc1, 2), mids_hp=bass.MID_HP)
    y = Ph.two_pass(lane, x, SEAM_S)
    lo = Ph.circular(lambda u: D.lr4(u, SUB_XOVER, 'lowpass'), y)
    hi = Ph.circular(lambda u: D.lr4(u, SUB_XOVER, 'highpass'), y)
    return D.mono_below(lo * D.undb(pump_db(**SUB_DUCK))[:, None] + hi * D.undb(pump_db(**PUMP))[:, None], SUB_XOVER)


def pump_db(depth_db, ramp, hold, release):
    """The bus insert's ducker on the loop's kicks, in dB per sample (phrase.pump_db)."""
    return Ph.pump_db([G.at(step) for step, _ in KICK], L, depth_db, ramp, hold, release)


def pad_lane():
    """The pad held through the loop and across its seam: the voice runs two loop lengths, the second is kept, and
    its end crossfades into the moment before it, so the harmony never stops at the loop point."""
    _, got, _ = Ph.kit_files(K, 'AirPadFsm9')
    voice = Sm.sampler_voice(got[(None, 1, 1)], 2 * L, attack=0.002, release=0.01)[:2 * L]
    return Ph.second_pass(voice, SEAM_HOLD, L, power=True)


def pluck_lane():
    _, got, _ = Ph.kit_files(K, 'ChromePluckPhrase')
    buf = np.zeros((L, 2))
    for step in PLUCK_AT:
        Ph.place(buf, got[(None, 1, 1)].audio, G.at(step))
    return buf


def chop_onset(name):
    """Seconds from the start of a chop's file to its note: where the beat falls, after the onset consonant."""
    from tools.sound import vocals as V
    from tools.sound.recipes.fut01 import vox
    _, first, last, _, _ = vox.CHOP_BY_NAME[name]
    track, _ = V.chop(vox.sung_track(), first, last)
    return float(track.events[0]['t_on'])


def vox_parts():
    """Each chop as its own part of the vox lane (so a throw can take its own sends)."""
    parts = []
    for step, name, _ in CHOPS:
        _, got, _ = Ph.kit_files(K, name)
        buf = np.zeros((L, 2))
        Ph.place(buf, got[(None, 1, 1)].audio, G.at(step) - int(np.floor(chop_onset(name) * SR + 0.5)))
        parts.append(buf)
    return parts


def fill_lane():
    _, got, _ = Ph.kit_files(K, 'GlitchFill')
    buf = np.zeros((L, 2))
    Ph.place(buf, got[(None, 1, 1)].audio, G.at(FILL_AT))
    return buf


@functools.lru_cache(maxsize=1)
def lanes():
    """Each lane as the loop hears it, after its fader and pan; the vox lane's parts (one per chop), the same way;
    and the kick onsets."""
    fader = lambda k, x: Ph.pan(x, LANES[k][2]) * D.undb(LANES[k][1])
    parts = tuple(fader('vox', p) for p in vox_parts())
    hits = lambda name, part: Ph.hits_lane(K, name, part, G)
    raw = {'kick': hits('KickSplit', KICK), 'bass': bass_lane(), 'snare': hits('SnareMetal', SNARE),
           'hat': hits('ClosedHatFM', HAT), 'perc': hits('AlloyHit', PERC), 'pad': pad_lane(),
           'pluck': pluck_lane(), 'fill': fill_lane()}
    out = {k: sum(parts) if k == 'vox' else fader(k, raw[k]) for k in LANES}
    return out, parts, tuple(G.at(s) for s, _ in KICK)


def send_bus(mix, parts, send):
    """What a send hears: every lane at its send level, each chop at its own (a throw) or the vox lane's."""
    j = SENDS[send]
    bus = sum(mix[k] * LANES[k][j] for k in LANES if k != 'vox' and LANES[k][j])
    for p, (_, _, own) in zip(parts, CHOPS):
        level = own[j - 3] if own and j < 5 else LANES['vox'][j]
        bus = bus + p * level
    return bus


def lane_meta(mix):
    return {k: {'source': LANES[k][0] or ', '.join(n for _, n, _ in CHOPS), 'fader_db': LANES[k][1],
                'pan': LANES[k][2], **{f'{s}_send': LANES[k][j] for s, j in SENDS.items()},
                'lufs_before_gain': round(qa.lufs(mix[k]), 2)}
            for k in LANES}


GRID = {'bpm': K.BPM, 'steps': STEPS, 'samples': L, 'swing': K.SWING, 'kick_steps': [s for s, _ in KICK]}
INSERT = {'reese_sub_duck': {**SUB_DUCK, 'under_hz': SUB_XOVER}, 'reese_pump': PUMP}


# ------------------------------------------------------------------ the three files
def context_dry(take):
    mix, _, _ = lanes()
    meta = {'grid': GRID, 'lanes': lane_meta(mix), 'insert': INSERT,
            'not_modelled': 'the other inserts (drive, crush), the master chain, the reese glide'}
    return Out(sum(mix.values()), meta=meta)


def resonator_freqs():
    return tuple(float(D.midi_hz(n)) * a for n in RESONATOR_TONES for a in K.ALLOY[:4])


@functools.lru_cache(maxsize=1)
def returns():
    """The three returns (after their return gains), each over two passes so its tail wraps."""
    mix, parts, _ = lanes()
    two_pass = lambda fn, x: Ph.two_pass(fn, x, SEAM_S)
    space = two_pass(lambda u: S.space(u, K.space_rng('send'), **S.SPACE), D.mono(send_bus(mix, parts, 'space')))
    key = np.concatenate([mix['vox'], mix['vox']])
    space = two_pass(lambda u: S.duck(u, key, **S.DUCK)[0], space)
    delay = S.ECHO_STEPS * K.STEP
    echo = two_pass(lambda u: S.echo(u, K.space_rng('echo'), delay, **S.ECHO), D.mono(send_bus(mix, parts, 'echo')))
    res = two_pass(lambda u: S.resonator(u, resonator_freqs(), **S.RESONATOR),
                   D.mono(send_bus(mix, parts, 'resonator')))
    return {'space': space * D.undb(S.SPACE_RETURN_DB), 'echo': echo * D.undb(S.ECHO_RETURN_DB),
            'resonator': res * D.undb(S.RESONATOR_RETURN_DB)}


def context_wet(take):
    mix, _, _ = lanes()
    ret = returns()
    delay = S.ECHO_STEPS * K.STEP
    space_meta = {**S.SPACE, 'return_db': S.SPACE_RETURN_DB, 'seed': [K.SPACE_SEED, 'send'], 'duck_under': 'vox',
                  **{f'duck_{k}': v for k, v in S.DUCK.items()}}
    echo_meta = {**S.ECHO, 'delay_steps': S.ECHO_STEPS, 'delay_s': round(delay, 6), 'return_db': S.ECHO_RETURN_DB,
                 'seed': [K.SPACE_SEED, 'echo']}
    res_meta = {**S.RESONATOR, 'return_db': S.RESONATOR_RETURN_DB, 'tones_midi': list(RESONATOR_TONES),
                'ratios': list(K.ALLOY[:4]), 'stand_in': 'a 2-pole bank on one chord; the engine retunes per chord'}
    meta = {'target': 'the target the engine must match', 'grid': GRID, 'lanes': lane_meta(mix), 'insert': INSERT,
            'throws': [{'step': s, 'chop': n, 'space_send': t[0], 'echo_send': t[1]} for s, n, t in CHOPS if t],
            'space': space_meta, 'echo': echo_meta, 'resonator': res_meta,
            'return_lufs': {k: round(qa.lufs(v), 2) for k, v in ret.items()},
            'not_modelled': "the other inserts (drive, crush), the resonator's retuning, the master chain, the reese "
                            'glide'}
    return Out(sum(mix.values()) + sum(ret.values()), meta=meta)


def context_low(take):
    mix, _, kicks = lanes()
    meta = {'grid': GRID, 'lanes': {k: lane_meta(mix)[k] for k in LOW}, 'insert': INSERT}
    return Out(sum(mix[k] for k in LOW), hits=kicks, meta=meta)


# The full loops carry vocals, snare and pad in 150-400 Hz by design, so the mud gate runs on their low end alone
# (ContextLoopLow); every other lane comes from an item that passed its own low-cut check.
ABOUT = '4-bar context loop, 134 BPM, F# minor'
FULL_MIX = {'mud': 'off', 'why': 'the full mix: vocals, snare and pad belong in 150-400 Hz; its low end is gated as '
                                 'ContextLoopLow, and every other lane passed its own low cut'}
ITEMS = [
    Item('Loops', 'ContextLoop', 'loop', context_dry, loop=True, sets=('s1a',), tags=('dry',), qa=FULL_MIX,
         about=f'{ABOUT}, dry: the S1a files as the sampler plays them'),
    Item('Loops', 'ContextLoopWet', 'loop', context_wet, loop=True, sets=('s1a',), tags=('wet',), qa=FULL_MIX,
         about=f"{ABOUT}, wet: Space (FDN, a little shimmer), Echo (tape ping-pong) and the Alloy resonator, a "
               "throw on the last chop; the engine's target"),
    Item('Loops', 'ContextLoopLow', 'loop', context_low, loop=True, sets=('s1a',), tags=('with_kick',),
         qa={'mud': 'gate', 'why': 'the kick and reese lanes as mixed: gated over the loop and in every gap'},
         about=f'{ABOUT}: the kick and reese lanes alone, mud-gated in every gap between kicks'),
]
