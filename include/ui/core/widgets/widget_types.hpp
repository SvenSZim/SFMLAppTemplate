#ifndef WIDGET_TYPES
#define WIDGET_TYPES

#include <string>
#include <vector>
#include <cstdint>

#include "../types.hpp"
#include "../../utils/rect.hpp"

namespace ui::core::widgets {

using ui::utils::FloatRect;
using ui::utils::FloatAnimRect;

enum class WidgetType : uint8_t {
    Button = 0,
    Slider = 1,
    TextInput = 2,
    Dropdown = 3,
    Switch = 4,
    ProgressBar = 5,
    TextDisplay = 6,
    Graph = 7
};

enum class WidgetSize : uint8_t {
    Small = 0,
    Normal = 1,
    Large = 2
};

enum class ButtonState : uint8_t {
    Idle = 0,
    Hovered = 1,
    Pressed = 2,
    Disabled = 3
};

struct ButtonWidget {
    WidgetID id;
    ContainerID container;
    ButtonState state = ButtonState::Idle;
    StyleID style = 0;
    std::string label;
    WidgetSize sizeHint = WidgetSize::Normal;
    FloatRect rect;
    DirtyFlag flag = {0};
};

struct SliderWidget {
    WidgetID id;
    ContainerID container;
    float value = 0.f;
    float min = 0.f;
    float max = 1.f;
    StyleID style = 0;
    std::string name;
    WidgetSize sizeHint = WidgetSize::Normal;
    FloatRect rect;
    DirtyFlag flag = {0};
    bool dragging = false;
};

struct TextInputWidget {
    WidgetID id;
    ContainerID container;
    std::string text;
    uint16_t cursor = 0;
    StyleID style = 0;
    std::string name;
    WidgetSize sizeHint = WidgetSize::Normal;
    FloatRect rect;
    DirtyFlag flag = {0};
    bool focused = false;
};

struct DropdownWidget {
    WidgetID id;
    ContainerID container;
    uint8_t selectedIndex = 0;
    std::vector<std::string> options;
    bool open = false;
    StyleID style = 0;
    std::string name;
    WidgetSize sizeHint = WidgetSize::Normal;
    FloatRect rect;
    DirtyFlag flag = {0};
};

struct SwitchWidget {
    WidgetID id;
    ContainerID container;
    bool on = false;
    StyleID style = 0;
    std::string name;
    WidgetSize sizeHint = WidgetSize::Normal;
    FloatRect rect;
    DirtyFlag flag = {0};
};

struct ProgressBarWidget {
    WidgetID id;
    ContainerID container;
    float value = 0.f;
    StyleID style = 0;
    std::string name;
    WidgetSize sizeHint = WidgetSize::Normal;
    FloatRect rect;
    DirtyFlag flag = {0};
};

struct TextDisplayWidget {
    WidgetID id;
    ContainerID container;
    std::string text;
    StyleID style = 0;
    std::string name;
    WidgetSize sizeHint = WidgetSize::Normal;
    FloatRect rect;
    DirtyFlag flag = {0};
};

struct GraphWidget {
    WidgetID id;
    ContainerID container;
    std::vector<float> data;
    StyleID style = 0;
    std::string name;
    WidgetSize sizeHint = WidgetSize::Normal;
    FloatRect rect;
    DirtyFlag flag = {0};
};

}

#endif
