#include "atpl/ui/setup.hpp"
#include "atpl/ui/theme.hpp"
#include "atpl/ui/widget.hpp"
#include "atpl/ui/widgets.hpp"

#include "ui/binding/sync.hpp"
#include "ui/layout/arrange.hpp"
#include "ui/model/store.hpp"
#include "ui/render/draw_list.hpp"
#include "ui/render/text_measurer.hpp"
#include "ui/widgets/animation.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

using namespace atpl;
using Catch::Approx;

namespace {

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
const Sizes sizes = Layout().sizesAt({ 1280.f, 720.f });

/// A UI's insides without a window or input: one panel with the given widgets, laid out, with
/// time passed by hand.
struct Harness {
    model::Store store;
    Theme theme;

    explicit Harness(std::vector<WidgetSetup> widgets) :
        store(setupOf(std::move(widgets))) {
        layout::prepare(store, {});
        binding::attachAll(store);
        binding::sync(store, binding::Clock::now());
        layout::arrange(store, { 1280.f, 720.f }, {}, theme, sizes, &measurer);
    }

    static UISetup setupOf(std::vector<WidgetSetup> widgets) {
        UISetup setup;
        setup.panels = {
            { .name = "Panel", .placement = Anchor::TopLeft, .width = 300.f, .widgets = std::move(widgets) }
        };
        return setup;
    }

    [[nodiscard]] model::WidgetSlot& slot(std::string_view name) { return store.widget(store.names().widget(name)); }
    [[nodiscard]] model::Panel& panel() { return store.panels()[0]; }

    /// Lets `seconds` pass; returns whether anything moved.
    bool pass(float seconds) { return widgets::animateWidgets(store, seconds, theme, sizes); }

    /// Lets time pass, a frame at a time, until nothing moves; returns how many frames moved.
    int settle(float frame = 1.f / 60.f) {
        int frames = 0;
        while (pass(frame)) {
            ++frames;
            REQUIRE(frames < 600); // ten seconds: something never settles
        }
        return frames;
    }

    [[nodiscard]] render::DrawList paint(std::string_view name) {
        const model::WidgetSlot& widget = slot(name);
        render::DrawList list;
        Painter painter(list, { 0.f, 0.f }, widget.rect.size(), &measurer);
        widget.widget->paint(painter, Style(theme, widget.colors, model::stateOf(widget), sizes, widget.blend));
        return list;
    }
};

bool hasColor(const render::VertexList& vertices, sf::Color color) {
    return std::any_of(vertices.begin(), vertices.end(), [&](const sf::Vertex& v) { return v.color == color; });
}

/// The left end of what is drawn in this colour.
float leftmost(const render::VertexList& vertices, sf::Color color) {
    float left = 1.e9f;
    for (const sf::Vertex& vertex : vertices) {
        if (vertex.color == color) {
            left = std::min(left, vertex.position.x);
        }
    }
    return left;
}

} // namespace

// ----- Easing towards a target -----

TEST_CASE("approaching a target is fast at first and settles exactly", "[ui][animation]") {
    float value = 0.f;
    REQUIRE(widgets::approach(value, 1.f, 0.05f, 0.1f));
    const float first = value;
    REQUIRE(first > 0.5f); // more than half of the way in half the time: it eases out
    REQUIRE(widgets::approach(value, 1.f, 0.05f, 0.1f));
    REQUIRE(value == Approx(0.95f).margin(0.001f)); // 95 % after the duration
    int steps = 0;
    while (widgets::approach(value, 1.f, 0.01f, 0.1f)) {
        ++steps;
        REQUIRE(value <= 1.f);
    }
    REQUIRE(value == 1.f); // exactly, in the end
    REQUIRE(steps < 20);
    REQUIRE_FALSE(widgets::approach(value, 1.f, 0.01f, 0.1f)); // at rest: nothing to do
}

TEST_CASE("approaching turns back from where it is when the target changes", "[ui][animation]") {
    float value = 0.f;
    widgets::approach(value, 1.f, 0.02f, 0.1f);
    const float halfway = value;
    widgets::approach(value, 0.f, 0.001f, 0.1f); // a tiny step back: no jump
    REQUIRE(value < halfway);
    REQUIRE(value > halfway * 0.9f);
}

TEST_CASE("without a duration, a target is reached at once", "[ui][animation]") {
    float value = 0.f;
    REQUIRE(widgets::approach(value, 1.f, 0.f, 0.f));
    REQUIRE(value == 1.f);
}

// ----- Mixing styles -----

TEST_CASE("a mixed style lies between its ends", "[ui][animation]") {
    PartStyle from;
    from.color = sf::Color(0, 0, 0);
    from.border = sf::Color(100, 100, 100);
    from.borderThickness = 1.f;
    from.textSize = 10.f;
    PartStyle to = from;
    to.color = sf::Color(200, 100, 50);
    to.borderThickness = 3.f;
    to.textSize = 20.f;

    REQUIRE(mix(from, to, 0.f).color == from.color);
    REQUIRE(mix(from, to, 1.f).color == to.color);
    const PartStyle half = mix(from, to, 0.5f);
    REQUIRE(half.color == sf::Color(100, 50, 25));
    REQUIRE(half.borderThickness == Approx(2.f));
    REQUIRE(mix(from, to, 0.4f).textSize == 10.f); // text keeps the nearer end's size
    REQUIRE(mix(from, to, 0.6f).textSize == 20.f);
}

TEST_CASE("a part shown at one end only fades in", "[ui][animation]") {
    PartStyle hidden;
    hidden.shown = false;
    PartStyle shown;
    shown.color = sf::Color(255, 255, 255, 200);
    const PartStyle half = mix(hidden, shown, 0.5f);
    REQUIRE(half.shown);
    REQUIRE(half.color == sf::Color(255, 255, 255, 100));
    REQUIRE_FALSE(mix(hidden, shown, 0.f).shown);
}

TEST_CASE("a style between states lies between their looks", "[ui][animation]") {
    const Theme theme;
    const PartStyle normal = theme.resolve(Button::Face, State::Normal, {}, sizes.text);
    const PartStyle hovered = theme.resolve(Button::Face, State::Hovered, {}, sizes.text);
    REQUIRE(normal.border != hovered.border); // hover turns the outline towards the accent

    const Style atRest(theme, {}, State::Hovered, sizes);
    REQUIRE(atRest.part(Button::Face).border == hovered.border);

    const Style fading(theme, {}, State::Hovered, sizes, { .hovered = 0.5f });
    REQUIRE(fading.part(Button::Face).border == mix(normal, hovered, 0.5f).border);

    // Fading out: no longer hovered, but not all the way back yet.
    const Style leaving(theme, {}, State::Normal, sizes, { .hovered = 0.25f });
    REQUIRE(leaving.part(Button::Face).border == mix(normal, hovered, 0.25f).border);
}

TEST_CASE("a widget's own transition mixes a part with and without a state", "[ui][animation]") {
    const Theme theme;
    const Style style(theme, {}, State::Normal, sizes);
    const PartStyle off = style.part(Switch::Track);
    const PartStyle on = style.part(Switch::Track, State::Active);
    REQUIRE(style.part(Switch::Track, State::Active, 0.f).color == off.color);
    REQUIRE(style.part(Switch::Track, State::Active, 1.f).color == on.color);
    REQUIRE(style.part(Switch::Track, State::Active, 0.5f).color == mix(off, on, 0.5f).color);
}

// ----- Widgets over time -----

TEST_CASE("hover fades in over the theme's time, and then nothing moves", "[ui][animation]") {
    Harness ui({ Button("Go") });
    REQUIRE_FALSE(ui.pass(1.f / 60.f)); // at rest: no frames asked for
    ui.panel().dirty = false;

    ui.slot("Go").hovered = true;
    REQUIRE(ui.pass(1.f / 60.f));
    REQUIRE(ui.panel().dirty); // only its panel is painted again
    REQUIRE(ui.slot("Go").blend.hovered > 0.f);
    REQUIRE(ui.slot("Go").blend.hovered < 1.f);

    const int frames = ui.settle();
    REQUIRE(frames > 3);  // it took a while ...
    REQUIRE(frames < 30); // ... but not long: the theme says 0.12 s to be nearly there
    REQUIRE(ui.slot("Go").blend.hovered == 1.f);
    ui.panel().dirty = false;
    REQUIRE_FALSE(ui.pass(1.f / 60.f));
    REQUIRE_FALSE(ui.panel().dirty); // nothing is redrawn once it has settled
}

TEST_CASE("hover fades out again when the pointer leaves halfway", "[ui][animation]") {
    Harness ui({ Button("Go") });
    ui.slot("Go").hovered = true;
    ui.pass(0.03f);
    const float halfway = ui.slot("Go").blend.hovered;
    ui.slot("Go").hovered = false;
    ui.pass(0.005f);
    REQUIRE(ui.slot("Go").blend.hovered < halfway); // back from where it was, no jump
    ui.settle();
    REQUIRE(ui.slot("Go").blend.hovered == 0.f);
}

TEST_CASE("a press shows faster than a hover", "[ui][animation]") {
    Harness ui({ Button("Go") });
    ui.slot("Go").hovered = true;
    ui.slot("Go").pressed = true;
    ui.pass(0.03f);
    REQUIRE(ui.slot("Go").blend.pressed > ui.slot("Go").blend.hovered);
}

TEST_CASE("without motion in the theme, states change at once", "[ui][animation]") {
    Harness ui({ Button("Go") });
    ui.theme.motion = { .hover = 0.f, .press = 0.f, .focus = 0.f, .toggle = 0.f };
    ui.slot("Go").hovered = true;
    REQUIRE(ui.pass(0.001f));
    REQUIRE(ui.slot("Go").blend.hovered == 1.f);
    REQUIRE_FALSE(ui.pass(0.001f));
}

TEST_CASE("a widget nobody sees is in its state at once", "[ui][animation]") {
    Harness ui({ Button("Go") });
    ui.slot("Go").visible = false;
    ui.slot("Go").hovered = true;
    REQUIRE_FALSE(ui.pass(0.001f)); // nothing to redraw
    REQUIRE(ui.slot("Go").blend.hovered == 1.f);
}

TEST_CASE("a switch's knob slides over and its track fades to the accent look", "[ui][animation]") {
    Harness ui({ Switch("Heat") });
    ui.settle();
    const sf::Color on = ui.theme.resolve(Switch::Track, State::Active, {}, sizes.text).color;
    const sf::Color knob = ui.theme.resolve(Switch::Knob, State::Normal, {}, sizes.text).color;
    const float offAt = leftmost(ui.paint("Heat").shapes(), knob);
    REQUIRE_FALSE(hasColor(ui.paint("Heat").shapes(), on));

    ui.slot("Heat").widget->setValue(Value(true));
    REQUIRE(ui.pass(ui.theme.motion.toggle * 0.5f));        // halfway
    REQUIRE_FALSE(hasColor(ui.paint("Heat").shapes(), on)); // between the looks

    const int frames = ui.settle();
    REQUIRE(frames > 0);
    REQUIRE(hasColor(ui.paint("Heat").shapes(), on));
    const sf::Color onKnob = ui.theme.resolve(Switch::Knob, State::Active, {}, sizes.text).color;
    REQUIRE(leftmost(ui.paint("Heat").shapes(), onKnob) > offAt); // the knob is at the right
}

TEST_CASE("a switch is where its value is from the start, without sliding", "[ui][animation]") {
    Param<bool> heat = true;
    Harness ui({ Switch("Heat", heat) }); // its initial option says off, its parameter on
    ui.pass(1.f / 60.f);                  // the first update: at once
    const sf::Color on = ui.theme.resolve(Switch::Track, State::Active, {}, sizes.text).color;
    REQUIRE(hasColor(ui.paint("Heat").shapes(), on));
    REQUIRE_FALSE(ui.pass(1.f / 60.f)); // and at rest
}

TEST_CASE("a dropdown's arrow turns while its list opens, then rests", "[ui][animation]") {
    Harness ui({ Dropdown("Draw", { "Filled", "Outlined" }) });
    ui.settle();
    ui.slot("Draw").overlayOpen = true;
    REQUIRE(ui.pass(1.f / 60.f));
    REQUIRE(ui.settle() > 0);
    REQUIRE_FALSE(ui.pass(1.f / 60.f));
}

namespace {

/// A widget of the application's that animates by itself until it is done.
class Pulse final : public Widget {
public:
    int steps = 3;

    [[nodiscard]] SizeRequest measure(const MeasureContext&) const override {
        return { .min = sf::Vector2f(10.f, 10.f),
                 .preferred = sf::Vector2f(10.f, 10.f),
                 .max = sf::Vector2f(10.f, 10.f) };
    }
    void update(float, UpdateContext& context) override {
        if (steps > 0) {
            --steps;
            context.markDirty();
        }
    }
    void paint(Painter&, const Style&) const override {}
};

struct PulseSetup {
    std::string name = "Pulse";
    [[nodiscard]] std::unique_ptr<Widget> create() const { return std::make_unique<Pulse>(); }
};

} // namespace

TEST_CASE("a widget's own animation keeps frames coming only while it runs", "[ui][animation]") {
    Harness ui({ PulseSetup{} });
    REQUIRE(ui.pass(0.01f));
    REQUIRE(ui.pass(0.01f));
    REQUIRE(ui.pass(0.01f));
    REQUIRE_FALSE(ui.pass(0.01f));
}
