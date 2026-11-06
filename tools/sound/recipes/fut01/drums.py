"""FUT01 drums: three kicks, three snares and two closed hats to A/B, a shaker, and the alloy hit on F#5.

Round 2 (set r2) kept every round 1 drum except kick B: round 1 picked A, Split ("B sounds a bit hard like its really
slamming the limiter"), so KickTight stays in the s1a set only, as round 1's history. Round 3 (set r3, the modern pass,
docs/plans/S1-modern-sound.md) keeps all of round 2's drums byte for byte and adds three for current UK garage:
- KickPunch, kick B: Split with a shorter sub (-20 dB by about 220 ms, two 16ths at 134 BPM; Split reads 317 ms) and
  a knock at 100-150 Hz over its first 50 ms, the standard's 2-step targets. Kick A is the one round 1 rated Cool 4,
  so this is an A/B, not a replacement.
- SnareClap, snare C: a layered garage snare-clap: snare A's crack, three tight hands and a short room. Snare A sets
  the round 2 loop's peak, 1.4 dB over a kick; the loop sets snare C so its peaks sit level with the kick's.
- Shaker: the swung 16th top layer the standard asks for (round 2's hats sit 11 LU under the kick).

Every drum has 5 velocity layers (the alloy hit and the shaker 4). Velocity moves the brightness, the transient and the
pitch envelope, not just the level. The kicks pass the mud gate (plan section 5.2): after ~100 ms only a clean sine on
F#1 is left, and the driven layers are gated short and kept out of 150-400 Hz. The snare bodies follow the same rule in
the low mids, and everything else here is high-passed far above the kick and sub.

Provenance: KickSplit is ported from the kick de-mud study's "split" kick (kick_split.py, 2026-10-29) with the judge's
two fixes: one gain over every layer and round robin (the renderer's shared gain; the recipe no longer normalises a
part per take), and a shorter sub glide that sits on F#1 by 100 ms. Its pieces (split_sweep, split_knock, split_top)
are shared with KickPunch; they render Split's bytes unchanged (tests/masters.sha256). KickTight is ported from the same
study's "tight" kick (kick_tight.py). ClosedHatFM and AlloyHit grow out of the S1 research prototype (recipes.py: hat,
alloy_hit), with their tails rendered out in full instead of cut by the fade.
"""
import numpy as np

from tools.sound import dsp48 as D
from tools.sound import parts as P
from tools.sound.item import Item
from tools.sound.kits import fut01 as K

SR = D.SR
LAYERS = (0.2, 0.4, 0.6, 0.8, 1.0)
F_KICK = float(D.midi_hz(K.KICK_NOTE))  # F#1, 46.25 Hz
ALLOY = np.array(K.ALLOY)


# ------------------------------------------------------------------ kick A: split
SPLIT_XOVER = 110.0          # sub / knock crossover (LR4, sums flat)
RING_SCALE = 0.23            # the top's ring peaks near 4.3 unscaled; the study normalised it per take instead


def split_top(v, r, n):
    """Split's top: an alloy FM click, a noise burst and a metal ring on F#6, through the heavy chain at 8x, then
    nothing under 1 kHz; (n,), its first 0.25 s sounding. Draws from r in a fixed order: the noise, the ring's pitch,
    its strike, its modes."""
    nt = int(0.25 * SR)
    tt = P.times(nt)
    fcar = 2900.0
    idx = (2.0 + 5.0 * v) * np.exp(-tt / 0.0015)
    fm = np.sin(2 * np.pi * fcar * tt + idx * np.sin(2 * np.pi * fcar * ALLOY[2] * tt))
    fm *= np.exp(-tt / (0.0015 + 0.0015 * v)) * D.taper(nt, 0.008, 0.004)
    nb = D.bfilt(r.uniform(-1, 1, nt), 3000.0, 'highpass') * D.env_exp(nt, 0.0010 + 0.0010 * v, 0.0002)
    f_ring = float(D.midi_hz(K.KICK_NOTE + 60)) * (1 + 0.002 * r.uniform(-1, 1))
    decays = [(0.020 + 0.025 * v) / (1 + 0.45 * k) for k in range(len(ALLOY))]
    amps = [1.0 / (1 + 0.7 * k) for k in range(len(ALLOY))]
    w = int(0.0015 * SR)
    ring = np.convolve(r.uniform(-1, 1, w) * np.hanning(w), D.modal_ir(f_ring, ALLOY, decays, amps, nt, r))[:nt]
    ring *= RING_SCALE
    x = (0.45 + 0.55 * v) * fm + (0.25 + 0.45 * v) * nb + (0.30 + 0.35 * v) * ring
    drive_t = 3.0 + 6.0 * v

    def chain(u):
        u = D.tanh_adaa(u, drive_t, bias=0.3)
        u = D.sinefold_adaa(u, 1.0 + 0.7 * v)
        return D.hardclip_adaa(u, 1.3)
    top = D.oversampled(chain, x, 8)
    top *= np.exp(-tt / (0.025 + 0.020 * v)) * D.taper(nt, 0.09, 0.06)
    top = D.bfilt(D.lr4(top, 1000.0, 'highpass'), 5500.0 + 8500.0 * v, 'lowpass')
    top_full = np.zeros(n)
    top_full[:nt] = top
    return top_full


def split_knock(s, t, v, n):
    """Split's knock: the sweep s, driven at 4x (biased tanh, then a gentle fold), high-passed at the crossover, dipped
    at 255 Hz, gone by 95 ms."""
    drive, fold = 2.0 + 4.0 * v, 1.0 + 0.5 * v
    kd = D.oversampled(lambda u: D.sinefold_adaa(D.tanh_adaa(u, drive, bias=0.12), fold), s * np.exp(-t / 0.05), 4)
    a_k = P.gate(t, 0.010 + 0.005 * v, 0.022 + 0.006 * v) * D.taper(n, 0.045, 0.050)
    knock = D.peaking(D.lr4(kd * a_k, SPLIT_XOVER, 'highpass'), 255.0, -10.0, 0.75)
    return D.bfilt(knock, 5000.0, 'lowpass')


def split_sweep(v, r, n):
    """Split's pitch drop: harder hits start higher and snap faster; the round robin (r) moves the start and the snap,
    never the end. The glide (16 ms) puts the sub on F#1 by 100 ms. Returns the phase in radians."""
    f_hi = (280.0 + 320.0 * v) * 2 ** (r.normal(0, 10) / 1200)
    tau1 = (0.0085 - 0.002 * v) * (1 + 0.04 * r.uniform(-1, 1))
    return P.sweep(n, f_hi, tau1, 0.016, 0.28, F_KICK)


def kick_split(take):
    """Three layers shaped apart, so the drive never sits on the low mids: a clean sub on the sweep, a driven knock
    above 110 Hz that is gone by 95 ms, and an alloy top (FM click, noise, a ring on the alloy ratios) whose heavy
    distortion is high-passed at 1 kHz."""
    v = take.vel
    r = take.rng('split')
    n = int(0.7 * SR)
    t = P.times(n)
    s = np.sin(split_sweep(v, r, n))
    # 1. sub: clean, hold-then-drop envelope, exactly zero from 0.45 s
    a_sub = np.exp(-(t / (0.165 + 0.025 * v)) ** 1.8) * D.taper(n, 0.34, 0.11)
    sub = D.lr4(s * a_sub, SPLIT_XOVER, 'lowpass')
    # 2. knock, gone by 95 ms; 3. the alloy top above 1 kHz
    knock = split_knock(s, t, v, n)
    y = sub + (0.55 + 0.25 * v) * knock + (0.11 + 0.165 * v) * split_top(v, r, n)
    return y * P.vel_gain(v, 12.0)


# ------------------------------------------------------------------ kick B (round 3): punch
# Split, retimed for 2-step and given a knock: the sub holds flatter and drops sooner (-20 dB at about 220 ms, two
# 16ths at 134 BPM, where Split reads 317 ms: the modern-sound standard's UKG target), and a knock at 100-150 Hz fills
# the first 50 ms. The knock is the sweep's own second harmonic, so it is phase-locked to the sub: between 20 and 60 ms
# the sweep runs 90-50 Hz, so its second harmonic traces 180-100 Hz. Where the knock sounds, the sub gives way a little
# (PUNCH_DUCK), so the sum never peaks over the attack: on every layer but the softest (as on Split) the peak is the
# click and the top's ring, in the first 12 ms. The top and Split's own knock are Split's.
PUNCH_SUB = dict(tau=(0.150, 0.162), power=3.0, taper=(0.24, 0.08), level=1.08)   # exp(-(t / tau)^power), tau soft
                                                                                  # to hard (s)
PUNCH_KNOCK = dict(level=0.55, on=0.008, rise=0.010, hold=0.040, tau=0.012, band=(85.0, 170.0))
PUNCH_DUCK = 0.35            # the sub's dip under the knock (share of the sub): the knock never tops the attack
PUNCH_TOP = 1.10             # the top against Split's: 0.8 dB more click


def kick_punch(take):
    """Kick B for round 3: Split's three layers (a clean F#1 sub, a driven knock above 110 Hz gone by 95 ms, the alloy
    top above 1 kHz) with a shorter sub and a phase-locked 100-150 Hz knock over its first 50 ms."""
    v = take.vel
    r = take.rng('punch')
    n = int(0.7 * SR)
    t = P.times(n)
    ph = split_sweep(v, r, n)
    s = np.sin(ph)
    k = PUNCH_KNOCK
    rise = np.clip((t - k['on']) / k['rise'], 0.0, 1.0)
    knock_env = (0.5 - 0.5 * np.cos(np.pi * rise)) * P.gate(t, k['hold'], k['tau']) * D.taper(n, 0.06, 0.04)
    a_sub = np.exp(-(t / P.lerp(PUNCH_SUB['tau'], v)) ** PUNCH_SUB['power']) * D.taper(n, *PUNCH_SUB['taper'])
    sub = PUNCH_SUB['level'] * D.lr4(s * a_sub * (1.0 - PUNCH_DUCK * knock_env), SPLIT_XOVER, 'lowpass')
    h2 = D.bfilt(D.bfilt(np.sin(2 * ph) * knock_env, k['band'][0], 'highpass', order=2), k['band'][1], 'lowpass',
                 order=4)
    y = sub + (0.55 + 0.25 * v) * split_knock(s, t, v, n) + k['level'] * (0.4 + 0.6 * v) * h2 \
        + PUNCH_TOP * (0.11 + 0.165 * v) * split_top(v, r, n)
    return y * P.vel_gain(v, 12.0)


# ------------------------------------------------------------------ kick B: tight
TIGHT_XOVER = 90.0           # clean sine below, distorted body above (LR4)
TIGHT_RELEASE = (0.10, 0.18)  # the sub's raised-cosine release, start and length (s); was (0.19, 0.26), longer than A


def kick_tight(take):
    """One driven body whose drive falls with time and whose low-mid part (90 Hz-1 kHz) is cut at 250 Hz and gated
    short; only a clean F#1 sine carries the tail, and it is short: released from 0.10 s, gone by 0.28 s (about -20 dB
    at 220 ms), so it is the tight, punchy choice against Split's long round sub, and clears before a 2-step skip kick
    336 ms later."""
    v = take.vel
    r = take.rng('tight')
    n = int(0.9 * SR)
    t = P.times(n)
    f_hi = P.lerp((420.0, 760.0), v) * 2 ** (r.normal(0, 12) / 1200)       # round robin: the start only
    tau2 = P.lerp((0.022, 0.016), v) * (1 + 0.03 * r.uniform(-1, 1))
    osc = np.sin(P.sweep(n, f_hi, P.lerp((0.0040, 0.0035), v), tau2, 0.15, F_KICK))

    # alloy transient: FM click (2900 Hz carrier, modulator on ALLOY[2]) and a 1.5 ms noise burst
    tc = P.times(int(0.012 * SR))
    fm = np.sin(2 * np.pi * 2900 * tc + (2 + 5 * v) * np.exp(-tc / 0.0015) * np.sin(2 * np.pi * 2900 * ALLOY[2] * tc))
    click = np.zeros(n)
    click[:len(tc)] = fm * np.exp(-tc / 0.002)
    nb = D.bfilt(r.uniform(-1, 1, n), 3000, 'highpass') * D.env_exp(n, 0.0015)

    # the body: a short pre-envelope, then a drive that holds and falls back to 1 (8x, biased tanh -> fold -> clip)
    x = np.exp(-t / P.lerp((0.060, 0.100), v)) * osc + (0.22 + 0.45 * v ** 1.5) * (0.7 * click + 0.5 * nb)
    d0, hold_d, tau_d = P.lerp((2.0, 9.0), v), P.lerp((0.020, 0.040), v), P.lerp((0.025, 0.035), v)
    fold = P.lerp((1.0, 1.6), v)

    def chain(u):
        drv = 1.0 + (d0 - 1.0) * P.gate(np.arange(len(u)) / (SR * 8), hold_d, tau_d)
        u = D.tanh_adaa(drv * u, 1.0, bias=0.25) * np.tanh(1.0) / np.tanh(drv)   # biased tanh, drive per sample
        return D.hardclip_adaa(D.sinefold_adaa(u, fold), 1.4)
    d = D.lr4(D.oversampled(chain, x, 8), TIGHT_XOVER, 'highpass')
    d_lo = D.peaking(D.lr4(d, 1000.0, 'lowpass'), 250.0, -7.0, 0.7)
    d_hi = D.lr4(d, 1000.0, 'highpass')
    d = d_lo * P.gate(t, P.lerp((0.012, 0.018), v), P.lerp((0.012, 0.014), v)) \
        + d_hi * P.gate(t, P.lerp((0.025, 0.045), v), P.lerp((0.024, 0.030), v))

    # the clean F#1 sine: slow fall, raised-cosine release from 0.10 s to 0.28 s
    rel = np.clip((t - TIGHT_RELEASE[0]) / TIGHT_RELEASE[1], 0, 1)
    s = D.lr4(0.9 * osc * np.exp(-t / (0.30 + 0.04 * v)) * (0.5 + 0.5 * np.cos(np.pi * rel)), TIGHT_XOVER, 'lowpass')

    y = D.bfilt(d + s, P.lerp((5200.0, 12000.0), v), 'lowpass')
    return y * P.vel_gain(v, 12.0)


# ------------------------------------------------------------------ snare A: FM metal
SNARE_BODY = float(D.midi_hz(61))       # C#4, 277.18 Hz: the key's fifth
SNARE_RING = float(D.midi_hz(90))       # F#6, 1479.98 Hz: the alloy ring starts above the box band
SNARE_FADE = (0.32, 0.15)               # the tail is 35-50 dB down at 0.32 s and gone by 0.47 s: a file under 0.5 s
                                        # is one 400 ms loudness block, so LUFS-I can't jump at the relative gate
SNARE_A_MIX = (0.55, 0.30, 0.10, 0.18)  # body, wires, wire side, ring (was 0.35 / 0.10: hiss-led, a 17 dB crest)
WIRE_DRIVE = 0.6                        # the wires' soft clip: takes the Gaussian noise's spikes off (about 3.5 dB of
                                        # crest), so the snare's level comes from body and crack, not from peaks
SNARE_A_BODY_VEL = (0.45, 0.55)         # body level: a + b v (was 0.3 + 0.7 v, which made hard hits darker)
SNARE_A_WIRE_VEL = (0.5, 0.5)           # wire level: a + b v
SNARE_A_CLIP = 2.5                      # a soft clip on the sum: shaves the hit's first 3 ms, most on hard hits, so
                                        # the crest (true peak over the loudest 400 ms) drops from 17 to about 13 dB


def metal_crack(take, n, t):
    """Snare A before its clip, (n, 2): the FM body, the wires and crack, the alloy ring, on the take's own streams."""
    v = take.vel
    r = take.rng('metal')

    # body: pitch drop and FM index follow velocity; it holds 2 ms, then falls fast so the low mids clear early
    f = SNARE_BODY * (1 + (0.10 + 0.40 * v) * np.exp(-t / 0.006))
    idx = (0.6 + 2.2 * v) * np.exp(-t / (0.010 + 0.008 * v))
    ph = 2 * np.pi * D.phase(f, n)
    body = np.sin(ph + idx * np.sin(ALLOY[3] * ph)) * P.gate(t, 0.002, 0.026 + 0.006 * v) * D.env_exp(n, 1.0, 0.0005)

    # wires: noise over 2 kHz with a presence peak, brighter with velocity, soft-clipped (4x); a short crack on top
    def wires(rr):
        x = D.peaking(P.hp_noise(rr, n, 2000.0), 5200.0, 4.0, 0.9)
        x = D.bfilt(x, 6000.0 + 6000.0 * v, 'lowpass', order=2)
        return D.oversampled(lambda u: P.soft_clip(u, WIRE_DRIVE), x)
    tail = D.env_exp(n, 0.050 + 0.030 * v, 0.0008)
    crack = (0.3 + 0.9 * v) * D.env_exp(n, 0.0035, 0.0003)

    # ring: alloy modes struck by a Hann pulse (shorter, so brighter, on harder hits), the same phases on each side and
    # 0.1% apart, so the ring widens slowly as it decays
    fk = SNARE_RING * ALLOY
    amps = P.hann_pulse_gain(fk, P.lerp((0.0005, 0.00018), v)) / (1 + 0.4 * np.arange(len(ALLOY)))
    decays = [(0.040 + 0.030 * v) / (1 + 0.2 * k) for k in range(len(ALLOY))]
    bend = (0.002 + 0.006 * v) * np.exp(-t / 0.010)
    ring = [P.modes(n, SNARE_RING * c, ALLOY, decays, amps, bend, take.rng('metal', 'ring')) for c in (0.9995, 1.0005)]

    g_wire = SNARE_A_MIX[1] * P.lerp(SNARE_A_WIRE_VEL, v)
    mid = SNARE_A_MIX[0] * P.lerp(SNARE_A_BODY_VEL, v) * body + g_wire * wires(r) * (tail + crack)
    side = SNARE_A_MIX[2] * P.lerp(SNARE_A_WIRE_VEL, v) * wires(take.rng('metal', 'side')) * tail
    g_ring = SNARE_A_MIX[3] * (0.4 + 0.6 * v)
    return np.stack([mid + side + g_ring * ring[0], mid - side + g_ring * ring[1]], 1)


def snare_metal(take):
    """An FM body on C#4 (modulator on the alloy ratio 2.546) that drops into pitch, shaped and soft-clipped
    snare-wire noise, and a ring of alloy modes from F#6 up (the 1.5-3 kHz crack). Body and ring lead, the wires sit
    under them. Harder hits drop further, carry more FM, a louder crack, brighter wires and a ring struck higher up."""
    v = take.vel
    n = int(1.4 * SR)
    t = P.times(n)
    y = D.oversampled(lambda u: P.soft_clip(u, SNARE_A_CLIP), metal_crack(take, n, t))
    return P.low_clean(y) * D.taper(n, *SNARE_FADE)[:, None] * P.vel_gain(v, 12.0)


# ------------------------------------------------------------------ snare B: modal metal + clap
SNARE_B_BODY = float(D.midi_hz(62))     # D4, 293.66 Hz: only its first mode sits in the low mids
CLAP_FLAMS = (0.0, 0.0072, 0.0151, 0.0238)  # seconds, at velocity 0.6
SNARE_B_MIX = (0.20, 0.40, 0.12)        # body, clap, room


def snare_modal(take):
    """A struck metal body (modes on the alloy ratios from D4, all short) and a clap-like burst of four flams with a
    short tail in the kit's room. Harder hits strike shorter (more upper modes), bend the modes further, tighten the
    flams and brighten the clap."""
    v = take.vel
    r = take.rng('modal')
    n = int(1.4 * SR)
    t = P.times(n)

    # body: a Hann strike of 1.4 ms (soft) to 0.4 ms (hard) through the modes; the modes start sharp and settle
    fk = SNARE_B_BODY * ALLOY
    amps = P.hann_pulse_gain(fk, P.lerp((0.0014, 0.0004), v)) / (1 + 0.3 * np.arange(len(ALLOY)))
    amps[0] *= 0.6                                              # the one mode in the low mids
    decays = [0.024] * len(ALLOY)
    bend = (0.02 + 0.06 * v) * np.exp(-t / 0.008)
    body = P.modes(n, SNARE_B_BODY, ALLOY, decays, amps, bend, r) * D.env_exp(n, 1.0, 0.0004)

    # clap: four flams of band noise, the last one carrying the tail
    spread = P.lerp((1.25, 0.85), v)
    lo, hi = 1200.0 * (1 + 0.2 * v), 3600.0 * (1 + 0.3 * v)
    nz = [P.band_noise(take.rng('modal', 'clap', ch), n, lo, hi) for ch in (0, 1)]
    env = np.zeros(n)
    for k, at in enumerate(CLAP_FLAMS):
        a = int(at * spread * SR * (1 + 0.05 * r.uniform(-1, 1)))
        env[a:] += (0.7 + 0.1 * k) * D.env_exp(n - a, 0.0035, 0.0003)
    env[a:] += 0.55 * D.env_exp(n - a, 0.060, 0.001)              # the tail starts with the last flam
    clap = np.stack([nz[0], 0.9 * nz[0] + 0.3 * nz[1]], 1) * env[:, None]
    crack = (0.1 + 0.6 * v) * P.hp_noise(take.rng('modal', 'crack'), n, 5000.0, order=2) * D.env_exp(n, 0.002, 0.0002)
    clap += crack[:, None]

    # a little of the kit's room on the clap, its send high-passed so the room stays out of the low mids
    send = D.bfilt(D.mono(clap), 1500.0, 'highpass', order=4)
    room = D.fdn_reverb(send, K.space_rng('room'), t60=0.45, size=0.35, damp_hz=9000.0)[:n]

    y = SNARE_B_MIX[0] * D.st(body) + SNARE_B_MIX[1] * (0.6 + 0.4 * v) * clap + SNARE_B_MIX[2] * room
    return P.low_clean(y) * D.taper(n, *SNARE_FADE)[:, None] * P.vel_gain(v, 12.0)


# ------------------------------------------------------------------ snare C (round 3): the garage snare-clap
# Layered the way current garage snares are: snare A's crack (its FM body on C#4, wires and alloy ring, the kit's
# metal and its snap), three tight hands of band noise around it (inside 18 ms, each a little softer than the one
# before) with a short tail, and a short room in the kit's space on all of it, its send high-passed at 900 Hz so the
# room stays out of the low mids. Then snare A's soft clip. The loop sets its level so its peaks sit level with the
# kick's.
CLAP_HANDS = ((0.0, 0.0), (0.0085, -0.8), (0.0175, -1.5))   # (seconds at velocity 0.6, dB): three hands in 18 ms
CLAP_MIX = dict(clap=0.5, tail=0.25, room=0.13)
CLAP_HAND_TAU = 0.0028                  # each hand's decay, s; the last one's tail falls over CLAP_TAIL_TAU
CLAP_TAIL_TAU = 0.060
CLAP_BAND = (1000.0, 4800.0, 1800.0)    # the hands' noise band and its presence peak, Hz, at velocity 0 (both rise
                                        # with velocity: the top by up to 50%, the peak by up to 20%)
CLAP_SIDE = 0.3                         # each side's own noise in the hands: a little width, the mono sum whole
CLAP_ROOM = dict(t60=0.45, size=0.32, damp_hz=9000.0, send_hp=900.0)
CLAP_AIR = 0.3                          # the top over 4 kHz lifted by up to this (a share, at full velocity)
CLAP_INTO_CLIP = 0.8                    # the level into snare A's clip: the hands and room add level, so 2 dB less in
                                        # keeps the clip as gentle as snare A's, and the hardest hit the brightest


def snare_clap(take):
    """Snare C: snare A's crack with three hands of band noise around it and a short room on both, soft-clipped as
    snare A is. Harder hits: everything snare A's harder hits do, plus tighter hands, a brighter band and louder
    hands."""
    v = take.vel
    hands = lambda *extra: D.rng(take.kit, take.item.name, take.rr, *extra)   # the hands' streams, per round robin:
    r = hands('timing')                                                        # every layer claps the same noise, so
    n = int(1.4 * SR)                                                          # only velocity makes a layer brighter
    t = P.times(n)
    y = metal_crack(take, n, t)
    spread = P.lerp((1.2, 0.9), v)
    env = np.zeros(n)
    for at, g in CLAP_HANDS:
        a = int(at * spread * SR * (1 + 0.04 * r.uniform(-1, 1)))
        env[a:] += D.undb(g) * D.env_exp(n - a, CLAP_HAND_TAU, 0.0003)
    env[a:] += CLAP_MIX['tail'] * D.env_exp(n - a, CLAP_TAIL_TAU, 0.001)      # the last hand's tail
    lo, hi, peak = CLAP_BAND
    nz = [D.peaking(P.band_noise(hands('hand', ch), n, lo, hi * (1 + 0.5 * v)), peak * (0.8 + 0.4 * v),
                    4.0, 1.2) for ch in (0, 1)]
    clap = np.stack([nz[0], (1 - CLAP_SIDE) * nz[0] + CLAP_SIDE * nz[1]], 1) * env[:, None]
    y = y + CLAP_MIX['clap'] * (0.75 + 0.25 * v) * clap
    room_in = D.bfilt(D.mono(y), CLAP_ROOM['send_hp'], 'highpass', order=4)
    room = D.fdn_reverb(room_in, K.space_rng('clap room'), t60=CLAP_ROOM['t60'], size=CLAP_ROOM['size'],
                        damp_hz=CLAP_ROOM['damp_hz'])[:n]
    y = y + CLAP_MIX['room'] * room
    y = y + CLAP_AIR * v * D.bfilt(y, 4000.0, 'highpass', order=2)           # harder hits brighter, layer by layer
    y = D.oversampled(lambda u: P.soft_clip(CLAP_INTO_CLIP * u, SNARE_A_CLIP) / CLAP_INTO_CLIP, y)
    return P.low_clean(y) * D.taper(n, *SNARE_FADE)[:, None] * P.vel_gain(v, 12.0)


# ------------------------------------------------------------------ closed hat A: FM metal, tight
HAT_PAIRS = ((1, 4), (2, 6), (3, 5))  # (carrier, modulator) indices into the alloy ratios
HAT_A_WIDTH = 0.5                     # radians: each side's modulators start this far either way of the centre


def hat_fm(take):
    """Tight and tonal: three FM pairs on the high alloy ratios over 2.9 kHz, rendered at 4x so no sideband folds
    back, over a quarter of noise, low-passed at 4th order (8-16 kHz with velocity) and 30 dB down in 45-70 ms (inside
    a 16th at 134 BPM, 112 ms, so swung 16ths stay apart). The width comes from each side's modulator phases
    (+-HAT_A_WIDTH), not a delayed copy, so the mono sum keeps every partial. Harder hits: more FM index, a brighter
    top, a sharper tick, a slightly longer decay."""
    v = take.vel
    r = take.rng('fm')
    n = int(0.6 * SR)
    k = 4
    tk = np.arange(n * k) / (SR * k)
    base = 2870.0 * (1 + 0.004 * r.uniform(-1, 1))
    idx = (0.5 + 1.3 * v) + (0.8 + 2.4 * v) * np.exp(-tk / (0.003 + 0.004 * v))
    # the same modulator phases on every take: on a hit this short, random phases moved the round robins by 1.6 dB
    ph = D.rng(take.kit, take.item.name, 'phases').uniform(0, 2 * np.pi, len(HAT_PAIRS))
    sides = []
    for w in (-HAT_A_WIDTH, HAT_A_WIDTH):
        metal = sum(np.sin(2 * np.pi * base * ALLOY[a] * tk + idx * np.sin(2 * np.pi * base * ALLOY[b] * tk + p + w))
                    for (a, b), p in zip(HAT_PAIRS, ph))
        sides.append(D.down(metal, k) / len(HAT_PAIRS))
    nz = r.uniform(-1, 1, n)
    x = 0.75 * np.stack(sides, 1) + 0.25 * D.st(nz)
    x = D.bfilt(D.bfilt(x, 6000.0 - 500.0 * v, 'highpass', order=4), 8000.0 + 8000.0 * v, 'lowpass', order=4)
    tick = D.bfilt(take.rng('fm', 'tick').uniform(-1, 1, n), (9000.0, 16000.0), 'bandpass', order=2)
    tick *= D.env_exp(n, 0.0012, 0.0002)                       # band-passed: nothing above 20 kHz eats the peak gain
    env = D.env_exp(n, 0.010 + 0.008 * v, 0.0003)
    y = x * env[:, None] + (0.2 + 0.8 * v) * D.st(tick)
    return y * P.vel_gain(v, 13.0)


# ------------------------------------------------------------------ closed hat B: ring-modulated noise, a darker sizzle
RING_HAT_F0 = 4200.0         # carrier base: islands of noise near 4.2, 6.2, 8.1, 10.7, 12.6, 15.6 and 18.8 kHz
RING_HAT_TONES = 700.0 * ALLOY[:4]  # a small alloy cluster under the noise: 700, 1038, 1352 and 1782 Hz
RING_HAT_TONE_LEVEL = 1.0    # the cluster against the noise (was 0.6): more of the kit's metal in the sizzle
HAT_PREROLL = 0.05           # seconds of noise before the hit, for the leveller to settle on
RING_HAT_FADE = (0.32, 0.15)  # like the snares: 35-55 dB down at 0.32 s, gone by 0.47 s, one 400 ms loudness block


def ring_noise(rr, n, f0):
    """Noise around 1 kHz plus a small alloy tone cluster, ring-modulated onto carriers on the alloy ratios from f0:
    islands of noise with inharmonic partials in them, about unit RMS. Each carrier gets its own noise, so the islands
    flicker independently."""
    t = P.times(n)
    tones = sum(np.sin(2 * np.pi * f * t + rr.uniform(0, 2 * np.pi)) for f in RING_HAT_TONES) * RING_HAT_TONE_LEVEL
    out = np.zeros(n)
    for k, ratio in enumerate(ALLOY):
        out += D.ring_mod(P.band_noise(rr, n, 400.0, 2200.0, order=4) + tones, f0, (ratio,), r=rr) / (1.0 + 0.4 * k)
    return out / 1.4


def hat_ring(take):
    """The other choice to hat A: longer and darker. Ring-modulated noise through a resonant band centred at 5-6.5 kHz
    whose centre falls into place (the pitch envelope), with a little air and a tick, 30 dB down in 0.2-0.26 s (a
    sizzle rather than a tick) and faded out by 0.47 s. Harder hits: a higher, deeper sweep, a wider band, more air and
    a sharper tick."""
    v = take.vel
    r = take.rng('ring')
    n = int(1.0 * SR)
    pre = int(HAT_PREROLL * SR)
    f0 = RING_HAT_F0 * (1 + 0.003 * r.uniform(-1, 1))
    fc = (5000.0 + 1500.0 * v) * (1 + (0.10 + 0.40 * v)
                                  * np.exp(-np.maximum(P.times(n + pre) - HAT_PREROLL, 0) / 0.010))
    q = P.lerp((3.5, 2.0), v)

    def voice(rr):
        """The band and the air, squashed by a hard, fast leveller that settles in the pre-roll: compressed noise, and
        the same level on every take (30 ms of noise alone wanders by +-0.25 dB from take to take)."""
        x = ring_noise(rr, n + pre, f0)
        _, bp, _ = D.svf_tpt(x, fc, q=q)
        y = bp / q + (0.25 + 0.45 * v) * D.bfilt(x, 12000.0, 'highpass', order=2)
        y = D.updown3(y, down_db=-16.0, down_ratio=20.0, up_db=-90.0, up_ratio=1.0, attack=0.0005, release=0.004,
                      rms=0.002)
        return D.bfilt(y, 3000.0, 'highpass', order=4)[pre:]      # the leveller leaves the band skirts uncompressed
    env = D.env_exp(n, 0.055 + 0.020 * v, 0.0003)
    tick = (0.07 + 0.21 * v) * P.hp_noise(take.rng('ring', 'tick'), n, 8000.0, order=2) * D.env_exp(n, 0.001, 0.0002)
    mid = voice(r) * env + tick
    side = 0.35 * voice(take.rng('ring', 'side')) * env
    return np.stack([mid + side, mid - side], 1) * D.taper(n, *RING_HAT_FADE)[:, None] * P.vel_gain(v, 13.0)


# ------------------------------------------------------------------ shaker (round 3): the swung top layer
# Garage's shuffle lives in the hats and the shakers. Metal beads: band noise from about 4 kHz, broken into grains by a
# random bead train, with a soft attack (faster on hard strokes) and a fall under a 16th, and a faint glint of the
# kit's alloy modes from 5.4 kHz. The beads' noise is set to one level under each stroke (its envelope-weighted RMS),
# so the round robins differ in their grains, not their level. A stereo shaker: one bead train (the same strokes on
# both sides, as two mics hear one shaker) and each side half its own noise, so the loop can keep the groove's driver
# near the centre and still have its width (the round 3 review: panned -0.6, it played 9.8 dB louder on the left).
SHAKER_LAYERS = (0.3, 0.55, 0.8, 1.0)
SHAKER_BAND = (4200.0, 14000.0, 8200.0)   # the beads' band at full velocity (both edges lower on soft strokes) and
                                          # its presence peak, Hz
SHAKER_ENV = dict(attack=(0.009, 0.004), fall=(0.018, 0.034))   # seconds, soft stroke to hard: a raised-cosine attack,
                                                                 # then an exponential fall (-20 dB in 56-89 ms)
SHAKER_BEADS = dict(rate=1600.0, tau=0.00025, depth=0.65)       # grains a second, each grain's decay, the AM depth
SHAKER_GLINT = (5400.0, 0.12)            # the alloy modes' first frequency, and their level at full velocity
SHAKER_SIDE = 0.5                        # each side's own noise: the files' side 3.8-6.2 dB under mid, an L/R
                                         # correlation of 0.41-0.62, about hat A's (5.0 dB, 0.52); 0.35 read 8.1-10.2 dB
                                         # and 0.73-0.83
SHAKER_S = 0.30                          # the file: faded out over its last 70 ms


def shaker(take):
    """One shaker stroke: grains of band noise (4-14 kHz at full velocity, darker on soft strokes) with a presence peak,
    a faint alloy glint, a soft attack and a short fall; one bead train on both sides and each side half its own noise,
    about as wide as hat A. Harder strokes: a faster attack, a longer fall, a brighter band and peak, more glint."""
    v = take.vel
    r = take.rng('beads')
    n = int(SHAKER_S * SR)
    t = P.times(n)
    b = SHAKER_BEADS
    grains = r.poisson(b['rate'] / SR, n).astype(float)
    g = np.convolve(grains, np.exp(-np.arange(int(0.003 * SR)) / (b['tau'] * SR)))[:n]
    g = g / (np.mean(g) + 1e-12)
    am = 1.0 - b['depth'] + b['depth'] * np.minimum(g, 4.0) / 2.0
    lo, hi, peak = SHAKER_BAND
    lo, hi = lo * (0.8 + 0.3 * v), hi * (0.7 + 0.3 * v)
    core = P.band_noise(take.rng('beads', 'core'), n, lo, hi)
    x = np.stack([(1 - SHAKER_SIDE) * core + SHAKER_SIDE * P.band_noise(take.rng('beads', 'side', ch), n, lo, hi)
                  for ch in (0, 1)], 1) * am[:, None]
    x = D.peaking(x, peak * (0.9 + 0.15 * v), 3.0 + 2.0 * v, 1.0)
    f_glint, g_glint = SHAKER_GLINT
    glint = D.modal_ir(f_glint * (1 + 0.003 * r.uniform(-1, 1)), ALLOY[:4], [0.010, 0.008, 0.006, 0.005],
                       [1.0, 0.7, 0.5, 0.4], n, D.rng(take.kit, take.item.name, 'glint'))
    x = x + g_glint * v * D.st(glint)
    att = P.lerp(SHAKER_ENV['attack'], v)
    env = P.raised(n, att) * np.exp(-np.maximum(t - att, 0) / P.lerp(SHAKER_ENV['fall'], v))
    w = env ** 2
    x = x / np.sqrt(np.sum(w[:, None] * x ** 2) / (2 * np.sum(w)) + 1e-20)
    y = D.bfilt(x * env[:, None], 2500.0, 'highpass', order=4)
    return D.mono_below(y, 150.0) * D.taper(n, SHAKER_S - 0.08, 0.07)[:, None] * P.vel_gain(v, 12.0, SHAKER_LAYERS[0])


# ------------------------------------------------------------------ alloy hit, F#5
ALLOY_NOTE = K.ROOT + 36     # F#5, 739.99 Hz
ALLOY_LAYERS = (0.25, 0.5, 0.75, 1.0)
# the first mode's decay (s), the fade (start, length) and the render length: the perc hit rings for a second; the stab
# transient (plan section 2.2: once per chord, on the top voice) is gone by 0.35 s, before the chops' formants
ALLOY_SHAPES = {'hit': (0.30, (1.5, 0.8), 2.4), 'short': (0.10, (0.25, 0.10), 0.6)}


def alloy_hit(take):
    return alloy(take, *ALLOY_SHAPES['hit'])


def alloy_short(take):
    return alloy(take, *ALLOY_SHAPES['short'])


def alloy(take, decay, fade, seconds):
    """Struck alloy modes on F#5 with a 3-operator FM strike, the strike also ring-modulated by the alloy ratios, then a
    frequency-shifted copy each side (+ on the left, - on the right, 5-50 Hz) beating against the centre: the inharmonic
    'alloy' shimmer. Harder hits: more upper modes, more FM, a deeper pitch bend and a wider detune."""
    v = take.vel
    r = take.rng('alloy')
    n = int(seconds * SR)
    t = P.times(n)
    f0 = take.f0 * 2 ** (r.uniform(-2, 2) / 1200)               # round robin: +-2 cents and its own scrape
    # every take gets the same mode and carrier phases: the strike's carrier sits on the first mode, and random phases
    # made the two cancel or add, 1.4 dB apart between round robins
    phases = D.rng(take.kit, take.item.name, 'phases')

    # modal body; harder hits light up more of the upper modes and bend further
    decays = [decay / (1 + 0.55 * k) for k in range(len(ALLOY))]
    amps = [v ** (0.35 * k) / (1 + 0.6 * k) for k in range(len(ALLOY))]
    bend = (0.004 + 0.016 * v) * np.exp(-t / 0.012)
    body = P.modes(n, f0, ALLOY, decays, amps, bend, phases)

    # FM strike: mod2 -> mod1 -> carrier, indices fall fast; the index is capped so the sidebands stay under 20 kHz
    fm1, fm2 = f0 * ALLOY[3], f0 * ALLOY[5]
    i1 = min((20000 - f0) / fm1 - 1, 1.0 + 3.5 * v) * np.exp(-t / (0.02 + 0.04 * v))
    i2 = 1.2 * v * np.exp(-t / 0.012)
    m1 = np.sin(2 * np.pi * fm1 * t + i2 * np.sin(2 * np.pi * fm2 * t))
    strike = np.sin(2 * np.pi * f0 * t + i1 * m1) * D.env_exp(n, 0.06 + 0.06 * v, 0.0003)
    clang = D.ring_mod(strike * D.env_exp(n, 0.02), f0 * 0.5, ALLOY, r=phases)
    scrape = (0.05 + 0.25 * v) * P.hp_noise(r, n, 5000.0, order=2) * D.env_exp(n, 0.0015, 0.0002)

    centre = 0.8 * body + (0.25 + 0.35 * v) * strike + 0.4 * v * clang + scrape
    shift = (6.0 + 30.0 * v) * (1 + 0.05 * r.uniform(-1, 1))
    left = D.freq_shift(centre, shift)
    right = D.freq_shift(centre, -1.3 * shift)
    g = 0.5 + 0.2 * v
    y = np.stack([centre + g * left, centre + g * right], 1)
    y = D.bfilt(y, 300.0, 'highpass', order=4) * D.taper(n, *fade)[:, None] * D.env_exp(n, 1.0, 0.0003)[:, None]
    return y * P.vel_gain(v, 12.0, ALLOY_LAYERS[0])


ITEMS = [
    Item('Drums', 'KickSplit', 'kick', kick_split, layers=LAYERS, rr=2, root=K.KICK_NOTE,
         sets=('s1a', 'r2', 'r3', 'r4', 'r5'),
         about='kick A, split: clean F#1 sub, a driven knock gone by 95 ms, an alloy top above 1 kHz'),
    Item('Drums', 'KickPunch', 'kick', kick_punch, layers=LAYERS, rr=2, root=K.KICK_NOTE, sets=('r3',),
         about="kick B, punch: Split's clean F#1 sub held flatter and gone sooner (-20 dB by about 220 ms), a "
               "100-150 Hz knock on the sweep's second harmonic over its first 50 ms, Split's knock and alloy top"),
    Item('Drums', 'KickTight', 'kick', kick_tight, layers=LAYERS, rr=2, root=K.KICK_NOTE, sets=('s1a',),
         about='kick B, tight: a driven body gated short and cut at 250 Hz, a short clean F#1 sine (gone by 0.28 s)'),
    Item('Drums', 'SnareMetal', 'snare', snare_metal, layers=LAYERS, rr=2, sets=('s1a', 'r2', 'r3'),
         about='metal snare A: FM body on C#4, bright wires, an alloy ring from F#6'),
    Item('Drums', 'SnareModal', 'snare', snare_modal, layers=LAYERS, rr=2, sets=('s1a', 'r2', 'r3'),
         about='metal snare B: struck alloy body on D4 and a four-flam clap in the kit room'),
    Item('Drums', 'SnareClap', 'snare', snare_clap, layers=LAYERS, rr=2, sets=('r3',),
         about="snare C, garage snare-clap: snare A's FM body, wires and alloy ring, three tight hands in 18 ms, a "
               'short room (0.45 s), soft-clipped'),
    Item('Drums', 'ClosedHatFM', 'hat', hat_fm, layers=LAYERS, rr=3, sets=('s1a', 'r2', 'r3'),
         about='closed hat A, tight: FM pairs on the high alloy ratios at 4x, 30 dB down in 45-70 ms'),
    Item('Drums', 'ClosedHatRing', 'hat', hat_ring, layers=LAYERS, rr=3, sets=('s1a', 'r2', 'r3'),
         about='closed hat B, a darker sizzle: ring-modulated noise and alloy tones through a falling 5-6.5 kHz band'),
    Item('Perc', 'Shaker', 'hat', shaker, layers=SHAKER_LAYERS, rr=4, sets=('r3', 'r4', 'r5'),
         about='shaker for swung 16ths: grains of 4-14 kHz bead noise, a soft attack, a fall under a 16th, a faint '
               'alloy glint'),
    Item('Perc', 'AlloyHit', 'perc', alloy_hit, layers=ALLOY_LAYERS, rr=2, root=ALLOY_NOTE,
         sets=('s1a', 'r2', 'r3', 'r4', 'r5'),
         about='alloy hit on F#5: struck alloy modes, FM strike, ring-mod clang, +-6-36 Hz shifted sides'),
    Item('Perc', 'AlloyHitShort', 'perc', alloy_short, layers=ALLOY_LAYERS, rr=2, root=ALLOY_NOTE,
         sets=('s1a', 'r2', 'r3', 'r4', 'r5'),
         about='the alloy hit as the stab transient: the same strike, its modes three times shorter, gone by 0.35 s'),
]
