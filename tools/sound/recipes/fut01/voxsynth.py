"""FUT01 round 5 vox synth (set r5): a wordless formant "vocal synth" built from synthesis alone (no speech engine, no
models, no words): the bright, pitched-up vowel chop that leads current UK garage, in F# minor.

The voice (voice()):
- the source: additive harmonics with random phases, exactly band-limited at FMAX (a pitch move can't alias), on a
  spectral tilt (a saw's -6 dB an octave on the hard layer, steeper and darker on the soft one), plus a broad body on
  the fundamental; its pitch the note with a quick scoop up into it, gentle vibrato that fades in on held notes and a
  little jitter (a slow random wander of a few cents);
- the vowel: five formants per vowel (FORMANTS: F1-F5, each a band-pass shape with its own width and level), moved up
  12 % for the pitched-up chop character, gliding between vowels in log frequency; every harmonic is weighted by the
  formant envelope at its own frequency, sample by sample, so vowels move while the pitch moves;
- breath: noise pulsed at the voice's pitch through the same moving formants (an STFT filter) with a little air on top;
  the soft layer breathier;
- the onset: a short consonant-like start: 'd' (an 8 ms noise burst at 2.5-7 kHz, the first formant opening over
  25 ms) or none (a soft 5 ms rise);
- the double: two quieter copies 9 cents sharp and flat, 7 and 12 ms late, their formants 3 % apart, one each side;
- a gentle soft clip, then a 4th-order high-pass at 280 Hz: far over the kick, the bass and the low mids.

Items:
- VoxSynth: the playable multisample, an open "ah" held 1.1 s (vibrato from 0.3 s), on A4, C#5 and F#5 (the key's
  third, fifth and root), two layers (soft: darker and breathier; hard: brighter).
- Five single chops, each gated on the grid on a stated root: "ah" on C#5 (an 8th), "ee" on E5 (a 16th), "ah" to
  "ee" on F#5 (a dotted 8th), "oh" on A4 scooping up a semitone (an 8th), "ah" to "oh" on B4 falling a tone as it
  ends (a dotted 8th).
- VoxSynthRiff: the 2-bar riff (RIFF) on the voice at exact pitches, as music.py's pluck riff plays its pluck: off-beat
  chops answering the warp bass over F#m9 then Bm9, on the kit's swing (0.59), the last chop thrown into a dotted-8th
  ping-pong and a short plate whose tail wraps into bar 1. Its notes are the MIDI phrase on channel 4 (midi.py).
"""
import numpy as np
from scipy import signal

from tools.sound import dsp48 as D
from tools.sound import parts as P
from tools.sound import phrase as Ph
from tools.sound import sends as S
from tools.sound.item import Item, Out
from tools.sound.kits import fut01 as K

SR = D.SR
FMAX = 15000.0                      # the harmonics stop here at the highest pitch the note reaches
FORMANT_SCALE = 1.12                # every formant 12 % up: the pitched-up chop's smaller, brighter voice
# (Hz, Q, level dB) for F1-F5 of each vowel, before FORMANT_SCALE
FORMANTS = {
    'ah': ((800.0, 5.0, 0.0), (1150.0, 6.0, -4.0), (2800.0, 8.0, -15.0), (3500.0, 9.0, -19.0), (4950.0, 10.0, -28.0)),
    'ee': ((310.0, 4.0, 0.0), (2300.0, 7.0, -7.0), (3000.0, 8.0, -9.0), (3700.0, 9.0, -17.0), (4950.0, 10.0, -26.0)),
    'oh': ((450.0, 5.0, 0.0), (800.0, 6.0, -5.0), (2830.0, 8.0, -19.0), (3500.0, 9.0, -23.0), (4950.0, 10.0, -32.0)),
}
FLOOR = 0.05                        # the envelope between formants (-26 dB): the synth's own saw under the vowel
BODY = (0.7, 1.4)                   # the fundamental's broad body: level, Q (centred on the pitch)
HP = 280.0                          # the voice's high-pass (4th order)
DOUBLE = dict(cents=9.0, delays=(0.007, 0.012), formant=0.03, db=-9.0)
VIBRATO = dict(rate=5.5, depth=0.20, rise=0.30)   # Hz, semitones (+-), seconds to full depth from its start
JITTER_CENTS = 1.5                  # the slow random wander, RMS
LAYERS = (0.6, 1.0)
ROOTS = (69, 73, 78)                # A4, C#5, F#5: the multisample's zones
STEP = K.STEP


def hz(note):
    return float(D.midi_hz(note))


def smooth(u):
    u = np.clip(u, 0.0, 1.0)
    return 0.5 - 0.5 * np.cos(np.pi * u)


def resonance(f, fc, q):
    """A band-pass's magnitude, 1 at fc: 1 / sqrt(1 + q^2 (f/fc - fc/f)^2)."""
    f = np.maximum(f, 1.0)
    return 1.0 / np.sqrt(1.0 + (q * (f / fc - fc / f)) ** 2)


def formant_tracks(path, n, scale=1.0):
    """The five formants' (Hz, Q, linear level) per sample along a vowel path: ((seconds, vowel), ...), each vowel
    reached at its time, raised-cosine glides between them in log frequency and dB."""
    t = P.times(n)
    F, Q, G = np.zeros((3, 5, n))
    keys = [(s, FORMANTS[v]) for s, v in path]
    for j in range(5):
        lf = np.full(n, np.log(keys[0][1][j][0]))
        qq = np.full(n, keys[0][1][j][1])
        gg = np.full(n, keys[0][1][j][2])
        for (s0, a), (s1, b) in zip(keys, keys[1:]):
            w = smooth((t - s0) / max(s1 - s0, 1e-6))
            lf = np.where(t >= s0, np.log(a[j][0]) + (np.log(b[j][0]) - np.log(a[j][0])) * w, lf)
            qq = np.where(t >= s0, a[j][1] + (b[j][1] - a[j][1]) * w, qq)
            gg = np.where(t >= s0, a[j][2] + (b[j][2] - a[j][2]) * w, gg)
        F[j], Q[j], G[j] = np.exp(lf) * FORMANT_SCALE * scale, qq, D.undb(gg)
    return F, Q, G


def envelope(f, F, Q, G, idx=None):
    """The vowel's envelope at frequencies f (per sample, or a column of bins against frames idx)."""
    if idx is None:
        return FLOOR + sum(G[j] * resonance(f, F[j], Q[j]) for j in range(5))
    return FLOOR + sum(G[j, idx][None, :] * resonance(f[:, None], F[j, idx][None, :], Q[j, idx][None, :])
                       for j in range(5))


def pitch_track(note, n, r, gate_s, scoop=0.6, scoop_tau=0.014, vib_start=0.30, fall=0.0, rel_s=0.05, cents=0.0):
    """The voice's pitch, Hz per sample: the note (cents off it, for the double) with a scoop from `scoop` semitones
    under, vibrato from vib_start, the jitter, and a fall of `fall` semitones over the release."""
    t = P.times(n)
    semis = -scoop * np.exp(-t / scoop_tau) + cents / 100.0
    v = VIBRATO
    semis = semis + v['depth'] * smooth((t - vib_start) / v['rise']) * np.sin(2 * np.pi * v['rate'] * t
                                                                               + r.uniform(0, 2 * np.pi))
    pre = int(0.2 * SR)
    jit = D.bfilt(r.standard_normal(n + pre), 8.0, 'lowpass', order=2)[pre:]
    semis = semis + jit * (JITTER_CENTS / 100.0) / (np.std(jit) + 1e-12)
    if fall:
        semis = semis - fall * smooth((t - gate_s) / rel_s)
    return hz(note) * 2.0 ** (semis / 12.0)


def voiced(f0, F, Q, G, tilt, r):
    """The harmonics: each k f0 weighted by k^-tilt and the formant envelope at its own frequency, sample by sample;
    the body on the fundamental; random phases; nothing over FMAX (a taper over its last 15 %)."""
    n = len(f0)
    ph = 2 * np.pi * D.phase(f0, n)
    y = np.zeros(n)
    for k in range(1, int(FMAX // f0.max()) + 1):
        fk = k * f0
        e = envelope(fk, F, Q, G) + BODY[0] * resonance(fk, f0, BODY[1])
        edge = np.clip((FMAX - fk) / (0.15 * FMAX), 0.0, 1.0)
        y += k ** -tilt * e * edge * np.sin(k * ph + r.uniform(0, 2 * np.pi))
    return y


def breath(f0, F, Q, G, r, n, air):
    """Noise pulsed at the voice's pitch through the moving formants (STFT, 512 / hop 128) plus `air` over 6 kHz."""
    x = r.standard_normal(n) * (0.55 + 0.45 * (0.5 + 0.5 * np.cos(2 * np.pi * D.phase(f0, n))))
    f, tt, X = signal.stft(x, SR, nperseg=512, noverlap=384, boundary='even')
    idx = np.clip(np.round(tt * SR).astype(int), 0, n - 1)
    air = air * smooth((f - 6000.0) / 4000.0) * np.clip((16000.0 - f) / 4000.0, 0.0, 1.0)
    _, y = signal.istft(X * (envelope(f, F, Q, G, idx) + air[:, None]), SR, nperseg=512, noverlap=384, boundary=True)
    return y[:n]


def onset_env(n, onset):
    """(the voice's amplitude rise, the burst): 'd' rises from 4 ms over 6 ms after an 8 ms burst; None over 5 ms."""
    t = P.times(n)
    if onset == 'd':
        return smooth((t - 0.004) / 0.006), np.exp(-np.maximum(t - 0.0005, 0.0) / 0.003) * smooth(t / 0.0005)
    return smooth(t / 0.005), np.zeros(n)


def release(n, gate_s, rel_s):
    """Held to gate_s, then an exponential fall (-26 dB at rel_s), ending at exactly 0 ten time constants on."""
    t = P.times(n)
    tau = rel_s / 3.0
    return np.exp(-np.maximum(t - gate_s, 0.0) / tau) * D.taper(n, gate_s + 9.0 * tau, tau)


def one_voice(note, vel, gate_s, path, onset, r, rel_s, scoop, vib_start, fall, cents=0.0, scale=1.0):
    """One voice (mono): harmonics and breath under the amplitude envelope, the 'd' onset's formant opening. The hard
    layer brighter (a flatter tilt, more air), the soft one darker and breathier."""
    n = int((gate_s + 3.5 * rel_s + 0.05) * SR)
    up = np.clip((vel - 0.5) / 0.5, 0.0, 1.0)
    tilt = 1.4 - 0.45 * up
    F, Q, G = formant_tracks(path, n, scale)
    if onset == 'd':
        F[0] = F[0] * (0.6 + 0.4 * smooth(P.times(n) / 0.025))       # the first formant opening: a closure released
    f0 = pitch_track(note, n, r, gate_s, scoop=scoop, vib_start=vib_start, fall=fall, rel_s=rel_s, cents=cents)
    rise, burst = onset_env(n, onset)
    amp = rise * release(n, gate_s, rel_s)
    v = voiced(f0, F, Q, G, tilt, r) * amp
    b = breath(f0, F, Q, G, r, n, 0.1 + 0.25 * up) * amp
    rms = lambda u: np.sqrt(np.mean(u ** 2) + 1e-20)
    y = v + b * rms(v) / rms(b) * D.undb(-15.0 - 4.0 * up)
    if onset == 'd':
        hit = P.band_noise(r, n, 2500.0, 7000.0) * burst
        y = y + hit * 0.35 * np.abs(v).max() / (np.abs(hit).max() + 1e-12)
    return y


def voice(note, vel, gate_s, path, onset, r, rel_s=0.05, scoop=0.6, vib_start=0.30, fall=0.0):
    """The vox synth, stereo: the voice in the centre, the double each side, soft-clipped, high-passed at HP; its
    level rising with the velocity."""
    up = np.clip((vel - 0.5) / 0.5, 0.0, 1.0)
    args = (note, vel, gate_s, path, onset)
    main = one_voice(*args, r, rel_s, scoop, vib_start, fall)
    n = len(main)
    y = np.zeros((n, 2))
    y += main[:, None]
    d = DOUBLE
    for c, (sign, delay) in enumerate(zip((1.0, -1.0), d['delays'])):
        dbl = one_voice(*args, r, rel_s, scoop, vib_start, fall, cents=sign * d['cents'],
                        scale=1.0 + sign * d['formant'])
        k = int(delay * SR)
        y[k:, c] += D.undb(d['db']) * dbl[:n - k]
    peak = np.abs(y).max()
    y = P.soft_clip(y / peak, 1.2 + 0.8 * up) * peak
    y = D.mono_below(D.bfilt(y, HP, 'highpass', order=4), 150.0)
    return y * (0.3 + 0.7 * vel)


# ------------------------------------------------------------------ the multisample
def vox_synth(take):
    """An open "ah" on the zone, held 1.1 s and released over 0.25 s, vibrato from 0.3 s."""
    r = take.rng('voice')
    return Out(voice(take.note, take.vel, 1.1, ((0.0, 'ah'),), 'd', r, rel_s=0.25), f0s=(take.f0,))


# ------------------------------------------------------------------ the single chops
# (name, note, gate in 16ths, vowel path, onset, scoop semitones, fall semitones, about)
CHOPS = (
    ('VoxChopAhCs5', 73, 2, ((0.0, 'ah'),), 'd', 0.6, 0.0, '"ah" on C#5, an 8th'),
    ('VoxChopEeE5', 76, 1, ((0.0, 'ee'),), 'd', 0.5, 0.0, '"ee" on E5, a 16th'),
    ('VoxChopAhEeFs5', 78, 3, ((0.0, 'ah'), (0.06, 'ah'), (0.20, 'ee')), None, 0.6, 0.0,
     '"ah" opening to "ee" on F#5, a dotted 8th'),
    ('VoxChopOhA4', 69, 2, ((0.0, 'oh'),), 'd', 1.0, 0.0, '"oh" on A4 scooping up a semitone, an 8th'),
    ('VoxChopAhOhB4', 71, 3, ((0.0, 'ah'), (0.05, 'ah'), (0.16, 'oh')), 'd', 0.6, 2.0,
     '"ah" to "oh" on B4, a dotted 8th, falling a whole tone as it ends'),
)
CHOP_BY_NAME = {c[0]: c for c in CHOPS}
CHOP_REL = 0.06
CHOP_VEL = 0.9


def chop(take):
    name, note, length, path, onset, scoop, fall, _ = CHOP_BY_NAME[take.item.name]
    y = voice(note, CHOP_VEL, length * STEP, path, onset, take.rng('voice'), rel_s=CHOP_REL, scoop=scoop,
              vib_start=10.0, fall=fall)
    return Out(y, f0s=(hz(note),))


def tune_window(length, onset):
    """The chop's steady stretch: from 45 ms (the scoop settled) to its gate's end."""
    return (0.045, round(length * STEP, 3))


# ------------------------------------------------------------------ the riff
FS5, E5, D5, CS5, B4, A4 = 78, 76, 74, 73, 71, 69
AH, EE, OH = ((0.0, 'ah'),), ((0.0, 'ee'),), ((0.0, 'oh'),)
AH_EE = ((0.0, 'ah'), (0.04, 'ah'), (0.14, 'ee'))
AH_OH = ((0.0, 'ah'), (0.04, 'ah'), (0.13, 'oh'))
# (16th, MIDI note, length in 16ths, velocity, vowel path, onset): bar 1 over F#m9 climbs C#5-E5-F#5 in the bass's
# gaps, bar 2 over Bm9 answers down D5-C#5-B4-A4; every chop off the beat; the last one is the throw
RIFF = (
    (6, CS5, 1, 0.8, AH, 'd'), (9, E5, 1, 0.85, EE, 'd'), (10, FS5, 2, 1.0, AH_EE, None), (15, E5, 1, 0.7, OH, 'd'),
    (22, D5, 1, 0.85, AH, 'd'), (25, CS5, 1, 0.75, EE, 'd'), (26, B4, 2, 0.95, AH_OH, 'd'), (30, A4, 2, 0.9, OH, 'd'),
)
RIFF_NOTES = tuple((s, n, ln, v, False) for s, n, ln, v, _, _ in RIFF)   # midi.py's phrase notes
RIFF_STEPS = 32
ONSET_LEAD = {'d': 0.006, None: 0.0}   # the onset before the vowel: a chop starts this early, its vowel on the beat
THROW_ECHO = dict(S.ECHO, feedback=0.45, hp_hz=600.0, lp_hz=4500.0)
THROW_PLATE = dict(S.SPACE, t60=1.2, size=0.5, damp_hz=9000.0, predelay=round(STEP / 4, 6), send_hp=600.0,
                   t60_low=0.4, shimmer=0.03)
THROW_MIX = (0.55, 0.22)            # the echo and plate returns
THROW_TAIL_S = 3.0


def riff_chop(i):
    """The riff's i-th chop: (audio, onset lead in samples)."""
    step, note, length, vel, path, onset = RIFF[i]
    r = D.rng(K.KIT, 'VoxSynthRiff', i)
    y = voice(note, vel, length * STEP, path, onset, r, rel_s=CHOP_REL, scoop=0.6 if note != A4 else 1.0,
              vib_start=10.0)
    return y, int(np.floor(ONSET_LEAD[onset] * SR + 0.5))


def throw(y):
    """A chop's throw: the dotted-8th ping-pong and a short plate, run out over THROW_TAIL_S (returns only)."""
    m = D.mono(y)
    tail = int(THROW_TAIL_S * SR)
    echo = S.echo(m, K.space_rng('vox synth throw echo'), S.ECHO_STEPS * STEP, n_tail=tail, **THROW_ECHO)
    plate = S.space(m, K.space_rng('vox synth throw plate'), n_tail=tail, **THROW_PLATE)[:len(m) + tail]
    return THROW_MIX[0] * echo + THROW_MIX[1] * plate


def riff(take):
    """The riff as a 2-bar loop: each chop on its 16th (its vowel on the grid), the last one with its throw; every tail
    wraps round the loop."""
    g = Ph.Grid(K, RIFF_STEPS)
    buf = np.zeros((g.length, 2))
    for i, (step, *_) in enumerate(RIFF):
        y, lead = riff_chop(i)
        at = g.at(step) - lead
        Ph.place(buf, y, at)
        if i == len(RIFF) - 1:
            Ph.place(buf, D.mono_below(D.bfilt(throw(y), HP, 'highpass', order=4), 150.0), at)
    return Out(buf, meta={'bars': 2, 'bpm': K.BPM, 'swing': K.SWING,
                          'notes': [{'step': s, 'note': D.note_name(n), 'length_16ths': ln, 'velocity': v,
                                     'vowels': [w for _, w in p], 'onset': o} for s, n, ln, v, p, o in RIFF],
                          'throw': {'step': RIFF[-1][0], 'echo_16ths': S.ECHO_STEPS}})


R5 = ('r5',)
WHY = ("a wordless synth voice (no script, no models): tuned on its steady stretch; no alias check, since its breath "
       "noise and vibrato are meant (its harmonics are additive, band-limited at 15 kHz)")
ITEMS = [
    Item('Vox', 'VoxSynth', 'vox', vox_synth, layers=LAYERS, notes=ROOTS, sets=R5, level=-18.0,
         tags=('synth',), qa={'tune_cents': 5.0, 'tune_window': (0.06, 0.30), 'why': WHY},
         about='vox synth, a wordless formant vocal synth: an open "ah" on A4, C#5 and F#5, held 1.1 s with vibrato '
               'from 0.3 s, a detuned double, high-passed at 280 Hz; soft layer darker and breathier'),
] + [
    Item('Vox', name, 'vox', chop, root=note, sets=R5, level=-18.0, tags=('synth', 'chop'),
         qa={'tune_cents': 5.0, 'tune_window': tune_window(length, onset), 'why': WHY},
         about=f'vox synth chop, {about}: synth vowels, no words')
    for name, note, length, path, onset, scoop, fall, about in CHOPS
] + [
    Item('Loops', 'VoxSynthRiff', 'loop', riff, loop=True, level=-16.0, sets=R5, tags=('synth',),
         qa={'mud': 'lowcut', 'why': 'a vox synth loop: the low cut, like its sound'},
         about='phrase, 2-bar vox synth riff in F# minor at 134 BPM: off-beat vowel chops answering the warp bass over '
               'F#m9 then Bm9, swung, the last chop thrown into a dotted-8th ping-pong'),
]
