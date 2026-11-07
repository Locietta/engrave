# engrave

`engrave` is a native, headless C++ score engine library providing core music notation, layout, engraving, rendering, import/export, and audio synthesis capabilities. It is built with [xmake](https://xmake.io/) and is completely free of Qt, GUI, and desktop application dependencies.

The library serves as the core engraving and playback engine for the [`engrave-mcp`](https://github.com/Locietta/engrave-mcp) Model Context Protocol (MCP) host.

## Relationship to MuseScore

`engrave` is a hard fork of MuseScore core (derived from upstream MuseScore 5.0 development).

- **Upstream Base**: The fork is based on upstream MuseScore commit `32f0a26e5b`, with the MuseScore framework (commit `da3885cb`) absorbed into `muse/`. Development moves forward independently; upstream fixes arrive only by cherry-pick.
- **Headless Score Library**: All desktop UI, Qt bindings, user-interaction widgets, QML, cloud services, and legacy score readers prior to MuseScore 4.6 have been removed.
- **Attribution & Disclaimer**: `engrave` acknowledges original copyright and contributions from MuseScore Limited and the MuseScore community. It is an independent library project and is not affiliated with, endorsed by, or maintained by MuseScore or Muse Group.

## Supported Formats

Capabilities can be queried programmatically via `Engine::capabilities()`.

### Input Formats

- **MuseScore**: `.mscz` (compressed archive) and `.mscx` (uncompressed XML), versions 4.60 to 5.00. Files created with MuseScore versions prior to 4.60 (mscVersion < 460) return `ErrorCode::UNSUPPORTED`; versions above the current 5.00 format (mscVersion > 500) are rejected.
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

### Execution Model and Lifecycle Rules

The engine operates on a single dedicated owner thread per process. Lifecycle rules per `engrave/include/engrave/engine.h:105`:

- **Single Instance Per Process**: A process has at most one `Engine`. A second `Engine::create()` aborts the process only after a `create()` call has succeeded in claiming the process-global singleton (`s_engine_created` via `engine.cpp:90`). Initial validation failures due to missing resources or nonexistent font/SoundFont file paths return an error (`ErrorCode::NOT_FOUND` or `ErrorCode::INVALID_ARGUMENT`) before the singleton is claimed, so a subsequent `Engine::create()` call can succeed. (A font that exists on disk but fails during engine runtime loading fails after the singleton is claimed, preventing retries). Once claimed, any subsequent `Engine::create()` aborts, even after the first `Engine` has been destroyed.
- **Open Documents at Teardown**: Destroying an `Engine` while any of its `Document` instances are still open aborts the process. All documents must be destroyed before the engine is destroyed.
- **Owner Thread Affiliation**: Every call on an `Engine` and its `Document` instances must occur on the thread that created the engine; a call from another thread aborts.

## Testing

### Test Layout

The engine test suite is organized into distinct directories:

- `tests/integration/`: Standalone executable integration test scripts (`test_assets.py`, `test_excerpts.py`, `test_engine_audio.py`, `test_roundtrip.py`, `test_archives.py`, `test_export_options.py`, `test_oracle_harness.py`, `test_public_output.py`, `test_upstream_gtest.py`). These scripts are run directly and are never imported as Python modules.
- `tests/engrave_testing/`: The shared testing package (`golden`, `paths`, `public_output`, `dependencies`, `inventory`, `links`, `mscx`, `export_formats`, etc.).
- `tests/gate.py`: The public C++ boundary gate runner.
- `tests/test_inventory.py`: Verification suite for golden comparators, inventory integrity, and baseline mutations.
- `tests/data/`: Shared golden baselines, test inputs, and platform allow-lists (`tests/data/golden/allow.json`).

### Building Test Binaries

Engine tests are built with xmake (pass `-P .` when building in a nested checkout):

```bash
xmake f -P . --engrave_tests=y
xmake build -P . engine_tests
```

### Running the Public Boundary Gate

The standalone gate (`tests/gate.py`) builds and executes the public API, import, playback, and adapter suites, asset verification, MNX schema validation, independent semantic checks, and indexed public-output comparisons.

In a fork-only checkout, `tests/gate.py` automatically prepends `tests/` and `tools/` to `PYTHONPATH` in its execution environment while preserving any existing `PYTHONPATH`:

```bash
python tools/install_assets.py .runtime/assets
python tests/gate.py --build-dir build/gate --assets .runtime/assets --soundfont /path/to/FluidR3Mono_GM.sf3 --cairo-prefix /path/to/cairo/prefix --output .runtime/gate-run
python tests/test_inventory.py
```

Use a fresh output directory for each run. Windows uses native Python/PowerShell and a Cairo prefix ending in `Library`; put its `bin` on `PATH`. Linux uses the native prefix with its `bin` on `PATH` and `lib` on `LD_LIBRARY_PATH`. When a prefix supplies the Conda compiler, the gate selects it; otherwise configure with the system compiler. The optional `--mutex-wrapper` supports callers sharing xmake's Windows lock. `--skip-build` is diagnostic only. The dense-audio cancellation timing assertion can fail on Windows; failures are retained and must not be reported as a pass.

Create a new virtual environment with Python 3.13 (`python -m venv .venv`), then install with `.venv/bin/python -m pip install -r tests/requirements.txt` (Windows: `.venv/Scripts/python.exe`). Run the commands above with that Python. All normalization and validation dependencies are exact pins matching the host `pixi.lock`. The gate rejects missing or different versions before building or capturing; PyMuPDF 2.1 changes normalized PDF bytes and is unsupported.

Validation tools: Python 3.13, jsonschema 4.26.0, numpy 2.5.4, soundfile 0.14.0, Pillow 13.0.0, PyMuPDF 2.0 and psutil 7.2.2. Cairo and DejaVu fonts must be installed in the supplied native prefix on Linux. xmake 3.1.1+, MSVC 14.51+ or GCC 15.3 build the C++23 closure. No MCP SDK or desktop MuseScore installation is needed. Shared input/baseline provenance is frozen under `tests/data`; the host reads these files through its submodule. `gate-inventory.json` explicitly selects the 251 master API artifacts from the complete shared platform index. The index's entire payload inventory and hashes are verified before comparison.

### Running a Single Fork Test Script Directly

To execute an individual integration script in a fork-only checkout without an editable package install, prepend `tests` and `tools` to `PYTHONPATH`:

```bash
PYTHONPATH="tests:tools${PYTHONPATH:+:$PYTHONPATH}" python tests/integration/test_assets.py --binary build/gate/assets_test --assets .runtime/assets --font fonts/DejaVuSans.ttf --soundfont /path/to/FluidR3Mono_GM.sf3 --output .runtime/test-assets
```

On Windows PowerShell:

```powershell
$env:PYTHONPATH = "tests;tools;$env:PYTHONPATH"
python tests/integration/test_assets.py --binary build/gate/assets_test.exe --assets .runtime/assets --font C:/Windows/Fonts/arial.ttf --soundfont /path/to/FluidR3Mono_GM.sf3 --output .runtime/test-assets
```

Alternatively, install the `engrave-testing` package in editable mode within the virtual environment (`pip install -e .`), allowing test scripts to run directly without specifying `PYTHONPATH`.

## Development

For code style, naming conventions, and contribution guidelines, see [docs/code-style-guide.md](docs/code-style-guide.md).

Style tools use clang-format/clang-tidy 23.1.4 on PATH:

```bash
python tools/format.py --check .
xmake project -k compile_commands
python tools/lint.py .
python tools/setup_hooks.py
```

The installed hook checks staged blobs without modifying the index. Naming lint
checks compiled owned translation units and rejects empty selections and `--fix`.
Third-party directories are excluded. Run these tools from the fork root.

## License

`engrave` is free software licensed under the **GNU General Public License version 3.0 (GPLv3)**. See [LICENSE.txt](LICENSE.txt).

Bundled font families and third-party components are licensed under their respective terms (including GNU FreeFont with embedding exception, MScore with font exception, SIL Open Font License 1.1, LGPL 2.1, MIT, and BSD). See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) for the complete attribution and license inventory.
