#pragma once

#include "atpl/ui/placement.hpp"
#include "atpl/ui/theme.hpp"
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

/// The parts of a panel, for theming: `theme[Panel::Outline].thickness = 2.f;`.
struct Panel {
    static constexpr Kind kind{ "panel" };
    static constexpr Part Background{ kind, "background", Role::Surface };
    static constexpr Part Outline{ kind, "outline", Role::Line, Shown::No };
    static constexpr Part Header{ kind, "header", Role::Surface };
    static constexpr Part Title{ kind, "title", Role::Title };
    static constexpr Part Scrollbar{ kind, "scrollbar", Role::Handle };
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

    /// The panel's widgets. How they are placed depends on whether any of them has a position:
    ///
    /// - No widget has a position: they are packed top to bottom into `columns` columns of
    ///   balanced height, in the order given. With two columns and four widgets, the first two
    ///   go into the left column and the last two into the right.
    ///
    ///       .columns = 2,
    ///       .widgets = { Slider("Speed"), Slider("Size"), Switch("Gravity"), Button("Reset") },
    ///
    /// - At least one widget has a position, given with `at(...)`: the panel is a grid. Positioned
    ///   widgets take their cells. The others then take, in the order given, the first free cell,
    ///   looking row by row from left to right, each without a span. Rows are as high as their
    ///   highest widget.
    ///
    ///       .columns = 2,
    ///       .widgets = {
    ///           Slider("Speed"),                                      // first free cell: 0, 0
    ///           Slider("Size"),                                       // next free cell:  1, 0
    ///           at({.row = 1, .columnSpan = 2}, Graph("Tick time")),  // whole second row
    ///           Button("Reset"),                                      // next free cell:  0, 2
    ///       },
    ///
    /// Giving one widget a position therefore changes how all others in the panel are placed.
    /// This is intended: the first way packs tightly, the second gives control.
    ///
    /// A cell outside the panel's columns, or two positioned widgets on the same cell, throws
    /// `SetupError`.
    ///
    /// Content that is higher than the panel can be is scrolled vertically inside the panel.
    std::vector<WidgetSetup> widgets;
};

/// The whole UI.
struct UISetup {
    /// How everything looks. See theme.hpp.
    Theme theme;

    /// Name of the background view: a view that fills the window behind all panels.
    /// Empty: no background view. Draw into it through `UI::view(name)`.
    std::string background;

    GridSetup grid;
    std::vector<PanelSetup> panels;
};

} // namespace atpl
