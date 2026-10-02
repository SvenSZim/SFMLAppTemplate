#pragma once

#include "atpl/ui/binding.hpp"
#include "atpl/ui/id.hpp"
#include "atpl/ui/placement.hpp"
#include "atpl/ui/rect.hpp"
#include "atpl/ui/theme.hpp"
#include "atpl/ui/widget.hpp"

#include <cstddef>
#include <memory>
#include <optional>
#include <string>

namespace atpl::model {

/// What the UI keeps for every widget, whatever its type. Each field has one writer.
struct WidgetSlot {
    // ----- From the setup; never changed afterwards -----

    std::string name;
    PanelId panel;
    std::optional<GridCell> cell; ///< Its position in the panel's grid, if it was given one.
    PanelColors colors;           ///< Its panel's colours, with its own override applied.
    std::optional<ViewId> view;   ///< Set if the widget is a view.

    /// The widget itself: its behaviour and its type-specific state. Never null.
    std::unique_ptr<Widget> widget;

    // ----- Written by layout -----

    FloatRect rect; ///< In its panel's content coordinates.
    bool visible = true;

    // ----- Written by input -----

    bool hovered = false;
    bool pressed = false;
    bool focused = false;

    // ----- Written by the application through its handle -----

    bool enabled = true;

    // ----- Written by the application (`bind`), kept in step by binding -----

    std::optional<AnyBinding> binding;
    std::size_t revisionSeen = 0; ///< The binding's change counter when the widget last got its value.
};

/// The widget's state as the theme and the widget itself see it.
[[nodiscard]] constexpr State stateOf(const WidgetSlot& slot) {
    State state = State::Normal;
    if (slot.hovered) {
        state = state | State::Hovered;
    }
    if (slot.pressed) {
        state = state | State::Pressed;
    }
    if (slot.focused) {
        state = state | State::Focused;
    }
    if (!slot.enabled) {
        state = state | State::Disabled;
    }
    return state;
}

} // namespace atpl::model
