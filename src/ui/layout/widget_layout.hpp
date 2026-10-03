#pragma once

#include "atpl/ui/id.hpp"
#include "atpl/ui/layout.hpp"
#include "atpl/ui/theme.hpp"

#include "ui/layout/rules.hpp"
#include "ui/model/store.hpp"
#include "ui/render/text_measurer.hpp"

#include <optional>

namespace atpl::layout {

// Where every widget is inside its panel (docs/LAYOUT.md 4 and 5).
//
// A widget says how small and how large it can be (`SizeRequest`); everything else is decided
// here. A panel's widgets are placed in one of two ways:
//
//   packed   every widget is as high as it needs to be, and they are stacked top to bottom into
//            the panel's columns, in the order listed, with columns of balanced height
//   grid     the content is split into equal columns and equal rows, and a widget gets the cells
//            it takes (see grid_packing.hpp for how cells are found)
//
// A panel is a grid if its size is given from outside (top-down: it fills cells of the window's
// grid), if the layout theme asks for equal cells, or if the panel itself names rows, or a
// widget a position or a span. Otherwise its widgets are packed.
//
// In a grid, a cell is as large as the largest preferred size among the panel's widgets, a
// widget that spans cells counting with its size divided over them. A panel with room to spare
// shares it among its rows; one that is short of room squeezes its rows, down to the largest
// minimum. Packed widgets are not squeezed: a panel too low for them scrolls.
//
// Rectangles are in the panel's content: (0, 0) is the corner below the header. The layout's
// padding lies between the panel's edge and the widgets, its gap between widgets.

/// The most columns a panel can have.
inline constexpr int maxPanelColumns = 3;

/// Decides for every panel whether its widgets are packed or in a grid, and finds the cells of
/// the widgets in a grid. Done when the UI is built, and again when the layout theme changes.
///
/// The grid's rows: as many as the panel names; if it names none, as many as the widgets with a
/// position use; if no widget has a position either, as many as the widgets need.
///
/// Throws `SetupError` for what can never be laid out: a column count outside 1 to 3, a negative
/// row count, a widget outside its panel's grid or with a span below 1, two widgets with a
/// position on the same cell, a widget that finds no room.
void prepareWidgets(model::Store& store, const Layout& layout = {});

/// What laying out a panel's widgets found.
struct WidgetLayout {
    /// The height of the content, padding included: what the widgets prefer when no height is
    /// given, and what they were laid out to when one is. 0 for a panel without widgets.
    float contentHeight = 0.f;

    /// The width the widgets prefer, padding included: every column as wide as the widest
    /// preferred width. 0 for a panel without widgets.
    float contentWidth = 0.f;

    /// Whether the widgets would use more height if the panel had any to spare: a grid, whose
    /// rows share it, or a packed widget that is dynamic.
    bool usesSpareHeight = false;

    /// The largest minimum width and height of a single widget.
    sf::Vector2f widestAndHighest;

    /// In a grid: the size a cell would like, from the widgets' preferred sizes.
    sf::Vector2f cell;

    /// In a grid: the height its rows were given. 0 for packed widgets.
    float rowHeight = 0.f;
};

/// Gives every widget of the panel its rectangle, for a panel `panelWidth` wide, and writes the
/// panel's content height.
///
/// Without `availableHeight`, everything is as high as it prefers to be. With it, the height
/// the panel actually has for its content: what is to spare is used (the rows of a grid share
/// it equally, packed widgets that are dynamic fill their column), and a grid that is short of
/// height squeezes its rows down to the widgets' minimum.
///
/// A widget whose room is narrower than its minimum width does not fit: it is marked so and is
/// not drawn; its place stays empty.
///
/// Inside its cell or its place in a column, a widget takes what its size request allows: a
/// dynamic widget all of it, a constant one at most its maximum, both within their limits on
/// shape. What is smaller than its room is placed in it at `rules.widgetAlignment`.
///
/// In a grid, `cellAtLeast` makes cells at least that large: for panels whose cells are the same
/// size as other panels' (`SizeRule::Equal` for `Layout::cells`).
///
/// `sizes` are the layout's sizes for the window as it is.
WidgetLayout layoutWidgets(
    model::Store& store,
    PanelId panel,
    float panelWidth,
    const Theme& theme,
    const Sizes& sizes,
    const render::TextMeasurer* measurer = nullptr,
    std::optional<float> availableHeight = std::nullopt,
    const PanelRules& rules = {},
    sf::Vector2f cellAtLeast = {}
);

/// How much of a panel's content does not fit into the height the panel has for it, or 0. This
/// is how far the content can be scrolled. While the panel folds or unfolds, its open height
/// counts: the content keeps its place meanwhile.
[[nodiscard]] float contentOverflow(const model::Panel& panel, const Sizes& sizes);

/// How far one notch of the wheel scrolls: the same in every panel. The layout's row height, or
/// the lowest row of a grid on screen if that is lower.
[[nodiscard]] float scrollStep(const model::Store& store, const Sizes& sizes);

} // namespace atpl::layout
