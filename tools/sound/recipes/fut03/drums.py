"""FUT03 drums: the split kick, the clap, the closed and open hats, the glass-bead shaker and the 2-bar hat preview.

Hyper-clean future house at 128 BPM in G minor. Every drum has 5 velocity layers and 2 round robins (the hats 3, the
shaker 4 and 4). Velocity moves the brightness, the transient and the pitch envelope, not just the level. Round 3 (the
modern-sound standard, docs/plans/S1-modern-sound.md) rebuilt the clap, gave the kick its clear click and added the
shaker; the hats are round 2's.

- KickSplit: round 1's pick, the split design (FUT01's A, Cool 4), rebuilt for this kit. A clean sine sub on the key's
  root, G1 (49 Hz), short: held for about 100 ms and gone by 0.32 s, so it clears before the offbeat bass (20 dB down
  at 224 ms, by the offbeat 8th). Above 110 Hz a knock that is only gently saturated (a tanh at 1.2-2.4, no fold, no
  clip) and gone by 80 ms; round 3 lets its sweep glide through 100-150 Hz a little longer (GLIDE: the punch, the
  125 Hz octave) and gives it a 2 ms rise, so the click owns the first milliseconds. On top a clear glass click: an FM
  blip on G7, a tick of noise and a ring on the kit's glass ratios from D7 (round 2: G6, whose fundamental sat under
  2 kHz), the most driven part (a biased tanh at 2-6 and a soft fold, at 8x), then nothing under 1 kHz. The click reads
  -18 dB against the body (the standard's measure; round 2 -23.3). Round 1 heard kick B (Tight) as "really slamming the
  limiter": here the sub is never driven and the knock only gently, and the top layer's crest stays near Split's (true
  peak over the loudest 50 ms 6.1-6.5 dB; Split 5.8-6.4, Tight 7.9-8.9, the floor 4.0).
- Clap (round 3): four hands of band noise in 23-31 ms (tighter on harder hits), each at an exact level with the first
  the loudest, and a stick on the first; the glass tone on D5 (D, F and A: the bell's minor triad, in the key) struck
  in phase on every hand; a glass body on G3 (the bell's prime and quint, 196 and 294 Hz) that is gone within 150 ms;
  air over 6 kHz on the hands; a short bright room after a 12 ms pre-delay; a gentle tanh at 4x for the crest. The
  standard's clap: the first hand within 2 dB of the loudest (round 2: 8 dB under), a body in 150-400 Hz (round 2:
  none), a tail 20 dB down at 120-250 ms (round 2: 73 ms) and a crest of about 12 dB (round 2: 18.4, the loop's peak).
- ClosedHat and OpenHat: the same glass metal, three FM pairs on the kit's ratios over G7 (rendered at 4x, so no
  sideband folds back) with a little air, its pitch settling from sharp. The carriers sit on the bell's tierce, quint
  and nominal over G7, so they ring Bb7, D8 and G8: a G minor triad, in the key like the plings and the kick's click.
  The closed hat is 30 dB down inside a 16th (117 ms at 128 BPM); the open one sizzles about an 8th. They share a
  choke group: a closed hat cuts the open one (meta 'choke', a 4 ms fade, as the sampler will).
- Shaker (round 3): glass beads. Each stroke is a cloud of 70-180 tiny struck beads, each ringing a few of the bell's
  partials over G8 for under a millisecond, its density following the stroke's swell and fall, over a soft "sh" of
  noise through the shell's resonance; each bead at its own place across the field, so it is wide and its mono sum is
  whole. 20 dB down inside a 16th: the swung 16ths the standard asks for.
- HatLoop: a 2-bar preview of the pair on the kit's swing, played on their files as written: closed 16ths around an
  open hat on every offbeat, each open hat choked by the next closed one; on beats 4 and 8 the 16th after the open hat
  is left out, so it rings to the next beat.

The kick passes the strict mud gate (plan section 5.2); everything else is high-passed far above the kick and sub and
mono under 150 Hz.
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


# ------------------------------------------------------------------ kick: split
XOVER = 110.0                # sub / knock crossover (LR4, sums flat)
GLIDE = (0.018, 0.28)        # the sweep's glide onto G1: its time constant (s) and weight (P.sweep); round 2's
                             # (0.013, 0.24) passed 100-150 Hz in 7 ms, this one holds it longer: the punch
SUB_SHAPE = (0.118, 0.020, 1.7)   # the sub's hold-then-drop: exp(-(t / (a + b v)) ** c)
SUB_FADE = (0.16, 0.16)      # and a raised-cosine fade: exactly zero from 0.32 s (a longer fade, so the last 10 ms
                             # of the file are well under -70 dBFS)
KNOCK_DRIVE = (1.2, 2.4)     # tanh drive, softest to hardest: gentle, so the knock stays clean (no fold, no clip)
KNOCK_MIX = (0.50, 0.90)     # the knock's level, softest to hardest
KNOCK_DIP = (260.0, -9.0, 0.8)   # a dip in the knock's low mids: Hz, dB, Q
KNOCK_RISE = 0.002           # the knock's raised-cosine rise (s): the click owns the first two milliseconds
CLICK_NOTE = K.KICK_NOTE + 72   # G7, 3136 Hz: the FM blip's carrier
RING_NOTE = K.KICK_NOTE + 67    # D7, 2349 Hz: the glass ring (its modes D F A, a minor triad in the key), all
                                # of it in the click's 2-8 kHz
TOP_MIX = (0.10, 0.38)       # the top's level, softest to hardest
TOP_DRIVE = (2.0, 6.0)       # the click's biased tanh, at 8x: the kick's hardest drive (the knock's is KNOCK_DRIVE)


def kick_top(take, n):
    """The glass click: an FM blip on G7 (modulator on the ratio 1.4983), a tick of noise and a short ring on the glass
    ratios from G6, driven at 8x (biased tanh, then a soft fold), then nothing under 1 kHz. Harder hits: more FM index,
    a longer ring, more drive and a brighter top."""
    v = take.vel
    r = take.rng('click')
    nt = int(0.12 * SR)
    tt = P.times(nt)
    fc = float(D.midi_hz(CLICK_NOTE))
    idx = (1.5 + 4.0 * v) * np.exp(-tt / 0.0012)
    fm = np.sin(2 * np.pi * fc * tt + idx * np.sin(2 * np.pi * fc * RATIOS[2] * tt))
    fm *= np.exp(-tt / (0.0012 + 0.0012 * v)) * D.taper(nt, 0.006, 0.004)
    tick = P.hp_noise(r, nt, 4000.0, order=2) * D.env_exp(nt, 0.0007 + 0.0007 * v, 0.0002)
    # the ring: the same mode phases on every take (a click this short moves the round robins' level with its phases)
    ph = D.rng(take.kit, take.item.name, 'ring')
    f_ring = float(D.midi_hz(RING_NOTE)) * (1 + 0.0015 * r.uniform(-1, 1))
    decays = [(0.010 + 0.016 * v) / (1 + 0.5 * k) for k in range(len(RATIOS))]
    amps = P.hann_pulse_gain(f_ring * RATIOS, P.lerp((0.0007, 0.0003), v)) / (1 + 0.5 * np.arange(len(RATIOS)))
    ring = P.modes(nt, f_ring, RATIOS, decays, amps, np.zeros(nt), ph) * P.raised(nt, 0.0003)
    x = (0.5 + 0.5 * v) * fm + (0.25 + 0.35 * v) * tick + (0.35 + 0.25 * v) * ring
    drive = P.lerp(TOP_DRIVE, v)

    def chain(u):
        return D.sinefold_adaa(D.tanh_adaa(u, drive, bias=0.2), 1.0 + 0.4 * v)
    top = D.dc_block(D.oversampled(chain, x, 8), 200.0)
    top *= np.exp(-tt / (0.012 + 0.012 * v)) * D.taper(nt, 0.06, 0.05)
    top = D.bfilt(D.lr4(top, 1000.0, 'highpass'), 6000.0 + 10000.0 * v, 'lowpass')
    out = np.zeros(n)
    out[:nt] = top
    return out


def kick_split(take):
    """Three parts shaped apart, so no drive ever sits on the body: a clean, short sub on the sweep, a knock above
    110 Hz that is only gently saturated and gone by 80 ms, and the driven glass click above 1 kHz."""
    v = take.vel
    r = take.rng('split')
    n = int(0.6 * SR)
    t = P.times(n)

    # sweep: harder hits start higher and snap faster; the round robin moves the start and the snap, never the end. It
    # sits on G1 by about 90 ms
    f_hi = P.lerp((250.0, 560.0), v) * 2 ** (r.normal(0, 8) / 1200)
    tau1 = (0.0082 - 0.0022 * v) * (1 + 0.04 * r.uniform(-1, 1))
    s = np.sin(P.sweep(n, f_hi, tau1, *GLIDE, F_KICK))

    # 1. sub: clean, short
    a, b, c = SUB_SHAPE
    a_sub = np.exp(-(t / (a + b * v)) ** c) * D.taper(n, *SUB_FADE)
    sub = D.lr4(s * a_sub, XOVER, 'lowpass')

    # 2. knock: the same phase, a gentle tanh at 4x, high-passed, dipped at 260 Hz, gone by 80 ms
    drive = P.lerp(KNOCK_DRIVE, v)
    kd = D.oversampled(lambda u: D.tanh_adaa(u, drive), s * np.exp(-t / 0.045), 4)
    a_k = P.gate(t, 0.008 + 0.004 * v, 0.016 + 0.005 * v) * D.taper(n, 0.040, 0.040) * P.raised(n, KNOCK_RISE)
    knock = D.peaking(D.lr4(kd * a_k, XOVER, 'highpass'), *KNOCK_DIP)
    knock = D.bfilt(knock, 4500.0, 'lowpass')

    # 3. the glass click
    top = kick_top(take, n)

    y = sub + P.lerp(KNOCK_MIX, v) * knock + P.lerp(TOP_MIX, v) * top
    return y * P.vel_gain(v, 12.0)


# ------------------------------------------------------------------ clap: four hands, a stick, a glass body, a room
CLAP_HANDS = (0.0, 0.0080, 0.0160, 0.0260)   # the hands' onsets (s) at velocity 0.6...
CLAP_SPREAD = (1.2, 0.9)                     # ...times this, softest to hardest: tighter on harder hits
CLAP_LEVELS = (1.0, 0.80, 0.72, 0.70)        # each hand's level: the first is the loudest, the last carries the tail
HAND_RISE = 0.0008                            # the first hand's rise (the others: 0.2 ms), so it reads as a hand
HAND_TAU = (0.0026, 0.0018)                   # each hand's fall, softest to hardest
CLAP_TONE_NOTE = 74                           # D5, 587.33 Hz: its glass modes ring D, F and A, in the key
CLAP_TONE = (0.40, 0.28)                      # its level, softest to hardest: it thins as the hands brighten
CLAP_BODY_NOTE = K.ROOT + 12                  # G3, 196 Hz: the body rings the bell's prime and quint (G3 and D4)
CLAP_FADE = (0.30, 0.15)                      # gone by 0.45 s: a file under 0.5 s is one 400 ms loudness block
                                              # (ADR 0009)
CLAP_BAND = ((1100.0, 1500.0), (4200.0, 6000.0), (2200.0, 3200.0))   # the hands' noise band (4th order) and its
                                                                    # presence peak, each (soft, hard)
CLAP_PRESENCE_DB = 4.0
CLAP_TAIL = (0.25, (0.050, 0.060))           # the last hand's own tail: level, fall (s) soft to hard
CLAP_SNAP = ((0.5, 0.9), (2500.0, 7000.0), 0.0009)   # the stick on the first hand: level, band, fall (s)
CLAP_BODY = ((0.30, 0.42), (0.040, 0.034), 0.30)     # the body: level, fall (s) soft to hard, the pitch drop
CLAP_ROOM = dict(level=0.9, pre=0.012, t60=1.1, size=0.40, damp_hz=10000.0, hp=1500.0)   # the bright room
CLAP_AIR = (0.5, 1.3)                         # the air over 6 kHz, soft to hard
CLAP_CLIP = (1.6, 0.3)                       # the crest: a tanh at 4x over the whole clap (drive, and the level
                                              # into it: the hands' noise peaks sit about 3 dB into its curve)


def hand(n, rise, tau):
    """One hand's envelope: a raised-cosine rise to 1 over `rise` seconds, then an exponential fall from 1 (tau)."""
    a = max(1, int(rise * SR))
    e = np.exp(-np.maximum(np.arange(n) - a, 0) / (tau * SR))
    e[:a] = 0.5 - 0.5 * np.cos(np.pi * np.arange(a) / a)
    return e


def clap_onsets(v, r):
    """The hands' onsets in samples (tighter on harder hits). Each sits on a whole number of periods of the tone's D5,
    so the tone's restrikes add in phase (the lock: at random spacings its level moved by 3 dB from take to take)."""
    spread = P.lerp(CLAP_SPREAD, v)
    period = SR / float(D.midi_hz(CLAP_TONE_NOTE))
    return [int(np.floor(round(at * spread * SR * (1 + 0.06 * r.uniform(-1, 1)) / period) * period + 0.5)) if k else 0
            for k, at in enumerate(CLAP_HANDS)]


def hands_layer(take, n, onsets, side):
    """The hands: each its own burst of band noise (CLAP_BAND, a presence peak) at exactly its CLAP_LEVELS level, so
    every take has the same shape and only its grain differs; then the last hand's tail. `side`: the side's own noise,
    so the clap is wide above 150 Hz and the mono sum keeps every hand."""
    v = take.vel
    (lo, hi, pk), tau = (P.lerp(b, v) for b in CLAP_BAND), P.lerp(HAND_TAU, v)
    m = int(0.012 * SR)
    y, env = np.zeros(n), np.zeros(n)
    for k, (a, lv) in enumerate(zip(onsets, CLAP_LEVELS)):
        e = hand(n - a, HAND_RISE if k == 0 else 0.0002, tau)
        x = D.peaking(P.band_noise(take.rng('clap', 'hand', k, side), n - a, lo, hi, order=4), pk, CLAP_PRESENCE_DB,
                      1.0) * e
        x *= lv / (np.sqrt(np.mean(x[:m] ** 2)) + 1e-12)
        y[a:] += x
        env[a:] += lv * e
    a = onsets[-1]
    level, tau_t = CLAP_TAIL[0], P.lerp(CLAP_TAIL[1], v)
    tail = D.peaking(P.band_noise(take.rng('clap', 'tail', side), n - a, lo, hi, order=4), pk, CLAP_PRESENCE_DB, 1.0)
    tail *= hand(n - a, 0.0008, tau_t)
    y[a:] += level * tail / (np.sqrt(np.mean(tail[:m] ** 2)) + 1e-12)
    env[a:] += level * hand(n - a, 0.0008, tau_t)
    return y, env


def clap(take):
    """Four hands of band noise (the first the loudest), a stick on the first, the glass tone on D5 struck in phase on
    every hand, a short glass body on G3 under them, air over 6 kHz on the hands, and a short bright room, then a 4x
    tanh for the crest. Harder hits: tighter hands, a brighter band, a sharper stick, more air and body."""
    v = take.vel
    r = take.rng('clap')
    n = int(0.5 * SR)
    t = P.times(n)
    onsets = clap_onsets(v, r)
    mid, env = hands_layer(take, n, onsets, 0)
    side = 0.45 * hands_layer(take, n, onsets, 1)[0]
    air = P.hp_noise(take.rng('clap', 'air'), n, 6000.0, order=2)
    air = D.bfilt(air, 11000.0 + 5000.0 * v, 'lowpass', order=2) * env * P.lerp(CLAP_AIR, v)
    (s_lo, s_hi), s_tau = CLAP_SNAP[1], CLAP_SNAP[2]
    stick = P.band_noise(take.rng('clap', 'snap'), n, s_lo, s_hi, order=2) * hand(n, HAND_RISE, s_tau)
    stick *= P.lerp(CLAP_SNAP[0], v)

    # the tone: one strike of glass modes (the same phases every take), restarted on every hand, softer each time
    m = int(0.08 * SR)
    f = float(D.midi_hz(CLAP_TONE_NOTE))
    decays = [0.020 / (1 + 0.35 * k) for k in range(len(RATIOS))]
    amps = P.hann_pulse_gain(f * RATIOS, P.lerp((0.0009, 0.0004), v)) / (1 + 0.4 * np.arange(len(RATIOS)))
    strike = P.modes(m, f, RATIOS, decays, amps, 0.004 * np.exp(-P.times(m) / 0.004),
                     D.rng(take.kit, take.item.name, 'tone')) * P.raised(m, 0.0003)
    tone = np.zeros(n)
    for k, a in enumerate(onsets):
        tone[a:a + m] += (1.0 - 0.1 * k) * strike[:n - a]
    tone *= P.lerp(CLAP_TONE, v)

    # the body: the bell's prime and quint on G3 (196 and 294 Hz), dropping onto pitch, and a breath of 160-420 Hz noise
    lv, (tau_b), bend0 = P.lerp(CLAP_BODY[0], v), P.lerp(CLAP_BODY[1], v), CLAP_BODY[2]
    fb = float(D.midi_hz(CLAP_BODY_NOTE))
    bend = bend0 * np.exp(-t / 0.006)
    body = (np.sin(2 * np.pi * D.phase(fb * (1 + bend), n)) +
            0.45 * np.sin(2 * np.pi * D.phase(fb * RATIOS[2] * (1 + bend), n) + 0.7)) * np.exp(-t / tau_b)
    body += 0.5 * P.band_noise(take.rng('clap', 'body'), n, 160.0, 420.0, order=2) * np.exp(-t / (0.6 * tau_b))
    body *= P.raised(n, 0.0006) * lv

    # the room: a short bright plate on the hands, after a pre-delay that leaves the hands their gaps
    rm = CLAP_ROOM
    pre = int(rm['pre'] * SR)
    send = D.bfilt(mid, rm['hp'], 'highpass', order=2)
    send = np.concatenate([np.zeros(pre), send[:n - pre]])
    room = D.fdn_reverb(send, K.space_rng('clap room'), t60=rm['t60'], size=rm['size'], damp_hz=rm['damp_hz'])[:n]

    c = mid + air + tone + stick + body
    y = np.stack([c + side, c - side], 1) + rm['level'] * room
    drive, into = CLAP_CLIP
    y = D.oversampled(lambda u: P.soft_clip(into * u, drive), y, 4) / into
    return P.low_clean(y, 150.0) * D.taper(n, *CLAP_FADE)[:, None] * P.vel_gain(v, 12.0)


# ------------------------------------------------------------------ hats: glass metal, closed and open
HAT_BASE = float(D.midi_hz(K.ROOT + 60))   # G7, 3136 Hz: the FM pairs sit on base x the kit's ratios
HAT_PAIRS = ((1, 4), (2, 5), (3, 6))   # (carrier, modulator) indices into RATIOS: carriers Bb7 D8 G8 (3.7, 4.7,
                                       # 6.3 kHz), modulators 7.5, 8.4 and 12.8 kHz
HAT_WIDTH = 0.6              # radians: each side's modulator phases start this far either way of the centre
CHOKE = {'group': 'hats', 'by': 'ClosedHat', 'fade_s': 0.004}   # a closed hat cuts an open one: the sampler's choke
HAT_FADE = (0.30, 0.15)      # the open hat is gone by 0.45 s, one loudness block
CLOSED_DECAY = (0.008, 0.006)    # seconds: a + b v
OPEN_DECAY = (0.075, 0.035)


def hat(take, decay):
    """The glass hat: three FM pairs on the kit's upper ratios (4x, the same modulator phases on every take), their pitch
    settling from up to 4 % sharp in 4 ms, over a little air, high-passed at 6 kHz and low-passed at 9-16 kHz with
    velocity. The width is each side's modulator phase, so the mono sum keeps every partial. Harder hits: more FM
    index, a higher start, a brighter top, a sharper tick and a slightly longer decay."""
    v = take.vel
    r = take.rng('hat')
    # the noise (air and tick) is the same on every round robin of a layer: 1 ms of noise alone moved the round robins'
    # level by 0.8 dB. A round robin differs in its pitch (+-0.3 %) and its modulator phases (+-0.5 rad)
    nz = D.rng(take.kit, take.item.name, take.layer, 'noise')
    n = int(0.5 * SR)
    k = 4
    tk = np.arange(n * k) / (SR * k)
    base = HAT_BASE * (1 + 0.003 * r.uniform(-1, 1))
    bend = 1 + (0.015 + 0.025 * v) * np.exp(-tk / 0.004)
    idx = (0.6 + 1.2 * v) + (0.8 + 2.0 * v) * np.exp(-tk / (0.003 + 0.004 * v))
    ph = D.rng(take.kit, 'glass hat', 'phases').uniform(0, 2 * np.pi, len(HAT_PAIRS)) + r.uniform(-0.5, 0.5, 3)
    sides = []
    for w in (-HAT_WIDTH, HAT_WIDTH):
        metal = 0.0
        for (a, b), p in zip(HAT_PAIRS, ph):
            ca = 2 * np.pi * np.cumsum(base * RATIOS[a] * bend) / (SR * k)
            cb = 2 * np.pi * np.cumsum(base * RATIOS[b] * bend) / (SR * k)
            metal = metal + np.sin(ca + idx * np.sin(cb + p + w))
        sides.append(D.down(metal, k) / len(HAT_PAIRS))
    air = P.hp_noise(nz, n, 8000.0, order=2)
    x = 0.8 * np.stack(sides, 1) + 0.22 * D.st(air)
    x = D.bfilt(D.bfilt(x, 6000.0, 'highpass', order=4), 9000.0 + 7000.0 * v, 'lowpass', order=4)
    tick = D.bfilt(nz.standard_normal(n), (9000.0, 16000.0), 'bandpass', order=2)
    tick *= D.env_exp(n, 0.0010, 0.0002)
    env = D.env_exp(n, P.lerp(decay, v), 0.0003)
    y = x * env[:, None] + (0.15 + 0.6 * v) * D.st(tick)
    return y * D.taper(n, *HAT_FADE)[:, None] * P.vel_gain(v, 13.0)


def closed_hat(take):
    return Out(hat(take, CLOSED_DECAY), meta={'choke': CHOKE})


def open_hat(take):
    return Out(hat(take, OPEN_DECAY), meta={'choke': CHOKE})


# ------------------------------------------------------------------ shaker: glass beads
SHAKER_LAYERS = (0.4, 0.6, 0.8, 1.0)
SHAKER_BASE = float(D.midi_hz(K.ROOT + 72))   # G8, 6272 Hz: each bead rings the glass bell's partials over it
SHAKER_MODES = 6             # the bell's first six partials over G8 (6.3-16.8 kHz); the seventh is past 20 kHz
SHAKER_BEADS = (70, 180)     # beads in one stroke, softest to hardest
SHAKER_RISE = (0.007, 0.0035)    # the stroke's swell, seconds: a hard stroke snaps in faster...
SHAKER_FALL = (0.016, 0.026)     # ...and falls longer (all of it inside a 16th, 117 ms at 128 BPM)
SHAKER_SHELL = (6200.0, 8800.0)  # the shell's resonance: the noise body's band-pass centre, soft to hard
SHAKER_SPREAD = 0.75         # each bead's pan, at most this far either way
SHAKER_FADE = (0.15, 0.08)   # gone by 0.23 s


def shaker(take):
    """One stroke of a shaker filled with glass beads: a cloud of tiny struck beads, each ringing a few of the kit's
    bell partials over G8 for under a millisecond, whose density follows the stroke's swell and fall, over a soft "sh"
    of noise through the shell's resonance. Each bead sits at its own place across the field, so the shaker is wide; the
    mono sum keeps every bead. Harder strokes: more beads, a faster swell, a longer fall, a brighter shell and more of
    the upper partials. The beads' count and level follow the envelope exactly, only their times and partials are
    random, so the round robins differ in grain, not in level."""
    v = take.vel
    r = take.rng('beads')
    n = int(0.25 * SR)
    t = P.times(n)
    rise, fall = P.lerp(SHAKER_RISE, v), P.lerp(SHAKER_FALL, v)
    env = (1 - np.exp(-t / rise)) * np.exp(-t / fall)
    env /= env.max()
    cdf = np.cumsum(env)
    cdf /= cdf[-1]
    nb = int(round(P.lerp(SHAKER_BEADS, v)))
    at = np.searchsorted(cdf, (np.arange(nb) + r.uniform(0.1, 0.9, nb)) / nb)
    m = int(0.004 * SR)
    tb = P.times(m)
    y = np.zeros((n + m, 2))
    tilt = 0.35 + 0.5 * v                      # the upper partials' share: brighter on harder strokes
    for a in at:
        f = SHAKER_BASE * RATIOS[:SHAKER_MODES] * (1 + 0.02 * r.uniform(-1, 1, SHAKER_MODES))
        amp = tilt ** np.arange(SHAKER_MODES) * r.uniform(0.5, 1.0, SHAKER_MODES)
        tau = r.uniform(0.00025, 0.0007, SHAKER_MODES)
        bead = np.sum(amp[:, None] * np.exp(-tb[None, :] / tau[:, None])
                      * np.sin(2 * np.pi * f[:, None] * tb[None, :] + r.uniform(0, 2 * np.pi, SHAKER_MODES)[:, None]),
                      0) * env[a]
        p = SHAKER_SPREAD * r.uniform(-1, 1)
        y[a:a + m] += bead[:, None] * np.array([np.cos((p + 1) * np.pi / 4), np.sin((p + 1) * np.pi / 4)])
    y = y[:n] * np.sqrt(2) / np.sqrt(SHAKER_BEADS[1])
    shell = P.lerp(SHAKER_SHELL, v)
    sh = [D.peaking(P.hp_noise(take.rng('sh', c), n, 4000.0, order=2), shell, 6.0, 1.2) for c in range(2)]
    common = P.hp_noise(take.rng('sh', 'common'), n, 4000.0, order=2)
    body = np.stack([0.6 * D.peaking(common, shell, 6.0, 1.2) + 0.4 * sh[0],
                     0.6 * D.peaking(common, shell, 6.0, 1.2) + 0.4 * sh[1]], 1) * env[:, None]
    x = 0.9 * y + (0.16 + 0.08 * v) * body
    x = D.bfilt(D.bfilt(x, 3500.0, 'highpass', order=4), 15000.0 + 3000.0 * v, 'lowpass', order=2)
    return x * D.taper(n, *SHAKER_FADE)[:, None] * P.vel_gain(v, 10.0, SHAKER_LAYERS[0])


# ------------------------------------------------------------------ the 2-bar hat preview loop
HAT_STEPS = 32
LET_RING = (3, 7)            # beats (0-7) whose open hat rings to the next beat: no closed hat after it


def hat_pattern():
    """(16th, 'closed' or 'open', velocity): per beat, a closed hat on the beat, a ghost on the 'e', the open hat on the
    offbeat and a closed one on the 'a' that chokes it; the beats in LET_RING skip that last one."""
    hits = []
    for b in range(HAT_STEPS // 4):
        s = 4 * b
        hits += [(s, 'closed', 0.6 if b % 2 == 0 else 0.5), (s + 1, 'closed', 0.3), (s + 2, 'open', 0.8)]
        if b not in LET_RING:
            hits.append((s + 3, 'closed', 0.7 if b % 2 else 0.55))
    return tuple(hits)


HAT_HITS = hat_pattern()


def hat_loop(take):
    """The pattern on the kit's grid, played on the hats' files as written: each hit takes the layer nearest its
    velocity, round robins in turn, each open hat faded out over 4 ms at the next closed hat (the choke); tails wrap
    round the loop."""
    G = Ph.Grid(K, HAT_STEPS)
    L = G.length
    y = np.zeros((L, 2))
    fade_n = int(CHOKE['fade_s'] * SR)
    closed_at = sorted(G.at(s) for s, kind, _ in HAT_HITS if kind == 'closed')
    count = {'closed': 0, 'open': 0}
    for step, kind, vel in HAT_HITS:
        name = 'ClosedHat' if kind == 'closed' else 'OpenHat'
        item, files, _ = Ph.kit_files(K, name)
        layer = 1 + min(range(len(item.layers)), key=lambda i: (abs(item.layers[i] - vel), -i))
        x = np.array(files[(None, layer, 1 + count[kind] % item.rr)].audio)
        count[kind] += 1
        pos = G.at(step)
        if kind == 'open':
            nxt = min((c for c in closed_at if c > pos), default=closed_at[0] + L) - pos
            g = np.ones(len(x))
            g[nxt:nxt + fade_n] = np.cos(0.5 * np.pi * np.arange(len(g[nxt:nxt + fade_n])) / fade_n) ** 2
            g[nxt + fade_n:] = 0.0
            x = x * g[:, None]
        Ph.place(y, x, pos)
    return Out(y, meta={'steps': HAT_STEPS, 'bpm': K.BPM, 'swing': K.SWING, 'choke': CHOKE,
                        'hits': [[s, k, v] for s, k, v in HAT_HITS]})


ITEMS = [
    Item('Drums', 'KickSplit', 'kick', kick_split, layers=LAYERS, rr=2, root=K.KICK_NOTE, sets=('r2', 'r3'),
         about='split kick: a clean short G1 sub (gone by 0.32 s), a gently saturated knock with a longer glide '
               'through 100-150 Hz, gone by 80 ms, and a clear driven glass click on D7 above 1 kHz'),
    Item('Drums', 'Clap', 'snare', clap, layers=LAYERS, rr=2, sets=('r2', 'r3'),
         about='clap: four hands within 30 ms, the first the loudest, a stick on it, the glass tone on D5 struck in '
               'phase on every hand, a glass body on G3, air, a short bright room'),
    Item('Drums', 'ClosedHat', 'hat', closed_hat, layers=LAYERS, rr=3, sets=('r2', 'r3'),
         about='closed hat: three FM pairs on the glass ratios over G7 at 4x, carriers on Bb7 D8 G8, 30 dB down '
               'inside a 16th; chokes the open hat'),
    Item('Drums', 'OpenHat', 'hat', open_hat, layers=LAYERS, rr=3, sets=('r2', 'r3'),
         about='open hat: the same glass metal as an offbeat sizzle about an 8th long, choked by the closed hat'),
    Item('Perc', 'Shaker', 'hat', shaker, layers=SHAKER_LAYERS, rr=4, sets=('r3',),
         about='glass-bead shaker for swung 16ths: a cloud of tiny beads ringing the bell partials over G8, a soft '
               '"sh" through the shell, wide, gone inside a 16th'),
    Item('Loops', 'HatLoop', 'loop', hat_loop, loop=True, sets=('r2', 'r3'), tags=('preview',),
         qa={'mud': 'lowcut', 'why': 'a hat loop: the low cut, like the hats'},
         about='2-bar hat preview at 128 BPM on the kit swing: closed 16ths round an open hat on every offbeat, '
               'choked by the next closed hat'),
]
