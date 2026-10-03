#include "atpl/core/series.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <atomic>
#include <thread>
#include <vector>

using atpl::Point;
using atpl::PointSeries;
using atpl::Revision;
using atpl::Series;

TEST_CASE("a series keeps the samples pushed into it, oldest first", "[core][series]") {
    Series series(4);
    REQUIRE(series.capacity() == 4);
    REQUIRE(series.size() == 0);

    series.push(1.f);
    series.push(2.f);
    std::array<float, 4> out{};
    REQUIRE(series.read(out) == 2);
    REQUIRE(out[0] == 1.f);
    REQUIRE(out[1] == 2.f);
}

TEST_CASE("a full series drops its oldest samples", "[core][series]") {
    Series series(3);
    for (float sample = 1.f; sample <= 5.f; sample += 1.f) {
        series.push(sample);
    }
    REQUIRE(series.size() == 3);
    std::array<float, 3> out{};
    REQUIRE(series.read(out) == 3);
    REQUIRE(out == std::array<float, 3>{ 3.f, 4.f, 5.f });
}

TEST_CASE("reading into less room gives the newest samples", "[core][series]") {
    Series series(10);
    const std::array<float, 5> samples{ 1.f, 2.f, 3.f, 4.f, 5.f };
    series.push(samples);
    std::array<float, 2> out{};
    REQUIRE(series.read(out) == 2);
    REQUIRE(out == std::array<float, 2>{ 4.f, 5.f });
}

TEST_CASE("pushing more samples than fit at once keeps the newest", "[core][series]") {
    Series series(3);
    series.push(0.f);
    const std::array<float, 5> samples{ 1.f, 2.f, 3.f, 4.f, 5.f };
    series.push(samples);
    std::array<float, 3> out{};
    REQUIRE(series.read(out) == 3);
    REQUIRE(out == std::array<float, 3>{ 3.f, 4.f, 5.f });
}

TEST_CASE("the revision of a series grows with every push and every clear", "[core][series]") {
    Series series(3);
    const Revision start = series.revision();
    series.push(1.f);
    REQUIRE(series.revision() == start + 1);
    const std::array<float, 2> two{ 2.f, 3.f };
    series.push(two); // several samples: one change
    REQUIRE(series.revision() == start + 2);
    series.clear();
    REQUIRE(series.revision() == start + 3);
    REQUIRE(series.size() == 0);
    std::array<float, 3> out{};
    REQUIRE(series.read(out) == 0);
}

TEST_CASE("a series without room keeps nothing", "[core][series]") {
    Series series(0);
    series.push(1.f);
    std::array<float, 1> out{};
    REQUIRE(series.read(out) == 0);
}

TEST_CASE("a point series works like a series of points", "[core][series]") {
    PointSeries points(2);
    points.push({ .x = 1.f, .y = 10.f });
    points.push({ .x = 2.f, .y = 20.f });
    points.push({ .x = 3.f, .y = 30.f });
    REQUIRE(points.size() == 2);

    std::array<Point, 2> out{};
    REQUIRE(points.read(out) == 2);
    REQUIRE(out[0] == Point{ .x = 2.f, .y = 20.f });
    REQUIRE(out[1] == Point{ .x = 3.f, .y = 30.f });

    const Revision before = points.revision();
    points.clear();
    REQUIRE(points.revision() == before + 1);
    REQUIRE(points.size() == 0);
}

TEST_CASE("readers see unbroken runs of samples while a writer pushes", "[core][series][threads]") {
    // One writer pushes 1, 2, 3, ... Any read must be consecutive numbers: never a gap, never a
    // sample from a half-finished push.
    Series series(64);
    std::atomic<bool> done = false;

    std::thread writer([&] {
        for (int i = 1; i <= 100000; ++i) {
            series.push(static_cast<float>(i));
        }
        done = true;
    });

    std::array<float, 64> out{};
    bool consecutive = true;
    float last = 0.f;
    while (!done) {
        const std::size_t count = series.read(out);
        for (std::size_t i = 1; i < count; ++i) {
            consecutive = consecutive && out[i] == out[i - 1] + 1.f;
        }
        if (count > 0) {
            consecutive = consecutive && out[count - 1] >= last;
            last = out[count - 1];
        }
    }
    writer.join();
    REQUIRE(consecutive);
    REQUIRE(series.size() == 64);
}

TEST_CASE("several threads may push, read and clear at once", "[core][series][threads]") {
    Series series(32);
    std::atomic<bool> stop = false;
    std::vector<std::thread> threads;
    for (int t = 0; t < 3; ++t) {
        threads.emplace_back([&] {
            for (int i = 0; i < 20000; ++i) {
                series.push(static_cast<float>(i));
            }
        });
    }
    threads.emplace_back([&] {
        std::array<float, 32> out{};
        while (!stop) {
            static_cast<void>(series.read(out));
            static_cast<void>(series.size());
        }
    });
    threads.emplace_back([&] {
        for (int i = 0; i < 1000; ++i) {
            series.clear();
        }
    });
    for (std::size_t t = 0; t < 3; ++t) {
        threads[t].join();
    }
    stop = true;
    threads[3].join();
    threads[4].join();
    REQUIRE(series.size() <= 32);
}
