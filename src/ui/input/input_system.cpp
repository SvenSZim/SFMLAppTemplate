#include "ui/input/input_system.hpp"

#include "atpl/ui/widget.hpp"

#include "ui/widgets/panel_frame.hpp"

#include <algorithm>
#include <utility>

namespace atpl::input {

namespace {

[[nodiscard]] Modifiers modifiersOf(bool alt, bool control, bool shift, bool system) {
    return { .shift = shift, .control = control, .alt = alt, .system = system };
}

[[nodiscard]] float notches(const sf::Event::MouseWheelScrolled& wheel) {
    return wheel.delta;
}

} // namespace

// ----- Where the pointer is -----

sf::Vector2f InputSystem::contentOrigin(PanelId panel) const {
    // Widget rectangles are in the panel's content, which starts below the header.
    return m_store->panel(panel).rect.position() + sf::Vector2f(0.f, m_sizes->headerHeight);
}

InputSystem::Hit InputSystem::hitTest(sf::Vector2f position) const {
    // The topmost panel under the pointer, then the widget under it in that panel.
    for (auto it = m_stacking.rbegin(); it != m_stacking.rend(); ++it) {
        const model::Panel& panel = m_store->panel(*it);
        if (!panel.shown || !panel.rect.contains(position)) {
            continue;
        }
        Hit hit{ .panel = *it,
                 .widget = std::nullopt,
                 .header = position.y < panel.rect.top() + m_sizes->headerHeight };
        if (hit.header) {
            return hit;
        }
        // Only what is inside the panel can be hit: content cut off at its edge is not there.
        const sf::Vector2f local = position - contentOrigin(*it);
        const std::uint32_t first = panel.firstWidget;
        for (std::uint32_t i = 0; i < panel.widgetCount; ++i) {
            const model::WidgetSlot& slot = m_store->widget(WidgetId{ first + i });
            if (slot.visible && slot.enabled && slot.rect.contains(local)) {
                hit.widget = WidgetId{ first + i };
                break;
            }
        }
        return hit;
    }
    return {};
}

PointerLocation InputSystem::locate(sf::Vector2f position) const {
    PointerLocation location{ .window = position, .view = std::nullopt, .viewName = {}, .inView = position };
    const Hit hit = hitTest(position);

    std::optional<ViewId> view;
    if (hit.panel.has_value()) {
        // Over a panel: only a view widget of it counts.
        if (hit.widget.has_value()) {
            view = m_store->widget(*hit.widget).view;
        }
    } else {
        view = m_store->backgroundView();
    }
    if (view.has_value()) {
        const model::View& entry = m_store->view(*view);
        location.view = view;
        location.viewName = entry.name;
        location.inView = position - entry.rect.position();
    }
    return location;
}

// ----- State that changes how widgets look -----

void InputSystem::setHover(const Hit& hit) {
    if (hit.widget != m_hovered) {
        if (m_hovered.has_value()) {
            model::WidgetSlot& slot = m_store->widget(*m_hovered);
            slot.hovered = false;
            m_store->panel(slot.panel).dirty = true;
        }
        if (hit.widget.has_value()) {
            model::WidgetSlot& slot = m_store->widget(*hit.widget);
            slot.hovered = true;
            m_store->panel(slot.panel).dirty = true;
        }
        m_hovered = hit.widget;
    }
    if (hit.panel != m_hoveredPanel) {
        if (m_hoveredPanel.has_value()) {
            model::Panel& panel = m_store->panel(*m_hoveredPanel);
            panel.hovered = false;
            if (panel.headerHovered) {
                panel.headerHovered = false;
                panel.dirty = true;
            }
        }
        if (hit.panel.has_value()) {
            m_store->panel(*hit.panel).hovered = true;
        }
        m_hoveredPanel = hit.panel;
    }
    if (hit.panel.has_value()) {
        // The header of a collapsible panel answers to the pointer: its arrow lights up.
        model::Panel& panel = m_store->panel(*hit.panel);
        if (panel.headerHovered != hit.header) {
            panel.headerHovered = hit.header;
            if (panel.collapsible) {
                panel.dirty = true;
            }
        }
    }
}

void InputSystem::setFocus(std::optional<WidgetId> widget) {
    if (widget == m_focused) {
        return;
    }
    if (m_focused.has_value()) {
        model::WidgetSlot& slot = m_store->widget(*m_focused);
        slot.focused = false;
        m_store->panel(slot.panel).dirty = true;
    }
    if (widget.has_value()) {
        model::WidgetSlot& slot = m_store->widget(*widget);
        slot.focused = true;
        m_store->panel(slot.panel).dirty = true;
    }
    m_focused = widget;
}

void InputSystem::setPressed(std::optional<WidgetId> widget) {
    if (widget == m_pressed) {
        return;
    }
    if (m_pressed.has_value()) {
        model::WidgetSlot& slot = m_store->widget(*m_pressed);
        slot.pressed = false;
        m_store->panel(slot.panel).dirty = true;
    }
    if (widget.has_value()) {
        model::WidgetSlot& slot = m_store->widget(*widget);
        slot.pressed = true;
        m_store->panel(slot.panel).dirty = true;
    }
    m_pressed = widget;
}

void InputSystem::forgetHover(model::Store& store) {
    m_store = &store;
    setHover({});
}

// ----- Handing events on -----

void InputSystem::deliver(WidgetId widget, const Event& event) {
    const model::WidgetSlot& slot = m_store->widget(widget);
    InputContext context(
        *this,
        widget,
        contentOrigin(slot.panel) + slot.rect.position(),
        slot.rect.size(),
        model::stateOf(slot),
        *m_sizes
    );
    m_store->widget(widget).widget->handleInput(event, context);
}

void InputSystem::forward(Event event) {
    m_events->push_back(std::move(event));
}

void InputSystem::handle(
    const sf::Event& event,
    model::Store& store,
    std::span<const PanelId> stacking,
    const Sizes& sizes,
    std::vector<Event>& events
) {
    m_store = &store;
    m_stacking = stacking;
    m_sizes = &sizes;
    m_events = &events;

    // Where pointer input goes while a press lasts: the widget that has the pointer, if any.
    const auto holder = [this]() -> std::optional<WidgetId> { return m_captured ? m_captured : m_pressed; };

    if (const auto* moved = event.getIf<sf::Event::MouseMoved>()) {
        m_pointer = sf::Vector2f(moved->position);
        const PointerLocation pointer = locate(m_pointer);
        const sf::Vector2f delta = m_lastForwarded ? m_pointer - *m_lastForwarded : sf::Vector2f();

        if (m_owner == Owner::Ui || m_captured.has_value()) {
            // A drag of a widget: the widget follows, wherever the pointer goes.
            if (const auto widget = holder()) {
                deliver(*widget, Event(PointerMoved{ pointer, delta }));
            }
            return;
        }
        if (m_owner == Owner::Application) {
            // A drag of the application's: forwarded, over panels too.
            forward(PointerMoved{ pointer, delta });
            m_lastForwarded = m_pointer;
            return;
        }

        const Hit hit = hitTest(m_pointer);
        setHover(hit);
        if (hit.panel.has_value()) {
            if (hit.widget.has_value()) {
                deliver(*hit.widget, Event(PointerMoved{ pointer, delta }));
            }
            m_lastForwarded.reset();
            return;
        }
        forward(PointerMoved{ pointer, delta });
        m_lastForwarded = m_pointer;
        return;
    }

    if (const auto* pressedButton = event.getIf<sf::Event::MouseButtonPressed>()) {
        m_pointer = sf::Vector2f(pressedButton->position);
        const PointerLocation pointer = locate(m_pointer);
        const PointerPressed press{ pressedButton->button, pointer };

        if (m_owner != Owner::None) {
            // Another button during a press: it goes where the press went.
            ++m_buttonsDown;
            if (m_owner == Owner::Application) {
                forward(press);
            } else if (const auto widget = holder()) {
                deliver(*widget, Event(press));
            }
            return;
        }

        const Hit hit = hitTest(m_pointer);
        setHover(hit);
        m_buttonsDown = 1;
        if (!hit.panel.has_value()) {
            m_owner = Owner::Application;
            setFocus(std::nullopt);
            m_lastForwarded = m_pointer;
            forward(press);
            return;
        }

        m_owner = Owner::Ui;
        if (hit.header && pressedButton->button == sf::Mouse::Button::Left) {
            m_pressedHeader = hit.panel;
        }
        if (m_focused != hit.widget) {
            setFocus(std::nullopt); // the widget may take it again while it handles the press
        }
        if (hit.widget.has_value()) {
            setPressed(hit.widget);
            deliver(*hit.widget, Event(press));
        }
        return;
    }

    if (const auto* releasedButton = event.getIf<sf::Event::MouseButtonReleased>()) {
        m_pointer = sf::Vector2f(releasedButton->position);
        const PointerLocation pointer = locate(m_pointer);
        const PointerReleased release{ releasedButton->button, pointer };

        if (m_owner == Owner::Application) {
            forward(release);
        } else if (m_owner == Owner::Ui) {
            if (const auto widget = holder()) {
                deliver(*widget, Event(release));
            }
            // A click on the header of a collapsible panel folds or unfolds it.
            const Hit hit = hitTest(m_pointer);
            if (m_pressedHeader.has_value() && hit.header && hit.panel == m_pressedHeader &&
                releasedButton->button == sf::Mouse::Button::Left) {
                model::Panel& panel = m_store->panel(*m_pressedHeader);
                if (panel.collapsible) {
                    widgets::setCollapsed(panel, !panel.collapsed);
                    m_folded = true;
                }
            }
        } else if (!hitTest(m_pointer).panel.has_value()) {
            forward(release); // a release without a press we saw: by where it happens
        }

        m_buttonsDown = std::max(m_buttonsDown - 1, 0);
        if (m_buttonsDown == 0) {
            m_owner = Owner::None;
            m_pressedHeader.reset();
            m_captured.reset(); // released together with the button
            setPressed(std::nullopt);
            setHover(hitTest(m_pointer));
        }
        return;
    }

    if (const auto* wheel = event.getIf<sf::Event::MouseWheelScrolled>()) {
        m_pointer = sf::Vector2f(wheel->position);
        const PointerLocation pointer = locate(m_pointer);
        const Scrolled scrolled{ notches(*wheel), wheel->wheel == sf::Mouse::Wheel::Horizontal, pointer };

        const Hit hit = hitTest(m_pointer);
        if (m_owner == Owner::Application || (m_owner == Owner::None && !hit.panel.has_value())) {
            forward(scrolled);
            return;
        }
        // Over a panel the wheel is the UI's, whether a widget uses it or not.
        if (const auto widget = m_owner == Owner::Ui ? holder() : hit.widget) {
            deliver(*widget, Event(scrolled));
        }
        return;
    }

    if (event.is<sf::Event::MouseLeft>()) {
        if (m_owner == Owner::None) {
            setHover({});
        }
        return;
    }

    if (const auto* key = event.getIf<sf::Event::KeyPressed>()) {
        const KeyPressed pressedKey{ key->code,
                                     modifiersOf(key->alt, key->control, key->shift, key->system),
                                     locate(m_pointer) };
        if (m_focused.has_value()) {
            deliver(*m_focused, Event(pressedKey));
        } else {
            forward(pressedKey);
        }
        return;
    }

    if (const auto* key = event.getIf<sf::Event::KeyReleased>()) {
        const KeyReleased releasedKey{ key->code,
                                       modifiersOf(key->alt, key->control, key->shift, key->system),
                                       locate(m_pointer) };
        if (m_focused.has_value()) {
            deliver(*m_focused, Event(releasedKey));
        } else {
            forward(releasedKey);
        }
        return;
    }

    if (const auto* text = event.getIf<sf::Event::TextEntered>()) {
        const TextEntered entered{ text->unicode };
        if (m_focused.has_value()) {
            deliver(*m_focused, Event(entered));
        } else {
            forward(entered);
        }
        return;
    }
}

bool InputSystem::takeFolded() {
    return std::exchange(m_folded, false);
}

// ----- What a widget does through its context -----

void InputSystem::markDirty(WidgetId widget) {
    m_store->panel(m_store->widget(widget).panel).dirty = true;
}

void InputSystem::capturePointer(WidgetId widget) {
    m_captured = widget;
}

void InputSystem::releasePointer(WidgetId widget) {
    if (m_captured == widget) {
        m_captured.reset();
    }
}

void InputSystem::requestFocus(WidgetId widget) {
    setFocus(widget);
}

void InputSystem::releaseFocus(WidgetId widget) {
    if (m_focused == widget) {
        setFocus(std::nullopt);
    }
}

void InputSystem::changeValue(WidgetId widget, Value value, bool final) {
    const model::WidgetSlot& slot = m_store->widget(widget);
    // Writing the bound value comes with bindings (WP 3.8); the event is raised already.
    m_events->push_back(
        ValueChanged{
            .widget = widget,
            .name = slot.name,
            .panel = m_store->panel(slot.panel).name,
            .value = std::move(value),
            .final = final,
        }
    );
}

void InputSystem::press(WidgetId widget) {
    const model::WidgetSlot& slot = m_store->widget(widget);
    m_events->push_back(ButtonPressed{ .widget = widget, .name = slot.name, .panel = m_store->panel(slot.panel).name });
}

} // namespace atpl::input
