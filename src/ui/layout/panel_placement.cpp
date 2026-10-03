#include "ui/layout/panel_placement.hpp"

#include "atpl/ui/error.hpp"

#include "ui/layout/cell.hpp"
#include "ui/layout/grid_packing.hpp"
#include "ui/layout/rules.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
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
    if (panel.wantedWidth > 0.f) {
        return std::round(std::min(panel.wantedWidth, window.x));
    }
    const float asked = panel.width > 0.f ? panel.width * sizes.scale.x : sizes.panelWidth;
    return std::round(std::min(asked, window.x));
}

/// The panels of the stack at `anchor` that the application wants to see, in the order listed.
[[nodiscard]] std::vector<model::Panel*> stackAt(std::span<model::Panel> panels, Anchor anchor) {
    std::vector<model::Panel*> stack;
    for (model::Panel& panel : panels) {
        const Anchor* own = std::get_if<Anchor>(&panel.placement);
        if (own != nullptr && *own == anchor && panel.visible && !panel.tooSmall) {
            stack.push_back(&panel);
        }
    }
    return stack;
}

/// The height a panel has when it is open.
[[nodiscard]] float openHeightOf(const model::Panel& panel, const Sizes& sizes) {
    return panel.wantedHeight > 0.f ? panel.wantedHeight : sizes.headerHeight + panel.contentHeight;
}

/// One stack of floating panels.
void placeStack(
    std::span<model::Panel> panels,
    Anchor anchor,
    sf::Vector2f window,
    const Sizes& sizes,
    StackOverflow overflow,
    float strip
) {
    std::vector<model::Panel*> stack = stackAt(panels, anchor);
    for (model::Panel* panel : stack) {
        panel->overlapped = false;
        panel->cardLayer = 0;
    }
    if (stack.empty()) {
        return;
    }

    const float header = sizes.headerHeight;
    const float gap = sizes.margin;
    const float available = window.y - 2.f * sizes.margin;
    const bool cards = overflow == StackOverflow::Cards;
    strip = std::clamp(strip > 0.f ? strip : header, 1.f, header);

    // From here on the stack is listed as it is seen, from the top down: a stack at a bottom
    // anchor starts with its last panel. Every panel but the lowest can be a covered card.
    const Side side = vertical(anchor);
    const auto seen = [&](std::vector<model::Panel*> panelsInOrder) {
        if (side == Side::End) {
            std::reverse(panelsInOrder.begin(), panelsInOrder.end());
        }
        return panelsInOrder;
    };

    // If not even the headers fit (or, with cards, the strips), the last panels have to go.
    const auto least = [&](std::size_t count) {
        return cards ? static_cast<float>(count - 1) * strip + header
                     : static_cast<float>(count) * header + static_cast<float>(count - 1) * gap;
    };
    std::size_t count = stack.size();
    while (count > 0 && least(count) > available) {
        --count;
        assign(*stack[count], {}, false);
    }
    stack.resize(count);
    if (stack.empty()) {
        return;
    }
    const std::vector<model::Panel*> view = seen(stack);

    // Heights: what each panel wants. From the top of one to the top of the next: its height and
    // the gap, unless it is covered.
    std::vector<float> heights(count);
    std::vector<float> steps(count);
    float total = static_cast<float>(count - 1) * gap;
    for (std::size_t i = 0; i < count; ++i) {
        model::Panel& panel = *view[i];
        heights[i] = wantedHeight(panel, sizes);
        panel.openHeight = openHeightOf(panel, sizes);
        steps[i] = heights[i] + gap;
        total += heights[i];
    }

    if (total > available && cards) {
        // Cards: collapsed panels are covered down to their strip, only as far as needed; the
        // expanded ones keep their height while there is room, and share it when there is not.
        std::vector<std::size_t> folded;
        std::vector<std::size_t> open;
        for (std::size_t i = 0; i + 1 < count; ++i) {
            (heights[i] <= header + 0.5f ? folded : open).push_back(i);
        }
        float openTotal = heights[count - 1];
        for (const std::size_t i : open) {
            openTotal += heights[i] + gap;
        }
        const float foldedRoom = available - openTotal;
        if (!folded.empty() && foldedRoom >= static_cast<float>(folded.size()) * strip) {
            const float step = std::min(foldedRoom / static_cast<float>(folded.size()), header + gap);
            for (const std::size_t i : folded) {
                steps[i] = std::floor(step);
            }
        } else {
            // Even with every collapsed panel at its strip the open ones are too high: they share
            // what is left, each keeping its header.
            for (const std::size_t i : folded) {
                steps[i] = strip;
            }
            std::vector<std::size_t> shared = open;
            shared.push_back(count - 1);
            std::vector<float> contents;
            float room = available - static_cast<float>(folded.size()) * strip - static_cast<float>(open.size()) * gap;
            for (const std::size_t i : shared) {
                contents.push_back(heights[i] - header);
                room -= header;
            }
            share(contents, std::max(room, 0.f));
            for (std::size_t k = 0; k < shared.size(); ++k) {
                heights[shared[k]] = std::floor(contents[k] + header);
                steps[shared[k]] = heights[shared[k]] + gap;
            }
        }
        total = heights[count - 1];
        for (std::size_t i = 0; i + 1 < count; ++i) {
            total += steps[i];
        }
        for (std::size_t i = 0; i < count; ++i) {
            view[i]->overlapped = true;
            view[i]->cardLayer = static_cast<int>(i); // the lower card covers the one above
        }
    } else if (total > available) {
        // Every panel keeps its header; what is left over is shared among the contents.
        for (float& height : heights) {
            height -= header;
        }
        share(heights, available - static_cast<float>(count - 1) * gap - static_cast<float>(count) * header);
        total = static_cast<float>(count - 1) * gap;
        for (std::size_t i = 0; i < count; ++i) {
            heights[i] = std::floor(heights[i] + header);
            steps[i] = heights[i] + gap;
            total += heights[i];
        }
    }

    // Vertically: from the top, from the bottom, or centred.
    float y = sizes.margin;
    if (side == Side::Middle) {
        y = std::round((window.y - total) * 0.5f);
    } else if (side == Side::End) {
        y = window.y - sizes.margin - total;
    }

    for (std::size_t i = 0; i < count; ++i) {
        model::Panel& panel = *view[i];

        // Horizontally: in a narrow window the margin shrinks first, then the panel.
        const float width = floatingWidth(panel, window, sizes);
        const float margin = std::clamp((window.x - width) * 0.5f, 0.f, sizes.margin);
        float x = margin;
        if (horizontal(anchor) == Side::Middle) {
            x = (window.x - width) * 0.5f;
        } else if (horizontal(anchor) == Side::End) {
            x = window.x - margin - width;
        }
        assign(panel, FloatRect(std::round(x), std::round(y), width, heights[i]), true);
        y += steps[i];
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

float openShare(const model::Panel& panel) {
    // Starts and ends gently: smoothstep.
    const float t = std::clamp(panel.openness(), 0.f, 1.f);
    return t * t * (3.f - 2.f * t);
}

float wantedHeight(const model::Panel& panel, const Sizes& sizes) {
    const float open = panel.wantedHeight > 0.f ? panel.wantedHeight : sizes.headerHeight + panel.contentHeight;
    return std::round(sizes.headerHeight + (open - sizes.headerHeight) * openShare(panel));
}

void placePanels(
    model::Store& store,
    sf::Vector2f windowSize,
    GridSetup grid,
    const Sizes& sizes,
    const Layout& layout,
    float stripHeight
) {
    const std::span<model::Panel> panels = store.panels();

    for (model::Panel& panel : panels) {
        if (!panel.visible || panel.tooSmall) {
            assign(panel, {}, false);
            continue;
        }
        const GridCell* cell = std::get_if<GridCell>(&panel.placement);
        if (cell == nullptr) {
            continue;
        }

        // Its cells, all of them or as much as its content wants, placed in them.
        const PanelRules rules = rulesFor(layout, panel);
        const FloatRect cells = cellRect(*cell, windowSize, grid, sizes.margin);
        FloatRect rect = cells;
        if (rules.fit == Fit::Content && panel.wantedWidth > 0.f) {
            const float height = panel.wantedHeight > 0.f ? panel.wantedHeight : wantedHeight(panel, sizes);
            rect = aligned({ panel.wantedWidth, height }, cells, rules.alignment);
        }

        // A collapsed panel is its header, at the side of its place the rules say; on the way
        // there it shrinks towards that side.
        panel.openHeight = rect.height();
        if (panel.openness() < 1.f) {
            const float height = sizes.headerHeight + (rect.height() - sizes.headerHeight) * openShare(panel);
            rect = aligned({ rect.width(), std::round(height) }, rect, rules.collapseTowards);
        }
        assign(
            panel,
            rect,
            rect.width() > 0.f && rect.height() >= std::min(sizes.headerHeight, cells.height()) && rect.height() > 0.f
        );
    }

    for (const Anchor anchor : anchors) {
        placeStack(panels, anchor, windowSize, sizes, layout.stackOverflow, stripHeight);
    }
}

float cardStrip(const Sizes& sizes, float titleSize) {
    // The title sits in the middle of the header: the strip ends a little below it.
    return std::min(std::ceil(sizes.headerHeight * 0.5f + titleSize * 0.5f + 2.f), sizes.headerHeight);
}

std::vector<PanelId> cardsToFold(
    const model::Store& store, PanelId unfolded, sf::Vector2f windowSize, const Sizes& sizes, const Layout& layout
) {
    const model::Panel& opened = store.panel(unfolded);
    const Anchor* anchor = std::get_if<Anchor>(&opened.placement);
    if (layout.stackOverflow != StackOverflow::Cards || anchor == nullptr) {
        return {};
    }
    // The stack as it would be with this panel open and the others as they are.
    std::vector<PanelId> others;
    float total = -sizes.margin;
    for (std::uint32_t i = 0; i < store.panels().size(); ++i) {
        const model::Panel& panel = store.panel(PanelId{ i });
        const Anchor* own = std::get_if<Anchor>(&panel.placement);
        if (own == nullptr || *own != *anchor || !panel.visible || panel.tooSmall) {
            continue;
        }
        const bool open = PanelId{ i } == unfolded || !panel.collapsed;
        total += (open ? openHeightOf(panel, sizes) : sizes.headerHeight) + sizes.margin;
        if (open && PanelId{ i } != unfolded) {
            others.push_back(PanelId{ i });
        }
    }
    if (total <= windowSize.y - 2.f * sizes.margin) {
        return {}; // there is room for all of them
    }
    return others;
}

std::vector<PanelId>
cardOrder(const model::Store& store, std::span<const PanelId> base, std::optional<PanelId> hovered) {
    std::vector<PanelId> order(base.begin(), base.end());
    // The cards of each overlapped stack, in their layers, in the places the stack has in `base`.
    for (const Anchor anchor : anchors) {
        std::vector<std::size_t> places;
        for (std::size_t i = 0; i < order.size(); ++i) {
            const model::Panel& panel = store.panel(order[i]);
            const Anchor* own = std::get_if<Anchor>(&panel.placement);
            if (own != nullptr && *own == anchor && panel.overlapped) {
                places.push_back(i);
            }
        }
        std::vector<PanelId> cards;
        for (const std::size_t i : places) {
            cards.push_back(order[i]);
        }
        std::stable_sort(cards.begin(), cards.end(), [&](PanelId a, PanelId b) {
            return store.panel(a).cardLayer < store.panel(b).cardLayer;
        });
        for (std::size_t k = 0; k < places.size(); ++k) {
            order[places[k]] = cards[k];
        }
    }
    // The card under the pointer above everything.
    if (hovered.has_value() && store.panel(*hovered).overlapped) {
        const auto at = std::find(order.begin(), order.end(), *hovered);
        if (at != order.end()) {
            order.erase(at);
            order.push_back(*hovered);
        }
    }
    return order;
}

ViewPlace placeOf(const model::Store& store, ViewId id, const Sizes& sizes) {
    const model::View& view = store.view(id);
    if (!view.widget.has_value()) {
        return { view.rect, view.rect }; // the background: the whole window
    }
    const model::Panel& panel = store.panel(store.widget(*view.widget).panel);
    const FloatRect rect(view.rect.position() - sf::Vector2f(0.f, panel.scroll), view.rect.size());
    // The panel's content area: below the header, above the margin at the bottom.
    const float top = panel.rect.top() + std::min(sizes.headerHeight, panel.rect.height());
    const float bottom = std::max(panel.rect.bottom() - sizes.padding.y, top);
    const float visibleTop = std::max(rect.top(), top);
    const float visibleBottom = std::min(rect.bottom(), bottom);
    if (rect.width() <= 0.f || visibleBottom <= visibleTop) {
        return { rect, {} };
    }
    return { rect, FloatRect(rect.left(), visibleTop, rect.width(), visibleBottom - visibleTop) };
}

void placeViews(model::Store& store, sf::Vector2f windowSize, const Sizes& sizes) {
    for (model::View& view : store.views()) {
        if (!view.widget.has_value()) {
            view.rect = FloatRect({ 0.f, 0.f }, windowSize);
            continue;
        }

        const model::WidgetSlot& slot = store.widget(*view.widget);
        const model::Panel& panel = store.panel(slot.panel);
        if (!panel.shown || panel.isClosed() || !slot.visible) {
            view.rect = {};
            continue;
        }
        // A widget's rectangle is in its panel's content, which starts below the header.
        const sf::Vector2f content = panel.rect.position() + sf::Vector2f(0.f, sizes.headerHeight);
        view.rect = FloatRect(slot.rect.position() + content, slot.rect.size());
    }
}

} // namespace atpl::layout
