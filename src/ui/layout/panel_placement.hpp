#pragma once

#include "atpl/ui/setup.hpp"
#include "atpl/ui/theme.hpp"

#include "ui/model/store.hpp"

#include <SFML/System/Vector2.hpp>

namespace atpl::layout {

// Where every panel and every view is in the window.
//
// A panel either floats at an anchor or fills cells of the window's grid; both kinds can be used
// in one UI (ARCHITECTURE.md 4.6a). What happens when things do not fit is decided here too
// (4.6b, D28).
//
// These functions run when something changed that moves or resizes panels: the window was
// resized, a panel was collapsed, expanded, shown or hidden, its content got a different height,
// or the theme was replaced. They do not run every frame.

/// Throws `SetupError` for a placement that can never work: a grid without columns or rows, a
/// panel in a cell outside the grid or with a span below 1, a panel with a negative width.
void requirePlaceable(const model::Store& store, GridSetup grid);

/// The height a panel wants: its header, plus its content unless it is collapsed.
[[nodiscard]] float wantedHeight(const model::Panel& panel, const Metrics& metrics);

/// Gives every panel its rectangle and says whether it is shown.
///
/// Floating panels that share an anchor form a stack, in the order they are listed: downwards
/// from a top anchor, upwards from a bottom anchor, centred for `Left` and `Right`.
/// - A panel is as wide as it asks to be, or as the theme says. In a window too narrow for that,
///   the margin at the sides shrinks first, then the panel.
/// - A stack that is too high for the window is fitted to it: collapsed panels keep the height
///   of their header, and expanded panels share the rest, those that need least first. If not
///   even the headers fit, the last panels of the stack are not shown.
///
/// Grid panels fill the cells they span. Cells are equal, with the theme's margin around the
/// grid and between cells. A collapsed grid panel is its header at the top of its cells.
///
/// A panel the application made invisible is not shown and leaves no gap.
///
/// A panel whose size changed, or that appears, is marked dirty: it has to be painted again.
/// One that only moved is not.
///
/// `metrics` are the theme's sizes with the GUI scale applied; `scale` is that scale, for the
/// widths panels ask for themselves.
void placePanels(
    model::Store& store, sf::Vector2f windowSize, GridSetup grid, const Metrics& metrics, float scale = 1.f
);

/// Gives every view its rectangle in the window: the whole window for the background view, and
/// for a view widget the place of its widget. A view whose widget is not on screen gets an
/// empty rectangle. Run after the panels and their widgets are placed.
void placeViews(model::Store& store, sf::Vector2f windowSize, const Metrics& metrics);

} // namespace atpl::layout
