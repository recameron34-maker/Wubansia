# 0030. Drum kits

**Status:** Accepted, 2026-11-18, in under ADR 0028 (build in, cut later). Built with these choices, which you can cut
or change: the button's word, "Drum kit"; one folder per kit beside the Samples drawer's, both kicks in it; pad files
named by the drum maps' notes (36 C1); the SFZ at the mix's faders and pans (not 0 dB); SFZ as the only sampler format;
a drag hands over the SFZ, not the pads.

## Context
You asked (2026-11-18) for "being able to pull in the full extended midi for drum kits; and from there being able to put
in my own drum samples", and for "drum kits I could pull out of the plugin into my daw". The drum MIDI (Loop, Song, the
Drums part of ADR 0029, 1 drum track of ADR 0025) is all on channel 10, but until now its sounds came out one file at a
time from the Samples drawer (ADR 0024), named for the drawer rather than for the pads, with no map of layers and round
robins and no levels.

The four kits' pads, as the plan gives them (level: the instrument's gain plus its channel's gain inserts on that kick
side; pan: the channel's pan insert):

| Kit | Pads: key note sound (level dB, pan %) |
|---|---|
| FUT01 | 36 C1 Kick Split (A) and Kick Garage (B) (0); 37 C#1 Rim Shot (-11, 30 R); 38 D1 Clap Garage (-2); 42 F#1 Hat Closed (-9.5, 22 R, chokes); 46 A#1 Hat Open (-12.5, 15 L, choked over 5 ms); 47 B1 Alloy Hit (-8, 25 L); 54 F#2 Tambourine (-12.5, 30 L) |
| FUT02 | 36 C1 Kick Split (-2); 38 D1 Electro Clap (-2.5); 42 F#1 Closed Hat (-3.44, 15 R, chokes); 46 A#1 Open Hat (-3.44, 15 R, choked over 4 ms); 47 B1 Zap (-12.3, pan cycle) |
| FUT03 | 36 C1 Kick Deep (0); 37 C#1 Rimshot (-5); 38 D1 Clap Snare (-1); 41 F1, 43 G1, 45 A1 Congas (-5); 42 F#1 Closed Hat Deep (-5, 15 R, chokes); 46 A#1 Open Hat Deep (-5, 15 R, choked over 4 ms); 47 B1, 48 C2 Bongos (-5); 54 F#2 Shaker Deep (-9, 20 L) |
| FUT04 | 35 B0 Rumble (-14.42, loops while held, 5 ms release, restrike_continues); 36 C1 Kick Split (-2.658); 38 D1 Snare Gated (-0.85); 42 F#1 Closed Hat (-3.717, 15 R, chokes); 46 A#1 Open Hat (-3.717, 15 R, choked over 4 ms); 47 B1 Anvil (-6.328, 25 L); 51 D#2 Ride (-14.829) |

## Decision
1. **The button.** "Drum kit" on the Drag MIDI strip (component `drag_drumkit`), 6 px right of 1 drum track, drawn as a
   drag chip with a 2 x 2 pad glyph, as wide as its widest label ("Drum kit", "Saving…", "Couldn't save") in its font
   plus the chip's 36 px of margins: 114 px in DejaVu Sans, leaving 44 of the strip's 164 free. A press asks for the
   kit (the work starts at once on the drum kit thread); a click shows the SFZ for the kick switch's side once saved,
   once; a 4 px drag hands that SFZ to the OS once saved, and while saving starts nothing (macOS starts an OS drag only
   inside its mouse event) and says to drag again. Return and the accessibility press act as a click; Space is left to
   the host. Dimmed while the kit's samples aren't ready. No parameter, no setting, nothing in the project.
2. **The folder.** `<samples folder>/<KIT> <Title> Drum Kit[ c<N>]/`, beside the drawer's `<KIT> <Title>/`, in safe
   characters, ` c<N>` from content version 2 on; both kick sides in one folder, so its bytes depend only on the kit's
   manifests. A folder of that name holding every planned file with the planned bytes is the kit: nothing is written
   (extra files, like Live's `.asd` files, don't count). Anything else at the name, a changed folder, a file or a link,
   is never touched: the kit goes to `(2)`, then `(3)`, up to `(999)`. Written whole into a hidden staging folder
   (`.wubansia-drumkit-<KIT>-<8 hex digits>`) and renamed into place without replacing anything (renameat2
   RENAME_NOREPLACE on Linux, renamex_np RENAME_EXCL on macOS, MoveFileExW without replace on Windows); a failure or a
   cancel deletes the staging folder. Old folders stay.
3. **The contents.** `Pads/<key> <note> <sound> - <KIT>.wav`, one per drum note, its top layer's first round robin;
   `Samples/<key> <note> <installed name> - <KIT>.wav`, every layer and round robin of the zone that plays the note;
   one SFZ per kick side (`<folder> - <label>.sfz`, FUT01's Kick Garage and Kick Split) or one (`<folder>.sfz`); and
   `README.txt`. The pads are every key of every channel-10 keymap entry and both sides of an A/B entry: the notes the
   drum MIDI plays. Notes are named as the drum maps name them, 36 C1. Every WAV is the installed file, byte for byte,
   checked against kit.json before it is written. FUT01: 100 files; FUT02: 67; FUT03: 115; FUT04: 80.
4. **The SFZ.** A `<group>` per pad: `key=` (or `lokey`/`hikey`/`pitch_keycenter` when the zone's root differs);
   layer i's `lovel`..`hivel` as engine::pickLayer picks (nearest, the louder on a tie); `seq_length` the lcm of the
   round robins and the pan cycle (refused over 100), region j playing `rr[(j - 1) % r]` panned `cycle[(j - 1) % p] / 10`;
   `amp_veltrack=0`; `loop_mode` one_shot, no_loop or loop_sustain with kit.json's loop points; `volume` and `pan` from
   the level and pan above; the hat choke as `group`/`off_by`/`off_mode=normal` with the fade as `ampeg_release`;
   `<control> default_path=Samples/` and `sample=` last on each line. Refused, naming the kit, the pad and its sound:
   end alignment, mono or a glide, layered_with, a pre-roll, pow15, `restrike_continues` on any pad but a looped one, a
   level or pan on the vox switch, a drum switch other than the kick's.
5. **The README.** The kit, the folder's parts, the pad table (note, sound, level to 0.1 dB rounded half up, pan, how it
   plays), and the steps for Live, Logic and SFZ players, with the kit-dependent lines (the SFZ list, the Live step for
   pads off C1 to D#2, the Gate and Classic steps for a looped pad). One line per paragraph, ASCII.
6. **Determinism.** `planDrumKit` is pure: integers only in the texts, no date, version or path, so the same manifests
   give the same bytes on every platform and build, and a saved folder is found again. The four kits' texts are
   committed in `plugin/tests/data/drumkit/`; a deliberate change lands with new texts in their own commit, and the next
   save goes to `(2)`.
7. **The thread.** `DrumKitService` ("Wubansia drum kits"), one per processor, started by its first request, destroyed
   first. One slot, latest wins: an equal request (kit, content version, samples folder, content folder, kick side)
   keeps its ticket; another cancels the job in hand between files. The last ready result is kept with its request: an
   equal press gets that ticket back, ready at once, after two stats (folder and SFZ there), so a drag right after the
   press starts in its mouse event (re-reading and hashing FUT01's 89 drum files took 92-106 ms, longer than a 4 px
   drag); if either is gone it saves again. Nothing touches the audio thread.
8. **The words.** "Drum kit", "Saving…" and "Couldn't save", with a tooltip sentence and one screen-reader announcement
   per change: "Saving FUT01's drum kit…" (plus "drag again when it's ready." after a drag while saving), "Saved FUT01's
   drum kit: FUT01 Alloy Signal Drum Kit, 100 files.", "FUT01's drum kit is saved: …", and "Couldn't save FUT01's drum
   kit: <the Samples drawer's problem>." with its advice; the disk's or the reader's words go in the tooltip only.
9. **The checks.** C++ tests read every SFZ back and check every (pad, velocity 1-127, sequence position) against
   engine::pickZone, engine::pickLayer and `rr % count`; the texts against the committed ones; the writer and the thread
   on the synthetic kit. `tools/content/tests/test_drumkit_sfz.py` runs sfzlint on the committed SFZ files and plays
   them through sfizz (pysfizz) with tone files, every one-shot pad at both ends of every layer for two rounds of round
   robins, FUT01's choke and FUT04's rumble loop. It runs in CI's sound job (`WUBANSIA_SFZ_TOOLS=1`) and skips where
   the modules are missing.

## What a sampler won't do as Wubansia does
- The mix beyond each channel's fader and pan: EQ, width, clip, ducks, sends, the drum bus and the club master. These
  are the dry sounds at the mix's balance.
- Round robins: Wubansia restarts them every 4 bars from the song's start (and at each transport start from MIDI in); an
  SFZ player counts on. The same files, in turn, in another phase.
- The choke's fade shape (linear or cos² over 4-5 ms) and the rumble's raised-cosine release become the player's release
  envelope over the same time.
- FUT04's rumble: struck again on its own end tick, Wubansia continues the voice (`restrike_continues`) while an SFZ
  player starts a new one, so each note restarts the loop; seamless at the kit's 136 BPM, where 85 of 92 rumble notes
  per Song start on the previous note's end, within 10 ms of the file's loop point.
- The pan law: Wubansia's is equal power with unity in the centre; sfizz's puts a stereo file at `pan=30` 2.63 dB down
  on the left.
- sfizz plays every pad 11.5 dB under its `volume`, and treats each note's first processing block differently.

## Consequences
- Up to 42.4 MB for all four kits per content version in the samples folder, never deleted by Wubansia.
- No state, no parameter, no setting. The Controller gains `exportDrumKit` and `drumKitStatus`, the processor a third
  service thread.
- Test-time dependencies only, nothing linked into the plugin: sfzlint 0.1.4, lark-parser 0.12.0, PyYAML 6.0.3 and
  appdirs 1.4.4 (MIT), pysfizz 0.1.3 (BSD-2-Clause, bundling sfizz, BSD-2-Clause, and nanobind, BSD-3-Clause), named in
  the PR as ADR 0028 decision 6 asks.
- `dist/INSTALL.md` gains "Drum kits into your DAW". Live and Logic don't open `.sfz` files, so a drag onto them does
  nothing: the tooltip points a drag at an SFZ player, and INSTALL.md and the README give the Pads/ steps instead.
- Sharps (`#`) in sample names: sfzlint and sfizz read them; other players aren't checked. The fallback, sharps spelled
  `s`, is a one-line change while nothing has shipped.

## Alternatives
- Decent Sampler presets, Ableton `.adg` racks, SoundFont 2: more formats to keep right, each for one host; SFZ is read
  by sfizz, sforzando and Bitwig's Sampler, and the pads cover Live and Logic.
- A multi-WAV drag onto a Drum Rack: Live fills pads in a row from the drop point and macOS drops only the first of
  several files (ADR 0025), so the pads would land wrong, silently. Silent filler pads to keep the row: clutter, and the
  order isn't verified.
- Rendering the pads through the mix (M6's audio export), or printing them at their levels: ADR 0024 keeps the
  installed bytes.
- A folder per kick side (every other pad twice), or an SFZ beside the drawer's flat kit folder (its names can't sort
  as pads, and a `(2)` there would change the SFZ's bytes).

## Not verified
Any DAW; sforzando, Bitwig and the sfizz plugin opening the SFZ; the README's Live and Logic steps; writing `~/Music` from
Logic's AU host; the Windows paths (MoveFileExW) and macOS's renamex_np, which no test here runs.
