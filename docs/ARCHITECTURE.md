# SFMLAppTemplate — Target Structure

Last updated: 2026-10-02 · Status: **approved** (decision D14 in [PROJECT_PLAN.md](PROJECT_PLAN.md)).

This describes the structure the codebase is reworked into. Decisions referenced as D*, P*, Q* are in the plan's decision log.

Root namespace and target prefix are `atpl` (D18). "Panel" is what the old code called "Container" (D17).

---

## 1. Layers

Three libraries (P6). Dependencies point one way only.

```
your application
      │
      ▼
   atpl_app      window, main loop, simulation thread, optional helpers
      │
      ▼
   atpl_ui       panels, widgets, layout, input, theme, rendering      ──► SFML Graphics
      │
      ▼
   atpl_core     threading, parameters, queues, timing, RNG, animation ──► C++ standard library only
```

Rules:
- `core` never includes SFML. It can be used and tested on any thread without a window.
- `ui` never includes `app`. It knows nothing about simulations or threads other than `Param<T>`.
- `app` is the only layer that starts threads.
- Only two files touch `sf::RenderWindow`: `ui/ui.cpp` and `ui/render/renderer.cpp`. Everything else in `ui` is plain data in, plain data out, and testable without a display.

## 2. Directory layout

```
SFMLAppTemplate/
├─ CMakeLists.txt
├─ cmake/                      warnings, resource copy
├─ docs/                       PROJECT_PLAN.md, ARCHITECTURE.md
├─ resources/fonts/
├─ include/atpl/                PUBLIC headers: everything an application may include
│  ├─ core/
│  ├─ ui/
│  └─ app/
├─ src/                        implementation and INTERNAL headers
│  ├─ core/
│  ├─ ui/
│  │  ├─ model/
│  │  ├─ input/
│  │  ├─ layout/
│  │  ├─ render/
│  │  ├─ theme/
│  │  └─ widgets/
│  └─ app/
├─ examples/
│  ├─ minimal/                 smallest possible app (also the smoke test)
│  ├─ pathfinding/
│  └─ particles/
└─ tests/
   ├─ core/
   ├─ ui/
   └─ app/
```

A header is in `include/atpl/` only if applications need it. Internal headers live next to their `.cpp` in `src/`. This makes the public API visible at a glance and keeps internals free to change.

CMake targets: `atpl_core`, `atpl_ui`, `atpl_app` (aliases `atpl::core`, `atpl::ui`, `atpl::app`), one executable per example, one test executable per layer (`atpl_<layer>_tests`). `atpl_core` tests need no display, so they always run in CI. The test `structure.layering` checks the rules of section 1 against the sources.

## 3. `core` — utilities (R8)

| File (`include/atpl/core/`) | Content | Origin |
|---|---|---|
| `param.hpp` | `Param<T>`: thread-safe value that reads and writes like a `T`, for numbers, enums and strings (D10, P10) | new |
| `series.hpp` | `Series`: thread-safe ring buffer of samples with fixed capacity; the data source for graphs (P10) | new |
| `queue.hpp` | Thread-safe queue, used for app → simulation commands (D11) | new |
| `snapshot.hpp` | Exchange of the latest complete state, simulation → main thread (P3) | new |
| `thread_pool.hpp` | Thread pool with `parallelFor(count, fn)`; leaves one core free (P8) | new |
| `random.hpp` | Random number generator, one instance per thread | new |
| `timing.hpp` | Stopwatch, scoped timer, rate limiter, fixed-timestep accumulator | new |
| `easing.hpp` | Easing functions | from `ui/utils/functions` |
| `interpolated.hpp` | Animated value with easing | from `ui/utils/interpolated` |
| `grid.hpp` | 2D grid container | new, Phase 5 |

The exact utility list for the first version is Q5.

## 4. `ui` — one owner per concern (D1)

### 4.1 Public headers (`include/atpl/ui/`)

| File | Content |
|---|---|
| `ui.hpp` | `UI`: the facade. Setup, `widget(name)`, `view(name)`, `panel(name)`, events, `update`, `render`. |
| `setup.hpp` | `UISetup`, `PanelSetup`, layout mode, anchors. |
| `widgets.hpp` | Descriptors for the widget pool: `Widget::Slider(...)`, `Widget::View(...)` and so on. |
| `handle.hpp` | `WidgetHandle` (`bind`, `set`, `get`), `ViewHandle` (`onDraw`, `requestRedraw`), `PanelHandle`. Light values; the app does not keep them (P2). |
| `binding.hpp` | The binding interfaces per kind of value, and adapters that build one from a getter and setter (P10). |
| `event.hpp` | `Event`: the one type for widget events and forwarded input (P4). |
| `theme.hpp` | `Theme` (tokens, part entries), `Metrics`, built-in themes, `Style`, `Kind`, `Part`, `Role` (P5, P11). |
| `widget.hpp` | `Widget` interface, `Painter`, input and measure contexts, for app-defined widgets (P1). |
| `rect.hpp` | Rectangle type. From `ui/utils/rect`, trimmed. |

### 4.2 Internal modules (`src/ui/`)

| Module | Owns | Reads | Must not |
|---|---|---|---|
| `model/` | Panels, widget slots, views, name index, lifetime | — | Lay out, draw, handle input |
| `input/` | Hit-testing, hover, press, drag capture, keyboard focus, what the UI consumes, emitting events | model, layout rects | Set geometry |
| `layout/` | Every rectangle and size: panel placement, widget packing inside panels, view regions, visibility | model, widget `measure`, `Metrics` | Touch interaction state or colours |
| `theme/` | Tokens, `Metrics`, role defaults, part entries, built-in themes, resolving a `Style` for a part and state | — | Decide any rectangle or position |
| `render/` | Draw list, tessellation, per-panel batches, text cache, draw order, frame flag, profiler | model, layout rects, theme | Modify the model |
| `widgets/` | One file per widget type: its behaviour only | its own state, contexts handed to it | Position itself, issue draw calls, read theme tokens directly |
| `ui.cpp` | Calling the modules in order; the public facade | everything | Contain logic of a single concern |

Files:

```
src/ui/
├─ ui.cpp
├─ model/    panel.hpp  widget_slot.hpp  view.hpp  store.hpp/.cpp  name_index.hpp/.cpp
├─ input/    input_system.hpp/.cpp
├─ layout/   layout_manager.hpp/.cpp  floating.cpp  grid.cpp  packing.hpp
├─ theme/    theme.cpp  presets.cpp
├─ render/   renderer.hpp/.cpp  draw_list.hpp/.cpp  shapes.hpp/.cpp
│            panel_batch.hpp/.cpp  text_cache.hpp/.cpp  profiler.hpp/.cpp
└─ widgets/  button.cpp  switch.cpp  slider.cpp  text_display.cpp  progress_bar.cpp
             graph.cpp  dropdown.cpp  text_input.cpp  view.cpp
```

### 4.3 The model

- **Panel**: name (unique, Q8), placement, collapsed/expanded and hovered state (P7), ordered list of widgets, one dirty flag.
- **Widget slot**: what the framework keeps for every widget, whatever its type.

  | Field | Written by |
  |---|---|
  | name, panel | model (at setup) |
  | rectangle (panel-local), visible | layout |
  | hovered, pressed, focused | input |
  | binding to a `Param<T>` | app, through `WidgetHandle::bind` |
  | the widget object itself | model (at setup) |

- **Widget object**: an implementation of the `Widget` interface. It holds only type-specific state (slider value, animation progress, dropdown open).
- **Ids** are dense indices, so every lookup by id is a direct array access. Names are resolved to ids once, when a handle is requested.
- **Names**: panel names are unique; widget names are unique per panel. `"Speed"` works if it is unique in the whole UI, otherwise `"Controls/Speed"`. Duplicates and unknown names fail loudly (Q8).

### 4.4 The widget interface (P1)

```cpp
class Widget {
public:
    virtual ~Widget() = default;
    virtual Size measure(const MeasureContext&) const = 0;        // layout asks for the wanted size
    virtual bool handleInput(const InputEvent&, InputContext&);   // true = consumed
    virtual void update(float dt, UpdateContext&);                // animations, bound values
    virtual void paint(Painter&, const Style&) const = 0;         // emit primitives
};
```

A widget acts on the outside world only through the context it is handed: `markDirty()`, `emit(Event)`, `captureInput()`, and reading or writing its bound `Param<T>`. Adding a widget type means adding one file in `widgets/` and one descriptor in `widgets.hpp`. An application adds its own by implementing the interface.

### 4.5 Bindings (P2, D10, P10)

A widget type states the **kind of value** it works with, not a C++ type. It binds to a small interface for that kind: get, set (if editable) and a change counter, which the UI uses to mark the panel dirty.

| Widget | Kind | Ready-made binding |
|---|---|---|
| Button | none (emits an event) | optionally `Param<bool>`, set on click |
| Switch | bool | `Param<bool>` |
| Slider | number | `Param<float>`, `Param<int>`, `Param<double>` |
| Progress bar | number, read-only | `Param<float>` |
| Text display | text, read-only | `Param<number>` plus a format, or `Param<std::string>` |
| Text input | text | `Param<std::string>` |
| Dropdown | index into its options | `Param<int>` or `Param<AnyEnum>` |
| Graph | series, read-only | `Series` |
| View | none (draw callback) | — |

Rules:
- Everything the template ships (`Param<T>`, `Series`) is thread-safe.
- Plain variables cannot be bound directly.
- An application may implement a binding interface over its own data, or build one from a getter and a setter: `ui.widget("Speed").bind(getter, setter)`. Thread safety of such a binding is the application's responsibility.
- What a widget **is** (range, step count, option labels) is part of its descriptor, not of the binding.

### 4.6 In-panel layout (P12)

- A widget reports the size it wants through `measure()`, given the available width and the `Metrics`.
- `layout/` packs the widgets of a panel according to the panel's inner layout mode (vertical, two or three columns; `packing.hpp`) and assigns each widget its rectangle in panel-local coordinates. A widget never positions itself.
- Inside its own rectangle, a widget arranges its parts (label, track, knob). `paint` and `handleInput` use the same part rectangles, computed in one place per widget.
- `Metrics` (padding, gaps, row heights, font sizes) is a token set next to the theme's colours. Only `layout/` and `measure()` read it. Nothing else defines sizes.

### 4.7 Views (Q3)

The application's own rendering appears in views:
- **Background view**: the whole window, behind all panels. Optional.
- **View widget**: a region inside a panel. Any number of them (main view, minimap, ...).

Both are the same thing to the app: a named view with a draw callback, set through `ui.view("name").onDraw(...)`. The renderer calls it at the right point in the draw order, with drawing clipped to the view's rectangle. The app calls `requestRedraw()` (thread-safe) when it has something new to show.

View regions are clipped to rectangles; a view cannot have rounded corners unless it is rendered to a texture first (opt-in, D7).

### 4.8 Events (P4, D11, Q9)

One `Event` type, one stream, read on the main thread:
- **Widget events**: button pressed, value changed. They identify the widget.
- **Forwarded input**: mouse button, mouse move, wheel, key, text that the UI did not consume. Mouse events name the view they happened in (or the background) and carry the position in window pixels and relative to that view.
- **Window events**: closed, resized.

The template only forwards. It does not interpret forwarded input (Q9). Pan and zoom for a view is an optional helper in `app` (section 5).

### 4.9 Rendering (D3–D7, plan section 5)

```
widget.paint() ──► Painter ──► draw list ──► PanelBatch (one vertex array + text) ──► window
```

- `shapes`: tessellation of rounded rectangles, circles, lines, shadows. All shapes become triangles so that a panel is one draw call. The rounded-rectangle code comes from the current `renderer.cpp`.
- `panel_batch`: one batch per panel in panel-local coordinates, rebuilt only when the panel is dirty (cache level 2).
- `text_cache`: text geometry per widget, rebuilt when the string changes (level 3).
- `renderer`: the frame flag (level 1), the fixed draw order, and calling view callbacks.
- `profiler`: build time, draw-call count, frame time (D6).

Draw order per frame: background view → panels in order (shapes, views, text) → overlay layer → profiler.

### 4.10 Painting and theme (P5, P11)

`paint` returns nothing. It calls the `Painter`, which appends primitives to the panel's draw list:

```cpp
painter.box(trackRect, style.part(Track));
painter.box(knobRect,  style.part(Knob));
painter.text(labelRect, name, style.part(Label));
if (auto ticks = style.part(Ticks))            // optional part
    for (auto& r : tickRects) painter.box(r, *ticks);
```

Who decides what:
- **The widget** declares its kind and its parts, and where each part is. For every part it states a **role** (surface, track, accent, handle, text, line) and whether it is shown by default.
- **The theme** decides how each part looks and whether it is shown.
- **The painter** turns rectangle plus style into triangles. A box style holds fill, border colour and thickness, corner radius (0 = sharp, "full" = pill or circle) and shadow.
- **The descriptor** decides what the widget is: range, step count, option labels. Tick positions come from the step count; whether ticks are drawn comes from the theme.

Kinds and parts are open identifiers declared by the widget, not enums owned by the template:

```cpp
class Slider : public Widget {
public:
    static constexpr Kind kind{"slider"};
    static constexpr Part Track{kind, "track", Role::Track};
    static constexpr Part Fill {kind, "fill",  Role::Accent};
    static constexpr Part Knob {kind, "knob",  Role::Handle};
    static constexpr Part Ticks{kind, "ticks", Role::Line, Shown::No};
    static constexpr Part Label{kind, "label", Role::Text};
};
```

The theme resolves a part's style in three layers; a later layer overrides an earlier one:

| Layer | What | Example |
|---|---|---|
| 1. Tokens | Global values | primary colour, standard radius, standard thickness |
| 2. Role defaults | Style derived from the part's role and the tokens; visibility from the part's own default | every track part is neutral with a small radius; ticks hidden |
| 3. Part entries | A specific setting for one part | `theme[Slider::Ticks].shown = true;` `theme[Panel::Outline].thickness = 5;` |

Consequences:
- The simple token theme uses layers 1 and 2 only. It needs no per-widget entries.
- App-defined widgets declare their own kind and parts and look right under every theme, including themes that have never heard of them.
- More detailed styling later (per panel, per widget instance) is another override layer on top; widgets do not change.
- A different structure (a part the widget does not declare) needs widget code, not a theme change.

`Metrics` (padding, gaps, row heights, font sizes) are tokens too, but only `layout/` and `measure()` read them (4.6).

## 5. `app` — running an application

| File (`include/atpl/app/`) | Content |
|---|---|
| `app.hpp` | `App`: creates the window and UI, runs the main loop, delivers events to the application's handler on the main thread (D11). |
| `simulation.hpp` | The interface an application's simulation implements, and the runner: own thread, fixed timestep, pause, single step, speed (P3). |
| `camera.hpp` | Optional pan/zoom helper for a view. It reads forwarded input events and produces an `sf::View` (Q9). |
| `resources.hpp` | Fonts and textures by name; resolves paths relative to the executable. |

## 6. One frame

Main thread:

| Step | Owner | What happens |
|---|---|---|
| 1 | `input` | Window events are read. Widgets react, bound parameters are written, panels are marked dirty, events are emitted. |
| 2 | application | Reads the events. Handles them or pushes commands to the simulation. |
| 3 | `widgets`, `layout` | Animations advance. Bound parameters are read for display. Layout runs if something changed size. |
| 4 | `render` | If nothing needs a redraw, the frame is skipped and the loop waits for input (with a timeout of one display frame). Otherwise dirty panels are rebuilt and everything is drawn. |

Simulation thread, per tick: take pending commands → advance the simulation → publish a snapshot → request a redraw.

The two threads share only `Param<T>` values, the command queue and the snapshot.

## 7. What the application writes

A sketch of the target, not final API (final in Phase 1):

```cpp
struct Params {
    atpl::Param<float> speed{5.f};
    atpl::Param<bool>  gravity{true};
};

int main() {
    Params params;
    MySimulation sim{params};

    atpl::App app({
        .window = {.title = "Showcase", .size = {1280, 720}},
        .ui = {
            .theme = atpl::Themes::Moon,
            .background = "world",                       // background view
            .panels = {
                {.name = "Controls", .anchor = Anchor::TopLeft, .widgets = {
                    Widget::Button("Reset"),
                    Widget::Slider("Speed", params.speed, 0.f, 10.f),   // bound in the descriptor
                    Widget::Switch("Gravity"),
                }},
                {.name = "Map", .anchor = Anchor::BottomRight, .widgets = {
                    Widget::View("minimap"),
                }},
            },
        },
    });

    app.ui().widget("Gravity").bind(params.gravity);     // bound by name
    app.ui().view("world").onDraw([&](sf::RenderTarget& target) { sim.draw(target); });
    app.ui().view("minimap").onDraw([&](sf::RenderTarget& target) { sim.drawMap(target); });

    app.onEvent([&](const atpl::Event& event) {
        if (event.isButton("Reset")) sim.commands().push(Command::Reset);
    });

    app.run(sim);
}
```

## 8. What happens to the existing code

| Existing | Fate |
|---|---|
| `ui/utils/functions`, `interpolated` | Moved to `core/easing`, `core/interpolated` |
| `ui/utils/rect` | Moved to `ui/rect`, trimmed |
| `layout/widget_packing.hpp` and its test | Kept as `layout/packing.hpp` |
| `layout_manager.cpp` (floating and grid placement) | Algorithms ported to the new panel model |
| Rounded-rectangle tessellation in `renderer.cpp` | Moved to `render/shapes` |
| Colour palettes in `renderstyle_templates.cpp` | Become theme presets |
| Drawing logic in `widget_renderers.cpp` | Reused inside each widget's `paint`, rewritten onto `Painter` |
| `container/`, `widgets/widget_manager`, `widget_types`, `widget_factory` | Replaced by `model/` and `widgets/` |
| `rendercache`, `renderstyle`, `text.hpp`, `interaction.hpp`, `ui_manager`, `app` | Replaced |
| Tests for rect, interpolated, packing | Kept and moved |
| Tests for container, event | Rewritten for the new model |
| `CMakeLists.txt`, CI workflow | Extended to three targets, examples and tests |

The old code is available in git history: commit `38a00b4` is the last one that contains all of it (for example `git show 38a00b4:src/ui/core/renderer/renderer.cpp`).

Files marked as moved stay at their old path, outside of any build target, until the work package that moves them (WP 0.2, WP 0.3).

## 9. Order of the rework

1. Skeleton: directory tree, three targets, moved utilities and their tests, an example that opens an empty window. Old UI code removed. Build and tests green.
2. Public headers and the `minimal` example agreed (plan Phase 1).
3. `render/` with a hand-written draw list, then `model/` and `layout/`, then `input/` and the widgets, then `app/` and threading (plan Phases 2–4).

Every step leaves the build green and the `minimal` example running.
