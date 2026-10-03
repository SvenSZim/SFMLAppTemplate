#include "atpl/ui/error.hpp"
#include "atpl/ui/setup.hpp"
#include "atpl/ui/widgets.hpp"

#include "ui/binding/sync.hpp"
#include "ui/input/input_system.hpp"
#include "ui/layout/arrange.hpp"
#include "ui/render/draw_list.hpp"
#include "ui/render/text_measurer.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <algorithm>
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

/// A UI's insides, without a window: one floating panel with the given widgets, laid out, with
/// the input system and the bindings in place.
struct Harness {
    model::Store store;
    Theme theme;
    std::vector<PanelId> stacking;
    input::InputSystem input;
    std::vector<Event> events;

    explicit Harness(std::vector<WidgetSetup> widgets) :
        store(setupOf(std::move(widgets))) {
        layout::prepare(store, {});
        binding::attachAll(store);
        binding::sync(store, binding::Clock::now());
        layout::arrange(store, { 1280.f, 720.f }, {}, theme, sizes, &measurer);
        stacking = store.stackingOrder();
    }

    static UISetup setupOf(std::vector<WidgetSetup> widgets) {
        UISetup setup;
        setup.panels = {
            { .name = "Panel", .placement = Anchor::TopLeft, .width = 300.f, .widgets = std::move(widgets) }
        };
        return setup;
    }

    /// A widget's rectangle in the window.
    [[nodiscard]] FloatRect rectOf(std::string_view name) {
        const model::WidgetSlot& slot = store.widget(store.names().widget(name));
        const model::Panel& panel = store.panel(slot.panel);
        return { panel.rect.position() + sf::Vector2f(0.f, sizes.headerHeight) + slot.rect.position(),
                 slot.rect.size() };
    }

    std::vector<Event> send(const sf::Event& event) {
        events.clear();
        input.handle(event, store, stacking, sizes, events);
        binding::sync(store, binding::Clock::now());
        return events;
    }
    std::vector<Event> press(sf::Vector2f at) {
        return send(sf::Event::MouseButtonPressed{ sf::Mouse::Button::Left, sf::Vector2i(at) });
    }
    std::vector<Event> moveTo(sf::Vector2f at) { return send(sf::Event::MouseMoved{ sf::Vector2i(at) }); }
    std::vector<Event> release(sf::Vector2f at) {
        return send(sf::Event::MouseButtonReleased{ sf::Mouse::Button::Left, sf::Vector2i(at) });
    }
    std::vector<Event> click(sf::Vector2f at) {
        std::vector<Event> all = press(at);
        for (Event& event : release(at)) {
            all.push_back(std::move(event));
        }
        return all;
    }

    [[nodiscard]] Widget& widget(std::string_view name) { return *store.widget(store.names().widget(name)).widget; }

    /// Paints a widget as the UI would, and returns what it drew.
    [[nodiscard]] render::DrawList paint(std::string_view name) {
        const model::WidgetSlot& slot = store.widget(store.names().widget(name));
        render::DrawList list;
        Painter painter(list, { 0.f, 0.f }, slot.rect.size(), &measurer);
        slot.widget->paint(painter, Style(theme, slot.colors, model::stateOf(slot), sizes));
        return list;
    }
};

bool hasColor(const render::VertexList& vertices, sf::Color color) {
    return std::any_of(vertices.begin(), vertices.end(), [&](const sf::Vertex& v) { return v.color == color; });
}

/// The right end of what is drawn in this colour.
float rightmost(const render::VertexList& vertices, sf::Color color) {
    float right = -1.f;
    for (const sf::Vertex& vertex : vertices) {
        if (vertex.color == color) {
            right = std::max(right, vertex.position.x);
        }
    }
    return right;
}

bool shows(const render::DrawList& list, std::string_view text) {
    const auto runs = list.texts();
    return std::any_of(runs.begin(), runs.end(), [&](const render::TextRun& run) { return run.text == text; });
}

} // namespace

// ----- Button -----

TEST_CASE("a button is pressed by a click on it, and raises ButtonPressed", "[ui][widgets][button]") {
    Harness ui({ Button("Reset") });
    const FloatRect button = ui.rectOf("Reset");

    const auto events = ui.click(button.center());
    REQUIRE(events.size() == 1);
    REQUIRE(events[0].isButton("Reset"));
}

TEST_CASE("a press dragged off the button and released elsewhere does nothing", "[ui][widgets][button]") {
    Harness ui({ Button("Reset") });
    const FloatRect button = ui.rectOf("Reset");

    ui.press(button.center());
    ui.moveTo({ 700.f, 500.f });
    REQUIRE(ui.release({ 700.f, 500.f }).empty());
}

TEST_CASE("a button bound to a parameter sets it on every press", "[ui][widgets][button]") {
    Param<bool> pressed;
    Harness ui({ Button("Burst", pressed) });

    ui.click(ui.rectOf("Burst").center());
    REQUIRE(pressed.get());
}

TEST_CASE("a button shows its label, or its name, on its face", "[ui][widgets][button]") {
    Harness ui({ Button("Reset"), Button("Save", { .label = "Save as..." }) });
    REQUIRE(shows(ui.paint("Reset"), "Reset"));
    REQUIRE(shows(ui.paint("Save"), "Save as..."));
    REQUIRE(ui.paint("Reset").texts().front().align == Align::Center);
}

TEST_CASE("a button is as wide as its column, and no higher than a little over a row", "[ui][widgets][button]") {
    Harness ui({ Button("Reset") });
    const MeasureContext context(280.f, ui.theme, {}, sizes, &measurer);
    const SizeRequest request = ui.widget("Reset").measure(context);
    REQUIRE(request.max.has_value());
    REQUIRE(request.max->y <= sizes.rowHeight * 1.25f);
    REQUIRE(request.preferred.y == sizes.rowHeight);
    REQUIRE(ui.rectOf("Reset").width() == 300.f - 2.f * sizes.padding.x);
}

// ----- Switch -----

TEST_CASE("a click flips a switch, which writes its parameter and raises ValueChanged", "[ui][widgets][switch]") {
    Param<bool> gravity = false;
    Harness ui({ Switch("Gravity", gravity) });

    auto events = ui.click(ui.rectOf("Gravity").center());
    REQUIRE(gravity.get());
    REQUIRE(events.size() == 1);
    REQUIRE(events[0].changeOf("Gravity")->as<bool>());
    REQUIRE(events[0].changeOf("Gravity")->final);

    ui.click(ui.rectOf("Gravity").center());
    REQUIRE_FALSE(gravity.get());
}

TEST_CASE("a switch follows its parameter", "[ui][widgets][switch]") {
    Param<bool> trails = false;
    Harness ui({ Switch("Trails", trails) });
    REQUIRE(ui.widget("Trails").value() == Value(false));

    trails = true;
    binding::sync(ui.store, binding::Clock::now());
    REQUIRE(ui.widget("Trails").value() == Value(true));
}

TEST_CASE("an unbound switch starts as its options say", "[ui][widgets][switch]") {
    Harness ui({ Switch("On", { .initial = true }), Switch("Off") });
    REQUIRE(ui.widget("On").value() == Value(true));
    REQUIRE(ui.widget("Off").value() == Value(false));
}

TEST_CASE("a switch that is on shows its track in the accent look", "[ui][widgets][switch]") {
    Harness ui({ Switch("Gravity", { .initial = false }) });
    const sf::Color active = ui.theme.resolve(Switch::Track, State::Active).color;
    REQUIRE_FALSE(hasColor(ui.paint("Gravity").shapes(), active));

    ui.widget("Gravity").setValue(Value(true));
    REQUIRE(hasColor(ui.paint("Gravity").shapes(), active));
    REQUIRE(shows(ui.paint("Gravity"), "Gravity"));
}

// ----- Slider -----

TEST_CASE("a press on a slider's track moves it there, and dragging follows", "[ui][widgets][slider]") {
    Param<float> speed = 0.f;
    Harness ui({ Slider("Speed", speed, { .min = 0.0, .max = 10.0 }) });
    const FloatRect slider = ui.rectOf("Speed");
    const float knob = sizes.rowHeight * 0.8f;
    const float y = slider.bottom() - knob * 0.5f;
    const float start = slider.left() + knob * 0.5f;
    const float travel = slider.width() - knob;

    auto events = ui.press({ start + travel * 0.5f, y });
    REQUIRE(speed.get() == Catch::Approx(5.f).margin(0.05f));
    REQUIRE(events.size() == 1);
    REQUIRE_FALSE(events[0].changeOf("Speed")->final); // the drag goes on

    events = ui.moveTo({ start + travel * 0.75f, y });
    REQUIRE(speed.get() == Catch::Approx(7.5f).margin(0.05f));
    REQUIRE_FALSE(events[0].changeOf("Speed")->final);

    events = ui.moveTo({ slider.right() + 100.f, y + 200.f }); // far beyond: the most
    REQUIRE(speed.get() == 10.f);

    events = ui.release({ slider.right() + 100.f, y + 200.f });
    REQUIRE(events.size() == 1);
    REQUIRE(events[0].changeOf("Speed")->final); // the end of it
}

TEST_CASE("a slider with a step only takes values on its steps", "[ui][widgets][slider]") {
    Param<int> size = 1;
    Harness ui({ Slider("Size", size, { .min = 1.0, .max = 32.0, .step = 1.0 }) });
    const FloatRect slider = ui.rectOf("Size");
    const float knob = sizes.rowHeight * 0.8f;
    ui.press({ slider.left() + knob * 0.5f + (slider.width() - knob) * 0.33f, slider.bottom() - knob * 0.5f });
    REQUIRE(size.get() == 11); // 1 + 31 * 0.33 = 11.2, on the nearest step

    ui.widget("Size").setValue(Value(7.4));
    REQUIRE(ui.widget("Size").value() == Value(7.0));
}

TEST_CASE("a press on a slider's label moves nothing", "[ui][widgets][slider]") {
    Param<float> speed = 0.5f;
    Harness ui({ Slider("Speed", speed) });
    const FloatRect slider = ui.rectOf("Speed");
    REQUIRE(ui.press({ slider.left() + slider.width() * 0.9f, slider.top() + 2.f }).empty());
    REQUIRE(speed.get() == 0.5f);
}

TEST_CASE("the wheel moves a slider by a step, or by a hundredth of its range", "[ui][widgets][slider]") {
    Param<float> speed = 5.f;
    Param<int> size = 4;
    Harness ui(
        { Slider("Speed", speed, { .min = 0.0, .max = 10.0 }),
          Slider("Size", size, { .min = 1.0, .max = 32.0, .step = 2.0 }) }
    );

    const auto events = ui.send(
        sf::Event::MouseWheelScrolled{ sf::Mouse::Wheel::Vertical, 2.f, sf::Vector2i(ui.rectOf("Speed").center()) }
    );
    REQUIRE(speed.get() == Catch::Approx(5.2f));
    REQUIRE(events[0].changeOf("Speed")->final);

    ui.send(
        sf::Event::MouseWheelScrolled{ sf::Mouse::Wheel::Vertical, -1.f, sf::Vector2i(ui.rectOf("Size").center()) }
    );
    REQUIRE(size.get() == 3); // 4 is not a step from 1; 5 - 2 is
}

TEST_CASE("a slider keeps its value within its range", "[ui][widgets][slider]") {
    Harness ui({ Slider("Speed", { .min = -1.0, .max = 1.0, .initial = 5.0 }) });
    REQUIRE(ui.widget("Speed").value() == Value(1.0));
    ui.widget("Speed").setValue(Value(-7.0));
    REQUIRE(ui.widget("Speed").value() == Value(-1.0));
}

TEST_CASE("a slider shows its label, its value in its format, and a fill up to the knob", "[ui][widgets][slider]") {
    Harness ui({ Slider("Speed", { .min = 0.0, .max = 10.0, .initial = 2.5, .format = "{:.1f} m/s" }) });
    const render::DrawList low = ui.paint("Speed");
    REQUIRE(shows(low, "Speed"));
    REQUIRE(shows(low, "2.5 m/s"));

    const sf::Color fill = ui.theme.resolve(Slider::Fill).color;
    const float lowEnd = rightmost(low.shapes(), fill);
    ui.widget("Speed").setValue(Value(7.5));
    const float highEnd = rightmost(ui.paint("Speed").shapes(), fill);
    REQUIRE(highEnd > lowEnd + 50.f);
}

TEST_CASE("a slider's ticks are hidden unless the theme shows them", "[ui][widgets][slider]") {
    Harness ui({ Slider("Size", { .min = 0.0, .max = 4.0, .step = 1.0 }) });
    const sf::Color tick(1, 2, 3);
    ui.theme[Slider::Ticks].color = tick;
    REQUIRE_FALSE(hasColor(ui.paint("Size").shapes(), tick));

    ui.theme[Slider::Ticks].shown = true; // one theme entry
    const render::DrawList shown = ui.paint("Size");
    REQUIRE(hasColor(shown.shapes(), tick));
    // One tick per step: five lines of two triangles each.
    const auto ticks = std::count_if(shown.shapes().begin(), shown.shapes().end(), [&](const sf::Vertex& v) {
        return v.color == tick;
    });
    REQUIRE(ticks >= 5 * 6);
}

TEST_CASE("a slider is two lines high: its texts, and its track below them", "[ui][widgets][slider]") {
    Harness ui({ Slider("Speed"), Button("Reset") });
    const MeasureContext context(280.f, ui.theme, {}, sizes, &measurer);
    const SizeRequest slider = ui.widget("Speed").measure(context);
    const SizeRequest button = ui.widget("Reset").measure(context);
    REQUIRE(slider.preferred.y > button.preferred.y);
    REQUIRE(slider.min.y < slider.preferred.y);
    REQUIRE(slider.max.has_value());
}

TEST_CASE("slider options that cannot work are refused when the UI is built", "[ui][widgets][slider]") {
    REQUIRE_THROWS_AS(Slider("Speed", { .min = 1.0, .max = 1.0 }).create(), SetupError);
    REQUIRE_THROWS_WITH(
        Slider("Speed", { .min = 2.0, .max = 1.0 }).create(),
        ContainsSubstring("slider \"Speed\": max (1) must be above min (2)")
    );
    REQUIRE_THROWS_WITH(Slider("Speed", { .step = -1.0 }).create(), ContainsSubstring("step must not be negative"));
    REQUIRE_THROWS_WITH(Slider("Speed", { .format = "{:s}" }).create(), ContainsSubstring("cannot show a number"));
    REQUIRE_NOTHROW(Slider("Speed", { .format = "{:.2f} %" }).create());
}

// ----- All three -----

TEST_CASE("the widgets refuse values of other kinds", "[ui][widgets]") {
    Param<std::string> text;
    Param<bool> flag;
    Harness ui({ Button("B"), Switch("S"), Slider("L") });

    REQUIRE_THROWS_AS(binding::attach(ui.store, ui.store.names().widget("S"), AnyBinding(text)), SetupError);
    REQUIRE_THROWS_AS(binding::attach(ui.store, ui.store.names().widget("L"), AnyBinding(flag)), SetupError);
    REQUIRE_NOTHROW(binding::attach(ui.store, ui.store.names().widget("B"), AnyBinding(flag)));
}

TEST_CASE("a disabled widget is painted faded and reacts to nothing", "[ui][widgets]") {
    Param<bool> gravity;
    Harness ui({ Switch("Gravity", gravity) });
    ui.store.widget(ui.store.names().widget("Gravity")).enabled = false;

    REQUIRE(ui.click(ui.rectOf("Gravity").center()).empty());
    REQUIRE_FALSE(gravity.get());
    const sf::Color faded = ui.theme.resolve(Switch::Track, State::Disabled).color;
    REQUIRE(hasColor(ui.paint("Gravity").shapes(), faded));
}
