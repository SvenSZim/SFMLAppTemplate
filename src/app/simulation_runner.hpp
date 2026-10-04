#pragma once

#include "atpl/app/simulation.hpp"

#include <atomic>
#include <exception>
#include <functional>
#include <thread>

namespace atpl::app {

/// Drives a simulation on a thread of its own (ARCHITECTURE.md 5.1, D62).
///
/// One pass of the thread: handle the commands that are waiting (sleeping until one arrives or
/// the next tick is due, 20 ms at most); unless paused, run the ticks that are due; write and
/// publish a state if the main thread has taken the last one, and ask for a frame.
/// - Ticks have a fixed size, `1 / tickRate` seconds of simulated time. They are due as real time
///   passes, `speed` times as fast; `unlimited` runs them back to back.
/// - A simulation that falls more than 0.25 s of real time behind drops what it could not do,
///   and a pass never spends more than 0.25 s catching up: it slows down instead of freezing,
///   and pausing, steps and commands are heard after one more tick at most.
/// - Paused, it only handles commands and runs the ticks asked for with `step`; `step` while
///   running is ignored.
/// - A state is written at most once per frame the main thread takes, and once more when the
///   simulation pauses or stops, so the last state is always shown.
/// - What the simulation throws ends the thread; `failure()` holds it for the main thread.
class SimulationRunner {
public:
    /// `redraw` is called on the simulation thread whenever there is something new to show.
    SimulationRunner(SimulationBase& simulation, std::function<void()> redraw);
    ~SimulationRunner();

    SimulationRunner(const SimulationRunner&) = delete;
    SimulationRunner& operator=(const SimulationRunner&) = delete;

    /// Starts the thread: `start()`, then the passes above.
    void start();

    /// Stops the thread after its current tick and waits for it. The last state is published.
    void stop();

    /// Main thread, once at the start of each pass of the application's loop: the newest
    /// published state becomes what `Simulation::state()` returns.
    void showNewestState();

    /// What the simulation threw, if it threw; then the thread has ended.
    [[nodiscard]] std::exception_ptr failure() const;

private:
    void run();

    SimulationBase* m_simulation;
    std::function<void()> m_redraw;
    std::thread m_thread;
    std::atomic<bool> m_stopping{ false };
    std::atomic<bool> m_failed{ false };
    std::exception_ptr m_failure; ///< Written by the thread before `m_failed` is set.
};

} // namespace atpl::app
