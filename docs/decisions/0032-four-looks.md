# 0032. Four looks, each switching with its kit

**Status:** Accepted, 2026-11-18, in under ADR 0028 (build in, cut later). Built with these choices, which you can cut
or change: any look's colours, type or words; the subtitle under the wordmark (the kit's direction); the mode row's
"Recorded" label; the hidden A/B switches a kit doesn't have; the drag strip's order (Loop, Song, Part, then Drum kit,
1 drum track and For my synths at the right end); machine room's hazard stripe and scanlines; deep glass's water lines.

## Context
The request was for a UI that is "cooler, more genre specific": "all four genre looks in the plugin, each switching
with its kit". v0.1 had one look, lit hardware with an accent per kit (ADR 0017). The v0.3 design round drew four
boards (night bus for FUT01, mission HUD for FUT02, deep glass for FUT03, machine room for FUT04) in HTML; they assumed
controls Wubansia doesn't have (Heat, Swing, Warp, Width) and displays fed by nothing. The other v0.3 features add
controls every look must hold: Phrases (ADR 0031), Part (ADR 0029), Drum kit (ADR 0030) and For my synths (ADR 0027).

## Decision
1. **Skins are data, painted by shared code.** A `Skin` (`plugin/source/editor/skin/Skin.h`) holds a palette, 32 text
   styles, box shapes, the look's words and its kinds of display, lane, FX row and texture. `skin::paint` draws boxes,
   knobs, faders, meters and drawers from it. The editor swaps the skin on the message thread, on the tick after the
   kit changes; nothing on the audio thread knows about looks.
2. **The regions are fixed.** Every control sits in the same place in every look (the header, display, transport,
   mode row, sections, ten lanes, A/B, FX and the drag strip), so the pointer and the keyboard find the same things.
3. **Every light is real.** Night bus's display is the kick, snare and bass lanes' step grid from the kit's embedded
   MIDI with the playhead; mission HUD's orbit is the lead's notes of the playing bar round the chord; deep glass's
   waves are the drums' and the music's levels; machine room's scope is the plugin's output after the master gain,
   triggered on its 150 Hz low band, and its lanes show the bar's 16 steps. The header names the chord at the playhead.
4. **No new parameter.** The FX row keeps the five real controls (Space, Echo, Pump, Club, Output). Heat, Swing, Warp
   and Width from the boards are left out: no control that does nothing.
5. **The fonts are built in:** 13 faces of seven families (Archivo, JetBrains Mono, Chakra Petch, IBM Plex Mono renamed
   "Wubansia HUD Mono" for its Reserved Font Name, Manrope, Barlow Condensed, Space Mono), SIL OFL 1.1, subset and
   renamed by `tools/fonts/subset_fonts.py` (fontTools 4.55.3, MIT, run by hand only), 326,016 bytes in all. Fallback
   to platform fonts only for strings the person or the run time supplies (paths, file names).
6. **Text fits or says so.** Each style shrinks in half-pixel steps to its floor, then cuts with "…"; a test reads
   every label in every look.
7. **Contrast** passes WCAG AA for text; the boards' colours that didn't are changed (machine room's lane numbers,
   held cells and control edges; night bus's control edge and fader track).
8. **Keys and names** stay as before: the same focus order, a focus ring drawn by the look, the same accessible titles.
9. **Repaints** cover only what changed in a tick; the backdrop (wordmark, texture, panels) is cached.
10. **New controls** take a role (`headerButton`, `stripChip`, ...) and get every look's styles for free.
11. **Lit hardware is retired**, with the one-look accent of ADR 0017.

## The four looks
| Look (kit) | Lead colours | Type | Display | Lanes |
|---|---|---|---|---|
| Night bus (FUT01) | pink #ff4fa3, cyan #3ee6ff | Archivo, JetBrains Mono | step grid | fader and 18-segment meter |
| Mission HUD (FUT02) | cyan #5ee7ff, gold #ffd166 | Chakra Petch, Wubansia HUD Mono, caps | orbit | telemetry column, CH n |
| Deep glass (FUT03) | aqua #7fe8d8, coral #ff9e7a | Manrope, lower case | level waves | glass pill |
| Machine room (FUT04) | amber #ffb000, molten #ff4a1c | Barlow Condensed, Space Mono, caps | output scope | 4 x 4 steps, horizontal slide |

Screenshots: the `look screenshots` test case writes `FUT01-look.png` to `FUT04-look.png` (and `@2x`) into
`$WUBANSIA_SCREENSHOTS`.

## Consequences
- The plugin grows by about 330 KB of fonts.
- While machine room shows, the audio thread copies each block's output into a lock-free FIFO (no allocation, no
  lock); otherwise it doesn't.
- The fit tests no longer depend on the platform's fonts; the DejaVu rerun still covers the drag strip, every label,
  the logo and the header.
- macOS and Windows draw the same faces with their own rasterisers: shapes match, pixels may differ.
- Not yet looked at in a DAW, at Retina scale, at host zoom or on Windows.

## Alternatives
- **The WebView now:** the boards are HTML, but S1f hasn't happened and would bring WKWebView and Node into this round
  (ADR 0017). The `Controller` keeps that road open.
- **One look with accents** (v0.1): what the request asked to change.
- **A class per look:** more code for what is mostly data.
- **Heat, Swing, Warp and Width as parameters:** four permanent IDs with DSP behind them; Swing could come later as its
  own feature.
- **Platform fonts:** widths differ per machine.
- **IBM Plex Mono unmodified:** 275 KB, and its Reserved Font Name forbids a subset under its name.
- **The selected kit first, as the boards order the kit pills:** moves controls under the pointer.
- **A real blur for deep glass:** costly to repaint; the glass is drawn with flat tints.
