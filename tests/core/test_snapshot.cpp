#include "atpl/core/snapshot.hpp"

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <cstdint>
#include <thread>
#include <vector>

using namespace atpl;

namespace {

/// A state that is only consistent if it was written as a whole.
struct State {
    std::uint64_t tick = 0;
    std::vector<std::uint64_t> copies = std::vector<std::uint64_t>(64, 0); ///< Each equal to `tick`.

    void fill(std::uint64_t value) {
        tick = value;
        for (std::uint64_t& copy : copies) {
            copy = value;
        }
    }
    [[nodiscard]] bool whole() const {
        for (const std::uint64_t copy : copies) {
            if (copy != tick) {
                return false;
            }
        }
        return true;
    }
};

} // namespace

TEST_CASE("a snapshot reads its initial state until something is published", "[core][snapshot]") {
    Snapshot<int> snapshot(7);
    REQUIRE_FALSE(snapshot.hasNew());
    REQUIRE(snapshot.read() == 7);
    REQUIRE(snapshot.revision() == 0);

    Snapshot<int> plain;
    REQUIRE(plain.read() == 0);
}

TEST_CASE("the reader gets the newest published state, and only once as new", "[core][snapshot]") {
    Snapshot<int> snapshot;
    snapshot.writeBuffer() = 1;
    snapshot.publish();
    REQUIRE(snapshot.hasNew());
    REQUIRE(snapshot.read() == 1);
    REQUIRE_FALSE(snapshot.hasNew());
    REQUIRE(snapshot.read() == 1); // the same state again

    // Two publishes before a read: the first is skipped.
    snapshot.publish(2);
    snapshot.publish(3);
    REQUIRE(snapshot.read() == 3);
    REQUIRE(snapshot.revision() == 3);
}

TEST_CASE("what the reader holds does not change until it reads again", "[core][snapshot]") {
    Snapshot<int> snapshot;
    snapshot.publish(1);
    const int& held = snapshot.read();
    for (int i = 2; i < 10; ++i) {
        snapshot.publish(i); // the writer goes on publishing
    }
    REQUIRE(held == 1);
    REQUIRE(snapshot.read() == 9);
}

TEST_CASE("the write buffer is never the state the reader holds", "[core][snapshot]") {
    Snapshot<int> snapshot;
    snapshot.publish(1);
    const int& held = snapshot.read();
    for (int i = 0; i < 6; ++i) {
        REQUIRE(&snapshot.writeBuffer() != &held);
        snapshot.publish(i);
    }
}

TEST_CASE("a reader on another thread only ever sees whole states, never older ones", "[core][snapshot][threads]") {
    Snapshot<State> snapshot;
    constexpr std::uint64_t ticks = 20000;
    std::atomic<bool> done{ false };

    std::thread writer([&] {
        for (std::uint64_t tick = 1; tick <= ticks; ++tick) {
            snapshot.writeBuffer().fill(tick);
            snapshot.publish();
        }
        done.store(true, std::memory_order_release);
    });

    std::uint64_t last = 0;
    bool whole = true;
    bool forward = true;
    while (!done.load(std::memory_order_acquire) || snapshot.hasNew()) {
        const State& state = snapshot.read();
        whole = whole && state.whole();
        forward = forward && state.tick >= last;
        last = state.tick;
    }
    writer.join();
    REQUIRE(whole);
    REQUIRE(forward);
    REQUIRE(snapshot.read().tick == ticks); // the last one arrives
    REQUIRE(snapshot.revision() == ticks);
}
