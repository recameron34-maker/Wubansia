"""FUT01 in the plugin (ADR 0020): what tools/sound/plugin_kit.py and phrase_tables.py read off this kit's recipes to
write content/plugin/fut01/FUT01_kit.json. Shared code never imports a kit (ADR 0008), so the kit's facts live here:
its phrase lanes, the rules its instruments play by (each with the recipe it comes from), its lane labels and the mix
channels its sounds play on (P3's, in FUT01_mix.json)."""
import math

from tools.sound.phrase_tables import SR, Spec, loops_path
from tools.sound.recipes.fut01 import context4 as C, context5 as C5, fx, music as M, voxsynth as V

LANE_LABELS = ('Kick', 'Clap', 'Hats', 'Perc', 'Warp Bass', 'Chrome Pluck', 'Organ Stab', 'Vox Synth', 'Accents', 'FX')
KICK_AB = ('KickSplit', 'KickGarage')   # drum note 36's A and B: context4 plays B, Garage (KICK, FILES)
MIX_CHANNELS = {'kick': 'kick', 'bass': 'bass', 'bass_organ': 'bass', 'clap': 'snare', 'rim': 'perc', 'hat_c': 'hats',
                'hat_o': 'hats', 'tamb': 'perc', 'perc': 'perc', 'stab': 'chords', 'pad': 'chords', 'pluck': 'lead',
                'fill': 'fx', 'vsynth': 'vox', 'acc_rev': 'accents', 'acc_up': 'accents', 'acc_hit': 'accents',
                'acc_warp': 'accents', 'vsynth_ms': 'vox'}
MIX = dict(drums={'KickGarage': 'kick', 'KickSplit': 'kick', 'ClapGarage': 'clap', 'RimShot': 'rim',
                  'HatClosed': 'hat_c', 'HatOpen': 'hat_o', 'Tambourine': 'tamb', 'AlloyHit': 'perc'},
           channels={1: 'bass', 2: 'pluck', 4: 'stab', 5: 'vsynth_ms', 11: 'bass_organ', 12: 'pluck', 13: 'pad'},
           accents={'ReverseStab': 'acc_rev', 'NoiseUplifter': 'acc_up', 'OrganHit': 'acc_hit', 'WarpStab': 'acc_warp',
                    None: 'acc_hit'},
           fx='fill', chops='vsynth')
EXTRAS = {'Organ Bass': 'bass', 'Organ Lead': 'lead', 'Air Pad': 'pad'}   # how each extra plays (plugin_kit.rules)

INTERP = 'hermite'                         # basslines.hermite: the bass voice and riffs.lead_lane repitch with it
GLIDE = ('raised_cosine', round(C.GLIDES['slide'] * 1e6), round(C.GLIDES['dip'] * 1e6))   # a slide, a dip
BASS_ENV = (1000, 30000)                   # basslines.ATTACK, RELEASE: the bass voice
LEAD_RELEASE = 40000                       # riffs.lead_lane: gated to its length plus 40 ms
CHORD_LAW = 'pow15'                        # context4.zoned_lane: the whole file, (v / layer) ^ 1.5
VOX = (round(V.CHOP_REL * 1e6), 'pow15', int(math.floor(V.ONSET_LEAD['d'] * SR + 0.5)))   # release, law, pre-roll
CHOKE = (5000, 'linear')                   # context4.choked_lane: the open hat, 5 ms linear
LAYERS = {}


def chop_preroll(item):
    """A chop's onset before its vowel: 6 ms for a 'd' onset (voxsynth.ONSET_LEAD)."""
    return int(math.floor(V.ONSET_LEAD[V.CHOP_BY_NAME[item][4]] * SR + 0.5))


def accent_gain_mdb(item, audio=None):
    return 0                               # each accent its own channel and fader (context5.LANES)


# The multisample path (a phrase channel's notes off the kit's tempo, with phrases off, and from MIDI in) at its stem's
# level: each multisample's own gain (mdB), measured with groovecore render, phrases off against phrases auto, pass 2 of
# the loop (content/plugin/README.md). The vox synth plays on vsynth_ms, vsynth with P3's corrective EQ in front (+4 dB
# at 400 Hz, -5.5 dB at 1.2 kHz): the riff's chops sit lower than its held "ah".
MS_GAIN_MDB = {'ChromePluck': 600, 'VoxSynth': 7550}


def play(kind, item, it, key, r):
    """FUT01's own playing rules, over plugin_kit's defaults."""
    if kind in ('bass', 'lead', 'vox') and item in MS_GAIN_MDB:   # the multisample at its stem's level
        r['gain_mdb'] = MS_GAIN_MDB[item]
    if kind == 'accents' and item == 'WarpStab':   # context5.accent: gated, a 15 ms fade (GATE_FADE_S)
        r.update(mode='gated', release_us=round(C5.GATE_FADE_S * 1e6))
    if kind == 'fx' and key is not None and key.start:   # context4.fill_lane: from its 16th 4, a 2 ms fade-in
        r.update(start_frames=fx.swung(key.start), attack_us=round(C.FILL_FADE_S * 1e6))
    return r


DERIVED = ()


def render_derived():
    return {}


def phrases():
    """The lead (the chrome pluck phrase) and the vox (the vox synth riff), as the loop plays their files. The riff
    has its last chop's throw in the audio (voxsynth.throw: the dotted-8th ping-pong and plate): its throw is baked."""
    riff_pre = {s: int(math.floor(V.ONSET_LEAD[o] * SR + 0.5)) for s, _, _, _, _, o in V.RIFF}
    return (
        Spec('FUT01', 'pluck', 'lead', 2, (loops_path('ChromePluckPhrase'), None), '', M.PHRASE_STEPS,
             tuple((s, n, ()) for s, n, _, _ in M.PHRASE), 0, 'pluck',
             'tools.sound.recipes.fut01.music.pluck_phrase'),
        Spec('FUT01', 'riff', 'vox', 5, (loops_path('VoxSynthRiff'), None), '', V.RIFF_STEPS,
             tuple((s, n, ()) for s, n, *_ in V.RIFF), riff_pre, 'vsynth', 'tools.sound.recipes.fut01.voxsynth.riff',
             throw='baked'),
    )
