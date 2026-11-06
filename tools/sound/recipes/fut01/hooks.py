"""FUT01 round 4 vocal hooks (set r4): R&B-style chopped hooks cut only from the kit's own, already title-checked line
"Come find me on the floor" (vox.py: SungLine1 and its chops). No new words.

Every hook is a chop of the pick re-pitched in the WORLD frame domain (vox.repitched: the f0 track times a ratio, the
spectral envelope stretched by a formant ratio), then polished with the whole line's levels like every chop: a WORLD
resynthesis at 48 kHz, never a resample (ADR 0005, D4).
- Formant-kept (formant ratio 1): the voice sings higher and keeps its own vowels and size. A fourth up (+5).
- Chipmunk (the formants moved with the pitch): the classic sped-up garage vocal. A fifth (+7) or an octave (+12) up.
- The call: the whole line a fourth up, formants kept (A4 to E5), as a call to the floor.
- Delay throws: a hook through a dotted-8th tape ping-pong and the garage plate, its tail rendered out (wet).
- ChopRiff: a 2-bar loop of the hooks on the off-beats, on the kit's grid and swing, as a vocal riff.

Every pitch stays in F# minor. Single words need no title check (as for the round 2 chops); the multi-word pieces are
the ones vox.CHOP_CHECKS kept ("find me on the floor", "come find me on the floor"), and no name spells a cut title.

Rendering needs the vocal models (WUBANSIA_MODELS_DIR); importing this module does not.
"""
import functools

import numpy as np

from tools.sound import dsp48 as D
from tools.sound import phrase as Ph
from tools.sound import sends as S
from tools.sound import vocals as V
from tools.sound.item import Item, Out
from tools.sound.kits import fut01 as K
from tools.sound.recipes.fut01 import vox

SR = D.SR
CHIP = lambda semis: 2.0 ** (semis / 12.0)    # chipmunk: the formants move with the pitch
# (name, the chop it re-pitches, semitones, formant ratio, about)
HOOKS = (
    ('ChopMeD5', 'ChopMe', 5, 1.0, '"me" a fourth up, D5, formants kept, held a dotted 8th'),
    ('ChopFloorE5', 'ChopFloor', 5, 1.0, '"floor" a fourth up, E5 falling to D5, formants kept'),
    ('ChopFindD5', 'ChopFind', 8, 1.0, '"find" a minor sixth up, D5, formants kept'),
    ('ChopMeA5Chip', 'ChopMe', 12, CHIP(12), '"me" an octave up, A5, chipmunk (formants an octave up with it)'),
    ('ChopComeB4Chip', 'ChopCome', 7, CHIP(7), '"come" a fifth up, B4, chipmunk'),
    ('ChopFloorFs5Chip', 'ChopFloor', 7, CHIP(7), '"floor" a fifth up, F#5 falling to E5, chipmunk'),
    ('CallLineA4', 'ChopWholeLine', 5, 1.0, 'the call: the whole line a fourth up, A4 to E5, formants kept'),
)
HOOK_BY_NAME = {h[0]: h for h in HOOKS}
THROWS = (('ThrowFloorE5', 'ChopFloorE5', '"floor" a fourth up thrown into a dotted-8th ping-pong and the plate'),
          ('ThrowMeA5Chip', 'ChopMeA5Chip', '"me" an octave up, chipmunk, thrown into a dotted-8th ping-pong and the '
                                            'plate'))
THROW_ECHO = dict(S.ECHO, feedback=0.48, hp_hz=500.0, lp_hz=4200.0)
THROW_PLATE = dict(S.SPACE, t60=1.3, size=0.5, damp_hz=9000.0, predelay=round(K.STEP / 4, 6), send_hp=500.0,
                   t60_low=0.4, shimmer=0.03)
THROW_MIX = (1.0, 0.75, 0.30)       # dry, echo return, plate return
THROW_TAIL_S = 3.0


def hook_cut(name):
    """A hook's frames: (track, junction seconds), the junction where its breath tail starts."""
    _, chop, semis, formant, _ = HOOK_BY_NAME[name]
    tr, junction = vox.pick_cut(chop)
    return vox.repitched(tr, semis, formant), junction


def hook_onset(name):
    """Seconds from the start of a hook's file to its note (where the beat falls)."""
    return float(hook_cut(name)[0].events[0]['t_on'])


@functools.lru_cache(maxsize=None)
def hook_audio(name):
    tr, junction = hook_cut(name)
    y = V.polish(V.synth(tr), hp=vox.HP, levels=vox.line_sung(vox.PICK)[1])
    return y, int(np.floor(junction * SR + 0.5))


def hook(take):
    y, junction = hook_audio(take.item.name)
    return Out(y, splices=(junction,))


def throw(take):
    """A hook, dry, plus its throw: a dotted-8th tape ping-pong (sends.echo, thinner and brighter than the loop's) and
    the garage plate, both run out over THROW_TAIL_S; mono under 150 Hz."""
    _, source, _ = THROW_BY_NAME[take.item.name]
    y, junction = hook_audio(source)
    dry = D.st(y)
    n = len(dry) + int(THROW_TAIL_S * SR)
    delay = S.ECHO_STEPS * K.STEP
    echo = S.echo(D.mono(dry), K.space_rng('throw echo'), delay, n_tail=n - len(dry), **THROW_ECHO)
    plate = S.space(D.mono(dry), K.space_rng('throw plate'), n_tail=n - len(dry), **THROW_PLATE)[:n]
    out = np.zeros((n, 2))
    g_dry, g_echo, g_plate = THROW_MIX
    out[:len(dry)] += g_dry * dry
    out += g_echo * echo[:n] + g_plate * plate
    out = D.mono_below(D.bfilt(out, vox.HP, 'highpass', order=4), 150.0)
    return Out(out, splices=(junction,))


THROW_BY_NAME = {t[0]: t for t in THROWS}

# ------------------------------------------------------------------ the riff: hooks on the off-beats
RIFF_STEPS = 32
# (16th the hook's note lands on, hook, level dB): bar 1 answers on the off-beats, bar 2 lifts to the chipmunk
RIFF = ((2, 'ChopMeD5', 0.0), (7, 'ChopMeD5', -3.0), (10, 'ChopFloorE5', 0.0),
        (18, 'ChopComeB4Chip', -1.0), (22, 'ChopMeA5Chip', -2.0), (26, 'ChopFloorFs5Chip', -1.0),
        (30, 'ChopMeD5', -4.0))


def chop_riff(take):
    """The riff as a loop: each hook's file (as written) on its 16th, its note on the grid (the file starts its onset
    consonant earlier), at its level; tails wrap."""
    G = Ph.Grid(K, RIFF_STEPS)
    buf = np.zeros((G.length, 2))
    for step, name, level in RIFF:
        _, got, _ = Ph.kit_files(K, name)
        Ph.place(buf, got[(None, 1, 1)].audio * D.undb(level), G.at(step) - int(np.floor(hook_onset(name) * SR + 0.5)))
    return Out(buf, meta={'steps': RIFF_STEPS, 'bpm': K.BPM, 'swing': K.SWING,
                          'hooks': [{'step': s, 'hook': n, 'level_db': lv} for s, n, lv in RIFF]})


R4 = ('r4',)
VOX = frozenset({'hf_synthetic'})
WHY = vox.WHY
WORDS = {name: dict((c[0], c[3]) for c in vox.PICK_CHOPS)[chop] for name, chop, *_ in HOOKS}
ROOTS = {name: dict((c[0], c[4]) for c in vox.PICK_CHOPS)[chop] + semis for name, chop, semis, *_ in HOOKS}
ITEMS = [
    Item('Vox', name, 'vox', hook, root=ROOTS[name], level=-18.0, flags=VOX, qa=WHY, tags=('clean', 'chop', 'sung'),
         sets=R4, script=WORDS[name],
         about=f'hook {about}, re-pitched in WORLD from {chop} of "{vox.LINE_BY_NAME[vox.PICK].text}"')
    for name, chop, semis, formant, about in HOOKS
] + [
    Item('Vox', name, 'vox', throw, root=ROOTS[source], level=-16.0, flags=VOX, qa=WHY,
         tags=('clean', 'wet', 'chop', 'sung'), sets=R4, script=WORDS[source], about=f'throw: {about}')
    for name, source, about in THROWS
] + [
    Item('Loops', 'ChopRiff', 'loop', chop_riff, loop=True, level=-18.0, flags=VOX, sets=R4, tags=('clean', 'chop'),
         script=' '.join(WORDS[n] for _, n, _ in RIFF),
         qa={'mud': 'lowcut', 'why': 'a vocal loop: the low cut, like the chops'},
         about='2-bar vocal riff: the hooks on the off-beats (me, me, floor; come, me, floor, me), formants kept in '
               'bar '
               '1, chipmunk in bar 2, 134 BPM, F# minor'),
]
