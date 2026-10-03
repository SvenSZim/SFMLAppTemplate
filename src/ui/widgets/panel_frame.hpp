#pragma once

#include "atpl/ui/layout.hpp"
#include "atpl/ui/rect.hpp"
#include "atpl/ui/theme.hpp"
#include "atpl/ui/widget.hpp"

#include "ui/model/panel.hpp"
#include "ui/model/store.hpp"

#include <optional>

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

/// Where a panel's widgets can be seen, in the panel's coordinates: below the header, and above
/// a margin as high as the padding at the bottom, so that content cut off there ends before the
/// panel's border. Widgets outside it are neither drawn nor found by the pointer.
[[nodiscard]] FloatRect contentArea(sf::Vector2f panelSize, const Sizes& sizes);

/// Where the scrollbar of a panel that scrolls is, in the panel's coordinates: in the padding at
/// the right of its content area.
struct ScrollbarPlace {
    FloatRect track;         ///< What the thumb moves along.
    FloatRect hitArea;       ///< Where the pointer finds it: the whole padding beside the track.
    float thumbLength = 0.f; ///< As much of the track as the content area is of the content.
    float range = 0.f;       ///< How far the content scrolls.

    /// The top of the thumb at this scroll offset.
    [[nodiscard]] float thumbTop(float scroll) const;

    /// The scroll offset at which the thumb's top is at `top`, kept within the range.
    [[nodiscard]] float scrollFor(float top) const;
};

/// The scrollbar of a panel whose content overflows by `overflow`; none if it does not.
[[nodiscard]] std::optional<ScrollbarPlace> scrollbarOf(const model::Panel& panel, const Sizes& sizes, float overflow);

/// Paints the thumb at the top of its track: the batch's scrollbar offset moves it down. Lighter
/// under the pointer and while it is dragged.
void paintScrollbar(
    Painter& painter, const model::Panel& panel, const ScrollbarPlace& place, const Theme& theme, const Sizes& sizes
);

/// Folds the panel down to its header, or unfolds it. Starts from where it is, so a panel that
/// is clicked again while it moves turns round. Returns whether anything changed.
bool setCollapsed(model::Panel& panel, bool collapsed);

/// Moves every panel that folds or unfolds on by `seconds`, for a fold that takes `foldSeconds`
/// in all (0: at once). Returns whether one moved: then layout has to run, and another frame has
/// to follow.
bool animatePanels(model::Store& store, float seconds, float foldSeconds);

} // namespace atpl::widgets
