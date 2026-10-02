#include "ui/input/window_events.hpp"

namespace atpl::input {

std::optional<Event> windowEvent(const sf::Event& event) {
    if (event.is<sf::Event::Closed>()) {
        return Event(WindowClosed{});
    }
    if (const auto* resized = event.getIf<sf::Event::Resized>()) {
        return Event(WindowResized{ resized->size });
    }
    if (event.is<sf::Event::FocusGained>()) {
        return Event(WindowFocusChanged{ true });
    }
    if (event.is<sf::Event::FocusLost>()) {
        return Event(WindowFocusChanged{ false });
    }
    return std::nullopt;
}

} // namespace atpl::input
