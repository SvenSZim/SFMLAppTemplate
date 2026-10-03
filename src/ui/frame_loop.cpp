#include "ui/frame_loop.hpp"

namespace atpl::frame {

namespace {

/// Asks for a frame after events that need one.
std::optional<sf::Event> noted(std::optional<sf::Event> event, RedrawFlag& flag) {
    if (event.has_value() && (event->is<sf::Event::Resized>() || event->is<sf::Event::FocusGained>())) {
        flag.request();
    }
    return event;
}

} // namespace

std::optional<sf::Event> nextEvent(sf::WindowBase& window, RedrawFlag& flag, sf::Time idleWait) {
    return noted(flag.isSet() ? window.pollEvent() : window.waitEvent(idleWait), flag);
}

std::optional<sf::Event> pendingEvent(sf::WindowBase& window, RedrawFlag& flag) {
    return noted(window.pollEvent(), flag);
}

} // namespace atpl::frame
