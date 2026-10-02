#include "ui/frame_loop.hpp"

namespace atpl::frame {

std::optional<sf::Event> nextEvent(sf::WindowBase& window, RedrawFlag& flag, sf::Time idleWait) {
    std::optional<sf::Event> event = flag.isSet() ? window.pollEvent() : window.waitEvent(idleWait);

    if (event.has_value() && (event->is<sf::Event::Resized>() || event->is<sf::Event::FocusGained>())) {
        flag.request();
    }
    return event;
}

} // namespace atpl::frame
