// The smallest application that shows the UI: a window with a few panels of controls bound to
// parameters, and displays of a made-up simulation.
//
// It drives the UI by hand, which is what `App` will do for an application once it exists
// (Phase 4): read input, update, draw; and nothing at all while nothing happens. Escape or the
// window's close button ends it; while a text field or a dropdown list has the keys, Escape
// first ends the typing or closes the list.
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

#include "atpl/app/resources.hpp"
#include "atpl/core/series.hpp"
#include "atpl/core/text_log.hpp"
#include "atpl/core/version.hpp"
#include "atpl/ui/ui.hpp"

#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/System/Clock.hpp>

#include <array>
#include <cmath>
#include <exception>
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

/// What the controls change. In an application, the simulation would read these.
struct Params {
    Param<float> speed = 1.f;
    Param<int> size = 8;
    Param<bool> gravity = true;
    Param<bool> trails = false;
    Param<DrawMode> drawMode = DrawMode::Filled;
    Param<std::string> runName;
    Param<bool> paused = false;

    void reset() {
        speed = 1.f;
        size = 8;
        gravity = true;
        trails = false;
        drawMode = DrawMode::Filled;
    }
};

/// What a simulation would report, made up here: a run that loops, and a measurement on it.
struct Stats {
    static constexpr float ticksPerSecond = 20.f;

    Param<float> rate = 0.f;     ///< Ticks in the last second.
    Param<float> progress = 0.f; ///< How far the run is, 0 to 100.
    Series energy{ 200 };
    TextLog events{ 100 }; ///< What happened: any thread may write to it.

    float time = 0.f; ///< Simulated seconds.
    int ticksThisSecond = 0;

    void tick(const Params& params) {
        time += params.speed.get() / ticksPerSecond;
        progress = std::fmod(time * 5.f, 100.f);
        const float wave = std::sin(time) + 0.3f * std::sin(time * 4.7f) + 0.1f * std::sin(time * 23.f);
        energy.push((params.gravity.get() ? 2.f : 1.f) * static_cast<float>(params.size.get()) * (1.5f + wave));
        ++ticksThisSecond;
    }
};

UISetup describeUI(std::shared_ptr<const sf::Font> font, Layout layout, bool profiler, Params& params, Stats& stats) {
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
                    // The controls on top, the log of what happened below them.
                    at({ .column = 0, .row = 0 }, Switch("Paused", params.paused)),
                    at({ .column = 1, .row = 0 }, Button("Step")),
                    at({ .column = 0, .row = 1, .columnSpan = 2, .rowSpan = 3 }, Log("Events", stats.events, {.lines = 4})),
                },
            },
            // ... and these leave it to the layout theme: floating at the top left with
            // "overlay", in the free cells of the window's grid with "dashboard".
            // Click a header to fold the panel, and again to unfold it.
            {
                .name = "Controls",
                .widgets = {
                    Slider("Speed", params.speed, {.min = 0.0, .max = 10.0, .format = "{:.1f}"}),
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
                    TextDisplay("Ticks per second", stats.rate, {.format = "{:.0f}"}),
                    ProgressBar("Progress", stats.progress, {.min = 0.0, .max = 100.0}),
                    spanning({ .rows = 3 }, Graph("Energy", stats.energy)),
                },
            },
        },
        .profiler = profiler,
    };
    setup.theme.font = std::move(font);
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

    const Resources resources = Resources::nextToExecutable(argv[0]);
    const auto font = std::make_shared<const sf::Font>(resources.loadFont("fonts/default.ttf"));

    sf::ContextSettings settings;
    settings.antiAliasingLevel = 8;
    sf::RenderWindow window(
        sf::VideoMode({ 1280, 720 }),
        "atpl minimal " + std::string(versionString()),
        sf::Style::Default,
        sf::State::Windowed,
        settings
    );
    window.setVerticalSyncEnabled(true);

    Params params;
    Stats stats;
    UISetup setup = describeUI(font, std::move(layout), profiler, params, stats);
    if (ticks) {
        setup.theme[Slider::Ticks].shown = true; // an optional part: one theme entry
    }
    UI ui(window, std::move(setup));

    int passesLeft = 5; // only counted in a smoke test
    stats.events.push("Started");
    sf::Clock tickClock;
    sf::Time untilTick;
    sf::Clock rateClock;
    while (window.isOpen()) {
        ui.handleInput(); // sleeps while there is nothing to do
        for (const Event& event : ui.events()) {
            if (event.is<WindowClosed>() || event.isKey(sf::Keyboard::Key::Escape)) {
                window.close();
            }
            if (event.isButton("Reset")) {
                params.reset(); // the sliders and switches follow by themselves
                stats.events.push("Reset");
            }
            if (event.isButton("Step") && params.paused.get()) {
                stats.tick(params);
                stats.events.push("Step");
            }
            // What the user changed, in the log: the run name once it is entered.
            if (const auto* changed = event.getIf<ValueChanged>(); changed != nullptr && changed->final) {
                if (changed->name == "Paused") {
                    stats.events.push(params.paused.get() ? "Paused" : "Running");
                } else if (changed->name == "Draw") {
                    stats.events.push("Drawing " + std::string(drawModeNames[std::get<std::size_t>(changed->value)]));
                } else if (changed->name == "Run name") {
                    stats.events.push("Run name: " + params.runName.get());
                }
            }
        }
        // The made-up simulation: a tick every 50 ms while it runs. The displays follow by
        // themselves, as the controls do.
        const sf::Time tickTime = sf::seconds(1.f / Stats::ticksPerSecond);
        for (untilTick -= tickClock.restart(); untilTick <= sf::Time::Zero; untilTick += tickTime) {
            if (!params.paused.get()) {
                stats.tick(params);
            }
        }
        if (rateClock.getElapsedTime().asSeconds() >= 1.f) {
            rateClock.restart();
            stats.rate = static_cast<float>(std::exchange(stats.ticksThisSecond, 0));
        }
        ui.update();
        ui.draw(); // draws only if something changed

        if (smokeTest) {
            ui.requestRedraw();
            if (--passesLeft == 0) {
                window.close();
            }
        }
    }
    return 0;
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
