#include "atpl/core/random.hpp"
#include "atpl/core/thread_pool.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <numeric>
#include <random>
#include <set>
#include <thread>
#include <vector>

using namespace atpl;

namespace {

template <typename T>
std::vector<T> firstOf(Random random, std::size_t count) {
    std::vector<T> values;
    for (std::size_t i = 0; i < count; ++i) {
        values.push_back(static_cast<T>(random.next()));
    }
    return values;
}

struct Point {
    float x;
    float y;
};

} // namespace

TEST_CASE("a seed gives the same numbers on every platform", "[core][random]") {
    // Golden values: if these change, every program that relies on its seed changes too.
    Random random(42, 54);
    const std::array<std::uint32_t, 4> expected{ 0xcd0fae4eU, 0xc3363e40U, 0xc448683dU, 0x69675842U };
    for (const std::uint32_t value : expected) {
        REQUIRE(random.next() == value);
    }
    Random other(7);
    REQUIRE(other.range(0, 999) == 908);
    REQUIRE(other.uniform<double>() == 0x1.c7de1e39aa024p-1);
    REQUIRE(other.uniform() == 0x1.e5aeccp-2f);
}

TEST_CASE("a generator is reproducible, and seeds and streams differ", "[core][random]") {
    REQUIRE(firstOf<std::uint32_t>(Random(5), 100) == firstOf<std::uint32_t>(Random(5), 100));
    REQUIRE(firstOf<std::uint32_t>(Random(), 100) == firstOf<std::uint32_t>(Random(Random::defaultSeed), 100));
    REQUIRE(firstOf<std::uint32_t>(Random(5), 100) != firstOf<std::uint32_t>(Random(6), 100));
    REQUIRE(firstOf<std::uint32_t>(Random(5, 0), 100) != firstOf<std::uint32_t>(Random(5, 1), 100));

    Random random(5);
    (void)random.next();
    random.seed(5);
    REQUIRE(firstOf<std::uint32_t>(random, 100) == firstOf<std::uint32_t>(Random(5), 100));
}

TEST_CASE("uniform numbers lie from 0 up to 1, without 1, and fill that evenly", "[core][random]") {
    Random random(1);
    std::array<int, 10> buckets{};
    for (int i = 0; i < 100000; ++i) {
        const float f = random.uniform();
        const double d = random.uniform<double>();
        REQUIRE(f >= 0.0f);
        REQUIRE(f < 1.0f);
        REQUIRE(d >= 0.0);
        REQUIRE(d < 1.0);
        ++buckets[static_cast<std::size_t>(d * 10.0)];
    }
    for (const int count : buckets) {
        REQUIRE(count > 9500);
        REQUIRE(count < 10500);
    }
}

TEST_CASE("a floating-point range excludes its end", "[core][random]") {
    Random random(2);
    float low = 10.0f;
    float high = -10.0f;
    for (int i = 0; i < 100000; ++i) {
        const float value = random.range(-2.0f, 3.0f);
        REQUIRE(value >= -2.0f);
        REQUIRE(value < 3.0f);
        low = std::min(low, value);
        high = std::max(high, value);
    }
    REQUIRE(low < -1.99f);
    REQUIRE(high > 2.99f);
    REQUIRE(random.range(1.5, 1.5) == 1.5); // an empty range gives its start
}

TEST_CASE("an integer range includes both ends, every value equally often", "[core][random]") {
    Random random(3);
    std::map<int, int> counts;
    for (int i = 0; i < 70000; ++i) {
        const int value = random.range(-3, 3);
        REQUIRE(value >= -3);
        REQUIRE(value <= 3);
        ++counts[value];
    }
    REQUIRE(counts.size() == 7);
    for (const auto& [value, count] : counts) {
        REQUIRE(count > 9500);
        REQUIRE(count < 10500);
    }
    REQUIRE(random.range(4, 4) == 4);
}

TEST_CASE("integer ranges work for every width, up to the whole type", "[core][random]") {
    Random random(4);
    for (int i = 0; i < 1000; ++i) {
        const auto byte = random.range<std::uint8_t>(250, 255);
        REQUIRE(byte >= 250);
        const auto small = random.range<std::int8_t>(-128, 127);
        REQUIRE(small >= -128);
        const auto wide = random.range<std::int64_t>(-5'000'000'000LL, 5'000'000'000LL);
        REQUIRE(wide >= -5'000'000'000LL);
        REQUIRE(wide <= 5'000'000'000LL);
        (void)random.range(std::numeric_limits<std::int64_t>::min(), std::numeric_limits<std::int64_t>::max());
        (void)random.range(std::numeric_limits<std::uint32_t>::min(), std::numeric_limits<std::uint32_t>::max());
    }

    // Values beyond 32 bits are reached.
    bool beyond = false;
    for (int i = 0; i < 100 && !beyond; ++i) {
        beyond = random.range<std::uint64_t>(0, 1ULL << 40U) > (1ULL << 32U);
    }
    REQUIRE(beyond);
}

TEST_CASE("below gives every value from 0 to n - 1", "[core][random]") {
    Random random(5);
    std::set<std::size_t> seen;
    for (int i = 0; i < 1000; ++i) {
        const std::size_t value = random.below(std::size_t{ 6 });
        REQUIRE(value < 6);
        seen.insert(value);
    }
    REQUIRE(seen.size() == 6);
    REQUIRE(random.below(1) == 0);
}

TEST_CASE("chance is right as often as it should be", "[core][random]") {
    Random random(6);
    int hits = 0;
    for (int i = 0; i < 100000; ++i) {
        hits += random.chance(0.25) ? 1 : 0;
        REQUIRE_FALSE(random.chance(0.0));
        REQUIRE(random.chance(1.0));
    }
    REQUIRE(hits > 24000);
    REQUIRE(hits < 26000);
}

TEST_CASE("normal numbers have the asked mean and deviation", "[core][random]") {
    Random random(7);
    constexpr int count = 100000;
    double sum = 0.0;
    double squares = 0.0;
    for (int i = 0; i < count; ++i) {
        const double value = random.normal(5.0, 2.0);
        REQUIRE(std::isfinite(value));
        sum += value;
        squares += value * value;
    }
    const double mean = sum / count;
    const double deviation = std::sqrt(squares / count - mean * mean);
    REQUIRE(std::abs(mean - 5.0) < 0.05);
    REQUIRE(std::abs(deviation - 2.0) < 0.05);
}

TEST_CASE("angles, directions and points in a disc", "[core][random]") {
    Random random(8);
    std::array<int, 4> quadrants{};
    for (int i = 0; i < 40000; ++i) {
        const float angle = random.angle();
        REQUIRE(angle >= 0.0f);
        REQUIRE(angle < 2.0f * std::numbers::pi_v<float>);

        const auto [x, y] = random.unitVector();
        REQUIRE(std::abs(std::sqrt(x * x + y * y) - 1.0f) < 1e-5f);
        ++quadrants[(x < 0.0f ? 1U : 0U) + (y < 0.0f ? 2U : 0U)];

        const Point point = random.inDisc<Point>(3.0f);
        REQUIRE(point.x * point.x + point.y * point.y < 9.0f);
    }
    for (const int count : quadrants) {
        REQUIRE(count > 9500);
        REQUIRE(count < 10500);
    }
}

TEST_CASE("a generator works with the standard library", "[core][random]") {
    std::vector<int> values(20);
    std::iota(values.begin(), values.end(), 0);
    std::vector<int> again = values;
    Random first(9);
    Random second(9);
    std::shuffle(values.begin(), values.end(), first);
    std::shuffle(again.begin(), again.end(), second);
    REQUIRE(values == again);
    REQUIRE_FALSE(std::is_sorted(values.begin(), values.end()));

    std::uniform_int_distribution<int> distribution(1, 6);
    const int die = distribution(first);
    REQUIRE(die >= 1);
    REQUIRE(die <= 6);
}

TEST_CASE("generators from entropy differ", "[core][random]") {
    REQUIRE(firstOf<std::uint32_t>(Random::fromEntropy(), 8) != firstOf<std::uint32_t>(Random::fromEntropy(), 8));
}

TEST_CASE("random streams are one independent generator per part", "[core][random]") {
    RandomStreams streams(11, 4);
    REQUIRE(streams.size() == 4);
    REQUIRE(firstOf<std::uint32_t>(streams[2], 10) == firstOf<std::uint32_t>(Random(11, 2), 10));
    REQUIRE(firstOf<std::uint32_t>(streams[0], 10) != firstOf<std::uint32_t>(streams[1], 10));

    (void)streams[1].next();
    streams.seed(11);
    REQUIRE(firstOf<std::uint32_t>(streams[1], 10) == firstOf<std::uint32_t>(Random(11, 1), 10));
}

TEST_CASE("a loop over a thread pool with random streams gives the same result every run", "[core][random]") {
    ThreadPool pool(3);
    const auto run = [&pool] {
        RandomStreams streams(12, pool.maxParts());
        std::vector<float> values(10000);
        for (int tick = 0; tick < 5; ++tick) {
            pool.parallelFor(values.size(), [&](std::size_t start, std::size_t end, std::size_t part) {
                for (std::size_t i = start; i < end; ++i) {
                    values[i] += streams[part].range(-1.0f, 1.0f);
                }
            });
        }
        return values;
    };
    const std::vector<float> first = run();
    for (int again = 0; again < 10; ++again) {
        REQUIRE(run() == first);
    }
}

TEST_CASE("every thread has a generator of its own", "[core][random]") {
    Random* mine = &threadRandom();
    REQUIRE(&threadRandom() == mine);
    Random* theirs = nullptr;
    std::uint32_t value = 0;
    std::thread thread([&] {
        theirs = &threadRandom();
        value = threadRandom().next();
    });
    thread.join();
    REQUIRE(theirs != mine);
    (void)value;
}
