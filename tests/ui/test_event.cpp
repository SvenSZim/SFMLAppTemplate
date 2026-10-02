#include "atpl/ui/event.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <type_traits>

using namespace atpl;

TEST_CASE("an event is exactly one of its types", "[ui][event]") {
    const Event event = Scrolled{ .delta = 1.f, .horizontal = false, .pointer = {} };

    REQUIRE(event.is<Scrolled>());
    REQUIRE_FALSE(event.is<PointerMoved>());
    REQUIRE_FALSE(event.is<WindowClosed>());

    REQUIRE(event.getIf<Scrolled>() != nullptr);
    REQUIRE(event.getIf<Scrolled>()->delta == 1.f);
    REQUIRE(event.getIf<KeyPressed>() == nullptr);
}

TEST_CASE("an event keeps the data it was made from", "[ui][event]") {
    const Event pressed = ButtonPressed{ .widget = WidgetId{ 7 }, .name = "Reset", .panel = "Controls" };
    const Event changed =
        ValueChanged{ .widget = WidgetId{ 3 }, .name = "Speed", .panel = "Controls", .value = 2.5, .final = false };
    const Event resized = WindowResized{ .size = { 800u, 600u } };

    REQUIRE(pressed.getIf<ButtonPressed>()->widget == WidgetId{ 7 });
    REQUIRE(pressed.getIf<ButtonPressed>()->name == "Reset");
    REQUIRE(pressed.getIf<ButtonPressed>()->panel == "Controls");

    REQUIRE(std::get<double>(changed.getIf<ValueChanged>()->value) == 2.5);
    REQUIRE_FALSE(changed.getIf<ValueChanged>()->final);

    REQUIRE(resized.getIf<WindowResized>()->size == sf::Vector2u(800u, 600u));
}

TEST_CASE("a value change is final unless stated otherwise", "[ui][event]") {
    const ValueChanged change{ .widget = {}, .name = "Gravity", .panel = "Controls", .value = true };

    REQUIRE(change.final);
}

TEST_CASE("visit calls the visitor with the actual type", "[ui][event]") {
    const auto nameOf = [](const Event& event) {
        return event.visit([](const auto& data) -> std::string {
            using T = std::remove_cvref_t<decltype(data)>;
            if constexpr (std::is_same_v<T, WindowClosed>) {
                return "closed";
            } else if constexpr (std::is_same_v<T, TextEntered>) {
                return "text";
            } else {
                return "other";
            }
        });
    };

    REQUIRE(nameOf(WindowClosed{}) == "closed");
    REQUIRE(nameOf(TextEntered{ .character = U'a' }) == "text");
    REQUIRE(nameOf(WindowFocusChanged{ .focused = true }) == "other");
}

TEST_CASE("a pointer location knows which view it is in", "[ui][event]") {
    const PointerLocation inWorld{
        .window = { 400.f, 300.f },
        .view = ViewId{ 0 },
        .viewName = "world",
        .inView = { 400.f, 300.f },
    };
    const PointerLocation nowhere{ .window = { 10.f, 10.f }, .view = {}, .viewName = {}, .inView = { 10.f, 10.f } };

    REQUIRE(inWorld.isIn("world"));
    REQUIRE_FALSE(inWorld.isIn("minimap"));
    REQUIRE_FALSE(nowhere.isIn("world"));
    REQUIRE_FALSE(nowhere.isIn(""));
}

TEST_CASE("ids of the same kind compare by value", "[ui][event]") {
    REQUIRE(WidgetId{ 4 } == WidgetId{ 4 });
    REQUIRE(WidgetId{ 4 } != WidgetId{ 5 });
    REQUIRE(ViewId{ 1 } == ViewId{ 1 });
}
