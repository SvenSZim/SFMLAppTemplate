# Code Style

These rules keep the codebase uniform. Formatting is done by tools; the rest is checked in review.
Where the architecture is concerned, [ARCHITECTURE.md](ARCHITECTURE.md) is the authority.

## 1. Formatting

Formatting is not a matter of taste here: run the formatter and move on.

- `.clang-format` at the repository root defines the C++ format. Run `tools/format.sh` before committing; `tools/format.sh --check` only reports. CI runs the check.
- CI uses clang-format 23.1.2 (`pipx install clang-format==23.1.2`). Other versions can format slightly differently; the CI result counts.
- `.editorconfig` covers everything else: UTF-8, LF line endings, final newline, no trailing whitespace.
- 4 spaces for C++, 2 spaces for CMake, YAML and Markdown. No tabs.
- 120 columns.
- Braces open on the same line. Namespaces are not indented.
- Plain braced lists have a space inside the braces: `{ 1280, 720 }`. Lists with designators are written without: `{.min = 0, .max = 10}`. clang-format enforces the first and leaves the second as written, so that one is on the author.

## 2. Naming

| Thing | Style | Example |
|---|---|---|
| Namespace | lowercase | `atpl`; internal code of a `ui` module in `atpl::<module>`, e.g. `atpl::layout` |
| Type (class, struct, enum, alias) | `PascalCase` | `PanelBatch`, `WidgetHandle` |
| Function, method | `camelCase` | `markDirty()`, `parallelFor()` |
| Local variable, parameter | `camelCase` | `panelRect` |
| Private or protected member | `m_camelCase` | `m_eventBuffer`, `m_nextId` |
| Public data member of a plain struct | `camelCase`, no prefix | `setup.windowSize` |
| Enumerator | `PascalCase` | `Role::Accent`, `Anchor::TopLeft` |
| Part of a widget (a `static constexpr Part`) | `PascalCase`, like an enumerator | `Slider::Ticks`, `Panel::Header` |
| Constant (`constexpr`, `static const`) | `camelCase` | `defaultPadding` |
| Template parameter | `PascalCase`, `T` for a single one | `template <typename T>` |
| Macro (avoid) | `ATPL_UPPER_CASE` | `ATPL_ASSERT` |
| File | `snake_case` | `panel_batch.hpp`, `panel_batch.cpp` |
| CMake target | `atpl_` prefix | `atpl_core`, `atpl_ui` |
| Test file | `test_<name>.cpp` | `tests/ui/test_packing.cpp` |

Names say what a thing is, not how it is implemented. No abbreviations beyond the common ones (`id`, `ui`, `dt`, `rect`).

## 3. Files and includes

- Headers start with `#pragma once`.
- One main type per file; the file is named after it.
- Public headers live in `include/atpl/<layer>/`. Everything else, including internal headers, lives in `src/`.
- Include project headers by their full path, never with `../`. Public headers from the include root, internal headers from the `src` root:
  ```cpp
  #include "atpl/ui/theme.hpp"      // public
  #include "ui/layout/packing.hpp"  // internal
  ```
- Include order (the formatter sorts it): own header, project headers, SFML, other libraries, standard library.
- A header includes what it uses and nothing more. Prefer forward declarations in headers.
- No `using namespace` and no `using` declarations at namespace scope in headers.
- Source files are plain ASCII. Characters beyond it are written as escapes in code (`U"\u2026"`) and spelled out in comments. Not every compiler reads source files as UTF-8 by default; `tools/format.sh --check` reports violations.

## 4. Language rules

- C++20. Only the standard library and SFML; no platform-specific APIs (P9).
- `enum class` always; never plain `enum`.
- `struct` for plain data without invariants, `class` for everything that protects an invariant.
- `[[nodiscard]]` on functions whose result must not be ignored (getters, factories, lookups).
- `const` wherever it is true: parameters by `const&`, methods that do not modify, local values that do not change.
- Ownership is explicit: `std::unique_ptr` for owning, references or ids for non-owning. No owning raw pointers, no `new`/`delete`.
- Modules refer to each other's objects by id or handle, not by pointer (ARCHITECTURE.md §4.3).
- No global mutable state and no singletons.
- Prefer `std::span`, `std::string_view` and `std::optional` over pointer-and-size, `const char*` and sentinel values.
- No C-style casts.

## 5. Errors

- Mistakes in setup (duplicate or unknown names, binding the wrong kind, missing resources) fail loudly and immediately, with a message that names the offending item.
- Code that runs every frame does not throw.
- A lookup never returns "some other object" when the requested one is missing.

## 6. Architecture rules in code

These come from the plan and are checked in every review.

- **Layers** (ARCHITECTURE.md §1): `core` does not include SFML. `ui` does not include `app`. Only `app` starts threads. The test `structure.layering` (`tests/check_layering.cmake`) checks these and the window-access rule on every test run.
- **One owner per concern** (D1, ARCHITECTURE.md §4.2): before writing to a piece of state, check that your module owns it. Layout writes rectangles, input writes interaction state, render writes nothing in the model.
- **Window access**: only `ui/ui.cpp` and `ui/render/renderer.cpp` touch `sf::RenderWindow`.
- **Widgets** act on the outside world only through the context they are handed. They do not position themselves, read theme tokens directly or issue draw calls.
- **Threads** share state only through `Param<T>`, `Series`, the command queue and snapshots.

## 7. Performance rules

The UI should cost as little as possible and nothing while idle (R3, plan §5).

- No heap allocation in steady-state per-frame code. Reuse buffers; reserve up front.
- No work for things that did not change: go through the dirty flags, do not rebuild "to be safe".
- No lookups by name in per-frame code. Names are resolved to ids once.
- Measure before optimising. The profiler readout is the reference, not intuition.

## 8. Comments and documentation

- Comments explain why, not what. Code that needs a "what" comment should be renamed or split.
- Every type and function in a public header has a short `///` comment: what it is for, and anything surprising (thread safety, ownership, cost).
- No commented-out code. No `TODO` without an issue number: `// TODO(#23): ...`.

## 9. Tests

- Catch2, one test file per source file, in `tests/<layer>/`.
- Everything that does not need a window is tested without one. That is all of `core` and, in `ui`, the model, layout, input, theme and draw-list generation.
- Code shared between threads has a concurrent test, run under ThreadSanitizer (`-DATPL_SANITIZE=thread`).
- Tests that need a display, because they open a window or draw off-screen and check pixels, go into their layer's `display` test executable (`tests/<layer>/display/`) and carry the CTest label `display`. Leave them out with `ctest -LE display`, or run them without a screen under `xvfb-run`.
- A bug fix comes with a test that fails without the fix.
- Every public header must compile on its own; the build checks this for all headers under `include/atpl/`.
- `tests/api/` holds usage examples of the public API that are compiled with every build. When the API changes, they change with it.

## 10. CMake

- Lowercase commands, 2 spaces.
- Properties are set per target (`target_*`), never globally.
- Sources are listed explicitly per target; no `file(GLOB)`.
