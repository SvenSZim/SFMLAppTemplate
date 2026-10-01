// The smallest application: a window with one line of text.
//
// Until the public API exists (Phase 1) this talks to SFML directly. It already follows the
// rules the template will enforce: anti-aliasing on, vsync on, no work while nothing happens,
// resources found next to the executable, and a clear error if one is missing.
//
// Run with --smoke-test to draw a few frames and exit, for automated checks.

#include "atpl/app/resources.hpp"
#include "atpl/core/version.hpp"

#include <SFML/Graphics.hpp>

#include <exception>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>

namespace {

int run(int argc, char* argv[]) {
    const bool smokeTest = argc > 1 && std::string_view(argv[1]) == "--smoke-test";
    const std::string title = "atpl minimal " + std::string(atpl::versionString());

    const atpl::Resources resources = atpl::Resources::nextToExecutable(argv[0]);
    const sf::Font font = resources.loadFont("fonts/default.ttf");

    sf::ContextSettings settings;
    settings.antiAliasingLevel = 8;

    sf::RenderWindow window(sf::VideoMode({1280, 720}), title, sf::Style::Default, sf::State::Windowed, settings);
    window.setVerticalSyncEnabled(true);

    const sf::Color background(36, 34, 32);
    sf::Text label(font, title + "  -  press Escape to close", 18);
    label.setFillColor(sf::Color(234, 229, 223));
    label.setPosition({24.f, 20.f});

    // Nothing on screen changes by itself, so the loop sleeps until an event arrives.
    // The timeout only bounds how long a missed redraw could stay on screen.
    const sf::Time idleWait = sf::milliseconds(250);

    const auto handle = [&window](const sf::Event& event) {
        if (event.is<sf::Event::Closed>()) {
            window.close();
        } else if (const auto* key = event.getIf<sf::Event::KeyPressed>()) {
            if (key->code == sf::Keyboard::Key::Escape) {
                window.close();
            }
        } else if (const auto* resized = event.getIf<sf::Event::Resized>()) {
            // Keep one unit per pixel instead of stretching the old view.
            window.setView(sf::View(sf::FloatRect({0.f, 0.f}, sf::Vector2f(resized->size))));
        }
    };

    int framesLeft = 3; // only counted in a smoke test
    while (window.isOpen()) {
        if (!smokeTest) {
            if (const std::optional event = window.waitEvent(idleWait)) {
                handle(*event);
            }
        }
        while (const std::optional event = window.pollEvent()) {
            handle(*event);
        }

        window.clear(background);
        window.draw(label);
        window.display();

        if (smokeTest && --framesLeft == 0) {
            window.close();
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
