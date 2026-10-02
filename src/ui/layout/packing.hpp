#pragma once

#include "atpl/ui/rect.hpp"

#include <SFML/System/Vector2.hpp>

#include <algorithm>
#include <cstdint>
#include <span>
#include <vector>

namespace atpl::layout {

/// Id the caller attaches to an item. It is handed back unchanged with the item's rectangle;
/// packing does not care what it identifies.
using PackId = std::uint32_t;

/// One thing to place: its id and the height it needs.
struct PackItem {
    PackId id;
    float resolvedHeight;
};

/// Where an item ended up, relative to the panel's top-left corner.
struct PackedWidget {
    PackId id;
    FloatRect rect;
};

struct PackingResult {
    /// One entry per item, in the order the items were given.
    std::vector<PackedWidget> placements;
    /// Height the panel needs to show everything, including padding.
    float contentHeight = 0.f;
    /// True if the items do not fit into the maximum panel height.
    bool overflows = false;
};

/// A size that follows a reference dimension (`refDim * dynamic`) within fixed limits.
[[nodiscard]] inline float resolveSize(float min, float max, float dynamic, float refDim) {
    return std::clamp(refDim * dynamic, min, max);
}

/// Places items top to bottom into `columnCount` columns of equal width. `padding` is the space
/// around everything and `spacing` the space between items, each horizontally and vertically.
///
/// Columns are filled in order. The column height is raised step by step, starting from an even
/// split, until everything fits, so the result is as low and as balanced as the item order allows.
/// If the items do not fit within `maxPanelHeight`, the last column runs over and `overflows` is set.
[[nodiscard]] PackingResult packWidgets(
    std::span<const PackItem> items,
    float panelWidth,
    int columnCount,
    float maxPanelHeight,
    sf::Vector2f padding = { 4.f, 4.f },
    sf::Vector2f spacing = { 4.f, 4.f }
);

} // namespace atpl::layout
