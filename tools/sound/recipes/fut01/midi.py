"""FUT01's written music for the MIDI export (tools/sound/midi.py, MIDI v2; the schema: tools/sound/README.md, "MIDI").

- The loop (Loop_Vox, Loop_NoVox): the round 5 context loop (context5.py on context4.py) lane by lane: the 2-step kick,
  the garage clap, the swung closed hats, the open hats, the tambourine on every 16th, the rim ghosts and the alloy hit
  (the drums stop on beat 4 of bar 4 for the glitch fill, which plays from its own beat 2, as the loop plays it); the
  Bounce warp line twice; the chrome pluck phrase twice; the organ stabs (no-vox: the one on 16th 30 gives way to the
  warp stab); the vox synth riff at 16ths 0 and 32; the variant's accents.
- The parts: the three warp-bass lines (the 2-step Bounce; Low, the organ bass's line on channel 11; the 4x4 Speed
  line), the pluck phrase, the organ stabs at their roots, F#m9 on F#3 and Bm9 on B3, and five synth phrases.
- The extended mix: the UK garage form (midi.UKG_FORM) until the kit engineer writes FUT01's own (TODO below).
Notes are (16th, MIDI note, length in 16ths, velocity 0-1, slide); a pitch dip is written as two slides, down and
back.
"""
import math

from tools.sound import channels as CH
from tools.sound.midi import Hit, Key, Note, hits
from tools.sound.recipes.fut01 import context4 as C, context5 as C5, keys as Ky, music as M, riffs as R, voxsynth as V

TAMBOURINE = 54                     # GM's tambourine
RIM = 37                            # GM's side stick
LINE_NAMES = {'garage': 'Bounce', 'organ': 'Low', 'speed': 'Speed'}
WOBBLE_TOP = C.WOBBLE['base'] * max(C.WOBBLE['depth'])   # the wobble's widest cutoff, Hz


def line(notes):
    """A context4 line, (16th, length, note, velocity, slide), as MIDI notes: a slide or a dip is a slide."""
    return [(s, n, ln, v, bool(sl)) for s, ln, n, v, sl in notes]


def wobble_values():
    """The bass lane's wobble as CC74 every 60 ticks over the 4 bars, from context4.wobble_cutoff: base x depth^w, w
    rising from 0 to 1 and back once a straight 8th, its depth per bar; on channels.cc_value's law between the base
    (650 Hz) and the widest (2210 Hz). Its Q is fixed (1.6): no CC71."""
    out = []
    for i in range(256):
        t = i * 60
        w = 0.5 - 0.5 * math.cos(2 * math.pi * t / 480)
        hz = C.WOBBLE['base'] * C.WOBBLE['depth'][t // 3840] ** w
        out.append(CH.cc_value(hz, C.WOBBLE['base'], WOBBLE_TOP))
    return out


def loop_accents(variant):
    """context5's accents for a variant as hits: 'end' ends the file on its 16th (64: the loop's end)."""
    return [Hit(step, name, layer, 'end' if how == 'end' else 'on', gate)
            for _, name, step, layer, how, gate in C5.ACCENTS[variant]]


PARTS = dict(
    drums=dict(
        map={36: 'KickGarage', RIM: 'RimShot', 38: 'ClapGarage', 42: 'HatClosed', 46: 'HatOpen', 47: 'AlloyHit',
             TAMBOURINE: 'Tambourine'},
        lanes={RIM: 'perc'},
        note=("Kick Split (kick A, round 1's pick) and the Shaker (for the tambourine) are the alternates. Choke Hat "
              'Open with Hat Closed (one choke group), as the loop plays them.'),
        # TODO(FUT01 engineer): busy (audit B: closed hats on the even 16ths at 0.25-0.3, plus the shaker), turn (a
        # kick on the 'a' of 4 at 0.6, the open hat on the 'and' of 4 at 1.0, the tambourine out on beat 4, a rim on
        # 16th 14) and variants={'4x4': ...} (context4's KICK_4X4 and CLAP_4X4, for the Speed line's switch-up)
    ),
    bass=dict(
        items=('WarpClean', 'WarpWarm'),
        lines={LINE_NAMES['garage']: line(C.WARP_LINE), LINE_NAMES['organ']: line(C.ORGAN_LINE),
               LINE_NAMES['speed']: line(C.SPEED_LINE)},
        pick=LINE_NAMES['garage'],
        on={LINE_NAMES['organ']: 'Organ Bass'},       # the Low line is the organ bass's (channel 11)
        # TODO(FUT01 engineer): switch='Speed' once the 4x4 drum variant exists (Speed was written for a 4x4 kick)
        note='The Low line plays the Organ Bass on channel 11, rooted as the warp bass (F#1, A1, C#2).',
    ),
    lead=dict(item='ChromePluck', name='Chrome pluck phrase', notes=[(s, n, ln, v, False) for s, n, ln, v in M.PHRASE]),
    chords=dict(items=('OrganStab', 'EPianoStab'), name='Organ stabs, F#m9 and Bm9',
                trigger=[(s, z, 1, v, False) for s, z, v in C.STABS],
                voicing=[(s, z + d, 1, v, False) for s, z, v in C.STABS for d in Ky.SHAPE]),
    vox=dict(item='VoxSynth', riff=list(V.RIFF_NOTES)),
    accents=(Key('ReverseStab'), Key('NoiseUplifter'), Key('OrganHit'), Key('WarpStab'), Key('RaveStab'),
             Key('ChordSweep'), Key('VoxSynthStab'), Key('AlloyBell')),
    fx=(Key('GlitchFillLift'), Key('GlitchFillLift', start=C.FILL_FROM, name='GlitchFillLiftBeat4'),
        Key('ShiftRiser'), Key('SubDrop')),
    chops=(Key('VoxChopAhCs5'), Key('VoxChopEeE5'), Key('VoxChopAhEeFs5'), Key('VoxChopOhA4'), Key('VoxChopAhOhB4')),
    extras={'Organ Bass': 'OrganBass', 'Organ Lead': 'OrganLead', 'Air Pad': 'AirPadFsm9'},
    frames={'ReverseStab': 62400, 'NoiseUplifter': 171940, 'OrganHit': 73300, 'WarpStab': 18739, 'RaveStab': 33989,
            'ChordSweep': 74548, 'VoxSynthStab': 22227, 'AlloyBell': 119440, 'GlitchFillLift': 42985,
            'ShiftRiser': 687761, 'SubDrop': 87223, 'VoxChopAhCs5': 20242, 'VoxChopEeE5': 14955,
            'VoxChopAhEeFs5': 25378, 'VoxChopOhA4': 20355, 'VoxChopAhOhB4': 25710},
    cues=dict(riser='ShiftRiser', impact=('SubDrop', 'AlloyBell'), swell='ReverseStab', uplift='NoiseUplifter',
              stab='OrganHit'),
    loop=dict(
        drums=(hits(C.KICK, 36) + hits(C.CLAP, 38) + hits(C.HAT_C, 42) + hits(C.HAT_O, 46) + hits(C.TAMB, TAMBOURINE)
               + hits(C.RIM, RIM) + hits(C.PERC, 47)),
        vox=[Note(s + at, n, ln, v) for at in C5.RIFF_AT['vox'] for s, n, ln, v, _ in V.RIFF_NOTES],
        accents={v: loop_accents(v) for v in ('vox', 'novox')},
        fx=[Hit(C.FILL_AT, 'GlitchFillLiftBeat4', 1)],
        stab_drop={'novox': tuple(C5.STAB_DROP['novox'])},
    ),
    phrases=[
        dict(name='Organ riff', file='OrganRiff', on='Organ Lead', item='OrganLead', notes=list(R.ORGAN_RIFF), bars=4),
        dict(name='Warp riff', file='WarpRiff', on='bass', item='WarpClean', notes=line(R.WARP_RIFF), bars=2),
        dict(name='Pluck riff', file='PluckRiff', on='lead', item='ChromePluck', notes=list(R.PLUCK_RIFF), bars=4),
        dict(name='E-piano stab rhythm', file='EPianoStabRhythm', on='chords', item='EPianoStab', notes=R.stab_notes(),
             bars=2),
        dict(name='Vox synth riff', file='VoxSynthRiff', on='vox', item='VoxSynth', notes=list(V.RIFF_NOTES), bars=2),
    ],
    rides=[dict(on='Bass', cc=74, law='log', lo=round(C.WOBBLE['base']), hi=round(WOBBLE_TOP), unit='Hz',
                what="the warp bass's wobble: the lane filter over 450 Hz opening once an 8th, deeper in bars 2 and 4",
                values=wobble_values())],
    recorded={'lead': 'the Chrome Pluck Phrase (`Loops/Chrome Pluck Phrase.wav`), the phrase as the loop plays it',
              'vox': 'the Vox Synth Riff (`Loops/Vox Synth Riff.wav`), its throw baked in'},
    # TODO(FUT01 engineer): form=(...): FUT01's own arrangement (audit B, FUT01): the groove on Low, the drop on
    # Bounce with the vox, a 4x4 switch-up on Speed, drop 2 with the organ riff (phrases=('Organ riff',)), the build's
    # clap roll into the whole glitch fill on beat 3 (cues fill='GlitchFillLift'), the Air Pad under both breaks
    # (channel 13: pad=dict(on='Air Pad', blocks=...), whole-bar chords: F#m9 on F#3 = 54, Bm9 on B3 = 59, the same
    # file), the pluck's call while the vox plays.
)
