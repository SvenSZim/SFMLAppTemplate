#include "atpl/core/thread_pool.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <mutex>
#include <set>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

using namespace atpl;

namespace {

/// The ranges one loop was cut into, in part order: (start, end) per part.
std::vector<std::pair<std::size_t, std::size_t>>
rangesOf(ThreadPool& pool, std::size_t count, std::size_t minPartSize = 1) {
    std::vector<std::pair<std::size_t, std::size_t>> ranges(pool.maxParts(), { 0, 0 });
    std::atomic<std::size_t> calls = 0;
    pool.parallelFor(
        count,
        [&](std::size_t start, std::size_t end, std::size_t part) {
            ranges[part] = { start, end };
            ++calls;
        },
        minPartSize
    );
    ranges.resize(calls);
    return ranges;
}

} // namespace

TEST_CASE("a thread pool leaves one core for the main thread by default", "[core][thread_pool]") {
    const std::size_t hardware = std::thread::hardware_concurrency();
    const std::size_t expected = hardware > 2 ? hardware - 2 : 0;
    REQUIRE(ThreadPool::defaultWorkerCount() == expected);

    const ThreadPool pool;
    REQUIRE(pool.workerCount() == expected);
    REQUIRE(pool.maxParts() == expected + 1); // the calling thread works too
}

TEST_CASE("parallelFor covers every item exactly once", "[core][thread_pool]") {
    ThreadPool pool(3);
    for (const std::size_t count : { 1u, 2u, 3u, 4u, 5u, 17u, 1000u }) {
        std::vector<int> hits(count, 0);
        pool.parallelFor(count, [&](std::size_t start, std::size_t end) {
            for (std::size_t i = start; i < end; ++i) {
                ++hits[i];
            }
        });
        REQUIRE(std::all_of(hits.begin(), hits.end(), [](int h) { return h == 1; }));
    }
}

TEST_CASE("parallelFor cuts a loop into contiguous parts that differ by at most one item", "[core][thread_pool]") {
    ThreadPool pool(3);
    REQUIRE(pool.maxParts() == 4);

    const auto ranges = rangesOf(pool, 10);
    REQUIRE(ranges == std::vector<std::pair<std::size_t, std::size_t>>{ { 0, 2 }, { 2, 5 }, { 5, 7 }, { 7, 10 } });

    // Fewer items than parts: one item each.
    REQUIRE(rangesOf(pool, 2) == std::vector<std::pair<std::size_t, std::size_t>>{ { 0, 1 }, { 1, 2 } });
}

TEST_CASE("parallelFor gives every part the same range on every run", "[core][thread_pool]") {
    ThreadPool pool(3);
    const auto first = rangesOf(pool, 1001);
    for (int run = 0; run < 50; ++run) {
        REQUIRE(rangesOf(pool, 1001) == first);
    }
}

TEST_CASE("a minimum part size makes fewer parts for a short loop", "[core][thread_pool]") {
    ThreadPool pool(3);
    REQUIRE(pool.partCount(0) == 0);
    REQUIRE(pool.partCount(100) == 4);
    REQUIRE(pool.partCount(100, 30) == 3);
    REQUIRE(pool.partCount(100, 50) == 2);
    REQUIRE(pool.partCount(10, 50) == 1); // shorter than one part: still one
    REQUIRE(pool.partCount(10, 0) == 4);  // 0 counts as 1

    REQUIRE(rangesOf(pool, 100, 50) == std::vector<std::pair<std::size_t, std::size_t>>{ { 0, 50 }, { 50, 100 } });

    // One part runs on the calling thread.
    std::thread::id ranOn;
    pool.parallelFor(10, [&](std::size_t, std::size_t) { ranOn = std::this_thread::get_id(); }, 50);
    REQUIRE(ranOn == std::this_thread::get_id());
}

TEST_CASE("parallelFor calls nothing for no items", "[core][thread_pool]") {
    ThreadPool pool(2);
    bool called = false;
    pool.parallelFor(0, [&](std::size_t, std::size_t) { called = true; });
    REQUIRE_FALSE(called);
}

TEST_CASE("a pool without workers runs every part on the calling thread", "[core][thread_pool]") {
    ThreadPool pool(0);
    REQUIRE(pool.maxParts() == 1);
    std::set<std::thread::id> threads;
    std::size_t sum = 0;
    pool.parallelFor(100, [&](std::size_t start, std::size_t end) {
        threads.insert(std::this_thread::get_id());
        for (std::size_t i = start; i < end; ++i) {
            sum += i;
        }
    });
    REQUIRE(sum == 4950);
    REQUIRE(threads == std::set<std::thread::id>{ std::this_thread::get_id() });
}

TEST_CASE("parallelFor runs parts on the workers and the calling thread at once", "[core][thread_pool]") {
    ThreadPool pool(3);
    // Every part waits until all four have started: that only ends if four threads run them at once.
    std::atomic<int> started = 0;
    std::mutex mutex;
    std::set<std::thread::id> threads;
    pool.parallelFor(4, [&](std::size_t, std::size_t) {
        {
            const std::lock_guard lock(mutex);
            threads.insert(std::this_thread::get_id());
        }
        ++started;
        while (started.load() < 4) {
            std::this_thread::yield();
        }
    });
    REQUIRE(threads.size() == 4);
    REQUIRE(threads.count(std::this_thread::get_id()) == 1);
}

TEST_CASE("per-part sums give the same total as a plain loop", "[core][thread_pool]") {
    ThreadPool pool(3);
    std::vector<double> values(100000);
    for (std::size_t i = 0; i < values.size(); ++i) {
        values[i] = 0.5 * static_cast<double>(i % 97);
    }
    double expected = 0.0;
    for (const double v : values) {
        expected += v;
    }
    for (int run = 0; run < 20; ++run) {
        std::vector<double> sums(pool.maxParts(), 0.0);
        pool.parallelFor(values.size(), [&](std::size_t start, std::size_t end, std::size_t part) {
            for (std::size_t i = start; i < end; ++i) {
                sums[part] += values[i];
            }
        });
        double total = 0.0;
        for (const double s : sums) {
            total += s;
        }
        REQUIRE(total == expected); // halves of small integers add up exactly
    }
}

TEST_CASE("an exception in a part is thrown again after every other part ran", "[core][thread_pool]") {
    ThreadPool pool(3);
    std::vector<int> hits(8, 0);
    REQUIRE_THROWS_AS(
        pool.parallelFor(
            8,
            [&](std::size_t start, std::size_t end) {
                for (std::size_t i = start; i < end; ++i) {
                    ++hits[i];
                }
                if (start == 0) {
                    throw std::runtime_error("part 0 failed");
                }
            }
        ),
        std::runtime_error
    );
    REQUIRE(std::all_of(hits.begin(), hits.end(), [](int h) { return h == 1; }));

    // The pool still works afterwards.
    std::atomic<std::size_t> covered = 0;
    pool.parallelFor(100, [&](std::size_t start, std::size_t end) { covered += end - start; });
    REQUIRE(covered == 100);
}

TEST_CASE("an exception is thrown again also where the loop runs on the caller", "[core][thread_pool]") {
    ThreadPool pool(0);
    REQUIRE_THROWS_AS(
        pool.parallelFor(3, [](std::size_t, std::size_t) { throw std::logic_error("no"); }), std::logic_error
    );
}

TEST_CASE("a parallelFor inside a part runs on that part's thread", "[core][thread_pool]") {
    ThreadPool pool(3);
    std::vector<std::vector<int>> hits(4, std::vector<int>(50, 0));
    std::atomic<bool> innerOnOwnThread = true;
    pool.parallelFor(4, [&](std::size_t outer, std::size_t) {
        const std::thread::id self = std::this_thread::get_id();
        pool.parallelFor(50, [&](std::size_t start, std::size_t end) {
            if (std::this_thread::get_id() != self) {
                innerOnOwnThread = false;
            }
            for (std::size_t i = start; i < end; ++i) {
                ++hits[outer][i];
            }
        });
    });
    REQUIRE(innerOnOwnThread);
    for (const std::vector<int>& row : hits) {
        REQUIRE(std::all_of(row.begin(), row.end(), [](int h) { return h == 1; }));
    }

    // Outside of a part, loops use the workers again.
    std::mutex mutex;
    std::set<std::thread::id> threads;
    std::atomic<int> started = 0;
    pool.parallelFor(4, [&](std::size_t, std::size_t) {
        {
            const std::lock_guard lock(mutex);
            threads.insert(std::this_thread::get_id());
        }
        ++started;
        while (started.load() < 4) {
            std::this_thread::yield();
        }
    });
    REQUIRE(threads.size() == 4);
}

TEST_CASE("loops from several threads at once take turns", "[core][thread_pool]") {
    ThreadPool pool(2);
    constexpr std::size_t callers = 4;
    constexpr int loops = 200;
    std::vector<std::size_t> covered(callers, 0);
    std::vector<std::thread> threads;
    for (std::size_t c = 0; c < callers; ++c) {
        threads.emplace_back([&, c] {
            for (int loop = 0; loop < loops; ++loop) {
                std::atomic<std::size_t> items = 0;
                pool.parallelFor(37, [&](std::size_t start, std::size_t end) { items += end - start; });
                covered[c] += items;
            }
        });
    }
    for (std::thread& thread : threads) {
        thread.join();
    }
    for (const std::size_t items : covered) {
        REQUIRE(items == 37u * loops);
    }
}

TEST_CASE("many short loops in a row all complete", "[core][thread_pool]") {
    ThreadPool pool(3);
    std::vector<int> values(64, 0);
    for (int loop = 0; loop < 2000; ++loop) {
        pool.parallelFor(values.size(), [&](std::size_t start, std::size_t end) {
            for (std::size_t i = start; i < end; ++i) {
                ++values[i];
            }
        });
    }
    REQUIRE(std::all_of(values.begin(), values.end(), [](int v) { return v == 2000; }));
}

TEST_CASE("a pool can be destroyed right after it was made, and after use", "[core][thread_pool]") {
    { const ThreadPool pool(4); }
    {
        ThreadPool pool(4);
        std::atomic<std::size_t> covered = 0;
        pool.parallelFor(10, [&](std::size_t start, std::size_t end) { covered += end - start; });
        REQUIRE(covered == 10);
    }
}
