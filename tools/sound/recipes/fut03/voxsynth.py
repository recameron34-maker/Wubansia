"""FUT03 round 5: the wordless vox synth (sound 9) and its riff (sound 10). No words, no speech engine, no models:
every file here is synthesis, so the round 5 kit renders without the vocal models.

The voice (sing): a glottal-like source built additively, every harmonic of the sung pitch up to 16 kHz (band-limited
at any pitch, faded out from 12 kHz), with a breathy spectral tilt (1/h, and a corner over which it falls 6 dB an octave
more; velocity raises the corner) and a vocal chain's presence and air (chain); shaped harmonic by harmonic by five
formants (each a two-pole band-pass at its own level over a floor, as a parallel formant synthesiser sets them) on a
glide from "ooh" to "aah"; a breath layer, noise pulsed with the glottal cycle, through the same formants on the same
glide (an STFT filter); a soft consonant onset: a breath puff 25 ms before the note and the lips opening, F1 and F2
rising from a "w" into the "ooh" over 50 ms; a scoop up into the note (40 cents in 70 ms); gentle delayed vibrato, slow
drift, jitter and a little shimmer; and a slight double, a pair of voices 8 cents either side, each with its own
vibrato and drift, panned apart. Everything is high-passed at 280 Hz (HP): the vox sits above the low mids.

- VoxSynth: the playable multisample, roots G3 D4 G4 D5 (in G minor), two layers (soft: breathier and darker); each
  note "ooh" opening to "aah" over half a second, held with vibrato, released closing a little. Dry.
- VoxChopOohAhG4, VoxChopOohAhC5, VoxChopAhD5, VoxChopStackGm7: single chops, each on its stated root (the stack: Gm7,
  G4 Bb4 D5 F5), through a moving low-pass that opens over the chop and the kit's 3/16 dub echo.
- PhraseVoxRiff: 2 bars of off-beat chops on the kit's swing over Gm9 (D5 Bb4 D5 F5) then Cm9 (Eb5 Bb4 C5 G4), a
  low-pass riding open and closed over the 2 bars, a light 3/16 dub echo on every chop and a dub throw on the last
  (seven repeats, darkening), wrapping round the loop. Its notes are VOX_RIFF (recipes/fut03/midi.py exports them).
"""
import functools

import numpy as np
from scipy import signal

from tools.sound import dsp48 as D
from tools.sound import phrase as Ph
from tools.sound import qa
from tools.sound.item import Item, Out
from tools.sound.kits import fut03 as K
from tools.sound.recipes.fut03 import accents as A

SR = D.SR
SETS = ('r5',)
HP = 280.0                   # the vox sits above the low mids
LEAD = 0.025                 # seconds from a file's start to its note: the breath puff before it
HOP = 64                     # control rate of the pitch and formant tracks (750 Hz)
FMAX = 16000.0               # the source's top harmonic, faded out from 12 kHz
# (formant Hz, bandwidth Hz, level dB re F1) x 5: a deep voice, its formants a little under the average adult's
OOH = ((330.0, 70.0, 0.0), (800.0, 90.0, -6.0), (2350.0, 150.0, -18.0), (3300.0, 220.0, -22.0),
       (3900.0, 280.0, -26.0))
AAH = ((760.0, 90.0, 0.0), (1150.0, 110.0, -3.0), (2550.0, 160.0, -12.0), (3400.0, 220.0, -15.0),
       (4000.0, 280.0, -19.0))
W = ((260.0, 70.0, 0.0), (620.0, 90.0, -9.0), (2250.0, 150.0, -22.0), (3250.0, 220.0, -26.0),
     (3850.0, 280.0, -30.0))   # the lips nearly closed: the onset
FLOOR_DB = -32.0             # the envelope between and over the formants, re F1: the voice keeps a top
EFFORT_DB = 5.0              # a harder note's upper formants, over 600 Hz and full from 2.4 kHz: vocal effort
SCOOP = (-40.0, 0.07)        # cents, seconds: up into the note
DOUBLE = ((8.0, 5.35), (-8.0, 4.7))   # the double, a pair of voices: cents off the note, each its own vibrato (Hz)
DOUBLE_LEVEL = 0.35          # each of the pair: they beat evenly either side, so the pitch stays on the note
PAN = (0.0, 0.45, -0.45)     # the voice and the pair
ECHO_STEPS = 3               # the kit's dub echo: a dotted 8th (context.ECHO_STEPS, chords.ECHO_STEPS)


def smooth(u):
    u = np.clip(u, 0.0, 1.0)
    return u * u * (3.0 - 2.0 * u)


def drift(r, frames, fc, rms):
    """Slow random wander at the control rate: white noise low-passed at fc, scaled to `rms`."""
    w = r.standard_normal(frames + 64)
    sos = signal.butter(2, fc, 'lowpass', fs=SR / HOP, output='sos')
    z = signal.sosfilt(sos, w)[64:]
    return rms * z / (np.sqrt(np.mean(z * z)) + 1e-12)


def chain(f):
    """The vocal chain's EQ, on the harmonics and the breath alike: presence (+8 dB at 3.3 kHz) and air (a shelf rising
    from 4.5 kHz to +8 dB by 14 kHz), so the deep voice still cuts through a club mix."""
    lf = np.log2(np.maximum(f, 20.0))
    return (1.0 + 1.5 * np.exp(-0.5 * ((lf - np.log2(3300.0)) / 0.7) ** 2)) * (1.0 + 1.5 * smooth((lf - np.log2(4500.0))
                                                                                                    / 1.6))


def tract(f, F, B, levels):
    """The formant envelope at frequencies f: five two-pole band-passes (peak 1, bandwidth B) at F, each at its level
    (linear), summed in power over FLOOR_DB, as a parallel formant synthesiser sets its formants."""
    f = np.maximum(f, 1.0)
    p = np.full(np.shape(f), D.undb(FLOOR_DB) ** 2)
    for Fi, Bi, Ai in zip(F, B, levels):
        q = Fi / Bi
        p = p + Ai ** 2 / (1.0 + q * q * (f / Fi - Fi / f) ** 2)
    return np.sqrt(p)


def vowels(tn, glide, hold, release, breathy, close=0.35):
    """Formants, bandwidths and levels at the times tn (seconds from the note): "w" to "ooh" over the first 50 ms,
    "ooh" to "aah" over `glide` (start, end), back toward "ooh" by `close` over the release. Each a list of five
    arrays."""
    w = smooth(tn / 0.05)
    p = smooth((tn - glide[0]) / (glide[1] - glide[0])) * (1.0 - close * smooth((tn - hold) / release))
    F, B, A = [], [], []
    for i in range(5):
        lw, lo, la = np.log(W[i][0]), np.log(OOH[i][0]), np.log(AAH[i][0])
        F.append(np.exp((1 - w) * lw + w * (lo + p * (la - lo))))
        B.append((OOH[i][1] + p * (AAH[i][1] - OOH[i][1])) * (1.0 + 0.15 * breathy * (i < 2)))
        A.append(D.undb((1 - w) * W[i][2] + w * (OOH[i][2] + p * (AAH[i][2] - OOH[i][2]))))
    return F, B, A


def sing(f0, n, v, r, hold, glide, release, vib, breath_db, detune=0.0):
    """One voice on f0 (Hz), n samples, velocity v: the note at LEAD, held `hold` s, released over `release` s; vib =
    (rate Hz, depth cents, delay s, fade s) or None. Mono."""
    t = np.arange(n) / SR
    tn = t - LEAD
    frames = n // HOP + 2
    ci = np.minimum(np.arange(frames) * HOP, n - 1)
    up = lambda c: np.interp(np.arange(n), ci, c)  # noqa: E731
    cents = detune + SCOOP[0] * (1.0 - smooth(tn / SCOOP[1]))
    if vib:
        rate, depth, delay, fade = vib
        cents = cents + depth * smooth((tn - delay) / fade) * np.sin(2 * np.pi * rate * t + r.uniform(0, 2 * np.pi))
    cents = cents + up(drift(r, frames, 3.0, 0.7) + drift(r, frames, 30.0, 0.5))
    f = f0 * 2.0 ** (cents / 1200.0)
    ph = D.phase(f, n)
    # the harmonics, shaped at the control rate
    corner = 800.0 + 2900.0 * v
    breathy = 1.0 - 0.6 * v
    F, B, Av = vowels(ci / SR - LEAD, glide, hold, release, breathy)
    H = int(FMAX // f.max())
    h = np.arange(1, H + 1)
    fh = f[ci][:, None] * h[None, :]
    amp = (1.0 / h) / np.sqrt(1.0 + (fh / corner) ** 2) * tract(fh, *([x[:, None] for x in z] for z in (F, B, Av)))
    effort = D.undb(EFFORT_DB * (v - LAYERS[0]) / (1.0 - LAYERS[0]) * smooth(np.log2(fh / 600.0) / 2.0))
    amp = amp * effort * chain(fh) * smooth((FMAX - fh) / 4000.0)
    shimmer = 1.0 + 0.04 * up(drift(r, frames, 20.0, 1.0))
    y = np.zeros(n)
    for k in range(H):
        y += up(amp[:, k]) * np.sin(2 * np.pi * h[k] * ph)
    env = smooth(tn / 0.035) * (0.5 + 0.5 * np.cos(np.pi * np.clip((tn - hold) / release, 0.0, 1.0)))
    voice = y * env * shimmer
    # the breath: noise pulsed with the glottal cycle, through the same formants on the same glide
    noise = r.standard_normal(n) * (0.55 + 0.45 * (0.5 + 0.5 * np.cos(2 * np.pi * ph)))
    fr, tt, X = signal.stft(noise, SR, nperseg=1024, noverlap=1024 - 128, boundary='even')
    Fb, Bb, Ab = vowels(tt - LEAD, glide, hold, release, breathy)
    tilt = (fr / 1200.0) ** 2 / (1.0 + (fr / 1200.0) ** 2) / np.sqrt(1.0 + (fr / (4000.0 + 5000.0 * v)) ** 2)
    tilt = tilt * chain(fr)
    env_b = tract(fr[:, None], *([x[None, :] for x in z] for z in (Fb, Bb, Ab))) * tilt[:, None]
    _, b = signal.istft(X * env_b, SR, nperseg=1024, noverlap=1024 - 128, boundary=True)
    b = np.concatenate([b, np.zeros(n)])[:n]
    puff = D.undb(8.0) * np.exp(-0.5 * ((tn + 0.004) / 0.012) ** 2)
    b_env = (smooth((tn + LEAD) / 0.012) * (1.0 + puff) *
             (0.5 + 0.5 * np.cos(np.pi * np.clip((tn - hold) / (release + 0.04), 0.0, 1.0))))
    on = (tn > 0.05) & (tn < hold)
    level = np.sqrt(np.mean(voice[on] ** 2)) / (np.sqrt(np.mean(b[on] ** 2)) + 1e-12) if on.any() else 1.0
    return voice + b * b_env * level * D.undb(breath_db)


def doubled(f0, n, v, key, hold, glide, release, vib, breath_db):
    """The voice and its slight double, a pair 8 cents either side (DOUBLE), softer, each with its own vibrato and
    drift, panned apart. Stereo."""
    y = Ph.pan(sing(f0, n, v, D.rng(K.KIT, 'vox synth', *key, 'a'), hold, glide, release, vib, breath_db), PAN[0])
    for j, (cents, rate) in enumerate(DOUBLE):
        vib2 = None if vib is None else (rate,) + tuple(vib[1:])
        b = sing(f0, n, v, D.rng(K.KIT, 'vox synth', *key, 'd', j), hold, glide, release, vib2, breath_db + 3.0,
                 detune=cents)
        y = y + DOUBLE_LEVEL * Ph.pan(b, PAN[1 + j])
    return y


def finish(y):
    return D.mono_below(D.bfilt(y, HP, 'highpass', order=2), 150.0)


# ------------------------------------------------------------------ the multisample
NOTES = (K.ROOT + 12, K.ROOT + 19, K.ROOT + 24, K.ROOT + 31)    # G3 D4 G4 D5
LAYERS = (0.6, 1.0)
HOLD, RELEASE = 1.15, 0.35
GLIDE = (0.08, 0.5)
ZONE_LUFS = -18.0            # each zone's hard layer, before the instrument's one gain


def vox_synth(take):
    v = take.vel
    n = int((LEAD + HOLD + RELEASE + 0.1) * SR)
    vib = (5.0, 12.0 + 6.0 * v, 0.45, 0.35)
    y = finish(doubled(take.f0, n, v, ('note', take.note, take.layer), HOLD, GLIDE, RELEASE, vib,
                       -16.0 - 2.0 * (v - 0.6) / 0.4))
    y = y * float(D.undb(ZONE_LUFS - qa.lufs(y) + 6.0 * (v - 1.0) / 0.4))   # every zone as loud, soft 6 dB down
    return Out(y, f0s=(take.f0,), meta={'root': D.note_name(take.note), 'vowels': 'ooh to aah', 'lead_s': LEAD})


# ------------------------------------------------------------------ the chops
def lp_open(y, lo, hi, t_open):
    """A 2-pole low-pass opening from lo to hi (exponentially) over t_open seconds after the note, per channel."""
    n = len(y)
    tn = np.arange(n) / SR - LEAD
    fc = lo * (hi / lo) ** smooth(tn / t_open)
    out = np.empty_like(y)
    for c in range(2):
        out[:, c] = D.svf_tpt(y[:, c], fc, q=0.9)[0]
    return out


# name: (notes, length s (from the note), velocity, low-pass (lo, hi) Hz, about)
CHOPS = {
    'VoxChopOohAhG4': ((67,), 0.42, 0.9, (800.0, 6000.0), '"ooh" opening to "aah" on G4, a beat long'),
    'VoxChopOohAhC5': ((72,), 0.22, 0.85, (1000.0, 7000.0), 'a quick "ooh-ah" on C5, an 8th'),
    'VoxChopAhD5': ((74,), 0.30, 1.0, (1500.0, 8000.0), 'an open "aah" on D5, a dotted 8th'),
    'VoxChopStackGm7': ((67, 70, 74, 77), 0.42, 0.85, (800.0, 5500.0), 'a Gm7 stack, G4 Bb4 D5 F5, "ooh" to "aah"'),
}
CHOP_ECHO = dict(taps=4, fb=0.5, lp=4500.0, hp=500.0, level=0.45)


def chop_dry(notes, length, v, key, release=0.07):
    """The doubled voice on each note, gated `length` s after the note: "ooh" to "aah" over the chop's first 70%."""
    n = int((LEAD + length + release + 0.05) * SR)
    glide = (0.03, max(0.08, 0.7 * length))
    y = np.zeros((n, 2))
    for k, note in enumerate(notes):
        y += doubled(float(D.midi_hz(note)), n, v, (*key, k), length, glide, release, None, -12.0)
    return y / np.sqrt(len(notes))


def vox_chop(take):
    notes, length, v, (lo, hi), _ = CHOPS[take.item.name]
    dry = lp_open(chop_dry(notes, length, v, (take.item.name,)), lo, hi, 0.8 * length)
    n = len(dry) + int(4.2 * ECHO_STEPS * K.STEP * SR)
    y = np.zeros((n, 2))
    y[:len(dry)] = dry
    e = CHOP_ECHO
    y = y + e['level'] * A.ping_pong(y, n, ECHO_STEPS, e['taps'], e['fb'], e['lp'], e['hp'])
    fs = tuple(float(D.midi_hz(m)) for m in notes)
    return Out(finish(y * D.taper(n, (n / SR) - 0.15, 0.15)[:, None]), f0s=fs,
               meta={'root': D.note_name(notes[0]), 'notes': [D.note_name(m) for m in notes], 'length_s': length,
                     'lowpass_hz': [lo, hi], 'echo': {**e, 'steps': ECHO_STEPS}, 'lead_s': LEAD})


# ------------------------------------------------------------------ the riff
RIFF_STEPS = 32
# (16th, MIDI, length in 16ths, velocity): off-beat chops; G4 67, Bb4 70, C5 72, D5 74, Eb5 75, F5 77
VOX_RIFF = (
    (2, 74, 2.0, 0.9), (6, 70, 1.5, 0.7), (10, 74, 1.5, 0.8), (13, 77, 1.0, 0.65),    # bar 1, Gm9: D5 Bb4 D5 F5
    (18, 75, 2.0, 0.9), (22, 70, 1.5, 0.7), (26, 72, 1.5, 0.8), (30, 67, 2.0, 0.85),  # bar 2, Cm9: Eb5 Bb4 C5 G4
)
RIFF_CHORDS = ('Gm9', 'Cm9')
CHORD_PCS = {'Gm9': (7, 10, 2, 5, 9), 'Cm9': (0, 3, 7, 10, 2)}
RIDE = ((1200.0, 3000.0), (3500.0, 9000.0))   # each chop's low-pass opens from its first to its second value, both
                                              # riding up and back down over the 2 bars
RIFF_ECHO = dict(taps=3, fb=0.4, lp=4200.0, hp=500.0, level=0.22)
THROW = dict(taps=7, fb=0.62, lp=4400.0, hp=520.0, level=0.75, darken=0.88)
TAIL_STEPS = 32              # the riff's echoes run on this far past its 2 bars before they wrap


@functools.lru_cache(maxsize=1)
def riff_tail():
    """The riff before it wraps: the chops on the 2-bar grid, each through its low-pass, the light echo on all and the
    throw on the last, over 2 bars plus TAIL_STEPS. Stereo, unnormalised."""
    G = Ph.Grid(K, RIFF_STEPS)
    L = G.length
    n = L + int(TAIL_STEPS * G.s16)
    dry = np.zeros((n, 2))
    last = None
    for i, (s, m, ln, v) in enumerate(VOX_RIFF):
        length = ln * G.s16 / SR
        u = G.at(s) / L
        ride = 0.5 - 0.5 * np.cos(2 * np.pi * u)
        lo = RIDE[0][0] * (RIDE[0][1] / RIDE[0][0]) ** ride
        hi = RIDE[1][0] * (RIDE[1][1] / RIDE[1][0]) ** ride
        x = lp_open(chop_dry((m,), length, v, ('riff', i)), lo, hi * (0.8 + 0.2 * v), 0.8 * length) * v
        pos = G.at(s) - int(np.floor(LEAD * SR + 0.5))
        dry[pos:pos + len(x)] += x
        last = (pos, x)
    e = RIFF_ECHO
    y = dry + e['level'] * A.ping_pong(dry, n, ECHO_STEPS, e['taps'], e['fb'], e['lp'], e['hp'])
    pos, x = last
    th = np.zeros((n - pos, 2))
    th[:len(x)] = x
    w = THROW
    y[pos:] += w['level'] * A.ping_pong(th, n - pos, ECHO_STEPS, w['taps'], w['fb'], w['lp'], w['hp'], w['darken'])
    return finish(y)


def phrase_vox_riff(take):
    y = riff_tail()
    L = Ph.loop_len(K, RIFF_STEPS)
    z = np.zeros((L, 2))
    Ph.place(z, y, 0)                                   # the echoes' tail wraps round the loop
    return Out(z, meta={'steps': RIFF_STEPS, 'bpm': K.BPM, 'swing': K.SWING, 'chords': list(RIFF_CHORDS),
                        'notes': [[s, D.note_name(m), ln, v] for s, m, ln, v in VOX_RIFF],
                        'echo': {**RIFF_ECHO, 'steps': ECHO_STEPS}, 'throw': {**THROW, 'step': VOX_RIFF[-1][0]},
                        'lowpass_ride_hz': [list(r) for r in RIDE], 'lead_s': LEAD})


WORDLESS = 'wordless, synthesis only: no words, no speech engine'
ITEMS = [
    Item('Vox', 'VoxSynth', 'lead', vox_synth, layers=LAYERS, notes=NOTES, sets=SETS,
         qa={'tonal': False, 'why': 'a vocal synth: its breath through the formants and its doubles (8 cents '
                                    'either side) sit between the harmonics by design, which the alias check reads '
                                    'as aliasing'},
         about=f'vox synth, a playable multisample on G3 D4 G4 D5, two layers: a breathy formant voice opening from '
               f'"ooh" to "aah", delayed vibrato, a slight double; high-passed at {HP:g} Hz; {WORDLESS}'),
] + [
    Item('Vox', name, 'vox', vox_chop, root=notes[0], sets=SETS,
         about=f'vox synth chop: {about}, through a low-pass opening over the chop and the 3/16 dub echo; root '
               f'{D.note_name(notes[0])}; {WORDLESS}')
    for name, (notes, _, _, _, about) in CHOPS.items()
] + [
    Item('Loops', 'PhraseVoxRiff', 'loop', phrase_vox_riff, loop=True, sets=SETS,
         qa={'mud': 'lowcut', 'why': 'a vox synth phrase: the low cut, like the vox synth'},
         about='phrase: a 2-bar off-beat vox synth riff on the kit swing, D5 Bb4 D5 F5 over Gm9, Eb5 Bb4 C5 G4 over '
               f'Cm9, a riding low-pass, the 3/16 dub echo and a dub throw on the last chop; {WORDLESS}'),
]
