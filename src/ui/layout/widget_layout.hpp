#pragma once

#include "atpl/ui/id.hpp"
#include "atpl/ui/layout.hpp"
#include "atpl/ui/theme.hpp"

#include "ui/model/store.hpp"
#include "ui/render/text_measurer.hpp"

#include <optional>

namespace atpl::layout {

// Where every widget is inside its panel (ARCHITECTURE.md 4.6).
//
// A panel's widgets are placed in one of two ways (D44):
//
//   packed   the panel says nothing about rows, and no widget has a position or a span: every
//            widget gets the height it asks for at the width it is offered, and they are stacked
//            top to bottom into the panel's columns, in the order listed, with columns of
//            balanced height
//   grid     otherwise: the panel is split into equal columns and equal rows, and a widget gets
//            the cells it takes, whatever height it would ask for (see grid_packing.hpp for how
//            cells are found)
//
// Rectangles are in the panel's content: (0, 0) is the corner below the header. The theme's
// padding lies between the panel's edge and the widgets, its gap between widgets.

/// The most columns a panel can have.
inline constexpr int maxPanelColumns = 3;

/// Decides for every panel whether its widgets are packed or in a grid, and finds the cells of
/// the widgets in a grid, once, when the UI is built.
///
/// Throws `SetupError` for what can never be laid out: a column count outside 1 to 3, a negative
/// row count, a widget outside its panel's grid or with a span below 1, two widgets with a
/// position on the same cell, a widget that finds no room.
void prepareWidgets(model::Store& store);

/// What laying out a panel's widgets found.
struct WidgetLayout {
    /// The height the widgets need, padding included. 0 for a panel without widgets.
    float contentHeight = 0.f;

    /// Whether the widgets would use more height if the panel had any to spare: a packed widget
    /// that stretches (`SizeRequest::stretch`), or a grid, whose rows share it.
    bool usesSpareHeight = false;
};

/// Gives every widget of the panel its rectangle, for a panel `panelWidth` wide, and writes the
/// panel's content height.
///
/// Without `availableHeight`, everything is as high as it needs to be: a packed widget as it
/// asks, a grid row one `Metrics::rowHeight`. With it, the height the panel actually has for
/// its content, what is to spare is used: the rows of a grid share it equally, and packed
/// widgets that stretch fill their column.
///
/// In a grid, a widget that takes several rows or stretches fills its cells. Any other keeps the
/// height it asks for, if that is less than its row's, and is centred in the row.
///
/// `sizes` are the layout's sizes for the window as it is.
WidgetLayout layoutWidgets(
    model::Store& store,
    PanelId panel,
    float panelWidth,
    const Theme& theme,
    const Sizes& sizes,
    const render::TextMeasurer* measurer = nullptr,
    std::optional<float> availableHeight = std::nullopt
);

/// How much of a panel's content does not fit into the height the panel has for it, or 0. This
/// is how far the content can be scrolled.
[[nodiscard]] float contentOverflow(const model::Panel& panel, const Sizes& sizes);

} // namespace atpl::layout
