#include "atpl/core/param.hpp"

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <cstdint>
#include <string>
#include <thread>
#include <vector>

using atpl::Param;
using atpl::Revision;

namespace {

enum class Mode { Normal, Fast, Debug };

} // namespace

TEST_CASE("a parameter reads and writes like a plain value", "[core][param]") {
    Param<float> speed = 5.f;
    REQUIRE(speed.get() == 5.f);
    REQUIRE(static_cast<float>(speed) == 5.f);

    speed = 7.5f;
    REQUIRE(speed == 7.5f);
    speed.set(2.f);
    REQUIRE(speed * 2.f == 4.f);

    const Param<int> count;
    REQUIRE(count.get() == 0); // value-initialised

    Param<std::string> status = std::string("ready");
    REQUIRE(status.get() == "ready");
    status = std::string("running");
    REQUIRE(status.get() == "running");

    Param<Mode> mode = Mode::Fast;
    REQUIRE(mode.get() == Mode::Fast);
}

TEST_CASE("the revision grows when the value changes, and only then", "[core][param]") {
    Param<int> count = 1;
    const Revision start = count.revision();

    count = 2;
    REQUIRE(count.revision() == start + 1);
    count = 2; // the same value: nothing changed
    REQUIRE(count.revision() == start + 1);
    count = 3;
    REQUIRE(count.revision() == start + 2);

    Param<std::string> text = std::string("a");
    const Revision before = text.revision();
    text = std::string("a");
    REQUIRE(text.revision() == before);
    text = std::string("b");
    REQUIRE(text.revision() == before + 1);
}

TEST_CASE("assigning one parameter to another copies the value", "[core][param]") {
    Param<float> a = 1.f;
    Param<float> b = 2.f;
    a = b;
    REQUIRE(a.get() == 2.f);
    REQUIRE(b.get() == 2.f);

    const Revision before = a.revision();
    a = a; // to itself: nothing happens
    REQUIRE(a.revision() == before);
}

TEST_CASE("small trivially copyable values need no lock, strings do", "[core][param]") {
    STATIC_REQUIRE(Param<float>::isLockFree);
    STATIC_REQUIRE(Param<double>::isLockFree);
    STATIC_REQUIRE(Param<int>::isLockFree);
    STATIC_REQUIRE(Param<std::int64_t>::isLockFree);
    STATIC_REQUIRE(Param<bool>::isLockFree);
    STATIC_REQUIRE(Param<Mode>::isLockFree);
    STATIC_REQUIRE_FALSE(Param<std::string>::isLockFree);
}

// ----- Between threads -----

TEST_CASE("a value written by one thread is read whole by another", "[core][param][threads]") {
    // The writer alternates between two long strings; a torn read would mix them.
    const std::string as(200, 'a');
    const std::string bs(200, 'b');
    Param<std::string> text = as;
    std::atomic<bool> done = false;

    std::thread writer([&] {
        for (int i = 0; i < 20000; ++i) {
            text = (i % 2 == 0) ? bs : as;
        }
        done = true;
    });

    int reads = 0;
    bool whole = true;
    while (!done || reads < 1000) {
        const std::string seen = text.get();
        whole = whole && (seen == as || seen == bs);
        ++reads;
    }
    writer.join();
    REQUIRE(whole);
}

TEST_CASE("a reader that sees a new revision sees the new value", "[core][param][threads]") {
    // The writer counts up. Whenever the revision has moved past what the reader saw, the value
    // must have moved too, and values never go back.
    Param<std::int64_t> counter = 0;
    std::atomic<bool> done = false;

    std::thread writer([&] {
        for (std::int64_t i = 1; i <= 50000; ++i) {
            counter = i;
        }
        done = true;
    });

    std::int64_t last = 0;
    bool ordered = true;
    while (!done) {
        const Revision revision = counter.revision();
        const std::int64_t value = counter.get();
        ordered = ordered && value >= last && value >= static_cast<std::int64_t>(revision);
        last = value;
    }
    writer.join();
    REQUIRE(ordered);
    REQUIRE(counter.get() == 50000);
    REQUIRE(counter.revision() == 50000);
}

TEST_CASE("several threads may write at once", "[core][param][threads]") {
    Param<int> value = 0;
    std::vector<std::thread> writers;
    for (int t = 1; t <= 4; ++t) {
        writers.emplace_back([&value, t] {
            for (int i = 0; i < 10000; ++i) {
                value = t * 100000 + i;
            }
        });
    }
    for (std::thread& writer : writers) {
        writer.join();
    }
    // Whatever came last, it is one of the values written, whole.
    const int last = value.get();
    REQUIRE(last / 100000 >= 1);
    REQUIRE(last / 100000 <= 4);
    REQUIRE(last % 100000 == 9999);
}
