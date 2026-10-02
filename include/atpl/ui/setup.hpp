#pragma once

#include "atpl/ui/placement.hpp"
#include "atpl/ui/widgets.hpp"

#include <string>
#include <vector>

namespace atpl {

// Everything needed to build a UI, written as one declarative value.
//
// Names (decision D13):
// - Every panel has a name, unique in the UI.
// - Every widget has a name, unique in its panel.
// - A view's name (background or view widget) is unique among views.
// - The application refers to a widget as "Name" if that is unique in the whole UI, and as
//   "Panel/Name" otherwise.
// A duplicate name, a name containing '/', or a lookup of a name that does not exist or is
// ambiguous throws `SetupError`.

/// The window's grid. Only matters if at least one panel is placed in a `GridCell`.
struct GridSetup {
    int columns = 1;
    int rows = 1;
};

/// One panel: a titled card with widgets.
struct PanelSetup {
    std::string name;
    std::string title; ///< Shown in the header. Empty: the name.
    Placement placement = Anchor::TopLeft;

    /// Number of equal columns in the panel, at least 1. See `widgets` for how they are used.
    int columns = 1;

    /// Width of a floating panel in pixels before GUI scaling. 0: the theme's default.
    /// Ignored for panels in a grid cell.
    float width = 0.f;

    bool collapsible = true; ///< Whether the user can fold the panel down to its header.
    bool collapsed = false;  ///< Whether it starts folded.

    /// The panel's widgets. There are two ways to place them, chosen per panel:
    ///
    /// - Automatic: list the widgets. They are packed top to bottom into `columns` columns of
    ///   balanced height, in the order given.
    ///
    ///       .columns = 2,
    ///       .widgets = { Slider("Speed"), Slider("Size"), Switch("Gravity") },
    ///
    /// - By cell: give every widget its position in the panel's grid with `at(...)`.
    ///
    ///       .columns = 2,
    ///       .widgets = {
    ///           at({.column = 0, .row = 0}, Slider("Speed")),
    ///           at({.column = 1, .row = 0}, Slider("Size")),
    ///           at({.row = 1, .columnSpan = 2}, Graph("Tick time")),
    ///       },
    ///
    /// Either every widget of a panel has a position or none has. A mix, a cell outside the
    /// panel's columns, or two widgets on the same cell throws `SetupError`.
    std::vector<WidgetSetup> widgets;
};

/// The whole UI.
struct UISetup {
    /// Name of the background view: a view that fills the window behind all panels.
    /// Empty: no background view. Draw into it through `UI::view(name)`.
    std::string background;

    GridSetup grid;
    std::vector<PanelSetup> panels;
};

} // namespace atpl
