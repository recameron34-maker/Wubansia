"""FUT03 in the plugin (ADR 0020): what tools/sound/plugin_kit.py and phrase_tables.py read off this kit's recipes to
write content/plugin/fut03/FUT03_kit.json. Shared code never imports a kit (ADR 0008), so the kit's facts live here:
its phrase lanes and derived stems, the rules its instruments play by (each with the recipe it comes from), its lane
labels and the mix channels its sounds play on (P3's, in FUT03_mix.json)."""
import numpy as np

from tools.sound import dsp48 as D
from tools.sound import phrase as Ph
from tools.sound.kits import fut03 as K
from tools.sound.phrase_tables import SR, Spec, iround, loops_path
from tools.sound.recipes.fut03 import context5 as C5, groove, rubber as R, voxsynth as VS

LANE_LABELS = ('Kick', 'Clap', 'Hats', 'Perc', 'Rubber Bass', 'Keys EP', 'Dub Stab', 'Vox Synth', 'Accents', 'FX')
KICK_AB = None
MIX_CHANNELS = {'kick': 'kick', 'clap': 'snare', 'rim': 'perc', 'hat': 'hats', 'shaker': 'perc', 'perc': 'perc',
                'bass': 'bass', 'stab': 'chords', 'pad': 'chords', 'vox': 'vox', 'accent': 'accents', 'lead': 'lead',
                'fx': 'fx'}
MIX = dict(drums={'KickDeep': 'kick', 'ClapSnare': 'clap', 'Rimshot': 'rim', 'ClosedHatDeep': 'hat',
                  'OpenHatDeep': 'hat', 'ShakerDeep': 'shaker', 'Conga': 'perc', 'Bongo': 'perc'},
           channels={1: 'bass', 2: 'lead', 4: 'stab', 5: 'vox', 11: 'pad'},
           accents={None: 'accent'}, fx='fx', chops='vox')
EXTRAS = {'Pad Warm': 'pad'}

INTERP = 'sinc'
GLIDE = ('raised_cosine', 50000, 0)        # rubber.LINES' glides inside notes, 40-70 ms (midi.glided writes them)
BASS_ENV = (1000, iround(R.RELEASE_S, 1e6))   # the files release over 80 ms after their loop (rubber.RELEASE_S)
LEAD_RELEASE = 40000                       # no recipe plays the Keys EP multisample: FUT01's lead_lane rule
CHORD_LAW = 'layer_only'                   # context.stab_lane: phrase.hits_lane, the layer alone
VOX = (70000, 'pow15', iround(VS.LEAD, SR))   # voxsynth.chop_dry's release; LEAD: the breath 25 ms before the note
CHOKE = (iround(groove.CHOKE['fade_s'], 1e6), 'cos2')   # groove.CHOKE: the closed hat cuts the open, 4 ms cos2
# Sub Deep under both Rubber Basses on multisample voices, 11 dB down: there its sine meets the Rubber Bass's own sub
# (both hard layers' loops under 120 Hz: Sub Deep 10.3-13.2 dB over them, 11.1 on average)
LAYERS = {'SubDeep': (('RubberBassClean', 'RubberBassWarm'), -11000)}
STEM_GAIN_DB = 0                           # the vox lane: -1.65 dBTP
TWICE_GAIN_DB = -1                         # the riff twice: its echoes under the second riff reach -0.92 dBTP
TWICE = (0, 32)                            # the extended mix's call, riff and answer vox lines start the riff here
DERIVED = ('Stems/Context Vox Riff.wav', 'Stems/Context Vox Riff Twice.wav')


def chop_preroll(item):
    return iround(VS.LEAD, SR)              # voxsynth.LEAD: 25 ms from the file's start to its note


def accent_gain_mdb(item, audio=None):
    """The accent lane's trims (context5.ACCENT_TRIM_DB): every accent shares one channel."""
    return iround(C5.ACCENT_TRIM_DB.get(item, 0.0), 1000)


def play(kind, item, it, key, r):
    return r


def riff_frames():
    """The riff before it wraps: its 2 bars and TAIL_STEPS of echoes (voxsynth.riff_tail's length)."""
    g = Ph.Grid(K, VS.RIFF_STEPS)
    return g.length + int(VS.TAIL_STEPS * g.s16)


def render_derived():
    """The loop's vox lane before its inserts (the riff from 16th 32 at PhraseVoxRiff's gain, unwrapped: its echoes
    run 2 bars past the 4-bar cycle), and the same riff from 16ths 0 and 32, TWICE_GAIN_DB down."""
    _, _, gain_db = Ph.kit_files(K, 'PhraseVoxRiff')      # context5.vox_lane: the riff at its item's gain
    tail = VS.riff_tail() * D.undb(gain_db)

    def placed(ats):
        y = np.zeros((C5.G.at(max(ats)) + len(tail), 2))
        for at in ats:
            y[C5.G.at(at):C5.G.at(at) + len(tail)] += tail
        return y
    return {DERIVED[0]: placed((C5.VOX_AT,)) * D.undb(STEM_GAIN_DB), DERIVED[1]: placed(TWICE) * D.undb(TWICE_GAIN_DB)}


def phrases():
    """The bass (the Rubber line, the loop's, and the Deep line) and the vox (the loop's riff, and the riff twice for
    the extended mix's other vox lines). The riff has its dub echo and its last chop's dub throw in the audio
    (voxsynth.riff_tail): its throw is baked."""
    kicks, sub = tuple(R.KICKS), iround(R.SUB_LEAD, SR)
    lines = tuple(
        Spec('FUT03', name.lower(), 'bass', 1, (loops_path(f'Line{name}Clean'), loops_path(f'Line{name}Warm')), 'bass',
             R.LOOP_STEPS, tuple((s, n, (g[0],) if g else ()) for s, _, n, _, g in R.LINES[name]), sub, 'bass',
             'tools.sound.recipes.fut03.rubber.line_lane', kicks=kicks)
        for name in (R.PICK,) + tuple(n for n in R.LINES if n != R.PICK))
    pre, why = iround(VS.LEAD, SR), 'tools.sound.recipes.fut03.voxsynth.riff_tail'
    return lines + (
        Spec('FUT03', 'vox', 'vox', 5, (DERIVED[0], None), '', 64,
             tuple((s + C5.VOX_AT, m, ()) for s, m, _, _ in VS.VOX_RIFF), pre, 'vox', why,
             gain_mdb=-1000 * STEM_GAIN_DB, derived=True, frames=C5.G.at(C5.VOX_AT) + riff_frames(), looped=False,
             throw='baked'),
        Spec('FUT03', 'vox_twice', 'vox', 5, (DERIVED[1], None), '', 64,
             tuple(sorted((s + at, m, ()) for at in TWICE for s, m, _, _ in VS.VOX_RIFF)), pre, 'vox', why,
             gain_mdb=-1000 * TWICE_GAIN_DB, derived=True, frames=C5.G.at(max(TWICE)) + riff_frames(), looped=False,
             throw='baked'),
    )
