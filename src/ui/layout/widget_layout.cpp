#include "ui/layout/widget_layout.hpp"

#include "atpl/ui/error.hpp"
#include "atpl/ui/widget.hpp"

#include "ui/layout/cell.hpp"
#include "ui/layout/grid_packing.hpp"
#include "ui/layout/packing.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace atpl::layout {

namespace {

[[nodiscard]] std::string inQuotes(const std::string& name) {
    return '"' + name + '"';
}

/// What the widget says about its size when it is offered this width. Sizes in whole pixels,
/// and the preferred size at least the minimum.
[[nodiscard]] SizeRequest measure(
    const model::WidgetSlot& slot,
    float width,
    const Theme& theme,
    const Sizes& sizes,
    const render::TextMeasurer* measurer
) {
    SizeRequest request = slot.widget->measure(MeasureContext(width, theme, slot.colors, sizes, measurer));
    request.min = { std::ceil(std::max(request.min.x, 0.f)), std::ceil(std::max(request.min.y, 0.f)) };
    // A widget never prefers less than its minimum.
    request.preferred = { std::max(std::ceil(request.preferred.x), request.min.x),
                          std::max(std::ceil(request.preferred.y), request.min.y) };
    return request;
}

/// The width of one of `columns` equal columns in a panel of this width.
[[nodiscard]] float columnWidth(float panelWidth, int columns, const Sizes& sizes) {
    const auto count = static_cast<float>(columns);
    return std::max((panelWidth - sizes.padding.x * 2.f - sizes.gap.x * (count - 1.f)) / count, 0.f);
}

/// The width a panel's content needs if every column is as wide as the widest need.
[[nodiscard]] float contentWidthFor(float widestNeed, int columns, const Sizes& sizes) {
    const auto count = static_cast<float>(columns);
    return sizes.padding.x * 2.f + widestNeed * count + sizes.gap.x * (count - 1.f);
}

/// A widget's need per cell when it spans `span` of them with `gap` between: its minimum divided
/// over its span.
[[nodiscard]] float perCell(float minimum, int span, float gap) {
    return std::max((minimum - gap * static_cast<float>(span - 1)) / static_cast<float>(span), 0.f);
}

// ----- Packed -----

WidgetLayout packed(
    std::span<model::WidgetSlot> slots,
    float panelWidth,
    int columns,
    const Theme& theme,
    const Sizes& sizes,
    const render::TextMeasurer* measurer,
    std::optional<float> availableHeight,
    const PanelRules& rules
) {
    const float width = columnWidth(panelWidth, columns, sizes);

    WidgetLayout result;
    std::vector<SizeRequest> requests(slots.size());
    std::vector<PackItem> items(slots.size());
    float total = sizes.padding.y * 2.f;
    float widestNeed = 0.f;
    for (std::size_t i = 0; i < slots.size(); ++i) {
        requests[i] = measure(slots[i], width, theme, sizes, measurer);
        items[i] = { static_cast<PackId>(i), requests[i].preferred.y };
        result.usesSpareHeight = result.usesSpareHeight || requests[i].isDynamic();
        total += requests[i].preferred.y + sizes.gap.y;
        widestNeed = std::max(widestNeed, requests[i].preferred.x);
    }
    result.contentWidth = contentWidthFor(widestNeed, columns, sizes);

    // In one column everything fits into `total`, so no column ever has to run over: a panel
    // that is too high for the window scrolls instead.
    const PackingResult packing = packWidgets(items, panelWidth, columns, total, sizes.padding, sizes.gap);
    result.contentHeight = packing.contentHeight;
    std::vector<FloatRect> places(slots.size());
    for (const PackedWidget& placed : packing.placements) {
        places[placed.id] = placed.rect;
    }

    // Height to spare: in every column, the dynamic widgets share what is left below the column,
    // and what follows them moves down. Widgets are in the order of their columns.
    if (availableHeight.has_value() && result.usesSpareHeight && *availableHeight > result.contentHeight) {
        std::size_t first = 0;
        while (first < slots.size()) {
            std::size_t end = first + 1;
            while (end < slots.size() && places[end].left() == places[first].left()) {
                ++end;
            }
            float dynamic = 0.f;
            for (std::size_t i = first; i < end; ++i) {
                dynamic += requests[i].isDynamic() ? 1.f : 0.f;
            }
            const float extra = *availableHeight - sizes.padding.y - places[end - 1].bottom();
            if (dynamic > 0.f && extra > 0.f) {
                const float share = std::floor(extra / dynamic);
                float shift = 0.f;
                for (std::size_t i = first; i < end; ++i) {
                    places[i].setPosition({ places[i].left(), places[i].top() + shift });
                    if (requests[i].isDynamic()) {
                        places[i].setHeight(places[i].height() + share);
                        shift += share;
                    }
                }
            }
            first = end;
        }
        result.contentHeight = *availableHeight;
    }

    for (std::size_t i = 0; i < slots.size(); ++i) {
        slots[i].rect = placeInCell(requests[i], places[i], rules.widgetAlignment);
        slots[i].fits = places[i].width() >= requests[i].min.x;
        result.widestAndHighest.x = std::max(result.widestAndHighest.x, requests[i].min.x);
        result.widestAndHighest.y = std::max(result.widestAndHighest.y, requests[i].min.y);
    }
    return result;
}

// ----- Grid -----

WidgetLayout grid(
    std::span<model::WidgetSlot> slots,
    float panelWidth,
    int columns,
    int rows,
    const Theme& theme,
    const Sizes& sizes,
    const render::TextMeasurer* measurer,
    std::optional<float> availableHeight,
    const PanelRules& rules,
    sf::Vector2f cellAtLeast
) {
    WidgetLayout result;
    result.usesSpareHeight = true;
    if (rows < 1) {
        return result;
    }
    const float width = columnWidth(panelWidth, columns, sizes);
    const auto rowCount = static_cast<float>(rows);
    const float gaps = sizes.gap.y * (rowCount - 1.f);
    const auto widthOf = [&](const GridCell& cell) {
        return width * static_cast<float>(cell.columnSpan) + sizes.gap.x * static_cast<float>(cell.columnSpan - 1);
    };

    // What a cell should be, and what it must be at least: the largest preferred and the largest
    // minimum size among the widgets, each divided over the cells the widget spans.
    std::vector<SizeRequest> requests(slots.size());
    sf::Vector2f wanted;
    float leastHeight = 0.f;
    for (std::size_t i = 0; i < slots.size(); ++i) {
        const GridCell& cell = *slots[i].cell; // found by `prepareWidgets`
        requests[i] = measure(slots[i], widthOf(cell), theme, sizes, measurer);
        wanted.x = std::max(wanted.x, perCell(requests[i].preferred.x, cell.columnSpan, sizes.gap.x));
        wanted.y = std::max(wanted.y, perCell(requests[i].preferred.y, cell.rowSpan, sizes.gap.y));
        leastHeight = std::max(leastHeight, perCell(requests[i].min.y, cell.rowSpan, sizes.gap.y));
    }
    wanted = { std::max(std::ceil(wanted.x), cellAtLeast.x), std::max(std::ceil(wanted.y), cellAtLeast.y) };
    result.cell = wanted;
    result.contentWidth = contentWidthFor(wanted.x, columns, sizes);

    // Rows are as high as the widgets prefer. A panel with height to spare shares it among its
    // rows; one that is short of height squeezes them, down to what the widgets need at least.
    // Below that the content is higher than the panel, and scrolls.
    float rowHeight = wanted.y;
    if (availableHeight.has_value()) {
        const float shared = (*availableHeight - sizes.padding.y * 2.f - gaps) / rowCount;
        rowHeight = shared >= rowHeight ? shared : std::max(std::floor(shared), std::ceil(leastHeight));
    }
    result.contentHeight = sizes.padding.y * 2.f + rowCount * rowHeight + gaps;

    // Whole pixels at every edge, so that neighbours line up.
    const auto topOf = [&](int row) {
        return std::round(sizes.padding.y + static_cast<float>(row) * (rowHeight + sizes.gap.y));
    };
    const auto bottomOf = [&](int row) {
        return std::round(sizes.padding.y + static_cast<float>(row) * (rowHeight + sizes.gap.y) + rowHeight);
    };
    for (std::size_t i = 0; i < slots.size(); ++i) {
        const GridCell& cell = *slots[i].cell;
        const float left = sizes.padding.x + static_cast<float>(cell.column) * (width + sizes.gap.x);
        const float top = topOf(cell.row);
        const FloatRect area(left, top, widthOf(cell), bottomOf(cell.row + cell.rowSpan - 1) - top);
        slots[i].rect = placeInCell(requests[i], area, rules.widgetAlignment);
        slots[i].fits = area.width() >= requests[i].min.x;
        result.widestAndHighest.x = std::max(result.widestAndHighest.x, requests[i].min.x);
        result.widestAndHighest.y = std::max(result.widestAndHighest.y, requests[i].min.y);
    }
    return result;
}

/// Finds a grid's cells when nothing says how many rows it has: as many as the widgets need.
[[nodiscard]] GridPacking packWithRowsAsNeeded(std::span<const GridItem> items, int columns) {
    int cells = 0;
    int extra = 0;
    for (const GridItem& item : items) {
        cells += std::max(item.span.columns, 1) * std::max(item.span.rows, 1);
        extra += std::max(item.span.rows, 1);
    }
    // Enough rows for all cells if nothing is wasted; at most one more per row a widget takes.
    const int fewest = std::max((cells + columns - 1) / columns, 1);
    GridPacking packing;
    for (int rows = fewest; rows <= fewest + extra; ++rows) {
        packing = packGrid(items, columns, rows);
        if (!packing.failed.has_value() || packing.problem != GridProblem::NoRoom) {
            break;
        }
    }
    return packing;
}

} // namespace

void prepareWidgets(model::Store& store, const Layout& layout) {
    const std::span<model::Panel> panels = store.panels();
    for (std::size_t p = 0; p < panels.size(); ++p) {
        model::Panel& panel = panels[p];
        if (panel.columns < 1 || panel.columns > maxPanelColumns) {
            throw SetupError(
                "panel " + inQuotes(panel.name) + " has " + std::to_string(panel.columns) +
                " columns; a panel can have 1 to " + std::to_string(maxPanelColumns)
            );
        }
        if (panel.declaredRows < 0) {
            throw SetupError("panel " + inQuotes(panel.name) + " has a negative number of rows");
        }

        const std::span<model::WidgetSlot> slots = store.widgetsOf(PanelId{ static_cast<std::uint32_t>(p) });
        std::vector<GridItem> items;
        items.reserve(slots.size());
        bool positions = false;
        bool spans = false;
        for (const model::WidgetSlot& slot : slots) {
            items.push_back({ .cell = slot.declaredCell, .span = slot.span });
            positions = positions || slot.declaredCell.has_value();
            spans = spans || slot.span.columns != 1 || slot.span.rows != 1;
        }

        // A grid, if the panel's size is given from outside, if the layout theme wants equal
        // cells, or if the panel or one of its widgets says something about cells.
        const PanelRules rules = rulesFor(layout, panel);
        panel.grid =
            isTopDown(panel, rules) || rules.rows == SizeRule::Equal || panel.declaredRows > 0 || positions || spans;
        if (!panel.grid) {
            panel.rows = 0;
            for (model::WidgetSlot& slot : slots) {
                slot.cell.reset();
            }
            continue;
        }

        const bool rowsAsNeeded = panel.declaredRows == 0 && !positions;
        const GridPacking packing = rowsAsNeeded ? packWithRowsAsNeeded(items, panel.columns)
                                                 : packGrid(items, panel.columns, panel.declaredRows);
        if (packing.failed.has_value()) {
            const model::WidgetSlot& slot = slots[*packing.failed];
            const std::string widget = "widget " + inQuotes(panel.name + "/" + slot.name);
            const std::string gridSize =
                std::to_string(panel.columns) + " columns and " + std::to_string(packing.rows) + " rows";
            switch (packing.problem) {
                case GridProblem::BadSpan:
                    throw SetupError(widget + " spans less than one cell");
                case GridProblem::Outside:
                    throw SetupError(
                        widget + " reaches outside its panel's grid of " + gridSize +
                        (slot.declaredCell.has_value() ? " (cells are counted from 0)" : "")
                    );
                case GridProblem::Overlap:
                    throw SetupError(
                        widget + " is placed on a cell another widget already has: column " +
                        std::to_string(slot.declaredCell->column) + ", row " + std::to_string(slot.declaredCell->row)
                    );
                case GridProblem::NoRoom:
                    break;
            }
            throw SetupError(
                widget + " finds no room in its panel's grid of " + gridSize + ": no " +
                std::to_string(slot.span.columns) + " by " + std::to_string(slot.span.rows) +
                " cells are free. Give the panel more rows (PanelSetup::rows)"
            );
        }

        panel.rows = packing.rows;
        for (std::size_t i = 0; i < slots.size(); ++i) {
            slots[i].cell = packing.cells[i];
        }
    }
}

WidgetLayout layoutWidgets(
    model::Store& store,
    PanelId panelId,
    float panelWidth,
    const Theme& theme,
    const Sizes& sizes,
    const render::TextMeasurer* measurer,
    std::optional<float> availableHeight,
    const PanelRules& rules,
    sf::Vector2f cellAtLeast
) {
    model::Panel& panel = store.panel(panelId);
    const std::span<model::WidgetSlot> slots = store.widgetsOf(panelId);

    WidgetLayout result;
    if (!slots.empty()) {
        result = panel.grid ? grid(
                                  slots,
                                  panelWidth,
                                  panel.columns,
                                  panel.rows,
                                  theme,
                                  sizes,
                                  measurer,
                                  availableHeight,
                                  rules,
                                  cellAtLeast
                              )
                            : packed(slots, panelWidth, panel.columns, theme, sizes, measurer, availableHeight, rules);
    }
    panel.contentHeight = result.contentHeight;
    panel.widestAndHighest = result.widestAndHighest;
    return result;
}

float contentOverflow(const model::Panel& panel, const Sizes& sizes) {
    if (!panel.shown || panel.collapsed) {
        return 0.f;
    }
    return std::max(panel.contentHeight - (panel.rect.height() - sizes.headerHeight), 0.f);
}

} // namespace atpl::layout
