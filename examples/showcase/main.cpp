// A tour of what the template offers: every built-in widget, bound to parameters and to a
// made-up simulation of particles that runs on a thread of its own, drawn in a main view you can
// drag and zoom and in a minimap. For how a small application is written, see examples/starter.
//
// It runs through `App`, which opens the window, loads the font, and runs the loop: input, the
// application's update, the UI's update, a frame if anything changed; and nothing at all while
// nothing happens. Escape or the window's close button ends it; while a text field or a dropdown
// list has the keys, Escape first ends the typing or closes the list.
//
//   showcase [--layout overlay|dashboard|cards|compact] [--cards] [--ticks] [--profiler] [--smoke-test]
//
// --layout chooses the layout theme: where panels go that do not say so, and how large things
// are. Everything grows and shrinks with the window, within the layout theme's limits.
// --cards lets a stack of floating panels that does not fit into the window overlap like a stack
// of cards instead of leaving panels out (not to be confused with the "cards" layout theme, whose
// panels sit in the window's grid): make the window low to see it.
// --ticks shows the sliders' ticks, an optional part a theme can switch on.
// --profiler shows the profiler readout. --smoke-test draws a few frames and exits, for
// automated checks.

#include "atpl/app/app.hpp"
#include "atpl/app/camera.hpp"
#include "atpl/app/minimap.hpp"
#include "atpl/core/series.hpp"
#include "atpl/core/text_log.hpp"
#include "atpl/core/version.hpp"
#include "atpl/ui/ui.hpp"

#include <SFML/Graphics/CircleShape.hpp>
#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/VertexArray.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <exception>
#include <format>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace {

using namespace atpl;

Layout layoutNamed(std::string_view name) {
    if (name == "overlay") {
        return layouts::overlay();
    }
    if (name == "dashboard") {
        return layouts::dashboard();
    }
    if (name == "cards") {
        return layouts::cards();
    }
    if (name == "compact") {
        return layouts::compact();
    }
    throw std::invalid_argument(
        "unknown layout theme \"" + std::string(name) + "\": overlay, dashboard, cards or compact"
    );
}

/// How the scene would be drawn: the entries of the "Draw" dropdown, in the same order.
enum class DrawMode { Filled, Outlined, Points };
constexpr std::array<const char*, 3> drawModeNames{ "Filled", "Outlined", "Points" };

/// What the controls change. The simulation reads these from its thread: they are `Param`s.
struct Params {
    Param<int> size = 8;
    Param<bool> gravity = true;
    Param<bool> trails = false;
    Param<DrawMode> drawMode = DrawMode::Filled;
    Param<std::string> runName;

    void reset() {
        size = 8;
        gravity = true;
        trails = false;
        drawMode = DrawMode::Filled;
    }
};

/// What the main thread needs from the simulation for one frame.
struct Moment {
    double time = 0.0;                             ///< Simulated seconds.
    std::vector<sf::Vector2f> particles;           ///< Where they are, in world units.
    std::vector<std::vector<sf::Vector2f>> trails; ///< Where each was lately, oldest first; empty without trails.
};

/// The world is a square of this half width around the origin.
constexpr float worldExtent = 100.f;

/// A made-up simulation on a thread of its own: particles in a box, pulled to the centre while
/// gravity is on. 20 ticks per second of simulated time; the speed slider changes how fast that
/// time passes.
class Particles final : public Simulation<Moment> {
public:
    explicit Particles(const Params& params) :
        m_params(params) {
        controls.tickRate = 20.0;
    }

    // What it reports, for widgets to show. Thread-safe: it writes them, the UI reads them.
    Param<float> progress = 0.f; ///< How far the run is, 0 to 100.
    Series energy{ 200 };        ///< Their kinetic energy.

private:
    struct Body {
        sf::Vector2f position;
        sf::Vector2f velocity;
        std::vector<sf::Vector2f> trail;
    };

    static constexpr std::size_t trailLength = 16;

    void tick(float dt) override {
        m_time += dt;
        progress = std::fmod(m_time * 5.f, 100.f);

        // As many as the size slider says: new ones start on a circle, moving round it.
        const auto wanted = static_cast<std::size_t>(std::max(m_params.size.get(), 1));
        while (m_bodies.size() < wanted) {
            const float angle = static_cast<float>(m_bodies.size()) * 2.399963f; // the golden angle
            const float radius = 30.f + 6.f * static_cast<float>(m_bodies.size() % 10);
            const sf::Vector2f out(std::cos(angle), std::sin(angle));
            m_bodies.push_back(
                { .position = out * radius, .velocity = sf::Vector2f(-out.y, out.x) * 45.f, .trail = {} }
            );
        }
        m_bodies.resize(wanted);

        float kinetic = 0.f;
        for (Body& body : m_bodies) {
            if (m_params.gravity.get()) {
                body.velocity -= body.position * (0.8f * dt); // pulled to the centre
            }
            body.position += body.velocity * dt;
            // The walls of the box bounce them back.
            if (std::abs(body.position.x) > worldExtent) {
                body.position.x = std::copysign(worldExtent, body.position.x);
                body.velocity.x = -body.velocity.x;
            }
            if (std::abs(body.position.y) > worldExtent) {
                body.position.y = std::copysign(worldExtent, body.position.y);
                body.velocity.y = -body.velocity.y;
            }
            body.trail.push_back(body.position);
            if (body.trail.size() > trailLength) {
                body.trail.erase(body.trail.begin());
            }
            kinetic += 0.5f * (body.velocity.x * body.velocity.x + body.velocity.y * body.velocity.y);
        }
        energy.push(kinetic / 1000.f);
    }

    void writeState(Moment& moment) const override {
        moment.time = m_time;
        moment.particles.resize(m_bodies.size());
        moment.trails.resize(m_params.trails.get() ? m_bodies.size() : 0);
        for (std::size_t i = 0; i < m_bodies.size(); ++i) {
            moment.particles[i] = m_bodies[i].position;
            if (i < moment.trails.size()) {
                moment.trails[i] = m_bodies[i].trail; // the buffers keep their memory
            }
        }
    }

    const Params& m_params;
    float m_time = 0.f;
    std::vector<Body> m_bodies;
};

/// The part of the world a view shows to see all of it: the box and a little room around it.
constexpr sf::Vector2f worldCorner(-worldExtent * 1.1f, -worldExtent * 1.1f);
constexpr sf::Vector2f worldSize(worldExtent * 2.2f, worldExtent * 2.2f);

/// Draws the world in world units, for a view whose camera is applied: the box, the particles as
/// the draw mode says, and their trails if there are any. `pixel` is how many world units one
/// pixel is, so that lines and points keep their thickness whatever the zoom.
void drawWorld(sf::RenderTarget& target, const Moment& world, DrawMode mode, const Theme& theme, float pixel) {
    const sf::Color ink = theme.resolve(Graph::Curve).color;
    const sf::Color line = theme.resolve(Graph::Baseline).color;

    sf::RectangleShape box(sf::Vector2f(worldExtent, worldExtent) * 2.f);
    box.setPosition({ -worldExtent, -worldExtent });
    box.setFillColor(sf::Color::Transparent);
    box.setOutlineColor(line);
    box.setOutlineThickness(pixel);
    target.draw(box);

    for (const std::vector<sf::Vector2f>& trail : world.trails) {
        sf::VertexArray strip(sf::PrimitiveType::LineStrip, trail.size());
        for (std::size_t i = 0; i < trail.size(); ++i) {
            sf::Color faded = ink;
            faded.a = static_cast<std::uint8_t>(200 * (i + 1) / trail.size()); // oldest the faintest
            strip[i] = sf::Vertex{ trail[i], faded };
        }
        target.draw(strip);
    }

    const float radius = mode == DrawMode::Points ? 1.5f * pixel : std::max(3.f, 2.f * pixel);
    sf::CircleShape dot(radius, 16);
    dot.setOrigin({ radius, radius });
    dot.setFillColor(mode == DrawMode::Outlined ? sf::Color::Transparent : ink);
    dot.setOutlineColor(ink);
    dot.setOutlineThickness(mode == DrawMode::Outlined ? 1.5f * pixel : 0.f);
    for (const sf::Vector2f particle : world.particles) {
        dot.setPosition(particle);
        target.draw(dot);
    }
}

/// What the main thread reports.
struct Report {
    TextLog events{ 100 };     ///< What happened: any thread may write to it.
    Param<std::string> status; ///< What the simulation is doing, in words.
};

UISetup describeUI(Layout layout, bool profiler, Params& params, Particles& simulation, Report& report) {
    // The minimap takes a cell of its own where the layout theme puts panels in the window's
    // grid ("dashboard", "cards"); where panels float ("overlay", "compact"), it floats at the
    // top right.
    const std::optional<Placement> mapPlacement =
        std::holds_alternative<GridSpan>(layout.placement) ? std::nullopt : std::optional<Placement>(Anchor::TopRight);
    UISetup setup{
        .layout = std::move(layout),
        .grid = {.columns = 4, .rows = 3},
        .panels = {
            // These two say where they go ...
            {
                .name = "Scene",
                .placement = GridCell{.column = 0, .row = 0, .columnSpan = 3, .rowSpan = 3},
                .collapsible = false,
                .widgets = { View("world") }, // the main view: as large as the panel
            },
            {
                .name = "Map", // listed before the others that take free cells: the first, at the top
                .placement = mapPlacement,
                .width = 200.f,
                .widgets = {
                    View("minimap", {.height = 150.f}), // the whole world, small
                    Switch("Select area"),              // drag a rectangle instead of moving the view
                },
            },
            {
                .name = "Playback",
                .placement = Anchor::Bottom,
                .columns = 2,
                .width = 420.f,
                .widgets = {
                    // The controls on top, what it is doing below them, and the log of what happened.
                    at({ .column = 0, .row = 0 }, Switch("Paused", simulation.controls.paused)),
                    at({ .column = 1, .row = 0 }, Button("Step")),
                    at({ .column = 0, .row = 1, .columnSpan = 2 }, TextDisplay("Status", report.status)),
                    at({ .column = 0, .row = 2, .columnSpan = 2, .rowSpan = 3 }, Log("Events", report.events, {.lines = 4})),
                },
            },
            // ... and these leave it to the layout theme: floating at the top left with
            // "overlay", in the free cells of the window's grid with "dashboard".
            // Click a header to fold the panel, and again to unfold it.
            {
                .name = "Controls",
                .widgets = {
                    Slider("Speed", simulation.controls.speed, {.min = 0.0, .max = 10.0, .format = "{:.1f}"}),
                    Slider("Size", params.size, {.min = 1.0, .max = 32.0, .step = 1.0, .format = "{:.0f}"}),
                    Switch("Gravity", params.gravity),
                    Switch("Trails", params.trails),
                    Dropdown("Draw", {drawModeNames.begin(), drawModeNames.end()}, params.drawMode),
                    TextInput("Run name", params.runName, {.placeholder = "untitled"}),
                    Button("Reset"),
                },
            },
            {
                .name = "About",
                .placement = Anchor::Top,
                .widgets = {
                    Paragraph("Title", {.heading = "A made-up simulation"}), // a section heading
                    Paragraph("Help", {.text = "Drag the sliders and flip the switches; the statistics follow.\nPause it, "
                                               "and Step moves it on by one tick."}),
                    Paragraph(
                        "Panels",
                        {.heading = "Panels",
                         .text = "Click a header to fold a panel. A panel too low for its widgets scrolls.",
                         .footer = "Escape quits."}
                    ),
                },
            },
            {
                .name = "Statistics",
                .widgets = {
                    ValueDisplay("Ticks per second", simulation.controls.ticksPerSecond, {.format = "{:.0f}"}),
                    ProgressBar("Progress", simulation.progress, {.min = 0.0, .max = 100.0}),
                    spanning({ .rows = 3 }, Graph("Energy", simulation.energy)),
                },
            },
        },
        .profiler = profiler,
    };
    setup.defaultView = "world"; // keys go to the main view unless another one was clicked
    return setup;
}

int run(int argc, char* argv[]) {
    bool smokeTest = false;
    bool profiler = false;
    bool ticks = false;
    bool cards = false;
    Layout layout = layouts::overlay();
    for (int i = 1; i < argc; ++i) {
        const std::string_view argument = argv[i];
        smokeTest = smokeTest || argument == "--smoke-test";
        profiler = profiler || argument == "--profiler";
        ticks = ticks || argument == "--ticks";
        cards = cards || argument == "--cards";
        if (argument == "--layout" && i + 1 < argc) {
            layout = layoutNamed(argv[++i]);
        }
    }
    if (cards) {
        layout.stackOverflow = StackOverflow::Cards; // one setting of the layout theme
    }

    Params params;
    Particles simulation(params);
    Report report;
    UISetup ui = describeUI(std::move(layout), profiler, params, simulation, report);
    if (ticks) {
        ui.theme[Slider::Ticks].shown = true; // an optional part: one theme entry
    }
    // The window, the UI with the font from the resources, and the loop. Closing the window
    // ends it.
    App app({ .window = { .title = "atpl showcase " + std::string(versionString()) }, .ui = std::move(ui) });
    report.events.push("Started");

    // The main view can be dragged and zoomed; the minimap always shows the whole world and
    // steers the main view. Both are optional helpers: without them, a view is in pixels.
    Camera camera("world");
    Minimap minimap("minimap", camera, FloatRect(worldCorner, worldSize));
    camera.show(worldCorner, worldSize);

    // What the user does: called once per event, on the main thread. The simulation hears of it
    // through its controls and parameters.
    app.onEvent([&](const Event& event) {
        if (event.isKey(sf::Keyboard::Key::Escape)) {
            app.quit();
        }
        if (event.isButton("Reset")) {
            params.reset(); // the sliders and switches follow by themselves
            simulation.controls.speed = 1.0;
            camera.show(worldCorner, worldSize);
            app.ui().requestRedraw();
            report.events.push("Reset");
        }
        if (const ValueChanged* mode = event.changeOf("Select area"); mode != nullptr && mode->final) {
            minimap.setMode(std::get<bool>(mode->value) ? MinimapMode::Select : MinimapMode::Pan);
        }
        // The minimap moves the main view (a press, a drag, a selection, the arrow keys while it
        // is selected); a drag or the wheel in the main view moves or zooms it. Both hear every
        // event.
        const bool mapMoved = minimap.handle(event);
        const bool viewMoved = camera.handle(event);
        if (mapMoved || viewMoved) {
            app.ui().requestRedraw();
        }
        if (event.isButton("Step") && simulation.controls.paused.get()) {
            simulation.controls.step(); // one tick, on the simulation's thread
            report.events.push("Step");
        }
        // What the user changed, in the log: the run name once it is entered.
        if (const auto* changed = event.getIf<ValueChanged>(); changed != nullptr && changed->final) {
            if (changed->name == "Paused") {
                report.events.push(simulation.controls.paused.get() ? "Paused" : "Running");
            } else if (changed->name == "Draw") {
                report.events.push("Drawing " + std::string(drawModeNames[std::get<std::size_t>(changed->value)]));
            } else if (changed->name == "Run name") {
                report.events.push("Run name: " + params.runName.get());
            }
        }
    });

    // Once per pass, on the main thread: the status from the state of this pass.
    int passesLeft = 5; // only counted in a smoke test
    app.onUpdate([&](float) {
        const double time = simulation.state().time;
        report.status = simulation.controls.paused.get() ? std::format("Paused at {:.1f} s", time)
                                                         : std::format("Running, {:.1f} s simulated", time);
        if (smokeTest) {
            app.ui().requestRedraw();
            if (--passesLeft == 0) {
                app.quit();
            }
        }
    });

    // The views: the main view and the minimap draw the same moment, the state of this pass.
    app.ui().view("world").onDraw([&](sf::RenderTarget& target, sf::Vector2f size) {
        camera.apply(target, size);
        drawWorld(target, simulation.state(), params.drawMode.get(), app.ui().theme(), 1.f / camera.zoom());
    });
    app.ui().view("minimap").onDraw([&](sf::RenderTarget& target, sf::Vector2f size) {
        minimap.apply(target, size);
        const float pixel = minimap.world().width() / size.x; // about one pixel, in world units
        drawWorld(target, simulation.state(), DrawMode::Points, app.ui().theme(), pixel);
        minimap.drawMarks(target, app.ui().theme().resolve(Graph::Curve).color); // what the main view sees
    });

    return app.run(simulation); // the simulation runs on its own thread until the app ends
}

} // namespace

int main(int argc, char* argv[]) {
    try {
        return run(argc, argv);
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 1;
    }
}
