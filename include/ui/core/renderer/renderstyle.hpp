#ifndef RENDERSTYLE
#define RENDERSTYLE

#include <vector>
#include <SFML/Graphics.hpp>
#include "../container/container.hpp"

namespace ui::core::renderer {

enum class Tier {
    None = 0,
    Low = 1,
    Medium = 2,
    High = 3
};

struct WidgetStateStyle {
    sf::Color fill;
    sf::Color border;
    sf::Color text;
    float borderThickness = 1.f;
};

struct WidgetStyle {
    WidgetStateStyle idle;
    WidgetStateStyle hovered;
    WidgetStateStyle pressed;
    WidgetStateStyle disabled;
    float cornerRadius = 4.f;
    float height = 28.f;
};

struct RenderStyleSetup {
    Tier colorResponsiveness;
    std::vector<sf::Color> backgroundColors;
    std::vector<sf::Color> accentColors;
    Tier hoverColorChange;
};

struct RenderStyle {
    sf::Color backgroundColor;
    uint8_t roundedRectCornerCount;
    std::vector<ui::core::container::ContainerStateStyle> containerStateStyles;
    std::vector<ui::core::container::ContainerStyle> containerStyles;

    WidgetStyle buttonStyle;
    WidgetStyle sliderStyle;
    WidgetStyle textInputStyle;
    WidgetStyle dropdownStyle;
    WidgetStyle switchStyle;
    WidgetStyle progressBarStyle;
    WidgetStyle textDisplayStyle;
    WidgetStyle graphStyle;

    explicit RenderStyle(RenderStyleSetup&& setup);

    ui::core::container::ContainerStyle& getContainerStyle(StyleID id);
    ui::core::container::ContainerStateStyle& getContainerStateStyle(StyleID id);
};

}

#endif