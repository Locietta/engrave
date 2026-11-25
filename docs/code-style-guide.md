# Code Style Guide

This is a concise code style guide for the `engrave` engine fork. Contributors and agents should follow these guidelines unless there is a compelling reason to deviate.

If not specified here, follow the style of modern refactored code nearby. Build, formatting, linting, and test commands live in [README.md](../README.md), not in this guide.

## Naming

Enforced by [.clang-tidy](../.clang-tidy):

- Types (`struct`, `enum`, `union`): `CamelCase`.
- Functions, methods, parameters, variables, and struct fields: `lower_case` (no `m_` prefix; avoid trailing `_` on public/plain data fields, while trailing `_` is permitted for private implementation fields per `.clang-tidy`).
- Enum constants: `UPPER_CASE`.
- Short uppercase template parameters or domain acronyms (`T`, `ID`) are fine.

## Basic Types

Prefer concise fixed-width and pointer-sized aliases in project code:

```cpp
using i8 = std::int8_t;
using i16 = std::int16_t;
using i32 = std::int32_t;
using i64 = std::int64_t;
using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;
using isize = std::ptrdiff_t;
using usize = std::size_t;
using f32 = float;
using f64 = double;
```

Use `std::string`, `std::string_view`, and `std::span` for text and contiguous slices. External public library headers under `engrave/include/engrave/` stay self-contained on standard C++ headers unless those aliases are exported in the `engrave` namespace.

## Structs and Data Layout

Always use `struct`, do not use `class` for custom types. This avoids `public:` boilerplate in struct definitions and inheritance lists.

- **Separate data from behavior**: Keep data as plain `struct` definitions with free functions or focused systems rather than monolithic stateful OOP hierarchies.
- **Owned value semantics**: Value types own their data. Do not add copy-on-write wrappers, reference-counted buffers, or `std::shared_ptr` ownership graphs where single ownership or flat storage works. Avoid copies on hot paths by passing views (`std::string_view`, `std::span`) or references.
- **Data-oriented layout**: Prefer contiguous flat arrays (`amc::vector`, `std::vector`) and integer indices/offsets over pointer-chasing node graphs.

## Polymorphism and Callables

Never introduce `virtual` functions, abstract base classes, or vtable hierarchies.

- **Multi-method value-semantic interfaces via `ngcpp-proxy`**: When runtime polymorphism across multiple methods is genuinely needed, use `pro::proxy` (owning) or `pro::proxy_view` (non-owning) from `<proxy/proxy.h>` instead of virtual interfaces. `proxy` keeps structs plain data without interfering with their memory layout.
- **Single-signature callables**:
  - **Non-storing inline callbacks**: When a callback does not cross a compilation boundary and is not stored, prefer a template parameter (`template <class Fn>`).
  - **Non-owning synchronous callbacks**: Use `muse::FnRef<Sig>` (`"global/fn.h"`) — a 16-byte trivially copyable view with zero heap allocation and no `<proxy/proxy.h>` include.
  - **Owning stored callbacks**: Use `muse::UniqueFn<Sig>` (`"global/unique_fn.h"`) instead of `std::function`. Public `engrave/include/engrave/` headers stay on standard C++ (`std::function`) so they carry no third-party header dependencies.
- **Closed sets**: When the set of alternatives is closed and known statically, use `std::variant` or enum dispatch.

## Containers and JSON

- **No node-based associative containers**: Do not use `std::map`, `std::unordered_map`, `std::set`, `std::unordered_set`, `std::multimap`, or `std::multiset`. Use the thin internal vocabulary headers under `muse/framework/global/` instead of including third-party container headers directly:
  - `muse::HashMap`, `muse::HashSet`, `muse::SegmentedHashMap`, `muse::SegmentedHashSet`, and `muse::Hash` (`"global/hashmap.h"`, backed by `ankerl::unordered_dense`) for lookup-only data (`std::string` keys support transparent `std::string_view` lookup; use `Segmented*` when references or pointers must survive insertion).
  - `muse::BTreeMap`, `muse::BTreeSet`, `muse::BTreeMultiMap`, `muse::BTreeMultiSet` (`"global/btree.h"`, backed by `gtl::btree_*` with `gtl::Less<Key>` default comparator for transparent lookup and three-way comparisons) for large or dynamically mutated ordered containers (`BTreeMap` provides aliasing-safe `insert_or_assign`).
  - `muse::FlatMap` (`"global/flatmap.h"`) and `muse::FlatSet` (`"global/flatset.h"`, backed by `amc::FlatSet`) for small or monotonically built ordered data.
- **JSON via typed `glaze` structs**: Define plain C++ structs and read/write JSON directly with `glaze` compile-time reflection rather than building or walking untyped dynamic DOM maps. Public `engrave/include/engrave/` headers must not expose any JSON library.
- **No Boost**: Do not introduce Boost dependencies.

## Headers

Use `#pragma once` (never `#ifndef` include guards). Group includes in the following order, separated by blank lines:

```cpp
// 1. Interface header of the current .cpp file
#include "sniff.h"

// 2. Standard library headers
#include <algorithm>
#include <cctype>
#include <string_view>

// 3. Third-party headers
#include <glaze/glaze.hpp>
#include <proxy/proxy.h>

// 4. Project headers (muse/..., engraving/..., engrave/...)
#include "global/hashmap.h"
#include "global/io/buffer.h"
#include "global/serialization/xmlstreamreader.h"
```

## Unused Values, Assertions, and Error Handling

- **Unused values**: Mark intentionally unused parameters/variables with `[[maybe_unused]]`. Discard `[[nodiscard]]` return values explicitly with `std::ignore = expr;`, never `(void)` casts.
- **Assertions and panic**: Use `<cassert>` `assert(...)` for internal preconditions checked in debug builds, `std::unreachable()` for impossible branches, and small local `check(condition, message)` / `panic(message)` helpers for invariants that must stay checked in release builds.
- **Error handling**: Distinguish the 3 kinds of failures by asking who can act on the failure:
  - **Expected failures** (invalid arguments, malformed score/JSON input, unknown IDs, resource limits): the immediate caller acts on it — return `std::expected` (`Result<T>`), never throw.
  - **Rare failures** (unexpected OS failures or internal store limits whose only sensible handler is a distant recovery boundary): throw a runtime exception.
  - **Fatal failures** (owner-thread or global-invariant corruption): no caller can recover — terminate or panic.

## Redundancy and Comments

- **No redundant or dead paths**: Every branch must be reachable. When replacing an old path, migrate all callers and delete the old path in the same change.
- **Comments**: Default to self-explanatory names and zero tautological comments. Add a comment only to explain a non-obvious _why_, an un-enforced invariant, or an external platform/library quirk. Do not add section-banner comments or comments that narrate git history.

## Python Test Boundaries

- Shared test helpers live only in the `engrave_testing` package; executable scripts under `tests/integration/` never import other `test_*` scripts.

## Concurrency in Tests

Tests for concurrent code must be deterministic and coordinate via observable state transitions (events, barriers, condition variables, atomics, futures), never `sleep` or wall-clock timing assumptions.

## Commit Messages

Format commit headlines as `<type>(<scope>): <subject>` (omitting type or scope when not applicable), with an optional bulleted body separated by a blank line:

```text
refactor(render): align cairo painter pen transform with upstream geometry

- Updates pen scaling logic to account for current staff magnification.
- Normalizes point coordinate transformations before rasterization.
- Verifies visual output against golden rendering baselines.
```

## Scope and Upstream Code

Untouched upstream MuseScore code under `src/` and `muse/` retains its existing classes, virtual hierarchies, and conventions to keep cherry-picks clean. Dependency-injection interfaces with one production implementation are removed when touched, and any newly written or refactored subsystem (such as `engrave/` and the flat playback/audio pipeline in [playback.md](playback.md)) follows all rules above. New `EngravingObject` subclasses under `src/engraving/` must keep `DECLARE_CLASSOF` / `item_type_allows` in sync with `concrete_type()` so `item_cast` dispatch stays sound.
