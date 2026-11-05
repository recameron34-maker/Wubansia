"""FUT04 "Furnace Code" context loop for listening rounds 2 and 3: 4 bars at 136 BPM in E minor, written for this kit
and played on its files, so each sound is heard where it will live.

Round 1 rated FUT01's loop Cool 4 with one fix: "Reese doesnt sound that musical and doesnt make me want to dance. The
high plucks are nice." So this loop is built on a bassline that dances with the kick and keeps the kit's melodic top,
the modular blips, up front.

- ContextLoopClub (round 3): the wet loop through the club master (tools/sound/master.py, ADR 0014, proposed) at the
  techno target (-8 LUFS-I, a PLR of at least 6.5 dB): the loop as a club record plays it. Tag club: QA checks it
  against the genre's targets (qa.CLUB). The master's report is in its metadata: the settings the engine's master must
  match (S1e).
- ContextLoop: dry, with bass A (clean). The kit's files exactly as written, played the way the sampler will: hits on
  the 16th grid, every lane on the kit's one swing (K.SWING, 0.54), a velocity picks the nearest layer, round robins
  cycle, the open hat is choked by the next closed hat (drums.CHOKE), the drone is held right through the loop point.
  A fader and a pan per lane, the ducks (the bass lane's band over 500 Hz and the fill's over 2.5 kHz dip about 6 dB
  under the vocals, DIPS, so the words read), and the buses (below), no master.
- ContextLoopWarm: the same loop with bass B (warm). Both bass lanes are set to one loudness (LANE_LUFS['bass']) before
  the dip, so the two dry loops differ only in the bass's character: B is A's sub and squelch plus a body from 120 Hz
  (bass.character).
- ContextLoopWet: the loop with the picked line in B, warm (WET_BASS: the integrator's pick, since the bass designer
  named a line, AcidLine1, but no character; B is round 1's answer to the hollow reese, body on small speakers, and the
  bass sends nothing, so the A/B in the dry loops is untouched), plus the three sends in their Python versions
  (tools/sound/sends.py) at the kit's own settings (SPACE, ECHO): Space, the kit's dark hall (round 3: 5 s, damped
  from 3.5 kHz), seeded by the kit's dark concrete space and ducked under the vocals and under every kick; Echo, the
  tape ping-pong on a dotted 8th, darker than the shared one (a dub delay); and the resonator stand-in, 2-pole modes on
  the drone's chord two octaves up times the kit's iron ratios (K.RATIOS[:4]). The last chop is a throw (full Space
  and Echo send) into the fill bar. The target the engine must match (S1e): its settings are in the file's metadata in
  qa_report.json.
- ContextLoopLow: the kick and bass A lanes alone (the acid ride and the rumble), as they sit in the loop: the kick out
  of the drum bus as it is when it plays alone, the bass without its dip under the vocals (over 500 Hz, which the
  low-end rules don't read), mud-gated over the loop and in every gap between its kicks (tag with_kick).
  ContextLoopLowWarm: the same with bass B, under the bass-body rule (ADR 0011).

The mix (round 3). Round 2's loop was lanes summed: -17.1 LUFS-I as rendered, a PLR of 15.7 dB, and the club master
stopped 5.3 LU short of the techno target (-13.3 LUFS) at the standard's 4 dB cap, needing 10.6 dB of peak reduction
(docs/plans/S1-modern-sound.md). Its peaks were not the kick: the acid's accents sat 5.4 dB over the kick's own true
peak, the snare 3.9, the fill 1.7 and the hats' ticks 1.3. Round 3 mixes on buses, as warehouse techno is mixed:
- the drum bus (kick, snare, hats, ride, anvil, fill): a parallel grit, the bus high-passed at 250 Hz, lifted at
  3 kHz, driven through a tanh at 4x, dipped at 600 Hz and low-passed at 9 kHz, mixed 8 dB under (DRUM_GRIT: the
  standard's multi-stage distortion with EQ between the stages, none of it under 250 Hz, so the sub stays clean); then
  a soft clip (master.soft_clip, 4x) at the kick lane's own true peak (DRUM_CLIP), so the backbeat, the ticks and the
  fill's roll stop setting the loop's peak, and the kick's very top loses a little;
- the vocal bus (round 3 change 7): the chops band-passed (120 Hz-8 kHz) with a 3 dB body at 250 Hz, driven at 4x,
  compressed (4:1, 6 dB at its loudest) and given two short slap delays (a 16th left, an 8th right); the robot keeps
  its own chain, already driven and compressed (through the bus Whisper lost its words). Both sit 2.5 LU lower, under
  the kick (VOX_BUS); the sends hear them as the loop does;
- the pump: the drone 3 dB and the returns 5 dB down under every kick, back by 210 and 270 ms (PUMP, RETURN_PUMP: the
  standard's "returns and rumble ducked; pads 2-4 dB"); the rumble pumps itself (bass.PUMP), the bass rests under
  every kick (bass.DUCK), and (the fix pass) the bass lane's sub, under 120 Hz, ducks 8 dB under each kick's first
  250 ms, back by 332 ms (BASS_SIDECHAIN, a band-split sidechain like the dips under the vocals), so the kick leads its
  own sub: the line's pedal E1, in phase with the kick's tail, had stacked onto it;
- the music bus (drone, blips, vocals and the returns): a 3 dB body at 250 Hz, a 1 kHz dip and a presence lift
  (MUSIC_EQ), filling the kit's low-mid hole from the parts the mud rules leave free (the kick, the rumble and bass A
  keep the strict gate, bass B the bass-body rule), against the blips' and the acid's 1 kHz octave.

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
- Ride (round 3, rebuilt in the fix pass): the iron ride on every off-beat 8th with the open hat (drums.RIDE_HITS),
  not choked: a ping and a noise wash that fades 20 dB before the next off-beat, 3 LU under the hats; it stops with
  the hats.
- Bass: the designers' pick, AcidLine1, "the rolling hook" (bass.PICK), as its 4-bar ride (round 3, bass.RIDES: the
  line twice, its cutoff and resonance opening through bar 3 and widest on the fill bar). It rests on every kick and
  answers in the gap: root on the 16th after the kick, an accented leap on the off-beat 8th whose target changes (E2,
  G1 sliding to A1, D2), a squelch-only tick on beat 3's kick, and bar 1 closes on a held B1; bar 2 brings the hook
  back varied (E2 sliding to D2), holds a dark C2 for a beat, peaks on G2 and climbs B1 D2 E2 home into the downbeat.
  Its slides, accents, ghosts and ticks are the acid voice's own (bass.LINES). Under each beat's first 250 ms its sub
  holds E1 in phase with the kick's tail (bass.SUB_PEDAL_S), so the low end adds up and never beats, and the mix
  ducks that sub 8 dB under the kick's first 250 ms (BASS_SIDECHAIN), so it fills in as the kick's tail fades.
- Rumble, the second bass gesture, under all 4 bars (round 3; round 2: bars 3 and 4): its 2 bars twice, the kick-fed
  sub in phase with the kick's tail (bass.RUMBLE_PHASE), swelling in as that tail fades and peaking 270-300 ms after
  each kick (bass.PUMP, the fix pass), so the kick leads and the rumble carries the gap, and gone before the next kick;
  it drops out with the kick on beat 4 of bar 4.
- Drone: Em(add9), its 4-bar loop region as the sampler holds it, read so the filter is darkest at bar 1 and most open
  at bar 3 (its sweep runs once per loop).
- Lead, up front: the modular blip sequence (2 bars, twice), the kit's signature. Call and response: the anvil answers
  in the sequence's gaps, on the swung 16th after its motif in bars 1-3, and round bar 2's C6 ratchet (before it,
  and a ghost after it).
- Vocals, call and response: the robot calls "Fill the furnace" on bar 2's downbeat; the sung line answers, "come
  down" on bar 3's downbeat, then "the furnace" with "fur" on bar 4's downbeat, thrown into the fill (wet loop).
- Fill: the stutter fill, all of bar 4 over the kick on beats 1-3 and the snare, its 64th roll landing on the loop's
  downbeat.
Everything loops: hits and tails wrap round the loop, every filter, ducker and clip runs round it, the sends run over
two passes, keeping the second.

The three full loops and the club loop need the vocal models (the chops and the robot: see tools/sound/README.md).
The low-end loops and the previews don't: each lane renders only its own files (raw_lane).
"""
import functools

import numpy as np

from tools.sound import dsp48 as D
from tools.sound import master as MS
from tools.sound import mixmeter as MM
from tools.sound import parts as P
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
RIDE = tuple((s + 32 * p, v) for p in (0, 1) for s, v in drums.RIDE_HITS if s + 32 * p < HAT_END)   # round 3
ANVIL = ((9, 0.8), (25, 1.0), (27, 0.4), (41, 0.8))   # in the blip sequence's gaps, on swung 16ths: round the C6
                                                      # ratchet in bar 2, before it and a ghost after it
LINE = bass.PICK                          # the bass designer's pick of the two lines: AcidLine1, the rolling hook
BASS_RIDE = True                          # round 3: the bass lane is the pick's 4-bar ride (bass.RIDES); round 2 played
                                          # the 2-bar line twice
LEAD_AT = (0, 32)                         # the 2-bar blip sequence, twice
RUMBLE_AT = (0, 32)                       # round 3: the rumble's 2 bars from these 16ths, all 4 bars (round 2: (32,))
ROBOT_AT = 16                             # "Fill the furnace": its step 0 ("fill") on bar 2's downbeat
THROW = (1.0, 1.0)                        # a throw (CC91-style): the chop's full Space and Echo send
CHOPS = ((32, 'ChopComeDown', None), (46, 'ChopTheFurnace', THROW))   # the chop's first note lands on the 16th (even
                                          # 16ths, so the chop's own swung timing lines up); its sends (None: the
                                          # lane's)
MUTE_EDGE = 0.005                         # the rumble's mute under KICK_OUT: a 5 ms raised cosine, in its own silence

# ------------------------------------------------------------------ the mix: source, pan, the three sends
# Each lane's fader is set so the lane reads LANE_LUFS over the loop (LUFS-I, after its inserts, before the buses, the
# loop's one gain, the bass's dip under the vocals and its sidechain under the kicks). Round 2 set the kick -17, the
# chops level with it and the robot 0.5 LU under, the acid 1.5 LU under, the blips 2.5 under, the drone and the hats
# well back, the rumble 8 LU under. Round 3 lets the kick lead (the lanes summed 6.0 LU over it, the most of the four
# kits): the vocals 2.5 LU down (processed on their bus, so they still read), the rumble under all 4 bars (the fix pass
# set it 13 LU under the kick, round 3 5 LU, the standard's 4-6: it is nearly all sub, which K-weighting reads low, and
# its later, shorter swell, in phase with the kick's tail, now sits under the kick's own 35-60 Hz peak, see bass.PUMP),
# the hats 1 LU up and the ride on the off-beats (3 LU under the hats since the fix pass: round 3 first put it 1 LU
# under), the drone up with its lifted low mids. Both bass characters read the same, so
# the A/B is the sound alone. The meta records every fader. The ride is stereo itself (pan 0); the snare and the anvil
# go into the hall harder (the standard: stabs and percussion sent hard)
LANES = {
    'kick': ('KickSplit', 0.0, 0.0, 0.0, 0.0),
    'bass': (None, 0.0, 0.0, 0.0, 0.0),             # its file per character: line_name()
    'rumble': ('Rumble', 0.0, 0.0, 0.0, 0.0),
    'snare': ('SnareGated', 0.0, 0.30, 0.0, 0.0),
    'hat': ('ClosedHat, OpenHat', 0.15, 0.08, 0.10, 0.10),
    'ride': ('Ride', 0.0, 0.10, 0.05, 0.0),
    'perc': ('Anvil', -0.25, 0.80, 0.50, 0.10),
    'pad': ('DroneEmAdd9', 0.0, 0.60, 0.0, 0.0),
    'lead': ('ModBlipPhrase', 0.0, 0.25, 0.20, 0.05),
    'chops': (', '.join(n for _, n, _ in CHOPS), 0.0, 0.80, 0.60, 0.20),
    'robot': (vox.ROBOT_ITEM, 0.0, 0.60, 0.40, 0.0),
    'fill': ('StutterFill', 0.0, 0.25, 0.25, 0.0),
}
LANE_LUFS = {'kick': -17.0, 'bass': -18.5, 'rumble': -30.0, 'snare': -20.0, 'hat': -25.0, 'ride': -28.0,
             'perc': -24.0, 'pad': -22.0, 'lead': -20.0, 'chops': -19.5, 'robot': -20.0, 'fill': -21.0}
LOOP_LUFS = -13.8                         # the three full loops at one loudness, so dry A, dry B and wet compare by
                                          # sound: just under the quietest one's level with its peak at the ceiling
                                          # (dry A, -13.7; dry B -13.65, the wet loop -13.2; round 2: -17, its loops
                                          # read -16.9 to -15.4 at the ceiling)
SENDS = {'space': 2, 'echo': 3, 'resonator': 4}   # column of each send in LANES
VOCALS = ('chops', 'robot')               # the Space return and the lanes' top bands (DIPS) duck under these
BASS_DUCK_HZ = 500.0                      # the bass lane's band over this dips under the vocals, so the words read
FILL_DUCK_HZ = 2500.0                     # ...and the fill's (its 2.5-9 kHz hiss): in the fill bar it covered the
                                          # consonants of "the furnace" at 2.4-4.8 kHz
DIPS = {'bass': BASS_DUCK_HZ, 'fill': FILL_DUCK_HZ}
BASS_DUCK = dict(depth_db=9.0, range_db=30.0, attack=0.010, release=0.150)   # sends.duck (the engine's ducker, as on
                                          # the Space return), for every dip: about 6 dB while a word sounds
LOW = ('kick', 'bass', 'rumble')
RESONATOR_TONES = tuple(n + 24 for n in pad.CHORD)   # the drone's Em(add9) two octaves up, E5 B5 E6 G6 F#7, times
                                          # RATIOS[:4] (the anvil's first bending mode, its warbling pair and the iron
                                          # third): modes from 0.66 to 7.2 kHz, where the hats, anvil, blips and chops
                                          # reach, out of the low mids

# the kit's own sends (round 3; the standard's S3: each kit passes its own Space and Echo, sends.py keeps the defaults).
# Space: the dark concrete hall of the standard's FUT04 change 5, 5 s (shared: 2.6 s), damped from 3.5 kHz (6.5), a
# pre-delay of a 64th note (27.6 ms at 136 BPM; shared 20 ms) and half the shimmer; its low mids still decay in 0.8 s,
# and its send is high-passed at 250 Hz (shared 450), so the hall carries a little body into the kit's low-mid hole.
# Echo: a dub delay on the dotted 8th, darker (LP 2.2 kHz, shared 2.6) and thinner (HP 400 Hz, shared 300)
SPACE = {**S.SPACE, 't60': 5.0, 'damp_hz': 3500.0, 'predelay': K.STEP / 4, 'shimmer': 0.03, 'send_hp': 250.0}
ECHO = {**S.ECHO, 'lp_hz': 2200.0, 'hp_hz': 400.0}

# ------------------------------------------------------------------ the buses (round 3)
DRUM_BUS = ('kick', 'snare', 'hat', 'ride', 'perc', 'fill')
DRUM_GRIT = dict(hp_hz=250.0, pre=(3000.0, 3.0, 0.7), drive=2.5, mid=(600.0, -3.0, 1.0), lp_hz=9000.0,
                 level_db=-8.0)           # the parallel grit: high-pass (4th order), presence (Hz, dB, Q), a tanh at
                                          # 4x around the kick lane's true peak, a dip (Hz, dB, Q), a low-pass (2nd
                                          # order), mixed this far under the bus
DRUM_CLIP = dict(over_kick_db=0.0, knee_db=1.5)   # the bus's soft clip: its ceiling over the kick lane's true peak,
                                          # linear up to knee_db under it (master.soft_clip)
VOX_BUS = dict(hp_hz=120.0, lp_hz=8000.0, body=(250.0, 3.0, 0.9), drive=1.8,
               comp=dict(gr_db=6.0, ratio=4.0, knee_db=6.0, attack_s=0.005, release_s=(0.080, 0.500)),
               slaps=((1.0, -9.0, -0.6), (2.0, -13.0, 0.6)), slap_band=(400.0, 4000.0))
                                          # band-pass (2nd order each side), a body lift (Hz, dB, Q: the low voice's
                                          # chest, which fills the low mids the vocals' 2.5 LU drop takes away), a tanh
                                          # at 4x around the lane's own peak, a compressor (master.glue: 6 dB at its
                                          # loudest), then short delays: (16ths, dB, pan) each, band-passed; the lane's
                                          # fader comes after (LANE_LUFS)
VOX_BUS_LANES = ('chops',)                # the robot keeps its own chain (already driven and compressed: through the
                                          # bus Whisper heard "and" for "Fill the furnace", without it word for word)
PUMP = dict(depth_db=3.0, ramp=0.002, hold=0.010, release=0.200)          # the drone under every kick
RETURN_PUMP = dict(depth_db=5.0, ramp=0.002, hold=0.020, release=0.250)   # the three returns under every kick
PUMPED = ('pad',)
MUSIC_BUS = ('pad', 'lead', 'chops', 'robot')   # and the returns, in the wet loop
MUSIC_EQ = ((250.0, 3.0, 1.0), (1000.0, -2.5, 0.8), (3500.0, 1.5, 0.8))   # (Hz, dB, Q) each, round the loop
MUSIC_GLUE = None                         # the music bus's compressor (master.glue keywords), or None
BASS_CLIP = None                          # the bass lane's soft clip: (dB over the kick lane's true peak, knee dB), or
                                          # None
BASS_SIDECHAIN = dict(depth_db=8.0, ramp=0.002, hold=0.250, release=0.080, split_hz=120.0)
# the fix pass: the bass lane's sub under every kick (Ph.pump_db's depth, ramp, hold and release, s; split_hz: its band,
# a zero-phase low-pass, so outside the duck the lane is untouched), held over the line's pedal (bass.SUB_PEDAL_S) and
# back by 332 ms. The pedal's E1 starts on the 16th after the kick in phase with the kick's tail, and with the rumble it
# lifted 35-60 Hz 3.6 dB over the kick's own peak; ducked, the kick leads. The standard's techno row: the bass 4-8 dB
# under the kick. None: no sidechain


def layer_of(item, v):
    """The layer nearest a velocity (the louder on a tie), as phrase.hits_lane picks it."""
    return 1 + min(range(len(item.layers)), key=lambda i: (abs(item.layers[i] - v), -i))


def tile(x, times=2):
    """A loop file `times` times: the 4 bars."""
    x = D.st(x)
    if times * len(x) != L:
        raise ValueError(f'a loop of {len(x)} samples does not tile the {L}-sample context loop {times} times')
    return np.concatenate([x] * times)


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
    return f"{LINE}{'Ride' if BASS_RIDE else ''}{'Clean' if char == CLEAN else 'Warm'}"


def bass_raw(char):
    """The bass lane before its fader, in the character: the pick's 4-bar ride (or the 2-bar line twice)."""
    return tile(kit_file(line_name(char)).audio, 1 if BASS_RIDE else 2)


def mute_mask(at, edge=MUTE_EDGE):
    """1 up to the 16th `at`, then a raised cosine down to 0 over `edge`, and 0 to the loop's end."""
    a, e = G.at(at), int(edge * SR)
    g = np.ones(L)
    g[a:a + e] = 0.5 + 0.5 * np.cos(np.pi * np.arange(e) / e)
    g[a + e:] = 0.0
    return g


def rumble_lane():
    """The rumble's 2 bars from each of RUMBLE_AT (round 3: all 4 bars), silent from KICK_OUT: it swells after each
    kick, so with no kick there it rests too. Its pump holds it 40 dB down from 380 ms after each kick, so the mute and
    both ends fall in its own silence."""
    x = kit_file('Rumble').audio
    if max(RUMBLE_AT) and G.at(max(RUMBLE_AT)) + len(x) != L:
        raise ValueError('the rumble must end exactly on the loop\'s downbeat')
    buf = np.zeros((L, 2))
    for at in RUMBLE_AT:
        Ph.place(buf, x, G.at(at))
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
    'ride': lambda: Ph.hits_lane(K, 'Ride', RIDE, G),
    'perc': lambda: Ph.hits_lane(K, 'Anvil', ANVIL, G),
    'pad': pad_lane,
    'lead': lead_lane,
    'robot': robot_lane,
    'fill': fill_lane,
}


def sidechained(x, onsets=None):
    """The bass lane with its sub ducked under every kick (BASS_SIDECHAIN), round the loop: x plus its band under
    split_hz (zero-phase, so in phase with x) times the ducker's gain less one, so outside the ducks it is x exactly.
    onsets: the kicks it ducks under (default: the context loop's; a preview passes its own)."""
    if not BASS_SIDECHAIN:
        return x
    v = BASS_SIDECHAIN
    lo = Ph.circular(lambda u: D.bfilt(u, v['split_hz'], 'lowpass', order=4, zero_phase=True), x)
    kw = {k: v[k] for k in ('depth_db', 'ramp', 'hold', 'release')}
    g = pump(kw) if onsets is None else D.undb(Ph.pump_db(tuple(onsets), len(x), **kw))[:, None]
    return x + lo * (g - 1.0)


# ------------------------------------------------------------------ the vocal bus (round 3)
def vox_bus(x):
    """A vocal part through the vocal bus (VOX_BUS), round the loop: band-pass, drive, compressor, slaps. The part's
    level is kept to its own true peak before the drive, so the fader after it sets the lane."""
    if not VOX_BUS:
        return x
    v = VOX_BUS
    circ = Ph.circular
    y = circ(lambda u: D.bfilt(D.bfilt(u, v['hp_hz'], 'highpass', order=2), v['lp_hz'], 'lowpass', order=2), x)
    if v.get('body'):
        y = circ(lambda u: D.peaking(u, *v['body']), y)
    if v.get('presence'):
        y = circ(lambda u: D.peaking(u, *v['presence']), y)
    ref = float(D.undb(MM.true_peak_db(y)))
    y = D.oversampled(lambda u: P.soft_clip(u / ref, v['drive']) * ref, y)
    y, _ = MS.glue(y, **v['comp'])
    lo, hi = v['slap_band']
    band = circ(lambda u: D.bfilt(u, (lo, hi), 'bandpass', order=2), D.mono(y))
    for steps, db, p in v['slaps']:
        d = int(np.floor(steps * K.STEP * SR + 0.5))
        y = y + Ph.pan(np.roll(band, d), p) * D.undb(db)
    return y


@functools.lru_cache(maxsize=None)
def raw_lane(k, char=WET_BASS):
    """One lane, panned and through its inserts, before its fader (the bass in the character; the chops as the sum of
    their parts). Each lane renders only its own files, so the low-end loops and the previews need no vocal models."""
    if k == 'bass':
        return centred(bass_raw(char), LANES['bass'][1])
    if k == 'chops':
        return sum(chop_parts_bus())
    x = centred(LANE_SOURCES[k](), LANES[k][1])
    return vox_bus(x) if k in VOX_BUS_LANES else x


@functools.lru_cache(maxsize=1)
def chop_parts_bus():
    """The chops' parts through the vocal bus (each part alone: they never overlap)."""
    return tuple(vox_bus(p) if 'chops' in VOX_BUS_LANES else p for p in chop_parts())


@functools.lru_cache(maxsize=None)
def fader_db(k, char=WET_BASS):
    """The lane's fader: the dB that sets it to LANE_LUFS over the loop (the bass per character)."""
    return float(LANE_LUFS[k] - qa.lufs(raw_lane(k, char)))


def mixed(k, char=WET_BASS):
    """One lane after its fader and pan; the bass's sub then sidechained to the kicks (BASS_SIDECHAIN), after the
    fader as a ducker is, so the duck takes level off and the fader doesn't raise the squelch to make up for it."""
    y = raw_lane(k, char) * D.undb(fader_db(k, char))
    return sidechained(y) if k == 'bass' else y


def dipped(x, key, hz=BASS_DUCK_HZ):
    """A lane with its band over hz dipped under the vocals (sends.duck on the vocal lanes, BASS_DUCK, round the loop):
    x plus its high band (zero-phase, so in phase with x and the dip is the dip) times the gain less one, so outside the
    dips it is x exactly; the bass's dip moves nothing under about 250 Hz (the low-end loops leave it out)."""
    hi = Ph.circular(lambda u: D.bfilt(u, hz, 'highpass', order=4, zero_phase=True), x)
    g = S.duck(np.ones(3 * L), np.tile(D.mono(key), 3), **BASS_DUCK)[1][L:2 * L]
    return x + hi * (D.undb(g) - 1.0)[:, None]


@functools.lru_cache(maxsize=None)
def lanes(char):
    """Each lane as the loop hears it, after its fader, pan and inserts, the bass in the character, and the bass and the
    fill with their top bands dipped under the vocals (DIPS); the chops' parts, the same way; and the kick onsets. The
    buses come after (mixdown)."""
    mix = {k: mixed(k, char) for k in LANES}
    key = sum(mix[k] for k in VOCALS)
    for k, hz in DIPS.items():
        mix[k] = dipped(mix[k], key, hz)
    parts = tuple(p * D.undb(fader_db('chops', char)) for p in chop_parts_bus())
    return mix, parts, tuple(G.at(s) for s, _ in KICK)


# ------------------------------------------------------------------ the buses (round 3)
def kick_onsets():
    return tuple(G.at(s) for s, _ in KICK)


def pump(kw):
    """A ducker's gain per sample, a factor (L, 1), keyed to the loop's kicks."""
    return D.undb(Ph.pump_db(kick_onsets(), L, **kw))[:, None]


def grit(x, ref_db):
    """The drum bus's parallel grit (DRUM_GRIT), round the loop: the bus over 250 Hz, lifted, driven around ref_db (the
    kick lane's true peak), dipped and low-passed."""
    v = DRUM_GRIT
    circ = Ph.circular
    u = circ(lambda w: D.peaking(D.bfilt(w, v['hp_hz'], 'highpass', order=4), *v['pre']), x)
    ref = float(D.undb(ref_db))
    u = D.oversampled(lambda w: D.tanh_adaa(w / ref, v['drive']) * ref, u, 4)
    u = circ(lambda w: D.bfilt(D.peaking(w, *v['mid']), v['lp_hz'], 'lowpass', order=2), u)
    return u * D.undb(v['level_db'])


def drum_ceiling_db(mix):
    """The drum bus clip's ceiling, dBFS: DRUM_CLIP's offset over the kick lane's own true peak, round the loop."""
    return MM.true_peak_db(mix['kick']) + (DRUM_CLIP or {}).get('over_kick_db', 0.0)


def drum_bus(mix, keys=DRUM_BUS):
    """The drum bus: the lanes summed, the parallel grit, the soft clip."""
    x = sum(mix[k] for k in keys if k in mix)
    if DRUM_GRIT:
        x = x + grit(x, MM.true_peak_db(mix['kick']))
    if DRUM_CLIP:
        x, _ = MS.soft_clip(x, drum_ceiling_db(mix), DRUM_CLIP['knee_db'], 4)
    return x


def music_eq(x):
    """The music bus's EQ (MUSIC_EQ), round the loop."""
    if not MUSIC_EQ:
        return x

    def eq(u):
        for band in MUSIC_EQ:
            u = D.peaking(u, *band)
        return u
    return Ph.circular(eq, x)


def bass_bus(mix):
    """The bass lane, soft-clipped at the kick's level (BASS_CLIP), so the acid's accents don't set the loop's peak."""
    x = mix['bass']
    if BASS_CLIP:
        over, knee = BASS_CLIP
        x, _ = MS.soft_clip(x, MM.true_peak_db(mix['kick']) + over, knee, 4)
    return x


def mixdown(mix, ret=None):
    """The lanes through the buses, plus the returns when given: the loop."""
    drums = drum_bus(mix)
    duck = lambda k: pump(PUMP) if PUMP and k in PUMPED else 1.0  # noqa: E731
    music = sum(mix[k] * duck(k) for k in MUSIC_BUS if k in mix)
    if ret is not None:
        music = music + sum(ret.values()) * (pump(RETURN_PUMP) if RETURN_PUMP else 1.0)
    if MUSIC_GLUE:
        music, _ = MS.glue(music, **MUSIC_GLUE)
    rest = sum(mix[k] for k in mix if k not in DRUM_BUS + MUSIC_BUS + ('bass',))   # the rumble
    return drums + bass_bus(mix) + rest + music_eq(music)


def send_bus(mix, parts, send):
    """What a send hears: every lane at its send level, each chop at its own (a throw) or the chops lane's."""
    j = SENDS[send]
    bus = sum(mix[k] * LANES[k][j] for k in LANES if k != 'chops' and LANES[k][j])
    for p, (_, _, own) in zip(parts, CHOPS):
        level = own[j - 2] if own and j < 4 else LANES['chops'][j]
        bus = bus + p * level
    return bus


def lane_meta(mix, char, keys=None):
    meta = {}
    for k in keys or tuple(LANES):
        meta[k] = {'source': line_name(char) if k == 'bass' else LANES[k][0], 'fader_db': round(fader_db(k, char), 2),
                   'pan': LANES[k][1], **{f'{s}_send': LANES[k][j] for s, j in SENDS.items()},
                   'lufs_before_gain': round(qa.lufs(mix[k]), 2)}
    return meta


GRID = {'bpm': K.BPM, 'steps': STEPS, 'samples': L, 'swing': K.SWING, 'kick_steps': [s for s, _ in KICK]}


def parts_meta(char):
    return {'bass': {'line': line_name(char), 'pick': LINE, 'character': 'A, clean' if char == CLEAN else 'B, warm',
                     'ride': BASS_RIDE, 'pick_about': bass.LINE_BY_NAME[LINE].about,
                     'pick_why': bass.LINE_BY_NAME[LINE].why},
            'rumble': {'steps': list(RUMBLE_AT), 'bars': 2 * len(RUMBLE_AT), 'muted_from_step': KICK_OUT},
            'snare': [list(h) for h in SNARE], 'hats': {'closed': [list(h) for h in HAT_CLOSED],
                                                        'open': [list(h) for h in HAT_OPEN], 'choke': drums.CHOKE},
            'ride': [list(h) for h in RIDE], 'anvil': [list(h) for h in ANVIL], 'lead_steps': list(LEAD_AT),
            'robot_step': ROBOT_AT, 'chops': [{'step': s, 'chop': n} for s, n, _ in CHOPS], 'fill_step': FILL_AT}


NOT_MODELLED = "the engine's voice stealing; the master (ContextLoopClub has it)"
DIP_META = {k: {'over_hz': hz, 'under': list(VOCALS), **BASS_DUCK} for k, hz in DIPS.items()}


def bus_meta(mix):
    return {'drum_bus': {'lanes': list(DRUM_BUS), 'grit': DRUM_GRIT, 'clip': 'master.soft_clip, 4x',
                         **(DRUM_CLIP or {}), 'ceiling_dbfs': round(drum_ceiling_db(mix), 4)},
            'vocal_bus': {'lanes': list(VOCALS), 'compressor': 'master.glue', **(VOX_BUS or {})},
            'music_bus': {'lanes': list(MUSIC_BUS), 'returns': 'in the wet loop',
                          'eq': [{'peaking_hz': f, 'gain_db': g, 'q': q} for f, g, q in MUSIC_EQ]},
            'pump': {**(PUMP or {}), 'keyed_to': 'the kicks', 'lanes': list(PUMPED)},
            'bass_sidechain': {**(BASS_SIDECHAIN or {}), 'keyed_to': 'the kicks', 'lanes': ['bass']},
            'return_pump': {**(RETURN_PUMP or {}), 'keyed_to': 'the kicks', 'lanes': ['space', 'echo', 'resonator']}}


# ------------------------------------------------------------------ the files
def dry(char):
    mix, _, _ = lanes(char)
    meta = {'grid': GRID, 'bass': char, 'lanes': lane_meta(mix, char), 'parts': parts_meta(char), 'dips': DIP_META,
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
    """The three returns (after their return gains), each over two passes so its tail wraps. The bass lanes send
    nothing, so they are the same under either bass."""
    mix, parts, _ = lanes(char)
    two_pass = lambda fn, x: Ph.two_pass(fn, x, SEAM_S)  # noqa: E731
    space = two_pass(lambda u: S.space(u, K.space_rng('send'), **SPACE), D.mono(send_bus(mix, parts, 'space')))
    key = sum(mix[k] for k in VOCALS)
    key = np.concatenate([key, key])
    space = two_pass(lambda u: S.duck(u, key, **S.DUCK)[0], space)
    delay = S.ECHO_STEPS * K.STEP
    echo = two_pass(lambda u: S.echo(u, K.space_rng('echo'), delay, **ECHO), D.mono(send_bus(mix, parts, 'echo')))
    res = two_pass(lambda u: S.resonator(u, resonator_freqs(), **S.RESONATOR),
                   D.mono(send_bus(mix, parts, 'resonator')))
    return {'space': space * D.undb(S.SPACE_RETURN_DB), 'echo': echo * D.undb(S.ECHO_RETURN_DB),
            'resonator': res * D.undb(S.RESONATOR_RETURN_DB)}


def wet():
    """The wet loop and its metadata: the lanes with bass B through the buses, plus the three returns."""
    char = WET_BASS
    mix, _, _ = lanes(char)
    ret = returns(char)
    delay = S.ECHO_STEPS * K.STEP
    space_meta = {**SPACE, 'return_db': S.SPACE_RETURN_DB, 'seed': [K.SPACE_SEED, 'send'], 'duck_under': 'vocals',
                  **{f'duck_{k}': v for k, v in S.DUCK.items()}}
    echo_meta = {**ECHO, 'delay_steps': S.ECHO_STEPS, 'delay_s': round(delay, 6), 'return_db': S.ECHO_RETURN_DB,
                 'seed': [K.SPACE_SEED, 'echo']}
    res_meta = {**S.RESONATOR, 'return_db': S.RESONATOR_RETURN_DB, 'tones_midi': list(RESONATOR_TONES),
                'ratios': list(K.RATIOS[:4]), 'stand_in': 'a 2-pole bank on one chord; the engine retunes per chord'}
    meta = {'target': 'the target the engine must match', 'grid': GRID, 'bass': char,
            'bass_why': "the designers' pick, AcidLine1 (the rolling hook), in B, warm: the integrator's pick of the "
                        'character (the bass designer named the line, not the character), the body round 1 missed in '
                        'the hollow reese; the A/B is the listener\'s call',
            'lanes': lane_meta(mix, char), 'parts': parts_meta(char), 'dips': DIP_META, 'buses': bus_meta(mix),
            'throws': [{'step': s, 'chop': n, 'space_send': t[0], 'echo_send': t[1]} for s, n, t in CHOPS if t],
            'space': space_meta, 'echo': echo_meta, 'resonator': res_meta,
            'return_lufs': {k: round(qa.lufs(v), 2) for k, v in ret.items()},
            'not_modelled': NOT_MODELLED + ", the resonator's retuning"}
    return mixdown(mix, ret), meta


def context_wet(take):
    y, meta = wet()
    return Out(y, meta=meta)


# the kit's overrides of the club master's settings (master.settings): the cap on peak reduction, 6 dB against the
# standard's 4 dB convention (C, L-M). At 4 dB the wet loop stops at -9.6 LUFS-I, 0.6 LU under the techno window
# (-8 +-1), at 5 dB at -8.8; the 6 dB cap takes it to -8.04 (4.5 dB of it the clipper's, 1.5 the limiter's; the design
# pass's loop reached -8.0 with 5.6: the fix pass's low end no longer stacks the rumble and the bass's sub on the kick's
# tail, so it has less sub energy for the same peaks). What the cap protects holds: the glue, the clip and the limiter
# cost the kick's jump 0.2 dB (full band) and 0.5 dB (40-120 Hz) and the clap's 0.5 dB, and the master moves the low
# mids at most 0.15 dB (125 and 250 Hz against the wet loop). Its 25 Hz low cut moves the 40-120 Hz reading 6.1 dB more,
# by delay, not level: 7.6 ms at E1 and 3.1 at 60 Hz against the measure's 30 ms window, late in which the E1 kick
# builds its 40-120 Hz (the loop 2 ms late reads 3.4 dB less). The engine's master must match the cap (S1e)
MASTER = {'max_reduction_db': 6.0}


def club_hits():
    """The hits the club master reports on (master.punch) and QA reads: every kick (all at full velocity) and the
    backbeat snares (velocity 0.8 and over), as onsets."""
    return {'kick': [G.at(s) for s, v in KICK if v == 1.0], 'clap': [G.at(s) for s, v in SNARE if v >= 0.8]}


def context_club(take):
    """The wet loop through the club master at the techno target (master.settings(K)) and the kit's cap (MASTER,
    declared on the item): the master's report goes into the metadata."""
    y, meta = wet()
    hits = club_hits()
    z, report = MS.master(y, MS.settings(K, **MASTER), hits=hits)
    return Out(z, top_hits=hits['kick'],
               meta={**meta, 'target': 'the club master the engine must match (S1e)', 'master': report})


def low(char):
    """The kick, bass and rumble lanes as the full loops mix them: the kick out of the drum bus as it is when it plays
    alone, the bass without its dip under the vocals (over 500 Hz, which the low-end rules don't read), so the low-end
    loops need no vocal models."""
    mix = {k: mixed(k, char) for k in LOW}
    meta = {'grid': GRID, 'bass': char, 'lanes': lane_meta(mix, char, LOW),
            'parts': {k: parts_meta(char)[k] for k in ('bass', 'rumble')}, 'drum_bus': 'the kick alone through it',
            'not_modelled': "the bass lane's dip under the vocals (over 500 Hz), which the full loops have; the other "
                            "drums in the drum bus's grit and clip"}
    y = drum_bus({'kick': mix['kick']}, ('kick',)) + mix['bass'] + mix['rumble']
    return Out(y, hits=kick_onsets(), top_hits=club_hits()['kick'], meta=meta)


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
R23 = ('r2', 'r3')
CLUB_QA = {'mud': 'off', 'club_bands': ('125', '250'), 'club_max_reduction_db': MASTER['max_reduction_db'],
           'club_crest_loss_db': 2.4,
           'why': FULL_MIX['why'] + '. The 125 and 250 Hz windows are exempt, every other band stays gated: those two '
                  '(conventions from a chart-record average, which the standard\'s S4 replaces with measured '
                  'references) read -1.1 and -1.6 dB re 1 kHz, 2.1 and 1.6 dB under theirs: the kick, the rumble and '
                  'bass A keep the strict mud gate, bass B the bass-body rule with its 2 dB margin, every other lane '
                  'is high-passed by rule, and the round 3 mix fills the low mids as far as those rules allow (the '
                  'hole -5.3 / -4.2 dB against the line from the sub to 500 Hz, round 2 -10.1 / -6.7). The '
                  f"master's cap is {MASTER['max_reduction_db']:g} dB, over the standard's 4 (at 4 dB the loop stops "
                  'at -9.55 LUFS-I, outside the window), for your decision by ear. It costs the kicks 2.33 dB of '
                  'crest and the snares 2.25 (the median per hit), over the 1.5 the A&R review proposed: declared at '
                  '2.4 until your ears calibrate that limit'}
ITEMS = [
    Item('Loops', 'ContextLoopClub', 'loop', context_club, loop=True, hp_hz=None, sets=('r3',), tags=('wet', 'club'),
         qa=CLUB_QA,
         about=f'{ABOUT}, wet, mastered: the wet loop through the club master at the techno target (-8 LUFS); the '
               'master\'s settings and what it did are in the metadata'),
    Item('Loops', 'ContextLoop', 'loop', context_dry, loop=True, level=LOOP_LUFS, sets=R23, tags=('dry',),
         qa=FULL_MIX, about=f'{ABOUT}, dry, bass A (clean): the kit\'s files as the sampler plays them, on the buses'),
    Item('Loops', 'ContextLoopWarm', 'loop', context_warm, loop=True, level=LOOP_LUFS, sets=R23, tags=('dry',),
         qa=FULL_MIX, about=f'{ABOUT}, dry, bass B (warm): the same loop, its bass lane at the same loudness'),
    Item('Loops', 'ContextLoopWet', 'loop', context_wet, loop=True, level=LOOP_LUFS, sets=R23, tags=('wet',),
         qa=FULL_MIX,
         about=f'{ABOUT}, wet, bass B (warm, the integrator\'s pick): Space (the kit\'s dark 5 s hall), Echo (a dub '
               'ping-pong) and the resonator on the iron ratios, a throw on the last chop; the engine\'s target'),
    Item('Loops', 'ContextLoopLow', 'loop', context_low, loop=True, sets=R23 + ('r5',), tags=('with_kick',),
         qa={'mud': 'gate', 'why': 'the kick and bass A lanes (acid ride and rumble) as mixed: gated over the loop and '
                                   'in every gap between its kicks'},
         about=f'{ABOUT}: the kick and bass A (clean) lanes alone, with the rumble, mud-gated in every gap between '
               'kicks'),
    Item('Loops', 'ContextLoopLowWarm', 'loop', context_low_warm, loop=True, sets=R23 + ('r5',),
         tags=('with_kick',),
         qa={'mud': 'bass-body', 'why': 'B, warm: the proposed bass-body rule (ADR 0011) on the kick and bass B lanes '
                                       '(acid ride and rumble) as mixed, in every gap between the kicks the loop '
                                       'holds'},
         about=f'{ABOUT}: the kick and bass B (warm) lanes alone, with the rumble, under the bass-body rule in every '
               'gap'),
]
