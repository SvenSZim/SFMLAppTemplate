#include "atpl/ui/widget.hpp"

#include "ui/input/input_system.hpp"
#include "ui/render/text_measurer.hpp"

#include <utility>

namespace atpl {

// What a widget may do while it handles input: everything goes to the input system, which owns
// the state it changes.

InputContext::InputContext(
    input::InputSystem& input,
    WidgetId widget,
    sf::Vector2f origin,
    sf::Vector2f size,
    State state,
    const Sizes& sizes,
    std::optional<FloatRect> overlay,
    const Theme* theme,
    PanelColors colors,
    const render::TextMeasurer* measurer
) :
    m_input(&input),
    m_widget(widget),
    m_origin(origin),
    m_size(size),
    m_state(state),
    m_sizes(&sizes),
    m_overlay(overlay),
    m_theme(theme),
    m_colors(colors),
    m_measurer(measurer) {}

sf::Vector2f InputContext::size() const {
    return m_size;
}

sf::Vector2f InputContext::local(const PointerLocation& pointer) const {
    return pointer.window - m_origin;
}

State InputContext::state() const {
    return m_state;
}

const Sizes& InputContext::sizes() const {
    return *m_sizes;
}

sf::Vector2f InputContext::textSize(std::string_view text, const Part& part) const {
    if (m_measurer == nullptr || m_theme == nullptr) {
        return {};
    }
    const PartStyle style = m_theme->resolve(part, State::Normal, m_colors, m_sizes->text);
    return m_measurer->measure(text, style.font, style.textSize);
}

std::optional<FloatRect> InputContext::overlay() const {
    if (!m_overlay.has_value()) {
        return std::nullopt;
    }
    return FloatRect(m_overlay->position() - m_origin, m_overlay->size());
}

void InputContext::openOverlay() {
    m_input->openOverlay(m_widget);
}

void InputContext::closeOverlay() {
    m_input->closeOverlay(m_widget);
}

void InputContext::markDirty() {
    m_input->markDirty(m_widget);
}

void InputContext::capturePointer() {
    m_input->capturePointer(m_widget);
}

void InputContext::releasePointer() {
    m_input->releasePointer(m_widget);
}

void InputContext::requestFocus() {
    m_input->requestFocus(m_widget);
}

void InputContext::releaseFocus() {
    m_input->releaseFocus(m_widget);
}

void InputContext::changeValue(Value value, bool final) {
    m_input->changeValue(m_widget, std::move(value), final);
}

void InputContext::press() {
    m_input->press(m_widget);
}

} // namespace atpl
