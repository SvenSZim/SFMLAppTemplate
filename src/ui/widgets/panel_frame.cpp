#include "ui/widgets/panel_frame.hpp"

#include "atpl/ui/setup.hpp"

#include <algorithm>

namespace atpl::widgets {

FloatRect headerRect(sf::Vector2f panelSize, const Sizes& sizes) {
    // A panel squeezed below its header's height shows as much of the header as there is.
    return { 0.f, 0.f, panelSize.x, std::min(sizes.headerHeight, panelSize.y) };
}

FloatRect contentArea(sf::Vector2f panelSize, const Sizes& sizes) {
    const float top = std::min(sizes.headerHeight, panelSize.y);
    return { 0.f, top, panelSize.x, std::max(panelSize.y - top - sizes.padding.y, 0.f) };
}

float ScrollbarPlace::thumbTop(float scroll) const {
    const float travel = track.height() - thumbLength;
    return track.top() + (range > 0.f ? std::clamp(scroll / range, 0.f, 1.f) * travel : 0.f);
}

float ScrollbarPlace::scrollFor(float top) const {
    const float travel = track.height() - thumbLength;
    return travel > 0.f ? std::clamp((top - track.top()) / travel, 0.f, 1.f) * range : 0.f;
}

std::optional<ScrollbarPlace> scrollbarOf(const model::Panel& panel, const Sizes& sizes, float overflow) {
    if (overflow <= 0.f || panel.contentHeight <= 0.f) {
        return std::nullopt;
    }
    const sf::Vector2f size = panel.rect.size();
    const float inset = sizes.padding.y * 0.5f;
    const float top = sizes.headerHeight + inset;
    const float height = size.y - top - inset;
    if (height <= 0.f) {
        return std::nullopt;
    }
    const float width = std::min(sizes.scrollbarWidth, sizes.padding.x);
    const float centre = size.x - sizes.padding.x * 0.5f;

    ScrollbarPlace place;
    place.track = FloatRect(centre - width * 0.5f, top, width, height);
    place.hitArea = FloatRect(size.x - sizes.padding.x, top, sizes.padding.x, height);
    const float visible = size.y - sizes.headerHeight;
    place.thumbLength = std::min(std::max(height * visible / panel.contentHeight, sizes.rowHeight), height);
    place.range = overflow;
    return place;
}

void paintScrollbar(
    Painter& painter, const model::Panel& panel, const ScrollbarPlace& place, const Theme& theme, const Sizes& sizes
) {
    PartStyle thumb = theme.resolve(Panel::Scrollbar, State::Normal, panel.colors, sizes.text);
    if (!thumb.shown) {
        return;
    }
    if (panel.scrollbarHovered || panel.scrollbarDragged) {
        thumb.color = theme.resolve(Panel::Title, State::Normal, panel.colors, sizes.text).color;
    }
    thumb.radius = fullyRound;
    thumb.borderThickness = 0.f;
    thumb.shadow = {};
    thumb.gradient = Gradient::None;
    painter.box(FloatRect(place.track.left(), place.track.top(), place.track.width(), place.thumbLength), thumb);
}

void paintPanelFrame(Painter& painter, const model::Panel& panel, const Theme& theme, const Sizes& sizes) {
    const sf::Vector2f size = painter.size();
    const PanelColors colors = panel.colors;
    const auto styleOf = [&](const Part& part, State state = State::Normal) {
        return theme.resolve(part, state, colors, sizes.text);
    };

    painter.box(FloatRect({ 0.f, 0.f }, size), styleOf(Panel::Background));

    const FloatRect header = headerRect(size, sizes);
    // The header area, where a theme shows one, lies inside the panel, a little in from its
    // edges: behind the title, not in place of the panel's top.
    const FloatRect area(
        sizes.padding.x * 0.5f,
        sizes.padding.y * 0.4f,
        std::max(header.width() - sizes.padding.x, 0.f),
        std::max(header.height() - sizes.padding.y * 0.4f, 0.f)
    );
    painter.box(area, styleOf(Panel::Header));

    // The arrow takes a square at the right end of the header, unless the theme hides it; the
    // title the room before it.
    const PartStyle arrowStyle = styleOf(Panel::Arrow);
    const bool arrowShown = panel.collapsible && arrowStyle.shown;
    const float arrowRoom = arrowShown ? header.height() : 0.f;
    painter.text(
        FloatRect(sizes.padding.x, 0.f, std::max(size.x - sizes.padding.x * 2.f - arrowRoom, 0.f), header.height()),
        panel.title,
        styleOf(Panel::Title)
    );

    if (arrowShown) {
        // A chevron: pointing down while open, to the right while folded, turning on the way.
        const sf::Vector2f centre(size.x - sizes.padding.x - arrowRoom * 0.25f, header.height() * 0.5f);
        const float arm = std::max(header.height() * 0.14f, 2.f);
        const float open = panel.openness();
        // Open: the tip is below the arms. Folded: to their right.
        const sf::Vector2f tip(centre.x + arm * 0.5f * (1.f - open), centre.y + arm * 0.5f * open);
        const sf::Vector2f first(tip.x - arm * open - arm * (1.f - open), tip.y - arm * open + arm * (1.f - open));
        const sf::Vector2f second(tip.x + arm * open - arm * (1.f - open), tip.y - arm * open - arm * (1.f - open));
        // Under the pointer it takes the title's colour: it stands out as something to click.
        // (A line's own style does not change with hover: most lines cannot be operated.)
        PartStyle arrow = arrowStyle;
        if (panel.headerHovered) {
            arrow.color = styleOf(Panel::Title).color;
        }
        painter.line(first, tip, arrow);
        painter.line(tip, second, arrow);
    }

    if (!panel.isClosed() && size.y > header.height()) {
        const float y = header.bottom();
        painter.line({ sizes.padding.x, y }, { size.x - sizes.padding.x, y }, styleOf(Panel::Underline));
    }
}

bool setCollapsed(model::Panel& panel, bool collapsed) {
    if (panel.collapsed == collapsed) {
        return false;
    }
    panel.opening = panel.openness(); // from where it is now
    panel.collapsed = collapsed;
    panel.dirty = true;
    return true;
}

bool animatePanels(model::Store& store, float seconds, float foldSeconds) {
    bool moved = false;
    const float step = foldSeconds > 0.f ? std::max(seconds, 0.f) / foldSeconds : 1.f;
    for (model::Panel& panel : store.panels()) {
        if (!panel.opening.has_value()) {
            continue;
        }
        const float target = panel.collapsed ? 0.f : 1.f;
        const float now = *panel.opening;
        const float next = now < target ? std::min(now + step, target) : std::max(now - step, target);
        if (next == target) {
            panel.opening.reset(); // at rest
        } else {
            panel.opening = next;
        }
        panel.dirty = true; // the arrow turns
        moved = true;
    }
    return moved;
}

} // namespace atpl::widgets
