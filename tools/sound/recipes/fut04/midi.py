"""FUT04's written music for the MIDI export (tools/sound/midi.py, MIDI v2; the schema: tools/sound/README.md, "MIDI").

- The loop (Loop_Vox, Loop_NoVox): the round 5 context loop (context5.py on context.py) lane by lane: four on the floor
  (out on beat 4 of bar 4), the gated snare, the hats and the ride on the off-beats (to beat 1 of bar 4), the anvil;
  the rumble on drum note 35, its 2-bar loop at 16ths 0 and 32, muted from 16th 60 with the kick; AcidLine1 twice (its
  4-bar ride: the knob and the ride's cutoff on CC74, its resonance on CC71); the modular blips twice, their ratchets
  as retriggers; the drone held on E3 (its root); the vox synth riff, its bar 1 again in bar 3 and D4 on 16th 46, the
  throws on 30 and 46 (CC91/CC94); the variant's accents (no-vox: the zap and the metal chord thrown); the stutter fill.
- The parts: the two acid lines (their squelch ticks as notes on the kicks: the voice above 800 Hz alone, which a MIDI
  note can't say), the blip sequence, the drone and its Em(add9) voicing, and the synth phrases: the vox synth riff
  (channel 5; the velocity picks the vowel: 64 'oo', 127 'oh'), the acid stab riff (channel 11), the metal chords
  (channel 12) and the rave stab riff (channel 13).
- The extended mix: FUT04's own form (FORM, below; audit B's FUT04 section): hypnotic, the acid's filter telling the
  story on CC74/CC71, the tension from the hats, the ride and the filter rather than rolls. Every line changes something
  and every 16 bars something structural; the drone is out wherever the vox plays (the riff's E3 and B3 are the
  drone's); the vox, the rave stabs, the acid stabs and the blips take turns at the top rather than stacking.
Notes are (16th, MIDI note, length in 16ths, velocity 0-1, slide).
"""
import math

from tools.sound import channels as CH
from tools.sound import dsp48 as D
from tools.sound.midi import Hit, Key, Note, hits, line, section, swapped
from tools.sound.recipes.fut04 import accents as A, bass as B, context as C, context5 as C5, lead as L, pad as Pd
from tools.sound.recipes.fut04 import voxsynth as V

RUMBLE = 35
SNARE, CLOSED, OPEN, ANVIL, RIDE = 38, 42, 46, 47, 51
# the acid's ride maps, the mix manifest's (plugin_mix.ride_points): CC74 the ride's cutoff multiplier on channel 1's
# low-pass, 5400 Hz x 1.0-2.6 (channels.cc_value's law: round(127 ln(m) / ln(2.6))); CC71 the ladder's resonance
# (channels.cc_linear). Both 0 at rest, where the samples play as they are
CUT = (5400, 14040)                 # Hz: the low-pass at the ride's x1 and x2.6
RESO = (0.90, 0.96)                 # the resonance at rest, and at the ride's top


def line_notes(line):
    """A line's notes and its squelch ticks, in step order."""
    ticks = [(s, n, ln, v, False) for s, n, ln, v in line.ticks]
    return sorted([(s, n, ln, v, bool(sl)) for s, n, ln, v, sl in line.notes] + ticks)


def blips():
    """The blip sequence: each note, its ratchets as retriggers a 16th / n apart, each 0.7 of the gap long and
    RATCHET_DB softer than the one before (lead.hits)."""
    out = []
    for s, n, ln, v, r in L.PHRASE:
        if r == 1:
            out.append((s, n, ln, v, False))
        else:
            out += [(s + k / r, n, 0.7 / r, v * D.undb(L.RATCHET_DB * k), False) for k in range(r)]
    return out


def interp(points, x, log):
    """Breakpoints (16th, value) joined (in log2 for a cutoff), read at 16th x."""
    for (x0, v0), (x1, v1) in zip(points, points[1:]):
        if x0 <= x <= x1:
            u = (x - x0) / (x1 - x0)
            return 2 ** (math.log2(v0) + u * (math.log2(v1) - math.log2(v0))) if log else v0 + u * (v1 - v0)
    raise ValueError(x)


def acid_rides(name):
    """A line's 4-bar ride (bass.RIDES) every 60 ticks, unswung: CC74 its cutoff multiplier (x1 to x2.6, joined in log
    frequency) on CUT's low-pass, CC71 its resonance (joined linearly) over RESO. A line without a ride stays at rest
    (0 and 0); the line's own knob is in its samples and its recorded stem."""
    ride = B.RIDES.get(name, dict(cutoff=((0, 1.0), (64, 1.0)), res=((0, B.RES), (64, B.RES))))
    cut, res = [], []
    for i in range(256):
        x = i * 60 / 240
        cut.append(CH.cc_value(CUT[0] * interp(ride['cutoff'], x, True), *CUT))
        res.append(CH.cc_linear(interp(ride['res'], x, False), *RESO))
    return cut, res


RIDES = {ln.name: acid_rides(ln.name) for ln in B.LINES}


def loop_accents(mode):
    """context5's accents for a mode as hits ('end': the file ends on the loop's downbeat; thrown where context5
    throws them)."""
    out = []
    for at, name in C5.ACCENTS[mode]:
        out.append(Hit(64, name, 1, 'end') if at == 'end' else Hit(at, name, 1, throw=at in C5.ACCENT_THROWS[mode]))
    return out


# ------------------------------------------------------------------ the extended mix's own material
# The drums' 4-bar groove block of the hats (the loop's, bar 2 again in its fill bar) and the anvil's, the variants'
# base: what a line plays where no variant is named
HATS = swapped(hits(C.HAT_CLOSED, CLOSED) + hits(C.HAT_OPEN, OPEN) + hits(C.RIDE, RIDE))
ANVILS = swapped(hits(C.ANVIL, ANVIL))
# the ride on 8ths: the loop's ride on the off-beat 8ths, plus its on-beat 8ths, softer (a ping under each kick's
# click), so the iron ride runs as one 8th-note pulse (from bar 81, the groove's last 16 bars)
RIDE_ON_8THS = sorted(HATS + [Note(16 * b + s, RIDE, 0.5, 0.6) for b in range(4) for s in (0, 4, 8, 12)])
# the hats on every 16th for the build and drop 2's peak: closed 16ths, machine-even (the beat 1.0, the rest 0.8, the
# 'a' of 4 filled in), the open hat on every off-beat, the ride on every 8th (one bar, repeated)
HATS_16THS = ([Note(s, CLOSED, 0.5, 1.0 if s % 4 == 0 else 0.8) for s in range(16) if s % 4 != 2]
              + [Note(s, OPEN, 0.5, 0.8) for s in (2, 6, 10, 14)]
              + [Note(s, RIDE, 0.5, 1.0 if s % 4 == 2 else 0.6) for s in range(0, 16, 2)])
# the anvil on 8ths (drop 2): a soft tick on the beats, the off-beats ringing, the last off-beat leading into the next
# bar, under the anvil's own syncopated phrase (the loop's, in the blips' gaps), which stays on top
ANVIL_8THS = sorted(ANVILS + [Note(16 * b + s, ANVIL, 0.5, v) for b in range(4)
                              for s, v in ((0, 0.2), (2, 0.6), (4, 0.2), (6, 0.8), (8, 0.2), (10, 0.6), (12, 0.2),
                                           (14, 0.8))])
# a line's turnaround bar (PARTS['drums']['turn']): the hats' groove bar with the closed 16ths of beat 4 left out and
# the open hat and the ride on its 'and' at full, the hats breathing into the next line; the anvil answering in that
# space on the swung 16ths, 'e' and 'a' of beats 3 and 4, as it answers the blips in the loop
TURN = {'hats': ([Note(s, CLOSED, 0.5, v) for s, v in ((0, 1.0), (1, 0.6), (3, 0.8), (4, 0.8), (5, 0.4), (7, 0.8),
                                                         (8, 1.0), (9, 0.6), (11, 0.8))]
                 + [Note(s, OPEN, 0.5, v) for s, v in ((2, 0.6), (6, 0.8), (10, 0.6), (14, 1.0))]
                 + [Note(s, RIDE, 0.5, v) for s, v in ((2, 1.0), (6, 0.8), (10, 1.0), (14, 1.0))]),
        'perc': [Note(s, ANVIL, 0.5, v) for s, v in ((9, 1.0), (11, 0.4), (13, 0.6), (15, 0.8))]}
# the 16-bar points' turnaround (PARTS['drums']['turns']['big']), bigger than the 8-bar one, so a DJ hears the 16s: the
# closed hats out for the bar's second half, the open hat and the ride on its two off-beats at full; the snare's ghosts
# dragging into the downbeat on the last beat's 'e', 'and' and 'a'; the anvil's own bar
TURNS = {'big': {
    'hats': ([Note(s, CLOSED, 0.5, v) for s, v in ((0, 1.0), (1, 0.6), (3, 0.8), (4, 0.8), (5, 0.4), (7, 0.8))]
             + [Note(s, OPEN, 0.5, v) for s, v in ((2, 0.6), (6, 0.8), (10, 1.0), (14, 1.0))]
             + [Note(s, RIDE, 0.5, v) for s, v in ((2, 1.0), (6, 0.8), (10, 1.0), (14, 1.0))]),
    'snare': [Note(s, SNARE, 0.5, v) for s, v in ((4, 1.0), (12, 0.8), (13, 0.2), (14, 0.4), (15, 0.6))],
    'perc': [Note(s, ANVIL, 0.5, v) for s, v in ((9, 1.0), (11, 0.4))]}}
VARIANTS = {'ride on 8ths': {'hats': RIDE_ON_8THS},
            'build': {'hats': HATS_16THS},
            'drop 2': {'hats': RIDE_ON_8THS, 'perc': ANVIL_8THS},
            'peak': {'hats': HATS_16THS, 'perc': ANVIL_8THS}}
# the blips' skeleton (a subset of the recorded sequence, so it plays the phrase's own slices): the arpeggio's notes on
# the beats, E5 G5 B5 in each bar, leaving the 'a' of each beat, where the vox riff sings, to the vox
SKELETON = [n for n in blips() if n[0] in (0, 4, 12, 16, 20, 28)]
# without the vox, drop 2's last line keeps its call and response: where the vox answers the blips' call (bars 2, 4, 6
# and 8), the zap and the metal chord answer on the 'and' of 4, thrown, as the no-vox loop answers the vox's throws
NOVOX_ANSWER = tuple(Hit(32 * k + 30, name, 1, throw=True) for k, name in enumerate(
    ('AccentZapE5', 'AccentMetalChordEm', 'AccentZapE5', 'AccentMetalChordEm')))
METAL, RAVE, ACID = 'Metal chords, Em Em Am Bm', 'Rave stab riff', 'Acid stab riff'
DRUMS = 'kick rumble snare hats perc'


def ride(*mode):
    """The acid's filter ride over a line: 'loop' (the 4-bar ride, the recorded stem), ('hold', v), ('ramp', a, b)
    or ('points', ((bar, v), ...)), CC74 and CC71 together (their values move together, as the recipe's ride does)."""
    return {'Bass': mode[0] if mode == ('loop',) else mode}


# The form (224 bars; the acid's CC74/CC71 value in brackets, 0 closed (the line as written) to 127 wide open (x2.6,
# resonance 0.96)):
# Intro 1-32, drums only: the kick alone (1-8); the rumble and the off-beat hats (9); the snare and the 16th hats (17);
#   the anvil (25). A turnaround in every line but the first: at 8 bars the hats breathe on beat 4 and the anvil
#   answers; at the 16-bar points a bigger one, the hats out for the bar's second half and the snare dragging in.
# Groove 33-96: the acid enters closed, the recorded line at rest [0] (33); the drone (41) as the filter starts to
#   open; the blips' call (49); the whole blip sequence, the stutter fill and a noise sweep into 65 (57); the metal
#   chords Em Em Am Bm on the off-beats, the drone out to give them room, the blips back to their call (65); the drone
#   under the chords, the blips' skeleton, the loop's accents in the second block (73); the ride on 8ths, the chords
#   out, the whole sequence over the drone (81); the chords back, the skeleton, the vox's D4 thrown into the break
#   with a reverse swell, the fill (89). The filter opens line by line as a hand rides it, not in even steps: barely
#   at first [0-8], then pushing into the first fill [20-40], easing as the chords arrive [40-46], pushing again into
#   the break [72-92].
# Break 97-112: no drums: the floor falls away (the sub drop on 97); the vox riff over the metal chords, the drone out,
#   the accents only in its second half, swelling into 105 (97); the drone back, the acid alone, closed and creeping
#   open [0-24], the off-beat hats rising from 40% (105), into the build.
# Build 113-128: the kick and the rumble back, the hats off-beat then the loop's, the anvil, the acid opening [24-80]
#   (113); the hats on every 16th, rising from half, the iron riser, the acid wide open [127] over the last 2 bars, the
#   anvil rolling on every 16th in 127 alone, the stutter fill alone in 128 (121).
# Drop 129-160: the impact (sub drop and distorted chord) on 129; the snare back; the vox's loop pattern, the blips'
#   skeleton, the drone out, the loop's accents (129); the rave stab riff over the drone, the drums out in 144 with the
#   acid at the top of its ride and a noise sweep into 145 (137); the vox riff over the metal chords (145); the rave
#   stab riff with the ride on 8ths and the skeleton, a reverse swell and the fill into drop 2 (153). The acid plays
#   its loop ride all through: bars 3-4 of every 4 swell (the recorded Acid Line 1 Ride).
# Drop 2 161-192: Acid Line 2, at rest [0] (the recorded line), the anvil on 8ths and the ride on 8ths, the acid stab
#   riff over the drone, the impact (161); the filter opening [0-64], the blips' skeleton, a noise sweep into 177
#   (169); the vox riff, the hats on every 16th, the drone and the stabs out, the filter wide open [64-127] (177); the
#   strip: the loop's drums, the blips' call answered by the vox, its last D4 thrown into the outro (without the vox,
#   the zap and the metal chord answer, thrown), the filter closing [127, held near the top 2 bars, to 0], the fill
#   (185).
# Outro 193-224, drums only: drop 2's drums (193); the snare out, the ride on 8ths on (201); the rumble out (209);
#   the kick and the off-beat hats (217): not the intro reversed.
FORM = (
    section('Intro', 'intro', (
        line('kick'),
        line('kick rumble hats', hats='thin', end='turn', turn='big'),
        line('kick rumble snare hats', end='turn'),
        line(DRUMS, end='turn', turn='big'))),
    section('Groove', 'groove', (
        line(DRUMS + ' bass', ride=ride('hold', 0), end='turn', stab=True),
        line(DRUMS + ' bass chords', ride=ride('ramp', 0, 8), end='turn', turn='big'),
        line(DRUMS + ' bass chords lead', lead='call', ride=ride('ramp', 8, 20), end='turn', stab=True),
        line(DRUMS + ' bass chords lead', ride=ride('ramp', 20, 40), end='fill', into='uplift'),
        line(DRUMS + ' bass lead', lead='call', phrases=(METAL,), ride=ride('ramp', 40, 46), end='turn', stab=True),
        line(DRUMS + ' bass chords lead', lead='skeleton', phrases=(METAL,), ride=ride('ramp', 46, 58),
             accents='alternate', end='turn', turn='big'),
        line(DRUMS + ' bass chords lead', drums='ride on 8ths', ride=ride('ramp', 58, 72), end='turn', stab=True),
        line(DRUMS + ' bass lead', drums='ride on 8ths', lead='skeleton', phrases=(METAL,), ride=ride('ramp', 72, 92),
             throw=True, end='fill', into='swell'))),
    section('Break', 'break', (
        line('vox', vox='riff', phrases=(METAL,), accents='alternate', downbeat='fall', cue='Vox'),
        line('hats bass chords', hats='thin', rise=('hats', 0.4, 1.0), ride=ride('ramp', 0, 24), accents='lead-in',
             end='turn', turn='big'))),
    section('Build', 'build', (
        line('kick rumble hats perc bass chords', hats=('thin', 'full'), ride=ride('ramp', 24, 80), end='turn'),
        line('kick rumble hats bass chords', drums='build', rise=('hats', 0.5, 1.0),
             ride=ride('points', ((0, 80), (6, 127))), cue='Riser', end='fill',
             roll=dict(lane='perc', bars=1, every=((1, 1),), ramp=(0.3, 1.0))))),
    section('Drop', 'drop', (
        line(DRUMS + ' bass lead vox', lead='skeleton', vox='loop', accents='loop', downbeat='impact',
             ride=ride('loop'), cue='Vox', end='turn', stab=True),
        line(DRUMS + ' bass chords', phrases=(RAVE,), ride=ride('loop'), end='drop', into='uplift'),
        line(DRUMS + ' bass vox', vox='riff', phrases=(METAL,), accents='loop', ride=ride('loop'), cue='Vox',
             end='turn'),
        line(DRUMS + ' bass chords lead', drums='ride on 8ths', lead='skeleton', phrases=(RAVE,), ride=ride('loop'),
             end='fill', into='swell'))),
    section('Drop 2', 'drop2', (
        line(DRUMS + ' bass chords', bass='AcidLine2', drums='drop 2', phrases=(ACID,), ride=ride('hold', 0),
             downbeat='impact', end='turn', stab=True),
        line(DRUMS + ' bass chords lead', bass='AcidLine2', drums='drop 2', lead='skeleton', phrases=(ACID,),
             ride=ride('ramp', 0, 64), end='turn', turn='big', into='uplift'),
        line(DRUMS + ' bass vox', bass='AcidLine2', drums='peak', vox='riff', accents='loop',
             ride=ride('points', ((0, 64), (6, 127))), cue='Vox', end='turn'),
        line(DRUMS + ' bass lead vox', bass='AcidLine2', lead='call', vox='answer', hits={'novox': NOVOX_ANSWER},
             ride=ride('points', ((0, 127), (2, 116), (8, 0))), cue='Vox', end='fill'))),
    section('Outro', 'outro', (
        line(DRUMS, drums='drop 2', end='turn'),
        line('kick rumble hats perc', drums='ride on 8ths', end='turn', turn='big'),
        line('kick hats perc', end='turn'),
        line('kick hats', hats='thin'))),
)


PARTS = dict(
    drums=dict(
        map={RUMBLE: 'Rumble', 36: 'KickSplit', 38: 'SnareGated', 42: 'ClosedHat', 46: 'OpenHat', 47: 'Anvil',
             51: 'Ride'},
        lanes={RUMBLE: 'rumble'},
        note=('Put the closed and open hats in one choke group: the closed hat cuts the open one; keep the ride out of '
              'it, a short ping over a noise wash that plays over them. The rumble (35) is a 2-bar loop under the '
              "kick, gated by its note's length."),
        turn=TURN, turns=TURNS, variants=VARIANTS,
    ),
    groove={'rumble': [(0, RUMBLE, 32, 1.0, False), (32, RUMBLE, 32, 1.0, False)]},   # where no fill cuts it
    bass=dict(items=('AcidClean', 'AcidWarm'), lines={line.name: line_notes(line) for line in B.LINES}, pick=B.PICK,
              note=("The squelch ticks on the kicks (velocity 57) are the voice above 800 Hz alone in the recipe: as "
                    'MIDI notes they play the whole acid sample.')),
    lead=dict(item='ModBlip', name='Modular blip sequence', notes=blips(), skeleton=SKELETON),
    chords=dict(items=('DroneEmAdd9',), name='Drone Em(add9), held',
                trigger=[(0, 52, 64, 1.0, False)], voicing=[(0, n, 64, 1.0, False) for n in Pd.CHORD]),
    vox=dict(item='VoxSynth', riff=V.riff_midi(), throws=(V.THROW_STEP,), vowels=('oo', 'oh')),
    accents=(Key('AccentRaveStabEm'), Key('AccentNoiseSweepE'), Key('AccentReverseHitEm'), Key('AccentZapE5'),
             Key('AccentMetalChordEm'), Key('AccentAcidStabE4'), Key('AccentDistChordEm'), Key('AccentVoxChordEm')),
    fx=(Key('StutterFill'), Key('IronRiser'), Key('AccentSubDropE2')),
    chops=(Key('VoxStabOhE3'), Key('VoxStabOoB3')),
    extras={'Acid Stab': 'AccentAcidStabE4', 'Metal Chord': 'AccentMetalChordEm', 'Rave Stab': 'AccentRaveStabEm'},
    frames={'AccentRaveStabEm': 52874, 'AccentNoiseSweepE': 84706, 'AccentReverseHitEm': 63529, 'AccentZapE5': 51506,
            'AccentMetalChordEm': 83323, 'AccentAcidStabE4': 27114, 'AccentDistChordEm': 62400,
            'AccentVoxChordEm': 148343, 'StutterFill': 84706, 'IronRiser': 677647, 'AccentSubDropE2': 78176,
            'VoxStabOhE3': 146275, 'VoxStabOoB3': 144878},
    cues=dict(riser='IronRiser', impact=('AccentSubDropE2', 'AccentDistChordEm'), swell='AccentReverseHitEm',
              uplift='AccentNoiseSweepE', stab='AccentRaveStabEm', fall='AccentSubDropE2'),
    loop=dict(
        drums=(hits(C.KICK, 36) + hits(C.SNARE, 38) + hits(C.HAT_CLOSED, 42) + hits(C.HAT_OPEN, 46)
               + hits(C.ANVIL, 47) + hits(C.RIDE, 51)
               + [Note(at, RUMBLE, min(32, C.KICK_OUT - at), 1.0) for at in C.RUMBLE_AT]),
        vox=[Note(s, m, ln, v) for s, m, ln, v in C5.VOX_NOTES],
        throws=tuple(C5.VOX_THROWS),
        accents={'vox': loop_accents(C5.VOX), 'novox': loop_accents(C5.NOVOX)},
        fx=[Hit(C.FILL_AT, 'StutterFill', 1)],
    ),
    phrases=[dict(name='Vox synth riff', file='VoxSynthRiff', on='vox', item='VoxSynth', notes=V.riff_midi(),
                  throws=(V.THROW_STEP,), bars=2)]
    + [dict(ph, file=file, on=on, item=item) for ph, (file, on, item) in zip(
        A.midi_phrases(), (('AcidStabRiff', 'Acid Stab', 'AccentAcidStabE4'),
                           ('MetalChords', 'Metal Chord', 'AccentMetalChordEm'),
                           ('RaveStabRiff', 'Rave Stab', 'AccentRaveStabEm')))],
    rides=[dict(on='Bass', cc=74, law='log', lo=CUT[0], hi=CUT[1], unit='Hz',
                what="the acid's ride: its cutoff x1 over bars 1-2, opening through bar 3, x2.6 on the fill bar's "
                     'third beat, closed again by the downbeat',
                values={name: cut for name, (cut, _) in RIDES.items()}),
           dict(on='Bass', cc=71, law='lin', lo=round(RESO[0] * 1000), hi=round(RESO[1] * 1000), unit='permille',
                what="the acid's resonance: at rest over bars 1-2, rising over bar 3 to its top on the fill bar",
                values={name: res for name, (_, res) in RIDES.items()})],
    recorded={'bass': 'the Acid Line 1 Ride (`Loops/Acid Line 1 Ride Clean.wav` and Warm), its ride baked in; the '
                      'other lines from `Loops/Acid Line 1` and `Acid Line 2`',
              'lead': 'the Mod Blip Phrase (`Loops/Mod Blip Phrase.wav`), its ratchets included'},
    form=FORM,
)
