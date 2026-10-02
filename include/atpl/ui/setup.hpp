#pragma once

#include "atpl/ui/placement.hpp"
#include "atpl/ui/theme.hpp"
#include "atpl/ui/widgets.hpp"

#include <cstddef>
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

/// The parts of a panel, for theming: `theme[Panel::Background].borderThickness = 2.f;`.
/// The panel's outline belongs to its background.
struct Panel {
    static constexpr Kind kind{ "panel" };
    static constexpr Part Background{ kind, "background", Role::Surface };
    static constexpr Part Header{ kind, "header", Role::Surface };
    static constexpr Part Title{ kind, "title", Role::Title };
    static constexpr Part Scrollbar{ kind, "scrollbar", Role::Line };
};

/// One panel: a titled card with widgets.
struct PanelSetup {
    std::string name;
    std::string title; ///< Shown in the header. Empty: the name.
    Placement placement = Anchor::TopLeft;

    /// Number of equal columns in the panel, 1 to 3. See `widgets` for how they are used.
    int columns = 1;

    /// Number of equal rows, which makes the panel a grid. 0: as many rows as the widgets with a
    /// position use, or no grid if there are none. See `widgets`.
    int rows = 0;

    /// Width of a floating panel in pixels before GUI scaling. 0: the theme's default.
    /// Ignored for panels in a grid cell.
    float width = 0.f;

    /// The three colours the panel and its widgets are drawn in, as indices into the theme's
    /// `Palette::mains` (main1: background, main2: outlines) and `Palette::accents`.
    /// A single widget can deviate with `colored(...)`.
    /// An index the theme does not have throws `SetupError` when the UI is built or the theme is
    /// replaced.
    ///
    ///     {.name = "Performance", ...}                       // the theme's defaults
    ///     {.name = "Playback", .accent = 1, ...}             // another accent
    ///     {.name = "Overlay", .main1 = 2, .main2 = 0, ...}   // other main colours
    std::size_t main1 = 0;
    std::size_t main2 = 1;
    std::size_t accent = 0;

    bool collapsible = true; ///< Whether the user can fold the panel down to its header.
    bool collapsed = false;  ///< Whether it starts folded.

    /// The panel's widgets. They are placed in one of two ways.
    ///
    /// **Packed**, if the panel says nothing about rows and no widget has a position or a span:
    /// each widget gets the height it needs, and they are stacked top to bottom into `columns`
    /// columns of balanced height, in the order given. With two columns and four widgets, the
    /// first two go into the left column and the last two into the right.
    ///
    ///     .columns = 2,
    ///     .widgets = { Slider("Speed"), Slider("Size"), Switch("Gravity"), Button("Reset") },
    ///
    /// **As a grid**, if the panel has `rows`, or a widget has a position (`at`) or a span
    /// (`spanning`): the panel is split into `columns` equal columns and equal rows. A widget
    /// takes one cell, or as many as its span says, and gets their size whatever height it would
    /// need by itself.
    ///
    /// - Widgets with a position take their cells.
    /// - The others follow, the larger ones first and equal ones in the order given, each into
    ///   the first free cells that hold it, looking row by row from the left.
    /// - The grid has `rows` rows; if that is 0, as many as the widgets with a position use.
    /// - A row nobody uses stays empty: a separator.
    ///
    ///     .columns = 2,
    ///     .rows = 5,
    ///     .widgets = {
    ///         Slider("Speed"),                                           // 0, 3
    ///         Slider("Size"),                                            // 1, 3
    ///         spanning({.columns = 2, .rows = 3}, Graph("Tick time")),   // the largest: first, rows 0 to 2
    ///         Button("Reset"),                                           // 0, 4
    ///     },
    ///
    /// To have the graph below the sliders instead, give it a position:
    /// `at({.row = 1, .columnSpan = 2, .rowSpan = 3}, Graph("Tick time"))`.
    ///
    /// A row is one `Metrics::rowHeight` high. A panel in the window's grid that has more height
    /// than its rows need shares it among them.
    ///
    /// `SetupError` is thrown for a cell outside the grid, for two widgets with a position on
    /// the same cell, and for a widget that finds no room: more rows are needed then.
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

    /// Whether the profiler readout is shown from the start. See `UI::setProfilerVisible`.
    bool profiler = false;
};

} // namespace atpl
