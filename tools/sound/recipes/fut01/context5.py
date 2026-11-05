"""FUT01's round 5 context loops (set r5): round 4's 4-bar UK garage loop (context4.py) with the text-to-speech vocals
gone, in two versions, both featuring round 4's synth accents as ear candy, and the 4x4 speed garage loop updated the
same way.

- Vox synth (GarageVox*): the vox synth riff (voxsynth.VoxSynthRiff, its file as written, twice) where round 4 had the
  vocal hooks, the call and the throw; three accents: the reverse stab swelling into bar 3's downbeat, the noise
  uplifter rising through bars 3-4 into the loop's end, and the organ hit on the and of 4 in bar 4, on the riff's
  last chop (its throw rings over the fill into bar 1).
- No vox (GarageNoVox*): the same loop instrumental, the accents carrying the top: the same three a little louder,
  and the warp stab on the and of 4 in bar 2, where the riff's first throw was.
- SpeedGarageVox: round 4's 2-bar 4x4 variant (bass B on the off-beats) with the riff once, the reverse stab into bar
  2 and the organ hit on the and of 4 in bar 2.

Every other lane plays round 4's parts on round 4's files, faders, pans, inserts and sends (the drums, the warp bass, A
or B, with its wobble and duckers, the organ stabs, the chrome pluck phrase and the glitch fill), mixed for round 5:
without the old vocals the master stopped 1.2-1.6 LU short at its cap, so the kick lane soft-clips 2 dB more, the bass
3.5 dB and the clap 1.5 dB; the stabs and the pluck duck 3 dB under every kick, the new lanes under the kicks (and the
accents under the claps); the no-vox mix lifts the pluck, drops the stabs a little and takes a bus EQ; the vox mix
plays the fill 2 dB down. The low-end loops don't change (round 4's GarageContextLow, GarageContextLowWarm and
SpeedGarageLow carry on in r5, as does round 4's SpeedGarageLoop).

Files: GarageVox / GarageNoVox (dry, bass A), GarageVoxWarm / GarageNoVoxWarm (dry, bass B), GarageVoxWet /
GarageNoVoxWet (bass B, the plate, the echo, the resonator), GarageVoxClub / GarageNoVoxClub (the wet loop through the
shared club master, master.py, at the genre's settings and the standard's 4 dB cap), SpeedGarageVox (dry, bass B).
"""
import functools

import numpy as np

from tools.sound import dsp48 as D
from tools.sound import master as MS
from tools.sound import phrase as Ph
from tools.sound import qa
from tools.sound import sends as S
from tools.sound.item import Item, Out
from tools.sound.kits import fut01 as K
from tools.sound.recipes.fut01 import context4 as C

SR = D.SR
G, G2, L = C.G, C.G2, C.L
VARIANTS = ('vox', 'novox')
# the accents: (lane, file, 16th, layer, how, gate): 'on' puts the file's start on the 16th, 'end' its end (a reverse
# stab or a riser ends on its beat; 64 is the loop's end, so it wraps into bar 1); gate: the 16ths it plays for (a
# 15 ms fade), or None for the whole file
ACCENTS = {
    'vox': (('acc_rev', 'ReverseStab', 32, 2, 'end', None), ('acc_up', 'NoiseUplifter', 64, 1, 'end', None),
            ('acc_hit', 'OrganHit', 62, 2, 'on', None)),
    'novox': (('acc_rev', 'ReverseStab', 32, 2, 'end', None), ('acc_up', 'NoiseUplifter', 64, 1, 'end', None),
              ('acc_hit', 'OrganHit', 62, 2, 'on', None), ('acc_warp', 'WarpStab', 30, 2, 'on', 1.75)),
    'speed': (('acc_rev', 'ReverseStab', 16, 2, 'end', None), ('acc_hit', 'OrganHit', 30, 2, 'on', None)),
}
STAB_DROP = {'vox': (), 'novox': (30,), 'speed': ()}   # organ stabs the accents take over (the 16ths they leave out)
GATE_FADE_S = 0.015
RIFF_AT = {'vox': (0, 32), 'novox': (), 'speed': (0,)}   # the riff's 2-bar file starts on these 16ths
# the new lanes: fader dB, pan, space, echo, resonator (as context4.LANES); the no-vox accents NOVOX_LIFT dB louder
LANES = {
    'vsynth': (-0.5, 0.0, 0.30, 0.10, 0.0),
    'acc_rev': (-8.0, 0.0, 0.10, 0.0, 0.0),
    'acc_up': (-11.0, 0.0, 0.10, 0.0, 0.0),
    'acc_hit': (-9.0, 0.0, 0.30, 0.15, 0.0),
    'acc_warp': (-9.0, 0.15, 0.20, 0.25, 0.0),
}
NOVOX_LIFT = 2.0
NOVOX_UPLIFT = 2.0
# the new lanes' inserts, as context4's: (Hz, dB, Q) peaking filters round the loop, then a soft clip this far under
# the lane's own true peak (master.soft_clip)
EQ = {'vsynth': ((600.0, -6.0, 1.0), (1000.0, -3.0, 1.2), (3800.0, 9.0, 0.8))}   # out of the 500 Hz and 1 kHz
# octaves (its fundamentals and first formants), into the presence octave
CLIP_DB = {'vsynth': 5.5, 'acc_hit': 2.0, 'acc_warp': 2.0}
# the mix bus, per variant: (Hz, dB, Q) peaking filters over the summed lanes (the dry loops) and over the wet loop
# (which the club master plays)
BUS_EQ = {'vox': ((16000.0, -1.5, 0.8),),
          'novox': ((16000.0, -2.5, 0.8), (8000.0, -1.5, 1.0), (3000.0, 3.0, 1.0), (1000.0, -1.5, 1.4),
                    (500.0, 1.0, 1.0)),
          'speed': ()}
# the new lanes ducked under every kick (as round 4's bass is), so a chop or a stab doesn't stack on the kick's peak
DUCK = {'vsynth': dict(depth_db=6.0, ramp=0.002, hold=0.060, release=0.080),
        'acc': dict(depth_db=5.0, ramp=0.002, hold=0.060, release=0.120),
        'acc_clap': dict(depth_db=4.0, ramp=0.002, hold=0.030, release=0.080)}
BASS_CLIP_DB = 3.5                  # the bass lane soft-clipped this far under its own true peak: its accents set the
                                    # mix's peaks away from the kicks (master.soft_clip)
KICK_CLIP_DB = 2.0                  # the kick lane (context4's, already clipped 1.5 dB) soft-clipped this much more
CLAP_CLIP_DB = 1.5                  # the clap lane soft-clipped this far under its true peak: after the master's
                                    # low cut the claps are the loop's highest peaks
# round 4's music lanes in this mix: ducked under every kick like the new lanes (the duck the bass has); and per
# variant, dB over round 4's faders: the no-vox top a little louder where the vox synth was, the fill under the throw
MUSIC_DUCK = dict(depth_db=3.0, ramp=0.002, hold=0.040, release=0.080)
MUSIC = ('stab', 'pluck')
OLD_LIFT = {'vox': {'fill': -2.0}, 'novox': {'pluck': 2.0, 'stab': -1.0, 'fill': 0.0}, 'speed': {}}


def fader_db(k, variant):
    """A new lane's fader in this variant: the no-vox accents NOVOX_LIFT louder, its uplifter NOVOX_UPLIFT more."""
    if variant != 'novox' or not k.startswith('acc_'):
        return LANES[k][0]
    return LANES[k][0] + NOVOX_LIFT + (NOVOX_UPLIFT if k == 'acc_up' else 0.0)


def fader(k, x, variant):
    return Ph.pan(x, LANES[k][1]) * D.undb(fader_db(k, variant))


def inserts(k, x):
    for fc, gain, q in EQ.get(k, ()):
        x = C.circ(lambda u, fc=fc, gain=gain, q=q: D.peaking(u, fc, gain, q), x)
    if k in CLIP_DB:
        x = MS.soft_clip(x, D.true_peak_db(x) - CLIP_DB[k] + 0.5)[0]
    return x


def bus(x, variant):
    for fc, gain, q in BUS_EQ[variant]:
        x = C.circ(lambda u, fc=fc, gain=gain, q=q: D.peaking(u, fc, gain, q), x)
    return x


def riff_lane(grid, starts):
    _, got, _ = Ph.kit_files(K, 'VoxSynthRiff')
    buf = np.zeros((grid.length, 2))
    for step in starts:
        Ph.place(buf, got[(None, 1, 1)].audio, grid.at(step))
    return buf


def accent(grid, name, step, layer, how, gate=None):
    """An accent's file on the grid: its start on the 16th, or its end (the beat a reverse stab or riser lands on);
    gated to `gate` 16ths."""
    _, got, _ = Ph.kit_files(K, name)
    x = got[(None, layer, 1)].audio
    if gate:
        n = int(round(gate * grid.s16))
        x = x[:n] * D.taper(n, n / SR - GATE_FADE_S, GATE_FADE_S)[:, None]
    buf = np.zeros((grid.length, 2))
    at = grid.at(step % grid.steps) if how == 'on' else (grid.at(step % grid.steps) - len(x)) % grid.length
    Ph.place(buf, x, at)
    return buf


def ducked(x, grid, kicks, duck):
    return x * D.undb(Ph.pump_db([grid.at(s) for s, _ in kicks], grid.length, **duck))[:, None]


@functools.lru_cache(maxsize=None)
def new_lanes(variant):
    """The vox synth and accent lanes of a variant ('vox', 'novox' or 'speed'): inserts, the duck under the kicks,
    the fader."""
    grid, kicks = (G2, C.KICK_4X4) if variant == 'speed' else (G, C.KICK)
    out = {}
    if RIFF_AT[variant]:
        x = inserts('vsynth', riff_lane(grid, RIFF_AT[variant]))
        out['vsynth'] = fader('vsynth', ducked(x, grid, kicks, DUCK['vsynth']), variant)
    claps = C.CLAP_4X4 if variant == 'speed' else tuple(h for h in C.CLAP if h[1] >= 0.8)
    for lane, name, step, layer, how, gate in ACCENTS[variant]:
        x = inserts(lane, accent(grid, name, step, layer, how, gate))
        x = ducked(ducked(x, grid, kicks, DUCK['acc']), grid, claps, DUCK['acc_clap'])
        out[lane] = fader(lane, x, variant)
    return out


@functools.lru_cache(maxsize=None)
def bass(char, line='garage'):
    """Round 4's bass lane (context4.bass_lane) through a soft clip BASS_CLIP_DB under its true peak."""
    x = C.bass_lane(char, line)[0]
    return MS.soft_clip(x, D.true_peak_db(x) - BASS_CLIP_DB + 0.5)[0] if BASS_CLIP_DB else x


@functools.lru_cache(maxsize=1)
def r4_lanes():
    """Round 4's lanes but the vox, exactly as context4 builds them (its parts, inserts and faders)."""
    hits = lambda k, part: Ph.hits_lane(K, C.FILES[k], part, G)
    raw = {'clap': hits('clap', C.CLAP), 'hat_c': hits('hat_c', C.HAT_C),
           'hat_o': C.choked_lane(C.FILES['hat_o'], C.HAT_O, C.HAT_C, G), 'tamb': hits('tamb', C.TAMB),
           'rim': hits('rim', C.RIM), 'perc': hits('perc', C.PERC), 'stab': C.zoned_lane(C.FILES['stab'], C.STABS, G),
           'pluck': C.pluck_lane(G), 'fill': C.fill_lane()}
    return {k: C.fader(k, C.inserts(k, x)) for k, x in raw.items()}


@functools.lru_cache(maxsize=None)
def old_lanes(variant):
    """Round 4's lanes for this mix: the organ stabs but those an accent takes over (STAB_DROP), the music lanes ducked
    under the kicks, the no-vox top lifted (OLD_LIFT)."""
    out = dict(r4_lanes())
    if STAB_DROP[variant]:
        stabs = tuple(h for h in C.STABS if h[0] not in STAB_DROP[variant])
        out['stab'] = C.fader('stab', C.inserts('stab', C.zoned_lane(C.FILES['stab'], stabs, G)))
    for k in MUSIC:
        out[k] = ducked(out[k], G, C.KICK, MUSIC_DUCK)
    for k, db in OLD_LIFT[variant].items():
        out[k] = out[k] * D.undb(db)
    if CLAP_CLIP_DB:
        out['clap'] = MS.soft_clip(out['clap'], D.true_peak_db(out['clap']) - CLAP_CLIP_DB + 0.5)[0]
    return out


@functools.lru_cache(maxsize=None)
def kick(line='garage'):
    """Round 4's kick lane (context4.kick_lane: its clip and EQ), soft-clipped KICK_CLIP_DB more."""
    x = C.kick_lane('KickGarage', line)
    return MS.soft_clip(x, D.true_peak_db(x) - KICK_CLIP_DB + 0.5)[0] if KICK_CLIP_DB else x


def lanes(variant, char):
    return {'kick': kick(), 'bass': bass(char), **old_lanes(variant), **new_lanes(variant)}


def lane_sends(k):
    return C.LANES[k] if k in C.LANES else LANES[k]


def send_bus(mix, send):
    j = C.SENDS[send]
    return sum(mix[k] * lane_sends(k)[j] for k in mix if lane_sends(k)[j])


@functools.lru_cache(maxsize=None)
def returns(variant):
    """Round 4's returns on this mix: the plate (ducked by the vox synth, as round 4's was by the vocals), the echo and
    the resonator, each round the loop."""
    mix = lanes(variant, C.WET_BASS)
    two = lambda fn, x: Ph.two_pass(fn, x, C.SEAM_S)
    space = two(lambda u: S.space(u, K.space_rng('send r4'), **C.PLATE), D.mono(send_bus(mix, 'space')))
    if 'vsynth' in mix:
        key = np.concatenate([mix['vsynth'], mix['vsynth']])
        space = two(lambda u: S.duck(u, key, **S.DUCK)[0], space)
    echo = two(lambda u: S.echo(u, K.space_rng('echo r4'), S.ECHO_STEPS * K.STEP, **C.ECHO),
               D.mono(send_bus(mix, 'echo')))
    res = two(lambda u: S.resonator(u, C.resonator_freqs(), **S.RESONATOR), D.mono(send_bus(mix, 'resonator')))
    return {'space': space * D.undb(S.SPACE_RETURN_DB), 'echo': echo * D.undb(S.ECHO_RETURN_DB),
            'resonator': res * D.undb(S.RESONATOR_RETURN_DB)}


def accent_meta(variant):
    return [{'lane': lane, 'file': name, 'step': step, 'layer': layer, 'placed': how, 'gate_16ths': gate,
             'fader_db': fader_db(lane, variant)} for lane, name, step, layer, how, gate in ACCENTS[variant]]


def mix_meta(mix, variant, char):
    lanes_ = {}
    for k, v in mix.items():
        if k in C.LANES or k in ('kick', 'bass'):
            src = C.FILES.get(k, C.WARP[char] if k == 'bass' else k)
            fdb = round(C.bass_lane(char)[1], 2) if k == 'bass' else C.LANES[k][0]
            pan = C.LANES[k][1]
        else:
            src = 'VoxSynthRiff' if k == 'vsynth' else next(a[1] for a in ACCENTS[variant] if a[0] == k)
            fdb, pan = fader_db(k, variant), LANES[k][1]
        lanes_[k] = {'source': src, 'fader_db': fdb, 'pan': pan, 'lufs': round(qa.lufs(v), 2)}
    return {'grid': C.GRID, 'variant': variant, 'lanes': lanes_, 'bass': C.line_meta(C.WARP_LINE, char),
            'kick_clip_db': C.KICK_CLIP_DB, 'riff_at': list(RIFF_AT[variant]), 'accents': accent_meta(variant),
            'bus_eq': [list(b) for b in BUS_EQ[variant]], 'bass_clip_db': BASS_CLIP_DB, 'duck': DUCK,
            'music_duck': MUSIC_DUCK, 'lift_db': dict(OLD_LIFT[variant]), 'kick_clip_more_db': KICK_CLIP_DB,
            'stabs_left_out': list(STAB_DROP[variant]), 'clap_clip_db': CLAP_CLIP_DB}


def dry(variant, char):
    mix = lanes(variant, char)
    return Out(bus(sum(mix.values()), variant), meta=mix_meta(mix, variant, char))


@functools.lru_cache(maxsize=None)
def wet_mix(variant):
    mix = lanes(variant, C.WET_BASS)
    rets = returns(variant)
    meta = {**mix_meta(mix, variant, C.WET_BASS), 'space': dict(C.PLATE), 'echo': dict(C.ECHO),
            'return_lufs': {k: round(qa.lufs(v), 2) for k, v in rets.items()},
            'target': 'the target the engine must match'}
    return bus(sum(mix.values()) + sum(rets.values()), variant), meta


def wet(variant):
    y, meta = wet_mix(variant)
    return Out(y, meta=meta)


def club(variant):
    """The wet loop as its file is (the renderer's 20 Hz high-pass, round the loop), through the club master at the
    genre's settings (master.settings(K): -9 LUFS, PLR >= 7.5 dB, at most 4 dB of peak reduction)."""
    y, meta = wet_mix(variant)
    w = C.circ(lambda u: D.dc_block(u, 20.0), y)
    hits = C.club_hits()
    z, report = MS.master(w, MS.settings(K), hits=hits)
    return Out(z, top_hits=hits['kick'], meta={**meta, 'target': 'the club master (ADR 0014)', 'master': report})


def speed(take):
    mix = {**C.speed_lanes(C.WET_BASS), 'kick': kick('speed'), 'bass': bass(C.WET_BASS, 'speed'), **new_lanes('speed')}
    mix['stab'] = ducked(mix['stab'], G2, C.KICK_4X4, MUSIC_DUCK)
    return Out(bus(sum(mix.values()), 'speed'), hits=C.kick_onsets(C.KICK_4X4, G2),
               top_hits=C.full_kicks(C.KICK_4X4, G2),
               meta={'grid': {'bpm': K.BPM, 'steps': 32, 'swing': K.SWING, 'kick_steps': [s for s, _ in C.KICK_4X4]},
                     'bass': C.line_meta(C.SPEED_LINE, C.WET_BASS), 'bass_clip_db': BASS_CLIP_DB,
                     'riff_at': list(RIFF_AT['speed']),
                     'accents': accent_meta('speed')})


def vox_dry(take):
    return dry('vox', 'clean')


def vox_dry_warm(take):
    return dry('vox', 'warm')


def vox_wet(take):
    return wet('vox')


def vox_club(take):
    return club('vox')


def novox_dry(take):
    return dry('novox', 'clean')


def novox_dry_warm(take):
    return dry('novox', 'warm')


def novox_wet(take):
    return wet('novox')


def novox_club(take):
    return club('novox')


R5 = ('r5',)
ABOUT = '4-bar UK garage context loop, 134 BPM, F# minor: the 2-step kick, the garage clap, swung hats and tambourine'
VOX = 'the vox synth riff and three synth accents'
NOVOX = 'instrumental, four synth accents on top'
FULL_MIX = {'mud': 'off', 'why': 'the full mix: the vox synth, the clap, the stabs, the accents and the pluck belong '
                                 'in 150-400 Hz; its low end is checked as GarageContextLow (the gate) and '
                                 'GarageContextLowWarm (bass-body), and every other lane passed its own low cut'}
ITEMS = [
    Item('Loops', 'GarageVox', 'loop', vox_dry, loop=True, level=C.LOOP_LUFS, sets=R5, tags=('dry',), qa=FULL_MIX,
         about=f'{ABOUT}, {VOX}, dry, warp bass A (clean): no sends, no master'),
    Item('Loops', 'GarageVoxWarm', 'loop', vox_dry_warm, loop=True, level=C.LOOP_LUFS, sets=R5, tags=('dry',),
         qa=FULL_MIX, about=f'{ABOUT}, {VOX}, dry, warp bass B (warm), its lane at the same loudness'),
    Item('Loops', 'GarageVoxWet', 'loop', vox_wet, loop=True, level=C.LOOP_LUFS, sets=R5, tags=('wet',), qa=FULL_MIX,
         about=f"{ABOUT}, {VOX}, wet, bass B: the garage plate, the echo, a little resonator; the engine's target"),
    Item('Loops', 'GarageVoxClub', 'loop', vox_club, loop=True, hp_hz=None, sets=R5, tags=('wet', 'club'),
         qa=FULL_MIX, about=f'{ABOUT}, {VOX}: GarageVoxWet through the club master, aimed at the UK garage target, '
                            f'-9 LUFS'),
    Item('Loops', 'GarageNoVox', 'loop', novox_dry, loop=True, level=C.LOOP_LUFS, sets=R5, tags=('dry',), qa=FULL_MIX,
         about=f'{ABOUT}, {NOVOX}, dry, warp bass A (clean): no sends, no master'),
    Item('Loops', 'GarageNoVoxWarm', 'loop', novox_dry_warm, loop=True, level=C.LOOP_LUFS, sets=R5, tags=('dry',),
         qa=FULL_MIX, about=f'{ABOUT}, {NOVOX}, dry, warp bass B (warm), its lane at the same loudness'),
    Item('Loops', 'GarageNoVoxWet', 'loop', novox_wet, loop=True, level=C.LOOP_LUFS, sets=R5, tags=('wet',),
         qa=FULL_MIX, about=f"{ABOUT}, {NOVOX}, wet, bass B: the garage plate, the echo, a little resonator"),
    Item('Loops', 'GarageNoVoxClub', 'loop', novox_club, loop=True, hp_hz=None, sets=R5, tags=('wet', 'club'),
         qa=FULL_MIX, about=f'{ABOUT}, {NOVOX}: GarageNoVoxWet through the club master, aimed at the UK garage target, '
                            f'-9 LUFS'),
    Item('Loops', 'SpeedGarageVox', 'loop', speed, loop=True, level=C.LOOP_LUFS, sets=R5, tags=('dry',),
         qa={'mud': 'off', 'why': 'the full mix: the vox synth, the clap, the stabs and the accents belong in 150-400 '
                                  'Hz; its low end is checked as SpeedGarageLow (the gate), and every other lane '
                                  'passed its own low cut'},
         about='2-bar speed garage variant, 134 BPM, F# minor: the kick on every beat, the warp line (B) on the '
               'off-beats between the kicks, the clap, swung hats, tambourine and organ stabs, the vox synth riff, '
               'the reverse stab and the organ hit'),
]
