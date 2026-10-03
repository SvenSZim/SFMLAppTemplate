#pragma once

#include "atpl/core/param.hpp"
#include "atpl/core/queue.hpp"
#include "atpl/core/snapshot.hpp"

#include <atomic>
#include <chrono>
#include <utility>
#include <variant>
#include <vector>

namespace atpl {

namespace app {
class SimulationRunner; // drives a simulation on its own thread; internal
}

// A simulation runs on its own thread, so that neither it nor the UI ever waits for the other.
// The two threads share exactly three things, and this header sets all three up:
//
//   parameters   values both sides read and write          Param<T>     (yours, and the controls below)
//   commands     things the application tells the simulation   send() -> onCommand()
//   state        what the main thread draws                writeState() -> state()
//
// An application's simulation derives from `Simulation<State, Command>` and fills in three
// functions. `App::run(simulation)` does the rest.

/// How a running simulation is steered, and how it is doing. Everything here is a `Param`, so a
/// widget can be bound to it directly:
///
///     Switch("Pause", simulation.controls.paused)
///     Slider("Speed", simulation.controls.speed, {.min = 0.25, .max = 8.0})
///     ValueDisplay("Ticks/s", simulation.controls.ticksPerSecond, {.format = "{:.0f}"})
class SimulationControls {
public:
    // Steering. Written by the application or by widgets, from any thread.

    /// While true, no ticks happen. Commands are still handled.
    Param<bool> paused = false;

    /// Ticks per second of simulated time: every tick advances the simulation by `1 / tickRate`
    /// seconds. This fixes the step size, which keeps a simulation reproducible.
    Param<double> tickRate = 60.0;

    /// How fast simulated time passes compared to real time. 2 means twice as many ticks per real
    /// second; the step size of a tick does not change.
    Param<double> speed = 1.0;

    /// While true, ticks follow each other without waiting, as fast as the machine allows.
    /// For training runs and searches. `speed` is ignored.
    Param<bool> unlimited = false;

    /// Runs this many ticks although the simulation is paused, then stays paused.
    void step(int ticks = 1);

    // Measurements. Written by the simulation thread, for display.

    Param<double> ticksPerSecond;   ///< Ticks per real second, measured.
    Param<double> tickMilliseconds; ///< How long one tick takes, averaged.
    Param<double> tickCount;        ///< Ticks since the start.
    Param<double> time;             ///< Simulated seconds since the start.

private:
    friend class app::SimulationRunner;
    std::atomic<int> m_pendingSteps{ 0 };
};

/// What `App::run` needs from a simulation. Applications derive from `Simulation<State, Command>`
/// below, not from this.
class SimulationBase {
public:
    SimulationControls controls;

    virtual ~SimulationBase() = default;

    SimulationBase() = default;
    SimulationBase(const SimulationBase&) = delete;
    SimulationBase& operator=(const SimulationBase&) = delete;

private:
    friend class app::SimulationRunner;

    // Called on the simulation thread.
    virtual void runnerStart() = 0;
    /// Handles the commands that are waiting; if there are none, waits for one up to `wait`.
    virtual void runnerHandleCommands(std::chrono::milliseconds wait) = 0;
    virtual void runnerTick(float dt) = 0;
    virtual void runnerPublish() = 0;

    // Called on the main thread, once at the start of each pass of the application's loop.
    virtual void runnerShowNewestState() = 0;
};

/// The base class of an application's simulation.
///
/// `State` is everything the main thread needs to draw one frame. `Command` is what the
/// application can tell the simulation to do, usually an enum or a variant; leave it out if there
/// are no commands.
///
///     enum class Command { Reset };
///     struct World { std::vector<Particle> particles; };
///
///     class Particles : public atpl::Simulation<World, Command> {
///         void onCommand(const Command& command) override { ... }
///         void tick(float dt) override { ... }
///         void writeState(World& out) const override { out.particles = m_particles; }
///     };
///
/// Which function runs where:
/// - `start`, `onCommand`, `tick`, `writeState`: on the simulation thread, one at a time.
///   They may use the simulation's members freely.
/// - `send`: from any thread.
/// - `state`: on the main thread only.
/// Anything else the two threads share must be a `Param`.
template <typename State, typename Command = std::monostate>
class Simulation : public SimulationBase {
public:
    Simulation() = default;

    /// Starts all buffers of the published state as copies of `initial`.
    explicit Simulation(const State& initial) :
        m_snapshot(initial) {}

    /// Tells the simulation to do something. It is handled before the next tick, or at once if the
    /// simulation is paused. From any thread.
    void send(Command command) { m_commands.push(std::move(command)); }

    /// The state to draw: the newest one the simulation had published when the current pass of
    /// the application's loop began. It stays the same for the whole pass, so every view drawn in
    /// one frame shows the same moment. Main thread only.
    [[nodiscard]] const State& state() const { return *m_shown; }

protected:
    /// Called once before the first tick.
    virtual void start() {}

    /// Handles one command sent with `send`.
    virtual void onCommand(const Command& /*command*/) {}

    /// Advances the simulation by `dt` seconds of simulated time.
    virtual void tick(float dt) = 0;

    /// Writes everything the main thread needs for drawing into `state`. Write all of it every
    /// time: `state` holds an older state, not the last one written.
    ///
    /// Not called after every tick: at most once per frame the main thread draws, and once more
    /// when the simulation pauses or stops, so the last state is always shown.
    virtual void writeState(State& state) const = 0;

private:
    void runnerStart() final { start(); }

    void runnerHandleCommands(std::chrono::milliseconds wait) final {
        m_pending.clear();
        if (wait > std::chrono::milliseconds::zero()) {
            m_commands.waitDrain(m_pending, wait);
        } else {
            m_commands.drain(m_pending);
        }
        for (const Command& command : m_pending) {
            onCommand(command);
        }
    }

    void runnerTick(float dt) final { tick(dt); }

    void runnerPublish() final {
        writeState(m_snapshot.writeBuffer());
        m_snapshot.publish();
    }

    void runnerShowNewestState() final { m_shown = &m_snapshot.read(); }

    Queue<Command> m_commands;
    Snapshot<State> m_snapshot;
    const State* m_shown = &m_snapshot.read(); // what state() returns; replaced once per pass
    std::vector<Command> m_pending;            // kept between ticks so handling commands does not allocate
};

} // namespace atpl
