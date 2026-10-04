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
| D27 | 2026-10-02 | UI setup API as declared in `include/atpl/ui/` (WP 1.2). Options are structs with designated initializers; a widget's label defaults to its name. Numbers travel as `double` (exact up to 2^53; larger 64-bit values are bound as text), enums as the enumerator's index; writing back rounds and clamps. A descriptor rejects a wrong kind at compile time, binding by name at run time with `SetupError`. `requestRedraw()` exists on `UI` only. A view draws through `void(sf::RenderTarget&, sf::Vector2f size)`. Eight anchors; equal window grid cells. Widgets inside a panel: packed automatically into `PanelSetup::columns` balanced columns (default 1) when no widget has a position; as soon as one has a `GridCell` (through `at(cell, widget)`), the panel is a grid and widgets without a position take the first free cell, row by row, without a span. That one position changes the placement of the others is intended. | Accepted |
| D28 | 2026-10-02 | Overflow rules, for a start: panel content scrolls vertically inside the panel; a stack of floating panels is fitted to the window by letting expanded panels share the height and scroll, hiding the last panels only if not even headers fit; floating panel width is clamped to the window; text that does not fit ends in an ellipsis; a dropdown list stays inside the window; no horizontal scrolling; the app may set a minimum window size. See ARCHITECTURE.md §4.6b. To be revisited if they do not work in practice. | Accepted |
| D29 | 2026-10-02 | Events API as declared in `atpl/ui/event.hpp` (WP 1.3). The template declares its own event types; `sf::Event` does not appear, but keys and mouse buttons use SFML's enums. Twelve types for a start (widget, pointer, keyboard, window); the list is open and can grow. The app reads `UI::events()` after `handleInput()`. `ValueChanged` carries the new value and `final`, and is raised for bound widgets too. Pointer and key events carry the pointer's location including the view under it. The UI keeps pointer input over panels and keyboard input while a widget has focus; a press and everything up to its release go to the same place, so input is never cut off mid-interaction. Window close is only reported. | Accepted |
| D30 | 2026-10-02 | Widget, painter and theme API as declared in `atpl/ui/widget.hpp` and `theme.hpp` (WP 1.4). A widget never sees its binding: it keeps its own value, is handed new ones through `setValue`, and reports user changes through its context. `handleInput` gets the same `Event` types as the application. `measure` answers with a height for the offered width. Hidden parts are skipped by the painter. The UI tracks hover, press, focus and disabled; widgets add `Active`. Nine roles: five for shapes, and four text types (`Title`, `Heading`, `Text`, `MutedText`). Tokens in four groups: palette, shape, metrics, typography (a size and a font per text type). Part entries set only the named fields, including a font. The theme is part of `UISetup` and replaceable through `UI::setTheme`. New widget `Paragraph` with optional heading, body and footer. | Accepted |
| D31 | 2026-10-02 | A seventh internal module, `ui/binding/`, owns bindings: the adapters and the syncing in both directions. The binding itself is a field of the widget's slot; there is no separate table. Refines the structure of D14. | Accepted |
| D32 | 2026-10-02 | App and simulation API as declared in `include/atpl/app/` (WP 1.5). A simulation derives from `Simulation<State, Command>` and implements `tick`, `onCommand` and `writeState`; the base class owns the command queue and the state exchange, and `App::run(simulation)` runs it on its own thread. Its controls (`paused`, `speed`, `unlimited`, `tickRate`, and measurements) are `Param`s that widgets bind to; `step()` runs single ticks. The step size is fixed at `1 / tickRate`. Commands are handled while paused. `writeState` runs at most once per drawn frame, and once more on pause or stop. An exception in the simulation is rethrown on the main thread. `App` owns the window (`WindowSetup`, including a minimum size), quits on window close by default, and offers `onEvent` and `onUpdate`. `Camera` is an optional pan-and-zoom helper for one view. | Accepted |
| D33 | 2026-10-02 | Results of the API walk-through (WP 1.6). `Simulation::state()` returns the same state for a whole pass of the application's loop: the newest one published when the pass began, so all views of a frame show the same moment. `Camera::visibleArea()` is added. `examples/starter` is the reference application for the API; `examples/minimal` stays the runnable smoke test until the API is implemented. With this, the public API of Phase 1 is signed off. | Accepted |
| D34 | 2026-10-02 | Theme colours and look (WP 2.2). A theme has one list of main colours and one list of accents. A panel chooses `main1` (background), `main2` (outlines) and `accent` by index (defaults 0, 1, 0); a widget can deviate with `colored(...)`; an index the theme lacks is a `SetupError`. Text, muted text, the areas of buttons and fields and the knob colour are derived from these three. Panels and everything that can be operated are outlined in main2; the outline turns to the accent on hover and use. Between outline and fill lies a gap, one value per theme, adjustable per part. Knobs have no outline and show hover and press in their fill. An accent can be a gradient; any part can be given one by a part entry. `Button::Face` has the role `Track`, `Panel::Scrollbar` the role `Line`. Built-in themes: `moon` (default; black, grey outlines, white gradient accents) and `colorful` (the ant simulator's warm palette with four accents). Both are tested for readable contrast. | Accepted |
| D35 | 2026-10-02 | Widget design requests, recorded in the packages that build them: an optional underline below the panel header (WP 3.6); optional separators between a paragraph's sections (WP 3.13); graphs that are zeroed or average-centred, linear or logarithmic (per graph), with an optional shadow towards the axis, current value, axis labels and grid (per theme), fed by a stream of values (x-axis as count or time) or of points (WP 3.10, WP 3.7, and a painter call in WP 2.3). The graph mock-up of that day is the reference. | Accepted |
| D36 | 2026-10-02 | Draw list and painter (WP 2.3). Within a panel, text is always drawn on top of shapes; what must cover text goes on the overlay layer. `Painter::area` fills between a curve and a line and always fades to nothing at the line. `Panel::Outline` is removed: a panel's outline is the border of `Panel::Background`. Kept in mind as optional extensions, not built now: outlines that fade across their width (a glow) or along the box (a gradient), recorded in WP 6.2; a flat area fill, should a widget such as a box plot need one. | Accepted |
| D37 | 2026-10-02 | Text is drawn with one SFML text object per text run, kept between frames (closes Q7). Chosen for its simplicity. Building all glyphs of a panel into one batch, which would cut text draw calls from one per run to one per text size, stays a possible extension; `TextRenderer` is the seam for it. Measured on the check scene of WP 2.5 (55 text runs, software and hardware as on the development machine): about 85 microseconds of CPU time per unchanged frame with text, about 27 without. Single-line text is centred by the font's capital height. The refresh limit for live values moves to the binding sync (WP 3.8), where values are read. | Accepted |
| D38 | 2026-10-02 | Performance targets (WP 2.7). The three targets of 5.5 count as met; the worst case measured, every panel painted anew every frame, lies at the limit and gets no follow-up, and glyph batching stays an extension, not a requirement (D37). The application shows the profiler readout with `UI::setProfilerVisible(bool)`, or from the start with `UISetup::profiler`; it is off by default and has no built-in key. | Accepted |
| D39 | 2026-10-02 | Model (WP 3.1). A descriptor's optional members are picked up by name, the same for built-in and app-defined widgets: `binding` (what the widget is bound to from the start) and `static constexpr bool isView = true` (the widget is a view, found with `UI::view(name)`). `WidgetSetup` hands them on through `binding()`, `isView()` and `create()`. A view widget is found both as a widget and as a view. Names must not be empty. | Accepted |
| D40 | 2026-10-02 | Panel placement (WP 3.3), filling in D28. A stack of floating panels that is too high: every panel keeps its header, and the height left is shared so that each panel gets as much as it needs before the larger ones share the rest. A window narrower than a floating panel and its margins: the margins shrink first, then the panel. The window's grid has the theme's `margin` around it and between its cells. Grid panels may share cells; the setup allows it, so an application can swap panels in one place with `setVisible`. | Accepted |
| D41 | 2026-10-02 | Facade (WP 3.2). The `minimal` example uses the UI from now on, driven by a hand-written loop until `App` exists; it replaces the SFML-only window. Until the input system exists (WP 3.5), `UI::events()` carries window events only: closed, resized, focus. The profiler readout sits in the top-right corner of the window. | Accepted |
| D42 | 2026-10-02 | A stack of floating panels that does not fit can overlap its panels like a stack of cards instead of hiding the last ones: the hovered panel comes to the front, a selected one expands and the others make room. Opt-in; hiding stays the default. Built as WP 3.14 (#75), after input and panel collapse. | Accepted |
| D43 | 2026-10-02 | The profiler readout sits in the bottom-right corner of the window. Replaces that part of D41. | Accepted |
| D44 | 2026-10-02 | Layout themes: a UI has a layout theme next to its theme, a set of defaults for sizes and positions that a panel can override. Sizes are decided top-down (the window's grid gives a panel its size, the panel's grid gives a widget its size) or bottom-up (widgets' minimum sizes give the panel's). Widgets state a minimum size, constant ones a maximum and ratio limits, dynamic ones none. Everything scales with the window within limits. `Metrics` moves from the theme to the layout theme. The full concept is in `docs/LAYOUT.md`; it replaces the grid half of D27, the widget and panel overflow rules of D28, and changes D30 and D34. Built in WP 3.15 to 3.17 (#77 to #79). | Accepted |
| D45 | 2026-10-02 | Layout theme defaults (WP 3.15). Scaling: reference window 1280 x 720, factors between 0.75 and 1.5, text following the smaller side at half strength; on by default. Padding and gaps scale separately across and down; margins are uniform and follow the smaller factor. A floating panel's limit: 480 x 900 pixels, or half the window's width and nine tenths of its height. The sizes in effect are `Sizes`, read by widgets through `sizes()`; `Metrics` are the values a layout theme sets. | Accepted |
| D46 | 2026-10-03 | Widget sizes (WP 3.16). A widget states a minimum (the least it needs to be displayable), a preferred size (its normal size; bottom-up sizes are derived from it) and, if it is constant, a maximum (what it makes use of when a grid has room to spare); dynamic widgets have no maximum. A grid short of room squeezes its rows from preferred down to minimum before its content scrolls; packed widgets are not squeezed. All three follow the window through the layout's sizes. A panel that names no rows and no positions gets as many rows as its widgets need, also with spans. A panel that fills cells of the window's grid lays out its content as a grid of equal rows; spans decide how its room is shared. | Accepted |
| D47 | 2026-10-03 | Panel sizes (WP 3.17). A panel sized by its content is as wide as the widest of what its widgets prefer, its title and the layout's `panelWidth`, which so acts as a minimum. `collapseTowards` applies to panels in the window's grid; floating panels collapse within their stack. Equal cells inside a panel are per panel by default; an option (`Layout::cells = Equal`, overridable per panel) gives all panels sized by their content one shared cell size, so equal grids make equal panels. | Accepted |
| D48 | 2026-10-03 | Input (WP 3.5). Panels are stacked with the window's grid panels below the floating ones, each in setup order; drawing and hit-testing use the same order. While a button is held, pointer input goes to the pressed widget without it having to capture the pointer; `capturePointer` stays for naming it explicitly, and capture ends with the release. A press anywhere but on the focused widget takes the focus away. A hover is forgotten when layout runs and found again with the next move. | Accepted |
| D49 | 2026-10-03 | Panel frame (WP 3.6). `Panel::Header` is not shown unless a theme wants it, so the panel's outline and shadow are not drawn twice. A collapsible panel shows an arrow (`Panel::Arrow`) that a theme can hide; under the pointer it takes the title's colour, since lines do not react to hover. Folding takes `Layout::foldSeconds` (default 0.18 s), eased. Event handling waits at most for the first event of a pass and takes the rest without waiting, and frames keep coming while something moves; a stream of pointer moves once held up the animation. `minimal` uses a stand-in widget until the built-in ones exist. | Accepted |
| D50 | 2026-10-03 | Thread-safe values (WP 3.7). `Param<T>` is lock-free for small trivially copyable types and uses a short lock for others; its revision grows only on a change, after the value is stored. `Series` and the new `PointSeries` (points `{x, y}` in core, for graphs whose x-axis comes from the data) use a short lock held only while copying; their revision is read without it. CI runs a blocking ThreadSanitizer job (GCC), which needs less address randomisation on the runner. | Accepted |
| D51 | 2026-10-03 | Bindings (WP 3.8). A widget hears of changes to its binding at most as often as its `Widget::refreshInterval` says: by default at once for widgets that edit their value or show a series, every 125 ms for values that are only shown; widget types can ask for faster or slower. `WidgetHandle::set` writes the bound value without raising `ValueChanged`. A `PointSeries` binds as a series; the graph tells points from samples through the series binding. | Accepted |
| D52 | 2026-10-03 | First widgets (WP 3.9). Button, Switch and Slider follow the design agreed in WP 2.2. A slider reports its value on every move while it is dragged (`final` false) and once more on release (`final` true). `InputContext::press` also sets a bound on/off value. State changes such as the switch's knob jump for now; animating them belongs to WP 6.1 (#43). | Accepted |
| D53 | 2026-10-03 | Display widgets (WP 3.10). TextDisplay, ProgressBar and Graph follow the design agreed in WP 2.2. A graph's optional parts (shadow, grid, axis, axis labels, current value) are theme parts, hidden by default; its options are what the data means (`base`, `logarithmic`, `x`, `samples`, `min`/`max`, `format`). The shadow and the grid are drawn fainter than their colour. The current value is the newest sample. A logarithmic axis spans whole powers of ten. The shadow and grid strengths are fixed for now, not theme values. Widgets that only show say so (`Widget::reactsToPointer` false): the pointer passes over them, and they never look hovered or pressed. | Accepted |
| D54 | 2026-10-03 | Overlay, Dropdown and TextInput (WP 3.11). A widget opens one overlay at a time through its context (`openOverlay`, `State::Open`) and paints it with `Widget::paintOverlay`; the UI attaches it to the widget's anchor (`Widget::overlayAnchor`), right below without a gap, right above if there is no room, else on the larger side with fewer entries and inside the window. An open dropdown list is one shape with its field: one outline in the open field's outline colour, shared where they meet. While it is open all pointer input goes to its widget; a press elsewhere closes it and is used up. A dropdown shows at most `maxVisible` entries (default 8), fewer if the room is smaller, and scrolls the rest with the wheel; keys move, choose and close; the wheel does nothing to a closed one. A text input writes its value on every change (`final` false) and once more with `final` true when the editing ends (`Widget::focusLost`); its cursor is steady; while editing, dots mark text scrolled out at the left (and text following at the right); when the editing ends it is shown from its start again, with dots at the right; it pastes with Ctrl+V; selection, copying and jumping by words are left to WP 6.1 (#43). `InputContext::textSize` measures text while handling input. | Accepted |
| D55 | 2026-10-03 | Panel scrolling (WP 3.12). In a panel whose content overflows, the wheel scrolls the panel and no widget in it gets the wheel; in one that does not, the widget under the pointer does. One notch scrolls the same everywhere: the layout's row height, or the lowest grid row on screen if lower. The scrollbar is a thin rounded thumb in the right padding, so no widget is narrowed; lighter under the pointer and while dragged; dragging scrolls, a press beside the thumb moves it there. Content is cut off a padding's height above the panel's bottom border, not at the border. Scrolling is immediate; smooth scrolling is for WP 6.1 (#43). The offset is kept and clamped when the panel's size changes, and kept while folded. A scroll forgets the hover until the next move, so it repaints at most once. | Accepted |
| D56 | 2026-10-03 | Paragraph (WP 3.13). A paragraph is static: its heading, body and footer are set in the setup, it takes no binding (unlike the issue's first scope), and nothing about it changes while running, so it never causes a layout pass. In general a widget's value changing never re-runs layout; text that changes belongs in widgets of a fixed size, planned as a log (a stream of text) and a text value. All three texts wrap, including the heading; empty ones take no room. It would like the width of its longest line, at most a panel of the layout's width, and needs its longest word. `ParagraphOptions::align` places all three. A theme can show `Paragraph::Separator` lines between the texts; the gap is there either way. The body is static text and looks quieter than the values of widgets, which change, and clearly apart from its heading: unless a theme sets them, its colour and its size lie between the heading's and the footer's (as the theme has them), 0.65 of the way towards the footer. The log and the text value are WP 3.18 (#93) and WP 3.19 (#94). | Accepted |
| D57 | 2026-10-03 | Card stack (WP 3.14), filling in D42. Opt-in through `Layout::stackOverflow`; off in all presets; `minimal --cards` switches it on. A stack that does not fit overlaps its collapsed panels only as far as needed, each down to a strip in which its title stays readable; expanded panels keep their height while there is room. Of two cards the one further from the anchor covers the other. The card under the pointer comes to the front and goes back when the pointer leaves it; the order changes at once and rebuilds no panel. A header click folds or unfolds only its card, but unfolding one folds the other open cards of its stack if they would not fit together. | Accepted |
| D58 | 2026-10-03 | Log (WP 3.18). A thread-safe `TextLog` in core holds the newest lines with the time each was pushed; a `Log` widget is bound to it as a new value kind, `Lines` (`LinesBinding`), the way a graph is bound to a series. Its height is fixed by its `lines` option; lines are cut with an ellipsis, never wrapped. It follows the newest line; the wheel (where its panel does not scroll) and its own scrollbar scroll it back, and new lines do not move it while it is scrolled back. Time stamps (`Log::Time`, time of day) are an optional part, hidden by default. Levels (warnings, errors) come later. | Accepted |
| D59 | 2026-10-03 | Text display (WP 3.19), and a renaming. The widget that shows a label with one short value beside it (WP 3.10) is now `ValueDisplay` (its parts' theme key `value_display`); the name `TextDisplay` now belongs to a block of changing text below its label, bound to text, read-only. Its height is fixed by `lines` (default 1), never by the text; the text wraps within its lines, line breaks start new lines, and what does not fit ends in an ellipsis on the last line. `align` places each line. It fills its column. Numbers stay with `ValueDisplay` and its format. | Accepted |
| D60 | 2026-10-03 | CI builds on Linux with as many compile jobs as the runner has cores (`--parallel "$(nproc)"`), not without a limit: twice (PRs #96, #98) the ThreadSanitizer build was killed for lack of memory while compiling everything at once. Consistent and correct test runs go before speed. | Accepted |
| D61 | 2026-10-03 | App (WP 4.2). `App` opens the window from `WindowSetup` (full screen at the desktop's resolution, `size` unused then; `minimumSize` passed to the window), loads the font if the theme has none, builds the UI, and runs the loop: input, events to `onEvent`, `onUpdate(dt)`, the UI's update and draw. The idle loop keeps waiting at most one display frame, so an idle application calls `onUpdate` about 60 times a second; the comment that said a few times a second was wrong and is corrected. `run(simulation)` comes with the simulation thread (WP 4.3); `examples/starter` stays compile-only until views (WP 4.4) and the camera (WP 4.5) exist, then it becomes the executable and the smoke test. `examples/minimal` runs through `App`. | Accepted |
| D62 | 2026-10-03 | Simulation runner (WP 4.3). Fixed-size ticks due as real time passes times `speed`; a simulation more than 0.25 s of real time behind drops what it could not do, so it slows down instead of freezing the application. Paused, it sleeps on commands and wakes at least every 20 ms, so pausing, stepping and stopping take effect within 20 ms. `step` while running is ignored. A state is written only after the main thread took the last one (at most once per frame), and once more when pausing, stepping, handling a command while paused, and stopping. `examples/minimal` runs its simulation on its own thread. | Accepted |
| D63 | 2026-10-03 | Views (WP 4.4). The `View` widget is built: a height of its own or dynamic. Views are drawn by the UI at their place in the drawing order (the background view first, each view widget right after its panel), clipped to the part that can be seen, with (0, 0) at their corner and one unit a pixel. Over a view widget the pointer is as over no panel: presses, drags, moves and the wheel go to the application, also in a panel that scrolls. `examples/minimal` draws particles in a main view and a minimap: a Map panel that floats at the top right where panels float, and takes the top cell of the right column where they fill the window's grid (now three rows). | Accepted |
| D64 | 2026-10-04 | Camera, and the examples (WP 4.5). `Camera` is built as declared: a drag that starts in its view pans, the wheel zooms towards the pointer within its limits, `apply` keeps the UI's viewport and clip, and `show` waits for the view's size if it is not known yet. `examples/starter` is a normal executable with a `--smoke-test` option and a smoke test of its own (it now runs, so the widget named "Ticks/s", which a '/' made invalid, is "Ticks per second"). `examples/minimal` is kept and renamed `examples/showcase`: it shows every widget, the layout themes and a threaded simulation in two views, and keeps its smoke test; its main view can be dragged and zoomed, a click on the minimap moves it there, the minimap marks what it sees, and Reset fits it to the world again. | Accepted |
| D65 | 2026-10-04 | Selecting views, and the minimap (WP 4.5, asked for in review). A press on a view widget selects it: its outline (`View::Selection`, accent) shows it, and keys and text are forwarded for it until a press anywhere else, even after the pointer left it. A setup may name one default view (`UISetup::defaultView`): it has the keys while no view is selected, needs no selecting and shows no outline; without one, a view gets keys only while selected. Key and text events say which view they are for (`view`, `viewName`, `isFor`). A `Minimap` helper steers a main camera: `Pan` (press and drag centre the main view) or `Select` (a dragged rectangle becomes the main view; a click centres it), chosen in its options and changeable at runtime; the arrow keys move the main view while the minimap is selected; the main view's centre stays inside the world. The showcase switches the mode with a "Select area" switch; both examples use the helper. | Accepted |
| D66 | 2026-10-04 | Responsiveness (WP 4.6). The goal of Phase 4 counts as met: with every tick taking a second, the main loop keeps the display's rate and no pass is longer than with fast ticks (5.5). It is measured by `atpl_responsiveness_bench`, whose test only checks that the UI kept going, since times on shared CI machines are not reliable. The showcase gets a "Tick cost" slider (0 to 1000 ms of busy work per tick) to see it by hand. | Accepted |
| D67 | 2026-10-04 | Utility scope (WP 5.1, closes Q5). The first version ships: a thread pool with `parallelFor` (WP 5.2, P8); random numbers, one seedable generator per thread, with helpers for ranges and unit vectors, deterministic for a fixed seed and thread count (WP 5.3); timing, kept small: a stopwatch, a cooldown that fires every N seconds, a scoped timer that writes into a `Series`, and a running average (WP 5.4; the fixed timestep stays in the simulation runner); a 2D grid with bounds-checked access, neighbour iteration and row ranges for `parallelFor` (WP 5.5); in `app`, since they need SFML: fonts and textures loaded once and shared by name, and a quad batch that draws many sprites or cells in one call (WP 5.6). Left out: a config loader (settings saved and loaded fit better later as saving and loading `Param`s), vector and maths helpers (SFML 3's `Vector2` has them), colour helpers (added when an example needs one), a stable-handle container, and the rest of peztool's utilities (raycasts, segments, graphs, observers), which are specific to particular apps. A colour picker widget, like the one in the ant reference, may come later; it is not part of Phase 5 (Backlog, #106). | Accepted |
| D68 | 2026-10-04 | Thread pool (WP 5.2). `ThreadPool` in `core`; the application owns it (for example as a member of its simulation), there is no global pool. The thread that calls `parallelFor` works on parts too, so by default the pool has two workers fewer than the machine has hardware threads (at least none): with the caller, one core stays free for the main thread (P8); without workers, loops run on the caller. A loop is cut into contiguous parts that differ by at most one item; how depends only on the count, the number of workers and an optional minimum part size, never on timing, so `fn(start, end, part)` can keep one accumulator or random generator per part and get the same result every run. Idle workers sleep on a condition variable, and so does the caller waiting for the last part: nothing spins. If a part throws, the others still run and the first exception is thrown again on the caller. A `parallelFor` inside a part runs on that part's thread; calls from several threads take turns. Only `parallelFor`, no tasks of their own. `app` stays the only layer that starts threads, apart from this pool; the layering check enforces it. The examples gain nothing from it yet; a grid example will use it. | Accepted |
| D69 | 2026-10-04 | Random numbers (WP 5.3). `Random` is PCG32 (16 bytes of state, 2^63 streams per seed), seeded through splitmix64 so nearby seeds start far apart. Its helpers do not use the standard distributions, whose results differ between standard libraries: a seed gives the same integers, uniform numbers, ranges, chances, angles, directions and points in a disc on every platform (directions and discs by rejection, without trigonometry); only `normal` uses `std::log`. Integer ranges include both ends and have no bias; floating-point ranges exclude their end. `unitVector<V>` and `inDisc<V>` build any `V{x, y}`, since core cannot name SFML's vectors. A default `Random` has a fixed seed; `fromEntropy()` differs per run. Reproducible parallel loops use a generator per part, not per thread, since parts go to whichever thread is free: `RandomStreams(seed, pool.maxParts())`, stream `i` for part `i`. `threadRandom()` gives each thread its own generator from entropy for quick use; it is not called `random()`, which an unqualified call would confuse with POSIX's `::random()`. | Accepted |
| D70 | 2026-10-04 | Timing (WP 5.4). `Stopwatch` on the steady clock (seconds and milliseconds as `double`, `restart`; no pause). `Cooldown` reads no clock and calls nothing: `advance(seconds)` returns how many periods ended, left-over time counts towards the next, so it follows simulated time (deterministic, pauses with the simulation) or the wall clock, as it is given; `progress()`, `reset()`, `setPeriod()`. `RunningAverage` over a window of the newest N values (not exponential), its sum a `double` computed afresh once per window; it belongs to one thread. `ScopedTimer` writes the milliseconds of its scope into a `Series` (which a graph may show from the main thread) or a `RunningAverage`; no macro. The showcase times its ticks with them: the Status text says how long a tick takes, on average over the last second, so the Tick cost slider shows in it. | Accepted |
| D71 | 2026-10-04 | Grid (WP 5.5). `Grid<T>` in `core`, header only. Signed `int` coordinates, so neighbour offsets need no casts; every function also takes any point with `x` and `y` (`sf::Vector2i`). `grid(x, y)` is unchecked and asserted in debug builds, `at` throws `std::out_of_range`, `wrapped` joins the edges. Cells are stored row after row in a plain array rather than a `std::vector`, so `Grid<bool>` holds real bools: every cell has an address, and different threads may write different cells. Rows are contiguous: a `parallelFor` over the height splits by rows, and `rows(first, end)` is the span a part covers; the grid knows nothing of the pool. `forEachNeighbour(x, y, fn)` with `Neighbourhood::Four`/`Eight` and `Edges::Skip`/`Wrap`; the offset tables are public. Stepping reads one grid and writes another, then `swap`s them; no extra class. `resize` discards the old cells. Mapping world positions to cells is left to the application. The grid is shown in an example with the quad batch (WP 5.6). | Accepted |
| D72 | 2026-10-04 | CI time. The build was nearly all of each job's time (2.5 to 7 minutes; SFML at most a fifth of it, Catch2 and the template's own code the rest), so the Linux jobs compile through ccache, with one cache per job kept between runs by `actions/cache`: a run compiles only what changed, SFML and Catch2 included, and a pull request starts from the caches of `main`. Every run saves a cache of its own, since cache keys cannot be overwritten, and starts from the newest; GitHub drops the least recently used beyond its limit. The Windows (MSVC) job is removed to save action minutes; it comes back at the end of the project, with the platform fixes it finds (WP 7.5, #112). P9 stands: no platform-specific code. | Accepted |
| D73 | 2026-10-04 | Resources and the quad batch (WP 5.6). `Resources::font(name)` and `texture(name, options)` load on first use and share afterwards; the name is the path relative to the resource directory; what they return lives as long as the `Resources`, whose copies share the cache; a failed load is not cached; a texture keeps the options of its first call. The main thread only, without locks. `loadTexture` joins `loadFont`. `QuadBatch` in `app`: two triangles per quad in one vertex array, one texture per batch, drawn in one call under the target's view; quads are added (rectangles, texture rectangles, turned around their centre) or changed in place, so a grid's cells are laid out once and only recoloured. After `resize`, threads may set different quads at once. The showcase shows Phase 5 together: a "Heat" switch lets the particles warm a 200 by 200 `Grid<float>` on the simulation thread; the heat spreads to the four neighbours and fades, row by row over a `ThreadPool`, into a second grid that is swapped in. The main thread draws the ground as 40,000 quads of the theme's accent in one call, and the filled particles as quads of `textures/particle.png` (made by `tools/make_particle_texture.py`, so it needs no licence) in another; outlined particles stay shapes. A grid copied into a grid of the same size now reuses its memory, so the heat grid in each published state allocates nothing. `--theme moon|colorful` chooses the showcase's look. | Accepted |

### Open questions

| ID | Question | Blocks |
|---|---|---|

Closed: Q1 (by P1), Q2 (by P3: the simulation thread is the template's model), Q3 (D12), Q4 (D14), Q6 (D16), Q8 (D13), Q9 (D15), Q10 (D18), Q11 (D17), Q12 (P11), Q7 (D37), Q5 (D67).

---

## 4. Target responsibilities

One owner per concern (D1). The "must not" column is as binding as the "owns" column.

| Owner | Owns | Must not |
|---|---|---|
| `App` | The window, the app loop, receiving UI events on the main thread and forwarding to the simulation (D11, D26) | Touch UI internals |
| `UI` (facade) | Calling the systems in order, public API. Uses the window it is given (D26) | Contain the logic of any single concern, create or own the window |
| Storage (`ContainerManager`, widget storage) | Creation, lifetime, lookup | Lay out, draw, handle input |
| `binding` module | Adapters for `Param<T>`, `Series` and functions; handing changed bound values to widgets; writing the user's changes into bindings (D31) | Touch layout, drawing or interaction state; raise events |
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

### 5.5 Targets, and what was measured

The targets were set before anything was built. They were measured in WP 2.7 (2026-10-02):

| Target | Measured | Met |
|---|---|---|
| Idle UI and no new snapshot: no frames rendered | 0 frames; the loop uses 0.4 % of one processor core | yes |
| At most 2 draw calls per panel, plus text | 2 per panel for shapes (10 for 5 panels), plus 1 per text run (160) | yes |
| UI build and submit under 0.2 ms per frame at 100 widgets | 0.08 ms when the UI is unchanged, 0.10 ms when one panel changes; 0.20 ms when every panel is painted anew every frame | yes; the worst case is at the limit (D38) |

**How it was measured.** `atpl_render_bench` (`tests/bench/render_bench.cpp`) builds five panels with 100 widgets between them: sliders, switches, buttons, progress bars, values, dropdowns and graphs, painted the way real widgets will be, with every part's style resolved from the theme. Each scenario runs the main loop for 2000 frames with vsync off, and the profiler (`render/profiler`) reports the averages. Release build, GCC 13, Ryzen 7 7700X, GeForce RTX 4070 Super, window 1772 x 854 with 4x anti-aliasing.

| Scenario | Build | Submit | Build + submit | Panels rebuilt | Texts built | Allocations |
|---|---|---|---|---|---|---|
| First frame, everything built (once) | 0.43 ms | 2.84 ms | 3.27 ms | 5 | 160 | 1960 |
| Simulation running, UI unchanged | 0 | 0.080 ms | **0.080 ms** | 0 | 0 | 0 |
| A panel dragged | 0 | 0.076 ms | **0.076 ms** | 0 | 0 | 0 |
| One slider dragged | 0.024 ms | 0.077 ms | **0.101 ms** | 1 | 0.9 | 8.6 |
| A live value and a graph change in every panel, every frame | 0.119 ms | 0.084 ms | **0.203 ms** | 5 | 5 | 47 |

All values are per frame. Every scenario draws 170 draw calls and 7740 triangles. "Build" is painting the panels that changed; "submit" is clearing the window and handing the batches to the graphics card. Putting the frame on screen is measured apart and took 0.02 to 0.10 ms with vsync off.

What the numbers say about the estimates in 5.1:
- **Frame skipping (level 1) is the largest saving by far**: an idle UI costs nothing, where every drawn frame costs at least 0.08 ms plus the display's own work.
- **Panel batches (level 2) work as intended**: painting one panel of 20 widgets, graph included, takes 0.024 ms, so only the panel that changed costs anything. Moving a panel costs nothing extra.
- **Submitting is the larger part**, and of it the 160 text draw calls. Building all text of a panel into one batch (the extension recorded in D37) is where the next saving is, should one be needed.
- **A frame in which nothing changed allocates nothing.** A text that changes costs about 9 allocations, inside SFML's text object.
- The worst case in the table does not occur with the rule of 5.2 that live values refresh at 5 to 10 Hz instead of every frame.

The times depend on the machine; run `atpl_render_bench` to get them for another one. The first two targets do not, and are checked by the test `bench.render` on every run.

#### A slow simulation (WP 4.6, 2026-10-04)

The goal of Phase 4: the UI keeps its pace and its input while one simulation tick takes a second.

| | Ticks of 1 ms | Ticks of 1 s |
|---|---|---|
| Ticks in 10 s | 10 | 9 |
| Passes of the main loop | 55 to 59 per second | 56 to 57 per second |
| Pass time, median | 16.7 ms | 16.7 ms |
| Pass time, 99th percentile | 36.8 to 37.1 ms | 36.5 to 36.8 ms |
| Pass time, longest | 37.4 to 39.1 ms | 37.2 to 37.3 ms |

**Met.** A tick of a second makes no difference to the UI: the loop runs at display rate (60 Hz here), and no pass is longer with slow ticks than without them. A pass is the longest the UI goes without reading input, so input is handled within two display frames at worst, as it is with a fast simulation. The longest passes, two display frames, happen with fast ticks too: they are frames the display missed, not the simulation's doing.

**How it was measured.** `atpl_responsiveness_bench` (`tests/bench/responsiveness_bench.cpp`) runs an application with a panel of widgets and the profiler readout, and a simulation whose every tick keeps a core busy for the given time (at one tick per second, as the 1 s case needs). It asks for a frame in every pass of the loop, with vsync on, and records how long each pass takes. Two runs of 10 s each, on the reference machine (16 cores, a 60 Hz display under X11). The test `bench.responsiveness` runs it for 3 s on every run and checks only that the UI kept going: at least ten passes for every tick. In the showcase, the "Tick cost" slider makes every tick that slow by hand.

---

## 6. Roadmap

Work is tracked on GitHub:
- Board: https://github.com/users/SvenSZim/projects/3 (columns: Backlog, Ready, In progress, Done)
- Work packages as issues, titled `WP <phase>.<n>`, one milestone per phase, labels per area.
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

### Phase 1 — App-facing API (done 2026-10-02)
- Write the public headers only: setup, widget descriptors, widget, view and panel handles (P2, D12), `Param<T>` (D10), the event type and stream (P4, D15), the command queue (D11), simulation hooks.
- Write the reference application `examples/starter` against them.

**Done when:** the example compiles against the headers and the API is agreed. Everything below is built to fit it.

### Phase 2 — Render pipeline (done 2026-10-02)
- Draw list and primitives: styled box (fill, border, radius, shadow), line, text.
- Theme core: tokens, role defaults, part entries (P11).
- Per-panel batch and dirty flag (Level 2), kept text (Level 3), frame skipping (Level 1).
- Explicit draw order and overlay layer.
- Profiler readout (D6).

**Done when:** the targets in 5.5 are measured and recorded here.

### Phase 3 — Widgets and input
- Widget model (P1) and `InputSystem`: hit-testing, hover, press, capture during drag, keyboard focus.
- Wire `packWidgets` into layout.
- Widget pool: button, switch, slider, value display, text display, progress bar, graph, log, dropdown, text input, paragraph.
- `Param<T>` and `Series` implementations (D10, P10).
- Data bindings by name and in the descriptor (P2).
- Panel scrolling and the other overflow rules (D28).
- Card-stack overflow for floating panels, opt-in (D42).
- Layout themes: sizes and placement defaults next to the theme, top-down and bottom-up sizing, widget size requests, scaling with the window (D44, `docs/LAYOUT.md`).
- The `Paragraph` widget (D30).

**Done when:** every widget in the demo is interactive and bound to app data.

### Phase 4 — Simulation layer and threading
- Simulation runner: fixed timestep, pause, single step, speed.
- Command queue and snapshot exchange (P3, D11).
- Background view and view widgets (D12); unconsumed input arrives through the event stream (P4).
- Optional pan/zoom helper for a view (D15).

**Done when:** the demo simulation runs on its own thread and the UI stays responsive while one tick takes a second.

### Phase 5 — Core utilities (done 2026-10-04)
- Thread pool with parallel-for (P8), per-thread RNG, timing (stopwatch, cooldown, scoped timer, running average), grid, resource store, quad batch for world rendering.
- Scope fixed by D67 (Q5): no config loader, vector, maths or colour helpers.

### Phase 6 — Polish
- Theme tokens behind the style-resolving interface (P5), second theme.
- Hover, press and toggle animations; panel collapse animation.
- Shadows, GUI scale.

### Phase 7 — Packaging
- README quick start.
- Two example apps (for instance a pathfinding grid and a particle simulation).
- CI for every supported platform, the Windows build check again (WP 7.5, D72).

---

## 7. Current state (2026-10-04)

- **Phase 0 is done**: structure, kept utilities, resources, the `minimal` example, CI.
- **Phase 1 is done**: the whole public API is declared, documented and agreed (D23 to D33). It is compiled with every build through the usage examples in `tests/api/` and the reference application `examples/starter`. None of it is implemented yet, except small pieces that had to be: `Event`, `Part`, `State`, the number conversion, and the glue in a few templates.
- Tests: 674, of which 72 need a display: the `starter` and `showcase` examples as smoke tests, the render and responsiveness benchmarks, and 68 that draw and check pixels, load glyphs, or run the frame loop, the UI or the app in a window. The tests of what is shared between threads also run under ThreadSanitizer in CI.
- **Phase 2 is done**: shapes, theme core, draw list and painter, panel batches and renderer, text, the redraw flag with frame skipping and an idle loop that sleeps, and the profiler. The targets of 5.5 are measured and met. What exists is the pipeline from a painter call to the screen; there are no panels or widgets yet to feed it, apart from the stand-ins in the tests and in the benchmark.
- **Phase 3 is done** (widgets and input): WP 3.1 (the model: panels, widget slots and views in one store with dense ids, the name index, colours checked against the theme) and WP 3.3 (placing panels and views in the window: anchored stacks, grid cells, and what happens when they do not fit). WP 3.2 (the facade: `UI` is built from a setup, finds things by name, and runs the frame steps; declared panels appear on screen as empty frames, are placed anew when the window is resized, and nothing is drawn while nothing changes). WP 3.4 (widgets inside panels: packed into balanced columns, or placed in a grid of equal cells; cells found largest first, for widgets and for panels; the whole layout pass in order; widgets are painted at their places). WP 3.15 (the layout theme: sizes moved out of the theme, presets, per-panel overrides, a default placement, and sizes that follow the window within limits). WP 3.16 (widget size requests: minimum, preferred, maximum and shape; a widget's rectangle in its room with alignment; panel content top-down and bottom-up, with equal cells sized by the largest minimum). WP 3.17 (panel sizes: fill or fit in the grid, own or equal floating sizes, limits from the window, collapse direction; widgets and panels that do not fit are left out and come back when there is room). With it the layout concept of D44 is built. WP 3.5 (input: hit-testing, hover, press and drag, keyboard focus, widget events, and forwarding what the UI does not use, with the view under the pointer). WP 3.6 (the panel frame: header, title, arrow and optional underline; folding on a header click, animated). WP 3.7 (the thread-safe values: `Param<T>`, `Series` and `PointSeries`, tested under ThreadSanitizer). WP 3.8 (bindings: parameters, series, points, functions and the application's own; syncing in both directions, with a refresh limit for values that are only shown). WP 3.9 (Button, Switch, Slider; the `minimal` example's controls are real and bound to parameters). WP 3.10 (ValueDisplay, then called TextDisplay; ProgressBar, Graph; the `minimal` example shows a made-up simulation live). WP 3.11 (the overlay above every panel; Dropdown and TextInput, both in the `minimal` example). WP 3.12 (panel scrolling: the wheel, a draggable scrollbar, hit-testing through the offset, no rebuild while scrolling). WP 3.13 (Paragraph: static heading, body and footer, wrapped; an About panel in the `minimal` example). WP 3.14 (the card stack: floating panels that do not fit overlap like cards, opt-in). WP 3.18 (the log: a thread-safe `TextLog` and a `Log` widget of a fixed size; an Events log in the `minimal` example). WP 3.19 (TextDisplay, a block of changing text of a fixed size; the former TextDisplay is now ValueDisplay; a Status text in the `minimal` example).
- **Phase 4 is done** (the simulation layer and threading): WP 4.1 (`Queue` and `Snapshot` implemented: a locked queue that a waiting reader sleeps on, and a lock-free triple buffer; tested from several threads, also under ThreadSanitizer). WP 4.2 (`App`: the window from its setup, the font, the UI and the main loop with events and an update; `examples/minimal` runs through it). WP 4.3 (the simulation runner: its own thread, fixed-size ticks, speed, pause, steps, a state at most once per frame, and failures thrown again on the main thread; `examples/minimal` runs its simulation on it). WP 4.4 (views: the view widget, drawing the application's views in order and clipped, input over views forwarded; particles in a main view and a minimap in `examples/minimal`). WP 4.5 (the camera and the minimap helper; views selected by a click get the keys, with a default view for the rest; `examples/starter` runs and is a smoke test; `examples/minimal` is now `examples/showcase`, with a main view to drag and zoom). WP 4.6 (a slow simulation does not slow the UI: measured with `atpl_responsiveness_bench`, recorded in 5.5; a "Tick cost" slider in the showcase).
- **Phase 5 is done** (core utilities): WP 5.1 (the utility scope, D67). WP 5.2 (the thread pool with `parallelFor`, tested also under ThreadSanitizer, D68). WP 5.3 (random numbers: the same for a seed on every platform, one generator per loop part for reproducible parallel work, D69). WP 5.4 (timing: stopwatch, cooldown, running average, scoped timer; the showcase's Status says how long a tick takes, D70). WP 5.5 (the grid: checked, unchecked and wrapped access, neighbours, rows for parallel loops, D71). WP 5.6 (fonts and textures by name, the quad batch; a heat map in the showcase that uses the grid, the pool and both batches, D73).
- Next: Phase 6 (polish), starting with WP 6.1 (widget and panel animations).
- No open questions. No proposals are pending.
- CI runs a ThreadSanitizer job since WP 3.7. The Linux jobs compile through ccache, and the Windows job is paused until WP 7.5 (D72).
