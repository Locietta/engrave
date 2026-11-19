# engrave

`engrave` is a native, headless C++ score engine library providing core music notation, layout, engraving, rendering, import/export, and audio synthesis capabilities. It is built with [Bazel](https://bazel.build/) and a hermetic Clang/libc++ (`xclang`) toolchain, completely free of Qt, GUI, and desktop application dependencies.

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
  - Layout always uses pages; native `layoutMode` values are preserved on read/write.
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

`engrave` uses [Bazel](https://bazel.build/) (pinned in `.bazelversion`) with a hermetic Clang 23 / `libc++` (`xclang`) C++23 toolchain across Windows and Linux.

### Prerequisites

- **Bazelisk** (or Bazel matching `.bazelversion`)
- **Python** 3 and **Git** (for asset installation and manifest verification)
- **[Pixi](https://pixi.sh/)**: cairo and zlib are the conda-forge builds pinned in `pixi.toml` / `pixi.lock`. Run `pixi install --locked` once; `bazel/deps.bzl` exposes them as `@pixi_deps//:cairo` and `@pixi_deps//:zlib`. A superproject consuming `@engrave` via Bzlmod supplies its own Pixi prefix via `--repo_env=ENGRAVE_PIXI_PREFIX=...`. Windows binaries need `.pixi/envs/default/Library/bin` on `PATH` at run time; Linux binaries load `.pixi/envs/default/lib`.

All other C/C++ third-party dependencies (FreeType, HarfBuzz, libflac, LAME, libopus, libopusenc, fdk-aac, pugixml, utfcpp, picojson, glaze, proxy, amc, unordered_dense, GoogleTest) are fetched from hash-pinned archives in `MODULE.bazel` (`bazel/deps.bzl`) and built via `bazel/third_party/*.BUILD.bazel`. `glaze` is the engine's only JSON library; MNX is read and written through a document model generated from the MNX schema (`docs/mnx.md`).

### Build Commands (Windows and Linux)

```bash
pixi install --locked
bazelisk build --config=release //:engrave
```

### Configuration Flags

- `--config=<release|debug|releasedbg|asan>`: Select optimization/debug/sanitizer profile (`bazel/engrave.bazelrc`).
- `--//bazel:pch=false`: Disable binary precompiled headers for `//:engraving` (enabled by default).
- `--//bazel:playback_harness=true`: Enable opt-in playback capture instrumentation (`ENGRAVE_PLAYBACK_HARNESS`).

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

The engine operates on a single dedicated owner thread per process. Lifecycle rules (see `engrave/include/engrave/engine.h`):

- **Single Instance Per Process**: A process has at most one `Engine`. A second `Engine::create()` aborts the process only after a `create()` call has succeeded in claiming the process-global singleton (`s_engine_created` in `engrave/src/api/engine.cpp`). Initial validation failures due to missing resources or nonexistent font/SoundFont file paths return an error (`ErrorCode::NOT_FOUND` or `ErrorCode::INVALID_ARGUMENT`) before the singleton is claimed, so a subsequent `Engine::create()` call can succeed. (A font that exists on disk but fails during engine runtime loading fails after the singleton is claimed, preventing retries). Once claimed, any subsequent `Engine::create()` aborts, even after the first `Engine` has been destroyed.
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

Engine tests are built with Bazel:

```bash
bazelisk build --config=release //:engine_tests
```

### Running the Public Boundary Gate

The standalone gate (`tests/gate.py`) builds and executes the public API, import, playback, and adapter suites, asset verification, MNX schema validation, independent semantic checks, and indexed public-output comparisons.

In a fork-only checkout, `tests/gate.py` automatically prepends `tests/` and `tools/` to `PYTHONPATH` in its execution environment while preserving any existing `PYTHONPATH`:

```bash
python tools/install_assets.py .runtime/assets
python tests/gate.py --build-dir build/gate --assets .runtime/assets --soundfont /path/to/FluidR3Mono_GM.sf3 --output notes/runs/gate
python tests/test_inventory.py
```

Use a fresh output directory under `notes/runs/<name>` for each run (never `.runtime/`, which holds runtime assets only). The gate builds against the Pixi environment of `--pixi-manifest` (default: this checkout's `pixi.toml`), installing it with `pixi install --locked` if absent, and puts its library and binary directories on the runtime search path. `--skip-build` runs the gate on `engine_tests` binaries already staged in `--build-dir`, such as an embedding project's build (the `engrave-mcp` wrapper does this by default). `--quick` skips the exclusive `output-timing` and `dense-cancellation` checks while running all other suites and captures; use it for per-step checks, and run the full gate without `--quick` at milestones and for engine bumps. `--repo-cache` and `--disk-cache` set the Bazel repository and action cache directories (default: `<engine-source>/build/.bazel_repo_cache` and `<engine-source>/build/.bazel_disk_cache`).

Create a new virtual environment with Python 3.13 (`python -m venv .venv`), then install dependencies and the test package in editable mode:

```bash
.venv/bin/python -m pip install -r tests/requirements.txt
.venv/bin/python -m pip install -e .
```

(Windows: `.venv/Scripts/python.exe`). Running `python -m pip install -e .` in the fork root inside the virtual environment provides `engrave_testing`, which `tests/gate.py` and `tests/test_inventory.py` import at startup (and which `tests/requirements.txt` does not supply). Run the commands above with that Python. All normalization and validation dependencies are exact pins matching the host `pixi.lock`. The gate rejects missing or different versions before building or capturing; PyMuPDF 2.1 changes normalized PDF bytes and is unsupported.

Validation tools: Python 3.13, jsonschema 4.26.0, numpy 2.5.4, soundfile 0.14.0, Pillow 13.0.0, PyMuPDF 2.0 and psutil 7.2.2. Cairo and DejaVu fonts must be installed in the supplied native prefix on Linux. Bazel 9.3.2 with hermetic `xclang` 23.1.5.2 builds the C++23 closure on both Windows and Linux. No MCP SDK or desktop MuseScore installation is needed. Shared input/baseline provenance is frozen under `tests/data`; the host reads these files through its submodule. `gate-inventory.json` explicitly selects the 251 master API artifacts from the complete shared platform index. The index's entire payload inventory and hashes are verified before comparison.

### Running a Single Fork Test Script Directly

To execute an individual integration script in a fork-only checkout without an editable package install, prepend `tests` and `tools` to `PYTHONPATH`:

```bash
PYTHONPATH="tests:tools${PYTHONPATH:+:$PYTHONPATH}" python tests/integration/test_assets.py --binary build/gate/assets_test --assets .runtime/assets --font .pixi/envs/default/fonts/DejaVuSans.ttf --soundfont /path/to/FluidR3Mono_GM.sf3 --output notes/runs/test-assets
```

On Windows PowerShell:

```powershell
$env:PYTHONPATH = "tests;tools;$env:PYTHONPATH"
python tests/integration/test_assets.py --binary build/gate/assets_test.exe --assets .runtime/assets --font C:/Windows/Fonts/arial.ttf --soundfont /path/to/FluidR3Mono_GM.sf3 --output notes/runs/test-assets
```

Alternatively, install the `engrave-testing` package in editable mode within the virtual environment (`pip install -e .`), allowing test scripts to run directly without specifying `PYTHONPATH`.

### Continuous Integration

`.github/workflows/ci.yml` runs on pushes to `main`, pull requests and manual dispatch. A `python-checks` job installs `tests/requirements.txt` and `pip install -e .`, then runs `tools/test_install_assets.py`, `tests/test_inventory.py`, `tools/mnx_codegen.py --check`, and `tests/test_playback_witness.py` (also with `python -O`). A `build-test` job on Windows (`windows-latest`) and Linux (`ubuntu-latest`), both using Bazel + hermetic `xclang` with the conda-forge cairo and zlib from `pixi.lock`, builds the unit-test targets in `release` mode, installs and verifies the runtime assets, and runs the same suites and arguments as `tests/gate.py` except `engrave_output_test`: `engrave_api_test`, `engrave_handles_test`, `engrave_describe_test`, `engrave_read_test`, `engrave_ops_test`, `engrave_gp_import_test`, `engrave_mnx_import_test` with its MNX schema check, `engrave_mnx_model_test`, `diagnostics_test` and `io_boundary_test`, then the upstream `engraving_playback_tests` and `mpe_tests` through `tests/integration/test_upstream_gtest.py`. `example_embedder` is built as the public-header leak check. CI does not run the full gate, the golden comparisons, the audio corpus or anything that needs a SoundFont (`engrave_output_test`, `engine_audio`, the render goldens); those stay manual on the Windows and Linux hosts. Formatting and naming lint are not part of CI.

## Development

- [docs/code-style-guide.md](docs/code-style-guide.md): Code style, naming conventions, and contribution guidelines.
- [docs/playback.md](docs/playback.md): Offline audio render pipeline and synthesis architecture.

Style tools use clang-format/clang-tidy 23.1.4 on PATH (compilation databases are generated from the enclosing `engrave-mcp` checkout via `pixi run compdb`):

```bash
python tools/format.py --check .
python tools/lint.py .
python tools/setup_hooks.py
```

The installed hook checks staged blobs without modifying the index. Naming lint
checks compiled owned translation units and rejects empty selections and `--fix`.
Third-party directories are excluded. Run these tools from the fork root.

## License

`engrave` is free software licensed under the **GNU General Public License version 3.0 (GPLv3)**. See [LICENSE.txt](LICENSE.txt).

Bundled font families and third-party components are licensed under their respective terms (including GNU FreeFont with embedding exception, MScore with font exception, SIL Open Font License 1.1, LGPL 2.1, MIT, and BSD). See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) for the complete attribution and license inventory.
