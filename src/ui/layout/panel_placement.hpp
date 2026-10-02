#pragma once

#include "atpl/ui/layout.hpp"
#include "atpl/ui/setup.hpp"

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

/// Finds the cells of every panel that is in the window's grid, once, when the UI is built.
/// Panels with a `GridCell` take it; panels with a `GridSpan` get the first free cells that hold
/// them, the larger ones first (see grid_packing.hpp). Afterwards every grid panel's placement
/// is a `GridCell`.
///
/// Throws `SetupError` for a placement that can never work: a grid without columns or rows, a
/// panel outside the grid or with a span below 1, a panel that finds no room, a negative width.
void preparePanels(model::Store& store, GridSetup grid);

/// How wide a panel is in a window of this size. Known before anything else about its place, so
/// that its widgets can be laid out first: their height decides the panel's.
[[nodiscard]] float panelWidth(const model::Panel& panel, sf::Vector2f windowSize, GridSetup grid, const Sizes& sizes);

/// The height a panel wants: its header if it is collapsed; otherwise what layout worked out
/// (`Panel::wantedHeight`), or its header and its content.
[[nodiscard]] float wantedHeight(const model::Panel& panel, const Sizes& sizes);

/// Gives every panel its rectangle and says whether it is shown.
///
/// Floating panels that share an anchor form a stack, in the order they are listed: downwards
/// from a top anchor, upwards from a bottom anchor, centred for `Left` and `Right`.
/// - A panel is as wide as layout worked out (`Panel::wantedWidth`), or else as it asks to be
///   or as the layout theme says. In a window too narrow for that, the margin at the sides
///   shrinks first, then the panel.
/// - Panels that are too small for their widgets (`Panel::tooSmall`) are left out.
/// - A stack that is too high for the window is fitted to it: collapsed panels keep the height
///   of their header, and expanded panels share the rest, those that need least first. If not
///   even the headers fit, the last panels of the stack are not shown.
///
/// Grid panels fill the cells they span, or, with `Fit::Content`, take as much of them as they
/// want (`Panel::wantedWidth` and `wantedHeight`) and sit in them at the rules' alignment. A
/// collapsed grid panel is its header, placed in its rectangle towards `collapseTowards`. A
/// grid panel lower than a header is not shown. Cells are equal, with the layout's margin
/// around the grid and between cells.
///
/// A panel the application made invisible is not shown and leaves no gap.
///
/// A panel whose size changed, or that appears, is marked dirty: it has to be painted again.
/// One that only moved is not.
///
/// `sizes` are the layout's sizes for this window.
void placePanels(
    model::Store& store, sf::Vector2f windowSize, GridSetup grid, const Sizes& sizes, const Layout& layout = {}
);

/// Gives every view its rectangle in the window: the whole window for the background view, and
/// for a view widget the place of its widget. A view whose widget is not on screen gets an
/// empty rectangle. Run after the panels and their widgets are placed.
void placeViews(model::Store& store, sf::Vector2f windowSize, const Sizes& sizes);

} // namespace atpl::layout
