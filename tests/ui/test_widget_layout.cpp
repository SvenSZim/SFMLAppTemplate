#include "atpl/ui/error.hpp"

#include "ui/layout/arrange.hpp"
#include "ui/layout/panel_placement.hpp"
#include "ui/layout/widget_layout.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <memory>
#include <string>
#include <vector>

using namespace atpl;
using atpl::model::Store;
using Catch::Matchers::ContainsSubstring;

namespace {

// Round numbers, so the expected rectangles can be worked out by hand.
Sizes sizes() {
    Sizes result;
    result.margin = 10.f;
    result.padding = { 10.f, 10.f };
    result.gap = { 5.f, 5.f };
    result.headerHeight = 30.f;
    result.rowHeight = 20.f;
    result.panelWidth = 200.f;
    return result;
}

/// A widget of a fixed height. It never decides where it is: that is what is tested.
class Block final : public Widget {
public:
    Block(float height, bool stretch, float aspectRatio) :
        m_height(height),
        m_stretch(stretch),
        m_aspectRatio(aspectRatio) {}

    [[nodiscard]] SizeRequest measure(const MeasureContext& context) const override {
        const float height = m_aspectRatio > 0.f ? context.width() / m_aspectRatio : m_height;
        return { .height = height, .stretch = m_stretch };
    }
    void paint(Painter&, const Style&) const override {}

private:
    float m_height;
    bool m_stretch;
    float m_aspectRatio;
};

struct Item {
    std::string name;
    float height = 20.f;
    bool stretch = false;
    float aspectRatio = 0.f; ///< Above 0: as high as its width divided by this.

    [[nodiscard]] std::unique_ptr<Widget> create() const {
        return std::make_unique<Block>(height, stretch, aspectRatio);
    }
};

struct Region {
    static constexpr bool isView = true;
    std::string name;
    [[nodiscard]] std::unique_ptr<Widget> create() const { return std::make_unique<Block>(40.f, true, 0.f); }
};

/// A panel with these widgets, as it is when the UI has been built: cells found.
Store panelOf(std::vector<WidgetSetup> widgets, int columns = 1, int rows = 0) {
    UISetup setup;
    setup.panels = { { .name = "Panel", .columns = columns, .rows = rows, .widgets = std::move(widgets) } };
    Store store{ setup };
    layout::prepareWidgets(store);
    return store;
}

/// The same without the checks, for setups that are meant to be refused.
Store unchecked(std::vector<WidgetSetup> widgets, int columns = 1, int rows = 0) {
    UISetup setup;
    setup.panels = { { .name = "Panel", .columns = columns, .rows = rows, .widgets = std::move(widgets) } };
    return Store{ setup };
}

const PanelId panel{ 0 };
const Theme theme;

layout::WidgetLayout lay(Store& store, float width = 200.f, std::optional<float> available = std::nullopt) {
    return layout::layoutWidgets(store, panel, width, theme, sizes(), nullptr, available);
}

FloatRect rectOf(const Store& store, std::uint32_t widget) {
    return store.widget(WidgetId{ widget }).rect;
}

/// Text of a fixed width per character, so tests need no font.
class FixedWidthText final : public render::TextMeasurer {
public:
    [[nodiscard]] sf::Vector2f measure(std::string_view text, const sf::Font*, float size) const override {
        return { static_cast<float>(text.size()) * size * 0.5f, size };
    }
    [[nodiscard]] float wrappedHeight(std::string_view, const sf::Font*, float size, float width) const override {
        return width < 100.f ? size * 2.f : size;
    }
};

} // namespace

// ----- Packed: no widget has a position -----

TEST_CASE("widgets without positions are stacked in one column, in the order listed", "[ui][layout][widgets]") {
    Store store = panelOf({ Item{ "a", 20.f }, Item{ "b", 30.f }, Item{ "c", 20.f } });
    const auto result = lay(store);

    // 200 wide with a padding of 10: widgets are 180 wide, with a gap of 5 between them.
    REQUIRE(rectOf(store, 0) == FloatRect(10.f, 10.f, 180.f, 20.f));
    REQUIRE(rectOf(store, 1) == FloatRect(10.f, 35.f, 180.f, 30.f));
    REQUIRE(rectOf(store, 2) == FloatRect(10.f, 70.f, 180.f, 20.f));
    REQUIRE(result.contentHeight == 100.f); // padding above and below included
    REQUIRE(store.panel(panel).contentHeight == 100.f);
    REQUIRE_FALSE(result.usesSpareHeight);
}

TEST_CASE(
    "with several columns, widgets fill them one after the other with balanced heights", "[ui][layout][widgets]"
) {
    Store store = panelOf({ Item{ "a" }, Item{ "b" }, Item{ "c" }, Item{ "d" } }, 2);
    const auto result = lay(store);

    // Two columns of (200 - 20 - 5) / 2 = 87.5.
    REQUIRE(rectOf(store, 0) == FloatRect(10.f, 10.f, 87.5f, 20.f));
    REQUIRE(rectOf(store, 1) == FloatRect(10.f, 35.f, 87.5f, 20.f));
    REQUIRE(rectOf(store, 2) == FloatRect(102.5f, 10.f, 87.5f, 20.f));
    REQUIRE(rectOf(store, 3) == FloatRect(102.5f, 35.f, 87.5f, 20.f));
    REQUIRE(result.contentHeight == 65.f);
}

TEST_CASE("a widget is asked for its height at the width it gets", "[ui][layout][widgets]") {
    // As high as half its width.
    Store one = panelOf({ Item{ .name = "view", .aspectRatio = 2.f } });
    lay(one);
    REQUIRE(rectOf(one, 0) == FloatRect(10.f, 10.f, 180.f, 90.f));

    Store two = panelOf({ Item{ .name = "view", .aspectRatio = 2.f }, Item{ "other" } }, 2);
    lay(two);
    REQUIRE(rectOf(two, 0).width() == 87.5f);
    REQUIRE(rectOf(two, 0).height() == 44.f); // 43.75, rounded up to whole pixels

    lay(one, 400.f); // a wider panel
    REQUIRE(rectOf(one, 0) == FloatRect(10.f, 10.f, 380.f, 190.f));
}

TEST_CASE("a panel without widgets has no content", "[ui][layout][widgets]") {
    Store store = panelOf({});
    REQUIRE(lay(store).contentHeight == 0.f);
    REQUIRE(store.panel(panel).contentHeight == 0.f);
}

// ----- Grid: the panel has rows, or a widget has a position or a span -----

TEST_CASE("a panel with rows is a grid of equal cells, filled row by row", "[ui][layout][widgets]") {
    Store store = panelOf({ Item{ "a" }, Item{ "b" }, Item{ "c" } }, 2, 2);
    const auto result = lay(store);

    // Columns of 87.5, rows of one standard row (20), gaps of 5.
    REQUIRE(store.panel(panel).grid);
    REQUIRE(rectOf(store, 0) == FloatRect(10.f, 10.f, 87.5f, 20.f));
    REQUIRE(rectOf(store, 1) == FloatRect(102.5f, 10.f, 87.5f, 20.f));
    REQUIRE(rectOf(store, 2) == FloatRect(10.f, 35.f, 87.5f, 20.f));
    REQUIRE(result.contentHeight == 65.f); // 10 + 20 + 5 + 20 + 10
    REQUIRE(result.usesSpareHeight);
}

TEST_CASE("a panel that says nothing about rows, positions or spans is not a grid", "[ui][layout][widgets]") {
    const Store store = panelOf({ Item{ "a" }, Item{ "b" } }, 2);
    REQUIRE_FALSE(store.panel(panel).grid);
    REQUIRE_FALSE(store.widget(WidgetId{ 0 }).cell.has_value());
}

TEST_CASE("larger widgets are placed first, and each gets the size of its cells", "[ui][layout][widgets]") {
    // The example of setup.hpp.
    Store store = panelOf(
        { Item{ "Speed" },
          Item{ "Size" },
          spanning({ .columns = 2, .rows = 3 }, Item{ "Tick time", 50.f }),
          Item{ "Reset" } },
        2,
        5
    );
    const auto result = lay(store);

    REQUIRE(rectOf(store, 2) == FloatRect(10.f, 10.f, 180.f, 70.f));   // rows 0 to 2: three rows and two gaps
    REQUIRE(rectOf(store, 0) == FloatRect(10.f, 85.f, 87.5f, 20.f));   // 0, 3
    REQUIRE(rectOf(store, 1) == FloatRect(102.5f, 85.f, 87.5f, 20.f)); // 1, 3
    REQUIRE(rectOf(store, 3) == FloatRect(10.f, 110.f, 87.5f, 20.f));  // 0, 4
    REQUIRE(result.contentHeight == 140.f);
}

TEST_CASE("a widget with a position takes its cells", "[ui][layout][widgets]") {
    Store store = panelOf(
        {
            Item{ "Speed" },
            Item{ "Size" },
            at({ .row = 1, .columnSpan = 2, .rowSpan = 3 }, Item{ "Tick time", 50.f }),
            Item{ "Reset" },
        },
        2,
        5
    );
    lay(store);

    REQUIRE(rectOf(store, 0) == FloatRect(10.f, 10.f, 87.5f, 20.f));
    REQUIRE(rectOf(store, 1) == FloatRect(102.5f, 10.f, 87.5f, 20.f));
    REQUIRE(rectOf(store, 2) == FloatRect(10.f, 35.f, 180.f, 70.f)); // rows 1 to 3
    REQUIRE(rectOf(store, 3) == FloatRect(10.f, 110.f, 87.5f, 20.f));
}

TEST_CASE("in a grid, the height a widget asks for does not decide its row's", "[ui][layout][widgets]") {
    Store store = panelOf({ Item{ "high", 50.f }, Item{ "low", 10.f }, Item{ "next" } }, 2, 2);
    lay(store);

    REQUIRE(rectOf(store, 0) == FloatRect(10.f, 10.f, 87.5f, 20.f));   // wants 50, gets its cell
    REQUIRE(rectOf(store, 1) == FloatRect(102.5f, 15.f, 87.5f, 10.f)); // wants 10: keeps it, centred
    REQUIRE(rectOf(store, 2).top() == 35.f);                           // rows stay equal
}

TEST_CASE("a widget that takes several rows fills them", "[ui][layout][widgets]") {
    Store store = panelOf({ spanning({ .rows = 2 }, Item{ "tall", 20.f }), Item{ "upper" }, Item{ "lower" } }, 2, 2);
    lay(store);

    REQUIRE(rectOf(store, 0) == FloatRect(10.f, 10.f, 87.5f, 45.f)); // although it asked for 20
    REQUIRE(rectOf(store, 1) == FloatRect(102.5f, 10.f, 87.5f, 20.f));
    REQUIRE(rectOf(store, 2) == FloatRect(102.5f, 35.f, 87.5f, 20.f));
}

TEST_CASE("a row nobody uses stays empty, as a separator", "[ui][layout][widgets]") {
    Store store = panelOf({ at({ .row = 0 }, Item{ "above" }), at({ .row = 2 }, Item{ "below" }) });
    const auto result = lay(store);

    REQUIRE(store.panel(panel).rows == 3); // as many as the positions use
    REQUIRE(rectOf(store, 0).top() == 10.f);
    REQUIRE(rectOf(store, 1).top() == 60.f); // one row and two gaps further down
    REQUIRE(result.contentHeight == 90.f);
}

TEST_CASE("the rows of a grid share the height the panel has to spare", "[ui][layout][widgets]") {
    Store store = panelOf({ Item{ "view", 40.f, true }, Item{ "slider", 25.f } }, 1, 2);
    REQUIRE(lay(store).contentHeight == 65.f);

    // 295 high: two rows of 135 instead of 20.
    const auto result = lay(store, 200.f, 295.f);
    REQUIRE(result.contentHeight == 295.f);
    REQUIRE(rectOf(store, 0) == FloatRect(10.f, 10.f, 180.f, 135.f)); // stretches: fills its row
    REQUIRE(rectOf(store, 1) == FloatRect(10.f, 205.f, 180.f, 25.f)); // centred in its row of 135

    // Less height than the rows need changes nothing: the panel scrolls.
    REQUIRE(lay(store, 200.f, 40.f).contentHeight == 65.f);
    REQUIRE(rectOf(store, 0).height() == 20.f);
}

// ----- Stretching -----

TEST_CASE("without height to spare, a widget that stretches gets the least it needs", "[ui][layout][widgets]") {
    Store store = panelOf({ Item{ "above" }, Item{ "view", 40.f, true }, Item{ "below" } });
    const auto result = lay(store);

    REQUIRE(result.usesSpareHeight);
    REQUIRE(result.contentHeight == 110.f);
    REQUIRE(rectOf(store, 1) == FloatRect(10.f, 35.f, 180.f, 40.f));

    // The same if the panel is lower than its content: it scrolls instead.
    REQUIRE(lay(store, 200.f, 80.f).contentHeight == 110.f);
    REQUIRE(rectOf(store, 1).height() == 40.f);
}

TEST_CASE("a packed widget that stretches takes the height that is left in its column", "[ui][layout][widgets]") {
    Store store = panelOf({ Item{ "above" }, Item{ "view", 40.f, true }, Item{ "below" } });
    const auto result = lay(store, 200.f, 200.f);

    REQUIRE(rectOf(store, 0) == FloatRect(10.f, 10.f, 180.f, 20.f));
    REQUIRE(rectOf(store, 1) == FloatRect(10.f, 35.f, 180.f, 130.f)); // 90 more than it asked for
    REQUIRE(rectOf(store, 2) == FloatRect(10.f, 170.f, 180.f, 20.f)); // moved down
    REQUIRE(rectOf(store, 2).bottom() == 190.f);                      // a padding above the end
    REQUIRE(result.contentHeight == 200.f);
}

// ----- What can never be laid out -----

TEST_CASE("widget layouts that can never work are refused", "[ui][layout][widgets]") {
    const auto refuses = [](Store store, const std::string& message) {
        REQUIRE_THROWS_AS(layout::prepareWidgets(store), SetupError);
        REQUIRE_THROWS_WITH(layout::prepareWidgets(store), ContainsSubstring(message));
    };

    SECTION("no columns") {
        refuses(unchecked({ Item{ "a" } }, 0), "panel \"Panel\" has 0 columns");
    }
    SECTION("too many columns") {
        refuses(unchecked({ Item{ "a" } }, 4), "a panel can have 1 to 3");
    }
    SECTION("a negative number of rows") {
        refuses(unchecked({ Item{ "a" } }, 1, -1), "negative number of rows");
    }
    SECTION("a cell outside the panel's columns") {
        refuses(
            unchecked({ at({ .column = 2 }, Item{ "Speed" }) }, 2),
            "widget \"Panel/Speed\" reaches outside its panel's grid"
        );
    }
    SECTION("a cell below the panel's rows") {
        refuses(unchecked({ at({ .row = 3 }, Item{ "Speed" }) }, 2, 3), "grid of 2 columns and 3 rows");
    }
    SECTION("a span wider than the panel") {
        refuses(unchecked({ spanning({ .columns = 3 }, Item{ "Graph" }) }, 2, 4), "reaches outside");
    }
    SECTION("a span of nothing") {
        refuses(unchecked({ at({ .rowSpan = 0 }, Item{ "Speed" }) }), "spans less than one cell");
    }
    SECTION("two widgets with a position on the same cell") {
        refuses(
            unchecked(
                { at({ .column = 0, .row = 0, .columnSpan = 2 }, Item{ "Graph" }),
                  at({ .column = 1, .row = 0 }, Item{ "Speed" }) },
                2
            ),
            "widget \"Panel/Speed\" is placed on a cell another widget already has"
        );
    }
    SECTION("more widgets than cells") {
        refuses(unchecked({ Item{ "a" }, Item{ "b" }, Item{ "c" } }, 2, 1), "widget \"Panel/c\" finds no room");
    }
    SECTION("a span, but no rows to put it in") {
        // Five rows high, without a position, in a panel that does not say how many rows it has.
        refuses(
            unchecked({ Item{ "Speed" }, spanning({ .rows = 5 }, Item{ "Graph" }) }),
            "widget \"Panel/Graph\" finds no room"
        );
        refuses(unchecked({ spanning({ .rows = 5 }, Item{ "Graph" }) }), "Give the panel more rows");
    }
    SECTION("the same with enough rows is fine") {
        Store store = unchecked({ Item{ "Speed" }, spanning({ .rows = 5 }, Item{ "Graph" }) }, 1, 6);
        REQUIRE_NOTHROW(layout::prepareWidgets(store));
    }
}

// ----- What a widget may know while it is measured -----

TEST_CASE(
    "a widget being measured can ask for its width, the theme's sizes and the size of text", "[ui][layout][widgets]"
) {
    const Sizes available = sizes();
    const FixedWidthText measurer;
    const Part label{ Kind{ "test" }, "label", Role::Text };
    const float textSize = theme.resolve(label).textSize;

    const MeasureContext context(180.f, theme, {}, available, &measurer);
    REQUIRE(context.width() == 180.f);
    REQUIRE(context.sizes().padding == sf::Vector2f(10.f, 10.f));
    REQUIRE(context.textSize("abcd", label) == sf::Vector2f(2.f * textSize, textSize)); // in the part's text size
    REQUIRE(context.wrappedTextHeight("some text", label, 50.f) == 2.f * textSize);
    REQUIRE(context.wrappedTextHeight("some text", label, 150.f) == textSize);

    // Without anything to measure text with, text has no size.
    const MeasureContext blind(180.f, theme, {}, available);
    REQUIRE(blind.textSize("abcd", label) == sf::Vector2f());
    REQUIRE(blind.wrappedTextHeight("some text", label, 50.f) == 0.f);
}

TEST_CASE("padding and gaps can differ horizontally and vertically", "[ui][layout][widgets]") {
    // What scaling with the window does: wide spacing across, narrow spacing down.
    Sizes wide = sizes();
    wide.padding = { 20.f, 10.f };
    wide.gap = { 10.f, 5.f };

    Store packed = panelOf({ Item{ "a" }, Item{ "b" }, Item{ "c" }, Item{ "d" } }, 2);
    layout::layoutWidgets(packed, panel, 200.f, theme, wide);
    // Two columns of (200 - 40 - 10) / 2 = 75, 10 apart; rows 5 apart.
    REQUIRE(rectOf(packed, 0) == FloatRect(20.f, 10.f, 75.f, 20.f));
    REQUIRE(rectOf(packed, 1) == FloatRect(20.f, 35.f, 75.f, 20.f));
    REQUIRE(rectOf(packed, 2) == FloatRect(105.f, 10.f, 75.f, 20.f));

    Store grid = panelOf({ Item{ "a" }, Item{ "b" }, Item{ "c" } }, 2, 2);
    const auto result = layout::layoutWidgets(grid, panel, 200.f, theme, wide);
    REQUIRE(rectOf(grid, 1) == FloatRect(105.f, 10.f, 75.f, 20.f));
    REQUIRE(rectOf(grid, 2) == FloatRect(20.f, 35.f, 75.f, 20.f));
    REQUIRE(result.contentHeight == 65.f);
}

// ----- Everything together -----

TEST_CASE("a floating panel is as high as its header and its widgets", "[ui][layout]") {
    UISetup setup;
    setup.panels = {
        { .name = "Controls", .placement = Anchor::TopLeft, .widgets = { Item{ "a", 20.f }, Item{ "b", 30.f } } },
        { .name = "Below", .placement = Anchor::TopLeft, .widgets = { Item{ "c", 20.f } } },
    };
    Store store{ setup };
    layout::prepare(store, {});
    layout::arrange(store, { 800.f, 600.f }, {}, theme, sizes());

    // Content: 10 + 20 + 5 + 30 + 10 = 75, below a header of 30.
    REQUIRE(store.panel(PanelId{ 0 }).rect == FloatRect(10.f, 10.f, 200.f, 105.f));
    REQUIRE(store.panel(PanelId{ 1 }).rect == FloatRect(10.f, 125.f, 200.f, 70.f));
    REQUIRE(rectOf(store, 1) == FloatRect(10.f, 35.f, 180.f, 30.f));
    REQUIRE(layout::contentOverflow(store.panel(PanelId{ 0 }), sizes()) == 0.f);
}

TEST_CASE("a view in a grid panel takes the height the other widgets leave", "[ui][layout]") {
    UISetup setup;
    setup.panels = {
        { .name = "Scene", .placement = GridCell{}, .widgets = { Item{ "toolbar", 20.f }, Region{ "world" } } }
    };
    Store store{ setup };
    layout::prepare(store, {});
    layout::arrange(store, { 800.f, 600.f }, {}, theme, sizes());

    // The panel fills the window inside the margins: 780 x 580, of which 550 are content.
    REQUIRE(store.panel(panel).rect == FloatRect(10.f, 10.f, 780.f, 580.f));
    REQUIRE(rectOf(store, 0) == FloatRect(10.f, 10.f, 760.f, 20.f));
    REQUIRE(rectOf(store, 1) == FloatRect(10.f, 35.f, 760.f, 505.f)); // down to a padding above the end

    // The view is its widget's place in the window: panel at (10, 10), content below the header.
    REQUIRE(store.view(ViewId{ 0 }).rect == FloatRect(20.f, 75.f, 760.f, 505.f));

    // A larger window gives the view more room, and nothing else.
    layout::arrange(store, { 1000.f, 700.f }, {}, theme, sizes());
    REQUIRE(rectOf(store, 0) == FloatRect(10.f, 10.f, 960.f, 20.f));
    REQUIRE(rectOf(store, 1) == FloatRect(10.f, 35.f, 960.f, 605.f));
}

TEST_CASE("a panel that cannot be as high as its content says how far it has to scroll", "[ui][layout]") {
    UISetup setup;
    setup.panels = {
        { .name = "Long", .placement = Anchor::TopLeft, .widgets = { Item{ "a", 300.f }, Item{ "b", 300.f } } }
    };
    Store store{ setup };
    layout::prepare(store, {});

    // Content: 10 + 300 + 5 + 300 + 10 = 625. In a window of 400 the panel is 380 high: 350 for
    // content.
    layout::arrange(store, { 800.f, 400.f }, {}, theme, sizes());
    REQUIRE(store.panel(panel).rect.height() == 380.f);
    REQUIRE(store.panel(panel).contentHeight == 625.f);
    REQUIRE(layout::contentOverflow(store.panel(panel), sizes()) == 275.f);

    layout::arrange(store, { 800.f, 1000.f }, {}, theme, sizes());
    REQUIRE(layout::contentOverflow(store.panel(panel), sizes()) == 0.f);
}

TEST_CASE("the widgets of a panel that is collapsed or not shown are not visible", "[ui][layout]") {
    UISetup setup;
    setup.panels = { { .name = "Controls", .placement = Anchor::TopLeft, .widgets = { Item{ "a" } } } };
    Store store{ setup };
    layout::prepare(store, {});
    const auto visible = [&] {
        layout::arrange(store, { 800.f, 600.f }, {}, theme, sizes());
        return store.widget(WidgetId{ 0 }).visible;
    };
    REQUIRE(visible());

    store.panel(panel).collapsed = true;
    REQUIRE_FALSE(visible());
    REQUIRE(store.panel(panel).rect.height() == 30.f); // its header
    store.panel(panel).collapsed = false;

    store.panel(panel).visible = false;
    REQUIRE_FALSE(visible());
    store.panel(panel).visible = true;
    REQUIRE(visible());
}

TEST_CASE("a setup is checked for panels and for widgets", "[ui][layout]") {
    const auto prepare = [](UISetup setup) {
        Store store{ setup };
        layout::prepare(store, {});
    };
    UISetup setup;

    setup.panels = { { .name = "Controls", .columns = 5 } };
    REQUIRE_THROWS_AS(prepare(setup), SetupError);

    setup.panels = { { .name = "Map", .placement = GridCell{ .column = 1 } } };
    REQUIRE_THROWS_AS(prepare(setup), SetupError);

    setup.panels = { { .name = "Fine", .columns = 3 } };
    REQUIRE_NOTHROW(prepare(setup));
}
