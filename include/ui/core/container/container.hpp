#ifndef CONTAINER
#define CONTAINER

#include <cstdint>
#include <string>

#include <SFML/Graphics.hpp>

#include "../../utils/rect.hpp"
#include "../types.hpp"

namespace ui::core::container {

using ui::utils::FloatRect;
using ui::utils::FloatAnimRect;

enum class ContainerState : uint8_t {
    // meta/error 0-31
    DISABLED                = 0b00000000,
    // collapsed 32-63
    COLLAPSED               = 0b00100000,
    COLLAPSED_HOVERED       = 0b00100001,
    // expanded/default 64-127
    EXPANDED                = 0b01000000,
    EXPANDED_HOVERED        = 0b01000001
    // additional types 128-255
};

enum class InnerLayout : uint8_t {
    // basic layouts 0-31
    Vertical            = 0b00000000,
    TwoColumn           = 0b00000001,
    ThreeColumn         = 0b00000010
    // 1 focus element 32-47
    // 2 focus elements 48-63
    // additional layouts 64-255
};

struct Container {
    ContainerID id;
    std::string title;
    FloatAnimRect rect;
    LayoutID layoutId;
    StyleID styleId;
    WidgetID firstWidget;
    uint8_t widgetCount;
    InnerLayout innerLayout;
    ContainerState state;
    DirtyFlag flag;
    
    void update();
    void switchState(ContainerState new_state);
};

struct ContainerStateStyle {
    StyleID id;
    sf::Color fillColor;
    sf::Color outlineColorOuter;
    sf::Color outlineColorInner;
    sf::Color borderColor;
};

struct ContainerStyle {
    StyleID id;
    float cornerRadius;
    bool reduceInnerCornerRadius;

    float outlineThickness;

    bool outlineBorder;
    std::vector<StyleID> stateStyles;
};

}

#endif
