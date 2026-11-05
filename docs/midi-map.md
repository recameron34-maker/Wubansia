# The MIDI map (MIDI v2)

Every MIDI file in `content/midi/` follows this map, and so does Wubansia: its player plays those files, its drag strip
hands them out, and its MIDI input reads the same channels. The table lives in code as `tools/sound/channels.py`, which
`tools/sound/midi.py` and the manifest exporters import, and as its C++ copy in the engine. ADR 0019 records the
decision (status: Proposed).

## Channels

| Ch | Role | What it plays | Wubansia lane |
|---|---|---|---|
| 1 | Bass | The kit's bass multisample, A or B by the bass A/B switch | bass |
| 2 | Lead | The kit's lead multisample | lead |
| 3 | Chord voicing | The chords as single notes, for your own synth. Wubansia ignores channel 3 | chords |
| 4 | Chords | The kit's chord sample, each chord triggered at its root | chords |
| 5 | Vox | The vox synth multisample. On FUT02 and FUT04 the velocity picks the vowel | vox |
| 6 | Accents | One key per sound from 48 up: accent stabs and hits, zaps, the reverse swells and uplifters | accents |
| 7 | FX | One key per sound from 48 up: risers, fills, impacts, sub drops | fx |
| 8 | Vox chops | One key per chop from 48 up | vox |
| 9 | Reserved | Nothing | |
| 10 | Drums | Drums and percussion on notes 35-59 | kick, snare, hats, perc |
| 11-16 | The kit's extras | Instruments one kit adds, each on its own channel (below) | per extra |

The kits' extras:

| Kit | 11 | 12 | 13 |
|---|---|---|---|
| FUT01 | Organ Bass (the Low line; bass) | Organ Lead (the organ riff; lead) | |
| FUT02 | Drop Glide (follows the bass A/B; bass) | Laser Chord Stab (the stab riff; chords) | Glass Ping (the glass melody; accents) |
| FUT03 | Pad Warm (the pad bed; chords) | | |
| FUT04 | Acid Stab (the acid stab riff; accents) | Metal Chord (the metal chords; chords) | Rave Stab (the rave stab riff; accents) |

Each kit's README (`content/midi/<kit>/README.md`) lists its keys: the drum map, the zones and roots of every pitched
instrument, and the sound on each key of channels 6, 7 and 8.

### Drums (channel 10)
- 36 kick (kick A or B by the kick A/B switch), 37 rim, 38 clap or snare.
- 42 closed hat and 46 open hat, in one choke group (the closed hat cuts the open one); 51 ride, outside it.
- The other percussion notes as each kit's README lists them; FUT03's congas and bongos are zones of one sample each.
- 35: FUT04's rumble, a 2-bar loop gated by its note's length (its own track, Rumble, on the kick lane).
- Drum notes are 120-tick triggers: play them as one-shots, so the samples ring out (a gated pad would cut the open
  hats).

## Conventions
- **Roots.** Every pitched sample is triggered at its musical root: root each file at its MIDI number. The sample names
  count C4 = 60 (the note Ableton and Logic call C3).
- **Grid.** 960 ticks a quarter, 4/4. The swing is written in: every odd 16th is late by round((swing - 0.5) x 480)
  ticks, 43, 10, 29 and 19 for FUT01-FUT04. Wubansia's player snaps those to the exact swing (43.2, 9.6, 28.8 and 19.2
  ticks: at most 9 samples), so a recorded phrase joins its stem.
- **Tempo.** One tempo event at tick 0, round(60,000,000 / BPM) microseconds a quarter: the integer closest to the
  kit's tempo. 128 BPM is exact; 134, 130 and 136 BPM come out 0.4-1.1 parts per million fast (134.00006, 130.00013
  and 136.00015 BPM, which a DAW showing four decimals rounds to x.0001), at most 0.42 ms early over a 224-bar mix.
  No integer microsecond count comes closer: the next one is 1.1-4.2 times further off.
- **Key.** Every file carries a key signature (FF 59) at tick 0: F# minor, D minor, G minor, E minor.
- **On a tick:** note-offs first, then controllers, then note-ons. No note overlaps another on its own key; tracks
  end on a bar line; no note runs past its track's end.
- **Gates.** A note's length is its gate. Swing never makes a note overlap the next one on its track unless it was
  written to (a slide): where a written slide glides into a note, the note before is held 12 ticks into it, so a mono
  synth set to legato glides there.
- **Velocity.** velocity = round(v x 127) for the recipe's velocity v, and it picks the sample layer the recipe picked
  (the nearest layer, the louder on a tie; the exporter checks it). One-shots the recipes place by layer carry that
  layer's velocity, round(layer x 127). On the vox synths the velocity picks the vowel.
- **End-aligned sounds** (reverse swells, risers, uplifters, sweeps, fills) end on their target: the note starts at
  the target less the sample's length at the kit's tempo and ends on the target, a bar or beat line. A plain sampler
  lands them there at the kit's tempo; Wubansia at any tempo.
- **One-shots** on channels 6, 7 and 8 last their sample's length (or the gate the recipe gives, like FUT01's warp
  stab at 1.75 16ths), cut at the clip's end.

## Controllers
- **Filter rides:** CC74 (cutoff) and CC71 (resonance), every 60 ticks while the ride moves, on the channel whose
  filter rides: FUT01's bass wobble (channel 1), FUT02's arp ride (channel 2), FUT03's dub stab low-pass (channel 4),
  FUT04's acid cutoff and resonance (channel 1). Each kit's README gives the range a value maps to (a cutoff is
  logarithmic between two frequencies, a resonance linear between two values), and so do the extended mixes'
  `regions.json`.
- **Throws:** CC91 (Space) and CC94 (Echo) go to 127 on a thrown note's tick, before its note-on, and back to 0 at its
  note-off: FUT02's vox throw, FUT04's vox throws and thrown accents, and every vox note an extended mix throws.
  Wubansia latches them per note (send = base + (1 - base) x value / 127). They are the GS send controllers, so other
  samplers either use them or ignore them.

## Tracks
One track per instrument, each resolving to one Wubansia lane: Kick, Rumble, Snare (or Clap), Hats, Perc, Bass, Lead,
Chords, Chord voicing, Vox, Vox chops, Accents, FX, plus the kit's extras by name. The first track (the conductor)
holds the file's name, the tempo, 4/4, the key, and in the extended mixes a marker (FF 06) at every section start and a
cue point (FF 07) at every riser, fill and vox entry. The cues are labels for the reader: the notes play.

## Files per kit
- `<KIT>_Loop_Vox_4bars.mid` and `<KIT>_Loop_NoVox_4bars.mid`: the kit's context loop as its club master plays it,
  lane by lane, with the vox synth or without.
- Parts: `<KIT>_Drums_4bars.mid`; `<KIT>_Bassline_<Name>_2bars.mid` (`_InTheLoop` for the loop's); `<KIT>_Lead_..._2bars.mid`;
  `<KIT>_Chords_<Sample>_4bars.mid` (channel 4) and `<KIT>_Chords_<Sample>_Voicing_4bars.mid` (channel 3);
  `<KIT>_Accents_Vox_4bars.mid`, `<KIT>_Accents_NoVox_4bars.mid`, `<KIT>_FX_4bars.mid`; and the synth phrases,
  `<KIT>_Phrase_<Name>_<n>bars.mid`, each on its instrument's channel.
- `extended/vox/` and `extended/novox/`: the 224-bar extended mix (`<KIT>_ExtendedMix_Vox_224bars.mid`, or `_NoVox_`),
  one region file per section, and `regions.json` (integers and strings only).

## In a DAW
- **Ableton Live.** A dragged file's tracks come in as clips on consecutive MIDI tracks; Live keeps the notes but not
  their channels. For Wubansia, drag one lane at a time from its drag strip, or set Wubansia's MIDI input to one lane
  (Lane mode). To play a whole multi-track file into Wubansia, give each clip its own MIDI track, set that track's
  "MIDI To" to Wubansia's track and its channel to the clip's channel from the table above, and arm or monitor them.
  For your own instruments, put a sampler on each track and root every sample at its MIDI number.
- **Logic Pro.** File > Open on a whole-mix file brings its tempo, key and section markers with it (an import doesn't).
  Imported tracks keep their channels: on Wubansia's track, set the MIDI input to Multi (channels as above), or to one
  lane. For your own instruments, one software instrument track per MIDI track.
- **Empty tracks.** A loop file carries only the tracks that play in it. Every region of an extended mix carries the
  same tracks in the same order, empty or not: if a DAW leaves an empty track out, move the clips under it onto the
  tracks of the same name.

Not checked yet: none of this has been opened in a DAW from this session; the routing notes are from the DAWs' manuals
and behaviour as documented, not observed.
