#include "atpl/ui/error.hpp"
#include "atpl/ui/setup.hpp"
#include "atpl/ui/widgets.hpp"

#include "ui/binding/sync.hpp"
#include "ui/render/draw_list.hpp"
#include "ui/render/text_measurer.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <algorithm>
#include <cmath>
#include <memory>
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
const Sizes sizes = Layout().sizesAt({ 1280.f, 720.f });

/// Paints a widget at this size and returns what it drew.
render::DrawList paint(const Widget& widget, const Theme& theme, sf::Vector2f size = { 260.f, 160.f }) {
    render::DrawList list;
    Painter painter(list, { 0.f, 0.f }, size, &measurer);
    widget.paint(painter, Style(theme, {}, State::Normal, sizes));
    return list;
}

bool shows(const render::DrawList& list, std::string_view text) {
    const auto runs = list.texts();
    return std::any_of(runs.begin(), runs.end(), [&](const render::TextRun& run) { return run.text == text; });
}

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

/// The vertical extent of what is drawn in this colour: top and bottom.
std::pair<float, float> extentOf(const render::VertexList& vertices, sf::Color color) {
    float top = 1.0e9f;
    float bottom = -1.0e9f;
    for (const sf::Vertex& vertex : vertices) {
        if (vertex.color == color) {
            top = std::min(top, vertex.position.y);
            bottom = std::max(bottom, vertex.position.y);
        }
    }
    return { top, bottom };
}

/// A graph of this series with these options, connected as the UI would.
struct BoundGraph {
    std::unique_ptr<Widget> widget;
    AnyBinding binding;

    BoundGraph(Series& series, GraphOptions options) :
        widget(Graph("Graph", options).create()),
        binding(series) {
        widget->setSeries(binding.series());
    }
    BoundGraph(PointSeries& points, GraphOptions options) :
        widget(Graph("Graph", options).create()),
        binding(points) {
        widget->setSeries(binding.series());
    }
};

Theme everythingOn() {
    Theme theme;
    theme[Graph::Shadow].shown = true;
    theme[Graph::Value].shown = true;
    theme[Graph::AxisLabels].shown = true;
    theme[Graph::Grid].shown = true;
    theme[Graph::Axis].shown = true;
    return theme;
}

} // namespace

// ----- ValueDisplay -----

TEST_CASE("a value display shows numbers in its format, and other kinds as text", "[ui][widgets][value_display]") {
    const Theme theme;
    const auto display = ValueDisplay("Ticks", { .format = "{:.1f} /s" }).create();

    display->setValue(Value(59.94));
    REQUIRE(shows(paint(*display, theme), "59.9 /s"));
    REQUIRE(shows(paint(*display, theme), "Ticks"));
    display->setValue(Value(true));
    REQUIRE(shows(paint(*display, theme), "on"));
    display->setValue(Value(std::size_t{ 3 }));
    REQUIRE(shows(paint(*display, theme), "3"));
    display->setValue(Value(std::string("running")));
    REQUIRE(shows(paint(*display, theme), "running"));
    REQUIRE(display->value() == Value(std::string("running")));
}

TEST_CASE("a value display accepts every kind but a series, and only shows", "[ui][widgets][value_display]") {
    const auto display = ValueDisplay("Value").create();
    REQUIRE(display->accepts(ValueKind::Number));
    REQUIRE(display->accepts(ValueKind::Text));
    REQUIRE(display->accepts(ValueKind::Bool));
    REQUIRE(display->accepts(ValueKind::Index));
    REQUIRE_FALSE(display->accepts(ValueKind::Series));
    REQUIRE_FALSE(display->editsValue());
    REQUIRE_FALSE(display->reactsToPointer());                             // never looks hovered
    REQUIRE(display->refreshInterval() == std::chrono::milliseconds(125)); // throttled
}

TEST_CASE(
    "a value display starts with its initial text, and is bound like any widget", "[ui][widgets][value_display]"
) {
    Param<int> count = 42;
    UISetup setup;
    setup.panels = { { .name = "Stats",
                       .widgets = { ValueDisplay("Count", count), ValueDisplay("Status", { .initial = "idle" }) } } };
    model::Store store{ setup };
    binding::attachAll(store);
    binding::sync(store, binding::Clock::now());

    REQUIRE(store.widget(store.names().widget("Count")).widget->value() == Value(std::string("42")));
    REQUIRE(store.widget(store.names().widget("Status")).widget->value() == Value(std::string("idle")));
}

TEST_CASE("a value display refuses a format that cannot show a number", "[ui][widgets][value_display]") {
    REQUIRE_THROWS_WITH(
        ValueDisplay("Ticks", { .format = "{:s}" }).create(), ContainsSubstring("value display \"Ticks\": format")
    );
}

// ----- ProgressBar -----

TEST_CASE("a progress bar is filled as far as its value is between min and max", "[ui][widgets][progress]") {
    const Theme theme;
    const auto bar = ProgressBar("Progress", { .min = 0.0, .max = 200.0 }).create();
    const sf::Color fill = theme.resolve(ProgressBar::Fill).color;

    bar->setValue(Value(50.0));
    const float quarter = rightmost(paint(*bar, theme).shapes(), fill);
    bar->setValue(Value(150.0));
    const float threeQuarters = rightmost(paint(*bar, theme).shapes(), fill);
    REQUIRE(threeQuarters > quarter + 100.f);

    bar->setValue(Value(500.0)); // beyond max: full, not beyond
    REQUIRE(rightmost(paint(*bar, theme).shapes(), fill) <= 260.f);
    bar->setValue(Value(0.0)); // empty: no fill at all
    REQUIRE_FALSE(hasColor(paint(*bar, theme).shapes(), fill));
    REQUIRE(shows(paint(*bar, theme), "Progress"));
}

TEST_CASE("a progress bar only shows numbers, and refuses a range that cannot work", "[ui][widgets][progress]") {
    const auto bar = ProgressBar("Progress").create();
    REQUIRE(bar->accepts(ValueKind::Number));
    REQUIRE_FALSE(bar->accepts(ValueKind::Bool));
    REQUIRE_FALSE(bar->editsValue());
    REQUIRE_FALSE(bar->reactsToPointer());
    REQUIRE_THROWS_WITH(
        ProgressBar("Progress", { .min = 1.0, .max = 1.0 }).create(), ContainsSubstring("max (1) must be above min (1)")
    );
}

// ----- Graph -----

TEST_CASE("a graph needs two rows of plot at least, unless it asks to be smaller", "[ui][widgets][graph]") {
    const Theme theme;
    Series series(8);
    const BoundGraph plain(series, {});
    const BoundGraph small(series, { .height = 10.f });
    const BoundGraph tall(series, { .height = 200.f });
    const MeasureContext context(300.f, theme, {}, sizes, &measurer);
    const float leastPlain = plain.widget->measure(context).min.y;
    REQUIRE(small.widget->measure(context).min.y < leastPlain); // a small graph may be small
    REQUIRE(tall.widget->measure(context).min.y == leastPlain); // a tall one can still give way
    REQUIRE(small.widget->measure(context).min.y > 10.f);       // the label stays
}

TEST_CASE("a graph draws the samples of its series as a curve", "[ui][widgets][graph]") {
    const Theme theme;
    Series series(100);
    BoundGraph graph(series, {});
    const sf::Color curve = theme.resolve(Graph::Curve).color;
    REQUIRE_FALSE(hasColor(paint(*graph.widget, theme).shapes(), curve)); // nothing yet

    for (int i = 0; i < 50; ++i) {
        series.push(static_cast<float>(i % 10));
    }
    const render::DrawList list = paint(*graph.widget, theme);
    REQUIRE(hasColor(list.shapes(), curve));
    REQUIRE(shows(list, "Graph"));     // its label
    REQUIRE_FALSE(shows(list, "9.0")); // the current value only if the theme shows it
}

TEST_CASE(
    "a zeroed graph has its reference line at zero, an average-centred one in the middle", "[ui][widgets][graph]"
) {
    Theme theme;
    const sf::Color line(9, 8, 7);
    theme[Graph::Baseline].color = line;
    Series series(10);
    for (const float value : { 10.f, 20.f, 30.f, 20.f }) {
        series.push(value);
    }

    BoundGraph zeroed(series, { .base = GraphBase::Zero });
    BoundGraph centred(series, { .base = GraphBase::Average });
    const float zeroTop = extentOf(paint(*zeroed.widget, theme).shapes(), line).first;
    const float centreTop = extentOf(paint(*centred.widget, theme).shapes(), line).first;
    REQUIRE(zeroTop > centreTop + 40.f); // zero is at the bottom of 0..30, the average 20 in the middle of 10..30
}

TEST_CASE("with everything on, a graph shows its value, labels on both axes, and a grid", "[ui][widgets][graph]") {
    const Theme theme = everythingOn();
    Series series(120);
    for (int i = 0; i < 120; ++i) {
        series.push(static_cast<float>(i % 20));
    }
    BoundGraph graph(series, { .format = "{:.0f}" });
    const render::DrawList list = paint(*graph.widget, theme, { 300.f, 180.f });

    REQUIRE(shows(list, "19"));   // the newest value, and the top of the axis
    REQUIRE(shows(list, "0"));    // the bottom of the value axis, and the newest sample on the x-axis
    REQUIRE(shows(list, "-119")); // the oldest sample
    REQUIRE(shows(list, "-59"));
}

TEST_CASE("a logarithmic graph has whole powers of ten at the ends of its axis", "[ui][widgets][graph]") {
    const Theme theme = everythingOn();
    Series series(10);
    for (const float value : { 0.3f, 5.f, 879.f }) {
        series.push(value);
    }
    BoundGraph graph(series, { .logarithmic = true, .format = "{:g}" });
    const render::DrawList list = paint(*graph.widget, theme);
    REQUIRE(shows(list, "0.1"));
    REQUIRE(shows(list, "10"));
    REQUIRE(shows(list, "1000"));
}

TEST_CASE("a graph's x-axis counts samples, or seconds", "[ui][widgets][graph]") {
    const Theme theme = everythingOn();
    Series series(200);
    for (int i = 0; i < 200; ++i) {
        series.push(1.f);
    }
    BoundGraph count(series, { .samples = 101 });
    REQUIRE(shows(paint(*count.widget, theme), "-100")); // the window of the options, not the data

    BoundGraph time(series, { .samples = 101, .x = GraphX::Time, .secondsPerSample = 0.1f });
    const render::DrawList list = paint(*time.widget, theme);
    REQUIRE(shows(list, "-10 s"));
    REQUIRE(shows(list, "-5 s"));
    REQUIRE(shows(list, "0 s"));
}

TEST_CASE("a graph of points takes its x-axis from the data", "[ui][widgets][graph]") {
    const Theme theme = everythingOn();
    PointSeries points(16);
    points.push({ .x = 20.f, .y = 1.f });
    points.push({ .x = 30.f, .y = 3.f });
    points.push({ .x = 40.f, .y = 2.f });
    BoundGraph graph(points, { .format = "{:.0f}" });
    const render::DrawList list = paint(*graph.widget, theme);
    REQUIRE(shows(list, "20"));
    REQUIRE(shows(list, "40"));
    REQUIRE(hasColor(list.shapes(), theme.resolve(Graph::Curve).color));
}

TEST_CASE("a graph with fixed ends keeps them", "[ui][widgets][graph]") {
    const Theme theme = everythingOn();
    Series series(4);
    series.push(5.f);
    BoundGraph graph(series, { .min = 0.f, .max = 100.f, .format = "{:.0f}" });
    REQUIRE(shows(paint(*graph.widget, theme), "100"));
}

TEST_CASE("graph options that cannot work are refused when the UI is built", "[ui][widgets][graph]") {
    REQUIRE_THROWS_WITH(
        Graph("G", { .min = 5.f, .max = 5.f }).create(), ContainsSubstring("graph \"G\": max must be above min")
    );
    REQUIRE_THROWS_WITH(
        Graph("G", { .min = 0.f, .logarithmic = true }).create(),
        ContainsSubstring("logarithmic axis needs a min above zero")
    );
    REQUIRE_THROWS_WITH(
        Graph("G", { .x = GraphX::Time, .secondsPerSample = 0.f }).create(), ContainsSubstring("time between samples")
    );
    REQUIRE_THROWS_WITH(Graph("G", { .format = "{:d}" }).create(), ContainsSubstring("cannot show a number"));
}

TEST_CASE("a graph fills whatever it is given and hears of every change", "[ui][widgets][graph]") {
    const auto graph = Graph("G").create();
    const Theme theme;
    const MeasureContext context(260.f, theme, {}, sizes, &measurer);
    const SizeRequest request = graph->measure(context);
    REQUIRE(request.isDynamic());
    REQUIRE(request.preferred.y > sizes.rowHeight * 3.f);
    REQUIRE(graph->accepts(ValueKind::Series));
    REQUIRE_FALSE(graph->reactsToPointer());
    REQUIRE(graph->refreshInterval() == std::chrono::milliseconds(0));

    const auto tall = Graph("G", { .height = 300.f }).create();
    REQUIRE(tall->measure(context).preferred.y > 300.f);
}
