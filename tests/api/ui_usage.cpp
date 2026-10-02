// How the UI setup API reads in an application.
//
// Compiled with every build, never linked or run. See core_usage.cpp for why the namespace is named.

#include "atpl/ui/ui.hpp"

#include <SFML/Graphics/CircleShape.hpp>

#include <memory>
#include <string>

namespace ui_usage {

using namespace atpl;

enum class Mode { Normal, Debug, Wireframe };

// Values the UI edits and the simulation reads.
struct Params {
    Param<float> speed = 5.f;
    Param<int> particleCount = 1000;
    Param<bool> gravity = true;
    Param<Mode> mode = Mode::Normal;
    Param<std::string> runName;
};

// Values the simulation writes and the UI shows.
struct Stats {
    Param<float> ticksPerSecond;
    Param<float> progress;
    Series tickTimes{ 240 };
};

// Data that is not a Param: reached through functions or through an own binding.
struct LegacyConfig {
    float speedKmh = 18.f;
    double volume = 0.5;
};

// An application's own implementation of a binding interface.
class VolumeBinding final : public NumberBinding {
public:
    explicit VolumeBinding(LegacyConfig& config) :
        m_config(config) {}

    [[nodiscard]] double get() const override { return m_config.volume; }
    void set(const double& value) override {
        m_config.volume = value;
        ++m_revision;
    }
    [[nodiscard]] Revision revision() const override { return m_revision; }

private:
    LegacyConfig& m_config;
    Revision m_revision = 0;
};

// An application's own widget: a descriptor with a name and a way to make the widget.
struct NetworkView {
    std::string name;
    int layers = 3;

    [[nodiscard]] std::unique_ptr<Widget> create() const;
};

// The whole UI as one value.
UISetup makeSetup(Params& params, Stats& stats, VolumeBinding& volume) {
    return {
        .background = "world",
        .grid = {.columns = 4, .rows = 2},
        .panels = {
            // Floating panels, on top of the background view.
            {
                .name = "Controls",
                .placement = Anchor::TopLeft,
                // Widgets placed automatically: one column, in the order listed.
                .widgets = {
                    Button("Reset"),
                    Slider("Speed", params.speed, {.min = 0.0, .max = 10.0}),
                    Slider("Particles", params.particleCount, {.min = 0.0, .max = 5000.0, .step = 100.0}),
                    Slider("Volume", volume),
                    Switch("Gravity"), // bound later, by name
                    Dropdown("Mode", {"Normal", "Debug", "Wireframe"}, params.mode),
                    TextInput("Run name", params.runName, {.placeholder = "untitled"}),
                },
            },
            {
                .name = "Statistics",
                .title = "Stats",
                .placement = Anchor::BottomLeft,
                .columns = 2,
                .collapsed = true,
                // Every widget placed by cell: two values side by side, wide widgets below.
                .widgets = {
                    at({.column = 0, .row = 0}, TextDisplay("Ticks/s", stats.ticksPerSecond, {.format = "{:.1f}"})),
                    at({.column = 1, .row = 0}, ProgressBar("Progress", stats.progress)),
                    at(
                        {.row = 1, .columnSpan = 2},
                        Graph("Tick time", stats.tickTimes, {.label = "Tick time (ms)", .min = 0.f})
                    ),
                    at({.row = 2, .columnSpan = 2}, NetworkView{.name = "Network", .layers = 4}),
                },
            },
            // A panel in the grid: the two right-hand columns of the upper row hold a minimap.
            {
                .name = "Map",
                .placement = GridCell{.column = 2, .row = 0, .columnSpan = 2},
                .collapsible = false,
                .widgets = {View("minimap")},
            },
        },
    };
}

void link(UI& ui, Params& params, LegacyConfig& config) {
    // Binding by name, after the UI exists. No handle is kept.
    ui.widget("Gravity").bind(params.gravity);

    // "Panel/Name" where a name alone would be ambiguous.
    ui.widget("Controls/Speed").bind(params.speed);

    // Data of any type, through two functions.
    ui.widget("Speed").bind(
        [&config] { return config.speedKmh / 3.6f; },
        [&config](float metresPerSecond) { config.speedKmh = metresPerSecond * 3.6f; }
    );

    // Read-only, through one function.
    ui.widget("Ticks/s").bind([&config] { return config.volume * 100.0; });

    // Reading and setting a widget directly.
    const bool gravity = ui.widget("Gravity").get<bool>();
    ui.widget("Speed").set(gravity ? 5.0 : 0.0);
    ui.widget("Reset").setEnabled(false);

    // The application's own drawing.
    ui.view("world").onDraw([](sf::RenderTarget& target, sf::Vector2f size) {
        sf::CircleShape shape(40.f);
        shape.setPosition(size * 0.5f);
        target.draw(shape);
    });
    ui.view("minimap").onDraw([](sf::RenderTarget&, sf::Vector2f) {});

    ui.panel("Statistics").setCollapsed(false);
}

// One frame, as `App` will run it.
bool frame(UI& ui) {
    ui.handleInput();
    ui.update();
    return ui.draw();
}

// From the simulation thread, after publishing a new state.
void newStateAvailable(UI& ui) {
    ui.requestRedraw();
}

} // namespace ui_usage
