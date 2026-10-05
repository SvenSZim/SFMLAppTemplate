#pragma once

#include "atpl/ui/layout.hpp"
#include "atpl/ui/rect.hpp"
#include "atpl/ui/widget.hpp"

#include <SFML/System/Vector2.hpp>

#include <algorithm>
#include <cmath>

namespace atpl::widgets {

// Sizes the built-in widgets share, worked out from the layout's sizes so that they follow the
// window like everything else.

/// The diameter of a knob, and the height of a switch: a little less than a row.
[[nodiscard]] inline float knobSize(const Sizes& sizes) {
    return std::max(sizes.rowHeight * 0.8f, 8.f);
}

/// A switch's track is this many times as wide as it is high.
inline constexpr float switchAspect = 1.8f;

/// A slider's track is this share of the knob's height.
inline constexpr float trackShare = 0.45f;

/// The most height a one-line widget makes use of, as a share of the row height.
inline constexpr float tallest = 1.25f;

/// A width larger than any window: "as wide as it gets".
inline constexpr float anyWidth = 100000.f;

/// Space between a label and the field below it.
inline constexpr float labelGap = 2.f;

/// The field of a widget whose label is above it (text input, dropdown): the rest of its height.
[[nodiscard]] inline FloatRect fieldBelow(sf::Vector2f size, float labelHeight) {
    const float top = std::min(labelHeight + labelGap, size.y);
    return { 0.f, top, size.x, size.y - top };
}

/// The field of a widget whose label is beside it: the right two thirds, the whole height.
[[nodiscard]] inline FloatRect fieldBeside(sf::Vector2f size, float gap) {
    const float left = std::min(std::round(size.x / 3.f + gap * 0.5f), size.x);
    return { left, 0.f, size.x - left, size.y };
}

/// The label of a widget whose label is beside its field: the left third, less half a gap.
[[nodiscard]] inline FloatRect labelBeside(sf::Vector2f size, float gap) {
    return { 0.f, 0.f, std::max(std::round(size.x / 3.f - gap * 0.5f), 0.f), size.y };
}

/// What a widget with its label beside a field of one row asks for: the field a third wider than
/// the label's third, as high as a row (a button), with the label in the remaining third.
[[nodiscard]] inline SizeRequest labelBesideField(const Sizes& sizes, sf::Vector2f label, sf::Vector2f widest) {
    const float third = std::max(label.x, (widest.x + sizes.padding.x * 2.f) * 0.5f);
    const float least = std::min(std::max(label.y, widest.y) + 6.f, sizes.rowHeight);
    return {
        .min = { std::max(label.x, widest.x * 0.25f) * 3.f + sizes.gap.x, least },
        .preferred = { third * 3.f + sizes.gap.x, sizes.rowHeight },
        .max = sf::Vector2f(anyWidth, sizes.rowHeight * tallest),
    };
}

/// What a widget with a label above a field of one row asks for: `widest` is the size of the
/// widest text it shows in the field. Squeezed, the field keeps some room around its text.
[[nodiscard]] inline SizeRequest labelledField(const Sizes& sizes, sf::Vector2f label, sf::Vector2f widest) {
    const float wanted = label.y + labelGap + sizes.rowHeight;
    const float least = std::min(widest.y + 6.f, sizes.rowHeight);
    return {
        .min = { std::max(label.x, widest.x * 0.5f) + sizes.padding.x, label.y + labelGap + least },
        .preferred = { std::max(label.x, widest.x) + sizes.padding.x * 2.f, wanted },
        .max = sf::Vector2f(anyWidth, label.y + labelGap + sizes.rowHeight * tallest),
    };
}

} // namespace atpl::widgets
