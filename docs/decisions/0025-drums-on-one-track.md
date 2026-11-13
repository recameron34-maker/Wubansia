# 0025. Drums on one track for MIDI drags (v0.2)

**Status:** Proposed, 2026-11-12. Built with these choices, which you can still change until v0.2 ships and its file names freeze:
- the switch's wording, "1 drum track" (or "Drum Rack", your words, or "Drums: 1 track");
- the file-name mark, "(1 drum track)";
- the merged track's name, `Drums`;
- the default, off, so today's files stay what they are (it could be on);
- the scope, Loop and Song drags only (it could cover FUT04's kick lane too).

## Context
You asked, working in Ableton Live 12 and Logic Pro on a Mac: "when dragging full song midi's I'd like an option to input the drum midis as a single drum rack midi".

The drag strip's Loop and Song chips hand over the kit's own SMF type-1 files (ADR 0019, `docs/midi-map.md`): one track per instrument, so the drums come as 4 or 5 tracks (Kick, Clap or Snare, Hats, Perc, and FUT04's Rumble), all on channel 10. Live and Logic put each track of a type-1 file on its own DAW track, so a Drum Rack (Live) or Drum Machine Designer (Logic) track gets one drum part and the others land on tracks of their own.

What the 16 Loop and Song files hold, checked on the content:
- the drum tracks are tracks 1-4 (FUT04: 1-5), right after the conductor, in format 1 files at 960 PPQ;
- they hold only their names and notes, all on channel 10, note-offs with velocity 0 and no running status, no zero-length or unended notes, and they end on the conductor's tick;
- in all 137 files in `content/midi`, no key is on two drum tracks of one file, and no drum track has a controller, program change or text event;
- one drum track's note-off meets another's note-on on 103 ticks in FUT02's Song, 419 in FUT03's, 90 in FUT04's and 2 in FUT04's Loop, so tracks interleaved in file order would put a note-on before a note-off there (`docs/midi-map.md`: note-offs first on a tick).

## Decision

### Which drags change
1. **Loop and Song**, the two whole-kit drags, with one switch.
2. **Lane drags don't change.** A lane is already one track, except FUT04's kick lane (Kick and Rumble): its rumble needs a gated pad of its own anyway, and merging it would rename that kit's lane files.
3. **Phrase drags don't change**: phrase files hold no drums.
4. **No Drums-only chip**: the strip has no room (ADR 0024). The switch lit and the six other lanes muted give the conductor and `Drums` from Loop or Song.
5. **With fewer than two drum tracks left after the mutes, the drag is the switch-off drag**, the same name and bytes: there is nothing to merge.
6. **`content/midi`, the region files and the embedded MIDI don't change**: the merge runs at drag time, on the embedded bytes.

### The merged track
7. **Which tracks:** the drag's kept tracks whose name has the drums role in `engine/kit/channel_map.h`'s `kTracks` (channels.py's TRACKS): Kick, Rumble, Snare, Clap, Snare/Clap, Hats, Perc. Never Accents, FX, Vox chops or a kit's extras on channels 11-16.
8. **One track named `Drums`**, in the place of the first of them: track 1, right after the conductor, in every kit file. `Drums` isn't a TRACKS name: it spans four lanes, and no content file holds it.
9. **Every note as it was**: key, tick, length, velocity and release velocity, on channel 10. The keys stay the kit map's: 36 kick, 37 rim, 38 clap or snare, 42 and 46 hats, 51 ride, the percussion per each kit's README, and FUT04's rumble on 35. No GM remap.
10. **Laid out by the SMF writer** (`core/midi/writer.h`): on a tick the name, text events, note-offs, program changes, controllers, note-ons, then by channel and key; every status byte written; ending on the latest end among the merged tracks, which is the conductor's. So note-offs come first on a tick wherever one drum track's note-off meets another's note-on.
11. **Carried as they are:** controllers, program changes and text events (FF 01-0F); each merged track's own name (FF 03) is dropped. **Refused:** pitch bend, key or channel pressure, sysex, any other meta event, a note-on with no note-off, a note-off with nothing to end, and what the writer refuses (overlapping notes on one channel and key, two values of one controller or two program changes on one channel and tick, the same text twice on a tick, a zero-length note). A refused merge fails the drag, and the chip says "Couldn't write"; the tests merge every committed file, so a kit that breaks this fails CI before it reaches you.
12. **Everything else byte for byte:** the header (its track count rewritten), the conductor and every other kept track.

### Names
13. `<KIT> <Title> <BPM> <Key> <Camelot> - <Loop|Song>[ NoVox][ (<marks>)].mid`, the marks being "1 drum track" when the drums merged, then "no <muted lanes>": `FUT01 Alloy Signal 134 F#m 11A - Song NoVox (1 drum track, no Clap, Warp Bass).mid`. With the switch off, or one drum track left at most, the names are today's. Different bytes get different names, one name is one file, and every name passes the three denylists.

### The switch
14. **"1 drum track" on the drag strip**, between Song and the phrase chips: the editor's LED pill toggle, always enabled. Its tooltip names Drum Rack and Drum Machine Designer. Its room comes out of the phrase chips, so a phrase title that won't fit its chip even squeezed to 0.8, in the platform's font, drops its bar count ("E Piano Stab Rhythm · 2 bars" shows "E Piano Stab Rhythm"); the chip's tooltip still names the file.
15. **Off by default**, so today's files stay as they are.
16. **Kept for you, in `settings.xml`** beside the content folder (`drag_drums_one_track="1"` while lit), never in a project and never as a parameter: like the content folder, it's how you work, not part of a song. Every plugin instance and the Standalone app in one process share it; another process picks a change up when it next starts.

### The code
17. `midi::mergeTracks (smf, trackIndices, mergeIndices, name)` in `core/midi/track_select.h`, beside `keepTracks`. `writer.{h,cpp}` don't change, so the generator snapshots don't either.
18. `dragout::DragState::drumsOnOneTrack`, which `wholeDrag` follows.
19. `Controller::dragDrumsOnOneTrack ()` and `setDragDrumsOnOneTrack (bool)`. `ControllerImpl` reads and writes `UserSettings` and never calls `changeSettings`, so the host isn't told the project changed.

## Consequences
- New file names, which freeze with v0.2 like every drag name.
- The drag cache (the 32 newest files) and the content don't change: no MIDI is regenerated, and the four kit READMEs gain a sentence.
- No new parameter, no state change, nothing for ADR 0018, and no new dependency.
- A Song merge takes 1.0 to 2.2 ms in Release on the Linux build machine (FUT03's, with 8,233 drum notes, the longest), inside the mouse event the OS drag starts from.
- Not checked in a DAW: where the other instruments land when a Song is dropped onto an existing track in Live or Logic, the pad settings `dist/INSTALL.md` gives, Logic's region names and its pad count, and drags on Windows and Linux. How a merged clip sounds or plays in either DAW isn't checked at all.
- Once the Samples drawer (ADR 0024, the `sounds` branch) is merged too, one small commit folds this branch's settings seam (`ControllerImpl::setDragSettingsForTesting`, `dragSettingsFile ()`) and save helper (`UserSettings::saveAttributeLocked`) into the drawer's `setUserSettingsForTesting`, `settingsFile ()` and `saveLocked`, extends the tests' own settings file and its round trip to `samples_folder`, and mentions the drum option in the header comments of `Controller.h`, `ControllerImpl.h` and `UserSettings.h`, and on `dist/INSTALL.md`'s settings.xml line. They stay apart until then, so the two branches merge cleanly.

## Alternatives
- **A right-click menu on the chips:** the state is hidden, and people forget it's on.
- **An icon in the label slot:** cryptic.
- **A field in `DragRequest`:** two sources of truth, and the switch is plugin-wide, like the vox switch and the mutes.
- **A parameter:** parameter IDs are permanent, and automating this means nothing. **Project state:** it would mark projects changed, and ADR 0018 fixes the state's root attributes.
- **A Drums-only chip:** no room on the strip.
- **Lane drags too:** see 2.
- **A format-0 file:** it would merge every instrument, not only the drums.
- **Two files per drag (the drums, then the rest):** macOS drops only the first.
- **A `Drums` track written into `content/midi`:** lane mutes and lane drags need one track per instrument.
- **A GM remap of the clap to 39:** ADR 0019 owns the map, and a Drum Rack needs no GM.
