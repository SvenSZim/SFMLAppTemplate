// Measures what the render pipeline costs, against the targets of docs/PROJECT_PLAN.md 5.5.
//
// The scene is five panels with a hundred widgets between them, painted the way real widgets
// will be: every part asks the theme for its style and goes through the painter. Each scenario
// runs the main loop for a number of frames with vsync off and prints what the profiler measured.
//
//   atpl_render_bench [--frames N] [--idle-ms N] [--screenshot file.png]
//
// The exit code says whether the targets that do not depend on the machine are met: no frames
// while idle, and at most two draw calls for shapes per panel. Times are printed, not judged.

#include "atpl/app/resources.hpp"
#include "atpl/ui/setup.hpp"
#include "atpl/ui/widget.hpp"
#include "atpl/ui/widgets.hpp"

#include "ui/frame_loop.hpp"
#include "ui/render/profiler.hpp"
#include "ui/render/renderer.hpp"
#include "ui/render/text_cache.hpp"

#include <SFML/Graphics/Image.hpp>
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <SFML/System/Clock.hpp>

#include <array>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <memory>
#include <new>
#include <string>
#include <string_view>
#include <vector>

// ----- Counting allocations -----
//
// Steady-state frames are meant to allocate nothing (docs/CODE_STYLE.md 7). Replacing the global
// allocation functions is the one way to see whether that holds.

namespace {
std::atomic<std::size_t> allocations{ 0 };
}

void* operator new(std::size_t size) {
    allocations.fetch_add(1, std::memory_order_relaxed);
    if (void* memory = std::malloc(size > 0 ? size : 1)) {
        return memory;
    }
    throw std::bad_alloc();
}
void* operator new[](std::size_t size) {
    return operator new(size);
}
void operator delete(void* memory) noexcept {
    std::free(memory);
}
void operator delete[](void* memory) noexcept {
    std::free(memory);
}
void operator delete(void* memory, std::size_t) noexcept {
    std::free(memory);
}
void operator delete[](void* memory, std::size_t) noexcept {
    std::free(memory);
}

namespace {

using namespace atpl;
using atpl::render::PanelBatch;
using atpl::render::Profiler;
using Section = atpl::render::Profiler::Section;

constexpr std::size_t panelCount = 5;
constexpr std::size_t widgetsPerPanel = 20;
constexpr std::size_t graphPoints = 120;

enum class WidgetType { Slider, Button, Switch, Progress, Value, Dropdown, Graph };

// What every panel holds, top to bottom: 20 widgets.
constexpr std::array<WidgetType, widgetsPerPanel> widgetTypes = {
    WidgetType::Value,  WidgetType::Value,    WidgetType::Slider,   WidgetType::Slider, WidgetType::Slider,
    WidgetType::Switch, WidgetType::Switch,   WidgetType::Dropdown, WidgetType::Graph,  WidgetType::Progress,
    WidgetType::Slider, WidgetType::Slider,   WidgetType::Slider,   WidgetType::Switch, WidgetType::Value,
    WidgetType::Value,  WidgetType::Progress, WidgetType::Button,   WidgetType::Button, WidgetType::Button,
};
constexpr std::array<std::string_view, widgetsPerPanel> widgetLabels = {
    "Frames",   "Agents",  "Speed",    "Evaporation", "Diffusion", "Paused",  "Trails", "Colony", "Food",  "Progress",
    "Rotation", "Sensing", "Strength", "Markers",     "Collected", "Elapsed", "Batch",  "Step",   "Reset", "Save",
};

class Scene {
public:
    explicit Scene(std::shared_ptr<const sf::Font> font) {
        m_theme.font = std::move(font);

        float contentHeight = 0.f;
        for (const WidgetType type : widgetTypes) {
            contentHeight += heightOf(type) + m_sizes.gap.y;
        }
        m_panelSize = { m_sizes.panelWidth, m_sizes.headerHeight + m_sizes.padding.x * 2.f + contentHeight };

        for (std::size_t panel = 0; panel < panelCount; ++panel) {
            m_list[panel] = &m_batches[panel];
            m_batches[panel].setSize(m_panelSize);
            m_batches[panel].setPosition(home(panel));
            for (std::size_t widget = 0; widget < widgetsPerPanel; ++widget) {
                m_values[panel][widget] =
                    0.15f + 0.7f * std::fmod(0.37f * static_cast<float>(panel * 7 + widget * 3), 1.f);
            }
        }
    }

    [[nodiscard]] const Theme& theme() const { return m_theme; }
    [[nodiscard]] const Sizes& sizes() const { return m_sizes; }
    [[nodiscard]] std::span<PanelBatch* const> panels() const { return m_list; }
    [[nodiscard]] PanelBatch& batch(std::size_t panel) { return m_batches[panel]; }
    [[nodiscard]] sf::Vector2f panelSize() const { return m_panelSize; }
    [[nodiscard]] float margin() const { return m_sizes.margin; }

    /// Where a panel is when nobody drags it.
    [[nodiscard]] sf::Vector2f home(std::size_t panel) const {
        return { m_sizes.margin + static_cast<float>(panel) * (m_sizes.panelWidth + m_sizes.margin), m_sizes.margin };
    }

    /// Changes one widget's value; its panel has to be painted again.
    void setValue(std::size_t panel, std::size_t widget, float value) {
        m_values[panel][widget] = value;
        m_batches[panel].markDirty();
    }

    /// Paints the panels that changed, as the UI will before every frame.
    void build(Profiler& profiler) {
        for (std::size_t panel = 0; panel < panelCount; ++panel) {
            if (m_batches[panel].isDirty()) {
                const auto scope = profiler.measure(Section::Build);
                paintPanel(panel);
            }
        }
    }

private:
    [[nodiscard]] float heightOf(WidgetType type) const {
        return type == WidgetType::Graph ? m_sizes.rowHeight * 2.5f : m_sizes.rowHeight;
    }

    [[nodiscard]] PartStyle style(const Part& part, State state = State::Normal) const {
        return m_theme.resolve(part, state);
    }

    void paintPanel(std::size_t panel) {
        const auto layers = m_batches[panel].rebuild();

        Painter frame(layers.frame, { 0.f, 0.f }, m_panelSize);
        frame.box(FloatRect({ 0.f, 0.f }, m_panelSize), style(Panel::Background));
        std::array<char, 24> title{};
        std::snprintf(title.data(), title.size(), "Panel %zu", panel + 1);
        frame.text(
            FloatRect(m_sizes.padding.x, 0.f, m_panelSize.x - m_sizes.padding.x * 2.f, m_sizes.headerHeight),
            title.data(),
            style(Panel::Title)
        );

        const float width = m_panelSize.x - m_sizes.padding.x * 2.f;
        float top = m_sizes.headerHeight + m_sizes.padding.x;
        for (std::size_t widget = 0; widget < widgetsPerPanel; ++widget) {
            const float height = heightOf(widgetTypes[widget]);
            Painter painter(layers.content, { m_sizes.padding.x, top }, { width, height });
            paintWidget(painter, widgetTypes[widget], widgetLabels[widget], m_values[panel][widget]);
            top += height + m_sizes.gap.y;
        }
    }

    void paintWidget(Painter& painter, WidgetType type, std::string_view label, float value) {
        const float width = painter.size().x;
        const float height = painter.size().y;
        const float labelWidth = width * 0.38f;
        const FloatRect labelRect(0.f, 0.f, labelWidth, height);
        std::array<char, 24> number{};

        switch (type) {
            case WidgetType::Slider: {
                const PartStyle track = style(Slider::Track);
                const float valueWidth = 40.f;
                const FloatRect trackRect(labelWidth, height * 0.5f - 5.f, width - labelWidth - valueWidth - 8.f, 10.f);
                const float inset = track.contentInset();
                painter.text(labelRect, label, style(Slider::Label));
                painter.box(trackRect, track);
                painter.box(
                    FloatRect(
                        trackRect.left() + inset,
                        trackRect.top() + inset,
                        (trackRect.width() - inset * 2.f) * value,
                        trackRect.height() - inset * 2.f
                    ),
                    style(Slider::Fill)
                );
                painter.box(
                    FloatRect(trackRect.left() + trackRect.width() * value - 7.f, height * 0.5f - 7.f, 14.f, 14.f),
                    style(Slider::Knob)
                );
                std::snprintf(number.data(), number.size(), "%.2f", static_cast<double>(value));
                painter.text(
                    FloatRect(width - valueWidth, 0.f, valueWidth, height),
                    number.data(),
                    style(Slider::ValueText),
                    Align::Right
                );
                break;
            }
            case WidgetType::Button:
                painter.box(FloatRect(0.f, 0.f, width, height), style(Button::Face));
                painter.text(FloatRect(0.f, 0.f, width, height), label, style(Button::Label), Align::Center);
                break;
            case WidgetType::Switch: {
                const bool on = value > 0.5f;
                const State state = on ? State::Active : State::Normal;
                const FloatRect trackRect(width - 40.f, height * 0.5f - 10.f, 40.f, 20.f);
                painter.text(labelRect, label, style(Switch::Label));
                painter.box(trackRect, style(Switch::Track, state));
                painter.box(
                    FloatRect(trackRect.left() + (on ? 22.f : 4.f), trackRect.top() + 3.f, 14.f, 14.f),
                    style(Switch::Knob, state)
                );
                break;
            }
            case WidgetType::Progress: {
                const PartStyle track = style(ProgressBar::Track);
                const FloatRect trackRect(labelWidth, height * 0.5f - 6.f, width - labelWidth, 12.f);
                const float inset = track.contentInset();
                painter.text(labelRect, label, style(ProgressBar::Label));
                painter.box(trackRect, track);
                painter.box(
                    FloatRect(
                        trackRect.left() + inset,
                        trackRect.top() + inset,
                        (trackRect.width() - inset * 2.f) * value,
                        trackRect.height() - inset * 2.f
                    ),
                    style(ProgressBar::Fill)
                );
                break;
            }
            case WidgetType::Value:
                std::snprintf(number.data(), number.size(), "%.0f", static_cast<double>(value) * 10000.);
                painter.text(labelRect, label, style(ValueDisplay::Label));
                painter.text(
                    FloatRect(labelWidth, 0.f, width - labelWidth, height),
                    number.data(),
                    style(ValueDisplay::ValueText),
                    Align::Right
                );
                break;
            case WidgetType::Dropdown: {
                const FloatRect field(labelWidth, 0.f, width - labelWidth, height);
                const PartStyle arrow = style(Dropdown::Arrow);
                const sf::Vector2f tip(field.right() - 14.f, height * 0.5f + 3.f);
                painter.text(labelRect, label, style(Dropdown::Label));
                painter.box(field, style(Dropdown::Field));
                painter.text(
                    FloatRect(field.left() + 8.f, 0.f, field.width() - 32.f, height),
                    "Largest first",
                    style(Dropdown::Selected)
                );
                painter.line(tip - sf::Vector2f(5.f, 5.f), tip, arrow);
                painter.line(tip, tip + sf::Vector2f(5.f, -5.f), arrow);
                break;
            }
            case WidgetType::Graph: {
                for (std::size_t i = 0; i < graphPoints; ++i) {
                    const float x = static_cast<float>(i) / static_cast<float>(graphPoints - 1);
                    const float wave =
                        std::sin(x * 9.f + value * 40.f) * 0.25f + std::sin(x * 23.f + value * 90.f) * 0.1f;
                    m_curve[i] = { 4.f + x * (width - 8.f), height * (0.5f - wave) };
                }
                painter.box(FloatRect(0.f, 0.f, width, height), style(Graph::Background));
                painter.area(m_curve, height - 4.f, style(Graph::Curve));
                painter.polyline(m_curve, style(Graph::Curve));
                painter.text(FloatRect(8.f, 2.f, width, 18.f), label, style(Graph::Label));
                break;
            }
        }
    }

    Theme m_theme;
    Sizes m_sizes = Layout().sizesAt({ 1280.f, 720.f }); // the reference size: nothing is scaled
    sf::Vector2f m_panelSize;
    std::array<PanelBatch, panelCount> m_batches;
    std::array<PanelBatch*, panelCount> m_list{};
    std::array<std::array<float, widgetsPerPanel>, panelCount> m_values{};
    std::array<sf::Vector2f, graphPoints> m_curve{};
};

struct Result {
    std::string name;
    Profiler::Readout readout;
    double allocationsPerFrame = 0.;
};

struct Options {
    int frames = 2000;
    int idleMilliseconds = 1000;
    std::string screenshot;
};

Options parse(int argc, char** argv) {
    Options options;
    for (int i = 1; i < argc; ++i) {
        const std::string_view argument = argv[i];
        const bool hasValue = i + 1 < argc;
        if (argument == "--frames" && hasValue) {
            options.frames = std::max(1, std::atoi(argv[++i]));
        } else if (argument == "--idle-ms" && hasValue) {
            options.idleMilliseconds = std::max(1, std::atoi(argv[++i]));
        } else if (argument == "--screenshot" && hasValue) {
            options.screenshot = argv[++i];
        } else {
            std::fprintf(stderr, "usage: atpl_render_bench [--frames N] [--idle-ms N] [--screenshot file.png]\n");
            std::exit(2);
        }
    }
    return options;
}

} // namespace

int main(int argc, char** argv) {
    const Options options = parse(argc, argv);

    const auto font = std::make_shared<const sf::Font>(
        Resources::nextToExecutable(argc > 0 ? argv[0] : "").loadFont("fonts/default.ttf")
    );
    Scene scene(font);

    // Room for the panels side by side, and for the readout next to them.
    render::Profiler profiler;
    profiler.setLook(scene.theme(), scene.sizes());
    const sf::Vector2f readoutPosition = { scene.home(panelCount).x, scene.margin() };
    profiler.setPosition(readoutPosition);
    const sf::Vector2u windowSize(
        static_cast<unsigned>(readoutPosition.x + profiler.batch().size().x + scene.margin()),
        static_cast<unsigned>(scene.panelSize().y + scene.margin() * 2.f)
    );

    sf::ContextSettings settings;
    settings.antiAliasingLevel = 4;
    sf::RenderWindow window(
        sf::VideoMode(windowSize), "atpl render bench", sf::Style::Default, sf::State::Windowed, settings
    );
    window.setVerticalSyncEnabled(false); // measure the work, not the display's rhythm

    render::TextCache textCache;
    render::Renderer renderer(&textCache);
    renderer.setProfiler(&profiler);
    frame::RedrawFlag flag;
    const sf::Color background = scene.theme().palette.window;

    std::vector<Result> results;
    bool shapeCallsWithinTarget = true;

    // One pass of the main loop, without waiting.
    const auto pass = [&] {
        while (window.pollEvent().has_value()) {}
        scene.build(profiler);
        const auto stats = renderer.present(window, background, flag, scene.panels());
        if (stats.has_value() && stats->drawCalls - stats->textCalls > 2 * stats->panelsDrawn) {
            shapeCallsWithinTarget = false;
        }
    };

    // Runs a scenario: `change` is what happens to the scene before each frame.
    const auto run = [&](std::string name, int frames, const auto& change) {
        static_cast<void>(profiler.take(Profiler::Clock::now()));
        const std::size_t allocationsBefore = allocations.load();
        for (int f = 0; f < frames; ++f) {
            change(f);
            pass();
        }
        const std::size_t allocated = allocations.load() - allocationsBefore;
        results.push_back(
            { std::move(name), profiler.take(Profiler::Clock::now()), static_cast<double>(allocated) / frames }
        );
    };
    const auto wave = [](int f) { return 0.5f + 0.45f * std::sin(static_cast<float>(f) * 0.05f); };

    // The first frame: every panel painted, every text built.
    run("First frame (everything built)", 1, [](int) {});

    // Idle: the loop of a real application, which sleeps while nothing asks for a frame.
    static_cast<void>(profiler.take(Profiler::Clock::now()));
    const std::size_t drawnBeforeIdle = renderer.framesDrawn();
    const std::clock_t cpuBeforeIdle = std::clock();
    const sf::Clock idleClock;
    int idlePasses = 0;
    while (idleClock.getElapsedTime() < sf::milliseconds(options.idleMilliseconds)) {
        for (auto event = frame::nextEvent(window, flag); event; event = frame::pendingEvent(window, flag)) {}
        scene.build(profiler);
        static_cast<void>(renderer.present(window, background, flag, scene.panels()));
        ++idlePasses;
    }
    const double idleSeconds = static_cast<double>(idleClock.getElapsedTime().asSeconds());
    const double idleCpuSeconds = static_cast<double>(std::clock() - cpuBeforeIdle) / CLOCKS_PER_SEC;
    const std::size_t idleFrames = renderer.framesDrawn() - drawnBeforeIdle;

    run("Simulation running, UI unchanged", options.frames, [&](int) { flag.request(); });
    run("One slider dragged", options.frames, [&](int f) { scene.setValue(0, 2, wave(f)); });
    run("A panel dragged", options.frames, [&](int f) {
        scene.batch(1).setPosition(scene.home(1) + sf::Vector2f(0.f, 20.f * wave(f)));
    });
    scene.batch(1).setPosition(scene.home(1));
    run("A live value and a graph in every panel", options.frames, [&](int f) {
        for (std::size_t panel = 0; panel < panelCount; ++panel) {
            scene.setValue(panel, 0, wave(f + static_cast<int>(panel) * 17));
            scene.setValue(panel, 8, static_cast<float>(f) * 0.0005f);
        }
    });

    // ----- Report -----

    std::printf(
        "Scene: %zu panels, %zu widgets, window %ux%u, anti-aliasing %u, vsync off\n\n",
        panelCount,
        panelCount * widgetsPerPanel,
        windowSize.x,
        windowSize.y,
        window.getSettings().antiAliasingLevel
    );
    std::printf(
        "| Scenario | Frames | Build | Submit | Build + submit | Show | Draw calls (text) | Triangles | Panels rebuilt "
        "| Texts built | Allocations |\n"
    );
    std::printf("|---|---|---|---|---|---|---|---|---|---|---|\n");
    for (const Result& result : results) {
        const Profiler::Readout& r = result.readout;
        std::printf(
            "| %s | %zu | %.3f ms | %.3f ms | **%.3f ms** | %.3f ms | %zu (%zu) | %zu | %.1f | %.1f | %.1f |\n",
            result.name.c_str(),
            r.frames,
            r.build.average,
            r.submit.average,
            r.build.average + r.submit.average,
            r.show.average,
            r.lastFrame.drawCalls,
            r.lastFrame.textCalls,
            r.lastFrame.triangles,
            r.panelsRebuilt,
            r.textsBuilt,
            result.allocationsPerFrame
        );
    }
    std::printf("\nTimes are averages per frame; the last four columns are per frame too.\n");
    std::printf(
        "\nIdle for %.2f s: %zu frames drawn, %d loop passes, %.1f %% of one processor core.\n",
        idleSeconds,
        idleFrames,
        idlePasses,
        100. * idleCpuSeconds / idleSeconds
    );
    std::printf("Shapes: %s two draw calls per panel.\n", shapeCallsWithinTarget ? "at most" : "MORE THAN");

    // ----- A picture of the scene with the readout on -----

    if (!options.screenshot.empty()) {
        profiler.setVisible(true);
        const sf::Clock clock;
        int f = 0;
        while (clock.getElapsedTime() < sf::milliseconds(700)) {
            scene.setValue(0, 2, wave(f++));
            pass();
        }
        sf::Texture capture(window.getSize());
        capture.update(window);
        if (!capture.copyToImage().saveToFile(options.screenshot)) {
            return 2;
        }
    }

    return idleFrames == 0 && shapeCallsWithinTarget ? 0 : 1;
}
