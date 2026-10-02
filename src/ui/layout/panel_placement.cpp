#include "ui/layout/panel_placement.hpp"

#include "atpl/ui/error.hpp"

#include "ui/layout/grid_packing.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <variant>
#include <vector>

namespace atpl::layout {

namespace {

constexpr std::array anchors = {
    Anchor::TopLeft, Anchor::Top,        Anchor::TopRight, Anchor::Left,
    Anchor::Right,   Anchor::BottomLeft, Anchor::Bottom,   Anchor::BottomRight,
};

enum class Side { Start, Middle, End };

/// Where an anchor is horizontally: at the left edge, centred, or at the right edge.
[[nodiscard]] Side horizontal(Anchor anchor) {
    switch (anchor) {
        case Anchor::TopLeft:
        case Anchor::Left:
        case Anchor::BottomLeft:
            return Side::Start;
        case Anchor::Top:
        case Anchor::Bottom:
            return Side::Middle;
        case Anchor::TopRight:
        case Anchor::Right:
        case Anchor::BottomRight:
            break;
    }
    return Side::End;
}

/// The same vertically: at the top edge, centred, or at the bottom edge.
[[nodiscard]] Side vertical(Anchor anchor) {
    switch (anchor) {
        case Anchor::TopLeft:
        case Anchor::Top:
        case Anchor::TopRight:
            return Side::Start;
        case Anchor::Left:
        case Anchor::Right:
            return Side::Middle;
        case Anchor::BottomLeft:
        case Anchor::Bottom:
        case Anchor::BottomRight:
            break;
    }
    return Side::End;
}

/// Sets a panel's place. A different size, or appearing, means painting it again.
void assign(model::Panel& panel, const FloatRect& rect, bool shown) {
    if (shown && (!panel.shown || rect.size() != panel.rect.size())) {
        panel.dirty = true;
    }
    panel.shown = shown;
    if (shown) {
        panel.rect = rect;
    }
}

/// Shares `available` height among panels that want `heights[i]` each, and writes what each
/// gets back into `heights`: nobody gets more than it wants, and what the modest ones leave goes to the others in equal
/// parts.
void share(std::vector<float>& heights, float available) {
    std::vector<std::size_t> order(heights.size());
    for (std::size_t i = 0; i < order.size(); ++i) {
        order[i] = i;
    }
    std::sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) { return heights[a] < heights[b]; });

    float left = available;
    for (std::size_t i = 0; i < order.size(); ++i) {
        const float equalPart = left / static_cast<float>(order.size() - i);
        float& height = heights[order[i]];
        height = std::min(height, equalPart);
        left -= height;
    }
}

/// A floating panel is as wide as it asks to be, or as the theme says, but never wider than the
/// window.
[[nodiscard]] float floatingWidth(const model::Panel& panel, sf::Vector2f window, const Sizes& sizes) {
    const float asked = panel.width > 0.f ? panel.width * sizes.scale.x : sizes.panelWidth;
    return std::round(std::min(asked, window.x));
}

/// One stack of floating panels.
void placeStack(std::span<model::Panel> panels, Anchor anchor, sf::Vector2f window, const Sizes& sizes) {
    // The panels of this stack that the application wants to see, in the order listed.
    std::vector<model::Panel*> stack;
    for (model::Panel& panel : panels) {
        const Anchor* own = std::get_if<Anchor>(&panel.placement);
        if (own != nullptr && *own == anchor && panel.visible) {
            stack.push_back(&panel);
        }
    }
    if (stack.empty()) {
        return;
    }

    const float header = sizes.headerHeight;
    const float gap = sizes.margin;
    const float available = window.y - 2.f * sizes.margin;

    // If not even the headers fit, the last panels have to go.
    std::size_t count = stack.size();
    while (count > 0 && static_cast<float>(count) * header + static_cast<float>(count - 1) * gap > available) {
        --count;
        assign(*stack[count], {}, false);
    }
    stack.resize(count);
    if (stack.empty()) {
        return;
    }

    // Heights: what each panel wants, cut down if the stack is too high.
    std::vector<float> heights(count);
    float total = static_cast<float>(count - 1) * gap;
    for (std::size_t i = 0; i < count; ++i) {
        heights[i] = wantedHeight(*stack[i], sizes);
        total += heights[i];
    }
    if (total > available) {
        // Every panel keeps its header; what is left over is shared among the contents.
        for (float& height : heights) {
            height -= header;
        }
        share(heights, available - static_cast<float>(count - 1) * gap - static_cast<float>(count) * header);
        total = static_cast<float>(count - 1) * gap;
        for (float& height : heights) {
            height = std::floor(height + header);
            total += height;
        }
    }

    // Vertically: from the top, from the bottom, or centred.
    const Side side = vertical(anchor);
    float y = sizes.margin;
    if (side == Side::Middle) {
        y = std::round((window.y - total) * 0.5f);
    } else if (side == Side::End) {
        y = window.y - sizes.margin;
    }

    for (std::size_t i = 0; i < count; ++i) {
        model::Panel& panel = *stack[i];

        // Horizontally: in a narrow window the margin shrinks first, then the panel.
        const float width = floatingWidth(panel, window, sizes);
        const float margin = std::clamp((window.x - width) * 0.5f, 0.f, sizes.margin);
        float x = margin;
        if (horizontal(anchor) == Side::Middle) {
            x = (window.x - width) * 0.5f;
        } else if (horizontal(anchor) == Side::End) {
            x = window.x - margin - width;
        }

        if (side == Side::End) {
            y -= heights[i]; // upwards: the first panel is the lowest
            assign(panel, FloatRect(std::round(x), std::round(y), width, heights[i]), true);
            y -= gap;
        } else {
            assign(panel, FloatRect(std::round(x), std::round(y), width, heights[i]), true);
            y += heights[i] + gap;
        }
    }
}

/// The rectangle of grid cells, with whole pixels at every edge so that neighbours line up.
[[nodiscard]] FloatRect cellRect(const GridCell& cell, sf::Vector2f window, GridSetup grid, float margin) {
    const auto edges = [margin](float extent, int count, int first, int span) {
        const float size = (extent - margin * static_cast<float>(count + 1)) / static_cast<float>(count);
        const float start = margin + static_cast<float>(first) * (size + margin);
        const float end = start + static_cast<float>(span) * size + static_cast<float>(span - 1) * margin;
        return std::pair(std::round(start), std::round(end));
    };
    const auto [left, right] = edges(window.x, grid.columns, cell.column, cell.columnSpan);
    const auto [top, bottom] = edges(window.y, grid.rows, cell.row, cell.rowSpan);
    return { left, top, std::max(right - left, 0.f), std::max(bottom - top, 0.f) };
}

[[nodiscard]] std::string inQuotes(const std::string& name) {
    return '"' + name + '"';
}

} // namespace

void preparePanels(model::Store& store, GridSetup grid) {
    if (grid.columns < 1 || grid.rows < 1) {
        throw SetupError(
            "the window's grid has " + std::to_string(grid.columns) + " columns and " + std::to_string(grid.rows) +
            " rows; it needs at least 1 of each"
        );
    }
    const std::string gridSize =
        std::to_string(grid.columns) + " columns and " + std::to_string(grid.rows) + " rows (counted from 0)";

    // The panels that are in the grid, with a position or without.
    std::vector<model::Panel*> inGrid;
    std::vector<GridItem> items;
    for (model::Panel& panel : store.panels()) {
        if (panel.width < 0.f) {
            throw SetupError("panel " + inQuotes(panel.name) + " has a negative width");
        }
        if (const GridCell* cell = std::get_if<GridCell>(&panel.placement)) {
            inGrid.push_back(&panel);
            items.push_back({ .cell = *cell, .span = {} });
        } else if (const GridSpan* span = std::get_if<GridSpan>(&panel.placement)) {
            inGrid.push_back(&panel);
            items.push_back({ .cell = std::nullopt, .span = *span });
        }
    }

    // Panels with a position may share cells: an application can show one of several panels in
    // the same place (D40).
    const GridPacking packing = packGrid(items, grid.columns, grid.rows, true);
    if (packing.failed.has_value()) {
        const std::string panel = "panel " + inQuotes(inGrid[*packing.failed]->name);
        const GridItem& item = items[*packing.failed];
        switch (packing.problem) {
            case GridProblem::BadSpan:
                throw SetupError(panel + " spans less than one grid cell");
            case GridProblem::Outside:
                if (item.cell.has_value()) {
                    throw SetupError(
                        panel + " is placed outside the window's grid: columns " + std::to_string(item.cell->column) +
                        " to " + std::to_string(item.cell->column + item.cell->columnSpan - 1) + ", rows " +
                        std::to_string(item.cell->row) + " to " +
                        std::to_string(item.cell->row + item.cell->rowSpan - 1) + ", but the grid has " + gridSize
                    );
                }
                throw SetupError(
                    panel + " spans " + std::to_string(item.span.columns) + " columns and " +
                    std::to_string(item.span.rows) + " rows, but the window's grid has only " + gridSize
                );
            case GridProblem::Overlap:
            case GridProblem::NoRoom:
                break;
        }
        throw SetupError(
            panel + " finds no room in the window's grid: no " + std::to_string(item.span.columns) + " by " +
            std::to_string(item.span.rows) + " cells are free; the grid has " + gridSize
        );
    }

    for (std::size_t i = 0; i < inGrid.size(); ++i) {
        inGrid[i]->placement = packing.cells[i];
    }
}

float panelWidth(const model::Panel& panel, sf::Vector2f windowSize, GridSetup grid, const Sizes& sizes) {
    if (const GridCell* cell = std::get_if<GridCell>(&panel.placement)) {
        return cellRect(*cell, windowSize, grid, sizes.margin).width();
    }
    return floatingWidth(panel, windowSize, sizes);
}

float wantedHeight(const model::Panel& panel, const Sizes& sizes) {
    return sizes.headerHeight + (panel.collapsed ? 0.f : panel.contentHeight);
}

void placePanels(model::Store& store, sf::Vector2f windowSize, GridSetup grid, const Sizes& sizes) {
    const std::span<model::Panel> panels = store.panels();

    for (model::Panel& panel : panels) {
        if (!panel.visible) {
            assign(panel, {}, false);
            continue;
        }
        if (const GridCell* cell = std::get_if<GridCell>(&panel.placement)) {
            FloatRect rect = cellRect(*cell, windowSize, grid, sizes.margin);
            if (panel.collapsed) {
                rect.setHeight(std::min(rect.height(), sizes.headerHeight));
            }
            assign(panel, rect, rect.width() > 0.f && rect.height() > 0.f);
        }
    }

    for (const Anchor anchor : anchors) {
        placeStack(panels, anchor, windowSize, sizes);
    }
}

void placeViews(model::Store& store, sf::Vector2f windowSize, const Sizes& sizes) {
    for (model::View& view : store.views()) {
        if (!view.widget.has_value()) {
            view.rect = FloatRect({ 0.f, 0.f }, windowSize);
            continue;
        }

        const model::WidgetSlot& slot = store.widget(*view.widget);
        const model::Panel& panel = store.panel(slot.panel);
        if (!panel.shown || panel.collapsed || !slot.visible) {
            view.rect = {};
            continue;
        }
        // A widget's rectangle is in its panel's content, which starts below the header.
        const sf::Vector2f content = panel.rect.position() + sf::Vector2f(0.f, sizes.headerHeight);
        view.rect = FloatRect(slot.rect.position() + content, slot.rect.size());
    }
}

} // namespace atpl::layout
