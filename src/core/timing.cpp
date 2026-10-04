#include "atpl/core/timing.hpp"

#include <algorithm>
#include <cmath>

namespace atpl {

Stopwatch::Stopwatch() :
    m_start(Clock::now()) {}

double Stopwatch::seconds() const {
    return std::chrono::duration<double>(Clock::now() - m_start).count();
}

double Stopwatch::milliseconds() const {
    return std::chrono::duration<double, std::milli>(Clock::now() - m_start).count();
}

double Stopwatch::restart() {
    const Clock::time_point now = Clock::now();
    const double passed = std::chrono::duration<double>(now - m_start).count();
    m_start = now;
    return passed;
}

Cooldown::Cooldown(double period) :
    m_period(period) {}

int Cooldown::advance(double seconds) {
    if (m_period <= 0.0) {
        return 1;
    }
    m_elapsed += std::max(seconds, 0.0);
    if (m_elapsed < m_period) {
        return 0;
    }
    const double ended = std::floor(m_elapsed / m_period);
    m_elapsed -= ended * m_period;
    m_elapsed = std::clamp(m_elapsed, 0.0, m_period); // rounding may leave it a hair outside
    if (m_elapsed >= m_period) {
        m_elapsed = 0.0;
    }
    return static_cast<int>(ended);
}

double Cooldown::progress() const {
    return m_period <= 0.0 ? 1.0 : m_elapsed / m_period;
}

void Cooldown::reset() {
    m_elapsed = 0.0;
}

double Cooldown::period() const {
    return m_period;
}

void Cooldown::setPeriod(double period) {
    m_period = period;
}

RunningAverage::RunningAverage(std::size_t window) :
    m_values(std::max<std::size_t>(window, 1), 0.0) {}

void RunningAverage::add(double value) {
    if (m_count == m_values.size()) {
        m_sum -= m_values[m_next];
    } else {
        ++m_count;
    }
    m_values[m_next] = value;
    m_sum += value;
    m_next = (m_next + 1) % m_values.size();
    if (m_next == 0) {
        // Once per window: the sum afresh, so that rounding errors do not add up.
        m_sum = 0.0;
        for (std::size_t i = 0; i < m_count; ++i) {
            m_sum += m_values[i];
        }
    }
}

double RunningAverage::average() const {
    return m_count == 0 ? 0.0 : m_sum / static_cast<double>(m_count);
}

std::size_t RunningAverage::count() const {
    return m_count;
}

std::size_t RunningAverage::window() const {
    return m_values.size();
}

bool RunningAverage::full() const {
    return m_count == m_values.size();
}

void RunningAverage::clear() {
    m_next = 0;
    m_count = 0;
    m_sum = 0.0;
}

ScopedTimer::ScopedTimer(Series& series) :
    m_series(&series) {}

ScopedTimer::ScopedTimer(RunningAverage& average) :
    m_average(&average) {}

ScopedTimer::~ScopedTimer() {
    const double passed = m_stopwatch.milliseconds();
    if (m_series != nullptr) {
        m_series->push(static_cast<float>(passed));
    }
    if (m_average != nullptr) {
        m_average->add(passed);
    }
}

double ScopedTimer::milliseconds() const {
    return m_stopwatch.milliseconds();
}

} // namespace atpl
