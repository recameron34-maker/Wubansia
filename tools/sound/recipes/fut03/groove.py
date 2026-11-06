"""FUT03 round 4 drums and perc: garage-house and rolling deep tech house at 128 BPM in G minor.

Round 4 moves the kit from hyper-clean glass to warm and organic. Every drum here is new; the glass bell (K.RATIOS)
survives only as a faint colour in the hats, the shaker and the rim.

- KickDeep: a deep, round house kick in round 1's split design (binding since round 1): a clean sine sub on the key's
  root, G1 (49 Hz), held a little longer than round 3's for depth and still gone by 0.33 s (20 dB down by the offbeat
  8th, so the rolling bass has the gap); a round knock above 110 Hz from a low, slow sweep (it starts at 170-330 Hz,
  where round 3's started at 250-560, so it thumps instead of clicking), only gently saturated and gone by 80 ms; and
  a soft felt beater on top: a short band of noise and a damped 1.6 kHz blip, nothing under 1 kHz, the kick's only
  drive. No glass ring. The top layer keeps the crest floor (ADR 0011).
- ClapSnare: a clap and a snare layered in a short room. The clap is four hands of band noise in 24-32 ms, the first the
  loudest; the snare is a tuned body on G3 (196 Hz) with its membrane's second mode on Eb4 (in the key), dropping onto
  pitch, under snare wires of noise; a short warm room (0.6 s, 8 ms pre-delay) after them; a gentle tanh at 4x.
- ClosedHatDeep, OpenHatDeep: warm, organic hats: band-passed noise with a stick tick, darker than round 3's glass
  hats (their top rolls off from 11-14 kHz), a faint ring of the kit's bell partials over C7 underneath (the glass,
  as a colour, 14 dB down). The closed hat is gone inside a 16th; the open one breathes about an 8th. They share a
  choke group.
- ShakerDeep: a gourd shaker for the 16ths: a cloud of seed grains (short noise grains, 1 in 5 of them a glass bead
  ringing the bell's partials) on a swell and a fall, over a soft "sh"; wide, gone inside a 16th.
- Conga (G3, C4, D4) and Bongo (G4, D5): hand drums on a membrane's modes (1, 1.59, 2.14, 2.30, 2.65), each zone in the
  key, the pitch settling from a little sharp. Velocity is the stroke: the soft layer a muffled ghost (short, dark),
  the middle an open tone, the hard one a slap (more skin noise and upper modes, shorter).
- Rimshot: a woody rim click: a stick tick over two short wood modes and, faint, a glass ring on D6.
- PercLoop: 2 bars of the congas, bongos and rims as the context loop plays them, with swung ghost notes.
"""
import numpy as np

from tools.sound import dsp48 as D
from tools.sound import parts as P
from tools.sound import phrase as Ph
from tools.sound.item import Item, Out
from tools.sound.kits import fut03 as K

SR = D.SR
LAYERS = (0.2, 0.4, 0.6, 0.8, 1.0)
RATIOS = np.array(K.RATIOS)
F_KICK = float(D.midi_hz(K.KICK_NOTE))            # G1, 49.0 Hz
SETS = ('r4', 'r5')

# ------------------------------------------------------------------ the deep kick: split, round, warm
XOVER = 110.0                # sub / knock crossover (LR4, sums flat)
SWEEP_HI = (180.0, 380.0)    # the sweep's start, soft to hard: low, so the knock thumps (round 3: 250-560)
SNAP = (0.0105, 0.0070)      # its fast time constant, soft to hard
GLIDE = (0.020, 0.30)        # its glide onto G1: time constant (s) and weight: holds 90-140 Hz, the round punch
SUB_SHAPE = (0.130, 0.022, 1.8)   # the sub's hold-then-drop, exp(-(t / (a + b v)) ** c): deeper than round 3's
SUB_FADE = (0.17, 0.16)      # and a raised-cosine fade: exactly zero from 0.33 s
KNOCK_DRIVE = (1.1, 1.9)     # tanh drive, softest to hardest: gentle, round
KNOCK_MIX = (0.60, 1.00)
KNOCK_DIP = (270.0, -9.0, 0.8)   # a dip in the knock's low mids: Hz, dB, Q (the mud gate)
KNOCK_LP = 2800.0            # the knock rolls off early: round, no click in it
KNOCK_RISE = 0.0015
BEATER_HZ = 1600.0           # the felt beater's blip
BEATER_MIX = (0.12, 0.40)    # the top's level, softest to hardest
BEATER_DRIVE = (1.4, 3.0)    # the beater's tanh at 8x: the only drive on the kick worth the name


def beater(take, n):
    """The felt beater: a damped blip at 1.6 kHz and a short band of noise (1.2-6 kHz), driven gently at 8x, then
    nothing under 1 kHz. Harder hits: a brighter, slightly longer tick."""
    v = take.vel
    r = take.rng('beater')
    nt = int(0.06 * SR)
    t = P.times(nt)
    blip = np.sin(2 * np.pi * BEATER_HZ * t) * np.exp(-t / (0.0016 + 0.0010 * v))
    tick = P.band_noise(r, nt, 1200.0, 5000.0 + 2000.0 * v, order=2) * D.env_exp(nt, 0.0010 + 0.0008 * v, 0.0002)
    x = 0.7 * blip + (0.5 + 0.4 * v) * tick
    drive = P.lerp(BEATER_DRIVE, v)
    top = D.dc_block(D.oversampled(lambda u: D.tanh_adaa(u, drive), x, 8), 200.0)
    top = D.bfilt(D.lr4(top * D.taper(nt, 0.03, 0.025), 1000.0, 'highpass'), 7000.0 + 4000.0 * v, 'lowpass')
    out = np.zeros(n)
    out[:nt] = top
    return out


def kick_deep(take):
    """Three parts shaped apart (the split design): a clean sine sub on the sweep, held for depth; a round knock above
    110 Hz, gently saturated and gone by 80 ms; the felt beater above 1 kHz."""
    v = take.vel
    r = take.rng('deep')
    n = int(0.6 * SR)
    t = P.times(n)
    f_hi = P.lerp(SWEEP_HI, v) * 2 ** (r.normal(0, 6) / 1200)
    tau1 = P.lerp(SNAP, v) * (1 + 0.04 * r.uniform(-1, 1))
    s = np.sin(P.sweep(n, f_hi, tau1, *GLIDE, F_KICK))
    a, b, c = SUB_SHAPE
    sub = D.lr4(s * np.exp(-(t / (a + b * v)) ** c) * D.taper(n, *SUB_FADE), XOVER, 'lowpass')
    drive = P.lerp(KNOCK_DRIVE, v)
    kd = D.oversampled(lambda u: D.tanh_adaa(u, drive), s * np.exp(-t / 0.040), 4)
    a_k = P.gate(t, 0.007 + 0.004 * v, 0.015 + 0.005 * v) * D.taper(n, 0.040, 0.040) * P.raised(n, KNOCK_RISE)
    knock = D.bfilt(D.peaking(D.lr4(kd * a_k, XOVER, 'highpass'), *KNOCK_DIP), KNOCK_LP, 'lowpass')
    y = sub + P.lerp(KNOCK_MIX, v) * knock + P.lerp(BEATER_MIX, v) * beater(take, n)
    return y * P.vel_gain(v, 12.0)


# ------------------------------------------------------------------ the clap and snare, in a short room
HANDS = (0.0, 0.0085, 0.0170, 0.0275)        # the hands' onsets (s) at velocity 0.6, times SPREAD
SPREAD = (1.15, 0.90)
HAND_LEVELS = (1.0, 0.82, 0.74, 0.70)        # the first the loudest
HAND_TAU = (0.0030, 0.0022)
HAND_BAND = ((800.0, 1400.0), (4000.0, 7500.0))   # the hands' noise band, soft to hard
HAND_TAIL = (0.30, (0.045, 0.055))           # the last hand's tail: level, fall soft to hard
SNARE_NOTE = K.ROOT + 12                     # G3, 196 Hz: the snare body; its second mode 1.593 x is Eb4 (311 Hz)
SNARE_MODE2 = 1.5926
SNARE_BODY = ((0.52, 0.52), (0.045, 0.038))  # level, fall (s), soft to hard
WIRES = ((0.22, 0.50), (0.070, 0.095), (1800.0, 9000.0))   # the snare wires: level, fall, band
ROOM = dict(level=0.55, pre=0.008, t60=0.6, size=0.32, damp_hz=7500.0, hp=450.0)   # the short warm room
CLAP_CLIP = (1.5, 0.3)
CLAP_FADE = (0.30, 0.15)


def hand(n, rise, tau):
    a = max(1, int(rise * SR))
    e = np.exp(-np.maximum(np.arange(n) - a, 0) / (tau * SR))
    e[:a] = 0.5 - 0.5 * np.cos(np.pi * np.arange(a) / a)
    return e


def hands(take, n, side):
    """Four hands of band noise, each at exactly its level (normalised over its first 12 ms), and the last hand's
    tail; the hands' envelope, for the air."""
    v = take.vel
    r = take.rng('hands')
    lo, hi = (P.lerp(b, v) for b in HAND_BAND)
    tau = P.lerp(HAND_TAU, v)
    spread = P.lerp(SPREAD, v)
    onsets = [0] + [int(at * spread * SR * (1 + 0.06 * r.uniform(-1, 1))) for at in HANDS[1:]]
    m = int(0.012 * SR)
    y, env = np.zeros(n), np.zeros(n)
    for k, (a, lv) in enumerate(zip(onsets, HAND_LEVELS)):
        e = hand(n - a, 0.0007 if k == 0 else 0.0002, tau)
        x = D.peaking(P.band_noise(take.rng('hand', k, side), n - a, lo, hi, order=4), 2400.0, 3.0, 1.0) * e
        x *= lv / (np.sqrt(np.mean(x[:m] ** 2)) + 1e-12)
        y[a:] += x
        env[a:] += lv * e
    a = onsets[-1]
    level, tau_t = HAND_TAIL[0], P.lerp(HAND_TAIL[1], v)
    tail = P.band_noise(take.rng('tail', side), n - a, lo, hi, order=4) * hand(n - a, 0.0008, tau_t)
    y[a:] += level * tail / (np.sqrt(np.mean(tail[:m] ** 2)) + 1e-12)
    env[a:] += level * hand(n - a, 0.0008, tau_t)
    return y, env


def clap_snare(take):
    """The clap's four hands over the snare's tuned body and its wires, in a short warm room, then a 4x tanh. Harder
    hits: tighter hands, a brighter band, more body and wires."""
    v = take.vel
    n = int(0.5 * SR)
    t = P.times(n)
    mid, env = hands(take, n, 0)
    side = 0.40 * hands(take, n, 1)[0]
    lv, tau_b = (P.lerp(b, v) for b in SNARE_BODY)
    fb = float(D.midi_hz(SNARE_NOTE))
    bend = 0.25 * np.exp(-t / 0.007)
    body = (np.sin(2 * np.pi * D.phase(fb * (1 + bend), n)) * np.exp(-t / tau_b)
            + 0.45 * np.sin(2 * np.pi * D.phase(fb * SNARE_MODE2 * (1 + bend), n) + 0.5) * np.exp(-t / (0.6 * tau_b)))
    body *= P.raised(n, 0.0008) * lv
    w_lv, w_tau, (w_lo, w_hi) = P.lerp(WIRES[0], v), P.lerp(WIRES[1], v), WIRES[2]
    wires = P.band_noise(take.rng('wires'), n, w_lo, w_hi, order=2) * hand(n, 0.001, w_tau) * w_lv
    air = D.bfilt(P.hp_noise(take.rng('air'), n, 6500.0, order=2), 12000.0, 'lowpass', order=2) * env * (0.3 + 0.5 * v)
    c = mid + body + wires + air
    rm = ROOM
    pre = int(rm['pre'] * SR)
    send = D.bfilt(c, rm['hp'], 'highpass', order=2)
    send = np.concatenate([np.zeros(pre), send[:n - pre]])
    room = D.fdn_reverb(send, K.space_rng('snare room'), t60=rm['t60'], size=rm['size'], damp_hz=rm['damp_hz'])[:n]
    y = np.stack([c + side, c - side], 1) + rm['level'] * room
    drive, into = CLAP_CLIP
    y = D.oversampled(lambda u: P.soft_clip(into * u, drive), y, 4) / into
    return P.low_clean(y, 150.0) * D.taper(n, *CLAP_FADE)[:, None] * P.vel_gain(v, 12.0)


# ------------------------------------------------------------------ hats: warm and organic, a faint glass ring
CHOKE = {'group': 'hats', 'by': 'ClosedHatDeep', 'fade_s': 0.004}
HAT_RING = float(D.midi_hz(K.ROOT + 53))     # C7, 2093 Hz x the bell's upper ratios: the faint glass colour
CLOSED_DECAY = (0.010, 0.008)
OPEN_DECAY = (0.085, 0.045)
HAT_FADE = (0.30, 0.15)


def hat(take, decay):
    """Band-passed noise (two decorrelated sides over a common core), a stick tick, and 14 dB down a faint ring of
    the bell's upper partials over C7; high-passed at 5.5 kHz, rolled off from 11-14 kHz with velocity."""
    v = take.vel
    n = int(0.5 * SR)
    t = P.times(n)
    nz = D.rng(take.kit, take.item.name, take.layer, 'noise')        # one grain per layer: the rr's differ in the ring
    r = take.rng('hat')
    core = P.band_noise(nz, n, 5500.0, 15000.0, order=2)
    sides = [P.band_noise(D.rng(take.kit, take.item.name, take.layer, 'side', c), n, 5500.0, 15000.0, order=2)
             for c in range(2)]
    x = np.stack([0.75 * core + 0.45 * sides[0], 0.75 * core + 0.45 * sides[1]], 1)
    ring = sum(np.sin(2 * np.pi * HAT_RING * rt * (1 + 0.002 * r.uniform(-1, 1)) * t + r.uniform(0, 2 * np.pi))
               * a for rt, a in zip(RATIOS[3:], (1.0, 0.8, 0.6, 0.5))) / 2.9
    x = x + D.undb(-14.0) * D.st(ring)
    x = D.bfilt(D.bfilt(x, 5500.0, 'highpass', order=4), 8500.0 + 6500.0 * v, 'lowpass', order=2)
    tick = D.bfilt(nz.standard_normal(n), (6000.0, 14000.0), 'bandpass', order=2) * D.env_exp(n, 0.0009, 0.0002)
    env = D.env_exp(n, P.lerp(decay, v), 0.0004)
    y = x * env[:, None] + (0.2 + 0.3 * v) * D.st(tick)
    return y * D.taper(n, *HAT_FADE)[:, None] * P.vel_gain(v, 13.0)


def closed_hat(take):
    return Out(hat(take, CLOSED_DECAY), meta={'choke': CHOKE})


def open_hat(take):
    return Out(hat(take, OPEN_DECAY), meta={'choke': CHOKE})


# ------------------------------------------------------------------ the gourd shaker
SHAKER_LAYERS = (0.4, 0.6, 0.8, 1.0)
SHAKER_GRAINS = (90, 200)
SHAKER_RISE = (0.008, 0.004)
SHAKER_FALL = (0.018, 0.028)
SHAKER_BEAD = float(D.midi_hz(K.ROOT + 72))   # G8: 1 grain in 5 is a glass bead ringing the bell's partials over it
SHAKER_FADE = (0.15, 0.08)


def shaker(take):
    """Seed grains (40-110 us noise grains, each its own band and place across the field) whose density follows the
    stroke's swell and fall, one in five a glass bead, over a soft "sh"; high-passed at 3 kHz."""
    v = take.vel
    r = take.rng('grains')
    n = int(0.25 * SR)
    t = P.times(n)
    env = (1 - np.exp(-t / P.lerp(SHAKER_RISE, v))) * np.exp(-t / P.lerp(SHAKER_FALL, v))
    env /= env.max()
    cdf = np.cumsum(env)
    cdf /= cdf[-1]
    ng = int(round(P.lerp(SHAKER_GRAINS, v)))
    at = np.searchsorted(cdf, (np.arange(ng) + r.uniform(0.1, 0.9, ng)) / ng)
    m = int(0.003 * SR)
    tg = P.times(m)
    y = np.zeros((n + m, 2))
    for a in at:
        if r.uniform() < 0.2:
            f = SHAKER_BEAD * RATIOS[:4] * (1 + 0.02 * r.uniform(-1, 1, 4))
            g = np.sum(np.exp(-tg[None, :] / 0.0004) * np.sin(2 * np.pi * f[:, None] * tg[None, :]
                                                             + r.uniform(0, 2 * np.pi, 4)[:, None]), 0) * 0.4
        else:
            ln = r.uniform(0.00004, 0.00011)
            g = r.standard_normal(m) * np.exp(-tg / ln)
        p = 0.45 * r.uniform(-1, 1)
        y[a:a + m] += (g * env[a])[:, None] * np.array([np.cos((p + 1) * np.pi / 4), np.sin((p + 1) * np.pi / 4)])
    y = y[:n] * np.sqrt(2) / np.sqrt(SHAKER_GRAINS[1])
    common = P.hp_noise(take.rng('sh', 'common'), n, 4000.0, order=2)
    sh = np.stack([0.7 * common + 0.3 * P.hp_noise(take.rng('sh', c), n, 4000.0, order=2) for c in range(2)], 1) \
        * env[:, None]
    x = 1.0 * y + (0.10 + 0.06 * v) * sh
    x = D.bfilt(D.bfilt(x, 3000.0, 'highpass', order=4), 8000.0 + 9000.0 * v, 'lowpass', order=2)
    return x * D.taper(n, *SHAKER_FADE)[:, None] * P.vel_gain(v, 10.0, SHAKER_LAYERS[0])


# ------------------------------------------------------------------ congas and bongos: tuned membranes
HAND_LAYERS = (0.3, 0.6, 1.0)          # the strokes: muffled ghost, open tone, slap
CONGA_NOTES = (K.ROOT + 12, K.ROOT + 17, K.ROOT + 19)   # G3, C4, D4
BONGO_NOTES = (K.ROOT + 24, K.ROOT + 31)                # G4, D5
MEMBRANE = (1.0, 1.593, 2.136, 2.296, 2.653)
MEMBRANE_LEVEL = (1.0, 0.45, 0.30, 0.22, 0.15)


def membrane(take, decay):
    """One stroke on a tuned membrane: the modes of a circular membrane (MEMBRANE) on the zone's note, the pitch settling
    from 3-6 % sharp in 12 ms, a skin noise on the attack; the soft layer muffled (short, few modes), the hard one a
    slap (shorter, more skin and upper modes). Mono, high-passed at 100 Hz."""
    v = take.vel
    r = take.rng('skin')
    f0 = take.f0
    n = int(1.0 * SR)
    t = P.times(n)
    slap = max(0.0, (v - 0.6) / 0.4)
    muffle = max(0.0, (0.6 - v) / 0.3)
    tau0 = decay * (1.0 - 0.55 * muffle - 0.35 * slap)
    bend = (0.03 + 0.03 * v) * np.exp(-t / 0.012)
    ph = D.rng(take.kit, take.item.name, take.note, 'modes')
    y = np.zeros(n)
    for k, (rt, lv) in enumerate(zip(MEMBRANE, MEMBRANE_LEVEL)):
        if f0 * rt > 16000:
            continue
        lvl = lv * (0.4 + 0.6 * v + 0.8 * slap) ** (k > 0) * (1.0 - 0.7 * muffle) ** k
        y += lvl * np.sin(2 * np.pi * D.phase(f0 * rt * (1 + bend), n) + ph.uniform(0, 2 * np.pi)) \
            * np.exp(-t / (tau0 / (1 + 0.8 * k)))
    skin = P.band_noise(r, n, 600.0 + 2000.0 * v, 6000.0 + 4000.0 * slap, order=2)
    skin *= 0.25 / (np.sqrt(np.mean(skin[:int(0.01 * SR)] ** 2)) + 1e-12)     # one level on every round robin
    skin *= D.env_exp(n, 0.004 + 0.004 * slap, 0.0003) * (0.05 + 0.45 * v + 0.9 * slap)
    y = (y + skin) * P.raised(n, 0.0006)
    y = D.bfilt(y, 100.0, 'highpass', order=4) * D.taper(n, 0.55, 0.40)
    return y * (0.35 + 0.65 * v)


def conga(take):
    return membrane(take, 0.20)


def bongo(take):
    return membrane(take, 0.11)


RIM_LAYERS = (0.3, 0.6, 1.0)
RIM_MODES = ((1240.0, 0.018), (2010.0, 0.011))     # the wood: Hz, fall (s)
RIM_GLASS = float(D.midi_hz(K.ROOT + 43))          # D6, 1175 Hz: a faint glass ring on the bell's low ratios


def rimshot(take):
    """A stick tick (2-9 kHz, under a millisecond) over two short wood modes and, 18 dB down, the bell's prime and
    quint on D6: the kit's glass, as a colour. High-passed at 300 Hz."""
    v = take.vel
    r = take.rng('rim')
    n = int(0.3 * SR)
    t = P.times(n)
    wood = sum(np.sin(2 * np.pi * f * (1 + 0.004 * r.uniform(-1, 1)) * t + r.uniform(0, 2 * np.pi))
               * np.exp(-t / (tau * (0.8 + 0.4 * v))) for f, tau in RIM_MODES)
    glass = sum(np.sin(2 * np.pi * RIM_GLASS * rt * t) * np.exp(-t / 0.05) for rt in RATIOS[[0, 2]]) * D.undb(-18.0)
    tick = P.band_noise(r, n, 2000.0, 9000.0, order=2) * D.env_exp(n, 0.0007 + 0.0005 * v, 0.0001)
    y = (0.8 * wood + glass + (0.6 + 0.8 * v) * tick) * P.raised(n, 0.0002)
    y = D.oversampled(lambda u: D.tanh_adaa(u, 1.6), y, 4)
    return D.bfilt(y, 300.0, 'highpass', order=4) * D.taper(n, 0.15, 0.12) * P.vel_gain(v, 10.0, RIM_LAYERS[0])


# ------------------------------------------------------------------ the 2-bar perc preview
PERC_STEPS = 32
# (16th, item, zone or None, velocity): the congas' tumbao-like figure, the bongos answering, rims on the offbeat
# "a"s; ghosts (0.3) on the swung 16ths
PERC = (
    (2, 'Conga', 60, 0.6), (3, 'Conga', 60, 0.3), (6, 'Conga', 62, 1.0), (7, 'Conga', 55, 0.3),
    (10, 'Conga', 55, 0.6), (11, 'Bongo', 67, 0.3), (13, 'Rimshot', None, 0.6), (14, 'Conga', 62, 0.6),
    (15, 'Conga', 60, 0.3),
    (18, 'Conga', 60, 0.6), (19, 'Conga', 60, 0.3), (22, 'Conga', 62, 1.0), (23, 'Bongo', 74, 0.3),
    (26, 'Bongo', 67, 0.6), (27, 'Bongo', 74, 0.3), (29, 'Rimshot', None, 0.6), (30, 'Conga', 55, 1.0),
    (31, 'Conga', 60, 0.3),
)
PERC_PAN = {'Conga': -0.25, 'Bongo': 0.35, 'Rimshot': 0.15}


def perc_hits(hits, grid, length):
    """Hand-drum and rim hits on the grid, each the layer nearest its velocity, round robins in turn per item, panned."""
    y = np.zeros((length, 2))
    count = {}
    for step, name, note, vel in hits:
        item, files, _ = Ph.kit_files(K, name)
        layer = 1 + min(range(len(item.layers)), key=lambda i: (abs(item.layers[i] - vel), -i))
        k = count.get(name, 0)
        count[name] = k + 1
        x = Ph.pan(files[(note, layer, 1 + k % item.rr)].audio, PERC_PAN[name])
        Ph.place(y, x, grid.at(step))
    return y


def perc_loop(take):
    G = Ph.Grid(K, PERC_STEPS)
    return Out(perc_hits(PERC, G, G.length),
               meta={'steps': PERC_STEPS, 'bpm': K.BPM, 'swing': K.SWING,
                     'hits': [[s, n, D.note_name(z) if z else None, v] for s, n, z, v in PERC]})


PERC_QA = {'tune_window': (0.03, 0.20), 'why': 'a hand drum rings short: its pitch is read over 30-200 ms'}
ITEMS = [
    Item('Drums', 'KickDeep', 'kick', kick_deep, layers=LAYERS, rr=2, root=K.KICK_NOTE, sets=SETS,
         about='deep round house kick, split: a clean G1 sine sub held for depth (gone by 0.33 s), a round knock from a '
               'low slow sweep above 110 Hz, gone by 80 ms, and a soft felt beater above 1 kHz'),
    Item('Drums', 'ClapSnare', 'snare', clap_snare, layers=LAYERS, rr=2, sets=SETS,
         about='clap and snare layered in a short room: four hands within 32 ms, the first the loudest, over a snare body '
               'on G3 (its second mode on Eb4) and its wires; a 0.6 s warm room'),
    Item('Drums', 'ClosedHatDeep', 'hat', closed_hat, layers=LAYERS, rr=3, sets=SETS,
         about='warm closed hat: band-passed noise and a stick tick, a faint ring of the bell partials over C7, gone '
               'inside a 16th; chokes the open hat'),
    Item('Drums', 'OpenHatDeep', 'hat', open_hat, layers=LAYERS, rr=3, sets=SETS,
         about='warm open hat for the offbeats, about an 8th long, choked by the closed hat'),
    Item('Perc', 'ShakerDeep', 'hat', shaker, layers=SHAKER_LAYERS, rr=4, sets=SETS,
         about='gourd shaker for the 16ths: seed grains on a swell and a fall, one in five a glass bead, over a soft '
               '"sh"; wide, gone inside a 16th'),
    Item('Perc', 'Conga', 'perc', conga, layers=HAND_LAYERS, rr=2, notes=CONGA_NOTES, sets=SETS, qa=PERC_QA,
         about='conga on a tuned membrane, G3 C4 D4: muffled ghost, open tone and slap by velocity'),
    Item('Perc', 'Bongo', 'perc', bongo, layers=HAND_LAYERS, rr=2, notes=BONGO_NOTES, sets=SETS, qa=PERC_QA,
         about='bongo on a tuned membrane, G4 D5: muffled ghost, open tone and slap by velocity'),
    Item('Perc', 'Rimshot', 'perc', rimshot, layers=RIM_LAYERS, rr=2, sets=SETS,
         about='woody rimshot: a stick tick over two wood modes and a faint glass ring on D6'),
    Item('Loops', 'PercLoop', 'loop', perc_loop, loop=True, sets=SETS, tags=('preview',),
         qa={'mud': 'lowcut', 'why': 'a perc loop: the low cut, like the congas and the rims'},
         about='2-bar perc preview at 128 BPM on the kit swing: congas and bongos with swung ghosts, rims on the "a"'),
]
