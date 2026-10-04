#include "atpl/ui/theme.hpp"

#include "atpl/ui/setup.hpp"
#include "atpl/ui/widgets.hpp"

#include "ui/theme/color.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <type_traits>

namespace atpl {

namespace {

using theme::contrast;
using theme::faded;
using theme::luminance;
using theme::mix;
using theme::readableOn;

// How the derived colours relate to the three a panel chooses. Small steps on purpose: states
// should be noticed, not shout.
constexpr float areaTowardsOutline = 0.14f;     // the area of a button or field: main1 moved towards main2
constexpr float staticTextTowardsMuted = 0.65f; // a paragraph's body: between heading and footer, nearer the footer
constexpr float hoverTowardsAccent = 0.55f;     // an outline when hovered: main2 moved towards the accent
constexpr float pressAreaTowardsAccent = 0.16f; // an area while pressed takes a little of the accent
constexpr float knobHoverAmount = 0.22f;        // a hovered knob, towards the accent
constexpr float knobPressAmount = 0.45f;        // a pressed knob, towards the accent
constexpr float disabledOpacity = 0.4f;         // of everything
constexpr float handleShadow = 0.4f;            // a handle's shadow, as a share of a surface's
constexpr float textContrastWanted = 4.6f;      // muted text still has to be readable

const sf::Color lightText(245, 245, 247);
const sf::Color darkText(18, 18, 20);

/// The colours one panel is drawn in: the three it chose, and what follows from them.
struct Colors {
    sf::Color main1;     // background
    sf::Color main2;     // outlines
    AccentColors accent; // fills, and outlines in use

    sf::Color text;      // reads best on main1
    sf::Color mutedText; // between text and main1, still readable
    sf::Color area;      // main1 moved a little towards main2

    sf::Color window;
    sf::Color shadow;
};

/// The colours for a panel. An index the theme does not have falls back to the first entry; a
/// theme without entries gives transparent colours. Neither should happen: the UI checks
/// `Theme::supports` for every panel and refuses a theme that does not fit.
[[nodiscard]] Colors colorsFor(const Palette& palette, PanelColors chosen) {
    const auto pick = [](const auto& list, std::size_t index) {
        using Entry = typename std::remove_cvref_t<decltype(list)>::value_type;
        return index < list.size() ? list[index] : (list.empty() ? Entry{} : list.front());
    };

    Colors colors;
    colors.main1 = pick(palette.mains, chosen.main1);
    colors.main2 = pick(palette.mains, chosen.main2);
    colors.accent = pick(palette.accents, chosen.accent);
    colors.window = palette.window;
    colors.shadow = palette.shadow;

    colors.text = readableOn(colors.main1, lightText, darkText);
    colors.area = mix(colors.main1, colors.main2, areaTowardsOutline);

    // Muted text: as far towards the background as it can go and still be read.
    colors.mutedText = colors.text;
    for (float amount = 0.5f; amount > 0.f; amount -= 0.05f) {
        const sf::Color candidate = mix(colors.text, colors.main1, amount);
        if (contrast(candidate, colors.main1) >= textContrastWanted) {
            colors.mutedText = candidate;
            break;
        }
    }
    return colors;
}

[[nodiscard]] const sf::Font* fontOf(const TextType& type, const Theme& theme) {
    return type.font != nullptr ? type.font.get() : theme.font.get();
}

/// The accent look: the accent colour, as a gradient if the accent defines where it starts.
void setAccent(const AccentColors& accent, PartStyle& style) {
    style.color = accent.accent;
    style.gradientStart = accent.accentStart;
    style.gradient = accent.accentStart != accent.accent ? Gradient::Horizontal : Gradient::None;
}

[[nodiscard]] bool isText(Role role) {
    return role == Role::Title || role == Role::Heading || role == Role::Text || role == Role::MutedText;
}

/// Layer 2: the style a part gets from its role and the theme's tokens alone, in the normal
/// state and before GUI scaling.
[[nodiscard]] PartStyle roleDefault(Role role, const Theme& theme, const Colors& colors) {
    const Shape& shape = theme.shape;
    const Typography& typography = theme.typography;

    PartStyle style;
    style.thickness = shape.line;
    style.textSize = typography.text.size;
    style.font = fontOf(typography.text, theme);

    switch (role) {
        case Role::Surface:
            style.color = colors.main1;
            style.radius = shape.radius;
            style.border = colors.main2;
            style.borderThickness = shape.outline;
            style.borderGap = shape.outlineGap;
            style.shadow = { shape.shadowOffset, shape.shadowSize, colors.shadow };
            break;
        case Role::Track:
            style.color = colors.area;
            style.radius = shape.smallRadius;
            style.border = colors.main2;
            style.borderThickness = shape.outline;
            style.borderGap = shape.outlineGap;
            break;
        case Role::Accent:
            setAccent(colors.accent, style);
            style.radius = shape.smallRadius;
            break;
        case Role::Handle: // a knob: solid, without an outline of its own
            style.color = colors.text;
            style.radius = shape.smallRadius;
            style.shadow = { shape.shadowOffset * handleShadow, shape.shadowSize * handleShadow, colors.shadow };
            break;
        case Role::Line:
            style.color = colors.main2;
            break;
        case Role::Title:
            style.color = colors.text;
            style.textSize = typography.title.size;
            style.font = fontOf(typography.title, theme);
            break;
        case Role::Heading:
            style.color = colors.text;
            style.textSize = typography.heading.size;
            style.font = fontOf(typography.heading, theme);
            break;
        case Role::Text:
            style.color = colors.text;
            break;
        case Role::MutedText:
            style.color = colors.mutedText;
            style.textSize = typography.muted.size;
            style.font = fontOf(typography.muted, theme);
            break;
    }
    return style;
}

/// Layer 3: the fields a part entry sets replace what the role gave.
void applyEntry(const PartOverride& entry, PartStyle& style) {
    if (entry.shown.has_value()) {
        style.shown = *entry.shown;
    }
    if (entry.color.has_value()) {
        style.color = *entry.color;
        // A colour of its own replaces the role's gradient, unless the entry sets one too.
        style.gradient = Gradient::None;
    }
    if (entry.gradientStart.has_value()) {
        style.gradientStart = *entry.gradientStart;
    }
    if (entry.gradient.has_value()) {
        style.gradient = *entry.gradient;
    }
    if (entry.border.has_value()) {
        style.border = *entry.border;
    }
    if (entry.borderThickness.has_value()) {
        style.borderThickness = *entry.borderThickness;
    }
    if (entry.borderGap.has_value()) {
        style.borderGap = *entry.borderGap;
    }
    if (entry.radius.has_value()) {
        style.radius = *entry.radius;
    }
    if (entry.shadow.has_value()) {
        style.shadow = *entry.shadow;
    }
    if (entry.thickness.has_value()) {
        style.thickness = *entry.thickness;
    }
    if (entry.textSize.has_value()) {
        style.textSize = *entry.textSize;
    }
    if (entry.font != nullptr) {
        style.font = entry.font.get();
    }
}

/// The look of a state, derived from the part's own colours, so it also works for colours a part
/// entry has set.
void applyState(Role role, State state, const Theme& theme, const Colors& colors, PartStyle& style) {
    const sf::Color accent = colors.accent.accent;

    // Active: the part is "on". A track takes the accent look. Text and handles are assumed to sit
    // on something accent-coloured and take the light or the dark text colour, whichever stands
    // out best there.
    if (has(state, State::Active)) {
        if (role == Role::Track) {
            setAccent(colors.accent, style);
            style.border = accent;
        } else if (isText(role) || role == Role::Handle) {
            style.color = readableOn(accent, lightText, darkText);
        }
    }

    // Hover and use, for the areas that can be operated: the outline turns to the accent colour,
    // partly when hovered and fully when pressed, focused or open. A pressed area also takes a little
    // of the accent.
    if (role == Role::Track) {
        if (has(state, State::Pressed) || has(state, State::Focused) || has(state, State::Open)) {
            style.border = accent;
            style.borderThickness = std::max(style.borderThickness, theme.shape.outline);
        } else if (has(state, State::Hovered)) {
            style.border = mix(style.border, accent, hoverTowardsAccent);
        }
        if (has(state, State::Pressed) && !has(state, State::Active)) {
            style.color = mix(style.color, accent, pressAreaTowardsAccent);
        }
    }

    // Hover and use, for knobs: they have no outline, so the knob itself takes on some of the
    // accent colour, or of main2 where knob and accent look alike.
    if (role == Role::Handle && !has(state, State::Active)) {
        const sf::Color towards = contrast(style.color, accent) < 1.5f ? colors.main2 : accent;
        if (has(state, State::Pressed)) {
            style.color = mix(style.color, towards, knobPressAmount);
        } else if (has(state, State::Hovered)) {
            style.color = mix(style.color, towards, knobHoverAmount);
        }
    }

    // Disabled: everything fades.
    if (has(state, State::Disabled)) {
        style.color = faded(style.color, disabledOpacity);
        style.gradientStart = faded(style.gradientStart, disabledOpacity);
        style.border = faded(style.border, disabledOpacity);
        style.shadow.color = faded(style.shadow.color, disabledOpacity);
    }
}

/// Sizes are given before GUI scaling; a resolved style has it applied.
void applyScale(float scale, PartStyle& style) {
    style.borderThickness *= scale;
    style.borderGap *= scale;
    style.radius *= scale;
    style.shadow.offset *= scale;
    style.shadow.size *= scale;
    style.thickness *= scale;
    // Whole pixels: text stays sharp, and a window being resized does not change every text
    // with every pixel of movement.
    style.textSize = std::round(style.textSize * scale);
}

/// Moon: black and white.
[[nodiscard]] Palette moonPalette() {
    return {
        .window = sf::Color(6, 6, 8),
        .shadow = sf::Color(0, 0, 0, 0),
        .mains = {
            sf::Color(15, 15, 18),    // 0: black
            sf::Color(92, 94, 104),   // 1: grey
            sf::Color(255, 255, 255), // 2: white
        },
        .accents = { { .accent = sf::Color(255, 255, 255), .accentStart = sf::Color(126, 130, 142) } },
    };
}

[[nodiscard]] Shape moonShape() {
    Shape shape;
    shape.radius = 12.f;
    shape.smallRadius = 6.f;
    shape.outline = 1.f;
    shape.shadowSize = 0.f; // a shadow would not show on black
    return shape;
}

} // namespace

Theme::Theme() :
    palette(moonPalette()),
    shape(moonShape()) {
    // Where the gap between outline and fill differs from the theme's `shape.outlineGap`.
    (*this)[Switch::Track].borderGap = 0.f;
}

PartOverride& Theme::operator[](const Part& part) {
    return m_entries[part.id()];
}

const PartOverride* Theme::entry(const Part& part) const {
    const auto found = m_entries.find(part.id());
    return found != m_entries.end() ? &found->second : nullptr;
}

bool Theme::supports(PanelColors colors) const {
    return colors.main1 < palette.mains.size() && colors.main2 < palette.mains.size() &&
           colors.accent < palette.accents.size();
}

namespace {

sf::Color mixColor(sf::Color a, sf::Color b, float t) {
    const auto channel = [t](std::uint8_t x, std::uint8_t y) {
        return static_cast<std::uint8_t>(
            std::lround(static_cast<float>(x) + (static_cast<float>(y) - static_cast<float>(x)) * t)
        );
    };
    return { channel(a.r, b.r), channel(a.g, b.g), channel(a.b, b.b), channel(a.a, b.a) };
}

float mixFloat(float a, float b, float t) {
    return a + (b - a) * t;
}

/// The style made invisible: every colour transparent, the rest kept, so that fading to it
/// changes nothing but how much shows.
PartStyle transparentCopy(PartStyle style) {
    style.color.a = 0;
    style.gradientStart.a = 0;
    style.border.a = 0;
    style.shadow.color.a = 0;
    return style;
}

} // namespace

PartStyle mix(const PartStyle& from, const PartStyle& to, float t) {
    t = std::clamp(t, 0.f, 1.f);
    if (t <= 0.f) {
        return from;
    }
    if (t >= 1.f) {
        return to;
    }
    if (!from.shown && !to.shown) {
        return to;
    }
    // A part shown at one end only fades in or out there.
    const PartStyle a = from.shown ? from : transparentCopy(to);
    const PartStyle b = to.shown ? to : transparentCopy(from);

    PartStyle style = t < 0.5f ? a : b; // what does not move: the nearer end's
    style.shown = true;
    style.color = mixColor(a.color, b.color, t);
    style.gradientStart = mixColor(a.gradientStart, b.gradientStart, t);
    style.border = mixColor(a.border, b.border, t);
    style.borderThickness = mixFloat(a.borderThickness, b.borderThickness, t);
    style.borderGap = mixFloat(a.borderGap, b.borderGap, t);
    style.radius = mixFloat(a.radius, b.radius, t);
    style.shadow.offset = a.shadow.offset + (b.shadow.offset - a.shadow.offset) * t;
    style.shadow.size = mixFloat(a.shadow.size, b.shadow.size, t);
    style.shadow.color = mixColor(a.shadow.color, b.shadow.color, t);
    style.thickness = mixFloat(a.thickness, b.thickness, t);
    return style;
}

PartStyle Theme::resolve(const Part& part, State state, PanelColors chosen, float scale) const {
    const Colors colors = colorsFor(palette, chosen);
    PartStyle style = roleDefault(part.role, *this, colors);
    style.shown = part.shown == Shown::Yes;

    // A paragraph's body is static text: quieter than the values of widgets, which change, and
    // clearly set apart from its heading. Its colour and its size lie between the heading's and
    // the footer's, as this theme has them, nearer the footer.
    if (part == Paragraph::Body) {
        const PartStyle heading = resolve(Paragraph::Heading, State::Normal, chosen);
        const PartStyle footer = resolve(Paragraph::Footer, State::Normal, chosen);
        style.color = mix(heading.color, footer.color, staticTextTowardsMuted);
        style.textSize = heading.textSize + (footer.textSize - heading.textSize) * staticTextTowardsMuted;
    }

    if (const PartOverride* override = entry(part)) {
        applyEntry(*override, style);
    }
    applyState(part.role, state, *this, colors, style);
    applyScale(scale, style);
    return style;
}

namespace themes {

Theme moon() {
    return Theme();
}

Theme colorful() {
    const auto flat = [](sf::Color color) { return AccentColors{ .accent = color, .accentStart = color }; };

    Theme theme;
    theme.palette = {
        .window = sf::Color(36, 34, 32),
        .shadow = sf::Color(0, 0, 0, 140),
        .mains = {
            sf::Color(56, 53, 50),    // 0: brown-grey
            sf::Color(122, 116, 110), // 1: light brown-grey
            sf::Color(27, 26, 25),    // 2: near-black
            sf::Color(240, 235, 228), // 3: warm white
        },
        .accents = {
            flat(sf::Color(212, 163, 115)), // 0: sand
            flat(sf::Color(122, 205, 96)),  // 1: green
            flat(sf::Color(84, 164, 226)),  // 2: blue
            flat(sf::Color(232, 108, 92)),  // 3: red
        },
    };
    theme.shape = Shape{};
    theme.shape.radius = 18.f;
    theme.shape.smallRadius = 9.f;
    theme.shape.outline = 1.5f;
    theme.shape.shadowSize = 14.f;
    theme.shape.shadowOffset = { 0.f, 5.f };

    // Where the gap between outline and fill differs from the theme's `shape.outlineGap`:
    // the soft, shadowed surfaces of this theme look better with their outline right on the fill.
    theme[Panel::Background].borderGap = 0.f;
    theme[Dropdown::List].borderGap = 0.f;
    theme[Switch::Track].borderGap = 0.f;
    return theme;
}

} // namespace themes

} // namespace atpl
