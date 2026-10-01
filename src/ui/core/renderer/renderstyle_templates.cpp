#include "ui/core/renderer/renderstyle_templates.hpp"

namespace ui::core::renderer {

RenderStyle getRenderStyle(RenderStyles style) {
    switch (style) {
    case RenderStyles::Colorful:
        return RenderStyle({
            Tier::None,
            {
                sf::Color(0xA52A2A),
                sf::Color(0xB63B3B)
            },
            {
                sf::Color::Red,
                sf::Color::Yellow,
                sf::Color::Magenta,
                sf::Color::Green,
                sf::Color::Blue
            },
            Tier::Medium
        });
    case RenderStyles::Moon:
        return RenderStyle({
            Tier::None,
            {
                sf::Color::Black,
                sf::Color(0x941919)
            },
            {
                sf::Color::White
            },
            Tier::Medium
        });
    default:
        return RenderStyle({
            Tier::None,
            {
                sf::Color::Black,
                sf::Color(0x941919)
            },
            {
                sf::Color::White
            },
            Tier::Medium
        });
    }
}

}