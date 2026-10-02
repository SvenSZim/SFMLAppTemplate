# Layout concept

**Status: accepted (D44), being built.** This document describes how sizes and positions are decided. It is the authority for layout; [ARCHITECTURE.md](ARCHITECTURE.md) §4.6, §4.6a and §4.6b describe what is built so far and are brought in line package by package. Section 9 lists the earlier decisions this changes and the packages that build it.

## 1. The idea in one page

A UI has a **layout theme**, the way it has a theme for its looks. The theme says how things look; the layout theme says how large things are and where they go. A layout theme is a set of defaults; a single panel can override any of them.

Sizes are decided in one of two directions:

```
top-down                                  bottom-up
the space is given, content adapts        content says what it needs, the space follows

window                                    widgets' minimum sizes
  -> panel's grid cells                     -> cell or row sizes
    -> panel's own grid                       -> panel size
      -> widget's cells                         (capped by the window)
```

| | Top-down | Bottom-up |
|---|---|---|
| **Panels** | **Grid**: the window is an even grid. A panel gets its cells and either *fills* them or *fits its content* inside them. | **Floating**: panels stack at an anchor. A panel is as large as its content, or all panels are *equal* to the largest. |
| **Widgets** | The panel is an even grid. A widget gets its cells. | The widgets' minimum sizes decide: each widget its *own* size, or *equal* cells sized by the largest. |

Which direction a panel's inside uses follows from the panel: if the panel's size is given from outside (grid, fill), its content is laid out top-down; otherwise bottom-up.

Every widget states a minimum size. A *constant* widget (slider, button) also states a maximum; a *dynamic* widget (graph, view) takes whatever it is given.

Everything grows and shrinks with the window, within limits the layout theme sets.

## 2. The layout theme

```cpp
UISetup setup{
    .theme  = themes::moon(),        // looks
    .layout = layouts::overlay(),    // sizes and positions
    .panels = { ... },
};
```

What a layout theme holds (names are a proposal):

| Group | Setting | Meaning |
|---|---|---|
| Sizes | `metrics` | margin, padding, gap, row height, header height, scrollbar width. Moved here from the theme. |
| Scaling | `scaling` | reference window size, lowest and highest scale, how strongly fonts follow (section 6) |
| Panels | `placement` | where a panel goes if it does not say: an anchor, or "somewhere in the grid" |
| Grid panels | `fit`, `alignment` | fill the cells, or fit the content and sit at one of nine positions inside the cells |
| Floating panels | `width`, `height` | each `Own` (from the panel's content) or `Equal` (the same for all floating panels) |
| Floating panels | `limit` | the most a floating panel may take: fixed pixels and a fraction of the window |
| Content | `rows` | bottom-up content: `Own` (every widget its own height) or `Equal` (equal cells) |
| Content | `widgetAlignment` | where a widget sits in a cell that is larger than the widget may be |
| Collapsing | `collapsible`, `collapseTowards` | whether panels fold to their header, and to which of nine positions |
| Overflow | `stackOverflow` | a stack of floating panels that does not fit: hide the last ones, or card stack (D42) |

A panel overrides any of these in its own setup:

```cpp
{ .name = "Scene", .placement = GridCell{.columnSpan = 3}, .layout = {.fit = Fit::Fill} },
{ .name = "Legend", .layout = {.alignment = Alignment::BottomRight} },
```

`Alignment` has nine values: three horizontal positions by three vertical ones (`TopLeft` ... `Center` ... `BottomRight`). The same type is used for a panel in its cells, a widget in its cell, and the direction a panel collapses to.

## 3. Panels

### 3.1 Grid panels (top-down)

The window is split into `UISetup::grid` equal columns and rows, with the margin around the grid and between cells.

```
+---------------------------+---------+        +---------------------------+---------+
|                           |         |        | +-------+                 |         |
|   Scene (Fill)            | Inspec- |        | |Legend |  Fit-content,   |         |
|                           | tor     |        | +-------+  top-left       |         |
|                           |         |        |                           |         |
+---------------------------+---------+        +---------------------------+---------+
```

- A panel names its cells (`GridCell`) or only its size (`GridSpan`). Cells are found once, when the UI is built: panels with a position first, then the others, the larger first, each into the first free cells that hold it, row by row from the left. A panel that finds no room is a `SetupError`. (This is built and tested.)
- **Fill**: the panel is exactly as large as its cells. Its content is laid out top-down.
- **Fit-content**: the panel is as large as its content needs, at most as large as its cells, and sits inside them at the `alignment`. The rest of the cells stays empty. Its content is laid out bottom-up.
- Panels with an explicit `GridCell` may share cells, so an application can show one of several panels in the same place (D40).

### 3.2 Floating panels (bottom-up)

Panels that name an anchor stack there, on top of the grid and of the background view: downwards from a top anchor, upwards from a bottom anchor, centred for `Left` and `Right`. (Built and tested.)

- **Own** size: the panel is as wide and as high as its content needs (section 4.2).
- **Equal** size: all floating panels of the UI get the same width, the same height, or both: the largest of what each would need. Width and height are chosen separately; "equal width, own height" is what the ant simulator's panels look like.
- **Limit**: a floating panel is never larger than `min(fixed pixels, fraction of the window)`, per axis. The fraction limits panels in small windows, the fixed pixels on very large screens. The limit overrides the content: content that no longer fits scrolls or is dropped by the rules in section 7.

### 3.3 Mixing

Both kinds can be used in one UI: a grid for the main layout and floating panels on top.

### 3.4 Collapsing

A collapsible panel folds down to its header. The header's place inside the panel's former rectangle is `collapseTowards` (top-left by default). In a stack of floating panels, the panels after a collapsed one move up as they do today.

## 4. Content of a panel

A panel has `columns` equal columns (1 to 3).

### 4.1 Top-down: the panel's size is given

The content area is split into equal columns and equal rows, with the padding around and the gap between.

```
+--Statistics-------------------+
| Ticks/s  12.4 | Progress ==== |   row 0
| +---------------------------+ |   row 1
| |        Tick time          | |   row 2   the graph spans rows 1 to 3
| +---------------------------+ |   row 3
|                               |   row 4   nobody uses it: a separator
| [ Reset ]     | [ Save ]      |   row 5
+-------------------------------+
```

- The number of rows is `PanelSetup::rows`; if that is not given, as many as the widgets with a position use; if no widget has a position either, as many as the widgets need (cells used, divided by columns, rounded up).
- Cells are found as for panels: positions first, then the others, larger first, first free place. What finds no room is a `SetupError`.
- A widget is given its cells. What size it actually takes inside them is decided by its size request (section 5).
- A row nobody uses stays empty.

### 4.2 Bottom-up: the panel's size follows from its content

Two rules, chosen by the layout theme (`rows`):

- **Own**: every widget is as high as it needs to be. Widgets are stacked top to bottom into the panel's columns, in the order listed, with columns of balanced height. There are no rows. (This is today's packing.) A column is as wide as the widest minimum width in the panel.
- **Equal**: the content is a grid of equal cells, as in 4.1, and the cell size is the largest minimum size among the panel's widgets. A widget that spans several cells counts with its minimum size **divided over its span** first: a graph that needs 300 x 180 and spans 2 x 3 cells asks for cells of about 150 x 60, not 300 x 180.

Either way the panel's size is its content plus padding and header, then limited as section 3.2 says.

If the panel ends up with more room than its content needs (equal-size panels, or fit-content with a small content), dynamic widgets take what is to spare; constant widgets stay at their maximum and are aligned.

## 5. What a widget says about its size

```cpp
struct SizeRequest {
    sf::Vector2f min;                  // the least it needs to be displayable
    std::optional<sf::Vector2f> max;   // constant widgets: never larger. Empty: dynamic.
    float widestRatio = 0.f;           // width / height at most this. 0: no limit.
    float tallestRatio = 0.f;          // height / width at most this. 0: no limit.
};
```

- **Minimum**: from the widget's font size and its complexity. A slider needs room for its label, its track and its value; a graph needs room for a curve and its labels.
- **Constant widgets** (button, switch, slider, progress bar, text display, text input, dropdown): a maximum width and height, and the two ratio limits.
- **Dynamic widgets** (graph, view, paragraph): no maximum, by design. The ratios are optional (a view may ask to keep 16:9).

Given a cell, a widget's rectangle is:

1. Dynamic: the whole cell; if it has a ratio, the largest rectangle of that ratio that fits.
2. Constant: the cell, cut down to its maximum and then to its ratios.
3. If the result is smaller than the cell, it is placed in the cell at `widgetAlignment`.

Dynamic widgets are handled differently when sizes are derived bottom-up: they contribute their minimum, and afterwards absorb whatever room is left over, where constant widgets stop at their maximum.

A widget never positions itself. Inside its rectangle it arranges its own parts, and must cope with any size between its minimum and its maximum.

## 6. Scaling with the window

Everything grows and shrinks with the window, up to a point.

```
scaleX    = clamp(window width  / reference width,  lowest, highest)
scaleY    = clamp(window height / reference height, lowest, highest)
scaleFont = 1 + (min(scaleX, scaleY) - 1) * fontStrength
```

| What | Follows |
|---|---|
| Widget widths, side padding, gaps between columns, panel width limits | `scaleX` |
| Row heights, header height, vertical padding and gaps, panel height limits | `scaleY` |
| Text sizes, outline thickness, corner radius | `scaleFont` |

- The reference window size, the two limits and `fontStrength` (0: fonts never change; 1: fonts follow the smaller side fully) come from the layout theme.
- Text sizes are rounded to whole pixels, so a window being dragged does not rebuild every text on every pixel of movement.
- The GUI scale that the theme has today (`Metrics::scale`) stays as a factor on top, for high-resolution displays.

## 7. When things do not fit

In this order:

1. **Height is short inside a panel**: the panel's content scrolls. Only an expanded panel scrolls.
2. **Width is short for a widget** (its cell is narrower than its minimum width): that widget is not drawn. Its place stays empty; nothing moves.
3. **A panel is not drawn** if
   - it is smaller than its header needs, or
   - its content area is lower than the highest minimum height among its widgets, or
   - its content area is narrower than the widest minimum width among its widgets.

A stack of floating panels that is too high for the window keeps today's rule (headers stay, the rest is shared, D40), with the card stack as an option (D42).

Text that is too wide for its widget still ends in an ellipsis. There is still no horizontal scrolling.

## 8. Presets (proposal)

| Preset | Panels | Content | Looks like |
|---|---|---|---|
| `layouts::overlay()` (default) | floating at `TopLeft`; equal width, own height | own heights, packed | controls floating over a simulation: the ant simulator |
| `layouts::dashboard()` | in the window grid, fill | top-down grid | an application made of panes: main view, inspector, statistics |
| `layouts::cards()` | in the window grid, fit-content, centred | equal cells | tidy cards of related controls with room around them |
| `layouts::compact()` | as `overlay`, with smaller metrics and narrower limits | own heights, packed | small windows, many panels |

## 9. What this changes

**Earlier decisions**

| Decision | Change |
|---|---|
| D25 (placement per panel) | Stays. A panel that names no placement now gets the layout theme's. |
| D27 (in-panel layout) | The grid half is replaced: rows are equal, not as high as their highest widget; a widget gets its cells' size. Packing stays as the bottom-up "own" rule. |
| D28 (overflow) | The rules for widgets and panels that do not fit are replaced by section 7. |
| D34 (theme model) | `Metrics` moves from the theme to the layout theme. |
| D40 (panel placement details) | Stays: margins, the stack sharing rule, shared grid cells. |
| D30 (widget interface) | `measure` returns the size request of section 5 instead of a height and a stretch flag. |

**Public API**

- New: `Layout` (the layout theme), `layouts::...`, `UISetup::layout`, `PanelSetup::layout` (overrides), `PanelSetup::rows`, `Alignment`, `GridSpan`, `spanning(...)`.
- Changed: `SizeRequest`; `Theme::metrics` moves to `Layout::metrics`; `PanelSetup::placement` becomes optional.

**Code that exists**

| Part | Fate |
|---|---|
| Anchor stacks, window grid cells, margins (`panel_placement`, merged) | kept; gains fit-content, equal sizes and limits |
| Grid cell finding, largest first (`grid_packing`, local) | kept as it is |
| Balanced column packing (`packing`, merged) | kept as the "own" rule |
| Equal rows of one standard row height (`widget_layout`, local) | replaced by sections 4 and 5 |
| The order of a layout pass (`arrange`, local) | kept in outline: widths, content, panels, spare room, views |
| `Metrics` in `Theme`, used by painter, style and profiler | moved; mechanical change in many places |

**Work packages**

| Package | Content | State |
|---|---|---|
| WP 3.4 (#23) | Finding grid cells, largest first, for widgets and panels; balanced packing; the order of a layout pass; widgets painted at their places. Grid rows are one standard row high for the time being. | done |
| WP 3.15 (#77) | Layout theme: the `Layout` type, presets, `Metrics` moved out of the theme, per-panel overrides, scaling with the window | next |
| WP 3.16 (#78) | Widget size requests (minimum, maximum, ratios), a widget's rectangle in a cell, alignment; content top-down and bottom-up (own, equal, spans dividing the minimum) | |
| WP 3.17 (#79) | Panel sizing: fill and fit-content in the grid, own and equal floating panels, limits; the overflow rules of section 7; collapse direction | |

## 10. Details settled while writing this down

1. **Which direction a panel's content uses** follows from the panel alone: grid and fill is top-down; everything else is bottom-up.
2. **Equal floating panels** are equal across the whole UI, not per stack.
3. **Width and height are chosen separately** for floating panels, so "equal width, own height" is possible.
4. **A floating panel's own width** comes from its widgets' minimum widths. Today it is a fixed theme value (280 px).
5. **A top-down panel that names no rows and no positions** gets as many rows as its widgets need.
6. **Fonts are rounded to whole pixels** when scaling.
7. **A widget's minimum height may depend on the width it gets** (a paragraph that wraps). Layout therefore asks in two steps: first for minimum widths, then, with the widths known, for heights.
8. **The GUI scale stays** as a separate factor for high-resolution displays.
