#pragma once

#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/Font.hpp>
#include <SFML/System/Vector2.hpp>

#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>
#include <unordered_map>

namespace atpl {

// How things look.
//
// A widget does not choose colours or shapes. It declares its parts (track, knob, label, ...) and
// where they are; the theme decides how each part looks and whether it is shown. A part's style is
// worked out in three layers, each overriding the one before:
//
//   1. Tokens         global values of the theme: palette, shape, metrics, typography
//   2. Role defaults  every part has a role; the theme derives a style from role and tokens
//   3. Part entries   settings for one specific part: theme[Slider::Ticks].shown = true;
//
// A theme that sets only tokens is complete: every widget, including ones the theme has never
// heard of, gets its look from the roles of its parts.

// ----- Parts -----

/// What a part is for. The theme derives a part's default look from its role.
///
/// The last four are the types of text. A widget never chooses a font or a text size: it gives
/// each piece of text a part, and the part's role says what type of text it is.
enum class Role {
    // Shapes
    Surface, ///< A background that holds other things: a panel, a dropdown's list.
    Track,   ///< A recessed area: slider track, switch track, text field, graph background.
    Accent,  ///< The highlighted part, in the theme's main colour: slider fill, graph curve.
    Handle,  ///< Something to grab or click: knob, button face, scrollbar.
    Line,    ///< Thin strokes: outlines, ticks, axes, separators.

    // Text
    Title,     ///< The title of a panel.
    Heading,   ///< A heading inside a panel.
    Text,      ///< Text that carries content: values, button labels, paragraphs.
    MutedText, ///< Text that explains: widget labels, placeholders, footers.
};

/// Whether a part is shown unless the theme says otherwise.
enum class Shown { No, Yes };

/// Names a kind of element: one per widget type, plus the panel. Any code can declare one.
struct Kind {
    std::string_view name;

    constexpr explicit Kind(std::string_view kindName) :
        name(kindName) {}

    [[nodiscard]] friend constexpr bool operator==(Kind, Kind) = default;
};

/// One part of a kind of element. Widgets declare their parts as constants:
///
///     struct Slider {
///         static constexpr Kind kind{ "slider" };
///         static constexpr Part Track{ kind, "track", Role::Track };
///         static constexpr Part Ticks{ kind, "ticks", Role::Line, Shown::No }; // optional part
///     };
///
/// A part is identified by its kind and its name; the names must be unique within the kind.
struct Part {
    Kind kind;
    std::string_view name;
    Role role;
    Shown shown;

    constexpr Part(Kind partKind, std::string_view partName, Role partRole, Shown shownByDefault = Shown::Yes) :
        kind(partKind),
        name(partName),
        role(partRole),
        shown(shownByDefault) {}

    /// A number that identifies the part, computed from kind and name.
    [[nodiscard]] constexpr std::uint64_t id() const {
        std::uint64_t hash = 14695981039346656037ull; // FNV-1a
        const auto mix = [&hash](std::string_view text) {
            for (const char c : text) {
                hash = (hash ^ static_cast<unsigned char>(c)) * 1099511628211ull;
            }
        };
        mix(kind.name);
        mix("/");
        mix(name);
        return hash;
    }

    [[nodiscard]] friend constexpr bool operator==(const Part& a, const Part& b) {
        return a.kind == b.kind && a.name == b.name;
    }
};

// ----- States -----

/// What is going on with an element. Flags; combine with `|`.
enum class State : std::uint8_t {
    Normal = 0,
    Hovered = 1,  ///< The pointer is over it.
    Pressed = 2,  ///< It is being pressed or dragged.
    Focused = 4,  ///< It has the keyboard focus.
    Disabled = 8, ///< It does not react to input.
    Active = 16,  ///< It is "on" or selected: a switch that is on, the chosen dropdown entry.
};

[[nodiscard]] constexpr State operator|(State a, State b) {
    return static_cast<State>(static_cast<std::uint8_t>(a) | static_cast<std::uint8_t>(b));
}

/// Whether `flag` is set in `state`.
[[nodiscard]] constexpr bool has(State state, State flag) {
    return (static_cast<std::uint8_t>(state) & static_cast<std::uint8_t>(flag)) != 0;
}

// ----- Styles -----

/// A soft shadow behind a box.
struct Shadow {
    sf::Vector2f offset;
    float size = 0.f; ///< How far the shadow spreads, in pixels. 0: no shadow.
    sf::Color color = sf::Color::Transparent;
};

/// A corner radius larger than any box: the painter limits it to half the box's smaller side,
/// which gives a pill or a circle.
inline constexpr float fullyRound = 1.0e6f;

/// How one part looks, worked out by the theme for the part's current state.
/// Sizes are in pixels and already include the GUI scale.
struct PartStyle {
    bool shown = true; ///< A part that is not shown is skipped by the painter.

    sf::Color color = sf::Color::Transparent;  ///< Fill of a box; colour of a line or of text.
    sf::Color border = sf::Color::Transparent; ///< Outline of a box.
    float borderThickness = 0.f;
    float radius = 0.f;    ///< Corner radius of a box. 0: sharp corners.
    Shadow shadow;         ///< Shadow of a box.
    float thickness = 1.f; ///< Thickness of a line.

    float textSize = 14.f;          ///< Height of text.
    const sf::Font* font = nullptr; ///< Font of text. Set by the theme; never null in a resolved style.
};

/// Settings for one part that replace what its role would give. Only the fields that are set
/// have an effect; the rest still comes from the role and the tokens.
///
///     theme[Slider::Ticks].shown = true;
///     theme[Panel::Outline].thickness = 2.f;
///     theme[Button::Face].radius = 0.f;       // sharp buttons
///
/// A colour set here applies to every state; the theme still derives the hovered, pressed and
/// disabled variants from it.
struct PartOverride {
    std::optional<bool> shown;
    std::optional<sf::Color> color;
    std::optional<sf::Color> border;
    std::optional<float> borderThickness;
    std::optional<float> radius;
    std::optional<Shadow> shadow;
    std::optional<float> thickness;
    std::optional<float> textSize;
    std::shared_ptr<const sf::Font> font; ///< Null: the font of the part's text type.
};

// ----- Tokens -----

/// The theme's colours.
struct Palette {
    sf::Color window;    ///< Behind everything, where no view or panel is.
    sf::Color surface;   ///< Role::Surface.
    sf::Color track;     ///< Role::Track.
    sf::Color accent;    ///< Role::Accent, and whatever is Active.
    sf::Color handle;    ///< Role::Handle.
    sf::Color text;      ///< Role::Title, Role::Heading and Role::Text.
    sf::Color mutedText; ///< Role::MutedText.
    sf::Color line;      ///< Role::Line.
    sf::Color shadow;    ///< Shadows of surfaces.
};

/// The theme's shapes. Pixels before GUI scaling.
struct Shape {
    float radius = 10.f;     ///< Corners of surfaces.
    float smallRadius = 5.f; ///< Corners of tracks and handles.
    float outline = 0.f;     ///< Outline thickness of surfaces. 0: no outline.
    float line = 1.5f;       ///< Thickness of Role::Line and Role::Accent strokes.
    float shadowSize = 12.f; ///< Spread of the shadow of surfaces. 0: no shadows.
    sf::Vector2f shadowOffset = { 0.f, 4.f };
};

/// One type of text: its size and its font.
struct TextType {
    float size = 14.f;                    ///< Height in pixels before GUI scaling.
    std::shared_ptr<const sf::Font> font; ///< Null: the theme's default font.
};

/// The theme's text types, one per text role.
///
///     theme.typography.title = {.size = 18.f, .font = boldFont};
///     theme.typography.muted.size = 11.f;
struct Typography {
    TextType title{ .size = 16.f, .font = {} };   ///< Role::Title
    TextType heading{ .size = 15.f, .font = {} }; ///< Role::Heading
    TextType text{ .size = 14.f, .font = {} };    ///< Role::Text
    TextType muted{ .size = 12.f, .font = {} };   ///< Role::MutedText
};

/// The theme's sizes. Pixels before GUI scaling. Layout and `Widget::measure` read these and the
/// text sizes in `Typography`; nothing else defines a size.
struct Metrics {
    float scale = 1.f; ///< GUI scale: every size, including fonts, is multiplied by it.

    float margin = 16.f;      ///< Between the window edge and floating panels, and between stacked panels.
    float padding = 12.f;     ///< Between a panel's edge and its content.
    float gap = 8.f;          ///< Between widgets.
    float rowHeight = 28.f;   ///< Height of a one-line widget.
    float panelWidth = 280.f; ///< Width of a floating panel that does not set its own.
    float headerHeight = 36.f;
    float scrollbarWidth = 6.f;
};

// ----- Theme -----

/// A complete look: tokens, plus entries for individual parts.
///
/// A theme is a value: copy it, change it, hand it to the UI. Switching themes at runtime is
/// `UI::setTheme`.
class Theme {
public:
    Palette palette;
    Shape shape;
    Metrics metrics;
    Typography typography;

    /// The default font: used by every text type that does not name its own. Must be set before
    /// the theme is used; `App` sets the bundled font if the application sets none.
    std::shared_ptr<const sf::Font> font;

    /// The default look: same as `themes::moon()`.
    Theme();

    /// The entry for one part, created if there is none yet. See `PartOverride`.
    [[nodiscard]] PartOverride& operator[](const Part& part);

    /// The entry for one part, or null if the theme has none.
    [[nodiscard]] const PartOverride* entry(const Part& part) const;

    /// The style of a part in a state, through all three layers.
    [[nodiscard]] PartStyle resolve(const Part& part, State state = State::Normal) const;

private:
    std::unordered_map<std::uint64_t, PartOverride> m_entries;
};

/// Ready-made themes.
namespace themes {

[[nodiscard]] Theme moon();     ///< Dark and quiet.
[[nodiscard]] Theme colorful(); ///< Bright, with strong accent colours.

} // namespace themes

} // namespace atpl
