"""FUT03's mix for the plugin: what the shared exporters need to know about this kit's round 5 context loops
(context5.py on context.py), kept here because shared code imports no kit (ADR 0008). tools/sound/mix_trace.py,
plugin_mix.py and engine_ref.py read it through kits.load('FUT03').RECIPES.

The rimshot gets its own channel on the perc lane's strip (the perc lane split by instrument); the vocal bus, whose
only lane is the vox, is the vox channel's own inserts; the Keys EP and the FX, which the loops don't play, get plain
music-bus strips. The mix bus's zero-phase 10 Hz DC clean is dropped: flat over 25 Hz, while any causal high-pass
there turns the kick's 49 Hz (a 4th-order one by about 46 degrees); the club master's 25 Hz low cut follows.
"""
from tools.sound import mix_trace as T
from tools.sound import plugin_mix as PM
from tools.sound.mix_trace import Rule

VARIANTS = {'vox': 'vox', 'novox': 'novox'}
R5_LOOPS = {'vox': ('ContextLoopVoxWet', 'ContextLoopVoxClub'),
            'novox': ('ContextLoopNoVoxWet', 'ContextLoopNoVoxClub')}
STAB = (900.0, 5000.0)              # the stab's CC74 map: context.STAB_RIDE


def render(variant):
    from tools.sound.recipes.fut03 import context5 as C5
    C5.club(variant)                 # the wet loop (bass B) through the club master


class Capture(T.Capture):
    def hook(self, rec, a, out):
        f, par = rec.fn, rec.parent()
        lane = rec.under('context.lane')
        if f == 'context.panned' and par == 'context.lane' and lane['key'] in ('kick', 'clap', 'hat', 'shaker',
                                                                                 'perc', 'bass'):
            self.put(lane['key'], a['x'])
        elif f == 'chords.lp_ride' and lane and lane['key'] == 'stab':
            self.put('stab', a['x'])
        elif f == 'context.file_lane' and lane and lane['key'] == 'pad':
            self.put('pad', out)
        elif f == 'context5.vox_lane':
            self.put('vox', out)
        elif f == 'phrase.circular' and par == 'context5.accent_lane':
            self.put('accent', a['x'])
        elif f == 'master.glue' and par == 'context5.lane':
            self.value(('comp', 'vox'), out[1]['threshold_dbfs'])
        elif f == 'master.soft_clip' and par == 'context5.mixdown':
            self.value(('clip', 'drums', 0, ''), a['ceiling_db'])
        elif f == 'sends.duck':
            from tools.sound import rt_ref
            self.value(('top', 'space'), rt_ref.loudest_block_db(a['key']))
        elif f == 'context5.mix' and par == 'context5.wet':
            self.lanes.update({k: self.copy(v) for k, v in out.items()})
        elif f == 'context5.wet':
            self.wet = self.copy(out.audio)
        self.master_hook(rec, a, out)

    def finish(self):
        # the rimshot plays on its own channel (lane perc, the perc lane's strip): the perc lane split by instrument,
        # each instrument's round robins counted as perc_hits counts them
        import numpy as np
        from tools.sound.recipes.fut03 import context as C, groove as Gr
        rim = Gr.perc_hits(tuple(h for h in C.PERC if h[1] == 'Rimshot'), C.G, C.L)
        rest = Gr.perc_hits(tuple(h for h in C.PERC if h[1] != 'Rimshot'), C.G, C.L)
        if float(np.max(np.abs(rim + rest - self.dry['perc']))) > 1e-9:
            raise ValueError("FUT03: the rim and the hand drums don't add up to the recipe's perc lane")
        self.put('rim', rim)
        self.put('perc', rest)


def rules(variant):
    return T.COMMON + (
        Rule('context5.(wet|mix|lane|mixdown|returns|send_bus|vox_lane|accent_lane)', 'structure',
             'the lanes, their sends, the buses and the returns, composed'),
        Rule('context5.(meta|lane_meta)', 'structure', "the file's meta: no audio"),
        Rule(r'[a-z0-9_]+\.[A-Za-z0-9_]+', 'structure', "the file's meta: no audio", under='context5.meta'),
        Rule('qa.lufs', 'structure', 'the return levels for the meta', under='context5.wet'),
        Rule('context.(club_hits|kicks|pump|fader_db|layer_of|lane)', 'structure',
             "the lane chains (their DSP below), names, the kick onsets the ducks key on"),
        # the dry lanes
        Rule('(phrase.place|phrase.kit_files|dsp48.st|groove.perc_hits|context.file_lane|context.bass_lane|'
             'phrase.pan)', 'dry', "the voices: drums, hats (choked), hand drums (their own pans: kit.json), the "
                                   "pad held, the bass line tiled", under='context.lane',
             when=lambda c: c.parent() != 'context.panned'),
        Rule('(voxsynth.riff_tail|phrase.place|phrase.kit_files|dsp48.undb)', 'dry',
             "the vox riff's chops and echoes (synthesized at render time, at the riff file's gain: P2's stem)",
             under='context5.vox_lane'),
        Rule(r'[a-z0-9_]+\.[A-Za-z0-9_]+', 'dry', 'the vox riff, synthesized', under='voxsynth.riff_tail'),
        Rule('(phrase.place|phrase.kit_files|dsp48.st|dsp48.undb)', 'dry',
             "the accents placed at their trims (ACCENT_TRIM_DB: kit.json gain_mdb, P2)",
             under='context5.accent_lane', when=lambda c: not c.under('phrase.circular')),
        # the channels
        Rule('chords.lp_ride', 'live', 'channels[stab].inserts: cc_filter lowpass 900-5000 Hz, Q 0.9 (the ride on '
                                       'CC74: plugin_mix.ride_points)'),
        Rule('dsp48.svf_tpt', 'live', "the cc_filter's TPT SVF", under='chords.lp_ride'),
        Rule('phrase.pump_db', 'live', 'onset ducks keyed on the kick lane: the stab pump (7 dB), the pad pump (4 dB), '
                                       "the kick's room (the vox, the accents) and the returns' pump (4 dB)"),
        Rule('dsp48.undb', 'live', "the ducks' gain", under='context.pump'),
        Rule('context.panned', 'live', 'a pan insert (none at the centre)'),
        Rule('phrase.pan', 'live', 'a pan insert', under='context.panned'),
        Rule('dsp48.st', 'structure', 'mono to stereo', under='context.panned'),
        Rule('dsp48.undb', 'live', 'a gain insert: the fader (and the vox makeup)',
             when=lambda c: c.parent() in ('context.lane', 'context5.lane')),
        Rule('master.glue', 'fixed', "channels[vox].inserts: comp at the threshold solved for 5 dB of reduction "
                                     "(master.glue's algorithm, fixed: rt_ref.glue)", under='context5.lane'),
        Rule('(phrase.circular|dsp48.bfilt)', 'live', 'channels[accent].inserts: highpass 300 Hz, order 2',
             under='context5.accent_lane', when=lambda c: not c.under('context.eq')),
        Rule('context.eq', 'live', 'peak inserts: the accent EQ, the music bus EQ (groups.music, copied into the '
                                   'returns), the vocal bus EQ (channels[vox], its only lane)'),
        Rule('(phrase.circular|dsp48.peaking)', 'live', 'peak inserts', under='context.eq'),
        Rule('master.soft_clip', 'live', 'groups.drums.inserts: clip at the fixed ceiling', under='context5.mixdown'),
        Rule('context.drum_ceiling_db', 'fixed', "the drum bus clip's ceiling: the kick lane's true peak, fixed"),
        Rule('mixmeter.true_peak_db', 'fixed', "the kick lane's true peak", under='context.drum_ceiling_db'),
        Rule('context.dc_clean', 'dropped', "the mix bus's zero-phase 10 Hz DC clean: flat over 25 Hz, while a causal "
                                            "high-pass there turns the kick's 49 Hz (46 degrees at 4th order); the "
                                            "club master's 25 Hz low cut follows"),
        Rule('(phrase.circular|dsp48.bfilt)', 'dropped', 'the DC clean (dropped)', under='context.dc_clean'),
        Rule('dsp48.mono', 'structure', 'a send is mono (sends.py takes D.mono)', under='context5.returns'),
        Rule('dsp48.undb', 'live', 'returns.<return>.return_mdb', under='context5.returns'),
    )


def build(caps):
    from tools.sound import master as MS, sends as S
    from tools.sound.kits import fut03 as K
    from tools.sound.recipes.fut03 import context as C, context5 as C5
    v = caps['vox'].values
    L5 = C5.LANES
    fader = lambda k: ([PM.pan(L5[k][2])] if L5[k][2] else []) + ([PM.gain(L5[k][1])] if L5[k][1] else [])  # noqa
    sends = lambda k: tuple(L5[k][3:5]) + (0.0,)  # noqa: E731
    room = PM.pump_duck(C.KICK_ROOM)
    comp_kw = C.VOX_COMP
    ch = PM.channel
    chans = [
        ch('kick', 'kick', 'drums', fader('kick'), sends('kick')),
        ch('clap', 'snare', 'drums', fader('clap'), sends('clap')),
        ch('rim', 'perc', 'drums', fader('perc'), sends('perc')),
        ch('hat', 'hats', 'drums', fader('hat'), sends('hat')),
        ch('shaker', 'perc', 'drums', fader('shaker'), sends('shaker')),
        ch('perc', 'perc', 'drums', fader('perc'), sends('perc')),
        ch('bass', 'bass', 'none', [PM.gain(C.BASS_FADER['clean'], PM.when(ab='bass', side='a'))]
           + ([PM.gain(C.BASS_FADER['warm'], PM.when(ab='bass', side='b'))] if C.BASS_FADER['warm'] else []),
           sends('bass')),
        ch('stab', 'chords', 'music', [PM.cc_filter('lowpass', *STAB, 0.9, 0.9), PM.pump_duck(C.PUMP['stab'])]
           + fader('stab'), sends('stab')),
        ch('pad', 'chords', 'music', [PM.pump_duck(C.PUMP['pad'])] + fader('pad'), sends('pad')),
        # no loop lane: the Keys EP (the extended mix's lead) and the FX on plain music-bus strips, the stab's level
        # and sends for the lead, the other kits' fill level and sends for the FX
        ch('lead', 'lead', 'music', [PM.gain(L5['stab'][1])], sends('stab')),
        ch('fx', 'fx', 'music', [PM.gain(-5.0)], (0.25, 0.25, 0.0)),
        ch('vox', 'vox', 'none', fader('vox') + [
            PM.comp(v[('comp', 'vox')], comp_kw['ratio'], comp_kw['knee_db'], comp_kw['attack_s'],
                    comp_kw['release_s'][0], comp_kw['release_s'][1], C5.VOX_MAKEUP_DB), room]
           + PM.peaks(C5.VOCAL_EQ), sends('vox')),
        ch('accent', 'accents', 'music', [PM.highpass(C5.ACCENT_HP, 2)] + PM.peaks(C5.ACCENT_EQ) + fader('accent')
           + [room], sends('accent')),
    ]
    rp = PM.pump_duck(C.RETURN_PUMP)
    music_eq = PM.peaks(C.MUSIC_EQ)
    returns = {
        'space': PM.space_return(K, 'send', C.SPACE, [PM.duck_level(
            'vox', S.DUCK['depth_db'], S.DUCK['range_db'], v[('top', 'space')], S.DUCK['attack'], S.DUCK['release']),
            rp] + music_eq, S.SPACE_RETURN_DB),
        'echo': PM.echo_return(K, 'echo', C.ECHO, [rp] + music_eq, C.ECHO_STEPS, S.ECHO_RETURN_DB),
        'resonator': PM.resonator_return((), [], S.RESONATOR['tau'], S.RESONATOR['send_hp'], S.RESONATOR_RETURN_DB),
    }
    groups = {'drums': {'inserts': [PM.clip(v[('clip', 'drums', 0, '')], C.DRUM_KNEE_DB)]},
              'music': {'inserts': music_eq}}
    return chans, groups, returns, {'vox': [], 'novox': []}, MS.settings(K, max_reduction_db=C.MASTER_REDUCTION_DB)


def keys():
    from tools.sound.recipes.fut03 import context as C
    return tuple([('kick', C.G.at(s), PM.iround(v, 127)) for s, v in C.KICK]
                 + [('snare', C.G.at(s), PM.iround(v, 127)) for s, v in C.CLAP])


def loop_info():
    from tools.sound.recipes.fut03 import context as C
    return int(C.L), float(C.K.BPM)


def ride_points():
    import numpy as np
    from tools.sound.recipes.fut03 import context as C
    u = np.arange(C.L) / C.L
    fc = C.STAB_RIDE[0] * (C.STAB_RIDE[1] / C.STAB_RIDE[0]) ** (0.5 - 0.5 * np.cos(2 * np.pi * u))
    lo, hi = STAB
    return {'stab': {74: PM.curve_points(fc, C.L, lambda f: PM.cc_value(f, lo, hi)), 71: [(0, 0)]}}


def item_hp_hz(name):
    from tools.sound.recipes.fut03 import context5 as C5
    return next(i for i in C5.ITEMS if i.name == name).hp_hz
