"""FUT01 round 4 bass (set r4): the speed garage warp bass, the hook, as A clean and B warm, and an organ bass option.

The warp voice (WarpClean, WarpWarm): a detuned saw pair (one a side, +-9 cents) over a square in the centre, each
driven on its own at 4x (no intermodulation, so every partial stays on its note's harmonic series), through a resonant
low-pass that opens on each attack and closes over about 100 ms (the "wah" of every note: a sampler restarts the file
on each note), over a mono sine sub. The wobble and the pitch dips are the line's, as the engine plays them: the lane
filter wobbling on the grid and glides on the sampler voice (context4.py). A wobble baked into the file read as
inharmonic energy (its resonant sweep's sidebands, -35 dB against the alias check's -60), so the files stay steady.

- A, WarpClean (the strict mud gate, the category default): the stack only above 450 Hz, about 96 dB per octave, so
  150-400 Hz stays empty; its weight sits on the filter's resonance above.
- B, WarpWarm (the proposed bass-body rule, ADR 0011): A exactly, plus the stack's own 120-450 Hz through the same
  filter (WARM_BODY of the stack's level) and a quiet second harmonic on the sub: body up to a plain saw's level.
  Everything above 450 Hz is A's, so the A/B is the body alone.
- OrganBass, the option: a round organ bass, additive (16', 8', 5 1/3' and 4' drawbars, a percussion partial on the
  attack and a short key click), its low mids its own harmonics under a saw's roll-off (bass-body).

The multisamples (F#1, A1, C#2, two layers) loop over one 2-bar turn, every partial on a grid of whole cycles per loop,
so the loop joins by itself. The onsets follow the reese's (bass.py): the
sub rises over 18-35 ms per zone, the stack starts 3 ms later and rises over 8 ms, B's body with the sub.
"""
import numpy as np

from tools.sound import dsp48 as D
from tools.sound import parts as P
from tools.sound.item import Item, Out
from tools.sound.kits import fut01 as K
from tools.sound.recipes.fut01 import bass as Rb

SR = D.SR
NOTES = Rb.REESE_NOTES               # F#1, A1, C#2: the zones a line repitches up from
LAYERS = Rb.REESE_LAYERS             # 0.6, 1.0
LOOP_S = 32 * K.STEP                 # one 2-bar turn
ATTACK_S = Rb.ATTACK_S
DETUNE_CENTS = 9.0                   # the saw pair, either way of the square
MID_HP, WARM_HP = Rb.MID_HP, Rb.WARM_HP
SUB_LEVEL = 0.5
MID_LEVEL = 2.4
WARM_BODY = 0.38                     # B's body band against the stack above the wall: 0.50 read -1.8 dB against the sub
                                     # on C#2's hard layer (the bass-body rule allows -2), and in the loop's lines the
                                     # notes repitched up to F#2 read -1.2 at 0.45
WARM_H2 = 0.2
LP = dict(base=(620.0, 950.0), env=(2.5, 3.5), env_tau=0.10, q=2.2)   # Hz soft..hard; the attack opens the cutoff
                                     # by env x, back over env_tau
SQUARE = 0.45                        # the square's share of the stack


def square(freq, n, ph0, drive, k=4):
    """A band-limited square at k x (two polyBLEP saws half a cycle apart), driven, back at the output rate."""
    u = 0.5 * (D.polyblep_saw(freq, n, ph0, k=k) - D.polyblep_saw(freq, n, (ph0 + 0.5) % 1.0, k=k))
    return D.down(D.tanh_adaa(u, drive), k)


def warp_stack(f, grid, n, vel, phases):
    """(n, 2): the saw pair (left -DETUNE, right +DETUNE, on the loop's grid) over the shared square; and every
    fundamental, for the alias check."""
    d = round(f * (2 ** (DETUNE_CENTS / 1200) - 1) / grid) * grid
    drive = 1.3 + 1.2 * vel
    bite = np.ones(n)
    saws = [Rb.saw(f + s * d, n, phases[i], drive, bite) for i, s in enumerate((-1, 1))]
    sq = square(f, n, phases[2], drive)
    stack = np.stack([(1 - SQUARE) * saws[0] + SQUARE * sq, (1 - SQUARE) * saws[1] + SQUARE * sq], 1)
    return stack, (f - d, f, f + d)


def env_filter(x, vel, n, hp):
    """The warp's pluck: a resonant TPT low-pass whose cutoff opens on the attack and closes over LP['env_tau']; hp:
    the band's floor (A's wall, or B's body from WARM_HP), applied before and, for A, again after."""
    t = np.arange(n)
    opening = np.exp(-t / SR / LP['env_tau'])
    fc = np.minimum(P.lerp(LP['base'], vel) * (1.0 + P.lerp(LP['env'], vel) * opening), 16000.0)
    x = P.hp_steep(x, hp)
    y = np.stack([D.svf_tpt(x[:, c], fc, q=LP['q'])[0] for c in range(2)], 1)
    return P.hp_steep(y, hp) if hp == MID_HP else y


def warp(take, char):
    """One zone and layer, looped: the attack, then one 2-bar turn."""
    L, grid, f = P.loop_grid(take.f0, LOOP_S)
    A0 = int(ATTACK_S * SR)
    n = A0 + int(0.02 * SR) + L
    vel = take.vel
    r = D.rng(take.kit, 'Warp', 'phases')                   # A and B share every phase: they differ in the body only
    phases = [r.uniform() for _ in range(3)]
    x, f0s = warp_stack(f, grid, n, vel, phases)
    later = lambda delay, rise: np.concatenate([np.zeros(int(delay * SR)), P.raised(n - int(delay * SR), rise)])
    sub_rise = P.raised(n, Rb.SUB_ONSET[take.note])
    mids = env_filter(P.hp_steep(x * later(*Rb.MID_ONSET)[:, None], MID_HP), vel, n, MID_HP)
    if char == 'warm':
        body_rise = later(Rb.BODY_ONSET[0], Rb.BODY_ONSET[1] * Rb.SUB_ONSET[take.note])[:, None]
        mids = mids + WARM_BODY * env_filter(Rb.lp_steep(x * body_rise, MID_HP), vel, n, WARM_HP)
    sub = Rb.sub_wave(D.phase(f, n), char) * sub_rise
    y = D.mono_below(SUB_LEVEL * vel ** 0.6 * D.st(sub) + MID_LEVEL * vel ** 1.5 * mids, 150.0)
    y, loop = P.close_loop(y, A0, L)
    return Out(y, loop=loop, f0s=tuple(sorted(set(f0s))))


def warp_clean(take):
    return warp(take, 'clean')


def warp_warm(take):
    return warp(take, 'warm')


# ------------------------------------------------------------------ the organ bass option
DRAWBARS = ((1, 1.0), (2, 0.55), (3, 0.30), (4, 0.20))     # (harmonic, level): 16', 8', 5 1/3', 4'
DRAWBARS_UP = ((6, 0.10), (8, 0.08))                       # 2 2/3' and 2': brighter on the hard layer
PERC = (6, 0.35, 0.07)                                     # percussion: harmonic, level, decay (s)
CLICK = (0.08, 0.003)                                      # the key click: level, decay (s)


def organ_bass(take):
    """A round organ bass: additive drawbars on the loop's grid (every partial a harmonic of the note), a percussion
    partial and a key click on the attack, both gone long before the loop; the hard layer opens two upper drawbars.
    Mono."""
    L, grid, f = P.loop_grid(take.f0, LOOP_S)
    A0 = int(ATTACK_S * SR)
    n = A0 + int(0.02 * SR) + L
    vel = take.vel
    t = P.times(n)
    ph = 2 * np.pi * D.phase(f, n)
    up = (vel - LAYERS[0]) / (LAYERS[1] - LAYERS[0])
    bars = DRAWBARS + tuple((h, a * (0.3 + 0.7 * up)) for h, a in DRAWBARS_UP)
    upper = sum(a * np.sin(h * ph) for h, a in bars if 1 < h and h * f < 18000)
    h, a, d = PERC
    upper = upper + a * vel * np.sin(h * ph) * np.exp(-t / d)
    # the onsets, as the reese's (bass.py): the fundamental rises over its zone's time, the drawbars above it start
    # 6 ms later and rise over 1.5 x that, so no 20 ms window at a note's start holds more low mids than sub
    rise = Rb.SUB_ONSET[take.note]
    body_rise = np.concatenate([np.zeros(int(Rb.BODY_ONSET[0] * SR)),
                                P.raised(n - int(Rb.BODY_ONSET[0] * SR), Rb.BODY_ONSET[1] * rise)])
    click = P.hp_noise(take.rng('click'), n, 2500.0, order=2) * D.env_exp(n, CLICK[1], 0.0003)
    y = np.sin(ph) * P.raised(n, rise) + upper * body_rise + CLICK[0] * vel * click
    y = D.st(0.42 * vel ** 1.2 * y)
    y, loop = P.close_loop(y, A0, L)
    return Out(y, loop=loop, f0s=(f,))


R4 = ('r4',)
WARM = {'mud': 'bass-body', 'why': "B, warm: the proposed bass-body rule (ADR 0011); A's stack plus its own 120-450 Hz "
                                   'through the same filter, over a mono sine sub'}
ORGAN = {'mud': 'bass-body', 'why': 'an organ bass keeps its own drawbar harmonics in 150-400 Hz, under a saw\'s '
                                    'roll-off: the proposed bass-body rule (ADR 0011)'}
WARP_LUFS = -13.2                    # A's and B's loudest takes at one LUFS-I, so the A/B is by character
WARP_ABOUT = ('speed garage warp bass: a detuned saw pair over a square, each driven at 4x, a resonant low-pass that '
              'opens on each attack and closes over 100 ms, over a mono sine sub; 2-bar loops')
ITEMS = [
    Item('Bass', 'WarpClean', 'bass', warp_clean, layers=LAYERS, notes=NOTES, loop=True, sets=R4, level=WARP_LUFS,
         about=f'A, clean: {WARP_ABOUT}; the stack from 450 Hz (the strict mud gate)'),
    Item('Bass', 'WarpWarm', 'bass', warp_warm, layers=LAYERS, notes=NOTES, loop=True, sets=R4, level=WARP_LUFS,
         qa=WARM, about=f"B, warm: {WARP_ABOUT}; A's stack plus its own 120-450 Hz body (bass-body)"),
    Item('Bass', 'OrganBass', 'bass', organ_bass, layers=LAYERS, notes=NOTES, loop=True, sets=R4, level=WARP_LUFS,
         qa=ORGAN, about='round organ bass, the option: additive 16\', 8\', 5 1/3\' and 4\' drawbars (2 2/3\' and 2\' '
                         'opening on the hard layer), a percussion partial and a key click on the attack; mono, '
                         '2-bar loops'),
]
