// The smallest application that shows the UI: a window with a few panels of controls bound to
// parameters.
//
// It drives the UI by hand, which is what `App` will do for an application once it exists
// (Phase 4): read input, update, draw; and nothing at all while nothing happens. Escape or the
// window's close button ends it.
//
//   minimal [--layout overlay|dashboard|cards|compact] [--ticks] [--profiler] [--smoke-test]
//
// --layout chooses the layout theme: where panels go that do not say so, and how large things
// are. Everything grows and shrinks with the window, within the layout theme's limits.
// --ticks shows the sliders' ticks, an optional part a theme can switch on.
// --profiler shows the profiler readout. --smoke-test draws a few frames and exits, for
// automated checks.

#include "atpl/app/resources.hpp"
#include "atpl/core/version.hpp"
#include "atpl/ui/ui.hpp"

#include <SFML/Graphics/RenderWindow.hpp>

#include <exception>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace {

using namespace atpl;

// A stand-in for the built-in widgets, which come later (WP 3.9): a box with a name. It is
// written the way an application writes a widget of its own: a descriptor with its parts, and
// the widget's behaviour; no positions, no colours.
struct Placeholder {
    static constexpr Kind kind{ "placeholder" };
    static constexpr Part Box{ kind, "box", Role::Track };
    static constexpr Part Name{ kind, "name", Role::MutedText };

    std::string name;
    float rows = 1.f; ///< How many rows high it would like to be.

    [[nodiscard]] std::unique_ptr<Widget> create() const;
};

class PlaceholderWidget final : public Widget {
public:
    PlaceholderWidget(std::string name, float rows) :
        m_name(std::move(name)),
        m_rows(rows) {}

    [[nodiscard]] SizeRequest measure(const MeasureContext& context) const override {
        const float row = context.sizes().rowHeight;
        return { .min = { 60.f, row * 0.75f * m_rows }, .preferred = { 160.f, row * m_rows } };
    }

    void paint(Painter& painter, const Style& style) const override {
        const FloatRect all({ 0.f, 0.f }, painter.size());
        painter.box(all, style.part(Placeholder::Box));
        painter.text(all, m_name, style.part(Placeholder::Name), Align::Center);
    }

private:
    std::string m_name;
    float m_rows;
};

std::unique_ptr<Widget> Placeholder::create() const {
    return std::make_unique<PlaceholderWidget>(name, rows);
}

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

/// What the controls change. In an application, the simulation would read these.
struct Params {
    Param<float> speed = 1.f;
    Param<int> size = 8;
    Param<bool> gravity = true;
    Param<bool> trails = false;
    Param<bool> paused = false;

    void reset() {
        speed = 1.f;
        size = 8;
        gravity = true;
        trails = false;
    }
};

UISetup describeUI(std::shared_ptr<const sf::Font> font, Layout layout, bool profiler, Params& params) {
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
                .widgets = { Switch("Paused", params.paused), Button("Step") },
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
                    Button("Reset"),
                },
            },
            {
                .name = "Statistics",
                // Stand-ins until text displays and graphs exist (WP 3.10).
                .widgets = { Placeholder{ "Ticks per second" }, Placeholder{ .name = "Graph", .rows = 3.f } },
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
    Layout layout = layouts::overlay();
    for (int i = 1; i < argc; ++i) {
        const std::string_view argument = argv[i];
        smokeTest = smokeTest || argument == "--smoke-test";
        profiler = profiler || argument == "--profiler";
        ticks = ticks || argument == "--ticks";
        if (argument == "--layout" && i + 1 < argc) {
            layout = layoutNamed(argv[++i]);
        }
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
    UISetup setup = describeUI(font, std::move(layout), profiler, params);
    if (ticks) {
        setup.theme[Slider::Ticks].shown = true; // an optional part: one theme entry
    }
    UI ui(window, std::move(setup));

    int passesLeft = 5; // only counted in a smoke test
    while (window.isOpen()) {
        ui.handleInput(); // sleeps while there is nothing to do
        for (const Event& event : ui.events()) {
            if (event.is<WindowClosed>() || event.isKey(sf::Keyboard::Key::Escape)) {
                window.close();
            }
            if (event.isButton("Reset")) {
                params.reset(); // the sliders and switches follow by themselves
            }
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
