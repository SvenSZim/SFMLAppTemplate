#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "ui/utils/rect.hpp"

using ui::utils::FloatRect;

TEST_CASE("Rect constructors and operations", "[Rect]") {
    // OUTDATED
    FloatRect r1(10.f, 20.f, 30.f, 40.f);
    REQUIRE(r1.left() == Catch::Approx(10.f));
    REQUIRE(r1.top() == Catch::Approx(20.f));
    REQUIRE(r1.width() == Catch::Approx(30.f));
    REQUIRE(r1.height() == Catch::Approx(40.f));

    FloatRect r2(sf::Vector2<float>(5.f, 5.f), sf::Vector2<float>(2.f, 3.f));
    REQUIRE(r2.left() == Catch::Approx(5.f));
    REQUIRE(r2.top() == Catch::Approx(5.f));
    REQUIRE(r2.width() == Catch::Approx(2.f));
    REQUIRE(r2.height() == Catch::Approx(3.f));

    auto r3 = r1 + r2;
    REQUIRE(r3.left() == Catch::Approx(15.f));
    REQUIRE(r3.top() == Catch::Approx(25.f));
    REQUIRE(r3.width() == Catch::Approx(32.f));
    REQUIRE(r3.height() == Catch::Approx(43.f));

    auto r4 = r1 - r2;
    REQUIRE(r4.left() == Catch::Approx(5.f));
    REQUIRE(r4.top() == Catch::Approx(15.f));
    REQUIRE(r4.width() == Catch::Approx(28.f));
    REQUIRE(r4.height() == Catch::Approx(37.f));

    auto r5 = r1 * 2.f;
    REQUIRE(r5.left() == Catch::Approx(20.f));
    REQUIRE(r5.top() == Catch::Approx(40.f));
    REQUIRE(r5.width() == Catch::Approx(60.f));
    REQUIRE(r5.height() == Catch::Approx(80.f));

    auto inset = r1.inset(5.f);
    REQUIRE(inset.left() == Catch::Approx(15.f));
    REQUIRE(inset.top() == Catch::Approx(25.f));
    REQUIRE(inset.width() == Catch::Approx(20.f));
    REQUIRE(inset.height() == Catch::Approx(30.f));

    REQUIRE(r1 != r2);
    r2 = r1;
    REQUIRE(r2 == r1);
}
