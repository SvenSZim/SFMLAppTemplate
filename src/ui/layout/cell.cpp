#include "ui/layout/cell.hpp"

#include <algorithm>
#include <cmath>

namespace atpl::layout {

namespace {

/// 0, a half or 1: how far towards the right or the bottom an alignment puts things.
[[nodiscard]] sf::Vector2f share(Alignment alignment) {
    switch (alignment) {
        case Alignment::TopLeft:
            return { 0.f, 0.f };
        case Alignment::Top:
            return { 0.5f, 0.f };
        case Alignment::TopRight:
            return { 1.f, 0.f };
        case Alignment::Left:
            return { 0.f, 0.5f };
        case Alignment::Center:
            return { 0.5f, 0.5f };
        case Alignment::Right:
            return { 1.f, 0.5f };
        case Alignment::BottomLeft:
            return { 0.f, 1.f };
        case Alignment::Bottom:
            return { 0.5f, 1.f };
        case Alignment::BottomRight:
            break;
    }
    return { 1.f, 1.f };
}

} // namespace

FloatRect aligned(sf::Vector2f size, const FloatRect& area, Alignment alignment) {
    const float width = std::min(size.x, area.width());
    const float height = std::min(size.y, area.height());
    const sf::Vector2f towards = share(alignment);
    return {
        std::round(area.left() + (area.width() - width) * towards.x),
        std::round(area.top() + (area.height() - height) * towards.y),
        width,
        height,
    };
}

sf::Vector2f sizeIn(const SizeRequest& request, sf::Vector2f room) {
    sf::Vector2f size = room;
    if (request.max.has_value()) {
        // A maximum below the preferred or the minimum size makes no sense: the larger counts.
        size.x = std::min(size.x, std::max({ request.max->x, request.preferred.x, request.min.x }));
        size.y = std::min(size.y, std::max({ request.max->y, request.preferred.y, request.min.y }));
    }
    // The shape: cut whichever side is too long for the other.
    if (request.widestRatio > 0.f && size.x > size.y * request.widestRatio) {
        size.x = size.y * request.widestRatio;
    }
    if (request.tallestRatio > 0.f && size.y > size.x * request.tallestRatio) {
        size.y = size.x * request.tallestRatio;
    }
    return { std::max(std::floor(size.x), 0.f), std::max(std::floor(size.y), 0.f) };
}

FloatRect placeInCell(const SizeRequest& request, const FloatRect& cell, Alignment alignment) {
    return aligned(sizeIn(request, cell.size()), cell, alignment);
}

} // namespace atpl::layout
