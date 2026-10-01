#ifndef WIDGET_FACTORY
#define WIDGET_FACTORY

#include <string>
#include <vector>

#include "./widget_types.hpp"

namespace ui::core::widgets {

struct WidgetDescriptor {
    WidgetType type;
    std::string name;
    WidgetSize size = WidgetSize::Normal;
    float min = 0.f;
    float max = 1.f;
    float initial = 0.f;
    bool initialBool = false;
    std::string initialText = "";
    std::vector<std::string> options = {};
};

struct Widget {
    static WidgetDescriptor Button(const std::string& label, WidgetSize size = WidgetSize::Normal) {
        return {WidgetType::Button, label, size};
    }

    static WidgetDescriptor Slider(const std::string& name, float min, float max, float initial, WidgetSize size = WidgetSize::Normal) {
        return {WidgetType::Slider, name, size, min, max, initial};
    }

    static WidgetDescriptor TextInput(const std::string& name, WidgetSize size = WidgetSize::Normal) {
        return {WidgetType::TextInput, name, size};
    }

    static WidgetDescriptor Dropdown(const std::string& name, const std::vector<std::string>& options, WidgetSize size = WidgetSize::Normal) {
        return {WidgetType::Dropdown, name, size, 0.f, 0.f, 0.f, false, "", options};
    }

    static WidgetDescriptor Switch(const std::string& name, bool initial = false, WidgetSize size = WidgetSize::Normal) {
        return {WidgetType::Switch, name, size, 0.f, 0.f, 0.f, initial};
    }

    static WidgetDescriptor ProgressBar(const std::string& name, WidgetSize size = WidgetSize::Normal) {
        return {WidgetType::ProgressBar, name, size};
    }

    static WidgetDescriptor TextDisplay(const std::string& name, WidgetSize size = WidgetSize::Normal) {
        return {WidgetType::TextDisplay, name, size};
    }

    static WidgetDescriptor Graph(const std::string& name, WidgetSize size = WidgetSize::Normal) {
        return {WidgetType::Graph, name, size};
    }
};

}

#endif
