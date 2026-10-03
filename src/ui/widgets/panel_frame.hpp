#pragma once

#include "atpl/ui/layout.hpp"
#include "atpl/ui/rect.hpp"
#include "atpl/ui/theme.hpp"
#include "atpl/ui/widget.hpp"

#include "ui/model/panel.hpp"
#include "ui/model/store.hpp"

namespace atpl::widgets {

// The panel as an element of its own: what it draws around its widgets, and how it folds.
//
// A collapsible panel folds down to its header when its header is clicked, and unfolds again
// with the next click. It does so over a short time (`Layout::foldSeconds`), during which its
// content keeps its place and is cut off at the panel's edge.

/// Paints what a panel itself consists of, around its widgets: its background with its outline,
/// a header area if the theme shows one, its title, the arrow of a collapsible panel unless the
/// theme hides it, and the line below the header if the theme shows one. The painter's size is the panel's size.
///
/// `sizes` are the layout's sizes for the window as it is.
void paintPanelFrame(Painter& painter, const model::Panel& panel, const Theme& theme, const Sizes& sizes);

/// The header of a panel of this size, in the panel's coordinates.
[[nodiscard]] FloatRect headerRect(sf::Vector2f panelSize, const Sizes& sizes);

/// Folds the panel down to its header, or unfolds it. Starts from where it is, so a panel that
/// is clicked again while it moves turns round. Returns whether anything changed.
bool setCollapsed(model::Panel& panel, bool collapsed);

/// Moves every panel that folds or unfolds on by `seconds`, for a fold that takes `foldSeconds`
/// in all (0: at once). Returns whether one moved: then layout has to run, and another frame has
/// to follow.
bool animatePanels(model::Store& store, float seconds, float foldSeconds);

} // namespace atpl::widgets
