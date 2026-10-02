#pragma once

#include "atpl/ui/layout.hpp"
#include "atpl/ui/theme.hpp"
#include "atpl/ui/widget.hpp"

#include <string_view>

namespace atpl::widgets {

/// Paints what a panel itself consists of, around its widgets: its background with its outline,
/// and its title in the header. The painter's size is the panel's size.
///
/// `sizes` are the layout's sizes for the window as it is.
void paintPanelFrame(
    Painter& painter, std::string_view title, const Theme& theme, PanelColors colors, const Sizes& sizes
);

} // namespace atpl::widgets
