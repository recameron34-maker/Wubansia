"""FUT01's context loop for listening round 3, the modern pass (docs/plans/S1-modern-sound.md): 4 bars at 134 BPM in
F# minor, written for this kit and played on its round 3 files, so each sound is heard where it will live, and mixed
and mastered against the standard's UK garage targets.

Round 2's loop (round 1's drums, pluck phrase and pad, the Walk bassline, the chops of "Come find me on the floor" and
the glitch fill's roll) measured as a mix, not a master: -16.2 LUFS at a peak-to-loudness ratio (PLR) of 14 dB, a
low-mid hole (the 125 and 250 Hz octaves 9-10 dB under the line from the sub to 500 Hz), the snare's backbeat 1.4 dB
over the kick, the hats 11 LU under the kick, and its low mids wide (side 5.6 dB under mid at 250 Hz). Round 3 keeps the
parts and the song (the same kick, snare, hat, pluck, pad, bass and vocal parts, the fill in the same one-beat stop) and
changes the sounds and the mix, in the standard's order of FUT01 changes:
1. Density, then a master: ContextLoopClub is the wet loop through the shared club master (master.py), aimed at the
   UKG target, -9 LUFS; the kick lane takes a soft clip of KICK_CLIP_DB, light enough that every full-velocity kick
   keeps the 5-7 dB crest round 1 liked (the round 3 review: at 4 dB it read 3.6, under ADR 0011's floor), and the
   master stops at its cap of 4 dB of peak reduction, inside the -9 +/- 1 window. The density comes from the bass, the
   genre's hook (BASS_LUFS, 1.2 LU over the kick lane).
2. The low mids filled without mud, and central: the bass lane mono under 450 Hz and narrower over it, its body lifted
   at 180 Hz; the pad mono under 350 Hz and its low mids lifted at 250 Hz (its file dips 5 dB at 280 Hz); the mids that
   crowded the 1 kHz octave (the reese's top, the pad's middle, the chops' 500 Hz) taken down. The kick and bass lanes
   alone are still checked as ContextLoopLow (the strict gate) and ContextLoopLowWarm (bass-body).
3. Kick B, KickPunch: a shorter sub (-20 dB by 220 ms) and a 100-150 Hz knock (drums.py). Kick A is the one round 1
   rated Cool 4, so the A/B is in its own preview (preview.DrumGroove*), through this loop's kick clip; this loop plays
   B.
4. Snare C, SnareClap: snare A's crack with three hands and a short room, set so its backbeat sits level with the kick.
5. The reese talks against the backbeat: a CC74-style low-pass over the bass's top (above 450 Hz, BASS_MOVE) shuts
   into beats 2 and 4 of every bar (577 Hz) and opens after them (3.2-6.1 kHz, the widest in bar 4, into the
   turnaround), on top of each note's own filter opening; the lane dips 4 dB under every backbeat (SNARE_DUCK). So the
   louder bass leaves the snare its jump and its peak. The 6 dB kick ducker stays.
6. A garage plate for the Space send (1.1 s, a 1/64-note pre-delay, 28 ms at 134 BPM) in place of the shared 2.6 s hall,
   and a thinner Echo (its loop high-passed at 600 Hz instead of 300).
7. The swung top: the Shaker on every 16th up to the fill, accented on the "a" (each beat's last 16th, swung late),
   near the centre on the other side of the hats (its width from the shaker's own stereo noise, drums.SHAKER_SIDE), and
   the hats 2 dB up.
8. A re-pitched hook: "me" a fifth up answering bar 3's last snare (ChopMeE5, on a little plate and no echo), and
   "floor" doubled a fifth up on the throw into the fill (ChopFloorFs5): the echo only on the phrase's end.

- ContextLoop: dry, with bass A, clean (ReeseClean); ContextLoopWarm: the same with bass B, warm (ReeseWarm), both bass
  lanes at one loudness (BASS_LUFS). The kit's files exactly as written, played the way the sampler will: hits on the
  16th grid, every lane on the kit's one swing (K.SWING), a velocity picks the nearest layer, round robins cycle; a
  fader, a pan and the lane inserts (EQ, mono, the bass move, the kick clip) per lane. The three full loops sit at one
  loudness (LOOP_LUFS).
- ContextLoopWet: the same with the sends (sends.py): Space (the plate), Echo and the resonator stand-in, the last chop
  and its double thrown into the fill; bass B. The target the engine must match (S1e): its settings are in its metadata.
- ContextLoopClub: ContextLoopWet as its file is (with the renderer's 20 Hz high-pass), through master.master at the
  genre's settings: the round's loud reference, aimed at -9 LUFS. Its metadata holds the master's report, how far
  short of the target included.
- ContextLoopLow, ContextLoopLowWarm: the kick and bass lanes alone as mixed, gated in every gap between kicks.

Everything loops: hits and tails wrap, and the pad and the sends run over two passes, keeping the second.

Rendering needs the vocal models (the chops; not the low-end loops): see tools/sound/README.md.
"""
import functools

import numpy as np

from tools.sound import dsp48 as D
from tools.sound import master as MS
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
                                    # ends on the downbeat, as round 1's one-beat stop did
FILL_FROM = 4                       # the fill's 16th the loop plays it from: the roll and the swell (GlitchFillLift
                                    # plays the whole half bar in its own file)
FILL_FADE_S = 0.002                 # the fade-in where the loop starts the fill (the swell sounds there)
KICK = ((0, 1.0), (10, 0.8), (16, 1.0), (23, 0.4), (26, 0.8), (32, 1.0), (42, 0.8), (46, 0.6), (48, 1.0), (55, 0.4),
        (58, 0.8))
SNARE = ((4, 1.0), (12, 0.8), (20, 1.0), (28, 0.8), (31, 0.2), (36, 1.0), (39, 0.2), (44, 0.8), (52, 1.0))
HAT_BAR = ((2, 0.8), (3, 0.2), (6, 1.0), (7, 0.4), (10, 0.8), (11, 0.2), (14, 1.0), (15, 0.4))
HAT = tuple((16 * b + s, v) for b in range(4) for s, v in HAT_BAR if 16 * b + s < FILL_AT) + ((29, 0.2),)
SHAKER_BEAT = (0.45, 0.3, 0.75, 1.0)   # the shaker over each beat's four 16ths: soft on the beat, softest on the "e",
                                       # the "and" with the hat, the "a" (swung late, pushing into the next beat)
                                       # loudest
SHAKER_BAR = tuple((s, SHAKER_BEAT[s % 4]) for s in range(16))
SHAKER = tuple((16 * b + s, v) for b in range(4) for s, v in SHAKER_BAR if 16 * b + s < FILL_AT)
PERC = ((14, 0.75), (38, 1.0), (45, 0.5))
LINE = B.PICK                       # the bass designer's pick of the three lines: B, the Walk
BASS_LUFS = -15.8                   # the bass lane's loudness here, A and B alike (before the loop's gain): 1.2 LU over
                                    # the kick lane, the genre's hook (round 3's first mix: -18.8, 1.7 LU under it); the
                                    # density the club master needs once the kick clip is light (KICK_CLIP_DB)
LOOP_LUFS = -16.3                   # the three full loops at one loudness, so dry A, dry B and wet compare by sound
WET_BASS = 'warm'                   # the wet and club loops' bass: the bass designer's lean ("B, warm, for the body on
                                    # small speakers"); the A/B itself is the listener's call
THROW = (1.0, 1.0)                  # a throw (CC91-style): the chop's full Space and Echo send, ringing into the fill
ANSWER = (0.4, 0.0)                 # the answer's sends: a little plate and no echo, so its dotted-8th repeats never
                                    # sit on bar 4's kicks (the echo is for the phrase's end, the throw)
# (16th the chop's first note lands on, the chop, its own sends (None: the vox lane's), its level against the vox fader)
CHOPS = ((18, 'ChopFindToFloor', None, 0.0), (45, 'ChopMeE5', ANSWER, -3.0), (58, 'ChopFloor', THROW, 0.0),
         (58, 'ChopFloorFs5', THROW, -5.0))       # "me" a fifth up answers on the swung 16th after bar 3's last snare
PLUCK_AT = (0, 32)                  # the 2-bar phrase, twice
PLUCK_XOVER = 1000.0                # the bass lane's top, over this, ducks under every pluck attack...
PLUCK_DUCK = dict(depth_db=6.0, ramp=0.002, hold=0.038, release=0.040)   # ...6 dB for its first 40 ms

# ------------------------------------------------------------------ the mix: fader dB, pan, the three sends
LANES = {
    'kick': ('KickPunch', 0.1, 0.0, 0.0, 0.0, 0.0),           # kick B's peaks where kick A's sat in round 2
    'bass': (None, None, 0.0, 0.0, 0.0, 0.0),                 # its file and fader per character: bass_lane()
    'snare': ('SnareClap', -2.2, 0.0, 0.22, 0.0, 0.0),        # its backbeat within 1 dB of the kick's peaks
    'hat': ('ClosedHatFM', -1.7, 0.2, 0.06, 0.10, 0.15),      # 2 dB up on round 2
    'shaker': ('Shaker', -5.5, -0.25, 0.05, 0.10, 0.0),       # the groove's driver near the centre, mirroring the
                                                              # hats (+0.2); its width is its own (drums.SHAKER_SIDE)
    'perc': ('AlloyHit', -5.0, -0.25, 0.70, 0.60, 0.0),
    'pad': ('AirPadFsm9', -9.0, 0.0, 0.40, 0.0, 0.0),         # 3 dB up on round 2: the held harmony carries the density
    'pluck': ('ChromePluckPhrase', -8.0, 0.0, 0.30, 0.25, 0.0),
    'vox': (None, -0.5, 0.0, 1.00, 0.80, 0.30),
    'fill': ('GlitchFillLift', -2.0, 0.0, 0.25, 0.25, 0.0),
}
SENDS = {'space': 3, 'echo': 4, 'resonator': 5}   # column of each send in LANES
REESE = {'clean': 'ReeseClean', 'warm': 'ReeseWarm'}
LOW = ('kick', 'bass')
SEAM_HOLD = 0.25                    # a held voice across the seam: its last 0.25 s crossfade (equal power) into the
                                    # samples before the window, so the pad never drops out at the loop point
RESONATOR_TONES = tuple(n + 24 for n in (54, 64, 69, 73, 80))   # the pad's F#m9 two octaves up: F#5 E6 A6 C#7 G#7

# ------------------------------------------------------------------ the lane inserts (changes 1, 2 and 5)
KICK_CLIP_DB = 2.0                  # the kick lane soft-clipped this far under its own true peak (master.soft_clip,
                                    # 4x, its 1.5 dB knee and 0.5 dB over): 1.5 dB off the attack, so every full-velocity
                                    # kick keeps a crest (qa.kick_crest_db, KICK_CREST_S from its onset) of 5.2-5.3 dB,
                                    # inside the 5-7 dB band round 1 liked (the file: 6.65 / 5.96). At 4 dB it read 3.6,
                                    # under ADR 0011's 4 dB floor: a kick flattened onto its body
KICK_CREST_S = 0.200                # the window each kick's crest is read in, from its onset
EQ = {                              # (Hz, dB, Q) peaking filters, run round the loop
    'bass': ((180.0, 2.0, 1.0), (900.0, -2.5, 0.8)),         # the body up, the reese's top down
    'pad': ((250.0, 5.5, 0.8), (700.0, -3.0, 0.7)),          # its 280 Hz dip undone, its middle down
    'vox': ((500.0, -2.0, 1.2),),                             # the chops' 500 Hz down
}
MONO_BELOW = {'bass': 450.0, 'pad': 350.0}   # the low mids central (dsp48.mono_below, 8th order on the side)
BASS_WIDTH = 0.6                    # the bass's side over 450 Hz scaled to this: a narrower reese stack
# The reese move: a CC74 lane (sampler.cc_curve; CC74 maps to 300 Hz x 2^(6 cc / 127), as the engine's lane filter,
# sampler.lane_filter) that shuts the bass's top into beats 2 and 4 of every bar and opens it after them, so the line
# talks between the backbeats: a note pushed onto the swung 16th before the snare starts open and closes into it, one
# that starts on the snare opens out of it. Round 3's first move (shut on the downbeats, where the line rests for the
# kick) moved each note's 1-4 kHz by -2.4 to +1.7 dB; this one is under 650 Hz from 50 ms before every snare to 50 ms
# after it (under 900 Hz from 60 ms before: the lane's 8 ms smoother lags the CC), over the 60-10 ms the clap's jump
# reads the mix in, so a louder bass leaves the snare its jump.
MOVE_CC = dict(rest=48, shut=20, open=(72, 84, 76, 92))   # 1.44 kHz on the downbeats; 577 Hz into each backbeat; open
                                                           # to 3.2, 4.7, 3.6 and 6.1 kHz in bars 1-4 (bar 4 the widest,
                                                           # into the turnaround)
MOVE_16THS = dict(close=0.35, before=0.55, after=0.45, reopen=0.9)   # the close takes 0.35 of a 16th, ending 0.55 of
                                                                     # one (61 ms) before the backbeat; shut until 0.45
                                                                     # after it (50 ms); open again over 0.9 (101 ms)
BACKBEATS = (4, 12)                 # each bar's beats 2 and 4 (in bar 4 the fill takes beat 4: the pickup opens into
                                    # the turnaround there)


def backbeat_move():
    """BASS_MOVE, (16th, CC74) over the 4 bars, from MOVE_CC, MOVE_16THS and BACKBEATS."""
    m, c = MOVE_16THS, MOVE_CC
    pts = []
    for bar in range(4):
        b0, op = 16 * bar, c['open'][bar]
        pts.append((b0, c['rest']))
        for beat in BACKBEATS:
            at = b0 + beat
            pts += [(at - m['before'] - m['close'], op), (at - m['before'], c['shut']), (at + m['after'], c['shut']),
                    (at + m['after'] + m['reopen'], op)]
        pts.append((b0 + 15.5, c['rest']))
    return tuple((round(s, 3), v) for s, v in pts)


BASS_MOVE = backbeat_move()
BASS_MOVE_XOVER = 450.0             # the move acts over A's wall: the sub and B's body pass untouched
BASS_MOVE_Q = 1.3                   # a little resonance, so the top talks as it opens
SNARE_DUCK = dict(depth_db=4.0, ramp=0.002, hold=0.030, release=0.060)   # the bass lane under every backbeat: 4 dB
                                    # for its first 32 ms, back in 60 ms; the louder bass's sub and body under the snare
                                    # set the loop's peaks there (the backbeat read +1.45-1.69 dB over a kick)
SNARE_DUCK_VEL = 0.8                # the snares it ducks under: the backbeats, not the ghosts

# ------------------------------------------------------------------ the sends (change 6)
PLATE = dict(S.SPACE, t60=1.1, size=0.45, damp_hz=8500.0, predelay=round(K.STEP / 4, 6), send_hp=500.0, t60_low=0.4,
             shimmer=0.03)          # the garage plate: 1.1 s, bright, a 1/64-note pre-delay, low mids gone in 0.4 s,
                                    # a little of the kit's shimmer
ECHO = dict(S.ECHO, hp_hz=600.0)    # the throws thinner, out of the low mids
MASTER = {}                         # the club master's settings over the genre's defaults (master.settings): none


# ------------------------------------------------------------------ the lanes, on the kit's files as written
def pluck_onsets():
    """Every note of the pluck phrase in the loop (it plays twice), as sample positions."""
    return [G.at(at + step) for at in PLUCK_AT for step, *_ in music.PHRASE]


def circ(fn, x):
    """A filter run round the loop (phrase.circular)."""
    return Ph.circular(fn, x)


def inserts(k, x):
    """A lane's inserts, in order: mono under MONO_BELOW[k], then its EQ (each insert left out when the lane has
    none)."""
    if k in MONO_BELOW:
        x = circ(lambda u: D.mono_below(u, MONO_BELOW[k]), x)
    for fc, gain, q in EQ.get(k, ()):
        x = circ(lambda u, fc=fc, gain=gain, q=q: D.peaking(u, fc, gain, q), x)
    return x


def width(x, amount):
    """The side scaled by amount (the mid untouched); 1 leaves x as it is."""
    if amount == 1.0:
        return x
    m, s = 0.5 * (x[:, 0] + x[:, 1]), 0.5 * (x[:, 0] - x[:, 1]) * amount
    return np.stack([m + s, m - s], 1)


def move_cutoff():
    """The bass move's cutoff, Hz per sample: BASS_MOVE as a CC74 lane the engine receives (sampler.cc_curve: 7-bit
    steps, the parameter smoother), mapped as the lane filter maps CC74."""
    return 300.0 * 2 ** (6.0 * Sm.cc_curve(BASS_MOVE, L, K.STEP) / 127.0)


def bass_move(x):
    """The reese's top through the moving low-pass: over BASS_MOVE_XOVER (an LR4 split, so the bands sum flat) a TPT
    state-variable low-pass on move_cutoff(), run over three passes of the loop so its state joins at the seam. No
    BASS_MOVE: x as it is."""
    if not BASS_MOVE:
        return x
    lo = circ(lambda u: D.lr4(u, BASS_MOVE_XOVER, 'lowpass'), x)
    hi = circ(lambda u: D.lr4(u, BASS_MOVE_XOVER, 'highpass'), x)
    fc = np.tile(move_cutoff(), 3)
    hh = np.tile(hi, (3, 1))
    top = np.stack([D.svf_tpt(hh[:, c], fc, q=BASS_MOVE_Q)[0] for c in range(2)], 1)[L:2 * L]
    return lo + top


def snare_duck():
    """The bass lane's gain under the backbeats (one value per sample): SNARE_DUCK on every snare of SNARE_DUCK_VEL or
    more, round the loop."""
    return D.undb(Ph.pump_db([G.at(s) for s, v in SNARE if v >= SNARE_DUCK_VEL], L, **SNARE_DUCK))


@functools.lru_cache(maxsize=None)
def bass_lane(char):
    """The picked line on the 4-bar grid, for the character ('clean' or 'warm'): basslines.lane() (the sampler voice
    with pitch and slides on the reese's files, the bus ducker on every kick of this loop); the move on its top; the
    dip under every backbeat (SNARE_DUCK); its top over PLUCK_XOVER keyed by the pluck lane (PLUCK_DUCK); narrower over
    450 Hz, mono under it, its EQ; then a fader that sets the lane to BASS_LUFS, the same for A and B. Returns (lane,
    fader dB)."""
    raw = bass_move(B.lane(LINE, char, KICK, G)) * snare_duck()[:, None]
    lo, hi = (circ(lambda u, k=k: D.lr4(u, PLUCK_XOVER, k), raw) for k in ('lowpass', 'highpass'))
    raw = lo + hi * D.undb(Ph.pump_db(pluck_onsets(), L, **PLUCK_DUCK))[:, None]
    raw = inserts('bass', width(raw, BASS_WIDTH))
    fader_db = BASS_LUFS - qa.lufs(raw)
    return raw * D.undb(fader_db), float(fader_db)


def pad_lane():
    """The pad held through the loop and across its seam: the voice runs two loop lengths, the second is kept, and
    its end crossfades into the moment before it, so the harmony never stops at the loop point (as written: the mix
    adds its inserts, pad_mixed())."""
    _, got, _ = Ph.kit_files(K, 'AirPadFsm9')
    voice = Sm.sampler_voice(got[(None, 1, 1)], 2 * L, attack=0.002, release=0.01)[:2 * L]
    return Ph.second_pass(voice, SEAM_HOLD, L, power=True)


def pad_mixed():
    """The pad lane with its inserts: mono under 350 Hz, its EQ."""
    return inserts('pad', pad_lane())


def pluck_lane():
    _, got, _ = Ph.kit_files(K, 'ChromePluckPhrase')
    buf = np.zeros((L, 2))
    for step in PLUCK_AT:
        Ph.place(buf, got[(None, 1, 1)].audio, G.at(step))
    return buf


def vox_parts():
    """Each chop as its own part of the vox lane (so a throw can take its own sends), its first note on its 16th, at
    its own level, with the vox lane's EQ."""
    from tools.sound.recipes.fut01 import vox
    parts = []
    for step, name, _, level in CHOPS:
        _, got, _ = Ph.kit_files(K, name)
        buf = np.zeros((L, 2))
        Ph.place(buf, got[(None, 1, 1)].audio * D.undb(level),
                 G.at(step) - int(np.floor(vox.chop_onset(name) * SR + 0.5)))
        parts.append(inserts('vox', buf))
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


def kick_clip(x):
    """The kick lane's soft clip (master.soft_clip, round the loop): its ceiling KICK_CLIP_DB under the lane's true
    peak plus the clipper's 0.5 dB over. No KICK_CLIP_DB: x as it is. The drum groove previews run their kick lanes
    through it too, so the kick A/B is the kick the loops play."""
    if not KICK_CLIP_DB:
        return x
    y, _ = MS.soft_clip(x, D.true_peak_db(x) - KICK_CLIP_DB + 0.5)
    return y


def kick_crests(x, onsets):
    """The kick crest (qa.kick_crest_db) read over KICK_CREST_S from each onset (samples), round the loop, dB."""
    n = len(x)
    return [round(qa.kick_crest_db(x[(o + np.arange(int(KICK_CREST_S * SR))) % n]), 2) for o in onsets]


def full_kicks(grid=None):
    """The onsets of the loop's full-velocity kicks (samples), on its grid or another."""
    g = grid or G
    return [g.at(s) for s, v in KICK if v == 1.0 and s < g.steps]


@functools.lru_cache(maxsize=1)
def kick_lane():
    """The kick lane after its fader and clip, and the kick onsets: all the low-end loops need besides the bass."""
    return kick_clip(fader('kick', Ph.hits_lane(K, LANES['kick'][0], KICK, G))), tuple(G.at(s) for s, _ in KICK)


@functools.lru_cache(maxsize=1)
def other_lanes():
    """Every lane but the kick and the bass, after its inserts, fader and pan; and the vox lane's parts (one per chop),
    the same way."""
    parts = tuple(fader('vox', p) for p in vox_parts())
    hits = lambda k, part: Ph.hits_lane(K, LANES[k][0], part, G)
    raw = {'snare': hits('snare', SNARE), 'hat': hits('hat', HAT), 'shaker': hits('shaker', SHAKER),
           'perc': hits('perc', PERC), 'pad': pad_mixed(), 'pluck': pluck_lane(), 'fill': fill_lane()}
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
    for p, (_, _, own, _) in zip(parts, CHOPS):
        level = own[j - 3] if own and j < 5 else LANES['vox'][j]
        bus = bus + p * level
    return bus


def lane_meta(mix, char, keys=tuple(LANES)):
    meta = {}
    for k in keys:
        source = {'bass': REESE[char], 'vox': ', '.join(n for _, n, _, _ in CHOPS)}.get(k, LANES[k][0])
        db = bass_lane(char)[1] if k == 'bass' else LANES[k][1]
        meta[k] = {'source': source, 'fader_db': round(db, 2), 'pan': LANES[k][2],
                   **{f'{s}_send': LANES[k][j] for s, j in SENDS.items()},
                   'lufs_before_gain': round(qa.lufs(mix[k]), 2)}
        if k in EQ:
            meta[k]['eq'] = [list(b) for b in EQ[k]]
        if k in MONO_BELOW:
            meta[k]['mono_below_hz'] = MONO_BELOW[k]
    meta['kick']['clip_db'] = KICK_CLIP_DB
    meta['kick']['crest_db_full_kicks'] = kick_crests(mix['kick'], full_kicks())   # the lane as clipped, before the
    return meta                                                                     # renderer's high-pass


def bass_meta(char, kicks):
    """The bass lane: the line, the character, the move, the bus ducker, and the lane alone against this loop's kicks
    (the duck, gap and before-kick readings that a file holding its kicks can't show)."""
    return {'line': f'{LINE}, {B.NAMES[LINE]}', 'character': 'A, clean' if char == 'clean' else 'B, warm',
            'source': REESE[char], 'pick_why': B.PICK_WHY, 'bass_lufs': BASS_LUFS, 'bus_ducker': B.DUCK,
            'snare_ducker': {**SNARE_DUCK, 'key': f'every snare of velocity {SNARE_DUCK_VEL} or more'},
            'move': {'cc74': [list(p) for p in BASS_MOVE], 'over_hz': BASS_MOVE_XOVER, 'q': BASS_MOVE_Q,
                     'cc74_to_hz': '300 x 2^(6 cc / 127)', 'shut_into_16ths': [16 * b + s for b in range(4)
                                                                                for s in BACKBEATS]}
            if BASS_MOVE else None,
            'width_over_450hz': BASS_WIDTH,
            'pluck_sidechain': {'over_hz': PLUCK_XOVER, **PLUCK_DUCK, 'key': 'every pluck note'},
            'bass_alone_vs_kicks': B.bass_only_rows(bass_lane(char)[0], kicks)}


GRID = {'bpm': K.BPM, 'steps': STEPS, 'samples': L, 'swing': K.SWING, 'kick_steps': [s for s, _ in KICK],
        'fill_step': FILL_AT, 'fill_from_16th': FILL_FROM,
        'chops': [{'step': s, 'chop': n, 'level_db': lv} for s, n, _, lv in CHOPS]}
NOT_MODELLED = 'the other inserts (drive, crush)'


# ------------------------------------------------------------------ the files
def context_dry(char):
    mix, _, kicks = lanes(char)
    meta = {'grid': GRID, 'lanes': lane_meta(mix, char), 'bass': bass_meta(char, kicks),
            'not_modelled': f'{NOT_MODELLED}, the master chain (ContextLoopClub)'}
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
    space = two_pass(lambda u: S.space(u, K.space_rng('send'), **PLATE), D.mono(send_bus(mix, parts, 'space')))
    key = np.concatenate([mix['vox'], mix['vox']])
    space = two_pass(lambda u: S.duck(u, key, **S.DUCK)[0], space)
    delay = S.ECHO_STEPS * K.STEP
    echo = two_pass(lambda u: S.echo(u, K.space_rng('echo'), delay, **ECHO), D.mono(send_bus(mix, parts, 'echo')))
    res = two_pass(lambda u: S.resonator(u, resonator_freqs(), **S.RESONATOR),
                   D.mono(send_bus(mix, parts, 'resonator')))
    return {'space': space * D.undb(S.SPACE_RETURN_DB), 'echo': echo * D.undb(S.ECHO_RETURN_DB),
            'resonator': res * D.undb(S.RESONATOR_RETURN_DB)}


def wet_meta(mix, kicks):
    ret = returns()
    delay = S.ECHO_STEPS * K.STEP
    space_meta = {**PLATE, 'return_db': S.SPACE_RETURN_DB, 'seed': [K.SPACE_SEED, 'send'], 'duck_under': 'vox',
                  'kind': 'the garage plate (round 3); the shared hall is sends.SPACE',
                  **{f'duck_{k}': v for k, v in S.DUCK.items()}}
    echo_meta = {**ECHO, 'delay_steps': S.ECHO_STEPS, 'delay_s': round(delay, 6), 'return_db': S.ECHO_RETURN_DB,
                 'seed': [K.SPACE_SEED, 'echo']}
    res_meta = {**S.RESONATOR, 'return_db': S.RESONATOR_RETURN_DB, 'tones_midi': list(RESONATOR_TONES),
                'ratios': list(K.RATIOS[:4]), 'stand_in': 'a 2-pole bank on one chord; the engine retunes per chord'}
    why = "the designers' pick: line B (the Walk), in B, warm, the bass designer's lean (body on small speakers)"
    return {'target': 'the target the engine must match', 'grid': GRID, 'lanes': lane_meta(mix, WET_BASS),
            'bass': {**bass_meta(WET_BASS, kicks), 'why_this_bass': why},
            'throws': [{'step': s, 'chop': n, 'space_send': t[0], 'echo_send': t[1]} for s, n, t, _ in CHOPS
                       if t == THROW],
            'own_sends': [{'step': s, 'chop': n, 'space_send': t[0], 'echo_send': t[1]} for s, n, t, _ in CHOPS
                          if t and t != THROW],
            'space': space_meta, 'echo': echo_meta, 'resonator': res_meta,
            'return_lufs': {k: round(qa.lufs(v), 2) for k, v in ret.items()},
            'not_modelled': f"{NOT_MODELLED}, the resonator's retuning"}


@functools.lru_cache(maxsize=1)
def wet_mix():
    """The wet loop before the renderer: every lane and the three returns; and its metadata."""
    mix, _, kicks = lanes(WET_BASS)
    return sum(mix.values()) + sum(returns().values()), wet_meta(mix, kicks)


def context_wet(take):
    y, meta = wet_mix()
    return Out(y, meta=meta)


def club_hits():
    """The hits the club master reports on (master.punch) and QA reads: the full-velocity kicks and the backbeat
    snares (SNARE_DUCK_VEL and over), as onsets."""
    return {'kick': full_kicks(), 'clap': [G.at(s) for s, v in SNARE if v >= SNARE_DUCK_VEL]}


def context_club(take):
    """The wet loop as its file is (the renderer's 20 Hz high-pass, round the loop), through the club master at the
    genre's settings (master.settings(K): -9 LUFS, PLR >= 7.5 dB, at most 4 dB of peak reduction). The master sets the
    loudness, so the item has no level and no high-pass of its own."""
    y, meta = wet_mix()
    wet = circ(lambda u: D.dc_block(u, 20.0), y)
    hits = club_hits()
    z, report = MS.master(wet, MS.settings(K, **MASTER), hits=hits)
    return Out(z, top_hits=hits['kick'],
               meta={**meta, 'target': 'the club master: the loud reference (ADR 0014)', 'master': report})


def context_low(char):
    kick, kicks = kick_lane()
    bass, _ = bass_lane(char)
    meta = {'grid': GRID, 'lanes': lane_meta({'kick': kick, 'bass': bass}, char, LOW), 'bass': bass_meta(char, kicks)}
    return Out(kick + bass, hits=kicks, top_hits=full_kicks(), meta=meta)


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
CLUB = {'mud': 'off', 'club_bands': ('125', '250'), 'club_crest_loss_db': 1.6,
        'why': f"{FULL_MIX['why']}. The 125 and 250 Hz windows are exempt, every other band stays gated: those two "
               '(conventions from a chart-record average, which the standard\'s S4 replaces with measured '
               'references) read 3.0 and 3.4 dB under theirs (-1.0 and -2.4 dB re 1 kHz): the kick and bass '
               'A keep the strict mud gate, bass B sits 1.7 dB inside the bass-body window, every other lane is '
               'high-passed by rule, so the round 3 mix fills the low mids as far as those rules allow (the hole '
               '-6.0 / -5.0 dB, round 2 -9.3 / -8.8). The master, at the standard\'s 4 dB cap, costs the snares 1.54 '
               'dB of crest and the kicks 1.52 (the median per hit), over the 1.5 the A&R review proposed: declared '
               'at 1.6 until your ears calibrate that limit'}
ITEMS = [
    Item('Loops', 'ContextLoop', 'loop', context_dry_clean, loop=True, level=LOOP_LUFS, sets=('r3',), tags=('dry',),
         qa=FULL_MIX,
         about=f'{ABOUT}, dry, bass A (clean): the round 3 files and mix, no sends, no master'),
    Item('Loops', 'ContextLoopWarm', 'loop', context_dry_warm, loop=True, level=LOOP_LUFS, sets=('r3',), tags=('dry',),
         qa=FULL_MIX,
         about=f'{ABOUT}, dry, bass B (warm): the same loop, its bass lane at the same loudness'),
    Item('Loops', 'ContextLoopWet', 'loop', context_wet, loop=True, level=LOOP_LUFS, sets=('r3',), tags=('wet',),
         qa=FULL_MIX,
         about=f"{ABOUT}, wet, bass B (warm, the designer's lean): the plate, the Echo and the Alloy resonator, the "
               "throw and its double on the last chop; the engine's target"),
    Item('Loops', 'ContextLoopClub', 'loop', context_club, loop=True, hp_hz=None, sets=('r3',), tags=('wet', 'club'),
         qa=CLUB,
         about=f'{ABOUT}: ContextLoopWet through the club master (master.py), aimed at the UK garage target, -9 LUFS'),
    Item('Loops', 'ContextLoopLow', 'loop', context_low_clean, loop=True, sets=('r3',), tags=('with_kick',),
         qa={'mud': 'gate', 'why': 'the kick and bass A lanes as mixed: gated over the loop and in every gap'},
         about=f'{ABOUT}: the kick and bass A (clean) lanes alone, mud-gated in every gap between kicks'),
    Item('Loops', 'ContextLoopLowWarm', 'loop', context_low_warm, loop=True, sets=('r3',), tags=('with_kick',),
         qa={'mud': 'bass-body', 'why': 'B, warm: the proposed bass-body rule (ADR 0011) on the kick and bass B lanes '
                                       'as mixed, in every gap between the kicks the loop holds'},
         about=f'{ABOUT}: the kick and bass B (warm) lanes alone, under the bass-body rule in every gap'),
]
