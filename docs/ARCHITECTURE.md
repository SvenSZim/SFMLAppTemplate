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
- The window is created and owned by `app`; the UI is handed a reference (D26). Inside `ui`, only two source files touch `sf::RenderWindow`: `ui/ui.cpp` and `ui/render/renderer.cpp`. Everything else in `ui` is plain data in, plain data out, and testable without a display.

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
│  │  ├─ binding/
│  │  ├─ layout/
│  │  ├─ render/
│  │  ├─ theme/
│  │  └─ widgets/
│  └─ app/
├─ examples/
│  ├─ minimal/                 an SFML-only window; the smoke test until the API is implemented
│  ├─ starter/                 the smallest app written against the API; compiled, not yet linked
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
| `param.hpp` | `Param<T>`: thread-safe value that reads and writes like a `T`, for numbers, enums and strings; any number of readers and writers (D10, P10) | new |
| `series.hpp` | `Series`: thread-safe ring buffer of `float` samples with fixed capacity; the data source for graphs (P10) | new |
| `queue.hpp` | `Queue<T>`: thread-safe, unbounded, ordered; a tick takes all waiting items at once with `drain`; used for app → simulation commands (D11) | new |
| `snapshot.hpp` | `Snapshot<T>`: the latest complete state from one writer thread to one reader thread; three reused buffers, no copying, no blocking (P3) | new |
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
| `ui.hpp` | `UI`: the facade. Built from a window reference and a `UISetup`. `widget(name)`, `view(name)`, `panel(name)`; the frame steps `handleInput()`, `update()`, `draw()`; `setProfilerVisible()`; `requestRedraw()` (any thread); events (WP 1.3). |
| `event.hpp` | `Event` and the twelve event types (D29). |
| `value.hpp` | `Value`: a widget's value of any kind, as carried by `ValueChanged`. |
| `error.hpp` | `SetupError`: thrown for mistakes in setup or addressing. |
| `id.hpp` | `PanelId`, `WidgetId`, `ViewId`. |
| `setup.hpp` | `UISetup` (background view, grid, panels, profiler readout), `PanelSetup`. |
| `placement.hpp` | `Anchor`, `GridCell`, `Placement` (D25). `GridCell` is used for panels in the window and for widgets in a panel (D27). |
| `widgets.hpp` | The widget pool. Each widget has one public type, its descriptor: `Button`, `Switch`, `Slider`, `ProgressBar`, `TextDisplay`, `TextInput`, `Dropdown`, `Graph`, `Paragraph`, `View` (D24). A descriptor also declares its widget's parts. `WidgetSetup` is what a panel stores; any type with a name and a `create()` converts to it, including app-defined ones. |
| `handle.hpp` | `WidgetHandle` (`bind`, `unbind`, `get`, `set`, `setEnabled`), `ViewHandle` (`onDraw`, `rect`), `PanelHandle` (`setCollapsed`, `setVisible`, `rect`). Light values; the app does not keep them (P2). |
| `binding.hpp` | `ValueKind`, the interfaces `Binding<T>` (bool, number, index, text) and `SeriesBinding`, and `AnyBinding`, which everything bindable converts to (P10). |
| `theme.hpp` | `Role`, `Kind`, `Part`, `State`, `PartStyle`, `PartOverride`; the tokens `Palette`, `Shape`, `Metrics`, `Typography`; `Theme` and the built-in themes (P5, P11, D30). |
| `widget.hpp` | `Widget`, the interface widget types implement; `MeasureContext`, `InputContext`, `UpdateContext`, `Style`, `Painter` (P1, D30). |
| `rect.hpp` | Rectangle type. From `ui/utils/rect`, trimmed. |

### 4.2 Internal modules (`src/ui/`)

| Module | Owns | Reads | Must not |
|---|---|---|---|
| `model/` | Panels, widget slots, views, name index, lifetime | — | Lay out, draw, handle input |
| `input/` | Hit-testing, hover, press, drag capture, keyboard focus, what the UI consumes, emitting events (after asking `binding/` to write a changed value) | model, layout rects | Set geometry |
| `layout/` | Every rectangle and size: panel placement, widget packing inside panels, view regions, visibility | model, widget `measure`, `Metrics` | Touch interaction state or colours |
| `binding/` | Making a `Param<T>`, a `Series` or two functions look like a binding; syncing: each frame it compares each bound widget's revision and, only on a change, hands the widget the new value and marks its panel dirty; it writes the user's changes into the binding | model | Touch layout, drawing or interaction state; raise events |
| `theme/` | Tokens, `Metrics`, role defaults, part entries, built-in themes, resolving a `Style` for a part and state | — | Decide any rectangle or position |
| `render/` | Draw list, tessellation, per-panel batches, text cache, draw order, frame flag, profiler | model, layout rects, theme | Modify the model |
| `widgets/` | One file per widget type: its behaviour only | its own state, contexts handed to it | Position itself, issue draw calls, read theme tokens directly |
| `ui.cpp` | Calling the modules in order; the public facade | everything | Contain logic of a single concern |

Files:

```
src/ui/
├─ ui.cpp     frame_loop.hpp/.cpp
├─ model/    panel.hpp  widget_slot.hpp  view.hpp  store.hpp/.cpp  name_index.hpp/.cpp
├─ input/    input_system.hpp/.cpp
├─ binding/  adapters.hpp/.cpp  sync.hpp/.cpp
├─ layout/   layout_manager.hpp/.cpp  floating.cpp  grid.cpp  packing.hpp
├─ theme/    theme.cpp  presets.cpp
├─ render/   renderer.hpp/.cpp  draw_list.hpp/.cpp  shapes.hpp/.cpp  painter.cpp
│            panel_batch.hpp/.cpp  text_measurer.hpp  text_renderer.hpp
│            text_layout.hpp/.cpp  font_measurer.hpp/.cpp  text_cache.hpp/.cpp
│            profiler.hpp/.cpp  frame_stats.hpp
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
  | binding, and the revision last seen | app, through `WidgetHandle::bind`; kept in step by `binding/` |
  | the widget object itself | model (at setup) |

- **Widget object**: an implementation of the `Widget` interface. It holds only type-specific state (slider value, animation progress, dropdown open).
- **Ids** are dense indices, so every lookup by id is a direct array access. Names are resolved to ids once, when a handle is requested.
- **Names**: panel names are unique; widget names are unique per panel. `"Speed"` works if it is unique in the whole UI, otherwise `"Controls/Speed"`. Duplicates and unknown names fail loudly (Q8).

### 4.4 The widget interface (P1, D30)

```cpp
class Widget {
public:
    virtual ~Widget() = default;

    virtual SizeRequest measure(const MeasureContext&) const = 0;  // height wanted at the offered width
    virtual bool handleInput(const Event&, InputContext&);         // true = used
    virtual void update(float dt, UpdateContext&);                 // animations
    virtual void paint(Painter&, const Style&) const = 0;          // emit shapes

    virtual bool accepts(ValueKind) const;                         // what it can be bound to
    virtual bool editsValue() const;
    virtual std::optional<Value> value() const;
    virtual void setValue(const Value&);                           // from the binding or the app
    virtual void setSeries(const SeriesBinding*);                  // graphs only
};
```

Rules:
- **Size.** A widget answers with a height for the width it is offered, and may ask to stretch over the height left in its panel (views). It never decides a width.
- **Input.** `handleInput` gets the same `Event` types the application gets; the context converts pointer positions into the widget's own coordinates. Hovered, pressed, focused and disabled are kept by the UI and read through the context.
- **Values.** A widget never sees what it is bound to. It keeps its own copy of its value; `binding/` hands it new values through `setValue`, and the widget reports the user's changes through `InputContext::changeValue(value, final)` or `press()`. An app-defined widget therefore contains no binding code. Graphs are the exception: they are handed their series source and read from it.
- **Outside world.** A widget acts only through the context it is handed: `markDirty()`, `capturePointer()`, `requestFocus()`, `changeValue()`, `press()`.

Adding a widget type means one file in `widgets/` and one descriptor in `widgets.hpp`. An application adds its own by implementing the interface and writing a descriptor; `tests/api/widget_usage.cpp` shows a complete one.

Known gap: a dropdown's open list has to draw above other panels and take input outside the widget's rectangle. The interface gets a call for that with the dropdown itself (WP 3.11).

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
| Paragraph | text (its body), read-only | `Param<std::string>` |
| View | none (draw callback) | — |

Rules:
- Everything the template ships (`Param<T>`, `Series`) is thread-safe.
- Plain variables cannot be bound directly.
- An application may implement a binding interface over its own data, or build one from a getter and a setter: `ui.widget("Speed").bind(getter, setter)`. Thread safety of such a binding is the application's responsibility.
- What a widget **is** (range, step count, option labels) is part of its descriptor, not of the binding.
- Numbers of every C++ type travel as `double`, enums as the index of the enumerator. Exact for `float`, for integers up to 32 bits and for 64-bit integers up to 2^53; larger 64-bit values are bound as text to be shown exactly. Writing back rounds integers and clamps to the type's range (`numberTo<T>`).
- A descriptor only accepts sources of its widget's kind, checked by the compiler. Binding by name is checked when it runs and throws `SetupError` for a wrong kind.

### 4.6 In-panel layout (P12)

- A widget reports the size it wants through `measure()`, given the available width and the `Metrics`.
- A panel has a number of equal columns (`PanelSetup::columns`, default 1). Its widgets are placed in one of two ways, chosen per panel (D27):
  - **No widget has a position**: `layout/` packs them top to bottom into the columns with balanced heights, in the order listed (`packing.hpp`).
  - **At least one widget has a position** (a `GridCell` given with `at(cell, widget)`, with column and row spans): the panel is a grid. Positioned widgets take their cells; the others take, in order, the first free cell, row by row from left to right, without a span. Rows are as high as their highest widget.
  - Giving one widget a position therefore changes how the others are placed. This is intended.
- Either way, `layout/` assigns each widget its rectangle in panel-local coordinates. A widget never positions itself.
- Inside its own rectangle, a widget arranges its parts (label, track, knob). `paint` and `handleInput` use the same part rectangles, computed in one place per widget.
- `Metrics` (padding, gaps, row heights, font sizes) is a token set next to the theme's colours. Only `layout/` and `measure()` read it. Nothing else defines sizes.

### 4.6a Placing panels (R5, D25)

Each panel states its own placement:
- **`Anchor`**: the panel floats at an edge or corner of the window, on top of the background view and of the grid. Panels that share an anchor are stacked in the order they are listed.
- **`GridCell`**: the panel fills one or more cells of the window's grid (`UISetup::grid`, equal cells).

Both kinds can be mixed. Typical arrangements:
- Simulation as background, controls floating over it: a background view plus anchored panels.
- Simulation inside the layout: a grid, one panel with a `View` widget spanning most cells, control panels in the rest.
- Main view plus minimap: either of the above with a second `View` widget in a small panel.

### 4.6b Overflow (D28)

What happens when things do not fit. Rules for a start; to be revisited if they do not work in practice.

| Case | Rule |
|---|---|
| A panel's content is higher than the panel can be | The content scrolls vertically inside the panel (mouse wheel, thin scrollbar). The header stays fixed. |
| A stack of floating panels is higher than the window | Collapsed panels keep their header height. Expanded panels share the remaining height and scroll inside. If not even the headers fit, the last panels are hidden. |
| A grid-cell panel in a small window | Its cell shrinks with the window; its content scrolls. |
| A floating panel wider than the window | Its width is clamped to the window width. |
| Text wider than its widget | Cut off with an ellipsis. |
| A dropdown list that would leave the window | Opens upward, or is clamped to the window. |
| Horizontal scrolling | None. |
| Very small windows | The application may set a minimum window size (app setup). |

Scrolling changes only an offset and a clip rectangle of the panel's batch; the geometry is not rebuilt (cache level 2 stays valid). The scroll offset is interaction state and belongs to `input/`; the content height and the visible height are layout outputs.

### 4.7 Views (Q3)

The application's own rendering appears in views:
- **Background view**: the whole window, behind all panels. Optional.
- **View widget**: a region inside a panel. Any number of them (main view, minimap, ...).

Both are the same thing to the app: a named view with a draw callback, set through `ui.view("name").onDraw(...)`. The renderer calls it at the right point in the draw order, with drawing clipped to the view's rectangle. The app calls `UI::requestRedraw()` (thread-safe) when it has something new to show. There is one such call for the whole UI, not one per view, because a frame is always drawn as a whole.

View regions are clipped to rectangles; a view cannot have rounded corners unless it is rendered to a texture first (opt-in, D7).

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

The template only forwards. It does not interpret forwarded input (D15). Pan and zoom for a view is an optional helper in `app` (section 5).

### 4.9 Rendering (D3–D7, plan section 5)

```
widget.paint() ──► Painter ──► draw list ──► PanelBatch (one vertex array + text) ──► window
```

- `shapes`: tessellation of boxes (fill or gradient, outline with gap, corner radius, shadow), lines, polylines and areas under a curve. All shapes become triangles so that a panel is one draw call.
- `draw_list`: what one panel draws: its triangles and its text runs. Reused from rebuild to rebuild without allocating.
- `painter` (the public `Painter`): moves a widget's own coordinates to its place in the panel and hands shapes to `shapes` and text to the draw list. `text_measurer` is the interface it measures text through, so everything above it is testable without a font.
- `panel_batch`: one batch per panel in panel-local coordinates, rebuilt only when the panel is dirty (cache level 2). It has two layers: the frame (background, header, scrollbar) and the content (the widgets). Moving the panel and scrolling the content only change how the batch is placed when drawn; nothing is rebuilt. Scrolled content is clipped to the content area (D28).
- `text_layout`: fitting text into its room, independent of fonts: measuring a line, cutting it short with an ellipsis, wrapping at word boundaries.
- `font_measurer`: measures text with the real fonts.
- `text_cache`: draws a layer's text and keeps what it built (cache level 3). One SFML text object per text run, built when the run first appears and again only when it changes; a panel repainted with the same text builds nothing. One draw call per run (D37).
- `renderer`: draws batches in the order of the list it is given, then the overlay; at most two calls for shapes per panel. Reports draw calls and triangles per frame. `present` shows a frame in the window only if one is needed (cache level 1): if the redraw flag asks for one, or a batch changed since the last frame. Otherwise it does nothing at all. View callbacks are added in WP 4.4.
- `frame_loop` (at the root of `ui`, next to the facade): the `RedrawFlag`, which any thread may set, and `nextEvent`, which fetches the window's next event and sleeps for up to one display frame while no frame is asked for. A batch notes by itself when it was repainted, moved or scrolled, so changes inside the UI need no request; the flag is for what the UI cannot see, such as a new simulation state.
- `text_renderer`: the interface the renderer hands a layer's text to; `text_cache` implements it.
- `profiler`: measures what the UI costs (D6). Three sections of a frame are timed: build (painting the panels that changed), submit (clearing and handing the batches to the graphics card) and show (putting the frame on screen). Whoever does the work reports it: the code that paints panels wraps that in `measure(Section::Build)`, the renderer reports the rest, together with draw calls, triangles, panels rebuilt and texts built. The readout is a small batch of its own with its own parts (`Profiler::Background`, `Label`, `Value`), so a theme styles it like anything else. It is off by default, refreshed at 5 Hz and painted only when its text changes, so an idle application stays idle with the readout on; a frame drawn only for the readout is not counted in its numbers. The application switches the readout on with `UI::setProfilerVisible` or `UISetup::profiler` (D38). `tests/bench/render_bench.cpp` uses the profiler to measure a scene of 100 widgets (plan 5.5).

Draw order per frame: background view → panels in order (shapes, views, text) → overlay layer → profiler.

Within a panel, text is always drawn on top of shapes: a panel is one batch of triangles followed by its text. What must cover text, such as an open dropdown list, goes on the overlay layer.

An outline is a band of triangles between two edges, each with its own colour, like a shadow. Today both edges have the same colour. An outline that fades across its width or along the box is a possible extension (D36).

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
| 1. Tokens | Global values, in four groups: `palette` (the main colours and accents a panel chooses from), `shape` (radii, outline, outline gap, line thickness, shadow), `metrics` (sizes, GUI scale), `typography` (size and font per text type) | `theme.palette.accents[0].accent = ...;` `theme.shape.outlineGap = 2;` `theme.typography.title = {.size = 18, .font = bold};` |
| 2. Role defaults | Style derived from the part's role, the tokens and the three colours of the part's panel; visibility from the part's own default | every track part is an outlined area with a small radius; ticks hidden |
| 3. Part entries | Settings for one part; only the fields that are set have an effect | `theme[Slider::Ticks].shown = true;` `theme[Button::Face].radius = 0;` `theme[Switch::Track].borderGap = 0;` `theme[Button::Label].font = bold;` |

**Colours (D34).** A theme offers a list of main colours and a list of accents. A panel is drawn in three of them, chosen by index in its setup; a single widget can deviate with `colored(...)`:

| Colour | Chosen with | Used for |
|---|---|---|
| main1 | `PanelSetup::main1` (default 0) | the panel's background |
| main2 | `PanelSetup::main2` (default 1) | the outline of the panel and of everything that can be operated; lines |
| accent | `PanelSetup::accent` (default 0) | fills (slider, progress bar, a switch that is on, graph curve); what outlines turn to in use |

Everything else is derived: text is the light or dark colour that reads best on main1, muted text lies between text and main1 and stays readable, the areas of buttons and fields are main1 moved a little towards main2, knobs have the text colour. An accent can define where a gradient starts; accent-coloured boxes then fade in from that colour. An index the theme does not have is a `SetupError`.

**Outlines.** Panels and everything that can be operated have an outline of `shape.outline` in main2. Between outline and fill lies `shape.outlineGap`, the same for every part unless a part entry sets `borderGap`. The outline stays at the box's edge and the fill moves inwards, so layout is unaffected. Knobs have no outline.

**States.** The UI tracks hovered, pressed, focused and disabled per widget; a widget can add `Active` for a part that is "on".

| State | Effect |
|---|---|
| Hovered | the outline of an area moves partly to the accent; a knob takes on some of the accent |
| Pressed, Focused | the outline is the accent; a pressed area is tinted slightly |
| Active | a track is filled with the accent; text and knobs on it take the light or dark colour that reads best there |
| Disabled | everything fades |

The built-in themes: `themes::moon()` (the default: black, grey outlines, white accents as gradients, no shadows) and `themes::colorful()` (warm brown-grey with sand, green, blue and red accents, soft shadows). A test checks both for readable contrast.

Consequences:
- A theme that sets only tokens is complete. It needs no per-widget entries.
- App-defined widgets declare their own kind and parts and look right under every theme, including themes that have never heard of them.
- More detailed styling later (per panel, per widget instance) is another override layer on top; widgets do not change.
- A different structure (a part the widget does not declare) needs widget code, not a theme change.

Sizes come from `metrics` and from the text sizes in `typography`. Only `layout/` and `measure()` read them (4.6).

The theme is part of `UISetup` and can be replaced at runtime with `UI::setTheme`.

## 5. `app` — running an application (D32)

| File (`include/atpl/app/`) | Content |
|---|---|
| `app.hpp` | `App`: creates and owns the window (`WindowSetup`), creates the UI, runs the main loop. `onEvent(handler)` delivers every UI event on the main thread (D11); `onUpdate(handler)` runs once per pass of the loop. `run()` or `run(simulation)`; `quit()`. |
| `simulation.hpp` | `Simulation<State, Command>`: the base class of an application's simulation, and `SimulationControls` (P3). |
| `camera.hpp` | `Camera`: optional pan and zoom for one view, driven by forwarded events (D15). `visibleArea()` tells what part of the world it shows, for example for a minimap. |
| `resources.hpp` | `Resources`: finds files in the `resources` directory next to the executable and loads fonts; `ResourceError` when something is missing. Fonts and textures by name are added in WP 5.6. |

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

### 5.2 The application loop

- Closing the window ends the application by default (`AppSetup::quitOnClose`). Turned off, the request only arrives as a `WindowClosed` event.
- `App` loads the bundled font if the theme has none.
- The loop only spins while there is something to do (section 6), so `onUpdate` is not a clock.

## 6. One frame

Main thread, one pass of `App`'s loop:

| Step | Owner | What happens |
|---|---|---|
| 0 | `app` | The newest published simulation state is taken, once, for this pass. |
| 1 | `input` | `UI::handleInput()`: window events are read. Widgets react, bound parameters are written, panels are marked dirty, events are collected. |
| 2 | application | `App` hands each event to the `onEvent` handler, which handles it or sends a command to the simulation. Then the `onUpdate` handler runs. |
| 3 | `binding`, `widgets`, `layout` | `UI::update()`: bound values that changed are handed to their widgets. Animations advance. Layout runs if something changed size. |
| 4 | `render` | `UI::draw()`: if nothing needs a redraw, the frame is skipped and the loop waits for input (with a timeout of one display frame). Otherwise dirty panels are rebuilt and everything is drawn, including the views, whose draw functions read `simulation.state()`. |

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
                    TextDisplay("Ticks/s", simulation.controls.ticksPerSecond),
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
