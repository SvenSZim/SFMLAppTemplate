#pragma once

#include "atpl/ui/widgets.hpp"

#include <string>
#include <variant>
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

/// Where a floating panel sits: at an edge or corner of the window, on top of the background
/// and of the grid.
///
/// Panels that share an anchor are stacked in the order they are listed: downwards from a top
/// anchor, upwards from a bottom anchor. `Left` and `Right` centre their stack vertically.
enum class Anchor {
    TopLeft,
    Top,
    TopRight,
    Left,
    Right,
    BottomLeft,
    Bottom,
    BottomRight,
};

/// Where a panel sits in the window's grid. The grid divides the window into `GridSetup::columns`
/// by `GridSetup::rows` equal cells; a panel fills the cells it spans.
struct GridCell {
    int column = 0;
    int row = 0;
    int columnSpan = 1;
    int rowSpan = 1;
};

/// A panel either floats at an anchor or fills grid cells. Both kinds can be used in one UI.
using Placement = std::variant<Anchor, GridCell>;

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

    /// Number of columns the widgets are packed into, 1 to 3.
    int columns = 1;

    /// Width of a floating panel in pixels before GUI scaling. 0: the theme's default.
    /// Ignored for panels in a grid cell.
    float width = 0.f;

    bool collapsible = true; ///< Whether the user can fold the panel down to its header.
    bool collapsed = false;  ///< Whether it starts folded.

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
