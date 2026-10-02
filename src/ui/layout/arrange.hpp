#pragma once

#include "atpl/ui/layout.hpp"
#include "atpl/ui/setup.hpp"
#include "atpl/ui/theme.hpp"

#include "ui/model/store.hpp"
#include "ui/render/text_measurer.hpp"

#include <SFML/System/Vector2.hpp>

namespace atpl::layout {

/// What layout decides once, when the UI is built: which cells of the window's grid every panel
/// has, which panels are grids themselves, and which cells their widgets have. Throws
/// `SetupError` for a setup that can never be laid out. See `preparePanels` and `prepareWidgets`.
void prepare(model::Store& store, GridSetup grid);

/// Lays out the whole UI: every widget in its panel, every panel and every view in the window.
///
/// The steps depend on each other in this order:
///   1. A panel's width follows from the window alone.
///   2. With the width, its widgets are laid out; that gives the height of its content.
///   3. With the content heights, the panels are placed.
///   4. A panel that got more height than its content needs gives it to the rows of its grid,
///      or to the packed widgets that stretch.
///   5. Views are where their widgets ended up.
///
/// Runs when something changed that moves or resizes anything: the window, a panel collapsed,
/// expanded, shown or hidden, or the theme. Not every frame.
///
/// `sizes` are the layout's sizes for this window (`Layout::sizesAt`).
void arrange(
    model::Store& store,
    sf::Vector2f windowSize,
    GridSetup grid,
    const Theme& theme,
    const Sizes& sizes,
    const render::TextMeasurer* measurer = nullptr
);

} // namespace atpl::layout
