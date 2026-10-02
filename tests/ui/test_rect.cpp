#include "atpl/core/interpolated.hpp"
#include "atpl/ui/rect.hpp"

#include "support/manual_clock.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using atpl::FloatRect;
using atpl::IntRect;
using Catch::Approx;

TEST_CASE("a rect can be built from numbers, vectors or an SFML rect", "[ui][rect]") {
    const FloatRect fromNumbers(10.f, 20.f, 30.f, 40.f);
    const FloatRect fromVectors(sf::Vector2f(10.f, 20.f), sf::Vector2f(30.f, 40.f));
    const FloatRect fromSfml(sf::FloatRect({ 10.f, 20.f }, { 30.f, 40.f }));

    REQUIRE(fromNumbers.left() == 10.f);
    REQUIRE(fromNumbers.top() == 20.f);
    REQUIRE(fromNumbers.width() == 30.f);
    REQUIRE(fromNumbers.height() == 40.f);
    REQUIRE(fromVectors == fromNumbers);
    REQUIRE(fromSfml == fromNumbers);
    REQUIRE(FloatRect() == FloatRect(0.f, 0.f, 0.f, 0.f));
}

TEST_CASE("edges, corners and center follow from position and size", "[ui][rect]") {
    const FloatRect rect(10.f, 20.f, 30.f, 40.f);

    REQUIRE(rect.right() == 40.f);
    REQUIRE(rect.bottom() == 60.f);
    REQUIRE(rect.position() == sf::Vector2f(10.f, 20.f));
    REQUIRE(rect.size() == sf::Vector2f(30.f, 40.f));
    REQUIRE(rect.topLeft() == sf::Vector2f(10.f, 20.f));
    REQUIRE(rect.topRight() == sf::Vector2f(40.f, 20.f));
    REQUIRE(rect.bottomLeft() == sf::Vector2f(10.f, 60.f));
    REQUIRE(rect.bottomRight() == sf::Vector2f(40.f, 60.f));
    REQUIRE(rect.center() == sf::Vector2f(25.f, 40.f));
    REQUIRE(IntRect(0, 0, 10, 6).center() == sf::Vector2i(5, 3));
}

TEST_CASE("setters change one aspect and leave the rest", "[ui][rect]") {
    FloatRect rect(10.f, 20.f, 30.f, 40.f);

    rect.setLeft(1.f);
    rect.setTop(2.f);
    REQUIRE(rect == FloatRect(1.f, 2.f, 30.f, 40.f));

    rect.setWidth(3.f);
    rect.setHeight(4.f);
    REQUIRE(rect == FloatRect(1.f, 2.f, 3.f, 4.f));

    rect.setPosition({ 5.f, 6.f });
    rect.setSize({ 7.f, 8.f });
    REQUIRE(rect == FloatRect(5.f, 6.f, 7.f, 8.f));
}

TEST_CASE("contains includes the top-left edges and excludes the bottom-right edges", "[ui][rect]") {
    const FloatRect rect(10.f, 20.f, 30.f, 40.f);

    REQUIRE(rect.contains({ 25.f, 40.f }));
    REQUIRE(rect.contains({ 10.f, 20.f }));
    REQUIRE_FALSE(rect.contains({ 40.f, 40.f }));
    REQUIRE_FALSE(rect.contains({ 25.f, 60.f }));
    REQUIRE_FALSE(rect.contains({ 9.9f, 40.f }));
    REQUIRE_FALSE(rect.contains({ 25.f, 19.9f }));

    // Two rects that share an edge never both contain a point on it.
    const FloatRect neighbour(40.f, 20.f, 30.f, 40.f);
    REQUIRE(neighbour.contains({ 40.f, 40.f }));
}

TEST_CASE("inset shrinks on every side", "[ui][rect]") {
    const FloatRect rect(10.f, 20.f, 30.f, 40.f);

    REQUIRE(rect.inset(5.f) == FloatRect(15.f, 25.f, 20.f, 30.f));
    REQUIRE(rect.inset(-5.f) == FloatRect(5.f, 15.f, 40.f, 50.f));
    REQUIRE(rect.inset(0.f) == rect);
}

TEST_CASE("arithmetic works per component", "[ui][rect]") {
    const FloatRect a(10.f, 20.f, 30.f, 40.f);
    const FloatRect b(5.f, 5.f, 2.f, 3.f);

    REQUIRE(a + b == FloatRect(15.f, 25.f, 32.f, 43.f));
    REQUIRE(a - b == FloatRect(5.f, 15.f, 28.f, 37.f));
    REQUIRE(a * 2.f == FloatRect(20.f, 40.f, 60.f, 80.f));
    REQUIRE(0.5f * a == FloatRect(5.f, 10.f, 15.f, 20.f));
    REQUIRE(0.5f * IntRect(10, 20, 30, 40) == IntRect(5, 10, 15, 20));
    REQUIRE(a != b);
}

TEST_CASE("a rect converts back to an SFML rect", "[ui][rect]") {
    const sf::FloatRect sfml = FloatRect(10.f, 20.f, 30.f, 40.f).toSFMLRect();

    REQUIRE(sfml.position == sf::Vector2f(10.f, 20.f));
    REQUIRE(sfml.size == sf::Vector2f(30.f, 40.f));
}

TEST_CASE("a rect can be animated as an interpolated value", "[ui][rect]") {
    ManualClock::reset();
    atpl::Interpolated<FloatRect, ManualClock> rect(
        FloatRect(0.f, 0.f, 100.f, 20.f), atpl::TransitionFunction::Linear, 1.0f
    );
    REQUIRE_FALSE(rect.running());

    rect = FloatRect(50.f, 10.f, 100.f, 220.f); // for example a panel that moves and expands
    REQUIRE(rect.running());

    ManualClock::advance(0.5f);
    const FloatRect halfway = rect.get();
    REQUIRE(halfway.left() == Approx(25.f));
    REQUIRE(halfway.top() == Approx(5.f));
    REQUIRE(halfway.width() == Approx(100.f));
    REQUIRE(halfway.height() == Approx(120.f));

    ManualClock::advance(0.5f);
    REQUIRE_FALSE(rect.running());
    REQUIRE(rect.get() == FloatRect(50.f, 10.f, 100.f, 220.f));
}
