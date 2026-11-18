# The MIDI map (MIDI v2)

Every MIDI file in `content/midi/` follows this map, and so does Wubansia: its player plays those files, its drag strip
hands them out, and its MIDI input reads the same channels. The table lives in code as `tools/sound/channels.py`, which
`tools/sound/midi.py` and the manifest exporters import, and as its C++ copy in the engine; change a number only there,
through ADR 0019 (status: Accepted), and re-export.

The generator's files (`groovecore gen`, ADR 0013) follow the channel table, not every convention below: they are
480 PPQ, a slide overlaps its next note by its style's overlap (30 ticks in FUT01 and FUT04, 24 in FUT03, 20 in
FUT02), FUT04's rumble sits on reserved channel 9 until it becomes drum note 35, and Wubansia plays no generated
chords, since they sit on channel 3.

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
| FUT01 | Organ Bass (the Low line; bass) | Organ Lead (the organ riff; lead) | Air Pad (F#m9 on F#3, Bm9 on B3; chords) |
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
- One track for a drum rack (ADR 0025): with the drag strip's 1 drum track switch lit, a Loop or Song drag carries the
  drum tracks as one track, Drums, where the first of them was, every note as it is and note-offs first on a tick; the
  other tracks don't change. A lane drag still gives the lane's own tracks.

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
- **Gates.** A note's length is its gate. Swing never makes a note overlap a note it didn't overlap as written, and on a
  bass or vox track (a mono line) no note runs into the next one unless that note is a written slide: then the note
  before is held 12 ticks into it, so a mono synth set to legato glides there. A glide inside one note (FUT03's rubber
  bass) is written the same way: the note, then its target slid into from the glide's start. FUT04's blip ratchets are
  retriggers a 16th / n apart.
- **Velocity.** A velocity plays the layer nearest it by |velocity - round(layer x 127)|, the louder on a tie (Wubansia's
  sampler; each kit's README lists every layer's velocity). The files write round(v x 127) for the recipe's velocity
  v, unless that would play another layer than the recipe picks from v (the nearest layer, the louder on a tie), or
  sits where reading velocity / 127 against the layers would: then the integer nearest v x 127 that plays the
  recipe's layer both ways. FUT01's 0.9 kicks, halfway between its 0.8 and 1.0 layers, are 115, not 114; FUT01's and
  FUT03's 0.3 ghost claps are 37, not 38 (as near 25 as 51). One-shots the recipes place by layer carry round(layer x 127); on the vox synths the velocity picks
  the vowel (FUT02: 64 'oh', 95 'ah', 127 'ee'; FUT04: 64 'oo', 127 'oh').
- **End-aligned sounds** (reverse swells, risers, uplifters, sweeps, fills) end on their target: the note starts at
  the target less the sample's length at the kit's tempo and ends on the target, a bar or beat line. A plain sampler
  lands them there at the kit's tempo; Wubansia at any tempo.
- **One-shots** on channels 6, 7 and 8 last their sample's length (or the gate the recipe gives, like FUT01's warp
  stab at 1.75 16ths), cut at the clip's end. A key may start its file part-way: FUT01's loop fill plays Glitch Fill
  Lift from its 16th 4 (the kit manifest's start offset); its whole file is the key below it.

## Controllers
- **Filter rides:** CC74 (cutoff) and CC71 (resonance), every 60 ticks (unswung) while the ride moves, read off the
  recipe's own curve, on the channel whose filter rides. One law on both sides (`channels.cc_value` and `cc_hz`): a
  cutoff's value is round(127 ln(f / lo) / ln(hi / lo)), held to 0-127, and stands for lo x (hi / lo)^(v / 127); a
  resonance's is linear (`cc_linear`). The maps, the mix manifests' (`plugin_mix`):

  | Kit | Channel | Ride | CC74 range | CC71 |
  |---|---|---|---|---|
  | FUT01 | 1 | the warp bass's wobble, once an 8th, deeper in bars 2 and 4 | 650-2210 Hz | none (Q 1.6) |
  | FUT02 | 2 | the arp's ride, open at 16th 46 | 1600-16000 Hz | none (Q 1.4) |
  | FUT03 | 4 | the dub stab's low-pass, open mid-loop | 900-5000 Hz | none (Q 0.9) |
  | FUT04 | 1 | the acid's 4-bar ride: its cutoff multiplier x1-x2.6 | 5400-14040 Hz | 0.90-0.96 (Q 0.707-2.5) |

  FUT04's values are 0 at rest, where the samples play as they are. Each kit's README and the extended mixes'
  `regions.json` give the same ranges.
- **Throws:** CC91 (Space) and CC94 (Echo) go to 127 on a thrown note's tick, before its note-on, and back to 0 at its
  note-off, or sooner at the next note-on on its track that isn't thrown (on that tick, before the note-on), so no
  other note takes the throw: FUT02's vox throw, FUT04's vox throws and thrown accents, and every vox note an extended
  mix throws. Every track on a channel that throws anywhere in a kit's files (a loop, a phrase, a region or a whole
  mix) starts with CC91 and CC94 at 0 on tick 0, before any note-on, so it starts dry whatever played before it.
  Wubansia latches them per note (send = base + (1 - base) x value / 127). They are the GS send controllers, so other
  samplers either use them or ignore them.

## Tracks
One track per instrument, each resolving to one Wubansia lane (`channels.track_lane`): Kick and Rumble (kick), Snare,
Clap or Snare/Clap (snare), Hats, Perc, Bass, Lead, Chords and Chord voicing (chords), Vox and Vox chops (vox),
Accents, FX, plus the kit's extras by name on the lanes in the table above. The first track (the conductor) holds the
file's name, the tempo, 4/4, the key, and in the extended mixes a marker (FF 06) at every section start and a cue point
(FF 07) at every riser, fill and, in the vox mix only, vox entry (on the bar where the line's vox comes in). The cues
are labels for the reader: the notes play.

A Loop or Song drag with 1 drum track lit carries one track named Drums in place of Kick, Rumble, Snare, Clap,
Snare/Clap, Hats and Perc. It isn't a TRACKS name: it spans four lanes, and no file here holds it.

## Files per kit
- `<KIT>_Loop_Vox_4bars.mid` and `<KIT>_Loop_NoVox_4bars.mid`: the kit's context loop as its club master plays it,
  lane by lane, with the vox synth or without.
- Parts: `<KIT>_Drums_4bars.mid`; `<KIT>_Bassline_<Name>_2bars.mid` (`_InTheLoop` for the loop's); `<KIT>_Lead_..._2bars.mid`;
  `<KIT>_Chords_<Sample>_4bars.mid` (channel 4) and `<KIT>_Chords_<Sample>_Voicing_4bars.mid` (channel 3);
  `<KIT>_Accents_Vox_4bars.mid`, `<KIT>_Accents_NoVox_4bars.mid`, `<KIT>_FX_4bars.mid`; and the synth phrases,
  `<KIT>_Phrase_<Name>_<n>bars.mid`, each on its instrument's channel.
- `extended/vox/` and `extended/novox/`: the 224-bar extended mix (`<KIT>_ExtendedMix_Vox_224bars.mid`, or `_NoVox_`),
  one region file per section, and `regions.json` (integers and strings only: the tempo, the swing in permille, the
  length in ms, 1-based start bars, each line's lanes and moves, the cues, the tracks and the rides' ranges; a line's
  `throw` marks only the riff's last note the line adds, while every throw, that one or one written into the riff, the
  loop's vox, a phrase or a hit, is in the files' CC91/CC94). The two differ only in the Vox track (and its Vox cues),
  the accents (and their throws) and the chord stabs the no-vox accents take over (FUT01's at 16th 30).

## The extended mix
Each kit declares its music and its arrangement as data (`PARTS`, `tools/sound/README.md`, "MIDI"). A line is 8 bars;
each lane plays its loop block twice, the fill block where the line ends on a fill. The genres' default forms (sci-fi
club, house, techno, UK garage) give every kit 32 bars of drums at each end, the bass from bar 33, the harmony, lead
and vox coming in line by line, a break with the pad and the vox riff, a per-genre build that rolls up to the fill's
start (its velocity rising hit by hit), a drop that isn't the groove (the vox's loop pattern, the accents in every
block, an impact on its first beat, busier hats in its third line), a switch-up on another bassline, a second break,
drop 2 stripping into the outro, turnarounds every 16 bars where no fill falls, and the accents' rules (the loop's in
the drops, every other block in the grooves, lead-ins into the breaks' ends, none in the drum-only ends). Each kit's
engineer replaces these with the kit's own form.

## In a DAW
- **Ableton Live.** A dragged file's tracks come in as clips on consecutive MIDI tracks; Live keeps the notes but not
  their channels. For Wubansia, turn For my synths off first (lit, the bass comes an octave up, its slides become
  separate notes, short notes hold longer and soft notes play at velocity 72), then drag one lane at a time from its
  drag strip, or set Wubansia's MIDI input to one lane (Lane mode). To play a whole multi-track file into Wubansia,
  give each clip its own MIDI track, set that track's "MIDI To" to Wubansia's track and its channel to the clip's
  channel from the table above, and arm or monitor them. For your own instruments, put a sampler on each track and root
  every sample at its MIDI number. With 1 drum track lit, the drums come as one clip: drop the Song on a track holding
  a Drum Rack with the kit's drums on the notes above. The other instruments then land on the tracks after it, over the
  clips there: `dist/INSTALL.md` gives the drops that write over nothing.
- **Logic Pro.** File > Open on a whole-mix file brings its tempo, key and section markers with it (an import doesn't).
  Imported tracks keep their channels: to play them into Wubansia, turn For my synths off first, as for Live, then on
  Wubansia's track set the MIDI input to Multi (channels as above), or to one lane. For your own instruments, one
  software instrument track per MIDI track. With 1 drum track lit, drop the Song on a Drum Machine Designer track: the
  drums are one region on it.
- **Your own synths.** The drag strip's For my synths switch, lit until you turn it off (ADR 0027), hands every drag
  over for a synth rather than for Wubansia's samples: the bass lane an octave up (38-55, around C1-C2), every pitched
  note a 16th at least but ending 10 ticks before the next note on its track, so a written slide is two plain notes,
  and velocities 72 and up. The drums, the one-key Accents, FX and Vox chops tracks and the conductor stay as this map
  has them (the pitched extras on the accents lane change like the rest); switched off, a drag is the file as it is
  here. Channel 4's Chords and the kits' chord extras still carry each chord's root only: on a synth, play Chord
  voicing (channel 3), or put a chord device before it.
- **Empty tracks.** A loop file carries only the tracks that play in it. Every region of an extended mix carries the
  same tracks in the same order, empty or not: if a DAW leaves an empty track out, move the clips under it onto the
  tracks of the same name.

Not checked yet: none of this has been opened in a DAW from this session; the routing notes are from the DAWs' manuals
and behaviour as documented, not observed.
