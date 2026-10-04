#pragma once

#include "atpl/core/series.hpp"

#include <chrono>
#include <cstddef>
#include <vector>

namespace atpl {

/// Measures how much time passed since it was started, on a clock that never jumps.
///
///     Stopwatch stopwatch;
///     loadLevel();
///     log.push(std::format("loaded in {:.1f} ms", stopwatch.milliseconds()));
///
/// Thread safety: several threads may read it while none restarts it.
class Stopwatch {
public:
    using Clock = std::chrono::steady_clock;

    /// Starts now.
    Stopwatch();

    /// Seconds since the start.
    [[nodiscard]] double seconds() const;

    /// Milliseconds since the start.
    [[nodiscard]] double milliseconds() const;

    /// Starts again now, and returns the seconds until now.
    double restart();

private:
    Clock::time_point m_start;
};

/// Something that should happen every `period` seconds, on the time it is given.
///
///     Cooldown spawn(0.5);                     // every half second
///     if (spawn.advance(dt)) { spawnFood(); }  // in the tick
///
/// It reads no clock: given the tick's `dt` it follows simulated time, so it is deterministic and
/// pauses with the simulation; given a frame's time it follows the wall clock. Time left over
/// after a period counts towards the next one, so it does not drift.
///
/// Thread safety: none; it belongs to one thread.
class Cooldown {
public:
    /// Fires every `period` seconds; a period of 0 or less fires at every `advance`.
    explicit Cooldown(double period);

    /// Lets `seconds` pass, and returns how many periods ended in them (0, 1, or more after a
    /// long pause). A period of 0 or less returns 1.
    int advance(double seconds);

    /// How far the current period is, from 0 to 1, for a progress bar.
    [[nodiscard]] double progress() const;

    /// Starts the current period again.
    void reset();

    [[nodiscard]] double period() const;

    /// Changes the period; the time passed in the current one is kept.
    void setPeriod(double period);

private:
    double m_period;
    double m_elapsed = 0.0; ///< Seconds into the current period.
};

/// The average of the newest `window` values added: "over the last 60 ticks" means exactly that.
///
///     RunningAverage tickTimes(60);
///     tickTimes.add(stopwatch.milliseconds());
///     shown = tickTimes.average();
///
/// The sum is kept as a `double` and computed afresh once per window, so rounding does not build
/// up however long it runs.
///
/// Thread safety: none; it belongs to the thread that adds to it. To show it in the UI, write
/// its average into a `Param`.
class RunningAverage {
public:
    /// Averages the newest `window` values; a window of 0 counts as 1.
    explicit RunningAverage(std::size_t window);

    /// Adds a value; once the window is full, the oldest drops out.
    void add(double value);

    /// The average of the values in the window, or 0 if there are none.
    [[nodiscard]] double average() const;

    /// How many values are in the window, at most `window()`.
    [[nodiscard]] std::size_t count() const;

    [[nodiscard]] std::size_t window() const;

    /// Whether the window holds `window()` values.
    [[nodiscard]] bool full() const;

    /// Forgets every value.
    void clear();

private:
    std::vector<double> m_values;
    std::size_t m_next = 0;  ///< Where the next value goes.
    std::size_t m_count = 0; ///< Values in the window.
    double m_sum = 0.0;
};

/// Measures from where it is made to where it ends, and writes the milliseconds into a target.
///
///     {
///         ScopedTimer timer(pathfindingTimes);   // a Series that a graph shows
///         findPaths();
///     }                                          // one sample pushed here
///
/// Thread safety: like its target. A `Series` may be shown by a graph on the main thread while
/// the simulation pushes; a `RunningAverage` belongs to the thread that times.
class ScopedTimer {
public:
    /// Pushes the milliseconds into `series` at the end.
    explicit ScopedTimer(Series& series);

    /// Adds the milliseconds to `average` at the end.
    explicit ScopedTimer(RunningAverage& average);

    /// Writes the milliseconds.
    ~ScopedTimer();

    ScopedTimer(const ScopedTimer&) = delete;
    ScopedTimer(ScopedTimer&&) = delete;
    ScopedTimer& operator=(const ScopedTimer&) = delete;
    ScopedTimer& operator=(ScopedTimer&&) = delete;

    /// The milliseconds so far.
    [[nodiscard]] double milliseconds() const;

private:
    Stopwatch m_stopwatch;
    Series* m_series = nullptr;
    RunningAverage* m_average = nullptr;
};

} // namespace atpl
