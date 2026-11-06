"""FUT02 "Orbit Relay" context loop for listening rounds 2 and 3: 4 bars at 130 BPM in D minor, written for this kit and
played on its files, so each sound is heard where it will live.

- ContextLoopClub (round 3): the wet loop through the club master (tools/sound/master.py, ADR 0014, proposed) at the
  genre's settings for sci-fi club (-8.5 LUFS-I, a PLR of at least 7 dB), with the kit's cap on peak reduction
  (MASTER_REDUCTION_DB, see there): the loop as a club record plays it. Tag club: QA checks it against the genre's
  targets (qa.CLUB). The master's report is in its metadata, the settings the engine's master must match (S1e).
- ContextLoop: dry, with bass A (clean). The kit's files exactly as written, played the way the sampler will: hits on
  the 16th grid, every lane on the kit's one swing (K.SWING), a velocity picks the nearest layer, round robins cycle,
  the open hat is choked by the next closed hat, held voices loop while the key is down and release when it lifts. A
  fader and a pan per lane, the lane inserts (the vocal compressor, the vocoder's chorus, the arp's filter ride and its
  rides under the vocals and bar 1's backbeats, the clap's snap, the drop glide's sidechain) and the buses (below), no
  master.
- ContextLoopWarm: the same loop with bass B (warm), the line and the glide level-matched to A's (the same loudness on
  the bass lane), so the A/B is the low end alone.
- ContextLoopWet: the loop with the picked bass (bass.PICK, "Orbit", in its B, warm character: round 1 found the strict
  gate's hollow reese "doesnt make me want to dance", and B is the one with body on small speakers) plus the three
  sends, in their Python versions (tools/sound/sends.py) at the kit's own settings (SPACE, ECHO): Space, the 8-line FDN
  with a little shimmer, a 3.5 s sci-fi hall ducked under the vocals; Echo, the arp's bright ping-pong on a dotted 8th;
  and the resonator stand-in, 2-pole modes on the pad's chord two octaves up times the kit's stiff-wire ratios
  (K.RATIOS). The last chop is a throw (full send) into the fill bar. The target the engine must match (S1e): its
  settings are in the file's metadata in qa_report.json.
- ContextLoopLow: the kick and bass A lanes alone, as they sit in the loop. The strict mud gate runs on it in every
  gap between its kicks (tag with_kick). ContextLoopLowWarm is the same with bass B, under the bass-body rule. These
  two need no vocal models.

The mix (round 3): round 2 summed its lanes and read -14.9 LUFS-I at a PLR of 13.9 dB, too sparse for a club master
(the modern-sound standard, docs/plans/S1-modern-sound.md). Round 3 mixes on buses, as a club record is mixed (see
'the buses' below): a vocal bus compressor; a drum bus (kick, clap, hats) soft-clipped at the kick's own peak, so the
backbeat and the hats' ticks stop setting the loop's peak; the pad, the arp and the returns pumped by every kick, the
vocals, zaps and fill given the kick's room; a music bus EQ that trims the 400-800 Hz bulge. The sends are the kit's
own: a longer, brighter hall and a brighter echo. The arp rides a filter over three bars. After the A&R review: the
club master's cap is 4.25 dB (MASTER_REDUCTION_DB), the clap snaps and the arp makes room for bar 1's backbeats, and
the arp, zaps and hats sit balanced between the speakers (their recipes); the arp's and zaps' faders keep their mono
level, the hats' fader their loudness.

The parts (16ths 0-63, bar b starts at 16 b):
- Kick: four on the floor, every beat on the hard layer (the even pulse of a drum machine: the first render's softer
  2 and 4 were 3.3 LU down, and the clap on them led the pulse), out on beat 4 of bar 4, where the fill dives.
- Clap: 2 and 4 of bars 1-3, three ghosts on the a of a beat, 2.5 dB under its first level, so the kick leads the
  backbeat; in bar 4 the fill's own claps carry the backbeat.
- Hats: closed 16ths around an open hat on every off-beat 8th, each open hat choked by the next closed one; bar 2
  varies the a's. Bar 4 is the fill's own hat crescendo.
- Zap: four lasers on D5: a pickup in bar 1, a turnaround into bar 2, an answer to the robot, a lead-in to the fill,
  each on its own side of the field (ZAP_PAN).
- Bass: the designers' pick, the "Orbit" line (2 bars, twice). It rests on every kick and answers it, D1 on the e and
  D2 on the and, with ghosts on the a, a slide and a B-flat in bar 2. Bar 4 opens with the drop glide (the second bass
  gesture, once): struck on the kick, D2 diving to D1 over beat 1, held over beat 2, its key lifted before beat 3's
  kick; then the line returns with its B-flat on the and of beat 3 and climbs A1-C2-D2 into the loop: D1, Bb1, A1,
  C2, D2, a cadence into the downbeat.
- Pad: the starfield Dm9 held right through the loop point, its 4-bar sweep closed at bar 1 and open at bar 3.
- Lead, up front: the chrome arp phrase (2 bars, twice), the kit's signature, through a filter ride that opens from
  bar 1 to bar 3 (ARP_RIDE). It makes room for the words, the vocal pocket: 4 dB down with its top (1.2-6.5 kHz)
  carved a further 8-10 dB under the robot and "pull me", its top carved 6 dB under "into orbit", so it sits under the
  vocals where the words are heard (at or under them in 1.4-5.6 kHz) and keeps its melody. It rests in bar 4, where
  the fill's lasers take over its 3-3-2 rhythm (playing on, it rubbed a major second against the fill's A5 on step 54).
- Vocals, call and response: the robot calls "Relay online" on bar 2's downbeat, the zap answers, and the sung line
  replies across the bar line, "pull me" on beat 4 of bar 2 and "into orbit" from beat 2 of bar 3.
- Fill: the laser fill, all of bar 4, its dive landing on D5 at the loop's downbeat.
Everything loops: hits and tails wrap round the loop, every filter and ducker runs round it, the sends run over two
passes, keeping the second.

Rendering needs the vocal models (the chops and the robot): see tools/sound/README.md.
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
ZAP_PAN = (-0.55, 0.55, -0.55, 0.35)      # round 3: each laser on its own side of the field, bouncing (round 2: all at
                                          # -0.2), so the ear candy moves
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
# ...and the backbeat (the A&R review: the clap was buried). The arp's tresillo puts an accent on the 16th before each
# backbeat (steps 3 and 11 of a bar), so before a clap the 1-5 kHz band was mostly arp (65% of it before bar 1's
# claps), and the clap's jump over the 60-10 ms before it read 8.2 and 9.9 dB there. The vocal rides already carve the
# arp under bars 2 and 3's backbeats; bar 1's two get a carve of their own, the same band, shallower (the accent's
# first 10 ms ring before it ramps in)
ARP_BACKBEAT = ((4, 5, -4.0), (12, 13, -4.0))
CARVE_HZ = (1200.0, 6500.0)               # a 4th-order Butterworth band, zero phase: its full depth near 2.8 kHz,
                                          # 2.5-3.6 dB at the edges, 0 dB outside the rides
ARP_RAMP = 0.020
ARP_PRE = 0.080
ARP_REST = (48, 0.010, 0.003)             # resting from this 16th to the loop's end: out over 10 ms before it, back
                                          # over 3 ms on the downbeat (the F5 accent keeps its attack)

# ------------------------------------------------------------------ the mix: fader dB, pan, the three sends
# Faders are set so each lane reads, over the loop (LUFS-I after its fader and inserts, before the buses and the loop's
# one gain; the meta records every lane's reading). Round 2: kick -17, bass -18.5, clap -21 (2.5 dB under its first
# level: the kick leads the backbeat), hats -25, zap -25, pad -24, arp -23.5 (up front: where it plays alone it read
# -20.5, 3.5 LU under the kick; its rides under the vocals and its rest in the fill bar take the other 3 LU), chops
# -19.5, robot -20.5, fill -19.5. Round 3: the bass -16.5 (a big rolling bass, 0.5 LU over the kick lane, which still
# leads on its transient), the hats -23.8 (their new tick reads 4 dB louder at the same peak; the fader takes back 2),
# the pad -21.4 (wide and pumped), the arp -21.4 (the bus trim and the pump take 2.5 dB of it back: it sits as round 2
# set it), the chops -18.5 and the robot -19.9 (compressed and made up), the zaps -24.2, the fill -19. The bass fader
# differs between A and B so both bass lanes read the same. The arp's sends are light in the hall (a dotted-8th echo
# on a running 16th line smears it) and heavier on the bright ping-pong. After the A&R review the arp's, zaps' and
# hats' files changed only on their sides, and each file's one gain moved with its stereo peak: the arp's and zaps'
# faders move by their files' gain changes (-1.74 and -2.80 dB), so their mono level is round 3's exactly (the arp
# lane reads -21.9 with bar 1's backbeat carve, the zaps -25.2, round 2's: their side is round 2's again); the hats'
# fader keeps their lane at -23.8. The clap lane reads -20.5 with its snap, 3.5 LU under the kick
LANES = {
    'kick': ('KickSplit', -2.0, 0.0, 0.0, 0.0, 0.0),
    'bass': ('OrbitLine + DropGlide', None, 0.0, 0.0, 0.0, 0.0),
    'clap': ('ElectroClap', -2.5, 0.0, 0.08, 0.0, 0.0),
    'hat': ('ClosedHat, OpenHat', -3.44, 0.15, 0.06, 0.10, 0.15),
    'zap': ('Zap', -7.80, 0.0, 0.50, 0.60, 0.20),
    'pad': ('StarfieldPadDm9', -10.1, 0.0, 0.45, 0.0, 0.0),
    'arp': ('ChromeArpPhrase', -7.14, 0.0, 0.10, 0.25, 0.10),
    'chops': (', '.join(n for _, n, _ in CHOPS), -0.7, 0.0, 0.80, 0.60, 0.30),
    'robot': ('VocoderRelay', -1.5, 0.0, 0.60, 0.50, 0.0),
    'fill': ('LaserFill', -5.2, 0.0, 0.25, 0.25, 0.0),
}
BASS_FADER = {CLEAN: -3.42, WARM: -2.50}    # dB: level-matched (see above)
SENDS = {'space': 3, 'echo': 4, 'resonator': 5}   # column of each send in LANES
VOCALS = ('chops', 'robot')               # the Space return ducks under these
LOW = ('kick', 'bass')
RESONATOR_TONES = tuple(n + 24 for n in (pad.CHORD[0],) + pad.CHORD[2:])   # the pad's Dm9 two octaves up, D5 C6 F6 A6
                                          # E7, times RATIOS[:4]: modes from 0.59 to 10.6 kHz, where the hats, zap,
                                          # arp and chops reach, out of the low mids

# the kit's own sends (round 3; the standard's S3: each kit passes its own Space and Echo, sends.py keeps the defaults).
# Space: a longer, brighter sci-fi hall for the pad and the vocoder (the standard's 3-4 s with a 1/32-note pre-delay):
# 3.5 s (the shared one 2.6 s), pre-delay a 32nd (57.7 ms; shared 20 ms), damped from 9 kHz (6.5 kHz) and a touch more
# of its octave-up shimmer; its low mids still decay in 0.8 s and its send is still high-passed at 450 Hz.
# Echo: the arp's ping-pong, brighter and thinner than the shared tape echo (HP 300 Hz, LP 2.6 kHz), whose repeats sat
# in the 400-800 Hz bulge (its return read -5 dB re the loop's 1 kHz octave in the 500 Hz one, louder there than the
# arp itself); from 700 Hz to 7 kHz they sparkle over the mids and spread the top
SPACE = {**S.SPACE, 't60': 3.5, 'predelay': K.STEP / 2, 'damp_hz': 9000.0, 'shimmer': 0.08}
SPACE_RETURN_DB = S.SPACE_RETURN_DB
ECHO = {**S.ECHO, 'hp_hz': 700.0, 'lp_hz': 7000.0}
ECHO_RETURN_DB = S.ECHO_RETURN_DB

# ------------------------------------------------------------------ the buses (round 3)
# Round 2's loop was lanes summed: -14.9 LUFS-I at a PLR of 13.9 dB, and the club master would have needed 7.6 dB of
# peak reduction to reach the genre's -8.5. Its peaks were the kick plus whatever sounded on it: the clap on 2 and 4
# (the two summed to 3.6 dB over a kick alone), a chop, the fill's first laser, the hats' ticks, the bass's offbeat
# accents. Round 3 mixes on buses, as a club record is mixed:
# - the vocal bus: each chop and the robot through a compressor (VOX_COMP: 4:1, its loudest moment 6 dB down, a 5 ms
#   attack), made up by VOX_MAKEUP_DB, so that after the music bus's trim each sits about where round 2 had it; the
#   sends hear it (post-insert sends);
# - the drum bus (kick, clap, hats): a soft clip (master.soft_clip, 4x) whose ceiling sits DRUM_CLIP_DB over the kick
#   lane's own true peak, its knee 1.5 dB under that: the backbeat (kick and clap) and the hats' ticks are shaved to
#   the kick's level; the kick alone loses 0.34 dB at its very peak, its body passes as it is;
# - the music bus (everything but the drums and the bass, and the returns in the wet loop): a trim of the 400-800 Hz
#   bulge the standard measured (+2 to +4 dB over the 1 kHz third octave) and a little presence: MUSIC_EQ, RBJ peaking
#   bands run round the loop (phrase.circular). The arp's body, the pad's resonant sweep, the fill's lasers and the
#   vocals' fundamentals all sit in the bulge; the lift at 4 kHz keeps that octave in the genre's window once the
#   louder bass has raised the 1 kHz octave it is read against;
# - the pump: the pad, the arp and the returns ducked under every kick (PUMP: down 3 dB over 2 ms, held 10 ms, back
#   over a raised-cosine 180 ms, so whole by 192 ms, inside the next 8th's 231 ms), the sci-fi club breath the standard
#   asks for (2-4 dB, back by the next 8th); round 2 pumped only the drop glide. The bass needs none: it rests on
#   every kick;
# - the kick's room: the vocals, the zaps and the fill ducked under every kick, shorter than the pump (KICK_ROOM: down
#   3 dB over 2 ms, held 15 ms, back by 117 ms). Round 3's loudest moments were the kick with something on it: "pull me"
#   and "into orbit" on the kick and clap, the fill's first laser on bar 4's downbeat; each set a peak about 1 dB over
#   the next, and the master's drive is capped by the loudest one;
# - the clap's snap (CLAP_SNAP, a lane insert, a transient shaper's attack): its 1-5 kHz band 3 dB up over the first
#   12 ms of every backbeat clap, back by 25 ms, so the clap's onset stands over the music (the A&R review: the clap was
#   buried, its jump over the 60-10 ms before it 10.0 dB in the club loop against the standard's 12, round 2's dry loop
#   12.1). Its body and tail are the file's, and its fader stays under the kick's.
CLAP_SNAP = dict(db=3.0, hz=(1000.0, 5000.0), full_s=0.012, end_s=0.025, min_vel=0.8)
VOX_COMP = dict(gr_db=6.0, ratio=4.0, knee_db=6.0, attack_s=0.005, release_s=(0.080, 0.500))
VOX_MAKEUP_DB = {'chops': 4.0, 'robot': 4.5}
DRUM_BUS = ('kick', 'clap', 'hat')
DRUM_CLIP_DB = 0.0
DRUM_KNEE_DB = 1.5
MUSIC_EQ = ((900.0, -3.5, 0.8), (4000.0, 1.5, 0.8))   # (Hz, dB, Q) each
PUMP = dict(depth_db=3.0, ramp=0.002, hold=0.010, release=0.180)
PUMPED = ('pad', 'arp')                   # and the returns
KICK_ROOM = dict(depth_db=3.0, ramp=0.002, hold=0.015, release=0.100)
ROOMED = ('zap', 'chops', 'robot', 'fill')


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


# the arp's filter ride (round 3): a resonant 2-pole low-pass (TPT SVF) whose cutoff rises over 3 bars, from lo_hz on
# bar 1's downbeat to hi_hz by top_step (log frequency, raised cosine), then falls back while the arp rests in the fill
# bar, so the loop restarts dark. The standard's change 5 (a cutoff ride over the 4 bars): the signature evolves, the
# energy builds into the fill. The automation the engine plays on the arp's filter
ARP_RIDE = dict(lo_hz=1600.0, hi_hz=16000.0, top_step=46, q=1.4)
RIDE_RUN_IN = 0.5                         # seconds of the loop's own end the filter runs in from


def ride_hz():
    """The ride's cutoff per sample over the loop, Hz."""
    lo, hi, top = ARP_RIDE['lo_hz'], ARP_RIDE['hi_hz'], G.at(ARP_RIDE['top_step'])
    t = np.arange(L)
    u = np.where(t < top, 0.5 - 0.5 * np.cos(np.pi * t / top), 0.5 + 0.5 * np.cos(np.pi * (t - top) / (L - top)))
    return lo * (hi / lo) ** u


def filter_ride(x):
    """The arp lane through its ride, each side through the same filter, round the loop."""
    fc, n = ride_hz(), int(RIDE_RUN_IN * SR)
    out = np.empty_like(x)
    for c in range(2):
        u = np.concatenate([x[-n:, c], x[:, c]])
        out[:, c] = D.svf_tpt(u, np.concatenate([fc[-n:], fc]), q=ARP_RIDE['q'])[0][n:]
    return out


def arp_lane():
    """The arp phrase twice through its filter ride, ducked and carved under the vocals and carved before bar 1's
    backbeats, resting in the fill bar."""
    _, got, _ = Ph.kit_files(K, 'ChromeArpPhrase')
    x = filter_ride(tile(got[(None, 1, 1)].audio)) * D.undb(ride_db(ARP_DUCK))[:, None]
    top = Ph.circular(lambda u: D.bfilt(u, CARVE_HZ, 'bandpass', order=2, zero_phase=True), x)
    x = x - (1 - D.undb(ride_db(ARP_CARVE + ARP_BACKBEAT)))[:, None] * top
    return x * rest_mask()[:, None]


def zap_lane():
    """The zaps on the grid, each at its own pan (ZAP_PAN), round robins in turn, layers by velocity."""
    item, got, _ = Ph.kit_files(K, 'Zap')
    buf = np.zeros((L, 2))
    for k, ((step, vel), p) in enumerate(zip(ZAP, ZAP_PAN)):
        Ph.place(buf, Ph.pan(got[(None, layer_of(item, vel), 1 + k % item.rr)].audio, p), G.at(step))
    return buf


# the vocoder's chorus (round 3): its side is the robot itself read through a delay swaying round CHORUS's base time,
# high-passed, so L = robot + side and R = robot - side: wide, and the mono sum is the robot exactly (round 2's robot
# was mono). The sway completes a whole number of cycles per loop, so the loop joins
CHORUS = dict(base_s=0.011, depth_s=0.0035, cycles=4, level=0.55, hp_hz=1000.0)


def chorus(x, base_s, depth_s, cycles, level, hp_hz):
    """A lane with a chorus on its side (see CHORUS), round the loop."""
    m = D.mono(x)
    n = len(m)
    t = np.arange(n)
    pos = t - (base_s + depth_s * np.sin(2 * np.pi * cycles * t / n)) * SR
    i = np.floor(pos).astype(int)
    fr = pos - i
    y = (1 - fr) * m[i % n] + fr * m[(i + 1) % n]
    side = level * Ph.circular(lambda u: D.bfilt(u, hp_hz, 'highpass', order=2), y)
    return np.stack([m + side, m - side], 1)


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
    raw = {'kick': kick_raw(), 'clap': hits('ElectroClap', CLAP), 'hat': hat_lane(), 'zap': zap_lane(),
           'pad': pad_lane(), 'arp': arp_lane(), 'robot': robot_lane(), 'fill': fill_lane()}
    return raw, tuple(chop_parts())


@functools.lru_cache(maxsize=None)
def bass_raw(char):
    return bass_lane(char)


def low_lanes(char):
    """The kick and bass lanes after their faders: what lanes(char) holds for them, without the other lanes (no
    vocal models needed)."""
    return {'kick': faded('kick', kick_raw(), char), 'bass': faded('bass', bass_raw(char), char)}


def vox_comp(x, lane):
    """The vocal bus's compressor on one vocal part (they never overlap: the call, then each answer), made up."""
    y, _ = MS.glue(x, **VOX_COMP)
    return y * D.undb(VOX_MAKEUP_DB[lane])


def snap_gain():
    """The clap snap's gain on its band, a factor per sample over the loop: 1 + (undb(db) - 1) x a window that is 1
    for full_s from each backbeat clap's onset and falls (raised cosine) to 0 by end_s."""
    s = CLAP_SNAP
    t = np.arange(int(s['end_s'] * SR)) / SR
    w = np.where(t < s['full_s'], 1.0, 0.5 + 0.5 * np.cos(np.pi * (t - s['full_s']) / (s['end_s'] - s['full_s'])))
    env = np.zeros(L)
    for step, v in CLAP:
        if v >= s['min_vel']:
            i = (G.at(step) + np.arange(len(w))) % L
            env[i] = np.maximum(env[i], w)
    return 1.0 + (D.undb(s['db']) - 1.0) * env


def clap_snap(x):
    """The clap lane with its snap (CLAP_SNAP): its band, zero phase round the loop, lifted on each onset."""
    band = Ph.circular(lambda u: D.bfilt(u, CLAP_SNAP['hz'], 'bandpass', order=2, zero_phase=True), x)
    return x + (snap_gain() - 1.0)[:, None] * band


@functools.lru_cache(maxsize=None)
def lanes(char):
    """Each lane as the loop hears it, after its fader, pan and inserts (the bass in character `char`); the chops'
    parts, the same way; and the kick onsets. The buses come after (mixdown)."""
    raw, parts = raw_lanes()
    parts = tuple(vox_comp(faded('chops', p, char), 'chops') for p in parts)
    out = {}
    for k in LANES:
        out[k] = sum(parts) if k == 'chops' else faded(k, bass_raw(char) if k == 'bass' else raw[k], char)
    out['robot'] = chorus(vox_comp(out['robot'], 'robot'), **CHORUS)
    out['clap'] = clap_snap(out['clap'])
    return out, parts, kick_onsets()


def drum_ceiling_db(mix):
    """The drum bus clip's ceiling, dBFS: DRUM_CLIP_DB over the kick lane's own true peak (read round the loop)."""
    return MM.true_peak_db(mix['kick']) + DRUM_CLIP_DB


def music_eq(x):
    """The music bus's EQ (MUSIC_EQ), round the loop."""
    def eq(u):
        for band in MUSIC_EQ:
            u = D.peaking(u, *band)
        return u
    return Ph.circular(eq, x)


def pump(kw=None):
    """A ducker's gain per sample, a factor (n, 1), keyed to the loop's kicks: the pump (PUMP) by default."""
    return D.undb(Ph.pump_db(kick_onsets(), L, **(kw or PUMP)))[:, None]


def mixdown(mix, ret=None):
    """The lanes through the buses (see 'the buses' above), plus the returns when given: the loop."""
    drums, _ = MS.soft_clip(sum(mix[k] for k in DRUM_BUS), drum_ceiling_db(mix), DRUM_KNEE_DB, 4)
    p, room = pump(), pump(KICK_ROOM)
    duck = lambda k: p if k in PUMPED else (room if k in ROOMED else 1.0)
    music = sum(mix[k] * duck(k) for k in LANES if k not in DRUM_BUS + ('bass',))
    if ret is not None:
        music = music + sum(ret.values()) * p
    return drums + mix['bass'] + music_eq(music)


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
                    'backbeat_carve': [list(r) for r in ARP_BACKBEAT], 'carve_hz': list(CARVE_HZ), 'ramp_s': ARP_RAMP,
                    'pre_s': ARP_PRE, 'rest_from_step': ARP_REST[0], 'rest_fades_s': list(ARP_REST[1:])}}


INSERT = {'glide_sidechain': {**GLIDE_DUCK, 'on': 'the kicks under the drop glide'},
          'arp_rides': 'the arp lane: volume and a 1.2-6.5 kHz carve automated under the vocals and before bar 1\'s '
                       'backbeats, a rest in the fill bar', 'arp_filter_ride': ARP_RIDE,
          'vocal_compressor': {**VOX_COMP, 'makeup_db': VOX_MAKEUP_DB, 'on': list(VOCALS)},
          'vocoder_chorus': {**CHORUS, 'on': 'robot'}, 'zap_pans': list(ZAP_PAN),
          'clap_snap': {**CLAP_SNAP, 'on': 'every clap of min_vel or more'}}
NOT_MODELLED = 'the engine\'s voice stealing; the master (ContextLoopClub has it)'


def bus_meta(mix):
    return {'drum_bus': {'lanes': list(DRUM_BUS), 'clip': 'master.soft_clip, 4x', 'ceiling_over_kick_db': DRUM_CLIP_DB,
                         'knee_db': DRUM_KNEE_DB, 'ceiling_dbfs': round(drum_ceiling_db(mix), 4)},
            'vocal_bus': {'lanes': list(VOCALS), 'compressor': 'master.glue', **VOX_COMP, 'makeup_db': VOX_MAKEUP_DB},
            'music_bus': {'lanes': [k for k in LANES if k not in DRUM_BUS + ('bass',)], 'returns': 'in the wet loop',
                          'eq': [{'peaking_hz': f, 'gain_db': g, 'q': q} for f, g, q in MUSIC_EQ]},
            'pump': {**PUMP, 'keyed_to': 'the kicks', 'lanes': list(PUMPED) + ['the returns']},
            'kick_room': {**KICK_ROOM, 'keyed_to': 'the kicks', 'lanes': list(ROOMED)}}


# ------------------------------------------------------------------ the files
def dry(char):
    mix, _, _ = lanes(char)
    meta = {'grid': GRID, 'bass': char, 'lanes': lane_meta(mix, char), 'parts': parts_meta(char), 'insert': INSERT,
            'buses': bus_meta(mix), 'not_modelled': NOT_MODELLED}
    return Out(mixdown(mix), meta=meta)


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
    space = two_pass(lambda u: S.space(u, K.space_rng('send'), **SPACE), D.mono(send_bus(mix, parts, 'space')))
    key = sum(mix[k] for k in VOCALS)
    key = np.concatenate([key, key])
    space = two_pass(lambda u: S.duck(u, key, **S.DUCK)[0], space)
    delay = S.ECHO_STEPS * K.STEP
    echo = two_pass(lambda u: S.echo(u, K.space_rng('echo'), delay, **ECHO), D.mono(send_bus(mix, parts, 'echo')))
    res = two_pass(lambda u: S.resonator(u, resonator_freqs(), **S.RESONATOR),
                   D.mono(send_bus(mix, parts, 'resonator')))
    return {'space': space * D.undb(SPACE_RETURN_DB), 'echo': echo * D.undb(ECHO_RETURN_DB),
            'resonator': res * D.undb(S.RESONATOR_RETURN_DB)}


def wet():
    """The wet loop and its metadata: the lanes with bass B through the buses, plus the three returns."""
    char = WET_BASS
    mix, _, _ = lanes(char)
    ret = returns(char)
    delay = S.ECHO_STEPS * K.STEP
    space_meta = {**SPACE, 'return_db': SPACE_RETURN_DB, 'seed': [K.SPACE_SEED, 'send'], 'duck_under': 'vocals',
                  **{f'duck_{k}': v for k, v in S.DUCK.items()}}
    echo_meta = {**ECHO, 'delay_steps': S.ECHO_STEPS, 'delay_s': round(delay, 6), 'return_db': ECHO_RETURN_DB,
                 'seed': [K.SPACE_SEED, 'echo']}
    res_meta = {**S.RESONATOR, 'return_db': S.RESONATOR_RETURN_DB, 'tones_midi': list(RESONATOR_TONES),
                'ratios': list(K.RATIOS[:4]), 'stand_in': 'a 2-pole bank on one chord; the engine retunes per chord'}
    meta = {'target': 'the target the engine must match', 'grid': GRID, 'bass': char,
            'bass_why': 'the picked line in B, warm: the body round 1 missed in the hollow reese',
            'lanes': lane_meta(mix, char), 'parts': parts_meta(char), 'insert': INSERT, 'buses': bus_meta(mix),
            'throws': [{'step': s, 'chop': n, 'space_send': t[0], 'echo_send': t[1]} for s, n, t in CHOPS if t],
            'space': space_meta, 'echo': echo_meta, 'resonator': res_meta,
            'return_lufs': {k: round(qa.lufs(v), 2) for k, v in ret.items()},
            'not_modelled': NOT_MODELLED + ", the resonator's retuning"}
    return mixdown(mix, ret), meta


def context_wet(take):
    y, meta = wet()
    return Out(y, meta=meta)


# The club master's cap: the peak reduction (clipper and limiter together) it may use toward the genre's -8.5 LUFS-I.
# The standard's convention is 4 dB. Round 3's first pass raised it to 6 dB and reached -8.5 with 5.58 dB, which
# flattened the kick and the clap (the A&R review): each kick's crest in the loop (qa.kick_crest_db over the 200 ms from
# it) lost 2.59 dB through the master (the median of the kicks' losses), each clap's 2.68 (true peak of its first 30 ms
# over its loudest 50 ms), and one kick read 3.55 dB, under the crest floor's 4.0. Measured on wet(), the loop this
# master gets, as mixed now:
#   cap 4.0 dB:  -9.61 LUFS-I, 0.11 LU under the genre's window (-9.5 to -7.5); kick crest loss 1.29 dB, clap 1.48
#   cap 4.25 dB: -9.41, 0.09 LU inside it; kick 1.47, clap 1.68, every kick's crest 4.27 dB or more under the master
#   cap 4.5 dB:  -9.22; kick 1.66, clap 1.86
# 4.25 is the least reduction that lands inside the window, and the kicks lose at most the review's 1.5 dB. The
# ContextLoopWet file is that loop after the renderer's 20 Hz high-pass: through the master it reads 0.1 LU louder
# (-9.51 at 4 dB, -9.31 at 4.25; 5.30 dB of reduction for -8.5 against the input's 5.44), and its kicks read 0.35 dB
# more crest (median; most likely the high-pass's phase shift on the kick's 55 Hz sub moves where its peak lands), so
# against the file the loss at 4.25 reads 1.81 dB.
# The loop stops 0.9 LU short of -8.5: the kit's tight kick (its sub 20 dB down by 122 ms) and a bass that rests on
# every kick keep its PLR high by design, and density that adds no peaks would change the balance. The report records
# the cap; the engine's master must match it
MASTER_REDUCTION_DB = 4.25
CLUB_TARGET_LUFS = qa.CLUB[K.DIRECTION]['lufs'][0]     # the genre's target, -8.5 (the master's settings default)


def club_hits():
    """The hits the club master reports on (master.punch) and QA reads: every kick (all at full velocity) and the
    backbeat claps (velocity 0.8 and over), as onsets."""
    return {'kick': [G.at(s) for s, v in KICK if v == 1.0], 'clap': [G.at(s) for s, v in CLAP if v >= 0.8]}


def context_club(take):
    """The wet loop through the club master (tools/sound/master.py) at the genre's settings (MS.settings(K): sci-fi
    club, -8.5 LUFS-I, PLR at least 7 dB) with the kit's cap (MASTER_REDUCTION_DB, declared on the item): the loop as a
    club record plays it. The master's report is in the metadata, with the wet loop's: the settings the engine's
    master must match."""
    y, meta = wet()
    hits = club_hits()
    z, report = MS.master(y, MS.settings(K, max_reduction_db=MASTER_REDUCTION_DB), hits=hits)
    return Out(z, top_hits=hits['kick'],
               meta={**meta, 'master': report, 'target': 'the target the engine must match, through its master'})


def low(char):
    mix = low_lanes(char)
    meta = {'grid': GRID, 'bass': char, 'lanes': lane_meta(mix, char, LOW), 'parts': {'bass': parts_meta(char)['bass']},
            'insert': {'glide_sidechain': INSERT['glide_sidechain']}}
    return Out(sum(mix[k] for k in LOW), hits=kick_onsets(), top_hits=club_hits()['kick'], meta=meta)


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
CLUB_QA = {'mud': 'off', 'club_max_reduction_db': MASTER_REDUCTION_DB, 'club_crest_loss_db': 1.6,
           'why': 'the full mix, mastered: vocals, clap, pad and robot belong in 150-400 Hz; its low end is gated as '
                  'ContextLoopLow and ContextLoopLowWarm, and every other lane passed its own low cut. The master\'s '
                  f'cap is {MASTER_REDUCTION_DB:g} dB, over the standard\'s 4 (at 4 dB the loop stops at -9.61 LUFS-I, '
                  '0.11 LU under the window): the least that lands in the window, for your decision by ear. It costs '
                  'the claps 1.53 dB of crest and the kicks 1.47 (the median per hit), over the 1.5 the A&R review '
                  'proposed: declared at 1.6 until your ears calibrate that limit'}
R3 = ('r2', 'r3')
ITEMS = [
    Item('Loops', 'ContextLoopClub', 'loop', context_club, loop=True, hp_hz=None, sets=('r3',), tags=('wet', 'club'),
         qa=CLUB_QA,
         about=f"{ABOUT}, the wet loop through the club master (sci-fi club: toward {CLUB_TARGET_LUFS:g} LUFS-I, at "
               f"most {MASTER_REDUCTION_DB:g} dB of peak reduction): the loop as a club record plays it"),
    Item('Loops', 'ContextLoop', 'loop', context_dry, loop=True, sets=R3, tags=('dry',), qa=FULL_MIX,
         about=f'{ABOUT}, dry, bass A (clean): the kit files as the sampler plays them, mixed on the buses'),
    Item('Loops', 'ContextLoopWarm', 'loop', context_warm, loop=True, sets=R3, tags=('dry',), qa=FULL_MIX,
         about=f'{ABOUT}, dry, bass B (warm), level-matched to A'),
    Item('Loops', 'ContextLoopWet', 'loop', context_wet, loop=True, sets=R3, tags=('wet',), qa=FULL_MIX,
         about=f"{ABOUT}, wet, bass B (warm), the picked Orbit line: Space (FDN, a little shimmer), Echo (tape "
               "ping-pong) and the resonator on the kit ratios, a throw on the last chop; the engine's target"),
    Item('Loops', 'ContextLoopLow', 'loop', context_low, loop=True, sets=R3, tags=('with_kick',),
         qa={'mud': 'gate', 'why': 'the kick and bass A lanes as mixed: gated in every gap between its kicks'},
         about=f'{ABOUT}: the kick and bass A (clean) lanes alone, mud-gated in every gap between kicks'),
    Item('Loops', 'ContextLoopLowWarm', 'loop', context_low_warm, loop=True, sets=R3, tags=('with_kick',),
         qa={'mud': 'bass-body', 'why': f'{bass.WHY_WARM}: the kick and bass B lanes as mixed, in every gap'},
         about=f'{ABOUT}: the kick and bass B (warm) lanes alone, under the bass-body rule in every gap between kicks'),
]
