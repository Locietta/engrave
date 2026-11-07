# Code Style Guide

This is a concise code style guide for the `engrave` engine fork. Contributors or agents should follow these guidelines unless there is a compelling reason to deviate.

If not specified here, follow the code style of the existing code around the code you are writing.

## Scope and Upstream Code

The engine is a hard fork of MuseScore core and preserves upstream MuseScore music algorithms, data structures, and object models. Existing upstream code is not rewritten simply to fit this guide: virtual-heavy object models, inheritance hierarchies, and existing error-handling patterns stay as they are.

The guidelines here apply to new code and touched code where practical.

## Formatting and Linting

The engine checkout does not use Pixi tasks; formatters and linters are run directly using Python scripts in `tools/`. Both scripts operate in engine C++ mode, targeting tracked, owned engine C++ files (`.c`, `.cc`, `.cpp`, `.cxx`, `.h`, `.hh`, `.hpp`, `.hxx`) under `engrave/`, `src/`, or `muse/` (excluding third-party directories).

### Formatting (`tools/format.py`)

`tools/format.py` formats owned engine C++ source and header files using `clang-format`.

Run `python tools/format.py <paths...>` to format specific engine C++ files or directories, and `python tools/format.py --check <paths...>` to verify formatting without modifying files.

Supported options:

- `<paths...>`: one or more files or directories to format. Only owned engine C++ files under `engrave/`, `src/`, or `muse/` are formatted; paths with no owned engine C++ files fail with an error.
- `--check`: report files that require formatting without modifying them (exits non-zero if unformatted files are found).
- `--engine-root <path>`: engine checkout location (defaults to repository root).

Commands:

```bash
# Check formatting across all owned engine C++ files in engrave/
python tools/format.py --check engrave/

# Format a specific engine C++ file
python tools/format.py engrave/src/api/sniff.cpp
```

### Linting (`tools/lint.py`)

`tools/lint.py` checks compiled owned engine translation units against `.clang-tidy` using `clang-tidy`. It requires `compile_commands.json` in the engine root (generate via `xmake project -k compile_commands`). Automatic fixing (`--fix`) is unsupported in engine mode because shared headers require coordinated changes.

Run `python tools/lint.py [paths...]` to lint compiled engine translation units matching the specified paths.

Supported options:

- `[paths...]`: files or directories to lint (defaults to `.`). Only compiled translation units matching owned engine C++ files are checked.
- `-j, --jobs <N>`: control linting parallelism (defaults to available CPU cores, capped at 12).
- `--engine-root <path>`: engine checkout location (defaults to repository root).

Commands:

```bash
# Lint compiled engine translation units matching a file
python tools/lint.py engrave/src/api/sniff.cpp

# Lint with explicit parallelism
python tools/lint.py -j 4 engrave/src/api/sniff.cpp
```

You can also run `python tools/setup_hooks.py` to install a Git pre-commit hook (`.git/hooks/pre-commit`) that validates staged engine C++ blobs against `.clang-format` before commit without modifying the index.

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

Example from `engrave/src/api/sniff.cpp`:

```cpp
#include "sniff.h"

#include <algorithm>
#include <cctype>
#include <string_view>

#include <nlohmann/json.hpp>

#include "global/io/buffer.h"
#include "global/serialization/xmlstreamreader.h"
#include "global/serialization/zipreader.h"
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
