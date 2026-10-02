#include "ui/layout/rules.hpp"

#include <variant>

namespace atpl::layout {

PanelRules rulesFor(const Layout& layout, const model::Panel& panel) {
    const PanelLayout& own = panel.layout;
    return {
        .fit = own.fit.value_or(layout.fit),
        .alignment = own.alignment.value_or(layout.alignment),
        .width = own.width.value_or(layout.width),
        .height = own.height.value_or(layout.height),
        .limit = own.limit.value_or(layout.limit),
        .rows = own.rows.value_or(layout.rows),
        .cells = own.cells.value_or(layout.cells),
        .widgetAlignment = own.widgetAlignment.value_or(layout.widgetAlignment),
        .collapseTowards = own.collapseTowards.value_or(layout.collapseTowards),
    };
}

bool isTopDown(const model::Panel& panel, const PanelRules& rules) {
    return !std::holds_alternative<Anchor>(panel.placement) && rules.fit == Fit::Fill;
}

} // namespace atpl::layout
