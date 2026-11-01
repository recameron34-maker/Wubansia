"""FUT02 "Orbit Relay": sci-fi club, 130 BPM, D minor (Camelot 7A). Four on the floor, a rolling offbeat bass.

The kit DNA every recipe draws on (tools/sound/kits/__init__.py lists the fields; the brief is
docs/plans/S1-four-kits.md). The DNA is "Orbital": near-harmonic, slightly stretched chrome-synth partials (RATIOS),
and a laser pitch-drop gesture shared by its perc, fill and riser (written in the recipes). Its recipes are the
package tools/sound/recipes/fut02/: each module there exposes ITEMS = [Item(...), ...].

The working name was title-checked on 2026-10-29 (the exact phrase as a song, album, label and lyric): no release
uses it.
"""
from tools.sound import dsp48 as D
from tools.sound.kits import scale_of

KIT = 'FUT02'
TITLE = 'Orbit Relay'
DIRECTION = 'sci-fi club'
BPM = 130.0
KEY = 'D minor'
CAMELOT = '7A'
SCALE = scale_of(KEY)        # (2, 4, 5, 7, 9, 10, 0): D E F G A Bb C
ROOT = 38                    # D2, 73.42 Hz (MIDI, C4 = 60): the rolling bass's home note. Its lines stay in D1-G2, the
                             # bass-body range: the octave down (D1), the fifth below (A1), the minor seventh (C2)
KICK_NOTE = 33               # A1, 55.0 Hz: the key's fifth, a fourth under the bass's D2 and a fifth over D1. A punchy
                             # club sub, a little higher than FUT01's F#1 (46.25 Hz), so it settles well inside the
                             # 461 ms beat and leaves D1 and D2 to the bass

# the kit DNA (plan section 2.1): the partials of a stiff steel wire, f_n = n * sqrt((1 + B n^2) / (1 + B)) for
# n = 1..7, with B = 7.645e-4, the stiffness that puts partial 7 on the equal-tempered minor seventh two octaves up
# (2^(34/12) = 7.1272: the key's C over D). Near-harmonic, so every sound built on it stays pitched in the key;
# stretched +2 to +31 cents (partials 2-7), so its tones shimmer like chrome and beat slowly against any plain harmonic
# layer. Unlike FUT01's alloy (inharmonic struck metal). Tonal items declare every partial in Out.f0s: from partial 4
# on they sit outside the alias check's 0.5% window around the harmonics
RATIOS = (1.0, 2.0023, 3.0092, 4.0229, 5.0456, 6.0797, 7.1272)
VOICE = (('af_nova', 0.6), ('af_sky', 0.4))            # the sung chops: af_nova's clear body, af_sky's air on top
ROBOT_VOICE = (('am_puck', 0.65), ('am_echo', 0.35))   # the male vocoder phrase (brief item 10), from the same pool
SPACE_SEED = 'FUT02/space'   # seeds every room and FDN in the kit
SWING = 0.52                 # the one swing of every written lane: odd 16ths land 0.04 of a 16th late (4.6 ms at
                             # 130 BPM). Felt more than heard: the rolling 16th bass keeps its machine drive. The
                             # straightest of the four kits (FUT01 0.57)
GROOVE = '4x4'               # QA loops the kicks and bass shots four on the floor (qa.GROOVES)
RECIPES = 'tools.sound.recipes.fut02'

BEAT = 60.0 / BPM            # seconds
STEP = BEAT / 4              # one 16th


def space_rng(*extra):
    """The kit's space: pass it to fdn_reverb so every sound sits in the same room."""
    return D.rng(SPACE_SEED, *extra)
