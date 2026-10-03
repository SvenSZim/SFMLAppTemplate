#include "ui/layout/overlay_placement.hpp"

#include "atpl/ui/widget.hpp"

#include <algorithm>

namespace atpl::layout {

FloatRect placeOverlay(
    const FloatRect& anchor,
    sf::Vector2f window,
    float margin,
    float gap,
    const std::function<sf::Vector2f(float)>& sizeFor
) {
    const float room = std::max(window.y - margin * 2.f, 0.f);
    const float below = std::max(window.y - margin - anchor.bottom() - gap, 0.f);
    const float above = std::max(anchor.top() - gap - margin, 0.f);

    sf::Vector2f size = sizeFor(room);
    float top = anchor.bottom() + gap;
    if (size.y > below) {
        if (size.y <= above) {
            top = anchor.top() - gap - size.y;
        } else {
            // It fits neither way: the side with more room, and only as much as there is.
            const bool up = above > below;
            size = sizeFor(up ? above : below);
            size.y = std::min(size.y, room);
            top = up ? anchor.top() - gap - size.y : anchor.bottom() + gap;
            top = std::clamp(top, margin, std::max(window.y - margin - size.y, margin));
        }
    }

    const float width = std::min(size.x > 0.f ? size.x : anchor.width(), std::max(window.x - margin * 2.f, 0.f));
    const float left = std::clamp(anchor.left(), margin, std::max(window.x - margin - width, margin));
    return { left, top, width, size.y };
}

void placeOverlays(
    model::Store& store,
    std::optional<WidgetId> open,
    sf::Vector2f window,
    const Theme& theme,
    const Sizes& sizes,
    const render::TextMeasurer* measurer
) {
    for (model::WidgetSlot& slot : store.widgets()) {
        slot.overlayRect.reset();
    }
    if (!open.has_value()) {
        return;
    }
    model::WidgetSlot& slot = store.widget(*open);
    const model::Panel& panel = store.panel(slot.panel);
    const MeasureContext context(slot.rect.width(), theme, slot.colors, sizes, measurer);
    const FloatRect anchor = slot.widget->overlayAnchor(context, slot.rect.size());
    const sf::Vector2f origin =
        panel.rect.position() + sf::Vector2f(0.f, sizes.headerHeight - panel.scroll) + slot.rect.position();
    slot.overlayAnchor = FloatRect(origin + anchor.position(), anchor.size());
    // Attached: right at the anchor, no gap between them.
    slot.overlayRect = placeOverlay(slot.overlayAnchor, window, sizes.margin, 0.f, [&](float maxHeight) {
        return slot.widget->overlaySize(context, maxHeight);
    });
}

} // namespace atpl::layout
