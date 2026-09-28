#ifndef WIDGET_MANAGER
#define WIDGET_MANAGER

#include <vector>
#include <string>
#include <unordered_map>

#include "../types.hpp"
#include "../event.hpp"
#include "./widget_types.hpp"

namespace ui::core::widgets {

class WidgetManager {
private:
    WidgetID m_nextId = 0;
    std::vector<ButtonWidget> m_buttons;
    std::vector<SliderWidget> m_sliders;
    std::vector<TextInputWidget> m_textInputs;
    std::vector<DropdownWidget> m_dropdowns;
    std::vector<SwitchWidget> m_switches;
    std::vector<ProgressBarWidget> m_progressBars;
    std::vector<TextDisplayWidget> m_textDisplays;
    std::vector<GraphWidget> m_graphs;

    std::unordered_map<std::string, WidgetID> m_nameIndex;

    WidgetID nextId() { return ++m_nextId; }

public:
    WidgetManager() = default;

    // Creation
    WidgetID createButton(ContainerID container, const std::string& label, WidgetSize size = WidgetSize::Normal);
    WidgetID createSlider(ContainerID container, const std::string& name, float min, float max, float initial, WidgetSize size = WidgetSize::Normal);
    WidgetID createTextInput(ContainerID container, const std::string& name, WidgetSize size = WidgetSize::Normal);
    WidgetID createDropdown(ContainerID container, const std::string& name, const std::vector<std::string>& options, WidgetSize size = WidgetSize::Normal);
    WidgetID createSwitch(ContainerID container, const std::string& name, bool initial = false, WidgetSize size = WidgetSize::Normal);
    WidgetID createProgressBar(ContainerID container, const std::string& name, WidgetSize size = WidgetSize::Normal);
    WidgetID createTextDisplay(ContainerID container, const std::string& name, const std::string& initial = "", WidgetSize size = WidgetSize::Normal);
    WidgetID createGraph(ContainerID container, const std::string& name, WidgetSize size = WidgetSize::Normal);

    // Name-based lookup (init-time convenience)
    WidgetID getIdByName(const std::string& name) const;

    // Value access
    float getSliderValue(WidgetID id) const;
    void setSliderValue(WidgetID id, float value);
    bool getSwitchValue(WidgetID id) const;
    const std::string& getTextInputValue(WidgetID id) const;
    uint8_t getDropdownIndex(WidgetID id) const;
    void setTextDisplay(WidgetID id, const std::string& text);
    void setProgressBar(WidgetID id, float value);
    void pushGraphData(WidgetID id, float value);

    // Access for rendering/layout
    std::vector<ButtonWidget>& getButtons() { return m_buttons; }
    std::vector<SliderWidget>& getSliders() { return m_sliders; }
    std::vector<TextInputWidget>& getTextInputs() { return m_textInputs; }
    std::vector<DropdownWidget>& getDropdowns() { return m_dropdowns; }
    std::vector<SwitchWidget>& getSwitches() { return m_switches; }
    std::vector<ProgressBarWidget>& getProgressBars() { return m_progressBars; }
    std::vector<TextDisplayWidget>& getTextDisplays() { return m_textDisplays; }
    std::vector<GraphWidget>& getGraphs() { return m_graphs; }

    // Update (process interactions, push events)
    void update(std::vector<Event>& eventBuffer);
};

}

#endif
