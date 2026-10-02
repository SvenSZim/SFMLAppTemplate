// The smallest application that shows the UI: a window with a few empty panels.
//
// It drives the UI by hand, which is what `App` will do for an application once it exists
// (Phase 4): read input, update, draw; and nothing at all while nothing happens.
//
//   minimal [--profiler] [--smoke-test]
//
// --profiler shows the profiler readout. --smoke-test draws a few frames and exits, for
// automated checks.

#include "atpl/app/resources.hpp"
#include "atpl/core/version.hpp"
#include "atpl/ui/ui.hpp"

#include <SFML/Graphics/RenderWindow.hpp>

#include <exception>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>

namespace {

using namespace atpl;

UISetup describeUI(std::shared_ptr<const sf::Font> font, bool profiler) {
    UISetup setup{
        // Two panels in the window's grid, as an application with its simulation in a panel
        // would have them ...
        .grid = {.columns = 4, .rows = 1},
        .panels = {
            {
                .name = "Scene",
                .placement = GridCell{.column = 0, .columnSpan = 3},
                .collapsible = false,
            },
            { .name = "Inspector", .placement = GridCell{.column = 3} },
            // ... and three that float on top of them.
            { .name = "Controls", .placement = Anchor::Left },
            { .name = "Statistics", .placement = Anchor::Left },
            { .name = "Playback", .placement = Anchor::Bottom, .width = 420.f },
        },
        .profiler = profiler,
    };
    setup.theme.font = std::move(font);
    return setup;
}

int run(int argc, char* argv[]) {
    bool smokeTest = false;
    bool profiler = false;
    for (int i = 1; i < argc; ++i) {
        const std::string_view argument = argv[i];
        smokeTest = smokeTest || argument == "--smoke-test";
        profiler = profiler || argument == "--profiler";
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

    UI ui(window, describeUI(font, profiler));

    int passesLeft = 5; // only counted in a smoke test
    while (window.isOpen()) {
        ui.handleInput(); // sleeps while there is nothing to do
        for (const Event& event : ui.events()) {
            if (event.is<WindowClosed>()) {
                window.close();
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
