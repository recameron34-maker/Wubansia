"""FUT04 "Furnace Code" (a working name: title-check it): dark industrial / techno, 136 BPM. A stub for its designer.

Fill every TODO below, then add the kit's recipes to tools/sound/recipes/fut04/ (copy
tools/sound/recipes/fut01/example.py to start). Until no field is TODO, render.py refuses the kit and names the fields.
The brief is docs/plans/S1-four-kits.md: four on the floor, 16th acid, rumble; the DNA is "Iron": heavy plate and anvil
modal ratios, a dark concrete space, and a low male voice blend.
"""
from tools.sound import dsp48 as D
from tools.sound.kits import TODO, scale_of

KIT = 'FUT04'
TITLE = 'Furnace Code'
DIRECTION = 'dark industrial / techno'
BPM = 136.0
KEY = TODO                   # the plan suggests E minor
CAMELOT = TODO               # the key's Camelot code: kits.camelot(KEY) (9A for E minor)
SCALE = scale_of(KEY)        # follows the key
ROOT = TODO                  # the key's root in the bass register (MIDI, C4 = 60)
KICK_NOTE = TODO             # the kick's and the sub's note, in the key, MIDI 24-40

RATIOS = TODO                # the kit's inharmonic ratio set: 1.0 first, ascending, its own (no other kit's)
VOICE = TODO                 # the one Kokoro voice blend: ((voice, weight), ...), weights summing to 1
SPACE_SEED = 'FUT04/space'    # seeds every room and FDN in the kit
SWING = TODO                 # the one swing of every written lane (0.5 straight); the plan suggests about 0.54
GROOVE = '4x4'               # QA loops the kicks and bass shots four on the floor (qa.GROOVES)
RECIPES = 'tools.sound.recipes.fut04'

BEAT = 60.0 / BPM            # seconds
STEP = BEAT / 4              # one 16th


def space_rng(*extra):
    """The kit's space: pass it to fdn_reverb so every sound sits in the same room."""
    return D.rng(SPACE_SEED, *extra)
