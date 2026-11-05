"""FUT02 "Orbit Relay" round 5 vox synth (set r5): a wordless vocal synth made by synthesis alone, in place of round 3's
text-to-speech vocals. No words, no speech engine, no models: a robotic but musical vocoder choir.

The voice, sing():
- the source: a band-limited glottal-like buzz, additive (every partial a harmonic of the note, each faded out before
  14 kHz at its instantaneous pitch, so nothing aliases), a saw's 1/k tilt (the glottis and the lips: 6 dB an octave)
  with a pulse's mild ripple;
- the vowel: five formant band-passes (F1-F3 from dsp48.VOWELS, 'oh' the open o, two more above), their magnitudes
  applied in the STFT domain (1024 points, hop 128), one set per frame, so a vowel glides into the next;
- the onset: a short 'd'-like consonant: a 7 ms noise burst at 2.5-7 kHz, the formants opening from a closed locus
  (F1 250, F2 1700 Hz) onto the vowel in 35 ms, the pitch scooping up 35 cents into the note over about 25 ms;
- life: vibrato from 0.25 s into a note, at +-13 cents and 5.4 Hz by 0.6 s; a slow pitch jitter (about +-1 cent);
  a breath layer (noise through the same formants, stronger on each onset);
- the choir: three voices, the centre on pitch, a double 8 cents sharp on the left and one 8 cents flat on the right,
  both with formants 2% higher (a second, smaller singer, the same on both sides) and high-passed above the note's
  fundamental, which stays the centre voice's: a shimmering detune on the overtones that stays centred, the note
  itself in tune;
- the sparkle: the top above 3.5 kHz frequency-shifted 9 Hz up on the left and 9 Hz down on the right, 18 dB under:
  a robotic shimmer;
- high-passed at 180 Hz and mono under 150 Hz: it sits above the kick and the bass.

VoxSynth: the playable multisample on D4 F4 A4 C5 F5 A5 (each a note of D minor), three velocity layers whose vowel the
velocity picks, darker to brighter as a filter opens: soft 'oh' (v1), medium 'ah' (v2), hard 'ee' (v3). Each note is
held 2 s and released over 0.3 s.
VoxChop*: five single hits on stated roots (D5 'ah', F5 'ee', A4 'oh', C5 'ee' opening to 'ah', A5 'ah' closing to
'oh').
VoxSynthRiff: a 2-bar riff on the voice at 130 BPM in D minor on the kit's swing. It locks with the chrome arp: each
bar opens on the arp's 3-3-2 accents a third under its motif (D5 F5 E5 under F5 A5 G5), then answers in the arp's
held notes (bar 1: D5, C5; bar 2: A5, C5), and ends on a throw, A4 held a beat into a dotted-8th ping-pong whose
repeats fall away in pitch (the kit's laser gesture) and wrap round the loop. Its vowels follow the velocities as the
multisample plays them (oh 0.5, ah 0.75, ee 1.0), so the MIDI phrase plays it back on VoxSynth.
"""
import functools
from dataclasses import dataclass

import numpy as np
from scipy import signal

from tools.sound import dsp48 as D
from tools.sound import parts as P
from tools.sound import phrase as Ph
from tools.sound.item import Item, Out
from tools.sound.kits import fut02 as K

SR = D.SR
R5 = ('r5',)
BW = (80.0, 100.0, 150.0, 220.0, 300.0)       # the formants' bandwidths, Hz


def _vowel(f123, f45, gains):
    return tuple(zip(tuple(float(f) for f in f123) + f45, BW, gains))


# five formants a vowel: (Hz, bandwidth, gain). 'd' is the consonant's closed locus the onset opens from
VOWELS = {
    'ah': _vowel(D.VOWELS['ah'], (3400.0, 4300.0), (1.0, 0.70, 0.30, 0.14, 0.07)),
    'ee': _vowel(D.VOWELS['ee'], (3700.0, 4500.0), (0.30, 0.70, 0.65, 0.30, 0.12)),
    'oh': _vowel((570.0, 840.0, 2410.0), (3300.0, 4200.0), (1.0, 0.80, 0.14, 0.07, 0.035)),
    'oo': _vowel(D.VOWELS['oo'], (3300.0, 4200.0), (1.0, 0.45, 0.08, 0.04, 0.02)),
    'd': _vowel((250.0, 1700.0, 2700.0), (3600.0, 4500.0), (0.35, 0.50, 0.40, 0.18, 0.09)),
}
FLOOR = 0.012                 # the formant envelope's floor, re its peaks
FMAX = 14000.0                # every partial fades out over the 1.5 kHz under this
ONSET = dict(burst_s=0.007, lead_s=0.006, burst_band=(2500.0, 7000.0), open_s=0.035, scoop=-0.35, scoop_tau=0.009,
             attack_s=0.010)
VIBRATO = dict(start_s=0.25, full_s=0.60, cents=13.0, hz=5.4)
JITTER = dict(cents=1.2, hz=5.0)
CHOIR = ((0.0, 1.0, 0.62, 0.62), (8.0, 1.02, 0.48, 0.0), (-8.0, 1.02, 0.0, 0.48))   # (cents, formant scale, L, R)
DOUBLE_HP = 1.6               # the doubles are high-passed (4th order) at this times the line's lowest fundamental: their
                              # beating on the fundamental itself (1.6 Hz on F4) moved its phase enough to read as a 5
                              # cent mistuning over a 0.2 s window
BREATH = dict(level=0.035, onset=0.14, onset_s=0.04)
SPARKLE = dict(hp_hz=3500.0, shift_hz=9.0, level=0.125)
HP = 180.0
END_FADE_S = 0.08              # every render fades out over its last 80 ms (after the release has died away)
NPER, HOP = 1024, 128


@dataclass(frozen=True)
class Note:
    """One sung note: the vowel's onset `on` (s; the consonant's burst starts ONSET['lead_s'] before it), the key
    lifted at `off`, its MIDI pitch, its vowel path ((vowel, arrival s after the onset), ...): from the closed 'd'
    locus to the first by its arrival, then gliding to each next by its own; velocity and release (s)."""
    on: float
    off: float
    midi: float
    path: tuple
    vel: float = 1.0
    release: float = 0.05


def _smooth_noise(r, n, hz):
    """Unit-RMS noise low-passed at hz: the slow wander of a pitch or a level."""
    x = D.bfilt(r.standard_normal(n + 2 * SR), hz, 'lowpass', order=2)[SR:SR + n]
    return x / (np.std(x) + 1e-12)


def _envelope(notes, n):
    """The voice's level per sample: each note raised over the attack from its onset (raised cosine), held, then
    released (an exponential, 35 dB down by its release time); overlapping notes take the larger."""
    t = P.times(n)
    env = np.zeros(n)
    a = ONSET['attack_s']
    for nt in notes:
        u_on = np.clip((t - nt.on) / a, 0.0, 1.0)
        e = (0.5 - 0.5 * np.cos(np.pi * u_on)) * np.exp(-np.maximum(t - nt.off, 0.0) / (nt.release / 4.0))
        env = np.maximum(env, e * nt.vel)
    return env


def _pitch(notes, n, cents, r):
    """The pitch per sample, fractional MIDI: each note from its onset (until the next one's), scooping up into it,
    its vibrato fading in, the choir voice's detune and a slow jitter."""
    t = P.times(n)
    m = np.full(n, float(notes[0].midi))
    vib = np.zeros(n)
    rate = VIBRATO['hz'] * (1.0 + 0.03 * cents / 8.0)
    for i, nt in enumerate(notes):
        nxt = notes[i + 1].on - ONSET['lead_s'] if i + 1 < len(notes) else np.inf
        k = (t >= nt.on - ONSET['lead_s']) & (t < nxt)
        u = np.maximum(t[k] - nt.on, 0.0)
        m[k] = nt.midi + ONSET['scoop'] * np.exp(-u / ONSET['scoop_tau'])
        depth = np.clip((u - VIBRATO['start_s']) / (VIBRATO['full_s'] - VIBRATO['start_s']), 0.0, 1.0)
        vib[k] = depth * np.sin(2 * np.pi * rate * u)
    jit = JITTER['cents'] / 100.0 * _smooth_noise(r, n, JITTER['hz'])
    return m + VIBRATO['cents'] / 100.0 * vib + jit + cents / 100.0


def _source(m, r, vel_tilt=1.0):
    """The buzz: additive harmonics of the pitch track m (MIDI), each fading out under FMAX at its instantaneous
    frequency; a 1/k tilt with a pulse's ripple."""
    f = 440.0 * 2.0 ** ((m - 69.0) / 12.0)
    ph = 2 * np.pi * D.phase(f, len(m))
    y = np.zeros(len(m))
    for k in range(1, int(FMAX // f.min()) + 1):
        fade = np.clip((FMAX - k * f) / 1500.0, 0.0, 1.0)
        if not fade.any():
            break
        a = (0.78 + 0.22 * np.cos(2 * np.pi * k * 0.35)) / k ** vel_tilt
        y += a * fade * np.sin(k * ph + r.uniform(0, 2 * np.pi))
    return y


def _formants(table, scale):
    """(F, B, g) arrays of a vowel table, its frequencies scaled (a singer's size)."""
    F = np.array([f for f, _, _ in table]) * scale
    B = np.array([b for _, b, _ in table])
    g = np.array([gg for _, _, gg in table])
    return F, B, g


def _track(notes, frames_t, scale):
    """The formants per STFT frame: (frames, 5) frequencies, bandwidths and gains along each note's vowel path; frames
    before a note's onset (and between notes) hold the next note's closed locus, the release holds its last vowel."""
    nf = len(frames_t)
    F = np.zeros((nf, 5))
    B = np.zeros((nf, 5))
    G = np.zeros((nf, 5))
    which = np.searchsorted([nt.on for nt in notes], frames_t, side='right') - 1
    for j in range(nf):
        if which[j] < 0:
            F[j], B[j], G[j] = _formants(VOWELS['d'], scale)
            continue
        nt = notes[which[j]]
        u = frames_t[j] - nt.on
        prev, t_prev = 'd', 0.0
        cur = None
        for name, t_arr in nt.path:
            if u < t_arr:
                w = (u - t_prev) / max(t_arr - t_prev, 1e-6)
                w = 0.5 - 0.5 * np.cos(np.pi * np.clip(w, 0.0, 1.0))
                a, b = _formants(VOWELS[prev], scale), _formants(VOWELS[name], scale)
                cur = (a[0] * (b[0] / a[0]) ** w, a[1] + (b[1] - a[1]) * w, a[2] + (b[2] - a[2]) * w)
                break
            prev, t_prev = name, t_arr
        if cur is None:
            cur = _formants(VOWELS[prev], scale)
        F[j], B[j], G[j] = cur
    return F, B, G


def _vowel_filter(x, notes, scale):
    """x through the notes' formants, frame by frame (STFT)."""
    f, ft, X = signal.stft(x, SR, nperseg=NPER, noverlap=NPER - HOP, boundary='even')
    F, B, G = _track(notes, ft, scale)
    fb = np.maximum(f, 20.0)[:, None]
    H = np.full(X.shape, FLOOR)
    for i in range(5):
        q = (F[:, i] / B[:, i])[None, :]
        Fi = F[:, i][None, :]
        H += G[:, i][None, :] / np.sqrt(1.0 + q * q * (fb / Fi - Fi / fb) ** 2)
    _, y = signal.istft(X * H, SR, nperseg=NPER, noverlap=NPER - HOP, boundary=True)
    return y[:len(x)]


def _onsets(notes, n, r):
    """The consonant's burst before each onset, and the breath's extra push on it."""
    t = P.times(n)
    burst = np.zeros(n)
    push = np.zeros(n)
    noise = P.band_noise(r, n, *ONSET['burst_band'])
    for nt in notes:
        u = t - (nt.on - ONSET['lead_s'])
        k = (u >= 0) & (u < ONSET['burst_s'])
        burst[k] += nt.vel * np.sin(np.pi * u[k] / ONSET['burst_s']) ** 2
        v = t - nt.on
        k = (v >= -ONSET['lead_s']) & (v < BREATH['onset_s'])
        push[k] = np.maximum(push[k], np.sin(np.pi * (v[k] + ONSET['lead_s'])
                                             / (BREATH['onset_s'] + ONSET['lead_s'])) ** 2)
    return burst * noise, push


def sing(notes, n, rng, tilt=1.0):
    """The vox synth on a line of notes (Note, by onset; one at a time: a chord is a line per tone), stereo (n, 2).
    rng(*key) gives the take's independent noise streams."""
    notes = sorted(notes, key=lambda nt: nt.on)
    env = _envelope(notes, n)
    burst, push = _onsets(notes, n, rng('burst'))
    f_low = 440.0 * 2.0 ** ((min(nt.midi for nt in notes) - 69.0) / 12.0)
    out = np.zeros((n, 2))
    for v, (cents, scale, gl, gr) in enumerate(CHOIR):
        m = _pitch(notes, n, cents, rng('pitch', v))
        shimmer = 1.0 + 0.04 * _smooth_noise(rng('shimmer', v), n, 4.0)
        src = _source(m, rng('phases', v), tilt) * env * shimmer
        breath = rng('breath', v).standard_normal(n) * (BREATH['level'] * env + BREATH['onset'] * push * env.max())
        y = _vowel_filter(src + breath, notes, scale)
        if cents:      # a double: above the line's lowest fundamental only, so the note itself is the centre voice's
            y = D.bfilt(y, DOUBLE_HP * f_low, 'highpass', order=4)
        out[:, 0] += gl * y
        out[:, 1] += gr * y
    out += 0.3 * np.stack([burst, burst], 1)
    for c, sgn in ((0, 1.0), (1, -1.0)):
        top = D.bfilt(out[:, c], SPARKLE['hp_hz'], 'highpass', order=2)
        out[:, c] += SPARKLE['level'] * D.freq_shift(top, sgn * SPARKLE['shift_hz'])
    out = D.mono_below(D.bfilt(out, HP, 'highpass', order=4), 150.0)
    return out * D.taper(n, n / SR - END_FADE_S, END_FADE_S)[:, None]


TILT = {'oh': 1.15, 'ah': 1.0, 'ee': 0.85}   # the buzz's tilt a vowel: the brighter vowels on a brighter buzz
NOTE_DB = -20.0                              # every sung note's RMS over its held part (dBFS) before its layer level


def sung(nt, n, rng):
    """One note sung (sing()) on its vowel's buzz, at NOTE_DB RMS (mono, over the key-down part): every note and vowel
    at one level, so the velocity alone sets the loudness."""
    y = sing([nt], n, rng, TILT.get(nt.path[0][0], 1.0))
    a, b = int(nt.on * SR), max(int(nt.off * SR), int(nt.on * SR) + 1)
    rms = np.sqrt(np.mean(D.mono(y)[a:b] ** 2)) + 1e-12
    return y * D.undb(NOTE_DB) / rms


# ------------------------------------------------------------------ the multisample
ZONES = (62, 65, 69, 72, 77, 81)          # D4 F4 A4 C5 F5 A5: notes of D minor, 3-5 semitones apart
LAYERS = (0.5, 0.75, 1.0)                 # the velocity picks the vowel:
LAYER_VOWEL = ('oh', 'ah', 'ee')          # soft 'oh', medium 'ah', hard 'ee'
LAYER_DB = (-6.0, -3.0, 0.0)              # and a level a layer
VOWEL_OF = dict(zip(LAYERS, LAYER_VOWEL))
PRE = 0.02                                # seconds before the vowel's onset: room for the consonant's burst
HOLD_S, RELEASE_S = 2.0, 0.3
ROOT_F0S = lambda f: tuple(f * 2 ** (c / 1200) for c, _, _, _ in CHOIR)


def vowel_of(vel):
    """The vowel a velocity plays: the nearest layer's (the louder on a tie)."""
    i = min(range(len(LAYERS)), key=lambda j: (abs(LAYERS[j] - vel), -j))
    return LAYER_VOWEL[i]


def vox_note(take):
    """One note of the multisample: its layer's vowel, held HOLD_S, released over RELEASE_S."""
    i = take.layer - 1
    n = int((PRE + HOLD_S + RELEASE_S + 0.25) * SR)
    nt = Note(PRE, PRE + HOLD_S, take.pitch, ((LAYER_VOWEL[i], ONSET['open_s']),), 1.0, RELEASE_S)
    y = sung(nt, n, take.rng) * D.undb(LAYER_DB[i])
    return Out(y, f0s=ROOT_F0S(take.f0))


# ------------------------------------------------------------------ the chops
CHOPS = (   # name, root, vowel path, key down (s), release (s), about
    ('VoxChopAhD5', 74, (('ah', 0.035),), 0.20, 0.08, "'ah' on D5, an 8th"),
    ('VoxChopEeF5', 77, (('ee', 0.035),), 0.20, 0.08, "'ee' on F5, an 8th"),
    ('VoxChopOhA4', 69, (('oh', 0.035),), 0.30, 0.10, "'oh' on A4, a dotted 8th"),
    ('VoxChopEeAhC5', 72, (('ee', 0.035), ('ah', 0.16)), 0.30, 0.10, "'ee' opening to 'ah' on C5"),
    ('VoxChopAhOhA5', 81, (('ah', 0.035), ('oh', 0.40)), 0.60, 0.15, "'ah' closing to 'oh' on A5, a beat, with "
                                                                    'vibrato'),
)
CHOP_BY_NAME = {c[0]: c for c in CHOPS}


def vox_chop(take):
    _, root, path, hold, rel, _ = CHOP_BY_NAME[take.item.name]
    n = int((PRE + hold + rel + 0.2) * SR)
    y = sung(Note(PRE, PRE + hold, root, path, 1.0, rel), n, take.rng)
    return Out(y, f0s=ROOT_F0S(take.f0))


# ------------------------------------------------------------------ the riff
AH, EE, OH = 0.75, 1.0, 0.5               # the velocities that pick each vowel (LAYERS)
RIFF_STEPS = 32
RIFF = (    # (16th, MIDI, length in 16ths, velocity): bar 1 then bar 2
    (0, 74, 3, AH), (3, 77, 3, EE), (6, 76, 2, OH),                # D5 F5 E5: a third under the arp's F5 A5 G5
    (10, 74, 1, AH), (12, 72, 2, OH),                               # D5, C5: the answer
    (16, 74, 3, AH), (19, 77, 3, EE), (22, 76, 2, OH),              # the motif again...
    (24, 81, 2, EE), (26, 72, 2, AH),                               # ...lifting to A5, C5
    (28, 69, 4, AH),                                                # the throw: A4 held a beat into the echo
)
THROW_AT = 28
GATE = 0.85                               # each note's key is down for this share of its length (articulated)
RELEASE = 0.05
ECHO = dict(steps=3, taps=5, fb=0.5, first=0.6, shift_hz=-5.0, hp_hz=400.0, lp_hz=6000.0)


def riff_levels():
    """Each note's level: the multisample's layer level for its velocity."""
    return {s: D.undb(LAYER_DB[LAYERS.index(v)]) for s, _, _, v in RIFF}


def sung_parts(grid, at, length, rng, pre=0.05):
    """The riff on `grid` from 16th `at`, as two loops of `length` samples: the notes before the throw, and the throw
    alone (so a loop can send it on its own). Each note is sung on its own, its key down for GATE of its length, and
    placed with its consonant before its 16th, its tail wrapping round."""
    lv = riff_levels()
    main = np.zeros((length, 2))
    throw = np.zeros((length, 2))
    for s, m, ln, v in RIFF:
        dur = GATE * ln * grid.s16 / SR
        n = int((pre + dur + RELEASE + 0.3) * SR)
        nt = Note(pre, pre + dur, m, ((vowel_of(v), ONSET['open_s']),), 1.0, RELEASE)
        y = sung(nt, n, lambda *k: rng('riff', s, *k)) * lv[s]
        Ph.place(throw if s == THROW_AT else main, y, (grid.at(at + s) - int(pre * SR)) % length)
    return main, throw


def echo(x, length):
    """The throw's ping-pong on a dotted 8th, each repeat band-limited and frequency-shifted a little further down (the
    kit's laser falling away), wrapping round a loop of `length` samples. Mono sum in, stereo out, dry not included."""
    e = ECHO
    d = int(np.floor(e['steps'] * K.STEP * SR + 0.5))
    tap = D.mono(x)
    out = np.zeros((length, 2))
    for k in range(1, e['taps'] + 1):
        tap = D.freq_shift(D.bfilt(D.bfilt(tap, e['lp_hz'], 'lowpass'), e['hp_hz'], 'highpass'), e['shift_hz'])
        tap = tap * (e['first'] if k == 1 else e['fb'])
        side = (k - 1) % 2
        y = np.zeros((len(tap), 2))
        y[:, side] = tap
        y[:, 1 - side] = 0.25 * tap
        Ph.place(out, y, k * d)
    return out


@functools.lru_cache(maxsize=None)
def riff_loop():
    """The 2-bar riff as a loop: the notes, the throw and its echo."""
    g = Ph.Grid(K, RIFF_STEPS)
    main, throw = sung_parts(g, 0, g.length, lambda *k: D.rng(K.KIT, 'VoxSynthRiff', *k))
    return main + throw + echo(throw, g.length)


def vox_riff(take):
    meta = {'bars': 2, 'bpm': K.BPM, 'swing': K.SWING, 'throw_step': THROW_AT,
            'notes': [{'step': s, 'note': D.note_name(m), 'length_16ths': ln, 'velocity': v, 'vowel': vowel_of(v)}
                      for s, m, ln, v in RIFF],
            'echo': {**ECHO, 'on': 'the throw'}}
    return Out(riff_loop(), meta=meta)


def riff_midi():
    """The riff as MIDI notes for VoxSynth (the velocity picks the vowel): (16th, MIDI, length, velocity, slide)."""
    return [(s, m, ln, v, False) for s, m, ln, v in RIFF]


VOX_QA = {'tonal': False, 'tune_window': (0.06, 0.24),
          'why': 'a sung synth voice: breath noise, jitter and the sparkle sit between the harmonics, so no clean '
                 'sustain for the alias check; the tuning reads the steady note before the vibrato starts'}
CHOP_QA = {**VOX_QA, 'tune_window': (0.05, 0.18)}
ITEMS = [
    Item('Vox', 'VoxSynth', 'lead', vox_note, layers=LAYERS, notes=ZONES, sets=R5, qa=VOX_QA,
         about="vox synth, a wordless vocoder choir from formant synthesis: a glottal-like buzz through five formants, "
               "a 'd' onset, vibrato, jitter, breath, a detuned double and a frequency-shifted sparkle; the velocity "
               "picks the vowel (soft 'oh', medium 'ah', hard 'ee'); D4 F4 A4 C5 F5 A5"),
] + [
    Item('Vox', name, 'lead', vox_chop, root=root, sets=R5, qa=CHOP_QA, about=f'vox synth chop: {about}')
    for name, root, _, _, _, about in CHOPS
] + [
    Item('Loops', 'VoxSynthRiff', 'loop', vox_riff, loop=True, level=-16.0, sets=R5,
         qa={'mud': 'lowcut', 'why': 'a synth riff loop: the low cut, like its sound'},
         about='vox synth riff, 2 bars at 130 BPM in D minor: a third under the chrome arp on its 3-3-2 accents, an '
               'answer, and a throw into a falling ping-pong'),
]
