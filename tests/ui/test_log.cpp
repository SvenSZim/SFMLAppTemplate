#include "atpl/ui/error.hpp"
#include "atpl/ui/setup.hpp"
#include "atpl/ui/widgets.hpp"

#include "ui/binding/sync.hpp"
#include "ui/input/input_system.hpp"
#include "ui/layout/arrange.hpp"
#include "ui/render/draw_list.hpp"
#include "ui/render/text_measurer.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <algorithm>
#include <cmath>
#include <regex>
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
        input.setLook(theme, &measurer);
    }

    static UISetup setupOf(std::vector<WidgetSetup> widgets) {
        UISetup setup;
        setup.panels = {
            { .name = "Panel", .placement = Anchor::TopLeft, .width = 300.f, .widgets = std::move(widgets) }
        };
        return setup;
    }

    [[nodiscard]] model::WidgetSlot& slot(std::string_view name) { return store.widget(store.names().widget(name)); }

    [[nodiscard]] FloatRect rectOf(std::string_view name) {
        const model::WidgetSlot& widget = slot(name);
        const model::Panel& panel = store.panel(widget.panel);
        return { panel.rect.position() + sf::Vector2f(0.f, sizes.headerHeight) + widget.rect.position(),
                 widget.rect.size() };
    }

    std::vector<Event> send(const sf::Event& event) {
        events.clear();
        input.handle(event, store, stacking, sizes, events);
        return events;
    }
    void wheel(sf::Vector2f at, float delta) {
        send(sf::Event::MouseWheelScrolled{ sf::Mouse::Wheel::Vertical, delta, sf::Vector2i(at) });
    }
    void press(sf::Vector2f at) { send(sf::Event::MouseButtonPressed{ sf::Mouse::Button::Left, sf::Vector2i(at) }); }
    void moveTo(sf::Vector2f at) { send(sf::Event::MouseMoved{ sf::Vector2i(at) }); }
    void release(sf::Vector2f at) { send(sf::Event::MouseButtonReleased{ sf::Mouse::Button::Left, sf::Vector2i(at) }); }

    /// Paints a widget as the UI would, and returns what it drew.
    [[nodiscard]] render::DrawList paint(std::string_view name, const Theme* look = nullptr) {
        const model::WidgetSlot& widget = slot(name);
        render::DrawList list;
        Painter painter(list, { 0.f, 0.f }, widget.rect.size(), &measurer);
        widget.widget->paint(
            painter, Style(look != nullptr ? *look : theme, widget.colors, model::stateOf(widget), sizes)
        );
        return list;
    }

    /// The texts a log shows, top to bottom, without its label.
    [[nodiscard]] std::vector<std::string> shown(std::string_view name, const Theme* look = nullptr) {
        const render::DrawList list = paint(name, look);
        std::vector<std::string> texts;
        for (const render::TextRun& run : list.texts()) {
            texts.emplace_back(run.text);
        }
        texts.erase(texts.begin()); // the label
        return texts;
    }
};

void fill(TextLog& log, int count) {
    for (int i = 0; i < count; ++i) {
        log.push("line " + std::to_string(i));
    }
}

} // namespace

TEST_CASE("a log shows its newest lines, the newest at the bottom", "[ui][widgets][log]") {
    TextLog events(100);
    Harness ui({ Log("Events", events, { .lines = 3 }) });
    REQUIRE(ui.shown("Events").empty());

    fill(events, 2);
    REQUIRE(ui.shown("Events") == std::vector<std::string>{ "line 0", "line 1" }); // from the top
    events.push("line 2");
    events.push("line 3");
    REQUIRE(ui.shown("Events") == std::vector<std::string>{ "line 1", "line 2", "line 3" }); // as many as it shows
}

TEST_CASE("a log is as high as its lines say, whatever it holds", "[ui][widgets][log]") {
    TextLog empty(10);
    TextLog full(200);
    fill(full, 200);
    const auto measureOf = [](TextLog& log, std::size_t lines) {
        const auto widget = Log("Events", log, { .lines = lines }).create();
        AnyBinding binding(log);
        widget->setLines(binding.lines());
        return widget->measure(MeasureContext(280.f, Theme(), {}, sizes, &measurer));
    };
    const SizeRequest few = measureOf(empty, 4);
    REQUIRE(few.min.y == measureOf(full, 4).min.y);
    REQUIRE(few.max.has_value());
    REQUIRE(few.max->y == few.min.y); // fixed
    REQUIRE(measureOf(empty, 8).min.y > few.min.y);
}

TEST_CASE("a log line wider than the log is cut, not wrapped", "[ui][widgets][log]") {
    TextLog events(10);
    events.push(std::string(500, 'x'));
    Harness ui({ Log("Events", events, { .lines = 3 }) });
    const render::DrawList list = ui.paint("Events");
    const auto runs = list.texts();
    REQUIRE(runs.size() == 2);
    REQUIRE_FALSE(runs[1].wrapped);
    REQUIRE(runs[1].rect.right() <= ui.slot("Events").rect.width()); // cut short to the log's width
    REQUIRE(runs[1].rect.height() < ui.slot("Events").rect.height() / 3.f);
}

TEST_CASE("scrolled back, a log keeps showing the same lines; at the end it follows again", "[ui][widgets][log]") {
    TextLog events(100);
    fill(events, 10);
    Harness ui({ Log("Events", events, { .lines = 3 }) });
    REQUIRE(ui.shown("Events") == std::vector<std::string>{ "line 7", "line 8", "line 9" });

    const sf::Vector2f inside = ui.rectOf("Events").center();
    ui.wheel(inside, 1.f); // up: older lines
    const std::vector<std::string> back = ui.shown("Events");
    REQUIRE(back.back() != "line 9");

    events.push("line 10");
    events.push("line 11");
    REQUIRE(ui.shown("Events") == back); // new lines do not move it

    ui.wheel(inside, -100.f); // to the end
    REQUIRE(ui.shown("Events").back() == "line 11");
    events.push("line 12");
    REQUIRE(ui.shown("Events").back() == "line 12"); // following again
}

TEST_CASE("a log's scrollbar is dragged, and a press on its track moves it there", "[ui][widgets][log]") {
    TextLog events(100);
    fill(events, 30);
    Harness ui({ Log("Events", events, { .lines = 3 }) });
    static_cast<void>(ui.paint("Events")); // a log knows how much it holds once it has been painted

    // The track is in the box's right inset; a press at its top goes to the oldest lines.
    const FloatRect rect = ui.rectOf("Events");
    const float x = rect.right() - std::max(sizes.padding.x * 0.6f, 2.f) * 0.5f;
    ui.press({ x, rect.top() + (rect.height() - 1.f) * 0.5f });
    ui.moveTo({ x, rect.top() });
    ui.release({ x, rect.top() });
    REQUIRE(ui.shown("Events").front() == "line 0");

    const float trackBottom = rect.bottom() - std::max(std::round(sizes.padding.y * 0.4f), 2.f);
    ui.press({ x, trackBottom - 2.f });
    ui.release({ x, trackBottom - 2.f });
    REQUIRE(ui.shown("Events").back() == "line 29");
}

TEST_CASE("a log shows the time of each line only if the theme shows it", "[ui][widgets][log]") {
    TextLog events(10);
    events.push("started");
    Harness ui({ Log("Events", events) });
    REQUIRE(ui.shown("Events") == std::vector<std::string>{ "started" });

    Theme timed;
    timed[Log::Time].shown = true;
    const std::vector<std::string> texts = ui.shown("Events", &timed);
    REQUIRE(texts.size() == 2);
    REQUIRE(std::regex_match(texts[0], std::regex("[0-9]{2}:[0-9]{2}:[0-9]{2}")));
    REQUIRE(texts[1] == "started");
}

TEST_CASE("a log is bound to lines only, and needs to show at least one", "[ui][widgets][log]") {
    TextLog events(10);
    AnyBinding binding(events);
    REQUIRE(binding.kind() == ValueKind::Lines);
    REQUIRE(binding.isReadOnly());
    REQUIRE_FALSE(binding.get().has_value());
    REQUIRE(binding.lines() != nullptr);
    REQUIRE(binding.series() == nullptr);

    const auto log = Log("Events").create();
    REQUIRE(log->accepts(ValueKind::Lines));
    REQUIRE_FALSE(log->accepts(ValueKind::Series));
    REQUIRE_FALSE(ValueDisplay("T").create()->accepts(ValueKind::Lines));
    REQUIRE_THROWS_WITH(Log("Events", { .lines = 0 }).create(), ContainsSubstring("log \"Events\": it needs to show"));
}
