"""FUT03's context loop: 4 bars at 128 BPM in G minor, written for this kit and played on its sounds, so each sound is
heard where it will live. Round 3 (the modern production pass, docs/plans/S1-modern-sound.md) mixes it on buses and
adds the club master.

- ContextLoopClub (round 3): the wet loop through the club master (tools/sound/master.py, ADR 0014, proposed) at the
  genre's settings for hyper-clean future house (-8 LUFS-I, a PLR of at least 6.5 dB), with the kit's cap on peak
  reduction (MASTER_REDUCTION_DB, see there): the loop as a club record plays it. Tag club: QA checks it against the
  genre's targets (qa.CLUB). Its metadata holds the master's report, the settings the engine's master must match.
- ContextLoop: dry, with bass A (clean, the strict gate). The kit's files exactly as written, played the way the sampler
  will: hits on the 16th grid, every lane on the kit's one swing (K.SWING, 0.53), a velocity picks the nearest layer,
  round robins cycle, the closed hat chokes the open one. A fader and a pan per lane, the lane inserts (the music
  bus's sidechain pump keyed to the kicks: the stab 9 dB, the pluck 3 dB; the clap's room on the pluck's 1-5 kHz; the
  vocal compressor, the air shelves and the robot's peak limiter) and the buses (below); no master.
- ContextLoopWarm: the same loop with bass B and vowel bass B (warm, the proposed bass-body rule, ADR 0011), the two
  lanes level-matched to A's.
- ContextLoopWet: the loop with the designer's pick of bass (B, warm: see BASS_PICK) plus the three sends in their
  Python versions (tools/sound/sends.py), with the kit's own settings (round 3): Space as a short, bright plate (SPACE),
  ducked under the vocals; Echo, the ping-pong on an 8th (ECHO); and the resonator stand-in, 2-pole modes on the
  kit's in-tune bell ratios (K.RATIOS[:4], a minor triad and its octave) struck from G5 and D6, so they ring Gm9's own
  notes. The Space and Echo returns ride the music bus's pump and its EQ. The target the engine must match (S1e): its
  settings are in the file's metadata in qa_report.json.
- ContextLoopLow: the kick, bass A and vowel bass A, as they sit in the loop, mud-gated over the loop and in every gap
  between the kicks (tag with_kick). ContextLoopLowWarm is the same with bass B and vowel bass B, under the bass-body
  rule: the low end of the wet loop, checked the same way. A loop plays both basses in one character: the A loops are
  the strict gate throughout, the B loops bass-body throughout. The low-end loops are the lanes at their faders,
  before the drum bus's clip (which only shaves what sums over the kick's own peak).
- DrumLoop: a 2-bar preview of the kick, the clap, the hats and the shaker in the loop's pattern (bars 1-2), through
  the drum bus. FillPreview: 2 bars, the same groove for a bar, then the loop's bar 4 (the drums on beats 1-3 and the
  build fill), so the fill is heard running into the downbeat each time round.

The arrangement (16ths 0-63, bars of 16; odd 16ths on the swing):
- Kick on every beat, every hit on the top layer (the round robins alternating): the first version played beats 2 and
  4 at velocity 0.8, a whole layer down (3.4 LU), so the low end went strong-weak. The accents are the clap's and the
  hats'. It sits out beat 4 of bar 4, where the fill's 32nds run into the downbeat. Clap on 2 and 4 in bars 1-3, a
  ghost on the last 16th of bars 1-3 leading into the next; bar 4's claps are the fill's own. Hats: per beat a closed
  hat on the beat and a ghost on the "e", the open hat on the "and" choked by a closed hat on the "a"; beat 4 of each
  bar lets the open hat ring to the next beat; bar 4's beat 4 is left to the fill.
- The shaker (round 3): the glass-bead shaker on every 16th up to the fill's last beat, the swung "e" and "a" leading
  (SHAKER_BEAT), the top layer of 16ths the standard asks for (round 2's hats sat 11 LU under the kick).
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
from scipy import signal

from tools.sound import dsp48 as D
from tools.sound import master as MS
from tools.sound import mixmeter as MM
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
SHAKER_BEAT = (0.6, 0.8, 0.6, 1.0)  # the shaker's velocity on each 16th of a beat: the swung "e" and "a" lead, the "a"
                                    # most, pushing into the next beat; soft on the beat and under the open hat


def shaker_hits(bars=4, stop=DROP_OUT):
    """(16th, velocity): the shaker on every 16th up to `stop` (it sits out the fill's last beat with the hats)."""
    return tuple((s, SHAKER_BEAT[s % 4]) for s in range(16 * bars) if s < stop)


SHAKER = shaker_hits()
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
# The faders were set from each lane's measured loudness (LUFS-I over the loop, against the kick lane) and, in round 3,
# against the standard's targets for the club master, not by ear. Round 2: the bass and the vowel bass about 2 LU
# under the kick, the pluck about as loud as the bass, the chop and the robot 1 LU under the kick (both raised after
# the review: under the stab, the clap and the fill they read 1-4 dB under the rest of the mix), the clap 5 LU and the
# stab 7 LU under the kick, the pling 9 and the hats 11. Round 3 (measured in the wet loop, before the buses):
# - the kick 1 dB up: it carries the 125 Hz octave (with its longer glide) and its new click the 2 kHz one;
# - the clap 3.9 LU under the kick (round 2: 4.9; its new crest of about 12 dB, against round 2's 18, keeps its peak
#   near the kick's at that loudness): its jump over the wet mix in 1-5 kHz 10.1 dB in round 2, 12.4 dB now (10.8
#   before the review's clap room on the pluck, CLAP_ROOM);
# - the hats 10.4 LU (round 2: 11.0) and the shaker 8.8 LU under the kick, the top layer of 16ths;
# - the bass 2.5 dB up (both characters, level-matched): 0.9 LU under the kick (round 2: 2.0), the genre's signature;
#   then the review's fix raised its top 6 dB (bass.TOP_LEVEL), and it reads 0.5 LU under the kick, its sub where it was
#   (BASS_FADER);
# - the stab 1.5 dB up (its new low voice, D3, is the 125 Hz octave's body): 6.4 LU under the kick (round 2: 7.3);
# - the pluck 1.5 dB down, 4.7 LU under the kick (round 2: 1.9), 4.9 with the clap's room: in round 2 its notes
#   before 2 and 4 filled the clap's reference window, 1-5 kHz 60-10 ms before each clap, and it owned the 500 Hz and
#   1 kHz octaves.
# The bass and vowel faders depend on the character, so A and B sit at the same loudness in the loop and the A/B is
# heard level-matched.
LANES = {
    'kick': ('KickSplit', -0.4, 0.0, 0.0, 0.0, 0.0),
    'bass': (None, None, 0.0, 0.0, 0.0, 0.0),
    'vowel': (None, None, 0.0, 0.0, 0.0, 0.0),
    'clap': ('Clap', -0.8, 0.0, 0.15, 0.0, 0.04),
    'hat': ('ClosedHat, OpenHat', -4.2, 0.2, 0.06, 0.10, 0.14),
    'shaker': ('Shaker', -6.0, -0.15, 0.05, 0.0, 0.0),
    'perc': ('MetalPling', -4.0, -0.25, 0.55, 0.50, 0.07),
    'stab': ('StabGm9', -4.0, 0.0, 0.32, 0.0, 0.0),
    'pluck': ('GlassPluckPhrase', -4.0, 0.0, 0.32, 0.25, 0.04),
    'vox': ('ChopRideTheCurrent', 2.0, 0.0, 0.75, 0.50, 0.16),
    'robot': ('VocoderDrop', 1.8, 0.0, 0.50, 0.55, 0.0),
    'fill': ('BuildFill', -3.5, 0.0, 0.25, 0.0, 0.0),
}
BASS_FADER = {'clean': 0.02, 'warm': -0.18}    # dB: the two lines level-matched in the loop. The review's fix raised
                                               # the lines' top 6 dB, which raised their true peaks, so the renderer's
                                               # one gain fell (B 0.55 dB, A 0.67): B's fader is up by its drop, so its
                                               # sub sits where it was, and A's matches B's loudness (-15.07 LUFS, from
                                               # -15.49: the top's own loudness)
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

# ------------------------------------------------------------------ the kit's sends (round 3)
# Space: a short, bright plate instead of the shared 2.6 s hall (sends.SPACE), the standard's future-house space: 1.5 s,
# a 1/64-note pre-delay (29 ms at 128 BPM), dense (small lines), damped only from 12 kHz, its send high-passed at 600 Hz
# and its low band gone in 0.5 s, so nothing hangs under the clap and the pluck; a touch of shimmer. It still ducks
# under the vocals and pumps with the returns (RETURN_PUMP).
# Echo: an 8th-note ping-pong (the shared one is a dotted 8th, which lands an unswung 16th off the kit's grid: two 16ths
# keep every echo on the swing), brighter and thinner than the shared tape echo (HP 300 Hz, LP 2.6 kHz), so its repeats
# sparkle over the mids instead of filling the 1 kHz octave
SPACE = {**S.SPACE, 't60': 1.5, 'size': 0.55, 'damp_hz': 12000.0, 'predelay': K.STEP / 4, 'send_hp': 600.0,
         'low_hz': 600.0, 't60_low': 0.5, 'shimmer': 0.03}
ECHO_STEPS = 2
ECHO = {**S.ECHO, 'hp_hz': 600.0, 'lp_hz': 7000.0}

# ------------------------------------------------------------------ the buses (round 3)
# Round 2 summed its lanes: -16.1 LUFS-I at a PLR of 15.0 dB (the wet loop), and the club master would have needed
# 9.7 dB of peak reduction to reach the genre's -8. Its peaks were the backbeat: the clap lane alone peaked 3.4 dB over
# the kick lane, and with the kick under it on 2 and 4 it set the loop's true peak. Round 3 mixes on buses, as a club
# record is mixed:
# - the drum bus (kick, clap, hats): a soft clip (master.soft_clip, 4x) whose ceiling sits DRUM_CLIP_DB over the kick
#   lane's own true peak, its knee DRUM_KNEE_DB under that: the backbeat is shaved to the kick's level; the kick alone
#   loses about 0.3 dB at its very peak and its body passes as it is;
# - the vocal bus: the chop and the robot each through a compressor (VOX_COMP: 4:1, its loudest moment 6 dB down, a
#   5 ms attack), made up by VOX_MAKEUP_DB to the loudness each had before it; the sends hear them compressed;
# - the kick's room: the vocals, the pling and the fill ducked under every kick (KICK_ROOM), shorter than the music
#   bus's pump, so nothing stacks on the kick's own peak;
# - the clap's room (CLAP_ROOM, the review's fix): the pluck's 1-5 kHz ducked 3 dB from 60 ms before to 80 ms after
#   each clap of 0.9 or more, keyed to the written claps as the pump is to the kicks. The pluck's notes on the "a"
#   before 2 and 4 (110 ms ahead, swung) rang through the clap's own band: in its reference window (1-5 kHz, 60-10 ms
#   before) the pluck was 1.0-3.3 dB under the whole mix, so the clap jumped only 10.8 dB, and at the clap it added
#   to the backbeat's peak (+1.30 dB over a kick alone). With the room: 12.4 dB and +0.98 dB. A note's attack, its
#   first 40 ms, is untouched; only its top then falls 3 dB faster, and comes back 80 ms after the clap. The chop is
#   left alone: "cur-" of "the current" lands on the clap at 16th 36, so its consonant would be the part ducked;
# - the music bus pump stays as round 2 set it: the stab 9 dB, the pluck 3 dB, the returns 4 dB.
DRUM_BUS = ('kick', 'clap', 'hat', 'shaker')
BASS_BUS = ('bass', 'vowel')
# - the music bus (every lane but the drums and the bass, and the returns in the wet loop): RBJ peaking bands run round
#   the loop (MUSIC_EQ, (Hz, dB, Q) each), the standard's balance for future house measured on the club master: a cut
#   at 900 Hz, where the pluck, the pling, the vocals and the returns all sat (the 1 kHz octave every band is read
#   against was round 2's loudest region over 400 Hz); presence at 2.8 kHz (the 2 and 4 kHz octaves were round 2's
#   darkest, -3.9 and -8.4 dB re 1 kHz in the dry loop, against -2..+1 and -4..-1); body at 150 Hz (the stab's D3 and
#   the fill's low mids: the 125 Hz octave, -3.0 dB in round 2, against +1..+5). The vocals left the body lift in the
#   review's fix (VOCAL_EQ), and the bass's louder top raised the 1 kHz octave every band is read against, so the lift
#   went from 4 to 5 dB: the 125 Hz octave back at +1.3 (with 4 dB it read +1.1, 0.09 dB inside its window)
MUSIC_EQ = ((900.0, -3.5, 1.0), (2800.0, 4.5, 0.8), (150.0, 5.0, 0.9))
BODY_HZ = 150.0                     # the music bus's low-mid body band: for the stab's D3 and the fill, not the vocals
# - the vocal bus (the chop and the robot) takes the music bus's EQ without its body lift (the review's fix). In the
#   first round 3 mix the robot's "the" (16th 51: a 20-30 ms swell at the end of the syllable, 2.7 dB over the
#   phrase's other syllables in 120-500 Hz) was the loop's single highest peak, so the club master stopped at its cap
#   there, 1.4 dB over the loudest kick, and its deepest clip, 8 dB over the knee, was that sustained chord: in-band
#   distortion, the clip's 1-6 kHz error 12.4 dB under the music in that 16th. The lift was a small part of the peak
#   (without it, 0.06 dB lower: the swell itself is ROBOT_LIMIT's), but it put the chord's G3 Bb3 D4 3.2 / 2.4 /
#   1.5 dB up, low-mid body a club vocal doesn't need
VOCAL_EQ = tuple(b for b in MUSIC_EQ if b[0] != BODY_HZ)
DRUM_CLIP_DB = 0.0
DRUM_KNEE_DB = 1.5
VOX_COMP = dict(gr_db=6.0, ratio=4.0, knee_db=6.0, attack_s=0.005, release_s=(0.080, 0.500))
VOX_MAKEUP_DB = {'vox': 2.75, 'robot': 3.75}
# ...and the robot's peak limiter (master.limit, round the loop), after its compressor and air: its ceiling 4 dB under
# the lane's own true peak, which is "the" (the compressor's 5 ms attack doesn't catch a swell that short), so "the"
# takes 4 dB and the other syllables' peaks 0-2.2 dB: the phrase's peaks level, its 1-4 kHz within 0.5 dB of what it
# was. A 5 ms lookahead, a 5 ms hold and a 50 ms release, so "glass", 30 ms after "the", starts clear of it. The
# review's other proposal, a static EQ (a 180 Hz high-pass and -4 dB at 200 Hz), on its own left 16th 51 the loop's
# highest peak at the master's drive; with the body lift gone too it reached the same master, but cut the robot's
# 150-300 Hz by 7.9 dB and its loudness in the mix by 2.8 LU, against the first chain (this route: 3.8 dB, 2.2 LU)
ROBOT_LIMIT = dict(under_db=4.0, lookahead_s=0.005, hold_s=0.005, release_s=0.050)
KICK_ROOM = dict(depth_db=3.0, ramp=0.002, hold=0.015, release=0.100)
ROOMED = ('vox', 'robot', 'perc', 'fill')
CLAP_ROOM = dict(depth_db=3.0, band_hz=(1000.0, 5000.0), before=0.060, after=0.080, ramp=0.010, min_vel=0.9)
CLAP_ROOMED = ('pluck',)
# the air (round 3): a high shelf on the pluck and on the vocal bus (after its compressor, so the sends hear it), the
# standard's "air on the pluck and vocals" for the darkest top of the four kits; (Hz, dB)
AIR = {'pluck': (7000.0, 3.0), 'vox': (8000.0, 4.0), 'robot': (8000.0, 4.0)}


def high_shelf(x, fc, gain_db, slope=1.0):
    """RBJ cookbook high shelf (shelf slope 1), along axis 0."""
    A = 10 ** (gain_db / 40)
    w0 = 2 * np.pi * fc / SR
    cw, sa = np.cos(w0), 2 * np.sqrt(A) * np.sin(w0) / 2 * np.sqrt((A + 1 / A) * (1 / slope - 1) + 2)
    b = np.array([A * ((A + 1) + (A - 1) * cw + sa), -2 * A * ((A - 1) + (A + 1) * cw),
                  A * ((A + 1) + (A - 1) * cw - sa)])
    a = np.array([(A + 1) - (A - 1) * cw + sa, 2 * ((A - 1) - (A + 1) * cw), (A + 1) - (A - 1) * cw - sa])
    return signal.lfilter(b / a[0], a / a[0], x, axis=0)


def air(x, key):
    """The lane's air shelf (AIR), round the loop."""
    return Ph.circular(lambda u: high_shelf(u, *AIR[key]), x)


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


def clap_room_env(grid=G, hits=CLAP):
    """The clap's room, 0..1 per sample round the loop: 1 from CLAP_ROOM['before'] ahead of each clap of
    CLAP_ROOM['min_vel'] or more to CLAP_ROOM['after'] behind it, raised-cosine ramps CLAP_ROOM['ramp'] long outside
    that. Keyed to the written claps, as the pump is to the kicks: the engine knows them before they sound."""
    cr = CLAP_ROOM
    t = np.arange(grid.length)
    env = np.zeros(grid.length)
    a, b, r = int(cr['before'] * SR), int(cr['after'] * SR), int(cr['ramp'] * SR)
    for step, vel in hits:
        if vel < cr['min_vel']:
            continue
        d = (t - grid.at(step) + grid.length // 2) % grid.length - grid.length // 2   # signed, round the loop
        u = np.clip(np.minimum((d + a + r) / r, (b + r - d) / r), 0.0, 1.0)
        env = np.maximum(env, 0.5 - 0.5 * np.cos(np.pi * u))
    return env


def clap_room(x):
    """A lane's CLAP_ROOM band ducked CLAP_ROOM['depth_db'] under clap_room_env(): the band split zero-phase round the
    loop, so outside the room the lane passes exactly as it was."""
    lo, hi = CLAP_ROOM['band_hz']
    band = Ph.circular(lambda u: D.bfilt(u, (lo, hi), 'bandpass', order=4, zero_phase=True), x)
    return (x - band) + band * D.undb(-CLAP_ROOM['depth_db'] * clap_room_env())[:, None]


RAW = {
    'kick': lambda: Ph.hits_lane(K, 'KickSplit', KICK, G),
    'clap': lambda: Ph.hits_lane(K, 'Clap', CLAP, G),
    'hat': hat_lane,
    'shaker': lambda: Ph.hits_lane(K, 'Shaker', SHAKER, G),
    'perc': perc_lane,
    'stab': lambda: Ph.hits_lane(K, 'StabGm9', STAB, G) * pump(**PUMP['stab']),
    'pluck': lambda: clap_room(air(pluck_lane(), 'pluck') * pump(**PUMP['pluck'])),
    'vox': chop_lane,
    'robot': robot_lane,
    'fill': fill_lane,
}
assert set(CLAP_ROOMED) == {'pluck'}, 'RAW gives the clap room to the pluck only'


@functools.lru_cache(maxsize=None)
def lane(key, char=None):
    """One lane as the loop hears it, after its fader, pan and insert (the bass in the character `char`; the vocals
    through the vocal bus's compressor and air shelf, the robot then through its peak limiter)."""
    x = bass_lane(char) if key == 'bass' else vowel_lane(char) if key == 'vowel' else RAW[key]()
    y = panned(x, LANES[key][2]) * D.undb(fader_db(key, char))
    if key in VOCALS:
        y = air(vox_comp(y, key), key)
    return robot_limit(y) if key == 'robot' else y


def vox_comp(x, key):
    """The vocal bus's compressor on one vocal lane (the chop and the robot never overlap), made up."""
    y, _ = MS.glue(x, **VOX_COMP)
    return y * D.undb(VOX_MAKEUP_DB[key])


def robot_limit(x):
    """The robot's peak limiter (ROBOT_LIMIT): master.limit round the loop, its ceiling under the lane's own peak."""
    lm = ROBOT_LIMIT
    y, _ = MS.limit(x, MM.true_peak_db(x) - lm['under_db'], lm['lookahead_s'], lm['hold_s'], lm['release_s'])
    return y


def panned(x, p):
    """phrase.pan off centre; in the centre the lane as it is (pan's centre gains differ in the last bit, which would
    store a mono low end as two channels)."""
    return D.st(x) if p == 0 else Ph.pan(x, p)


def dc_clean(y):
    """The mix's DC and sub-sonic drift removed with no phase shift (DC_HZ, zero-phase, circular over the loop)."""
    return Ph.circular(lambda u: D.bfilt(u, DC_HZ, 'highpass', order=4, zero_phase=True), y)


def drum_ceiling_db(lanes):
    """The drum bus clip's ceiling, dBFS: DRUM_CLIP_DB over the kick lane's own true peak (read round the loop)."""
    return MM.true_peak_db(lanes['kick']) + DRUM_CLIP_DB


def music_eq(x, bands=MUSIC_EQ):
    """The music bus's EQ (MUSIC_EQ, or the vocal bus's VOCAL_EQ: RBJ peaking bands, (Hz, dB, Q) each), round the
    loop."""
    def eq(u):
        for band in bands:
            u = D.peaking(u, *band)
        return u
    return Ph.circular(eq, x) if bands else x


def mixdown(lanes, ret=None):
    """The lanes through the buses (see 'the buses' above), plus the returns when given, DC cleaned: the loop. The
    vocals reach the music bus's EQ without its body lift (VOCAL_EQ)."""
    drums, _ = MS.soft_clip(sum(lanes[k] for k in DRUM_BUS if k in lanes), drum_ceiling_db(lanes), DRUM_KNEE_DB, 4)
    room = pump(**KICK_ROOM)
    roomed = lambda k: lanes[k] * (room if k in ROOMED else 1.0)  # noqa: E731
    music = sum(roomed(k) for k in lanes if k not in DRUM_BUS + BASS_BUS + VOCALS)
    if ret is not None:
        music = music + sum(ret.values())
    vocal = sum((roomed(k) for k in VOCALS if k in lanes), np.zeros((L, 2)))
    bass = sum(lanes[k] for k in BASS_BUS if k in lanes)
    return dc_clean(drums + bass + music_eq(music) + music_eq(vocal, VOCAL_EQ))


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
               'drop_out_step': DROP_OUT, 'shaker': [[s, v] for s, v in SHAKER],
               'perc': [[s, D.note_name(m), v] for s, m, v in PERC], 'stab_steps': [s for s, _ in STAB],
               'pluck': [[s, D.note_name(m), ln, v] for s, m, ln, v in PLUCK]}
INSERT = {'music_bus_pump': PUMP, 'keyed_to': 'kicks', 'vowel_sub_duck': {**SUB_DUCK, 'under_hz': SUB_XOVER},
          'air_shelves': {k: {'hz': hz, 'db': db} for k, (hz, db) in AIR.items()},
          'clap_room': {'lanes': list(CLAP_ROOMED), **CLAP_ROOM, 'band_hz': list(CLAP_ROOM['band_hz']),
                        'keyed_to': 'the claps of min_vel or more, from `before` ahead of each',
                        'split': 'zero-phase 4th-order band-pass round the loop, the rest is the lane minus the band'},
          'robot_peak_limit': {**ROBOT_LIMIT, 'limiter': 'master.limit, round the loop, after the compressor and air',
                               'ceiling': "under_db under the lane's own true peak"},
          'dc': f'{DC_HZ:g} Hz zero-phase high-pass on the mix (circular); no renderer high-pass'}


def bus_meta(lanes):
    return {'drum_bus': {'lanes': list(DRUM_BUS), 'clip': 'master.soft_clip, 4x', 'ceiling_over_kick_db': DRUM_CLIP_DB,
                         'knee_db': DRUM_KNEE_DB, 'ceiling_dbfs': round(drum_ceiling_db(lanes), 3)},
            'vocal_bus': {'lanes': list(VOCALS), 'compressor': 'master.glue', **VOX_COMP, 'makeup_db': VOX_MAKEUP_DB,
                          'eq': [{'hz': f, 'db': g, 'q': q} for f, g, q in VOCAL_EQ],
                          'eq_note': f"the music bus's EQ without its {BODY_HZ:g} Hz body lift"},
            'bass_bus': {'lanes': list(BASS_BUS)},
            'music_bus': {'lanes': [k for k in LANES if k not in DRUM_BUS + BASS_BUS + VOCALS],
                          'returns': 'in the wet loop', 'eq': [{'hz': f, 'db': g, 'q': q} for f, g, q in MUSIC_EQ]},
            'kick_room': {'lanes': list(ROOMED), **KICK_ROOM}}


def meta(lanes, char, **extra):
    return {'grid': GRID, 'bass': {'character': char, 'item': LINE[char], 'vowel': VOWEL[1][char]},
            'arrangement': ARRANGEMENT,
            'lanes': lane_meta(lanes, char), 'insert': INSERT, 'buses': bus_meta(lanes), **extra,
            'not_modelled': "the master chain (ContextLoopClub has it), the engine's other inserts, portamento (the "
                            "line plays as rendered)"}


# ------------------------------------------------------------------ the files
def context_dry(take, char):
    lanes = mix(char)
    return Out(mixdown(lanes), splices=(fill_splice(),), meta=meta(lanes, char))


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
    space = two(lambda u: S.space(u, K.space_rng('send'), **SPACE), D.mono(send_bus(lanes, 'space')))
    key = sum(lanes[k] for k in VOCALS)
    space = two(lambda u: S.duck(u, np.concatenate([key, key]), **S.DUCK)[0], space)
    delay = ECHO_STEPS * K.STEP
    echo = two(lambda u: S.echo(u, K.space_rng('echo'), delay, **ECHO), D.mono(send_bus(lanes, 'echo')))
    res = two(lambda u: S.resonator(u, resonator_freqs(), **S.RESONATOR), D.mono(send_bus(lanes, 'resonator')))
    p = pump(**RETURN_PUMP)
    return {'space': space * D.undb(S.SPACE_RETURN_DB) * p, 'echo': echo * D.undb(S.ECHO_RETURN_DB) * p,
            'resonator': res * D.undb(S.RESONATOR_RETURN_DB)}


def context_wet(take):
    char = BASS_PICK
    lanes = mix(char)
    ret = returns(char)
    delay = ECHO_STEPS * K.STEP
    space_meta = {**SPACE, 'return_db': S.SPACE_RETURN_DB, 'seed': [K.SPACE_SEED, 'send'],
                  'duck_under': list(VOCALS), **{f'duck_{k}': v for k, v in S.DUCK.items()}, 'pump': RETURN_PUMP}
    echo_meta = {**ECHO, 'delay_steps': ECHO_STEPS, 'delay_s': round(delay, 6), 'return_db': S.ECHO_RETURN_DB,
                 'seed': [K.SPACE_SEED, 'echo'], 'pump': RETURN_PUMP}
    res_meta = {**S.RESONATOR, 'return_db': S.RESONATOR_RETURN_DB, 'tones_midi': list(RESONATOR_TONES),
                'ratios': list(K.RATIOS[:4]), 'modes_hz': [round(f, 3) for f in resonator_freqs()],
                'stand_in': 'a 2-pole bank on one chord; the engine retunes per chord'}
    m = meta(lanes, char, target='the target the engine must match', bass_pick=BASS_PICK,
             space=space_meta, echo=echo_meta, resonator=res_meta,
             return_lufs={k: round(qa.lufs(v), 2) for k, v in ret.items()})
    return Out(mixdown(lanes, ret), splices=(fill_splice(),), meta=m)


# The club master's cap: the peak reduction (clipper and limiter together) it may use to reach the genre's -8 LUFS-I.
# The standard's convention is 4 dB, a guard against a master that crushes the kick and the clap. The first round 3 mix
# needed 7 dB, the heaviest master of the four kits, and what set it was one peak: the robot's "the" through the
# music bus's body lift (16th 51, 1.4 dB over the loudest kick; see VOCAL_EQ and ROBOT_LIMIT). With that peak gone the
# cap binds on the kicks and the offbeat stacks together, and 5 dB reaches -8.5 LUFS-I, inside the window (-9 to -7).
# Measured against the unmastered wet loop, with the review's numbers for 7 dB in brackets: the kick's crest (qa's
# measure, per kick in the loop) loses 2.3 dB (2.8) and the clap's 2.0 (2.6); the clipper takes 3.5 dB (5.4); its
# 1-6 kHz error, in the 16th where it is worst, is 14.6 dB under the music (12.4). The standard's 4 dB stops at -9.2,
# out of the window, with crest costs of 1.5 and 1.2: the loop's PLR is its arrangement's (a kick on every beat, a bass
# that rests on each kick, short plucked parts), and a pumped pad bed did not change that (it raised the offbeat
# stacks with the loudness, or at 3 LU over the stab it took the 125 Hz octave out of its window). The report records
# the cap; the engine's master must match it.
MASTER_REDUCTION_DB = 5.0


def club_hits():
    """The hits the club master reports on (master.punch) and QA reads: every kick (all at full velocity) and the
    backbeat claps (velocity 0.8 and over), as onsets."""
    return {'kick': list(kicks(hits=tuple(h for h in KICK if h[1] == 1.0))),
            'clap': list(kicks(hits=tuple(h for h in CLAP if h[1] >= 0.8)))}


def context_club(take):
    """The wet loop through the club master (tools/sound/master.py) at the genre's settings (MS.settings(K): hyper-clean
    future house, -8 LUFS-I, PLR at least 6.5 dB) with the kit's cap (MASTER_REDUCTION_DB, declared on the item): the
    loop as a club record plays it. The master's report is in the metadata, with the wet loop's: the settings the
    engine's master must match."""
    wet = context_wet(take)
    hits = club_hits()
    z, report = MS.master(wet.audio, MS.settings(K, max_reduction_db=MASTER_REDUCTION_DB), hits=hits)
    return Out(z, splices=wet.splices, top_hits=hits['kick'],
               meta={**wet.meta, 'master': report, 'target': 'the target the engine must match, through its master'})


def context_low(take, char):
    lanes = mix(char, LOW)
    return Out(dc_clean(sum(lanes.values())), hits=kicks(), top_hits=club_hits()['kick'], meta=meta(lanes, char))


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
DRUM_SHAKER = shaker_hits(bars=2, stop=DRUM_STEPS)


def drums_2bar(kick, clap, hats, shaker, fill_at=None):
    """Two bars of the loop's drums at their faders through the drum bus (the clip at the kick's own peak), and the
    fill on its 16th: (audio, meta)."""
    fader = lambda k, x: panned(x, LANES[k][2]) * D.undb(LANES[k][1])
    parts = {'kick': fader('kick', Ph.hits_lane(K, 'KickSplit', kick, DG)),
             'clap': fader('clap', Ph.hits_lane(K, 'Clap', clap, DG)), 'hat': fader('hat', hat_lane(DG, hats)),
             'shaker': fader('shaker', Ph.hits_lane(K, 'Shaker', shaker, DG))}
    y, _ = MS.soft_clip(sum(parts.values()), drum_ceiling_db(parts), DRUM_KNEE_DB, 4)
    meta = {'steps': DRUM_STEPS, 'bpm': K.BPM, 'swing': K.SWING, 'kick': [list(h) for h in kick],
            'clap': [list(h) for h in clap], 'hats': [list(h) for h in hats], 'shaker': [list(h) for h in shaker],
            'drum_bus': {'clip': 'master.soft_clip, 4x', 'ceiling_over_kick_db': DRUM_CLIP_DB, 'knee_db': DRUM_KNEE_DB}}
    if fill_at is not None:
        _, files, _ = Ph.kit_files(K, FILL[1])
        f = np.zeros((DG.length, 2))
        Ph.place(f, files[(None, 1, 1)].audio, DG.at(fill_at))
        y = y + fader('fill', f)
        meta['fill'] = {'step': fill_at, 'item': FILL[1]}
    return dc_clean(y), meta


def drum_loop(take):
    """Bars 1-2 of the loop's drums alone: the kick, the clap, the hats and the shaker."""
    y, meta = drums_2bar(DRUM_KICK, DRUM_CLAP, DRUM_HATS, DRUM_SHAKER)
    return Out(y, meta=meta)


# the fill preview: bar 1 as the loop's bar 1, bar 2 as its bar 4 (the drums on beats 1-3, the fill)
FILL_KICK = tuple(h for h in KICK if h[0] < BAR) + tuple((s - 32, v) for s, v in KICK if s >= 48)
FILL_CLAP = tuple(h for h in CLAP if h[0] < BAR)
FILL_HATS = tuple(h for h in HATS if h[0] < BAR) + tuple((s - 32, k, v) for s, k, v in HATS if s >= 48)
FILL_SHAKER = tuple(h for h in SHAKER if h[0] < BAR) + tuple((s - 32, v) for s, v in SHAKER if s >= 48)


def fill_preview(take):
    y, meta = drums_2bar(FILL_KICK, FILL_CLAP, FILL_HATS, FILL_SHAKER, fill_at=BAR)
    return Out(y, meta=meta)


# The full loops carry vocals, the clap, the stab and the fill in 150-400 Hz by design, so the mud gate runs on their
# low end alone (ContextLoopLow, ContextLoopLowWarm); every other lane comes from an item that passed its own low cut.
ABOUT = '4-bar context loop, 128 BPM, G minor'
FULL_MIX = {'mud': 'off', 'why': 'the full mix: vocals, clap, stab and fill belong in 150-400 Hz; its low end is gated '
                                 'as ContextLoopLow (A) and ContextLoopLowWarm (B), and every other lane passed its '
                                 'own low cut'}
LOOP = dict(loop=True, hp_hz=None, sets=('r2', 'r3'))   # every file here: a loop from sample 0, no renderer
                                                        # high-pass
CLUB = dict(loop=True, hp_hz=None, sets=('r3',))          # new in round 3
DRY_B_LUFS = -13.16                 # dry A and B at one loudness, so the A/B is the character alone: at their own
                                    # peaks B read 0.57 LU over A (-12.59 against -13.16 LUFS-I), so B comes down to A,
                                    # which keeps its peak
CLUB_QA = {'mud': 'off', 'club_max_reduction_db': MASTER_REDUCTION_DB, 'club_crest_loss_db': 2.0,
           'why': f"{FULL_MIX['why']}. The master's cap is {MASTER_REDUCTION_DB:g} dB, over the standard's 4 (at 4 dB "
                  'the loop stops at -9.23 LUFS-I, outside the window): the arrangement\'s own PLR (a kick on every '
                  'beat, a bass resting on each, short plucked parts), for your decision by ear. It costs the claps '
                  '1.97 dB of crest and the kicks 1.86 (the median per hit), over the 1.5 the A&R review proposed: '
                  'declared at 2.0 until your ears calibrate that limit'}
ITEMS = [
    Item('Loops', 'ContextLoopClub', 'loop', context_club, tags=('wet', 'club'), qa=CLUB_QA, **CLUB,
         about=f'{ABOUT}, the wet loop through the club master (hyper-clean future house: -8 LUFS-I, a PLR of at '
               f'least 6.5 dB, at most {MASTER_REDUCTION_DB:g} dB of peak reduction): the loop as a club record plays '
               f'it'),
    Item('Loops', 'ContextLoop', 'loop', context_clean, tags=('dry',), qa=FULL_MIX, **LOOP,
         about=f'{ABOUT}, dry, bass A (clean, both basses): the kit files as the sampler plays them, mixed on the '
               f'buses, the music bus pumped'),
    Item('Loops', 'ContextLoopWarm', 'loop', context_warm, tags=('dry',), qa=FULL_MIX, level=DRY_B_LUFS, **LOOP,
         about=f'{ABOUT}, dry, bass B (warm, both basses): the same loop with the warm line and vowel bass'),
    Item('Loops', 'ContextLoopWet', 'loop', context_wet, tags=('wet',), qa=FULL_MIX, **LOOP,
         about=f"{ABOUT}, wet, bass B (the pick): a short bright plate, an 8th-note ping-pong and the resonator on the "
               f"kit's bell ratios, unmastered; the engine's target"),
    Item('Loops', 'ContextLoopLow', 'loop', context_low_clean, tags=('with_kick',), **LOOP,
         qa={'mud': 'gate', 'why': 'the kick, bass A and vowel bass A lanes as mixed: gated over the loop and in every '
                                   'gap'},
         about=f'{ABOUT}: the kick, bass A and vowel bass A (clean) alone, mud-gated in every gap between kicks'),
    Item('Loops', 'ContextLoopLowWarm', 'loop', context_low_warm, tags=('with_kick',), **LOOP,
         qa={'mud': 'bass-body', 'why': "B, warm: the wet loop's low end (kick, bass B, vowel bass B) under the "
                                        'proposed bass-body rule (ADR 0011), in every gap'},
         about=f'{ABOUT}: the kick, bass B and vowel bass B (warm) alone, under the bass-body rule in every gap'),
    Item('Loops', 'DrumLoop', 'loop', drum_loop, tags=('preview',), **LOOP,
         qa={'mud': 'off', 'why': "a drum preview: the clap's body and tail belong in 150-400 Hz; the kick passed its "
                                  'own gate and groove loop, the clap, hats and shaker their low cut'},
         about='2-bar drum preview at 128 BPM on the kit swing, through the drum bus: the kick four on the floor, the '
               'clap on 2 and 4 with a ghost, the hats round an open hat on the offbeats, the shaker on the 16ths'),
    Item('Loops', 'FillPreview', 'loop', fill_preview, tags=('preview',), **LOOP,
         qa={'mud': 'off', 'why': "a fill preview: the clap's body and tail and the toms belong in 150-400 Hz; the "
                                  'kick passed its own gate, the fill, hats and shaker their low cut'},
         about='2-bar fill preview at 128 BPM: a bar of the groove, then the build fill under the drums on beats 1-3, '
               'running into the downbeat'),
]
