"""FUT01 vocals for the first listen: eight sung chops cut from one sung phrase, and a robot phrase on the 16th grid.

Both lines are original and clean, written for this kit:
- sung, in F# minor: "Copper light, moving slow" (A4 G#4 | F#4 | E4 F#4 | C#4 held), 2 bars at 134 BPM
- robot: "Boost the signal", one syllable per slice on steps 0, 3, 4 and 6 of a bar, monotone F#3

Every file is a WORLD resynthesis at 48 kHz from the features of a 24 kHz Kokoro take in the kit's voice blend (ADR
0005, D4: hf_synthetic). The chops are cut in the frame domain (vocals.chop): each keeps its onset consonant whole and
ends in a breath tail, so no cut lands in the audio. The full sung phrase is here too, dry, so Whisper can check the
line the chops come from. The robot's slice points are fixed by the grid (ROBOT_SLICES): the sampler fires slice k at
its step, REX-style, so the phrase stays on the grid at 128 or 140. Everything is high-passed at 100 Hz; the wet robot
is mono under 150 Hz.

Rendering needs requirements-vocals.txt and the models (WUBANSIA_MODELS_DIR); importing this module does not.
"""
import functools
from dataclasses import replace

import numpy as np

from tools.sound import dsp48 as D
from tools.sound import vocals as V
from tools.sound.item import Item, Out
from tools.sound.kits import fut01 as K

SR = D.SR
HP = 100.0                   # nothing under ~100 Hz competes with the kick and sub
VOX = frozenset({'hf_synthetic'})
WHY = {'why': 'WORLD resynthesis at 48 kHz of a 24 kHz Kokoro take: the top band is synthetic (extend_hf, D4)'}

# ------------------------------------------------------------------ the sung phrase and its chops
SUNG = 'Copper light, moving slow'
SUNG_NOTES = ((0, 2, 69, 'copper'), (2, 2, 68, '~'), (4, 6, 66, 'light'), (12, 3, 64, 'moving'), (15, 3, 66, '~'),
              (18, 10, 61, 'slow'))            # (16th step, length, MIDI, word); '~' carries the word on
SUNG_HOW = V.Sing(speed=0.8, consonants=1.3)
SUNG_ACCENT = 'us'           # US phonemes in the kit's voice: Whisper hears the British /kɒpə/ as "copa", 6 of 6 times
SUNG_DRAW = 2                # the take: of draws 0-5 the only one Whisper hears word for word padded, 10 dB down and
                             # unpolished alike (the others: "cover light", or "copperlight" as one word)
SUNG_PRE = 0.5               # seconds before step 0: the inhale and the first onset
SUNG_LEN = 30 * K.STEP + 0.4
# name, first and last syllable (one per note, so syllable i sings SUNG_NOTES[i]), the word it is cut from, about
CHOPS = (
    ('ChopCo', 0, 0, 'copper', '"co" of copper, A4'),
    ('ChopPer', 1, 1, 'copper', '"pper" of copper, G#4'),
    ('ChopCopper', 0, 1, 'copper', '"copper", A4 to G#4'),
    ('ChopLight', 2, 2, 'light', '"light", F#4, its t released into breath'),
    ('ChopMo', 3, 3, 'moving', '"mo" of moving, E4'),
    ('ChopVing', 4, 4, 'moving', '"ving" of moving, F#4'),
    ('ChopMoving', 3, 4, 'moving', '"moving", E4 to F#4'),
    ('ChopSlow', 5, 5, 'slow', '"slow", C#4 held with vibrato, a fall and a breath tail'),
)
CHOP_BY_NAME = {c[0]: c for c in CHOPS}


@functools.lru_cache(maxsize=1)
def sung_track():
    """The one sung performance every chop is cut from: its frames before synthesis, the phrase ends not yet
    released."""
    perf = V.performance(V.sing_text(SUNG_NOTES), K.VOICE, SUNG_HOW.speed, SUNG_HOW.glottal, SUNG_ACCENT, SUNG_DRAW)
    # straight (0.5), as rendered for S1a; swinging the line on K.SWING is a sound change for the fix round's own commit
    return V.sing_track(perf, SUNG_NOTES, K.BPM, SUNG_HOW, SUNG_LEN, SUNG_PRE, D.rng(K.KIT, 'SungPhrase', 'sing'),
                        swing=0.5)


@functools.lru_cache(maxsize=1)
def sung_line():
    """The whole line as sung (phrase ends released) with its inhale, before polish, and the levels polish works
    against on it."""
    tr = V.release_ends(replace(sung_track(), sp=sung_track().sp.copy()))
    y = V.breaths(V.synth(tr), tr.events, D.rng(K.KIT, 'SungPhrase', 'breath'))
    return y, V.polish_levels(y, hp=HP)


def sung_phrase(take):
    """The whole line, dry: the inhale, the phrase, polished."""
    y, levels = sung_line()
    return V.polish(y, hp=HP, levels=levels)


def sung_chop(take):
    """One chop: its syllables cut from the sung frames, a breath tail, polished exactly as inside the phrase (the
    phrase's levels, not the chop's own peak). The junction where the tail starts is logged as a splice."""
    _, first, last, _, _ = CHOP_BY_NAME[take.item.name]
    tr, junction = V.chop(sung_track(), first, last)
    return Out(V.polish(V.synth(tr), hp=HP, levels=sung_line()[1]), splices=(int(np.floor(junction * SR + 0.5)),))


# ------------------------------------------------------------------ the robot phrase
ROBOT = 'Boost the signal'
ROBOT_SYLLABLES = ('boost', 'the', 'sig', 'nal')
ROBOT_GRID = ((0, 3), (3, 1), (4, 2), (6, 3))  # (16th step, length) of each syllable, one bar
ROBOT_NOTE = K.ROOT + 12     # F#3, 185 Hz: monotone
ROBOT_LEAD = 0.06            # a slice starts this long before its step: room for the onset consonant
ROBOT_PRE = ROBOT_LEAD + 0.01
ROBOT_LEN = 9 * K.STEP + 0.3
ECHO_STEPS = 3               # dotted 8th


def slice_sample(step):
    return int(np.floor((ROBOT_PRE + step * K.STEP - ROBOT_LEAD) * SR + 0.5))


# (nominal 16th step, onset sample in the file, syllable): the slice table for index.json, the same in dry and wet
ROBOT_SLICES = tuple((step, slice_sample(step), syl) for (step, _), syl in zip(ROBOT_GRID, ROBOT_SYLLABLES))
SLICE_NOTE = 'slices (16th step @ onset sample): ' + ', '.join(f'{s}@{n} {w}' for s, n, w in ROBOT_SLICES)


@functools.lru_cache(maxsize=1)
def robot_voice():
    """The robot, before its level: Kokoro's take on the grid, WORLD on a flat buzzing F#3 with the formants 10% down,
    then the metal: a +9 Hz frequency-shifted copy beating against it and a ring modulator on alloy-ratio carriers from
    F#1, both mixed under the voice; then grit, 4x oversampled, which also keeps the peaks near the loudness."""
    perf = V.performance('boost the signal.', K.VOICE, 1.0, False)
    tr = V.robot_track(perf, ROBOT_GRID, K.BPM, float(D.midi_hz(ROBOT_NOTE)), ROBOT_LEN, ROBOT_PRE, ROBOT_LEAD,
                       buzz=0.05, formant=0.9)
    y = D.bfilt(V.synth(tr), HP, 'highpass', order=4)
    y = 0.65 * y + 0.35 * D.freq_shift(y, 9.0)
    y = D.ring_mod(y, float(D.midi_hz(K.KICK_NOTE)), K.ALLOY, mix=0.18, r=D.rng(K.KIT, 'RobotBoost', 'ring'))
    y = V.compress(V.deess(y), -18, 3.0, 0.004, 0.08)
    y = y / (np.max(np.abs(y)) + 1e-12)
    y = 0.4 * y + 0.6 * D.oversampled(lambda u: D.tanh_adaa(u, 3.0), y)
    return D.bfilt(y, HP, 'highpass', order=4)


def slices():
    return tuple(n for _, n, _ in ROBOT_SLICES)


SLICE_META = {'slices': [{'step': step, 'onset': n, 'syllable': syl} for step, n, syl in ROBOT_SLICES]}


def robot_dry(take):
    return Out(robot_voice(), splices=slices(), meta=SLICE_META)


def echo(x, taps=6, fb=0.45, shift_hz=7.0):
    """Ping-pong echo on a dotted 8th whose every repeat is frequency-shifted a little further up (a barber-pole echo),
    darker and thinner each time. Mono in, stereo out, the dry signal not included."""
    d = int(np.floor(ECHO_STEPS * K.STEP * SR + 0.5))
    out = np.zeros((len(x) + d * taps, 2))
    tap = x
    for k in range(1, taps + 1):
        tap = D.freq_shift(D.bfilt(D.bfilt(tap, 5000, 'lowpass'), 300, 'highpass'), shift_hz)
        tap = tap * (fb if k > 1 else 0.7)
        out[k * d:k * d + len(x), (k - 1) % 2] += tap
        out[k * d:k * d + len(x), k % 2] += 0.25 * tap
    return out


def robot_wet(take):
    """The dry robot in the kit's space (the FDN on the FUT01/space seed) with the barber-pole echo. The space and the
    echo are mono under 150 Hz."""
    y = robot_voice()
    n_tail = int(4.5 * SR)
    send = D.bfilt(y, 300, 'highpass')
    verb = D.fdn_reverb(send, K.space_rng(), t60=2.4, size=0.9, damp_hz=6000.0, n_tail=n_tail)
    dly = echo(y)
    out = np.zeros((len(y) + n_tail, 2))
    out[:len(y)] += D.st(y)
    out[:len(dly)] += 0.55 * dly
    out += 0.32 * verb[:len(out)]
    out = D.mono_below(D.bfilt(out, HP, 'highpass', order=4), 150.0)
    return Out(out, splices=slices(), meta=SLICE_META)


ITEMS = [
    Item('Vox', 'SungPhrase', 'vox', sung_phrase, level=-18.0, flags=VOX, qa=WHY, tags=('clean', 'dry', 'sung'),
         sets=('s1a',), script=SUNG, about=f'the sung line the chops come from, dry: "{SUNG}", F# minor'),
] + [
    Item('Vox', name, 'vox', sung_chop, root=SUNG_NOTES[first][2], level=-18.0, flags=VOX, qa=WHY,
         tags=('clean', 'chop', 'sung'), sets=('s1a',), script=word, about=f'sung chop {about}, cut from "{SUNG}"')
    for name, first, last, word, about in CHOPS
] + [
    Item('Vox', 'RobotBoost', 'vox', robot_dry, root=ROBOT_NOTE, level=-18.0, flags=VOX, qa=WHY,
         tags=('clean', 'dry', 'robot'),
         sets=('s1a',), script=ROBOT, about=f'robot phrase "{ROBOT}" on the 16th grid at 134 BPM, dry; {SLICE_NOTE}'),
    Item('Vox', 'RobotBoostWet', 'vox', robot_wet, root=ROBOT_NOTE, level=-16.0, flags=VOX, qa=WHY,
         tags=('clean', 'wet', 'robot'), sets=('s1a',), script=ROBOT,
         about=f'robot phrase "{ROBOT}", wet: the kit space and a barber-pole echo; {SLICE_NOTE}'),
]
