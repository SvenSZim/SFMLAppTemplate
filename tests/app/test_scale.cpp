#include "atpl/app/app.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace atpl;

TEST_CASE("the automatic GUI scale follows the desktop's height in quarter steps", "[app][scale]") {
    REQUIRE(autoScale({ 1920u, 1080u }) == 1.f);
    REQUIRE(autoScale({ 2560u, 1440u }) == 1.25f);
    REQUIRE(autoScale({ 3840u, 2160u }) == 2.f);
    REQUIRE(autoScale({ 3000u, 2000u }) == 1.75f);
    REQUIRE(autoScale({ 1280u, 720u }) == 1.f);  // never below 1
    REQUIRE(autoScale({ 7680u, 4320u }) == 3.f); // nor above 3
    REQUIRE(autoScale({ 0u, 0u }) == 1.f);
}
