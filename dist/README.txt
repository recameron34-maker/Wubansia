Wubansia content: the kits' samples
===================================

This holds the samples of one or more Wubansia kits, 48 kHz / 24-bit WAV, in a folder per kit and content
version: <kit>/v<N>/, for example fut01/v1/. The plugin plays them; you don't need to open them.


Install them (macOS)
--------------------
The quick way, with the plugin download:

1. Unzip the plugin download (Wubansia-macOS-....zip). You get a folder called Wubansia.
2. Put this zip, or this "Wubansia Content" folder, in the same folder as that Wubansia folder (for
   example both in Downloads).
3. Open Terminal, type "bash " (with the space), drag install-macos.sh from the Wubansia folder onto the
   Terminal window, and press Return.

install-macos.sh checks every file against SHA256SUMS and puts each kit in

    ~/Library/Application Support/Wubansia Audio/Wubansia/Content/<kit>/v<N>/

It never replaces a file that differs from this one; it stops and tells you which. Add --dry-run to see
what it would do first: bash install-macos.sh --dry-run

By hand: in Finder, choose Go > Go to Folder, paste

    ~/Library/Application Support/Wubansia Audio/Wubansia/Content

(make the folders if they aren't there). In it, make a folder for each kit (fut01, fut02, ...) if there isn't
one, and move only the kit's version folder (v1) from "Wubansia Content/fut01" into it, so you have for example
.../Content/fut01/v1/Drums/. If a v1 folder is already there, don't click Replace: that's another build of the
same version. Move the old one aside first, or use install-macos.sh, which checks. (INSTALL.md, in the plugin
download, also has the same steps as Terminal commands that stop at the first problem.)

Or leave "Wubansia Content" wherever you like and point the plugin at it: "Choose content folder…" in the
plugin's window.

On Windows or Linux, from a clone of the Wubansia repo:
    python3 tools/content/install.py --from-zip <this zip>


Check the files
---------------
In Terminal, in a kit's version folder:

    cd "Wubansia Content/fut01/v1"
    shasum -a 256 -c SHA256SUMS

Every line should end in OK.


What's in a kit's folder
------------------------
Drums/ Bass/ Music/ Vox/ FX/   the multisamples and one-shots the plugin plays
Loops/ Stems/                  the recorded lines the plugin's phrase lanes slice
SHA256SUMS                     every file's SHA-256

The sounds are original, made for Wubansia by its synthesis scripts. A later content version goes in its own
v<N> folder next to this one, so both can stay installed.
