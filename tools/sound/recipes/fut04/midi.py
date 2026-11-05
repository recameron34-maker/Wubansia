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
- The extended mix: the techno form (midi.TECHNO_FORM) until the kit engineer writes FUT04's own (TODO below).
Notes are (16th, MIDI note, length in 16ths, velocity 0-1, slide).
"""
import math

from tools.sound import channels as CH
from tools.sound import dsp48 as D
from tools.sound.midi import Hit, Key, Note, hits
from tools.sound.recipes.fut04 import accents as A, bass as B, context as C, context5 as C5, lead as L, pad as Pd
from tools.sound.recipes.fut04 import voxsynth as V

RUMBLE = 35
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


PARTS = dict(
    drums=dict(
        map={RUMBLE: 'Rumble', 36: 'KickSplit', 38: 'SnareGated', 42: 'ClosedHat', 46: 'OpenHat', 47: 'Anvil',
             51: 'Ride'},
        lanes={RUMBLE: 'rumble'},
        note=('Put the closed and open hats in one choke group: the closed hat cuts the open one; keep the ride out of '
              'it, a short ping over a noise wash that plays over them. The rumble (35) is a 2-bar loop under the '
              "kick, gated by its note's length."),
        # TODO(FUT04 engineer): busy and turn; variants (the ride on 8ths from 81, the anvil on 8ths in drop 2)
    ),
    groove={'rumble': [(0, RUMBLE, 32, 1.0, False), (32, RUMBLE, 32, 1.0, False)]},   # where no fill cuts it
    bass=dict(items=('AcidClean', 'AcidWarm'), lines={line.name: line_notes(line) for line in B.LINES}, pick=B.PICK,
              note=("The squelch ticks on the kicks (velocity 57) are the voice above 800 Hz alone in the recipe: as "
                    'MIDI notes they play the whole acid sample.')),
    lead=dict(item='ModBlip', name='Modular blip sequence', notes=blips()),
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
              uplift='AccentNoiseSweepE', stab='AccentRaveStabEm'),
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
    # TODO(FUT04 engineer): form=(...) (audit B, FUT04): the acid closed when it enters and opening across the groove
    # (ride={'Bass': ('ramp', a, b)}), the RIDES swell in the drops, wide open in the build's last 2 bars; the rumble
    # every 2 bars over 9-96 and 113-208, out in the break; the metal chords on the off-beats from 65 (phrases),
    # the rave stab riff in the drop's lines 2 and 4, acid line 2 and the acid stab riff in drop 2; the drone out
    # wherever the vox plays (or the riff up to the E4/G4/B4 zones), the build's anvil or snare roll in 127 only.
)
