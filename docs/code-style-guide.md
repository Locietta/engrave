# Code Style Guide

This is a concise code style guide for the `engrave` engine fork. Contributors or agents should follow these guidelines unless there is a compelling reason to deviate.

If not specified here, follow the code style of the existing code around the code you are writing.

## Scope and Upstream Code

The engine is a hard fork of MuseScore core and preserves upstream MuseScore music algorithms, data structures, and object models. Existing upstream code is not rewritten simply to fit this guide: virtual-heavy object models, inheritance hierarchies, and existing error-handling patterns stay as they are.

The guidelines here apply to new code and touched code where practical.

## Formatting and Linting

The engine checkout does not use Pixi tasks; formatters and linters are run directly using Python scripts in `tools/`.

### Formatting (`tools/format.py`)

Run `python tools/format.py <paths...>` to format specific files or directories, and `python tools/format.py --check <paths...>` to verify that files comply with formatting rules without modifying them.

Supported options:

- `-l, --language <cpp|json|markdown|python|toml>`: override language detection for every input.
- `--check`: report files that require formatting without modifying them.
- `--engine`: format tracked owned engine C++ files.
- `--engine-root <path>`: engine checkout location (used with `--engine`).

Under the hood, formatters are selected by file extension:

- `clang-format`: C/C++ source and header files (`.c`, `.cc`, `.cpp`, `.cxx`, `.h`, `.hh`, `.hpp`, `.hxx`).
- `ruff`: Python files (`.py`, `.pyi`).
- `prettier`: JSON, Markdown, and YAML files (`.json`, `.md`, `.yaml`, `.yml`).
- `taplo`: TOML files (`.toml`).

### Linting (`tools/lint.py`)

Run `python tools/lint.py [paths...]` to lint project files (`ruff` for Python, `clang-tidy` for C++).

Supported options:

- `--engine`: check compiled owned engine translation units against `.clang-tidy`. Requires `compile_commands.json` (generate via `xmake project -k compile_commands`).
- `--engine-root <path>`: engine checkout location (used with `--engine`).
- `--fix`: automatically apply lint fixes where available (note: `--fix` is unsupported with `--engine`).
- `-j, --jobs <N>`: control linting parallelism (defaults to available CPU cores, capped at 12).

You can also run `python tools/setup_hooks.py` to install a Git pre-commit hook that checks staged blobs before commit without modifying the index.

## Identifier Naming

See [.clang-tidy](../.clang-tidy).

- Types, structs, classes, enums, and unions use `CamelCase`.
- Functions, methods, parameters, local variables, and global constants use `lower_case`.
- Enum constants use `UPPER_CASE`.
- Single-letter and two-letter uppercase names (such as template parameters or coordinate identifiers like `T`, `ID`) are permitted.
- Struct and class member variables use `lower_case`. Members may end in one trailing `_` (e.g. `member_`), but never have to.

## Structs and Custom Types

Always use `struct`, do not use `class` for new custom types. This way, we do not need to write `public:` for every member, and when inheriting, we do not need to write `public` in the inheritance list.

As noted above, existing upstream classes retain their original declarations.

## Header Inclusion

Headers should be grouped in the following order, with blank lines separating groups:

1. Interface header of the current `.cpp` file
2. Standard library headers (`<algorithm>`, `<vector>`, `<string>`, etc.)
3. Third-party headers (`<cairo/cairo.h>`, `<ft2build.h>`, `<hb.h>`, `<nlohmann/json.hpp>`, etc.)
4. Project headers (`muse/...`, `engraving/...`, `engrave/...`)

Real example from `engrave/src/render/cairo_painter.cpp`:

```cpp
// Interface header of the current .cpp file
#include "cairo_painter.h"

// std headers
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

// 3rd party headers
#include <cairo/cairo-ft.h>
#include <cairo/cairo.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <hb-ft.h>
#include <hb.h>

// project headers: muse/..., engraving/..., engrave/...
#include "draw/internal/fontfaceutils.h"
#include "draw/internal/ifontsdatabase.h"
```

Another example from `engrave/src/api/audio.cpp`:

```cpp
// Interface header of the current .cpp file
#include <engrave/document.h>

// std headers
#include <cmath>
#include <filesystem>
#include <format>
#include <fstream>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

// project headers: engrave/..., engraving/...
#include "engrave/src/api/handles.h"
#include "engrave/src/api/state.h"
#include "engrave/src/audio/engine_audio.h"
#include "engrave/src/audio/score_audio.h"
#include "engrave/src/runtime/score_io.h"
#include "engraving/dom/excerpt.h"
#include "engraving/dom/masterscore.h"
#include "engraving/dom/score.h"
#include "engraving/engravingproject.h"
```

## Engine Gate Validation

Every engine change must be verified against the engine gate (`tests/gate.py`).

The gate builds the fork's `engine_tests` target and executes the public API, import, playback, and adapter suites, asset manifest verification, MNX schema validation, independent semantic checks, and indexed public-output comparisons against pinned goldens and the platform allow-list (`tests/data/golden/allow.json`).

Run the gate:

```bash
python tools/install_assets.py .runtime/assets
python tests/gate.py --build-dir build/gate --assets .runtime/assets --soundfont /path/to/FluidR3Mono_GM.sf3 --cairo-prefix /path/to/native-prefix --output .runtime/gate-run
python tests/test_inventory.py
```

Use a fresh output directory for each run. On Windows, use native Python/PowerShell with a Cairo prefix ending in `Library` on `PATH`. On Linux, use the native prefix with `bin` on `PATH` and `lib` on `LD_LIBRARY_PATH`.

## Commit Messages

The headline of a commit should follow the format: `<type>(<scope>): <subject>`. Type or scope can be omitted if not applicable.

If the commit cannot be fully described in the headline, add a body separated by a blank line, preferably formatted as a list of bullet points.

Example:

```
refactor(render): align cairo painter pen transform with upstream geometry

- Updates pen scaling logic to account for current staff magnification.
- Normalizes point coordinate transformations before rasterization.
- Verifies visual output against golden rendering baselines.
```
