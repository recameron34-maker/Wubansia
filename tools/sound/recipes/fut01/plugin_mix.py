"""FUT01's mix for the plugin: what the shared exporters need to know about this kit's round 5 context loops
(context5.py on context4.py), kept here because shared code imports no kit (ADR 0008). tools/sound/mix_trace.py,
plugin_mix.py and engine_ref.py read it through kits.load('FUT01').RECIPES, as midi.py reads the kit's PARTS.

- render(variant): what the trace runs: the club loop (the wet loop, bass B, through the club master), plus bass A,
  the organ bass and kick A through their chains, for the values their manifest entries fix.
- Capture: what the trace keeps (the dry lanes, the measured and solved values).
- rules(variant): the classification of every call the recipe makes (mix_trace.Rule, after mix_trace.COMMON).
- build(caps): the mix manifest's channels, groups, returns and bus EQ, and the master's settings.
- keys(), loop_info(), ride_points(): the duck keys, the loop, the wobble's CC74 ride.
"""
from tools.sound import mix_trace as T
from tools.sound import plugin_mix as PM
from tools.sound.mix_trace import Rule, array_arg, lane_of, scalar_arg

VARIANTS = {'vox': 'vox', 'novox': 'novox'}                  # the manifest's variants: the recipe's own names
R5_LOOPS = {'vox': ('GarageVoxWet', 'GarageVoxClub'), 'novox': ('GarageNoVoxWet', 'GarageNoVoxClub')}
WOBBLE = (650.0, 2210.0)          # the wobble's CC74 map: base 650 Hz x depth up to 3.4 (context4.WOBBLE)


def render(variant):
    from tools.sound.recipes.fut01 import context4 as C4, context5 as C5
    C5.club(variant)                 # the wet loop (bass B) through the club master
    C5.bass('clean')                 # bass A: its fader and clip ceiling (the dry A loops' bass lane)
    C4.bass_lane('organ')            # the organ bass's chain (no wall, no wobble): its fader
    C4.kick_lane('KickSplit')        # kick A through the kick lane's chain: its clip ceiling


class Capture(T.Capture):
    def hook(self, rec, a, out):
        f, par = rec.fn, rec.parent()
        if f == 'phrase.hits_lane' and par == 'context4.kick_lane' and rec.under(par)['line'] == 'garage':
            side = 'b' if rec.under(par)['name'] == 'KickGarage' else 'a'
            self.put('kick', out, side, loops=side == 'b')
        elif f == 'context4.kick_lane' and a['line'] == 'garage':
            self.lanes[('kick_r4', 'b' if a['name'] == 'KickGarage' else 'a')] = self.copy(out)
        elif f == 'context4.wall' and par == 'context4.bass_lane' and rec.under(par)['line'] == 'garage':
            ch = 'bass_organ' if a['char'] == 'organ' else 'bass'
            side = {'warm': 'b', 'clean': 'a', 'organ': 'organ'}[a['char']]
            self.put(ch, a['x'], side, loops=a['char'] == 'warm')     # the loops play bass B, not A or the organ
        elif f == 'context4.bass_lane' and a['line'] == 'garage':
            ch = 'bass_organ' if a['char'] == 'organ' else 'bass'
            side = {'warm': 'b', 'clean': 'a', 'organ': ''}[a['char']]
            self.value(('fader', ch, side), out[1])
        elif f == 'context4.inserts' and par in ('context5.r4_lanes', 'context5.old_lanes'):
            self.put(a['k'], a['x'])     # the stab: the last one, old_lanes's, without the stabs an accent takes
        elif f == 'context5.inserts':
            self.put(a['k'], a['x'])
        elif f == 'master.soft_clip':
            self.clip(rec, a)
        elif f == 'sends.duck':
            from tools.sound import rt_ref
            self.value(('top', 'space'), rt_ref.loudest_block_db(a['key']))
        elif f == 'context5.lanes' and par == 'context5.wet_mix':
            self.lanes.update({k: self.copy(v) for k, v in out.items()})
        elif f == 'context5.wet_mix':
            self.wet = self.copy(out[0])
        self.master_hook(rec, a, out)

    def clip(self, rec, a):
        par, c = rec.parent(), a['ceiling_db']
        if par == 'context4.kick_clip':
            name = rec.under('context4.kick_lane')['name']
            self.value(('clip', 'kick', 0, 'b' if name == 'KickGarage' else 'a'), c)
        elif par == 'context5.kick':
            self.value(('clip', 'kick', 1, 'b'), c)
        elif par == 'context4.inserts':
            self.value(('clip', rec.under('context4.inserts')['k'], 0, ''), c)
        elif par == 'context5.bass':
            self.value(('clip', 'bass', 0, 'b' if rec.under('context5.bass')['char'] == 'warm' else 'a'), c)
        elif par == 'context5.old_lanes':
            self.value(('clip', 'clap', 0, ''), c)
        elif par == 'context5.inserts':
            self.value(('clip', rec.under('context5.inserts')['k'], 0, ''), c)

    def finish(self):
        from tools.sound import dsp48 as D
        from tools.sound.recipes.fut01 import context5 as C5
        # context5.kick() clips only KickGarage's lane: kick A gets the same second clip, KICK_CLIP_DB under its own
        # true peak, measured on the lane the recipe's chain gives it
        x = self.lanes[('kick_r4', 'a')]
        self.value(('clip', 'kick', 1, 'a'), D.true_peak_db(x) - C5.KICK_CLIP_DB + 0.5)


def rules(variant):
    lane4, lane5 = lane_of('context4.inserts'), lane_of('context5.inserts')
    return T.COMMON + (
        # the recipe's composition
        Rule('context5.(wet_mix|lanes|new_lanes|old_lanes|r4_lanes|returns|bus|kick|bass|ducked|fader_db|inserts)',
             'structure', 'the lanes, their sends and the mix bus, composed'),
        Rule('context5.(mix_meta|accent_meta)', 'structure', "the file's meta: no audio"),
        Rule(r'[a-z0-9_]+\.[A-Za-z0-9_]+', 'structure', "the file's meta: no audio", under='context5.mix_meta'),
        Rule('context4.(club_hits|full_kicks|kick_lane|bass_lane|inserts|kick_clip|circ)', 'structure',
             'the lane chains, composed (their DSP below)'),
        Rule('qa.lufs', 'structure', "the return levels for the file's meta", under='context5.wet_mix'),
        # the dry lanes
        Rule('context4.(bass_voice|line_voices|bass_files|choked_lane|zoned_lane|pluck_lane|fill_lane)', 'dry',
             "the voices: the sampler's bass voice with its glides, the choked open hat, zoned stabs, the pluck "
             "phrase, the fill from its 4th 16th"),
        Rule(r'[a-z0-9_]+\.[A-Za-z0-9_]+', 'dry', 'the bass voice: zone, layer, Hermite repitch, envelope',
             under='context4.bass_voice'),
        Rule('phrase.place', 'dry', 'a voice placed on the grid, its tail wrapping'),
        Rule('(dsp48.fade|fx.swung)', 'dry', 'the fill from its 4th 16th, a 2 ms fade-in (the voice)',
             under='context4.fill_lane'),
        Rule('context5.(accent|riff_lane)', 'dry', 'the accents and the vox synth riff placed (the voices)'),
        Rule('dsp48.taper', 'dry', "the warp stab gated to 1.75 16ths, a 15 ms fade: its note's length and release",
             under='context5.accent'),
        # the channels' inserts
        Rule('dsp48.peaking', 'live', lambda c: f'channels[{lane4(c)}].inserts: peak', under='context4.inserts'),
        Rule('dsp48.peaking', 'live', lambda c: f'channels[{lane5(c)}].inserts: peak', under='context5.inserts'),
        Rule('dsp48.peaking', 'live', f'bus_eq.{variant}: peak', under='context5.bus'),
        Rule('context4.widen', 'live', lambda c: f'channels[{lane4(c)}].inserts: width (freq_hz, order 4)'),
        Rule('dsp48.bfilt', 'live', 'the width insert: its side high-pass', under='context4.widen'),
        Rule('dsp48.true_peak_db', 'fixed', "a clip ceiling: the lane's true peak in the reference loop, less the "
                                            'clip dB, plus 0.5 (absolute, ceiling_mdb)'),
        Rule('master.soft_clip', 'live', 'a lane clip at its fixed ceiling (clip insert)'),
        Rule('context4.fader', 'live', lambda c: f'channels[{c.args.get("k")}].inserts: pan and gain (the fader)'),
        Rule('phrase.pan', 'live', 'a pan insert', under='context4.fader'),
        Rule('phrase.pan', 'live', 'a pan insert', under='context5.fader'),
        Rule('context5.fader', 'live', lambda c: f'channels[{c.args.get("k")}].inserts: pan and gain'),
        Rule('dsp48.undb', 'live', 'a gain insert (the fader)', under='context4.fader'),
        Rule('dsp48.undb', 'live', 'a gain insert (the fader)', under='context5.fader'),
        Rule('phrase.pump_db', 'live', 'an onset duck on the kick or snare key lane (min_velocity for claps >= 0.8)'),
        Rule('dsp48.undb', 'live', "an onset duck's gain", when=array_arg('d')),
        Rule('dsp48.undb', 'live', 'gain inserts (OLD_LIFT, when the variant)', under='context5.old_lanes'),
        Rule('dsp48.undb', 'live', 'the solved fader: a gain insert (when bass a / b)', under='context4.bass_lane',
             when=scalar_arg('d')),
        Rule('qa.lufs', 'fixed', 'the bass fader, solved to BASS_LUFS: exported fixed per A/B side',
             under='context4.bass_lane'),
        Rule('context4.wall', 'live', 'channels[bass].inserts: highpass 450 Hz, order 8, 2 stages, band {250, 0}, '
                                      'when bass a (identity for B and the organ)'),
        Rule('dsp48.lr4', 'live', 'the band split (LR4) of a band insert', under='context4.wall'),
        Rule('parts.hp_steep', 'live', 'the wall: highpass 450 Hz order 8, 2 stages', under='context4.wall'),
        Rule('context4.wobble', 'live', 'channels[bass].inserts: cc_filter lowpass, band {450, 0}, Q 1.6'),
        Rule('context4.wobble_cutoff', 'live', "the cc_filter's CC74 ride (plugin_mix.ride_points; P1 writes it)"),
        Rule('dsp48.(lr4|svf_tpt)', 'live', 'the cc_filter: its LR4 band split and TPT SVF', under='context4.wobble'),
        Rule('context4.width', 'live', 'channels[bass].inserts: width 600 (the whole side)'),
        Rule('dsp48.mono_below', 'live', 'channels[bass].inserts: mono_below 450 Hz, order 8',
             under='context4.bass_lane'),
        Rule('dsp48.dc_block', 'live', f'bus_eq.{variant}: highpass 20 Hz, order 4 (the club loop\'s input, as the '
                                       'wet file is)'),
        # the sends and returns
        Rule('context5.send_bus', 'live', 'the post-fader sends: channels[].sends_pm'),
        Rule('context5.lane_sends', 'live', 'channels[].sends_pm'),
        Rule('dsp48.mono', 'structure', 'a send is mono (sends.py takes D.mono)', under='context5.returns'),
        Rule('dsp48.undb', 'live', 'returns.<return>.return_mdb', under='context5.returns'),
        Rule('context4.resonator_freqs', 'live', 'returns.resonator.tones_mhz'),
        Rule('dsp48.midi_hz', 'live', 'returns.resonator.tones_mhz', under='context4.resonator_freqs'),
    )


def build(caps):
    """The mix: every lane of context4 and context5 as a channel, its inserts in the recipe's order, plus the organ
    bass (context4's chain for it) and the Air Pad (round 3's strip, context.py), which the extended mix plays."""
    from tools.sound import master as MS, sends as S
    from tools.sound.kits import fut01 as K
    from tools.sound.recipes.fut01 import context as C3, context4 as C4, context5 as C5
    lanes4, lanes5 = C4.LANES, C5.LANES
    sends = lambda k: (lanes4[k] if k in lanes4 else lanes5[k])[2:5]  # noqa: E731
    fader4 = lambda k: ([PM.pan(lanes4[k][1])] if lanes4[k][1] else []) + \
        ([PM.gain(lanes4[k][0])] if lanes4[k][0] else [])  # noqa: E731
    kick_duck = PM.pump_duck(C4.DUCK)
    clap_duck = PM.pump_duck(C4.CLAP_DUCK, ('snare',), PM.iround(0.8, 127))
    acc_ducks = [PM.pump_duck(C5.DUCK['acc']), PM.pump_duck(C5.DUCK['acc_clap'], ('snare',), PM.iround(0.8, 127))]
    music_duck = PM.pump_duck(C5.MUSIC_DUCK)

    def lifts(k):
        """OLD_LIFT: dB over round 4's fader, per variant, as when-gains."""
        return [PM.gain(C5.OLD_LIFT[var][k], PM.when(variant=var)) for var in ('vox', 'novox')
                if C5.OLD_LIFT[var].get(k)]

    def widen_ins(k):
        f, a = C4.WIDEN[k]
        return [PM.width(a, f, 4)]

    def clip_of(k, var='vox'):
        return PM.value(caps, ('clip', k, 0, ''), var)

    v = caps['vox'].values
    kick = (PM.clip_per({'b': v[('clip', 'kick', 0, 'b')], 'a': v[('clip', 'kick', 0, 'a')]}, 1.5, 'kick')
            + PM.peaks(C4.EQ['kick'])
            + PM.clip_per({'b': v[('clip', 'kick', 1, 'b')], 'a': v[('clip', 'kick', 1, 'a')]}, 1.5, 'kick'))
    bass_tail = [PM.mono_below(C4.MONO_BELOW, 8)] + PM.peaks(C4.EQ['bass'])
    bass = ([PM.highpass(C4.B.bass.MID_HP, 8, 0.0, 2, (C4.WALL_SPLIT, 0), PM.when(ab='bass', side='a')),
             PM.cc_filter('lowpass', *WOBBLE, C4.WOBBLE['q'], C4.WOBBLE['q'], 0, 0, (C4.WOBBLE['xover'], 0)),
             kick_duck, clap_duck, PM.width(C4.BASS_WIDTH)] + bass_tail
            + [PM.gain(v[('fader', 'bass', 'b')], PM.when(ab='bass', side='b')),
               PM.gain(v[('fader', 'bass', 'a')], PM.when(ab='bass', side='a'))]
            + PM.clip_per({'b': v[('clip', 'bass', 0, 'b')], 'a': v[('clip', 'bass', 0, 'a')]}, 1.5, 'bass'))
    organ = [kick_duck, clap_duck, PM.width(C4.BASS_WIDTH)] + bass_tail + [PM.gain(v[('fader', 'bass_organ', '')])]
    stab_clip = {'vox': clip_of('stab', 'vox'), 'novox': clip_of('stab', 'novox')}
    ch = PM.channel
    chans = [
        ch('kick', 'kick', 'none', kick),
        ch('bass', 'bass', 'none', bass),
        ch('bass_organ', 'bass', 'none', organ),
        ch('clap', 'snare', 'none', PM.peaks(C4.EQ['clap']) + widen_ins('clap') + fader4('clap')
           + [PM.clip(clip_of('clap'))], sends('clap')),
        ch('hat_c', 'hats', 'none', PM.peaks(C4.EQ['hat_c']) + widen_ins('hat_c') + fader4('hat_c'), sends('hat_c')),
        ch('hat_o', 'hats', 'none', PM.peaks(C4.EQ['hat_o']) + widen_ins('hat_o') + fader4('hat_o'), sends('hat_o')),
        ch('tamb', 'perc', 'none', PM.peaks(C4.EQ['tamb']) + widen_ins('tamb') + fader4('tamb'), sends('tamb')),
        ch('rim', 'perc', 'none', fader4('rim'), sends('rim')),
        ch('perc', 'perc', 'none', [PM.clip(clip_of('perc'))] + fader4('perc'), sends('perc')),
        ch('stab', 'chords', 'none', PM.peaks(C4.EQ['stab']) + PM.clip_per(stab_clip, 1.5, 'variant')
           + fader4('stab') + [music_duck] + lifts('stab'), sends('stab')),
        ch('pluck', 'lead', 'none', fader4('pluck') + [music_duck] + lifts('pluck'), sends('pluck')),
        ch('fill', 'fx', 'none', fader4('fill') + lifts('fill'), sends('fill')),
        # no loop lane in round 5: the Air Pad (MIDI channel 13, held under the extended mix's breaks) on round 3's
        # strip for it (context.py: mono under 350 Hz, its EQ, its fader and Space send; no duck)
        ch('pad', 'chords', 'none', [PM.mono_below(C3.MONO_BELOW['pad'], 8)] + PM.peaks(C3.EQ['pad'])
           + ([PM.pan(C3.LANES['pad'][2])] if C3.LANES['pad'][2] else []) + [PM.gain(C3.LANES['pad'][1])],
           C3.LANES['pad'][3:6]),
        ch('vsynth', 'vox', 'none', PM.peaks(C5.EQ['vsynth']) + [PM.clip(clip_of('vsynth'))]
           + [PM.pump_duck(C5.DUCK['vsynth']), PM.gain(C5.LANES['vsynth'][0])], sends('vsynth')),
    ]
    for k in ('acc_rev', 'acc_up', 'acc_hit', 'acc_warp'):
        var = 'novox' if k == 'acc_warp' else 'vox'
        ins = [PM.clip(clip_of(k, var))] if k in C5.CLIP_DB else []
        p = C5.LANES[k][1]
        novox_lift = C5.fader_db(k, 'novox') - C5.fader_db(k, 'vox')
        ins += acc_ducks + ([PM.pan(p)] if p else []) + [PM.gain(C5.LANES[k][0]),
                                                         PM.gain(novox_lift, PM.when(variant='novox'))]
        chans.append(ch(k, 'accents', 'none', ins, sends(k)))
    returns = {
        'space': PM.space_return(K, 'send r4', C4.PLATE, [PM.duck_level(
            'vsynth', S.DUCK['depth_db'], S.DUCK['range_db'], v[('top', 'space')], S.DUCK['attack'],
            S.DUCK['release'])], S.SPACE_RETURN_DB),
        'echo': PM.echo_return(K, 'echo r4', C4.ECHO, [], S.ECHO_STEPS, S.ECHO_RETURN_DB),
        'resonator': PM.resonator_return(C4.resonator_freqs(), [], S.RESONATOR['tau'], S.RESONATOR['send_hp'],
                                         S.RESONATOR_RETURN_DB),
    }
    bus_eq = {var: PM.peaks(C5.BUS_EQ[var]) + [PM.highpass(20.0, 4)] for var in ('vox', 'novox')}
    return chans, {'drums': {'inserts': []}, 'music': {'inserts': []}}, returns, bus_eq, MS.settings(K)


def keys():
    """The duck keys: every kick on the kick lane, every clap on the snare lane (round(v x 127))."""
    from tools.sound.recipes.fut01 import context4 as C4
    return tuple([('kick', C4.G.at(s), PM.iround(v, 127)) for s, v in C4.KICK]
                 + [('snare', C4.G.at(s), PM.iround(v, 127)) for s, v in C4.CLAP])


def loop_info():
    from tools.sound.recipes.fut01 import context4 as C4
    return int(C4.L), float(C4.K.BPM)


def ride_points():
    from tools.sound.recipes.fut01 import context4 as C4
    lo, hi = WOBBLE
    return {'bass': {74: PM.curve_points(C4.wobble_cutoff(C4.G), C4.L, lambda f: PM.cc_value(f, lo, hi)),
                     71: [(0, 0)]}}


def item_hp_hz(name):
    from tools.sound.recipes.fut01 import context5 as C5
    return next(i for i in C5.ITEMS if i.name == name).hp_hz
