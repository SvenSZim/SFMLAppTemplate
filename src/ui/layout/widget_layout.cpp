#include "ui/layout/widget_layout.hpp"

#include "atpl/ui/error.hpp"
#include "atpl/ui/widget.hpp"

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

struct Measured {
    float height = 0.f;
    bool stretch = false;
};

[[nodiscard]] Measured measure(
    const model::WidgetSlot& slot,
    float width,
    const Theme& theme,
    const Sizes& sizes,
    const render::TextMeasurer* measurer
) {
    const SizeRequest request = slot.widget->measure(MeasureContext(width, theme, slot.colors, sizes, measurer));
    return { std::ceil(std::max(request.height, 0.f)), request.stretch };
}

/// The width of one of `columns` equal columns in a panel of this width.
[[nodiscard]] float columnWidth(float panelWidth, int columns, const Sizes& sizes) {
    const auto count = static_cast<float>(columns);
    return std::max((panelWidth - sizes.padding.x * 2.f - sizes.gap.x * (count - 1.f)) / count, 0.f);
}

// ----- Packed -----

WidgetLayout packed(
    std::span<model::WidgetSlot> slots,
    float panelWidth,
    int columns,
    const Theme& theme,
    const Sizes& sizes,
    const render::TextMeasurer* measurer,
    std::optional<float> availableHeight
) {
    const float width = columnWidth(panelWidth, columns, sizes);

    WidgetLayout result;
    std::vector<PackItem> items(slots.size());
    std::vector<bool> stretches(slots.size());
    float total = sizes.padding.y * 2.f;
    for (std::size_t i = 0; i < slots.size(); ++i) {
        const Measured measured = measure(slots[i], width, theme, sizes, measurer);
        items[i] = { static_cast<PackId>(i), measured.height };
        stretches[i] = measured.stretch;
        result.usesSpareHeight = result.usesSpareHeight || measured.stretch;
        total += measured.height + sizes.gap.y;
    }

    // In one column everything fits into `total`, so no column ever has to run over: a panel
    // that is too high for the window scrolls instead (D28).
    const PackingResult packing = packWidgets(items, panelWidth, columns, total, sizes.padding, sizes.gap);
    result.contentHeight = packing.contentHeight;
    for (const PackedWidget& placed : packing.placements) {
        slots[placed.id].rect = placed.rect;
    }

    if (!availableHeight.has_value() || !result.usesSpareHeight || *availableHeight <= result.contentHeight) {
        return result;
    }

    // Stretching: in every column, the widgets that stretch share what is left below the column,
    // and what follows them moves down. Widgets are in the order of their columns.
    std::size_t first = 0;
    while (first < slots.size()) {
        std::size_t end = first + 1;
        while (end < slots.size() && slots[end].rect.left() == slots[first].rect.left()) {
            ++end;
        }
        const auto stretching = static_cast<float>(
            std::count(stretches.begin() + static_cast<long>(first), stretches.begin() + static_cast<long>(end), true)
        );
        const float extra = *availableHeight - sizes.padding.y - slots[end - 1].rect.bottom();
        if (stretching > 0.f && extra > 0.f) {
            const float share = std::floor(extra / stretching);
            float shift = 0.f;
            for (std::size_t i = first; i < end; ++i) {
                slots[i].rect.setPosition({ slots[i].rect.left(), slots[i].rect.top() + shift });
                if (stretches[i]) {
                    slots[i].rect.setHeight(slots[i].rect.height() + share);
                    shift += share;
                }
            }
        }
        first = end;
    }
    result.contentHeight = *availableHeight;
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
    std::optional<float> availableHeight
) {
    WidgetLayout result;
    result.usesSpareHeight = true;
    if (rows < 1) {
        return result;
    }
    const float width = columnWidth(panelWidth, columns, sizes);
    const auto rowCount = static_cast<float>(rows);
    const float gaps = sizes.gap.y * (rowCount - 1.f);

    // Rows are one standard row high. A panel with height to spare shares it among them.
    float rowHeight = sizes.rowHeight;
    result.contentHeight = sizes.padding.y * 2.f + rowCount * rowHeight + gaps;
    if (availableHeight.has_value() && *availableHeight > result.contentHeight) {
        rowHeight = (*availableHeight - sizes.padding.y * 2.f - gaps) / rowCount;
        result.contentHeight = *availableHeight;
    }
    // Whole pixels at every edge, so that neighbours line up.
    const auto topOf = [&](int row) {
        return std::round(sizes.padding.y + static_cast<float>(row) * (rowHeight + sizes.gap.y));
    };
    const auto bottomOf = [&](int row) {
        return std::round(sizes.padding.y + static_cast<float>(row) * (rowHeight + sizes.gap.y) + rowHeight);
    };

    for (model::WidgetSlot& slot : slots) {
        const GridCell& cell = *slot.cell; // found when the UI was built
        const float left = sizes.padding.x + static_cast<float>(cell.column) * (width + sizes.gap.x);
        const float cellWidth =
            width * static_cast<float>(cell.columnSpan) + sizes.gap.x * static_cast<float>(cell.columnSpan - 1);
        const float top = topOf(cell.row);
        const float cellHeight = bottomOf(cell.row + cell.rowSpan - 1) - top;

        // A widget that takes several rows, or stretches, fills its cells: that is what was
        // asked for. Any other is at most as high as it wants to be, centred in its row.
        const Measured wanted = measure(slot, cellWidth, theme, sizes, measurer);
        if (cell.rowSpan > 1 || wanted.stretch || wanted.height >= cellHeight) {
            slot.rect = FloatRect(left, top, cellWidth, cellHeight);
        } else {
            slot.rect =
                FloatRect(left, top + std::round((cellHeight - wanted.height) * 0.5f), cellWidth, wanted.height);
        }
    }
    return result;
}

} // namespace

void prepareWidgets(model::Store& store) {
    const std::span<model::Panel> panels = store.panels();
    for (std::size_t p = 0; p < panels.size(); ++p) {
        model::Panel& panel = panels[p];
        if (panel.columns < 1 || panel.columns > maxPanelColumns) {
            throw SetupError(
                "panel " + inQuotes(panel.name) + " has " + std::to_string(panel.columns) +
                " columns; a panel can have 1 to " + std::to_string(maxPanelColumns)
            );
        }
        if (panel.rows < 0) {
            throw SetupError("panel " + inQuotes(panel.name) + " has a negative number of rows");
        }

        // A grid, if the panel has rows or any widget has a position or a span.
        const std::span<model::WidgetSlot> slots = store.widgetsOf(PanelId{ static_cast<std::uint32_t>(p) });
        std::vector<GridItem> items;
        items.reserve(slots.size());
        panel.grid = panel.rows > 0;
        for (const model::WidgetSlot& slot : slots) {
            items.push_back({ .cell = slot.cell, .span = slot.span });
            panel.grid = panel.grid || slot.cell.has_value() || slot.span.columns != 1 || slot.span.rows != 1;
        }
        if (!panel.grid) {
            continue;
        }

        const GridPacking packing = packGrid(items, panel.columns, panel.rows);
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
                        (slot.cell.has_value() ? " (cells are counted from 0)" : "")
                    );
                case GridProblem::Overlap:
                    throw SetupError(
                        widget + " is placed on a cell another widget already has: column " +
                        std::to_string(slot.cell->column) + ", row " + std::to_string(slot.cell->row)
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
    std::optional<float> availableHeight
) {
    model::Panel& panel = store.panel(panelId);
    const std::span<model::WidgetSlot> slots = store.widgetsOf(panelId);

    WidgetLayout result;
    if (!slots.empty()) {
        result = panel.grid
                     ? grid(slots, panelWidth, panel.columns, panel.rows, theme, sizes, measurer, availableHeight)
                     : packed(slots, panelWidth, panel.columns, theme, sizes, measurer, availableHeight);
    }
    panel.contentHeight = result.contentHeight;
    return result;
}

float contentOverflow(const model::Panel& panel, const Sizes& sizes) {
    if (!panel.shown || panel.collapsed) {
        return 0.f;
    }
    return std::max(panel.contentHeight - (panel.rect.height() - sizes.headerHeight), 0.f);
}

} // namespace atpl::layout
