#include "atpl/ui/layout.hpp"
#include "atpl/ui/setup.hpp"

#include "ui/layout/arrange.hpp"
#include "ui/model/store.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <variant>

using namespace atpl;
using Catch::Approx;

namespace {

const sf::Vector2f reference(1280.f, 720.f);

} // namespace

// ----- Sizes -----

TEST_CASE("at the reference window size, sizes are the layout theme's metrics", "[ui][layout][theme]") {
    const Layout layout;
    const Sizes sizes = layout.sizesAt(reference);

    REQUIRE(sizes.scale == sf::Vector2f(1.f, 1.f));
    REQUIRE(sizes.text == 1.f);
    REQUIRE(sizes.margin == layout.metrics.margin);
    REQUIRE(sizes.padding == sf::Vector2f(layout.metrics.padding, layout.metrics.padding));
    REQUIRE(sizes.gap == sf::Vector2f(layout.metrics.gap, layout.metrics.gap));
    REQUIRE(sizes.rowHeight == layout.metrics.rowHeight);
    REQUIRE(sizes.headerHeight == layout.metrics.headerHeight);
    REQUIRE(sizes.panelWidth == layout.metrics.panelWidth);
    REQUIRE(sizes.scrollbarWidth == layout.metrics.scrollbarWidth);
}

TEST_CASE("widths follow the window's width and heights its height", "[ui][layout][theme]") {
    Layout layout;
    layout.metrics = {
        .margin = 20.f, .padding = 10.f, .gap = 10.f, .rowHeight = 30.f, .panelWidth = 300.f, .headerHeight = 40.f
    };

    // A quarter wider and a tenth lower than the reference size.
    const Sizes sizes = layout.sizesAt({ 1600.f, 648.f });
    REQUIRE(sizes.scale.x == Approx(1.25f));
    REQUIRE(sizes.scale.y == Approx(0.9f));

    REQUIRE(sizes.panelWidth == 375.f);                // a width
    REQUIRE(sizes.rowHeight == 27.f);                  // a height
    REQUIRE(sizes.headerHeight == 36.f);               // a height
    REQUIRE(sizes.margin == 18.f);                     // the same on all sides: it follows the smaller factor
    REQUIRE(sizes.padding == sf::Vector2f(13.f, 9.f)); // 12.5 rounds to 13
    REQUIRE(sizes.gap == sf::Vector2f(13.f, 9.f));
}

TEST_CASE("no factor leaves the layout theme's limits", "[ui][layout][theme]") {
    Layout layout;
    layout.scaling.lowest = 0.5f;
    layout.scaling.highest = 2.f;

    REQUIRE(layout.sizesAt({ 100.f, 100.f }).scale == sf::Vector2f(0.5f, 0.5f));   // tiny window
    REQUIRE(layout.sizesAt({ 10000.f, 10000.f }).scale == sf::Vector2f(2.f, 2.f)); // huge window
    REQUIRE(layout.sizesAt({ 100.f, 10000.f }).scale == sf::Vector2f(0.5f, 2.f));  // each axis by itself
    REQUIRE(layout.sizesAt({ 0.f, 0.f }).scale == sf::Vector2f(0.5f, 0.5f));       // a minimised window

    // Limits the wrong way round are taken the right way round.
    layout.scaling.lowest = 2.f;
    layout.scaling.highest = 0.5f;
    REQUIRE(layout.sizesAt({ 100.f, 100.f }).scale == sf::Vector2f(0.5f, 0.5f));
}

TEST_CASE("text follows the smaller side of the window, as strongly as the layout theme says", "[ui][layout][theme]") {
    Layout layout;
    layout.scaling.lowest = 0.5f;
    layout.scaling.highest = 2.f;

    // Twice as wide, half again as high: the smaller factor is 1.5.
    const sf::Vector2f window(2560.f, 1080.f);

    layout.scaling.fontStrength = 1.f;
    REQUIRE(layout.sizesAt(window).text == Approx(1.5f));
    layout.scaling.fontStrength = 0.5f;
    REQUIRE(layout.sizesAt(window).text == Approx(1.25f));
    layout.scaling.fontStrength = 0.f;
    REQUIRE(layout.sizesAt(window).text == 1.f); // text never changes

    // The same when the window shrinks.
    layout.scaling.fontStrength = 0.5f;
    REQUIRE(layout.sizesAt({ 640.f, 720.f }).text == Approx(0.75f));
}

TEST_CASE("with both limits at 1, nothing follows the window", "[ui][layout][theme]") {
    Layout layout;
    layout.scaling.lowest = 1.f;
    layout.scaling.highest = 1.f;

    REQUIRE(layout.sizesAt({ 320.f, 200.f }) == layout.sizesAt(reference));
    REQUIRE(layout.sizesAt({ 3840.f, 2160.f }) == layout.sizesAt(reference));
}

TEST_CASE("the GUI scale applies on top of the scaling with the window", "[ui][layout][theme]") {
    Layout layout;
    layout.metrics.scale = 2.f;

    const Sizes atReference = layout.sizesAt(reference);
    REQUIRE(atReference.scale == sf::Vector2f(2.f, 2.f));
    REQUIRE(atReference.text == 2.f);
    REQUIRE(atReference.rowHeight == layout.metrics.rowHeight * 2.f);

    const Sizes small = layout.sizesAt({ 960.f, 540.f }); // three quarters of the reference size
    REQUIRE(small.scale == sf::Vector2f(1.5f, 1.5f));
    REQUIRE(small.text == Approx(2.f * 0.875f)); // half as strongly as the window
}

TEST_CASE("sizes are whole pixels", "[ui][layout][theme]") {
    const Layout layout;
    for (const sf::Vector2f window :
         { sf::Vector2f(1000.f, 613.f), sf::Vector2f(1437.f, 911.f), sf::Vector2f(1919.f, 1079.f) }) {
        const Sizes sizes = layout.sizesAt(window);
        for (const float value : { sizes.margin,
                                   sizes.padding.x,
                                   sizes.padding.y,
                                   sizes.gap.x,
                                   sizes.gap.y,
                                   sizes.rowHeight,
                                   sizes.headerHeight,
                                   sizes.panelWidth }) {
            REQUIRE(value == std::round(value));
        }
    }
}

TEST_CASE("text sizes are whole pixels at every scale", "[ui][layout][theme]") {
    Theme theme;
    const Part label{ Kind{ "test" }, "label", Role::Text };
    for (const float scale : { 0.8f, 0.93f, 1.f, 1.17f, 1.4f }) {
        const float size = theme.resolve(label, State::Normal, {}, scale).textSize;
        REQUIRE(size == std::round(size));
        REQUIRE(size == std::round(theme.typography.text.size * scale));
    }
}

// ----- Presets -----

TEST_CASE("the ready-made layout themes differ in what they describe", "[ui][layout][theme]") {
    const Layout overlay = layouts::overlay();
    REQUIRE(std::holds_alternative<Anchor>(overlay.placement));
    REQUIRE(overlay.collapsible);
    REQUIRE(overlay.width == SizeRule::Equal);
    REQUIRE(overlay.height == SizeRule::Own);
    REQUIRE(overlay.rows == SizeRule::Own);

    const Layout dashboard = layouts::dashboard();
    REQUIRE(std::holds_alternative<GridSpan>(dashboard.placement));
    REQUIRE(dashboard.fit == Fit::Fill);
    REQUIRE_FALSE(dashboard.collapsible);

    const Layout cards = layouts::cards();
    REQUIRE(std::holds_alternative<GridSpan>(cards.placement));
    REQUIRE(cards.fit == Fit::Content);
    REQUIRE(cards.alignment == Alignment::Center);
    REQUIRE(cards.rows == SizeRule::Equal);

    const Layout compact = layouts::compact();
    REQUIRE(compact.metrics.rowHeight < overlay.metrics.rowHeight);
    REQUIRE(compact.metrics.panelWidth < overlay.metrics.panelWidth);
    REQUIRE(compact.limit.pixels.x < overlay.limit.pixels.x);

    // The default layout theme is the overlay.
    REQUIRE(Layout().sizesAt(reference) == overlay.sizesAt(reference));
}

// ----- What a panel takes from the layout theme, and what it says itself -----

TEST_CASE("a panel that says nothing gets the layout theme's placement and collapsing", "[ui][layout][theme]") {
    UISetup setup;
    setup.layout = layouts::dashboard(); // somewhere in the grid, not collapsible
    setup.panels = {
        { .name = "default" },
        { .name = "own place", .placement = Anchor::Bottom },
        { .name = "own choice", .collapsible = true },
    };
    const model::Store store{ setup };

    REQUIRE(std::holds_alternative<GridSpan>(store.panel(PanelId{ 0 }).placement));
    REQUIRE_FALSE(store.panel(PanelId{ 0 }).collapsible);

    REQUIRE(std::get<Anchor>(store.panel(PanelId{ 1 }).placement) == Anchor::Bottom);
    REQUIRE_FALSE(store.panel(PanelId{ 1 }).collapsible);

    REQUIRE(std::holds_alternative<GridSpan>(store.panel(PanelId{ 2 }).placement));
    REQUIRE(store.panel(PanelId{ 2 }).collapsible);
}

TEST_CASE("another layout theme changes only what the panels left open", "[ui][layout][theme]") {
    UISetup setup; // the overlay: floating at the top left, collapsible
    setup.grid = { .columns = 2, .rows = 1 };
    setup.panels = { { .name = "default" }, { .name = "own place", .placement = Anchor::Bottom, .collapsible = true } };
    model::Store store{ setup };
    REQUIRE(std::get<Anchor>(store.panel(PanelId{ 0 }).placement) == Anchor::TopLeft);
    REQUIRE(store.panel(PanelId{ 0 }).collapsible);

    store.applyLayout(layouts::dashboard());
    layout::prepare(store, setup.grid); // grid cells are found anew
    REQUIRE(std::holds_alternative<GridCell>(store.panel(PanelId{ 0 }).placement));
    REQUIRE_FALSE(store.panel(PanelId{ 0 }).collapsible);
    REQUIRE(std::get<Anchor>(store.panel(PanelId{ 1 }).placement) == Anchor::Bottom);
    REQUIRE(store.panel(PanelId{ 1 }).collapsible);

    // And back.
    store.applyLayout(layouts::overlay());
    layout::prepare(store, setup.grid);
    REQUIRE(std::get<Anchor>(store.panel(PanelId{ 0 }).placement) == Anchor::TopLeft);
}

TEST_CASE("a panel keeps what it does differently from the layout theme", "[ui][layout][theme]") {
    UISetup setup;
    setup.panels = {
        { .name = "Legend", .layout = { .fit = Fit::Content, .alignment = Alignment::BottomRight } },
        { .name = "Plain" },
    };
    const model::Store store{ setup };

    const PanelLayout& legend = store.panel(PanelId{ 0 }).layout;
    REQUIRE(legend.fit == Fit::Content);
    REQUIRE(legend.alignment == Alignment::BottomRight);
    REQUIRE_FALSE(legend.rows.has_value()); // not named: the layout theme's

    const PanelLayout& plain = store.panel(PanelId{ 1 }).layout;
    REQUIRE_FALSE(plain.fit.has_value());
    REQUIRE_FALSE(plain.alignment.has_value());
}
