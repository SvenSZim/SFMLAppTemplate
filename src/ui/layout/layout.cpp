#include "atpl/ui/layout.hpp"

#include <algorithm>
#include <cmath>

namespace atpl {

Sizes Layout::sizesAt(sf::Vector2f windowSize) const {
    // How far the window is from the reference size, within the limits.
    const float lowest = std::min(scaling.lowest, scaling.highest);
    const float highest = std::max(scaling.lowest, scaling.highest);
    const auto factor = [&](float size, float reference) {
        return reference > 0.f ? std::clamp(size / reference, lowest, highest) : 1.f;
    };
    const float horizontal = factor(windowSize.x, scaling.referenceWindow.x);
    const float vertical = factor(windowSize.y, scaling.referenceWindow.y);
    const float text = 1.f + (std::min(horizontal, vertical) - 1.f) * scaling.fontStrength;

    Sizes sizes;
    sizes.scale = { horizontal * metrics.scale, vertical * metrics.scale };
    sizes.text = text * metrics.scale;

    // Whole pixels: edges stay sharp, and neighbours line up.
    const auto across = [&](float value) { return std::round(value * sizes.scale.x); };
    const auto down = [&](float value) { return std::round(value * sizes.scale.y); };
    sizes.margin = std::round(metrics.margin * std::min(sizes.scale.x, sizes.scale.y)); // the same on all sides
    sizes.padding = { across(metrics.padding), down(metrics.padding) };
    sizes.gap = { across(metrics.gap), down(metrics.gap) };
    sizes.rowHeight = down(metrics.rowHeight);
    sizes.headerHeight = down(metrics.headerHeight);
    sizes.panelWidth = across(metrics.panelWidth);
    sizes.scrollbarWidth = across(metrics.scrollbarWidth);
    return sizes;
}

namespace layouts {

Layout overlay() {
    return {};
}

Layout dashboard() {
    Layout layout;
    layout.placement = GridSpan{};
    layout.collapsible = false;
    layout.fit = Fit::Fill;
    return layout;
}

Layout cards() {
    Layout layout;
    layout.placement = GridSpan{};
    layout.fit = Fit::Content;
    layout.alignment = Alignment::Center;
    layout.rows = SizeRule::Equal;
    return layout;
}

Layout compact() {
    Layout layout;
    layout.metrics.margin = 8.f;
    layout.metrics.padding = 8.f;
    layout.metrics.gap = 4.f;
    layout.metrics.rowHeight = 22.f;
    layout.metrics.panelWidth = 220.f;
    layout.metrics.headerHeight = 28.f;
    layout.scaling.lowest = 0.6f;
    layout.limit.pixels = { 360.f, 720.f };
    layout.limit.windowFraction = { 0.4f, 0.9f };
    return layout;
}

} // namespace layouts

} // namespace atpl
