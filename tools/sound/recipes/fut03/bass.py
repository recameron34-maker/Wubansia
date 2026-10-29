"""FUT03 "Glass Current" bass for listening round 2: the bouncy metallic house bass that dips in pitch on every note
(multisamples, A clean and B warm), two original 2-bar basslines played on it against the four-on-the-floor kick
(each in both characters), and the vowel bass, two tempo-synced shots that move between vowels (each in both
characters too).

**The voice ("dip bass").** Every note starts on its pitch, dips 2.2-3 semitones within 22 ms and springs back over the
next 68 ms (raised-cosine both ways), so each note goes "bwop": the bounce of the brief. Under it, a mono sine sub on
the same pitch curve. (That is the held note of the multisamples; a line's short notes dip faster, on the bite only,
over a sub that holds the written pitch: see the basslines.) Over it, the bite: phase-locked mids, every partial
harmonic, starting 5 ms after the sub:
- 2-op FM with the modulator at twice the note (odd harmonics: hollow and metallic) and a little PolyBLEP saw for
  the even ones, both on the sub's own phase, so nothing in the voice ever beats;
- a feed-forward comb on the kit's quint partial (K.RATIOS[2] = 1.4983 x the note): a fixed glassy colour on the
  harmonics (a filter, so the note stays harmonic);
- a resonant low-pass that snaps shut with the FM index on every note (brighter, higher and faster with velocity), a
  soft drive at 4x;
- a glass tine on each attack: the kit's bell partials (K.RATIOS) 4 octaves over the note, gone in about 30 ms.

Two low ends, the same sub and bite (ADR 0011, round 2):
- A, clean (the strict mud gate, the category default): the bite starts at 480 Hz behind about 96 dB per octave, so
  150-400 Hz stays empty and the sine sub carries the note.
- B, warm (the proposed bass-body rule): A plus the body, the voice at rest (no snap, and darker than the bite) from
  120 to 480 Hz on the sub's envelope, so the FM's own third and fifth harmonics give the note body on small speakers
  and never flare on the attack; the sub carries a quiet second harmonic.

The multisamples (G1, D2, G2: root, fifth and octave, two layers) loop over 1 s of the steady note: every partial is on
the loop's grid of whole cycles, and every envelope (the index, the cutoff, the dip) reaches its resting value exactly,
by 0.30 s, before the loop starts. After the loop's end, an 80 ms release (the loop played on, fading out).

**The basslines** (2 bars at 128 BPM, kicks on every beat). The bass rests on every kick and every note is released 45
ms before the next one (the gate's before-hit rule), so the kick always has its own room; the lane then gets the bus
insert's stand-in, a 6 dB duck held over each kick's first 80 ms. Odd 16ths sit on the kit's one swing (phrase.Grid,
K.SWING 0.53). A line is played as a producer layers a house bass, in two parts:
- The sub: in each gap between two kicks, one phase-continuous mono sine on the written notes, the same in A and B. It
  swells in over 30 ms from 15 ms before the gap's first note (so it is up when the bite hits), glides 15 ms into each
  held note's pitch and along a slide, holds under the pops (notes under half a 16th: octave pops, steps) and is
  released 25 ms after the gap's last note. It never dips: every held note sits on its written pitch (the round 1
  review measured the old per-note sub, which dipped with the voice, 1-3 semitones flat). A short sine started on D2-F2
  over 12 ms leaked into 150-400 Hz past the gate's -20 dB; the slow swell, once per gap, doesn't.
- The bite (and in B the body): each note from the voice at its own pitch, a slide gliding inside it, with a short dip
  for the bounce: down 8 ms and back in 30 ms on the main notes, none on the ghosts (velocity under 0.7), so even a
  64 ms note sounds its pitch. B's body starts 10 ms after its note (the sub leads it), and only where the sub plays.
- LineBounce (the pick for the context loop): the offbeat house bounce with an octave pop. Bar 1 is the motif: G1 on
  each offbeat 8th with an octave pop G2 or a step (Bb1) on the swung 16th after it, then the fifth (D2) held from the
  "e" of beat 3 to the next kick, the minor seventh (F2) on the "e" of beat 4 and back to D2. Bar 2 brings the motif
  back varied: the same octave pop, then Bb1-C2, a held F2 from the "e" that slides down to D2, and C2, A1
  approaching G1 on the next bar.
- LineRoll: a rolling 16th line that fills each gap as e-and-a. Bar 1 calls from G1 (ghost on the e, held on the
  and), climbs Bb1-C2 to a D2 held from the "e" of beat 3, and runs F2-G2 up into the bar line; bar 2 answers an
  octave down from that G2 (a G2 pop, G1 held), turns down (Bb1-A1) where bar 1 went up, holds F1 from the "e" and
  slides it up to G1, and climbs C2-D2 back to the top.

**The vowel bass** (one-shots on G1, 2 beats each, A clean and B warm like the bass): the same FM source, richer,
through three moving formant band-passes (formants 1.5x a voice's, a small synthetic throat: first formants 405-1095
Hz). Each move speaks in the gaps as the lines do: silent from 45 ms before beat 2 and before its end, so the kick on
beat 2 and the next downbeat have their room.
- A, clean (the strict gate): the mids start at 480 Hz on the first 16th and lift, level-matched, to 1.5 kHz inside it,
  over a G1 sine sub held as a pedal (a gliding sine splatters into the low mids past the gate's falls). The vowels
  talk above the low mids and the box band, which costs them their first formants (round 1's growl, built the same
  way, was rated "fix, 3").
- B, warm (the proposed bass-body rule): no lift, the mids from 370 Hz all the way, so every first formant stays, over
  a sub that follows the move's pitch in each beat (a 12 ms glide between syllables); bass-body has no falls.
- VowelBassTalk: five syllables on the swung 16ths, a vowel move each, three in beat 1 (wow-wee-yah: G1 G1 G2) and two
  after the kick of beat 2 (wow-yeah: Bb1 D2): a rhythm and a pitch shape over the root.
- VowelBassRise: G1 opening oo-ah over beat 1; after the kick it comes back on the "e" as eh-ee while the filter
  opens and the note slides up an octave to G2: a dark-to-bright lift for the bar before a drop.

Provenance: new for FUT03. The loop closing (parts.loop_grid, close_loop), the steep high-pass and the grid come from
the shared toolkit; the lift and the talk gate follow the idea of FUT01's growl moves, written fresh here.
"""
import numpy as np

from tools.sound import dsp48 as D
from tools.sound import parts as P
from tools.sound import phrase as Ph
from tools.sound.item import Item, Out
from tools.sound.kits import fut03 as K

SR = D.SR
CLEAN, WARM = 'clean', 'warm'
CHARACTERS = (CLEAN, WARM)
MID_HP = 480.0                      # A, clean: the mids start here, about 96 dB per octave (context loops read it)
WARM_XO = 120.0                     # B, warm: the mids start here, an LR4 crossover over the sine sub
LOOP_STEPS = 32                     # 2 bars of 16ths: the basslines' loop (a context loop reads it)
G = Ph.Grid(K, LOOP_STEPS)          # the 16th grid on the kit's one swing
KICKS = tuple(range(0, LOOP_STEPS, 4))   # four on the floor: the kicks every line plays against
F_KICK = float(D.midi_hz(K.KICK_NOTE))

# ------------------------------------------------------------------ the voice
DIP_ST = (2.2, 3.0)                 # the dip's depth in semitones, soft to hard
DIP_DOWN, DIP_UP = 0.022, 0.068     # seconds: down, then back up to the note (done by 90 ms, before the tuning window);
                                    # the multisamples' dip, for held notes (a line's notes take LINE_DIP)
SETTLE = 0.30                       # every envelope reaches its resting value exactly here
IDX_ATTACK = (3.2, 5.0)             # the FM index on the attack, soft to hard...
IDX_REST = (2.0, 2.8)               # ...and at rest: bright enough over 480 Hz to carry a held note in A
IDX_TAU = 0.060
SAW_MIX = 0.25                      # the saw under the FM: the even harmonics
COMB = -0.30                        # the comb on the kit's quint partial; negative, so it thins the 3rd and 6th
                                    # harmonics (3 / 1.4983 is nearly 2) and lifts the ones between: the metal colour
FC_REST = (1200.0, 1900.0)          # the low-pass at rest on G1, soft to hard; it tracks the note (exponent below)
FC_TRACK = 0.4
FC_PEAK = (2200.0, 5200.0)          # where it snaps from on the attack
FC_TAU = 0.045
BODY_IDX = (1.1, 1.5)               # B's body is darker than the bite at rest: the FM index and the low-pass it rests
BODY_FC = (520.0, 760.0)            # on, so its low mids stay near a saw's (bass-body: body_over_saw)
FC_Q = 1.2                          # a little resonance: the "bwop"
DRIVE = 1.6
TINE = 16.0                         # the tine sits 4 octaves over the note, on the kit's bell partials
TINE_HP = 1500.0
SUB_ATTACK = 0.012                  # the sub's raised-cosine attack and release: a sine started or stopped faster
SUB_RELEASE = 0.025                 # splatters into 150-400 Hz (a 98 Hz sine gated 8 / 12 ms reads -8 dB there)
MID_ATTACK = 0.010                  # the bite's attack, before its high-pass (so its click stays above 480 Hz)...
BITE_LAG = 0.005                    # ...starting this long after the sub's: the sub leads every note, so no window
                                    # at a note's start reads the bite's edge in 150-400 Hz over a sub not there yet
RELEASE = 0.020                     # the mids' release
SUB_LEVEL = 0.62
BITE_LEVEL = 1.35                   # the bite: the mids from MID_HP, the same in both characters
BODY_LEVEL = 0.75                   # B's body: the resting voice from 120 Hz to MID_HP, on the sub's envelope
WARM_H2 = 0.16                      # B's sub: a quiet mono second harmonic, 16 dB down


def settle(t, tau, end=SETTLE):
    """An exponential fall from 1 that reaches exactly 0 at `end` seconds (and stays there): the envelopes of a looped
    note are then exactly at rest before the loop starts."""
    floor = np.exp(-end / tau)
    return np.clip((np.exp(-t / tau) - floor) / (1.0 - floor), 0.0, 1.0)


def dip(n, vel):
    """The pitch dip in semitones per sample (<= 0): down DIP_ST over DIP_DOWN, back over DIP_UP, raised-cosine."""
    t = P.times(n)
    down = 0.5 - 0.5 * np.cos(np.pi * np.clip(t / DIP_DOWN, 0.0, 1.0))
    up = 0.5 + 0.5 * np.cos(np.pi * np.clip((t - DIP_DOWN) / DIP_UP, 0.0, 1.0))
    return -P.lerp(DIP_ST, vel) * np.where(t < DIP_DOWN, down, up)


def mids_source(freq, n, vel, note_hz, ph, snap=True):
    """The phase-locked mids before their low end: FM (modulator at 2x, odd harmonics) and a little saw on the sub's
    own phase, the comb on the kit's quint partial, the low-pass, the drive at 4x. snap: the index and the cutoff
    snap from bright to rest on the attack (the bite); without it, the darker voice at rest (B's body)."""
    t = P.times(n)
    e = settle(t, IDX_TAU) if snap else np.zeros(n)
    idx_rest, fc_rest = (IDX_REST, FC_REST) if snap else (BODY_IDX, BODY_FC)
    idx = P.lerp(idx_rest, vel) + (P.lerp(IDX_ATTACK, vel) - P.lerp(idx_rest, vel)) * e
    fm = np.sin(2 * np.pi * ph + idx * np.sin(4 * np.pi * ph))
    saw = D.down(D.polyblep_saw(freq, n, 0.25, k=4), 4)
    x = (1 - SAW_MIX) * fm + SAW_MIX * saw
    x = x + COMB * P.frac_delay(x, SR / (note_hz * K.RATIOS[2]))
    rest = P.lerp(fc_rest, vel) * (note_hz / F_KICK) ** FC_TRACK
    fc = rest + (P.lerp(FC_PEAK, vel) - rest) * (settle(t, FC_TAU) if snap else 0.0)
    x = D.svf_tpt(x, fc, q=FC_Q)[0]
    return D.oversampled(lambda u: D.tanh_adaa(u, DRIVE), x, 4)


def tine(note_hz, n, vel, r):
    """The glass tine on the attack: the kit's bell partials (K.RATIOS) 4 octaves over the note, high-passed."""
    decays = [0.010 / (1 + 0.4 * k) for k in range(len(K.RATIOS))]
    amps = [1.0 / (1 + k) for k in range(len(K.RATIOS))]
    ir = D.modal_ir(note_hz * TINE, K.RATIOS, decays, amps, n, r) * P.raised(n, 0.0004)
    return D.bfilt(ir, TINE_HP, 'highpass', order=2) * (0.03 + 0.07 * vel)


def voice(freq, n, vel, char, note_hz, r):
    """One held note, mono (the multisamples; a line plays its sub and its mids apart, see line_lane): freq is its pitch
    curve (Hz per sample, the dip in it); note_hz its written pitch (the comb, the cutoff's tracking and the tine follow
    it). Every character has the sub and the bite (the snapping mids and the tine from 480 Hz, steep); B adds the body,
    the same voice at rest from 120 to 480 Hz on the sub's envelope, so its low mids hold steady under the bite and
    never flare on the attack."""
    ph = D.phase(freq, n)
    low = np.sin(2 * np.pi * ph) + (WARM_H2 * np.sin(4 * np.pi * ph) if char == WARM else 0.0)
    sub_env = P.raised(n, SUB_ATTACK)
    lag = int(BITE_LAG * SR)
    bite_env = np.concatenate([np.zeros(lag), P.raised(n - lag, MID_ATTACK)])
    bite = (mids_source(freq, n, vel, note_hz, ph) + tine(note_hz, n, vel, r)) * bite_env
    y = SUB_LEVEL * vel ** 0.6 * low * sub_env + BITE_LEVEL * vel ** 1.5 * P.hp_steep(bite, MID_HP)
    if char == WARM:
        body = D.lr4(D.lr4(mids_source(freq, n, vel, note_hz, ph, snap=False), WARM_XO, 'highpass'), MID_HP, 'lowpass')
        y = y + BODY_LEVEL * vel * body * sub_env
    return y


# ------------------------------------------------------------------ the multisamples
BASS_NOTES = (31, 38, 43)           # G1, D2, G2: root, fifth, octave (G2 is the top of the bass-body range)
BASS_LAYERS = (0.6, 1.0)
ATTACK_S = 0.40                     # the attack and the dip, settled, before the loop
LOOP_S = 1.0
RELEASE_S = 0.08                    # the release after the loop's end


def dip_bass(take, char):
    """One zone and layer: the note with its dip, then 1 s of the steady note, looped. Both characters strike the same
    tine (one seed per zone), so A and B differ only in their low end."""
    L, _, f = P.loop_grid(take.f0, LOOP_S)
    A0 = int(ATTACK_S * SR)
    n = A0 + int(0.02 * SR) + L
    freq = f * 2.0 ** (dip(n, take.vel) / 12.0)
    y = voice(freq, n, take.vel, char, f, D.rng(take.kit, 'DipBass', take.note, 'tine'))
    y, loop = P.close_loop(D.st(y), A0, L)
    return Out(with_release(y, loop), loop=loop, f0s=(f,))


def with_release(y, loop):
    """The looped file plus a release after the loop's end: the loop played on from its start (so it joins), fading
    out over RELEASE_S. A sampler loops (start, end) while the key is down; the file itself ends quietly instead of
    stopping mid-cycle (an edge the low-mid measures would read as a burst)."""
    a, _ = loop
    m = int(RELEASE_S * SR)
    return np.concatenate([y, y[a:a + m] * (0.5 + 0.5 * np.cos(np.pi * np.arange(m) / m))[:, None]])


def dip_bass_clean(take):
    return dip_bass(take, CLEAN)


def dip_bass_warm(take):
    return dip_bass(take, WARM)


# ------------------------------------------------------------------ the basslines
# (16th step, length in 16ths, MIDI note, velocity, slide): slide = (target note, where it starts as a share of the
# note's length, glide seconds) or None. G1 31, A1 33, Bb1 34, C2 36, D2 38, F2 41, G2 43; F1 29. Each bar holds one
# note from the "e" (2.3 16ths, 270 ms) to 50 ms before the next kick.
LINES = {
    'Bounce': (
        (2, 0.9, 31, 1.0, None), (3, 0.33, 43, 0.55, None),           # bar 1: the motif, G1 and its octave pop
        (6, 0.9, 31, 0.9, None), (7, 0.33, 34, 0.5, None),            # G1, a step up to Bb1
        (9, 2.3, 38, 1.0, None),                                      # the fifth, held from the e of 3
        (13, 0.55, 41, 0.6, None), (14, 0.9, 38, 0.85, None),         # the seventh on the e of 4, back to D2
        (18, 0.9, 31, 1.0, None), (19, 0.33, 43, 0.55, None),         # bar 2: the motif again...
        (22, 0.9, 34, 0.9, None), (23, 0.33, 36, 0.5, None),          # ...varied: Bb1 C2
        (25, 2.3, 41, 1.0, (38, 0.5, 0.06)),                          # F2 held from the e, sliding down to D2
        (29, 0.55, 36, 0.6, None), (30, 0.9, 33, 0.85, None),         # C2, A1: the approach back to G1
    ),
    'Roll': (
        (1, 0.55, 31, 0.45, None), (2, 1.4, 31, 1.0, None),           # bar 1, the call: e ghost, and held
        (5, 0.55, 31, 0.45, None), (6, 0.8, 34, 0.85, None), (7, 0.33, 36, 0.7, None),   # e-and-a: G1 Bb1 C2
        (9, 2.3, 38, 1.0, None), (13, 0.5, 38, 0.4, None),            # D2 held from the e, a ghost
        (14, 0.6, 41, 0.8, None), (15, 0.33, 43, 0.7, None),          # F2 G2: the run up into the bar line
        (17, 0.33, 43, 0.5, None), (18, 1.4, 31, 1.0, None),          # bar 2, the answer: a G2 pop, the octave drop
        (21, 0.55, 31, 0.45, None), (22, 0.8, 34, 0.85, None), (23, 0.33, 33, 0.7, None),  # turns down: Bb1 A1
        (25, 2.3, 29, 1.0, (31, 0.5, 0.06)),                          # F1 held from the e, sliding up to G1
        (29, 0.5, 38, 0.4, None), (30, 0.6, 36, 0.8, None), (31, 0.33, 38, 0.7, None),   # C2 D2, back to the top
    ),
}
PICK = 'Bounce'                     # the designer's pick for the context loop
LINE_ABOUT = {
    'Bounce': 'the offbeat bounce: G1 on the offbeats with an octave pop on the swung 16th, a D2 held from the e, F2 '
              'and back to D2; bar 2 repeats the motif varied (Bb1 C2, a held F2 sliding to D2, C2 A1 back to G1)',
    'Roll': 'a rolling e-and-a line: bar 1 climbs G1 Bb1 C2 to a held D2 and runs F2 G2 into the bar; bar 2 answers an '
            'octave down, turns down (Bb1 A1), holds F1 sliding to G1 and climbs C2 D2',
}
DUCK = dict(depth_db=6.0, ramp=0.002, hold=0.080, release=0.040)   # the bus insert's stand-in, on the kicks
CLEAR_S = 0.045                     # every note is released this long before the next kick
GHOST_VEL = 0.7                     # notes under this velocity are ghosts: no dip
POP_LEN = 0.5                       # notes under this many 16ths are pops: the sub holds the gap's pitch under them
LINE_DIP = (0.008, 0.030)           # the line's dip on the bite: down, then back (seconds), DIP_ST deep
SUB_GLIDE = 0.015                   # the sub glides into each held note over this
LINE_SUB_ATTACK = 0.030             # the sub's swell at the start of a gap...
SUB_LEAD = 0.015                    # ...starting this long before the gap's first note (still 100 ms after the kick)
BODY_LAG = 0.010                    # B's body starts this long after its note: the sub always leads it


def kick_onsets():
    return tuple(G.at(s) for s in KICKS)


def note_end_s(step, length):
    """Seconds from the loop start to where a note's release ends."""
    return G.at(step) / SR + length * G.s16 / SR + max(RELEASE, SUB_RELEASE)


def next_kick_s(step):
    """Seconds from the loop start to the first kick after a 16th (the loop's end for the last beat)."""
    k = min((s for s in KICKS + (LOOP_STEPS,) if s > step))
    return k * G.s16 / SR


def slide_curve(n, gate_s, note, slide):
    """Semitones per sample: 0, then a smoothstep glide to the slide's target."""
    if slide is None:
        return np.zeros(n)
    target, start, glide = slide
    u = np.clip((P.times(n) - start * gate_s) / glide, 0.0, 1.0)
    return (target - note) * u * u * (3 - 2 * u)


def smoothstep(u):
    u = np.clip(u, 0.0, 1.0)
    return u * u * (3 - 2 * u)


def line_dip(n, vel):
    """The line's dip on the bite, semitones per sample (<= 0): DIP_ST deep, down over LINE_DIP[0] and back over
    LINE_DIP[1] (raised-cosine), back within 30 cents by 32 ms; none on a ghost."""
    if vel < GHOST_VEL:
        return np.zeros(n)
    t = P.times(n)
    down_s, up_s = LINE_DIP
    down = 0.5 - 0.5 * np.cos(np.pi * np.clip(t / down_s, 0.0, 1.0))
    up = 0.5 + 0.5 * np.cos(np.pi * np.clip((t - down_s) / up_s, 0.0, 1.0))
    return -P.lerp(DIP_ST, vel) * np.where(t < down_s, down, up)


def is_pop(note):
    return note[1] < POP_LEN


def gap_notes(name):
    """The line's notes grouped by the kick before them: [(kick step, [notes])], gaps without notes left out."""
    out = []
    for k in KICKS:
        notes = [n for n in LINES[name] if max(s for s in KICKS if s <= n[0]) == k]
        if notes:
            out.append((k, notes))
    return out


def sub_gaps(name):
    """The sub of each gap: (start sample, MIDI pitch, level, envelope), one value per sample from the start. The pitch
    starts on the gap's first held note (a pop before it starts the sub on that pitch), glides SUB_GLIDE into each held
    note and follows its slide; pops leave it where it is. The level follows each held note's velocity over the same
    glide. The envelope swells over LINE_SUB_ATTACK from SUB_LEAD before the gap's first note and is released over
    SUB_RELEASE after the gap's last note ends."""
    out = []
    for _, notes in gap_notes(name):
        held = [n for n in notes if not is_pop(n)]
        a = G.at(notes[0][0]) - int(SUB_LEAD * SR)
        end_s = max((G.at(s) - a) / SR + ln * G.s16 / SR for s, ln, *_ in notes)
        n = int((end_s + SUB_RELEASE + 0.01) * SR)
        t = P.times(n)
        cur, lv = float(held[0][2]), SUB_LEVEL * held[0][3] ** 0.6
        pitch, level = np.full(n, cur), np.full(n, lv)
        for j, (s, ln, note, vel, slide) in enumerate(held):
            tj = (G.at(s) - a) / SR
            if j:
                pitch += (note - cur) * smoothstep((t - tj) / SUB_GLIDE)
                level += (SUB_LEVEL * vel ** 0.6 - lv) * smoothstep((t - tj) / SUB_GLIDE)
                cur, lv = float(note), SUB_LEVEL * vel ** 0.6
            if slide:
                target, start, glide = slide
                pitch += (target - cur) * smoothstep((t - tj - start * ln * G.s16 / SR) / glide)
                cur = float(target)
        out.append((a, pitch, level, P.raised(n, LINE_SUB_ATTACK) * D.taper(n, end_s, SUB_RELEASE)))
    return out


def sub_lane(name, char):
    """The line's sub, (L, 2) mono, and its envelope (where it plays, 0..1): one sine per gap (B adds its quiet second
    harmonic), the same notes and shape in A and B."""
    y, where = np.zeros((G.length, 2)), np.zeros((G.length, 2))
    for a, pitch, level, env in sub_gaps(name):
        ph = D.phase(D.midi_hz(pitch), len(pitch))
        low = np.sin(2 * np.pi * ph) + (WARM_H2 * np.sin(4 * np.pi * ph) if char == WARM else 0.0)
        Ph.place(y, level * low * env, a)
        Ph.place(where, env, a)
    return y, where


def mids_lane(name, char):
    """The bite of every note (and in B its body), (L, 2) each: the voice at the note's pitch with the short line dip
    and any slide, gated and released. The bite starts BITE_LAG after the note, B's body BODY_LAG after it."""
    bite, body = np.zeros((G.length, 2)), np.zeros((G.length, 2))
    lag, blag = int(BITE_LAG * SR), int(BODY_LAG * SR)
    for i, (step, length, note, vel, slide) in enumerate(LINES[name]):
        gate = length * G.s16 / SR
        n = int((gate + SUB_RELEASE + 0.01) * SR)
        f = float(D.midi_hz(note))
        freq = f * 2.0 ** ((line_dip(n, vel) + slide_curve(n, gate, note, slide)) / 12.0)
        ph = D.phase(freq, n)
        env = np.concatenate([np.zeros(lag), P.raised(n - lag, MID_ATTACK)])
        x = (mids_source(freq, n, vel, f, ph) + tine(f, n, vel, D.rng(K.KIT, 'Line', name, i, 'tine'))) * env
        Ph.place(bite, BITE_LEVEL * vel ** 1.5 * P.hp_steep(x, MID_HP) * D.taper(n, gate, RELEASE), G.at(step))
        if char == WARM:
            env = np.concatenate([np.zeros(blag), P.raised(n - blag, SUB_ATTACK)]) * D.taper(n, gate, SUB_RELEASE)
            b = D.lr4(D.lr4(mids_source(freq, n, vel, f, ph, snap=False), WARM_XO, 'highpass'), MID_HP, 'lowpass')
            Ph.place(body, BODY_LEVEL * vel * b * env, G.at(step))
    return bite, body


def line_lane(name, char):
    """A line as the lane plays it: the sub, the bite, and in B the body where the sub plays (tails wrap), then the
    duck held over each kick's first 80 ms. Mono, (L, 2)."""
    sub, where = sub_lane(name, char)
    bite, body = mids_lane(name, char)
    y = sub + bite + body * where
    return y * D.undb(Ph.pump_db(kick_onsets(), G.length, **DUCK))[:, None]


def line_out(name, char):
    meta = {'bpm': K.BPM, 'swing': K.SWING, 'steps': LOOP_STEPS, 'kick_steps': list(KICKS), 'character': char,
            'pick': name == PICK, 'duck': DUCK,
            'sub': {'per_gap': True, 'attack_s': LINE_SUB_ATTACK, 'lead_s': SUB_LEAD, 'glide_s': SUB_GLIDE,
                    'release_s': SUB_RELEASE, 'holds_under_pops_shorter_than_16ths': POP_LEN},
            'bite_dip': {'down_s': LINE_DIP[0], 'up_s': LINE_DIP[1], 'semitones': list(DIP_ST),
                         'ghosts_under_velocity': GHOST_VEL},
            'notes': [{'step': s, 'len16': ln, 'note': D.note_name(m), 'vel': v,
                       'slide_to': D.note_name(sl[0]) if sl else None} for s, ln, m, v, sl in LINES[name]]}
    return Out(line_lane(name, char), hits=kick_onsets(), meta=meta)


def line_bounce_clean(take):
    return line_out('Bounce', CLEAN)


def line_bounce_warm(take):
    return line_out('Bounce', WARM)


def line_roll_clean(take):
    return line_out('Roll', CLEAN)


def line_roll_warm(take):
    return line_out('Roll', WARM)


# ------------------------------------------------------------------ the vowel bass
VOWEL_NOTE = 31                     # G1
SHOT_STEPS = 8                      # 2 beats of 16ths
SHOT_TAIL = 0.35                    # rendered past the move: silence, so a shot ends cleanly however it is cut
FORMANT_SCALE = 1.5                 # a small synthetic throat: first formants 405-1095 Hz, the rest above 1.3 kHz
VOW = {'oo': (300, 870, 2240), 'ah': (730, 1090, 2440), 'eh': (530, 1840, 2480), 'ee': (270, 2290, 3010)}
FORMANT_GAIN = (1.0, 0.8, 0.45)
FORMANT_Q = (4.0, 6.0, 8.0)
LIFT_HP = 1500.0                    # A: where the mids move after the first 16th, clear of 400 Hz-1 kHz
LIFT = (0.050, 0.100)               # the lift's crossfade, seconds: inside the first 16th
WARM_MIDS_HP = 370.0                # B: the mids from here, steep, all the way: every first formant stays
GLIDE = 0.012                       # the pitch moves between syllables over this
SUB_END = 0.13                      # A: the pedal's 80 ms fall starts this long before the move ends, so it is gone
                                    # 50 ms before the bar's next kick (where a shot played on every beat ends)
F_VOWEL = float(D.midi_hz(VOWEL_NOTE))   # A's sub holds G1 throughout, a pedal under the moving mids: a sine that
                                         # glides splatters into the low mids (a 30 ms move G1 to Bb1: -34 dB, against
                                         # the gate's 30 dB fall). B's sub follows the move (bass-body has no falls)
ROOM = 0.045                        # both moves fall silent this long before beat 2 and before the shot's end, as the
                                    # basslines do: the kick's room (the groove loop's before-hit window)
SUB_SWELL = (0.030, 0.015)          # B: after beat 2's room the sub swells back over 30 ms from 15 ms before the voice
# (16th, MIDI, vowel from, vowel to, length in 16ths): Talk's syllables, three in beat 1 and two in beat 2 after the
# kick; Rise is one note, its vowels on the 8ths, coming back on the "e" of beat 2 (RISE_RESUME) after the kick
TALK = ((0, 31, 'oo', 'ah', 1.6), (2, 31, 'oo', 'ee', 0.8), (3, 43, 'ee', 'ah', 0.5), (5, 34, 'ah', 'oo', 0.9),
        (6, 38, 'oo', 'ee', 1.4))
RISE_VOWELS = ((0, 'oo'), (2, 'ah'), (5, 'eh'), (6.5, 'ee'))
RISE_RESUME = 5
RISE_SLIDE = (5, 43, 1.0)           # from the 16th, up to G2, over this many 16ths


def step_s(step):
    """A position in 16ths in a shot (fractional: a share of the 16th after it), seconds, on the kit's swing."""
    k = int(np.floor(step))
    return (G.at(k) + (step - k) * G.s16) / SR


def vowel_tracks(n, keys):
    """F1-F3 per sample: keys = [(seconds, vowel)], joined smoothly (smoothstep) and held after the last."""
    t = P.times(n)
    out = np.zeros((3, n))
    for j in range(3):
        times = [k[0] for k in keys]
        vals = np.log2([VOW[k[1]][j] * FORMANT_SCALE for k in keys])
        idx = np.clip(np.searchsorted(times, t, side='right') - 1, 0, len(keys) - 1)
        nxt = np.minimum(idx + 1, len(keys) - 1)
        span = np.maximum(np.array(times)[nxt] - np.array(times)[idx], 1e-9)
        u = np.clip((t - np.array(times)[idx]) / span, 0.0, 1.0)
        u = u * u * (3 - 2 * u)
        out[j] = 2.0 ** (vals[idx] + (vals[nxt] - vals[idx]) * u)
    return out


def formant_bank(x, tracks):
    """Three formant band-passes on moving centres (SVF, unity at their peaks) and a little of the dry low-pass."""
    y = 0.12 * D.bfilt(x, 900.0, 'lowpass', order=2)
    for j in range(3):
        y = y + FORMANT_GAIN[j] * D.svf_tpt(x, tracks[j], q=FORMANT_Q[j])[1] / FORMANT_Q[j]
    return y


def pitch_track(n, points, glide=GLIDE):
    """Hz per sample from [(seconds, MIDI)], each move a `glide`-long smoothstep."""
    t = P.times(n)
    m = np.full(n, float(points[0][1]))
    for (a, m0), (b, m1) in zip(points, points[1:]):
        u = np.clip((t - b) / glide, 0.0, 1.0)
        m = np.where(t >= b, m0 + (m1 - m0) * u * u * (3 - 2 * u), m)
    return D.midi_hz(m)


def lift(y):
    """The mids from MID_HP on the first 16th, then from LIFT_HP at the same level (the gain matches the two bands
    over the 50 ms the crossfade starts on), so the vowels go on above 400 Hz-1 kHz and the box band falls."""
    n = len(y)
    a = int(LIFT[0] * SR)
    lo, hi = P.hp_steep(y, MID_HP), P.hp_steep(y, LIFT_HP)
    w = slice(a, a + int(0.05 * SR))
    g = float(np.sqrt(np.sum(lo[w] ** 2) / (np.sum(hi[w] ** 2) + 1e-30)))
    u = D.taper(n, LIFT[0], LIFT[1] - LIFT[0])
    return u * lo + (1 - u) * g * hi


def talk_gate(n, ons, lengths):
    """The syllable rhythm on the mids: each opens over 4 ms, holds 80% of its length and falls 15 dB over 14 ms."""
    t = P.times(n)
    g = np.full(n, D.undb(-15.0))
    for on, ln in zip(ons, lengths):
        tau = t - on
        rise = np.clip(tau / 0.004, 0.0, 1.0)
        fall = np.clip((tau - 0.8 * ln) / 0.014, 0.0, 1.0)
        e = np.where(tau < 0, 0.0, (0.5 - 0.5 * np.cos(np.pi * rise)) * (0.5 + 0.5 * np.cos(np.pi * fall)))
        g = np.maximum(g, e)
    return g


def pushes(t, ons, tau):
    """1 at each onset, falling over tau until the next: the FM index's push on every syllable."""
    e = np.zeros(len(t))
    for on in ons:
        e = np.where(t >= on, np.exp(-(t - on) / tau), e)
    return e


def spoken(t, a, b, fade=0.010):
    """1 from a to b seconds, with raised-cosine edges `fade` long inside them; 0 elsewhere."""
    up = np.clip((t - a) / fade, 0.0, 1.0)
    down = np.clip((b - t) / fade, 0.0, 1.0)
    return (0.5 - 0.5 * np.cos(np.pi * up)) * (0.5 - 0.5 * np.cos(np.pi * down))


def spans(move):
    """Where a move speaks, one span per beat, (start, end) in seconds: each ends ROOM before beat 2 or the shot's end,
    or with its last syllable. Rise comes back on the "e" of beat 2 (RISE_RESUME)."""
    if move == 'talk':
        out = []
        for lo, hi in ((0, 4), (4, SHOT_STEPS)):
            syl = [x for x in TALK if lo <= x[0] < hi]
            out.append((step_s(syl[0][0]), min(step_s(hi) - ROOM, max(step_s(s + ln) for s, _, _, _, ln in syl))))
        return out
    return [(0.0, step_s(4) - ROOM), (step_s(RISE_RESUME), step_s(SHOT_STEPS) - ROOM)]


def vowel_shot(take, move, char):
    """One move in one character. Both speak only inside spans(move) (the kick's room before beat 2 and the end, as
    the basslines leave it). A, clean: the mids from MID_HP on the first 16th, then lifted to LIFT_HP, over the G1
    pedal (the gate). B, warm: the mids from WARM_MIDS_HP all the way, so the first formants stay, over a sub on the
    move's own pitch in each span (bass-body)."""
    length = SHOT_STEPS * G.s16 / SR
    n = int((length + SHOT_TAIL) * SR)
    t = P.times(n)
    sp = spans(move)
    where = sum(spoken(t, a, b) for a, b in sp)
    if move == 'talk':
        ons = [step_s(s) for s, *_ in TALK]
        ends = [step_s(s + ln) for s, _, _, _, ln in TALK]
        freq = pitch_track(n, [(o, m) for o, (_, m, *_) in zip(ons, TALK)])
        keys = []
        for o, e, (_, _, v0, v1, _) in zip(ons, ends, TALK):
            keys += [(o, v0), (o + 0.6 * (e - o), v1)]
        gate = talk_gate(n, ons, [e - o for o, e in zip(ons, ends)])
        idx = 3.0 + 1.5 * pushes(t, ons, 0.05)
        fc = 1800.0 + 2500.0 * gate
    else:
        s0, target, glide16 = RISE_SLIDE
        a, b = step_s(s0), step_s(s0) + glide16 * G.s16 / SR
        freq = D.midi_hz(VOWEL_NOTE + (target - VOWEL_NOTE) * smoothstep((t - a) / (b - a)))
        keys = [(step_s(s), v) for s, v in RISE_VOWELS] + [(length, 'ee')]
        gate = np.ones(n)
        rise = np.clip(t / length, 0.0, 1.0)
        idx = 2.2 + 2.8 * rise
        fc = 1500.0 + 4500.0 * rise ** 1.5
    ph = D.phase(freq, n)
    src = np.sin(2 * np.pi * ph + idx * np.sin(2 * np.pi * ph)) + 0.35 * D.down(D.polyblep_saw(freq, n, 0.25, k=4), 4)
    src = D.svf_tpt(src, fc, q=0.8)[0]
    src = D.oversampled(lambda v: D.tanh_adaa(v, 2.2), src, 4)
    lag = int(BITE_LAG * SR)
    onset = np.concatenate([np.zeros(lag), P.raised(n - lag, MID_ATTACK)])    # the sub leads, as in the dip bass
    mids = formant_bank(src, vowel_tracks(n, keys)) * gate * onset * where
    drive = lambda v: D.oversampled(lambda u: D.tanh_adaa(u, 1.8), v / (np.max(np.abs(v)) + 1e-12), 4)
    if char == CLEAN:
        mids = lift(drive(lift(mids))) * where
        sub = np.sin(2 * np.pi * F_VOWEL * t) * P.raised(n, SUB_ATTACK) * D.taper(n, length - SUB_END, 0.08)
    else:
        mids = P.hp_steep(drive(P.hp_steep(mids, WARM_MIDS_HP)), WARM_MIDS_HP) * where
        att, lead = SUB_SWELL
        env = P.raised(n, SUB_ATTACK) * D.taper(n, sp[0][1], SUB_RELEASE)
        for a, b in sp[1:]:
            u = np.clip((t - (a - lead)) / att, 0.0, 1.0)
            env = env + (0.5 - 0.5 * np.cos(np.pi * u)) * D.taper(n, b, SUB_RELEASE)
        sub = np.sin(2 * np.pi * ph) * env
    meta = {'move': move, 'character': char, 'bpm': K.BPM, 'swing': K.SWING, 'length_s': round(length, 4),
            'spans_s': [[round(a, 4), round(b, 4)] for a, b in sp],
            'syllables': [{'step': s, 'note': D.note_name(m), 'vowels': [v0, v1], 'len16': ln}
                          for s, m, v0, v1, ln in TALK]
            if move == 'talk' else [{'step': s, 'vowel': v} for s, v in RISE_VOWELS],
            'slide': None if move == 'talk' else {'from_step': RISE_SLIDE[0], 'to': D.note_name(RISE_SLIDE[1])},
            'sub': 'G1 pedal' if char == CLEAN else "the move's own pitch, per span",
            'mids_from_hz': [MID_HP, LIFT_HP] if char == CLEAN else WARM_MIDS_HP}
    return Out(0.9 * mids + 0.55 * sub, meta=meta)


def vowel_talk_clean(take):
    return vowel_shot(take, 'talk', CLEAN)


def vowel_talk_warm(take):
    return vowel_shot(take, 'talk', WARM)


def vowel_rise_clean(take):
    return vowel_shot(take, 'rise', CLEAN)


def vowel_rise_warm(take):
    return vowel_shot(take, 'rise', WARM)


# ------------------------------------------------------------------ the items
WARM_QA = {'mud': 'bass-body', 'why': 'B, warm: the proposed bass-body rule (ADR 0011); the resting voice from 120 Hz '
                                      'over a mono sine sub'}
CLEAN_LINE_QA = {'mud': 'gate', 'why': 'A, clean: a bass phrase under the strict gate, like the bass'}
WARM_LINE_QA = {'mud': 'bass-body', 'why': 'B, warm: a bass phrase under the proposed bass-body rule (ADR 0011), '
                                           'resting on every kick'}
SHOT_QA = {'tonal': False,
           'why': 'a moving shot (syllables, vowels, a slide) has no steady tone for the alias check'}
WARM_SHOT_QA = {'mud': 'bass-body', 'tonal': False,
                'why': 'B, warm: the proposed bass-body rule (ADR 0011), the first formants kept from 370 Hz over a '
                       'sub on the move; a moving shot has no steady tone for the alias check'}
TUNE = {'talk': (0.05, 0.22), 'rise': (0.10, 0.40)}   # the G1 of each move's first beat
SHOT_ABOUT = {
    'talk': 'vowel bass, talk: 2 beats of swung 16ths, five syllables wow-wee-yah | wow-yeah on G1 G1 G2 | Bb1 D2, '
            'leaving the kick room before beat 2',
    'rise': 'vowel bass, rise: G1 opening oo-ah on beat 1, then after the kick eh-ee as the filter opens and the note '
            'slides up an octave to G2',
}
VOICE_ABOUT = ('bouncy metallic house bass: every note dips 2-3 semitones and springs back in 90 ms; phase-locked FM '
               '(odd harmonics) and saw, a comb on the glass quint partial, a snapping low-pass, a glass tine on the '
               'attack, a mono sine sub; G1 D2 G2, 1 s loops')
ITEMS = [
    Item('Bass', 'DipBassClean', 'bass', dip_bass_clean, layers=BASS_LAYERS, notes=BASS_NOTES, loop=True,
         sets=('r2',), about=f'A, clean: {VOICE_ABOUT}; the mids from 480 Hz (the strict mud gate)'),
    Item('Bass', 'DipBassWarm', 'bass', dip_bass_warm, layers=BASS_LAYERS, notes=BASS_NOTES, loop=True,
         sets=('r2',), qa=WARM_QA, about=f'B, warm: {VOICE_ABOUT}; plus its body, the resting voice from 120 Hz '
                                       f'(bass-body)'),
] + [
    Item('Loops', f'Line{name}{char.title()}', 'loop', fn, loop=True, sets=('r2',),
         qa=CLEAN_LINE_QA if char == CLEAN else WARM_LINE_QA,
         about=f'bassline {name}{" (the pick for the context loop)" if name == PICK else ""}, '
               f'{"A, clean" if char == CLEAN else "B, warm"}: 2 bars at 128 BPM against four-on-the-floor kicks '
               f'(declared), on the kit swing; {LINE_ABOUT[name]}')
    for name, char, fn in (('Bounce', CLEAN, line_bounce_clean), ('Bounce', WARM, line_bounce_warm),
                           ('Roll', CLEAN, line_roll_clean), ('Roll', WARM, line_roll_warm))
] + [
    Item('Bass', f'VowelBass{move.title()}{char.title()}', 'bass', fn, notes=(VOWEL_NOTE,), sets=('r2',),
         qa={**(SHOT_QA if char == CLEAN else WARM_SHOT_QA), 'tune_window': TUNE[move]},
         about=f'{SHOT_ABOUT[move]}; ' + ('A, clean: the vowels above a 1.5 kHz lift over a G1 sine pedal (the strict '
                                         'gate)' if char == CLEAN else
                                         'B, warm: the vowels whole from 370 Hz over a sub on the move (bass-body)'))
    for move, char, fn in (('talk', CLEAN, vowel_talk_clean), ('talk', WARM, vowel_talk_warm),
                           ('rise', CLEAN, vowel_rise_clean), ('rise', WARM, vowel_rise_warm))
]
