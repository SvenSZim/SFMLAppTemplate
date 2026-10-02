#pragma once

#include "atpl/ui/layout.hpp"
#include "atpl/ui/rect.hpp"
#include "atpl/ui/widget.hpp"

#include <SFML/System/Vector2.hpp>

namespace atpl::layout {

/// A rectangle of this size inside `area`, at one of the nine positions. On whole pixels.
/// A size larger than the area is cut to it.
[[nodiscard]] FloatRect aligned(sf::Vector2f size, const FloatRect& area, Alignment alignment);

/// The size a widget takes in room of this size: all of it for a dynamic widget, at most its
/// maximum for a constant one, and in both cases within the widget's limits on its shape.
/// Never more than the room. (Less than the widget's minimum is possible: what happens then is
/// decided by the overflow rules, not here.)
[[nodiscard]] sf::Vector2f sizeIn(const SizeRequest& request, sf::Vector2f room);

/// The widget's rectangle in its cell: the size it takes, placed at the alignment.
[[nodiscard]] FloatRect placeInCell(const SizeRequest& request, const FloatRect& cell, Alignment alignment);

} // namespace atpl::layout
