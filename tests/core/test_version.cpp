#include "atpl/core/version.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string>

TEST_CASE("version text matches the version numbers", "[core][version]") {
    const atpl::Version v = atpl::version();
    const std::string expected =
        std::to_string(v.major) + "." + std::to_string(v.minor) + "." + std::to_string(v.patch);

    REQUIRE(atpl::versionString() == expected);
}

TEST_CASE("version numbers are not negative", "[core][version]") {
    const atpl::Version v = atpl::version();

    REQUIRE(v.major >= 0);
    REQUIRE(v.minor >= 0);
    REQUIRE(v.patch >= 0);
}
