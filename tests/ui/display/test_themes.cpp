#include "atpl/core/param.hpp"
#include "atpl/core/series.hpp"
#include "atpl/core/text_log.hpp"
#include "atpl/ui/error.hpp"
#include "atpl/ui/ui.hpp"
#include "atpl/ui/widgets.hpp"

#include <SFML/Graphics/Font.hpp>
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/System/Clock.hpp>

#include <catch2/catch_test_macros.hpp>

#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>

using namespace atpl;

namespace {

/// Loads fonts from the resources next to the test executable (the tests run from there), and
/// counts what it was asked for.
struct Loader {
    std::map<std::string, std::shared_ptr<const sf::Font>> loaded;
    int calls = 0;

    FontLoader function() {
        return [this](std::string_view name) {
            ++calls;
            auto& font = loaded[std::string(name)];
            if (font == nullptr) {
                auto fresh = std::make_shared<sf::Font>();
                if (!fresh->openFromFile("resources/" + std::string(name))) {
                    throw std::runtime_error("no font " + std::string(name));
                }
                font = fresh;
            }
            return font;
        };
    }
};

std::shared_ptr<const sf::Font> bundled() {
    auto font = std::make_shared<sf::Font>();
    REQUIRE(font->openFromFile("resources/fonts/default.ttf"));
    return font;
}

/// One pass of the main loop; whether a frame was drawn.
bool frame(UI& ui) {
    ui.handleInput();
    ui.update();
    return ui.draw();
}

void settle(UI& ui) {
    const sf::Clock clock;
    while (clock.getElapsedTime() < sf::milliseconds(150)) {
        frame(ui);
    }
}

} // namespace

TEST_CASE(
    "every built-in theme can be switched to on a UI of every widget, without widget changes", "[ui][theme][display]"
) {
    sf::RenderWindow window(sf::VideoMode({ 900u, 700u }), "atpl theme test");
    Param<double> number = 3.0;
    Series series(16);
    series.push(1.f);
    TextLog log(8);
    log.push("hello");
    Param<std::string> text = std::string("some text");
    Loader fonts;

    UISetup setup;
    setup.theme.font = bundled();
    setup.fonts = fonts.function();
    setup.panels = {
        { .name = "Inputs",
          .placement = Anchor::TopLeft,
          .widgets = { Button("Go"),
                       Switch("On"),
                       Slider("Speed", number, { .min = 0.0, .max = 10.0 }),
                       Dropdown("Mode", { "A", "B" }),
                       TextInput("Name") } },
        { .name = "Outputs",
          .placement = Anchor::TopRight,
          .widgets = { ValueDisplay("Value", number),
                       ProgressBar("Done", number, { .min = 0.0, .max = 10.0 }),
                       Graph("Curve", series),
                       Log("Events", log),
                       TextDisplay("Status", text),
                       Paragraph("About", { .heading = "Heading", .text = "Body text.", .footer = "Footer" }),
                       View("map", { .height = 60.f }) } },
    };
    UI ui(window, std::move(setup));
    ui.update();
    REQUIRE(ui.draw());
    settle(ui);

    for (Theme theme : { themes::colorful(), themes::moon(), themes::colorful() }) {
        theme[Slider::Ticks].shown = true; // a part entry, set by the application
        ui.setTheme(std::move(theme));
        REQUIRE(frame(ui)); // drawn anew
        settle(ui);
        REQUIRE_FALSE(frame(ui)); // and then at rest
        REQUIRE(ui.theme().font != nullptr);
    }
}

TEST_CASE("a theme's named fonts are loaded when it is set, and others get the default font", "[ui][theme][display]") {
    sf::RenderWindow window(sf::VideoMode({ 400u, 300u }), "atpl theme test");
    const std::shared_ptr<const sf::Font> inconsolata = bundled();
    Loader fonts;
    UISetup setup;
    setup.theme.font = inconsolata;
    setup.fonts = fonts.function();
    UI ui(window, std::move(setup));
    REQUIRE(fonts.calls == 0); // the default theme names none
    REQUIRE(ui.theme().font == inconsolata);

    ui.setTheme(themes::colorful());
    REQUIRE(ui.theme().font->getInfo().family == "Roboto");
    REQUIRE(ui.theme().typography.title.font != nullptr);
    REQUIRE(ui.theme().typography.title.font != ui.theme().font); // Medium for titles
    REQUIRE(fonts.calls == 3);                                    // the default, the title's and the heading's

    ui.setTheme(themes::moon()); // names none: the font the UI started with, not Roboto
    REQUIRE(ui.theme().font == inconsolata);

    Theme missing = themes::moon();
    missing.fontName = "fonts/missing.ttf";
    REQUIRE_THROWS(ui.setTheme(missing));
    REQUIRE(ui.theme().font == inconsolata); // nothing changed
}

TEST_CASE("without a font loader, named fonts are passed over", "[ui][theme][display]") {
    sf::RenderWindow window(sf::VideoMode({ 400u, 300u }), "atpl theme test");
    const std::shared_ptr<const sf::Font> inconsolata = bundled();
    UISetup setup;
    setup.theme.font = inconsolata;
    UI ui(window, std::move(setup));
    ui.setTheme(themes::colorful());
    REQUIRE(ui.theme().font == inconsolata);
}
