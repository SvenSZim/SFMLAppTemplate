#include "ui/widgets/panel_frame.hpp"

#include "atpl/ui/setup.hpp"

#include <algorithm>

namespace atpl::widgets {

void paintPanelFrame(
    Painter& painter, std::string_view title, const Theme& theme, PanelColors colors, const Metrics& metrics
) {
    const sf::Vector2f size = painter.size();
    painter.box(FloatRect({ 0.f, 0.f }, size), theme.resolve(Panel::Background, State::Normal, colors));

    // A panel squeezed below its header's height shows as much of the header as there is.
    const float headerHeight = std::min(metrics.headerHeight, size.y);
    painter.text(
        FloatRect(metrics.padding, 0.f, size.x - metrics.padding * 2.f, headerHeight),
        title,
        theme.resolve(Panel::Title, State::Normal, colors)
    );
}

} // namespace atpl::widgets
