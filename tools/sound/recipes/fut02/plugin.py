"""FUT02 in the plugin (ADR 0020): what tools/sound/plugin_kit.py and phrase_tables.py read off this kit's recipes to
write content/plugin/fut02/FUT02_kit.json. Shared code never imports a kit (ADR 0008), so the kit's facts live here:
its phrase lanes and derived stems, the rules its instruments play by (each with the recipe it comes from), its lane
labels and the mix channels its sounds play on (P3's, in FUT02_mix.json)."""
from tools.sound import dsp48 as D
from tools.sound.kits import fut02 as K
from tools.sound.phrase_tables import SR, Spec, iround, loops_path
from tools.sound.recipes.fut02 import bass as B, context as C, context5 as C5, lead as Ld, voxsynth as Vs

LANE_LABELS = ('Kick', 'Clap', 'Hats', 'Zap', 'Roll Bass', 'Chrome Arp', 'Starfield Pad', 'Vox Synth', 'Accents', 'FX')
KICK_AB = None
MIX_CHANNELS = {'kick': 'kick', 'clap': 'snare', 'hat': 'hats', 'zap': 'perc', 'bass': 'bass', 'glide': 'bass',
                'pad': 'chords', 'arp': 'lead', 'fill': 'fx', 'vox': 'vox', 'acc_laser': 'accents',
                'acc_glass': 'accents', 'acc_sweep': 'accents', 'acc_revchord': 'accents', 'acc_arpstab': 'accents'}
MIX = dict(drums={'KickSplit': 'kick', 'ElectroClap': 'clap', 'ClosedHat': 'hat', 'OpenHat': 'hat', 'Zap': 'zap'},
           channels={1: 'bass', 2: 'arp', 4: 'pad', 5: 'vox', 11: 'glide', 12: 'pad', 13: 'acc_glass'},
           accents={'LaserChordStab': 'acc_laser', 'GlassPing': 'acc_glass', 'StarfieldSweep': 'acc_sweep',
                    'ReverseChord': 'acc_revchord', 'ArpStab': 'acc_arpstab', 'NoiseUplifter': 'acc_sweep',
                    None: 'acc_arpstab'},
           fx='fill', chops='vox')
EXTRAS = {'Drop Glide': 'drop_glide', 'Laser Chord Stab': 'gated_stab', 'Glass Ping': 'melodic'}

INTERP = 'sinc'
GLIDE = ('exponential', iround(B.GLIDE_TAU, 1e6), 0)   # bass.GLIDE_TAU: the line's glide, a time constant
BASS_ENV = (1000, 30000)                   # no recipe plays the Roll Bass multisample: the bass voice's (FUT01)
LEAD_RELEASE = iround(Ld.RELEASE, 1e6)     # lead.RELEASE: each arp note released over 80 ms
CHORD_LAW = 'layer_only'
VOX = (iround(Vs.RELEASE, 1e6), 'layer_only', iround(Vs.PRE, SR))   # voxsynth.RELEASE; the velocity picks the vowel;
#                                                                      PRE: 20 ms before the vowel
CHOKE = (iround(C.drums.CHOKE['fade_s'], 1e6), 'cos2')   # drums.CHOKE: the closed hat cuts the open, 4 ms cos2
LAYERS = {}
STEM_GAIN_DB = -3                          # the vox lane peaks at +1.75 dBTP: its stems are stored 3 dB down
TWICE = (0, 32)                            # the extended mix's call, riff and answer vox lines start the riff here
DERIVED = ('Stems/Context Vox Riff.wav', 'Stems/Context Vox Riff Twice.wav')


def chop_preroll(item):
    return iround(Vs.PRE, SR)               # voxsynth.vox_chop: sung from PRE


def accent_gain_mdb(item, audio=None):
    return 0                                # each accent its own channel and gain (context5.ACCENTS, P3)


def play(kind, item, it, key, r):
    """FUT02's own playing rules, over plugin_kit's defaults."""
    if kind == 'drum' and item == 'Zap':    # context.ZAP_PAN: each laser its own side, by hit in the 4 bars
        r['pan_cycle_pm'] = [iround(p, 1000) for p in C.ZAP_PAN]
    if kind == 'drop_glide':                # context.glide_lane: struck, its key lifted, released over 20 ms
        r.update(mode='gated', release_us=iround(C.GLIDE_RELEASE, 1e6))
    return r


def sung(at):
    """The loop's vox riff from 16th `at` (voxsynth.sung_parts with the loop's seed): the notes and the throw."""
    return sum(Vs.sung_parts(C5.G, at, C5.L, lambda *k: D.rng(K.KIT, 'ContextVoxRiff', *k)))


def render_derived():
    """The loop's vox lane before its inserts (from 16th 16), and the same riff from 16ths 0 and 32 (wrapped round
    the 4 bars), each stored STEM_GAIN_DB down."""
    g = D.undb(STEM_GAIN_DB)
    return {DERIVED[0]: sung(C5.RIFF_AT) * g, DERIVED[1]: (sung(TWICE[0]) + sung(TWICE[1])) * g}


def phrases():
    """The bass (the Orbit line, the loop's, and the Relay line), the lead (the chrome arp phrase) and the vox (the
    loop's riff, and the riff twice for the extended mix's other vox lines)."""
    kicks, sub = tuple(B.KICK_STEPS), iround(B.SUB_LEAD, SR)
    lines = tuple(
        Spec('FUT02', name.lower(), 'bass', 1, (loops_path(f'{name}LineClean'), loops_path(f'{name}LineWarm')),
             'bass', B.LINE_STEPS, tuple((s, n, ()) for s, n, *_ in B.LINES[name]), sub, 'bass',
             'tools.sound.recipes.fut02.bass.line_lane', kicks=kicks, fade_us=iround(C.MUTE_EDGE, 1e6))
        for name in (B.PICK,) + tuple(n for n in B.LINES if n != B.PICK))
    pre, gain = int(0.05 * SR), -1000 * STEM_GAIN_DB       # sung_parts' pre: 50 ms before each note
    return lines + (
        Spec('FUT02', 'arp', 'lead', 2, (loops_path('ChromeArpPhrase'), None), '', Ld.PHRASE_STEPS,
             tuple((s, n, ()) for s, n, _, _ in Ld.phrase_notes()), 0, 'arp',
             'tools.sound.recipes.fut02.lead.arp_phrase', fade_us=iround(C.ARP_REST[1], 1e6)),
        Spec('FUT02', 'vox', 'vox', 5, (DERIVED[0], None), '', 64,
             tuple((s + C5.RIFF_AT, n, ()) for s, n, _, _ in Vs.RIFF), pre, 'vox',
             'tools.sound.recipes.fut02.voxsynth.sung_parts', gain_mdb=gain, derived=True, frames=C5.L, looped=False),
        Spec('FUT02', 'vox_twice', 'vox', 5, (DERIVED[1], None), '', 64,
             tuple(sorted((s + at, n, ()) for at in TWICE for s, n, _, _ in Vs.RIFF)), pre, 'vox',
             'tools.sound.recipes.fut02.voxsynth.sung_parts', gain_mdb=gain, derived=True, frames=C5.L),
    )
