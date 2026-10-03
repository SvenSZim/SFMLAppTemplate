#pragma once

#include "atpl/ui/id.hpp"
#include "atpl/ui/layout.hpp"
#include "atpl/ui/rect.hpp"
#include "atpl/ui/theme.hpp"

#include "ui/model/store.hpp"

#include <SFML/System/Vector2.hpp>

#include <functional>
#include <optional>

namespace atpl::render {
class TextMeasurer;
} // namespace atpl::render

namespace atpl::layout {

/// Where an overlay goes for a widget at `anchor`, both in the window (D28):
/// - below the widget, if it fits there;
/// - else above it, if it fits there;
/// - else on the side with more room, asked again for that much at most (a list then shows
///   fewer entries); what is still too high is kept inside the window.
///
/// `sizeFor(maxHeight)` is the size the overlay would like with that much room; a width of zero
/// means as wide as the widget. Nothing comes closer than `margin` to the window's edges, and
/// the overlay keeps `gap` from the widget.
[[nodiscard]] FloatRect placeOverlay(
    const FloatRect& anchor,
    sf::Vector2f window,
    float margin,
    float gap,
    const std::function<sf::Vector2f(float)>& sizeFor
);

/// Places the open overlay, if there is one, and forgets the places of all others.
void placeOverlays(
    model::Store& store,
    std::optional<WidgetId> open,
    sf::Vector2f window,
    const Theme& theme,
    const Sizes& sizes,
    const render::TextMeasurer* measurer
);

} // namespace atpl::layout
