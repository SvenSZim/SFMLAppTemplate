#include "atpl/ui/event.hpp"

namespace atpl {

namespace {

/// Whether a widget event is about the widget named `name`: "Name" in any panel, or "Panel/Name".
template <typename WidgetEvent>
[[nodiscard]] bool names(const WidgetEvent& event, std::string_view name) {
    const std::size_t slash = name.find('/');
    if (slash == std::string_view::npos) {
        return event.name == name;
    }
    return event.panel == name.substr(0, slash) && event.name == name.substr(slash + 1);
}

} // namespace

bool Event::isButton(std::string_view name) const {
    const auto* pressed = getIf<ButtonPressed>();
    return pressed != nullptr && names(*pressed, name);
}

bool Event::isButton(WidgetId widget) const {
    const auto* pressed = getIf<ButtonPressed>();
    return pressed != nullptr && pressed->widget == widget;
}

const ValueChanged* Event::changeOf(std::string_view name) const {
    const auto* changed = getIf<ValueChanged>();
    return changed != nullptr && names(*changed, name) ? changed : nullptr;
}

const ValueChanged* Event::changeOf(WidgetId widget) const {
    const auto* changed = getIf<ValueChanged>();
    return changed != nullptr && changed->widget == widget ? changed : nullptr;
}

bool Event::isKey(sf::Keyboard::Key key) const {
    const auto* pressed = getIf<KeyPressed>();
    return pressed != nullptr && pressed->key == key;
}

} // namespace atpl
