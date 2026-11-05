"""The MIDI channel map: one table for every kit (MIDI v2, ADR 0019, docs/midi-map.md).

tools/sound/midi.py writes every content/midi file with it, the kit and mix manifest exporters read it, and the
engine keeps a C++ copy with the same numbers. Change a number only through ADR 0019's owner: the committed MIDI, the
manifests and the plugin's MIDI router all follow it.

Channels count from 1, as DAWs and docs/midi-map.md count them; a MIDI status byte carries the channel less one
(status(), below).

- 1 bass: the kit's bass multisample, A or B by the bass A/B switch.
- 2 lead: the kit's lead multisample.
- 3 voicing: the chord voicing as single notes, for your own synth. Wubansia ignores channel 3.
- 4 chords: the kit's chord sample, each chord triggered at its musical root.
- 5 vox: the vox synth multisample (on FUT02 and FUT04 the velocity picks the vowel).
- 6 accents: one key each from KEY_BASE['accents'] up: accent stabs and hits, zaps, and the reverse swells and
  uplifters the loops use as accents, each at its own zone and layer.
- 7 fx: one key each from KEY_BASE['fx'] up: risers, fills, impacts, sub drops.
- 8 chops: vox chops, one key each from KEY_BASE['chops'] up.
- 9 reserved.
- 10 drums: drums and percussion on DRUM_RANGE (35-59), the hats in one choke group, the ride outside it.
- 11-16: the kit's extras, EXTRAS[kit]: instruments one kit adds, each on its own channel.

Every track of every file plays one of these, and resolves to one of the plugin's LANES (track_lane()).

The controllers (docs/midi-map.md): CC74 and CC71 ride a channel's filter, its cutoff on a logarithmic law between two
frequencies (cc_value(), cc_hz()) and its resonance on a linear one (cc_linear()); CC91 and CC94 throw a note.
"""
import math

CHANNELS = {
    'bass': 1,
    'lead': 2,
    'voicing': 3,
    'chords': 4,
    'vox': 5,
    'accents': 6,
    'fx': 7,
    'chops': 8,
    'reserved': 9,
    'drums': 10,
}
EXTRA_CHANNELS = (11, 12, 13, 14, 15, 16)
KEY_BASE = {'accents': 48, 'fx': 48, 'chops': 48}    # one key per sound from here up
DRUM_RANGE = (35, 59)                                 # every drum and perc note, both ends included
ONE_KEY_ROLES = ('accents', 'fx', 'chops', 'drums')   # a key names a sound; on every other channel a key is a pitch

# the plugin's lanes, in engine/kit/kit.h's order (Lane): permanent, because parameter IDs follow it
LANES = ('kick', 'snare', 'hats', 'perc', 'bass', 'lead', 'chords', 'vox', 'accents', 'fx')
DRUM_LANES = ('kick', 'snare', 'hats', 'perc')
ROLE_LANE = {'bass': 'bass', 'lead': 'lead', 'voicing': 'chords', 'chords': 'chords', 'vox': 'vox',
             'accents': 'accents', 'fx': 'fx', 'chops': 'vox'}

# every kit's extras on channels 11-16: (channel, name, lane). The name is the track's and the instrument's.
EXTRAS = {
    'FUT01': ((11, 'Organ Bass', 'bass'),           # the Low line
              (12, 'Organ Lead', 'lead')),          # the organ riff
    'FUT02': ((11, 'Drop Glide', 'bass'),           # follows the bass A/B switch
              (12, 'Laser Chord Stab', 'chords'),   # the laser stab riff, Dm9 and B-flat maj9
              (13, 'Glass Ping', 'accents')),       # the glass melody
    'FUT03': ((11, 'Pad Warm', 'chords'),),         # the pad bed under the loop
    'FUT04': ((11, 'Acid Stab', 'accents'),         # the acid stab riff
              (12, 'Metal Chord', 'chords'),        # the metal chords, Em Em Am Bm
              (13, 'Rave Stab', 'accents')),        # the rave stab riff
}

# the tracks every file may hold, by name: (role, lane). A drum track's role is 'drums', its lane its own; the kit's
# extras add their own names (track_lane())
TRACKS = {
    'Kick': ('drums', 'kick'),
    'Rumble': ('drums', 'kick'),           # FUT04's rumble, drum note 35, gated by its note's length
    'Snare': ('drums', 'snare'),
    'Clap': ('drums', 'snare'),
    'Snare/Clap': ('drums', 'snare'),
    'Hats': ('drums', 'hats'),
    'Perc': ('drums', 'perc'),
    'Bass': ('bass', 'bass'),
    'Lead': ('lead', 'lead'),
    'Chords': ('chords', 'chords'),
    'Chord voicing': ('voicing', 'chords'),
    'Vox': ('vox', 'vox'),
    'Vox chops': ('chops', 'vox'),
    'Accents': ('accents', 'accents'),
    'FX': ('fx', 'fx'),
}


CUTOFF_CC, RESONANCE_CC = 74, 71   # the filter rides, every RIDE_TICKS while they move
SPACE_CC, ECHO_CC = 91, 94          # the throws: 127 on a thrown note's tick, before its note-on; 0 at its note-off
RIDE_TICKS = 60                     # a ride's points sit on this grid, straight (unswung) ticks from the clip's start


def cc_value(f, lo, hi):
    """A cutoff ride's controller value for a frequency f on a channel whose ride spans lo-hi Hz (logarithmic): round
    (127 ln(f / lo) / ln(hi / lo)), held to 0-127. midi.py writes it from the recipes' own curves; the mix manifest
    (plugin_mix) reads it back with cc_hz(). One law on both sides."""
    v = round(127 * math.log(f / lo) / math.log(hi / lo))
    return max(0, min(127, v))


def cc_hz(v, lo, hi):
    """The frequency a cutoff ride's value stands for: lo x (hi / lo)^(v / 127)."""
    return lo * (hi / lo) ** (v / 127)


def cc_linear(x, lo, hi):
    """A linear ride's value (a resonance) for x between lo and hi: round(127 (x - lo) / (hi - lo)), held to 0-127."""
    return max(0, min(127, round(127 * (x - lo) / (hi - lo))))


def channel(role):
    """A role's channel, 1-16: CHANNELS[role]."""
    return CHANNELS[role]


def status(kind, ch):
    """A channel message's status byte: kind 0x80 (note-off), 0x90 (note-on) or 0xB0 (controller) on channel ch
    (1-16)."""
    if not 1 <= ch <= 16:
        raise ValueError(f'channel {ch} is not 1-16')
    return kind | (ch - 1)


def extras(kit):
    """The kit's extras: ((channel, name, lane), ...), empty for a kit with none."""
    return EXTRAS.get(kit, ())


def extra(kit, name):
    """The channel of the kit's extra called `name` (KeyError if the kit has none by that name)."""
    for ch, n, _ in extras(kit):
        if n == name:
            return ch
    raise KeyError(f'{kit} has no extra called {name!r} on channels 11-16')


def track_lane(kit, name):
    """The plugin lane a track of the kit's files plays on (KeyError for a name no file may carry)."""
    if name in TRACKS:
        return TRACKS[name][1]
    for _, n, lane in extras(kit):
        if n == name:
            return lane
    raise KeyError(f'{kit} files have no track called {name!r}')


def track_channel(kit, name):
    """The channel a track of the kit's files plays on."""
    if name in TRACKS:
        return CHANNELS[TRACKS[name][0]]
    return extra(kit, name)


def channel_roles(kit):
    """{channel: role or extra name} for the kit: the one role each channel plays in all of its files."""
    out = {ch: role for role, ch in CHANNELS.items() if role != 'reserved'}
    out.update({ch: name for ch, name, _ in extras(kit)})
    return out
