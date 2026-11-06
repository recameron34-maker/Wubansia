"""FUT04 "Furnace Code": dark industrial / techno, 136 BPM, E minor (Camelot 9A).

The kit DNA every recipe draws on (tools/sound/kits/__init__.py lists the fields). Its recipes are the package
tools/sound/recipes/fut04/. The brief is docs/plans/S1-four-kits.md: four on the floor, 16th acid, rumble. The DNA is
"Iron": heavy plate and anvil modal ratios (RATIOS), a dark concrete space (SPACE_SEED), and a low male voice blend.

The name was title-checked on 2026-10-29 as a song, an album, lyrics and a label: no release uses it. The nearest hits
are other things called "Furnace" alone (albums, a tracker program) and furnace error codes.
"""
from tools.sound import dsp48 as D
from tools.sound.kits import scale_of

KIT = 'FUT04'
TITLE = 'Furnace Code'
DIRECTION = 'dark industrial / techno'
BPM = 136.0
KEY = 'E minor'
CAMELOT = '9A'
SCALE = scale_of(KEY)        # E natural minor pitch classes: E F# G A B C D
ROOT = 40                    # E2, 82.41 Hz (MIDI, C4 = 60): the key's root in the bass register. The acid lines live in
                             # E1-G2, inside the bass-body range (fundamentals up to G2, 98 Hz; ADR 0011)
KICK_NOTE = 28               # E1, 41.20 Hz: the kick, its sub and the rumble. On the tonic, an octave under ROOT, so
                             # the kick's tail, the rumble and the bass's low E are one note and never beat. It is the
                             # deepest of the four kits' kicks, for the heavy industrial low end: 441 ms between
                             # kicks at 136 BPM leaves the sub room to bloom and fall, and the short knock above
                             # 110 Hz carries the hit on small speakers

# the kit DNA (plan section 2.1). "Iron": an anvil cut from heavy steel plate, a 0.75 x 0.216 x 0.12 m slab of about
# 150 kg (steel: E 200 GPa, Poisson 0.29, 7850 kg/m3) resting lightly on its stand, so free at both ends, struck. Its
# seven lowest modes, by a Timoshenko beam model (finite elements, free-free) bending across its width and across its
# height, plus its Saint-Venant twist; a model, not a measurement, rounded to 4 decimals:
#   1.0     bending across the width, 1st          (about 1.02 kHz on the slab itself)
#   1.5719  bending across the height, 1st   } a 44-cent pair near the minor sixth: the clank's warble, in key over E
#   1.6124  twist, 1st                       }
#   2.4446  bending across the width, 2nd          (a neutral third over the octave: the iron colour)
#   3.2247  twist, 2nd (the twist's octave)  } a second rough pair, 75 cents apart
#   3.3671  bending across the height, 2nd   }
#   4.2095  bending across the width, 3rd          (a minor second over the double octave: the dark top)
# A slab this thick (length 3.5 times its height) is heavy in the physical sense: shear and rotary inertia pull its
# upper modes far under a thin bar's (2.4446 and 4.2095 against 2.7565 and 5.4039), so it reads as mass, not as a
# glockenspiel or glass, and its two bending planes and its twist give it the doubled, beating partials of struck
# iron. Every ratio after 1.0 sits at least 70 cents from FUT01's alloy set and 88 cents from a harmonic (2 to 8);
# the second (1.5719) clears the 1.3 x f0 band that QA measures a pitched sound's tuning in.
RATIOS = (1.0, 1.5719, 1.6124, 2.4446, 3.2247, 3.3671, 4.2095)
VOICE = (('am_onyx', 0.6), ('am_fenrir', 0.4))   # the one Kokoro blend, sung chops and robot alike: am_onyx, the
                                                 # pool's deepest voice and its darkest male one (85 Hz median
                                                 # speaking F0), leads; am_fenrir (128 Hz) adds 6 dB more 2-5 kHz
                                                 # presence, to keep the words clear through the robot's distortion.
                                                 # The blend speaks at 100 Hz. US lexicon, the heavier voice's
SPACE_SEED = 'FUT04/space'    # seeds every room and FDN in the kit: the dark concrete space
SWING = 0.54                 # the one swing of every written lane: odd 16ths land 0.08 of a 16th late (8.8 ms at
                             # 136 BPM). Enough to lift the 16th acid and the hats off the grid, while the four on the
                             # floor (even 16ths) stays straight and hard
GROOVE = '4x4'               # QA loops the kicks and bass shots four on the floor (qa.GROOVES)
RECIPES = 'tools.sound.recipes.fut04'

BEAT = 60.0 / BPM            # seconds
STEP = BEAT / 4              # one 16th


def space_rng(*extra):
    """The kit's space: pass it to fdn_reverb so every sound sits in the same room."""
    return D.rng(SPACE_SEED, *extra)
