#include "app/simulation_runner.hpp"

#include <algorithm>
#include <chrono>
#include <utility>

namespace atpl::app {

namespace {

using Clock = std::chrono::steady_clock;
using Seconds = std::chrono::duration<double>;

/// The longest a pass sleeps waiting for commands: how soon pausing, stepping and stopping take
/// effect.
constexpr std::chrono::microseconds longestWait = std::chrono::milliseconds(20);

/// How far a simulation may fall behind real time before it drops what it could not do, and how
/// long a pass may spend catching up.
constexpr double mostBehind = 0.25;

} // namespace

} // namespace atpl::app

namespace atpl {

void SimulationControls::step(int ticks) {
    if (ticks > 0) {
        m_pendingSteps.fetch_add(ticks, std::memory_order_acq_rel);
    }
}

} // namespace atpl

namespace atpl::app {

SimulationRunner::SimulationRunner(SimulationBase& simulation, std::function<void()> redraw) :
    m_simulation(&simulation),
    m_redraw(std::move(redraw)) {}

SimulationRunner::~SimulationRunner() {
    stop();
}

void SimulationRunner::start() {
    m_stopping.store(false, std::memory_order_release);
    m_thread = std::thread([this] { run(); });
}

void SimulationRunner::stop() {
    m_stopping.store(true, std::memory_order_release);
    if (m_thread.joinable()) {
        m_thread.join();
    }
}

void SimulationRunner::showNewestState() {
    m_simulation->runnerShowNewestState();
}

std::exception_ptr SimulationRunner::failure() const {
    return m_failed.load(std::memory_order_acquire) ? m_failure : nullptr;
}

void SimulationRunner::run() {
    SimulationBase& simulation = *m_simulation;
    SimulationControls& controls = simulation.controls;
    const auto stopping = [this] { return m_stopping.load(std::memory_order_acquire); };
    const auto publish = [&](bool always) {
        if (simulation.runnerPublish(always) && m_redraw) {
            m_redraw();
        }
    };

    try {
        simulation.runnerStart();
        publish(true);

        Clock::time_point last = Clock::now();
        double owed = 0.0; // simulated seconds that are due and not yet ticked
        double simulated = 0.0;
        double ticks = 0.0;
        bool wasPaused = false;

        // Measured over about a second of real time.
        Clock::time_point measuredSince = last;
        int ticksMeasured = 0;
        Seconds tickTime{ 0.0 };

        const auto tickOnce = [&](double dt) {
            const Clock::time_point begin = Clock::now();
            simulation.runnerTick(static_cast<float>(dt));
            tickTime += Clock::now() - begin;
            ++ticksMeasured;
            simulated += dt;
            ticks += 1.0;
            controls.time = simulated;
            controls.tickCount = ticks;
        };

        while (!stopping()) {
            const double rate = std::max(controls.tickRate.get(), 1.0e-6);
            const double dt = 1.0 / rate;
            const bool paused = controls.paused.get();
            const bool unlimited = controls.unlimited.get();
            const double speed = std::max(controls.speed.get(), 0.0);

            if (paused) {
                // Only commands, and the ticks asked for. Wakes at least every 20 ms.
                const int steps = controls.m_pendingSteps.exchange(0, std::memory_order_acq_rel);
                const std::size_t handled =
                    simulation.runnerHandleCommands(steps > 0 ? std::chrono::microseconds::zero() : longestWait);
                for (int i = 0; i < steps && !stopping(); ++i) {
                    tickOnce(dt);
                }
                // Pausing, a step or a command: the state the user sees has to say so.
                if (!wasPaused || steps > 0 || handled > 0) {
                    publish(true);
                }
                wasPaused = true;
                owed = 0.0;
                last = Clock::now();
            } else {
                controls.m_pendingSteps.store(0, std::memory_order_release); // stepping is for a paused simulation
                wasPaused = false;
                if (unlimited) {
                    simulation.runnerHandleCommands(std::chrono::microseconds::zero());
                    tickOnce(dt);
                    last = Clock::now();
                } else {
                    // Sleep until the next tick is due (or a command arrives), 20 ms at most.
                    const double untilDue = speed > 0.0 ? (dt - owed) / speed : 1.0;
                    const auto wait = std::clamp(
                        std::chrono::duration_cast<std::chrono::microseconds>(Seconds(untilDue)),
                        std::chrono::microseconds::zero(),
                        longestWait
                    );
                    simulation.runnerHandleCommands(wait);

                    const Clock::time_point now = Clock::now();
                    owed += Seconds(now - last).count() * speed;
                    last = now;
                    // Too far behind: what could not be done is dropped. Catching up never takes
                    // longer than that either: with slow ticks, a pass that has ticked for 0.25 s
                    // drops the rest, so the simulation slows down instead of freezing, and
                    // commands and pausing are heard after one more tick at most.
                    owed = std::min(owed, mostBehind * speed + dt);
                    const Clock::time_point catchingUpSince = Clock::now();
                    while (owed >= dt && !stopping()) {
                        tickOnce(dt);
                        owed -= dt;
                        if (Seconds(Clock::now() - catchingUpSince).count() >= mostBehind) {
                            owed = std::min(owed, dt); // the rest could not be done
                            break;
                        }
                    }
                }
                publish(false);
            }

            // The measurements, about once a second.
            const Clock::time_point now = Clock::now();
            const double elapsed = Seconds(now - measuredSince).count();
            if (elapsed >= 1.0) {
                controls.ticksPerSecond = static_cast<double>(ticksMeasured) / elapsed;
                controls.tickMilliseconds = ticksMeasured > 0 ? tickTime.count() * 1000.0 / ticksMeasured : 0.0;
                measuredSince = now;
                ticksMeasured = 0;
                tickTime = Seconds(0.0);
            }
        }
        publish(true); // the last state is shown
    } catch (...) {
        m_failure = std::current_exception();
        m_failed.store(true, std::memory_order_release);
        if (m_redraw) {
            m_redraw(); // wakes the main loop, which ends the run
        }
    }
}

} // namespace atpl::app
