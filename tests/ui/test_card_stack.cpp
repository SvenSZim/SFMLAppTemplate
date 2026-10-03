#include "ui/input/input_system.hpp"
#include "ui/layout/panel_placement.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <string>
#include <vector>

using namespace atpl;
using atpl::model::Store;

namespace {

// Round numbers, so the expected rectangles can be worked out by hand.
Sizes sizes() {
    Sizes result;
    result.margin = 10.f;
    result.headerHeight = 30.f;
    result.panelWidth = 200.f;
    return result;
}

constexpr float strip = 22.f;

Layout cards() {
    Layout layout;
    layout.stackOverflow = StackOverflow::Cards;
    return layout;
}

/// `count` panels at `anchor`, all collapsed except those listed in `open`, each 100 high when
/// open (30 header and 70 content).
Store stackOf(int count, Anchor anchor, std::vector<int> open = {}) {
    UISetup setup;
    for (int i = 0; i < count; ++i) {
        const bool isOpen = std::find(open.begin(), open.end(), i) != open.end();
        setup.panels.push_back({ .name = "P" + std::to_string(i), .placement = anchor, .collapsed = !isOpen });
    }
    Store store{ setup };
    for (model::Panel& panel : store.panels()) {
        panel.contentHeight = 70.f;
    }
    return store;
}

void place(Store& store, sf::Vector2f window, const Layout& layout = cards()) {
    layout::placePanels(store, window, {}, sizes(), layout, strip);
}

const model::Panel& panel(const Store& store, std::uint32_t index) {
    return store.panel(PanelId{ index });
}

std::vector<std::uint32_t> indices(const std::vector<PanelId>& order) {
    std::vector<std::uint32_t> result;
    for (const PanelId id : order) {
        result.push_back(id.index);
    }
    return result;
}

} // namespace

TEST_CASE("a stack of cards shows all its panels where hiding would leave some out", "[ui][layout][cards]") {
    // 180 of height: four headers with their gaps fit, eight do not.
    Store hidden = stackOf(8, Anchor::TopLeft);
    place(hidden, { 800.f, 200.f }, Layout());
    REQUIRE(panel(hidden, 3).shown);
    REQUIRE_FALSE(panel(hidden, 4).shown);
    REQUIRE_FALSE(panel(hidden, 0).overlapped);

    // As cards, seven fit: six strips of at least 22 and one whole header.
    Store stack = stackOf(8, Anchor::TopLeft);
    place(stack, { 800.f, 200.f });
    for (std::uint32_t i = 0; i < 7; ++i) {
        INFO("panel " << i);
        REQUIRE(panel(stack, i).shown);
        REQUIRE(panel(stack, i).overlapped);
        REQUIRE(panel(stack, i).rect.height() == 30.f);                             // each keeps its whole header ...
        REQUIRE(panel(stack, i).rect.top() == 10.f + 25.f * static_cast<float>(i)); // ... the next covers it to 25
        REQUIRE(panel(stack, i).cardLayer == static_cast<int>(i));
    }
    REQUIRE_FALSE(panel(stack, 7).shown); // not even its strip fits
}

TEST_CASE("cards overlap only as far as needed", "[ui][layout][cards]") {
    // 230 of height for three headers and their gaps (110): no need to overlap.
    Store roomy = stackOf(3, Anchor::TopLeft);
    place(roomy, { 800.f, 250.f });
    REQUIRE_FALSE(panel(roomy, 0).overlapped);
    REQUIRE(panel(roomy, 1).rect.top() == 50.f); // a header and a gap below the first

    // Five headers in 130: covered by 3 each, keeping 37 of 40 in view.
    Store tight = stackOf(5, Anchor::TopLeft);
    place(tight, { 800.f, 150.f });
    REQUIRE(panel(tight, 0).overlapped);
    REQUIRE(panel(tight, 1).rect.top() - panel(tight, 0).rect.top() == 25.f);
    REQUIRE(panel(tight, 4).rect.bottom() <= 140.f);
}

TEST_CASE("an open card keeps its height, and the collapsed ones make room", "[ui][layout][cards]") {
    // 230 of height: four headers, one open panel of 100, and gaps (260) do not fit.
    Store stack = stackOf(5, Anchor::TopLeft, { 2 });
    place(stack, { 800.f, 250.f });
    REQUIRE(panel(stack, 2).rect.height() == 100.f);
    REQUIRE(panel(stack, 0).rect.top() == 10.f);
    REQUIRE(panel(stack, 1).rect.top() == 40.f); // the two above it covered down to 30
    REQUIRE(panel(stack, 2).rect.top() == 70.f);
    REQUIRE(panel(stack, 3).rect.top() == 180.f); // below the open one: its height and a gap
    REQUIRE(panel(stack, 4).rect.top() == 210.f);
    REQUIRE(panel(stack, 4).rect.bottom() <= 240.f);
}

TEST_CASE("open cards that are too high share what the strips leave", "[ui][layout][cards]") {
    Store stack = stackOf(4, Anchor::TopLeft, { 1, 3 });
    place(stack, { 800.f, 200.f }); // 180: two strips, a gap, and the two open ones share 114
    REQUIRE(panel(stack, 1).rect.height() < 100.f);
    REQUIRE(panel(stack, 1).rect.height() >= 30.f);
    REQUIRE(panel(stack, 3).rect.bottom() <= 190.f);
    REQUIRE(panel(stack, 1).rect.top() - panel(stack, 0).rect.top() == strip);
}

TEST_CASE("a stack of cards at the bottom grows upwards, the first card in front", "[ui][layout][cards]") {
    Store stack = stackOf(4, Anchor::BottomLeft);
    place(stack, { 800.f, 120.f });                  // 100: three strips of 23 and a header
    REQUIRE(panel(stack, 0).rect.bottom() == 110.f); // the first panel is the lowest
    REQUIRE(panel(stack, 1).rect.top() == panel(stack, 0).rect.top() - 23.f);
    REQUIRE(panel(stack, 3).rect.top() >= 10.f);

    // Each card covers the lower part of the one above it, whose top stays in view.
    const std::vector<PanelId> base{ PanelId{ 0 }, PanelId{ 1 }, PanelId{ 2 }, PanelId{ 3 } };
    REQUIRE(indices(layout::cardOrder(stack, base, std::nullopt)) == std::vector<std::uint32_t>{ 3, 2, 1, 0 });
}

TEST_CASE("the card under the pointer comes to the front", "[ui][layout][cards]") {
    Store stack = stackOf(4, Anchor::TopLeft);
    place(stack, { 800.f, 120.f });
    const std::vector<PanelId> base{ PanelId{ 0 }, PanelId{ 1 }, PanelId{ 2 }, PanelId{ 3 } };
    REQUIRE(indices(layout::cardOrder(stack, base, std::nullopt)) == std::vector<std::uint32_t>{ 0, 1, 2, 3 });
    REQUIRE(indices(layout::cardOrder(stack, base, PanelId{ 1 })) == std::vector<std::uint32_t>{ 0, 2, 3, 1 });

    // A stack that does not overlap keeps its order.
    Store roomy = stackOf(4, Anchor::TopLeft);
    place(roomy, { 800.f, 600.f });
    REQUIRE(indices(layout::cardOrder(roomy, base, PanelId{ 1 })) == std::vector<std::uint32_t>{ 0, 1, 2, 3 });
}

TEST_CASE("the pointer finds the card it sees: its strip, and all of it once it is in front", "[ui][input][cards]") {
    Store stack = stackOf(4, Anchor::TopLeft);
    place(stack, { 800.f, 120.f });
    const std::vector<PanelId> base{ PanelId{ 0 }, PanelId{ 1 }, PanelId{ 2 }, PanelId{ 3 } };
    const Sizes look = sizes();
    input::InputSystem input;
    std::vector<Event> events;
    std::vector<PanelId> order = layout::cardOrder(stack, base, std::nullopt);

    const FloatRect second = panel(stack, 1).rect;
    const float stepDown = panel(stack, 2).rect.top() - second.top();
    input.handle(sf::Event::MouseMoved{ { 100, static_cast<int>(second.top() + 5.f) } }, stack, order, look, events);
    REQUIRE(input.hoveredPanel() == PanelId{ 1 }); // its strip

    // Lower down its header is covered by the next card, until it comes to the front.
    const int covered = static_cast<int>(second.top() + stepDown + 3.f);
    input.handle(sf::Event::MouseMoved{ { 100, covered } }, stack, order, look, events);
    REQUIRE(input.hoveredPanel() == PanelId{ 2 });
    order = layout::cardOrder(stack, base, PanelId{ 1 });
    input.handle(sf::Event::MouseMoved{ { 100, covered } }, stack, order, look, events);
    REQUIRE(input.hoveredPanel() == PanelId{ 1 });
}

TEST_CASE("a strip keeps the title readable", "[ui][layout][cards]") {
    REQUIRE(layout::cardStrip(sizes(), 16.f) == 25.f); // the middle of the header, half the text, and a little
    REQUIRE(layout::cardStrip(sizes(), 40.f) == 30.f); // never more than the header
}

TEST_CASE("unfolding a card folds the others if the stack would not fit with them", "[ui][layout][cards]") {
    Store stack = stackOf(3, Anchor::TopLeft, { 0 });
    Sizes look = sizes();
    // 180 of height: two open panels (200) do not fit with a header.
    REQUIRE(
        layout::cardsToFold(stack, PanelId{ 1 }, { 800.f, 200.f }, look, cards()) ==
        std::vector<PanelId>{ PanelId{ 0 } }
    );
    // With room for both, nothing folds.
    REQUIRE(layout::cardsToFold(stack, PanelId{ 1 }, { 800.f, 600.f }, look, cards()).empty());
    // Without cards, nothing folds either: the stack shares its height as before.
    REQUIRE(layout::cardsToFold(stack, PanelId{ 1 }, { 800.f, 200.f }, look, Layout()).empty());
}
