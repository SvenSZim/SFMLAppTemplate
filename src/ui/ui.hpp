#ifndef UIDEF
#define UIDEF

#include <SFML/Graphics.hpp>

#include "./event.hpp"
#include "./core/container.hpp"

namespace ui {

struct UISetup {
    std::string windowName = "SFML App Template";
    sf::Vector2u windowSize = {800, 600};
    std::vector<ui::ContainerSetup> containers = { {} };
};

class UI {
public:
    UI(const UISetup &setup);
    ~UI() = default;

    void update();

    [[nodiscard]]
    sf::RenderWindow &getWindow() { return m_window; }
    [[nodiscard]]
    const std::vector<ui::Event> &getEvents() const { return m_event_buffer; }

private:
    sf::RenderWindow m_window;
    std::vector<ui::Event> m_event_buffer;
    std::vector<ui::Container> m_containers;

    void render();

    void handleEvents();
    void handleKeyboardInput(const sf::Event &event);
    void handleMouseInput(const sf::Event &event);
};

}

#endif