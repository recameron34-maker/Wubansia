"""FUT01's context loop for listening round 2: 4 bars at 134 BPM in F# minor, written for this kit and played on its
round 2 files, so each sound is heard where it will live.

Round 1 rated the loop Cool 4 with one fix: "Reese doesnt sound that musical and doesnt make me want to dance. The high
plucks are nice." So the reese line, which started a note on every kick, gives way to the bass designer's pick of the
three round 2 basslines (basslines.PICK: B, the Walk), played twice; the chops come from the new line, "Come find me on
the floor"; and the glitch fill is round 2's, its roll in round 1's one-beat stop. The rest is round 1's, as the user
liked it: the drum parts, the pluck phrase twice, the pad held across the seam, kick A, snare A, hat A and the alloy
hit, at round 1's faders, pans and sends.

- ContextLoop: dry, with bass A, clean (ReeseClean). The kit's files exactly as written, played the way the sampler
  will: hits on the 16th grid, every lane on the kit's one swing (K.SWING), a velocity picks the nearest layer, round
  robins cycle. A fader and a pan per lane, no master chain. The three full loops sit at one loudness (LOOP_LUFS).
- ContextLoopWarm: the same, with bass B, warm (ReeseWarm). Both bass lanes are set to the same loudness (BASS_LUFS,
  round 1's reese lane's), so the two loops differ only in the bass's character.
- ContextLoopWet: the shared sends in their Python versions (tools/sound/sends.py): Space, the 8-line FDN with a little
  octave-up shimmer, ducked under the vox; Echo, the tape ping-pong on a dotted 8th; and the stand-in of the Alloy
  resonator, on the pad's chord times the kit's ratios. The last chop ("floor") is a throw (full send) into the fill.
  With the designers' picked bass: line B, warm (the bass designer's lean; the A/B is the listener's call). The target
  the engine must match (S1e): its settings are in the file's metadata in qa_report.json.
- ContextLoopLow: the kick and bass A lanes alone, as they sit in the loop, mud-gated over the loop and in every gap
  between kicks (tag with_kick). ContextLoopLowWarm: the same with bass B under the bass-body rule (ADR 0011), so both
  characters are checked against the loop's own 4-bar kick pattern.

The parts. Over one F#m9 bar after bar (the pad), kick A on 1 and the and of 3, varied in bars 2-4 (bar 4 plays bar
2's: 1, the skip kick, the and of 3); snare A on 2 and 4 with two ghosts; hat A on the off-beats with ghosts; the alloy
hit three times; the pluck phrase twice. The bass plays the Walk twice through basslines.lane(): the sampler voice with
pitch and slides on the reese's files, the bus ducker on every kick (6 dB, held over its first 80 ms), its top keyed by
the pluck (6 dB down for each pluck note's first 40 ms, over 1 kHz, so the reese's filter opening and tine never mask
the hook's attacks), then a fader. The line leaves every kick its first 80 ms, starts nothing but ghosts on a loud
kick's tail, and answers in the gaps. No talking filter (sampler.lane_filter): round 1's formant band-passes and drive
coloured the reese it rated 2; every note opens the reese's own low-pass on its attack, so the movement is the line's.
The chops sing the line's pieces that no released song is titled (vox.CHOP_CHECKS): "find me on the floor" in bar 2,
"find" on the and of 1, "on" on the skip kick and "floor" on the and-of-3 kick, held into bar 3; and "floor" alone in
bar 4, again on the and-of-3 kick, thrown into the fill and held over it into the downbeat. The glitch fill plays its
roll and swell on beat 4 of bar 4, where the drums stop, as round 1's one-beat fill did, and ends on the downbeat.
Everything loops: hits and tails wrap, and the pad and the sends run over two passes, keeping the second.

Rendering needs the vocal models (the chops; not the low-end loops): see tools/sound/README.md.
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
from tools.sound.recipes.fut01 import basslines as B
from tools.sound.recipes.fut01 import fx, music

SR = D.SR
STEPS = 64                          # 4 bars of 16ths
L = 2 * Ph.loop_len(K, B.STEPS)     # 343880 samples: twice the 2-bar phrases, so the pluck phrase and the line tile
G = Ph.Grid(K, STEPS, L)            # the 16th grid on the kit's one swing (G.s16 samples per 16th)
SEAM_S = 0.02                       # the processed lanes' last 20 ms blend into what precedes the loop's start

# ------------------------------------------------------------------ the parts: (16th, velocity) unless noted
FILL_AT = 60                        # beat 4 of bar 4: the drums stop for the glitch fill's second beat, its roll, which
                                    # ends on the downbeat, as round 1's one-beat stop did (round 2's first take stopped
                                    # them for half a bar, kick 58 and its hats gone: 2 of every 16 beats)
FILL_FROM = 4                       # the fill's 16th the loop plays it from: the roll and the swell (GlitchFillLift
                                    # plays the whole half bar in its own file)
FILL_FADE_S = 0.002                 # the fade-in where the loop starts the fill (the swell sounds there)
KICK = ((0, 1.0), (10, 0.8), (16, 1.0), (23, 0.4), (26, 0.8), (32, 1.0), (42, 0.8), (46, 0.6), (48, 1.0), (55, 0.4),
        (58, 0.8))
SNARE = ((4, 1.0), (12, 0.8), (20, 1.0), (28, 0.8), (31, 0.2), (36, 1.0), (39, 0.2), (44, 0.8), (52, 1.0))
HAT_BAR = ((2, 0.8), (3, 0.2), (6, 1.0), (7, 0.4), (10, 0.8), (11, 0.2), (14, 1.0), (15, 0.4))
HAT = tuple((16 * b + s, v) for b in range(4) for s, v in HAT_BAR if 16 * b + s < FILL_AT) + ((29, 0.2),)
PERC = ((14, 0.75), (38, 1.0), (45, 0.5))
LINE = B.PICK                       # the bass designer's pick of the three lines: B, the Walk
BASS_LUFS = -19.0                   # the bass lane's loudness here, A and B alike (before the loop's gain): round 1's
                                    # reese lane read -19.07 in this loop, so the balance rated Cool 4 stays and only
                                    # the line and the sound change (the bassline loops play it at B.BASS_LUFS, -17)
LOOP_LUFS = -16.3                   # the three full loops at one loudness, so dry A, dry B and wet compare by sound:
                                    # just under the dry A loop's level with its peak at the ceiling (-16.0)
WET_BASS = 'warm'                   # the wet loop's bass: the bass designer's lean ("B, warm, for the body on small
                                    # speakers"); the A/B itself is the listener's call
THROW = (1.0, 1.0)                  # a throw (CC91-style): the chop's full Space and Echo send, ringing into the fill
CHOPS = ((18, 'ChopFindToFloor', None), (58, 'ChopFloor', THROW))   # the chop's first note lands on the 16th; its own
                                                                     # sends (None: the vox lane's)
PLUCK_AT = (0, 32)                  # the 2-bar phrase, twice
PLUCK_XOVER = 1000.0                # the bass lane's top, over this, ducks under every pluck attack...
PLUCK_DUCK = dict(depth_db=6.0, ramp=0.002, hold=0.038, release=0.040)   # ...6 dB for its first 40 ms: the bass's
                                                                          # filter opening and tine stay off the
                                                                          # hook's transients

# ------------------------------------------------------------------ the mix: fader dB, pan, the three sends
LANES = {
    'kick': ('KickSplit', -1.0, 0.0, 0.0, 0.0, 0.0),
    'bass': (None, None, 0.0, 0.0, 0.0, 0.0),       # its file and fader per character: bass_lane()
    'snare': ('SnareMetal', -1.7, 0.0, 0.15, 0.0, 0.0),
    'hat': ('ClosedHatFM', -3.7, 0.2, 0.06, 0.10, 0.15),
    'perc': ('AlloyHit', -5.0, -0.25, 0.70, 0.60, 0.0),
    'pad': ('AirPadFsm9', -12.0, 0.0, 0.40, 0.0, 0.0),
    'pluck': ('ChromePluckPhrase', -8.0, 0.0, 0.30, 0.25, 0.0),
    'vox': (None, -0.5, 0.0, 1.00, 0.80, 0.30),
    'fill': ('GlitchFillLift', -2.0, 0.0, 0.25, 0.25, 0.0),    # its roll's RMS above 150 Hz that of the drums on
                                                                # beat 4 of bars 1-3 (-24.1 against -24.4 dB)
}
SENDS = {'space': 3, 'echo': 4, 'resonator': 5}   # column of each send in LANES
REESE = {'clean': 'ReeseClean', 'warm': 'ReeseWarm'}
LOW = ('kick', 'bass')
SEAM_HOLD = 0.25                    # a held voice across the seam: its last 0.25 s crossfade (equal power) into the
                                    # samples before the window, so the pad never drops out at the loop point
RESONATOR_TONES = tuple(n + 24 for n in (54, 64, 69, 73, 80))   # the pad's F#m9 two octaves up: F#5 E6 A6 C#7 G#7, so
                                                                # the modes (x RATIOS[:4], 0.74-8.5 kHz) sit where hats
                                                                # and chops reach, and stay out of the low mids


# ------------------------------------------------------------------ the lanes, on the kit's files as written
def pluck_onsets():
    """Every note of the pluck phrase in the loop (it plays twice), as sample positions."""
    return [G.at(at + step) for at in PLUCK_AT for step, *_ in music.PHRASE]


@functools.lru_cache(maxsize=None)
def bass_lane(char):
    """The picked line on the 4-bar grid, for the character ('clean' or 'warm'): basslines.lane() (the sampler voice
    with pitch and slides on the reese's files, the bus ducker on every kick of this loop); its top, over PLUCK_XOVER
    (an LR4 split, so the bands sum flat), keyed by the pluck lane (PLUCK_DUCK); then a fader that sets the lane to
    BASS_LUFS, the same for A and B. Returns (lane, fader dB)."""
    raw = B.lane(LINE, char, KICK, G)
    lo, hi = (Ph.circular(lambda u, k=k: D.lr4(u, PLUCK_XOVER, k), raw) for k in ('lowpass', 'highpass'))
    raw = lo + hi * D.undb(Ph.pump_db(pluck_onsets(), L, **PLUCK_DUCK))[:, None]
    fader_db = BASS_LUFS - qa.lufs(raw)
    return raw * D.undb(fader_db), float(fader_db)


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


def vox_parts():
    """Each chop as its own part of the vox lane (so a throw can take its own sends), its first note on its 16th."""
    from tools.sound.recipes.fut01 import vox
    parts = []
    for step, name, _ in CHOPS:
        _, got, _ = Ph.kit_files(K, name)
        buf = np.zeros((L, 2))
        Ph.place(buf, got[(None, 1, 1)].audio, G.at(step) - int(np.floor(vox.chop_onset(name) * SR + 0.5)))
        parts.append(buf)
    return parts


def fill_lane():
    """The fill from its 16th FILL_FROM (its roll and the swell into the downbeat) on FILL_AT, faded in over
    FILL_FADE_S where it starts mid-swell; it ends on the loop's downbeat."""
    _, got, _ = Ph.kit_files(K, 'GlitchFillLift')
    buf = np.zeros((L, 2))
    Ph.place(buf, D.fade(got[(None, 1, 1)].audio[fx.swung(FILL_FROM):], FILL_FADE_S, 0.0), G.at(FILL_AT))
    return buf


def fader(k, x):
    """A lane's pan and fader (LANES)."""
    return Ph.pan(x, LANES[k][2]) * D.undb(LANES[k][1])


@functools.lru_cache(maxsize=1)
def kick_lane():
    """The kick lane after its fader, and the kick onsets: all the low-end loops need besides the bass."""
    return fader('kick', Ph.hits_lane(K, 'KickSplit', KICK, G)), tuple(G.at(s) for s, _ in KICK)


@functools.lru_cache(maxsize=1)
def other_lanes():
    """Every lane but the kick and the bass, after its fader and pan; and the vox lane's parts (one per chop), the
    same way."""
    parts = tuple(fader('vox', p) for p in vox_parts())
    hits = lambda name, part: Ph.hits_lane(K, name, part, G)
    raw = {'snare': hits('SnareMetal', SNARE), 'hat': hits('ClosedHatFM', HAT), 'perc': hits('AlloyHit', PERC),
           'pad': pad_lane(), 'pluck': pluck_lane(), 'fill': fill_lane()}
    out = {k: fader(k, x) for k, x in raw.items()}
    out['vox'] = sum(parts)
    return out, parts


def lanes(char):
    """Each lane as the loop hears it, in LANES order, the bass in the character ('clean' or 'warm'); the vox parts;
    the kick onsets."""
    kick, kicks = kick_lane()
    others, parts = other_lanes()
    mix = {k: kick if k == 'kick' else bass_lane(char)[0] if k == 'bass' else others[k] for k in LANES}
    return mix, parts, kicks


def send_bus(mix, parts, send):
    """What a send hears: every lane at its send level, each chop at its own (a throw) or the vox lane's."""
    j = SENDS[send]
    bus = sum(mix[k] * LANES[k][j] for k in LANES if k != 'vox' and LANES[k][j])
    for p, (_, _, own) in zip(parts, CHOPS):
        level = own[j - 3] if own and j < 5 else LANES['vox'][j]
        bus = bus + p * level
    return bus


def lane_meta(mix, char, keys=tuple(LANES)):
    meta = {}
    for k in keys:
        source = {'bass': REESE[char], 'vox': ', '.join(n for _, n, _ in CHOPS)}.get(k, LANES[k][0])
        db = bass_lane(char)[1] if k == 'bass' else LANES[k][1]
        meta[k] = {'source': source, 'fader_db': round(db, 2), 'pan': LANES[k][2],
                   **{f'{s}_send': LANES[k][j] for s, j in SENDS.items()},
                   'lufs_before_gain': round(qa.lufs(mix[k]), 2)}
    return meta


def bass_meta(char, kicks):
    """The bass lane: the line, the character, the bus ducker, and the lane alone against this loop's kicks (the duck,
    gap and before-kick readings that a file holding its kicks can't show)."""
    return {'line': f'{LINE}, {B.NAMES[LINE]}', 'character': 'A, clean' if char == 'clean' else 'B, warm',
            'source': REESE[char], 'pick_why': B.PICK_WHY, 'bass_lufs': BASS_LUFS, 'bus_ducker': B.DUCK,
            'pluck_sidechain': {'over_hz': PLUCK_XOVER, **PLUCK_DUCK, 'key': 'every pluck note'},
            'talk_filter': None, 'bass_alone_vs_kicks': B.bass_only_rows(bass_lane(char)[0], kicks)}


GRID = {'bpm': K.BPM, 'steps': STEPS, 'samples': L, 'swing': K.SWING, 'kick_steps': [s for s, _ in KICK],
        'fill_step': FILL_AT, 'fill_from_16th': FILL_FROM,
        'chops': [{'step': s, 'chop': n} for s, n, _ in CHOPS]}
NOT_MODELLED = 'the other inserts (drive, crush), the master chain'


# ------------------------------------------------------------------ the files
def context_dry(char):
    mix, _, kicks = lanes(char)
    meta = {'grid': GRID, 'lanes': lane_meta(mix, char), 'bass': bass_meta(char, kicks),
            'not_modelled': NOT_MODELLED}
    return Out(sum(mix.values()), meta=meta)


def context_dry_clean(take):
    return context_dry('clean')


def context_dry_warm(take):
    return context_dry('warm')


def resonator_freqs():
    return tuple(float(D.midi_hz(n)) * a for n in RESONATOR_TONES for a in K.RATIOS[:4])


@functools.lru_cache(maxsize=1)
def returns():
    """The three returns (after their return gains), each over two passes so its tail wraps. The bass sends nothing,
    so they are the same under either bass."""
    mix, parts, _ = lanes(WET_BASS)
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
    mix, _, kicks = lanes(WET_BASS)
    ret = returns()
    delay = S.ECHO_STEPS * K.STEP
    space_meta = {**S.SPACE, 'return_db': S.SPACE_RETURN_DB, 'seed': [K.SPACE_SEED, 'send'], 'duck_under': 'vox',
                  **{f'duck_{k}': v for k, v in S.DUCK.items()}}
    echo_meta = {**S.ECHO, 'delay_steps': S.ECHO_STEPS, 'delay_s': round(delay, 6), 'return_db': S.ECHO_RETURN_DB,
                 'seed': [K.SPACE_SEED, 'echo']}
    res_meta = {**S.RESONATOR, 'return_db': S.RESONATOR_RETURN_DB, 'tones_midi': list(RESONATOR_TONES),
                'ratios': list(K.RATIOS[:4]), 'stand_in': 'a 2-pole bank on one chord; the engine retunes per chord'}
    why = "the designers' pick: line B (the Walk), in B, warm, the bass designer's lean (body on small speakers)"
    meta = {'target': 'the target the engine must match', 'grid': GRID, 'lanes': lane_meta(mix, WET_BASS),
            'bass': {**bass_meta(WET_BASS, kicks), 'why_this_bass': why},
            'throws': [{'step': s, 'chop': n, 'space_send': t[0], 'echo_send': t[1]} for s, n, t in CHOPS if t],
            'space': space_meta, 'echo': echo_meta, 'resonator': res_meta,
            'return_lufs': {k: round(qa.lufs(v), 2) for k, v in ret.items()},
            'not_modelled': f"{NOT_MODELLED}, the resonator's retuning"}
    return Out(sum(mix.values()) + sum(ret.values()), meta=meta)


def context_low(char):
    kick, kicks = kick_lane()
    bass, _ = bass_lane(char)
    meta = {'grid': GRID, 'lanes': lane_meta({'kick': kick, 'bass': bass}, char, LOW), 'bass': bass_meta(char, kicks)}
    return Out(kick + bass, hits=kicks, meta=meta)


def context_low_clean(take):
    return context_low('clean')


def context_low_warm(take):
    return context_low('warm')


# The full loops carry vocals, snare and pad in 150-400 Hz by design, so the low-end rule runs on their low end alone
# (ContextLoopLow, ContextLoopLowWarm); every other lane comes from an item that passed its own low-cut check.
ABOUT = '4-bar context loop, 134 BPM, F# minor, bassline B (the Walk)'
FULL_MIX = {'mud': 'off', 'why': 'the full mix: vocals, snare and pad belong in 150-400 Hz; its low end is checked as '
                                 'ContextLoopLow (the gate) and ContextLoopLowWarm (bass-body), and every other lane '
                                 'passed its own low cut'}
ITEMS = [
    Item('Loops', 'ContextLoop', 'loop', context_dry_clean, loop=True, level=LOOP_LUFS, sets=('r2',), tags=('dry',),
         qa=FULL_MIX,
         about=f'{ABOUT}, dry, bass A (clean): the round 2 files as the sampler plays them'),
    Item('Loops', 'ContextLoopWarm', 'loop', context_dry_warm, loop=True, level=LOOP_LUFS, sets=('r2',), tags=('dry',),
         qa=FULL_MIX,
         about=f'{ABOUT}, dry, bass B (warm): the same loop, its bass lane at the same loudness'),
    Item('Loops', 'ContextLoopWet', 'loop', context_wet, loop=True, level=LOOP_LUFS, sets=('r2',), tags=('wet',),
         qa=FULL_MIX,
         about=f"{ABOUT}, wet, bass B (warm, the designer's lean): Space (FDN, a little shimmer), Echo (tape "
               "ping-pong) and the Alloy resonator, a throw on the last chop; the engine's target"),
    Item('Loops', 'ContextLoopLow', 'loop', context_low_clean, loop=True, sets=('r2',), tags=('with_kick',),
         qa={'mud': 'gate', 'why': 'the kick and bass A lanes as mixed: gated over the loop and in every gap'},
         about=f'{ABOUT}: the kick and bass A (clean) lanes alone, mud-gated in every gap between kicks'),
    Item('Loops', 'ContextLoopLowWarm', 'loop', context_low_warm, loop=True, sets=('r2',), tags=('with_kick',),
         qa={'mud': 'bass-body', 'why': 'B, warm: the proposed bass-body rule (ADR 0011) on the kick and bass B lanes '
                                       'as mixed, in every gap between the kicks the loop holds'},
         about=f'{ABOUT}: the kick and bass B (warm) lanes alone, under the bass-body rule in every gap'),
]
