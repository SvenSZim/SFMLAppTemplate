#include "atpl/ui/setup.hpp"
#include "atpl/ui/widget.hpp"
#include "atpl/ui/widgets.hpp"

#include "ui/render/draw_list.hpp"
#include "ui/render/text_measurer.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <memory>

using namespace atpl;
using atpl::render::DrawList;
using atpl::render::TextRun;

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

FloatRect boundsOf(const render::VertexList& vertices) {
    float left = vertices.front().position.x;
    float top = vertices.front().position.y;
    float right = left;
    float bottom = top;
    for (const sf::Vertex& vertex : vertices) {
        left = std::min(left, vertex.position.x);
        top = std::min(top, vertex.position.y);
        right = std::max(right, vertex.position.x);
        bottom = std::max(bottom, vertex.position.y);
    }
    return { left, top, right - left, bottom - top };
}

bool hasColor(const render::VertexList& vertices, sf::Color color) {
    return std::any_of(vertices.begin(), vertices.end(), [&](const sf::Vertex& v) { return v.color == color; });
}

// A widget written the way real ones are: it only says where its parts are.
struct Meter {
    static constexpr Kind kind{ "meter" };
    static constexpr Part Track{ kind, "track", Role::Track };
    static constexpr Part Fill{ kind, "fill", Role::Accent };
    static constexpr Part Marks{ kind, "marks", Role::Line, Shown::No }; // optional
    static constexpr Part Label{ kind, "label", Role::MutedText };
    static constexpr Part Reading{ kind, "reading", Role::Text };
};

class MeterWidget final : public Widget {
public:
    float level = 0.5f;

    [[nodiscard]] SizeRequest measure(const MeasureContext&) const override { return { .min = { 0.f, 40.f } }; }

    void paint(Painter& painter, const Style& style) const override {
        const sf::Vector2f size = painter.size();
        const FloatRect track(0.f, 20.f, size.x, 20.f);
        const PartStyle trackStyle = style.part(Meter::Track);

        painter.text(FloatRect(0.f, 0.f, size.x, 20.f), "Level", style.part(Meter::Label));
        painter.text(FloatRect(0.f, 0.f, size.x, 20.f), "50 %", style.part(Meter::Reading), Align::Right);

        painter.box(track, trackStyle);
        const FloatRect inner = track.inset(trackStyle.contentInset());
        painter.box(
            FloatRect(inner.left(), inner.top(), inner.width() * level, inner.height()), style.part(Meter::Fill)
        );

        // An optional part: drawn without asking whether the theme shows it.
        for (int i = 1; i < 4; ++i) {
            const float x = size.x * static_cast<float>(i) / 4.f;
            painter.line({ x, 20.f }, { x, 40.f }, style.part(Meter::Marks));
        }
    }
};

struct Fixture {
    Theme theme = themes::colorful();
    Sizes sizes = Layout().sizesAt({ 1280.f, 720.f }); // the reference size: nothing is scaled
    DrawList list;
    FixedWidthText measurer;

    Fixture() { theme.font = std::make_shared<sf::Font>(); }

    void paint(const Widget& widget, sf::Vector2f origin, sf::Vector2f size, State state = State::Normal) {
        const Style style(theme, PanelColors{}, state, sizes);
        Painter painter(list, origin, size, &measurer);
        widget.paint(painter, style);
    }
};

} // namespace

TEST_CASE("a widget's paint function fills the draw list", "[ui][painter]") {
    Fixture f;
    const MeterWidget meter;

    f.paint(meter, { 100.f, 200.f }, { 240.f, 40.f });

    // The shapes: the track with its outline, and the fill inside it.
    const auto& shapes = f.list.shapes();
    REQUIRE_FALSE(shapes.empty());
    REQUIRE(shapes.size() % 3 == 0);
    REQUIRE(hasColor(shapes, f.theme.palette.mains[1]));          // the outline, in main2
    REQUIRE(hasColor(shapes, f.theme.palette.accents[0].accent)); // the fill, in the accent

    // The two pieces of text, with the look of their text types.
    REQUIRE(f.list.texts().size() == 2);
    const TextRun& label = f.list.texts()[0];
    const TextRun& reading = f.list.texts()[1];
    REQUIRE(label.text == "Level");
    REQUIRE(label.size == f.theme.typography.muted.size);
    REQUIRE(label.align == Align::Left);
    REQUIRE(reading.text == "50 %");
    REQUIRE(reading.size == f.theme.typography.text.size);
    REQUIRE(reading.align == Align::Right);
    REQUIRE(reading.font == f.theme.font.get());
    REQUIRE_FALSE(reading.wrapped);
}

TEST_CASE("a widget paints in its own coordinates and lands at its place in the panel", "[ui][painter]") {
    Fixture f;
    const MeterWidget meter;

    f.paint(meter, { 100.f, 200.f }, { 240.f, 40.f });

    // The track was painted at (0, 20) with the widget's width: it is at (100, 220) in the panel.
    REQUIRE(boundsOf(f.list.shapes()) == FloatRect(100.f, 220.f, 240.f, 20.f));
    REQUIRE(f.list.texts()[0].rect == FloatRect(100.f, 200.f, 240.f, 20.f));
}

TEST_CASE("a part the theme does not show adds nothing, and appears once it does", "[ui][painter]") {
    Fixture f;
    const MeterWidget meter;

    f.paint(meter, {}, { 240.f, 40.f });
    const std::size_t withoutMarks = f.list.shapes().size();

    f.list.clear();
    f.theme[Meter::Marks].shown = true;
    f.paint(meter, {}, { 240.f, 40.f });

    REQUIRE(f.list.shapes().size() == withoutMarks + 3 * 6); // three marks, one quad each
}

TEST_CASE("the same paint function gives the look of the state it is painted in", "[ui][painter]") {
    Fixture f;
    const MeterWidget meter;
    const sf::Color accent = f.theme.palette.accents[0].accent;

    // Count the vertices in the accent colour: the fill, and in use also the track's outline.
    const auto accentVertices = [&] {
        const auto& shapes = f.list.shapes();
        return std::count_if(shapes.begin(), shapes.end(), [&](const sf::Vertex& v) { return v.color == accent; });
    };

    f.paint(meter, {}, { 240.f, 40.f });
    const auto idle = accentVertices();

    f.list.clear();
    f.paint(meter, {}, { 240.f, 40.f }, State::Pressed);
    REQUIRE(accentVertices() > idle);
}

TEST_CASE("a style adds state for a single part", "[ui][painter]") {
    Fixture f;
    const Style style(f.theme, PanelColors{}, State::Hovered, f.sizes);

    REQUIRE(style.state() == State::Hovered);
    REQUIRE(style.part(Switch::Track).color != f.theme.palette.accents[0].accent);
    REQUIRE(style.part(Switch::Track, State::Active).color == f.theme.palette.accents[0].accent);

    // The widget's own state is kept: hovered and active together.
    const PartStyle both = f.theme.resolve(Switch::Track, State::Hovered | State::Active);
    REQUIRE(style.part(Switch::Track, State::Active).border == both.border);
}

TEST_CASE("a style uses the colours of its panel", "[ui][painter]") {
    Fixture f;
    const Style green(f.theme, PanelColors{ .main1 = 0, .main2 = 1, .accent = 1 }, State::Normal, f.sizes);

    REQUIRE(green.part(Slider::Fill).color == f.theme.palette.accents[1].accent);
}

TEST_CASE("a style hands out the layout's sizes and scales its parts with them", "[ui][painter]") {
    Theme theme;
    Layout layout;
    layout.metrics.scale = 2.f;
    const Sizes sizes = layout.sizesAt({ 1280.f, 720.f });
    const Style style(theme, PanelColors{}, State::Normal, sizes);

    REQUIRE(style.sizes().rowHeight == layout.metrics.rowHeight * 2.f);
    REQUIRE(style.sizes().padding == sf::Vector2f(layout.metrics.padding, layout.metrics.padding) * 2.f);
    REQUIRE(style.sizes().panelWidth == layout.metrics.panelWidth * 2.f);

    // Text sizes, outlines and radii follow the sizes' text factor.
    REQUIRE(style.part(Panel::Title).textSize == theme.resolve(Panel::Title).textSize * 2.f);
    REQUIRE(style.part(Panel::Background).radius == theme.resolve(Panel::Background).radius * 2.f);
}

TEST_CASE("the painter draws lines, curves and areas at the widget's place", "[ui][painter]") {
    Fixture f;
    Painter painter(f.list, { 10.f, 20.f }, { 100.f, 50.f });
    PartStyle style;
    style.color = sf::Color::White;
    style.thickness = 2.f;

    painter.line({ 0.f, 0.f }, { 100.f, 0.f }, style);
    REQUIRE(boundsOf(f.list.shapes()) == FloatRect(10.f, 19.f, 100.f, 2.f));

    f.list.clear();
    const std::array<sf::Vector2f, 3> curve = { { { 0.f, 10.f }, { 50.f, 0.f }, { 100.f, 10.f } } };
    painter.polyline(curve, style);
    REQUIRE(f.list.shapes().size() == 2 * 6);
    REQUIRE(boundsOf(f.list.shapes()).left() >= 9.f);

    f.list.clear();
    painter.area(curve, 50.f, style);
    REQUIRE(boundsOf(f.list.shapes()) == FloatRect(10.f, 20.f, 100.f, 50.f));
}

TEST_CASE("the painter records wrapped text as such", "[ui][painter]") {
    Fixture f;
    Painter painter(f.list, { 5.f, 5.f }, { 100.f, 60.f });
    const PartStyle style = f.theme.resolve(Paragraph::Body);

    painter.wrappedText(FloatRect(0.f, 0.f, 100.f, 60.f), "a longer text that wraps", style, Align::Center);

    REQUIRE(f.list.texts().size() == 1);
    REQUIRE(f.list.texts()[0].wrapped);
    REQUIRE(f.list.texts()[0].align == Align::Center);
    REQUIRE(f.list.texts()[0].rect == FloatRect(5.f, 5.f, 100.f, 60.f));
}

TEST_CASE("the painter measures text through the measurer it was given", "[ui][painter]") {
    Fixture f;
    PartStyle style;
    style.textSize = 20.f;

    const Painter withMeasurer(f.list, {}, { 100.f, 20.f }, &f.measurer);
    REQUIRE(withMeasurer.textSize("abcd", style) == sf::Vector2f(40.f, 20.f));
    REQUIRE(withMeasurer.size() == sf::Vector2f(100.f, 20.f));

    const Painter without(f.list, {}, { 100.f, 20.f });
    REQUIRE(without.textSize("abcd", style) == sf::Vector2f(0.f, 0.f));
}

// ----- The draw list -----

TEST_CASE("text that cannot be seen is not added", "[ui][draw_list]") {
    DrawList list;
    PartStyle style;
    style.color = sf::Color::White;
    const FloatRect room(0.f, 0.f, 100.f, 20.f);

    PartStyle hidden = style;
    hidden.shown = false;
    PartStyle transparent = style;
    transparent.color.a = 0;

    list.addText(room, "", style, Align::Left, false);
    list.addText(room, "hidden", hidden, Align::Left, false);
    list.addText(room, "transparent", transparent, Align::Left, false);
    list.addText(FloatRect(0.f, 0.f, 0.f, 20.f), "no room", style, Align::Left, false);

    REQUIRE(list.texts().empty());
    REQUIRE(list.empty());

    list.addText(room, "visible", style, Align::Left, false);
    REQUIRE(list.texts().size() == 1);
    REQUIRE_FALSE(list.empty());
}

TEST_CASE("clearing a draw list empties it and keeps its memory", "[ui][draw_list]") {
    DrawList list;
    PartStyle style;
    style.color = sf::Color::White;
    const FloatRect room(0.f, 0.f, 100.f, 20.f);

    render::appendBox(list.shapes(), room, style);
    list.addText(room, "a text long enough not to fit a small string buffer", style, Align::Left, false);
    list.addText(room, "second", style, Align::Left, false);
    const std::size_t shapeCapacity = list.shapes().capacity();
    const char* const textStorage = list.texts()[0].text.data();

    list.clear();
    REQUIRE(list.empty());
    REQUIRE(list.texts().empty());
    REQUIRE(list.shapes().capacity() == shapeCapacity);

    // Refilling reuses the first entry's string memory instead of allocating again.
    list.addText(room, "another text of about the same length as the first", style, Align::Left, false);
    REQUIRE(list.texts().size() == 1);
    REQUIRE(list.texts()[0].text == "another text of about the same length as the first");
    REQUIRE(list.texts()[0].text.data() == textStorage);
}
