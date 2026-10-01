// How the core API reads in an application.
//
// This file is compiled with every build but never linked or run. It keeps the usage shown in the
// documentation honest: if the API changes in a way that breaks it, the build fails.

#include "atpl/core/param.hpp"
#include "atpl/core/queue.hpp"
#include "atpl/core/series.hpp"
#include "atpl/core/snapshot.hpp"

#include <algorithm>
#include <array>
#include <string>
#include <vector>

namespace {

enum class Mode { Normal, Debug, Wireframe };

// Values the UI edits and the simulation reads, or the other way round.
struct Params {
    atpl::Param<float> speed = 5.f;
    atpl::Param<int> particleCount = 1000;
    atpl::Param<bool> gravity = true;
    atpl::Param<Mode> mode = Mode::Normal;
    atpl::Param<std::string> status;
};

enum class Command { Reset, SpawnBurst };

struct Particle {
    float x = 0.f;
    float y = 0.f;
};

// Everything the main thread needs to draw one frame.
struct World {
    std::vector<Particle> particles;
    float time = 0.f;
};

struct Simulation {
    Params& params;
    atpl::Queue<Command>& commands;
    atpl::Snapshot<World>& snapshot;
    atpl::Series& tickTimes;

    std::vector<Command> pending; // kept between ticks so draining does not allocate
    float time = 0.f;

    // Runs on the simulation thread.
    void tick(float dt) {
        commands.drain(pending);
        for (const Command command : pending) {
            if (command == Command::Reset) {
                time = 0.f;
            }
        }
        pending.clear();

        // Parameters read like plain values.
        const float step = params.gravity ? dt * params.speed : 0.f;
        time += step;
        if (params.mode == Mode::Debug) {
            params.status = "t = " + std::to_string(time);
        }

        // Publish the whole state for the main thread.
        World& next = snapshot.writeBuffer();
        next.time = time;
        next.particles.resize(static_cast<std::size_t>(std::max(params.particleCount.get(), 0)));
        snapshot.publish();

        tickTimes.push(dt * 1000.f);
    }
};

// Runs on the main thread, once per frame.
[[maybe_unused]] float mainThreadFrame(
    Params& params,
    atpl::Queue<Command>& commands,
    atpl::Snapshot<World>& snapshot,
    const atpl::Series& tickTimes,
    atpl::Revision& lastSeenSpeed
) {
    // Writing a parameter, as a slider would.
    params.speed = 7.5f;

    // Sending a command, as a button handler would.
    commands.push(Command::Reset);

    // Doing work only when something changed.
    if (params.speed.revision() != lastSeenSpeed) {
        lastSeenSpeed = params.speed.revision();
    }

    // Reading graph data.
    std::array<float, 240> window{};
    const std::size_t count = tickTimes.read(window);

    // Drawing the newest complete state.
    const World& world = snapshot.read();
    return world.time + static_cast<float>(count + world.particles.size());
}

[[maybe_unused]] void wiring() {
    Params params;
    atpl::Queue<Command> commands;
    atpl::Snapshot<World> snapshot;
    atpl::Series tickTimes(240);

    Simulation simulation{params, commands, snapshot, tickTimes, {}, 0.f};
    simulation.tick(1.f / 60.f);

    atpl::Revision lastSeenSpeed = 0;
    (void)mainThreadFrame(params, commands, snapshot, tickTimes, lastSeenSpeed);
}

} // namespace
