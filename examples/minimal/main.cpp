// The smallest application that shows the UI: a window with a few panels of controls bound to
// parameters, and displays of a made-up simulation that runs on a thread of its own.
//
// It runs through `App`, which opens the window, loads the font, and runs the loop: input, the
// application's update, the UI's update, a frame if anything changed; and nothing at all while
// nothing happens. Escape or the window's close button ends it; while a text field or a dropdown
// list has the keys, Escape first ends the typing or closes the list.
//
//   minimal [--layout overlay|dashboard|cards|compact] [--cards] [--ticks] [--profiler] [--smoke-test]
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
#include "atpl/core/series.hpp"
#include "atpl/core/text_log.hpp"
#include "atpl/core/version.hpp"
#include "atpl/ui/ui.hpp"

#include <array>
#include <cmath>
#include <exception>
#include <format>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

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
    double time = 0.0; ///< Simulated seconds.
};

/// A made-up simulation on a thread of its own: a run that loops, and a measurement on it.
/// 20 ticks per second of simulated time; the speed slider changes how fast that time passes.
class MadeUp final : public Simulation<Moment> {
public:
    explicit MadeUp(const Params& params) :
        m_params(params) {
        controls.tickRate = 20.0;
    }

    // What it reports, for widgets to show. Thread-safe: it writes them, the UI reads them.
    Param<float> progress = 0.f; ///< How far the run is, 0 to 100.
    Series energy{ 200 };

private:
    void tick(float dt) override {
        m_time += dt;
        progress = std::fmod(m_time * 5.f, 100.f);
        const float wave = std::sin(m_time) + 0.3f * std::sin(m_time * 4.7f) + 0.1f * std::sin(m_time * 23.f);
        energy.push((m_params.gravity.get() ? 2.f : 1.f) * static_cast<float>(m_params.size.get()) * (1.5f + wave));
    }

    void writeState(Moment& moment) const override { moment.time = m_time; }

    const Params& m_params;
    float m_time = 0.f;
};

/// What the main thread reports.
struct Report {
    TextLog events{ 100 };     ///< What happened: any thread may write to it.
    Param<std::string> status; ///< What the simulation is doing, in words.
};

UISetup describeUI(Layout layout, bool profiler, Params& params, MadeUp& simulation, Report& report) {
    UISetup setup{
        .layout = std::move(layout),
        .grid = {.columns = 4, .rows = 2},
        .panels = {
            // These two say where they go ...
            {
                .name = "Scene",
                .placement = GridCell{.column = 0, .row = 0, .columnSpan = 3, .rowSpan = 2},
                .collapsible = false,
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
    MadeUp simulation(params);
    Report report;
    UISetup ui = describeUI(std::move(layout), profiler, params, simulation, report);
    if (ticks) {
        ui.theme[Slider::Ticks].shown = true; // an optional part: one theme entry
    }
    // The window, the UI with the font from the resources, and the loop. Closing the window
    // ends it.
    App app({ .window = { .title = "atpl minimal " + std::string(versionString()) }, .ui = std::move(ui) });
    report.events.push("Started");

    // What the user does: called once per event, on the main thread. The simulation hears of it
    // through its controls and parameters.
    app.onEvent([&](const Event& event) {
        if (event.isKey(sf::Keyboard::Key::Escape)) {
            app.quit();
        }
        if (event.isButton("Reset")) {
            params.reset(); // the sliders and switches follow by themselves
            simulation.controls.speed = 1.0;
            report.events.push("Reset");
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
