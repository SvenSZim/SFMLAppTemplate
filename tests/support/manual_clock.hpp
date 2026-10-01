#pragma once

#include <chrono>

/// A clock that only moves when a test tells it to. Use it as the `Clock` of animated values.
struct ManualClock {
    using duration = std::chrono::nanoseconds;
    using rep = duration::rep;
    using period = duration::period;
    using time_point = std::chrono::time_point<ManualClock>;
    static constexpr bool is_steady = true;

    static inline time_point current{};

    static time_point now() { return current; }

    static void reset(std::chrono::seconds sinceEpoch = std::chrono::seconds(0)) { current = time_point(sinceEpoch); }

    static void advance(float seconds) {
        current += std::chrono::duration_cast<duration>(std::chrono::duration<float>(seconds));
    }
};
