// The smallest application written against the template's API: a simulation on its own thread,
// two panels, a main view with pan and zoom, and a minimap.
//
// For now this file is only compiled, not linked: the API it uses is declared (Phase 1) but not
// yet implemented (Phases 2 to 4). It is the reference for how an application is meant to read,
// and it becomes a runnable example when the implementation is there. Until then, the runnable
// example is examples/minimal.

#include "atpl/app/app.hpp"
#include "atpl/app/camera.hpp"

#include <SFML/Graphics/CircleShape.hpp>
#include <SFML/Graphics/RectangleShape.hpp>

#include <cmath>
#include <exception>
#include <iostream>

using namespace atpl;

// ----- The application's data -----

// What the user can change. Widgets write these, the simulation reads them.
struct Params {
    Param<float> speed = 1.f;
    Param<float> radius = 200.f;
    Param<bool> clockwise = true;
};

// What the application can tell the simulation to do.
enum class Command { Reset };

// What the main thread needs to draw one frame.
struct World {
    sf::Vector2f planet;
    float orbitRadius = 0.f;
};

// ----- The simulation: runs on its own thread -----

class Orbit final : public Simulation<World, Command> {
public:
    explicit Orbit(Params& params) :
        m_params(params) {}

private:
    void onCommand(const Command& command) override {
        if (command == Command::Reset) {
            m_angle = 0.f;
        }
    }

    void tick(float dt) override {
        const float direction = m_params.clockwise ? 1.f : -1.f;
        m_angle += direction * m_params.speed * dt;
    }

    void writeState(World& state) const override {
        const float radius = m_params.radius;
        state.planet = { std::cos(m_angle) * radius, std::sin(m_angle) * radius };
        state.orbitRadius = radius;
    }

    Params& m_params;
    float m_angle = 0.f;
};

// ----- Drawing: on the main thread, in world coordinates -----

void drawWorld(sf::RenderTarget& target, const World& world) {
    sf::CircleShape orbit(world.orbitRadius, 96);
    orbit.setOrigin({ world.orbitRadius, world.orbitRadius });
    orbit.setFillColor(sf::Color::Transparent);
    orbit.setOutlineColor(sf::Color(120, 120, 120));
    orbit.setOutlineThickness(1.f);
    target.draw(orbit);

    sf::CircleShape planet(12.f);
    planet.setOrigin({ 12.f, 12.f });
    planet.setPosition(world.planet);
    target.draw(planet);
}

// ----- The application -----

int run() {
    Params params;
    Orbit simulation(params);

    Camera camera("world");
    Camera mapCamera("minimap");

    App app({
        .window = {.title = "atpl starter"},
        .ui = {
            .background = "world", // the simulation fills the window, behind the panels
            .panels = {
                {
                    .name = "Orbit",
                    .placement = Anchor::TopLeft,
                    .widgets = {
                        Slider("Speed", params.speed, {.min = 0.0, .max = 5.0}), // bound in the setup
                        Slider("Radius", params.radius, {.min = 50.0, .max = 400.0}),
                        Switch("Clockwise"), // bound by name, below
                        Button("Reset"),
                    },
                },
                {
                    .name = "Simulation",
                    .placement = Anchor::TopRight,
                    .widgets = {
                        Switch("Pause", simulation.controls.paused),
                        ValueDisplay("Ticks/s", simulation.controls.ticksPerSecond, {.format = "{:.0f}"}),
                    },
                },
                {
                    .name = "Map",
                    .placement = Anchor::BottomRight,
                    .collapsible = false,
                    .widgets = { View("minimap", {.height = 160.f}) },
                },
            },
        },
    });

    app.ui().widget("Clockwise").bind(params.clockwise);

    // The main view: seen through a camera the user can move.
    app.ui().view("world").onDraw([&](sf::RenderTarget& target, sf::Vector2f size) {
        camera.apply(target, size);
        drawWorld(target, simulation.state());
    });

    // The minimap: always the whole orbit, plus a frame showing what the main view sees.
    app.ui().view("minimap").onDraw([&](sf::RenderTarget& target, sf::Vector2f size) {
        const World& world = simulation.state();
        const float extent = world.orbitRadius * 1.2f;
        mapCamera.show({ -extent, -extent }, { 2.f * extent, 2.f * extent });
        mapCamera.apply(target, size);
        drawWorld(target, world);

        const FloatRect visible = camera.visibleArea();
        sf::RectangleShape frame(visible.size());
        frame.setPosition(visible.position());
        frame.setFillColor(sf::Color::Transparent);
        frame.setOutlineColor(sf::Color::White);
        frame.setOutlineThickness(world.orbitRadius * 0.02f);
        target.draw(frame);
    });

    // Events: the application decides what the simulation hears about.
    app.onEvent([&](const Event& event) {
        if (event.isButton("Reset")) {
            simulation.send(Command::Reset);
        }
        if (event.isKey(sf::Keyboard::Key::Space)) {
            simulation.controls.paused = !simulation.controls.paused;
        }
        if (event.isKey(sf::Keyboard::Key::Escape)) {
            app.quit();
        }

        // A click on the minimap moves the main view there.
        if (const auto* press = event.getIf<PointerPressed>(); press != nullptr && press->pointer.isIn("minimap")) {
            camera.setCenter(mapCamera.toWorld(press->pointer.inView));
            app.ui().requestRedraw();
        }

        // Drag and wheel in the main view move and zoom it.
        if (camera.handle(event)) {
            app.ui().requestRedraw();
        }
    });

    return app.run(simulation);
}

int main() {
    try {
        return run();
    } catch (const std::exception& error) {
        // A missing resource, a mistake in the UI setup, or a failure in the simulation.
        std::cerr << "error: " << error.what() << '\n';
        return 1;
    }
}
