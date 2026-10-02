#pragma once

#include "atpl/ui/event.hpp"

#include <SFML/Window/Event.hpp>

#include <optional>

namespace atpl::input {

/// What the application is told about something that happened to the window itself: a request
/// to close it, a new size, focus gained or lost. Nothing for every other event.
///
/// Window events are always forwarded; no panel or widget can use them up.
[[nodiscard]] std::optional<Event> windowEvent(const sf::Event& event);

} // namespace atpl::input
