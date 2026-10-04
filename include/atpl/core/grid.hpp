#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>

namespace atpl {

/// Which cells count as neighbours: the four that share a side, or also the four diagonal ones.
enum class Neighbourhood { Four, Eight };

/// What happens at the border: neighbours outside the grid are left out, or the grid wraps
/// around, its right edge joined to its left and its bottom to its top.
enum class Edges { Skip, Wrap };

/// A step from a cell to a neighbour.
struct GridOffset {
    int x;
    int y;
};

/// The neighbours that share a side, in reading order: up, left, right, down.
inline constexpr std::array<GridOffset, 4> fourNeighbours{ { { 0, -1 }, { -1, 0 }, { 1, 0 }, { 0, 1 } } };

/// All eight neighbours, in reading order.
inline constexpr std::array<GridOffset, 8> eightNeighbours{
    { { -1, -1 }, { 0, -1 }, { 1, -1 }, { -1, 0 }, { 1, 0 }, { -1, 1 }, { 0, 1 }, { 1, 1 } }
};

/// Anything with integer-like `x` and `y`, such as `sf::Vector2i`, to name a cell with.
template <typename V>
concept GridPoint = requires(const V& v) {
    { static_cast<int>(v.x) };
    { static_cast<int>(v.y) };
};

/// A 2D grid of cells: the data structure pathfinding, cellular automata and diffusion start from.
///
///     Grid<float> heat(200, 100);                // 200 wide, 100 high, every cell 0
///     heat(10, 20) = 1.f;                        // unchecked, fast
///     heat.at(x, y);                             // checked: throws outside the grid
///     heat.forEachNeighbour(x, y, [&](int nx, int ny, float& cell) { ... });
///
/// The cells are stored row after row in one block, so a loop over the rows spreads over a
/// `ThreadPool` by rows, each part with rows of its own:
///
///     pool.parallelFor(heat.height(), [&](std::size_t first, std::size_t end) {
///         for (int y = static_cast<int>(first); y < static_cast<int>(end); ++y) {
///             for (int x = 0; x < heat.width(); ++x) { ... }
///         }
///     });
///
/// For a step that reads the old state and writes a new one, keep two grids and `swap` them,
/// which costs nothing.
///
/// Every `T` is stored as itself, `bool` too (unlike `std::vector<bool>`, which packs bits), so
/// every cell has an address and different cells may be written from different threads.
/// `T` must be default-constructible and copyable. Copying into a grid of the same size reuses
/// its memory, so a grid copied into every published state allocates nothing after the first.
///
/// Thread safety: like an array. Threads may read at once, and write different cells at once.
template <typename T>
class Grid {
public:
    /// An empty grid, 0 by 0.
    Grid() = default;

    /// `width` by `height` cells, each a copy of `fill`. Throws `std::invalid_argument` if a
    /// side is negative.
    Grid(int width, int height, const T& fill = T{});

    Grid(const Grid& other);
    Grid& operator=(const Grid& other);
    Grid(Grid&& other) noexcept;
    Grid& operator=(Grid&& other) noexcept;
    ~Grid() = default;

    [[nodiscard]] int width() const { return m_width; }
    [[nodiscard]] int height() const { return m_height; }

    /// The number of cells.
    [[nodiscard]] std::size_t size() const {
        return static_cast<std::size_t>(m_width) * static_cast<std::size_t>(m_height);
    }
    [[nodiscard]] bool empty() const { return size() == 0; }

    /// Whether (x, y) is a cell of the grid.
    [[nodiscard]] bool contains(int x, int y) const { return x >= 0 && y >= 0 && x < m_width && y < m_height; }
    template <GridPoint V>
    [[nodiscard]] bool contains(const V& p) const {
        return contains(static_cast<int>(p.x), static_cast<int>(p.y));
    }

    /// The cell at (x, y), unchecked (asserted in debug builds).
    [[nodiscard]] T& operator()(int x, int y) {
        assert(contains(x, y));
        return m_cells[index(x, y)];
    }
    [[nodiscard]] const T& operator()(int x, int y) const {
        assert(contains(x, y));
        return m_cells[index(x, y)];
    }
    template <GridPoint V>
    [[nodiscard]] T& operator()(const V& p) {
        return (*this)(static_cast<int>(p.x), static_cast<int>(p.y));
    }
    template <GridPoint V>
    [[nodiscard]] const T& operator()(const V& p) const {
        return (*this)(static_cast<int>(p.x), static_cast<int>(p.y));
    }

    /// The cell at (x, y). Throws `std::out_of_range` outside the grid.
    [[nodiscard]] T& at(int x, int y) { return m_cells[checkedIndex(x, y)]; }
    [[nodiscard]] const T& at(int x, int y) const { return m_cells[checkedIndex(x, y)]; }
    template <GridPoint V>
    [[nodiscard]] T& at(const V& p) {
        return at(static_cast<int>(p.x), static_cast<int>(p.y));
    }
    template <GridPoint V>
    [[nodiscard]] const T& at(const V& p) const {
        return at(static_cast<int>(p.x), static_cast<int>(p.y));
    }

    /// The cell at (x, y) with the grid wrapped around: (-1, 0) is the last cell of the first row.
    /// The grid must not be empty.
    [[nodiscard]] T& wrapped(int x, int y) {
        assert(!empty());
        return m_cells[index(wrap(x, m_width), wrap(y, m_height))];
    }
    [[nodiscard]] const T& wrapped(int x, int y) const {
        assert(!empty());
        return m_cells[index(wrap(x, m_width), wrap(y, m_height))];
    }
    template <GridPoint V>
    [[nodiscard]] T& wrapped(const V& p) {
        return wrapped(static_cast<int>(p.x), static_cast<int>(p.y));
    }
    template <GridPoint V>
    [[nodiscard]] const T& wrapped(const V& p) const {
        return wrapped(static_cast<int>(p.x), static_cast<int>(p.y));
    }

    /// Where (x, y) is in `cells()`: `y * width() + x`.
    [[nodiscard]] std::size_t index(int x, int y) const {
        return static_cast<std::size_t>(y) * static_cast<std::size_t>(m_width) + static_cast<std::size_t>(x);
    }

    /// Every cell, row after row.
    [[nodiscard]] std::span<T> cells() { return { m_cells.get(), size() }; }
    [[nodiscard]] std::span<const T> cells() const { return { m_cells.get(), size() }; }

    /// The cells of row `y`.
    [[nodiscard]] std::span<T> row(int y) {
        assert(y >= 0 && y < m_height);
        return cells().subspan(index(0, y), static_cast<std::size_t>(m_width));
    }
    [[nodiscard]] std::span<const T> row(int y) const {
        assert(y >= 0 && y < m_height);
        return cells().subspan(index(0, y), static_cast<std::size_t>(m_width));
    }

    /// The cells of the rows `first` to `end - 1`, one block: what a part of a `parallelFor`
    /// over the height covers.
    [[nodiscard]] std::span<T> rows(std::size_t first, std::size_t end) {
        assert(first <= end && end <= static_cast<std::size_t>(m_height));
        return cells().subspan(
            first * static_cast<std::size_t>(m_width), (end - first) * static_cast<std::size_t>(m_width)
        );
    }
    [[nodiscard]] std::span<const T> rows(std::size_t first, std::size_t end) const {
        assert(first <= end && end <= static_cast<std::size_t>(m_height));
        return cells().subspan(
            first * static_cast<std::size_t>(m_width), (end - first) * static_cast<std::size_t>(m_width)
        );
    }

    T* begin() { return m_cells.get(); }
    T* end() { return m_cells.get() + size(); }
    const T* begin() const { return m_cells.get(); }
    const T* end() const { return m_cells.get() + size(); }

    /// Sets every cell to `value`.
    void fill(const T& value) { std::fill(begin(), end(), value); }

    /// Makes it `width` by `height` cells, each a copy of `fill`; the old cells are gone.
    /// Throws `std::invalid_argument` if a side is negative.
    void resize(int width, int height, const T& fill = T{});

    /// Calls `fn(x, y, cell)` for each neighbour of (x, y), in reading order. With `Edges::Skip`,
    /// neighbours outside the grid are left out; with `Edges::Wrap`, the grid wraps around (on a
    /// grid narrower than 3 cells, a neighbour can then be the same cell twice, or (x, y) itself).
    template <typename Fn>
    void forEachNeighbour(
        int x, int y, Fn&& fn, Neighbourhood neighbourhood = Neighbourhood::Eight, Edges edges = Edges::Skip
    );
    template <typename Fn>
    void forEachNeighbour(
        int x, int y, Fn&& fn, Neighbourhood neighbourhood = Neighbourhood::Eight, Edges edges = Edges::Skip
    ) const;

    friend void swap(Grid& a, Grid& b) noexcept {
        std::swap(a.m_width, b.m_width);
        std::swap(a.m_height, b.m_height);
        std::swap(a.m_cells, b.m_cells);
    }

private:
    static int wrap(int value, int extent) {
        const int rest = value % extent;
        return rest < 0 ? rest + extent : rest;
    }

    std::size_t checkedIndex(int x, int y) const;

    template <typename Self, typename Fn>
    static void visitNeighbours(Self& self, int x, int y, Fn& fn, Neighbourhood neighbourhood, Edges edges);

    int m_width = 0;
    int m_height = 0;
    std::unique_ptr<T[]> m_cells; ///< Not a vector: `std::vector<bool>` would pack the cells.
};

// Implementation

template <typename T>
Grid<T>::Grid(int width, int height, const T& fill) {
    resize(width, height, fill);
}

template <typename T>
Grid<T>::Grid(const Grid& other) :
    m_width(other.m_width),
    m_height(other.m_height),
    m_cells(other.size() == 0 ? nullptr : std::make_unique<T[]>(other.size())) {
    std::copy(other.begin(), other.end(), begin());
}

template <typename T>
Grid<T>& Grid<T>::operator=(const Grid& other) {
    if (this == &other) {
        return *this;
    }
    if (m_width == other.m_width && m_height == other.m_height) {
        std::copy(other.begin(), other.end(), begin()); // the same size: no new memory
        return *this;
    }
    Grid copy(other);
    swap(*this, copy);
    return *this;
}

template <typename T>
Grid<T>::Grid(Grid&& other) noexcept :
    m_width(std::exchange(other.m_width, 0)),
    m_height(std::exchange(other.m_height, 0)),
    m_cells(std::move(other.m_cells)) {}

template <typename T>
Grid<T>& Grid<T>::operator=(Grid&& other) noexcept {
    Grid moved(std::move(other));
    swap(*this, moved);
    return *this;
}

template <typename T>
void Grid<T>::resize(int width, int height, const T& fill) {
    if (width < 0 || height < 0) {
        throw std::invalid_argument("Grid: a side of " + std::to_string(std::min(width, height)) + " cells");
    }
    const std::size_t count = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    m_cells = count == 0 ? nullptr : std::make_unique<T[]>(count);
    m_width = width;
    m_height = height;
    this->fill(fill);
}

template <typename T>
std::size_t Grid<T>::checkedIndex(int x, int y) const {
    if (!contains(x, y)) {
        throw std::out_of_range(
            "Grid: no cell (" + std::to_string(x) + ", " + std::to_string(y) + ") in " + std::to_string(m_width) +
            " by " + std::to_string(m_height)
        );
    }
    return index(x, y);
}

template <typename T>
template <typename Self, typename Fn>
void Grid<T>::visitNeighbours(Self& self, int x, int y, Fn& fn, Neighbourhood neighbourhood, Edges edges) {
    const auto visit = [&](const auto& offsets) {
        for (const GridOffset offset : offsets) {
            int nx = x + offset.x;
            int ny = y + offset.y;
            if (edges == Edges::Wrap) {
                nx = wrap(nx, self.m_width);
                ny = wrap(ny, self.m_height);
            } else if (!self.contains(nx, ny)) {
                continue;
            }
            fn(nx, ny, self.cells()[self.index(nx, ny)]); // through cells(): const for a const grid
        }
    };
    if (self.empty()) {
        return;
    }
    if (neighbourhood == Neighbourhood::Four) {
        visit(fourNeighbours);
    } else {
        visit(eightNeighbours);
    }
}

template <typename T>
template <typename Fn>
void Grid<T>::forEachNeighbour(int x, int y, Fn&& fn, Neighbourhood neighbourhood, Edges edges) {
    visitNeighbours(*this, x, y, fn, neighbourhood, edges);
}

template <typename T>
template <typename Fn>
void Grid<T>::forEachNeighbour(int x, int y, Fn&& fn, Neighbourhood neighbourhood, Edges edges) const {
    visitNeighbours(*this, x, y, fn, neighbourhood, edges);
}

} // namespace atpl
