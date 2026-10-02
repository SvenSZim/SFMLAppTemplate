#include "ui/input/window_events.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace atpl;

TEST_CASE("what happens to the window is forwarded to the application", "[ui][input][events]") {
    const auto closed = input::windowEvent(sf::Event(sf::Event::Closed{}));
    REQUIRE(closed.has_value());
    REQUIRE(closed->is<WindowClosed>());

    const auto resized = input::windowEvent(sf::Event(sf::Event::Resized{ { 1024u, 768u } }));
    REQUIRE(resized.has_value());
    REQUIRE(resized->getIf<WindowResized>() != nullptr);
    REQUIRE(resized->getIf<WindowResized>()->size == sf::Vector2u(1024u, 768u));

    const auto gained = input::windowEvent(sf::Event(sf::Event::FocusGained{}));
    REQUIRE(gained.has_value());
    REQUIRE(gained->getIf<WindowFocusChanged>()->focused);

    const auto lost = input::windowEvent(sf::Event(sf::Event::FocusLost{}));
    REQUIRE(lost.has_value());
    REQUIRE_FALSE(lost->getIf<WindowFocusChanged>()->focused);
}

TEST_CASE("other events are not window events", "[ui][input][events]") {
    REQUIRE_FALSE(input::windowEvent(sf::Event(sf::Event::MouseMoved{ { 10, 20 } })).has_value());
    REQUIRE_FALSE(input::windowEvent(sf::Event(sf::Event::KeyPressed{})).has_value());
    REQUIRE_FALSE(input::windowEvent(sf::Event(sf::Event::MouseEntered{})).has_value());
}
