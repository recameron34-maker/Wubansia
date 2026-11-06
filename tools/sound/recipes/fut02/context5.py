"""FUT02 "Orbit Relay" round 5 context loops (set r5): round 3's 4 bars at 130 BPM in D minor (context.py) with its
text-to-speech vocals gone, in two versions, each through the same buses, sends and club master as round 3:

- vox synth (ContextVoxSynth*): the vox synth riff (voxsynth.py) in bars 2-3, where round 3's robot called and the
  sung chops answered: from bar 2's downbeat, a third under the arp on its 3-3-2 accents, its throw (A4 held a beat,
  full Space and Echo sends, as round 3's last chop) falling into the fill bar. The arp keeps a light pocket under it
  (2 dB down, its 1.2-6.5 kHz band 5 dB further) instead of round 3's word pocket (4 dB, 8-10 dB).
- no vox (ContextNoVox*): the same loop instrumental: no pocket on the arp, and four accents carrying the top.

Both feature the new accents (accents.py), as a record's ear candy: each accent's loudest 400 ms in the loop reads its
own momentary loudness (-22.5 to -23.5 LUFS, ACCENTS), well under the drums where it plays, through the kick's room (the
short duck the zaps and fill take) and into the hall and the echo:
- vox synth: the laser chord stab (Dm9) on the and of 4 in bar 1, a pickup into the riff; the glass ping on the and of
  4 in bar 2, answering the riff in its rest; the starfield sweep into the loop's end, over the fill;
- no vox: the laser chord stab on the and of 4 in bar 1; the reverse chord swelling into bar 3; the arp stab on the
  and of 2 in bar 3; the starfield sweep into the loop's end.
Everything else is round 3's, as context.py plays it: the drums, the bass (the Orbit line and the drop glide, A clean
or B warm), the pad, the arp's filter ride and backbeat carve, the zaps, the fill, the faders, the drum bus clip, the
music bus EQ, the pump and the kick's room (the vox riff pumps with the pad and arp), the sends and the club master's
cap (MASTER_REDUCTION_DB, 4.25 dB), except three moves the master needs (R5_FADERS, VOX_CLIP): without round 3's two
compressed vocal lanes the loop is sparser between its kicks, and at the 4.25 dB cap the vox synth version stopped at
-10.86 LUFS-I, the instrumental at -10.15, under the genre's window (-9.5 to -7.5). The peaks were the music's, not the
kick's: the vox synth's bright 'ee' notes (its buzz peaks 0.8 dB over the kicks), and the zaps (round 3's loudest
peak too). So the vox lane goes through a soft clip 6 dB under its own peak (3 dB knee, 4x oversampled: a vocal synth
driven, its level kept), the zaps sit 4.5 dB under round 3's fader (their sends unchanged, so their echoes still
bounce) and the bass 1.5 dB over it: the one lane that adds loudness between the kicks without adding peaks (it rests
on every kick). Raising the cap instead would have needed about 5.1 dB for the instrumental, near the 5.6 dB the A&R
review found flattened the kicks.

Files: <Version>Club (the wet loop through the club master), <Version>Wet (bass B with the three returns),
<Version> (dry, bass A) and <Version>Warm (dry, bass B); ContextLow5 and ContextLow5Warm, the kick and the bass (A or
B) as the round 5 loops play them (the bass 1.5 dB up on round 3's ContextLoopLow), gated as round 3's. Nothing here
needs the vocal models.
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
from tools.sound.recipes.fut02 import context as C, voxsynth as Vs

SR = D.SR
L, G, STEPS = C.L, C.G, C.STEPS
CLEAN, WARM = C.CLEAN, C.WARM
VOX, NOVOX = 'vox synth', 'no vox'
KEEP = ('kick', 'bass', 'clap', 'hat', 'zap', 'pad', 'arp', 'fill')   # round 3's lanes, as round 3 mixes them

# ------------------------------------------------------------------ the vox riff
RIFF_AT = 16                              # from bar 2's downbeat, where round 3's robot called
VOX_LUFS = -19.5                          # the vox lane over the loop, after its fader (round 3: chops -18.5, robot
                                          # -19.9): up front with the arp, under the kick lane's -17
VOX_SENDS = (0.40, 0.30, 0.15)            # Space, Echo, resonator: the riff's notes
THROW = (1.0, 1.0)                        # the throw's Space and Echo sends (CC91-style, round 3's)
ARP_POCKET = {VOX: (((16, 44, -2.0),), ((16, 44, -5.0),)), NOVOX: ((), ())}   # the arp's (duck, carve) rides

# ------------------------------------------------------------------ the accents
# (16th, item, zone or None, layer, momentary LUFS, pan, Space, Echo, resonator, ends there): a one-shot starts on
# its 16th; a reverse or a sweep (cut_end) ends on it (64: the loop's end, its downbeat)
ACCENTS = {
    VOX: (
        (14, 'LaserChordStab', 62, 2, -22.5, 0.0, 0.35, 0.25, 0.10, False),
        (30, 'GlassPing', None, 2, -23.5, 0.40, 0.40, 0.35, 0.10, False),
        (64, 'StarfieldSweep', None, 1, -22.5, 0.0, 0.30, 0.0, 0.0, True),
    ),
    NOVOX: (
        (14, 'LaserChordStab', 62, 2, -22.5, 0.0, 0.35, 0.25, 0.10, False),
        (32, 'ReverseChord', None, 2, -22.5, 0.0, 0.25, 0.0, 0.0, True),
        (38, 'ArpStab', None, 2, -23.0, -0.35, 0.30, 0.40, 0.10, False),
        (64, 'StarfieldSweep', None, 1, -22.5, 0.0, 0.30, 0.0, 0.0, True),
    ),
}
PUMPED = ('pad', 'arp', 'vox')            # and the returns
R5_FADERS = {'zap': -4.5, 'bass': 1.5}    # dB on round 3's faders (see the docstring)
VOX_CLIP = dict(under_db=6.0, knee_db=3.0, os=4)   # the vox lane's soft clip: its ceiling under the lane's true peak
ROOMED = ('zap', 'fill', 'accents')


def arp_lane(version):
    """Round 3's arp lane (context.arp_lane) with this version's pocket: its filter ride, the duck and carve rides,
    bar 1's backbeat carve, its rest in the fill bar."""
    duck, carve = ARP_POCKET[version]
    _, got, _ = Ph.kit_files(K, 'ChromeArpPhrase')
    x = C.filter_ride(C.tile(got[(None, 1, 1)].audio)) * D.undb(C.ride_db(duck))[:, None]
    top = Ph.circular(lambda u: D.bfilt(u, C.CARVE_HZ, 'bandpass', order=2, zero_phase=True), x)
    x = x - (1 - D.undb(C.ride_db(carve + C.ARP_BACKBEAT)))[:, None] * top
    return x * C.rest_mask()[:, None]


@functools.lru_cache(maxsize=None)
def base_raw():
    """Round 3's lanes on the kit's files, before their faders (as context.raw_lanes plays them), but the arp."""
    return {'kick': C.kick_raw(), 'clap': Ph.hits_lane(K, 'ElectroClap', C.CLAP, G), 'hat': C.hat_lane(),
            'zap': C.zap_lane(), 'pad': C.pad_lane(), 'fill': C.fill_lane()}


@functools.lru_cache(maxsize=None)
def vox_raw():
    """The riff's notes and its throw, each a loop, through the vox lane's soft clip (VOX_CLIP, one ceiling for both),
    before the fader."""
    parts = Vs.sung_parts(G, RIFF_AT, L, lambda *k: D.rng(K.KIT, 'ContextVoxRiff', *k))
    c = VOX_CLIP
    ceiling = MM.true_peak_db(sum(parts)) - c['under_db']
    return tuple(MS.soft_clip(p, ceiling, c['knee_db'], c['os'])[0] for p in parts)


def vox_fader_db():
    main, throw = vox_raw()
    return VOX_LUFS - qa.lufs(main + throw)


def accent_file(name, zone, layer):
    _, got, _ = Ph.kit_files(K, name)
    return got[(zone, layer, 1)].audio


@functools.lru_cache(maxsize=None)
def accent_parts(version):
    """Each accent as its own part of the accents lane (its own sends), placed and levelled: (part, its row)."""
    out = []
    for row in ACCENTS[version]:
        step, name, zone, layer, lufs_m, pan, _, _, _, ends = row
        x = accent_file(name, zone, layer)
        at = G.at(step % STEPS) - (len(x) if ends else 0)
        buf = np.zeros((L, 2))
        Ph.place(buf, C.centred(x, pan), at)
        out.append((buf * D.undb(lufs_m - float(np.max(MM.lufs_m(buf)))), row))
    return tuple(out)


@functools.lru_cache(maxsize=None)
def lanes(version, char):
    """Each lane after its fader, pan and inserts (round 3's, the bass in character `char`), the vox lane (its parts:
    the notes and the throw) and the accents lane (its parts)."""
    raw = base_raw()
    mix = {k: C.faded(k, C.bass_raw(char) if k == 'bass' else (arp_lane(version) if k == 'arp' else raw[k]), char)
           * D.undb(R5_FADERS.get(k, 0.0)) for k in KEEP}
    mix['clap'] = C.clap_snap(mix['clap'])
    vparts = ()
    if version == VOX:
        g = D.undb(vox_fader_db())
        vparts = tuple(p * g for p in vox_raw())
        mix['vox'] = sum(vparts)
    aparts = accent_parts(version)
    mix['accents'] = sum(p for p, _ in aparts)
    return mix, vparts, aparts


def mixdown(mix, ret=None):
    """The lanes through round 3's buses (context.mixdown), the vox riff pumped with the pad and the arp, the accents
    in the kick's room; plus the returns, pumped, when given."""
    drums, _ = MS.soft_clip(sum(mix[k] for k in C.DRUM_BUS), C.drum_ceiling_db(mix), C.DRUM_KNEE_DB, 4)
    p, room = C.pump(), C.pump(C.KICK_ROOM)
    duck = lambda k: p if k in PUMPED else (room if k in ROOMED else 1.0)
    music = sum(mix[k] * duck(k) for k in mix if k not in C.DRUM_BUS + ('bass',))
    if ret is not None:
        music = music + sum(ret.values()) * p
    return drums + mix['bass'] + C.music_eq(music)


def send_bus(mix, vparts, aparts, send):
    """What a send hears: round 3's lanes at their send levels, the riff's notes at VOX_SENDS and its throw at THROW,
    each accent at its own."""
    j = C.SENDS[send]
    bus = sum(mix[k] * C.LANES[k][j] for k in KEEP if C.LANES[k][j])
    if vparts:
        main, throw = vparts
        bus = bus + main * VOX_SENDS[j - 3] + throw * (THROW[j - 3] if j < 5 else VOX_SENDS[2])
    for part, row in aparts:
        if row[3 + j]:
            bus = bus + part * row[3 + j]
    return bus


@functools.lru_cache(maxsize=None)
def returns(version, char):
    """The three returns, over two passes as round 3's; the Space return ducks under the vox riff."""
    mix, vparts, aparts = lanes(version, char)
    two_pass = lambda fn, x: Ph.two_pass(fn, x, C.SEAM_S)
    bus = lambda s: D.mono(send_bus(mix, vparts, aparts, s))
    space = two_pass(lambda u: S.space(u, K.space_rng('send'), **C.SPACE), bus('space'))
    if 'vox' in mix:
        key = np.concatenate([mix['vox'], mix['vox']])
        space = two_pass(lambda u: S.duck(u, key, **S.DUCK)[0], space)
    delay = S.ECHO_STEPS * K.STEP
    echo = two_pass(lambda u: S.echo(u, K.space_rng('echo'), delay, **C.ECHO), bus('echo'))
    res = two_pass(lambda u: S.resonator(u, C.resonator_freqs(), **S.RESONATOR), bus('resonator'))
    return {'space': space * D.undb(C.SPACE_RETURN_DB), 'echo': echo * D.undb(C.ECHO_RETURN_DB),
            'resonator': res * D.undb(S.RESONATOR_RETURN_DB)}


def lane_meta(mix, char):
    out = {}
    for k in mix:
        if k in C.LANES:
            src, fader, pan = C.LANES[k][0], C.fader_db(k, char) + R5_FADERS.get(k, 0.0), C.LANES[k][2]
            sends = {f'{s}_send': C.LANES[k][j] for s, j in C.SENDS.items()}
        elif k == 'vox':
            src, fader, pan = 'the vox synth riff (voxsynth.sung_parts)', round(vox_fader_db(), 2), 0.0
            sends = {f'{s}_send': VOX_SENDS[j - 3] for s, j in C.SENDS.items()}
        else:
            src, fader, pan, sends = 'the accents (accents.py), each its own part', None, None, {}
        out[k] = {'source': src, 'fader_db': fader, 'pan': pan, **sends, 'lufs_before_gain': round(qa.lufs(mix[k]), 2)}
    return out


def parts_meta(version, char, mix):
    m = {'bass': C.parts_meta(char)['bass'], 'version': version, 'fill_step': C.FILL_AT, 'zap': list(C.ZAP),
         'clap': list(C.CLAP), 'hats': C.parts_meta(char)['hats'],
         'arp': {'duck': [list(r) for r in ARP_POCKET[version][0]], 'carve': [list(r) for r in ARP_POCKET[version][1]],
                 'backbeat_carve': [list(r) for r in C.ARP_BACKBEAT], 'carve_hz': list(C.CARVE_HZ),
                 'rest_from_step': C.ARP_REST[0]},
         'accents': [{'step': r[0], 'item': r[1], 'zone': r[2], 'layer': r[3], 'lufs_m': r[4], 'pan': r[5],
                      'space_send': r[6], 'echo_send': r[7], 'resonator_send': r[8], 'ends_on_step': r[9],
                      'gain_db': round(float(D.db(np.max(np.abs(p)) / (np.max(np.abs(accent_file(r[1], r[2], r[3])))
                                                                       + 1e-12))), 2)}
                     for p, r in accent_parts(version)]}
    if version == VOX:
        m['vox'] = {'riff': 'VoxSynthRiff', 'step': RIFF_AT, 'throw_step': RIFF_AT + Vs.THROW_AT,
                    'throw_sends': {'space': THROW[0], 'echo': THROW[1]}, 'notes': Vs.riff_midi()}
    return m


GRID = C.GRID
INSERT = {k: v for k, v in C.INSERT.items() if k not in ('vocal_compressor', 'vocoder_chorus', 'arp_rides')}
NOT_MODELLED = C.NOT_MODELLED


def bus_meta(mix):
    b = C.bus_meta(mix)
    b.pop('vocal_bus')
    b['music_bus']['lanes'] = [k for k in mix if k not in C.DRUM_BUS + ('bass',)]
    b['pump']['lanes'] = list(PUMPED) + ['the returns']
    b['kick_room']['lanes'] = list(ROOMED)
    return b


def dry(version, char):
    mix, _, _ = lanes(version, char)
    meta = {'grid': GRID, 'version': version, 'bass': char, 'lanes': lane_meta(mix, char),
            'parts': parts_meta(version, char, mix), 'insert': INSERT, 'buses': bus_meta(mix),
            'not_modelled': NOT_MODELLED}
    return Out(mixdown(mix), meta=meta)


@functools.lru_cache(maxsize=None)
def wet(version):
    char = C.WET_BASS
    mix, _, _ = lanes(version, char)
    ret = returns(version, char)
    meta = {'target': 'the target the engine must match', 'grid': GRID, 'version': version, 'bass': char,
            'lanes': lane_meta(mix, char), 'parts': parts_meta(version, char, mix), 'insert': INSERT,
            'buses': bus_meta(mix), 'space': {**C.SPACE, 'return_db': C.SPACE_RETURN_DB,
                                               'duck_under': 'the vox riff' if version == VOX else None},
            'echo': {**C.ECHO, 'delay_steps': S.ECHO_STEPS, 'return_db': C.ECHO_RETURN_DB},
            'resonator': {**S.RESONATOR, 'return_db': S.RESONATOR_RETURN_DB, 'tones_midi': list(C.RESONATOR_TONES)},
            'return_lufs': {k: round(qa.lufs(v), 2) for k, v in ret.items()},
            'not_modelled': NOT_MODELLED + ", the resonator's retuning"}
    return mixdown(mix, ret), meta


def low(char):
    """The kick and bass lanes as the round 5 loops play them (the bass R5_FADERS['bass'] up on round 3's)."""
    mix = {k: C.faded(k, C.kick_raw() if k == 'kick' else C.bass_raw(char), char) * D.undb(R5_FADERS.get(k, 0.0))
           for k in C.LOW}
    meta = {'grid': GRID, 'bass': char, 'lanes': {k: {'fader_db': C.fader_db(k, char) + R5_FADERS.get(k, 0.0),
                                                      'lufs_before_gain': round(qa.lufs(mix[k]), 2)} for k in C.LOW},
            'parts': {'bass': C.parts_meta(char)['bass']}, 'insert': {'glide_sidechain': C.INSERT['glide_sidechain']}}
    return Out(sum(mix[k] for k in C.LOW), hits=C.kick_onsets(), top_hits=C.club_hits()['kick'], meta=meta)


def low_clean(take):
    return low(CLEAN)


def low_warm(take):
    return low(WARM)


def club(version):
    y, meta = wet(version)
    hits = C.club_hits()
    z, report = MS.master(y, MS.settings(K, max_reduction_db=C.MASTER_REDUCTION_DB), hits=hits)
    return Out(z, top_hits=hits['kick'],
               meta={**meta, 'master': report, 'target': 'the target the engine must match, through its master'})


def vox_club(take):
    return club(VOX)


def novox_club(take):
    return club(NOVOX)


def vox_wet(take):
    y, meta = wet(VOX)
    return Out(y, meta=meta)


def novox_wet(take):
    y, meta = wet(NOVOX)
    return Out(y, meta=meta)


def vox_dry(take):
    return dry(VOX, CLEAN)


def vox_dry_warm(take):
    return dry(VOX, WARM)


def novox_dry(take):
    return dry(NOVOX, CLEAN)


def novox_dry_warm(take):
    return dry(NOVOX, WARM)


ABOUT = '4-bar context loop, 130 BPM, D minor'
FULL_MIX = {'mud': 'off', 'why': 'the full mix: clap, pad and vox synth belong in 150-400 Hz; its low end is gated as '
                                 'ContextLow5 (A) and ContextLow5Warm (B), and every other lane passed its own low cut'}
CLUB_QA = {'mud': 'off', 'club_max_reduction_db': C.MASTER_REDUCTION_DB, 'club_crest_loss_db': 1.6,
           'why': 'the full mix, mastered: clap, pad and vox synth belong in 150-400 Hz; its low end is gated as '
                  'ContextLow5 and ContextLow5Warm. The master keeps round 3\'s cap, '
                  f'{C.MASTER_REDUCTION_DB:g} dB over the standard\'s 4, and its declared 1.6 dB of crest loss, for '
                  'your decision by ear'}
ITEMS = [
    Item('Loops', 'ContextVoxSynthClub', 'loop', vox_club, loop=True, hp_hz=None, sets=('r5',), tags=('wet', 'club'),
         qa=CLUB_QA, about=f'{ABOUT}, vox synth: the wet loop with the vox synth riff through the club master'),
    Item('Loops', 'ContextNoVoxClub', 'loop', novox_club, loop=True, hp_hz=None, sets=('r5',), tags=('wet', 'club'),
         qa=CLUB_QA, about=f'{ABOUT}, no vox: the instrumental wet loop, accents on top, through the club master'),
    Item('Loops', 'ContextVoxSynthWet', 'loop', vox_wet, loop=True, sets=('r5',), tags=('wet',), qa=FULL_MIX,
         about=f'{ABOUT}, vox synth, wet, bass B: the hall, the echo and the resonator, a throw on the riff'),
    Item('Loops', 'ContextNoVoxWet', 'loop', novox_wet, loop=True, sets=('r5',), tags=('wet',), qa=FULL_MIX,
         about=f'{ABOUT}, no vox, wet, bass B: the hall, the echo and the resonator'),
    Item('Loops', 'ContextVoxSynth', 'loop', vox_dry, loop=True, sets=('r5',), tags=('dry',), qa=FULL_MIX,
         about=f'{ABOUT}, vox synth, dry, bass A (clean)'),
    Item('Loops', 'ContextVoxSynthWarm', 'loop', vox_dry_warm, loop=True, sets=('r5',), tags=('dry',), qa=FULL_MIX,
         about=f'{ABOUT}, vox synth, dry, bass B (warm)'),
    Item('Loops', 'ContextNoVox', 'loop', novox_dry, loop=True, sets=('r5',), tags=('dry',), qa=FULL_MIX,
         about=f'{ABOUT}, no vox, dry, bass A (clean)'),
    Item('Loops', 'ContextNoVoxWarm', 'loop', novox_dry_warm, loop=True, sets=('r5',), tags=('dry',), qa=FULL_MIX,
         about=f'{ABOUT}, no vox, dry, bass B (warm)'),
    Item('Loops', 'ContextLow5', 'loop', low_clean, loop=True, sets=('r5',), tags=('with_kick',),
         qa={'mud': 'gate', 'why': 'the kick and bass A lanes as the round 5 loops mix them: gated in every gap'},
         about=f'{ABOUT}: the kick and bass A (clean) lanes of the round 5 loops, mud-gated in every gap between kicks'),
    Item('Loops', 'ContextLow5Warm', 'loop', low_warm, loop=True, sets=('r5',), tags=('with_kick',),
         qa={'mud': 'bass-body', 'why': f'{C.bass.WHY_WARM}: the kick and bass B lanes of the round 5 loops, in every gap'},
         about=f'{ABOUT}: the kick and bass B (warm) lanes of the round 5 loops, under the bass-body rule in every gap'),
]
