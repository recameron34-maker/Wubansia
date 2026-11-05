"""FUT02's mix for the plugin: what the shared exporters need to know about this kit's round 5 context loops
(context5.py on context.py), kept here because shared code imports no kit (ADR 0008). tools/sound/mix_trace.py,
plugin_mix.py and engine_ref.py read it through kits.load('FUT02').RECIPES.

The accents get a channel each (each sound has its own sends); the bass line and the drop glide get one each (the
glide has its own sidechain); the arp's vox pocket becomes two level ducks keyed on the vox channel and bar 1's
backbeat carve is dropped (both approved: position rides have no insert).
"""
import numpy as np

from tools.sound import mix_trace as T
from tools.sound import plugin_mix as PM
from tools.sound.mix_trace import Rule, nth

VARIANTS = {'vox': 'vox synth', 'novox': 'no vox'}
R5_LOOPS = {'vox': ('ContextVoxSynthWet', 'ContextVoxSynthClub'), 'novox': ('ContextNoVoxWet', 'ContextNoVoxClub')}
ACCENTS = {'LaserChordStab': 'acc_laser', 'GlassPing': 'acc_glass', 'StarfieldSweep': 'acc_sweep',
           'ReverseChord': 'acc_revchord', 'ArpStab': 'acc_arpstab'}
ARP = (1600.0, 16000.0)             # the arp's CC74 map: context.ARP_RIDE
POCKET = {'attack': 0.010, 'release': 0.300, 'range': 24.0, 'under_top': 12.0}


def render(version):
    from tools.sound.recipes.fut02 import context5 as C5
    C5.club(version)                 # the wet loop (bass B) through the club master


class Capture(T.Capture):
    def __init__(self, kit, variant):
        super().__init__(kit, variant)
        self._accent = None
        self._pan = {}
        self._arp = self._rest = self._line = self._mute = None

    def hook(self, rec, a, out):
        f, par = rec.fn, rec.parent()
        if f == 'context.kick_raw':
            self.put('kick', out)
        elif f == 'phrase.hits_lane' and par == 'context5.base_raw' and a['name'] == 'ElectroClap':
            self.put('clap', out)
        elif f in ('context.hat_lane', 'context.zap_lane', 'context.pad_lane', 'context.fill_lane') \
                and par == 'context5.base_raw':
            self.put(f.split('.')[1][:-5], out)
        elif f == 'context.tile' and par == 'context5.arp_lane':
            self._arp = self.copy(out)
        elif f == 'context.rest_mask' and par == 'context5.arp_lane':
            self._rest = np.array(out, dtype=float)
        elif f == 'context.tile' and par == 'context.bass_lane':
            self._line = self.copy(out)
        elif f == 'context.mute_mask' and par == 'context.bass_lane':
            self._mute = np.array(out, dtype=float)
        elif f == 'phrase.place' and par == 'context.glide_lane':
            self.put('glide', a['buf'])
        elif f == 'voxsynth.sung_parts' and par == 'context5.vox_raw':
            main, throw = out
            self.put('vox', np.asarray(main) + np.asarray(throw))
            self.throws['vox'] = [(self.copy(throw), 127, 127)]
        elif f == 'context5.accent_file' and par == 'context5.accent_parts':
            self._accent = ACCENTS[a['name']]
        elif f == 'context.centred' and par == 'context5.accent_parts':
            self._pan[self._accent] = (self.copy(a['x']), float(a['p']))
        elif f == 'phrase.place' and par == 'context5.accent_parts':
            x, p = self._pan[self._accent]
            buf = np.zeros_like(np.asarray(a['buf']))
            n = len(buf)
            idx = (int(a['pos']) + np.arange(len(x))) % n       # phrase.place: the tail wraps round the loop
            np.add.at(buf, idx, x)
            self.put(self._accent, buf)
            self.value(('pan', self._accent), p)
        elif f == 'dsp48.undb' and par == 'context5.accent_parts':
            self.value(('gain', self._accent), a['d'])
        elif f == 'master.soft_clip' and par == 'context5.mixdown':
            self.value(('clip', 'drums', 0, ''), a['ceiling_db'])
        elif f == 'master.soft_clip' and par == 'context5.vox_raw':
            self.value(('clip', 'vox', 0, ''), a['ceiling_db'])
        elif f == 'context5.vox_fader_db' and par == 'context5.lanes':
            self.value(('fader', 'vox', ''), out)
        elif f == 'sends.duck':
            from tools.sound import rt_ref
            self.value(('top', 'space'), rt_ref.loudest_block_db(a['key']))
        elif f == 'context5.lanes' and par == 'context5.wet':
            self.lanes.update({k: self.copy(v) for k, v in out[0].items()})
            for part, row in out[2]:
                self.lanes[ACCENTS[row[1]]] = self.copy(part)
        elif f == 'context5.wet':
            self.wet = self.copy(out[0])
        self.master_hook(rec, a, out)

    def finish(self):
        self.put('arp', self._arp * self._rest[:, None])           # the rest is the phrase lane's (P2's fades)
        self.put('bass', self._line * self._mute[:, None])         # the line muted under the glide (P2's fades)


def rules(variant):
    vox = variant == 'vox synth'

    def pocket(c):
        return any(r[0] == 16 for r in (c.args.get('rides') or ()))

    def backbeat(c):
        return any(r[0] in (4, 12) for r in (c.args.get('rides') or ()))

    return T.COMMON + (
        Rule('context5.(wet|lanes|returns|mixdown|base_raw|arp_lane|vox_raw|accent_parts|send_bus|vox_fader_db)',
             'structure', 'the lanes, their sends, the buses and the returns, composed'),
        Rule('context5.(lane_meta|parts_meta|bus_meta)', 'structure', "the file's meta: no audio"),
        Rule(r'[a-z0-9_]+\.[A-Za-z0-9_]+', 'structure', "the file's meta: no audio", under='context5.lane_meta'),
        Rule(r'[a-z0-9_]+\.[A-Za-z0-9_]+', 'structure', "the file's meta: no audio", under='context5.parts_meta'),
        Rule(r'[a-z0-9_]+\.[A-Za-z0-9_]+', 'structure', "the file's meta: no audio", under='context5.bus_meta'),
        Rule('qa.lufs', 'structure', 'the return levels for the meta', under='context5.wet'),
        Rule('context.(club_hits|kick_onsets|pump|fader_db|line_name|glide_name|layer_of)', 'structure',
             "names, the kick onsets the ducks key on (the kick lane's notes), the pump's gain"),
        # the dry lanes
        Rule('context.(kick_raw|hat_lane|zap_lane|pad_lane|fill_lane|bass_raw|bass_lane|tile|mute_mask|rest_mask)',
             'dry', "the voices: the drums and the zaps (their per-hit pans, kit.json's pan cycle) on the grid, the pad "
                    "held, the fill, the bass line and the arp phrase tiled with the recipe's mute edges (P2's phrase "
                    "lanes fade there; the arp's rest moves ahead of its filter, a ring-out of a few ms)"),
        Rule('context.glide_lane', 'dry', 'the drop glide struck and released (its own sidechain below)'),
        Rule('(dsp48.st|dsp48.taper|phrase.place|phrase.pan)', 'dry', 'the voices, placed (their pans and edges)',
             under='context.glide_lane', when=lambda c: c.fn != 'phrase.pump_db'),
        Rule('phrase.place', 'dry', 'a voice placed on the grid, its tail wrapping'),
        Rule('phrase.pan', 'dry', "a zap's own pan (kit.json's pan cycle)", under='context.zap_lane'),
        Rule('dsp48.st', 'structure', 'mono to stereo'),
        Rule('context5.accent_file', 'dry', "an accent's file"),
        Rule('voxsynth.sung_parts', 'dry', 'the vox riff, synthesized at render time (P2: a phrase lane)'),
        Rule(r'[a-z0-9_]+\.[A-Za-z0-9_]+', 'dry', 'the vox riff, synthesized', under='voxsynth.sung_parts'),
        # the channels
        Rule('context.faded', 'live', lambda c: f'channels[{c.args.get("k")}].inserts: pan and gain (the fader)'),
        Rule('context.centred', 'live', 'a pan insert (none at the centre)', under='context.faded'),
        Rule('phrase.pan', 'live', 'a pan insert', under='context.faded'),
        Rule('dsp48.undb', 'live', 'a gain insert (the fader)', under='context.faded'),
        Rule('dsp48.undb', 'live', 'gain inserts: R5_FADERS (zap -4.5, bass +1.5) and the vox fader',
             under='context5.lanes'),
        Rule('qa.lufs', 'fixed', 'the vox fader, solved to VOX_LUFS: a fixed gain insert',
             under='context5.vox_fader_db'),
        Rule('context.clap_snap', 'causal', 'channels[clap].inserts: an onset duck lifting 3 dB (depth -3000) on the '
                                            'snare key lane (min_velocity 102), band {1000, 5000}: the LR4 band '
                                            'added (x + (g - 1) band, rt_ref.band_duck) in place of the zero-phase '
                                            'band-pass, ramp 0, hold 12 ms, release 13 ms'),
        Rule('(context.snap_gain|dsp48.undb|phrase.circular)', 'causal', "the snap's envelope and band",
             under='context.clap_snap'),
        Rule('dsp48.bfilt', 'causal', "the snap's zero-phase band-pass: an LR4 band {1000, 5000}",
             under='context.clap_snap'),
        Rule('context.filter_ride', 'live', 'channels[arp].inserts: cc_filter lowpass 1600-16000 Hz, Q 1.4 (the '
                                            'ride on CC74: plugin_mix.ride_points)'),
        Rule('(context.ride_hz|dsp48.svf_tpt)', 'live', "the cc_filter's ride and TPT SVF",
             under='context.filter_ride'),
        Rule('context.ride_db', 'causal', 'the vox pocket (16-44, -2 dB): a level duck keyed on the vox channel',
             under='context5.arp_lane', when=lambda c: c.nth == 0 and pocket(c)),
        Rule('context.ride_db', 'structure', 'no pocket without the vox', under='context5.arp_lane',
             when=lambda c: c.nth == 0),
        Rule('context.ride_db', 'dropped', "bar 1's backbeat carve (4 dB in 1.2-6.5 kHz from 100 ms before claps "
                                           "4 and 12): position automation the mix has no insert for (approved); the "
                                           "vox pocket's carve rows become a band level duck on the vox",
             under='context5.arp_lane', when=lambda c: c.nth == 1 and backbeat(c)),
        Rule('dsp48.undb', 'causal', 'the pocket as a level duck (whole band)', under='context5.arp_lane',
             when=nth(0)),
        Rule('dsp48.undb', 'causal' if vox else 'dropped',
             'the carve as a band level duck on the vox' if vox else 'the backbeat carve (dropped)',
             under='context5.arp_lane', when=nth(1)),
        Rule('dsp48.bfilt', 'causal' if vox else 'dropped',
             "the carve's zero-phase band-pass: the level duck's LR4 band {1200, 6500}" if vox
             else "the backbeat carve's band (dropped)", under='context5.arp_lane'),
        Rule('phrase.pump_db', 'live', 'channels[glide].inserts: an onset duck keyed on the kick lane (6 dB, '
                                       '2/20/120 ms; the kicks it keys sound only under the glide)',
             under='context.glide_lane'),
        Rule('dsp48.undb', 'live', "the glide's sidechain gain", under='context.glide_lane'),
        Rule('phrase.pump_db', 'live', 'onset ducks keyed on the kick lane: the pump (pad, arp, vox, the returns) and '
                                       "the kick's room (zap, fill, the accents)", under='context.pump'),
        Rule('dsp48.undb', 'live', "the ducks' gain", under='context.pump'),
        Rule('context.music_eq', 'live', 'groups.music.inserts: peaks (copied into each return, which the music bus '
                                         'carries)'),
        Rule('dsp48.peaking', 'live', 'groups.music.inserts: peak', under='context.music_eq'),
        Rule('master.soft_clip', 'live', 'groups.drums.inserts: clip at the fixed ceiling', under='context5.mixdown'),
        Rule('context.drum_ceiling_db', 'fixed', "the drum bus clip's ceiling: the kick lane's true peak, fixed"),
        Rule('mixmeter.true_peak_db', 'fixed', "the kick lane's true peak (the drum clip's ceiling)",
             under='context.drum_ceiling_db'),
        Rule('master.soft_clip', 'live', 'channels[vox].inserts: clip at the fixed ceiling (the recipe clips the notes '
                                         'and the throw apart at one ceiling: the same wherever they don\'t overlap)',
             under='context5.vox_raw'),
        Rule('mixmeter.true_peak_db', 'fixed', "the vox clip's ceiling: 6 dB under the riff's true peak, fixed",
             under='context5.vox_raw'),
        Rule('context.centred', 'live', "an accent channel's pan insert", under='context5.accent_parts'),
        Rule('phrase.pan', 'live', "an accent channel's pan insert", under='context5.accent_parts'),
        Rule('mixmeter.lufs_m', 'fixed', "each accent's gain, solved to its momentary loudness: a fixed gain insert",
             under='context5.accent_parts'),
        Rule('dsp48.undb', 'fixed', "each accent's solved gain (a gain insert on its channel)",
             under='context5.accent_parts'),
        Rule('dsp48.mono', 'structure', 'a send is mono (sends.py takes D.mono)', under='context5.returns'),
        Rule('dsp48.undb', 'live', 'returns.<return>.return_mdb', under='context5.returns'),
        Rule('context.resonator_freqs', 'live', 'returns.resonator.tones_mhz'),
        Rule('dsp48.midi_hz', 'live', 'returns.resonator.tones_mhz', under='context.resonator_freqs'),
    )


def arp_pocket(caps):
    """The vox pocket on the arp (ARP_POCKET: 2 dB down, its carve band 5 dB further, over 16ths 16-44, a ride in
    the recipe) as two level ducks keyed on the vox channel (approved): the whole band and the carve band, each full
    while the vox sounds within POCKET['under_top'] of its loudest block, out by POCKET['range'] under that, with the
    Space duck's attack and a release that holds over the riff's rests."""
    from tools.sound.recipes.fut02 import context as C, context5 as C5
    top = caps['vox'].values[('top', 'space')] - POCKET['under_top']
    (duck_rows,), (carve_rows,) = C5.ARP_POCKET[C5.VOX]
    return [PM.duck_level('vox', -duck_rows[2], POCKET['range'], top, POCKET['attack'], POCKET['release']),
            PM.duck_level('vox', -carve_rows[2], POCKET['range'], top, POCKET['attack'], POCKET['release'],
                          C.CARVE_HZ)]


def build(caps):
    from tools.sound import master as MS, sends as S
    from tools.sound.kits import fut02 as K
    from tools.sound.recipes.fut02 import context as C, context5 as C5
    v, nv = caps['vox'].values, caps['novox'].values
    L = C.LANES
    fader = lambda k: ([PM.pan(L[k][2])] if L[k][2] else []) + [PM.gain(L[k][1])]  # noqa: E731
    sends = lambda k: L[k][3:6]  # noqa: E731
    pump, room = PM.pump_duck(C.PUMP), PM.pump_duck(C.KICK_ROOM)
    bass_fader = [PM.gain(C.BASS_FADER[C.WARM], PM.when(ab='bass', side='b')),
                  PM.gain(C.BASS_FADER[C.CLEAN], PM.when(ab='bass', side='a')), PM.gain(C5.R5_FADERS['bass'])]
    snap = C.CLAP_SNAP
    ch = PM.channel
    chans = [
        ch('kick', 'kick', 'drums', fader('kick'), sends('kick')),
        ch('clap', 'snare', 'drums', fader('clap') + [
            PM.duck_onset(('snare',), -snap['db'], 0.0, snap['full_s'], snap['end_s'] - snap['full_s'],
                          PM.iround(snap['min_vel'], 127), 0.0, snap['hz'], pump=False)], sends('clap')),
        ch('hat', 'hats', 'drums', fader('hat'), sends('hat')),
        ch('zap', 'perc', 'music', fader('zap') + [PM.gain(C5.R5_FADERS['zap']), room], sends('zap')),
        ch('bass', 'bass', 'none', bass_fader, sends('bass')),
        ch('glide', 'bass', 'none', [PM.pump_duck(C.GLIDE_DUCK)] + bass_fader, sends('bass')),
        ch('pad', 'chords', 'music', fader('pad') + [pump], sends('pad')),
        ch('arp', 'lead', 'music', [PM.cc_filter('lowpass', *ARP, C.ARP_RIDE['q'], C.ARP_RIDE['q'])] + arp_pocket(caps)
           + fader('arp') + [pump], sends('arp')),
        ch('fill', 'fx', 'music', fader('fill') + [room], sends('fill')),
        ch('vox', 'vox', 'music', [PM.clip(v[('clip', 'vox', 0, '')], C5.VOX_CLIP['knee_db']),
                                   PM.gain(v[('fader', 'vox', '')]), pump], C5.VOX_SENDS),
    ]
    rows = {r[1]: r for var in (C5.VOX, C5.NOVOX) for r in C5.ACCENTS[var]}
    for name, cid in ACCENTS.items():
        r = rows[name]
        vals = v if ('gain', cid) in v else nv
        ins = ([PM.pan(vals[('pan', cid)])] if vals[('pan', cid)] else []) + [PM.gain(vals[('gain', cid)]), room]
        chans.append(ch(cid, 'accents', 'music', ins, r[6:9]))
    music_eq = PM.peaks(C.MUSIC_EQ)
    returns = {
        'space': PM.space_return(K, 'send', C.SPACE, [PM.duck_level(
            'vox', S.DUCK['depth_db'], S.DUCK['range_db'], v[('top', 'space')], S.DUCK['attack'], S.DUCK['release']),
            pump] + music_eq, C.SPACE_RETURN_DB),
        'echo': PM.echo_return(K, 'echo', C.ECHO, [pump] + music_eq, S.ECHO_STEPS, C.ECHO_RETURN_DB),
        'resonator': PM.resonator_return(C.resonator_freqs(), [pump] + music_eq, S.RESONATOR['tau'],
                                         S.RESONATOR['send_hp'], S.RESONATOR_RETURN_DB),
    }
    groups = {'drums': {'inserts': [PM.clip(v[('clip', 'drums', 0, '')], C.DRUM_KNEE_DB)]},
              'music': {'inserts': music_eq}}
    return chans, groups, returns, {'vox': [], 'novox': []}, MS.settings(K, max_reduction_db=C.MASTER_REDUCTION_DB)


def keys():
    from tools.sound.recipes.fut02 import context as C
    return tuple([('kick', C.G.at(s), PM.iround(v, 127)) for s, v in C.KICK]
                 + [('snare', C.G.at(s), PM.iround(v, 127)) for s, v in C.CLAP])


def loop_info():
    from tools.sound.recipes.fut02 import context as C
    return int(C.L), float(C.K.BPM)


def ride_points():
    from tools.sound.recipes.fut02 import context as C
    lo, hi = ARP
    return {'arp': {74: PM.curve_points(C.ride_hz(), C.L, lambda f: PM.cc_value(f, lo, hi)), 71: [(0, 0)]}}


def item_hp_hz(name):
    from tools.sound.recipes.fut02 import context5 as C5
    return next(i for i in C5.ITEMS if i.name == name).hp_hz
