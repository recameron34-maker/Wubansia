"""FUT04 in the plugin (ADR 0020): what tools/sound/plugin_kit.py and phrase_tables.py read off this kit's recipes to
write content/plugin/fut04/FUT04_kit.json. Shared code never imports a kit (ADR 0008), so the kit's facts live here:
its phrase lanes and derived stems, the rules its instruments play by (each with the recipe it comes from), its lane
labels and the mix channels its sounds play on (P3's, in FUT04_mix.json)."""
import numpy as np

from tools.sound import phrase as Ph
from tools.sound.kits import fut04 as K
from tools.sound.phrase_tables import Spec, cycle_frames, iround, loops_path, words
from tools.sound.recipes.fut04 import accents as Ac, bass as B, context as C, context5 as C5, lead as Ld, midi as Md
from tools.sound.recipes.fut04 import pad as Pd
from tools.sound.recipes.fut04 import voxsynth as V

LANE_LABELS = ('Kick', 'Snare', 'Hats', 'Anvil', 'Acid', 'Mod Blip', 'Drone', 'Vox Synth', 'Accents', 'FX')
KICK_AB = None
MIX_CHANNELS = {'kick': 'kick', 'rumble': 'kick', 'bass': 'bass', 'bass_stem': 'bass', 'snare': 'snare', 'hat': 'hats',
                'ride': 'hats', 'perc': 'perc', 'fill': 'fx', 'pad': 'chords', 'lead': 'lead', 'vox': 'vox',
                'accent': 'accents'}
MIX = dict(drums={'KickSplit': 'kick', 'Rumble': 'rumble', 'SnareGated': 'snare', 'ClosedHat': 'hat',
                  'OpenHat': 'hat', 'Ride': 'ride', 'Anvil': 'perc'},
           channels={1: 'bass', 2: 'lead', 4: 'pad', 5: 'vox', 11: 'accent', 12: 'pad', 13: 'accent'},
           accents={None: 'accent'}, fx='fill', chops='vox')
EXTRAS = {'Acid Stab': 'melodic', 'Metal Chord': 'melodic', 'Rave Stab': 'melodic'}

INTERP = 'sinc'
GLIDE = ('raised_cosine', iround(B.SLIDE_S, 1e6), 0)   # bass.line_parts: a raised cosine in log frequency, 50 ms
BASS_ENV = (1000, 30000)                   # no recipe plays the Acid multisample: the bass voice's (FUT01)
LEAD_RELEASE = 30000                       # lead.voice: each blip's gate tapers over 30 ms
CHORD_LAW = 'layer_only'
VOX = (iround(V.RELEASE_GRID_S, 1e6), 'layer_only', 0)   # voxsynth.riff_hit: gated plus 30 ms; the velocity picks
CHOKE = (iround(C.drums.CHOKE['fade_s'], 1e6), 'cos2')  # the vowel; no lead. drums.CHOKE: 4 ms cos2
LAYERS = {}
# Chords as chords (ADR 0027, amended): each chord sample's chord, {Item: {zone root: its keys}} (plugin_kit writes
# zones[].voicing). pad.CHORD: the drone's Em(add9); accents.EM: the Metal Chord's and the Rave Stab's Em, open.
VOICINGS = {'DroneEmAdd9': {Pd.CHORD[0]: Pd.CHORD}, 'AccentMetalChordEm': {Ac.EM[0]: Ac.EM},
            'AccentRaveStabEm': {Ac.EM[0]: Ac.EM}}
DERIVED = tuple(f'Stems/Acid Line {n} {c} 4 Bars.wav' for n in (1, 2) for c in ('Clean', 'Warm'))


def chop_preroll(item):
    return 0                                # the vox stabs start on their note


def accent_gain_mdb(item, audio=None):
    """Each accent at its own loudness on the shared accent lane (context5.ACCENT_LUFS, over its file): None until the
    file's audio is given."""
    if item not in C5.ACCENT_LUFS:
        return 0
    if audio is None:
        return None
    from tools.sound import qa
    return iround(C5.ACCENT_LUFS[item] - qa.lufs(audio), 1000)


# The multisample path (a phrase channel's notes off the kit's tempo, with phrases off, and from MIDI in) at its stem's
# level: each multisample's own gain (mdB), measured with groovecore render, phrases off against phrases auto, pass 2 of
# the loop (content/plugin/README.md). The acid basses play on bass (which only they play), where P3's corrective EQ
# comes first: the line pedals and ducks its sub under the kicks (bass.play_line), the multisample sustains it, 13-18 dB
# more at 60-125 Hz.
MS_GAIN_MDB = {'AcidClean': 0, 'AcidWarm': 0, 'ModBlip': -470}


def play(kind, item, it, key, r):
    """FUT04's own playing rules, over plugin_kit's defaults."""
    if kind in ('bass', 'lead', 'vox') and item in MS_GAIN_MDB:   # the multisample at its stem's level
        r['gain_mdb'] = MS_GAIN_MDB[item]
    if kind == 'drum' and item == 'Rumble':     # context.rumble_lane: 2 bars looped, muted over a 5 ms raised cosine
        r.update(mode='sustain_loop', release_us=iround(C.MUTE_EDGE, 1e6), restrike_continues=True, duck_key=False)
    return r


def render_derived():
    """Each acid line at rest, its 2-bar Loops/ file as render.py writes it (phrase.kit_files), twice: the Ride's 4-bar
    cycle."""
    out = {}
    for path in DERIVED:
        n, char = path.split('Acid Line ')[1].split(' ')[:2]
        x = Ph.kit_files(K, f'AcidLine{n}{char}')[1][(None, 1, 1)].audio
        out[path] = np.concatenate([x, x])
    return out


def phrases():
    """The bass (Acid Line 1 Ride, the loop's, with its ride; Acid Lines 1 and 2 at rest) and the lead (the blip
    sequence, its ratchets held)."""
    cyc = B.RIDE_PASSES * B.LOOP_STEPS          # the ride's 4 bars: every acid phrase on channel 1 is one cycle long

    def notes(name):
        line = B.LINE_BY_NAME[name]
        ev = sorted([(s, n) for s, n, *_ in line.notes] + [(s, n) for s, n, *_ in line.ticks])
        return tuple((s + B.LOOP_STEPS * p, n, ()) for p in range(B.RIDE_PASSES) for s, n in ev)

    rest = ((0,) * (cyc * 4), (0,) * (cyc * 4))   # midi.acid_rides: 0 and 0 at rest, where the samples play as they are
    acid = dict(kicks=tuple(range(0, cyc, 4)))
    why = 'tools.sound.recipes.fut04.bass.play_line'
    lines = (Spec('FUT04', 'acid', 'bass', 1, (loops_path(f'{B.PICK}RideClean'), loops_path(f'{B.PICK}RideWarm')),
                  'bass', cyc, notes(B.PICK), 0, 'bass_stem', why, ride=tuple(map(tuple, Md.RIDES[B.PICK])), **acid),)
    for i, ln in enumerate(B.LINES, 1):         # each line at rest, its 2-bar Loops/ file twice (Stems/)
        stems = tuple(f'Stems/{words(ln.name)} {c} 4 Bars.wav' for c in ('Clean', 'Warm'))
        lines += (Spec('FUT04', f'line{i}', 'bass', 1, stems, 'bass', cyc, notes(ln.name), 0, 'bass_stem', why,
                       ride=rest, derived=True, frames=cycle_frames(K, cyc), **acid),)
    return lines + (
        Spec('FUT04', 'blip', 'lead', 2, (loops_path('ModBlipPhrase'), None), '', Ld.PHRASE_STEPS,
             tuple((s, n, (n,) * (r - 1)) for s, n, _, _, r in Ld.PHRASE), 0, 'lead',
             'tools.sound.recipes.fut04.lead.blip_phrase'),
    )
