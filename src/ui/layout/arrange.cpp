#include "ui/layout/arrange.hpp"

#include "ui/layout/panel_placement.hpp"
#include "ui/layout/rules.hpp"
#include "ui/layout/widget_layout.hpp"

#include <vector>

namespace atpl::layout {

void prepare(model::Store& store, GridSetup grid, const Layout& layout) {
    preparePanels(store, grid);
    prepareWidgets(
        store, layout
    ); // after the panels: whether a panel is in the grid decides how its content is laid out
}

void arrange(
    model::Store& store,
    sf::Vector2f windowSize,
    GridSetup grid,
    const Theme& theme,
    const Sizes& sizes,
    const render::TextMeasurer* measurer,
    const Layout& layout
) {
    const std::span<model::Panel> panels = store.panels();
    const auto idOf = [](std::size_t index) { return PanelId{ static_cast<std::uint32_t>(index) }; };

    // 1 and 2: widths, then widgets at the sizes they prefer, which gives the content heights.
    std::vector<float> widths(panels.size());
    std::vector<PanelRules> rules(panels.size());
    for (std::size_t i = 0; i < panels.size(); ++i) {
        rules[i] = rulesFor(layout, panels[i]);
        widths[i] = panelWidth(panels[i], windowSize, grid, sizes);
        layoutWidgets(store, idOf(i), widths[i], theme, sizes, measurer, std::nullopt, rules[i]);
    }

    // 3: the panels.
    placePanels(store, windowSize, grid, sizes);

    // 4: with the height each panel actually has, its content is laid out for good: spare height
    // goes to the rows of a grid or to the packed widgets that are dynamic, and a grid that is
    // short of height squeezes its rows.
    for (std::size_t i = 0; i < panels.size(); ++i) {
        const model::Panel& panel = panels[i];
        const bool open = panel.shown && !panel.collapsed;
        if (open) {
            const float available = panel.rect.height() - sizes.headerHeight;
            layoutWidgets(store, idOf(i), widths[i], theme, sizes, measurer, available, rules[i]);
        }
        for (model::WidgetSlot& slot : store.widgetsOf(idOf(i))) {
            slot.visible = open;
        }
    }

    // 5: the views.
    placeViews(store, windowSize, sizes);
}

} // namespace atpl::layout
