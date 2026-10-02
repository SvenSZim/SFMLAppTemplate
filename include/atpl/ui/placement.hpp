#pragma once

#include <variant>

namespace atpl {

// Where things go. The same two notions are used on two levels:
// - panels in the window: an `Anchor` (floating) or a `GridCell` of the window's grid
// - widgets in a panel:   a `GridCell` of the panel's grid, or nothing for automatic placement

/// Where a floating panel sits: at an edge or corner of the window, on top of the background
/// and of the grid.
///
/// Panels that share an anchor are stacked in the order they are listed: downwards from a top
/// anchor, upwards from a bottom anchor. `Left` and `Right` centre their stack vertically.
enum class Anchor {
    TopLeft,
    Top,
    TopRight,
    Left,
    Right,
    BottomLeft,
    Bottom,
    BottomRight,
};

/// A position in a grid: the cell at `column`, `row`, optionally spanning several cells.
/// Columns and rows count from 0.
///
/// For a panel, the grid is the window's (`UISetup::grid`): equal cells, and the panel fills the
/// cells it spans. For a widget, the grid is its panel's: `PanelSetup::columns` equal columns, and
/// rows that are each as high as their highest widget.
struct GridCell {
    int column = 0;
    int row = 0;
    int columnSpan = 1;
    int rowSpan = 1;
};

/// A panel either floats at an anchor or fills grid cells. Both kinds can be used in one UI.
using Placement = std::variant<Anchor, GridCell>;

} // namespace atpl
