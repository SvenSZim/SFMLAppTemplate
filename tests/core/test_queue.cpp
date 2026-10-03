#include "atpl/core/queue.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <chrono>
#include <memory>
#include <thread>
#include <vector>

using namespace atpl;
using namespace std::chrono_literals;

TEST_CASE("a queue hands out its items in the order they went in", "[core][queue]") {
    Queue<int> queue;
    REQUIRE(queue.empty());
    REQUIRE_FALSE(queue.tryPop().has_value());

    queue.push(1);
    queue.push(2);
    queue.push(3);
    REQUIRE(queue.size() == 3);
    REQUIRE(queue.tryPop() == 1);

    std::vector<int> out{ 0 };
    REQUIRE(queue.drain(out) == 2);
    REQUIRE(out == std::vector<int>{ 0, 2, 3 }); // appended, in order
    REQUIRE(queue.empty());
    REQUIRE(queue.drain(out) == 0);
}

TEST_CASE("a queue can be cleared, and takes items that can only be moved", "[core][queue]") {
    Queue<std::unique_ptr<int>> queue;
    queue.push(std::make_unique<int>(7));
    queue.push(std::make_unique<int>(8));
    const std::optional<std::unique_ptr<int>> first = queue.tryPop();
    REQUIRE(**first == 7);
    queue.clear();
    REQUIRE(queue.empty());
}

TEST_CASE("waiting for items returns at once if there are some, and gives up after the timeout", "[core][queue]") {
    Queue<int> queue;
    std::vector<int> out;
    queue.push(5);
    REQUIRE(queue.waitDrain(out, 10s) == 1); // nothing to wait for

    const auto start = std::chrono::steady_clock::now();
    REQUIRE(queue.waitDrain(out, 20ms) == 0);
    REQUIRE(std::chrono::steady_clock::now() - start >= 20ms);
}

TEST_CASE("an item pushed from another thread wakes a waiting reader", "[core][queue][threads]") {
    Queue<int> queue;
    std::vector<int> out;
    std::thread writer([&queue] {
        std::this_thread::sleep_for(10ms);
        queue.push(42);
    });
    const auto start = std::chrono::steady_clock::now();
    const std::size_t count = queue.waitDrain(out, 10s);
    writer.join();
    REQUIRE(count == 1);
    REQUIRE(out == std::vector<int>{ 42 });
    REQUIRE(std::chrono::steady_clock::now() - start < 5s); // woken, not timed out
}

TEST_CASE("items from several threads all arrive, each thread's in its order", "[core][queue][threads]") {
    constexpr int writers = 4;
    constexpr int perWriter = 1000;
    Queue<std::pair<int, int>> queue;
    std::vector<std::thread> threads;
    for (int w = 0; w < writers; ++w) {
        threads.emplace_back([&queue, w] {
            for (int i = 0; i < perWriter; ++i) {
                queue.push({ w, i });
            }
        });
    }

    std::vector<std::pair<int, int>> received;
    while (received.size() < static_cast<std::size_t>(writers * perWriter)) {
        queue.waitDrain(received, 100ms);
    }
    for (std::thread& thread : threads) {
        thread.join();
    }

    std::array<int, writers> next{};
    for (const auto& [writer, index] : received) {
        REQUIRE(index == next[static_cast<std::size_t>(writer)]);
        ++next[static_cast<std::size_t>(writer)];
    }
    REQUIRE(queue.empty());
}
