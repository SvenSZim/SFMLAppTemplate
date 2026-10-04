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
- `app` is the only layer that starts threads, apart from the `ThreadPool` in `core`, which the application creates and owns. `ui` starts none.
- The window is created and owned by `app`; the UI is handed a reference (D26). Inside `ui`, only two source files touch `sf::RenderWindow`: `ui/ui.cpp` and `ui/render/renderer.cpp`. Everything else in `ui` is plain data in, plain data out, and testable without a display.

## 2. Directory layout

```
SFMLAppTemplate/
├─ CMakeLists.txt
├─ cmake/                      warnings, resource copy
├─ docs/                       PROJECT_PLAN.md, ARCHITECTURE.md
├─ tools/                      format.sh, make_particle_texture.py
├─ resources/                fonts/, textures/ (copied next to the executables)
├─ include/atpl/                PUBLIC headers: everything an application may include
│  ├─ core/
│  ├─ ui/
│  └─ app/
├─ src/                        implementation and INTERNAL headers
│  ├─ core/
│  ├─ ui/
│  │  ├─ model/
│  │  ├─ input/
│  │  ├─ binding/
│  │  ├─ layout/
│  │  ├─ render/
│  │  ├─ theme/
│  │  └─ widgets/
│  └─ app/
├─ examples/
│  ├─ starter/                 the smallest app written against the API: the reference; a smoke test
│  ├─ showcase/                every widget, the layout themes, a threaded simulation in two views; a smoke test
│  ├─ pathfinding/
│  └─ particles/
└─ tests/
   ├─ core/
   ├─ ui/
   ├─ app/
   └─ bench/                   the render benchmark (plan 5.5)
```

A header is in `include/atpl/` only if applications need it. Internal headers live next to their `.cpp` in `src/`. This makes the public API visible at a glance and keeps internals free to change.

CMake targets: `atpl_core`, `atpl_ui`, `atpl_app` (aliases `atpl::core`, `atpl::ui`, `atpl::app`), one executable per example, one test executable per layer (`atpl_<layer>_tests`). `atpl_core` tests need no display, so they always run in CI. The test `structure.layering` checks the rules of section 1 against the sources.

## 3. `core` — utilities (R8)

| File (`include/atpl/core/`) | Content | Origin |
|---|---|---|
| `revision.hpp` | `Revision`: the change counter every shared value carries, so readers can skip unchanged values | new |
| `param.hpp` | `Param<T>`: thread-safe value that reads and writes like a `T`, for numbers, enums and strings; any number of readers and writers; lock-free for small trivially copyable types, a short lock for strings; a revision that grows with every change (D10, P10) | new |
| `series.hpp` | `Series`: thread-safe ring buffer of `float` samples with fixed capacity; `PointSeries`: the same for points (x, y), for graphs whose x-axis comes from the data. The data sources for graphs; a short lock per push or read, nothing allocated after construction (P10, D50) | new |
| `text_log.hpp` | `TextLog`: thread-safe log of text lines with fixed capacity, each with the time it was pushed, and a count of all lines ever pushed. The data source for log widgets; a short lock per push or read (WP 3.18, D58) | new |
| `queue.hpp` | `Queue<T>`: thread-safe, unbounded, ordered; a tick takes all waiting items at once with `drain`; used for app → simulation commands (D11). One lock around a deque; `waitDrain` sleeps on a condition variable that every push signals (WP 4.1) | new |
| `snapshot.hpp` | `Snapshot<T>`: the latest complete state from one writer thread to one reader thread; three reused buffers, no copying, no blocking (P3). A triple buffer: the writer and the reader each own one buffer, and hand the third over with one atomic exchange (acquire-release) that also says whether it holds an unread state (WP 4.1) | new |
| `thread_pool.hpp` | `ThreadPool`: `parallelFor(count, fn)` cuts a loop into contiguous parts, one per worker and one for the calling thread, which works too; by default two workers fewer than hardware threads, so one core stays free (P8). The parts depend only on the count, the workers and a minimum part size, so `fn(start, end, part)` can keep per-part data and get the same result every run. Idle workers sleep; a loop inside a part runs on its thread; calls from several threads take turns; the first exception is thrown again on the caller (WP 5.2, D68) | new |
| `random.hpp` | `Random`: PCG32, 16 bytes, the same numbers for a seed on every platform (its own helpers instead of the standard distributions): `range` (floating point half-open, integers inclusive), `below`, `chance`, `normal`, `angle`, `unitVector<V>`, `inDisc<V>`; a fixed seed by default, `fromEntropy()` to differ per run; meets the standard's generator requirements. `RandomStreams`: one generator per part of a `ThreadPool` loop from one seed, reproducible whichever thread runs which part. `threadRandom()`: a generator per thread from entropy, for quick use (WP 5.3, D69) | new |
| `timing.hpp` | `Stopwatch` (steady clock, seconds or milliseconds, `restart`); `Cooldown`: fires every N seconds of the time it is given (`advance(dt)` returns how many periods ended; left-over time carries over; `progress()` for a bar), so it follows simulated time in a tick and the wall clock in a frame; `RunningAverage`: the average of the newest N values, its sum computed afresh once per window; `ScopedTimer`: the milliseconds of a scope into a `Series` or a `RunningAverage`. The fixed timestep stays in the simulation runner (WP 5.4, D70) | new |
| `easing.hpp` | Easing functions | from `ui/utils/functions` |
| `interpolated.hpp` | Animated value with easing | from `ui/utils/interpolated` |
| `grid.hpp` | `Grid<T>`: cells row after row in one block, `int` coordinates or any point with `x` and `y`; `grid(x, y)` unchecked (asserted in debug builds), `at` checked, `contains`, `wrapped` for joined edges; `cells()`, `row(y)` and `rows(first, end)` as spans, the last for a `parallelFor` over the height; `forEachNeighbour` with four or eight neighbours, skipping or wrapping at the border; `fill`, `resize`, deep copies and a `swap` that costs nothing, for reading one grid and writing another. Every `T` is stored as itself, `bool` too (WP 5.5, D71) | new |

The utility list for the first version is fixed by D67. Utilities that need SFML (textures by name, the quad batch) are in `app` (section 5).

## 4. `ui` — one owner per concern (D1)

### 4.1 Public headers (`include/atpl/ui/`)

| File | Content |
|---|---|
| `ui.hpp` | `UI`: the facade. Built from a window reference and a `UISetup`. `widget(name)`, `view(name)`, `panel(name)`; the frame steps `handleInput()`, `update()`, `draw()`; `setProfilerVisible()`; `requestRedraw()` (any thread); events (WP 1.3). |
| `event.hpp` | `Event` and the twelve event types (D29). |
| `value.hpp` | `Value`: a widget's value of any kind, as carried by `ValueChanged`. |
| `error.hpp` | `SetupError`: thrown for mistakes in setup or addressing. |
| `id.hpp` | `PanelId`, `WidgetId`, `ViewId`. |
| `setup.hpp` | `UISetup` (background view, grid, panels, profiler readout), `PanelSetup`. |
| `placement.hpp` | `Anchor`, `GridCell`, `GridSpan`, `Placement` (D25, D44). Grid places are used for panels in the window and for widgets in a panel. |
| `layout.hpp` | The layout theme (D44): `Layout` with its `Metrics`, `Scaling` and the defaults for placing panels and content; `PanelLayout`, what one panel does differently; `Sizes`, the sizes for the window as it is; `Alignment`, `Fit`, `SizeRule`; the presets in `layouts::`. |
| `widgets.hpp` | The widget pool. Each widget has one public type, its descriptor: `Button`, `Switch`, `Slider`, `ProgressBar`, `ValueDisplay`, `TextDisplay`, `TextInput`, `Dropdown`, `Graph`, `Log`, `Paragraph`, `View` (D24). A descriptor also declares its widget's parts. `WidgetSetup` is what a panel stores; any type with a name and a `create()` converts to it, including app-defined ones. |
| `handle.hpp` | `WidgetHandle` (`bind`, `unbind`, `get`, `set`, `setEnabled`), `ViewHandle` (`onDraw`, `rect`), `PanelHandle` (`setCollapsed`, `setVisible`, `rect`). Light values; the app does not keep them (P2). |
| `binding.hpp` | `ValueKind`, the interfaces `Binding<T>` (bool, number, index, text) and `SeriesBinding`, and `AnyBinding`, which everything bindable converts to (P10). |
| `theme.hpp` | `Role`, `Kind`, `Part`, `State`, `PartStyle`, `PartOverride`; the tokens `Palette`, `Shape`, `Typography`, `Motion`; `mix` of two part styles; `Theme` and the built-in themes (P5, P11, D30, D74). |
| `widget.hpp` | `Widget`, the interface widget types implement; `MeasureContext`, `InputContext`, `UpdateContext`, `StateBlend`, `Style`, `Painter` (P1, D30, D74). |
| `rect.hpp` | Rectangle type. From `ui/utils/rect`, trimmed. |

### 4.2 Internal modules (`src/ui/`)

| Module | Owns | Reads | Must not |
|---|---|---|---|
| `model/` | Panels, widget slots, views, name index, lifetime | — | Lay out, draw, handle input |
| `input/` | Hit-testing, hover, press, drag capture, keyboard focus, what the UI consumes, emitting events (after asking `binding/` to write a changed value) | model, layout rects | Set geometry |
| `layout/` | Every rectangle and size: panel placement, widget packing inside panels, view regions, visibility | model, widget `measure`, the layout theme | Touch interaction state or colours |
| `binding/` | Making a `Param<T>`, a `Series` or two functions look like a binding; syncing: each frame it compares each bound widget's revision and, only on a change, hands the widget the new value and marks its panel dirty; it writes the user's changes into the binding | model | Touch layout, drawing or interaction state; raise events |
| `theme/` | Tokens, role defaults, part entries, built-in themes, resolving a `Style` for a part and state | — | Decide any rectangle or position |
| `render/` | Draw list, tessellation, per-panel batches, text cache, draw order, frame flag, profiler | model, layout rects, theme | Modify the model |
| `widgets/` | One file per widget type: its behaviour only | its own state, contexts handed to it | Position itself, issue draw calls, read theme tokens directly |
| `ui.cpp` | Calling the modules in order; the public facade | everything | Contain logic of a single concern |

Files:

```
src/ui/
├─ ui.cpp     frame_loop.hpp/.cpp
├─ model/    panel.hpp  widget_slot.hpp  view.hpp  store.hpp/.cpp  name_index.hpp/.cpp  setup.cpp
├─ input/    input_system.hpp/.cpp  input_context.cpp  event.cpp  window_events.hpp/.cpp
├─ binding/  any_binding.cpp  sync.hpp/.cpp
├─ layout/   layout.cpp  arrange.hpp/.cpp  panel_placement.hpp/.cpp  widget_layout.hpp/.cpp
│            grid_packing.hpp/.cpp  packing.hpp/.cpp  cell.hpp/.cpp  rules.hpp/.cpp
│            measure_context.cpp  overlay_placement.hpp/.cpp
├─ theme/    theme.cpp  presets.cpp
├─ render/   renderer.hpp/.cpp  draw_list.hpp/.cpp  shapes.hpp/.cpp  painter.cpp
│            panel_batch.hpp/.cpp  text_measurer.hpp  text_renderer.hpp
│            text_layout.hpp/.cpp  font_measurer.hpp/.cpp  text_cache.hpp/.cpp
│            profiler.hpp/.cpp  frame_stats.hpp
└─ widgets/  panel_frame.hpp/.cpp  shapes_of_widgets.hpp  number_format.hpp  utf8.hpp  paragraph.cpp
             button.cpp  switch.cpp  slider.cpp  value_display.cpp  progress_bar.cpp
             graph.cpp  dropdown.cpp  text_input.cpp  view.cpp
```

### 4.3 The model

The model is one `Store`, built once from the `UISetup`. Nothing is added or removed afterwards.

- **Panel**: name (unique, Q8), title, placement, columns, its three colours, collapsed, visible and hovered state (P7), its widgets, one dirty flag; from input its scroll offset and whether its scrollbar is hovered or dragged; and from layout its rectangle, the height of its content (and of a grid's rows), and whether it is shown. `visible` is what the application wants; `shown` is whether the panel is actually on screen.
- **Widget slot**: what the framework keeps for every widget, whatever its type.

  | Field | Written by |
  |---|---|
  | name, panel, grid cell, colours, the widget object, its view | model (at setup) |
  | rectangle (in the panel's content), visible, where its open overlay is | layout |
  | hovered, pressed, focused, overlay open | input |
  | enabled | app, through `WidgetHandle::setEnabled` |
  | binding, and the revision last seen | app, through `WidgetHandle::bind` or the descriptor; kept in step by `binding/` |

- **View**: name, the view widget it belongs to (none for the background view), its rectangle (layout), its draw function (app).
- **Widget object**: an implementation of the `Widget` interface. It holds only type-specific state (slider value, animation progress, dropdown open). It is made by its descriptor's `create()`, once.
- **Ids** are dense indices, so every lookup by id is a direct array access: panels in the order of the setup; widgets panel by panel, so a panel's widgets are a row of slots; views with the background view first. Names are resolved to ids once, when a handle is requested.
- **Names** (`NameIndex`): panel names are unique; widget names are unique per panel; view names are unique among views. `"Speed"` works if it is unique in the whole UI, otherwise `"Controls/Speed"`. A name must not be empty or contain `/`. Duplicates, unknown and ambiguous names fail loudly (Q8), with a message that names the offender and, where it helps, what would have been right: the existing panels, or the `"Panel/Name"` forms to choose from.
- **Colours**: a panel's `main1`, `main2` and `accent` are indices into the theme. A widget's colours are its panel's with the fields of its `colored(...)` override replacing them, worked out once at setup. `Store::requireColors(theme)` throws for an index the theme does not have; it runs when the store is built and before a theme replaces another.
- **The dirty flag** of a panel is set by whoever changes how the panel looks (input, bindings, layout). The facade takes the flags before drawing and marks the panels' batches; render never writes to the model.
- **What the setup hands over** (`WidgetSetup`): the name, the grid cell from `at(...)`, the colours from `colored(...)`, and the descriptor itself to make the widget from. Two optional members of a descriptor are picked up by name, for built-in and app-defined widgets alike (D39): `binding` (a `std::optional<AnyBinding>`) is what the widget is bound to from the start, and `static constexpr bool isView = true` makes the widget a view.

### 4.4 The widget interface (P1, D30)

```cpp
class Widget {
public:
    virtual ~Widget() = default;

    virtual SizeRequest measure(const MeasureContext&) const = 0;  // minimum, maximum, shape
    virtual bool handleInput(const Event&, InputContext&);         // true = used
    virtual void update(float dt, UpdateContext&);                 // animations
    virtual void paint(Painter&, const Style&) const = 0;          // emit shapes
    virtual bool reactsToPointer() const;                          // false: only shows, never hovered
    virtual void focusLost(InputContext&);                         // a text input reports here

    virtual FloatRect overlayAnchor(const MeasureContext&, sf::Vector2f size) const; // what it hangs on
    virtual sf::Vector2f overlaySize(const MeasureContext&, float maxHeight) const; // an open list
    virtual void paintOverlay(Painter&, const Style&, const FloatRect& anchor) const;

    virtual bool accepts(ValueKind) const;                         // what it can be bound to
    virtual bool editsValue() const;
    virtual std::optional<Value> value() const;
    virtual void setValue(const Value&);                           // from the binding or the app
    virtual void setSeries(const SeriesBinding*);                  // graphs only
    virtual void setLines(const LinesBinding*);                    // logs only
};
```

Rules:
- **Size.** A widget answers with the least it needs and, if it has one, the most it makes use of (`SizeRequest`, 4.6). It never decides its own size or place.
- **Input.** `handleInput` gets the same `Event` types the application gets; the context converts pointer positions into the widget's own coordinates. Hovered, pressed, focused and disabled are kept by the UI and read through the context.
- **Values.** A widget never sees what it is bound to. It keeps its own copy of its value; `binding/` hands it new values through `setValue`, and the widget reports the user's changes through `InputContext::changeValue(value, final)` or `press()`. An app-defined widget therefore contains no binding code. Graphs are the exception: they are handed their series source and read from it.
- **Outside world.** A widget acts only through the context it is handed: `markDirty()`, `capturePointer()`, `requestFocus()`, `changeValue()`, `press()`, `openOverlay()`.
- **Time** (WP 6.1, D74). Hover, press and focus ease in and out without widget code: the UI keeps a `StateBlend` per widget (how far each of the three is in, 0 to 1) and moves it towards the widget's state over the theme's `Motion` times, exponentially, so it eases out and turns back from where it is without a jump. `Style::part` then mixes the looks of the states by those blends (`mix`: colours, outlines, corners, shadows, line thickness; text keeps the nearer end's size; a part shown at one end only fades). A widget's own transitions go through `update(dt, context)`, which the UI calls once per update for every visible widget, after bound values were handed over; the widget moves its own number and paints with `style.part(part, state, amount)`. The built-in switch slides its knob and fades its track; the dropdown turns its arrow. Every step calls `context.markDirty()`, which repaints only the widget's panel; once nothing moves, no frames are asked for. A theme whose motion is all 0 has no animations.
- **Overlay** (WP 3.11). A widget can have one thing drawn above every panel that reaches beyond its rectangle: a dropdown's list. `InputContext::openOverlay()` opens it and closes any other. While it is open the widget's state has `State::Open`, all pointer input goes to the widget, and `InputContext::overlay()` says where the overlay is in the widget's coordinates. A press anywhere but on the widget or its overlay closes it and is used up: it is not forwarded, and nothing under it sees it. The UI also closes it when the widget's panel folds or is hidden, or the widget is hidden or disabled.

Adding a widget type means one file in `widgets/` and one descriptor in `widgets.hpp`. An application adds its own by implementing the interface and writing a descriptor; `tests/api/widget_usage.cpp` shows a complete one.

Placing the overlay (`layout/overlay_placement`, D28): attached to the widget's anchor (`overlayAnchor`: a dropdown's field, by default the whole widget), right below it with no gap and as wide as it; right above it if it does not fit below; if it fits neither way, on the side with more room, asked again with only that much (`overlaySize(context, maxHeight)`, so a list shows fewer entries), and kept inside the window. It never comes closer than the margin to the window's edges. The facade places and paints it in `draw()`, into one batch drawn after every panel (4.9): again when it opens, moves or changes size, or when its widget's panel is redrawn. `paintOverlay` is told where the anchor is and may draw over it, to make the two one shape.

### 4.5 Bindings (P2, D10, P10)

A widget type states the **kind of value** it works with, not a C++ type. It binds to a small interface for that kind: get, set (if editable) and a change counter, which the UI uses to mark the panel dirty.

| Widget | Kind | Ready-made binding |
|---|---|---|
| Button | none (emits an event) | optionally `Param<bool>`, set on click |
| Switch | bool | `Param<bool>` |
| Slider | number | `Param<float>`, `Param<int>`, `Param<double>` |
| Progress bar | number, read-only | `Param<float>` |
| Value display | text, read-only | `Param<number>` plus a format, or `Param<std::string>` |
| Text display | text, read-only | `Param<std::string>` |
| Text input | text | `Param<std::string>` |
| Dropdown | index into its options | `Param<int>` or `Param<AnyEnum>` |
| Graph | series, read-only | `Series` |
| Log | lines, read-only | `TextLog` |
| Paragraph | none: static text, set in the setup (D56) | — |
| View | none (draw callback) | — |

Rules:
- Everything the template ships (`Param<T>`, `Series`, `TextLog`) is thread-safe.
- Plain variables cannot be bound directly.
- An application may implement a binding interface over its own data, or build one from a getter and a setter: `ui.widget("Speed").bind(getter, setter)`. Thread safety of such a binding is the application's responsibility.
- What a widget **is** (range, step count, option labels) is part of its descriptor, not of the binding.
- Numbers of every C++ type travel as `double`, enums as the index of the enumerator. Exact for `float`, for integers up to 32 bits and for 64-bit integers up to 2^53; larger 64-bit values are bound as text to be shown exactly. Writing back rounds integers and clamps to the type's range (`numberTo<T>`).
- A descriptor only accepts sources of its widget's kind, checked by the compiler. Binding by name is checked when it runs and throws `SetupError` for a wrong kind.

How it is built (`binding/`, WP 3.8):
- **`AnyBinding`** holds a pointer to one of the six interfaces. For a `Param<T>`, a `Series`, a `PointSeries`, a `TextLog` or two functions it makes an adapter and keeps it alive; an application's own implementation is used as it is. `get`, `set` and `revision` work in the kind's type (`Value`), so the UI never sees the application's types.
- **Bindings of functions** notice changes by asking the getter when the revision is asked for.
- **Points**: a `PointSeries` is bound like a `Series`; its binding says `hasPoints()` and gives the points through `readPoints`, its y values through `read`.
- **Lines** (WP 3.18): a `TextLog` is bound as kind `Lines` through `LinesBinding` (`read` the newest lines, `size`, `pushed`, `revision`). Like a graph, a log is handed its source (`Widget::setLines`) and reads it when it paints; sync only marks its panel when the revision changes.
- **Syncing** (`binding::sync`, every `UI::update`): for each bound widget, the revision is compared with the one the widget last got; only on a change is the value handed over (`Widget::setValue`) and the panel marked dirty. A series only marks its graph's panel: the graph reads the series itself. A widget hears of changes at most as often as `Widget::refreshInterval` says: by default at once for widgets that edit their value or show a series, every 125 ms for values that are only shown; a widget type can ask for something else. A change in between is not lost, it arrives with the next hand-over.
- **Writing** (`binding::write`): what the user enters goes into the binding before `ValueChanged` is raised, and the widget is not handed it back. `WidgetHandle::set` writes the same way, without an event.
- **Checks**: a binding of a kind the widget does not work with, or a read-only binding for a widget that edits its value, throws `SetupError` naming the widget: in `bind`, and for the bindings of the setup when the UI is built.

### 4.6 In-panel layout (P12, D44)

> The layout concept is [LAYOUT.md](LAYOUT.md) (D44). This section, 4.6a and 4.6b describe what is built so far; they are brought in line with it by WP 3.15 to 3.17.

- **Size requests** (`SizeRequest`, LAYOUT.md §5): a widget reports through `measure()` the least it needs (`min`), its normal size (`preferred`) and, if it is *constant* (slider, button), the most it makes use of (`max`); a *dynamic* widget (graph, view) has no maximum. Both can limit their shape with two ratios. All three come from the layout's sizes and so follow the window. Layout offers the widget the width of its column or cells, so that a height that depends on the width (wrapping text) can be worked out.
- **A widget's rectangle in its room** (`layout/cell`): a dynamic widget takes all of it, a constant one at most its maximum, both within their ratios. What is smaller than its room is placed in it at one of nine positions, the layout theme's `widgetAlignment`, which a panel can override. Rectangles are on whole pixels.
- A panel has a number of equal columns (`PanelSetup::columns`, 1 to 3, default 1). Its widgets are placed in one of two ways:
  - **Packed**: every widget is as high as it prefers, and `layout/` stacks them top to bottom into the columns with balanced heights, in the order listed (`packing.hpp`). Height the panel has to spare goes to the dynamic widgets of each column; what is below them moves down. A panel too low for its packed widgets scrolls; they are not squeezed.
  - **Grid**: the content is split into equal columns and equal rows, and a widget gets the cells it takes. A cell is as large as the largest preferred size among the panel's widgets, a widget that spans cells counting with its size divided over them. Height the panel has to spare is shared equally among the rows, and widgets grow up to their maximum; a panel short of height squeezes its rows down to the largest minimum before its content scrolls.
- **When a panel is a grid** (`layout::prepareWidgets`): if its size is given from outside (top-down: it fills cells of the window's grid), if the layout theme asks for equal cells (`Layout::rows`), or if the panel itself names rows, or a widget a position (`at`) or a span (`spanning`). Otherwise it is packed. This is decided when the UI is built and again when the layout theme changes.
- **Finding cells in a grid** (`layout/grid_packing`), the same for widgets in a panel and for panels in the window:
  1. Things with a position take their cells.
  2. The others follow, the larger ones first and equal ones in the order listed, each into the first free cells that hold it, looking row by row from the left. No rearranging.
  3. A panel's grid has the rows the panel names; if it names none, as many as its positioned widgets use; if no widget has a position either, as many as the widgets need.
  4. What finds no room is a `SetupError`, as is a cell outside the grid and, for widgets, two positions on the same cell.
- A row nobody uses stays empty and works as a separator.
- Rectangles are in the panel's content, whose corner is below the header. The theme's `padding` lies between the panel's edge and the widgets, its `gap` between widgets. A widget never positions itself.
- Inside its own rectangle, a widget arranges its parts (label, track, knob). `paint` and `handleInput` use the same part rectangles, computed in one place per widget. In a grid a widget can be given more or less height than it asked for, so it must cope with any size.
- Sizes (margin, padding, gaps, row height, header height) belong to the layout theme (`Layout::metrics`), not to the theme. They follow the window: `Layout::sizesAt(window)` gives the `Sizes` in effect, in whole pixels, with widths scaled by the window's width, heights by its height, margins by the smaller of the two so that they are the same on all sides, and a gentler factor for text, outlines and radii (LAYOUT.md §6). Only `layout/` and `measure()` read them. Nothing else defines sizes.
- **The order of a layout pass** (`layout/arrange`): a panel's width follows from the window alone; with it the widgets are laid out, which gives the content height; with the content heights the panels are placed; a panel with height to spare gives it to the rows of its grid or to its stretching widgets; views are where their widgets ended up. `contentOverflow` says how far a panel's content can be scrolled (WP 3.12).

### 4.6a Placing panels (R5, D25)

Each panel states its own placement:
- **`Anchor`**: the panel floats at an edge or corner of the window, on top of the background view and of the grid. Panels that share an anchor are stacked in the order they are listed.
- **`GridCell`**: the panel fills one or more cells of the window's grid (`UISetup::grid`, equal cells).
- **`GridSpan`**: the panel fills that many cells of the window's grid, wherever there is room: the first free cells that hold it, larger panels first (4.6).

Both kinds can be mixed; floating panels lie on top of the grid. The rules, in `layout/panel_placement`:
- **Floating**: a panel is `margin` away from the window's edges. Panels that share an anchor form a stack with `margin` between them: downwards from a top anchor, upwards from a bottom anchor (the first listed is the lowest), centred as a whole for `Left` and `Right`. A panel is as wide as its setup says (times the GUI scale) or as the theme's `panelWidth`, and as high as its header plus its content, or its header alone when collapsed.
- **Panel sizes** (`layout/arrange`, LAYOUT.md §3): a panel in the window's grid either fills its cells (`Fit::Fill`) or is as large as its content and sits in its cells at the rules' alignment (`Fit::Content`). Any panel sized by its content is as wide as the widest of: what its widgets prefer, its title, and the layout's `panelWidth`; a width the panel names itself wins. Floating panels with `SizeRule::Equal` all get the widest width, or the highest height, among them. A floating panel never exceeds its limit, `min(fixed pixels, fraction of the window)` per axis; content that no longer fits scrolls or is dropped. A collapsed grid panel is its header, at the side of its former rectangle that `collapseTowards` names; floating panels collapse within their stack. With `Layout::cells` set to `Equal`, all panels sized by their content whose content is a grid share one cell size, the largest any of them wants.
- **Grid**: equal cells, with `margin` around the grid and between cells. A panel covers its cells and the margins between them. Edges are rounded to whole pixels so that neighbours line up. A collapsed grid panel is its header at the top of its cells. Panels with a `GridCell` may share cells (D40); panels with a `GridSpan` are placed clear of all others, and one that finds no room is a `SetupError`.
- A panel the application hides (`PanelHandle::setVisible(false)`) leaves no gap.
- A panel whose size changed, or that appears, is marked dirty; one that only moved is not (the batch is moved, not rebuilt).
- `preparePanels` finds the cells when the UI is built and refuses, with `SetupError`, a grid without columns or rows, a cell or span outside the grid, a panel without room, and a negative width.
- **Views**: the background view is the whole window; a view widget's view is its widget's rectangle in the window, or empty while the widget is not on screen.

Placement runs when something changed that moves or resizes panels (window size, collapsed, visible, content height, theme), not every frame.

Typical arrangements:
- Simulation as background, controls floating over it: a background view plus anchored panels.
- Simulation inside the layout: a grid, one panel with a `View` widget spanning most cells, control panels in the rest.
- Main view plus minimap: either of the above with a second `View` widget in a small panel.

### 4.6b Overflow (D28)

What happens when things do not fit. Rules for a start; to be revisited if they do not work in practice.

| Case | Rule |
|---|---|
| A panel's content is higher than the panel can be | A grid squeezes its rows down to the widgets' minimum; beyond that, and in a packed panel at once, the content scrolls vertically inside the panel (mouse wheel, thin scrollbar). The header stays fixed. |
| A widget's room is narrower than the widget needs at least | The widget is not drawn; its place stays empty. |
| A panel's content area is narrower than its widest widget needs, or lower than its highest | The panel is not drawn; the others are placed without it. It comes back when there is room. |
| A stack of floating panels is higher than the window (the default) | Every panel keeps its header. The height that is left is shared among the contents of the expanded panels: none gets more than it needs, and what the small ones leave goes to the others in equal parts; they scroll inside. If not even the headers fit, the last panels of the stack are not shown. |
| The same with `Layout::stackOverflow = StackOverflow::Cards` (WP 3.14, D57) | The collapsed panels overlap like a stack of cards, only as far as needed: each is covered by the next one down to a strip in which its title can be read (`layout::cardStrip`). Expanded panels keep their height while there is room and share it when there is not. Only if not even the strips fit are the last panels not shown. Of two cards, the one further from the anchor covers the other, so every title stays in view; the card under the pointer comes to the front (`layout::cardOrder`), which changes only the order of drawing and hit-testing. Unfolding a card folds the other open ones of its stack if they would not fit together (`layout::cardsToFold`, done by the facade, also for `PanelHandle::setCollapsed`). |
| A grid-cell panel in a small window | Its cell shrinks with the window; its content scrolls. With no room at all left, it is not shown. |
| A floating panel wider than the window | Its limit (a fraction of the window) keeps it narrower; beyond that the margin at the sides shrinks first, then the panel's width is clamped to the window width. |
| Text wider than its widget | Cut off with an ellipsis. |
| A dropdown list that would leave the window | Opens upward, or is clamped to the window. |
| Horizontal scrolling | None. |
| Very small windows | The application may set a minimum window size (app setup). |

Scrolling changes only an offset and a clip rectangle of the panel's batch; the geometry is not rebuilt (cache level 2 stays valid). The scroll offset is interaction state and belongs to `input/`; the content height and the visible height are layout outputs.

How it works (WP 3.12, D55):
- **Range**: from 0 to `layout::contentOverflow`, what the content is higher than the content area (the open height counts while a panel folds). `InputSystem::clampScroll` keeps every offset within it after layout; a panel that is not shown or folded keeps its offset for when it comes back.
- **Wheel**: in a panel whose content overflows, the wheel scrolls the content and no widget gets it, so nothing in the panel is moved by accident while it passes under the pointer. In a panel that does not overflow, the wheel goes to the widget under the pointer. One notch is the same in every panel (`layout::scrollStep`): the layout's row height, or the lowest row of a grid on screen if that is lower.
- **Scrollbar** (`Panel::Scrollbar`): a thin, fully rounded thumb in the padding at the right of the content area, shown only while the content overflows, so no widget is narrowed. It takes the title's colour under the pointer and while dragged. Its thumb is as much of the track as the content area is of the content. Dragging the thumb scrolls; a press beside it moves the thumb's middle there first.
- **Content area** (`widgets::contentArea`): below the header, and above a margin as high as the padding at the bottom, so that content cut off there ends before the panel's border. Widgets are clipped to it.
- **Hit-testing** takes the offset into account: a widget's place on screen is its place in the content, moved up by the offset; what is scrolled out of the content area is not there: under the header the header is, in the margin at the bottom nothing.
- **Hover**: a scroll forgets which widget is hovered, and the next move finds it, as after layout. So a scroll repaints the panel at most once, to drop the hover, and never with every notch.
- **Drawing**: the facade gives the batch the offset and the thumb's place every frame; the thumb is painted once at the top of its track into the batch's scrollbar layer and moved down it by an offset. The panel is painted again only when the thumb appears, disappears or changes length, or its look changes.

### 4.7 Views (Q3)

The application's own rendering appears in views:
- **Background view**: the whole window, behind all panels. Optional.
- **View widget**: a region inside a panel. Any number of them (main view, minimap, ...).

Both are the same thing to the app: a named view with a draw callback, set through `ui.view("name").onDraw(...)`. The renderer calls it at the right point in the draw order, with drawing clipped to the view's rectangle. The app calls `UI::requestRedraw()` (thread-safe) when it has something new to show. There is one such call for the whole UI, not one per view, because a frame is always drawn as a whole.

View regions are clipped to rectangles; a view cannot have rounded corners unless it is rendered to a texture first (opt-in, D7).

How it is built (WP 4.4, D63):
- **The view widget** (`View`): a height of its own (`ViewOptions::height`, scaled like other sizes, at least two rows), or none, and then dynamic: as high as its width and `aspectRatio` say, and in a grid cell all the height the other widgets leave. It draws nothing itself but an optional `Frame` a theme can show.
- **Where a view is now** (`layout::placeOf`): its place from layout, moved up by its panel's scroll, and the part of it that can be seen, inside the panel's content area. Drawing, the pointer and `ViewHandle::rect()` use it.
- **Drawing**: the renderer calls the UI before the first panel, for the background view, and after each panel, for the views inside it, so a view is above its panel and below the panels above it, the overlay and the readout. Each draw function gets the target with (0, 0) at the view's top-left corner and one unit a pixel (a `sf::View` whose viewport is the view's place) and a scissor for its visible part; the target's view is restored after it. A view without a draw function, or with nothing visible, costs nothing.
- **Keys and selection** (WP 4.5, D65): a press on a view widget selects it. It takes the keyboard focus, its outline shows it (`View::Selection`, drawn by the UI above what the application draws into the view), and keys and text are forwarded for it (`KeyPressed::view`, `isFor`) until a press anywhere else, also when the pointer has left it. While no view is selected, keys go to the setup's default view (`UISetup::defaultView`, usually the main view, often the background), which never shows an outline and needs no selecting; without one they carry no view. A widget that takes keys itself (a text input) still gets them while it has the focus.
- **Input**: over a view widget the pointer is as over no panel: presses, drags, moves and the wheel are forwarded with the view's name and the position in it, a drag that starts there stays the application's, and the wheel over a view goes to the application even in a panel that scrolls, so that a view can zoom. The view widget itself gets no input and is never hovered.

### 4.8 Events (P4, D11, D15, D29)

One `Event` type, one stream, read on the main thread after `UI::handleInput()` through `UI::events()`:

| Group | Types |
|---|---|
| Widget events | `ButtonPressed`, `ValueChanged` (with the new value and `final`: false while a drag or typing is still going on) |
| Pointer | `PointerPressed`, `PointerReleased`, `PointerMoved`, `Scrolled` |
| Keyboard | `KeyPressed`, `KeyReleased`, `TextEntered` |
| Window | `WindowClosed`, `WindowResized`, `WindowFocusChanged` |

- The types are the template's own; `sf::Event` does not appear. Keys and mouse buttons use SFML's enums. The list can grow.
- Widget events carry the widget's id, name and panel name, so they can be matched by `"Name"`, `"Panel/Name"` or id.
- Pointer and key events carry where the pointer is: window position, the view under it (if any) and the position relative to that view.

What the UI keeps for itself, and what it forwards:
- Kept: pointer presses, releases, moves and scrolling over a panel; keys and text while a widget has keyboard focus.
- A press and everything up to its release go to the same place. A drag that starts in a view keeps being forwarded when it crosses a panel; a drag that starts on a widget is never forwarded. Input is never cut off in the middle of an interaction.
- Window events are always forwarded. A close request is only reported; the application decides.

### 4.7a The built-in widgets

One file per widget in `widgets/`, each a descriptor (public, `widgets.hpp`) and a widget class (internal). They follow the design agreed in WP 2.2. Sizes come from the layout's (`widgets/shapes_of_widgets.hpp`: knob size, track shares), so they follow the window.

| Widget | Look | Input | Value |
|---|---|---|---|
| Button (WP 3.9) | a face with its label in the middle | pressed on, released on: `press()`; a press dragged away does nothing | none; a bound `Param<bool>` is set on every press |
| Switch (WP 3.9) | its label at the left, a pill at the right; on: the track in the accent look | a click flips it: `changeValue(on)` | Bool |
| Slider (WP 3.9) | label at the top left, value at the top right in its `format`, the track below, filled up to the knob; `Ticks` hidden unless a theme shows them | a press on the track row moves it there and dragging follows (`final` false), the release reports `final` true; the wheel moves it a step or a hundredth of the range | Number, kept in `[min, max]` and on `step` |
| ValueDisplay (WP 3.10; called TextDisplay until WP 3.19) | label at the left in the muted text, value at the right | none | any but a series, shown only: numbers in its `format`, on/off values as "on" and "off", choices as their number |
| TextDisplay (WP 3.19) | label above in the muted text, the text below as a block of `lines` lines; it wraps within them, line breaks start new lines, and what does not fit ends in an ellipsis on the last line; `align` places each line | none: it does not react to the pointer | Text, shown only (refresh as D51). Its height is fixed by `lines`, never by the text (D56) |
| ProgressBar (WP 3.10) | label above, a bar below, filled in the accent look as far as the value is between `min` and `max` | none | Number, shown only |
| Graph (WP 3.10) | label above with the newest value at the right (`Value`, hidden unless a theme shows it), a box below with the curve; the baseline at zero or at the average (`base`); optional parts a theme can show: `Shadow` (the area between curve and baseline), `Grid`, `Axis`, `AxisLabels` | none | Series or PointSeries, read by the graph itself when it paints |
| Log (WP 3.18) | label above; a box with the newest lines, the newest at the bottom; a line too wide is cut with an ellipsis, never wrapped; optional `Time` (the time of day each line was pushed, hidden unless a theme shows it); a thin scrollbar thumb in the box's right inset when it holds more than it shows | the wheel (where its panel does not scroll, D55) and its scrollbar scroll it back; scrolled back it keeps showing the same lines while new ones come, and at the end it follows again | Lines (`TextLog`). Its height is fixed by `lines`, never by the text, so new lines never run layout (D56) |
| Dropdown (WP 3.11) | label above; a field with the chosen entry and an arrow, pointing up while open; the open list in the overlay, joined to the field: one outline around both in the open field's outline colour, and that outline once more where they meet; the highlighted entry in the accent look, a scrollbar when it scrolls | a click on the field opens the list; a click on an entry, or a press dragged to one and released, chooses it; a click elsewhere or on the field closes it. While open: Up, Down, Home, End move the highlight, Enter chooses, Escape closes, the wheel scrolls. It shows `maxVisible` entries at most, fewer if there is less room | Index; an integer parameter is bound as one |
| TextInput (WP 3.11) | label above; a field with the text, or the muted `placeholder` while it is empty; a steady cursor while focused, the outline in the accent colour; dots where text is hidden: while editing, at the left once it has scrolled and at the right if text follows; afterwards it is shown from its start, with dots at the right | a click takes the focus and puts the cursor there; typing, Left, Right, Home, End, Backspace, Delete, Ctrl+V; Enter or Escape gives the focus up. Text wider than the field scrolls with the cursor. At most `maxLength` characters (code points, not bytes) | Text; written on every change (`final` false), once more with `final` true when the editing ends (`Widget::focusLost`) |
| Paragraph (WP 3.13) | a heading, a body and a footer, each optional and each a part of its own text type; the body, static text, is quieter than the values of widgets and set apart from its heading: its colour and its size lie between the heading's and the footer's, nearer the footer (0.65 of the way), unless a theme sets them; all three wrap to the widget's width, line breaks start new lines, and empty ones take no room; `align` places all three; `Separator` lines between them if a theme shows them, in a gap that is there either way | none: it does not react to the pointer | none: static. As wide as its longest line, at most a panel of the layout's width (it wraps instead), and at least its longest word; as high as its wrapped texts at the width it gets |

The displays only show their values, so by default they hear of changes at most every 125 ms (D51); a graph asks for every change. A graph's value axis is fixed where `min` and `max` say so and otherwise follows the samples in its window: from zero (`GraphBase::Zero`) or around their average (`GraphBase::Average`), on a logarithmic axis in whole powers of ten. Its window is the last `samples` samples, or all there are (up to 1024). The x-axis counts samples back from the newest (`GraphX::Count`) or seconds (`GraphX::Time`, with `secondsPerSample`); a graph of points takes it from the points.

Options that cannot work (max not above min, a negative step, a logarithmic axis down to zero or below, no time between samples, a format that cannot show a number) throw `SetupError` when the UI is built.

### 4.8a The panel frame (WP 3.6)

`widgets/panel_frame` is the panel as an element of its own:
- **Parts** (`Panel::...` in setup.hpp): `Background` (with the outline and shadow), `Header` (an area behind the title, not shown unless a theme wants it), `Title`, `Arrow` (the chevron of a collapsible panel), `Underline` (a line below the header, not shown unless a theme wants it), `Scrollbar`.
- **Folding**: a click on the header of a collapsible panel folds it to its header, the next unfolds it (`input/` sees the click; `PanelHandle::setCollapsed` does the same). It takes `Layout::foldSeconds` (default 0.18 s), eased at both ends. While it moves, `Panel::opening` says how far open it is, layout gives it a height in between, the panels after it in a stack follow, and its content keeps the place it has in the open panel and is cut off at the panel's edge (the batch's content clip). A panel is never left out for being too small while it moves.
- **The arrow** points down while the panel is open and to the right while it is folded, turning on the way. Under the pointer it takes the title's colour. A theme can hide it.
- `UI::update()` moves folding panels on by the time since the last update and keeps frames coming while one moves; at rest nothing is drawn.
- `UI::handleInput()` waits at most for the first event of a pass (`frame::nextEvent`) and takes the rest as they are (`frame::pendingEvent`), so a stream of pointer moves cannot hold up the frame.

How `input/input_system` does it (WP 3.5):
- **Views**: over a view widget, input is the application's (4.7).
- **Hit-testing**: panels from the top down, in the stacking order (`Store::stackingOrder`: the window's grid panels at the bottom, floating panels above them, each in setup order; the renderer draws in the same order). In the topmost panel under the pointer, its header, its scrollbar, or the widget under it (scrolled, 4.6b) that is drawn, enabled and reacts to the pointer (`Widget::reactsToPointer`; the display widgets do not, WP 3.10), so a widget that only shows never looks hovered.
- **Owner of a press**: when a button goes down, the press belongs to the UI (over a panel) or to the application (anywhere else) until the last button is up. While the UI owns it, pointer input goes to the pressed widget, or to the one that captured the pointer; while the application owns it, everything is forwarded, over panels too.
- **Hover, pressed, focused, overlay open** are written into the widget slots, and a change marks the panel dirty. A hover is forgotten when layout runs and found again with the next move.
- **Focus**: a widget takes it through its context. A press anywhere but on the focused widget takes it away. Keys and text go to the focused widget; without one they are forwarded. A widget that loses it, however that happens, hears of it (`Widget::focusLost`).
- **Wheel and scrollbar**: see 4.6b (WP 3.12).
- **Overlay**: while one is open, moves, presses and the wheel go to its widget; a press on neither the widget nor the overlay closes it and is used up (WP 3.11).
- **Pointer location**: the view under the pointer is a view widget when the pointer is over one, the background view when it is over no panel, and none over the rest of a panel.
- **Widget reports**: `press()` raises `ButtonPressed`, `changeValue()` raises `ValueChanged` (writing the bound value comes with bindings, WP 3.8), `markDirty()` marks the panel.

The template only forwards. It does not interpret forwarded input (D15). Pan and zoom for a view is an optional helper in `app` (section 5).

### 4.9 Rendering (D3–D7, plan section 5)

```
widget.paint() ──► Painter ──► draw list ──► PanelBatch (one vertex array + text) ──► window
```

- `shapes`: tessellation of boxes (fill or gradient, outline with gap and an optional gradient of its own, corner radius, shadow), lines, polylines and areas under a curve. All shapes become triangles so that a panel is one draw call. Edges are smoothed by the window's multisampling (`WindowSetup::antiAliasing`, 8 by default); the shapes themselves have no feathered rim (WP 6.2, D75).
- `draw_list`: what one panel draws: its triangles and its text runs. Reused from rebuild to rebuild without allocating.
- `painter` (the public `Painter`): moves a widget's own coordinates to its place in the panel and hands shapes to `shapes` and text to the draw list. `text_measurer` is the interface it measures text through, so everything above it is testable without a font.
- `panel_batch`: one batch per panel in panel-local coordinates, rebuilt only when the panel is dirty (cache level 2). It has three layers: the frame (background, header), the content (the widgets) and the scrollbar (the thumb, drawn above the content). Moving the panel and scrolling the content only change how the batch is placed when drawn: the content's offset and the thumb's; nothing is rebuilt. Scrolled content is clipped to the content area (D28).
- `text_layout`: fitting text into its room, independent of fonts: measuring a line, cutting it short with an ellipsis, wrapping at word boundaries.
- `font_measurer`: measures text with the real fonts.
- `text_cache`: draws a layer's text and keeps what it built (cache level 3). One SFML text object per text run, built when the run first appears and again only when it changes; a panel repainted with the same text builds nothing. One draw call per run (D37).
- `renderer`: draws batches in the order of the list it is given, then the overlay; at most two calls for shapes per panel. Reports draw calls and triangles per frame. `present` shows a frame in the window only if one is needed (cache level 1): if the redraw flag asks for one, or a batch changed since the last frame. Otherwise it does nothing at all. View callbacks are added in WP 4.4.
- `frame_loop` (at the root of `ui`, next to the facade): the `RedrawFlag`, which any thread may set, and `nextEvent`, which fetches the window's next event and sleeps for up to one display frame while no frame is asked for. A batch notes by itself when it was repainted, moved or scrolled, so changes inside the UI need no request; the flag is for what the UI cannot see, such as a new simulation state.
- `text_renderer`: the interface the renderer hands a layer's text to; `text_cache` implements it.
- `profiler`: measures what the UI costs (D6). Three sections of a frame are timed: build (painting the panels that changed), submit (clearing and handing the batches to the graphics card) and show (putting the frame on screen). Whoever does the work reports it: the code that paints panels wraps that in `measure(Section::Build)`, the renderer reports the rest, together with draw calls, triangles, panels rebuilt and texts built. The readout is a small batch of its own with its own parts (`Profiler::Background`, `Label`, `Value`), so a theme styles it like anything else. It is off by default, refreshed at 5 Hz and painted only when its text changes, so an idle application stays idle with the readout on; a frame drawn only for the readout is not counted in its numbers. The application switches the readout on with `UI::setProfilerVisible` or `UISetup::profiler` (D38). `tests/bench/render_bench.cpp` uses the profiler to measure a scene of 100 widgets (plan 5.5).

Draw order per frame: background view → panels in order (shapes, views, text) → overlay layer → profiler.

Within a panel, text is always drawn on top of shapes: a panel is one batch of triangles followed by its text. What must cover text, such as an open dropdown list, goes on the overlay layer.

An outline is a band of triangles between two edges, each with its own colour, like a shadow. It can fade along the box like a fill (`borderGradient` from `borderStart` to `border`: horizontal, vertical or diagonal), each point taking the colour of its position (WP 6.2). An outline that fades across its width (a glow) is still a possible extension (D36).

A shadow is solid from its size inside the shifted box and fades to nothing its size outside, over six bands along (1 - smoothstep)^2, so a quarter is left at the box's edge: the profile the reference project draws with a shader, as geometry in the panel's one batch (WP 6.2, D75).

### 4.10 Painting and theme (P5, P11, D30)

`paint` returns nothing. It calls the `Painter`, which appends shapes to the panel's draw list, in the widget's own coordinates:

```cpp
painter.box(trackRect, style.part(Slider::Track));
painter.box(fillRect,  style.part(Slider::Fill));
painter.box(knobRect,  style.part(Slider::Knob));
painter.text(labelRect, label, style.part(Slider::Label));
for (const FloatRect& tick : tickRects)
    painter.box(tick, style.part(Slider::Ticks));     // optional part: skipped unless the theme shows it
```

Who decides what:
- **The widget** declares its parts and where each one is. For every part it states a **role** and whether it is shown by default.
- **The theme** decides how each part looks and whether it is shown.
- **The painter** turns rectangle plus style into triangles, and skips parts that are not shown. Its calls are `box`, `line`, `polyline`, `text` and `wrappedText`.
- **The descriptor** decides what the widget is: range, step count, option labels. Tick positions come from the step count; whether ticks are drawn comes from the theme.

Roles:

| Role | For |
|---|---|
| `Surface` | backgrounds that hold other things |
| `Track` | recessed areas |
| `Accent` | the highlighted part, in the main colour |
| `Handle` | things to grab or click |
| `Line` | thin strokes |
| `Title` | panel titles |
| `Heading` | headings inside a panel |
| `Text` | values, button labels, paragraphs |
| `MutedText` | widget labels, placeholders, footers |

The last four are the text types. A widget never chooses a font or a text size; it gives each piece of text a part, and the part's role says which type of text it is.

Kinds and parts are open identifiers, declared on the widget's descriptor (D24), not enums owned by the template:

```cpp
struct Slider {
    static constexpr Kind kind{ "slider" };
    static constexpr Part Track{ kind, "track", Role::Track };
    static constexpr Part Fill{ kind, "fill", Role::Accent };
    static constexpr Part Knob{ kind, "knob", Role::Handle };
    static constexpr Part Ticks{ kind, "ticks", Role::Line, Shown::No };
    static constexpr Part Label{ kind, "label", Role::MutedText };
    static constexpr Part ValueText{ kind, "value", Role::Text };
    ...
};
```

The theme resolves a part's style in three layers; a later layer overrides an earlier one:

| Layer | What | Example |
|---|---|---|
| 1. Tokens | Global values, in three groups: `palette` (the main colours and accents a panel chooses from), `shape` (radii, outline, outline gap, outline gradient, line thickness, shadow), `typography` (size and font per text type) | `theme.palette.accents[0].accent = ...;` `theme.shape.outlineGap = 2;` `theme.typography.title = {.size = 18, .font = bold};` |
| 2. Role defaults | Style derived from the part's role, the tokens and the three colours of the part's panel; visibility from the part's own default | every track part is an outlined area with a small radius; ticks hidden |
| 3. Part entries | Settings for one part; only the fields that are set have an effect | `theme[Slider::Ticks].shown = true;` `theme[Button::Face].radius = 0;` `theme[Switch::Track].borderGap = 0;` `theme[Button::Label].font = bold;` |

**Colours (D34).** A theme offers a list of main colours and a list of accents. A panel is drawn in three of them, chosen by index in its setup; a single widget can deviate with `colored(...)`:

| Colour | Chosen with | Used for |
|---|---|---|
| main1 | `PanelSetup::main1` (default 0) | the panel's background |
| main2 | `PanelSetup::main2` (default 1) | the outline of the panel and of everything that can be operated; lines |
| accent | `PanelSetup::accent` (default 0) | fills (slider, progress bar, a switch that is on, graph curve); what outlines turn to in use |

Everything else is derived: text is the light or dark colour that reads best on main1, muted text lies between text and main1 and stays readable, the areas of buttons and fields are main1 moved a little towards main2, knobs have the text colour. An accent can define where a gradient starts; accent-coloured boxes then fade in from that colour. An index the theme does not have is a `SetupError`.

**Outlines.** Panels and everything that can be operated have an outline of `shape.outline` in main2. Between outline and fill lies `shape.outlineGap`, the same for every part unless a part entry sets `borderGap`. The outline stays at the box's edge and the fill moves inwards, so layout is unaffected. Knobs have no outline. A theme can let surfaces' outlines fade (`shape.outlineGradient`, `shape.outlineAccent`): from their accent at the gradient's start into main2; `colorful` does, diagonally from the top left.

**States.** The UI tracks hovered, pressed, focused and disabled per widget; a widget can add `Active` for a part that is "on".

| State | Effect |
|---|---|
| Hovered | the outline of an area moves partly to the accent; a knob takes on some of the accent |
| Pressed, Focused | the outline is the accent; a pressed area is tinted slightly |
| Active | a track is filled with the accent; text and knobs on it take the light or dark colour that reads best there |
| Disabled | everything fades |

The built-in themes: `themes::moon()` (the default: black, grey outlines, white accents as gradients, no shadows) and `themes::colorful()` (warm brown-grey with sand, green, blue and red accents, soft shadows, panel outlines that fade from the accent). A test checks both for readable contrast.

Consequences:
- A theme that sets only tokens is complete. It needs no per-widget entries.
- App-defined widgets declare their own kind and parts and look right under every theme, including themes that have never heard of them.
- More detailed styling later (per panel, per widget instance) is another override layer on top; widgets do not change.
- A different structure (a part the widget does not declare) needs widget code, not a theme change.

Sizes come from the layout theme and from the text sizes in `typography`. `Theme::resolve` takes the factor for everything it measures in pixels (text sizes, outlines, radii, shadows) from its caller, which passes `Sizes::text`; text sizes come out in whole pixels. Only `layout/` and `measure()` read sizes (4.6).

The theme is part of `UISetup` and can be replaced at runtime with `UI::setTheme`.

## 5. `app` — running an application (D32)

| File (`include/atpl/app/`) | Content |
|---|---|
| `app.hpp` | `App`: creates and owns the window (`WindowSetup`), creates the UI, runs the main loop. `onEvent(handler)` delivers every UI event on the main thread (D11); `onUpdate(handler)` runs once per pass of the loop. `run()` or `run(simulation)`; `quit()`. |
| `simulation.hpp` | `Simulation<State, Command>`: the base class of an application's simulation, and `SimulationControls` (P3). |
| `camera.hpp` | `Camera`: optional pan and zoom for one view, driven by forwarded events (D15). `visibleArea()` tells what part of the world it shows, for example for a minimap. A drag with `dragButton` that starts in its view pans, wherever the pointer then goes; the wheel in its view zooms towards the pointer, between `minZoom` and `maxZoom`. `apply` keeps where the UI put the view and what of it can be seen, and only sets what the view shows; `show` works before the view's size is known and fits once it is (WP 4.5, D64). |
| `minimap.hpp` | `Minimap`: optional helper for a view that always shows the whole world, marks what a main `Camera` sees, and steers it: in `Pan` mode a press centres the main view under the pointer and a drag keeps it there; in `Select` mode a dragged rectangle becomes what the main view shows (a click centres it); while the minimap's view is selected, the arrow keys move the main view by a share of what it shows. The main view's centre stays inside the world. The mode is set in its options and can change while it runs (WP 4.5, D65). |
| `resources.hpp` | `Resources`: finds files in the `resources` directory next to the executable; `ResourceError` when something is missing or unusable. `font(name)` and `texture(name, options)` load a file on first use and hand out the same object afterwards, valid as long as the `Resources` (copies share them); the name is the path relative to the resource directory, and a texture keeps the options (smooth, repeated) of its first call. `loadFont` and `loadTexture` load a fresh copy. Main thread only (WP 5.6, D73). |
| `quad_batch.hpp` | `QuadBatch`: many rectangles, plain or textured, in one vertex array and drawn in one call, for the application's views. Quads are added (a rectangle, a texture rectangle, or one turned around its centre), changed in place (`set`, `setColor`, corner colours for gradients), or laid out once and recoloured every frame, as for grid cells. One texture per batch; two triangles per quad. After `resize`, different threads may set different quads (WP 5.6, D73). |

### 5.1 The simulation

An application's simulation derives from `Simulation<State, Command>` and implements three functions, all called on the simulation thread:

| Function | What it does |
|---|---|
| `onCommand(command)` | Handles one command the application sent with `send(command)`. |
| `tick(dt)` | Advances the simulation by `dt` seconds of simulated time. |
| `writeState(state)` | Writes everything the main thread needs for drawing. |

The base class owns the command queue and the state exchange. The main thread draws from `state()`: the newest state the simulation had published when the current pass of the application's loop began. It stays the same for the whole pass, so every view drawn in one frame shows the same moment (D33).

`SimulationControls` are `Param`s, so widgets bind to them directly:

| Control | Meaning |
|---|---|
| `paused` | No ticks while true. Commands are still handled; a paused simulation sleeps until one arrives. |
| `tickRate` | Ticks per second of simulated time. The step size of a tick is always `1 / tickRate`, which keeps a run reproducible. |
| `speed` | How fast simulated time passes compared to real time. Changes how often ticks happen, not their size. |
| `unlimited` | Ticks follow each other without waiting. For training runs and searches. |
| `step(n)` | Runs `n` ticks while paused. |
| `ticksPerSecond`, `tickMilliseconds`, `tickCount`, `time` | Measurements, written by the simulation thread. |

Rules:
- `writeState` is not called after every tick: at most once per frame the main thread draws, and once more when the simulation pauses or stops, so the last state is always shown.
- An exception thrown by the simulation ends the run and is thrown again on the main thread, from `App::run`.
- `App::run(simulation)` starts the thread, and stops and joins it when the application ends.

How the runner (`app/simulation_runner`, WP 4.3, D62) does it:
- **Timing**: ticks have a fixed size, `1 / tickRate`. Real time that passes, times `speed`, is owed as simulated time, and ticks run while a whole one is owed. Between them the thread sleeps waiting for commands until the next tick is due. `unlimited` runs ticks back to back.
- **Falling behind**: a simulation more than 0.25 s of real time behind drops what it could not do, and a pass never spends more than 0.25 s catching up (it drops the rest then); it slows down, the application stays responsive, and pausing, steps and commands are heard after one more tick at most. `ticksPerSecond` shows the real rate.
- **Paused**: it handles commands and the ticks asked for with `step`, and otherwise sleeps, waking at least every 20 ms, so unpausing, stepping and stopping take effect within 20 ms. `step` while running is ignored.
- **States**: the main thread marks each state it takes (once per pass, after the wait for input). The runner writes and publishes a new one only after that, so `writeState` runs at most once per frame; also when it pauses, after a step or a command while paused, and when it stops. Each publish asks for a frame.
- **Measurements**: `tickCount` and `time` after every tick; `ticksPerSecond` and `tickMilliseconds` (the average time a tick takes) about once a second.
- **Failure**: an exception from `start`, `onCommand`, `tick` or `writeState` ends the thread; the main loop ends after its pass, joins the thread, and throws it again from `App::run`.

### 5.2 The application loop

- Closing the window ends the application by default (`AppSetup::quitOnClose`). Turned off, the request only arrives as a `WindowClosed` event.
- `App` loads the bundled font if the theme has none, before the window opens, so a missing resource is reported first.
- The window follows `WindowSetup`: title, size, the smallest size (`minimumSize`), whether it can be resized, anti-aliasing and vsync (on by default). Full screen uses the desktop's resolution, not `size`. Resizing is the UI's: it places everything anew (WP 3.2).
- One pass (WP 4.2): `UI::handleInput()`, then every event to the `onEvent` handler in order (and `WindowClosed` ends the run if `quitOnClose`), then `onUpdate(dt)` with the real time since the last pass, then `UI::update()` and `UI::draw()`. `quit()` may come from any thread; it asks for a frame so that the loop does not wait for input once more, and `run()` returns its code after the current pass.
- While nothing happens the loop waits for input, but never longer than one display frame (section 6), so that what another thread asks to show appears at once. An idle application therefore runs `onUpdate` about once per display frame; it is still not a clock: use its `dt`.

## 6. One frame

Main thread, one pass of `App`'s loop:

| Step | Owner | What happens |
|---|---|---|
| 0 | `app` | The newest published simulation state is taken, once, for this pass. |
| 1 | `input` | `UI::handleInput()`: window events are read. Widgets react, bound parameters are written, panels are marked dirty, events are collected. |
| 2 | application | `App` hands each event to the `onEvent` handler, which handles it or sends a command to the simulation. Then the `onUpdate` handler runs. |
| 3 | `binding`, `widgets`, `layout` | `UI::update()`: bound values that changed are handed to their widgets. Animations advance. Layout runs if something changed size. |
| 4 | `render` | `UI::draw()`: if nothing needs a redraw, the frame is skipped and the loop waits for input (with a timeout of one display frame). Otherwise dirty panels are rebuilt and everything is drawn, including the views, whose draw functions read `simulation.state()`. |

What `ui.cpp` does in each step, as of WP 3.2 (the rest is added by the packages that build the modules):

- **Construction**: the model is built from the setup, which checks names and colours; `layout::prepare` finds the grid cells of panels and widgets and refuses what cannot be laid out. One render batch is made per panel.
- **`handleInput()`**: reads the window's events with `frame::nextEvent`, which is where an idle application sleeps. A resize sets the window's view back to one unit per pixel, works out the layout theme's sizes for the new window size, and marks the placement as out of date; if the sizes changed, every panel is painted anew. Window events are forwarded (`input/window_events`).
- **`update()`**: moves folding panels on; hands bound values that changed to their widgets; moves every widget's state blends and calls `Widget::update` (`widgets/animation`, D74); lays everything out (`layout::arrange`) if the placement is out of date: after a resize, after a panel was collapsed, expanded, shown or hidden, and after a new theme. While anything moves, the next pass does not wait for input.
- **`draw()`**: hands every panel's place to its batch, takes the model's dirty flags, paints the panels that changed (`widgets/panel_frame`, then each widget's `paint` at the place layout gave it), and lets the renderer present, which skips the frame if nothing changed.
- **Handles** read and write the model, and say what that makes out of date. The facade keeps no state of its own beyond "the placement is out of date".


Simulation thread, one pass: handle pending commands → if not paused, tick → if the main thread has taken the last state, write a new one → request a redraw.

The two threads share only `Param<T>` values, the command queue and the published state.

## 7. What the application writes

This is the agreed API (Phase 1). `examples/starter/main.cpp` is the reference application: a simulation, three panels, a main view with pan and zoom, and a minimap. It is compiled with every build and becomes runnable when the API is implemented.

```cpp
using namespace atpl;

struct Params {
    Param<float> gravity = 9.81f;
    Param<int>   particleCount = 2000;
};
enum class Command { Reset };
struct World { std::vector<sf::Vector2f> positions; };

class Particles final : public Simulation<World, Command> {
public:
    explicit Particles(Params& params) : m_params(params) {}

private:
    void onCommand(const Command&) override { m_positions.clear(); }
    void tick(float dt) override { /* advance m_positions, reading m_params */ }
    void writeState(World& state) const override { state.positions = m_positions; }

    Params& m_params;
    std::vector<sf::Vector2f> m_positions;
};

int main() {
    Params params;
    Particles simulation(params);
    Camera camera("world");

    App app({
        .window = {.title = "Particles"},
        .ui = {
            .background = "world",                                        // the simulation, behind the panels
            .panels = {
                {.name = "Simulation", .placement = Anchor::TopLeft, .widgets = {
                    Switch("Pause", simulation.controls.paused),          // controls are parameters
                    Slider("Speed", simulation.controls.speed, {.min = 0.25, .max = 8}),
                    ValueDisplay("Ticks per second", simulation.controls.ticksPerSecond),
                }},
                {.name = "Particles", .placement = Anchor::TopRight, .widgets = {
                    Slider("Count", params.particleCount, {.min = 0, .max = 20000}),
                    Slider("Gravity"),
                    Button("Reset"),
                }},
            },
        },
    });

    app.ui().widget("Gravity").bind(params.gravity);                      // bound by name

    app.ui().view("world").onDraw([&](sf::RenderTarget& target, sf::Vector2f size) {
        camera.apply(target, size);
        draw(target, simulation.state());                                 // newest published state
    });

    app.onEvent([&](const Event& event) {
        if (event.isButton("Reset")) simulation.send(Command::Reset);
        if (camera.handle(event)) app.ui().requestRedraw();
    });

    return app.run(simulation);
}
```

## 8. What happens to the existing code

| Existing | Fate |
|---|---|
| `ui/utils/functions`, `interpolated` | Moved to `core/easing`, `core/interpolated` |
| `ui/utils/rect` | Moved to `atpl/ui/rect.hpp`, trimmed to the value type `Rect<T>`. Dropped: `AnimRect` (use `Interpolated<FloatRect>`), the shared `iRect` base, and the edge/corner "rescale" setters, none of which the old code used |
| `layout/widget_packing.hpp` and its test | Kept as `src/ui/layout/packing.hpp/.cpp`. `WidgetSizing` dropped (replaced by `Metrics`, P12) |
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

## 9. Order of the rework

1. Skeleton: directory tree, three targets, moved utilities and their tests, an example that opens an empty window. Old UI code removed. Build and tests green.
2. Public headers and the `minimal` example agreed (plan Phase 1).
3. `render/` with a hand-written draw list, then `model/` and `layout/`, then `input/` and the widgets, then `app/` and threading (plan Phases 2–4).

Every step leaves the build green and the `minimal` example running.
