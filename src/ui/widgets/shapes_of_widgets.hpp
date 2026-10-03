#pragma once

#include "atpl/ui/layout.hpp"
#include "atpl/ui/rect.hpp"

#include <algorithm>

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

} // namespace atpl::widgets
