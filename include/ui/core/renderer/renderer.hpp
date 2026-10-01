#ifndef RENDERER
#define RENDERER

#include <vector>
#include <SFML/Graphics.hpp>

#include "../../utils/rect.hpp"
#include "../container/container.hpp"
#include "../widgets/widget_manager.hpp"
#include "./renderstyle.hpp"
#include "./renderstyle_templates.hpp"
#include "./rendercache.hpp"
#include "./text.hpp"
#include "./widget_renderers.hpp"

namespace ui::core::renderer {

using ui::utils::FloatRect;
using ui::core::container::Container;
using ui::core::container::ContainerStyle;
using ui::core::container::ContainerStateStyle;
using ui::core::widgets::WidgetManager;

class Renderer {
private:
    RenderStyle m_style;
    RenderCache m_cache;
    TextRenderer m_text;

    void renderEnvironment(sf::RenderWindow& window);
    void updateContainer(Container& container);
    void updateWidgets(WidgetManager& widgets);
public:
    Renderer(RenderStyle&& style, const std::string& fontPath = "resources/fonts/default.ttf");
    Renderer(RenderStyles style, const std::string& fontPath = "resources/fonts/default.ttf");

    [[nodiscard]]
    const TextRenderer& text() const { return m_text; }
    [[nodiscard]]
    TextRenderer& text() { return m_text; }

    void initStyles(std::vector<Container>& container);

    void switchStyle(RenderStyle&& style, std::vector<Container>& container);
    void switchStyle(RenderStyles style, std::vector<Container>& container);

    void update(std::vector<Container>& container, WidgetManager& widgets);
    void render(sf::RenderWindow& window);
};

}

#endif