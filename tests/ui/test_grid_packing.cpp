#include "ui/layout/grid_packing.hpp"

#include <catch2/catch_test_macros.hpp>

#include <vector>

using namespace atpl;
using atpl::layout::GridItem;
using atpl::layout::GridProblem;
using atpl::layout::packGrid;

namespace {

GridItem placed(GridCell cell) {
    return { .cell = cell, .span = {} };
}

GridItem sized(int columns = 1, int rows = 1) {
    return { .cell = std::nullopt, .span = { .columns = columns, .rows = rows } };
}

bool same(const GridCell& a, const GridCell& b) {
    return a.column == b.column && a.row == b.row && a.columnSpan == b.columnSpan && a.rowSpan == b.rowSpan;
}

} // namespace

TEST_CASE("items without a position fill the grid row by row from the left", "[ui][layout][grid]") {
    const std::vector<GridItem> items = { sized(), sized(), sized() };
    const auto packing = packGrid(items, 2, 2);

    REQUIRE_FALSE(packing.failed.has_value());
    REQUIRE(packing.rows == 2);
    REQUIRE(same(packing.cells[0], { .column = 0, .row = 0 }));
    REQUIRE(same(packing.cells[1], { .column = 1, .row = 0 }));
    REQUIRE(same(packing.cells[2], { .column = 0, .row = 1 }));
}

TEST_CASE("items with a position take their cells, and the others go around them", "[ui][layout][grid]") {
    const std::vector<GridItem> items = {
        sized(), placed({ .column = 0, .row = 0 }), sized(), placed({ .column = 0, .row = 1, .columnSpan = 2 }),
        sized(),
    };
    const auto packing = packGrid(items, 2, 3);

    REQUIRE_FALSE(packing.failed.has_value());
    REQUIRE(same(packing.cells[1], { .column = 0, .row = 0 }));
    REQUIRE(same(packing.cells[3], { .column = 0, .row = 1, .columnSpan = 2 }));
    REQUIRE(same(packing.cells[0], { .column = 1, .row = 0 })); // the first free cell
    REQUIRE(same(packing.cells[2], { .column = 0, .row = 2 })); // the second row is full
    REQUIRE(same(packing.cells[4], { .column = 1, .row = 2 }));
}

TEST_CASE("larger items are placed first, equal ones in the order given", "[ui][layout][grid]") {
    const std::vector<GridItem> items = { sized(), sized(), sized(2, 3), sized(2, 1), sized() };
    const auto packing = packGrid(items, 2, 6);

    REQUIRE_FALSE(packing.failed.has_value());
    REQUIRE(same(packing.cells[2], { .column = 0, .row = 0, .columnSpan = 2, .rowSpan = 3 })); // six cells: first
    REQUIRE(same(packing.cells[3], { .column = 0, .row = 3, .columnSpan = 2, .rowSpan = 1 })); // two cells: second
    REQUIRE(same(packing.cells[0], { .column = 0, .row = 4 }));
    REQUIRE(same(packing.cells[1], { .column = 1, .row = 4 }));
    REQUIRE(same(packing.cells[4], { .column = 0, .row = 5 }));
}

TEST_CASE("an item goes into the first place that holds all of it", "[ui][layout][grid]") {
    // The cell at 1, 0 is taken, so two cells side by side are free only in the second row.
    const std::vector<GridItem> items = { placed({ .column = 1, .row = 0 }), sized(2, 1), sized() };
    const auto packing = packGrid(items, 2, 2);

    REQUIRE_FALSE(packing.failed.has_value());
    REQUIRE(same(packing.cells[1], { .column = 0, .row = 1, .columnSpan = 2, .rowSpan = 1 }));
    REQUIRE(same(packing.cells[2], { .column = 0, .row = 0 })); // the gap it left is used
}

TEST_CASE("without a number of rows, the grid has as many as the positions use", "[ui][layout][grid]") {
    const std::vector<GridItem> items = { placed({ .column = 0, .row = 2, .rowSpan = 2 }), sized(), sized() };
    const auto packing = packGrid(items, 1, 0);

    REQUIRE_FALSE(packing.failed.has_value());
    REQUIRE(packing.rows == 4);
    REQUIRE(same(packing.cells[1], { .column = 0, .row = 0 }));
    REQUIRE(same(packing.cells[2], { .column = 0, .row = 1 }));
}

TEST_CASE("what does not fit is reported, with the item and the reason", "[ui][layout][grid]") {
    SECTION("no room left") {
        const std::vector<GridItem> items = { sized(), sized(), sized() };
        const auto packing = packGrid(items, 2, 1);
        REQUIRE(packing.failed == 2);
        REQUIRE(packing.problem == GridProblem::NoRoom);
    }
    SECTION("a span without rows to put it in") {
        const std::vector<GridItem> items = { sized(1, 5) };
        const auto packing = packGrid(items, 1, 0); // no rows asked for, and no position uses any
        REQUIRE(packing.failed == 0);
        REQUIRE(packing.problem == GridProblem::NoRoom);
    }
    SECTION("a span larger than the grid") {
        const std::vector<GridItem> items = { sized(3, 1) };
        const auto packing = packGrid(items, 2, 4);
        REQUIRE(packing.failed == 0);
        REQUIRE(packing.problem == GridProblem::Outside);
    }
    SECTION("a position outside the grid") {
        const std::vector<GridItem> items = { sized(), placed({ .column = 2, .row = 0 }) };
        const auto packing = packGrid(items, 2, 4);
        REQUIRE(packing.failed == 1);
        REQUIRE(packing.problem == GridProblem::Outside);

        const std::vector<GridItem> below = { placed({ .column = 0, .row = 3, .rowSpan = 2 }) };
        REQUIRE(packGrid(below, 2, 4).problem == GridProblem::Outside);
        const std::vector<GridItem> negative = { placed({ .column = -1, .row = 0 }) };
        REQUIRE(packGrid(negative, 2, 4).failed == 0);
    }
    SECTION("a span of nothing") {
        const std::vector<GridItem> items = { sized(0, 1) };
        REQUIRE(packGrid(items, 2, 4).problem == GridProblem::BadSpan);
        const std::vector<GridItem> positioned = { placed({ .column = 0, .row = 0, .rowSpan = 0 }) };
        REQUIRE(packGrid(positioned, 2, 4).problem == GridProblem::BadSpan);
    }
    SECTION("two positions on the same cell") {
        const std::vector<GridItem> items = { placed({ .column = 0, .row = 0, .columnSpan = 2 }),
                                              placed({ .column = 1, .row = 0 }) };
        const auto packing = packGrid(items, 2, 4);
        REQUIRE(packing.failed == 1);
        REQUIRE(packing.problem == GridProblem::Overlap);

        // Unless that is allowed: then both keep their cells, and the others stay clear of them.
        const std::vector<GridItem> shared = { items[0], items[1], sized() };
        const auto allowed = packGrid(shared, 2, 4, true);
        REQUIRE_FALSE(allowed.failed.has_value());
        REQUIRE(same(allowed.cells[1], { .column = 1, .row = 0 }));
        REQUIRE(same(allowed.cells[2], { .column = 0, .row = 1 }));
    }
}

TEST_CASE("an empty grid is fine", "[ui][layout][grid]") {
    const auto packing = packGrid({}, 2, 0);
    REQUIRE_FALSE(packing.failed.has_value());
    REQUIRE(packing.rows == 0);
    REQUIRE(packing.cells.empty());
}
