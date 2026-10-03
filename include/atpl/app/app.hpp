#pragma once

#include "atpl/app/resources.hpp"
#include "atpl/app/simulation.hpp"
#include "atpl/ui/ui.hpp"

#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/System/Vector2.hpp>

#include <filesystem>
#include <functional>
#include <memory>
#include <string>

namespace atpl {

/// The application's window.
struct WindowSetup {
    std::string title = "Application";
    sf::Vector2u size = { 1280u, 720u };

    /// The smallest size the user can resize the window to. { 0, 0 }: no limit.
    sf::Vector2u minimumSize = { 0u, 0u };

    bool resizable = true;

    /// Fills the screen at the desktop's resolution; `size` is not used then.
    bool fullscreen = false;

    /// Samples per pixel for smooth edges. 0: off.
    unsigned int antiAliasing = 8;

    /// Draw in step with the display. Leave this on unless there is a reason not to.
    bool vsync = true;
};

/// Everything needed to start an application.
struct AppSetup {
    WindowSetup window;
    UISetup ui;

    /// Where the application's resources are. Empty: the `resources` directory next to the
    /// executable, where the build puts them.
    std::filesystem::path resources;

    /// The font used if `ui.theme.font` is not set, relative to the resource directory.
    std::filesystem::path font = "fonts/default.ttf";

    /// End `run()` when the user closes the window. Turn this off to decide yourself, for example
    /// to ask before quitting; the request still arrives as a `WindowClosed` event.
    bool quitOnClose = true;
};

/// An application: a window, its UI, and the loop that keeps both alive.
///
///     atpl::App app({
///         .window = {.title = "Particles"},
///         .ui = {.background = "world", .panels = { ... }},
///     });
///     app.ui().view("world").onDraw([&](sf::RenderTarget& target, sf::Vector2f) { draw(target, simulation.state());
///     }); app.onEvent([&](const atpl::Event& event) {
///         if (event.isButton("Reset")) simulation.send(Command::Reset);
///     });
///     return app.run(simulation);
///
/// Everything here is for the main thread, except `quit()`.
class App {
public:
    /// Creates the window and the UI. Throws `ResourceError` if the font cannot be loaded and
    /// `SetupError` if the UI setup is invalid.
    explicit App(AppSetup setup);
    ~App();

    App(const App&) = delete;
    App(App&&) = delete;
    App& operator=(const App&) = delete;
    App& operator=(App&&) = delete;

    [[nodiscard]] UI& ui();
    [[nodiscard]] sf::RenderWindow& window();
    [[nodiscard]] const Resources& resources() const;

    /// Sets the function that receives every event from the UI: widget events and the input the
    /// UI had no use for. Called on the main thread, once per event, in order. This is where the
    /// application decides what the simulation should hear about. Replaces an earlier function.
    void onEvent(std::function<void(const Event&)> handler);

    /// Sets a function called once per pass of the main loop, after events and before the UI is
    /// updated, with the real time since the last call in seconds. For main-thread work that is
    /// not a reaction to an event. Replaces an earlier function.
    ///
    /// While nothing happens the loop waits for input, but never longer than one display frame,
    /// so that what other threads ask to show appears without delay: an idle application calls
    /// this about once per display frame. Do not use it as a clock; use `dt`.
    void onUpdate(std::function<void(float dt)> handler);

    /// Runs the application without a simulation until `quit()` or until the window is closed.
    /// Returns the exit code.
    int run();

    /// Runs the application with `simulation` on its own thread: starts it, keeps the UI going,
    /// and stops and joins it when the application ends. An exception thrown by the simulation
    /// ends the run and is thrown again here, on the main thread.
    int run(SimulationBase& simulation);

    /// Ends `run()` after the current pass. May be called from any thread.
    void quit(int exitCode = 0);

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace atpl
