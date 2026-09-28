#include <catch2/catch_test_macros.hpp>

#include "ui/core/event.hpp"

using namespace ui::core;

TEST_CASE("Event creation", "[Event]") {
    auto closed = Event::closed();
    REQUIRE(closed.type == Event::Type::Closed);
    REQUIRE(closed.source == 0);

    auto changed = Event::widgetChanged(42);
    REQUIRE(changed.type == Event::Type::WidgetChanged);
    REQUIRE(changed.source == 42);
}

TEST_CASE("Event buffer accumulation", "[Event]") {
    std::vector<Event> buffer;
    buffer.push_back(Event::widgetChanged(1));
    buffer.push_back(Event::widgetChanged(2));
    buffer.push_back(Event::closed());

    REQUIRE(buffer.size() == 3);
    REQUIRE(buffer[0].source == 1);
    REQUIRE(buffer[1].source == 2);
    REQUIRE(buffer[2].type == Event::Type::Closed);

    buffer.clear();
    REQUIRE(buffer.empty());
}
