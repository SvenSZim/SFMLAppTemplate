// Text needs glyphs, and glyphs need a display: these tests carry the CTest label "display".

#include "ui/render/font_measurer.hpp"
#include "ui/render/text_cache.hpp"

#include <SFML/Graphics/Image.hpp>
#include <SFML/Graphics/RenderTexture.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <stdexcept>
#include <string>

using namespace atpl;
using atpl::render::DrawList;
using atpl::render::FontMeasurer;
using atpl::render::TextCache;
using Catch::Approx;

namespace {

const sf::Color clear(0, 0, 0);
const sf::Color ink(255, 255, 255);

/// The bundled font. The build copies the resources next to the test executable, and the tests
/// run from its directory.
const sf::Font& font() {
    static const sf::Font loaded = [] {
        sf::Font result;
        if (!result.openFromFile("resources/fonts/default.ttf")) {
            throw std::runtime_error("resources/fonts/default.ttf not found; run the tests through ctest");
        }
        return result;
    }();
    return loaded;
}

PartStyle textStyle(float size = 16.f, sf::Color color = ink) {
    PartStyle style;
    style.color = color;
    style.textSize = size;
    style.font = &font();
    return style;
}

struct Canvas {
    sf::RenderTexture target{ { 300u, 120u } };
    TextCache cache;
    sf::Image image;

    std::size_t draw(const DrawList& layer) {
        target.clear(clear);
        const std::size_t calls = cache.draw(target, sf::RenderStates::Default, layer);
        target.display();
        image = target.getTexture().copyToImage();
        return calls;
    }

    /// Whether anything was drawn in the given part of the picture.
    [[nodiscard]] bool hasInk(unsigned left, unsigned top, unsigned width, unsigned height) const {
        for (unsigned y = top; y < top + height; ++y) {
            for (unsigned x = left; x < left + width; ++x) {
                if (image.getPixel({ x, y }) != clear) {
                    return true;
                }
            }
        }
        return false;
    }
};

} // namespace

// ----- Measuring -----

TEST_CASE("text is as wide as its characters and one line high", "[ui][text][display]") {
    const FontMeasurer measurer;

    const sf::Vector2f one = measurer.measure("a", &font(), 16.f);
    const sf::Vector2f four = measurer.measure("abcd", &font(), 16.f);

    REQUIRE(one.x > 0.f);
    REQUIRE(four.x == Approx(4.f * one.x)); // the bundled font has one width for all characters
    REQUIRE(four.y == Approx(font().getLineSpacing(16)));
    REQUIRE(measurer.measure("", &font(), 16.f).x == 0.f);
}

TEST_CASE("larger text measures larger", "[ui][text][display]") {
    const FontMeasurer measurer;

    REQUIRE(measurer.measure("abc", &font(), 24.f).x > measurer.measure("abc", &font(), 12.f).x);
    REQUIRE(measurer.measure("abc", &font(), 24.f).y > measurer.measure("abc", &font(), 12.f).y);
}

TEST_CASE("without a font, text measures as nothing", "[ui][text][display]") {
    const FontMeasurer measurer;

    REQUIRE(measurer.measure("abc", nullptr, 16.f) == sf::Vector2f(0.f, 0.f));
    REQUIRE(measurer.wrappedHeight("abc", nullptr, 16.f, 100.f) == 0.f);
}

TEST_CASE("wrapped text is as high as its lines", "[ui][text][display]") {
    const FontMeasurer measurer;
    const float line = font().getLineSpacing(16);
    const float charWidth = measurer.measure("a", &font(), 16.f).x;

    REQUIRE(measurer.wrappedHeight("one two", &font(), 16.f, 1000.f) == Approx(line));
    REQUIRE(measurer.wrappedHeight("one two three", &font(), 16.f, charWidth * 5.5f) == Approx(3.f * line));
    REQUIRE(measurer.wrappedHeight("one\ntwo", &font(), 16.f, 1000.f) == Approx(2.f * line));
}

// ----- The cache -----

TEST_CASE("text is built once and then only drawn", "[ui][text][display]") {
    Canvas canvas;
    DrawList layer;
    layer.addText(FloatRect(10.f, 10.f, 200.f, 20.f), "Speed", textStyle(), Align::Left, false);
    layer.addText(FloatRect(10.f, 40.f, 200.f, 20.f), "6.0", textStyle(), Align::Right, false);

    REQUIRE(canvas.draw(layer) == 2); // one draw call per run
    REQUIRE(canvas.cache.buildCount() == 2);

    canvas.draw(layer);
    canvas.draw(layer);
    REQUIRE(canvas.cache.buildCount() == 2); // nothing was built again
    REQUIRE(canvas.hasInk(10, 10, 200, 20));
}

TEST_CASE("a panel repainted with the same text builds nothing", "[ui][text][display]") {
    Canvas canvas;
    DrawList layer;
    const auto paint = [&](const char* value) {
        layer.clear();
        layer.addText(FloatRect(10.f, 10.f, 200.f, 20.f), "Speed", textStyle(), Align::Left, false);
        layer.addText(FloatRect(10.f, 40.f, 200.f, 20.f), value, textStyle(), Align::Right, false);
    };

    paint("6.0");
    canvas.draw(layer);
    REQUIRE(canvas.cache.buildCount() == 2);

    paint("6.0"); // the panel was dirty for another reason
    canvas.draw(layer);
    REQUIRE(canvas.cache.buildCount() == 2);

    paint("6.1"); // the value changed: one run is built, the label is not
    canvas.draw(layer);
    REQUIRE(canvas.cache.buildCount() == 3);
}

TEST_CASE("a run is built again when its look or its room changes", "[ui][text][display]") {
    Canvas canvas;
    DrawList layer;
    const auto paint = [&](FloatRect room, const PartStyle& style, Align align) {
        layer.clear();
        layer.addText(room, "value", style, align, false);
        canvas.draw(layer);
    };
    const FloatRect room(10.f, 10.f, 200.f, 20.f);

    paint(room, textStyle(), Align::Left);
    REQUIRE(canvas.cache.buildCount() == 1);

    paint(room, textStyle(16.f, sf::Color::Red), Align::Left); // another colour
    REQUIRE(canvas.cache.buildCount() == 2);
    paint(room, textStyle(20.f, sf::Color::Red), Align::Left); // another size
    REQUIRE(canvas.cache.buildCount() == 3);
    paint(room, textStyle(20.f, sf::Color::Red), Align::Right); // another alignment
    REQUIRE(canvas.cache.buildCount() == 4);
    paint(FloatRect(10.f, 50.f, 200.f, 20.f), textStyle(20.f, sf::Color::Red), Align::Right); // another place
    REQUIRE(canvas.cache.buildCount() == 5);
    paint(FloatRect(10.f, 50.f, 200.f, 20.f), textStyle(20.f, sf::Color::Red), Align::Right); // the same again
    REQUIRE(canvas.cache.buildCount() == 5);
}

TEST_CASE("runs that disappear are dropped, and a run without a font draws nothing", "[ui][text][display]") {
    Canvas canvas;
    DrawList layer;
    layer.addText(FloatRect(10.f, 10.f, 200.f, 20.f), "one", textStyle(), Align::Left, false);
    layer.addText(FloatRect(10.f, 40.f, 200.f, 20.f), "two", textStyle(), Align::Left, false);
    REQUIRE(canvas.draw(layer) == 2);

    layer.clear();
    layer.addText(FloatRect(10.f, 10.f, 200.f, 20.f), "one", textStyle(), Align::Left, false);
    REQUIRE(canvas.draw(layer) == 1);
    REQUIRE_FALSE(canvas.hasInk(0, 40, 300, 20));

    PartStyle noFont = textStyle();
    noFont.font = nullptr;
    layer.clear();
    layer.addText(FloatRect(10.f, 10.f, 200.f, 20.f), "nothing to draw with", noFont, Align::Left, false);
    REQUIRE(canvas.draw(layer) == 0);
    REQUIRE_FALSE(canvas.hasInk(0, 0, 300, 120));
}

TEST_CASE("each layer has its own cached text", "[ui][text][display]") {
    Canvas canvas;
    DrawList first;
    DrawList second;
    first.addText(FloatRect(10.f, 10.f, 200.f, 20.f), "first", textStyle(), Align::Left, false);
    second.addText(FloatRect(10.f, 40.f, 200.f, 20.f), "second", textStyle(), Align::Left, false);

    canvas.draw(first);
    canvas.draw(second);
    canvas.draw(first);
    canvas.draw(second);
    REQUIRE(canvas.cache.buildCount() == 2);

    canvas.cache.forget(first);
    canvas.draw(first);
    REQUIRE(canvas.cache.buildCount() == 3); // forgotten, so built anew
}

// ----- Where text ends up -----

TEST_CASE("text is placed in its room as its alignment says", "[ui][text][display]") {
    Canvas canvas;
    DrawList layer;
    const FloatRect room(20.f, 20.f, 260.f, 24.f);

    layer.addText(room, "ab", textStyle(), Align::Left, false);
    canvas.draw(layer);
    REQUIRE(canvas.hasInk(20, 20, 40, 24));
    REQUIRE_FALSE(canvas.hasInk(100, 20, 180, 24));

    layer.clear();
    layer.addText(room, "ab", textStyle(), Align::Right, false);
    canvas.draw(layer);
    REQUIRE(canvas.hasInk(240, 20, 40, 24));
    REQUIRE_FALSE(canvas.hasInk(20, 20, 180, 24));

    layer.clear();
    layer.addText(room, "ab", textStyle(), Align::Center, false);
    canvas.draw(layer);
    REQUIRE(canvas.hasInk(130, 20, 40, 24));
    REQUIRE_FALSE(canvas.hasInk(20, 20, 80, 24));
    REQUIRE_FALSE(canvas.hasInk(200, 20, 80, 24));
}

TEST_CASE("text stays inside its room vertically", "[ui][text][display]") {
    Canvas canvas;
    DrawList layer;
    layer.addText(FloatRect(20.f, 40.f, 260.f, 30.f), "Apgjy", textStyle(), Align::Left, false);
    canvas.draw(layer);

    REQUIRE(canvas.hasInk(20, 40, 100, 30));
    REQUIRE_FALSE(canvas.hasInk(0, 0, 300, 40));  // nothing above the room
    REQUIRE_FALSE(canvas.hasInk(0, 70, 300, 50)); // nothing below it
}

TEST_CASE("text that is too wide is cut short and does not leave its room", "[ui][text][display]") {
    Canvas canvas;
    DrawList layer;
    layer.addText(
        FloatRect(20.f, 20.f, 100.f, 24.f),
        "a very long text that cannot possibly fit into the room it was given",
        textStyle(),
        Align::Left,
        false
    );
    canvas.draw(layer);

    REQUIRE(canvas.hasInk(20, 20, 100, 24));
    REQUIRE_FALSE(canvas.hasInk(122, 0, 178, 120)); // nothing beyond the right edge
}

TEST_CASE("wrapped text continues on further lines within its width", "[ui][text][display]") {
    Canvas canvas;
    DrawList layer;
    const float line = font().getLineSpacing(16);
    layer.addText(
        FloatRect(20.f, 10.f, 100.f, 100.f),
        "several words that need more than one line",
        textStyle(),
        Align::Left,
        true
    );
    canvas.draw(layer);

    const auto top = static_cast<unsigned>(10.f + line);
    REQUIRE(canvas.hasInk(20, 10, 100, static_cast<unsigned>(line)));  // the first line
    REQUIRE(canvas.hasInk(20, top, 100, static_cast<unsigned>(line))); // a second line below it
    REQUIRE_FALSE(canvas.hasInk(122, 0, 178, 120));                    // and nothing beyond the width
    REQUIRE(canvas.cache.buildCount() == 1);                           // all lines are one run
}
