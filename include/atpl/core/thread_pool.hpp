#pragma once

#include <condition_variable>
#include <cstddef>
#include <exception>
#include <mutex>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace atpl {

/// Splits a loop over many items across the cores, for the heavy work inside a simulation tick.
///
///     ThreadPool pool;                       // a member of the simulation, made once
///
///     pool.parallelFor(ants.size(), [&](std::size_t start, std::size_t end) {
///         for (std::size_t i = start; i < end; ++i) {
///             ants[i].update(dt);
///         }
///     });                                    // returns when every ant is updated
///
/// `count` is cut into contiguous ranges, one per part. The thread that calls `parallelFor` works
/// on parts too, so a pool with `n` workers keeps `n + 1` cores busy. By default it has two
/// workers fewer than the machine has hardware threads: with the caller, one core stays free
/// for the main thread (P8). On a machine with one or two hardware threads it has no workers,
/// and `parallelFor` runs the loop on the calling thread.
///
/// The ranges depend only on `count`, the number of workers and `minPartSize`, never on timing:
/// part `i` always covers the same items, whichever thread runs it. A callback that also takes
/// the part index can keep one accumulator or one random generator per part and get the same
/// result on every run:
///
///     std::vector<double> sums(pool.maxParts());
///     pool.parallelFor(values.size(), [&](std::size_t start, std::size_t end, std::size_t part) {
///         for (std::size_t i = start; i < end; ++i) {
///             sums[part] += values[i];
///         }
///     });
///
/// Idle workers sleep and use no processor time.
///
/// Thread safety: any thread may call `parallelFor`; calls from different threads take turns.
/// A `parallelFor` inside a callback runs its whole loop on the calling thread, so nested loops
/// cannot wait for each other. If a callback throws, the other parts still run, and the first
/// exception is thrown again from `parallelFor`.
class ThreadPool {
public:
    /// A pool with `defaultWorkerCount()` workers.
    ThreadPool();

    /// A pool with exactly `workers` worker threads (0 is allowed: every loop runs on the caller).
    explicit ThreadPool(std::size_t workers);

    /// Waits for a running `parallelFor` to finish, then stops the workers.
    ~ThreadPool();

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool(ThreadPool&&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    ThreadPool& operator=(ThreadPool&&) = delete;

    /// Two fewer than the hardware threads, at least 0: with the calling thread, one core stays
    /// free for the main thread (P8).
    [[nodiscard]] static std::size_t defaultWorkerCount();

    /// The number of worker threads.
    [[nodiscard]] std::size_t workerCount() const;

    /// The most parts a loop is cut into: one per worker and one for the calling thread.
    /// Size per-part data with this.
    [[nodiscard]] std::size_t maxParts() const;

    /// The number of parts a loop over `count` items is cut into: `maxParts()`, but no more than
    /// leaves every part `minPartSize` items (at least one part when `count` is not 0).
    [[nodiscard]] std::size_t partCount(std::size_t count, std::size_t minPartSize = 1) const;

    /// Calls `fn(start, end)` or `fn(start, end, part)` once for every part of the items
    /// `0 .. count - 1`, and returns when all calls have returned. The parts are contiguous, in
    /// order (part 0 starts at 0) and differ in size by at most one item. A part of fewer than
    /// `minPartSize` items is not made: a short loop gets fewer parts or runs on the caller alone.
    /// Nothing is called when `count` is 0.
    template <typename Fn>
    void parallelFor(std::size_t count, Fn&& fn, std::size_t minPartSize = 1);

private:
    /// One loop, as the workers see it: how many parts, and what to call for each.
    struct Job {
        std::size_t parts = 0;
        void (*call)(void* context, std::size_t part) = nullptr;
        void* context = nullptr;
    };

    void run(const Job& job);
    void workOn(const Job& job);
    void workerLoop();

    std::vector<std::thread> m_workers;

    std::mutex m_callMutex; ///< Lets calls from different threads take turns.

    // Guarded by m_mutex.
    std::mutex m_mutex;
    std::condition_variable m_wake; ///< A job was posted, or the pool stops.
    std::condition_variable m_left; ///< A worker is done with the current job.
    const Job* m_job = nullptr;     ///< The job workers may still join; null when there is none.
    std::size_t m_generation = 0;   ///< Grows with every job, so a worker joins each job once.
    std::size_t m_joined = 0;       ///< Workers that took the current job and have not left it.
    std::size_t m_nextPart = 0;     ///< The next part nobody has taken yet.
    std::exception_ptr m_failure;   ///< The first exception of the current job.
    bool m_stopping = false;
};

template <typename Fn>
void ThreadPool::parallelFor(std::size_t count, Fn&& fn, std::size_t minPartSize) {
    constexpr bool withPart = std::is_invocable_v<Fn&, std::size_t, std::size_t, std::size_t>;
    static_assert(
        withPart || std::is_invocable_v<Fn&, std::size_t, std::size_t>,
        "parallelFor needs fn(start, end) or fn(start, end, part)"
    );

    const std::size_t parts = partCount(count, minPartSize);
    if (parts == 0) {
        return;
    }
    struct Context {
        Fn& fn;
        std::size_t count;
        std::size_t parts;
    } context{ fn, count, parts };

    // Part i covers [count * i / parts, count * (i + 1) / parts): sizes differ by at most one.
    const Job job{ parts,
                   [](void* opaque, std::size_t part) {
                       Context& c = *static_cast<Context*>(opaque);
                       const std::size_t start = c.count * part / c.parts;
                       const std::size_t end = c.count * (part + 1) / c.parts;
                       if constexpr (withPart) {
                           c.fn(start, end, part);
                       } else {
                           c.fn(start, end);
                       }
                   },
                   &context };
    run(job);
}

} // namespace atpl
