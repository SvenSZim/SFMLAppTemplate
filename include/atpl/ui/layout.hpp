#pragma once

#include "atpl/ui/placement.hpp"

#include <SFML/System/Vector2.hpp>

#include <optional>

namespace atpl {

// How large things are and where they go.
//
// A UI has a layout theme next to its theme. The theme decides how things look; the layout theme
// decides sizes and positions. It is a set of defaults: a single panel can override any of them
// in its setup (`PanelSetup::layout`).
//
// Sizes are decided in one of two directions:
//
//   top-down    the space is given and the content adapts: the window's grid gives a panel its
//               size, the panel's grid gives a widget its size
//   bottom-up   the content says what it needs and the space follows: the widgets' minimum sizes
//               give the panel's size
//
// The whole concept is described in docs/LAYOUT.md.

/// One of nine positions inside a rectangle: where a panel sits in its grid cells, where a widget
/// sits in its cell, and towards which side a panel collapses.
enum class Alignment {
    TopLeft,
    Top,
    TopRight,
    Left,
    Center,
    Right,
    BottomLeft,
    Bottom,
    BottomRight,
};

/// How a panel in the window's grid uses its cells.
enum class Fit {
    Fill,    ///< The panel is exactly as large as its cells. Its content is laid out top-down.
    Content, ///< The panel is as large as its content needs, at most its cells. Bottom-up.
};

/// How a size that follows from content is chosen among several things.
enum class SizeRule {
    Own,   ///< Each as large as its own content needs.
    Equal, ///< All the same: as large as the largest needs.
};

/// What a stack of floating panels does when not even its headers fit into the window.
enum class StackOverflow {
    Hide,  ///< The last panels of the stack are not shown.
    Cards, ///< The panels overlap like a stack of cards (decision D42).
};

/// The layout theme's sizes, in pixels for a window of the reference size (`Scaling`). Layout and
/// `Widget::measure` read these and the text sizes of the theme; nothing else defines a size.
struct Metrics {
    /// GUI scale: every size, including fonts, is multiplied by it. For displays of high
    /// resolution; it applies on top of the scaling with the window.
    float scale = 1.f;

    float margin = 16.f;      ///< Between the window edge and panels, and between panels.
    float padding = 12.f;     ///< Between a panel's edge and its content.
    float gap = 8.f;          ///< Between widgets.
    float rowHeight = 28.f;   ///< Height of a one-line widget.
    float panelWidth = 280.f; ///< Width of a floating panel that does not set its own.
    float headerHeight = 36.f;
    float scrollbarWidth = 6.f;
};

/// How sizes follow the size of the window.
///
/// Everything grows and shrinks with the window, up to a point:
///
///     horizontal = clamp(window width  / reference width,  lowest, highest)
///     vertical   = clamp(window height / reference height, lowest, highest)
///     text       = 1 + (min(horizontal, vertical) - 1) * fontStrength
///
/// Widths, and padding and gaps across, follow `horizontal`; heights, and padding and gaps
/// down, follow `vertical`. The margin is the same on all sides and follows the smaller of the
/// two. Text sizes, outlines and corner radii follow `text`. Text sizes are rounded to whole
/// pixels.
///
/// With `lowest` and `highest` both 1, nothing scales with the window.
struct Scaling {
    sf::Vector2f referenceWindow = { 1280.f, 720.f }; ///< The window size at which every factor is 1.
    float lowest = 0.75f;                             ///< No factor goes below this.
    float highest = 1.5f;                             ///< No factor goes above this.

    /// How strongly text follows the window. 0: text never changes. 1: it follows the smaller
    /// side of the window fully.
    float fontStrength = 0.5f;
};

/// The most a floating panel may take of the window, per axis: the smaller of a fixed size and a
/// fraction of the window. The fraction limits panels in small windows, the fixed size on very
/// large screens. The limit overrides the content: what no longer fits scrolls or is not drawn.
struct PanelLimit {
    sf::Vector2f pixels = { 480.f, 900.f }; ///< At the reference window size; scales like other sizes.
    sf::Vector2f windowFraction = { 0.5f, 0.9f };
};

/// The layout theme's sizes as they are for the window at its current size: scaling applied, in
/// whole pixels. This is what widgets and layout work with.
struct Sizes {
    sf::Vector2f scale = { 1.f, 1.f }; ///< The horizontal and vertical factor in effect, GUI scale included.
    float text = 1.f;                  ///< The factor for text sizes, outlines and corner radii.

    float margin = 0.f;   ///< The same on all sides.
    sf::Vector2f padding; ///< Horizontally and vertically.
    sf::Vector2f gap;     ///< Between columns, and between rows.
    float rowHeight = 0.f;
    float panelWidth = 0.f;
    float headerHeight = 0.f;
    float scrollbarWidth = 0.f;

    [[nodiscard]] friend bool operator==(const Sizes&, const Sizes&) = default;
};

/// A layout theme: sizes, how they scale, and the defaults for placing panels and their content.
///
/// A layout theme is a value: copy it, change it, hand it to the UI. Switching at runtime is
/// `UI::setLayout`.
struct Layout {
    Metrics metrics;
    Scaling scaling;

    // ----- Panels -----

    /// Where a panel goes that does not say so itself: an anchor, or `GridSpan{}` for "somewhere
    /// in the window's grid".
    Placement placement = Anchor::TopLeft;

    /// Whether panels can be folded down to their header, and towards which side the header
    /// goes in a panel of the window's grid. (Floating panels collapse within their stack.)
    bool collapsible = true;
    Alignment collapseTowards = Alignment::TopLeft;

    /// How long folding or unfolding a panel takes, in seconds. 0: at once.
    float foldSeconds = 0.18f;

    // Panels in the window's grid.
    Fit fit = Fit::Fill;
    Alignment alignment = Alignment::TopLeft; ///< Where a panel that does not fill its cells sits.

    // Floating panels.
    SizeRule width = SizeRule::Equal;                  ///< Equal: as wide as the widest floating panel.
    SizeRule height = SizeRule::Own;                   ///< Equal: as high as the highest.
    PanelLimit limit;                                  ///< The most a floating panel may take of the window.
    StackOverflow stackOverflow = StackOverflow::Hide; ///< (WP 3.14)

    // ----- Content of a panel -----

    /// When a panel's size follows from its content: every widget as high as it needs
    /// (`Own`), or a grid of equal cells sized by the largest (`Equal`). A panel whose size is
    /// given from outside always has a grid.
    SizeRule rows = SizeRule::Own;

    /// The cells of panels whose content is a grid and whose size follows from it: each panel's
    /// as large as its own widgets want (`Own`), or the same in all such panels, as large as
    /// the largest (`Equal`), so that panels with the same number of cells are the same size.
    /// Panels that fill cells of the window's grid get their size from the window instead.
    SizeRule cells = SizeRule::Own;

    /// Where a widget sits in room that is larger than the widget may be.
    Alignment widgetAlignment = Alignment::Center;

    /// The sizes for a window of this size.
    [[nodiscard]] Sizes sizesAt(sf::Vector2f windowSize) const;
};

/// What a single panel does differently from the layout theme. Only the fields that are set
/// differ; the rest comes from the layout theme.
///
///     {.name = "Scene", .layout = {.fit = Fit::Fill}, ...}
///     {.name = "Legend", .layout = {.fit = Fit::Content, .alignment = Alignment::BottomRight}, ...}
struct PanelLayout {
    std::optional<Alignment> collapseTowards;
    std::optional<Fit> fit;
    std::optional<Alignment> alignment;
    std::optional<SizeRule> width;
    std::optional<SizeRule> height;
    std::optional<PanelLimit> limit;
    std::optional<SizeRule> rows;
    std::optional<SizeRule> cells;
    std::optional<Alignment> widgetAlignment;
};

/// Ready-made layout themes.
namespace layouts {

/// Panels float over the application's view: stacked at the top left unless they say otherwise,
/// all equally wide, each as high as its content. The default.
[[nodiscard]] Layout overlay();

/// An application made of panes: panels fill cells of the window's grid, and their content is
/// laid out in a grid too. Panels that name no cells get the first free one. Not collapsible.
[[nodiscard]] Layout dashboard();

/// Tidy cards: panels sit in the window's grid but are only as large as their content, centred
/// in their cells, with their widgets in equal cells.
[[nodiscard]] Layout cards();

/// As `overlay`, with smaller sizes and narrower limits: for small windows and many panels.
[[nodiscard]] Layout compact();

} // namespace layouts

} // namespace atpl
