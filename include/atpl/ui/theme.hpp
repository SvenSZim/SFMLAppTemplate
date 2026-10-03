#pragma once

#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/Font.hpp>
#include <SFML/System/Vector2.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace atpl {

// How things look.
//
// A widget does not choose colours or shapes. It declares its parts (track, knob, label, ...) and
// where they are; the theme decides how each part looks and whether it is shown. A part's style is
// worked out in three layers, each overriding the one before:
//
//   1. Tokens         global values of the theme: palette, shape, typography
//                     (a panel picks two main colours and an accent from the palette)
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
    Open = 32,    ///< Its overlay is open: a dropdown showing its list.
};

[[nodiscard]] constexpr State operator|(State a, State b) {
    return static_cast<State>(static_cast<std::uint8_t>(a) | static_cast<std::uint8_t>(b));
}

/// Whether `flag` is set in `state`.
[[nodiscard]] constexpr bool has(State state, State flag) {
    return (static_cast<std::uint8_t>(state) & static_cast<std::uint8_t>(flag)) != 0;
}

// ----- Styles -----

/// How the fill of a box changes colour across it.
enum class Gradient {
    None,       ///< One colour.
    Horizontal, ///< From `gradientStart` at the left edge to `color` at the right edge.
    Vertical,   ///< From `gradientStart` at the top edge to `color` at the bottom edge.
};

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

    sf::Color color = sf::Color::Transparent; ///< Fill of a box; colour of a line or of text.

    /// A box can be filled with a gradient that ends in `color`. Lines and text ignore it.
    Gradient gradient = Gradient::None;
    sf::Color gradientStart = sf::Color::Transparent; ///< Where the gradient starts.

    sf::Color border = sf::Color::Transparent; ///< Outline of a box.
    float borderThickness = 0.f;

    /// Space between the outline and the fill. The outline stays at the edge of the box; the fill
    /// is drawn that much further in, so a box with a gap is no larger than one without.
    float borderGap = 0.f;

    /// How far the fill lies inside the box's edge: outline plus gap. A widget that draws one part
    /// inside another (the fill of a slider inside its track) insets it by this much to line up.
    [[nodiscard]] float contentInset() const { return borderThickness > 0.f ? borderThickness + borderGap : 0.f; }

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
///     theme[Panel::Background].borderThickness = 2.f;
///     theme[Button::Face].radius = 0.f;       // sharp buttons
///     theme[Panel::Background].borderGap = 0.f; // this part's outline right on its fill
///     theme[Panel::Background].gradient = Gradient::Vertical;
///     theme[Panel::Background].gradientStart = sf::Color(40, 40, 46);
///
/// A colour set here applies to every state; the theme still derives the hovered, pressed and
/// disabled variants from it.
struct PartOverride {
    std::optional<bool> shown;
    std::optional<sf::Color> color;
    std::optional<Gradient> gradient;
    std::optional<sf::Color> gradientStart;
    std::optional<sf::Color> border;
    std::optional<float> borderThickness;
    std::optional<float> borderGap;
    std::optional<float> radius;
    std::optional<Shadow> shadow;
    std::optional<float> thickness;
    std::optional<float> textSize;
    std::shared_ptr<const sf::Font> font; ///< Null: the font of the part's text type.
};

// ----- Tokens -----

/// An accent colour.
struct AccentColors {
    sf::Color accent; ///< Role::Accent, and what interactables turn to when hovered or used.

    /// Where accent-coloured boxes start their gradient, which runs from here on the left to
    /// `accent` on the right. The same as `accent`: no gradient.
    sf::Color accentStart;
};

/// The theme's colours.
///
/// A theme offers a list of main colours and a list of accents. A panel is drawn in three of
/// them, chosen by index in its setup: two main colours and one accent.
///
///   main1    the background of the panel
///   main2    the outline of the panel and of everything that can be operated
///   accent   what is filled (slider, progress bar, a switch that is on) and what outlines turn
///            to when hovered or used
///
/// Everything else is derived: text is the light or dark colour that reads best on main1, muted
/// text lies between text and main1, and the areas of buttons and fields are main1 moved a
/// little towards main2. Both main colours come from the same list, so they may also be equal.
struct Palette {
    sf::Color window; ///< Behind everything, where no view or panel is.
    sf::Color shadow; ///< Shadows of surfaces.

    std::vector<sf::Color> mains;      ///< At least two, so the defaults below exist.
    std::vector<AccentColors> accents; ///< At least one.
};

/// The three colours something is drawn in: indices into `Palette::mains` and `Palette::accents`.
/// The defaults are the theme's first main colour as background, its second as outline, and its
/// first accent.
struct PanelColors {
    std::size_t main1 = 0;
    std::size_t main2 = 1;
    std::size_t accent = 0;
};

/// Colours for a single widget that differ from its panel's. Only the ones that are set differ.
struct ColorOverride {
    std::optional<std::size_t> main1;
    std::optional<std::size_t> main2;
    std::optional<std::size_t> accent;
};

/// The theme's shapes. Pixels before GUI scaling.
struct Shape {
    float radius = 10.f;     ///< Corners of surfaces.
    float smallRadius = 5.f; ///< Corners of tracks and handles.
    float outline = 1.f;     ///< Outline thickness of panels and of everything that can be operated. 0: none.

    /// Space between such an outline and the fill inside it. The same for every part, unless a
    /// part entry says otherwise: `theme[Switch::Track].borderGap = 0.f;`.
    float outlineGap = 2.f;

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

// ----- Theme -----

/// A complete look: tokens, plus entries for individual parts.
///
/// A theme is a value: copy it, change it, hand it to the UI. Switching themes at runtime is
/// `UI::setTheme`.
class Theme {
public:
    Palette palette;
    Shape shape;
    Typography typography;

    /// The default font: used by every text type that does not name its own. Must be set before
    /// the theme is used; `App` sets the bundled font if the application sets none.
    std::shared_ptr<const sf::Font> font;

    /// The default look: the same as `themes::moon()`.
    Theme();

    /// The entry for one part, created if there is none yet. See `PartOverride`.
    [[nodiscard]] PartOverride& operator[](const Part& part);

    /// The entry for one part, or null if the theme has none.
    [[nodiscard]] const PartOverride* entry(const Part& part) const;

    /// The style of a part in a state, through all three layers, in the colours of a panel.
    /// Colours the theme does not have fall back to its first; `supports` tells beforehand.
    ///
    /// `scale` is the factor for everything measured in pixels: text sizes, outlines, corner
    /// radii, shadows. The UI passes the layout's (`Sizes::text`). Text sizes come out in whole
    /// pixels.
    [[nodiscard]] PartStyle
    resolve(const Part& part, State state = State::Normal, PanelColors colors = {}, float scale = 1.f) const;

    /// Whether the theme has these main and accent colours. The UI checks this for every panel
    /// when it is built and when the theme is replaced, and throws `SetupError` if not.
    [[nodiscard]] bool supports(PanelColors colors) const;

private:
    std::unordered_map<std::uint64_t, PartOverride> m_entries;
};

/// Ready-made themes.
namespace themes {

/// Black and white: black panels, grey outlines, white accents that fade in as gradients.
/// No shadows.
///
/// Main colours: 0 black, 1 grey, 2 white.
/// Accents: 0 white.
[[nodiscard]] Theme moon();

/// Warm and dark, with soft, round shapes and several accents to tell panels apart.
///
/// Main colours: 0 brown-grey, 1 light brown-grey, 2 near-black, 3 warm white.
/// Accents: 0 sand, 1 green, 2 blue, 3 red.
[[nodiscard]] Theme colorful();

} // namespace themes

} // namespace atpl
