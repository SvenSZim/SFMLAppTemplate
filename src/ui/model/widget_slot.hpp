#pragma once

#include "atpl/ui/binding.hpp"
#include "atpl/ui/id.hpp"
#include "atpl/ui/placement.hpp"
#include "atpl/ui/rect.hpp"
#include "atpl/ui/theme.hpp"
#include "atpl/ui/widget.hpp"

#include <chrono>
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
    std::optional<GridCell> declaredCell; ///< The position the setup gave it, if any.
    GridSpan span;                        ///< How many cells it takes.
    PanelColors colors;                   ///< Its panel's colours, with its own override applied.
    std::optional<ViewId> view;           ///< Set if the widget is a view.

    /// The widget itself: its behaviour and its type-specific state. Never null.
    std::unique_ptr<Widget> widget;

    // ----- Written by layout -----

    /// Its cells in its panel's grid, found when the UI is built and again when the layout theme
    /// changes. Empty in a panel whose widgets are packed.
    std::optional<GridCell> cell;

    FloatRect rect;      ///< In its panel's content coordinates.
    bool fits = true;    ///< Whether its room is at least as wide as the widget needs at least.
    bool visible = true; ///< Whether it is drawn: its panel is open, and it fits.

    // ----- Written by input -----

    bool hovered = false;
    bool pressed = false;
    bool focused = false;

    // ----- Written by the application through its handle -----

    bool enabled = true;

    // ----- Written by the application (`bind`), kept in step by binding -----

    std::optional<AnyBinding> binding;
    Revision revisionSeen = 0; ///< The binding's change counter when the widget last got its value.
    bool synced = false;       ///< Whether the widget got a value from its binding at all yet.
    std::chrono::steady_clock::time_point nextHandOver; ///< For values only shown: not before this.
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
