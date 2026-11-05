"""FUT03 "Glass Current": garage-house and rolling deep tech house, 128 BPM, G minor (Camelot 6A).

The kit DNA every recipe draws on (tools/sound/kits/__init__.py lists the fields). Its recipes are the package
tools/sound/recipes/fut03/. Rounds 1-3 were hyper-clean future house (docs/plans/S1-four-kits.md). Round 4 moves the
kit to warm, organic garage-house and rolling deep tech house: a deep round kick, a clap and snare in a short room,
shuffled hats and shakers, congas and rims with swung ghosts, a rubbery rolling bassline over a deep sub, dubby minor 9
chords on an electric piano with delay and reverb tails, a warm pad bed and a filtered, chopped vocal hook. The glass
bell (RATIOS) stays only as a subtle colour in the perc and the accents.

The name was title-checked on 2026-10-29 as a song, an album, lyrics and a label: no release uses it. The nearest hit,
"Glass Currents", is an art exhibition.
"""
from tools.sound import dsp48 as D
from tools.sound.kits import scale_of

KIT = 'FUT03'
TITLE = 'Glass Current'
DIRECTION = 'garage-house / deep tech house'
BPM = 128.0
KEY = 'G minor'
CAMELOT = '6A'
SCALE = scale_of(KEY)        # G natural minor pitch classes: G A Bb C D Eb F
ROOT = 43                    # G2, 98.0 Hz (MIDI, C4 = 60): the key's root in the bass register; the bassline jumps
                             # between G1 and G2, the top of the bass-body range (ADR 0011)
KICK_NOTE = 31               # G1, 49.0 Hz: the kick and every sub. On the tonic, the kick's short sub and the offbeat
                             # bass's root are the same note (or an octave), so a tail that overlaps never beats

# the kit DNA (plan section 2.1): a tuned glass bell. Its four lowest partials are tuned like a minor-third bell's
# strike partials (prime, tierce, quint, nominal), in equal temperament so they sit with the kit's synths: a minor
# triad and its octave, so a pling struck on G, C or D rings a chord that is in G minor. Above the nominal, the partials
# are in-key intervals (the minor tenth, the eleventh, the double octave) stretched sharp the way a stiff glass shell's
# upper modes run: r * sqrt((1 + B r^2) / (1 + 4B)), B = 0.004, so +5.6, +10.6 and +40 cents. The clean triad carries
# the pitch; the stretched top gives the glass shimmer and keeps it from reading as an organ chord. No hum partial
# (0.5): it would put an octave under every sound, where this kit stays clean.
RATIOS = (1.0, 1.1892, 1.4983, 2.0, 2.3862, 2.6861, 4.0934)
VOICE = (('af_heart', 0.6), ('af_jessica', 0.4))    # every Kokoro take, the sung chops and the vocoder phrase: a
                                                    # bright female blend. af_heart, the pool's clearest voice, leads;
                                                    # af_jessica adds forward 2-5 kHz presence
SPACE_SEED = 'FUT03/space'    # seeds every room and FDN in the kit
SWING = 0.56                 # the one swing of every written lane: odd 16ths land 0.12 of a 16th late (14.1 ms at
                             # 128 BPM; rounds 1-3: 0.53). Round 4's garage-house shuffle: the 16th hats, the shakers,
                             # the congas' ghosts and the bass's notes on the "e" and the "a" roll, while everything on
                             # the 8ths stays on the grid
GROOVE = '4x4'               # QA loops the kicks and bass shots four on the floor (qa.GROOVES)
RECIPES = 'tools.sound.recipes.fut03'

BEAT = 60.0 / BPM            # seconds
STEP = BEAT / 4              # one 16th


def space_rng(*extra):
    """The kit's space: pass it to fdn_reverb so every sound sits in the same room."""
    return D.rng(SPACE_SEED, *extra)
