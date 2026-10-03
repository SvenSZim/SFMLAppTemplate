#include "atpl/ui/error.hpp"
#include "atpl/ui/setup.hpp"
#include "atpl/ui/widgets.hpp"

#include "ui/binding/sync.hpp"
#include "ui/render/draw_list.hpp"
#include "ui/render/text_measurer.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <string>
#include <vector>

using namespace atpl;
using Catch::Matchers::ContainsSubstring;

namespace {

/// Text of a fixed width per character, so tests need no font.
class FixedWidthText final : public render::TextMeasurer {
public:
    [[nodiscard]] sf::Vector2f measure(std::string_view text, const sf::Font*, float size) const override {
        return { static_cast<float>(text.size()) * size * 0.5f, size };
    }
    [[nodiscard]] float wrappedHeight(std::string_view, const sf::Font*, float size, float) const override {
        return size;
    }
};

const FixedWidthText measurer;
const Sizes sizes = Layout().sizesAt({ 1280.f, 720.f }); // nothing scaled
const Theme theme;

/// The width of `count` characters of the text.
float chars(int count) {
    return static_cast<float>(count) * theme.resolve(TextDisplay::Text).textSize * 0.5f;
}

/// What a text display with this text shows, line by line, `count` characters wide.
std::vector<render::TextRun> shown(const std::string& text, TextDisplayOptions options, int count = 20) {
    const auto widget = TextDisplay("Status", options).create();
    widget->setValue(Value(text));
    render::DrawList list;
    Painter painter(list, { 0.f, 0.f }, { chars(count), 200.f }, &measurer);
    widget->paint(painter, Style(theme, {}, State::Normal, sizes));
    const auto runs = list.texts();
    return { runs.begin() + 1, runs.end() }; // without the label
}

std::vector<std::string> textsOf(const std::vector<render::TextRun>& runs) {
    std::vector<std::string> texts;
    for (const render::TextRun& run : runs) {
        texts.push_back(run.text);
    }
    return texts;
}

} // namespace

TEST_CASE("a text display wraps its text within its lines", "[ui][widgets][text_display]") {
    // 20 characters to a line.
    const std::string text = "Running, 12.3 s simulated at speed 1.0 with gravity";
    REQUIRE(
        textsOf(shown(text, { .lines = 3 })) ==
        std::vector<std::string>{ "Running, 12.3 s", "simulated at speed", "1.0 with gravity" }
    );
    REQUIRE(textsOf(shown("short", { .lines = 3 })) == std::vector<std::string>{ "short" });
}

TEST_CASE("what does not fit goes into the last line, which is cut there", "[ui][widgets][text_display]") {
    const std::string text = "Running, 12.3 s simulated at speed 1.0 with gravity";
    const std::vector<render::TextRun> two = shown(text, { .lines = 2 });
    REQUIRE(textsOf(two) == std::vector<std::string>{ "Running, 12.3 s", "simulated at speed 1.0 with gravity" });
    REQUIRE(two[1].rect.width() == chars(20)); // the renderer ends it with an ellipsis
    REQUIRE_FALSE(two[1].wrapped);

    // One line: the whole text in it, line breaks as spaces.
    REQUIRE(textsOf(shown("first\nsecond", { .lines = 1 })) == std::vector<std::string>{ "first second" });
}

TEST_CASE(
    "a line break in the text starts a new line, and a word wider than a line is broken", "[ui][widgets][text_display]"
) {
    REQUIRE(textsOf(shown("one\ntwo", { .lines = 3 })) == std::vector<std::string>{ "one", "two" });
    REQUIRE(
        textsOf(shown(std::string(30, 'x'), { .lines = 2 })) ==
        std::vector<std::string>{ std::string(20, 'x'), std::string(10, 'x') }
    );
}

TEST_CASE("a text display is as high as its lines say, whatever its text", "[ui][widgets][text_display]") {
    const auto measureWith = [](const std::string& text, std::size_t lines) {
        const auto widget = TextDisplay("Status", { .lines = lines }).create();
        widget->setValue(Value(text));
        return widget->measure(MeasureContext(280.f, theme, {}, sizes, &measurer));
    };
    const SizeRequest one = measureWith("", 1);
    REQUIRE(one.min.y == measureWith(std::string(500, 'x'), 1).min.y);
    REQUIRE(one.max.has_value());
    REQUIRE(one.max->y == one.min.y);
    REQUIRE(measureWith("", 3).min.y > one.min.y);
}

TEST_CASE("a text display places its lines as asked", "[ui][widgets][text_display]") {
    for (const render::TextRun& run : shown("a b c", { .lines = 2, .align = Align::Center })) {
        REQUIRE(run.align == Align::Center);
    }
}

TEST_CASE("a text display shows bound text, or its initial text, and only shows", "[ui][widgets][text_display]") {
    Param<std::string> status = std::string("Paused at 3.0 s");
    UISetup setup;
    setup.panels = { { .name = "Stats",
                       .widgets = { TextDisplay("Status", status), TextDisplay("Note", { .initial = "idle" }) } } };
    model::Store store{ setup };
    binding::attachAll(store);
    binding::sync(store, binding::Clock::now());
    REQUIRE(store.widget(store.names().widget("Status")).widget->value() == Value(std::string("Paused at 3.0 s")));
    REQUIRE(store.widget(store.names().widget("Note")).widget->value() == Value(std::string("idle")));

    const auto widget = TextDisplay("T").create();
    REQUIRE(widget->accepts(ValueKind::Text));
    REQUIRE_FALSE(widget->accepts(ValueKind::Number)); // numbers belong in a value display
    REQUIRE_FALSE(widget->editsValue());
    REQUIRE_FALSE(widget->reactsToPointer());
    REQUIRE(widget->refreshInterval() == std::chrono::milliseconds(125));
    REQUIRE_THROWS_WITH(TextDisplay("T", { .lines = 0 }).create(), ContainsSubstring("text display \"T\": it needs"));
}
