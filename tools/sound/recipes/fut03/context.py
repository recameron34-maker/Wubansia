"""FUT03's context loop for listening round 2: 4 bars at 128 BPM in G minor, written for this kit and played on its
round 2 sounds, so each sound is heard where it will live.

- ContextLoop: dry, with bass A (clean, the strict gate). The kit's files exactly as written, played the way the sampler
  will: hits on the 16th grid, every lane on the kit's one swing (K.SWING, 0.53), a velocity picks the nearest layer,
  round robins cycle, the closed hat chokes the open one. A fader and a pan per lane and one insert, the music bus's
  sidechain pump keyed to the kicks (the stab 9 dB, the pluck 3 dB); no master chain.
- ContextLoopWarm: the same loop with bass B and vowel bass B (warm, the proposed bass-body rule, ADR 0011).
- ContextLoopWet: the loop with the designer's pick of bass (B, warm: see BASS_PICK) plus the three shared sends in
  their Python versions (tools/sound/sends.py): Space, the 8-line FDN with a little shimmer, ducked under the vocals;
  Echo, the tape ping-pong on a dotted 8th; and the resonator stand-in, 2-pole modes on the kit's in-tune bell ratios
  (K.RATIOS[:4], a minor triad and its octave) struck from G5 and D6, so they ring Gm9's own notes. The Space and Echo
  returns ride the same pump as the music bus. The target the engine must match (S1e): its settings are in the file's
  metadata in qa_report.json.
- ContextLoopLow: the kick, bass A and vowel bass A, as they sit in the loop, mud-gated over the loop and in every gap
  between the kicks (tag with_kick). ContextLoopLowWarm is the same with bass B and vowel bass B, under the bass-body
  rule: the low end of the wet loop, checked the same way. A loop plays both basses in one character: the A loops are
  the strict gate throughout, the B loops bass-body throughout.
- DrumLoop: a 2-bar preview of the kick, the clap and the hats in the loop's pattern (bars 1-2), for listening to the
  clap and the kick in their groove. FillPreview: 2 bars, the same groove for a bar, then the loop's bar 4 (the kick
  and hats on beats 1-3 and the build fill), so the fill is heard running into the downbeat each time round.

The arrangement (16ths 0-63, bars of 16; odd 16ths on the swing):
- Kick on every beat, every hit on the top layer (the round robins alternating): the first version played beats 2 and
  4 at velocity 0.8, a whole layer down (3.4 LU), so the low end went strong-weak. The accents are the clap's and the
  hats'. It sits out beat 4 of bar 4, where the fill's 32nds run into the downbeat. Clap on 2 and 4 in bars 1-3, a
  ghost on the last 16th of bars 1-3 leading into the next; bar 4's claps are the fill's own. Hats: per beat a closed
  hat on the beat and a ghost on the "e", the open hat on the "and" choked by a closed hat on the "a"; beat 4 of each
  bar lets the open hat ring to the next beat; bar 4's beat 4 is left to the fill.
- Bass: the designer's pick, LineBounce (recipes/fut03/bass.py PICK), played twice. It rests on every kick and answers
  in the gaps: G1 on the offbeats with the octave pop, the fifth held from the "e", the seventh and back, the motif
  back varied in its second bar. In bars 1-2 its last half bar rests and the vowel bass answers instead (VowelBassTalk,
  in the loop's character, on beat 3 of bar 2, "wow-wee-yah | wow-yeah"): the second bass gesture, once. Bars 3-4 play
  the line whole, the held F2 sliding to D2 and C2 A1 leading back to G1.
- The chord: StabGm9 on the off-beat hits of the designer's 3-3-4-3-3 / 3-3-2-3-3-2 push (recipes/fut03/pad.py
  PUMP_HITS: 16ths 3 6 10 13 | 19 22 27 30), twice, pumped 9 dB by the kick (the "glass" DNA's sidechain), sitting out
  beat 4 of bar 4. The push's hits on the kick are left out: a 176 ms stab under a 9 dB pump that takes 220 ms to
  come back read 11-18 dB under the rest of the mix, swallowed.
- The lead, up front: the glass pluck's signature phrase (recipes/fut03/lead.py PHRASE) whole in bars 1-2, call and
  answer. In bars 3-4 the voice takes the first half of each bar and the pluck answers with the second: bar 3, the
  chop "ride the current" (Bb4 A4 G4 A4), then the pluck's Bb4 C5 D5 G4; bar 4, the robot "let the glass drop" ("drop"
  on beat 3, falling G3 to D3), then the pluck's C5 Bb4 A4 leaning back into the D5 on the one. The robot sits 3 dB
  and the chop 2 dB over their first faders: under the fill's first half the robot's four syllables each landed on a
  tom and clap hit, and Whisper heard neither line whole in the loop.
- The metal pling on the "and" of 3 in every bar, G5 D5 G5 D5: each strike rings its own chord in the key (G-Bb-D,
  D-F-A) in a gap of the phrase.
- The build fill in bar 4, its second half only (from its 16th 8, on beat 3, so its 8ths don't cover the robot's
  words): glass toms climbing C4 D4 F4 G4 under the kit's clap, 16ths then 32nds, ending on the downbeat. The whole
  fill is FillPreview's.
Everything loops: hits and tails wrap, the pump is circular and the sends run over two passes, keeping the second.

Rendering needs the vocal models (the chop and the robot): see tools/sound/README.md.
"""
import functools

import numpy as np

from tools.sound import dsp48 as D
from tools.sound import parts as P
from tools.sound import phrase as Ph
from tools.sound import qa
from tools.sound import sends as S
from tools.sound.item import Item, Out
from tools.sound.kits import fut03 as K
from tools.sound.recipes.fut03 import bass, drums, fx, lead, pad, vox

SR = D.SR
STEPS = 64                          # 4 bars of 16ths
L = Ph.loop_len(K, STEPS)           # 360000 samples, 7.5 s: twice the 2-bar phrases, so the line and the phrase tile
G = Ph.Grid(K, STEPS)               # the 16th grid on the kit's one swing (G.s16 = 5625 samples)
SEAM_S = 0.02                       # the sends' last 20 ms blend into what precedes the loop's start
# The renderer's sub-sonic high-pass (20 Hz, 4th order): every file the loop plays already had it once, so the loops
# skip it (Item hp_hz=None) and only the lane built from a recipe, the pluck, gets it here. Run a second time, its group
# delay at 50-60 Hz holds back each bass note's sub behind its low mids: bass B's worst gap read -0.1 dB, not -4.8.
# Instead each mix gets a zero-phase high-pass at DC_HZ (4th order each way, circular): the kicks' sub-sonic residue
# adds up over 15 hits (DC -45 dBFS without it, -93 with), and the sub keeps its level (0.000 dB) and its timing.
HP_HZ = 20.0
DC_HZ = 10.0
BAR = 16

# ------------------------------------------------------------------ the parts: (16th, velocity) unless noted
DROP_OUT = 60                       # beat 4 of bar 4: kick, hats and stab sit out, the fill's 32nds run to the one
KICK = tuple((s, 1.0) for s in range(0, STEPS, 4) if s != DROP_OUT)   # every beat on the top layer
CLAP = ((4, 1.0), (12, 0.9), (15, 0.2), (20, 1.0), (28, 0.9), (31, 0.3), (36, 1.0), (44, 0.9), (47, 0.25))
LET_RING = 3                        # the beat of each bar whose open hat rings to the next beat


def hat_hits(bars=4, stop=DROP_OUT):
    """(16th, 'closed' or 'open', velocity): per beat a closed hat on the beat and a ghost on the "e", the open hat on
    the "and" and a closed hat on the "a" that chokes it, except on beat LET_RING of each bar; nothing from `stop`."""
    hits = []
    for b in range(4 * bars):
        s = 4 * b
        if s >= stop:
            break
        hits += [(s, 'closed', 0.45 if b % 2 == 0 else 0.4), (s + 1, 'closed', 0.25), (s + 2, 'open', 0.8)]
        if b % 4 != LET_RING:
            hits.append((s + 3, 'closed', 0.55 if b % 2 else 0.5))
    return tuple(hits)


HATS = hat_hits()
PERC = ((10, 79, 0.8), (26, 74, 0.6), (42, 79, 0.8), (58, 74, 0.7))   # (16th, zone: G5 or D5, velocity)
STAB = tuple((s + 32 * p, v) for p in range(2) for s, v in pad.PUMP_HITS if s % 4 and s + 32 * p < DROP_OUT)
# the pluck: (16th, MIDI, length in 16ths, velocity), the phrase's own notes. Bars 1-2 whole; bar 3 the call's second
# half (from its 16th 8), bar 4 the answer's last notes (from its 16th 27), after the voice
PLUCK = tuple(lead.PHRASE) + tuple((s + 32, m, ln, v) for s, m, ln, v in lead.PHRASE if 8 <= s < 16) \
    + tuple((s + 32, m, ln, v) for s, m, ln, v in lead.PHRASE if s >= 27)
BASS_REST = (25, 32)                # pass 1 of the line rests over these 16ths (its notes on 25, 29 and 30)...
REST_LEAD = 0.022                   # ...from 22 ms before the first of them, where the line is about 98 dB down (its
                                    # sub swells in from 15 ms before the note; earlier, the 20 Hz high-pass's ring)
VOWEL = (24, {'clean': 'VowelBassTalkClean', 'warm': 'VowelBassTalkWarm'})   # ...and the vowel bass answers on the
                                    # kick of beat 3, bar 2, in the loop's character
CHOP = (32, 'ChopRideTheCurrent')   # bar 3, beats 1-2: "ride the current", its note on the 16th
ROBOT = (48, 'VocoderDrop')         # bar 4, beats 1-3: "let the glass drop", its step 0 on the 16th
FILL = (56, 'BuildFill', 8)         # bar 4: the fill from its 16th 8, placed on beat 3, ending on the downbeat

# ------------------------------------------------------------------ the mix: source, fader dB, pan, the three sends
LINE_NAME = 'Bounce'                # the designer's pick (bass.PICK); BASS_REST is written for its notes
assert bass.PICK == LINE_NAME, 'the context loop plays the designer\'s pick: re-place BASS_REST for the new line'
LINE = {'clean': f'Line{LINE_NAME}Clean', 'warm': f'Line{LINE_NAME}Warm'}
BASS_PICK = 'warm'                  # the designer's pick for the wet loop: see the module docstring and LISTEN.md
# The faders were set from each lane's measured loudness (LUFS-I over the loop, against the kick lane), not by ear:
# the bass and the vowel bass about 2 LU under the kick, the pluck about as loud as the bass (the lead, up front), the
# chop and the robot 1 LU under the kick (both raised after the review: under the stab, the clap and the fill they read
# 1-4 dB under the rest of the mix), the clap 5 LU and the stab 7 LU under the kick, the pling 9 and the hats 11. The
# kick's fader takes back the 1.4 LU its lane gained when every beat moved to the top layer, so every other lane keeps
# its level. The bass and vowel faders depend on the character, so A and B sit at the same loudness in the loop and the
# A/B is heard level-matched.
LANES = {
    'kick': ('KickSplit', -1.4, 0.0, 0.0, 0.0, 0.0),
    'bass': (None, None, 0.0, 0.0, 0.0, 0.0),
    'vowel': (None, None, 0.0, 0.0, 0.0, 0.0),
    'clap': ('Clap', 2.0, 0.0, 0.15, 0.0, 0.04),
    'hat': ('ClosedHat, OpenHat', -6.2, 0.2, 0.06, 0.10, 0.14),
    'perc': ('MetalPling', -4.0, -0.25, 0.55, 0.50, 0.07),
    'stab': ('StabGm9', -5.5, 0.0, 0.32, 0.0, 0.0),
    'pluck': ('GlassPluckPhrase', -2.5, 0.0, 0.32, 0.25, 0.04),
    'vox': ('ChopRideTheCurrent', 2.0, 0.0, 0.75, 0.50, 0.16),
    'robot': ('VocoderDrop', 1.8, 0.0, 0.50, 0.55, 0.0),
    'fill': ('BuildFill', -3.5, 0.0, 0.25, 0.0, 0.0),
}
BASS_FADER = {'clean': -3.05, 'warm': -3.45}   # dB: the two lines level-matched in the loop
VOWEL_FADER = {'clean': -1.25, 'warm': -3.8}    # dB: the two vowel shots level-matched in the loop
SENDS = {'space': 3, 'echo': 4, 'resonator': 5}   # column of each send in LANES
LOW = ('kick', 'bass', 'vowel')
VOCALS = ('vox', 'robot')           # the Space return ducks under these
# the music bus's sidechain pump, keyed to the kicks: down depth_db over `ramp`, held, back over `release` (raised
# cosine). The stab as in its preview (pad.PUMP); the pluck lightly, so it bounces and still leads; the two returns
PUMP = {'stab': dict(zip(('depth_db', 'ramp', 'hold', 'release'), pad.PUMP)),
        'pluck': dict(depth_db=3.0, ramp=0.003, hold=0.015, release=0.150)}
RETURN_PUMP = dict(depth_db=4.0, ramp=0.005, hold=0.020, release=0.250)
# the bass bus's sub duck on the vowel bass: it starts on a kick, and its G1 sine against the kick's G1 sub lost 3 dB
# over the first 0.2 s (the two partly cancel). Under 120 Hz it ducks out for the kick's sub and comes back as that
# fades, so the low end hands over from one to the other with no dip
SUB_DUCK = dict(depth_db=24.0, ramp=0.002, hold=0.12, release=0.10)
SUB_XOVER = 120.0
RESONATOR_TONES = (K.ROOT + 36, K.ROOT + 43)   # G5 and D6: x K.RATIOS[:4] they ring G5 Bb5 D6 G6 and D6 F6 A6 D7, the
                                               # notes of Gm9 between 0.78 and 2.4 kHz, every mode in tune and in key


# ------------------------------------------------------------------ playing the kit's files
def layer_of(item, v):
    """The velocity layer nearest v (the louder on a tie), as phrase.hits_lane picks it."""
    return 1 + min(range(len(item.layers)), key=lambda i: (abs(item.layers[i] - v), -i))


def kicks(grid=G, hits=KICK):
    return tuple(grid.at(s) for s, _ in hits)


def hat_lane(grid=G, hits=HATS):
    """The hats on the grid: each hit the layer nearest its velocity, round robins in turn per hat; each open hat
    faded out over drums.CHOKE's 4 ms at the next closed hat (the choke), its tail wrapping round the loop."""
    y = np.zeros((grid.length, 2))
    fade_n = int(drums.CHOKE['fade_s'] * SR)
    closed_at = sorted(grid.at(s) for s, kind, _ in hits if kind == 'closed')
    count = {'closed': 0, 'open': 0}
    for step, kind, vel in hits:
        item, files, _ = Ph.kit_files(K, 'ClosedHat' if kind == 'closed' else 'OpenHat')
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


def perc_lane():
    """The metal pling: each strike on its zone's files, the layer nearest its velocity, round robins in turn."""
    item, files, _ = Ph.kit_files(K, 'MetalPling')
    y = np.zeros((L, 2))
    for k, (step, note, vel) in enumerate(PERC):
        Ph.place(y, files[(note, layer_of(item, vel), 1 + k % item.rr)].audio, G.at(step))
    return y


def pluck_lane():
    """The phrase's notes on the glass pluck, as lead.pluck_phrase plays them (each note damped DAMP_AFTER after its
    length), with the renderer's 20 Hz high-pass (circular, as render.py runs it on a loop) and at the GlassPluckPhrase
    file's own gain: bars 1-2 are that file, bars 3-4 leave the voice its half bars."""
    _, _, gain_db = Ph.kit_files(K, 'GlassPluckPhrase')
    y = np.zeros((L, 2))
    n = int(1.2 * SR)
    for step, note, length, vel in PLUCK:
        x = lead.pluck(float(D.midi_hz(note)), vel, n)
        x = x * D.taper(n, length * G.s16 / SR + lead.DAMP_AFTER[0], lead.DAMP_AFTER[1])[:, None]
        Ph.place(y, x, G.at(step))
    return Ph.circular(lambda u: D.dc_block(u, HP_HZ), y) * D.undb(gain_db)


def bass_lane(char):
    """The picked line (LINE_NAME) in one character, its 2-bar file played twice; pass 1 rests over BASS_REST, where
    the vowel bass answers. The cut sits in the line's own silence (about 98 dB down), so it needs no fade."""
    _, files, _ = Ph.kit_files(K, LINE[char])
    x = D.st(files[(None, 1, 1)].audio)
    y = np.concatenate([x, x])
    a, b = BASS_REST
    y[G.at(a) - int(REST_LEAD * SR):G.at(b)] = 0.0
    return y


def vowel_lane(char):
    """The vowel bass in the loop's character on its kick, through the bus's sub duck (SUB_DUCK) under SUB_XOVER."""
    step, names = VOWEL
    item, files, _ = Ph.kit_files(K, names[char])
    y = np.zeros((L, 2))
    Ph.place(y, files[(item.notes[0], 1, 1)].audio, G.at(step))
    lo = Ph.circular(lambda u: D.lr4(u, SUB_XOVER, 'lowpass'), y)
    hi = Ph.circular(lambda u: D.lr4(u, SUB_XOVER, 'highpass'), y)
    return lo * pump(**SUB_DUCK) + hi


def one_shot(step, name, offset=0):
    """One file of an unzoned, single-layer item, placed `offset` samples before its 16th."""
    _, files, _ = Ph.kit_files(K, name)
    y = np.zeros((L, 2))
    Ph.place(y, files[(None, 1, 1)].audio, G.at(step) - offset)
    return y


FILL_FADE = 0.010                   # the fill plays from its 16th 8 as a sampler with a start offset would: from this
                                    # long before it, faded in, so the earlier hits' tails cut there don't click


def fill_lane():
    """The fill from its 16th FILL[2] (fx.swung) to its end, placed so that 16th lands on loop step FILL[0]: it ends
    on the downbeat."""
    step, name, from16 = FILL
    _, files, _ = Ph.kit_files(K, name)
    m = int(FILL_FADE * SR)
    x = D.st(files[(None, 1, 1)].audio)[fx.swung(from16) - m:]
    x = x * P.raised(len(x), FILL_FADE)[:, None]
    y = np.zeros((L, 2))
    Ph.place(y, x, G.at(step) - m)
    return y


def fill_splice():
    """Where the fill's lane starts in the loop: logged as a splice, so QA checks it for a click."""
    return G.at(FILL[0]) - int(FILL_FADE * SR)


def chop_lane():
    step, name = CHOP
    return one_shot(step, name, int(np.floor(vox.chop_onset(name) * SR + 0.5)))


def robot_lane():
    step, name = ROBOT
    return one_shot(step, name, int(np.floor(vox.ROBOT_PRE * SR + 0.5)))


def pump(**kw):
    """The pump's gain per sample, as a factor: kw (depth_db, ramp, hold, release), keyed to the loop's kicks."""
    return D.undb(Ph.pump_db(kicks(), L, **kw))[:, None]


RAW = {
    'kick': lambda: Ph.hits_lane(K, 'KickSplit', KICK, G),
    'clap': lambda: Ph.hits_lane(K, 'Clap', CLAP, G),
    'hat': hat_lane,
    'perc': perc_lane,
    'stab': lambda: Ph.hits_lane(K, 'StabGm9', STAB, G) * pump(**PUMP['stab']),
    'pluck': lambda: pluck_lane() * pump(**PUMP['pluck']),
    'vox': chop_lane,
    'robot': robot_lane,
    'fill': fill_lane,
}


@functools.lru_cache(maxsize=None)
def lane(key, char=None):
    """One lane as the loop hears it, after its fader and pan (the bass in the character `char`)."""
    x = bass_lane(char) if key == 'bass' else vowel_lane(char) if key == 'vowel' else RAW[key]()
    return panned(x, LANES[key][2]) * D.undb(fader_db(key, char))


def panned(x, p):
    """phrase.pan off centre; in the centre the lane as it is (pan's centre gains differ in the last bit, which would
    store a mono low end as two channels)."""
    return D.st(x) if p == 0 else Ph.pan(x, p)


def dc_clean(y):
    """The mix's DC and sub-sonic drift removed with no phase shift (DC_HZ, zero-phase, circular over the loop)."""
    return Ph.circular(lambda u: D.bfilt(u, DC_HZ, 'highpass', order=4, zero_phase=True), y)


def fader_db(key, char=None):
    return BASS_FADER[char] if key == 'bass' else VOWEL_FADER[char] if key == 'vowel' else LANES[key][1]


CHARACTER_LANES = ('bass', 'vowel')  # the lanes that come in the loop's character, A or B


def mix(char, keys=tuple(LANES)):
    return {k: lane(k, char if k in CHARACTER_LANES else None) for k in keys}


def send_bus(lanes, send):
    j = SENDS[send]
    return sum(lanes[k] * LANES[k][j] for k in LANES if LANES[k][j])


def lane_meta(lanes, char=None):
    out = {}
    for k, x in lanes.items():
        src = LINE[char] if k == 'bass' else VOWEL[1][char] if k == 'vowel' else LANES[k][0]
        out[k] = {'source': src, 'fader_db': fader_db(k, char), 'pan': LANES[k][2],
                  **{f'{s}_send': LANES[k][j] for s, j in SENDS.items()}, 'lufs_before_gain': round(qa.lufs(x), 2)}
    return out


GRID = {'bpm': K.BPM, 'steps': STEPS, 'samples': L, 'swing': K.SWING, 'kick_steps': [s for s, _ in KICK]}
ARRANGEMENT = {'bass_line': f'Line{LINE_NAME}', 'bass_rest_steps': list(BASS_REST),
               'vowel': {'step': VOWEL[0], 'items': VOWEL[1]}, 'chop': {'step': CHOP[0], 'item': CHOP[1]},
               'robot': {'step': ROBOT[0], 'item': ROBOT[1]},
               'fill': {'step': FILL[0], 'item': FILL[1], 'from_16th': FILL[2], 'fade_in_s': FILL_FADE},
               'drop_out_step': DROP_OUT,
               'perc': [[s, D.note_name(m), v] for s, m, v in PERC], 'stab_steps': [s for s, _ in STAB],
               'pluck': [[s, D.note_name(m), ln, v] for s, m, ln, v in PLUCK]}
INSERT = {'music_bus_pump': PUMP, 'keyed_to': 'kicks', 'vowel_sub_duck': {**SUB_DUCK, 'under_hz': SUB_XOVER},
          'dc': f'{DC_HZ:g} Hz zero-phase high-pass on the mix (circular); no renderer high-pass'}


def meta(lanes, char, **extra):
    return {'grid': GRID, 'bass': {'character': char, 'item': LINE[char], 'vowel': VOWEL[1][char]},
            'arrangement': ARRANGEMENT,
            'lanes': lane_meta(lanes, char), 'insert': INSERT, **extra,
            'not_modelled': "the master chain, the engine's other inserts, portamento (the line plays as rendered)"}


# ------------------------------------------------------------------ the files
def context_dry(take, char):
    lanes = mix(char)
    return Out(dc_clean(sum(lanes.values())), splices=(fill_splice(),), meta=meta(lanes, char))


def context_clean(take):
    return context_dry(take, 'clean')


def context_warm(take):
    return context_dry(take, 'warm')


def resonator_freqs():
    """The resonator's modes: each tone times the kit's in-tune bell ratios (K.RATIOS[:4]), a mode within 5 cents of
    one already there left out (G5 x 1.4983 is D6)."""
    out = []
    for n in RESONATOR_TONES:
        for r in K.RATIOS[:4]:
            f = float(D.midi_hz(n)) * r
            if all(abs(D.cents(f, g)) > 5.0 for g in out):
                out.append(f)
    return tuple(sorted(out))


@functools.lru_cache(maxsize=None)
def returns(char):
    """The three returns (after their return gains), each over two passes so its tail wraps; Space and Echo then ride
    the music bus's pump."""
    lanes = mix(char)
    two = lambda fn, x: Ph.two_pass(fn, x, SEAM_S)
    space = two(lambda u: S.space(u, K.space_rng('send'), **S.SPACE), D.mono(send_bus(lanes, 'space')))
    key = sum(lanes[k] for k in VOCALS)
    space = two(lambda u: S.duck(u, np.concatenate([key, key]), **S.DUCK)[0], space)
    delay = S.ECHO_STEPS * K.STEP
    echo = two(lambda u: S.echo(u, K.space_rng('echo'), delay, **S.ECHO), D.mono(send_bus(lanes, 'echo')))
    res = two(lambda u: S.resonator(u, resonator_freqs(), **S.RESONATOR), D.mono(send_bus(lanes, 'resonator')))
    p = pump(**RETURN_PUMP)
    return {'space': space * D.undb(S.SPACE_RETURN_DB) * p, 'echo': echo * D.undb(S.ECHO_RETURN_DB) * p,
            'resonator': res * D.undb(S.RESONATOR_RETURN_DB)}


def context_wet(take):
    char = BASS_PICK
    lanes = mix(char)
    ret = returns(char)
    delay = S.ECHO_STEPS * K.STEP
    space_meta = {**S.SPACE, 'return_db': S.SPACE_RETURN_DB, 'seed': [K.SPACE_SEED, 'send'],
                  'duck_under': list(VOCALS), **{f'duck_{k}': v for k, v in S.DUCK.items()}, 'pump': RETURN_PUMP}
    echo_meta = {**S.ECHO, 'delay_steps': S.ECHO_STEPS, 'delay_s': round(delay, 6), 'return_db': S.ECHO_RETURN_DB,
                 'seed': [K.SPACE_SEED, 'echo'], 'pump': RETURN_PUMP}
    res_meta = {**S.RESONATOR, 'return_db': S.RESONATOR_RETURN_DB, 'tones_midi': list(RESONATOR_TONES),
                'ratios': list(K.RATIOS[:4]), 'modes_hz': [round(f, 3) for f in resonator_freqs()],
                'stand_in': 'a 2-pole bank on one chord; the engine retunes per chord'}
    m = meta(lanes, char, target='the target the engine must match', bass_pick=BASS_PICK,
             space=space_meta, echo=echo_meta, resonator=res_meta,
             return_lufs={k: round(qa.lufs(v), 2) for k, v in ret.items()})
    return Out(dc_clean(sum(lanes.values()) + sum(ret.values())), splices=(fill_splice(),), meta=m)


def context_low(take, char):
    lanes = mix(char, LOW)
    return Out(dc_clean(sum(lanes.values())), hits=kicks(), meta=meta(lanes, char))


def context_low_clean(take):
    return context_low(take, 'clean')


def context_low_warm(take):
    return context_low(take, 'warm')


# ------------------------------------------------------------------ the 2-bar drum preview
DRUM_STEPS = 32
DG = Ph.Grid(K, DRUM_STEPS)
DRUM_KICK = tuple(h for h in KICK if h[0] < DRUM_STEPS)
DRUM_CLAP = tuple(h for h in CLAP if h[0] < DRUM_STEPS)
DRUM_HATS = hat_hits(bars=2, stop=DRUM_STEPS)


def drums_2bar(kick, clap, hats, fill_at=None):
    """Two bars of the loop's drums at their faders (and the fill on its 16th): (audio, meta)."""
    fader = lambda k, x: panned(x, LANES[k][2]) * D.undb(LANES[k][1])
    y = fader('kick', Ph.hits_lane(K, 'KickSplit', kick, DG)) + fader('clap', Ph.hits_lane(K, 'Clap', clap, DG)) \
        + fader('hat', hat_lane(DG, hats))
    meta = {'steps': DRUM_STEPS, 'bpm': K.BPM, 'swing': K.SWING, 'kick': [list(h) for h in kick],
            'clap': [list(h) for h in clap], 'hats': [list(h) for h in hats]}
    if fill_at is not None:
        _, files, _ = Ph.kit_files(K, FILL[1])
        f = np.zeros((DG.length, 2))
        Ph.place(f, files[(None, 1, 1)].audio, DG.at(fill_at))
        y = y + fader('fill', f)
        meta['fill'] = {'step': fill_at, 'item': FILL[1]}
    return dc_clean(y), meta


def drum_loop(take):
    """Bars 1-2 of the loop's drums alone: the kick, the clap and the hats."""
    y, meta = drums_2bar(DRUM_KICK, DRUM_CLAP, DRUM_HATS)
    return Out(y, meta=meta)


# the fill preview: bar 1 as the loop's bar 1, bar 2 as its bar 4 (kick and hats on beats 1-3, the fill)
FILL_KICK = tuple(h for h in KICK if h[0] < BAR) + tuple((s - 32, v) for s, v in KICK if s >= 48)
FILL_CLAP = tuple(h for h in CLAP if h[0] < BAR)
FILL_HATS = tuple(h for h in HATS if h[0] < BAR) + tuple((s - 32, k, v) for s, k, v in HATS if s >= 48)


def fill_preview(take):
    y, meta = drums_2bar(FILL_KICK, FILL_CLAP, FILL_HATS, fill_at=BAR)
    return Out(y, meta=meta)


# The full loops carry vocals, the clap, the stab and the fill in 150-400 Hz by design, so the mud gate runs on their
# low end alone (ContextLoopLow, ContextLoopLowWarm); every other lane comes from an item that passed its own low cut.
ABOUT = '4-bar context loop, 128 BPM, G minor'
FULL_MIX = {'mud': 'off', 'why': 'the full mix: vocals, clap, stab and fill belong in 150-400 Hz; its low end is gated '
                                 'as ContextLoopLow (A) and ContextLoopLowWarm (B), and every other lane passed its '
                                 'own low cut'}
LOOP = dict(loop=True, hp_hz=None, sets=('r2',))   # every file here: a loop from sample 0, no renderer high-pass
ITEMS = [
    Item('Loops', 'ContextLoop', 'loop', context_clean, tags=('dry',), qa=FULL_MIX, **LOOP,
         about=f'{ABOUT}, dry, bass A (clean, both basses): the round 2 files as the sampler plays them, the music bus '
               f'pumped'),
    Item('Loops', 'ContextLoopWarm', 'loop', context_warm, tags=('dry',), qa=FULL_MIX, **LOOP,
         about=f'{ABOUT}, dry, bass B (warm, both basses): the same loop with the warm line and vowel bass'),
    Item('Loops', 'ContextLoopWet', 'loop', context_wet, tags=('wet',), qa=FULL_MIX, **LOOP,
         about=f"{ABOUT}, wet, bass B (the pick): Space (FDN, a little shimmer), Echo (tape ping-pong) and the "
               f"resonator on the kit's bell ratios; the engine's target"),
    Item('Loops', 'ContextLoopLow', 'loop', context_low_clean, tags=('with_kick',), **LOOP,
         qa={'mud': 'gate', 'why': 'the kick, bass A and vowel bass A lanes as mixed: gated over the loop and in every '
                                   'gap'},
         about=f'{ABOUT}: the kick, bass A and vowel bass A (clean) alone, mud-gated in every gap between kicks'),
    Item('Loops', 'ContextLoopLowWarm', 'loop', context_low_warm, tags=('with_kick',), **LOOP,
         qa={'mud': 'bass-body', 'why': "B, warm: the wet loop's low end (kick, bass B, vowel bass B) under the "
                                        'proposed bass-body rule (ADR 0011), in every gap'},
         about=f'{ABOUT}: the kick, bass B and vowel bass B (warm) alone, under the bass-body rule in every gap'),
    Item('Loops', 'DrumLoop', 'loop', drum_loop, tags=('preview',), **LOOP,
         qa={'mud': 'off', 'why': "a drum preview: the clap's tail belongs in 150-400 Hz; the kick passed its own gate "
                                  'and groove loop, the clap and hats their low cut'},
         about='2-bar drum preview at 128 BPM on the kit swing: the kick four on the floor, the clap on 2 and 4 with a '
               'ghost, the hats round an open hat on the offbeats'),
    Item('Loops', 'FillPreview', 'loop', fill_preview, tags=('preview',), **LOOP,
         qa={'mud': 'off', 'why': "a fill preview: the clap's tail and the toms belong in 150-400 Hz; the kick passed "
                                  'its own gate, the fill and the hats their low cut'},
         about='2-bar fill preview at 128 BPM: a bar of the groove, then the build fill under the kick and hats on '
               'beats 1-3, running into the downbeat'),
]
