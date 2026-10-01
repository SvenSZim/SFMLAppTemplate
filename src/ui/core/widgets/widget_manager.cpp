#include <algorithm>
#include "ui/core/widgets/widget_manager.hpp"

namespace ui::core::widgets {

WidgetID WidgetManager::createButton(ContainerID container, const std::string& label, WidgetSize size) {
    WidgetID id = nextId();
    m_buttons.push_back({id, container, ButtonState::Idle, 0, label, size, {}, {0}});
    m_nameIndex[label] = id;
    return id;
}

WidgetID WidgetManager::createSlider(ContainerID container, const std::string& name, float min, float max, float initial, WidgetSize size) {
    WidgetID id = nextId();
    m_sliders.push_back({id, container, initial, min, max, 0, name, size, {}, {0}, false});
    m_nameIndex[name] = id;
    return id;
}

WidgetID WidgetManager::createTextInput(ContainerID container, const std::string& name, WidgetSize size) {
    WidgetID id = nextId();
    m_textInputs.push_back({id, container, "", 0, 0, name, size, {}, {0}, false});
    m_nameIndex[name] = id;
    return id;
}

WidgetID WidgetManager::createDropdown(ContainerID container, const std::string& name, const std::vector<std::string>& options, WidgetSize size) {
    WidgetID id = nextId();
    m_dropdowns.push_back({id, container, 0, options, false, 0, name, size, {}, {0}});
    m_nameIndex[name] = id;
    return id;
}

WidgetID WidgetManager::createSwitch(ContainerID container, const std::string& name, bool initial, WidgetSize size) {
    WidgetID id = nextId();
    m_switches.push_back({id, container, initial, 0, name, size, {}, {0}});
    m_nameIndex[name] = id;
    return id;
}

WidgetID WidgetManager::createProgressBar(ContainerID container, const std::string& name, WidgetSize size) {
    WidgetID id = nextId();
    m_progressBars.push_back({id, container, 0.f, 0, name, size, {}, {0}});
    m_nameIndex[name] = id;
    return id;
}

WidgetID WidgetManager::createTextDisplay(ContainerID container, const std::string& name, const std::string& initial, WidgetSize size) {
    WidgetID id = nextId();
    m_textDisplays.push_back({id, container, initial, 0, name, size, {}, {0}});
    m_nameIndex[name] = id;
    return id;
}

WidgetID WidgetManager::createGraph(ContainerID container, const std::string& name, WidgetSize size) {
    WidgetID id = nextId();
    m_graphs.push_back({id, container, {}, 0, name, size, {}, {0}});
    m_nameIndex[name] = id;
    return id;
}

WidgetID WidgetManager::getIdByName(const std::string& name) const {
    auto it = m_nameIndex.find(name);
    if (it != m_nameIndex.end()) return it->second;
    return 0;
}

float WidgetManager::getSliderValue(WidgetID id) const {
    for (const auto& s : m_sliders) {
        if (s.id == id) return s.value;
    }
    return 0.f;
}

void WidgetManager::setSliderValue(WidgetID id, float value) {
    for (auto& s : m_sliders) {
        if (s.id == id) {
            s.value = std::clamp(value, s.min, s.max);
            s.flag.flags.visual = 1;
            return;
        }
    }
}

bool WidgetManager::getSwitchValue(WidgetID id) const {
    for (const auto& s : m_switches) {
        if (s.id == id) return s.on;
    }
    return false;
}

const std::string& WidgetManager::getTextInputValue(WidgetID id) const {
    for (const auto& t : m_textInputs) {
        if (t.id == id) return t.text;
    }
    static const std::string empty;
    return empty;
}

uint8_t WidgetManager::getDropdownIndex(WidgetID id) const {
    for (const auto& d : m_dropdowns) {
        if (d.id == id) return d.selectedIndex;
    }
    return 0;
}

void WidgetManager::setTextDisplay(WidgetID id, const std::string& text) {
    for (auto& t : m_textDisplays) {
        if (t.id == id) {
            t.text = text;
            t.flag.flags.visual = 1;
            return;
        }
    }
}

void WidgetManager::setProgressBar(WidgetID id, float value) {
    for (auto& p : m_progressBars) {
        if (p.id == id) {
            p.value = std::clamp(value, 0.f, 1.f);
            p.flag.flags.visual = 1;
            return;
        }
    }
}

void WidgetManager::pushGraphData(WidgetID id, float value) {
    for (auto& g : m_graphs) {
        if (g.id == id) {
            g.data.push_back(value);
            g.flag.flags.visual = 1;
            return;
        }
    }
}

void WidgetManager::update(std::vector<Event>& eventBuffer) {
    // Process state changes and push events as needed
    // Widget interactions are handled by the UIManager which sets states directly
}

}
