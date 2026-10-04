#include "app/simulation_runner.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <atomic>
#include <chrono>
#include <cmath>
#include <functional>
#include <stdexcept>
#include <thread>

using namespace atpl;
using namespace std::chrono_literals;

namespace {

enum class Command { Add, Fail };

/// Counts what happens to it. Everything the test reads from the main thread is atomic.
class Counter final : public Simulation<int, Command> {
public:
    std::atomic<int> ticks{ 0 };
    mutable std::atomic<int> writes{ 0 }; // counted in writeState, which is const
    std::atomic<int> commands{ 0 };
    std::atomic<bool> slow{ false };
    std::atomic<bool> failInTick{ false };

private:
    void onCommand(const Command& command) override {
        if (command == Command::Fail) {
            throw std::runtime_error("a command went wrong");
        }
        ++commands;
    }
    void tick(float /*dt*/) override {
        if (failInTick) {
            throw std::runtime_error("a tick went wrong");
        }
        if (slow) {
            std::this_thread::sleep_for(30ms);
        }
        ++ticks;
    }
    void writeState(int& state) const override {
        state = ticks.load();
        ++writes;
    }
};

/// Waits until `done` holds, two seconds at most. Returns whether it did.
bool eventually(const std::function<bool()>& done) {
    const auto until = std::chrono::steady_clock::now() + 2s;
    while (!done()) {
        if (std::chrono::steady_clock::now() > until) {
            return false;
        }
        std::this_thread::sleep_for(1ms);
    }
    return true;
}

} // namespace

TEST_CASE("a running simulation ticks at its rate, in steps of a fixed size", "[app][simulation]") {
    Counter simulation;
    simulation.controls.tickRate = 200.0;
    app::SimulationRunner runner(simulation, nullptr);
    runner.start();
    std::this_thread::sleep_for(300ms);
    runner.stop();

    const int ticks = simulation.ticks;
    REQUIRE(ticks >= 20);  // 60 expected; a slow machine (or a sanitizer) may manage fewer
    REQUIRE(ticks <= 130); // but never more than real time allows
    REQUIRE(simulation.controls.tickCount.get() == ticks);
    REQUIRE(std::abs(simulation.controls.time.get() - ticks / 200.0) < 1e-6);
}

TEST_CASE("at speed zero no time passes", "[app][simulation]") {
    Counter simulation;
    simulation.controls.speed = 0.0;
    app::SimulationRunner runner(simulation, nullptr);
    runner.start();
    std::this_thread::sleep_for(100ms);
    runner.stop();
    REQUIRE(simulation.ticks == 0);
}

TEST_CASE("a paused simulation ticks no more, but still handles commands", "[app][simulation]") {
    Counter simulation;
    simulation.controls.tickRate = 500.0;
    app::SimulationRunner runner(simulation, nullptr);
    runner.start();
    REQUIRE(eventually([&] { return simulation.ticks > 5; }));

    simulation.controls.paused = true;
    std::this_thread::sleep_for(60ms); // it notices within 20 ms
    const int paused = simulation.ticks;
    std::this_thread::sleep_for(100ms);
    REQUIRE(simulation.ticks == paused);

    simulation.send(Command::Add);
    REQUIRE(eventually([&] { return simulation.commands == 1; }));
    runner.stop();
}

TEST_CASE("a paused simulation runs exactly the steps asked for; running, steps are ignored", "[app][simulation]") {
    Counter simulation;
    simulation.controls.paused = true;
    app::SimulationRunner runner(simulation, nullptr);
    runner.start();
    simulation.controls.step(3);
    REQUIRE(eventually([&] { return simulation.ticks == 3; }));
    std::this_thread::sleep_for(60ms);
    REQUIRE(simulation.ticks == 3);

    simulation.controls.tickRate = 1.0; // a tick a second: steps would show at once
    simulation.controls.paused = false;
    std::this_thread::sleep_for(40ms);
    simulation.controls.step(50);
    std::this_thread::sleep_for(100ms);
    runner.stop();
    REQUIRE(simulation.ticks < 10);
}

TEST_CASE("an unlimited simulation ticks as fast as it can", "[app][simulation]") {
    Counter simulation;
    simulation.controls.tickRate = 1.0;
    simulation.controls.unlimited = true;
    app::SimulationRunner runner(simulation, nullptr);
    runner.start();
    REQUIRE(eventually([&] { return simulation.ticks > 1000; }));
    runner.stop();
}

TEST_CASE("a simulation too slow for its rate drops what it could not do", "[app][simulation]") {
    Counter simulation;
    simulation.controls.tickRate = 1000.0;
    simulation.slow = true; // 30 ms per tick: far behind
    app::SimulationRunner runner(simulation, nullptr);
    runner.start();
    std::this_thread::sleep_for(600ms);
    simulation.slow = false;
    const auto since = std::chrono::steady_clock::now();
    const int before = simulation.ticks;
    std::this_thread::sleep_for(100ms);
    const int after = simulation.ticks;
    const auto waited = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - since);
    runner.stop();
    // Catching up on everything would take some 600 ticks more. What it may still do: what is
    // due while the test waits (a tick a millisecond; sleeps take longer on some systems), and
    // at most 0.25 s it was behind; nothing more.
    INFO("waited " << waited.count() << " ms");
    REQUIRE(after - before < static_cast<int>(waited.count()) + 251 + 50);
}

TEST_CASE("catching up never keeps a slow simulation from hearing that it is paused", "[app][simulation]") {
    Counter simulation;
    simulation.controls.tickRate = 1000.0;
    simulation.slow = true; // 30 ms per tick: hundreds of ticks behind at once
    app::SimulationRunner runner(simulation, nullptr);
    runner.start();
    std::this_thread::sleep_for(300ms);

    simulation.controls.paused = true;
    // Heard after the current pass of catching up (0.25 s at most) and one tick more.
    std::this_thread::sleep_for(500ms);
    const int paused = simulation.ticks;
    std::this_thread::sleep_for(200ms);
    runner.stop();
    REQUIRE(simulation.ticks == paused);
}

TEST_CASE("a state is written only when the main thread has taken the last one", "[app][simulation]") {
    Counter simulation;
    simulation.controls.unlimited = true;
    std::atomic<int> redraws{ 0 };
    app::SimulationRunner runner(simulation, [&redraws] { ++redraws; });
    runner.start();
    REQUIRE(eventually([&] { return simulation.ticks > 1000; }));
    REQUIRE(simulation.writes <= 2); // thousands of ticks, but nobody took a state

    runner.showNewestState(); // a pass of the main loop
    REQUIRE(eventually([&] { return simulation.writes >= 2; }));
    REQUIRE(redraws >= 1);
    runner.stop();

    // Stopping publishes the last state; the main thread sees it with its next pass.
    runner.showNewestState();
    REQUIRE(simulation.state() == simulation.ticks);
}

TEST_CASE("what the simulation throws ends its thread and is kept for the main thread", "[app][simulation]") {
    Counter simulation;
    simulation.controls.tickRate = 500.0;
    std::atomic<int> redraws{ 0 };
    app::SimulationRunner runner(simulation, [&redraws] { ++redraws; });
    runner.start();
    simulation.failInTick = true;
    REQUIRE(eventually([&] { return runner.failure() != nullptr; }));
    REQUIRE_THROWS_WITH(std::rethrow_exception(runner.failure()), "a tick went wrong");
    runner.stop(); // the thread has ended already

    Counter other;
    other.controls.paused = true;
    app::SimulationRunner second(other, nullptr);
    second.start();
    other.send(Command::Fail);
    REQUIRE(eventually([&] { return second.failure() != nullptr; }));
    REQUIRE_THROWS_WITH(std::rethrow_exception(second.failure()), "a command went wrong");
}
