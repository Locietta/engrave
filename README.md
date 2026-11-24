# engrave

`engrave` is a native, headless C++ score engine library providing core music notation, layout, engraving, rendering, import/export, and audio synthesis capabilities. It is built with [Bazel](https://bazel.build/) and a hermetic Clang/libc++ (`xclang`) toolchain.

The library serves as the core engraving and playback engine for the [`engrave-mcp`](https://github.com/engrave-music/engrave-mcp) Model Context Protocol (MCP) host.

## Relationship to MuseScore

`engrave` is a hard fork of MuseScore core (derived from upstream MuseScore 5.0 development).

- **Upstream Base**: Based on upstream MuseScore commit `32f0a26e5b`, with the MuseScore framework (commit `da3885cb`) absorbed into `muse/`. Development moves forward independently; upstream fixes arrive only by cherry-pick.
- The library omits the desktop interface, interactive widgets, and cloud services.
- **Attribution & Disclaimer**: `engrave` acknowledges original copyright and contributions from MuseScore Limited and the MuseScore community. It is an independent library project and is not affiliated with, endorsed by, or maintained by MuseScore or Muse Group.

## Supported Formats

Capabilities can be queried programmatically via `Engine::capabilities()`.

### Input Formats

- **MuseScore**: `.mscz` (compressed archive) and `.mscx` (uncompressed XML), versions 4.60 to 5.00. Files prior to 4.60 (`mscVersion < 460`) return `ErrorCode::UNSUPPORTED`; versions above 5.00 (`mscVersion > 500`) are rejected.
- **MusicXML**: Uncompressed `.musicxml` / `.xml` and compressed `.mxl`.
- **MIDI**: Standard MIDI Files (`.mid`, `.midi`, `.kar`).
- **Guitar Pro**: Guitar Pro 1–7 (`.gtp`, `.gp3`, `.gp4`, `.gp5`, `.gpx`, `.gp`) and PowerTab (`.ptb`).
- **MNX**: W3C MNX JSON (schema 27; import only in the public API — see [docs/mnx.md](docs/mnx.md)).

### Export Formats

- **Score Files** (`Document::export_score`, `Document::export_score_with_warnings`):
  - **MuseScore**: `.mscz` and `.mscx` (default native version `"5.00"`). A bounded compatibility writer is available for version `"4.70"` (interoperable with MuseScore 4.7.0–4.7.6; see [docs/write470.md](docs/write470.md)).
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
  - Encoders: **WAV** (16, 24, 32-bit float), **FLAC** (16, 24-bit), **Opus** (Ogg container, 48 kHz), **MP3** (128 kbps default, 32–512 kbps).
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
- **[Pixi](https://pixi.sh/)**: Cairo, FreeType, and zlib are conda-forge builds pinned in `pixi.toml` / `pixi.lock`. Run `pixi install --locked` once; `bazel/deps.bzl` exposes them as `@pixi_deps//:cairo`, `@pixi_deps//:freetype`, and `@pixi_deps//:zlib`. A superproject consuming `@engrave` via Bzlmod supplies its own Pixi prefix via `--repo_env=ENGRAVE_PIXI_PREFIX=...`. Windows binaries need `.pixi/envs/default/Library/bin` on `PATH` at run time; Linux binaries load `.pixi/envs/default/lib`.

All other C/C++ third-party dependencies (HarfBuzz, libflac, LAME, libopus, libopusenc, pugixml, utfcpp, glaze, proxy, amc, unordered_dense, GoogleTest) are fetched from hash-pinned archives in `MODULE.bazel` (`bazel/deps.bzl`) and built via `bazel/third_party/*.BUILD.bazel`. `glaze` is the engine's only JSON library.

### Build Commands (Windows and Linux)

```bash
pixi install --locked
bazelisk build --config=release //:engrave
```

### Configuration Flags

- `--config=<release|debug|releasedbg|sanitize>`: Select optimization/debug/sanitizer profile (`bazel/engrave.bazelrc`).
- `--//bazel:pch=false`: Disable binary precompiled headers for `//:engraving` (enabled by default).
- `--//bazel:playback_harness=true`: Enable opt-in playback capture instrumentation (`ENGRAVE_PLAYBACK_HARNESS`).

The `sanitize` profile enables ASan and fatal UBSan on Linux and Windows
x86-64. ASan enables LSan on Linux x86-64. Windows x86-64 has no LSan support.

## Asset Installation

Runtime assets (SMuFL music fonts, text fonts, instrument templates, engraving styles, and MPE profiles) are installed using `tools/install_assets.py`. The installer reads committed Git blobs to preserve exact bytes regardless of local line-ending settings.

```bash
# Install assets to a target directory
python tools/install_assets.py <destination_dir>

# Verify installed files against the pinned manifest (engrave/assets.json)
python tools/install_assets.py <destination_dir> --verify
```

Pass `<destination_dir>` to `EngineOptions::resources` when initializing the engine. See [docs/assets.md](docs/assets.md) for details.

## Public API

Public headers live under `engrave/include/engrave/`:

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

The engine operates on a single dedicated owner thread per process (see `engrave/include/engrave/engine.h`):

- **One Active Runtime Per Process**: `Engine::create()` validates options and returns `ErrorCode::UNSUPPORTED` while an Engine or its Documents are active. Documents remain usable on the original thread after their Engine is destroyed. Once the Engine and its last Document are destroyed, the process can create another Engine. Initialization failures also allow retries.
- **Owner Thread Affiliation**: Every `Engine` and `Document` call must occur on the thread that created the engine; calling from another thread aborts.

## Testing

### Test Layout

- `tests/integration/`: Standalone executable integration test scripts (`test_assets.py`, `test_excerpts.py`, `test_engine_audio.py`, `test_roundtrip.py`, `test_archives.py`, `test_export_options.py`, `test_oracle_harness.py`, `test_public_output.py`, `test_upstream_gtest.py`). Run directly; never imported as Python modules.
- `tests/engrave_testing/`: Shared helper package (`golden`, `paths`, `public_output`, `dependencies`, `inventory`, `links`, `mscx`, `export_formats`, etc.).
- `tests/gate.py`: Public C++ boundary gate runner.
- `tests/test_inventory.py`: Verification suite for golden comparators, inventory integrity, and baseline mutations.
- `tests/data/`: Golden baselines, test inputs, and platform allow-lists (`tests/data/golden/allow.json`).

### Building Test Binaries

```bash
bazelisk build --config=release //:engine_tests
```

### Running the Public Boundary Gate

`tests/gate.py` builds and runs the public API, import, playback, and adapter suites, asset verification, MNX schema validation, independent semantic checks, and indexed public-output comparisons (`gate-inventory.json` selects the 251 master API artifacts from the verified platform index).

```bash
python tools/install_assets.py .runtime/assets
python tests/gate.py --build-dir build/gate --assets .runtime/assets --soundfont /path/to/FluidR3Mono_GM.sf3 --output notes/runs/gate
python tests/test_inventory.py
```

Key options and environment requirements:

- **Fresh output directory**: Always pass a new `--output` directory under `notes/runs/<name>` (never `.runtime/`, which holds runtime assets only).
- **Quick vs. full**: `--quick` skips the `engine-audio` corpus check for faster per-step verification; run without `--quick` at milestones and engine bumps.
- `--no-audio` runs the SoundFont-free gate, including 226 indexed outputs across 25 fixtures. Add `--goldens-only` to run just the manifest check and comparison.
- **Build and caches**: The gate builds against `--pixi-manifest` (default: `pixi.toml`, installing with `pixi install --locked` if absent) and puts its libraries on the runtime search path. `--skip-build` reuses binaries already staged in `--build-dir` (the `engrave-mcp` wrapper uses this by default). `--repo-cache` and `--disk-cache` select Bazel cache directories (default: `build/.bazel_repo_cache` and `build/.bazel_disk_cache`).
- **Persistent Bazel JVM for Windows agent sessions**: Register `tools/bazel_server_mcp.py` (stdlib-only stdio MCP server exposing `bazel_server_start` and `bazel_server_status`, with `--warm` background keepalive and `--project NAME=PATH` / `--config projects.json` mapping) in your IDE/agent MCP config so the Bazel JVM stays warm outside per-command Windows Job Objects.
- **Standalone Python environment**: In a fork-only checkout, create a Python 3.13 venv and install pinned dependencies plus `engrave_testing` in editable mode:
  ```bash
  python -m venv .venv
  .venv/bin/python -m pip install -r tests/requirements.txt
  .venv/bin/python -m pip install -e .
  ```
  (Windows: `.venv/Scripts/python.exe`). All normalization/validation dependencies match the host `pixi.lock` pins (`jsonschema 4.26.0`, `numpy 2.5.4`, `soundfile 0.14.0`, `Pillow 13.0.0`, `PyMuPDF 2.0`, `psutil 7.2.2`); the gate rejects mismatched versions (PyMuPDF 2.1 alters normalized PDF bytes). Linux also requires Cairo and DejaVu fonts in the native prefix.

### Running a Single Fork Test Script Directly

With `engrave-testing` installed via `pip install -e .`, run scripts directly. Without an editable install, prepend `tests` and `tools` to `PYTHONPATH`:

```bash
PYTHONPATH="tests:tools${PYTHONPATH:+:$PYTHONPATH}" python tests/integration/test_assets.py --binary build/gate/assets_test --assets .runtime/assets --font .pixi/envs/default/fonts/DejaVuSans.ttf --soundfont /path/to/FluidR3Mono_GM.sf3 --output notes/runs/test-assets
```

```powershell
$env:PYTHONPATH = "tests;tools;$env:PYTHONPATH"
python tests/integration/test_assets.py --binary build/gate/assets_test.exe --assets .runtime/assets --font C:/Windows/Fonts/arial.ttf --soundfont /path/to/FluidR3Mono_GM.sf3 --output notes/runs/test-assets
```

### Continuous Integration

`.github/workflows/ci.yml` runs on pushes to `main`, pull requests, and manual dispatch:

- **`python-checks`**: Installs `tests/requirements.txt` and `pip install -e .`, then runs `tools/test_install_assets.py`, `tests/test_inventory.py`, `tools/mnx_codegen.py --check`, and `tests/test_playback_witness.py` (also with `python -O`).
- **`build-test`** builds release unit tests and `example_embedder` on Windows and Linux with Bazel, `xclang`, and Pixi cairo/zlib. It verifies runtime assets, then runs `engrave_core_tests`, the MNX schema check, both upstream GTest suites, and the SoundFont-free golden comparison.
- **`sanitize`** runs four shards of `engrave_core_tests`, both upstream GTest suites, and the SoundFont-free golden probes on Linux with ASan, LSan, and UBSan. Sanitized actions have a separate disk cache key.
- The full gate with audio goldens and the audio corpus are manual milestone checks.

## Documentation and Style Tools

- [docs/code-style-guide.md](docs/code-style-guide.md): Code style, naming conventions, and contribution guidelines.
- [docs/playback.md](docs/playback.md): Offline audio render pipeline, synthesis architecture, and cooperative cancellation contract.
- [docs/playback-harness.md](docs/playback-harness.md): Opt-in playback identity, timing, and allocation-counting harness (`ENGRAVE_PLAYBACK_HARNESS`).
- [docs/mnx.md](docs/mnx.md): Engine-owned MNX document model and schema departures.
- [docs/write470.md](docs/write470.md): Bounded MuseScore 4.70 compatibility export contract.
- [docs/assets.md](docs/assets.md): Runtime asset manifest and installation.

Style tools use `clang-format` / `clang-tidy` 23.1.4 on `PATH` (compilation databases are generated from the enclosing `engrave-mcp` checkout via `pixi run compdb`):

```bash
python tools/format.py --check .
python tools/lint.py .
python tools/setup_hooks.py
```

The installed pre-commit hook checks staged blobs without modifying the index. Naming lint checks compiled owned translation units and rejects empty selections and `--fix`. Third-party directories are excluded. Run these tools from the fork root.

## License

`engrave` is free software licensed under the **GNU General Public License version 3.0 (GPLv3)**. See [LICENSE.txt](LICENSE.txt).

Bundled font families and third-party components are licensed under their respective terms (including GNU FreeFont with embedding exception, MScore with font exception, SIL Open Font License 1.1, LGPL 2.1, MIT, and BSD). See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) for the complete attribution and license inventory.
