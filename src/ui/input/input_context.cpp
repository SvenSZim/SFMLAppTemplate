#include "atpl/ui/widget.hpp"

#include "ui/input/input_system.hpp"

namespace atpl {

// What a widget may do while it handles input: everything goes to the input system, which owns
// the state it changes.

InputContext::InputContext(
    input::InputSystem& input, WidgetId widget, sf::Vector2f origin, sf::Vector2f size, State state, const Sizes& sizes
) :
    m_input(&input),
    m_widget(widget),
    m_origin(origin),
    m_size(size),
    m_state(state),
    m_sizes(&sizes) {}

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
