#pragma once

#include "atpl/ui/placement.hpp"

#include <cstddef>
#include <optional>
#include <span>
#include <vector>

namespace atpl::layout {

// Finding cells in a grid of equal columns and rows. The same rule is used for panels in the
// window's grid and for widgets in a panel's grid:
//
//   1. Things with a position take their cells.
//   2. The others follow, the larger ones first and equal ones in the order given, each into the
//      first free cells that hold it, looking row by row from the left.
//
// Nothing clever: no moving things around to make room. What finds no room is a mistake in the
// setup, and reported as one.

/// One thing to place: its position if it has one, and how many cells it takes.
struct GridItem {
    std::optional<GridCell> cell; ///< If set, its spans are the size and `span` is ignored.
    GridSpan span;
};

enum class GridProblem {
    BadSpan, ///< A span below 1.
    Outside, ///< A position, or a span, that reaches outside the grid.
    Overlap, ///< A position on a cell that another position already has.
    NoRoom,  ///< Nothing free that is large enough.
};

struct GridPacking {
    /// The cells of every item, in the order the items were given. Complete only if nothing failed.
    std::vector<GridCell> cells;

    /// The number of rows of the grid: as asked for, or as many as the positions use.
    int rows = 0;

    /// The first item that could not be placed, and why.
    std::optional<std::size_t> failed;
    GridProblem problem = GridProblem::NoRoom;
};

/// Finds cells for all items in a grid of `columns` columns and `rows` rows.
///
/// With `rows` 0, the grid has as many rows as the items with a position use.
/// `positionsMayOverlap` allows several items with a position on the same cell: panels may share
/// a place in the window (only one of them being visible at a time), widgets may not.
[[nodiscard]] GridPacking
packGrid(std::span<const GridItem> items, int columns, int rows, bool positionsMayOverlap = false);

} // namespace atpl::layout
