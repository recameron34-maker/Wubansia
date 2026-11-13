# Installing Wubansia on a Mac

Wubansia is a VST3, AU and CLAP instrument and a standalone app. This zip holds the plugins and the app. The kits'
samples come as content zips, one per kit (`Wubansia-Content-FUT01-v1.zip` and so on). The plugin needs both: without
the samples it loads, shows its kits and stays silent.

## The quick way

1. Unzip this zip. You get a folder called `Wubansia`.
2. Put the content zips next to that folder, for example both in Downloads. Leave them zipped. If your browser
   unzipped them already, the "Wubansia Content" folders it made work too.
3. Quit your DAW.
4. Open Terminal (in Applications > Utilities), type `bash ` with a space after it, drag `install-macos.sh` from the
   Wubansia folder onto the Terminal window, and press Return.

That's all: open your DAW and add Wubansia as an instrument.

The command is `bash install-macos.sh`, with these options:
- `--dry-run` checks the zips and prints every step without changing anything;
- `--plugins-only` installs only the plugins and the app;
- `--content-only` installs only the samples, for example when a new content zip arrives.

You can also name content zips or folders after the options: `bash install-macos.sh --content-only ~/Desktop/Wubansia-Content-FUT01-v1.zip`.

Run it with `bash` as above rather than double-clicking it: macOS's Gatekeeper stops downloaded scripts you open, but
not a script that bash reads. It needs no password; don't use `sudo`, because it installs for your own user.

### What it does

- Copies `Wubansia.vst3` to `~/Library/Audio/Plug-Ins/VST3/`, `Wubansia.component` (the AU) to
  `~/Library/Audio/Plug-Ins/Components/` and `Wubansia.clap` to `~/Library/Audio/Plug-Ins/CLAP/`, and
  `Wubansia.app` to `/Applications` (or to `~/Applications` if you can't write to `/Applications`). Whatever copy is
  already there is replaced, whichever build it is: it prints the installed and the new build's versions first, so
  run it from the newest download.
- Checks each new copy before it replaces the old one: it clears the quarantine flag (`com.apple.quarantine`) on it
  (these builds are signed ad hoc, not notarized by Apple, so with the flag on, macOS refuses to open them) and
  checks its signature with `codesign --verify --strict`. If anything fails, or you press Ctrl-C, the old one stays.
- For each content zip, checks every file against the kit's `SHA256SUMS`, then puts the kit in
  `~/Library/Application Support/Wubansia Audio/Wubansia/Content/<kit>/v<N>/`, for example `.../Content/fut01/v1/`.
  Nothing is installed from a damaged zip. It never replaces a sample that's already there with different bytes:
  it stops, names the file and changes nothing for that kit. If two downloads hold different builds of one kit
  version (Safari's "Wubansia Content" and "Wubansia Content 2", say), it installs neither and names both. Running
  it again is safe.
- Restarts `AudioComponentRegistrar`, so Logic, GarageBand and other AU hosts find the new AU.
- Prints each step it takes.

## By hand

The same steps in Terminal. First go into the unzipped Wubansia folder: type `cd ` with a space, drag the folder onto
Terminal and press Return. Paste each block whole: it stops at the first command that fails.

1. The plugins, replacing whatever copies are there:

   ```
   mkdir -p ~/Library/Audio/Plug-Ins/VST3 ~/Library/Audio/Plug-Ins/Components ~/Library/Audio/Plug-Ins/CLAP &&
   rm -rf ~/Library/Audio/Plug-Ins/VST3/Wubansia.vst3 ~/Library/Audio/Plug-Ins/Components/Wubansia.component \
          ~/Library/Audio/Plug-Ins/CLAP/Wubansia.clap &&
   ditto Wubansia.vst3 ~/Library/Audio/Plug-Ins/VST3/Wubansia.vst3 &&
   ditto Wubansia.component ~/Library/Audio/Plug-Ins/Components/Wubansia.component &&
   ditto Wubansia.clap ~/Library/Audio/Plug-Ins/CLAP/Wubansia.clap &&
   xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/Wubansia.vst3 \
         ~/Library/Audio/Plug-Ins/Components/Wubansia.component ~/Library/Audio/Plug-Ins/CLAP/Wubansia.clap &&
   echo "The plugins are installed."
   ```

2. The app, in `/Applications`, or in `~/Applications` if you can't write to `/Applications`:

   ```
   apps=/Applications; [ -w /Applications ] || apps=~/Applications
   mkdir -p "$apps" && rm -rf "$apps/Wubansia.app" && ditto Wubansia.app "$apps/Wubansia.app" &&
   xattr -dr com.apple.quarantine "$apps/Wubansia.app" && echo "The app is in $apps."
   ```

3. Let AU hosts see the new AU: `killall -9 AudioComponentRegistrar`

4. For each content zip (FUT01 here; change the first line for another kit), unzip it, check every file and copy
   the kit's version folder in, only if that version isn't installed yet:

   ```
   ( set -e; zip=~/Downloads/Wubansia-Content-FUT01-v1.zip; kit=fut01; v=v1
     content="$HOME/Library/Application Support/Wubansia Audio/Wubansia/Content"
     if [ -e "$content/$kit/$v" ]; then echo "$content/$kit/$v is already there: move it aside first"; exit 1; fi
     rm -rf /tmp/wubansia-content
     ditto -x -k "$zip" /tmp/wubansia-content
     cd "/tmp/wubansia-content/Wubansia Content/$kit/$v"
     shasum -a 256 -c SHA256SUMS
     mkdir -p "$content/$kit"
     ditto . "$content/$kit/$v"
     rm -rf /tmp/wubansia-content
     echo "Installed $kit $v." )
   ```

   Every line `shasum` prints must end in `OK`; if one doesn't, the block stops and installs nothing.

   Or in Finder: open the zip, choose Go > Go to Folder, and enter
   `~/Library/Application Support/Wubansia Audio/Wubansia/Content` (make the folders if they aren't there). In it,
   make a folder named after the kit (`fut01`) if there isn't one, and drag the version folder (`v1`) from "Wubansia
   Content/fut01" into it. If a `v1` folder is already there, don't click Replace: that's another build of the same
   version. Move the old one aside first, or use the script, which checks.

Or leave the content wherever you like: unzip it anywhere and choose that "Wubansia Content" folder with "Choose
content folder…" in Wubansia's window. The plugin remembers it for your user, not per project.

## Samples into your DAW

Every kit's sounds are in Wubansia's Samples drawer: hear them, then drag the ones you like into your DAW as WAV
files.

1. Click **Samples** at the top right of Wubansia's window. The drawer opens over the lanes and the effects, on the
   kit that plays, and the groove keeps playing. Click **Samples** again, or ×, to close it.
2. Pick a category on the left: Accents, FX, Vox, Synths, Bass, Drums and Loops. The kit chips at the top show
   another kit's sounds without changing the kit that plays or your project; the playing kit's own chip brings the
   drawer back to it.
3. Click a sound to hear it. It plays dry, exactly as the file you'll get, 6 dB down on top of whatever plays, and
   the Output knob acts on it as on the groove. After a click in the list, the Up and Down arrows move through it
   and play each sound, Return plays it again, and Esc or ■ stops it. If your DAW keeps those keys for itself, use
   the mouse.
4. Drag the sound's row, or the "Drag into your DAW" tile on the right, onto a track. Wubansia copies the kit's own
   48 kHz / 24-bit WAV, byte for byte, into your samples folder, then hands that copy to your DAW. Its name says what
   it is: `Rave Stab v2 - FUT01 134 F#m 11A.wav` is Rave Stab's loud layer, from FUT01 at 134 BPM in F# minor, 11A
   on the Camelot wheel.

A sound recorded on several notes, velocity layers (`v1` is the softest) or round robins (`rr1`, `rr2`...) lists
every file in the **File** box: pick one to hear it, and the drag and Show in Finder use it from then on, while
Wubansia's window is open. Note names count middle C as C4, so `Chrome Pluck F#4 v2` is the note Live and Logic call
F#3.

Drops from the drawer are new in v0.2 and not yet checked in Live or Logic. If your DAW won't take one, click the
tile, or **Show in Finder**: that saves the file the same way and shows it in Finder, to drag from there.

The line above the tile says how a sound sits on the grid:
- "Ends on the beat: put its end on the downbeat (8 bars)": these sounds (risers, fills, reverse hits and some
  sweeps, marked END in the list) end on the beat they lead into. Line the end of the clip up with that downbeat;
  an 8-bar riser then starts 8 bars before it.
- "Starts 25 ms before its beat: slide it 25 ms earlier": the sound has a lead-in before its beat. Put it on the
  grid, then move it 25 ms earlier.
- "2 bars at 134 BPM": a loop of whole bars at the kit's tempo. At another tempo your DAW has to stretch it.
- "9 recorded notes: pick one in File": the File box has each note.

The drawer plays through Wubansia's output, so a recording or a real-time bounce of that output takes in a sound you
click; an offline bounce never does.

### Where the copies go

The drawer's footer says "Drags save to ~/Music/Wubansia/Samples". Each kit gets a folder there, so the copy above
is `~/Music/Wubansia/Samples/FUT01 Alloy Signal/Rave Stab v2 - FUT01 134 F#m 11A.wav`. **Change…** picks another
folder; Wubansia keeps your choice for you, not in projects. It refuses a folder inside the content folder, which
holds only the kits' own files, or inside Wubansia's own folders (`~/Library/Application Support/Wubansia Audio`,
which the Uninstall below deletes, and `~/Library/Caches/Wubansia Audio`), and one it can't make.

The folder is yours to keep. Wubansia never deletes or overwrites anything in it: dragging a sound again reuses its
copy, and if you edited a copy in place, the next drag of that sound saves a new one beside it, `... (2).wav`. A
project can play a file where it is (Live does, and Logic does unless it copies it), so keep each file as long as a
project uses it, or collect it into the project first (below). A file is at most 4.3 MB; every file of the four kits
comes to about 205 MB.

If a kit's samples aren't installed, its sounds show dimmed and the drawer says what's missing, with **Choose
content folder…**. **Content…** opens the panel that says where the kits' samples are.

### In Ableton Live

- Live doesn't copy a file you drop: the Set plays it from your samples folder. File > Collect All and Save copies
  every file the Set uses into its project folder; do that before you move the project to another computer.
- To find your copies again in Live's browser, add the samples folder to Places (Add Folder… under Places).
- Live may write `.asd` files (its analysis) beside the WAVs. Wubansia leaves them alone.

### In Logic Pro

- In File > Project Settings > Assets, "Copy audio files into project" decides whether Logic copies a file you drop
  into the project or plays it from your samples folder.
- The files are 48 kHz. In a 44.1 kHz project, turn on "Convert audio file sample rate when importing" in the same
  place before you drop, or the file can play at the wrong speed and pitch.
- To hear the drawer while Logic is stopped, select Wubansia's track (or arm it), or press Play. When no sound comes,
  the drawer says: "No sound from your DAW: Logic only runs a stopped instrument track while it's selected or armed.
  Select this track, or press Play."

## Where everything goes

- `~/Library/Audio/Plug-Ins/VST3/Wubansia.vst3`
- `~/Library/Audio/Plug-Ins/Components/Wubansia.component` (AU)
- `~/Library/Audio/Plug-Ins/CLAP/Wubansia.clap`
- `/Applications/Wubansia.app` (or `~/Applications/Wubansia.app`)
- `~/Library/Application Support/Wubansia Audio/Wubansia/Content/<kit>/v<N>/`: the samples, about 35-70 MB a kit,
  with the kit's `SHA256SUMS`. A later content version goes in its own `v<N>` folder next to it.
- `~/Library/Application Support/Wubansia Audio/Wubansia/settings.xml`: the content folder and the samples folder
  you chose, if any.
- `~/Music/Wubansia/Samples/<KIT> <Title>/`: the WAVs the Samples drawer saved for your drags (or the folder you
  chose with Change…). Wubansia never deletes them, and uninstalling leaves them: your projects use them.

On Windows the content folder is `%APPDATA%\Wubansia Audio\Wubansia\Content`, and on Linux
`~/.local/share/Wubansia Audio/Wubansia/Content`. From a clone of the repo, this installs a content zip there:
`python3 tools/content/install.py --from-zip <zip>`. On both, drags from the Samples drawer save to
`Wubansia/Samples` in your Music folder.

## If something's wrong

- **Your DAW doesn't list Wubansia.** Make it rescan its plug-ins. In Logic, open Logic Pro > Settings > Plug-in
  Manager; if Wubansia shows as failed, select it and click Reset & Rescan Selection. Ableton Live finds VST3s in
  `~/Library/Audio/Plug-Ins/VST3` when "Use VST3 Plug-In System Folders" is on in its Plug-Ins settings.
- **macOS says Wubansia "is damaged" or "can't be opened".** The quarantine flag is still on: run the `xattr` line
  of steps 1 and 2 of By hand.
- **Wubansia loads but is silent and says content is missing.** The kit's samples aren't where it looks, or they're
  another content version. Its window names the first missing file and the folders it searched. Install the content
  zips, or choose their folder with "Choose content folder…".
- **The script stops with "holds another build of".** A different build of that kit's content version is already
  installed. Move the folder it names aside (or delete it) and run the script again.
- **The script says two downloads "hold different builds of it".** Two of the zips or "Wubansia Content" folders
  next to it are different builds of one kit version. Delete the older one, or name the one you want:
  `bash install-macos.sh --content-only <zip>`.
- **Bitwig's offline bounce stalls with the CLAP.** A known issue with CLAP plug-ins that report latency
  (clap-juce-extensions #184): use the VST3 in Bitwig.
- **Your DAW won't take a drop from the Samples drawer.** Click the tile, or Show in Finder, and drag the file from
  Finder. If the drawer says "Couldn't save", choose another folder with Change….

## Uninstall

```
rm -rf ~/Library/Audio/Plug-Ins/VST3/Wubansia.vst3 ~/Library/Audio/Plug-Ins/Components/Wubansia.component \
       ~/Library/Audio/Plug-Ins/CLAP/Wubansia.clap /Applications/Wubansia.app ~/Applications/Wubansia.app \
       "$HOME/Library/Application Support/Wubansia Audio"
killall -9 AudioComponentRegistrar
```

This leaves the WAVs you dragged out of the Samples drawer, in `~/Music/Wubansia/Samples` (or the folder you chose
with Change…): projects that use them still need them. Delete `~/Music/Wubansia` yourself once none does.
