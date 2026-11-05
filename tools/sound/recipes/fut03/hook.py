"""FUT03 round 4 vocals: a filtered, chopped vocal hook and three ear-candy chops, every one cut from the kit's existing
line "Ride the current with me" (vox.py's pick, title-checked on 2026-10-29): no new words.

Each chop is vox.py's own frame-domain cut (vocals.chop) of the pick's sung track, resynthesised with WORLD at 48 kHz
(ADR 0005, D4), so every file here is a WORLD resynthesis like the kit's other vocals.

- VoxHook: 2 bars on the kit's swing, the line chopped into a hook: "ride", a stutter "ri-" on the swung "e", "the
  current", "me"; then "ride", "ri-" and "with me". Each chop is gated to its length in 16ths (a 25 ms fade), like a
  sampler playing a slice; the whole hook through a band-pass, the deep-house filtered vocal: a 2nd-order high-pass at
  320 Hz and a resonant low-pass that opens from 2 to 5.5 kHz over the 2 bars and closes again.
- CandyRideFifth: "ride" a fifth up (F5), WORLD keeping its formants, with a short 8th-note ping-pong echo baked in.
- CandyMeReverse: "me" through a short room and reversed: a breathy swell that ends on the downbeat (cut_end).
- CandyCurStutter: "cur-" of "current" retriggered on six 32nds, each softer and darker: a stutter into a phrase.
"""
import functools
from dataclasses import replace

import numpy as np

from tools.sound import dsp48 as D
from tools.sound import phrase as Ph
from tools.sound import vocals as V
from tools.sound.item import Item, Out
from tools.sound.kits import fut03 as K
from tools.sound.recipes.fut03 import vox

SR = D.SR
SETS = ('r4',)
VOX = frozenset({'hf_synthetic'})
WHY = vox.WHY
HOOK_STEPS = 32
# (16th, chop of the pick, length in 16ths): the hook's chops, every word from "Ride the current with me"
HOOK = ((2, 'ChopRide', 2.5), (5, 'ChopRide', 1.2), (8, 'ChopTheCurrent', 4.5), (14, 'ChopMe', 1.8),
        (18, 'ChopRide', 2.5), (21, 'ChopRide', 1.2), (24, 'ChopWithMe', 6.5))
HOOK_WORDS = 'ride ride the current me ride ride with me'
GATE_FADE = 0.025
HOOK_HP = 320.0
HOOK_LP = (2000.0, 5500.0)          # the low-pass opens and closes over the 2 bars
HOOK_Q = 1.4


@functools.lru_cache(maxsize=None)
def chop_audio(name, semitones=0.0):
    """One chop of the pick (vox.CHOPS), resynthesised (optionally re-pitched in WORLD, its formants kept) and polished
    with the whole line's levels, and the seconds from its start to its note."""
    tr, _ = vox.chop_cut(name)
    if semitones:
        tr = replace(tr, f0=tr.f0 * 2 ** (semitones / 12))
    y = V.polish(V.synth(tr), hp=vox.HP, levels=vox.line_sung(vox.PICK)[1])
    return y, float(tr.events[0]['t_on'])


def gated(y, onset, length_s):
    """A chop as a sampler slice plays it: from its start to `length_s` after its note, then a GATE_FADE fade."""
    n = min(len(y), int((onset + length_s) * SR))
    m = int(GATE_FADE * SR)
    out = np.array(y[:n + m]) if n + m <= len(y) else np.concatenate([y, np.zeros(n + m - len(y))])
    out[n:n + m] *= 0.5 + 0.5 * np.cos(np.pi * np.arange(m) / m)
    return out


def band(y, L):
    """The hook's filter, round the loop: the high-pass, then the resonant low-pass on its moving cutoff."""
    u = np.arange(L) / L
    fc = HOOK_LP[0] * (HOOK_LP[1] / HOOK_LP[0]) ** (0.5 - 0.5 * np.cos(2 * np.pi * u))
    hp = Ph.circular(lambda w: D.bfilt(w, HOOK_HP, 'highpass', order=2), y)
    return D.svf_tpt(np.tile(hp, 3), np.tile(fc, 3), q=HOOK_Q)[0][L:2 * L]


def vox_hook(take):
    G = Ph.Grid(K, HOOK_STEPS)
    L = G.length
    buf = np.zeros(L)
    for step, name, length in HOOK:
        y, onset = chop_audio(name)
        x = gated(y, onset, length * G.s16 / SR)
        pos = G.at(step) - int(np.floor(onset * SR + 0.5))
        np.add.at(buf, (pos + np.arange(len(x))) % L, x)
    return Out(band(buf, L), meta={'steps': HOOK_STEPS, 'bpm': K.BPM, 'swing': K.SWING, 'from': vox.LINE_BY_NAME[
        vox.PICK].text, 'chops': [{'step': s, 'chop': c, 'len16': ln} for s, c, ln in HOOK],
        'filter': {'hp_hz': HOOK_HP, 'lp_hz': list(HOOK_LP), 'q': HOOK_Q}})


def candy_ride_fifth(take):
    """ "ride" a fifth up, with a ping-pong echo on the 8th: three repeats, each darker, alternating sides."""
    y, onset = chop_audio('ChopRide', 7.0)
    d = int(np.floor(2 * K.STEP * SR + 0.5))
    out = np.zeros((len(y) + 3 * d, 2))
    out[:len(y)] += D.st(y)
    tap = y
    for k in range(1, 4):
        tap = D.bfilt(D.bfilt(tap, 5000.0, 'lowpass'), 500.0, 'highpass') * (0.45 if k == 1 else 0.5)
        side = np.array([1.0, 0.3]) if k % 2 else np.array([0.3, 1.0])
        out[k * d:k * d + len(y)] += tap[:, None] * side
    return Out(out, meta={'from': 'ChopRide', 'semitones': 7, 'echo_steps': 2})


def candy_me_reverse(take):
    """ "me" through a short room, then the whole reversed: a breathy swell into the downbeat."""
    y, _ = chop_audio('ChopMe')
    n_tail = int(1.0 * SR)
    room = D.fdn_reverb(D.bfilt(y, 300.0, 'highpass'), K.space_rng('candy room'), t60=1.6, size=0.6, damp_hz=7000.0,
                        n_tail=n_tail)
    room = room[:len(y) + n_tail]
    mid, side = 0.5 * (room[:, 0] + room[:, 1]), 0.5 * (room[:, 0] - room[:, 1])
    wet = np.zeros((len(y) + n_tail, 2))
    wet[:len(y)] += 0.6 * D.st(y)
    wet += 0.8 * np.stack([mid + 0.35 * side, mid - 0.35 * side], 1)       # the room narrowed: it folds to mono
    return Out(wet[::-1].copy(), meta={'from': 'ChopMe', 'reversed': True})


STUTTER = 6
STUTTER_S = 0.055                   # each retrigger plays this much of "cur-" from its consonant


def candy_cur_stutter(take):
    """ "cur-" retriggered on six 32nds (half a swung-free 16th each), each 1.5 dB softer and darker, then the whole
    word once."""
    y, onset = chop_audio('ChopCurrent')
    s32 = K.STEP / 2
    a = max(0, int((onset - 0.02) * SR))
    piece = np.array(y[a:a + int((0.02 + STUTTER_S) * SR)])
    m = int(0.006 * SR)
    piece[-m:] *= 0.5 + 0.5 * np.cos(np.pi * np.arange(m) / m)
    n = int((STUTTER * s32 + 0.05) * SR) + len(y)
    out = np.zeros(n)
    for k in range(STUTTER):
        p = int(np.floor(k * s32 * SR + 0.5))
        x = piece if k == 0 else D.bfilt(piece, 9000.0 - 900.0 * k, 'lowpass')
        out[p:p + len(x)] += x * D.undb(-1.5 * k)
    p = int(np.floor(STUTTER * s32 * SR + 0.5))
    out[p:p + len(y) - a] += y[a:]
    return Out(out, meta={'from': 'ChopCurrent', 'retriggers': STUTTER, 'every_s': round(s32, 6)})


ABOUT = 'cut from "Ride the current with me" (the kit\'s line, title-checked)'
ITEMS = [
    Item('Vox', 'VoxHook', 'vox', vox_hook, loop=True, root=70, level=-18.0, flags=VOX, qa=WHY, sets=SETS,
         tags=('clean', 'chop', 'sung', 'hook'), script=HOOK_WORDS,
         about=f'filtered chopped vocal hook, 2 bars on the kit swing: ride, ri-, the current, me | ride, ri-, with me; '
               f'gated slices through a band-pass whose low-pass opens and closes over the 2 bars; {ABOUT}'),
    Item('Vox', 'CandyRideFifth', 'vox', candy_ride_fifth, root=77, level=-18.0, flags=VOX, qa=WHY, sets=SETS,
         tags=('clean', 'chop', 'candy'), script='ride',
         about=f'ear candy: "ride" a fifth up on F5 (WORLD keeps its formants), an 8th-note ping-pong echo; {ABOUT}'),
    Item('Vox', 'CandyMeReverse', 'vox', candy_me_reverse, level=-18.0, flags=VOX | {'cut_end'}, qa=WHY, sets=SETS,
         tags=('clean', 'chop', 'candy'), script='me',
         about=f'ear candy: "me" through a short room, reversed: a breathy swell ending on the downbeat; {ABOUT}'),
    Item('Vox', 'CandyCurStutter', 'vox', candy_cur_stutter, root=67, level=-18.0, flags=VOX, qa=WHY, sets=SETS,
         tags=('clean', 'chop', 'candy'), script='current',
         about=f'ear candy: "cur-" retriggered on six 32nds, softer and darker each time, then "current" whole; {ABOUT}'),
]
