"""FUT02 "Orbit Relay" vocals for listening round 2: three sung candidate lines, the eight chops cut from the pick, and
the vocoder phrase on the 16th grid, dry and wet.

Every line is original and clean, written for this kit: 2 bars at 130 BPM in D minor, sung in the kit's female blend
(K.VOICE), on the kit's swing (K.SWING), each title-checked (the exact phrase with "song" and with "lyrics": no release
with that title or line) and sung whole once, dry, so Whisper checks it:
- SungLine1, the pick: "Pull me into orbit" (A4 C5 D5 C5, "or" held on A4 over beat 3, "bit" on G4 pushed onto the and
  of 4). A plain call to come closer, in the kit's own image; sung legato (no glottal stops: with them Whisper
  misheard "orbit" on 9 of 12 takes, as "a toilet" on 5)
- SungLine2: "Your light keeps calling me" (F4 A4, "keeps" pushed onto the swung 16th on C5, D5 C5, "me" held on A4)
- SungLine3: "Send your signal to me" (D5 C5 A4 G4 F4, "me" held on A4 from beat 3)
Takes: of draws 0-5 in either accent, each line's take is one Whisper hears word for word as rendered and again under
band noise 12 and 6 dB down: SungLine1 US draw 2 (the only take clean at both noise levels), SungLine2 US draw 4 (clean
at 12 dB), SungLine3 GB draw 1 (clean at 12 dB, 0.74 at 6 dB).
The eight chops (ChopPull ... ChopPullMeInto) are cut from SungLine1.

The vocoder phrase: "Relay online", male (K.ROBOT_VOICE), one syllable per slice on steps 0, 1, 4 and 6 of a bar
("re-LAY on-LINE": the stress on the swung e, then beat 2 and its and; the odd step on the kit's swing). WORLD resynthesises the syllables on a flat buzzing D3 and again on F3 and A3, the same spectral
envelope on all three: a D minor vocoder chord. Each syllable starts a fourth up and drops onto the chord in about 15
ms, the kit's laser gesture. The slice points are fixed by the grid (ROBOT_SLICES), so the sampler can fire slice k at
its step at any tempo; they are in each file's metadata.

Every file is a WORLD resynthesis at 48 kHz from the features of a 24 kHz Kokoro take in the kit's voice blend (ADR
0005, D4: hf_synthetic). The chops are cut in the frame domain (vocals.chop): each keeps its onset consonant whole and
ends in a breath tail, so no cut lands in the audio. Everything is high-passed at 100 Hz; the wet vocoder is mono under
150 Hz.

Rendering needs requirements-vocals.txt and the models (WUBANSIA_MODELS_DIR); importing this module does not.
"""
import functools
from dataclasses import dataclass, replace

import numpy as np
from scipy import signal

from tools.sound import dsp48 as D
from tools.sound import vocals as V
from tools.sound.item import Item, Out
from tools.sound.kits import fut02 as K

SR = D.SR
HP = 100.0                   # nothing under ~100 Hz competes with the kick and sub
VOX = frozenset({'hf_synthetic'})
WHY = {'why': 'WORLD resynthesis at 48 kHz of a 24 kHz Kokoro take: the top band is synthetic (extend_hf, D4)'}


# ------------------------------------------------------------------ three candidate lines, chops from the pick
@dataclass(frozen=True)
class Line:
    """One candidate line: its words, its notes ((16th step, length, MIDI, word); '~' carries the word on) and the
    Kokoro take it is sung from: the lexicon accent and the draw."""
    name: str
    text: str
    notes: tuple
    accent: str
    draw: int
    about: str
    glottal: bool = True     # a glottal stop before a vowel-initial word after the first (False: linked, legato)


LINE_HOW = V.Sing(speed=0.8, consonants=1.3)
LINE_PRE = 0.5               # seconds before step 0: the inhale and the first onset
LINE_LEN = 30 * K.STEP + 0.4
LINES = (
    Line('SungLine1', 'Pull me into orbit',
         ((0, 2, 69, 'pull'), (2, 2, 72, 'me'), (4, 2, 74, 'into'), (6, 2, 72, '~'), (8, 6, 69, 'orbit'),
          (14, 2, 67, '~')), 'us', 2,
         'A4 C5 D5 C5, "or" held on A4 over beat 3, "bit" on G4 pushed onto the and of 4; legato', glottal=False),
    Line('SungLine2', 'Your light keeps calling me',
         ((0, 2, 65, 'your'), (2, 3, 69, 'light'), (5, 3, 72, 'keeps'), (8, 2, 74, 'calling'), (10, 2, 72, '~'),
          (12, 10, 69, 'me')), 'us', 4,
         'F4 A4, "keeps" pushed onto the swung 16th on C5, D5 C5, "me" held on A4 from beat 4'),
    Line('SungLine3', 'Send your signal to me',
         ((0, 3, 74, 'send'), (3, 1, 72, 'your'), (4, 2, 69, 'signal'), (6, 2, 67, '~'), (8, 2, 65, 'to'),
          (10, 10, 69, 'me')), 'gb', 1,
         'D5 C5 A4 G4 F4, "me" held on A4 from the and of 3'),
)
LINE_BY_NAME = {ln.name: ln for ln in LINES}
PICK = 'SungLine1'           # the line the chops are cut from


@functools.lru_cache(maxsize=None)
def line_track(name):
    """One candidate line as one sung performance on the kit's swing: its frames before synthesis, the phrase ends not
    yet released (chops are cut from it)."""
    ln = LINE_BY_NAME[name]
    how = replace(LINE_HOW, glottal=ln.glottal)
    perf = V.performance(V.sing_text(ln.notes), K.VOICE, how.speed, how.glottal, ln.accent, ln.draw)
    return V.sing_track(perf, ln.notes, K.BPM, how, LINE_LEN, LINE_PRE, D.rng(K.KIT, name, 'sing'), swing=K.SWING)


@functools.lru_cache(maxsize=None)
def line_sung(name):
    """The whole line as sung (phrase ends released) with its inhale, before polish, and the levels polish works
    against on it."""
    tr = V.release_ends(replace(line_track(name), sp=line_track(name).sp.copy()))
    y = V.breaths(V.synth(tr), tr.events, D.rng(K.KIT, name, 'breath'))
    return y, V.polish_levels(y, hp=HP)


def sung_whole(take):
    """A candidate line, dry: the inhale, the line, polished."""
    y, levels = line_sung(take.item.name)
    return V.polish(y, hp=HP, levels=levels)


# name, first and last syllable (event index: pull 0, me 1, in 2, to 3, or 4, bit 5), the words, the chop's first note
# (its root), about
PICK_CHOPS = (
    ('ChopPull', 0, 0, 'pull', 69, '"pull", A4, on the beat'),
    ('ChopMe', 1, 1, 'me', 72, '"me", C5'),
    ('ChopPullMe', 0, 1, 'pull me', 69, '"pull me", A4 up to C5'),
    ('ChopInto', 2, 3, 'into', 74, '"into", D5 falling to C5'),
    ('ChopOr', 4, 4, 'or', 69, '"or" of orbit, A4 held into a breath tail'),
    ('ChopOrbit', 4, 5, 'orbit', 69, '"orbit", A4 held, then G4 on the and'),
    ('ChopIntoOrbit', 2, 5, 'into orbit', 74, '"into orbit", D5 C5, then A4 held and G4'),
    ('ChopPullMeInto', 0, 3, 'pull me into', 69, '"pull me into", A4 C5 D5 C5'),
)
PICK_CHOP_BY_NAME = {c[0]: c for c in PICK_CHOPS}


def pick_chop(take):
    """One chop of the picked line, cut in the frame domain (vocals.chop) and polished with the whole line's levels;
    the junction where its breath tail starts is logged as a splice."""
    _, first, last, _, _, _ = PICK_CHOP_BY_NAME[take.item.name]
    tr, junction = V.chop(line_track(PICK), first, last)
    return Out(V.polish(V.synth(tr), hp=HP, levels=line_sung(PICK)[1]), splices=(int(np.floor(junction * SR + 0.5)),))


def chop_onset(name):
    """Seconds from the start of a chop's file to its note (where the beat falls, after the onset consonant). Needs
    the vocal models."""
    _, first, last, _, _, _ = PICK_CHOP_BY_NAME[name]
    return float(V.chop(line_track(PICK), first, last)[0].events[0]['t_on'])


# ------------------------------------------------------------------ the vocoder phrase
ROBOT = 'Relay online'
ROBOT_SYLLABLES = ('re', 'lay', 'on', 'line')
ROBOT_GRID = ((0, 1), (1, 3), (4, 2), (6, 4))  # (16th step, length) of each syllable, one bar: re-LAY on-LINE. Of 4
                                               # grids tried, the only one Whisper heard as "relay" (not "really")
                                               # with and without the chord and the laser
ROBOT_NOTE = K.ROOT + 12     # D3, 146.8 Hz: the chord's root
ROBOT_CHORD = ((0, 1.0), (3, 0.5), (7, 0.7))   # D3, F3, A3 (semitones over the root, level): the D minor chord
ROBOT_LASER = (5.0, 0.015)   # each syllable starts a fourth up and drops onto the chord with this time constant (s)
ROBOT_ACCENT = 'gb'          # the British lexicon's long "ree-": in the US one Whisper hears "really online"
ROBOT_LEAD = 0.06            # a slice starts this long before its step: room for the onset consonant
ROBOT_PRE = ROBOT_LEAD + 0.01
ROBOT_LEN = 10 * K.STEP + 0.3
ECHO_STEPS = 3               # dotted 8th
ROBOT_SHELF = (2000.0, 8.0)  # a high shelf on the vocoder before its grit: corner (Hz), gain (dB). Without it the
                             # robot had only -27.5 dB of its energy at 3-8 kHz (the sung line: -16.5), so the arp
                             # covered its words in the loop; with it, -20 dB, and Whisper still hears every word


def swung(step):
    """A 16th on the kit's swing, in 16ths: odd steps land late, as on every lane (phrase.Grid)."""
    return step + (2 * K.SWING - 1) * (step % 2)


def slice_sample(step):
    return int(np.floor((ROBOT_PRE + swung(step) * K.STEP - ROBOT_LEAD) * SR + 0.5))


# (nominal 16th step, onset sample in the file, syllable): the slice table for index.json, the same in dry and wet
ROBOT_SLICES = tuple((step, slice_sample(step), syl) for (step, _), syl in zip(ROBOT_GRID, ROBOT_SYLLABLES))
SLICE_NOTE = 'slices (16th step @ onset sample): ' + ', '.join(f'{s}@{n} {w}' for s, n, w in ROBOT_SLICES)
SLICE_META = {'slices': [{'step': step, 'onset': n, 'syllable': syl} for step, n, syl in ROBOT_SLICES],
              'bpm': K.BPM, 'chord': ['D3', 'F3', 'A3']}


def laser(track):
    """The kit's pitch drop on every syllable: its voiced frames start a fourth over the chord and fall onto it."""
    t = np.arange(len(track.f0)) * track.fps
    bend = np.zeros(len(t))
    for e in track.events:
        u = t - (e['t_on'] - ROBOT_LEAD)
        bend = np.where((u >= 0) & (t < e['t_end']), ROBOT_LASER[0] * np.exp(-np.maximum(u, 0) / ROBOT_LASER[1]),
                        bend)
    return replace(track, f0=np.where(track.f0 > 0, track.f0 * 2 ** (bend / 12), 0.0))


def high_shelf(x, fc, gain_db):
    """RBJ cookbook high shelf (slope 1): gain_db above fc, unity well below it. Mono."""
    A = 10 ** (gain_db / 40)
    w0 = 2 * np.pi * fc / SR
    c, al = np.cos(w0), np.sin(w0) / np.sqrt(2)
    b = A * np.array([(A + 1) + (A - 1) * c + 2 * np.sqrt(A) * al, -2 * ((A - 1) + (A + 1) * c),
                      (A + 1) + (A - 1) * c - 2 * np.sqrt(A) * al])
    a = np.array([(A + 1) - (A - 1) * c + 2 * np.sqrt(A) * al, 2 * ((A - 1) - (A + 1) * c),
                  (A + 1) - (A - 1) * c - 2 * np.sqrt(A) * al])
    return signal.lfilter(b / a[0], a / a[0], x)


@functools.lru_cache(maxsize=1)
def robot_voice():
    """The vocoder, before its level: Kokoro's take on the grid in the kit's male blend, WORLD on a flat buzzing D3
    with the formants 8% down, the laser on each syllable, and the same frames again on F3 and A3; then a de-esser, a
    compressor, a high shelf (+8 dB from 2 kHz: its consonants and buzz) and a little grit, 4x oversampled (more grit
    blurred "relay" into "really" for Whisper)."""
    perf = V.performance('relay online.', K.ROBOT_VOICE, 1.0, False, ROBOT_ACCENT)
    grid = tuple((swung(step), ln) for step, ln in ROBOT_GRID)
    tr = V.robot_track(perf, grid, K.BPM, float(D.midi_hz(ROBOT_NOTE)), ROBOT_LEN, ROBOT_PRE, ROBOT_LEAD,
                       buzz=0.05, formant=0.92)
    tr = laser(tr)
    y = sum(g * V.synth(replace(tr, f0=tr.f0 * 2 ** (k / 12))) for k, g in ROBOT_CHORD)
    y = D.bfilt(y, HP, 'highpass', order=4)
    y = V.compress(V.deess(y), -18, 3.0, 0.004, 0.08)
    if ROBOT_SHELF[1]:
        y = high_shelf(y, *ROBOT_SHELF)
    y = y / (np.max(np.abs(y)) + 1e-12)
    y = 0.7 * y + 0.3 * D.oversampled(lambda u: D.tanh_adaa(u, 2.0), y)
    return D.bfilt(y, HP, 'highpass', order=4)


def slices():
    return tuple(n for _, n, _ in ROBOT_SLICES)


def robot_dry(take):
    return Out(robot_voice(), splices=slices(), meta=SLICE_META)


def echo(x, taps=6, fb=0.45, drop_hz=-6.0):
    """Ping-pong echo on a dotted 8th whose every repeat is frequency-shifted a little further down (the laser falling
    away), darker and thinner each time. Mono in, stereo out, the dry signal not included."""
    d = int(np.floor(ECHO_STEPS * K.STEP * SR + 0.5))
    out = np.zeros((len(x) + d * taps, 2))
    tap = x
    for k in range(1, taps + 1):
        tap = D.freq_shift(D.bfilt(D.bfilt(tap, 5500, 'lowpass'), 300, 'highpass'), drop_hz)
        tap = tap * (fb if k > 1 else 0.7)
        out[k * d:k * d + len(x), (k - 1) % 2] += tap
        out[k * d:k * d + len(x), k % 2] += 0.25 * tap
    return out


def robot_wet(take):
    """The dry vocoder in the kit's space (the FDN on the FUT02/space seed) with the falling ping-pong echo. The space
    and the echo are mono under 150 Hz."""
    y = robot_voice()
    n_tail = int(4.5 * SR)
    verb = D.fdn_reverb(D.bfilt(y, 300, 'highpass'), K.space_rng(), t60=2.6, size=0.9, damp_hz=6500.0, n_tail=n_tail)
    dly = echo(y)
    out = np.zeros((len(y) + n_tail, 2))
    out[:len(y)] += D.st(y)
    out[:len(dly)] += 0.55 * dly
    out += 0.32 * verb[:len(out)]
    out = D.mono_below(D.bfilt(out, HP, 'highpass', order=4), 150.0)
    return Out(out, splices=slices(), meta=SLICE_META)


ITEMS = [
    Item('Vox', ln.name, 'vox', sung_whole, root=ln.notes[0][2], level=-18.0, flags=VOX, qa=WHY,
         tags=('clean', 'dry', 'sung'), sets=('r2',), script=ln.text,
         about=f'candidate line {ln.name[-1]}{" (the pick: the chops come from it)" if ln.name == PICK else ""}, '
               f'dry: "{ln.text}", D minor, female, on the kit swing: {ln.about}')
    for ln in LINES
] + [
    Item('Vox', name, 'vox', pick_chop, root=root, level=-18.0, flags=VOX, qa=WHY, tags=('clean', 'chop', 'sung'),
         sets=('r2',), script=words, about=f'sung chop {about}, cut from "{LINE_BY_NAME[PICK].text}"')
    for name, first, last, words, root, about in PICK_CHOPS
] + [
    Item('Vox', 'VocoderRelay', 'vox', robot_dry, root=ROBOT_NOTE, level=-18.0, flags=VOX, qa=WHY,
         tags=('clean', 'dry', 'robot'), sets=('r2',), script=ROBOT,
         about=f'vocoder phrase "{ROBOT}", male, a D minor chord (D3 F3 A3) on the 16th grid at 130 BPM, each '
               f'syllable dropping a fourth onto it, dry; {SLICE_NOTE}'),
    Item('Vox', 'VocoderRelayWet', 'vox', robot_wet, root=ROBOT_NOTE, level=-16.0, flags=VOX, qa=WHY,
         tags=('clean', 'wet', 'robot'), sets=('r2',), script=ROBOT,
         about=f'vocoder phrase "{ROBOT}", wet: the kit space and a ping-pong echo falling in pitch; {SLICE_NOTE}'),
]
