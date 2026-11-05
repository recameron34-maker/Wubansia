"""FUT01's written music for the MIDI export (tools/sound/midi.py, MIDI v2; the schema: tools/sound/README.md, "MIDI").

- The loop (Loop_Vox, Loop_NoVox): the round 5 context loop (context5.py on context4.py) lane by lane: the 2-step kick,
  the garage clap, the swung closed hats, the open hats, the tambourine on every 16th, the rim ghosts and the alloy hit
  (the drums stop on beat 4 of bar 4 for the glitch fill, which plays from its own beat 2, as the loop plays it); the
  Bounce warp line twice; the chrome pluck phrase twice; the organ stabs (no-vox: the one on 16th 30 gives way to the
  warp stab); the vox synth riff at 16ths 0 and 32; the variant's accents.
- The parts: the three warp-bass lines (the 2-step Bounce; Low, the organ bass's line on channel 11; the 4x4 Speed
  line), the pluck phrase, the organ stabs at their roots, F#m9 on F#3 and Bm9 on B3, and five synth phrases.
- The extended mix: FUT01's own UK garage form (FORM, below; audit B's FUT01 section): 32 bars of drums to mix in on;
  the groove on Low, the organ bass; a break on the Air Pad with the vox riff; an 8-bar garage build (the 2-step kick,
  the off-beat open hats and the tambourine on, the clap in 8ths then 16ths, the whole glitch fill from beat 3); the
  drop on the Bounce warp line with the vox riff; a 4x4 speed garage switch-up on the Speed line; break 2 with the
  organ riff over the pad's slower F#m9 | Bm9; drop 2 with the organ riff as its new top line, the warp riff for 8
  bars, then the organ bass stripping into 32 bars of drums to mix out on. The vox and the pluck trade calls and
  answers; the drums turn round in every line that doesn't end on a fill, and a percussion element moves every 16 bars.
Notes are (16th, MIDI note, length in 16ths, velocity 0-1, slide); a pitch dip is written as two slides, down and
back.
"""
import math

from tools.sound import channels as CH
from tools.sound import midi as Mi
from tools.sound.midi import BAR, Hit, Key, Note, hits
from tools.sound.recipes.fut01 import context4 as C, context5 as C5, keys as Ky, music as M, riffs as R, voxsynth as V

KICK, CLAP, HAT, OPEN, ALLOY = 36, 38, 42, 46, 47
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


def bar_of(pairs, bar):
    """Bar `bar` (1-4) of a 4-bar (16th, velocity) part, on the bar's own 16ths."""
    lo = BAR * (bar - 1)
    return tuple((s - lo, v) for s, v in pairs if lo <= s < lo + BAR)


def four(*bars_):
    """Four bars of (16th, velocity) hits, bar by bar."""
    return tuple((s + BAR * b, v) for b, bar in enumerate(bars_) for s, v in bar)


# ------------------------------------------------------------------ the drums the extended mix adds (the loop's own
# hits, rearranged; new notes play on the drum rack)
TAMB_16 = four(*[C.TAMB_BAR] * 4)                         # the tambourine on every 16th, as the loop plays it
TAMB_TO_BEAT_4 = tuple((s, v) for s, v in C.TAMB_BAR if s < 12)
TAMB_AND_A = four(*[tuple((s, v) for s, v in C.TAMB_BAR if s % 4 > 1)] * 4)   # only on each beat's 'and' and 'a'
RIMS = four(bar_of(C.RIM, 1), bar_of(C.RIM, 2), bar_of(C.RIM, 3), bar_of(C.RIM, 2))   # where no fill is due
RIMS_ON_THE_A = four(*[((3, 0.35), (7, 0.45), (11, 0.35), (15, 0.6))] * 4)   # a rim on every swung 'a', pushing
HATS_RINGING = tuple((s, v) for s, v in C.HAT_C_BAR if s < 14)   # no closed hat after the 'and' of 4: the open rings
# audit B's turnaround, the loop's bar 2 and then: a kick on the 'a' of 4 at 0.6, the open hat on the 'and' of 4 at
# 1.0 left ringing into the downbeat, the tambourine out on beat 4, a rim on 16th 14 after the one on 13
TURN = {
    'kick': hits(bar_of(C.KICK, 2) + ((15, 0.6),), KICK),
    'hats': hits(HATS_RINGING, HAT) + hits(C.HAT_O_BAR, OPEN),
    'perc': hits(TAMB_TO_BEAT_4, TAMBOURINE) + hits(bar_of(C.RIM, 2) + ((14, 0.6),), RIM),
}
TURNS = {
    # in a line of off-beat hats: the same, its hats only the open ones
    'thin': dict(TURN, hats=hits(C.HAT_O_BAR, OPEN)),
    # the clap rolls in on beat 4's last three 16ths (rising), the kick holds back its pickup, the rims make room
    'flam': {'kick': hits(bar_of(C.KICK, 2), KICK),
             'snare': hits(((4, 1.0), (12, 1.0), (13, 0.35), (14, 0.45), (15, 0.65)), CLAP),
             'perc': hits(TAMB_TO_BEAT_4, TAMBOURINE) + hits(((3, 0.35), (9, 0.45)), RIM)},
    # a breath on beat 4: the kick, the clap and the closed hats stop, the open hat rings and a rim ticks into the 1
    'gap': {'kick': hits(bar_of(C.KICK, 2), KICK), 'snare': hits(((4, 1.0), (12, 1.0)), CLAP),
            'hats': hits(tuple((s, v) for s, v in C.HAT_C_BAR if s < 12), HAT) + hits(C.HAT_O_BAR, OPEN),
            'perc': hits(TAMB_TO_BEAT_4, TAMBOURINE) + hits(((3, 0.35), (9, 0.45), (15, 0.6)), RIM)},
    # under the 4x4 kick (which runs on): the clap's pickup, the open hat ringing, the tambourine out, the rim on 14
    '4x4': {'snare': hits(((4, 1.0), (12, 1.0), (14, 0.45), (15, 0.65)), CLAP), 'hats': TURN['hats'],
            'perc': TURN['perc']},
}
DRUM_VARIANTS = {
    # the speed garage switch-up, as context4's SpeedGarageLoop plays it: KICK_4X4 on every beat, CLAP_4X4 on 2 and 4,
    # the tambourine and the speed loop's two bars of rims, no alloy hit; its fill bar leaves beat 4 to the glitch fill
    '4x4': {'kick': hits(C.KICK_4X4[:4], KICK), 'snare': hits(C.CLAP_4X4[:2], CLAP),
            'perc': hits(TAMB_16, TAMBOURINE) + hits(four(*[bar_of(C.RIM, 1), bar_of(C.RIM, 2)] * 2), RIM),
            'fill': {'kick': hits(C.KICK_4X4[:3], KICK), 'snare': hits(C.CLAP_4X4[:1], CLAP)}},
    # the percussion moves, one element at a time
    'tamb': {'perc': hits(TAMB_16, TAMBOURINE),                                   # the tambourine alone (the build)
             'fill': {'perc': hits(TAMB_TO_BEAT_4, TAMBOURINE)}},
    'alloy': {'perc': hits(TAMB_16, TAMBOURINE) + hits(RIMS, RIM)                   # the alloy hit answers each 2 bars
              + hits(((30, 0.75), (54, 0.5)), ALLOY)},
    'rims': {'perc': hits(TAMB_16, TAMBOURINE) + hits(RIMS_ON_THE_A, RIM) + hits(C.PERC, ALLOY)},
    'tamb8': {'perc': hits(TAMB_AND_A, TAMBOURINE) + hits(RIMS, RIM) + hits(C.PERC, ALLOY)},   # a lighter tambourine
    'alloy2': {'perc': hits(TAMB_16, TAMBOURINE) + hits(RIMS_ON_THE_A, RIM)         # drop 2: both moves at once
               + hits(((14, 0.6), (38, 0.75)), ALLOY)},
}
BUSY = hits(((0, 0.45), (4, 0.4), (8, 0.45), (12, 0.4)), HAT)   # the closed hat on the beats too, on the 0.4 layer

# ------------------------------------------------------------------ harmony the extended mix adds
PAD_V = 0.85
PAD = {   # the Air Pad (channel 13): whole-bar chords, F#m9 on F#3, Bm9 on B3 (one file, one layer)
    'main': [(BAR * b, (54, 59)[b % 2], BAR, PAD_V, False) for b in range(4)],          # F#m9 | Bm9, as the stabs
    'slow': [(BAR * b, (54, 54, 59, 59)[b], BAR, PAD_V, False) for b in range(4)],      # F#m9 F#m9 | Bm9 Bm9
}
SLOW_STABS = [(s + BAR * b, z, v) for b, z in enumerate((54, 54, 59, 59)) for s, _, v in C.STABS_2[:4]
              if not (b == 3 and s == 14)]     # the stabs on break 2's slower harmony; as the loop's, none on the
                                               # 'and' of 4 of bar 4, where the fill plays


def chord_block(stabs):
    """Organ stabs (16th, zone, velocity) as a chord block: the trigger at the roots, the voicing on channel 3."""
    return dict(trigger=[(s, z, 1, v, False) for s, z, v in stabs],
                voicing=[(s, z + d, 1, v, False) for s, z, v in stabs for d in Ky.SHAPE])


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
              "Open with Hat Closed (one choke group), as the loop plays them. The loop's fill bar clears only beat 4: "
              'its drums play through beat 3 and stop at its 16th 12, where the glitch fill plays from its own beat 2 '
              "(FX key 49); the extended mix's build ends on the whole fill from beat 3 (FX key 48), every drum out "
              'from there.'),
        busy=BUSY, turn=TURN, turns=TURNS, variants=DRUM_VARIANTS,
    ),
    bass=dict(
        items=('WarpClean', 'WarpWarm'),
        lines={LINE_NAMES['garage']: line(C.WARP_LINE), LINE_NAMES['organ']: line(C.ORGAN_LINE),
               LINE_NAMES['speed']: line(C.SPEED_LINE)},
        pick=LINE_NAMES['garage'],
        on={LINE_NAMES['organ']: 'Organ Bass'},       # the Low line is the organ bass's (channel 11)
        switch=LINE_NAMES['speed'],                   # the 4x4 switch-up's (Speed was written for the 4x4 kick)
        note='The Low line plays the Organ Bass on channel 11, rooted as the warp bass (F#1, A1, C#2).',
    ),
    lead=dict(item='ChromePluck', name='Chrome pluck phrase', notes=[(s, n, ln, v, False) for s, n, ln, v in M.PHRASE]),
    chords=dict(items=('OrganStab', 'EPianoStab'), name='Organ stabs, F#m9 and Bm9',
                trigger=[(s, z, 1, v, False) for s, z, v in C.STABS],
                voicing=[(s, z + d, 1, v, False) for s, z, v in C.STABS for d in Ky.SHAPE],
                blocks={'slow': chord_block(SLOW_STABS)}),
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
              stab='OrganHit', sweep='ChordSweep'),
    pad=dict(on='Air Pad', blocks=PAD),
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
)

# ------------------------------------------------------------------ the extended mix: FUT01's own form
DRUMS = Mi.DRUMS                    # kick, snare (the clap), hats, perc
GROOVE = DRUMS + ' bass'
ROLL = dict(lane='snare', bars=1, every=((1, 2),), last=1, ramp=(0.45, 1.0))   # the clap: 8ths, then 16ths
SWITCH_CANDY = (Hit(22, 'VoxSynthStab', 2), Hit(86, 'VoxSynthStab', 2))         # 16th 6 of the line's bars 2 and 6
line, section = Mi.line, Mi.section
FORM = (
    # drums to mix in on: the off-beat open hats over the kick and the tambourine, the shuffle, the clap, busier hats
    section('Intro', 'intro', (
        line('kick hats perc', hats='thin', end='turn', turn='thin'),
        line('kick hats perc', end='turn', turn='gap'),
        line(DRUMS, end='turn'),
        line(DRUMS, hats='busy', end='turn', turn='flam'))),
    # the organ bass's Low line, then the stabs, the pluck's call (the alloy hit moving), the vox answering it over
    # busier hats
    section('Groove', 'groove', (
        line(GROOVE, bass='Low', end='turn'),
        line(GROOVE + ' chords', bass='Low', end='fill', into='swell'),
        line(GROOVE + ' chords lead', bass='Low', lead='call', drums='alloy', end='turn', turn='flam', stab=True),
        line(GROOVE + ' chords lead vox', bass='Low', hats='busy', lead='call', vox='answer', cue='Vox', end='fill',
             into='uplift'))),
    # no kick: the riff over the pad (the loop's accents in its second half), then the stabs, the pluck's call and the
    # off-beat hats rising into the fill
    section('Break', 'break', (
        line('pad vox', vox='riff', cue='Vox', downbeat='sweep', accents='alternate'),
        line('hats pad chords lead vox', hats='thin', rise=('hats', 0.4, 1.0), lead='call', vox='riff',
             accents='lead-in', end='fill'))),
    # UK garage: the 2-step kick, the off-beat open hats and the tambourine on, the bass out, the stabs swelling, the
    # vox's call over the riser; the clap in 8ths, then 16ths up to the whole glitch fill on beat 3
    section('Build', 'build', (
        line('kick snare hats perc chords vox', hats='thin', drums='tamb', vox='call', rise=('Chords', 0.55, 1.0),
             cue='Riser', end='fill', fill='GlitchFillLift', roll=ROLL),)),
    # the warp bass's Bounce and the whole vox riff from the sub drop, the pluck out; then its call under the riff; the
    # vox resting while the pluck plays its phrase over busy hats; the call and the answer, the drums out in bar 120
    section('Drop', 'drop', (
        line(GROOVE + ' chords vox', vox='riff', cue='Vox', accents='loop', downbeat='impact', end='turn'),
        line(GROOVE + ' chords lead vox', lead='call', vox='riff', accents='loop', end='fill'),
        line(GROOVE + ' chords lead', hats='busy', drums='rims', accents='alternate', end='turn', turn='gap'),
        line(GROOVE + ' chords lead vox', lead='call', vox='answer', cue='Vox', drums='alloy', accents='loop',
             end='drop', into='swell'))),
    # speed garage: the 4x4 kick and clap, the Speed line between the kicks, the vox synth stab as ear candy, a rave
    # stab into the second line, where the stabs come back over busy hats
    section('Switch-up', 'switch', (
        line(GROOVE, drums='4x4', bass='Speed', downbeat='impact', end='turn', turn='4x4',
             hits=SWITCH_CANDY + (Hit(126, 'RaveStab', 2),)),
        line(GROOVE + ' chords', drums='4x4', bass='Speed', hats='busy', end='fill', hits=SWITCH_CANDY))),
    # the organ riff over the pad's slower harmony (F#m9 for 2 bars, Bm9 for 2), then the stabs on it and the hats
    # rising under the riser into the loop's fill on beat 4 (the whole fill's opening F#4 would rub on the riff's G#4)
    section('Break 2', 'break', (
        line('pad', chords='slow', phrases=('Organ riff',), downbeat='sweep'),
        line('hats pad chords', chords='slow', hats='thin', rise=('hats', 0.4, 1.0), phrases=('Organ riff',),
             cue='Riser', end='fill'))),
    # the organ riff as the new top line, the vox answering it; the warp riff under the riff and the pluck's call; the
    # organ riff alone over the e-piano stab rhythm; the vox calling, the pluck answering; then the organ bass alone
    # with the drums, stripping into the outro
    section('Drop 2', 'drop2', (
        line(GROOVE + ' chords vox', hats='busy', drums='alloy2', vox='answer', cue='Vox', phrases=('Organ riff',),
             accents='loop', downbeat='impact', end='turn'),
        line(DRUMS + ' chords lead vox', hats='busy', lead='call', vox='riff', phrases=('Warp riff',),
             accents='loop', end='fill'),
        line(GROOVE + ' chords', drums='rims', phrases=('Organ riff', 'E-piano stab rhythm'), accents='alternate',
             end='turn', turn='flam'),
        line(GROOVE + ' chords lead vox', hats='busy', lead='answer', vox='call', cue='Vox', accents='loop',
             end='fill'),
        line(GROOVE, bass='Low', end='turn', turn='gap'))),
    # drums to mix out on, the percussion thinning while the kick and the shuffle carry on: busy hats and the rims on
    # the 'a', the alloy hit moving, the clap out under a lighter tambourine, then the tambourine alone
    section('Outro', 'outro', (
        line(DRUMS, hats='busy', drums='rims', end='turn', turn='gap'),
        line(DRUMS, drums='alloy', end='turn', turn='flam'),
        line('kick hats perc', drums='tamb8', end='turn'),
        line('kick hats perc', drums='tamb', end='turn', turn='gap'))),
)
PARTS['form'] = FORM
