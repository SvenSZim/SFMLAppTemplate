#include "ui/layout/grid_packing.hpp"

#include <algorithm>
#include <numeric>

namespace atpl::layout {

namespace {

/// Which cells of a grid are taken.
class Occupancy {
public:
    Occupancy(int columns, int rows) :
        m_columns(columns),
        m_rows(rows),
        m_taken(static_cast<std::size_t>(columns) * static_cast<std::size_t>(rows), false) {}

    [[nodiscard]] bool isFree(const GridCell& cell) const {
        for (int row = cell.row; row < cell.row + cell.rowSpan; ++row) {
            for (int column = cell.column; column < cell.column + cell.columnSpan; ++column) {
                if (m_taken[index(column, row)]) {
                    return false;
                }
            }
        }
        return true;
    }

    void take(const GridCell& cell) {
        for (int row = cell.row; row < cell.row + cell.rowSpan; ++row) {
            for (int column = cell.column; column < cell.column + cell.columnSpan; ++column) {
                m_taken[index(column, row)] = true;
            }
        }
    }

    /// The first free place for this many cells, row by row from the left.
    [[nodiscard]] std::optional<GridCell> firstFree(GridSpan span) const {
        for (int row = 0; row + span.rows <= m_rows; ++row) {
            for (int column = 0; column + span.columns <= m_columns; ++column) {
                const GridCell cell{ .column = column, .row = row, .columnSpan = span.columns, .rowSpan = span.rows };
                if (isFree(cell)) {
                    return cell;
                }
            }
        }
        return std::nullopt;
    }

private:
    [[nodiscard]] std::size_t index(int column, int row) const {
        return static_cast<std::size_t>(row) * static_cast<std::size_t>(m_columns) + static_cast<std::size_t>(column);
    }

    int m_columns;
    int m_rows;
    std::vector<bool> m_taken;
};

} // namespace

GridPacking packGrid(std::span<const GridItem> items, int columns, int rows, bool positionsMayOverlap) {
    GridPacking result;
    result.cells.resize(items.size());
    const auto fail = [&result](std::size_t item, GridProblem problem) {
        result.failed = item;
        result.problem = problem;
        return result;
    };

    // What each item asks for, and how many rows the positions use.
    result.rows = rows;
    int rowsUsed = 0;
    for (std::size_t i = 0; i < items.size(); ++i) {
        const GridItem& item = items[i];
        const int columnSpan = item.cell.has_value() ? item.cell->columnSpan : item.span.columns;
        const int rowSpan = item.cell.has_value() ? item.cell->rowSpan : item.span.rows;
        if (columnSpan < 1 || rowSpan < 1) {
            return fail(i, GridProblem::BadSpan);
        }
        if (item.cell.has_value()) {
            const GridCell& cell = *item.cell;
            const bool outside = cell.column < 0 || cell.row < 0 || cell.column + cell.columnSpan > columns ||
                                 (rows > 0 && cell.row + cell.rowSpan > rows);
            if (outside) {
                return fail(i, GridProblem::Outside);
            }
            rowsUsed = std::max(rowsUsed, cell.row + cell.rowSpan);
        } else if (columnSpan > columns || (rows > 0 && rowSpan > rows)) {
            return fail(i, GridProblem::Outside);
        }
    }
    result.rows = rows > 0 ? rows : rowsUsed;

    // 1. Positions.
    Occupancy grid(columns, result.rows);
    for (std::size_t i = 0; i < items.size(); ++i) {
        if (!items[i].cell.has_value()) {
            continue;
        }
        if (!positionsMayOverlap && !grid.isFree(*items[i].cell)) {
            return fail(i, GridProblem::Overlap);
        }
        grid.take(*items[i].cell);
        result.cells[i] = *items[i].cell;
    }

    // 2. Everything else, the larger first. Equal ones keep the order they were given in.
    std::vector<std::size_t> order;
    for (std::size_t i = 0; i < items.size(); ++i) {
        if (!items[i].cell.has_value()) {
            order.push_back(i);
        }
    }
    const auto area = [&items](std::size_t i) { return items[i].span.columns * items[i].span.rows; };
    std::stable_sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) { return area(a) > area(b); });

    for (const std::size_t i : order) {
        const std::optional<GridCell> place = grid.firstFree(items[i].span);
        if (!place.has_value()) {
            return fail(i, GridProblem::NoRoom);
        }
        grid.take(*place);
        result.cells[i] = *place;
    }
    return result;
}

} // namespace atpl::layout
