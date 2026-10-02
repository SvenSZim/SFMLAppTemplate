#pragma once

#include "atpl/ui/layout.hpp"

#include "ui/model/panel.hpp"

namespace atpl::layout {

/// How one panel is laid out: the layout theme's settings, with what the panel does differently.
struct PanelRules {
    Fit fit = Fit::Fill;
    Alignment alignment = Alignment::TopLeft;
    SizeRule width = SizeRule::Equal;
    SizeRule height = SizeRule::Own;
    PanelLimit limit;
    SizeRule rows = SizeRule::Own;
    SizeRule cells = SizeRule::Own;
    Alignment widgetAlignment = Alignment::Center;
    Alignment collapseTowards = Alignment::TopLeft;
};

[[nodiscard]] PanelRules rulesFor(const Layout& layout, const model::Panel& panel);

/// Whether the panel's size is given from outside, so that its content has to adapt to it: a
/// panel in the window's grid that fills its cells. Any other panel is as large as its content.
[[nodiscard]] bool isTopDown(const model::Panel& panel, const PanelRules& rules);

} // namespace atpl::layout
