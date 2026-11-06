# engrave

`engrave` is a native, headless C++ score engine library providing core music notation, layout, engraving, rendering, import/export, and audio synthesis capabilities. It is built with [xmake](https://xmake.io/) and is completely free of Qt, GUI, and desktop application dependencies.

The library serves as the core engraving and playback engine for the [`engrave-mcp`](https://github.com/Locietta/engrave-mcp) Model Context Protocol (MCP) host.

## Relationship to MuseScore

`engrave` is a hard fork of MuseScore core (derived from upstream MuseScore 5.0 development).
- **Ancestral Provenance**: The fork's upstream synchronization history terminates at `upstream-final` (syncing upstream MuseScore commit `32f0a26e5b` and framework `da3885cb`). Since that sync point, development moves forward independently.
- **Headless Score Library**: All desktop UI, Qt bindings, user-interaction widgets, QML, cloud services, and legacy score readers prior to MuseScore 4.6 have been removed.
- **Attribution & Disclaimer**: `engrave` acknowledges original copyright and contributions from MuseScore Limited and the MuseScore community. It is an independent library project and is not affiliated with, endorsed by, or maintained by MuseScore or Muse Group.

## Supported Formats

Capabilities can be queried programmatically via `Engine::capabilities()`.

### Input Formats
- **MuseScore**: `.mscz` (compressed archive) and `.mscx` (uncompressed XML), versions 4.60 and later. Files created with MuseScore versions prior to 4.6 (mscVersion < 460) return `ErrorCode::Unsupported`.
- **MusicXML**: Uncompressed `.musicxml` / `.xml` and compressed `.mxl`.
- **MIDI**: Standard MIDI Files (`.mid`, `.midi`, `.kar`).
- **Guitar Pro**: Guitar Pro 1–7 (`.gtp`, `.gp3`, `.gp4`, `.gp5`, `.gpx`, `.gp`) and PowerTab (`.ptb`).
- **MNX**: W3C MNX JSON (schema 27; import only).

### Export Formats
- **Score Files** (`Document::export_score`, `Document::export_score_with_warnings`):
  - **MuseScore**: `.mscz` and `.mscx` (default native version `"5.00"`). A restricted, bounded compatibility writer is available for version `"4.70"` (interoperable with MuseScore 4.7.0–4.7.6; see [WRITE470.md](engrave/WRITE470.md)).
  - **MusicXML**: Uncompressed MusicXML and compressed `.mxl`.
  - **MIDI**: Standard MIDI File, with configurable repeat expansion and RPN controls.
  - **Excerpt Export**: Export an individual linked part or excerpt as a standalone score via `ExportOptions::score`.
- **Visual Rendering** (`Document::render` via Cairo):
  - **PDF**: Multi-page vector document.
  - **PNG**: High-resolution raster images per page (36 to 1200 DPI; transparent or score-styled background).
  - **SVG**: Scalable vector graphics per page.
  - Inclusive page range selection (`first_page` to `last_page`).
- **Audio Synthesis** (`Document::render_audio` via FluidSynth):
  - Encoders: **WAV** (16, 24, 32-bit float), **FLAC** (16, 24-bit), **Opus** (in Ogg container, 48 kHz), **MP3** (128 kbps default, 32–512 kbps), **AAC** (128 kbps, ADTS).
  - Multi-track mixing: Per-track volume (dB), stereo pan/balance, mute, solo, and SoundFont selection.
  - Reverb: Aux send/return with Muse Reverb DSP.
  - Streaming: Delivers encoded bytes chunk-by-chunk to a caller-supplied `ByteSink`.
- **Geometry & Query**:
  - `Document::positions`: Exact 1200-DPI layout units for page-local measure/segment bounding boxes and repeat-expanded playback event positions.
  - `Document::query`: Structural inspection of score elements via stable typed handles.

## Building

`engrave` uses the [xmake](https://xmake.io/) build system and requires a C++23 conforming compiler.

### Prerequisites
- **xmake** >= 3.1.1
- **C++23 Compiler**:
  - Windows: MSVC (Visual Studio 2022 / Build Tools 17.10+) or clang-cl
  - Linux: GCC 13+ or Clang 16+
- **Python** 3 and **Git** (for asset installation and manifest verification)
- **Cairo**:
  - Linux: Install the system development package (e.g. `libcairo2-dev` on Ubuntu/Debian, `cairo-devel` on Fedora, `cairo` on Arch/CachyOS).
  - Windows: Provide the cairo install prefix using `--cairo_prefix=<path>` (e.g. pointing to a directory with `include/cairo` and `lib/cairo.lib`, such as a conda or pixi environment).

All other C/C++ third-party dependencies (FreeType, HarfBuzz, libflac, LAME, libopus, libopusenc, fdk-aac, pugixml, utfcpp, picojson, mnxdom, zlib, nlohmann-json) are managed and built from source recipes in `xmake/packages/`.

### Build Commands

#### Windows
```pwsh
# Configure with cairo prefix and release mode
xmake f -m release --cairo_prefix=C:/path/to/cairo/prefix

# Build the engrave library target
xmake build engrave
```

#### Linux
```bash
# Configure release build (uses system cairo by default)
xmake f -m release

# Build the engrave library target
xmake build engrave
```

### Configuration Options
- `--cairo_prefix=<path>`: Custom installation prefix for the Cairo graphics library.
- `--engrave_tests=y`: Enable development test targets (requires GoogleTest, resolved automatically).

## Asset Installation

Runtime assets (SMuFL music fonts, text fonts, instrument templates, engraving styles, and MPE profiles) are installed using `tools/install_assets.py`. The installer reads committed Git blobs to ensure byte-exact preservation regardless of local line-ending configurations.

```bash
# Install assets to a target directory
python tools/install_assets.py <destination_dir>

# Verify installed files against the pinned manifest (engrave/assets.json)
python tools/install_assets.py <destination_dir> --verify
```

Pass `<destination_dir>` to `EngineOptions::resources` when initializing the engine. For details, see [engrave/ASSETS.md](engrave/ASSETS.md).

## Public API

Public headers are located under `engrave/include/engrave/`:
- `engrave/engrave.h`: Umbrella include header.
- `engrave/engine.h`: `Engine` runtime lifecycle, font registration, SoundFont configuration, and document creation/opening.
- `engrave/document.h`: `Document` operations: layout, editing, rendering, audio synthesis, export, positions, and queries.
- `engrave/output.h`: Export and render options (`ExportOptions`, `RenderOptions`, formats).
- `engrave/audio.h`: Audio rendering options, mixer parameters, track selection, and format settings (`AudioOptions`, `AudioFormat`).
- `engrave/edit.h`: Batch score edit operations, note and measure manipulation, and undo/redo semantics.
- `engrave/query.h`: Score inspection, element handles, and property queries.
- `engrave/export_warning.h`: Structured warnings emitted by compatibility exporters.
- `engrave/error.h`: Error codes (`ErrorCode`) and `Result<T>` monadic wrapper.
- `engrave/version.h`: Engine version information.

### Execution Model
The engine operates on a single dedicated owner thread per process. An `Engine` instance manages process-global resources (fonts, audio context, and up to 16 concurrent open documents). All calls to an `Engine` and its `Document` instances must occur on the thread that created the engine.

## Testing

Engine tests are built with xmake:
```bash
xmake f --engrave_tests=y
xmake build c0_gate
```

Full conformance testing, golden render/audio regressions, and multi-format validation are currently driven from the companion host repository [`engrave-mcp`](https://github.com/Locietta/engrave-mcp) via `scripts/engine_gate.py`.

## License

`engrave` is free software licensed under the **GNU General Public License version 3.0 (GPLv3)**. See [LICENSE.txt](LICENSE.txt).

Bundled font families and third-party components are licensed under their respective terms (including GNU FreeFont with embedding exception, MScore with font exception, SIL Open Font License 1.1, LGPL 2.1, MIT, and BSD). See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) for the complete attribution and license inventory.
