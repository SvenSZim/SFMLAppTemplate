// A whole application: parameters, a simulation on its own thread, a UI, a camera.
//
// Compiled with every build, never linked or run. See core_usage.cpp for why the namespace is named.

#include "atpl/app/app.hpp"
#include "atpl/app/camera.hpp"

#include <SFML/Graphics/CircleShape.hpp>

#include <vector>

namespace app_usage {

using namespace atpl;

// What the user can change. Read by the simulation, written by widgets.
struct Params {
    Param<float> gravity = 9.81f;
    Param<int> particleCount = 2000;
};

// What the application can tell the simulation to do.
enum class Command { Reset, Burst };

// What the main thread needs to draw a frame.
struct World {
    std::vector<sf::Vector2f> positions;
};

// The simulation: three functions, all called on the simulation thread.
class Particles final : public Simulation<World, Command> {
public:
    explicit Particles(Params& params) :
        m_params(params) {}

private:
    void onCommand(const Command& command) override {
        if (command == Command::Reset) {
            m_positions.clear();
            m_velocities.clear();
        }
    }

    void tick(float dt) override {
        const auto count = static_cast<std::size_t>(m_params.particleCount.get());
        m_positions.resize(count);
        m_velocities.resize(count);
        for (std::size_t i = 0; i < count; ++i) {
            m_velocities[i].y += m_params.gravity * dt;
            m_positions[i] += m_velocities[i] * dt;
        }
    }

    void writeState(World& state) const override { state.positions = m_positions; }

    Params& m_params;
    std::vector<sf::Vector2f> m_positions;
    std::vector<sf::Vector2f> m_velocities;
};

int main() {
    Params params;
    Particles simulation(params);
    Camera camera("world");

    App app({
        .window = {.title = "Particles", .size = { 1600u, 900u }, .minimumSize = { 640u, 480u }},
        .ui = {
            .background = "world",
            .panels = {
                {
                    .name = "Simulation",
                    .placement = Anchor::TopLeft,
                    .widgets = {
                        // The simulation's controls are parameters, so widgets bind to them directly.
                        Switch("Pause", simulation.controls.paused),
                        Button("Step"),
                        Slider("Speed", simulation.controls.speed, {.min = 0.25, .max = 8.0}),
                        Switch("Unlimited", simulation.controls.unlimited),
                        ValueDisplay("Ticks/s", simulation.controls.ticksPerSecond, {.format = "{:.0f}"}),
                        ValueDisplay("Tick (ms)", simulation.controls.tickMilliseconds, {.format = "{:.2f}"}),
                    },
                },
                {
                    .name = "Particles",
                    .placement = Anchor::TopRight,
                    .widgets = {
                        Slider("Count", params.particleCount, {.min = 0.0, .max = 20000.0, .step = 500.0}),
                        Slider("Gravity", params.gravity, {.min = 0.0, .max = 30.0}),
                        Button("Reset"),
                        Button("Burst"),
                    },
                },
            },
        },
    });

    // Drawing: on the main thread, from the newest state the simulation published.
    app.ui().view("world").onDraw([&](sf::RenderTarget& target, sf::Vector2f size) {
        camera.apply(target, size);
        sf::CircleShape dot(2.f);
        for (const sf::Vector2f position : simulation.state().positions) {
            dot.setPosition(position);
            target.draw(dot);
        }
    });

    // Events: on the main thread. The application decides what the simulation hears about.
    app.onEvent([&](const Event& event) {
        if (event.isButton("Reset")) {
            simulation.send(Command::Reset);
        }
        if (event.isButton("Burst")) {
            simulation.send(Command::Burst);
        }
        if (event.isButton("Step")) {
            simulation.controls.step();
        }
        if (event.isKey(sf::Keyboard::Key::Space)) {
            simulation.controls.paused = !simulation.controls.paused;
        }
        if (event.isKey(sf::Keyboard::Key::Escape)) {
            app.quit();
        }
        if (camera.handle(event)) {
            app.ui().requestRedraw();
        }
    });

    return app.run(simulation);
}

} // namespace app_usage
