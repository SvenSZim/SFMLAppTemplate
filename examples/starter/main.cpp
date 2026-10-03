// The smallest application written against the template's API: a simulation on its own thread,
// two panels, a main view with pan and zoom, and a minimap. It is the reference for how an
// application is meant to read; examples/showcase shows everything else the template offers.
//
//   starter [--smoke-test]
//
// Drag the view to move it, use the wheel to zoom, click or drag on the minimap to look there;
// once the minimap was clicked, the arrow keys move the view. Space pauses, Escape quits. --smoke-test draws a few
// frames and exits, for automated checks.

#include "atpl/app/app.hpp"
#include "atpl/app/camera.hpp"
#include "atpl/app/minimap.hpp"

#include <SFML/Graphics/CircleShape.hpp>

#include <cmath>
#include <exception>
#include <iostream>
#include <string_view>

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

int run(bool smokeTest) {
    Params params;
    Orbit simulation(params);

    // The main view's camera, and a minimap that steers it. Both are optional helpers.
    Camera camera("world");
    Minimap minimap("minimap", camera, { { -240.f, -240.f }, { 480.f, 480.f } });

    App app({
        .window = {.title = "atpl starter"},
        .ui = {
            .background = "world",  // the simulation fills the window, behind the panels
            .defaultView = "world", // keys go to it, unless the minimap was clicked
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
                        ValueDisplay("Ticks per second", simulation.controls.ticksPerSecond, {.format = "{:.0f}"}),
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
        minimap.setWorld({ { -extent, -extent }, { 2.f * extent, 2.f * extent } }); // the orbit's size can change
        minimap.apply(target, size);
        drawWorld(target, world);
        minimap.drawMarks(target, sf::Color::White);
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

        // The minimap moves the main view: a press or a drag on it, or the arrow keys once it
        // was clicked. A drag or the wheel in the main view moves or zooms it. Both hear every event.
        const bool mapMoved = minimap.handle(event);
        const bool viewMoved = camera.handle(event);
        if (mapMoved || viewMoved) {
            app.ui().requestRedraw();
        }
    });

    // Only for automated checks: a few frames, then the end.
    int passesLeft = 5;
    if (smokeTest) {
        app.onUpdate([&](float) {
            app.ui().requestRedraw();
            if (--passesLeft == 0) {
                app.quit();
            }
        });
    }

    return app.run(simulation);
}

int main(int argc, char* argv[]) {
    try {
        return run(argc > 1 && std::string_view(argv[1]) == "--smoke-test");
    } catch (const std::exception& error) {
        // A missing resource, a mistake in the UI setup, or a failure in the simulation.
        std::cerr << "error: " << error.what() << '\n';
        return 1;
    }
}
