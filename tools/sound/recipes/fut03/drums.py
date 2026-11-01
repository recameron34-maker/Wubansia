"""FUT03 drums for round 2: the split kick, the phase-locked clap, the closed and open hats, and their 2-bar preview loop.

Hyper-clean future house at 128 BPM in G minor. Every drum has 5 velocity layers and 2 round robins (the hats 3).
Velocity moves the brightness, the transient and the pitch envelope, not just the level.

- KickSplit: round 1's pick, the split design (FUT01's A, Cool 4), rebuilt for this kit. A clean sine sub on the key's
  root, G1 (49 Hz), short: held for about 100 ms and gone by 0.32 s, so it clears before the offbeat bass. Above
  110 Hz a knock that is only gently saturated (a tanh at 1.2-2.4, no fold, no clip) and gone by 80 ms. On top a clear
  glass click: an FM blip on G7, a tick of noise and a ring on the kit's glass ratios from G6, the most driven part (a
  biased tanh at 2-6 and a soft fold, at 8x), then nothing under 1 kHz. Round 1 heard kick B (Tight) as "really
  slamming the limiter": here the sub is never driven and the knock only gently, and the top layer's crest stays in
  Split's range (true peak over the loudest 50 ms about 6 dB; Split 5.8-6.4, Tight 7.9-8.9, the floor 4.0).
- Clap: three layers fired from one envelope: a mid snap of band noise, an air crack over 6 kHz, and a tone of glass
  modes on D5 (D, F and A: the bell's minor triad, in the key) that restarts in phase on every burst. The layers share
  the three bursts and the tail sample for sample, so nothing smears or combs between them: a snappy, clean clap with a
  short room above 1.5 kHz. Harder hits: tighter bursts, a brighter snap, more crack and more tone.
- ClosedHat and OpenHat: the same glass metal, three FM pairs on the kit's ratios over G7 (rendered at 4x, so no
  sideband folds back) with a little air, its pitch settling from sharp. The carriers sit on the bell's tierce, quint
  and nominal over G7, so they ring Bb7, D8 and G8: a G minor triad, in the key like the plings and the kick's click.
  The closed hat is 30 dB down inside a 16th (117 ms at 128 BPM); the open one sizzles about an 8th. They share a
  choke group: a closed hat cuts the open one (meta 'choke', a 4 ms fade, as the sampler will).
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
SUB_SHAPE = (0.118, 0.020, 1.7)   # the sub's hold-then-drop: exp(-(t / (a + b v)) ** c)
SUB_FADE = (0.16, 0.16)      # and a raised-cosine fade: exactly zero from 0.32 s (a longer fade, so the last 10 ms
                             # of the file are well under -70 dBFS)
KNOCK_DRIVE = (1.2, 2.4)     # tanh drive, softest to hardest: gentle, so the knock stays clean (no fold, no clip)
KNOCK_MIX = (0.50, 0.95)     # the knock's level, softest to hardest
KNOCK_DIP = (260.0, -9.0, 0.8)   # a dip in the knock's low mids: Hz, dB, Q
CLICK_NOTE = K.KICK_NOTE + 72   # G7, 3136 Hz: the FM blip's carrier
RING_NOTE = K.KICK_NOTE + 60    # G6, 1568 Hz: the glass ring
TOP_MIX = (0.10, 0.34)       # the top's level, softest to hardest
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
    s = np.sin(P.sweep(n, f_hi, tau1, 0.013, 0.24, F_KICK))

    # 1. sub: clean, short
    a, b, c = SUB_SHAPE
    a_sub = np.exp(-(t / (a + b * v)) ** c) * D.taper(n, *SUB_FADE)
    sub = D.lr4(s * a_sub, XOVER, 'lowpass')

    # 2. knock: the same phase, a gentle tanh at 4x, high-passed, dipped at 260 Hz, gone by 80 ms
    drive = P.lerp(KNOCK_DRIVE, v)
    kd = D.oversampled(lambda u: D.tanh_adaa(u, drive), s * np.exp(-t / 0.045), 4)
    a_k = P.gate(t, 0.008 + 0.004 * v, 0.016 + 0.005 * v) * D.taper(n, 0.040, 0.040)
    knock = D.peaking(D.lr4(kd * a_k, XOVER, 'highpass'), *KNOCK_DIP)
    knock = D.bfilt(knock, 4500.0, 'lowpass')

    # 3. the glass click
    top = kick_top(take, n)

    y = sub + P.lerp(KNOCK_MIX, v) * knock + P.lerp(TOP_MIX, v) * top
    return y * P.vel_gain(v, 12.0)


# ------------------------------------------------------------------ clap: three phase-locked layers
CLAP_BURSTS = (0.0, 0.0058, 0.0121)   # burst onsets (s) at velocity 0.6; the last carries the tail
CLAP_TONE_NOTE = 74                   # D5, 587.33 Hz: its glass modes ring D, F and A, in the key
CLAP_FADE = (0.30, 0.15)              # gone by 0.45 s: a file under 0.5 s is one 400 ms loudness block (ADR 0009)
CLAP_MIX = (1.0, 0.45, 0.55, 0.16)    # snap, air, tone, room


def clap_env(n, v, r):
    """The one envelope all three layers share: three bursts (tighter on harder hits), the last carrying the tail; and
    each burst's onset in samples. The onsets sit on whole periods of the tone's D5, so its restrikes add in phase (the
    lock: at random spacings the tone's level moved by 3 dB from take to take)."""
    spread = P.lerp((1.3, 0.8), v)
    period = SR / float(D.midi_hz(CLAP_TONE_NOTE))
    env = np.zeros(n)
    onsets = []
    for k, at in enumerate(CLAP_BURSTS):
        a = int(np.floor(round(at * spread * SR * (1 + 0.06 * r.uniform(-1, 1)) / period) * period + 0.5)) if k else 0
        env[a:] += (0.75 + 0.12 * k) * D.env_exp(n - a, 0.0028, 0.0002)
        onsets.append(a)
    env[onsets[-1]:] += 0.45 * D.env_exp(n - onsets[-1], 0.045, 0.0008)
    return env, onsets


def clap(take):
    """Snap (band noise from 0.9-3 kHz to 1.2-4.5 kHz with velocity, a presence peak at 1.4-2 kHz), air (noise over 6 kHz) and tone (glass modes on D5,
    struck again in phase at every burst) on one envelope, plus a short room on the snap. The snap's side is its own
    noise, so the clap is wide above 150 Hz and the mono sum keeps every layer."""
    v = take.vel
    r = take.rng('clap')
    n = int(0.5 * SR)
    env, onsets = clap_env(n, v, r)
    hi = 3000.0 * (1 + 0.5 * v)

    def snap(rr):
        x = P.band_noise(rr, n, P.lerp((850.0, 1150.0), v), hi, order=2)
        return D.peaking(x, P.lerp((1400.0, 2000.0), v), 3.0, 1.0)
    mid = snap(r) * env
    side = 0.45 * snap(take.rng('clap', 'side')) * env
    air = P.hp_noise(take.rng('clap', 'air'), n, 6000.0, order=2)
    air = D.bfilt(air, 11000.0 + 5000.0 * v, 'lowpass', order=2) * env * P.lerp((0.5, 1.4), v)

    # tone: one strike of glass modes (the same phases every take), restarted at every burst onset
    m = int(0.08 * SR)
    f = float(D.midi_hz(CLAP_TONE_NOTE))
    decays = [0.020 / (1 + 0.35 * k) for k in range(len(RATIOS))]
    amps = P.hann_pulse_gain(f * RATIOS, P.lerp((0.0009, 0.0004), v)) / (1 + 0.4 * np.arange(len(RATIOS)))
    strike = P.modes(m, f, RATIOS, decays, amps, 0.004 * np.exp(-P.times(m) / 0.004),
                     D.rng(take.kit, take.item.name, 'tone')) * P.raised(m, 0.0003)
    tone = np.zeros(n)
    for k, a in enumerate(onsets):
        tone[a:a + m] += (0.7 + 0.15 * k) * strike[:n - a]
    tone *= P.lerp((1.0, 0.7), v)                   # the tone thins as the snap and the air take over on hard hits

    send = D.bfilt(mid, 1500.0, 'highpass', order=4)
    room = D.fdn_reverb(send, K.space_rng('clap room'), t60=0.40, size=0.30, damp_hz=8000.0)[:n]

    g_snap, g_air, g_tone, g_room = CLAP_MIX
    c = g_snap * mid + g_air * air + g_tone * tone
    y = np.stack([c + g_snap * side, c - g_snap * side], 1) + g_room * room
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
    Item('Drums', 'KickSplit', 'kick', kick_split, layers=LAYERS, rr=2, root=K.KICK_NOTE, sets=('r2',),
         about='split kick: a clean short G1 sub (gone by 0.32 s), a gently saturated knock gone by 80 ms, a driven '
               'glass click above 1 kHz'),
    Item('Drums', 'Clap', 'snare', clap, layers=LAYERS, rr=2, sets=('r2',),
         about='clap: three phase-locked layers on one envelope (snap, air, glass tone on D5), three tight bursts, a '
               'short room above 1.5 kHz'),
    Item('Drums', 'ClosedHat', 'hat', closed_hat, layers=LAYERS, rr=3, sets=('r2',),
         about='closed hat: three FM pairs on the glass ratios over G7 at 4x, carriers on Bb7 D8 G8, 30 dB down '
               'inside a 16th; chokes the open hat'),
    Item('Drums', 'OpenHat', 'hat', open_hat, layers=LAYERS, rr=3, sets=('r2',),
         about='open hat: the same glass metal as an offbeat sizzle about an 8th long, choked by the closed hat'),
    Item('Loops', 'HatLoop', 'loop', hat_loop, loop=True, sets=('r2',), tags=('preview',),
         qa={'mud': 'lowcut', 'why': 'a hat loop: the low cut, like the hats'},
         about='2-bar hat preview at 128 BPM on the kit swing: closed 16ths round an open hat on every offbeat, '
               'choked by the next closed hat'),
]
