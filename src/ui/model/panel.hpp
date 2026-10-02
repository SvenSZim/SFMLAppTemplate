#pragma once

#include "atpl/ui/placement.hpp"
#include "atpl/ui/rect.hpp"
#include "atpl/ui/theme.hpp"

#include <cstdint>
#include <string>

namespace atpl::model {

/// What the UI keeps for a panel. Each field has one writer.
struct Panel {
    // ----- From the setup; never changed afterwards -----

    std::string name;
    std::string title; ///< What the header shows: the setup's title, or the name if it had none.
    Placement placement = Anchor::TopLeft;
    int columns = 1;
    float width = 0.f; ///< As in the setup: before GUI scaling, 0 for the theme's default.
    PanelColors colors;
    bool collapsible = true;

    /// The panel's widgets are `widgetCount` slots in a row, starting at `firstWidget`, in the
    /// order they were listed.
    std::uint32_t firstWidget = 0;
    std::uint32_t widgetCount = 0;

    // ----- Written by input, and by the application through its handle -----

    bool collapsed = false;
    bool visible = true; ///< What the application wants; whether it fits is `shown`.
    bool hovered = false;

    // ----- Written by layout -----

    FloatRect rect; ///< In window pixels.

    /// Whether the panel is on screen: it is visible and there is room for it. A panel that is
    /// not shown is neither drawn nor reacts to input, and its rectangle means nothing.
    bool shown = false;

    /// The height the panel's widgets need, padding included. 0 for a panel without widgets.
    float contentHeight = 0.f;

    // ----- Written by whoever changes how the panel looks; taken by the UI before drawing -----

    bool dirty = true; ///< The panel has to be painted again. A new panel has never been painted.
};

} // namespace atpl::model
