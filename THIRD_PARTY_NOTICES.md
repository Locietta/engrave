# Third-Party Notices and Licenses

This repository bundles third-party fonts and libraries, and builds with
external third-party dependencies managed by xmake. This document inventories
per-font and per-component paths, upstream sources, licenses, embedding
exceptions, and Reserved Font Names.

All entries below have been verified against license files, font name tables,
source headers, and readmes present in the tree. Unverified aspects or external
build dependencies are explicitly marked as such.

---

## 1. Bundled Font Families

| Font Family | In-Tree Path | Upstream Origin | License | Exceptions / Reserved Font Names |
|---|---|---|---|---|
| **Bravura** | `fonts/bravura/` | Steinberg Media Technologies GmbH | SIL Open Font License 1.1 | Reserved Font Name: `Bravura` |
| **Campania** | `fonts/campania/` | Marc Sabatella, SIL International | SIL Open Font License 1.1 | Reserved Font Name: `Campania` |
| **Edwin** | `fonts/edwin/` | MuseScore BVBA / MuseScore Limited | SIL Open Font License 1.1 | Reserved Font Name: `Edwin` |
| **Finale Broadway** | `fonts/finalebroadway/` | MakeMusic, Inc. (Mark F. Adler) | SIL Open Font License 1.1 | Reserved Font Names: `Finale`, `Broadway`, `Maestro`, `Engraver`, `Jazz`, `Ash`, `Legacy` |
| **Finale Maestro** | `fonts/finalemaestro/` | MakeMusic, Inc. | SIL Open Font License 1.1 | Reserved Font Names: `Finale`, `Maestro`, `Broadway`, `Engraver`, `Jazz`, `Ash`, `Legacy` |
| **Gootville** | `fonts/gootville/` | Grzegorz Pruchniakowski (Gootector) | SIL Open Font License 1.1 | Derived from Gonville & Bravura/SMuFL. No RFN. |
| **Leland** | `fonts/leland/` | MuseScore BVBA (Martin Keary & Simon Smith) | SIL Open Font License 1.1 | Reserved Font Name: `Leland` |
| **MScore** | `fonts/mscore/MScore.otf` | Werner Schweer and contributors; derived from Parnassus & LilyPond Emmentaler | GPLv3 or later | Document embedding font exception (LilyPond exception) |
| **MScore Text** | `fonts/mscore/MScoreText.otf` | Werner Schweer and others | SIL Open Font License 1.1 | Reserved Font Name: `MScore Text` |
| **MuseJazz** | `fonts/musejazz/` | Torsten Hämmerle, Abraham Lee / Music Type Foundry | SIL Open Font License 1.1 | Reserved Font Name: `MuseJazz` |
| **Petaluma** | `fonts/petaluma/` | Steinberg Media Technologies GmbH (Anthony Hughes, Daniel Spreadbury) | SIL Open Font License 1.1 | Reserved Font Name: `Petaluma` |
| **GNU FreeFont** | `fonts/FreeSans.ttf`, `fonts/FreeSerif*.ttf` | GNU FreeFont Project | GPLv3 or later | Document embedding font exception (GNU FreeFont exception) |
| **Tablature & Historical** | `fonts/MuseScoreTab*.ttf` | Maurizio M. Gavioli | SIL Open Font License 1.1 | Reserved Font Name: `MScoreTabulature` |
| **MScoreBC** | `fonts/mscore-BC.ttf` | Maurizio M. Gavioli | SIL Open Font License 1.1 | Reserved Font Name: `MScoreBC` |
| **SMuFL Metadata** | `fonts/smufl/` | W3C Music Notation Community Group | MIT / W3C Software Notice | Standard Musical Instrument Font Layout tables |

### Font Details and Notices

#### Bravura (`fonts/bravura/`)
- **Files**: `Bravura.otf`, `BravuraText.otf`, `Bravura.json`
- **Notice**: Copyright (C) 2013-2026 Steinberg Media Technologies GmbH (http://www.steinberg.net/), with Reserved Font Name "Bravura".
- **License**: SIL Open Font License, Version 1.1 (`fonts/bravura/OFL.txt`, `fonts/bravura/LICENSE.txt`).
- **Trademark**: Bravura is a registered trademark of Steinberg Media Technologies GmbH in the European Union and other territories.

#### Campania (`fonts/campania/`)
- **Files**: `Campania.otf`
- **Notice**: Copyright (C) 1994-2019 Marc Sabatella, SIL International, with Reserved Font Name "Campania".
- **License**: SIL Open Font License, Version 1.1 (`fonts/campania/LICENSE`).

#### Edwin (`fonts/edwin/`)
- **Files**: `Edwin-Roman.otf`, `Edwin-Bold.otf`, `Edwin-Italic.otf`, `Edwin-BdIta.otf`
- **Notice**: Copyright (C) 2021-2022 MuseScore BVBA / MuseScore Limited, with Reserved Font Name "Edwin".
- **License**: SIL Open Font License, Version 1.1 (`fonts/edwin/LICENSE.txt`).

#### Finale Broadway (`fonts/finalebroadway/`)
- **Files**: `FinaleBroadway.otf`, `FinaleBroadwayText.otf`, `FinaleBroadway.json`, `Finale Broadway Text.json`
- **Notice**: Copyright (C) 2021 MakeMusic, Inc. (https://www.makemusic.com/), with Reserved Font Names "Finale", "Broadway", "Maestro", "Engraver", "Jazz", "Ash", and "Legacy".
- **License**: SIL Open Font License, Version 1.1 (`fonts/finalebroadway/OFL.txt`).

#### Finale Maestro (`fonts/finalemaestro/`)
- **Files**: `FinaleMaestro.otf`, `FinaleMaestroText-Regular.otf`, `FinaleMaestro.json`, `Finale Maestro Text.json`
- **Notice**: Copyright (C) 2021 MakeMusic, Inc. (https://www.makemusic.com/), with Reserved Font Names "Finale", "Maestro", "Broadway", "Engraver", "Jazz", "Ash", and "Legacy".
- **License**: SIL Open Font License, Version 1.1 (`fonts/finalemaestro/OFL.txt`).

#### Gootville (`fonts/gootville/`)
- **Files**: `Gootville.otf`, `GootvilleText.otf`, `metadata.json`, `readme.txt`
- **Notice**: Designed by Grzegorz Pruchniakowski (Gootector). Templates based on SMuFL 1.18 and Bravura 1.18 by Daniel Spreadbury at Steinberg.
- **License**: SIL Open Font License, Version 1.1 (`fonts/gootville/OFL.txt`, `fonts/gootville/readme.txt`).
- **Reserved Font Names**: None declared.

#### Leland (`fonts/leland/`)
- **Files**: `Leland.otf`, `LelandText.otf`, `leland_metadata.json`
- **Notice**: Copyright (C) 2021-2025 MuseScore BVBA / MuseScore Limited (Martin Keary & Simon Smith), with Reserved Font Name "Leland".
- **License**: SIL Open Font License, Version 1.1 (`fonts/leland/LICENSE.txt`).
- **Trademark**: Leland is a registered trademark of MuseScore BVBA in the European Union and other territories.

#### MScore & MScoreText (`fonts/mscore/`)
- **`MScore.otf`**:
  - **Notice**: Copyright (C) 2013-2014 Werner Schweer, 2014 John Pirie, 2018 Matt McClinch, 2013-2018 Nicolas Froment, 2013-2019 Maurizio M. Gavioli, 2018-2020 mirabilos. Derived in part from Parnassus (Copyright (C) 2005-2013 Kristof Bastiaensen) and Emmentaler/GNU LilyPond (Copyright (C) 1997-2012 Han-Wen Nienhuys, Jan Nieuwenhuizen, Juergen Reuter, Bertrand Bordage, Mats Bengtsson, Christian Mondrup).
  - **License**: GNU General Public License version 3 or later (`GPL-3.0-or-later`).
  - **Font Embedding Exception**:
    > As a special exception, if you create a document which uses fonts included in LilyPond [here: MScore], and embed this font or unaltered portions of this font into the document, then this font does not by itself cause the resulting document to be covered by the GNU General Public License. This exception does not however invalidate any other reasons why the document might be covered by the GNU General Public License. If you modify one or more of the fonts, you may extend this exception to your version of the fonts but you are not obliged to do so. If you do not wish to do so, delete this exception statement from your version.
- **`MScoreText.otf`**:
  - **Notice**: Copyright (C) 2014 Werner Schweer and Others, with Reserved Font Name "MScore Text".
  - **License**: SIL Open Font License, Version 1.1.

#### MuseJazz (`fonts/musejazz/`)
- **Files**: `MuseJazz.otf`, `MuseJazzText.otf`, `metadata.json`
- **Notice**: Copyright (C) 2016 Torsten Hämmerle, Abraham Lee / Music Type Foundry (https://www.musictypefoundry.com/), with Reserved Font Name "MuseJazz".
- **License**: SIL Open Font License, Version 1.1 (`fonts/musejazz/OFL.txt`).

#### Petaluma (`fonts/petaluma/`)
- **Files**: `Petaluma.otf`, `PetalumaScript.otf`, `PetalumaText.otf`, `petaluma_metadata.json`
- **Notice**: Copyright (C) 2018-2021 Steinberg Media Technologies GmbH (Anthony Hughes, Daniel Spreadbury), with Reserved Font Name "Petaluma".
- **License**: SIL Open Font License, Version 1.1 (`fonts/petaluma/OFL.txt`).

#### GNU FreeFont (`fonts/FreeSans.ttf`, `fonts/FreeSerif*.ttf`)
- **Files**: `FreeSans.ttf`, `FreeSerif.ttf`, `FreeSerifBold.ttf`, `FreeSerifBoldItalic.ttf`, `FreeSerifItalic.ttf`
- **Notice**: Copyright (C) 2002-2012 GNU Freefont contributors.
- **License**: GNU General Public License version 3 or later (`GPL-3.0-or-later`).
- **Font Embedding Exception**:
    > As a special exception, if you create a document which uses this font, and embed this font or unaltered portions of this font into the document, this font does not by itself cause the resulting document to be covered by the GNU General Public License. This exception does not however invalidate any other reasons why the document might be covered by the GNU General Public License. If you modify this font, you may extend this exception to your version of the font, but you are not obligated to do so. If you do not wish to do so, delete this exception statement from your version.

#### MuseScore Tablature & Historical Fonts (`fonts/`)
- **Files**:
  - `MuseScoreTabBonneuilDeVisee.ttf`
  - `MuseScoreTabBonneuilGaultier.ttf`
  - `MuseScoreTabDowland.ttf`
  - `MuseScoreTabFrench.ttf`
  - `MuseScoreTabFrenchBaroque.ttf`
  - `MuseScoreTabFrenchBaroqueHeadless.ttf`
  - `MuseScoreTabItalian.ttf`
  - `MuseScoreTabLuteDidactic.ttf`
  - `MuseScoreTabModern.ttf`
  - `MuseScoreTabPhalese.ttf`
  - `MuseScoreTabRenaiss.ttf`
  - `mscore-BC.ttf`
- **Notice**: Copyright (C) 2012-2015 Maurizio M. Gavioli (mmg@vistamaresoft.com).
- **License**: SIL Open Font License, Version 1.1 (`fonts/OFL.txt`).
- **Reserved Font Names**:
  - `MScoreTabulature` (for `MuseScoreTab*.ttf` tablature family)
  - `MScoreBC` (for `mscore-BC.ttf` figured bass font)

#### SMuFL Metadata (`fonts/smufl/`)
- **Files**: `classes.json`, `glyphnames.json`, `ranges.json`, `README.md`
- **Origin**: W3C Music Notation Community Group Standard Music Font Layout (SMuFL) specification.
- **License**: MIT / W3C Software and Document Notice and License.

---

## 2. Bundled Third-Party Source Code

| Component | In-Tree Path | Upstream Origin | License |
|---|---|---|---|
| **FluidSynth** | `muse/framework/audio/thirdparty/fluidsynth/fluidsynth-2.3.3/` | FluidSynth team (https://www.fluidsynth.org/) | GNU Lesser General Public License v2.1 or later (`LGPL-2.1-or-later`) |
| **stb_vorbis** | `muse/framework/audio/thirdparty/stb/stb_vorbis.c` | Sean Barrett (stb libraries, https://github.com/nothings/stb) | Dual MIT License / Public Domain (Unlicense) |
| **kors_logger** | `muse/framework/global/thirdparty/kors_logger/` | Igor Korsukov | MIT License |
| **sg14::inplace_function** | `muse/framework/global/thirdparty/sg14/inplace_function.h` | SG14 / David Krauss | Boost Software License 1.0 (`BSL-1.0`) |
| **dtl** | `src/engraving/thirdparty/dtl/` | Tatsuhiko Kubo (https://github.com/cubicdaiya/dtl) | 3-Clause BSD License |
| **IntervalTree** | `src/engraving/thirdparty/intervaltree/` | Erik Garrison (https://github.com/ekg/intervaltree) | MIT License |
| **BeatRoot** | `src/importexport/midi/thirdparty/beatroot/` | Simon Dixon & Chris Cannam, Queen Mary Univ. of London | GNU General Public License v2.0 or later (`GPL-2.0-or-later`) |

### Component Details and Notices

#### FluidSynth (`muse/framework/audio/thirdparty/fluidsynth/fluidsynth-2.3.3/`)
- **Version**: 2.3.3 (trimmed and patched to compile standalone with `NO_GLIB`).
- **License**: GNU Lesser General Public License version 2.1 "or (at your option) any later version" (`LGPL-2.1-or-later`), located at `muse/framework/audio/thirdparty/fluidsynth/fluidsynth-2.3.3/LICENSE`.
- **Note**: FluidSynth provides offline SoundFont software synthesis in the audio rendering subsystem.

#### stb_vorbis (`muse/framework/audio/thirdparty/stb/stb_vorbis.c`)
- **Author**: Sean Barrett.
- **License**: Dual-licensed under the MIT License and Public Domain (Unlicense). The full license alternatives are embedded at the end of `muse/framework/audio/thirdparty/stb/stb_vorbis.c`.
- **Note**: Used for decoding Vorbis audio streams in SoundFonts and audio clips.

#### kors_logger (`muse/framework/global/thirdparty/kors_logger/`)
- **Author**: Copyright (C) 2020 Igor Korsukov.
- **License**: MIT License, located at `muse/framework/global/thirdparty/kors_logger/LICENSE`.

#### sg14::inplace_function (`muse/framework/global/thirdparty/sg14/inplace_function.h`)
- **Author**: WG21 SG14 study group / David Krauss.
- **License**: Boost Software License - Version 1.0 - August 17th, 2003 (`BSL-1.0`). Notice is embedded in the header file.

#### Diff Template Library (dtl) (`src/engraving/thirdparty/dtl/`)
- **Author**: Copyright (C) 2015 Tatsuhiko Kubo <cubicdaiya@gmail.com>.
- **License**: 3-Clause BSD License, located at `src/engraving/thirdparty/dtl/COPYING`.

#### IntervalTree (`src/engraving/thirdparty/intervaltree/`)
- **Author**: Copyright (C) 2011 Erik Garrison.
- **License**: MIT License, located at `src/engraving/thirdparty/intervaltree/LICENSE`.

#### BeatRoot (`src/importexport/midi/thirdparty/beatroot/`)
- **Authors**: Simon Dixon and Chris Cannam, Centre for Digital Music, Queen Mary, University of London.
- **License**: GNU General Public License version 2 or later (`GPL-2.0-or-later`), located at `src/importexport/midi/thirdparty/beatroot/COPYING`.
- **Citation Request**: If used for public, commercial, or academic purposes, the authors request citation and attribution (see `src/importexport/midi/thirdparty/beatroot/README.txt`).

---

## 3. External Dependencies Managed by xmake

The standalone `xmake` build resolves and compiles the following third-party dependencies from source recipes in `xmake/packages/` or the central package repository:

| Package | Pinned Version | Typical Upstream License | Usage in engrave |
|---|---|---|---|
| **FreeType** | 2.14.1 | FreeType License (FTL) / GPL-2.0 | Font glyph loading and metrics |
| **HarfBuzz** | 12.3.0 | MIT License | Text and music font shaping |
| **FLAC (libflac)** | 1.4.3 | BSD-3-Clause / GPL | Lossless audio export |
| **LAME** | 3.100 | LGPL-2.0-or-later | MP3 audio export |
| **Opus (libopus)** | 1.5.2 | BSD-3-Clause | Opus audio encoding |
| **libopusenc** | 0.2.1 | BSD-3-Clause | Opus audio container encoding |
| **fdk-aac** | 2.0.3 | Fraunhofer FDK AAC License | AAC audio export |
| **pugixml** | 1.15 | MIT License | XML parsing (MusicXML, MSCX, metadata) |
| **utfcpp** | 4.1.1 | Boost Software License 1.0 | UTF-8 conversion routines |
| **picojson** | commit 111c9be | 2-Clause BSD License | JSON parsing for audio settings and styles |
| **mnxdom** | commit 45bf462 | MIT License | W3C MNX score representation |
| **mnx-json-schema** | commit 40af3ec (`pboettch/json-schema-validator`) | MIT License | JSON schema validator for MNX |
| **zlib** | 1.3.1 | zlib License | Compression for MSCZ and MXL zip archives |
| **nlohmann_json** | 3.12.0 | MIT License | JSON parser for asset manifests and templates |
| **cairo** | System / external prefix | LGPL-2.1 / MPL-1.1 | Vector (PDF/SVG) and raster (PNG) rendering |
| **GoogleTest** (opt-in tests) | 1.17.0 | BSD-3-Clause | Unit test execution |

*Note: The licenses in section 3 apply to external upstream packages fetched at build time, not in-tree files. The `mnx-json-schema` recipe builds `pboettch/json-schema-validator` (MIT); the W3C MNX JSON schema itself is vendored for test validation under `src/importexport/mnx/tests/data/schema/` (W3C Software and Document License).*
