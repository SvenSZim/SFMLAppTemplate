#include "atpl/app/app.hpp"

#include "app/simulation_runner.hpp"

#include <SFML/Graphics/Font.hpp>
#include <SFML/System/Clock.hpp>
#include <SFML/Window/ContextSettings.hpp>
#include <SFML/Window/VideoMode.hpp>
#include <SFML/Window/WindowEnums.hpp>

#include <atomic>
#include <exception>
#include <memory>
#include <utility>

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

/// The UI setup with the font filled in from the resources, if the theme has none.
[[nodiscard]] UISetup withFont(UISetup setup, const Resources& resources, const std::filesystem::path& font) {
    if (setup.theme.font == nullptr) {
        setup.theme.font = std::make_shared<const sf::Font>(resources.loadFont(font));
    }
    return setup;
}

} // namespace

struct App::Impl {
    explicit Impl(AppSetup setup) :
        resources(resourcesFor(setup)),
        quitOnClose(setup.quitOnClose) {
        // The font first: a missing resource is reported before a window appears.
        UISetup ui = withFont(std::move(setup.ui), resources, setup.font);
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
