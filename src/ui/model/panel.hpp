#pragma once

#include "atpl/ui/layout.hpp"
#include "atpl/ui/placement.hpp"
#include "atpl/ui/rect.hpp"
#include "atpl/ui/theme.hpp"

#include <SFML/System/Vector2.hpp>

#include <cstdint>
#include <optional>
#include <string>

namespace atpl::model {

/// What the UI keeps for a panel. Each field has one writer.
struct Panel {
    // ----- From the setup; never changed afterwards -----

    std::string name;
    std::string title; ///< What the header shows: the setup's title, or the name if it had none.
    /// An anchor or grid cells. A panel that left its place in the grid open (`GridSpan`) has
    /// its cells here once layout has found them, when the UI is built.
    Placement placement = Anchor::TopLeft;

    /// What the setup said, before the layout theme filled in what it left open.
    std::optional<Placement> declaredPlacement;
    std::optional<bool> declaredCollapsible;
    PanelLayout layout; ///< What the panel does differently from the layout theme.
    int columns = 1;

    /// Whether the widgets are placed in a grid of equal cells instead of being packed, and how
    /// many rows that grid has. Worked out by layout when the UI is built.
    bool grid = false;
    int rows = 0;
    int declaredRows = 0; ///< What the setup said; 0 for none.

    float width = 0.f; ///< As in the setup: before scaling, 0 for the layout theme's default.
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

    /// The size the panel would like before it is placed: from its content, the layout theme's
    /// rules and its limits. 0: none worked out; placement then uses the defaults.
    float wantedWidth = 0.f;
    float wantedHeight = 0.f;

    /// The largest minimum width and height of a single widget of the panel. A panel whose
    /// content area is smaller than either is not drawn.
    sf::Vector2f widestAndHighest;

    /// Set when the panel had a place but is too small for its widgets: it is left out, and the
    /// others are placed without it.
    bool tooSmall = false;

    // ----- Written by whoever changes how the panel looks; taken by the UI before drawing -----

    bool dirty = true; ///< The panel has to be painted again. A new panel has never been painted.
};

} // namespace atpl::model
