#include "atpl/ui/binding.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <cstdint>
#include <limits>

using atpl::numberTo;

TEST_CASE("integers survive the trip through double unchanged", "[ui][binding]") {
    // What a widget does with a bound integer: read it as double, write it back.
    const auto roundTrip = [](auto value) { return numberTo<decltype(value)>(static_cast<double>(value)); };

    REQUIRE(roundTrip(0) == 0);
    REQUIRE(roundTrip(-1) == -1);
    REQUIRE(roundTrip(123456789) == 123456789);
    REQUIRE(roundTrip(std::numeric_limits<int>::max()) == std::numeric_limits<int>::max());
    REQUIRE(roundTrip(std::numeric_limits<int>::lowest()) == std::numeric_limits<int>::lowest());
    REQUIRE(roundTrip(std::numeric_limits<std::uint32_t>::max()) == std::numeric_limits<std::uint32_t>::max());
    REQUIRE(roundTrip(std::int8_t{ -128 }) == std::int8_t{ -128 });

    // 64-bit integers are exact up to 2^53.
    const std::int64_t exactLimit = std::int64_t{ 1 } << 53;
    REQUIRE(roundTrip(exactLimit) == exactLimit);
    REQUIRE(roundTrip(-exactLimit) == -exactLimit);
    REQUIRE(roundTrip(exactLimit - 1) == exactLimit - 1);

    // Odd values just above 2^52 are where a careless "add 0.5 and truncate" goes wrong.
    const std::int64_t oddAbove2to52 = (std::int64_t{ 1 } << 52) + 1;
    REQUIRE(roundTrip(oddAbove2to52) == oddAbove2to52);
    REQUIRE(roundTrip(-oddAbove2to52) == -oddAbove2to52);
}

TEST_CASE("64-bit integers above 2^53 lose their lowest digits, as documented", "[ui][binding]") {
    const std::int64_t beyond = (std::int64_t{ 1 } << 53) + 1;

    REQUIRE(numberTo<std::int64_t>(static_cast<double>(beyond)) != beyond);
    REQUIRE(numberTo<std::int64_t>(static_cast<double>(beyond)) == beyond - 1);
}

TEST_CASE("float survives the trip through double unchanged", "[ui][binding]") {
    for (const float value : { 0.f, 0.1f, -3.75f, 1e-30f, 3.4e38f, std::numeric_limits<float>::max() }) {
        REQUIRE(numberTo<float>(static_cast<double>(value)) == value);
    }
}

TEST_CASE("a number written to an integer is rounded to the nearest value", "[ui][binding]") {
    REQUIRE(numberTo<int>(2.4) == 2);
    REQUIRE(numberTo<int>(2.5) == 3);
    REQUIRE(numberTo<int>(2.6) == 3);
    REQUIRE(numberTo<int>(-2.4) == -2);
    REQUIRE(numberTo<int>(-2.5) == -3);
    REQUIRE(numberTo<int>(-0.4) == 0);
    REQUIRE(numberTo<unsigned>(0.49) == 0u);
}

TEST_CASE("a number outside the type's range is clamped, never undefined", "[ui][binding]") {
    const double infinity = std::numeric_limits<double>::infinity();

    REQUIRE(numberTo<int>(1e30) == std::numeric_limits<int>::max());
    REQUIRE(numberTo<int>(-1e30) == std::numeric_limits<int>::lowest());
    REQUIRE(numberTo<int>(infinity) == std::numeric_limits<int>::max());
    REQUIRE(numberTo<int>(-infinity) == std::numeric_limits<int>::lowest());
    REQUIRE(numberTo<std::uint8_t>(300.0) == 255);
    REQUIRE(numberTo<std::uint8_t>(-5.0) == 0);
    REQUIRE(numberTo<unsigned>(-0.6) == 0u);

    // The limits of 64-bit types are not representable as double; values at or past them clamp.
    REQUIRE(numberTo<std::int64_t>(9.3e18) == std::numeric_limits<std::int64_t>::max());
    REQUIRE(numberTo<std::int64_t>(-9.3e18) == std::numeric_limits<std::int64_t>::lowest());
    REQUIRE(numberTo<std::uint64_t>(1.9e19) == std::numeric_limits<std::uint64_t>::max());
    REQUIRE(numberTo<std::int64_t>(9223372036854775808.0) == std::numeric_limits<std::int64_t>::max());

    REQUIRE(numberTo<float>(1e300) == std::numeric_limits<float>::max());
    REQUIRE(numberTo<float>(-1e300) == std::numeric_limits<float>::lowest());
}

TEST_CASE("not-a-number becomes 0 for integers and stays for floating point", "[ui][binding]") {
    const double nan = std::numeric_limits<double>::quiet_NaN();

    REQUIRE(numberTo<int>(nan) == 0);
    REQUIRE(std::isnan(numberTo<float>(nan)));
    REQUIRE(std::isnan(numberTo<double>(nan)));
}

TEST_CASE("the conversion can be used in constant expressions", "[ui][binding]") {
    static_assert(numberTo<int>(41.6) == 42);
    static_assert(numberTo<std::uint8_t>(1000.0) == 255);
    SUCCEED();
}
