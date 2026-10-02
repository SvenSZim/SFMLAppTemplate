#pragma once

#include <variant>

namespace atpl {

// Where things go. The same notions are used on two levels:
// - panels in the window: an `Anchor` (floating), or a place in the window's grid
// - widgets in a panel:   a place in the panel's grid, or nothing for automatic placement
//
// A grid has equal columns and equal rows. A place in it is either a `GridCell` (this cell) or a
// `GridSpan` (this many cells, wherever there is room). Things with a cell are placed first.
// The others follow, the larger ones first, each into the first free cells that hold it,
// looking row by row from the left. What does not fit is a `SetupError`.

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
/// For a panel, the grid is the window's (`UISetup::grid`). For a widget, it is its panel's
/// (`PanelSetup::columns` and `PanelSetup::rows`).
struct GridCell {
    int column = 0;
    int row = 0;
    int columnSpan = 1;
    int rowSpan = 1;
};

/// A size in a grid without a position: this many cells, wherever there is room for them.
struct GridSpan {
    int columns = 1;
    int rows = 1;
};

/// A panel floats at an anchor, fills the grid cells it names, or fills as many grid cells as it
/// says, wherever there is room. All three can be used in one UI.
using Placement = std::variant<Anchor, GridCell, GridSpan>;

} // namespace atpl
