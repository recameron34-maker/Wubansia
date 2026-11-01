"""FUT01 "Alloy Signal": future garage / 2-step with sci-fi colour, 134 BPM, F# minor (Camelot 11A).

The kit DNA every recipe draws on (tools/sound/kits/__init__.py lists the fields). Its recipes are the package
tools/sound/recipes/fut01/: each module there exposes ITEMS = [Item(...), ...], and kits.items() collects them in
module-name order, so designers add sounds in their own files and never edit the same one.
"""
from tools.sound import dsp48 as D

KIT = 'FUT01'
TITLE = 'Alloy Signal'
DIRECTION = 'future garage / 2-step'      # with sci-fi colour: the metallic FM drums, the reese, glitch and robot
BPM = 134.0
KEY = 'F# minor'
CAMELOT = '11A'
SCALE = (6, 8, 9, 11, 1, 2, 4)  # F# natural minor pitch classes: F# G# A B C# D E
ROOT = 42                    # F#2 (MIDI, C4 = 60): the key's root in the bass register
KICK_NOTE = 30               # F#1, 46.25 Hz: the kick and the sub

# the kit DNA (plan section 2.1)
ALLOY = (1.0, 1.483, 1.932, 2.546, 3.011, 3.715, 4.468)  # inharmonic ratios: perc, stabs, impacts, the resonator
RATIOS = ALLOY
VOICE = (('bf_emma', 0.6), ('af_nicole', 0.4))           # the one Kokoro voice blend
SPACE_SEED = 'FUT01/space'                                # seeds every room and FDN in the kit
SWING = 0.57                 # the groove's base swing for the kit's written phrases: where the second 16th of each 8th
                             # lands, as a share of the 8th (0.5 straight; 15.7 ms late at 134 BPM). One value for every
                             # lane, so hits that share a step land together (plan section 2.3)
GROOVE = '2step'             # QA loops the kicks and bass shots on qa.TWO_STEP
RECIPES = 'tools.sound.recipes.fut01'

BEAT = 60.0 / BPM            # seconds
STEP = BEAT / 4              # one 16th


def space_rng(*extra):
    """The kit's space: pass it to fdn_reverb so every sound sits in the same room."""
    return D.rng(SPACE_SEED, *extra)
