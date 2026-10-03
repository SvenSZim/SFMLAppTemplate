#include "atpl/ui/setup.hpp"
#include "atpl/ui/widgets.hpp"

#include "ui/binding/sync.hpp"
#include "ui/input/input_system.hpp"
#include "ui/layout/arrange.hpp"
#include "ui/layout/overlay_placement.hpp"
#include "ui/layout/widget_layout.hpp"
#include "ui/render/panel_batch.hpp"
#include "ui/render/text_measurer.hpp"
#include "ui/widgets/panel_frame.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

using namespace atpl;
using Catch::Approx;

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

/// Buttons "B0" to "B<count - 1>", with a slider after the first.
std::vector<WidgetSetup> manyWidgets(int count, Param<float>& level) {
    std::vector<WidgetSetup> widgets;
    for (int i = 0; i < count; ++i) {
        widgets.push_back(Button("B" + std::to_string(i)));
        if (i == 0) {
            widgets.push_back(Slider("Level", level, { .min = 0.0, .max = 10.0, .step = 1.0 }));
        }
    }
    return widgets;
}

/// A UI's insides, without a window: one floating panel in a low window, so that its widgets
/// do not fit and it scrolls.
struct Harness {
    model::Store store;
    Theme theme;
    std::vector<PanelId> stacking;
    input::InputSystem input;
    std::vector<Event> events;
    sf::Vector2f window;

    Harness(std::vector<WidgetSetup> widgets, sf::Vector2f windowSize) :
        store(setupOf(std::move(widgets))),
        window(windowSize) {
        layout::prepare(store, {});
        binding::attachAll(store);
        binding::sync(store, binding::Clock::now());
        arrange();
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

    void arrange() {
        layout::arrange(store, window, {}, theme, sizes, &measurer);
        input.clampScroll(store, sizes);
        input.forgetHover(store);
    }

    [[nodiscard]] model::Panel& panel() { return store.panel(PanelId{ 0 }); }
    [[nodiscard]] model::WidgetSlot& slot(std::string_view name) { return store.widget(store.names().widget(name)); }
    [[nodiscard]] float overflow() { return layout::contentOverflow(panel(), sizes); }
    [[nodiscard]] widgets::ScrollbarPlace bar() { return *widgets::scrollbarOf(panel(), sizes, overflow()); }

    /// A widget's rectangle in the window, as it is on screen now: scrolled.
    [[nodiscard]] FloatRect rectOf(std::string_view name) {
        const model::WidgetSlot& widget = slot(name);
        return { panel().rect.position() + sf::Vector2f(0.f, sizes.headerHeight - panel().scroll) +
                     widget.rect.position(),
                 widget.rect.size() };
    }
    /// The middle of the scrollbar's thumb, in the window.
    [[nodiscard]] sf::Vector2f thumb() {
        const widgets::ScrollbarPlace place = bar();
        return panel().rect.position() + sf::Vector2f(
                                             place.track.left() + place.track.width() * 0.5f,
                                             place.thumbTop(panel().scroll) + place.thumbLength * 0.5f
                                         );
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
    std::vector<Event> wheel(sf::Vector2f at, float delta) {
        return send(sf::Event::MouseWheelScrolled{ sf::Mouse::Wheel::Vertical, delta, sf::Vector2i(at) });
    }
};

/// The names of the buttons pressed among these events.
std::vector<std::string> pressed(const std::vector<Event>& events) {
    std::vector<std::string> names;
    for (const Event& event : events) {
        if (const auto* button = event.getIf<ButtonPressed>()) {
            names.emplace_back(button->name);
        }
    }
    return names;
}

} // namespace

TEST_CASE("the wheel over a panel that overflows scrolls it, within its range", "[ui][scrolling]") {
    Param<float> level = 0.f;
    Harness ui(manyWidgets(12, level), { 600.f, 260.f });
    REQUIRE(ui.overflow() > 0.f);
    const float step = layout::scrollStep(ui.store, sizes);
    const sf::Vector2f inside = ui.rectOf("B0").center();
    ui.panel().dirty = false;

    REQUIRE(ui.wheel(inside, -1.f).empty()); // down one notch: the UI's, not forwarded
    REQUIRE(ui.panel().scroll == Approx(step));
    REQUIRE_FALSE(ui.panel().dirty); // only an offset: nothing is painted again

    ui.wheel(inside, 2.f); // up beyond the top
    REQUIRE(ui.panel().scroll == 0.f);
    ui.wheel(inside, -1000.f); // down beyond the end
    REQUIRE(ui.panel().scroll == Approx(ui.overflow()));
}

TEST_CASE("scrolling drops the hover once, and the next move finds it again", "[ui][scrolling]") {
    Param<float> level = 0.f;
    Harness ui(manyWidgets(12, level), { 600.f, 260.f });
    const sf::Vector2f where = ui.rectOf("B0").center();
    ui.moveTo(where);
    REQUIRE(ui.slot("B0").hovered);

    ui.wheel(where, -1.f);
    REQUIRE_FALSE(ui.slot("B0").hovered); // repainted once, to show it is no longer hovered
    ui.panel().dirty = false;
    ui.wheel(where, -1.f);
    ui.wheel(where, -1.f);
    REQUIRE_FALSE(ui.panel().dirty); // further notches paint nothing

    ui.moveTo(where + sf::Vector2f(1.f, 0.f));
    REQUIRE_FALSE(ui.slot("B0").hovered); // another button is there now
    REQUIRE(ui.input.hovered().has_value());
}

TEST_CASE("in a panel that scrolls the wheel scrolls it, also over a slider", "[ui][scrolling]") {
    Param<float> level = 5.f;
    Harness ui(manyWidgets(12, level), { 600.f, 260.f });
    ui.wheel(ui.rectOf("Level").center(), -1.f);
    REQUIRE(level.get() == 5.f);
    REQUIRE(ui.panel().scroll > 0.f);

    // In a panel that fits, the slider has it.
    Param<float> other = 5.f;
    Harness roomy(manyWidgets(2, other), { 600.f, 700.f });
    REQUIRE(roomy.overflow() == 0.f);
    roomy.wheel(roomy.rectOf("Level").center(), 1.f);
    REQUIRE(other.get() == 6.f);
}

TEST_CASE("after scrolling, a click reaches the widget that is under the pointer now", "[ui][scrolling]") {
    Param<float> level = 0.f;
    Harness ui(manyWidgets(12, level), { 600.f, 260.f });
    const sf::Vector2f where = ui.rectOf("B0").center();
    REQUIRE(pressed(ui.click(where)) == std::vector<std::string>{ "B0" });

    ui.wheel(where, -1000.f); // to the end: the last button is in view
    const FloatRect last = ui.rectOf("B11");
    REQUIRE(last.bottom() <= ui.panel().rect.bottom());
    REQUIRE(pressed(ui.click(last.center())) == std::vector<std::string>{ "B11" });

    // A widget scrolled up under the header is not there: the header is.
    const sf::Vector2f header(ui.rectOf("B0").center().x, ui.panel().rect.top() + sizes.headerHeight * 0.5f);
    REQUIRE(ui.rectOf("B0").bottom() < ui.panel().rect.top() + sizes.headerHeight);
    REQUIRE(pressed(ui.click(header)).empty());
}

TEST_CASE("the scrollbar's thumb is dragged, and a press beside it moves it there", "[ui][scrolling]") {
    Param<float> level = 0.f;
    Harness ui(manyWidgets(12, level), { 600.f, 260.f });
    const widgets::ScrollbarPlace place = ui.bar();
    REQUIRE(place.thumbLength < place.track.height());

    // Grabbed in its middle and moved down by a quarter of the way it can travel.
    const sf::Vector2f start = ui.thumb();
    const float travel = place.track.height() - place.thumbLength;
    ui.press(start);
    REQUIRE(ui.panel().scrollbarDragged);
    REQUIRE(ui.panel().scroll == 0.f);                                       // grabbed where it is: nothing moves yet
    ui.moveTo(start + sf::Vector2f(40.f, travel * 0.25f));                   // sideways too: it still follows
    REQUIRE(ui.panel().scroll == Approx(ui.overflow() * 0.25f).margin(3.f)); // pointers move in whole pixels
    ui.release(start);
    REQUIRE_FALSE(ui.panel().scrollbarDragged);

    // A press at the bottom of the track: the thumb's middle goes there, as far as it can.
    const sf::Vector2f bottom =
        ui.panel().rect.position() +
        sf::Vector2f(place.track.left() + place.track.width() * 0.5f, place.track.bottom() - 1.f);
    REQUIRE(ui.click(bottom).empty());
    REQUIRE(ui.panel().scroll == Approx(ui.overflow()));
}

TEST_CASE("the scrollbar lights up under the pointer, and only a panel that overflows has one", "[ui][scrolling]") {
    Param<float> level = 0.f;
    Harness ui(manyWidgets(12, level), { 600.f, 260.f });
    ui.moveTo(ui.thumb());
    REQUIRE(ui.panel().scrollbarHovered);
    ui.moveTo(ui.rectOf("B0").center());
    REQUIRE_FALSE(ui.panel().scrollbarHovered);

    Param<float> other = 0.f;
    Harness roomy(manyWidgets(2, other), { 600.f, 700.f });
    REQUIRE_FALSE(widgets::scrollbarOf(roomy.panel(), sizes, roomy.overflow()).has_value());
}

TEST_CASE(
    "the scroll offset stays within the range when the window changes, and is kept while folded", "[ui][scrolling]"
) {
    Param<float> level = 0.f;
    Harness ui(manyWidgets(12, level), { 600.f, 260.f });
    ui.wheel(ui.rectOf("B0").center(), -1000.f);
    const float end = ui.panel().scroll;

    ui.window = { 600.f, 330.f }; // more room: less to scroll
    ui.arrange();
    REQUIRE(ui.overflow() < end);
    REQUIRE(ui.panel().scroll == Approx(ui.overflow()));

    const float kept = ui.panel().scroll;
    ui.panel().collapsed = true; // folded: nothing to scroll, but the offset waits for it
    ui.arrange();
    REQUIRE(ui.panel().scroll == kept);
}

TEST_CASE("a thumb is as much of its track as the content area is of the content", "[ui][scrolling]") {
    model::Panel panel;
    panel.rect = FloatRect(0.f, 0.f, 300.f, sizes.headerHeight + 200.f);
    panel.contentHeight = 800.f;
    panel.shown = true;
    const auto place = widgets::scrollbarOf(panel, sizes, 600.f);
    REQUIRE(place.has_value());
    REQUIRE(place->thumbLength == Approx(place->track.height() * 200.f / 800.f));
    REQUIRE(place->track.right() <= 300.f);
    REQUIRE(place->track.left() >= 300.f - sizes.padding.x); // in the padding: no widget is narrowed
    REQUIRE(place->thumbTop(0.f) == place->track.top());
    REQUIRE(place->thumbTop(600.f) == Approx(place->track.bottom() - place->thumbLength));
    REQUIRE(place->scrollFor(place->thumbTop(150.f)) == Approx(150.f));
}

TEST_CASE("the wheel scrolls every panel by the same step: the lowest row on screen", "[ui][scrolling]") {
    Param<float> level = 0.f;
    Harness ui(manyWidgets(3, level), { 600.f, 700.f });
    REQUIRE(layout::scrollStep(ui.store, sizes) == sizes.rowHeight);

    ui.panel().grid = true;
    ui.panel().rowHeight = sizes.rowHeight * 0.5f; // a grid squeezed below the row height
    REQUIRE(layout::scrollStep(ui.store, sizes) == sizes.rowHeight * 0.5f);
    ui.panel().rowHeight = sizes.rowHeight * 3.f; // higher rows do not make the step larger
    REQUIRE(layout::scrollStep(ui.store, sizes) == sizes.rowHeight);
}

TEST_CASE("a dropdown in a scrolled panel opens at its field where it is on screen", "[ui][scrolling]") {
    Param<float> level = 0.f;
    std::vector<WidgetSetup> widgets = manyWidgets(12, level);
    widgets.push_back(Dropdown("Mode", { "A", "B", "C" }));
    Harness ui(std::move(widgets), { 600.f, 260.f });
    ui.wheel(ui.rectOf("B0").center(), -1000.f);

    const FloatRect field = ui.rectOf("Mode");
    ui.click({ field.center().x, field.bottom() - 6.f });
    REQUIRE(ui.slot("Mode").overlayOpen);
    layout::placeOverlays(ui.store, ui.input.overlay(), ui.window, ui.theme, sizes, &measurer);
    REQUIRE(ui.slot("Mode").overlayAnchor.bottom() == Approx(field.bottom()));
}

TEST_CASE("scrolling a batch moves its content and its thumb, and rebuilds nothing", "[ui][scrolling][panel_batch]") {
    render::PanelBatch batch;
    static_cast<void>(batch.rebuild());
    batch.setPosition({ 100.f, 50.f });
    static_cast<void>(batch.takeChanged());
    const std::size_t rebuilt = batch.rebuildCount();

    batch.setScroll(30.f);
    batch.setScrollbarOffset(12.f);
    REQUIRE(batch.takeChanged());   // a new frame is needed ...
    REQUIRE_FALSE(batch.isDirty()); // ... but nothing is painted again
    REQUIRE(batch.rebuildCount() == rebuilt);
    REQUIRE(batch.scrollbarTransform().transformPoint({ 0.f, 0.f }) == sf::Vector2f(100.f, 62.f));
    REQUIRE(batch.contentTransform().transformPoint({ 0.f, 0.f }) == sf::Vector2f(100.f, 20.f));
}

TEST_CASE("content is cut off before the panel's bottom border, and cannot be clicked there", "[ui][scrolling]") {
    const FloatRect area = widgets::contentArea({ 300.f, 200.f }, sizes);
    REQUIRE(area == FloatRect(0.f, sizes.headerHeight, 300.f, 200.f - sizes.headerHeight - sizes.padding.y));

    // At the top, a widget runs into the margin at the bottom: there it is not there.
    Param<float> level = 0.f;
    Harness ui(manyWidgets(12, level), { 600.f, 260.f });
    const float margin = ui.panel().rect.bottom() - sizes.padding.y;
    const char* cut = nullptr;
    for (int i = 0; i < 12 && cut == nullptr; ++i) {
        const std::string name = "B" + std::to_string(i);
        const FloatRect rect = ui.rectOf(name);
        if (rect.top() < margin && rect.bottom() > margin + 2.f) {
            REQUIRE(pressed(ui.click({ rect.center().x, margin + 1.f })).empty());
            REQUIRE(pressed(ui.click({ rect.center().x, margin - 1.f })) == std::vector<std::string>{ name });
            cut = "found";
        }
    }
    REQUIRE(cut != nullptr);
}
