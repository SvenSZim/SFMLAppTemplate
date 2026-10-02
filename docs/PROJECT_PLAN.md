# SFMLAppTemplate — Project Plan

Last updated: 2026-10-02 · Branch: `static-rework`

This file is the single place where the vision, the decisions and the roadmap are tracked.
Every decision gets an entry in the [decision log](#3-decision-log). Nothing counts as decided until it is listed there as **Accepted**.

The target code structure is described in [ARCHITECTURE.md](ARCHITECTURE.md). Code conventions are in [CODE_STYLE.md](CODE_STYLE.md), the workflow in [CONTRIBUTING.md](../CONTRIBUTING.md).

---

## 1. Vision

A reusable template for SFML applications that show a simulation or an algorithm at work
(AI training, pathfinding, raytracing, particle systems). Look and scope are inspired by
JohnBuffer's Ant Simulator 2 and its `peztool` library, but with a much simpler setup and a
simulation that can run on its own thread.

The app's own rendering (the "world") is the main thing on screen. The UI frames or overlays it.

## 2. Requirements

| # | Requirement | Meaning |
|---|---|---|
| R1 | Simulation / showcase template | A world viewport with pan and zoom is a first-class part of the window. |
| R2 | Easy to set up | A new app is a few dozen lines of declarative setup, not a class per panel. |
| R3 | Light-weight | **The UI uses no more CPU, GPU or memory than necessary.** This is about runtime cost, not codebase size. |
| R4 | Clean and polished | Rounded cards, shadows, anti-aliasing, hover and press animations, consistent spacing and fonts. |
| R5 | Panels easy to place | Placing a panel in the window is one line of setup. |
| R6 | Widget pool linked to app data | A widget reads or writes app data without glue code. |
| R7 | Input handled and forwarded | The UI consumes its own input and forwards the rest, and its results, to the app. Safe across threads. |
| R8 | Common utilities | Thread pool, RNG, timing, viewport and similar helpers. |

---

## 3. Decision log

Status values: **Accepted** (decided), **Proposed** (suggested, waiting for a decision), **Done** (implemented). IDs are stable: a P entry keeps its ID when it is accepted.

| ID | Date | Decision | Status |
|---|---|---|---|
| D1 | 2026-10-01 | Responsibilities must be unambiguous: exactly one owner per concern (rendering, styling, layout, input, storage). See [section 4](#4-target-responsibilities). | Accepted |
| D2 | 2026-10-01 | "Light-weight" means low runtime resource use (R3), not small code size. | Accepted |
| D3 | 2026-10-01 | The per-widget render cache is dropped. A polished UI animates the widgets in use, so that cache is dirty exactly when it would matter, and it never reduced draw calls. | Accepted |
| D4 | 2026-10-01 | Render caching happens at three levels: frame, panel, text. See [section 5](#5-render-performance). | Accepted |
| D5 | 2026-10-01 | Geometry is batched per panel: one vertex array for shapes, plus text. | Accepted |
| D6 | 2026-10-01 | A profiler readout (UI build time, draw-call count, frame time) is built early, so estimates are replaced by measurements. | Accepted |
| D7 | 2026-10-01 | No static/dynamic layer split inside a panel, and no render-to-texture per panel by default. Render-to-texture stays an opt-in for expensive content. | Accepted |
| D8 | 2026-10-01 | C++20 (designated initializers in the setup API). | Done |
| D9 | 2026-10-01 | SFML Audio and Network modules are not built; only Graphics, Window and System are used. | Done |
| P1 | 2026-10-01 | Widget model: one small interface per widget type (`measure`, `handleInput`, `update`, `paint`). Widgets emit primitives into a draw list; they own neither position, colours nor GL calls. Replaces per-type vectors. Apps can define their own widget types. | Accepted |
| P2 | 2026-10-01 | Data binding by name: `ui.widget("Speed").bind(params.speed)` returns a light handle, binds or rebinds at any time. The app never stores widget handles. The descriptor may also take the reference directly as a shorthand: `Widget::Slider("Speed", params.speed, 0, 10)`. Replaces per-type getters. | Accepted (revised) |
| P3 | 2026-10-01 | Threading: main thread owns window, UI and all drawing. The simulation runs on its own thread. Three channels: `Param<T>` values (both directions), a command queue (app → sim, see D11), snapshots (sim → UI). | Accepted |
| P4 | 2026-10-01 | One uniform event stream from the UI to the app. Events raised by widgets and input the UI did not consume (mouse, keyboard in the world area) use the same interface. The world viewport is a layout region. | Accepted (revised) |
| P5 | 2026-10-01 | Theme starts as one small token set (palette, radii, spacing, font sizes, shadow); hover and pressed colours are derived by rule. It must stay extensible: widgets ask the theme for a resolved style per element kind and state and never read tokens directly, so per-container and per-widget styling can be added later without touching widgets. | Accepted (revised) |
| P6 | 2026-10-01 | Three library layers: `core/` (threading, RNG, time, math), `app/` (window, sim runner, viewport, resources), `ui/`. | Accepted |
| P7 | 2026-10-01 | `ContainerState` describes interaction only (collapsed/expanded, hovered). Visibility is a separate layout output. | Accepted |
| P8 | 2026-10-01 | The thread pool leaves one core free for the main thread. | Accepted |
| D10 | 2026-10-01 | Widgets can only be bound to `Param<T>`, a small thread-safe wrapper that reads and writes like a plain value. Plain variables cannot be bound, so an unsafe binding does not compile. | Accepted |
| D11 | 2026-10-01 | The app receives all UI events on the main thread and forwards to the simulation what it needs. The template provides the thread-safe command queue for that forwarding; the app decides what goes in. | Accepted |
| D12 | 2026-10-01 | The app renders into views. A view is either the background (whole window, behind the panels) or a view widget inside a panel. Any number of views can exist at once, for example a main view plus a minimap. (Was Q3.) | Accepted |
| D13 | 2026-10-01 | Names: panel names are unique; widget names are unique per panel. Lookup is `"Speed"` when unique in the whole UI, otherwise `"Controls/Speed"`. Duplicates and unknown names fail loudly. (Was Q8.) | Accepted |
| D14 | 2026-10-01 | The whole codebase and its structure are reworked, not migrated. The detailed structure in [ARCHITECTURE.md](ARCHITECTURE.md) must be approved before the rework starts. (Was Q4.) Structure approved 2026-10-01. | Accepted |
| D15 | 2026-10-01 | The template only forwards user input as events; the app decides what to do with it. Pan and zoom for a view is an optional helper, not built-in behaviour. (Was Q9.) | Accepted |
| D16 | 2026-10-01 | Linux/Unix must work. (Was Q6.) | Accepted |
| D17 | 2026-10-01 | "Container" is renamed to "Panel". (Was Q11.) | Accepted |
| P9 | 2026-10-01 | Stay portable at low cost: only the C++ standard library and SFML, no platform-specific APIs. A Windows (MSVC) build in CI is an optional, non-blocking check. | Accepted |
| P10 | 2026-10-01 | Bindings are generic. Each widget type states the kind of value it works with (bool, number, index, text, series). A widget binds to a small interface for that kind (get, set, change counter). The template ships thread-safe implementations: `Param<T>` for single values (any number type, any enum, strings) and `Series` for graph data. Apps may implement the interface for their own data types, or build one from a getter and setter; thread safety is then theirs. Refines D10: plain variables still cannot be bound directly. | Accepted |
| P11 | 2026-10-02 | Painting and styling. `paint` returns nothing; it calls the `Painter`, which appends to the panel's draw list. A widget declares its parts and where they are; it never chooses a shape style. Styles come from the theme in three layers, later overriding earlier: (1) tokens, global values; (2) role defaults: each part declares a role (surface, track, accent, handle, text, line) and whether it is shown by default, and the theme derives its style from role and tokens; (3) part entries: specific settings for one part, e.g. `theme[Slider::Ticks].shown = true`. Kinds and parts are open identifiers declared by the widget, so app-defined widgets use the same mechanism. Corner radius 0 gives sharp rectangles. What a widget is (range, step count, options) stays in its descriptor. Refines P5. | Accepted |
| P12 | 2026-10-01 | In-panel layout belongs to `layout/`. A widget only reports the size it wants (`measure`); layout assigns its rectangle. Inside that rectangle the widget arranges its own parts. Size tokens (padding, gaps, row heights, font sizes) are a separate `Metrics` set next to the theme's colours: layout and `measure` read them, nothing else defines sizes. | Accepted |
| D18 | 2026-10-01 | Root namespace and target prefix: `atpl` (app template). (Was Q10.) | Accepted |
| D19 | 2026-10-02 | License: MIT. Bundled third-party material is listed in `THIRD_PARTY.md` with its own license. | Accepted |
| D20 | 2026-10-02 | Line endings: LF everywhere, enforced by `.gitattributes`. Existing files are converted in one separate commit. | Accepted |
| D21 | 2026-10-02 | Private and protected members are named `m_camelCase`. | Accepted |
| D22 | 2026-10-02 | Headers use `#pragma once`. | Accepted |
| P13 | 2026-10-02 | Workflow as written in `CONTRIBUTING.md`: one branch per issue (`wp/<id>-<name>`), commit format `[area] summary`, one pull request per issue into `main`, squash merge, definition of done, decisions recorded in the same pull request. | Accepted |
| P14 | 2026-10-02 | Remaining code conventions as written in `docs/CODE_STYLE.md`: formatting by `.clang-format` (4 spaces, 120 columns), naming table, include rules, error rules (setup fails loudly, per-frame code does not throw), per-frame performance rules, test rules, CMake rules (explicit source lists), internal code of a `ui` module in namespace `atpl::<module>`. | Accepted |
| D23 | 2026-10-02 | Core API as declared in `include/atpl/core/` (WP 1.1). `Param<T>`: its revision grows only when the value changes; it cannot be copied or moved; `a = b` copies the value; no compound operators. `Series`: `float` samples only. `Queue<T>`: unbounded, with `drain` and `waitDrain`. `Snapshot<T>`: one writer thread, one reader thread, three reused buffers; the writer writes the whole state each time. Change counters are called `revision`. Value access is named `get()` / `set()` everywhere, including `Interpolated`. | Accepted |
| D24 | 2026-10-02 | Each widget has one public type, its descriptor: `atpl::Slider("Speed", params.speed, {.min = 0, .max = 10})`. It also carries the widget's parts for theming (`Slider::Ticks`). `Widget` is the interface widget types implement; the implementing classes are internal. Replaces the `Widget::Slider(...)` spelling of earlier sketches. | Accepted |
| D25 | 2026-10-02 | Placement is stated per panel: an `Anchor` (floating at a window edge or corner) or a `GridCell` (cells of a window-wide grid). Both can be mixed in one UI. There is no UI-wide layout mode. | Accepted |
| D26 | 2026-10-02 | The window is created and owned by the app layer; the UI is constructed with a reference to it. Window settings belong to the app layer. | Accepted |

### Open questions

| ID | Question | Blocks |
|---|---|---|
| Q5 | Which utilities are in the first version? | Phase 5 |
| Q7 | Text: keep `sf::Text` objects per widget, or build glyph quads into the batch (fewer draw calls, more code)? | Phase 2 |

Closed: Q1 (by P1), Q2 (by P3: the simulation thread is the template's model), Q3 (D12), Q4 (D14), Q6 (D16), Q8 (D13), Q9 (D15), Q10 (D18), Q11 (D17), Q12 (P11).

---

## 4. Target responsibilities

One owner per concern (D1). The "must not" column is as binding as the "owns" column.

| Owner | Owns | Must not |
|---|---|---|
| `App` | The window, the app loop, receiving UI events on the main thread and forwarding to the simulation (D11, D26) | Touch UI internals |
| `UI` (facade) | Calling the systems in order, public API. Uses the window it is given (D26) | Contain the logic of any single concern, create or own the window |
| Storage (`ContainerManager`, widget storage) | Creation, lifetime, lookup | Lay out, draw, handle input |
| `InputSystem` | Hit-testing, hover/press/focus state, deciding what the UI consumes, emitting widget events and unconsumed input into the one event stream (P4) | Set geometry, write app data other than through bindings |
| `LayoutManager` | Every rectangle and size, visibility, widget packing inside panels, view regions; reads `Metrics` and each widget's `measure` (P12) | Touch interaction state or colours |
| `Theme` | Tokens, role defaults and part entries; resolving the style for a part and state (P5, P11). Holds the `Metrics` tokens (padding, gaps, row heights, font sizes) as values (P12) | Decide any rectangle or position |
| `Renderer` | Draw lists, batching, draw order, the caches of section 5 | Modify model data |
| Widget type | Its own behaviour: measure, react to input, arrange its parts inside its rectangle, emit primitives | Position itself, pick theme values by hand, issue draw calls |

---

## 5. Render performance

Goal (R3): the UI costs as little as possible, and nothing at all while it is idle.

### 5.1 Where the cost is

Order-of-magnitude **estimates** for about 100 widgets. They are not measurements; D6 exists to replace them.

| Cost | Estimate per frame | Answer |
|---|---|---|
| Rendering frames nobody needs | a full core plus GPU time | Level 1 |
| Draw calls, 300–500 if unbatched | 0.5–2 ms | Batching (D5) |
| Text layout | 50–100 µs if rebuilt each frame | Level 3 |
| Shape geometry | 20–50 µs | Level 2 |

### 5.2 The three cache levels (D4)

**Level 1 — Frame.** One global "needs redraw" flag. A frame is drawn only if at least one of these is true:
- a new simulation snapshot was published,
- a panel is dirty,
- an animation is running,
- the window was resized or exposed.

Otherwise there is no clear, no draw and no display. Because a skipped frame does not block on vsync, the loop must sleep itself (`waitEvent` with a timeout, or an explicit sleep).

**Level 2 — Panel.** Each panel owns one batched vertex array, its text and one dirty flag.
- A widget that changes marks its panel dirty. That is the only way a panel becomes dirty.
- Only dirty panels are rebuilt. Clean panels are drawn from their existing batch.
- Geometry is stored in panel-local coordinates and placed with a transform. Moving a panel does not rebuild it; changing its size does.
- Overlays (open dropdown list, tooltips) go into one extra top layer with the same rules.

**Level 3 — Text.** Each widget keeps its text geometry and rebuilds it only when the string, font or size changes. Live values (FPS, counters) refresh at 5–10 Hz, not every frame.

### 5.3 Supporting rules

- Draw order is explicit: panels in a fixed order, then the overlay layer. No hash-map iteration.
- Vsync (or a frame limit) is on by default.
- Anti-aliasing is requested when the window is created.
- Shadows are pre-built geometry or a small texture, not a runtime blur.
- The thread pool leaves one core for the main thread (P8).

### 5.4 Not built (D7)

- Per-widget caches.
- A static/dynamic split inside a panel.
- Render-to-texture per panel as the default.

### 5.5 Targets to verify with the profiler

These are goals, checked in Phase 2:
- Idle UI and no new snapshot: no frames rendered.
- At most 2 draw calls per panel, plus text.
- UI build and submit under 0.2 ms per frame at 100 widgets.

---

## 6. Roadmap

Work is tracked on GitHub:
- Board: https://github.com/users/SvenSZim/projects/3 (columns: Backlog, Ready, In progress, Done)
- 50 work packages as issues, titled `WP <phase>.<n>`, one milestone per phase, labels per area.
- Each issue has its goal, scope, done-when conditions, dependencies and the decisions it rests on.

The phases below are the summary; the issues are the source of truth for scope and status.

Each phase ends with something that runs.

### Phase 0 — New skeleton (done 2026-10-02)
- Create the directory tree and the three library targets from ARCHITECTURE.md.
- Move the kept code (easing, interpolated, rect, widget packing) and its tests.
- Remove the old UI code (it stays on `main` and in git history).
- `minimal` example opens an empty window with vsync on.
- Copy `resources/` next to the binary; fail loudly if the font cannot be loaded.
- CI checks the format, builds with GCC and with Clang plus sanitizers, and runs the tests on Linux; a Windows (MSVC) job runs as a non-blocking check (P9).

**Done when:** build and tests are green locally and in CI, and the `minimal` example runs.

### Phase 1 — App-facing API
- Write the public headers only: setup, widget descriptors, widget, view and panel handles (P2, D12), `Param<T>` (D10), the event type and stream (P4, D15), the command queue (D11), simulation hooks.
- Write the example `main.cpp` against them.

**Done when:** the example compiles against the headers and the API is agreed. Everything below is built to fit it.

### Phase 2 — Render pipeline
- Draw list and primitives: styled box (fill, border, radius, shadow), line, text.
- Theme core: tokens, role defaults, part entries (P11).
- Per-panel batch and dirty flag (Level 2), kept text (Level 3), frame skipping (Level 1).
- Explicit draw order and overlay layer.
- Profiler readout (D6).
- Resolve Q7.

**Done when:** the targets in 5.5 are measured and recorded here.

### Phase 3 — Widgets and input
- Widget model (P1) and `InputSystem`: hit-testing, hover, press, capture during drag, keyboard focus.
- Wire `packWidgets` into layout.
- Widget pool: button, switch, slider, text display, progress bar, graph, dropdown, text input.
- `Param<T>` and `Series` implementations (D10, P10).
- Data bindings by name and in the descriptor (P2).

**Done when:** every widget in the demo is interactive and bound to app data.

### Phase 4 — Simulation layer and threading
- Simulation runner: fixed timestep, pause, single step, speed.
- Command queue and snapshot exchange (P3, D11).
- Background view and view widgets (D12); unconsumed input arrives through the event stream (P4).
- Optional pan/zoom helper for a view (D15).

**Done when:** the demo simulation runs on its own thread and the UI stays responsive while one tick takes a second.

### Phase 5 — Core utilities
- Thread pool with parallel-for (P8), per-thread RNG, timers, grid, config loader, resource store, quad batch for world rendering.
- Scope fixed by Q5.

### Phase 6 — Polish
- Theme tokens behind the style-resolving interface (P5), second theme.
- Hover, press and toggle animations; panel collapse animation.
- Shadows, GUI scale.

### Phase 7 — Packaging
- README quick start.
- Two example apps (for instance a pathfinding grid and a particle simulation).
- CI for every supported platform.

---

## 7. Current state (2026-10-02)

- **Phase 0 is done.** The new structure is in place: targets `atpl_core`, `atpl_ui`, `atpl_app`; easing and animated values in `core`; rect and widget packing in `ui`; resources next to the executable in `app`; the `minimal` example; 43 tests.
- CI on every pull request: format check, Linux with GCC (release, all tests including the window smoke test on a virtual display), Linux with Clang (debug, AddressSanitizer and UndefinedBehaviorSanitizer), Windows with MSVC (informational).
- No old code is left in the tree.
- Phase 1 in progress, the app-facing API. WP 1.1 (core API headers: `Param`, `Series`, `Queue`, `Snapshot`) is done and agreed (D23). WP 1.2 (setup, descriptors, handles, bindings, the `UI` facade) is written and waits for review. WP 2.1 (shape tessellation) can be started independently.
- Open questions: Q5 (utility scope, Phase 5) and Q7 (text drawing, Phase 2). No proposals are pending.
- A ThreadSanitizer job is added to CI with the first code that is shared between threads (WP 3.7).
