#include "atpl/ui/setup.hpp"
#include "atpl/ui/theme.hpp"
#include "atpl/ui/widgets.hpp"

#include "ui/theme/color.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <cstdlib>
#include <memory>

using namespace atpl;
using atpl::theme::contrast;
using atpl::theme::luminance;
using Catch::Approx;

namespace {

// A widget no theme has ever heard of.
struct Dial {
    static constexpr Kind kind{ "dial" };
    static constexpr Part Face{ kind, "face", Role::Track };
    static constexpr Part Needle{ kind, "needle", Role::Accent };
    static constexpr Part Caption{ kind, "caption", Role::MutedText };
    static constexpr Part Scale{ kind, "scale", Role::Line, Shown::No };
};

Theme themeWithFont() {
    Theme theme;
    theme.font = std::make_shared<sf::Font>();
    return theme;
}

// The three colours a panel with default settings is drawn in.
sf::Color main1(const Theme& theme) {
    return theme.palette.mains[0];
}
sf::Color main2(const Theme& theme) {
    return theme.palette.mains[1];
}
sf::Color accent(const Theme& theme) {
    return theme.palette.accents[0].accent;
}

} // namespace

// ----- Colours -----

TEST_CASE("mixing moves a colour towards another and keeps its transparency", "[ui][theme][color]") {
    const sf::Color from(0, 100, 200, 128);
    const sf::Color to(100, 200, 0, 255);

    REQUIRE(theme::mix(from, to, 0.f) == from);
    REQUIRE(theme::mix(from, to, 1.f) == sf::Color(100, 200, 0, 128));
    REQUIRE(theme::mix(from, to, 0.5f) == sf::Color(50, 150, 100, 128));
    REQUIRE(theme::mix(from, to, 7.f) == sf::Color(100, 200, 0, 128)); // limited to the range
}

TEST_CASE("fading changes only the transparency", "[ui][theme][color]") {
    REQUIRE(theme::faded(sf::Color(10, 20, 30, 200), 0.5f) == sf::Color(10, 20, 30, 100));
    REQUIRE(theme::faded(sf::Color(10, 20, 30, 200), 0.f).a == 0);
}

TEST_CASE("contrast runs from 1 for equal colours to 21 for black on white", "[ui][theme][color]") {
    REQUIRE(contrast(sf::Color::White, sf::Color::White) == Approx(1.f));
    REQUIRE(contrast(sf::Color::Black, sf::Color::White) == Approx(21.f));
    REQUIRE(contrast(sf::Color::White, sf::Color::Black) == Approx(21.f));
    REQUIRE(luminance(sf::Color::Black) == Approx(0.f));
    REQUIRE(luminance(sf::Color::White) == Approx(1.f));
}

TEST_CASE("readableOn picks the candidate that stands out more", "[ui][theme][color]") {
    const sf::Color dark(20, 20, 20);
    const sf::Color light(240, 240, 240);

    REQUIRE(theme::readableOn(sf::Color(250, 220, 90), dark, light) == dark);
    REQUIRE(theme::readableOn(sf::Color(40, 40, 120), dark, light) == light);
}

// ----- Layer 2: what follows from the three colours -----

TEST_CASE("a panel is main1 with an outline in main2", "[ui][theme]") {
    const auto theme = GENERATE(themes::moon(), themes::colorful());

    const PartStyle panel = theme.resolve(Panel::Background);
    REQUIRE(panel.color == main1(theme));
    REQUIRE(panel.border == main2(theme));
    REQUIRE(panel.borderThickness == theme.shape.outline);
    REQUIRE(panel.borderThickness > 0.f);
    REQUIRE(panel.radius == theme.shape.radius);
    REQUIRE(panel.shadow.size == theme.shape.shadowSize);
    REQUIRE(panel.shadow.color == theme.palette.shadow);
}

TEST_CASE("what can be operated has a subtle area and an outline in main2", "[ui][theme]") {
    const auto theme = GENERATE(themes::moon(), themes::colorful());

    for (const Part& part : { Button::Face, TextInput::Field, Dropdown::Field, Slider::Track, Switch::Track }) {
        const PartStyle style = theme.resolve(part);
        REQUIRE(style.border == main2(theme));
        REQUIRE(style.borderThickness == theme.shape.outline);

        // The area lies between main1 and main2, close to main1.
        REQUIRE(style.color != main1(theme));
        REQUIRE(contrast(style.color, main1(theme)) < contrast(main2(theme), main1(theme)));
        REQUIRE(contrast(style.color, main1(theme)) < 1.6f);
    }
}

TEST_CASE("a knob is solid, without an outline, and stands out", "[ui][theme]") {
    const auto theme = GENERATE(themes::moon(), themes::colorful());

    for (const Part& part : { Slider::Knob, Switch::Knob }) {
        const PartStyle knob = theme.resolve(part);
        REQUIRE(knob.borderThickness == 0.f);
        REQUIRE(knob.contentInset() == 0.f);
        REQUIRE(contrast(knob.color, main1(theme)) >= 7.f); // it has the text colour
    }
}

TEST_CASE("a knob shows hover and press in its own colour", "[ui][theme]") {
    const auto theme = GENERATE(themes::moon(), themes::colorful());

    const sf::Color idle = theme.resolve(Slider::Knob).color;
    const sf::Color hovered = theme.resolve(Slider::Knob, State::Hovered).color;
    const sf::Color pressed = theme.resolve(Slider::Knob, State::Pressed).color;

    REQUIRE(contrast(hovered, idle) >= 1.05f);
    REQUIRE(contrast(pressed, idle) > contrast(hovered, idle));
    REQUIRE(theme.resolve(Slider::Knob, State::Hovered).borderThickness == 0.f);
}

// The gap between an outline and the fill inside it: one value per theme, with exceptions per part.
TEST_CASE("the outline gap is the theme's everywhere, except where a part entry says otherwise", "[ui][theme]") {
    struct Expected {
        const Part& part;
        bool gapInMoon;
        bool gapInColorful;
    };
    const Expected table[] = {
        { Panel::Background, true, false }, { Dropdown::List, true, false },    { Button::Face, true, true },
        { Slider::Track, true, true },      { ProgressBar::Track, true, true }, { TextInput::Field, true, true },
        { Dropdown::Field, true, true },    { Graph::Background, true, true },  { Switch::Track, false, false },
    };

    const Theme moon = themes::moon();
    const Theme colorful = themes::colorful();
    REQUIRE(moon.shape.outlineGap > 0.f);
    REQUIRE(colorful.shape.outlineGap > 0.f);

    for (const Expected& expected : table) {
        CAPTURE(expected.part.kind.name, expected.part.name);
        REQUIRE(moon.resolve(expected.part).borderGap == (expected.gapInMoon ? moon.shape.outlineGap : 0.f));
        REQUIRE(
            colorful.resolve(expected.part).borderGap == (expected.gapInColorful ? colorful.shape.outlineGap : 0.f)
        );
    }
}

TEST_CASE("a theme can change the gap for all parts or for one", "[ui][theme]") {
    Theme theme = themes::moon();

    theme.shape.outlineGap = 5.f;
    REQUIRE(theme.resolve(Button::Face).borderGap == 5.f);
    REQUIRE(theme.resolve(TextInput::Field).borderGap == 5.f);
    REQUIRE(theme.resolve(Switch::Track).borderGap == 0.f); // its entry still applies

    theme[Button::Face].borderGap = 0.f;
    theme[Switch::Track].borderGap = 3.f;
    REQUIRE(theme.resolve(Button::Face).borderGap == 0.f);
    REQUIRE(theme.resolve(Switch::Track).borderGap == 3.f);
    REQUIRE(theme.resolve(TextInput::Field).borderGap == 5.f);
}

TEST_CASE("what is filled takes the accent, lines take main2", "[ui][theme]") {
    const auto theme = GENERATE(themes::moon(), themes::colorful());

    REQUIRE(theme.resolve(Slider::Fill).color == accent(theme));
    REQUIRE(theme.resolve(ProgressBar::Fill).color == accent(theme));
    REQUIRE(theme.resolve(Graph::Curve).color == accent(theme));
    REQUIRE(theme.resolve(Slider::Fill).borderThickness == 0.f);

    const PartStyle line = theme.resolve(Graph::Axis);
    REQUIRE(line.color == main2(theme));
    REQUIRE(line.thickness == theme.shape.line);
}

TEST_CASE("text is derived from main1: light on dark, dark on light, and always readable", "[ui][theme]") {
    Theme theme = themeWithFont();

    for (const sf::Color background : { sf::Color(0, 0, 0),
                                        sf::Color(15, 15, 18),
                                        sf::Color(56, 53, 50),
                                        sf::Color(128, 128, 128),
                                        sf::Color(200, 205, 215),
                                        sf::Color(255, 255, 255) }) {
        theme.palette.mains[0] = background;
        const sf::Color text = theme.resolve(ValueDisplay::ValueText).color;
        const sf::Color muted = theme.resolve(Paragraph::Footer).color;

        REQUIRE(contrast(text, background) >= 4.5f);
        REQUIRE(contrast(muted, background) >= 4.5f);
        REQUIRE(contrast(muted, background) <= contrast(text, background)); // muted is the quieter one

        const bool darkBackground = luminance(background) < 0.18f;
        REQUIRE((luminance(text) > 0.5f) == darkBackground);
    }
}

TEST_CASE("each text role has the size and font of its text type", "[ui][theme]") {
    Theme theme = themeWithFont();
    const auto titleFont = std::make_shared<sf::Font>();
    theme.typography.title.font = titleFont;

    const PartStyle title = theme.resolve(Panel::Title);
    REQUIRE(title.textSize == theme.typography.title.size);
    REQUIRE(title.font == titleFont.get()); // its own font

    const PartStyle heading = theme.resolve(Paragraph::Heading);
    REQUIRE(heading.textSize == theme.typography.heading.size);
    REQUIRE(heading.font == theme.font.get()); // no own font: the default

    REQUIRE(theme.resolve(ValueDisplay::ValueText).textSize == theme.typography.text.size);
    REQUIRE(theme.resolve(Slider::Label).textSize == theme.typography.muted.size);
    REQUIRE(theme.resolve(Slider::Label).font == theme.font.get());
    REQUIRE(title.color == theme.resolve(ValueDisplay::ValueText).color);
}

TEST_CASE("a widget the theme has never heard of gets its look from its roles", "[ui][theme]") {
    const Theme theme = themeWithFont();

    REQUIRE(theme.entry(Dial::Face) == nullptr);
    REQUIRE(theme.resolve(Dial::Face).color == theme.resolve(Button::Face).color);
    REQUIRE(theme.resolve(Dial::Face).border == main2(theme));
    REQUIRE(theme.resolve(Dial::Needle).color == accent(theme));
    REQUIRE(theme.resolve(Dial::Caption).color == theme.resolve(Slider::Label).color);
    REQUIRE(theme.resolve(Dial::Face).shown);
    REQUIRE_FALSE(theme.resolve(Dial::Scale).shown);
}

TEST_CASE("a part is shown or hidden as it declares", "[ui][theme]") {
    const Theme theme;

    REQUIRE(theme.resolve(Slider::Track).shown);
    REQUIRE_FALSE(theme.resolve(Slider::Ticks).shown);
}

// ----- Layer 3: part entries -----

TEST_CASE("a part entry replaces only the fields it sets", "[ui][theme]") {
    Theme theme;
    const PartStyle before = theme.resolve(Button::Face);

    theme[Button::Face].radius = 0.f;
    const PartStyle after = theme.resolve(Button::Face);

    REQUIRE(after.radius == 0.f);
    REQUIRE(after.color == before.color);
    REQUIRE(after.border == before.border);
    REQUIRE(after.thickness == before.thickness);
    REQUIRE(after.shown == before.shown);

    // Other parts, also of the same role, are untouched.
    REQUIRE(theme.resolve(Slider::Track).radius == theme.shape.smallRadius);
}

TEST_CASE("a part entry can show an optional part and hide a regular one", "[ui][theme]") {
    Theme theme;
    theme[Slider::Ticks].shown = true;
    theme[Slider::Label].shown = false;
    theme[Dial::Scale].shown = true; // works for parts of any widget

    REQUIRE(theme.resolve(Slider::Ticks).shown);
    REQUIRE_FALSE(theme.resolve(Slider::Label).shown);
    REQUIRE(theme.resolve(Dial::Scale).shown);
}

TEST_CASE("a part entry can set every field, including the font", "[ui][theme]") {
    Theme theme = themeWithFont();
    const auto ownFont = std::make_shared<sf::Font>();

    PartOverride& label = theme[Button::Label];
    label.color = sf::Color::Red;
    label.textSize = 20.f;
    label.font = ownFont;

    PartOverride& axis = theme[Graph::Axis];
    axis.shown = true;
    axis.thickness = 3.f;

    PartOverride& face = theme[Button::Face];
    face.border = sf::Color::Green;
    face.borderThickness = 2.f;
    face.shadow = Shadow{ .offset = { 1.f, 2.f }, .size = 5.f, .color = sf::Color::Black };

    REQUIRE(theme.resolve(Button::Label).color == sf::Color::Red);
    REQUIRE(theme.resolve(Button::Label).textSize == 20.f);
    REQUIRE(theme.resolve(Button::Label).font == ownFont.get());
    REQUIRE(theme.resolve(Graph::Axis).shown);
    REQUIRE(theme.resolve(Graph::Axis).thickness == 3.f);
    REQUIRE(theme.resolve(Button::Face).border == sf::Color::Green);
    REQUIRE(theme.resolve(Button::Face).borderThickness == 2.f);
    REQUIRE(theme.resolve(Button::Face).shadow.size == 5.f);
}

TEST_CASE("entries are found only where they were made, and travel with a copy", "[ui][theme]") {
    Theme theme;
    REQUIRE(theme.entry(Slider::Ticks) == nullptr);

    theme[Slider::Ticks].shown = true;
    REQUIRE(theme.entry(Slider::Ticks) != nullptr);
    REQUIRE(theme.entry(Slider::Track) == nullptr);
    REQUIRE(theme.entry(Switch::Track) != nullptr); // one of the entries the built-in theme comes with

    const Theme copy = theme;
    theme[Slider::Ticks].shown = false;
    REQUIRE(copy.resolve(Slider::Ticks).shown); // the copy is its own theme
}

// ----- Gradients -----

TEST_CASE("the accent is a gradient where it says where it starts", "[ui][theme]") {
    const Theme moon = themes::moon();
    const PartStyle fading = moon.resolve(Slider::Fill);
    REQUIRE(fading.gradient == Gradient::Horizontal);
    REQUIRE(fading.gradientStart == moon.palette.accents[0].accentStart);
    REQUIRE(fading.color == moon.palette.accents[0].accent);

    const Theme colorful = themes::colorful();
    REQUIRE(colorful.palette.accents[0].accentStart == colorful.palette.accents[0].accent);
    REQUIRE(colorful.resolve(Slider::Fill).gradient == Gradient::None);
}

TEST_CASE("a part entry can give any part a gradient, and a plain colour removes one", "[ui][theme]") {
    Theme theme = themes::moon();

    theme[Panel::Background].gradient = Gradient::Vertical;
    theme[Panel::Background].gradientStart = sf::Color(40, 40, 46);
    const PartStyle panel = theme.resolve(Panel::Background);
    REQUIRE(panel.gradient == Gradient::Vertical);
    REQUIRE(panel.gradientStart == sf::Color(40, 40, 46));
    REQUIRE(panel.color == main1(theme));

    // The accent's own gradient goes away when a part is given one flat colour.
    theme[Slider::Fill].color = sf::Color::Red;
    REQUIRE(theme.resolve(Slider::Fill).gradient == Gradient::None);
    REQUIRE(theme.resolve(Slider::Fill).color == sf::Color::Red);

    // Other accent parts keep it.
    REQUIRE(theme.resolve(ProgressBar::Fill).gradient == Gradient::Horizontal);
}

// ----- GUI scale -----

TEST_CASE("the scale multiplies every size and no colour", "[ui][theme]") {
    Theme theme = themes::colorful();
    theme.font = std::make_shared<sf::Font>();
    const PartStyle surface = theme.resolve(Panel::Background);
    const PartStyle title = theme.resolve(Panel::Title);
    const PartStyle line = theme.resolve(Graph::Axis);

    const PartStyle bigSurface = theme.resolve(Panel::Background, State::Normal, {}, 2.f);

    REQUIRE(bigSurface.radius == surface.radius * 2.f);
    REQUIRE(bigSurface.borderThickness == surface.borderThickness * 2.f);
    REQUIRE(bigSurface.borderGap == surface.borderGap * 2.f);
    REQUIRE(bigSurface.shadow.size == surface.shadow.size * 2.f);
    REQUIRE(bigSurface.shadow.offset == surface.shadow.offset * 2.f);
    REQUIRE(bigSurface.color == surface.color);
    REQUIRE(theme.resolve(Panel::Title, State::Normal, {}, 2.f).textSize == title.textSize * 2.f);
    REQUIRE(theme.resolve(Graph::Axis, State::Normal, {}, 2.f).thickness == line.thickness * 2.f);

    // Sizes set by a part entry are scaled like the tokens.
    theme[Button::Face].radius = 3.f;
    REQUIRE(theme.resolve(Button::Face, State::Normal, {}, 2.f).radius == 6.f);
}

// ----- States -----

TEST_CASE("the outline of what can be operated turns to the accent on hover and use", "[ui][theme]") {
    const Theme theme = themes::colorful();

    for (const Part& part : { Button::Face, TextInput::Field, Slider::Track }) {
        const sf::Color idle = theme.resolve(part).border;
        const sf::Color hovered = theme.resolve(part, State::Hovered).border;
        const sf::Color pressed = theme.resolve(part, State::Pressed).border;
        const sf::Color focused = theme.resolve(part, State::Focused).border;

        REQUIRE(idle == main2(theme));
        REQUIRE(hovered != idle);
        REQUIRE(hovered != accent(theme)); // on the way, not there yet
        REQUIRE(contrast(hovered, accent(theme)) < contrast(idle, accent(theme)));
        REQUIRE(pressed == accent(theme));
        REQUIRE(focused == accent(theme));

        // A press wins over a hover.
        REQUIRE(theme.resolve(part, State::Hovered | State::Pressed).border == accent(theme));
    }
}

TEST_CASE("hovering leaves an area's fill alone; pressing tints it a little", "[ui][theme]") {
    const Theme theme = themes::colorful();
    const sf::Color idle = theme.resolve(Button::Face).color;

    REQUIRE(theme.resolve(Button::Face, State::Hovered).color == idle);

    const sf::Color pressed = theme.resolve(Button::Face, State::Pressed).color;
    REQUIRE(pressed != idle);
    REQUIRE(contrast(pressed, idle) < 1.5f); // a little
}

TEST_CASE("what cannot be operated does not react to hover or press", "[ui][theme]") {
    const Theme theme = themes::colorful();

    for (const Part& part : { Panel::Background, Slider::Fill, Slider::ValueText, Slider::Label, Graph::Axis }) {
        const PartStyle idle = theme.resolve(part);
        for (const State state : { State::Hovered, State::Pressed, State::Focused }) {
            const PartStyle changed = theme.resolve(part, state);
            REQUIRE(changed.color == idle.color);
            REQUIRE(changed.border == idle.border);
        }
    }
}

TEST_CASE("an active track takes the accent look, and what sits on it stays visible", "[ui][theme]") {
    const auto theme = GENERATE(themes::moon(), themes::colorful());

    const PartStyle track = theme.resolve(Switch::Track, State::Active);
    REQUIRE(track.color == accent(theme));
    REQUIRE(track.gradientStart == theme.palette.accents[0].accentStart);
    REQUIRE(track.border == accent(theme));

    // Text on the accent colour, over the whole width of a gradient.
    const sf::Color text = theme.resolve(Dropdown::Entry, State::Active).color;
    REQUIRE(contrast(text, accent(theme)) >= 4.5f);
    REQUIRE(contrast(text, theme.palette.accents[0].accentStart) >= 4.5f);

    // The knob of a switch that is on.
    const sf::Color knob = theme.resolve(Switch::Knob, State::Active).color;
    REQUIRE(contrast(knob, accent(theme)) >= 3.f);
}

TEST_CASE("a disabled part fades, whatever its role", "[ui][theme]") {
    const Theme theme = themes::colorful();

    for (const Part& part : { Button::Face, Button::Label, Slider::Fill, Slider::Knob, Panel::Background }) {
        const PartStyle normal = theme.resolve(part);
        const PartStyle disabled = theme.resolve(part, State::Disabled);
        REQUIRE(disabled.color.a < normal.color.a);
        REQUIRE(disabled.color.r == normal.color.r);
    }
    REQUIRE(theme.resolve(Button::Face, State::Disabled).border.a < 255);
    REQUIRE(theme.resolve(Panel::Background, State::Disabled).shadow.color.a < theme.palette.shadow.a);
}

TEST_CASE("states are derived from colours a part entry has set", "[ui][theme]") {
    Theme theme = themes::colorful();
    theme[Button::Face].color = sf::Color(200, 40, 40);
    theme[Button::Face].border = sf::Color(10, 10, 10);

    REQUIRE(theme.resolve(Button::Face).color == sf::Color(200, 40, 40));
    REQUIRE(theme.resolve(Button::Face).border == sf::Color(10, 10, 10));

    // Hover moves the entry's outline towards the accent, not the role's.
    const sf::Color hovered = theme.resolve(Button::Face, State::Hovered).border;
    REQUIRE(hovered != sf::Color(10, 10, 10));
    REQUIRE(hovered != theme.resolve(TextInput::Field, State::Hovered).border);

    REQUIRE(theme.resolve(Button::Face, State::Disabled).color.a < 255);
}

// ----- Colours per panel and per widget -----

TEST_CASE("the defaults are the first main colour, the second, and the first accent", "[ui][theme]") {
    const PanelColors defaults;

    REQUIRE(defaults.main1 == 0);
    REQUIRE(defaults.main2 == 1);
    REQUIRE(defaults.accent == 0);
}

TEST_CASE("a panel is drawn in the three colours it chose", "[ui][theme]") {
    const Theme theme = themes::colorful();
    const Palette& palette = theme.palette;
    REQUIRE(palette.mains.size() >= 3);
    REQUIRE(palette.accents.size() >= 3);

    // Another accent changes what is accent-coloured, and nothing else.
    const PanelColors green{ .main1 = 0, .main2 = 1, .accent = 1 };
    REQUIRE(theme.resolve(Slider::Fill, State::Normal, green).color == palette.accents[1].accent);
    REQUIRE(theme.resolve(Switch::Track, State::Active, green).color == palette.accents[1].accent);
    REQUIRE(theme.resolve(Button::Face, State::Pressed, green).border == palette.accents[1].accent);
    REQUIRE(theme.resolve(Panel::Background, State::Normal, green).color == palette.mains[0]);
    REQUIRE(theme.resolve(Button::Face, State::Normal, green).border == palette.mains[1]);

    // Other main colours: background and outline swap their source.
    const PanelColors dark{ .main1 = 2, .main2 = 0, .accent = 0 };
    REQUIRE(theme.resolve(Panel::Background, State::Normal, dark).color == palette.mains[2]);
    REQUIRE(theme.resolve(Panel::Background, State::Normal, dark).border == palette.mains[0]);
    REQUIRE(theme.resolve(Slider::Fill, State::Normal, dark).color == palette.accents[0].accent);

    // A light background turns the text dark.
    const PanelColors light{ .main1 = 3, .main2 = 1, .accent = 2 };
    const sf::Color text = theme.resolve(ValueDisplay::ValueText, State::Normal, light).color;
    REQUIRE(luminance(text) < 0.2f);
    REQUIRE(contrast(text, palette.mains[3]) >= 7.f);
}

TEST_CASE(
    "a paragraph's body is between its heading and its footer, nearer the footer, unless the theme sets it",
    "[ui][theme]"
) {
    const auto between = [](float heading, float body, float footer) {
        return std::abs(body - footer) < std::abs(body - heading) && (body - heading) * (body - footer) <= 0.f;
    };
    for (Theme theme : { themes::moon(), themes::colorful() }) {
        const PartStyle heading = theme.resolve(Paragraph::Heading);
        const PartStyle footer = theme.resolve(Paragraph::Footer);
        const PartStyle body = theme.resolve(Paragraph::Body);
        REQUIRE(body.color != heading.color);
        REQUIRE(body.color != footer.color);
        REQUIRE(between(heading.color.r, body.color.r, footer.color.r));
        REQUIRE(between(heading.color.g, body.color.g, footer.color.g));
        REQUIRE(between(heading.textSize, body.textSize, footer.textSize));
        REQUIRE(body.textSize < theme.resolve(ValueDisplay::ValueText).textSize); // smaller than values

        // It follows the heading and the footer as the theme sets them ...
        theme[Paragraph::Heading].color = sf::Color(200, 0, 0);
        theme[Paragraph::Footer].color = sf::Color(0, 0, 100);
        theme[Paragraph::Heading].textSize = 20.f;
        theme[Paragraph::Footer].textSize = 10.f;
        const PartStyle followed = theme.resolve(Paragraph::Body);
        REQUIRE(std::abs(int(followed.color.r) - 70) <= 1);
        REQUIRE(followed.color.g == 0);
        REQUIRE(std::abs(int(followed.color.b) - 65) <= 1);
        REQUIRE(followed.textSize == Catch::Approx(13.5f).margin(0.5f)); // sizes are whole pixels
        // ... and its own colour and size replace them.
        theme[Paragraph::Body].color = sf::Color(1, 2, 3);
        theme[Paragraph::Body].textSize = 17.f;
        REQUIRE(theme.resolve(Paragraph::Body).color == sf::Color(1, 2, 3));
        REQUIRE(theme.resolve(Paragraph::Body).textSize == 17.f);
    }
}

TEST_CASE("both main colours come from the same list and may be the same", "[ui][theme]") {
    const Theme theme = themes::colorful();
    const PanelColors same{ .main1 = 0, .main2 = 0, .accent = 0 };

    const PartStyle panel = theme.resolve(Panel::Background, State::Normal, same);
    REQUIRE(panel.color == theme.palette.mains[0]);
    REQUIRE(panel.border == theme.palette.mains[0]);
    REQUIRE(theme.resolve(Button::Face, State::Normal, same).color == theme.palette.mains[0]);
}

TEST_CASE("a theme says whether it has the colours a panel asks for", "[ui][theme]") {
    const Theme moon = themes::moon();
    REQUIRE(moon.palette.mains.size() == 3);
    REQUIRE(moon.palette.accents.size() == 1);
    REQUIRE(moon.supports({}));
    REQUIRE(moon.supports({ .main1 = 0, .main2 = 2, .accent = 0 }));
    REQUIRE_FALSE(moon.supports({ .main1 = 0, .main2 = 1, .accent = 1 }));
    REQUIRE_FALSE(moon.supports({ .main1 = 3, .main2 = 1, .accent = 0 }));
    REQUIRE_FALSE(moon.supports({ .main1 = 0, .main2 = 3, .accent = 0 }));

    const Theme colorful = themes::colorful();
    REQUIRE(colorful.supports({ .main1 = 3, .main2 = 2, .accent = 3 }));
    REQUIRE_FALSE(colorful.supports({ .main1 = 0, .main2 = 1, .accent = colorful.palette.accents.size() }));
}

TEST_CASE("colours a theme does not have fall back to its first, without failing", "[ui][theme]") {
    const Theme moon = themes::moon();
    const PartStyle fallback = moon.resolve(Slider::Fill, State::Normal, { .main1 = 7, .main2 = 8, .accent = 9 });
    REQUIRE(fallback.color == moon.palette.accents[0].accent);

    Theme empty;
    empty.palette.mains.clear();
    empty.palette.accents.clear();
    REQUIRE_FALSE(empty.supports({}));
    REQUIRE(empty.resolve(Panel::Background).color == sf::Color()); // nothing to draw with, but no crash
}

TEST_CASE("a widget can deviate from its panel in any of the three colours", "[ui][theme]") {
    const ColorOverride none;
    REQUIRE_FALSE(none.main1.has_value());
    REQUIRE_FALSE(none.main2.has_value());
    REQUIRE_FALSE(none.accent.has_value());

    const ColorOverride red{ .accent = 3 };
    REQUIRE(red.accent == 3);
    REQUIRE_FALSE(red.main1.has_value());
}

// ----- The built-in themes -----

TEST_CASE("the default theme is moon", "[ui][theme]") {
    const Theme byDefault;
    const Theme moon = themes::moon();

    REQUIRE(byDefault.palette.window == moon.palette.window);
    REQUIRE(byDefault.palette.mains == moon.palette.mains);
    REQUIRE(byDefault.resolve(Switch::Track).borderGap == moon.resolve(Switch::Track).borderGap);
    REQUIRE(themes::colorful().palette.window != moon.palette.window);
}

TEST_CASE("the built-in themes are readable with their default main colours and every accent", "[ui][theme]") {
    const auto theme = GENERATE(themes::moon(), themes::colorful());
    const Palette& palette = theme.palette;
    const sf::Color background = palette.mains[0];

    // The outline can be told from the background.
    REQUIRE(contrast(palette.mains[1], background) >= 1.5f);

    // Panels can be told from the window: here always by their outline.
    REQUIRE(theme.shape.outline > 0.f);

    for (std::size_t a = 0; a < palette.accents.size(); ++a) {
        const AccentColors& accentColors = palette.accents[a];
        const PanelColors colors{ .main1 = 0, .main2 = 1, .accent = a };
        CAPTURE(a);

        // Text on the areas of buttons and fields.
        const sf::Color area = theme.resolve(Button::Face, State::Normal, colors).color;
        REQUIRE(contrast(theme.resolve(Button::Label, State::Normal, colors).color, area) >= 4.5f);
        REQUIRE(contrast(theme.resolve(Slider::Label, State::Normal, colors).color, background) >= 4.5f);

        // The accent against the panel and against an area.
        REQUIRE(contrast(accentColors.accent, background) >= 3.f);
        REQUIRE(contrast(accentColors.accent, area) >= 2.5f);

        // Text and the knob of a switch on the accent colour, over the whole width of a gradient.
        const sf::Color text = theme.resolve(Dropdown::Entry, State::Active, colors).color;
        REQUIRE(contrast(text, accentColors.accent) >= 4.5f);
        REQUIRE(contrast(text, accentColors.accentStart) >= 4.5f);
        REQUIRE(contrast(theme.resolve(Switch::Knob, State::Active, colors).color, accentColors.accent) >= 3.f);

        // A hovered outline can be told from an idle one.
        const sf::Color idle = theme.resolve(Button::Face, State::Normal, colors).border;
        const sf::Color hovered = theme.resolve(Button::Face, State::Hovered, colors).border;
        REQUIRE(contrast(hovered, idle) >= 1.15f);
    }
}
