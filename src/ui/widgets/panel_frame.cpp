#include "ui/widgets/panel_frame.hpp"

#include "atpl/ui/setup.hpp"

#include <algorithm>

namespace atpl::widgets {

FloatRect headerRect(sf::Vector2f panelSize, const Sizes& sizes) {
    // A panel squeezed below its header's height shows as much of the header as there is.
    return { 0.f, 0.f, panelSize.x, std::min(sizes.headerHeight, panelSize.y) };
}

void paintPanelFrame(Painter& painter, const model::Panel& panel, const Theme& theme, const Sizes& sizes) {
    const sf::Vector2f size = painter.size();
    const PanelColors colors = panel.colors;
    const auto styleOf = [&](const Part& part, State state = State::Normal) {
        return theme.resolve(part, state, colors, sizes.text);
    };

    painter.box(FloatRect({ 0.f, 0.f }, size), styleOf(Panel::Background));

    const FloatRect header = headerRect(size, sizes);
    painter.box(header, styleOf(Panel::Header));

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
