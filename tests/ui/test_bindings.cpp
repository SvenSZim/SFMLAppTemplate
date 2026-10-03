#include "atpl/ui/binding.hpp"
#include "atpl/ui/error.hpp"
#include "atpl/ui/setup.hpp"
#include "atpl/ui/value.hpp"

#include "ui/binding/sync.hpp"
#include "ui/input/input_system.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <array>
#include <chrono>
#include <memory>
#include <string>
#include <vector>

using namespace atpl;
using atpl::binding::Clock;
using Catch::Matchers::ContainsSubstring;

namespace {

enum class Form { Circle, Square, Triangle };

// ----- Sources -----

/// An application's own implementation of a binding interface.
class Volume final : public NumberBinding {
public:
    [[nodiscard]] double get() const override { return level; }
    void set(const double& value) override {
        level = value;
        ++changes;
    }
    [[nodiscard]] Revision revision() const override { return changes; }

    double level = 0.5;
    Revision changes = 0;
};

// ----- A widget that writes down what it is handed -----

struct Received {
    std::vector<Value> values;
    const SeriesBinding* series = nullptr;
};

class ValueWidget final : public Widget {
public:
    ValueWidget(ValueKind kind, bool edits, std::shared_ptr<Received> received, std::optional<int> refresh) :
        m_kind(kind),
        m_edits(edits),
        m_received(std::move(received)),
        m_refresh(refresh) {}

    [[nodiscard]] std::chrono::milliseconds refreshInterval() const override {
        return m_refresh ? std::chrono::milliseconds(*m_refresh) : Widget::refreshInterval();
    }

    [[nodiscard]] SizeRequest measure(const MeasureContext&) const override { return {}; }
    void paint(Painter&, const Style&) const override {}

    [[nodiscard]] bool accepts(ValueKind kind) const override { return kind == m_kind; }
    [[nodiscard]] bool editsValue() const override { return m_edits; }
    [[nodiscard]] std::optional<Value> value() const override { return m_value; }
    void setValue(const Value& value) override {
        m_value = value;
        m_received->values.push_back(value);
    }
    void setSeries(const SeriesBinding* source) override { m_received->series = source; }

private:
    ValueKind m_kind;
    bool m_edits;
    std::shared_ptr<Received> m_received;
    std::optional<int> m_refresh;
    std::optional<Value> m_value;
};

struct Valued {
    std::string name;
    ValueKind kind = ValueKind::Number;
    bool edits = true;
    std::shared_ptr<Received> received = std::make_shared<Received>();
    std::optional<AnyBinding> binding; ///< picked up by the setup, like a built-in descriptor's
    std::optional<int> refresh;        ///< milliseconds; empty: the widget interface's default

    [[nodiscard]] std::unique_ptr<Widget> create() const {
        return std::make_unique<ValueWidget>(kind, edits, received, refresh);
    }
};

/// A store of one panel with these widgets, the way the UI holds it.
model::Store storeOf(std::vector<WidgetSetup> widgets) {
    UISetup setup;
    setup.panels = { { .name = "Panel", .widgets = std::move(widgets) } };
    return model::Store{ setup };
}

const WidgetId first{ 0 };

} // namespace

// ----- What can be bound, and how it reads and writes -----

TEST_CASE("a parameter is bound as the kind of its type", "[ui][binding]") {
    Param<bool> flag = true;
    Param<float> speed = 2.5f;
    Param<int> count = 7;
    Param<Form> form = Form::Square;
    Param<std::string> name = std::string("ant");

    REQUIRE(AnyBinding(flag).kind() == ValueKind::Bool);
    REQUIRE(AnyBinding(speed).kind() == ValueKind::Number);
    REQUIRE(AnyBinding(count).kind() == ValueKind::Number);
    REQUIRE(AnyBinding(form).kind() == ValueKind::Index);
    REQUIRE(AnyBinding(name).kind() == ValueKind::Text);

    REQUIRE(AnyBinding(flag).get() == Value(true));
    REQUIRE(AnyBinding(speed).get() == Value(2.5));
    REQUIRE(AnyBinding(count).get() == Value(7.0));
    REQUIRE(AnyBinding(form).get() == Value(std::size_t{ 1 }));
    REQUIRE(AnyBinding(name).get() == Value(std::string("ant")));
}

TEST_CASE("writing through a parameter's binding converts to the parameter's type", "[ui][binding]") {
    Param<int> count = 0;
    AnyBinding(count).set(Value(3.6)); // rounded
    REQUIRE(count.get() == 4);

    Param<Form> form;
    AnyBinding(form).set(Value(std::size_t{ 2 })); // the enumerator at that index
    REQUIRE(form.get() == Form::Triangle);

    Param<float> speed;
    const AnyBinding binding(speed);
    const Revision before = binding.revision();
    binding.set(Value(1.5));
    REQUIRE(speed.get() == 1.5f);
    REQUIRE(binding.revision() > before);

    binding.set(Value(true)); // a value of another kind is ignored
    REQUIRE(speed.get() == 1.5f);
}

TEST_CASE("an application's own binding is used as it is", "[ui][binding]") {
    Volume volume;
    const AnyBinding binding(volume);
    REQUIRE(binding.kind() == ValueKind::Number);
    REQUIRE_FALSE(binding.isReadOnly());
    binding.set(Value(0.8));
    REQUIRE(volume.level == 0.8);
    REQUIRE(binding.revision() == 1);
}

TEST_CASE("a binding of two functions notices changes by asking the getter", "[ui][binding]") {
    struct Config {
        float speedKmh = 36.f;
    } config;
    const AnyBinding binding = AnyBinding::fromFunctions(
        [&config] { return config.speedKmh / 3.6f; },
        [&config](float metresPerSecond) { config.speedKmh = metresPerSecond * 3.6f; }
    );
    REQUIRE(binding.kind() == ValueKind::Number);
    REQUIRE(std::get<double>(*binding.get()) == 10.0);

    const Revision first = binding.revision();
    REQUIRE(binding.revision() == first); // nothing changed
    config.speedKmh = 72.f;
    REQUIRE(binding.revision() > first); // changed behind the binding's back: noticed

    binding.set(Value(5.0));
    REQUIRE(config.speedKmh == 18.f);
}

TEST_CASE("a binding of one function can only be read", "[ui][binding]") {
    int ticks = 3;
    const AnyBinding binding = AnyBinding::fromFunction([&ticks] { return ticks; });
    REQUIRE(binding.isReadOnly());
    binding.set(Value(10.0)); // ignored
    REQUIRE(ticks == 3);

    const AnyBinding label = AnyBinding::ofText([] { return std::string("hello"); });
    REQUIRE(label.kind() == ValueKind::Text);
    REQUIRE(label.isReadOnly());
}

TEST_CASE("a series is bound as a source of samples, points as a source of points", "[ui][binding]") {
    Series samples(8);
    samples.push(1.f);
    samples.push(2.f);
    const AnyBinding series(samples);
    REQUIRE(series.kind() == ValueKind::Series);
    REQUIRE(series.isReadOnly());
    REQUIRE_FALSE(series.get().has_value());
    REQUIRE(series.series() != nullptr);
    REQUIRE_FALSE(series.series()->hasPoints());
    std::array<float, 4> out{};
    REQUIRE(series.series()->read(out) == 2);
    REQUIRE(series.revision() == samples.revision());

    PointSeries points(8);
    points.push({ .x = 1.f, .y = 10.f });
    points.push({ .x = 2.f, .y = 20.f });
    const AnyBinding pointBinding(points);
    REQUIRE(pointBinding.series()->hasPoints());
    std::array<Point, 4> pointsOut{};
    REQUIRE(pointBinding.series()->readPoints(pointsOut) == 2);
    REQUIRE(pointsOut[1].x == 2.f);
    // Asked for samples, a series of points gives its y values.
    REQUIRE(pointBinding.series()->read(out) == 2);
    REQUIRE(out[0] == 10.f);
    REQUIRE(out[1] == 20.f);

    Param<float> speed;
    REQUIRE(AnyBinding(speed).series() == nullptr);
}

TEST_CASE("values convert to and from application types", "[ui][binding]") {
    REQUIRE(valueAs<float>(valueOf(2.5f)) == 2.5f);
    REQUIRE(valueAs<int>(Value(2.6)) == 3);
    REQUIRE(valueAs<Form>(valueOf(Form::Triangle)) == Form::Triangle);
    REQUIRE(valueAs<bool>(valueOf(true)));
    REQUIRE(valueAs<std::string>(valueOf(std::string("x"))) == "x");
    REQUIRE(kindOf(valueOf(Form::Circle)) == ValueKind::Index);
    REQUIRE(kindOf(valueOf(7)) == ValueKind::Number);

    REQUIRE_THROWS_AS(valueAs<bool>(Value(1.0)), SetupError);
    REQUIRE_THROWS_WITH(
        valueAs<std::string>(Value(1.0)), ContainsSubstring("number cannot be read as one of kind text")
    );
}

// ----- Keeping widgets and data in step -----

TEST_CASE("a widget is handed its bound value, and again only when it changes", "[ui][binding][sync]") {
    Param<float> speed = 2.f;
    Valued slider{ .name = "Speed" };
    model::Store store = storeOf({ slider });
    binding::attach(store, first, AnyBinding(speed));
    const auto now = Clock::now();

    REQUIRE(binding::sync(store, now));
    REQUIRE(slider.received->values == std::vector<Value>{ Value(2.0) });
    REQUIRE(store.panel(PanelId{ 0 }).dirty);

    store.panel(PanelId{ 0 }).dirty = false;
    REQUIRE_FALSE(binding::sync(store, now)); // nothing new: no work
    REQUIRE(slider.received->values.size() == 1);
    REQUIRE_FALSE(store.panel(PanelId{ 0 }).dirty);

    speed = 3.f; // e.g. from the simulation thread
    REQUIRE(binding::sync(store, now));
    REQUIRE(slider.received->values.back() == Value(3.0));
    REQUIRE(store.panel(PanelId{ 0 }).dirty);
}

TEST_CASE("what the user enters is written to the binding and not handed back", "[ui][binding][sync]") {
    Param<float> speed = 2.f;
    Valued slider{ .name = "Speed" };
    model::Store store = storeOf({ slider });
    binding::attach(store, first, AnyBinding(speed));
    binding::sync(store, Clock::now());

    binding::write(store.widget(first), Value(4.0));
    REQUIRE(speed.get() == 4.f);
    REQUIRE_FALSE(binding::sync(store, Clock::now())); // the widget has that value already
    REQUIRE(slider.received->values.size() == 1);
}

TEST_CASE("a value that is only shown is handed over a few times per second at most", "[ui][binding][sync]") {
    Param<int> ticks = 0;
    Valued counter{ .name = "Ticks", .edits = false };
    model::Store store = storeOf({ counter });
    binding::attach(store, first, AnyBinding(ticks));
    const auto start = Clock::now();
    binding::sync(store, start); // the first value at once

    // A tick every frame for a while: the widget hears of it at most every 125 ms.
    for (int frame = 1; frame <= 60; ++frame) {
        ticks = frame;
        binding::sync(store, start + std::chrono::milliseconds(frame * 16));
    }
    // 60 frames are 960 ms: the first value and about one per 125 ms.
    REQUIRE(counter.received->values.size() <= 9);
    REQUIRE(counter.received->values.size() >= 7);

    // The last change is not lost: it arrives once the time has come.
    binding::sync(store, start + std::chrono::milliseconds(2000));
    REQUIRE(counter.received->values.back() == Value(60.0));
}

TEST_CASE("a value that is edited is handed over at once, every time", "[ui][binding][sync]") {
    Param<int> value = 0;
    Valued slider{ .name = "Value", .edits = true };
    model::Store store = storeOf({ slider });
    binding::attach(store, first, AnyBinding(value));
    const auto start = Clock::now();
    for (int frame = 0; frame < 10; ++frame) {
        value = frame + 1;
        binding::sync(store, start + std::chrono::milliseconds(frame));
    }
    REQUIRE(slider.received->values.size() == 10);
}

TEST_CASE("a widget type can ask to hear of changes more or less often", "[ui][binding][sync]") {
    Param<int> ticks = 0;
    Valued fast{ .name = "Fast", .edits = false, .refresh = 0 };   // every change, although only shown
    Valued slow{ .name = "Slow", .edits = false, .refresh = 500 }; // twice per second
    model::Store store = storeOf({ fast, slow });
    binding::attach(store, WidgetId{ 0 }, AnyBinding(ticks));
    binding::attach(store, WidgetId{ 1 }, AnyBinding(ticks));
    const auto start = Clock::now();
    for (int frame = 0; frame < 60; ++frame) {
        ticks = frame + 1;
        binding::sync(store, start + std::chrono::milliseconds(frame * 16));
    }
    REQUIRE(fast.received->values.size() == 60);
    REQUIRE(slow.received->values.size() == 2); // at 0 and at 512 ms
}

TEST_CASE(
    "by default, values that are only shown are slowed down, edited values and series are not", "[ui][binding][sync]"
) {
    const auto received = std::make_shared<Received>();
    REQUIRE(
        ValueWidget(ValueKind::Number, false, received, std::nullopt).refreshInterval() ==
        std::chrono::milliseconds(125)
    );
    REQUIRE(
        ValueWidget(ValueKind::Number, true, received, std::nullopt).refreshInterval() == std::chrono::milliseconds(0)
    );
    REQUIRE(
        ValueWidget(ValueKind::Series, false, received, std::nullopt).refreshInterval() == std::chrono::milliseconds(0)
    );
}

TEST_CASE("a widget can be bound again, and unbound", "[ui][binding][sync]") {
    Param<float> before = 1.f;
    Param<float> second = 2.f;
    Valued slider{ .name = "Speed" };
    model::Store store = storeOf({ slider });

    binding::attach(store, WidgetId{ 0 }, AnyBinding(before));
    binding::sync(store, Clock::now());
    binding::attach(store, WidgetId{ 0 }, AnyBinding(second)); // rebound: the new value at once
    binding::sync(store, Clock::now());
    REQUIRE(slider.received->values.back() == Value(2.0));

    binding::attach(store, WidgetId{ 0 }, std::nullopt); // unbound: keeps what it shows, hears no more
    second = 5.f;
    REQUIRE_FALSE(binding::sync(store, Clock::now()));
    REQUIRE(slider.received->values.back() == Value(2.0));
    binding::write(store.widget(WidgetId{ 0 }), Value(9.0)); // and writes nowhere
    REQUIRE(second.get() == 5.f);
}

TEST_CASE("a graph is handed its series, and its panel repainted when the series changes", "[ui][binding][sync]") {
    Series samples(16);
    Valued graph{ .name = "Graph", .kind = ValueKind::Series, .edits = false };
    model::Store store = storeOf({ graph });
    binding::attach(store, first, AnyBinding(samples));
    REQUIRE(graph.received->series != nullptr);
    binding::sync(store, Clock::now());

    store.panel(PanelId{ 0 }).dirty = false;
    samples.push(1.f);
    REQUIRE(binding::sync(store, Clock::now()));
    REQUIRE(store.panel(PanelId{ 0 }).dirty);
    REQUIRE(graph.received->values.empty()); // a graph reads its series itself

    binding::attach(store, first, std::nullopt);
    REQUIRE(graph.received->series == nullptr);
}

TEST_CASE("a binding that does not suit the widget is refused loudly", "[ui][binding][sync]") {
    Param<bool> flag;
    Param<float> speed;
    Valued slider{ .name = "Speed" };
    model::Store store = storeOf({ slider });

    REQUIRE_THROWS_AS(binding::attach(store, first, AnyBinding(flag)), SetupError);
    REQUIRE_THROWS_WITH(
        binding::attach(store, first, AnyBinding(flag)),
        ContainsSubstring("widget \"Panel/Speed\" cannot be bound to an on/off value")
    );

    // A widget that edits its value refuses a binding it cannot write.
    REQUIRE_THROWS_WITH(
        binding::attach(store, first, AnyBinding::fromFunction([] { return 1.0; })),
        ContainsSubstring("changes its value, but the binding can only be read")
    );

    REQUIRE_NOTHROW(binding::attach(store, first, AnyBinding(speed)));
}

TEST_CASE("bindings given in the setup are checked and take effect", "[ui][binding][sync]") {
    Param<float> speed = 3.f;
    Param<std::string> name;
    Valued slider{ .name = "Speed", .binding = AnyBinding(speed) };
    model::Store store = storeOf({ slider });
    binding::attachAll(store);
    binding::sync(store, Clock::now());
    REQUIRE(slider.received->values == std::vector<Value>{ Value(3.0) });

    Valued wrong{ .name = "Wrong", .binding = AnyBinding(name) };
    model::Store refused = storeOf({ wrong });
    REQUIRE_THROWS_WITH(
        binding::attachAll(refused), ContainsSubstring("widget \"Panel/Wrong\" cannot be bound to text")
    );
}

TEST_CASE("a change the user makes is in the binding before the application hears of it", "[ui][binding][sync]") {
    Param<float> speed = 1.f;
    Valued slider{ .name = "Speed" };
    model::Store store = storeOf({ slider });
    binding::attach(store, first, AnyBinding(speed));

    input::InputSystem input;
    std::vector<Event> events;
    const std::vector<PanelId> stacking = store.stackingOrder();
    // A key event lets the input system know the store; the widget then reports a change.
    input.handle(sf::Event::MouseLeft{}, store, stacking, Layout().sizesAt({ 800.f, 600.f }), events);
    input.changeValue(first, Value(6.0), true);

    REQUIRE(speed.get() == 6.f);
    REQUIRE(events.size() == 1);
    REQUIRE(events[0].changeOf("Speed")->as<float>() == 6.f);
}
