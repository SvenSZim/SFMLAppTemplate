#ifndef UIHANDLER
#define UIHANDLER

#include <SFML/Graphics.hpp>

#include "./event.hpp"

namespace ui {

struct UISetup {
    std::string windowName = "SFML App Template";
    sf::Vector2u windowSize = {800, 600};
};

class UIHandler {
public:
    UIHandler(const UISetup &setup);
    ~UIHandler() = default;

    void update();

    [[nodiscard]]
    sf::RenderWindow &getWindow() { return m_window; }
    [[nodiscard]]
    const std::vector<ui::Event> &getEvents() const { return m_event_buffer; }

private:
    sf::RenderWindow m_window;
    std::vector<ui::Event> m_event_buffer;

    void render();

    void handleEvents();
    void handleKeyboardInput(const sf::Event &event);
    void handleMouseInput(const sf::Event &event);
};

}

#endif