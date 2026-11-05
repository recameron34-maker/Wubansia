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
  `Wubansia.app` to `/Applications` (or to `~/Applications` if you can't write to `/Applications`). An older copy is
  replaced.
- Clears the quarantine flag (`com.apple.quarantine`) on each. These builds are signed ad hoc, not notarized by
  Apple, so with the flag on, macOS refuses to open them. Then it checks each signature with
  `codesign --verify --strict`.
- For each content zip, checks every file against the kit's `SHA256SUMS`, then puts the kit in
  `~/Library/Application Support/Wubansia Audio/Wubansia/Content/<kit>/v<N>/`, for example `.../Content/fut01/v1/`.
  Nothing is installed from a damaged zip. It never replaces a sample that's already there with different bytes:
  it stops, names the file and changes nothing for that kit. Running it again is safe.
- Restarts `AudioComponentRegistrar`, so Logic, GarageBand and other AU hosts find the new AU.
- Prints each step it takes.

## By hand

The same steps in Terminal. First go into the unzipped Wubansia folder: type `cd ` with a space, drag the folder onto
Terminal and press Return.

1. Copy the plugins and the app, replacing older copies:

   ```
   mkdir -p ~/Library/Audio/Plug-Ins/VST3 ~/Library/Audio/Plug-Ins/Components ~/Library/Audio/Plug-Ins/CLAP
   rm -rf ~/Library/Audio/Plug-Ins/VST3/Wubansia.vst3 ~/Library/Audio/Plug-Ins/Components/Wubansia.component \
          ~/Library/Audio/Plug-Ins/CLAP/Wubansia.clap /Applications/Wubansia.app
   ditto Wubansia.vst3 ~/Library/Audio/Plug-Ins/VST3/Wubansia.vst3
   ditto Wubansia.component ~/Library/Audio/Plug-Ins/Components/Wubansia.component
   ditto Wubansia.clap ~/Library/Audio/Plug-Ins/CLAP/Wubansia.clap
   ditto Wubansia.app /Applications/Wubansia.app
   ```

2. Clear the quarantine flag:

   ```
   xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/Wubansia.vst3 \
         ~/Library/Audio/Plug-Ins/Components/Wubansia.component ~/Library/Audio/Plug-Ins/CLAP/Wubansia.clap \
         /Applications/Wubansia.app
   ```

3. Let AU hosts see the new AU: `killall -9 AudioComponentRegistrar`

4. For each content zip (FUT01 here), unzip it, check it (every line must end in `OK`) and merge it into the
   content folder:

   ```
   ditto -x -k ~/Downloads/Wubansia-Content-FUT01-v1.zip /tmp/wubansia-content
   (cd "/tmp/wubansia-content/Wubansia Content/fut01/v1" && shasum -a 256 -c SHA256SUMS)
   mkdir -p "$HOME/Library/Application Support/Wubansia Audio/Wubansia/Content"
   ditto "/tmp/wubansia-content/Wubansia Content" "$HOME/Library/Application Support/Wubansia Audio/Wubansia/Content"
   rm -rf /tmp/wubansia-content
   ```

   Or in Finder: open the zip, choose Go > Go to Folder, enter
   `~/Library/Application Support/Wubansia Audio/Wubansia/Content` (make the folders if they aren't there), and
   move the kit's folder (`fut01`) from "Wubansia Content" into it.

Or leave the content wherever you like: unzip it anywhere and choose that "Wubansia Content" folder with "Choose
content folder…" in Wubansia's window. The plugin remembers it for your user, not per project.

## Where everything goes

- `~/Library/Audio/Plug-Ins/VST3/Wubansia.vst3`
- `~/Library/Audio/Plug-Ins/Components/Wubansia.component` (AU)
- `~/Library/Audio/Plug-Ins/CLAP/Wubansia.clap`
- `/Applications/Wubansia.app` (or `~/Applications/Wubansia.app`)
- `~/Library/Application Support/Wubansia Audio/Wubansia/Content/<kit>/v<N>/`: the samples, about 35-70 MB a kit,
  with the kit's `SHA256SUMS`. A later content version goes in its own `v<N>` folder next to it.
- `~/Library/Application Support/Wubansia Audio/Wubansia/settings.xml`: the content folder you chose, if any.

## If something's wrong

- **Your DAW doesn't list Wubansia.** Make it rescan its plug-ins. In Logic, open Logic Pro > Settings > Plug-in
  Manager; if Wubansia shows as failed, select it and click Reset & Rescan Selection. Ableton Live finds VST3s in
  `~/Library/Audio/Plug-Ins/VST3` when "Use VST3 Plug-In System Folders" is on in its Plug-Ins settings.
- **macOS says Wubansia "is damaged" or "can't be opened".** The quarantine flag is still on: run step 2 of By hand.
- **Wubansia loads but is silent and says content is missing.** The kit's samples aren't where it looks, or they're
  another content version. Its window names the first missing file and the folders it searched. Install the content
  zips, or choose their folder with "Choose content folder…".
- **The script stops with "holds another build of".** A different build of that kit's content version is already
  installed. Move the folder it names aside (or delete it) and run the script again.
- **Bitwig's offline bounce stalls with the CLAP.** A known issue with CLAP plug-ins that report latency
  (clap-juce-extensions #184): use the VST3 in Bitwig.

## Uninstall

```
rm -rf ~/Library/Audio/Plug-Ins/VST3/Wubansia.vst3 ~/Library/Audio/Plug-Ins/Components/Wubansia.component \
       ~/Library/Audio/Plug-Ins/CLAP/Wubansia.clap /Applications/Wubansia.app \
       "$HOME/Library/Application Support/Wubansia Audio"
killall -9 AudioComponentRegistrar
```
