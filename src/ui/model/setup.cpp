#include "atpl/ui/widget.hpp"
#include "atpl/ui/widgets.hpp"

namespace atpl {

// The parts of `WidgetSetup` that are not templates.

std::string_view WidgetSetup::name() const {
    return m_name;
}

std::optional<GridCell> WidgetSetup::cell() const {
    return m_cell;
}

ColorOverride WidgetSetup::colors() const {
    return m_colors;
}

const std::optional<AnyBinding>& WidgetSetup::binding() const {
    return m_binding;
}

bool WidgetSetup::isView() const {
    return m_isView;
}

std::unique_ptr<Widget> WidgetSetup::create() const {
    return m_create();
}

WidgetSetup at(GridCell cell, WidgetSetup widget) {
    widget.m_cell = cell;
    return widget;
}

WidgetSetup colored(ColorOverride colors, WidgetSetup widget) {
    // Applied one after the other, the later call wins for the colours it names.
    if (colors.main1.has_value()) {
        widget.m_colors.main1 = colors.main1;
    }
    if (colors.main2.has_value()) {
        widget.m_colors.main2 = colors.main2;
    }
    if (colors.accent.has_value()) {
        widget.m_colors.accent = colors.accent;
    }
    return widget;
}

} // namespace atpl
