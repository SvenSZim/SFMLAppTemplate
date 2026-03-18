#include "./ui.hpp"

namespace ui {

UI::UI(const UISetup &setup) :
    m_window(sf::VideoMode({setup.windowSize.x, setup.windowSize.y}), setup.windowName),
    m_event_buffer(),
    m_containers()
{
    m_window.setFramerateLimit(144);

    for (const auto &containerSetup : setup.containers) {
        m_containers.emplace_back(ui::Container(std::move(containerSetup)));
    }
}

void UI::update() {
    handleEvents();
    render();
}

void UI::render() {
    m_window.clear(sf::Color(255, 245, 200));
    
    for (auto &container : m_containers) {
        container.render(m_window);
    }

    m_window.display();
}

void UI::handleEvents() {
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

void UI::handleKeyboardInput(const sf::Event &event) {
    switch (event.getIf<sf::Event::KeyPressed>()->code) {
        case sf::Keyboard::Key::Escape:
            m_window.close();
            m_event_buffer.push_back(ui::Event::Closed);
            break;
    }
}

void UI::handleMouseInput(const sf::Event &event) {
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