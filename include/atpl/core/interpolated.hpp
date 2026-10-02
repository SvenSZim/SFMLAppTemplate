#pragma once

#include "atpl/core/easing.hpp"

#include <algorithm>
#include <chrono>

namespace atpl {

/// Shared part of every animated value: duration, transition and the clock.
///
/// Animated values run on real time, not on frame count. `Derived` supplies `get()` and
/// `set()`. `Clock` exists so tests can drive time by hand; applications use the default.
template <class Derived, typename T, typename Clock = std::chrono::steady_clock>
class InterpolatedBase {
public:
    InterpolatedBase(TransitionFunction transition, float duration) :
        m_speed(duration > 0.0f ? 1.0f / duration : 0.0f),
        m_transition(transition) {}

    /// Length of a transition in seconds. 0 means the target is reached at once.
    [[nodiscard]] float duration() const { return m_speed > 0.0f ? 1.0f / m_speed : 0.0f; }

    void setDuration(float duration) { m_speed = duration > 0.0f ? 1.0f / duration : 0.0f; }

    [[nodiscard]] TransitionFunction getTransition() const { return m_transition; }

    void setTransition(TransitionFunction function) { m_transition = function; }

    [[nodiscard]] T get() const { return self().get(); }

    void set(const T& newValue) { self().set(newValue); }

    [[nodiscard]] operator T() const { return self().get(); }

    Derived& operator=(const T& newValue) {
        self().set(newValue);
        return self();
    }

protected:
    using TimePoint = typename Clock::time_point;

    [[nodiscard]] static TimePoint now() { return Clock::now(); }

    /// Seconds from `start` until now. Only the difference is converted to float, so precision
    /// does not degrade with the absolute clock value (for example after a long uptime).
    [[nodiscard]] static float secondsSince(TimePoint start) {
        return std::chrono::duration<float>(Clock::now() - start).count();
    }

    /// A start time far enough in the past that a transition of the current duration is over.
    [[nodiscard]] TimePoint finishedStartTime() const {
        const std::chrono::duration<float> offset(std::max(duration(), 1.0f));
        return now() - std::chrono::duration_cast<typename Clock::duration>(offset);
    }

    /// Whether a transition started at `start` is still under way.
    [[nodiscard]] bool isRunning(TimePoint start) const { return progress(start) < 1.0f; }

    /// Progress of a transition started at `start`: 0 at the start, 1 or more when it is over.
    [[nodiscard]] float progress(TimePoint start) const {
        if (m_speed <= 0.0f || m_transition == TransitionFunction::None) {
            return 1.0f;
        }
        return secondsSince(start) * m_speed;
    }

    float m_speed;
    TransitionFunction m_transition;

private:
    [[nodiscard]] Derived& self() { return static_cast<Derived&>(*this); }

    [[nodiscard]] const Derived& self() const { return static_cast<const Derived&>(*this); }
};

/// A value that moves to each newly set target over time instead of jumping.
///
/// Works for any `T` with `T - T`, `T + T` and `float * T`.
template <typename T, typename Clock = std::chrono::steady_clock>
class Interpolated : public InterpolatedBase<Interpolated<T, Clock>, T, Clock> {
public:
    using Base = InterpolatedBase<Interpolated<T, Clock>, T, Clock>;
    using Base::operator=;

    Interpolated(
        const T& initialValue = {}, TransitionFunction transition = TransitionFunction::Linear, float duration = 1.0f
    ) :
        Base(transition, duration),
        m_start(initialValue),
        m_end(initialValue),
        m_startTime(this->finishedStartTime()) {}

    /// Progress of the current transition: 0 at the start, 1 or more when it is over.
    [[nodiscard]] float status() const { return this->progress(m_startTime); }

    /// Whether the value is still moving. Use this to decide if a redraw is needed.
    [[nodiscard]] bool running() const { return this->isRunning(m_startTime); }

    /// The target the value is moving to (or resting at).
    [[nodiscard]] const T& target() const { return m_end; }

    [[nodiscard]] T get() const {
        const float t = status();
        if (t >= 1.0f) {
            return m_end;
        }
        const T delta = m_end - m_start;
        return static_cast<T>(m_start + getRatio(t, this->m_transition) * delta);
    }

    /// Starts a transition from the current value to `newValue`.
    void set(const T& newValue) {
        m_start = get();
        m_end = newValue;
        m_startTime = this->now();
    }

private:
    T m_start;
    T m_end;
    typename Base::TimePoint m_startTime;
};

} // namespace atpl
