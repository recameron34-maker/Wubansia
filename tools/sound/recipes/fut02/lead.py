"""FUT02 "Orbit Relay" lead for listening rounds 2 and 3: the chrome arp, as multisamples (D4 to B-flat 5, every
fourth) and an original 2-bar 16th arpeggio played on it, the kit's signature melodic element.

The voice, one note:
- the body: a band-limited pulse (35% width, PolyBLEP at 4x) through a 4-pole low-pass that plucks open on the attack,
  brighter and longer with velocity;
- the chrome: seven sines on the kit's stretched partials (K.RATIOS, the stiff wire), each decaying faster the higher
  it sits. They sit 2 to 31 cents over the body's harmonics 2-7 and beat slowly against them: the kit's shimmer;
- the laser: the chrome starts a fourth up and drops onto the note in about 10 ms, the kit's pitch-drop gesture in
  miniature, heard as a "pew" on the attack;
- the width: the voice again a few cents sharp and a few cents flat, half their difference high-passed at 1 kHz on the
  side (UNISON): L = voice + side, R = voice - side. The mono sum is the voice exactly, and each channel hears a
  three-voice unison whose beating differs left and right: width from a decorrelated source, not a comb. It starts
  in the middle with each note (the copies start in phase) and spreads as they drift apart. Round 3's first pass
  doubled an all-pass side instead (0.70, round 2 0.35): every channel then played a deep complementary comb above
  1 kHz (L against the mid -10.2 to +4.2 dB), and the all-pass's direct term (-0.6 times its input) put an
  anti-phase copy of the voice on the side, so the arp's top leaned 4.4 dB to the right (round 2: 2.8 dB) and its
  1 kHz region 3.8 dB to the left. In the phrase the side's polarity alternates note by note: the sharp and flat
  copies leave the body's plucked low-pass not quite alike, so their difference keeps a small image of the voice,
  always the same way (the chrome's alone has none), which alone would lean the phrase 0.9 dB to the right, most of
  it at 1-1.6 kHz, near that filter's resting cutoff. A multisample note can't alternate: each is within 0.6 dB
  left to right above 3 kHz and 1 dB over the whole file.
High-passed at 150 Hz and mono under 150 Hz: nothing competes with the kick and bass.

The phrase, 2 bars of 16ths at 130 BPM in D minor, on the kit's one swing: a melody on a 3-3-2 accent pattern (the
tresillo, steps 0, 3 and 6 of every half bar) over a D5-A4 drone that fills the 16ths in between. Bar 1 states the
motif and falls (F5 A5 G5 | F5 E5 C5, the C5 held); bar 2 starts it again and lifts instead (F5 A5 G5 | C6 A5 E5, the
E5 held, a semitone under the F5 the loop restarts on). The filter opens a little over the 2 bars, so bar 2 answers
brighter. Every note on phrase.Grid, so its odd 16ths land with the hats'.

Provenance: new for FUT02, on the shared parts; round 2's all-pass width followed FUT01's chrome pluck
(recipes/fut01/music.py), and the detuned side replaces it.
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
UNISON = (7.0, 1.0, 1000.0)         # the width: the two copies' detune (cents), the side's level against half their
                                    # difference, its high-pass (Hz, 2nd order, zero phase). At 1.0 the phrase's side
                                    # sits about 3 dB under its mid at 4-8 kHz, round 3's width
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


def detuned(f, *args, **kw):
    """A note's side source: half the difference of the voice UNISON's cents sharp and as many flat (the other
    arguments are note_voice's). It is zero at the note's start, where the copies are in phase, and grows as they
    drift apart."""
    c = 2.0 ** (UNISON[0] / 1200.0)
    return 0.5 * (note_voice(f * c, *args, **kw) - note_voice(f / c, *args, **kw))


def stereo(y, d, loop=False):
    """The mono voice y as mid and its detuned difference d (detuned()) as side, above UNISON's high-pass: the mono sum
    is y exactly. loop: y and d are a loop, so the high-pass runs round it."""
    _, level, hp = UNISON
    hpf = lambda u: D.bfilt(u, hp, 'highpass', order=2, zero_phase=True)
    side = level * (Ph.circular(hpf, d) if loop else hpf(d))
    out = np.stack([y + side, y - side], 1)
    return D.mono_below(D.bfilt(out, HP, 'highpass', order=2), 150.0)


# ------------------------------------------------------------------ the multisamples
ARP_NOTES = (62, 67, 72, 77, 82)    # D4 G4 C5 F5 B-flat 5: every fourth, each a note of the key
ARP_LAYERS = (0.55, 1.0)
NOTE_S = 2.4                        # rings out; the last 0.6 s fade it to silence


def arp_note(take):
    """One note ringing out. Its mono sum is the voice alone, so the fundamentals the alias check reads are the note's
    and its chrome's; the detuned copies live on the side only."""
    n = int(NOTE_S * SR)
    tail = D.taper(n, NOTE_S - 0.6, 0.6)
    g = P.vel_gain(take.vel, 6.0, v_min=0.55)
    y = note_voice(take.f0, take.vel, n, None, 0.0) * tail
    d = detuned(take.f0, take.vel, n, None, 0.0) * tail
    return Out(stereo(y * g, d * g), f0s=(take.f0,) + chrome_partials(take.f0))


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
    it loops. Each note's side (its detuned difference) flips polarity from one note to the next (see the docstring)."""
    L = G.length
    y, d = np.zeros(L), np.zeros(L)
    for j, (step, note, length, vel) in enumerate(phrase_notes()):
        at = G.at(step)
        n = int((length * K.STEP + RELEASE + 0.01) * SR)
        open_ = 1.0 + SWELL * (0.5 - 0.5 * np.cos(np.pi * at / L))
        f, g = float(D.midi_hz(note)), P.vel_gain(vel, 9.0, v_min=0.5)
        idx = (at + np.arange(n)) % L
        np.add.at(y, idx, note_voice(f, vel, n, length * K.STEP, RELEASE, open_) * g)
        np.add.at(d, idx, (-1) ** j * detuned(f, vel, n, length * K.STEP, RELEASE, open_) * g)
    meta = {'bpm': K.BPM, 'swing': K.SWING, 'steps': PHRASE_STEPS, 'accents': '3-3-2 per half bar',
            'notes': [{'step': s, 'note': D.note_name(m), 'length_16ths': ln, 'velocity': round(v, 2)}
                      for s, m, ln, v in phrase_notes()],
            'width': {'unison_cents': UNISON[0], 'side_level': UNISON[1], 'side_hp_hz': UNISON[2],
                      'side_polarity': 'alternates note by note'}}
    return Out(stereo(y, d, loop=True), meta=meta)


ITEMS = [
    Item('Lead', 'ChromeArp', 'lead', arp_note, layers=ARP_LAYERS, notes=ARP_NOTES, sets=('r2', 'r3', 'r5'),
         about='chrome arp: a plucked pulse body under seven sines on the kit ratios that drop a fourth onto the note '
               '(the laser), wide from a detuned unison on its side, D4 to B-flat 5 every fourth'),
    Item('Loops', 'ChromeArpPhrase', 'loop', arp_phrase, loop=True, sets=('r2', 'r3', 'r5'),
         qa={'mud': 'lowcut', 'why': 'a lead loop: the low cut, like the lead'},
         about='2-bar 16th chrome arp in D minor at 130 BPM: a tresillo melody (F5 A5 G5 | F5 E5 C5, then lifting to '
               'C6 A5 E5) over a D5-A4 drone'),
]
