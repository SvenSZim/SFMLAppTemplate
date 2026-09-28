#include <algorithm>
#include <cmath>
#include "ui/core/renderer/renderstyle.hpp"

namespace ui::core::renderer {

static WidgetStyle makeWidgetStyle(sf::Color accent, sf::Color bg, sf::Color textColor) {
    auto darken = [](sf::Color c, float factor) -> sf::Color {
        return sf::Color(
            static_cast<uint8_t>(c.r * factor),
            static_cast<uint8_t>(c.g * factor),
            static_cast<uint8_t>(c.b * factor),
            c.a
        );
    };
    auto lighten = [](sf::Color c, float factor) -> sf::Color {
        return sf::Color(
            static_cast<uint8_t>(std::min(255.f, c.r + (255 - c.r) * factor)),
            static_cast<uint8_t>(std::min(255.f, c.g + (255 - c.g) * factor)),
            static_cast<uint8_t>(std::min(255.f, c.b + (255 - c.b) * factor)),
            c.a
        );
    };

    WidgetStyle style;
    style.idle = {bg, accent, textColor, 1.f};
    style.hovered = {lighten(bg, 0.15f), lighten(accent, 0.2f), textColor, 1.5f};
    style.pressed = {darken(bg, 0.85f), darken(accent, 0.7f), textColor, 2.f};
    style.disabled = {darken(bg, 0.5f), darken(accent, 0.5f), darken(textColor, 0.5f), 1.f};
    style.cornerRadius = 4.f;
    style.height = 28.f;
    return style;
}

RenderStyle::RenderStyle(RenderStyleSetup&& setup) {
    const int n_bgcolors = setup.backgroundColors.size();
    backgroundColor = n_bgcolors == 0 ? sf::Color::White : setup.backgroundColors[0];
    const sf::Color containerBaseBG = n_bgcolors == 0 ? sf::Color::White : (n_bgcolors > 1 ? setup.backgroundColors[1] : setup.backgroundColors[0]);
    roundedRectCornerCount = 16;
    containerStateStyles = {};
    containerStyles = {};

    sf::Color primaryAccent = setup.accentColors.empty() ? sf::Color::White : setup.accentColors[0];
    sf::Color textColor = (backgroundColor.r + backgroundColor.g + backgroundColor.b < 384) ? sf::Color::White : sf::Color::Black;

    if (setup.accentColors.size() == 0 && n_bgcolors == 0) {
        containerStateStyles = { ui::core::container::ContainerStateStyle({0, containerBaseBG, sf::Color::Black, sf::Color::Black, sf::Color::Black}) };
        containerStyles = { ui::core::container::ContainerStyle({0, 10.f, true, 10.f, false, { 0 }}) };
    } else {
        StyleID n_cstates = 0;
        StyleID n_cstyles = 0;
        for (sf::Color accentColor : setup.accentColors) {
            containerStateStyles.push_back(ui::core::container::ContainerStateStyle({
                n_cstates++,
                containerBaseBG,
                accentColor,
                accentColor,
                accentColor == sf::Color::Black ? sf::Color::White : sf::Color::Black
            }));
            containerStyles.push_back(ui::core::container::ContainerStyle({
                n_cstyles++,
                10.f,
                true,
                10.f,
                false,
                { static_cast<StyleID>(n_cstates-1) }
            }));
        }
    }

    buttonStyle = makeWidgetStyle(primaryAccent, containerBaseBG, textColor);
    sliderStyle = makeWidgetStyle(primaryAccent, containerBaseBG, textColor);
    sliderStyle.height = 24.f;
    textInputStyle = makeWidgetStyle(primaryAccent, containerBaseBG, textColor);
    textInputStyle.height = 26.f;
    dropdownStyle = makeWidgetStyle(primaryAccent, containerBaseBG, textColor);
    dropdownStyle.height = 28.f;
    switchStyle = makeWidgetStyle(primaryAccent, containerBaseBG, textColor);
    switchStyle.height = 22.f;
    progressBarStyle = makeWidgetStyle(primaryAccent, containerBaseBG, textColor);
    progressBarStyle.height = 18.f;
    textDisplayStyle = makeWidgetStyle(primaryAccent, containerBaseBG, textColor);
    textDisplayStyle.height = 22.f;
    graphStyle = makeWidgetStyle(primaryAccent, containerBaseBG, textColor);
    graphStyle.height = 80.f;
}

ui::core::container::ContainerStyle& RenderStyle::getContainerStyle(StyleID id) {
    for (auto& style : containerStyles) {
        if (style.id == id) return style;
    }
    return containerStyles[0];
}

ui::core::container::ContainerStateStyle& RenderStyle::getContainerStateStyle(StyleID id) {
    for (auto& style : containerStateStyles) {
        if (style.id == id) return style;
    }
    return containerStateStyles[0];
}

}
