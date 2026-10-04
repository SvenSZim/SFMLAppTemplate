#include "atpl/core/grid.hpp"
#include "atpl/core/thread_pool.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <numeric>
#include <set>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

using namespace atpl;

namespace {

struct Cell {
    int x;
    int y;
};

/// The neighbours a call visits, as (x, y) pairs in the order they came.
template <typename G>
std::vector<std::pair<int, int>> neighboursOf(G& grid, int x, int y, Neighbourhood n, Edges e = Edges::Skip) {
    std::vector<std::pair<int, int>> found;
    grid.forEachNeighbour(x, y, [&](int nx, int ny, auto&) { found.emplace_back(nx, ny); }, n, e);
    return found;
}

} // namespace

TEST_CASE("a grid has its size, and every cell starts as the fill", "[core][grid]") {
    const Grid<int> grid(4, 3, 7);
    REQUIRE(grid.width() == 4);
    REQUIRE(grid.height() == 3);
    REQUIRE(grid.size() == 12);
    REQUIRE_FALSE(grid.empty());
    for (const int cell : grid) {
        REQUIRE(cell == 7);
    }

    const Grid<int> none;
    REQUIRE(none.empty());
    REQUIRE(none.cells().empty());
    REQUIRE(Grid<int>(0, 5).empty());
}

TEST_CASE("cells are stored row after row", "[core][grid]") {
    Grid<int> grid(3, 2);
    grid(0, 0) = 1;
    grid(2, 0) = 2;
    grid(0, 1) = 3;
    grid(2, 1) = 4;
    REQUIRE(grid.index(2, 1) == 5);
    REQUIRE(std::vector<int>(grid.cells().begin(), grid.cells().end()) == std::vector<int>{ 1, 0, 2, 3, 0, 4 });
    REQUIRE(std::vector<int>(grid.row(1).begin(), grid.row(1).end()) == std::vector<int>{ 3, 0, 4 });
}

TEST_CASE("a grid takes any point with x and y", "[core][grid]") {
    Grid<int> grid(3, 3);
    grid(Cell{ 1, 2 }) = 5;
    REQUIRE(grid(1, 2) == 5);
    REQUIRE(grid.at(Cell{ 1, 2 }) == 5);
    REQUIRE(grid.contains(Cell{ 2, 2 }));
    REQUIRE_FALSE(grid.contains(Cell{ 3, 0 }));
    REQUIRE(grid.wrapped(Cell{ -2, -1 }) == 5);
}

TEST_CASE("checked access throws outside the grid", "[core][grid]") {
    Grid<int> grid(3, 2);
    REQUIRE_NOTHROW(grid.at(2, 1));
    REQUIRE_THROWS_AS(grid.at(3, 0), std::out_of_range);
    REQUIRE_THROWS_AS(grid.at(0, 2), std::out_of_range);
    REQUIRE_THROWS_AS(grid.at(-1, 0), std::out_of_range);
    const Grid<int>& view = grid;
    REQUIRE_THROWS_AS(view.at(0, -1), std::out_of_range);
    REQUIRE_FALSE(grid.contains(-1, 0));
    REQUIRE(grid.contains(0, 0));
}

TEST_CASE("a negative side is refused", "[core][grid]") {
    REQUIRE_THROWS_AS(Grid<int>(-1, 3), std::invalid_argument);
    Grid<int> grid(2, 2);
    REQUIRE_THROWS_AS(grid.resize(2, -3), std::invalid_argument);
}

TEST_CASE("wrapped access joins the edges", "[core][grid]") {
    Grid<int> grid(4, 3);
    std::iota(grid.begin(), grid.end(), 0);
    REQUIRE(grid.wrapped(-1, 0) == grid(3, 0));
    REQUIRE(grid.wrapped(4, 0) == grid(0, 0));
    REQUIRE(grid.wrapped(0, -1) == grid(0, 2));
    REQUIRE(grid.wrapped(9, 7) == grid(1, 1));
    REQUIRE(grid.wrapped(-9, -7) == grid(3, 2));
}

TEST_CASE("neighbours are visited in reading order, skipping the outside", "[core][grid]") {
    Grid<int> grid(3, 3);
    using Found = std::vector<std::pair<int, int>>;
    REQUIRE(neighboursOf(grid, 1, 1, Neighbourhood::Four) == Found{ { 1, 0 }, { 0, 1 }, { 2, 1 }, { 1, 2 } });
    REQUIRE(neighboursOf(grid, 1, 1, Neighbourhood::Eight).size() == 8);
    REQUIRE(neighboursOf(grid, 0, 0, Neighbourhood::Eight) == Found{ { 1, 0 }, { 0, 1 }, { 1, 1 } });
    REQUIRE(neighboursOf(grid, 2, 2, Neighbourhood::Four) == Found{ { 2, 1 }, { 1, 2 } });
}

TEST_CASE("neighbours wrap around where asked", "[core][grid]") {
    Grid<int> grid(4, 4);
    using Found = std::vector<std::pair<int, int>>;
    REQUIRE(
        neighboursOf(grid, 0, 0, Neighbourhood::Four, Edges::Wrap) == Found{ { 0, 3 }, { 3, 0 }, { 1, 0 }, { 0, 1 } }
    );
    REQUIRE(neighboursOf(grid, 3, 3, Neighbourhood::Eight, Edges::Wrap).size() == 8);
}

TEST_CASE("neighbours can be changed through the call, and only read on a const grid", "[core][grid]") {
    Grid<int> grid(3, 3);
    grid.forEachNeighbour(1, 1, [](int, int, int& cell) { cell = 1; });
    REQUIRE(std::accumulate(grid.begin(), grid.end(), 0) == 8);
    REQUIRE(grid(1, 1) == 0);

    const Grid<int>& view = grid;
    int sum = 0;
    view.forEachNeighbour(0, 0, [&](int, int, auto& cell) {
        static_assert(std::is_const_v<std::remove_reference_t<decltype(cell)>>);
        sum += cell;
    });
    REQUIRE(sum == 2); // (1, 0) and (0, 1); (1, 1) is the untouched centre
}

TEST_CASE("an empty grid has no neighbours", "[core][grid]") {
    Grid<int> grid;
    REQUIRE(neighboursOf(grid, 0, 0, Neighbourhood::Eight, Edges::Wrap).empty());
}

TEST_CASE("a grid of bool stores real bools", "[core][grid]") {
    Grid<bool> alive(5, 5, false);
    bool& cell = alive(2, 3);
    cell = true;
    REQUIRE(alive.at(2, 3));
    REQUIRE(&alive(1, 0) == &alive(0, 0) + 1); // one byte each, an address each
    static_assert(std::is_same_v<decltype(alive.cells()), std::span<bool>>);
}

TEST_CASE("fill and resize", "[core][grid]") {
    Grid<int> grid(2, 2, 1);
    grid.fill(4);
    REQUIRE(std::accumulate(grid.begin(), grid.end(), 0) == 16);
    grid.resize(3, 1, 2);
    REQUIRE(grid.width() == 3);
    REQUIRE(grid.height() == 1);
    REQUIRE(std::accumulate(grid.begin(), grid.end(), 0) == 6);
    grid.resize(0, 0);
    REQUIRE(grid.empty());
}

TEST_CASE("copies are deep, moves and swaps hand the cells over", "[core][grid]") {
    Grid<int> grid(2, 2, 1);
    Grid<int> copy = grid;
    copy(0, 0) = 9;
    REQUIRE(grid(0, 0) == 1);
    copy = grid;
    REQUIRE(copy(0, 0) == 1);

    Grid<int> other(3, 1, 5);
    const int* cells = other.cells().data();
    swap(grid, other);
    REQUIRE(grid.width() == 3);
    REQUIRE(grid.cells().data() == cells); // nothing copied
    REQUIRE(other.width() == 2);

    const Grid<int> moved = std::move(grid);
    REQUIRE(moved.cells().data() == cells);
    REQUIRE(grid.empty()); // NOLINT(bugprone-use-after-move): a moved-from grid is empty

    Grid<int> empty;
    Grid<int> fromEmpty = empty;
    REQUIRE(fromEmpty.empty());
}

TEST_CASE("copying into a grid of the same size keeps its memory", "[core][grid]") {
    const Grid<int> source(4, 3, 2);
    Grid<int> target(4, 3, 0);
    const int* memory = target.cells().data();
    target = source;
    REQUIRE(target.cells().data() == memory);
    REQUIRE(target(3, 2) == 2);

    Grid<int> other(2, 2);
    other = source; // another size: new memory
    REQUIRE(other.width() == 4);
    REQUIRE(other(3, 2) == 2);
}

TEST_CASE("rows give the cells a part of a loop over the height covers", "[core][grid]") {
    Grid<int> grid(4, 5);
    std::iota(grid.begin(), grid.end(), 0);
    const std::span<int> middle = grid.rows(1, 3);
    REQUIRE(middle.size() == 8);
    REQUIRE(middle.front() == 4);
    REQUIRE(middle.back() == 11);
    REQUIRE(grid.rows(2, 2).empty());
}

TEST_CASE("a grid steps in parallel by rows into a second grid", "[core][grid]") {
    // One step of diffusion, read from one grid and written into another, then swapped.
    ThreadPool pool(3);
    Grid<int> now(30, 20, 0);
    now(15, 10) = 9000;
    Grid<int> next(30, 20);
    const auto step = [](const Grid<int>& from, Grid<int>& to, int x, int y) {
        int sum = from(x, y);
        int count = 1;
        from.forEachNeighbour(
            x,
            y,
            [&](int, int, const int& cell) {
                sum += cell;
                ++count;
            },
            Neighbourhood::Four
        );
        to(x, y) = sum / count;
    };
    Grid<int> expected = now;
    Grid<int> scratch(30, 20);
    for (int y = 0; y < expected.height(); ++y) {
        for (int x = 0; x < expected.width(); ++x) {
            step(expected, scratch, x, y);
        }
    }
    swap(expected, scratch);

    pool.parallelFor(static_cast<std::size_t>(now.height()), [&](std::size_t first, std::size_t end) {
        for (int y = static_cast<int>(first); y < static_cast<int>(end); ++y) {
            for (int x = 0; x < now.width(); ++x) {
                step(now, next, x, y);
            }
        }
    });
    swap(now, next);
    REQUIRE(std::vector<int>(now.begin(), now.end()) == std::vector<int>(expected.begin(), expected.end()));
    REQUIRE(now(15, 10) == 1800);
    REQUIRE(now(16, 10) == 1800); // a fifth of the heat in each of the five cells
    REQUIRE(now(17, 10) == 0);
}
