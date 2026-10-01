#include "ui/ui_manager.hpp"

namespace ui {

using ui::core::widgets::WidgetType;

UIManager::UIManager(UISetup&& setup) :
    m_window(sf::VideoMode({setup.windowSize.x, setup.windowSize.y}), setup.windowName),
    m_event_buffer(),
    m_containers(),
    m_widgets(),
    m_layout_manager(setup.layoutTemplate),
    m_renderer(setup.designTemplate, setup.fontPath)
{
    m_layout_manager.setOverflowMode(setup.overflowMode);
    m_layout_manager.setGridConfig(setup.gridConfig);

    for (auto& contSetup : setup.containers) {
        ContainerID cid = m_containers.createContainer({
            .title = contSetup.title,
            .rect = contSetup.rect,
            .innerLayout = contSetup.innerLayout,
            .layoutId = contSetup.layoutId,
            .styleId = contSetup.styleId
        });

        for (const auto& wd : contSetup.widgets) {
            switch (wd.type) {
                case WidgetType::Button:
                    m_widgets.createButton(cid, wd.name, wd.size);
                    break;
                case WidgetType::Slider:
                    m_widgets.createSlider(cid, wd.name, wd.min, wd.max, wd.initial, wd.size);
                    break;
                case WidgetType::TextInput:
                    m_widgets.createTextInput(cid, wd.name, wd.size);
                    break;
                case WidgetType::Dropdown:
                    m_widgets.createDropdown(cid, wd.name, wd.options, wd.size);
                    break;
                case WidgetType::Switch:
                    m_widgets.createSwitch(cid, wd.name, wd.initialBool, wd.size);
                    break;
                case WidgetType::ProgressBar:
                    m_widgets.createProgressBar(cid, wd.name, wd.size);
                    break;
                case WidgetType::TextDisplay:
                    m_widgets.createTextDisplay(cid, wd.name, wd.initialText, wd.size);
                    break;
                case WidgetType::Graph:
                    m_widgets.createGraph(cid, wd.name, wd.size);
                    break;
            }
        }
    }

    FloatRect screen = { {0, 0}, static_cast<sf::Vector2f>(m_window.getSize()) };
    m_layout_manager.generateLayout(screen, m_containers.getAllContainer());
    m_renderer.initStyles(m_containers.getAllContainer());
}

void UIManager::update() {
    handleEvents();
    m_containers.update();
    m_widgets.update(m_event_buffer);
    m_layout_manager.update(m_containers.getAllContainer());
    render();
}

void UIManager::render() {
    m_renderer.update(m_containers.getAllContainer(), m_widgets);
    m_renderer.render(m_window);
}

void UIManager::handleEvents() {
    while (const std::optional event = m_window.pollEvent()) {
        if (event->is<sf::Event::Closed>()) {
            m_window.close();
            m_event_buffer.push_back(ui::core::Event::closed());
        } else if (event->is<sf::Event::KeyPressed>()) {
            handleKeyboardInput(*event);
        } else if (event->is<sf::Event::MouseButtonPressed>() ||
                   event->is<sf::Event::MouseButtonReleased>() ||
                   event->is<sf::Event::MouseMoved>()) {
            handleMouseInput(*event);
        } else if (event->is<sf::Event::Resized>()) {
            const auto& resizeEvent = *event->getIf<sf::Event::Resized>();
            FloatRect newScreen = {0, 0, static_cast<float>(resizeEvent.size.x), static_cast<float>(resizeEvent.size.y)};
            m_layout_manager.onResize(newScreen, m_containers.getAllContainer());
        }
    }
}

void UIManager::handleKeyboardInput(const sf::Event &event) {
    switch (event.getIf<sf::Event::KeyPressed>()->code) {
        case sf::Keyboard::Key::Escape:
            m_window.close();
            m_event_buffer.push_back(ui::core::Event::closed());
            break;
        default:
            break;
    }
}

void UIManager::handleMouseInput(const sf::Event &event) {
    if (event.is<sf::Event::MouseButtonPressed>()) {
        const auto &mouseEvent = *event.getIf<sf::Event::MouseButtonPressed>();
        sf::Vector2f pos(static_cast<float>(mouseEvent.position.x),
                         static_cast<float>(mouseEvent.position.y));

        if (mouseEvent.button == sf::Mouse::Button::Left) {
            ContainerID hit = ui::core::hitTestContainers(pos, m_containers.getAllContainer());
            m_interaction.pressedContainer = hit;
            if (hit != 0) {
                auto& cont = m_containers.getContainer(hit);
                cont.toggle();
            }
        }
    } else if (event.is<sf::Event::MouseButtonReleased>()) {
        m_interaction.pressedContainer = 0;
        m_interaction.pressedWidget = 0;
    } else if (event.is<sf::Event::MouseMoved>()) {
        const auto &mouseEvent = *event.getIf<sf::Event::MouseMoved>();
        sf::Vector2f pos(static_cast<float>(mouseEvent.position.x),
                         static_cast<float>(mouseEvent.position.y));
        m_interaction.mousePos = pos;

        ContainerID prevHover = m_interaction.hoveredContainer;
        ContainerID newHover = ui::core::hitTestContainers(pos, m_containers.getAllContainer());
        m_interaction.hoveredContainer = newHover;

        if (prevHover != newHover) {
            if (prevHover != 0) {
                auto& prev = m_containers.getContainer(prevHover);
                prev.state.state.hovered = 0;
                prev.flag.flags.visual = 1;
            }
            if (newHover != 0) {
                auto& curr = m_containers.getContainer(newHover);
                curr.state.state.hovered = 1;
                curr.flag.flags.visual = 1;
            }
        }
    }
}

}
