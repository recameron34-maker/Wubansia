# Third-party notices

Wubansia includes the third-party software below. This file gives each one's license and the notice text that license asks for. It is not legal advice.

> **Re-check on macOS and Windows before M0 is done.** This list was built from `libs/JUCE/JUCE.spdx.json` and the license files in the pinned submodules, then checked against the Linux build on 2026-10-29. `ninja -C build/release -t deps` lists every file each object compiled, including the ones JUCE's unity files `#include`. A grep of those files' headers found the code with its own license that the SBOM leaves out (FreeType, stb, json.h, HarfBuzz's `hb-ucd.cc`, Unicode data), and `nm` on the binaries shows which formats keep it. `compile_commands.json` isn't enough: it lists only the unity files. Repeat this on each platform, and after any JUCE bump or change to the JUCE config flags.

## What ships, and why

The plugin links `juce_audio_utils`, `juce_audio_processors` and `juce_gui_basics`. With their dependencies that is `juce_audio_processors_headless`, `juce_audio_formats`, `juce_audio_devices`, `juce_audio_basics`, `juce_gui_extra`, `juce_graphics`, `juce_data_structures`, `juce_events` and `juce_core`, plus `juce_audio_plugin_client` for the format wrappers (dependencies from each module header and `JUCE.spdx.json`).

M0 builds with `JUCE_USE_OGGVORBIS=0`, `JUCE_USE_OPUS=0`, `JUCE_USE_WEBP=0`, `JUCE_WEB_BROWSER=0` and `JUCE_USE_CURL=0` (`plugin/CMakeLists.txt`). `JUCE_ASIO` and every `JUCE_PLUGINHOST_*` flag keep their default of 0. For each module, the table lists the third-party packages `JUCE.spdx.json` puts in it, minus the ones these flags turn off. It also lists code inside those packages that has its own license in its file headers.

| Component | Version | License | In | Source in this repo |
|---|---|---|---|---|
| JUCE | 9.0.4 | JUCE 9 EULA, Starter tier (the AGPLv3 option is not used) | all formats | `libs/JUCE` |
| zlib | 1.3.2 | Zlib | all formats | `libs/JUCE/modules/juce_core/zip/zlib` |
| HarfBuzz | 14.2.1 | MIT-Modern-Variant | all formats | `libs/JUCE/modules/juce_graphics/fonts/harfbuzz` |
| HarfBuzz `hb-ucd.cc` (Unicode functions) | in HarfBuzz 14.2.1 | ISC | all formats | `libs/JUCE/modules/juce_graphics/fonts/harfbuzz/hb-ucd.cc` |
| SheenBidi | 2.9.0 | Apache-2.0 | all formats | `libs/JUCE/modules/juce_graphics/unicode/sheenbidi` |
| Unicode Character Database (tables generated from it) | 17.0.0 in HarfBuzz | Unicode-3.0 | all formats | Generated tables in HarfBuzz (for example `hb-ucd-table.hh`), SheenBidi (`Source/*Lookup.c`) and JUCE (`juce_graphics/unicode/juce_UnicodeGenerated.cpp`) |
| LunaSVG | 3.5.0 | MIT | all formats | `libs/JUCE/modules/juce_graphics/drawables/lunasvg` |
| PlutoVG | 1.3.2 | MIT | all formats | `libs/JUCE/modules/juce_graphics/drawables/lunasvg/plutovg` |
| FreeType (portions, inside PlutoVG) | unversioned | FTL | all formats | `libs/JUCE/modules/juce_graphics/drawables/lunasvg/plutovg/source/plutovg-ft-*` |
| stb_truetype, stb_image, stb_image_write (inside PlutoVG) | 1.26, 2.30, 1.16 | MIT or public domain | all formats | `libs/JUCE/modules/juce_graphics/drawables/lunasvg/plutovg/source/plutovg-stb-*.h` |
| libpng | 1.6.58 | libpng-2.0 | all formats | `libs/JUCE/modules/juce_graphics/image_formats/pnglib` |
| jpeglib | 10.0 | IJG | all formats | `libs/JUCE/modules/juce_graphics/image_formats/jpglib` |
| FLAC | 1.5.0 | BSD-3-Clause | all formats | `libs/JUCE/modules/juce_audio_formats/codecs/flac` |
| VST3 SDK | 3.8.0 | MIT | VST3 | `libs/JUCE/modules/juce_audio_processors_headless/format_types/VST3_SDK` |
| json.h (sheredom, inside the VST3 SDK) | unversioned | Unlicense (public domain) | VST3 | `libs/JUCE/modules/juce_audio_processors_headless/format_types/VST3_SDK/public.sdk/source/vst/moduleinfo/json.h` |
| PreSonus Plug-In Extensions | unversioned | public domain | VST3 | `libs/JUCE/modules/juce_audio_processors_headless/format_types/pslextensions` |
| AudioUnitSDK | 1.1.0 | Apache-2.0 | AU (macOS) | `libs/JUCE/modules/juce_audio_plugin_client/AU/AudioUnitSDK` |
| clap-juce-extensions | `7adee3a` | MIT | CLAP (see below) | `libs/clap-juce-extensions` |
| CLAP | 1.2.7 (`29ffcc27`) | MIT | Headers only: CLAP (see below) | `libs/clap-juce-extensions/clap-libs/clap` |
| clap-helpers | `a61bcdf0` | MIT | Headers only: CLAP (see below) | `libs/clap-juce-extensions/clap-libs/clap-helpers` |
| Archivo font (the looks, ADR 0032): Regular 400, ExtraBold 800 (instanced at wdth 100), subset | 2.001 | SIL OFL 1.1 | all formats | `plugin/resources/fonts/Archivo-*.ttf` |
| JetBrains Mono font (the looks, ADR 0032): Regular 400 (instanced), subset | 2.211 | SIL OFL 1.1 | all formats | `plugin/resources/fonts/JetBrainsMono-Regular.ttf` |
| Chakra Petch font (the looks, ADR 0032): Regular 400, Bold 700, subset | 1.000 | SIL OFL 1.1 | all formats | `plugin/resources/fonts/ChakraPetch-*.ttf` |
| IBM Plex Mono font (the looks, ADR 0032): Regular 400, renamed "Wubansia HUD Mono", subset | 2.3 | SIL OFL 1.1 | all formats | `plugin/resources/fonts/WubansiaHudMono-Regular.ttf` |
| Manrope font (the looks, ADR 0032): Regular 400, SemiBold 600, ExtraBold 800 (instanced), subset | 4.505 | SIL OFL 1.1 | all formats | `plugin/resources/fonts/Manrope-*.ttf` |
| Barlow Condensed font (the looks, ADR 0032): SemiBold 600, Bold 700, ExtraBold 800, subset | 1.408 | SIL OFL 1.1 | all formats | `plugin/resources/fonts/BarlowCondensed-*.ttf` |
| Space Mono font (the looks, ADR 0032): Regular 400, subset | 1.003 | SIL OFL 1.1 | all formats | `plugin/resources/fonts/SpaceMono-Regular.ttf` |
| Pamplejuce (template) | `ca3cae0` | MIT | Our build files, plugin sources, tests and CI workflow started from it | `docs/third_party/pamplejuce-LICENSE` |

"All formats" means VST3, AU, CLAP and Standalone: JUCE's module code is compiled once into the shared code that every format links.

clap-juce-extensions also links its small `clap_juce_extensions` library into the shared code (`libs/clap-juce-extensions/cmake/ClapTargetHelpers.cmake:156`). In the Linux build only the CLAP keeps any of it: `nm` finds its symbols in the CLAP and none in the VST3 or Standalone, and no shared-code object includes a CLAP header (`ninja -t deps`). macOS and Windows may differ, so treat these three as possibly in every format.

The seven font families are built into the plugin as binary data (`WubansiaFonts`) and drawn by the editor's four
looks. Each is a subset (the UI's characters, no hinting) with a "Wubansia " prefix on its family name, made by
`tools/fonts/subset_fonts.py` from the Google Fonts files `tools/fonts/fonts.lock.json` lists by URL, upstream commit
and SHA-256. A subset is a Modified Version under the OFL; IBM Plex Mono carries the Reserved Font Name "Plex", so its
subset is renamed "Wubansia HUD Mono" and no name record of it says "Plex". The licence texts are below and, as the
upstream files, in `docs/third_party/fonts/`.

## Not shipped

These are used to build or test Wubansia, but nothing from them is linked into a plugin or the Standalone app.

| Component | Version | License | Use |
|---|---|---|---|
| Catch2 | v3.8.1 (`2b60af89`) | BSL-1.0 | Linked into the test binary `WubansiaPluginTests` only (`libs/Catch2`). |
| pluginval | v1.0.4 | GPL-3.0 | Downloaded and run by CI and `tools/ci/validate.sh`. |
| clap-validator | 0.4.1 | MIT | Downloaded and run by CI and `tools/ci/validate.sh`. |
| fontTools | 4.55.3 | MIT | Run by hand by `tools/fonts/subset_fonts.py` to make the embedded font subsets; never in CI or the build. |
| `actions/checkout` | v7 | MIT | GitHub Actions workflow. |
| `actions/cache` | v5 | MIT | GitHub Actions workflow. |
| `actions/upload-artifact` | v7 | MIT | GitHub Actions workflow. |
| `mozilla-actions/sccache-action` | v0.0.11 | Apache-2.0 | GitHub Actions workflow; it downloads sccache (Apache-2.0). |
| `ilammy/msvc-dev-cmd` | v1 | MIT | GitHub Actions workflow. |

Also not shipped:
- JUCE's build tools (`juceaide` and the VST3 manifest helper), which run during the build.
- Packages `JUCE.spdx.json` lists that M0 doesn't compile: libwebp, libogg, libvorbis, Opus, opusfile and libopusenc (turned off); the ASIO SDK (off) and the AAX SDK (no AAX format); LV2, lilv, serd, sord and sratom (no LV2 format or hosting); Oboe (Android only); Box2D, CHOC, QuickJS, Chromium `cubic_bezier` and the OpenGL registry headers (modules not linked); `@juce-framework/webview` (arrives with the WebView in M4); and the packages under JUCE's `examples`, `extras` and build system.
- The Karla test font in `juce_graphics/fonts/juce_TypefaceTestData.cpp` (SIL OFL 1.1), which compiles only with `JUCE_UNIT_TESTS`.

When any of these is turned on (for example the WebView in M4), add its notice here.

Not covered by this file: compiler runtimes and platform SDKs, which come with the toolchains (the Windows build links the MSVC runtime statically), and the system libraries the Linux build links dynamically (ALSA, FreeType, fontconfig, X11). Linux binaries are not distributed. Review both before the M7 release.

## License texts

Each text below is copied verbatim from the file named above it, except where a note says which lines were left out.

### JUCE

Copyright (c) Raw Material Software Limited. JUCE is dual-licensed under the AGPLv3 and the commercial JUCE license (`libs/JUCE/LICENSE.md`). Wubansia uses it under the JUCE 9 End User Licence Agreement, Starter tier: https://juce.com/legal/juce-9-licence/. The EULA's terms were not checked against juce.com for this file (see `docs/decisions/0001-framework.md`).

### zlib 1.3.2

From `libs/JUCE/modules/juce_core/zip/zlib/LICENSE`:

```text
Copyright notice:

 (C) 1995-2026 Jean-loup Gailly and Mark Adler

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.

  Jean-loup Gailly        Mark Adler
  jloup@gzip.org          madler@alumni.caltech.edu
```

### HarfBuzz 14.2.1

From `libs/JUCE/modules/juce_graphics/fonts/harfbuzz/COPYING`:

```text
HarfBuzz is licensed under the so-called "Old MIT" license.  Details follow.
For parts of HarfBuzz that are licensed under different licenses see individual
files names COPYING in subdirectories where applicable.

Copyright © 2010-2022  Google, Inc.
Copyright © 2015-2020  Ebrahim Byagowi
Copyright © 2019,2020  Facebook, Inc.
Copyright © 2012,2015  Mozilla Foundation
Copyright © 2011  Codethink Limited
Copyright © 2008,2010  Nokia Corporation and/or its subsidiary(-ies)
Copyright © 2009  Keith Stribley
Copyright © 2011  Martin Hosken and SIL International
Copyright © 2007  Chris Wilson
Copyright © 2005,2006,2020,2021,2022,2023  Behdad Esfahbod
Copyright © 2004,2007,2008,2009,2010,2013,2021,2022,2023  Red Hat, Inc.
Copyright © 1998-2005  David Turner and Werner Lemberg
Copyright © 2016  Igalia S.L.
Copyright © 2022  Matthias Clasen
Copyright © 2018,2021  Khaled Hosny
Copyright © 2018,2019,2020  Adobe, Inc
Copyright © 2013-2015  Alexei Podtelezhnikov

For full copyright notices consult the individual files in the package.


Permission is hereby granted, without written agreement and without
license or royalty fees, to use, copy, modify, and distribute this
software and its documentation for any purpose, provided that the
above copyright notice and the following two paragraphs appear in
all copies of this software.

IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE TO ANY PARTY FOR
DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES
ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN
IF THE COPYRIGHT HOLDER HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH
DAMAGE.

THE COPYRIGHT HOLDER SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING,
BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
FITNESS FOR A PARTICULAR PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS
ON AN "AS IS" BASIS, AND THE COPYRIGHT HOLDER HAS NO OBLIGATION TO
PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
```

### HarfBuzz `hb-ucd.cc`

This one HarfBuzz file is under the ISC license rather than HarfBuzz's own (`hb-ucd.cc`, lines 2-14):

```
Copyright (C) 2012 Grigori Goronzy <greg@kinoho.net>

Permission to use, copy, modify, and/or distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
```

### SheenBidi 2.9.0

Copyright (C) 2014-2025 Muhammad Tayyab Akram (the widest year range in its source headers). Licensed under the Apache License, Version 2.0; the full text is at the end of this file (`libs/JUCE/modules/juce_graphics/unicode/sheenbidi/LICENSE`). The vendored copy has no NOTICE file. JUCE changed its include paths, as recorded in `JUCE_UPSTREAM.txt` next to it.

### Unicode Character Database

HarfBuzz builds its character tables from Unicode Character Database 17.0.0 files, and the table headers quote those files' notice: "© 2025 Unicode®, Inc. [...] For terms of use and license, see https://www.unicode.org/terms_of_use.html" (`libs/JUCE/modules/juce_graphics/fonts/harfbuzz/hb-unicode-emoji-table.hh:11-13`; the data files are named in `hb-ucd-table.hh` and `hb-ot-shaper-use-table.hh`). SheenBidi's lookup tables ("Automatically generated by SheenBidiGenerator tool") and JUCE's `juce_UnicodeGenerated.cpp` are generated too, but their headers don't name their data. Whether generated tables need this notice is unclear, so it is included.

The Unicode License v3 below is from `LICENSE` in unicode-org/unicodetools at `578f6c05` (2026-10-18). Its copyright line (the tools' own, "Copyright © 2001-2024 Unicode, Inc.") is replaced with the data files' line, and its SPDX line is left out. https://www.unicode.org/license.txt couldn't be reached from this session to compare.

```text
UNICODE LICENSE V3

COPYRIGHT AND PERMISSION NOTICE

© 2025 Unicode®, Inc.

NOTICE TO USER: Carefully read the following legal agreement. BY
DOWNLOADING, INSTALLING, COPYING OR OTHERWISE USING DATA FILES, AND/OR
SOFTWARE, YOU UNEQUIVOCALLY ACCEPT, AND AGREE TO BE BOUND BY, ALL OF THE
TERMS AND CONDITIONS OF THIS AGREEMENT. IF YOU DO NOT AGREE, DO NOT
DOWNLOAD, INSTALL, COPY, DISTRIBUTE OR USE THE DATA FILES OR SOFTWARE.

Permission is hereby granted, free of charge, to any person obtaining a
copy of data files and any associated documentation (the "Data Files") or
software and any associated documentation (the "Software") to deal in the
Data Files or Software without restriction, including without limitation
the rights to use, copy, modify, merge, publish, distribute, and/or sell
copies of the Data Files or Software, and to permit persons to whom the
Data Files or Software are furnished to do so, provided that either (a)
this copyright and permission notice appear with all copies of the Data
Files or Software, or (b) this copyright and permission notice appear in
associated Documentation.

THE DATA FILES AND SOFTWARE ARE PROVIDED "AS IS", WITHOUT WARRANTY OF ANY
KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF
THIRD PARTY RIGHTS.

IN NO EVENT SHALL THE COPYRIGHT HOLDER OR HOLDERS INCLUDED IN THIS NOTICE
BE LIABLE FOR ANY CLAIM, OR ANY SPECIAL INDIRECT OR CONSEQUENTIAL DAMAGES,
OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THE DATA
FILES OR SOFTWARE.

Except as contained in this notice, the name of a copyright holder shall
not be used in advertising or otherwise to promote the sale, use or other
dealings in these Data Files or Software without prior written
authorization of the copyright holder.
```

### LunaSVG 3.5.0

From `libs/JUCE/modules/juce_graphics/drawables/lunasvg/LICENSE`:

```text
MIT License

Copyright (c) 2020-2025 Samuel Ugochukwu <sammycageagle@gmail.com>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

### PlutoVG 1.3.2

From `libs/JUCE/modules/juce_graphics/drawables/lunasvg/plutovg/LICENSE`:

```text
MIT License

Copyright (c) 2020-2025 Samuel Ugochukwu <sammycageagle@gmail.com>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

### FreeType (portions, inside PlutoVG)

**Portions of this software are copyright © 1996-2020 The FreeType Project (www.freetype.org). All rights reserved.** This software is based in part on the work of the FreeType Team.

PlutoVG's `plutovg-ft-math`, `plutovg-ft-raster`, `plutovg-ft-stroker` and `plutovg-ft-types` files come from FreeType (`fttrigon`, `ftgrays`, `ftimage`, `ftstroke`, `fttypes`). Their headers say they "may only be used, modified, and distributed under the terms of the FreeType project license, FTL.TXT". `juce_graphics_lunasvg.c:61-63` compiles the three `.c` files. The FTL asks a binary distribution to credit the FreeType Team in its documentation, which the lines above do; the years span the copyright lines of those seven files. In the Linux build, `nm` finds 31 `PVG_FT_*` functions in each Debug binary, and the Release link (LTO) drops them. macOS and Windows Release aren't checked, so this is listed for all formats.

From `libs/JUCE/modules/juce_graphics/drawables/lunasvg/plutovg/source/FTL.TXT`. The `�` in the suggested credit is in that file (presumably a © lost in an encoding conversion):

```text
                    The FreeType Project LICENSE
                    ----------------------------

                            2006-Jan-27

                    Copyright 1996-2002, 2006 by
          David Turner, Robert Wilhelm, and Werner Lemberg



Introduction
============

  The FreeType  Project is distributed in  several archive packages;
  some of them may contain, in addition to the FreeType font engine,
  various tools and  contributions which rely on, or  relate to, the
  FreeType Project.

  This  license applies  to all  files found  in such  packages, and
  which do not  fall under their own explicit  license.  The license
  affects  thus  the  FreeType   font  engine,  the  test  programs,
  documentation and makefiles, at the very least.

  This  license   was  inspired  by  the  BSD,   Artistic,  and  IJG
  (Independent JPEG  Group) licenses, which  all encourage inclusion
  and  use of  free  software in  commercial  and freeware  products
  alike.  As a consequence, its main points are that:

    o We don't promise that this software works. However, we will be
      interested in any kind of bug reports. (`as is' distribution)

    o You can  use this software for whatever you  want, in parts or
      full form, without having to pay us. (`royalty-free' usage)

    o You may not pretend that  you wrote this software.  If you use
      it, or  only parts of it,  in a program,  you must acknowledge
      somewhere  in  your  documentation  that  you  have  used  the
      FreeType code. (`credits')

  We  specifically  permit  and  encourage  the  inclusion  of  this
  software, with  or without modifications,  in commercial products.
  We  disclaim  all warranties  covering  The  FreeType Project  and
  assume no liability related to The FreeType Project.


  Finally,  many  people  asked  us  for  a  preferred  form  for  a
  credit/disclaimer to use in compliance with this license.  We thus
  encourage you to use the following text:

   """
    Portions of this software are copyright � <year> The FreeType
    Project (www.freetype.org).  All rights reserved.
   """

  Please replace <year> with the value from the FreeType version you
  actually use.


Legal Terms
===========

0. Definitions
--------------

  Throughout this license,  the terms `package', `FreeType Project',
  and  `FreeType  archive' refer  to  the  set  of files  originally
  distributed  by the  authors  (David Turner,  Robert Wilhelm,  and
  Werner Lemberg) as the `FreeType Project', be they named as alpha,
  beta or final release.

  `You' refers to  the licensee, or person using  the project, where
  `using' is a generic term including compiling the project's source
  code as  well as linking it  to form a  `program' or `executable'.
  This  program is  referred to  as  `a program  using the  FreeType
  engine'.

  This  license applies  to all  files distributed  in  the original
  FreeType  Project,   including  all  source   code,  binaries  and
  documentation,  unless  otherwise  stated   in  the  file  in  its
  original, unmodified form as  distributed in the original archive.
  If you are  unsure whether or not a particular  file is covered by
  this license, you must contact us to verify this.

  The FreeType  Project is copyright (C) 1996-2000  by David Turner,
  Robert Wilhelm, and Werner Lemberg.  All rights reserved except as
  specified below.

1. No Warranty
--------------

  THE FREETYPE PROJECT  IS PROVIDED `AS IS' WITHOUT  WARRANTY OF ANY
  KIND, EITHER  EXPRESS OR IMPLIED,  INCLUDING, BUT NOT  LIMITED TO,
  WARRANTIES  OF  MERCHANTABILITY   AND  FITNESS  FOR  A  PARTICULAR
  PURPOSE.  IN NO EVENT WILL ANY OF THE AUTHORS OR COPYRIGHT HOLDERS
  BE LIABLE  FOR ANY DAMAGES CAUSED  BY THE USE OR  THE INABILITY TO
  USE, OF THE FREETYPE PROJECT.

2. Redistribution
-----------------

  This  license  grants  a  worldwide, royalty-free,  perpetual  and
  irrevocable right  and license to use,  execute, perform, compile,
  display,  copy,   create  derivative  works   of,  distribute  and
  sublicense the  FreeType Project (in  both source and  object code
  forms)  and  derivative works  thereof  for  any  purpose; and  to
  authorize others  to exercise  some or all  of the  rights granted
  herein, subject to the following conditions:

    o Redistribution of  source code  must retain this  license file
      (`FTL.TXT') unaltered; any  additions, deletions or changes to
      the original  files must be clearly  indicated in accompanying
      documentation.   The  copyright   notices  of  the  unaltered,
      original  files must  be  preserved in  all  copies of  source
      files.

    o Redistribution in binary form must provide a  disclaimer  that
      states  that  the software is based in part of the work of the
      FreeType Team,  in  the  distribution  documentation.  We also
      encourage you to put an URL to the FreeType web page  in  your
      documentation, though this isn't mandatory.

  These conditions  apply to any  software derived from or  based on
  the FreeType Project,  not just the unmodified files.   If you use
  our work, you  must acknowledge us.  However, no  fee need be paid
  to us.

3. Advertising
--------------

  Neither the  FreeType authors and  contributors nor you  shall use
  the name of the  other for commercial, advertising, or promotional
  purposes without specific prior written permission.

  We suggest,  but do not require, that  you use one or  more of the
  following phrases to refer  to this software in your documentation
  or advertising  materials: `FreeType Project',  `FreeType Engine',
  `FreeType library', or `FreeType Distribution'.

  As  you have  not signed  this license,  you are  not  required to
  accept  it.   However,  as  the FreeType  Project  is  copyrighted
  material, only  this license, or  another one contracted  with the
  authors, grants you  the right to use, distribute,  and modify it.
  Therefore,  by  using,  distributing,  or modifying  the  FreeType
  Project, you indicate that you understand and accept all the terms
  of this license.

4. Contacts
-----------

  There are two mailing lists related to FreeType:

    o freetype@nongnu.org

      Discusses general use and applications of FreeType, as well as
      future and  wanted additions to the  library and distribution.
      If  you are looking  for support,  start in  this list  if you
      haven't found anything to help you in the documentation.

    o freetype-devel@nongnu.org

      Discusses bugs,  as well  as engine internals,  design issues,
      specific licenses, porting, etc.

  Our home page can be found at

    http://www.freetype.org
```

### stb_truetype 1.26, stb_image 2.30 and stb_image_write 1.16 (inside PlutoVG)

By Sean Barrett; PlutoVG includes them in `plutovg-font.c` and `plutovg-surface.c`. Each file offers the MIT License or the public domain, and the text below covers either. From the end of `libs/JUCE/modules/juce_graphics/drawables/lunasvg/plutovg/source/plutovg-stb-truetype.h` (lines 5037-5077; `plutovg-stb-image.h` and `plutovg-stb-image-write.h` end with the same text):

```text
/*
------------------------------------------------------------------------------
This software is available under 2 licenses -- choose whichever you prefer.
------------------------------------------------------------------------------
ALTERNATIVE A - MIT License
Copyright (c) 2017 Sean Barrett
Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
------------------------------------------------------------------------------
ALTERNATIVE B - Public Domain (www.unlicense.org)
This is free and unencumbered software released into the public domain.
Anyone is free to copy, modify, publish, use, compile, sell, or distribute this
software, either in source code form or as a compiled binary, for any purpose,
commercial or non-commercial, and by any means.
In jurisdictions that recognize copyright laws, the author or authors of this
software dedicate any and all copyright interest in the software to the public
domain. We make this dedication for the benefit of the public at large and to
the detriment of our heirs and successors. We intend this dedication to be an
overt act of relinquishment in perpetuity of all present and future rights to
this software under copyright law.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
------------------------------------------------------------------------------
*/
```

### libpng 1.6.58

From `libs/JUCE/modules/juce_graphics/image_formats/pnglib/LICENSE`:

```text
COPYRIGHT NOTICE, DISCLAIMER, and LICENSE
=========================================

PNG Reference Library License version 2
---------------------------------------

 * Copyright (c) 1995-2026 The PNG Reference Library Authors.
 * Copyright (c) 2018-2026 Cosmin Truta.
 * Copyright (c) 2000-2002, 2004, 2006-2018 Glenn Randers-Pehrson.
 * Copyright (c) 1996-1997 Andreas Dilger.
 * Copyright (c) 1995-1996 Guy Eric Schalnat, Group 42, Inc.

The software is supplied "as is", without warranty of any kind,
express or implied, including, without limitation, the warranties
of merchantability, fitness for a particular purpose, title, and
non-infringement.  In no event shall the Copyright owners, or
anyone distributing the software, be liable for any damages or
other liability, whether in contract, tort or otherwise, arising
from, out of, or in connection with the software, or the use or
other dealings in the software, even if advised of the possibility
of such damage.

Permission is hereby granted to use, copy, modify, and distribute
this software, or portions hereof, for any purpose, without fee,
subject to the following restrictions:

 1. The origin of this software must not be misrepresented; you
    must not claim that you wrote the original software.  If you
    use this software in a product, an acknowledgment in the product
    documentation would be appreciated, but is not required.

 2. Altered source versions must be plainly marked as such, and must
    not be misrepresented as being the original software.

 3. This Copyright notice may not be removed or altered from any
    source or altered source distribution.


PNG Reference Library License version 1 (for libpng 0.5 through 1.6.35)
-----------------------------------------------------------------------

libpng versions 1.0.7, July 1, 2000, through 1.6.35, July 15, 2018 are
Copyright (c) 2000-2002, 2004, 2006-2018 Glenn Randers-Pehrson, are
derived from libpng-1.0.6, and are distributed according to the same
disclaimer and license as libpng-1.0.6 with the following individuals
added to the list of Contributing Authors:

    Simon-Pierre Cadieux
    Eric S. Raymond
    Mans Rullgard
    Cosmin Truta
    Gilles Vollant
    James Yu
    Mandar Sahastrabuddhe
    Google Inc.
    Vadim Barkov

and with the following additions to the disclaimer:

    There is no warranty against interference with your enjoyment of
    the library or against infringement.  There is no warranty that our
    efforts or the library will fulfill any of your particular purposes
    or needs.  This library is provided with all faults, and the entire
    risk of satisfactory quality, performance, accuracy, and effort is
    with the user.

Some files in the "contrib" directory and some configure-generated
files that are distributed with libpng have other copyright owners, and
are released under other open source licenses.

libpng versions 0.97, January 1998, through 1.0.6, March 20, 2000, are
Copyright (c) 1998-2000 Glenn Randers-Pehrson, are derived from
libpng-0.96, and are distributed according to the same disclaimer and
license as libpng-0.96, with the following individuals added to the
list of Contributing Authors:

    Tom Lane
    Glenn Randers-Pehrson
    Willem van Schaik

libpng versions 0.89, June 1996, through 0.96, May 1997, are
Copyright (c) 1996-1997 Andreas Dilger, are derived from libpng-0.88,
and are distributed according to the same disclaimer and license as
libpng-0.88, with the following individuals added to the list of
Contributing Authors:

    John Bowler
    Kevin Bracey
    Sam Bushell
    Magnus Holmgren
    Greg Roelofs
    Tom Tanner

Some files in the "scripts" directory have other copyright owners,
but are released under this license.

libpng versions 0.5, May 1995, through 0.88, January 1996, are
Copyright (c) 1995-1996 Guy Eric Schalnat, Group 42, Inc.

For the purposes of this copyright and license, "Contributing Authors"
is defined as the following set of individuals:

    Andreas Dilger
    Dave Martindale
    Guy Eric Schalnat
    Paul Schmidt
    Tim Wegner

The PNG Reference Library is supplied "AS IS".  The Contributing
Authors and Group 42, Inc. disclaim all warranties, expressed or
implied, including, without limitation, the warranties of
merchantability and of fitness for any purpose.  The Contributing
Authors and Group 42, Inc. assume no liability for direct, indirect,
incidental, special, exemplary, or consequential damages, which may
result from the use of the PNG Reference Library, even if advised of
the possibility of such damage.

Permission is hereby granted to use, copy, modify, and distribute this
source code, or portions hereof, for any purpose, without fee, subject
to the following restrictions:

 1. The origin of this source code must not be misrepresented.

 2. Altered versions must be plainly marked as such and must not
    be misrepresented as being the original source.

 3. This Copyright notice may not be removed or altered from any
    source or altered source distribution.

The Contributing Authors and Group 42, Inc. specifically permit,
without fee, and encourage the use of this source code as a component
to supporting the PNG file format in commercial products.  If you use
this source code in a product, acknowledgment is not required but would
be appreciated.
```

### jpeglib 10.0 (Independent JPEG Group)

**This software is based in part on the work of the Independent JPEG Group.**

From `libs/JUCE/modules/juce_graphics/image_formats/jpglib/README`, section LEGAL ISSUES, up to the paragraph about the Unix configure scripts, which JUCE doesn't include:

```text
LEGAL ISSUES
============

In plain English:

1. We don't promise that this software works.  (But if you find any bugs,
   please let us know!)
2. You can use this software for whatever you want.  You don't have to pay us.
3. You may not pretend that you wrote this software.  If you use it in a
   program, you must acknowledge somewhere in your documentation that
   you've used the IJG code.

In legalese:

The authors make NO WARRANTY or representation, either express or implied,
with respect to this software, its quality, accuracy, merchantability, or
fitness for a particular purpose.  This software is provided "AS IS", and you,
its user, assume the entire risk as to its quality and accuracy.

This software is copyright (C) 1991-2026, Thomas G. Lane, Guido Vollbeding.
All Rights Reserved except as specified below.

Permission is hereby granted to use, copy, modify, and distribute this
software (or portions thereof) for any purpose, without fee, subject to these
conditions:
(1) If any part of the source code for this software is distributed, then this
README file must be included, with this copyright and no-warranty notice
unaltered; and any additions, deletions, or changes to the original files
must be clearly indicated in accompanying documentation.
(2) If only executable code is distributed, then the accompanying
documentation must state that "this software is based in part on the work of
the Independent JPEG Group".
(3) Permission for use of this software is granted only if the user accepts
full responsibility for any undesirable consequences; the authors accept
NO LIABILITY for damages of any kind.

These conditions apply to any software derived from or based on the IJG code,
not just to the unmodified library.  If you use our work, you ought to
acknowledge us.

Permission is NOT granted for the use of any IJG author's name or company name
in advertising or publicity relating to this software or products derived from
it.  This software may be referred to only as "the Independent JPEG Group's
software".

We specifically permit and encourage the use of this software as the basis of
commercial products, provided that all warranty or liability claims are
assumed by the product vendor.
```

### FLAC 1.5.0

FLAC's `libFLAC/md5.c` (Colin Plumb's MD5 code) is in the public domain, as its header says, and needs no notice.

From `libs/JUCE/modules/juce_audio_formats/codecs/flac/Flac Licence.txt`:

```text
libFLAC - Free Lossless Audio Codec library
Copyright (C) 2000-2009  Josh Coalson
Copyright (C) 2011-2023  Xiph.Org Foundation

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

- Redistributions of source code must retain the above copyright
notice, this list of conditions and the following disclaimer.

- Redistributions in binary form must reproduce the above copyright
notice, this list of conditions and the following disclaimer in the
documentation and/or other materials provided with the distribution.

- Neither the name of the Xiph.Org Foundation nor the names of its
contributors may be used to endorse or promote products derived from
this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
```

### VST3 SDK 3.8.0

VST is a trademark of Steinberg Media Technologies GmbH. The MIT license doesn't require using the name or logo. If we use either (for example in an about box or a manual), we must follow the Steinberg VST Usage Guidelines in `VST3_SDK/VST3_Usage_Guidelines.pdf` (`VST3_SDK/README.md`, "Trademark and Logo Usage").

From `libs/JUCE/modules/juce_audio_processors_headless/format_types/VST3_SDK/LICENSE.txt`:

```text
//-----------------------------------------------------------------------------
MIT License

Copyright (c) 2025, Steinberg Media Technologies GmbH

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

//---------------------------------------------------------------------------------
```

### json.h (sheredom, inside the VST3 SDK)

The VST3 SDK's `moduleinfo` parser uses it, so only the VST3 contains it. Public-domain code needs no notice; it is listed, like the PreSonus headers, so the list is complete. From `libs/JUCE/modules/juce_audio_processors_headless/format_types/VST3_SDK/public.sdk/source/vst/moduleinfo/json.h`, lines 6-31:

```text
/*
   This is free and unencumbered software released into the public domain.

   Anyone is free to copy, modify, publish, use, compile, sell, or
   distribute this software, either in source code form or as a compiled
   binary, for any purpose, commercial or non-commercial, and by any
   means.

   In jurisdictions that recognize copyright laws, the author or authors
   of this software dedicate any and all copyright interest in the
   software to the public domain. We make this dedication for the benefit
   of the public at large and to the detriment of our heirs and
   successors. We intend this dedication to be an overt act of
   relinquishment in perpetuity of all present and future rights to this
   software under copyright law.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
   EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
   IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
   OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
   ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
   OTHER DEALINGS IN THE SOFTWARE.

   For more information, please refer to <http://unlicense.org/>.
*/
```

### PreSonus Plug-In Extensions

From `libs/JUCE/modules/juce_audio_processors_headless/format_types/pslextensions/ipslviewembedding.h`, the header comment without its file name, date and description lines. All eight headers there carry the same statement:

```text
//************************************************************************************************
//
// PreSonus Plug-In Extensions
// Written and placed in the PUBLIC DOMAIN by PreSonus Software Ltd.
//************************************************************************************************
/*
	DISCLAIMER:
	The PreSonus Plug-In Extensions are host-specific extensions of existing proprietary technologies,
	provided to the community on an AS IS basis. They are not part of any official 3rd party SDK and
	PreSonus is not affiliated with the owner of the underlying technology in any way.
*/
//************************************************************************************************
```

### AudioUnitSDK 1.1.0

© 2000-2021 Apple Inc. All rights reserved (from its source headers). Licensed under the Apache License, Version 2.0; the full text is at the end of this file (`libs/JUCE/modules/juce_audio_plugin_client/AU/AudioUnitSDK/LICENSE.txt`). The vendored copy has no NOTICE file. JUCE modified `AUScopeElement.cpp` and `AUBase.h`, as recorded in `JUCE_UPSTREAM.txt` next to it.

### clap-juce-extensions (`7adee3a`)

From `libs/clap-juce-extensions/LICENSE.md`:

```text
Copyright 2019-2020, Paul Walker

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
```

### CLAP 1.2.7

From `libs/clap-juce-extensions/clap-libs/clap/LICENSE`:

```text
MIT License

Copyright (c) 2021 Alexandre BIQUE

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

### clap-helpers (`a61bcdf0`)

From `libs/clap-juce-extensions/clap-libs/clap-helpers/LICENSE`:

```text
MIT License

Copyright (c) 2021 Alexandre BIQUE

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

### Pamplejuce (`ca3cae0`)

From `docs/third_party/pamplejuce-LICENSE`:

```text
MIT License

Copyright (c) 2022 Sudara Williams

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

### SIL Open Font License 1.1 (the looks' fonts)

The seven families' copyright lines, as their OFL.txt files give them:

- Archivo: Copyright 2020 The Archivo Project Authors (https://github.com/Omnibus-Type/Archivo)
- JetBrains Mono: Copyright 2020 The JetBrains Mono Project Authors (https://github.com/JetBrains/JetBrainsMono)
- Chakra Petch: Copyright 2018 The Chakra Petch Project Authors (https://github.com/m4rc1e/Chakra-Petch.git)
- IBM Plex Mono: Copyright © 2017 IBM Corp. with Reserved Font Name "Plex"
- Manrope: Copyright 2018 The Manrope Project Authors (https://github.com/googlefonts/manrope)
- Barlow Condensed: Copyright 2017 The Barlow Project Authors (https://github.com/jpt/barlow)
- Space Mono: Copyright 2016 The Space Mono Project Authors (https://github.com/googlefonts/spacemono)

IBM Plex Mono's Reserved Font Name "Plex" is why its subset is called "Wubansia HUD Mono". The other six declare no
Reserved Font Name. Each family's own file is in `docs/third_party/fonts/`. The licence, once:

```
This Font Software is licensed under the SIL Open Font License, Version 1.1.
This license is copied below, and is also available with a FAQ at:
http://scripts.sil.org/OFL


-----------------------------------------------------------
SIL OPEN FONT LICENSE Version 1.1 - 26 February 2007
-----------------------------------------------------------

PREAMBLE
The goals of the Open Font License (OFL) are to stimulate worldwide
development of collaborative font projects, to support the font creation
efforts of academic and linguistic communities, and to provide a free and
open framework in which fonts may be shared and improved in partnership
with others.

The OFL allows the licensed fonts to be used, studied, modified and
redistributed freely as long as they are not sold by themselves. The
fonts, including any derivative works, can be bundled, embedded, 
redistributed and/or sold with any software provided that any reserved
names are not used by derivative works. The fonts and derivatives,
however, cannot be released under any other type of license. The
requirement for fonts to remain under this license does not apply
to any document created using the fonts or their derivatives.

DEFINITIONS
"Font Software" refers to the set of files released by the Copyright
Holder(s) under this license and clearly marked as such. This may
include source files, build scripts and documentation.

"Reserved Font Name" refers to any names specified as such after the
copyright statement(s).

"Original Version" refers to the collection of Font Software components as
distributed by the Copyright Holder(s).

"Modified Version" refers to any derivative made by adding to, deleting,
or substituting -- in part or in whole -- any of the components of the
Original Version, by changing formats or by porting the Font Software to a
new environment.

"Author" refers to any designer, engineer, programmer, technical
writer or other person who contributed to the Font Software.

PERMISSION & CONDITIONS
Permission is hereby granted, free of charge, to any person obtaining
a copy of the Font Software, to use, study, copy, merge, embed, modify,
redistribute, and sell modified and unmodified copies of the Font
Software, subject to the following conditions:

1) Neither the Font Software nor any of its individual components,
in Original or Modified Versions, may be sold by itself.

2) Original or Modified Versions of the Font Software may be bundled,
redistributed and/or sold with any software, provided that each copy
contains the above copyright notice and this license. These can be
included either as stand-alone text files, human-readable headers or
in the appropriate machine-readable metadata fields within text or
binary files as long as those fields can be easily viewed by the user.

3) No Modified Version of the Font Software may use the Reserved Font
Name(s) unless explicit written permission is granted by the corresponding
Copyright Holder. This restriction only applies to the primary font name as
presented to the users.

4) The name(s) of the Copyright Holder(s) or the Author(s) of the Font
Software shall not be used to promote, endorse or advertise any
Modified Version, except to acknowledge the contribution(s) of the
Copyright Holder(s) and the Author(s) or with their explicit written
permission.

5) The Font Software, modified or unmodified, in part or in whole,
must be distributed entirely under this license, and must not be
distributed under any other license. The requirement for fonts to
remain under this license does not apply to any document created
using the Font Software.

TERMINATION
This license becomes null and void if any of the above conditions are
not met.

DISCLAIMER
THE FONT SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO ANY WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT
OF COPYRIGHT, PATENT, TRADEMARK, OR OTHER RIGHT. IN NO EVENT SHALL THE
COPYRIGHT HOLDER BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
INCLUDING ANY GENERAL, SPECIAL, INDIRECT, INCIDENTAL, OR CONSEQUENTIAL
DAMAGES, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF THE USE OR INABILITY TO USE THE FONT SOFTWARE OR FROM
OTHER DEALINGS IN THE FONT SOFTWARE.
```

### Apache License 2.0

Applies to SheenBidi and AudioUnitSDK. From `libs/JUCE/modules/juce_graphics/unicode/sheenbidi/LICENSE` (identical to `AudioUnitSDK/LICENSE.txt`):

```text

                                 Apache License
                           Version 2.0, January 2004
                        http://www.apache.org/licenses/

   TERMS AND CONDITIONS FOR USE, REPRODUCTION, AND DISTRIBUTION

   1. Definitions.

      "License" shall mean the terms and conditions for use, reproduction,
      and distribution as defined by Sections 1 through 9 of this document.

      "Licensor" shall mean the copyright owner or entity authorized by
      the copyright owner that is granting the License.

      "Legal Entity" shall mean the union of the acting entity and all
      other entities that control, are controlled by, or are under common
      control with that entity. For the purposes of this definition,
      "control" means (i) the power, direct or indirect, to cause the
      direction or management of such entity, whether by contract or
      otherwise, or (ii) ownership of fifty percent (50%) or more of the
      outstanding shares, or (iii) beneficial ownership of such entity.

      "You" (or "Your") shall mean an individual or Legal Entity
      exercising permissions granted by this License.

      "Source" form shall mean the preferred form for making modifications,
      including but not limited to software source code, documentation
      source, and configuration files.

      "Object" form shall mean any form resulting from mechanical
      transformation or translation of a Source form, including but
      not limited to compiled object code, generated documentation,
      and conversions to other media types.

      "Work" shall mean the work of authorship, whether in Source or
      Object form, made available under the License, as indicated by a
      copyright notice that is included in or attached to the work
      (an example is provided in the Appendix below).

      "Derivative Works" shall mean any work, whether in Source or Object
      form, that is based on (or derived from) the Work and for which the
      editorial revisions, annotations, elaborations, or other modifications
      represent, as a whole, an original work of authorship. For the purposes
      of this License, Derivative Works shall not include works that remain
      separable from, or merely link (or bind by name) to the interfaces of,
      the Work and Derivative Works thereof.

      "Contribution" shall mean any work of authorship, including
      the original version of the Work and any modifications or additions
      to that Work or Derivative Works thereof, that is intentionally
      submitted to Licensor for inclusion in the Work by the copyright owner
      or by an individual or Legal Entity authorized to submit on behalf of
      the copyright owner. For the purposes of this definition, "submitted"
      means any form of electronic, verbal, or written communication sent
      to the Licensor or its representatives, including but not limited to
      communication on electronic mailing lists, source code control systems,
      and issue tracking systems that are managed by, or on behalf of, the
      Licensor for the purpose of discussing and improving the Work, but
      excluding communication that is conspicuously marked or otherwise
      designated in writing by the copyright owner as "Not a Contribution."

      "Contributor" shall mean Licensor and any individual or Legal Entity
      on behalf of whom a Contribution has been received by Licensor and
      subsequently incorporated within the Work.

   2. Grant of Copyright License. Subject to the terms and conditions of
      this License, each Contributor hereby grants to You a perpetual,
      worldwide, non-exclusive, no-charge, royalty-free, irrevocable
      copyright license to reproduce, prepare Derivative Works of,
      publicly display, publicly perform, sublicense, and distribute the
      Work and such Derivative Works in Source or Object form.

   3. Grant of Patent License. Subject to the terms and conditions of
      this License, each Contributor hereby grants to You a perpetual,
      worldwide, non-exclusive, no-charge, royalty-free, irrevocable
      (except as stated in this section) patent license to make, have made,
      use, offer to sell, sell, import, and otherwise transfer the Work,
      where such license applies only to those patent claims licensable
      by such Contributor that are necessarily infringed by their
      Contribution(s) alone or by combination of their Contribution(s)
      with the Work to which such Contribution(s) was submitted. If You
      institute patent litigation against any entity (including a
      cross-claim or counterclaim in a lawsuit) alleging that the Work
      or a Contribution incorporated within the Work constitutes direct
      or contributory patent infringement, then any patent licenses
      granted to You under this License for that Work shall terminate
      as of the date such litigation is filed.

   4. Redistribution. You may reproduce and distribute copies of the
      Work or Derivative Works thereof in any medium, with or without
      modifications, and in Source or Object form, provided that You
      meet the following conditions:

      (a) You must give any other recipients of the Work or
          Derivative Works a copy of this License; and

      (b) You must cause any modified files to carry prominent notices
          stating that You changed the files; and

      (c) You must retain, in the Source form of any Derivative Works
          that You distribute, all copyright, patent, trademark, and
          attribution notices from the Source form of the Work,
          excluding those notices that do not pertain to any part of
          the Derivative Works; and

      (d) If the Work includes a "NOTICE" text file as part of its
          distribution, then any Derivative Works that You distribute must
          include a readable copy of the attribution notices contained
          within such NOTICE file, excluding those notices that do not
          pertain to any part of the Derivative Works, in at least one
          of the following places: within a NOTICE text file distributed
          as part of the Derivative Works; within the Source form or
          documentation, if provided along with the Derivative Works; or,
          within a display generated by the Derivative Works, if and
          wherever such third-party notices normally appear. The contents
          of the NOTICE file are for informational purposes only and
          do not modify the License. You may add Your own attribution
          notices within Derivative Works that You distribute, alongside
          or as an addendum to the NOTICE text from the Work, provided
          that such additional attribution notices cannot be construed
          as modifying the License.

      You may add Your own copyright statement to Your modifications and
      may provide additional or different license terms and conditions
      for use, reproduction, or distribution of Your modifications, or
      for any such Derivative Works as a whole, provided Your use,
      reproduction, and distribution of the Work otherwise complies with
      the conditions stated in this License.

   5. Submission of Contributions. Unless You explicitly state otherwise,
      any Contribution intentionally submitted for inclusion in the Work
      by You to the Licensor shall be under the terms and conditions of
      this License, without any additional terms or conditions.
      Notwithstanding the above, nothing herein shall supersede or modify
      the terms of any separate license agreement you may have executed
      with Licensor regarding such Contributions.

   6. Trademarks. This License does not grant permission to use the trade
      names, trademarks, service marks, or product names of the Licensor,
      except as required for reasonable and customary use in describing the
      origin of the Work and reproducing the content of the NOTICE file.

   7. Disclaimer of Warranty. Unless required by applicable law or
      agreed to in writing, Licensor provides the Work (and each
      Contributor provides its Contributions) on an "AS IS" BASIS,
      WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
      implied, including, without limitation, any warranties or conditions
      of TITLE, NON-INFRINGEMENT, MERCHANTABILITY, or FITNESS FOR A
      PARTICULAR PURPOSE. You are solely responsible for determining the
      appropriateness of using or redistributing the Work and assume any
      risks associated with Your exercise of permissions under this License.

   8. Limitation of Liability. In no event and under no legal theory,
      whether in tort (including negligence), contract, or otherwise,
      unless required by applicable law (such as deliberate and grossly
      negligent acts) or agreed to in writing, shall any Contributor be
      liable to You for damages, including any direct, indirect, special,
      incidental, or consequential damages of any character arising as a
      result of this License or out of the use or inability to use the
      Work (including but not limited to damages for loss of goodwill,
      work stoppage, computer failure or malfunction, or any and all
      other commercial damages or losses), even if such Contributor
      has been advised of the possibility of such damages.

   9. Accepting Warranty or Additional Liability. While redistributing
      the Work or Derivative Works thereof, You may choose to offer,
      and charge a fee for, acceptance of support, warranty, indemnity,
      or other liability obligations and/or rights consistent with this
      License. However, in accepting such obligations, You may act only
      on Your own behalf and on Your sole responsibility, not on behalf
      of any other Contributor, and only if You agree to indemnify,
      defend, and hold each Contributor harmless for any liability
      incurred by, or claims asserted against, such Contributor by reason
      of your accepting any such warranty or additional liability.

   END OF TERMS AND CONDITIONS

   APPENDIX: How to apply the Apache License to your work.

      To apply the Apache License to your work, attach the following
      boilerplate notice, with the fields enclosed by brackets "[]"
      replaced with your own identifying information. (Don't include
      the brackets!)  The text should be enclosed in the appropriate
      comment syntax for the file format. We also recommend that a
      file or class name and description of purpose be included on the
      same "printed page" as the copyright notice for easier
      identification within third-party archives.

   Copyright [yyyy] [name of copyright owner]

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
```
