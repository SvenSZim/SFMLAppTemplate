// How the application reads events.
//
// Compiled with every build, never linked or run. See core_usage.cpp for why the namespace is named.

#include "atpl/core/queue.hpp"
#include "atpl/ui/ui.hpp"

#include <string>

namespace event_usage {

using namespace atpl;

enum class Mode { Normal, Debug, Wireframe };
enum class Command { Reset, Restart, PlaceWall };

struct Camera {
    sf::Vector2f offset;
    float zoom = 1.f;
    bool dragging = false;
};

// One frame's worth of events, on the main thread.
bool handleEvents(UI& ui, Queue<Command>& commands, Camera& camera, std::string& typed) {
    bool keepRunning = true;

    for (const Event& event : ui.events()) {
        // Widget events, by name.
        if (event.isButton("Reset")) {
            commands.push(Command::Reset);
        }
        if (event.isButton("Controls/Reset")) { // with the panel, where a name alone is ambiguous
            commands.push(Command::Reset);
        }

        // A changed value. `final` tells a finished interaction from a drag in progress.
        if (const ValueChanged* change = event.changeOf("Particles")) {
            if (change->final) {
                commands.push(Command::Restart); // expensive: only once the slider is let go
            }
            const int count = change->as<int>();
            (void)count;
        }
        if (const ValueChanged* change = event.changeOf("Mode")) {
            const Mode mode = change->as<Mode>();
            (void)mode;
        }

        // Forwarded input: a drag in the main view moves the camera, the wheel zooms.
        if (const auto* press = event.getIf<PointerPressed>()) {
            if (press->button == sf::Mouse::Button::Left && press->pointer.isIn("world")) {
                camera.dragging = true;
            }
            if (press->button == sf::Mouse::Button::Right && press->pointer.isIn("world")) {
                commands.push(Command::PlaceWall); // at press->pointer.inView
            }
        }
        if (event.is<PointerReleased>()) {
            camera.dragging = false;
        }
        if (const auto* move = event.getIf<PointerMoved>()) {
            if (camera.dragging) {
                camera.offset -= move->delta / camera.zoom;
            }
        }
        if (const auto* scroll = event.getIf<Scrolled>()) {
            if (scroll->pointer.isIn("world") && !scroll->horizontal) {
                camera.zoom *= scroll->delta > 0.f ? 1.1f : 1.f / 1.1f;
            }
        }

        // Keys the UI did not need.
        if (event.isKey(sf::Keyboard::Key::Escape)) {
            keepRunning = false;
        }
        if (const auto* key = event.getIf<KeyPressed>()) {
            if (key->key == sf::Keyboard::Key::R && key->modifiers.control) {
                commands.push(Command::Restart);
            }
        }
        if (const auto* text = event.getIf<TextEntered>()) {
            if (text->character < 128) {
                typed += static_cast<char>(text->character);
            }
        }

        // The window.
        if (event.is<WindowClosed>()) {
            keepRunning = false;
        }
    }

    return keepRunning;
}

// The same with a visitor, for code that wants to handle every type in one place.
int countPointerEvents(UI& ui) {
    int count = 0;
    for (const Event& event : ui.events()) {
        count += event.visit([](const auto& data) {
            using T = std::remove_cvref_t<decltype(data)>;
            return (std::same_as<T, PointerPressed> || std::same_as<T, PointerReleased> ||
                    std::same_as<T, PointerMoved>)
                       ? 1
                       : 0;
        });
    }
    return count;
}

} // namespace event_usage
