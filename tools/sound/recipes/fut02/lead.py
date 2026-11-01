"""FUT02 "Orbit Relay" lead for listening round 2: the chrome arp, as multisamples (D4 to B-flat 5, every fourth) and an
original 2-bar 16th arpeggio played on it, the kit's signature melodic element.

The voice, one note:
- the body: a band-limited pulse (35% width, PolyBLEP at 4x) through a 4-pole low-pass that plucks open on the attack,
  brighter and longer with velocity;
- the chrome: seven sines on the kit's stretched partials (K.RATIOS, the stiff wire), each decaying faster the higher
  it sits. They sit 2 to 31 cents over the body's harmonics 2-7 and beat slowly against them: the kit's shimmer;
- the laser: the chrome starts a fourth up and drops onto the note in about 10 ms, the kit's pitch-drop gesture in
  miniature, heard as a "pew" on the attack;
- the width: the chrome above 1 kHz again on the side, through an all-pass, so the mono sum is the voice whole (no
  harmonic cancels in mono) while the shimmer spreads.
High-passed at 150 Hz and mono under 150 Hz: nothing competes with the kick and bass.

The phrase, 2 bars of 16ths at 130 BPM in D minor, on the kit's one swing: a melody on a 3-3-2 accent pattern (the
tresillo, steps 0, 3 and 6 of every half bar) over a D5-A4 drone that fills the 16ths in between. Bar 1 states the
motif and falls (F5 A5 G5 | F5 E5 C5, the C5 held); bar 2 starts it again and lifts instead (F5 A5 G5 | C6 A5 E5, the
E5 held, a semitone under the F5 the loop restarts on). The filter opens a little over the 2 bars, so bar 2 answers
brighter. Every note on phrase.Grid, so its odd 16ths land with the hats'.

Provenance: new for FUT02; the all-pass width follows FUT01's chrome pluck (recipes/fut01/music.py), rebuilt here on
the shared parts.
"""
import numpy as np

from tools.sound import dsp48 as D
from tools.sound import parts as P
from tools.sound import phrase as Ph
from tools.sound.item import Item, Out
from tools.sound.kits import fut02 as K

SR = D.SR
HP = 150.0                          # nothing under ~150 Hz: the kick and bass own it
PULSE_W = 0.35                      # the body's pulse width
CHROME_GAIN = 0.55                  # the chrome against the body
LASER = (5.0, 0.004)                # the chrome starts this many semitones up and drops with this time constant (s)
SIDE = (0.35, 61, 0.6, 1000.0)      # the width: level, all-pass delay (samples, about 1.3 ms) and coefficient, the
                                    # side's high-pass (Hz)
AMP_TAU = (0.30, 0.45)              # the body's decay, soft to hard layer (s)


def chrome_partials(f):
    return tuple(f * r for r in K.RATIOS if f * r < 20000)


def note_voice(f, vel, n, gate_s, release_s, open_=1.0):
    """One arp note, mono: body plus chrome, key down for gate_s seconds (None: let it ring), then released over
    release_s. open_ scales the pluck's cutoff (the phrase's swell)."""
    t = P.times(n)
    k = 4
    saw = lambda ph: D.polyblep_saw(f, n, ph, k=k)
    pulse = D.down(saw(0.0) - saw(PULSE_W), k)
    pulse = pulse - np.mean(pulse)
    peak = (2200.0 + 7000.0 * vel) * open_
    rest = (900.0 + 700.0 * vel) * open_
    fc = rest + (peak - rest) * np.exp(-t / (0.06 + 0.06 * vel))
    body = D.svf_tpt(D.svf_tpt(pulse, fc, q=0.6)[0], fc, q=0.9)[0]
    amp = np.exp(-t / P.lerp(AMP_TAU, vel))
    m = np.log2(f / 440.0) * 12 + 69 + LASER[0] * np.exp(-t / LASER[1])
    fm = D.midi_hz(m)
    ph = D.phase(fm, n)
    chrome = np.zeros(n)
    for i, r in enumerate(K.RATIOS):
        if f * r * 1.35 < 20000:
            tau = (0.22 + 0.25 * vel) / (1 + 0.55 * i)
            chrome += np.exp(-t / tau) * np.sin(2 * np.pi * r * ph) * (0.5 + 0.5 * vel) ** i / (1 + i)
    y = (body * amp + CHROME_GAIN * chrome) * P.raised(n, 0.0008)
    if gate_s is not None:
        y = y * D.taper(n, gate_s, release_s)
    return y


def stereo(y):
    """The mono voice as mid, and its top on the side through an all-pass: the mono sum is y exactly."""
    g, d, a, hp = SIDE
    side = g * D.bfilt(P.allpass(y, d, a), hp, 'highpass', order=2)
    out = np.stack([y + side, y - side], 1)
    return D.mono_below(D.bfilt(out, HP, 'highpass', order=2), 150.0)


# ------------------------------------------------------------------ the multisamples
ARP_NOTES = (62, 67, 72, 77, 82)    # D4 G4 C5 F5 B-flat 5: every fourth, each a note of the key
ARP_LAYERS = (0.55, 1.0)
NOTE_S = 2.4                        # rings out; the last 0.6 s fade it to silence


def arp_note(take):
    n = int(NOTE_S * SR)
    y = note_voice(take.f0, take.vel, n, None, 0.0) * D.taper(n, NOTE_S - 0.6, 0.6)
    return Out(stereo(y * P.vel_gain(take.vel, 6.0, v_min=0.55)), f0s=(take.f0,) + chrome_partials(take.f0))


# ------------------------------------------------------------------ the phrase
PHRASE_STEPS = 32
G = Ph.Grid(K, PHRASE_STEPS)
D5, A4 = 74, 69                     # the drone under the melody
# the melody on the tresillo accents (steps 0, 3, 6 of each half bar): (step, MIDI, length in 16ths)
MELODY = ((0, 77, 1.5), (3, 81, 1.5), (6, 79, 1.5),          # F5 A5 G5   bar 1: the motif...
          (8, 77, 1.5), (11, 76, 1.5), (14, 72, 2.0),         # F5 E5 C5   ...falling, the C5 held
          (16, 77, 1.5), (19, 81, 1.5), (22, 79, 1.5),        # F5 A5 G5   bar 2: the motif again...
          (24, 84, 1.5), (27, 81, 1.5), (30, 76, 2.0))        # C6 A5 E5   ...lifting, the E5 held into the loop's F5
ACCENT = 1.0
DRONE_VEL = {D5: 0.62, A4: 0.5}
DRONE_LEN = 0.8                     # the drone notes: short, so the melody's accents ring over them
RELEASE = 0.08
SWELL = 0.35                        # the filter opens by this much over the 2 bars


def phrase_notes():
    """Every note of the phrase: (step, MIDI, length in 16ths, velocity). The drone fills each half bar's other
    steps: D5 then A4 after an accent, except where a held melody note rings over it."""
    mel = {s: (m, ln) for s, m, ln in MELODY}
    out = []
    for s in range(PHRASE_STEPS):
        if s in mel:
            m, ln = mel[s]
            lift = 0.05 if s >= 16 else 0.0                 # bar 2 a touch harder
            out.append((s, m, ln, min(1.0, 0.9 + lift + (0.1 if s % 8 == 0 else 0.0))))
            continue
        held = any(a < s < a + ln for a, (_, ln) in mel.items() if ln >= 2)
        if held:
            continue
        last = max(a for a in mel if a < s)
        note = D5 if (s - last) % 3 == 1 else A4
        out.append((s, note, DRONE_LEN, DRONE_VEL[note] + (0.04 if s >= 16 else 0.0)))
    return out


def arp_phrase(take):
    """The phrase played on the voice at its exact pitches, each note's gate its length, then released; tails wrap, so
    it loops."""
    L = G.length
    y = np.zeros(L)
    for step, note, length, vel in phrase_notes():
        at = G.at(step)
        n = int((length * K.STEP + RELEASE + 0.01) * SR)
        open_ = 1.0 + SWELL * (0.5 - 0.5 * np.cos(np.pi * at / L))
        x = note_voice(float(D.midi_hz(note)), vel, n, length * K.STEP, RELEASE, open_)
        np.add.at(y, (at + np.arange(n)) % L, x * P.vel_gain(vel, 9.0, v_min=0.5))
    meta = {'bpm': K.BPM, 'swing': K.SWING, 'steps': PHRASE_STEPS, 'accents': '3-3-2 per half bar',
            'notes': [{'step': s, 'note': D.note_name(m), 'length_16ths': ln, 'velocity': round(v, 2)}
                      for s, m, ln, v in phrase_notes()]}
    return Out(stereo(y), meta=meta)


ITEMS = [
    Item('Lead', 'ChromeArp', 'lead', arp_note, layers=ARP_LAYERS, notes=ARP_NOTES, sets=('r2',),
         about='chrome arp: a plucked pulse body under seven sines on the kit ratios that drop a fourth onto the note '
               '(the laser), D4 to B-flat 5 every fourth'),
    Item('Loops', 'ChromeArpPhrase', 'loop', arp_phrase, loop=True, sets=('r2',),
         qa={'mud': 'lowcut', 'why': 'a lead loop: the low cut, like the lead'},
         about='2-bar 16th chrome arp in D minor at 130 BPM: a tresillo melody (F5 A5 G5 | F5 E5 C5, then lifting to '
               'C6 A5 E5) over a D5-A4 drone'),
]
