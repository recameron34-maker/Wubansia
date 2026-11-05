"""FUT04's mix for the plugin: what the shared exporters need to know about this kit's round 5 context loops
(context5.py on context.py), kept here because shared code imports no kit (ADR 0008). tools/sound/mix_trace.py,
plugin_mix.py and engine_ref.py read it through kits.load('FUT04').RECIPES.

The loops play the acid stem (bass_stem, its ride baked in); the multisample (bass) has the same strip after a
cc_filter for the ride on CC74/CC71 (ACID). The vox keys the bass's and the fill's top bands (level ducks) and the
Space return; its throws and the no-vox accents' throws take the full Space and Echo sends.
"""
import numpy as np

from tools.sound import mix_trace as T
from tools.sound import plugin_mix as PM
from tools.sound.mix_trace import Rule

VARIANTS = {'vox': 'vox', 'novox': 'novox'}
R5_LOOPS = {'vox': ('ContextLoopWetVox', 'ContextLoopClubVox'),
            'novox': ('ContextLoopWetNoVox', 'ContextLoopClubNoVox')}
CHANNEL = {'bass': 'bass_stem'}     # the loops play the acid stem (its ride baked in), not the multisample
ACID = (5400.0, 14040.0)            # REST_HZ 900 x 2^2.6 (the attack's widest), x the ride's 2.6
ACID_Q = (0.707, 2.5)               # the ride's resonance 0.9-0.96 adds about 8 dB to the ladder's peak


def render(mode):
    from tools.sound.recipes.fut04 import context as C, context5 as C5
    C5.club(mode)                    # the wet loop (bass B) through the club master
    C.fader_db('bass', C.CLEAN)      # bass A's fader, solved to LANE_LUFS on its stem


class Capture(T.Capture):
    def hook(self, rec, a, out):
        f, par = rec.fn, rec.parent()
        if f == 'context.centred' and par == 'context.raw_lane':
            k = rec.under('context.raw_lane')['k']
            ch = CHANNEL.get(k, k)
            char = rec.under('context.raw_lane')['char']
            side = {'warm': 'b', 'clean': 'a'}[char] if k == 'bass' else None
            self.put(ch, a['x'], side, loops=k != 'bass' or char == 'warm')
        elif f == 'context.fader_db':
            k = a['k']
            self.value(('fader', CHANNEL.get(k, k), {'warm': 'b', 'clean': 'a'}[a['char']] if k == 'bass' else ''),
                       out)
        elif f == 'voxsynth.riff_parts' and par == 'context5.vox_lane':
            dry, thrown = out
            self.put('vox', np.asarray(dry) + np.asarray(thrown))
            self.throws['vox'] = [(self.copy(thrown), 127, 127)]
        elif f == 'dsp48.undb' and par == 'context5.vox_lane':
            self.value(('fader', 'vox', ''), a['d'])
        elif f == 'context5.accent_lane':
            lane, thrown = out
            self.put('accent', lane)
            if np.any(thrown):
                self.throws['accent'] = [(self.copy(thrown), 127, 127)]
        elif f == 'mixmeter.true_peak_db' and par == 'context.drum_bus':
            self.value('grit_ref_db', out)
        elif f == 'master.soft_clip' and par == 'context.drum_bus':
            self.value(('clip', 'drums', 0, ''), a['ceiling_db'])
        elif f == 'sends.duck':
            from tools.sound import rt_ref
            self.value(('top', 'dips' if par == 'context.dipped' else 'space'), rt_ref.loudest_block_db(a['key']))
        elif f == 'context5.lanes' and par == 'context5.wet':
            self.lanes.update({CHANNEL.get(k, k): self.copy(v) for k, v in out[0].items()})
        elif f == 'context5.wet':
            self.wet = self.copy(out[0])
        self.master_hook(rec, a, out)


def rules(mode):
    return T.COMMON + (
        Rule('context5.(wet|lanes|returns|mixdown|send_bus|lane_keys|vox_lane|accent_lane)', 'structure',
             'the lanes, their sends (the thrown parts at the throw), the buses and the returns, composed'),
        Rule('context5.meta', 'structure', "the file's meta: no audio"),
        Rule(r'[a-z0-9_]+\.[A-Za-z0-9_]+', 'structure', "the file's meta: no audio", under='context5.meta'),
        Rule('qa.lufs', 'structure', 'the return levels for the meta', under='context5.wet'),
        Rule('context.(club_hits|kick_onsets|pump|line_name|layer_of|bass_bus|raw_lane|drum_bus)', 'structure',
             "the lane chains and buses (their DSP below; the bass bus has no clip), the kick onsets the ducks key on"),
        # the dry lanes
        Rule('context.centred', 'live', 'a pan insert (none at the centre)', under='context.raw_lane',
             when=lambda c: c.parent() == 'context.raw_lane'),
        Rule('(phrase.pan|dsp48.st)', 'live', 'the pan insert', under='context.centred'),
        Rule(r'[a-z0-9_]+\.[A-Za-z0-9_]+', 'dry', "the voices: drums, hats (choked), the anvil, the rumble (muted from "
                                                  "16th 60), the drone held, the blip phrase and the acid stem tiled, "
                                                  "the fill", under='context.raw_lane'),
        Rule('voxsynth.riff_parts', 'dry', "the vox riff on VoxSynth's files (the multisample), its throws apart"),
        Rule(r'[a-z0-9_]+\.[A-Za-z0-9_]+', 'dry', 'the vox riff, played', under='voxsynth.riff_parts'),
        Rule('(context.kit_file|phrase.place)', 'dry', 'the accents placed', under='context5.accent_lane'),
        Rule('(qa.lufs|dsp48.undb)', 'fixed', "each accent's gain, solved to ACCENT_LUFS: in kit.json gain_mdb (P2), "
                                              "so the dry lane (and a throw) carries it", under='context5.accent_lane'),
        # the channels
        Rule('context.fader_db', 'fixed', 'a fader solved to LANE_LUFS: a fixed gain insert (per A/B side on the '
                                          'bass)'),
        Rule('qa.lufs', 'fixed', 'the fader solve', under='context.fader_db'),
        Rule('context.mixed', 'live', lambda c: f'channels[{CHANNEL.get(c.args.get("k"), c.args.get("k"))}]'
                                                f'.inserts: the fader (a gain insert)'),
        Rule('dsp48.undb', 'live', 'the fader: a gain insert', when=lambda c: c.parent() == 'context.mixed'),
        Rule('(qa.lufs|dsp48.undb)', 'fixed', 'the vox fader, solved to VOX_LUFS: a fixed gain insert',
             under='context5.vox_lane'),
        Rule('context.sidechained', 'causal', 'channels[bass, bass_stem].inserts: an onset duck keyed on the kick '
                                              'lane (8 dB, 2/250/80 ms) on the band {0, 120}: the LR4 band added '
                                              '(x + (g - 1) band, rt_ref.band_duck) in place of the zero-phase '
                                              'low-pass'),
        Rule('(phrase.circular|dsp48.bfilt)', 'causal', "the sidechain's band: LR4 {0, 120}",
             under='context.sidechained'),
        Rule('phrase.pump_db', 'live', 'onset ducks keyed on the kick lane: the bass sidechain, the drone pump (3 dB) '
                                       "and the returns' pump (5 dB)"),
        Rule('dsp48.undb', 'live', "the ducks' gain", under='context.pump'),
        Rule('context.dipped', 'causal', 'channels[bass, bass_stem, fill].inserts: a level duck keyed on the vox '
                                         'channel (9 dB, range 30, 10/150 ms) on the band {hz, 0} added as an LR4 '
                                         'band in place of the zero-phase high-pass, its top fixed, its gain 96 '
                                         'samples late'),
        Rule('(sends.duck|dsp48.mono|dsp48.undb|phrase.circular|dsp48.bfilt)', 'causal', 'the dip, causal',
             under='context.dipped'),
        Rule('context.grit', 'live', 'groups.drums.inserts: parallel at -8 dB: highpass 250 Hz order 4, peak 3 kHz '
                                     '+3 dB, gain -ref, drive 2.5 at 4x (output +ref), peak 600 Hz -3 dB, lowpass '
                                     '9 kHz order 2'),
        Rule('(phrase.circular|dsp48.bfilt|dsp48.peaking|dsp48.undb|dsp48.oversampled)', 'live', "the grit's chain",
             under='context.grit'),
        Rule('dsp48.tanh_adaa', 'live', "the grit's drive (lane_drive)", under='context.grit'),
        Rule('mixmeter.true_peak_db', 'fixed', "the kick lane's true peak: the grit's reference and the drum clip's "
                                               "ceiling, fixed", under='context.drum_bus'),
        Rule('master.soft_clip', 'live', 'groups.drums.inserts: clip at the fixed ceiling', under='context.drum_bus'),
        Rule('context.drum_ceiling_db', 'fixed', "the drum bus clip's ceiling: the kick lane's true peak, fixed"),
        Rule('mixmeter.true_peak_db', 'fixed', "the kick lane's true peak", under='context.drum_ceiling_db'),
        Rule('context.music_eq', 'live', 'groups.music.inserts: peaks (copied into each return, which the music bus '
                                         'carries)'),
        Rule('(phrase.circular|dsp48.peaking)', 'live', 'groups.music.inserts: peak', under='context.music_eq'),
        Rule('dsp48.mono', 'structure', 'a send is mono (sends.py takes D.mono)', under='context5.returns'),
        Rule('dsp48.undb', 'live', 'returns.<return>.return_mdb', under='context5.returns'),
        Rule('context.resonator_freqs', 'live', 'returns.resonator.tones_mhz'),
        Rule('dsp48.midi_hz', 'live', 'returns.resonator.tones_mhz', under='context.resonator_freqs'),
    )


def build(caps):
    from tools.sound import master as MS, sends as S
    from tools.sound.kits import fut04 as K
    from tools.sound.recipes.fut04 import context as C, context5 as C5
    v = caps['vox'].values
    L5 = C5.LANES5
    sends = lambda k: L5[k][2:5]  # noqa: E731
    fpan = lambda k: [PM.pan(L5[k][1])] if L5[k][1] else []  # noqa: E731
    fader = lambda c: [PM.gain(v[('fader', c, '')])]  # noqa: E731
    sc = C.BASS_SIDECHAIN
    sidechain = PM.duck_onset(('kick',), sc['depth_db'], sc['ramp'], sc['hold'], sc['release'],
                              bnd=(0, sc['split_hz']))
    bd = C.BASS_DUCK
    dip = lambda hz: PM.duck_level('vox', bd['depth_db'], bd['range_db'], v[('top', 'dips')], bd['attack'],  # noqa
                                   bd['release'], (hz, 0))
    bass_tail = [PM.gain(v[('fader', 'bass_stem', 'b')], PM.when(ab='bass', side='b')),
                 PM.gain(v[('fader', 'bass_stem', 'a')], PM.when(ab='bass', side='a')), sidechain,
                 dip(C.DIPS['bass'])]
    ch = PM.channel
    chans = [
        ch('kick', 'kick', 'drums', fpan('kick') + fader('kick'), sends('kick')),
        ch('rumble', 'kick', 'none', fpan('rumble') + fader('rumble'), sends('rumble')),
        ch('bass', 'bass', 'none', [PM.cc_filter('lowpass', *ACID, *ACID_Q)] + bass_tail, sends('bass')),
        ch('bass_stem', 'bass', 'none', bass_tail, sends('bass')),
        ch('snare', 'snare', 'drums', fpan('snare') + fader('snare'), sends('snare')),
        ch('hat', 'hats', 'drums', fpan('hat') + fader('hat'), sends('hat')),
        ch('ride', 'hats', 'drums', fpan('ride') + fader('ride'), sends('ride')),
        ch('perc', 'perc', 'drums', fpan('perc') + fader('perc'), sends('perc')),
        ch('fill', 'fx', 'drums', fpan('fill') + fader('fill') + [dip(C.DIPS['fill'])], sends('fill')),
        ch('pad', 'chords', 'music', fpan('pad') + fader('pad') + [PM.pump_duck(C.PUMP)], sends('pad')),
        ch('lead', 'lead', 'music', fpan('lead') + fader('lead'), sends('lead')),
        ch('vox', 'vox', 'music', [PM.gain(v[('fader', 'vox', '')])], sends('vox')),
        ch('accent', 'accents', 'music', [], sends('accent')),
    ]
    g = C.DRUM_GRIT
    ref = v['grit_ref_db']
    grit = PM.parallel(g['level_db'], [PM.highpass(g['hp_hz'], 4), PM.peak(*g['pre']), PM.gain(-ref),
                                       PM.drive(g['drive'], 0.0, ref), PM.peak(*g['mid']), PM.lowpass(g['lp_hz'], 2)])
    rp = PM.pump_duck(C.RETURN_PUMP)
    music_eq = PM.peaks(C.MUSIC_EQ)
    returns = {
        'space': PM.space_return(K, 'send', C.SPACE, [PM.duck_level(
            'vox', S.DUCK['depth_db'], S.DUCK['range_db'], v[('top', 'space')], S.DUCK['attack'], S.DUCK['release']),
            rp] + music_eq, S.SPACE_RETURN_DB),
        'echo': PM.echo_return(K, 'echo', C.ECHO, [rp] + music_eq, S.ECHO_STEPS, S.ECHO_RETURN_DB),
        'resonator': PM.resonator_return(C.resonator_freqs(), [rp] + music_eq, S.RESONATOR['tau'],
                                         S.RESONATOR['send_hp'], S.RESONATOR_RETURN_DB),
    }
    groups = {'drums': {'inserts': [grit, PM.clip(v[('clip', 'drums', 0, '')], C.DRUM_CLIP['knee_db'])]},
              'music': {'inserts': music_eq}}
    return chans, groups, returns, {'vox': [], 'novox': []}, MS.settings(K, **C.MASTER)


def keys():
    from tools.sound.recipes.fut04 import context as C
    return tuple([('kick', C.G.at(s), PM.iround(v, 127)) for s, v in C.KICK]
                 + [('snare', C.G.at(s), PM.iround(v, 127)) for s, v in C.SNARE])


def loop_info():
    from tools.sound.recipes.fut04 import context as C
    return int(C.L), float(C.K.BPM)


def ride_points():
    """The acid ride (bass.RIDES) on the multisample's cc_filter: CC74 from the cutoff multiplier (1.0-2.6, log), CC71
    from the ladder's resonance (0.9-0.96, linear)."""
    import math
    from tools.sound.recipes.fut04 import bass as B, context as C
    ride = B.RIDES[B.PICK]
    n = C.L
    m = B.ride_curve(ride['cutoff'], n, True)
    r = B.ride_curve(ride['res'], n, False)
    clamp = lambda x: min(127, max(0, x))  # noqa: E731
    return {'bass': {74: PM.curve_points(m, n, lambda x: clamp(PM.iround(127.0 * math.log(x) / math.log(2.6)))),
                     71: PM.curve_points(r, n, lambda x: clamp(PM.iround(127.0 * (x - 0.9) / 0.06)))}}


def item_hp_hz(name):
    from tools.sound.recipes.fut04 import context5 as C5
    return next(i for i in C5.ITEMS if i.name == name).hp_hz
