#ifndef UIDEF
#define UIDEF

#include <vector>
#include <string>
#include <SFML/Graphics.hpp>

#include "./core/event.hpp"
#include "./core/interaction.hpp"
#include "./core/container/container_manager.hpp"
#include "./core/widgets/widget_manager.hpp"
#include "./core/widgets/widget_factory.hpp"
#include "./core/layout/layout_manager.hpp"
#include "./core/renderer/renderer.hpp"

namespace ui {

using ui::core::WidgetID;
using ui::core::ContainerID;
using ui::utils::FloatRect;
using ui::core::container::ContainerManager;
using ui::core::container::ContainerSetup;
using ui::core::container::InnerLayout;
using ui::core::widgets::WidgetManager;
using ui::core::widgets::Widget;
using ui::core::widgets::WidgetSize;
using ui::core::layout::LayoutStyle;
using ui::core::layout::LayoutManager;
using ui::core::layout::OverflowMode;
using ui::core::layout::GridConfig;
using ui::core::renderer::RenderStyles;
using ui::core::renderer::Renderer;

struct UISetup {
    std::string windowName = "SFML App Template";
    sf::Vector2u windowSize = {800, 600};
    std::string fontPath = "resources/fonts/default.ttf";
    LayoutStyle layoutTemplate = LayoutStyle::Floating;
    OverflowMode overflowMode = OverflowMode::SpacingThenScroll;
    GridConfig gridConfig = {};
    RenderStyles designTemplate = RenderStyles::Colorful;
    std::vector<ContainerSetup> containers = {};
};

class UIManager {
public:
    UIManager(UISetup&& setup);
    ~UIManager() = default;

    void update();
    void render();

    [[nodiscard]]
    sf::RenderWindow &getWindow() { return m_window; }
    [[nodiscard]]
    const std::vector<ui::core::Event> &getEvents() const { return m_event_buffer; }
    void clearEvents() { m_event_buffer.clear(); }

    // Widget value access
    [[nodiscard]] WidgetManager& widgets() { return m_widgets; }
    [[nodiscard]] const WidgetManager& widgets() const { return m_widgets; }
    [[nodiscard]] WidgetID widgetId(const std::string& name) const { return m_widgets.getIdByName(name); }

    // Convenience accessors
    float getSliderValue(WidgetID id) const { return m_widgets.getSliderValue(id); }
    bool getSwitchValue(WidgetID id) const { return m_widgets.getSwitchValue(id); }
    void setTextDisplay(WidgetID id, const std::string& text) { m_widgets.setTextDisplay(id, text); }
    void setProgressBar(WidgetID id, float value) { m_widgets.setProgressBar(id, value); }
    void pushGraphData(WidgetID id, float value) { m_widgets.pushGraphData(id, value); }

private:
    sf::RenderWindow m_window;
    std::vector<ui::core::Event> m_event_buffer;
    ui::core::InteractionState m_interaction;
    ContainerManager m_containers;
    WidgetManager m_widgets;
    LayoutManager m_layout_manager;
    Renderer m_renderer;

    void handleEvents();
    void handleKeyboardInput(const sf::Event &event);
    void handleMouseInput(const sf::Event &event);
};

}

#endif
