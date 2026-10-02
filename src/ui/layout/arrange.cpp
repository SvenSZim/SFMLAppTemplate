#include "ui/layout/arrange.hpp"

#include "ui/layout/panel_placement.hpp"
#include "ui/layout/widget_layout.hpp"

#include <vector>

namespace atpl::layout {

void prepare(model::Store& store, GridSetup grid) {
    preparePanels(store, grid);
    prepareWidgets(store);
}

void arrange(
    model::Store& store,
    sf::Vector2f windowSize,
    GridSetup grid,
    const Theme& theme,
    const Metrics& metrics,
    const render::TextMeasurer* measurer
) {
    const float scale = theme.metrics.scale;
    const std::span<model::Panel> panels = store.panels();
    const auto idOf = [](std::size_t index) { return PanelId{ static_cast<std::uint32_t>(index) }; };

    // 1 and 2: widths, then widgets, which gives the content heights.
    std::vector<float> widths(panels.size());
    std::vector<bool> usesSpare(panels.size());
    for (std::size_t i = 0; i < panels.size(); ++i) {
        widths[i] = panelWidth(panels[i], windowSize, grid, metrics, scale);
        usesSpare[i] = layoutWidgets(store, idOf(i), widths[i], theme, metrics, measurer).usesSpareHeight;
    }

    // 3: the panels.
    placePanels(store, windowSize, grid, metrics, scale);

    // 4: height a panel has beyond what its content needs goes to the rows of its grid, or to
    // the packed widgets that stretch.
    for (std::size_t i = 0; i < panels.size(); ++i) {
        const model::Panel& panel = panels[i];
        const bool open = panel.shown && !panel.collapsed;
        if (open && usesSpare[i]) {
            const float available = panel.rect.height() - metrics.headerHeight;
            if (available > panel.contentHeight) {
                layoutWidgets(store, idOf(i), widths[i], theme, metrics, measurer, available);
            }
        }
        for (model::WidgetSlot& slot : store.widgetsOf(idOf(i))) {
            slot.visible = open;
        }
    }

    // 5: the views.
    placeViews(store, windowSize, metrics);
}

} // namespace atpl::layout
