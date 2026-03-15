#include "./UIHandler.hpp"

namespace ui {

UIHandler::UIHandler(const UISetup &setup) :
    m_window(sf::VideoMode({setup.windowSize.x, setup.windowSize.y}), setup.windowName)
{
    m_window.setFramerateLimit(144);
}

void UIHandler::update() {
    handleEvents();
    render();
}

void UIHandler::render() {
    m_window.clear(sf::Color::Black);
    m_window.display();
}

void UIHandler::handleEvents() {
    while (const std::optional event = m_window.pollEvent()) {
        if (event->is<sf::Event::Closed>()) {
            m_window.close();
            m_event_buffer.push_back(ui::Event::Closed);
        } else if (event->is<sf::Event::KeyPressed>()) {
            handleKeyboardInput(*event);
        } else if (event->is<sf::Event::MouseButtonPressed>() || event->is<sf::Event::MouseButtonReleased>() || event->is<sf::Event::MouseMoved>()) {
            handleMouseInput(*event);
        }
    }
}

void UIHandler::handleKeyboardInput(const sf::Event &event) {
    switch (event.getIf<sf::Event::KeyPressed>()->code) {
        case sf::Keyboard::Key::Space:
            break;
    }
}

void UIHandler::handleMouseInput(const sf::Event &event) {
    if (event.is<sf::Event::MouseButtonPressed>())
    {
        const auto &mouseEvent = *event.getIf<sf::Event::MouseButtonPressed>();
        switch (mouseEvent.button)
        {
            case sf::Mouse::Button::Left:
                break;
            case sf::Mouse::Button::Right:
                break;
            case sf::Mouse::Button::Middle:
                break;
            default:
                break;
        }
    } else if (event.is<sf::Event::MouseMoved>())
    {
        const auto &mouseEvent = *event.getIf<sf::Event::MouseMoved>();
        // mouseEvent.position
    }
}

}