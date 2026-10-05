#include "atpl/app/app.hpp"

#include "app/simulation_runner.hpp"

#include <SFML/Graphics/Font.hpp>
#include <SFML/System/Clock.hpp>
#include <SFML/Window/ContextSettings.hpp>
#include <SFML/Window/VideoMode.hpp>
#include <SFML/Window/WindowEnums.hpp>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <exception>
#include <memory>
#include <string_view>
#include <utility>
#include <variant>

namespace atpl {

namespace {

/// The resource directory: as the setup says, or next to the executable.
[[nodiscard]] Resources resourcesFor(const AppSetup& setup) {
    return setup.resources.empty() ? Resources::nextToExecutable() : Resources(setup.resources);
}

/// Opens the window as the setup says.
void open(sf::RenderWindow& window, const WindowSetup& setup) {
    sf::ContextSettings settings;
    settings.antiAliasingLevel = setup.antiAliasing;

    std::uint32_t style = sf::Style::Titlebar | sf::Style::Close;
    if (setup.resizable) {
        style |= sf::Style::Resize;
    }
    const sf::VideoMode mode = setup.fullscreen ? sf::VideoMode::getDesktopMode() : sf::VideoMode(setup.size);
    window.create(mode, setup.title, style, setup.fullscreen ? sf::State::Fullscreen : sf::State::Windowed, settings);
    window.setVerticalSyncEnabled(setup.vsync);
    if (setup.minimumSize.x > 0 || setup.minimumSize.y > 0) {
        window.setMinimumSize(setup.minimumSize);
    }
}

/// The UI setup with fonts from the resources: a loader for the fonts themes name, each file
/// loaded once and kept by `resources`, which outlives the UI; and the default font if the theme
/// has none, which the UI keeps for themes that neither set nor name one.
[[nodiscard]] UISetup withFont(UISetup setup, const Resources& resources, const std::filesystem::path& font) {
    if (!setup.fonts) {
        setup.fonts = [&resources](std::string_view name) {
            const sf::Font& loaded = resources.font(std::filesystem::path(name));
            return std::shared_ptr<const sf::Font>(&loaded, [](const sf::Font*) {}); // kept by the resources
        };
    }
    if (setup.theme.font == nullptr) { // also for themes that name theirs: kept for those that do not
        setup.theme.font = std::make_shared<const sf::Font>(resources.loadFont(font));
    }
    return setup;
}

/// The factor the setup asks for.
[[nodiscard]] float scaleOf(const AppSetup& setup) {
    if (std::holds_alternative<AutoScale>(setup.scale)) {
        return autoScale(sf::Vector2u(sf::VideoMode::getDesktopMode().size));
    }
    return std::clamp(std::get<float>(setup.scale), UI::minScale, UI::maxScale);
}

/// `size` times the GUI scale, in whole pixels.
[[nodiscard]] sf::Vector2u scaled(sf::Vector2u size, float scale) {
    return { static_cast<unsigned int>(std::lround(static_cast<float>(size.x) * scale)),
             static_cast<unsigned int>(std::lround(static_cast<float>(size.y) * scale)) };
}

} // namespace

float autoScale(sf::Vector2u desktop) {
    const float quarters = std::round(static_cast<float>(desktop.y) / 1080.f * 4.f);
    return std::clamp(quarters / 4.f, 1.f, 3.f);
}

struct App::Impl {
    explicit Impl(AppSetup setup) :
        resources(resourcesFor(setup)),
        quitOnClose(setup.quitOnClose) {
        // The font first: a missing resource is reported before a window appears.
        UISetup ui = withFont(std::move(setup.ui), resources, setup.font);
        // The GUI scale: for the UI's sizes, and for the window, so that it keeps its size on the
        // screen.
        const float scale = scaleOf(setup);
        ui.layout.metrics.scale *= scale;
        setup.window.size = scaled(setup.window.size, scale);
        setup.window.minimumSize = scaled(setup.window.minimumSize, scale);
        open(window, setup.window);
        interface = std::make_unique<UI>(window, std::move(ui));
    }

    /// One pass of the main loop: input and events, the application's update, the UI's update,
    /// and a frame if one is needed. Waits for input when nothing else is to do. With a
    /// simulation, its newest state is taken once, after the wait: the whole pass, and the frame
    /// it draws, show that one moment (D33).
    void pass(app::SimulationRunner* runner) {
        interface->handleInput();
        if (runner != nullptr) {
            runner->showNewestState();
        }
        for (const Event& event : interface->events()) {
            if (eventHandler) {
                eventHandler(event);
            }
            if (event.is<WindowClosed>() && quitOnClose) {
                requestQuit(0);
            }
        }
        const float dt = clock.restart().asSeconds();
        if (updateHandler) {
            updateHandler(dt);
        }
        interface->update();
        interface->draw();
    }

    void requestQuit(int code) {
        exitCode.store(code, std::memory_order_relaxed);
        quitting.store(true, std::memory_order_release);
        interface->requestRedraw(); // the loop does not wait for input once more
    }

    Resources resources;
    bool quitOnClose;
    sf::RenderWindow window;
    std::unique_ptr<UI> interface;

    std::function<void(const Event&)> eventHandler;
    std::function<void(float)> updateHandler;
    sf::Clock clock;

    std::atomic<bool> quitting{ false };
    std::atomic<int> exitCode{ 0 };
};

App::App(AppSetup setup) :
    m_impl(std::make_unique<Impl>(std::move(setup))) {}

App::~App() = default;

UI& App::ui() {
    return *m_impl->interface;
}

sf::RenderWindow& App::window() {
    return m_impl->window;
}

const Resources& App::resources() const {
    return m_impl->resources;
}

void App::onEvent(std::function<void(const Event&)> handler) {
    m_impl->eventHandler = std::move(handler);
}

void App::onUpdate(std::function<void(float dt)> handler) {
    m_impl->updateHandler = std::move(handler);
}

int App::run() {
    Impl& impl = *m_impl;
    impl.clock.restart();
    while (impl.window.isOpen() && !impl.quitting.load(std::memory_order_acquire)) {
        impl.pass(nullptr);
    }
    impl.quitting.store(false, std::memory_order_release); // a later run() starts afresh
    return impl.exitCode.load(std::memory_order_relaxed);
}

int App::run(SimulationBase& simulation) {
    Impl& impl = *m_impl;
    app::SimulationRunner runner(simulation, [&impl] { impl.interface->requestRedraw(); });
    runner.start();
    impl.clock.restart();
    while (impl.window.isOpen() && !impl.quitting.load(std::memory_order_acquire) && runner.failure() == nullptr) {
        impl.pass(&runner);
    }
    runner.stop(); // the simulation thread ends before anything it uses goes away
    impl.quitting.store(false, std::memory_order_release);
    if (const std::exception_ptr failure = runner.failure()) {
        std::rethrow_exception(failure); // on the main thread, from run()
    }
    return impl.exitCode.load(std::memory_order_relaxed);
}

void App::quit(int exitCode) {
    m_impl->requestQuit(exitCode);
}

} // namespace atpl
