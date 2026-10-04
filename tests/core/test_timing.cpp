#include "atpl/core/timing.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <thread>
#include <vector>

using namespace atpl;
using namespace std::chrono_literals;

// Timing is checked from below only: a sleep lasts at least as long as asked, but how much
// longer depends on the machine (shared CI runners, Windows' coarse timer).

TEST_CASE("a stopwatch measures at least the time that passed", "[core][timing]") {
    Stopwatch stopwatch;
    std::this_thread::sleep_for(20ms);
    const double seconds = stopwatch.seconds();
    REQUIRE(seconds >= 0.019);
    REQUIRE(stopwatch.milliseconds() >= seconds * 1000.0); // the clock never goes back

    const double passed = stopwatch.restart();
    REQUIRE(passed >= seconds);
    REQUIRE(stopwatch.seconds() < passed); // started again
}

TEST_CASE("a cooldown fires once per period of the time it is given", "[core][timing]") {
    Cooldown cooldown(0.5);
    REQUIRE(cooldown.period() == 0.5);
    REQUIRE(cooldown.advance(0.2) == 0);
    REQUIRE(cooldown.progress() == Catch::Approx(0.4));
    REQUIRE(cooldown.advance(0.2) == 0);
    REQUIRE(cooldown.advance(0.2) == 1); // 0.6: one period, 0.1 left over
    REQUIRE(cooldown.progress() == Catch::Approx(0.2));
    REQUIRE(cooldown.advance(0.4) == 1); // the left-over time counts
    REQUIRE(cooldown.progress() == Catch::Approx(0.0).margin(1e-9));
}

TEST_CASE("a cooldown does not drift over many small steps", "[core][timing]") {
    Cooldown cooldown(0.1);
    int fired = 0;
    for (int tick = 0; tick < 3600; ++tick) {
        fired += cooldown.advance(1.0 / 60.0); // a minute at 60 ticks per second
    }
    REQUIRE(fired >= 599);
    REQUIRE(fired <= 600);
}

TEST_CASE("a cooldown counts every period of a long step", "[core][timing]") {
    Cooldown cooldown(0.25);
    REQUIRE(cooldown.advance(1.1) == 4);
    REQUIRE(cooldown.progress() == Catch::Approx(0.4));
    REQUIRE(cooldown.advance(-1.0) == 0); // time does not go back
    REQUIRE(cooldown.progress() == Catch::Approx(0.4));
}

TEST_CASE("a cooldown can be reset and given a new period", "[core][timing]") {
    Cooldown cooldown(1.0);
    REQUIRE(cooldown.advance(0.7) == 0);
    cooldown.reset();
    REQUIRE(cooldown.progress() == 0.0);
    REQUIRE(cooldown.advance(0.7) == 0);
    cooldown.setPeriod(0.5); // the time passed is kept
    REQUIRE(cooldown.advance(0.0) == 1);
    REQUIRE(cooldown.progress() == Catch::Approx(0.4));
}

TEST_CASE("a cooldown without a period fires every time", "[core][timing]") {
    Cooldown cooldown(0.0);
    REQUIRE(cooldown.advance(0.0) == 1);
    REQUIRE(cooldown.advance(5.0) == 1);
    REQUIRE(cooldown.progress() == 1.0);
}

TEST_CASE("a running average is the average of the newest values", "[core][timing]") {
    RunningAverage average(3);
    REQUIRE(average.window() == 3);
    REQUIRE(average.count() == 0);
    REQUIRE(average.average() == 0.0);

    average.add(3.0);
    REQUIRE(average.average() == 3.0);
    average.add(6.0);
    REQUIRE(average.average() == 4.5);
    REQUIRE_FALSE(average.full());
    average.add(9.0);
    REQUIRE(average.full());
    REQUIRE(average.average() == 6.0);
    average.add(12.0); // 3 drops out
    REQUIRE(average.count() == 3);
    REQUIRE(average.average() == 9.0);

    average.clear();
    REQUIRE(average.count() == 0);
    REQUIRE(average.average() == 0.0);
    average.add(1.0);
    REQUIRE(average.average() == 1.0);
}

TEST_CASE("a running average does not gather rounding errors", "[core][timing]") {
    RunningAverage average(10);
    // Large values first, then small ones: a sum that is only ever updated would keep an error.
    for (int i = 0; i < 10; ++i) {
        average.add(1e15 + 0.1);
    }
    for (int i = 0; i < 1000; ++i) {
        average.add(0.1);
    }
    REQUIRE(average.average() == Catch::Approx(0.1).epsilon(1e-12));
}

TEST_CASE("a running average of window 0 holds one value", "[core][timing]") {
    RunningAverage average(0);
    REQUIRE(average.window() == 1);
    average.add(2.0);
    average.add(5.0);
    REQUIRE(average.average() == 5.0);
}

TEST_CASE("a scoped timer writes the milliseconds of its scope", "[core][timing]") {
    Series series(4);
    RunningAverage average(4);
    {
        const ScopedTimer toSeries(series);
        const ScopedTimer toAverage(average);
        std::this_thread::sleep_for(10ms);
        REQUIRE(toSeries.milliseconds() >= 9.0);
        REQUIRE(series.size() == 0); // only at the end
    }
    std::vector<float> samples(4);
    REQUIRE(series.read(samples) == 1);
    REQUIRE(samples[0] >= 9.0f);
    REQUIRE(average.count() == 1);
    REQUIRE(average.average() >= 9.0);
}
