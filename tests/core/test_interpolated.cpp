#include "atpl/core/interpolated.hpp"

#include "support/manual_clock.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <chrono>

using atpl::TransitionFunction;
using Catch::Approx;

namespace {

template <typename T>
using TestValue = atpl::Interpolated<T, ManualClock>;

} // namespace

TEST_CASE("a new value rests at its initial value", "[core][interpolated]") {
    ManualClock::reset();
    const TestValue<float> value(3.0f, TransitionFunction::Linear, 2.0f);

    REQUIRE_FALSE(value.running());
    REQUIRE(value.getValue() == 3.0f);
    REQUIRE(value.target() == 3.0f);
}

TEST_CASE("a value moves linearly to its target over the duration", "[core][interpolated]") {
    ManualClock::reset();
    TestValue<float> value(0.0f, TransitionFunction::Linear, 2.0f);

    value.setValue(10.0f);
    REQUIRE(value.running());
    REQUIRE(value.getValue() == Approx(0.0f));
    REQUIRE(value.target() == 10.0f);

    ManualClock::advance(0.5f);
    REQUIRE(value.getValue() == Approx(2.5f));

    ManualClock::advance(0.5f);
    REQUIRE(value.getValue() == Approx(5.0f));
    REQUIRE(value.status() == Approx(0.5f));

    ManualClock::advance(1.0f);
    REQUIRE_FALSE(value.running());
    REQUIRE(value.getValue() == 10.0f);

    ManualClock::advance(100.0f);
    REQUIRE(value.getValue() == 10.0f);
}

TEST_CASE("a new target starts from the current value", "[core][interpolated]") {
    ManualClock::reset();
    TestValue<float> value(0.0f, TransitionFunction::Linear, 1.0f);

    value.setValue(10.0f);
    ManualClock::advance(0.5f);
    value = 0.0f; // assignment sets a new target

    REQUIRE(value.getValue() == Approx(5.0f));
    ManualClock::advance(0.5f);
    REQUIRE(value.getValue() == Approx(2.5f));
    ManualClock::advance(0.5f);
    REQUIRE(value.getValue() == 0.0f);
}

TEST_CASE("the easing curve shapes the movement", "[core][interpolated]") {
    ManualClock::reset();
    TestValue<float> value(0.0f, TransitionFunction::EaseInOutQuint, 1.0f);

    value.setValue(1.0f);
    ManualClock::advance(0.25f);

    REQUIRE(value.getValue() == Approx(atpl::easeInOutQuint(0.25f)));
    REQUIRE(value.getValue() < 0.25f);
}

TEST_CASE("zero duration and the None transition jump to the target", "[core][interpolated]") {
    ManualClock::reset();

    TestValue<float> instant(0.0f, TransitionFunction::Linear, 0.0f);
    instant.setValue(4.0f);
    REQUIRE_FALSE(instant.running());
    REQUIRE(instant.getValue() == 4.0f);

    TestValue<float> unanimated(0.0f, TransitionFunction::None, 1.0f);
    unanimated.setValue(4.0f);
    REQUIRE_FALSE(unanimated.running());
    REQUIRE(unanimated.getValue() == 4.0f);
}

TEST_CASE("the duration can be changed", "[core][interpolated]") {
    ManualClock::reset();
    TestValue<float> value(0.0f, TransitionFunction::Linear, 1.0f);
    REQUIRE(value.duration() == Approx(1.0f));

    value.setDuration(4.0f);
    REQUIRE(value.duration() == Approx(4.0f));

    value.setValue(8.0f);
    ManualClock::advance(1.0f);
    REQUIRE(value.getValue() == Approx(2.0f));
}

TEST_CASE("integer values are interpolated too", "[core][interpolated]") {
    ManualClock::reset();
    TestValue<int> value(0, TransitionFunction::Linear, 1.0f);

    value.setValue(10);
    ManualClock::advance(0.5f);
    REQUIRE(value.getValue() == 5);

    ManualClock::advance(0.5f);
    REQUIRE(value.getValue() == 10);
}

TEST_CASE("precision does not degrade after a long uptime", "[core][interpolated]") {
    // 30 days on the clock: seconds since the epoch no longer fit a float with millisecond precision.
    ManualClock::reset(std::chrono::hours(24 * 30));
    TestValue<float> value(0.0f, TransitionFunction::Linear, 0.2f);

    value.setValue(1.0f);
    ManualClock::advance(0.05f);
    REQUIRE(value.getValue() == Approx(0.25f).margin(1e-3));

    ManualClock::advance(0.05f);
    REQUIRE(value.getValue() == Approx(0.5f).margin(1e-3));
}

TEST_CASE("the default clock is real time", "[core][interpolated]") {
    atpl::Interpolated<float> value(1.0f, TransitionFunction::Linear, 60.0f);
    REQUIRE_FALSE(value.running());

    value.setValue(2.0f);
    REQUIRE(value.running());
    REQUIRE(value.getValue() >= 1.0f);
    REQUIRE(value.getValue() < 2.0f);
}
