#include "atpl/ui/error.hpp"

#include "ui/layout/arrange.hpp"
#include "ui/layout/panel_placement.hpp"
#include "ui/layout/widget_layout.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <memory>
#include <optional>
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

/// A widget that only says how large it can be. It never decides where it is: that is what is
/// tested.
class Block final : public Widget {
public:
    Block(SizeRequest request, float aspectRatio) :
        m_request(request),
        m_aspectRatio(aspectRatio) {}

    [[nodiscard]] SizeRequest measure(const MeasureContext& context) const override {
        SizeRequest request = m_request;
        if (m_aspectRatio > 0.f) {
            request.min.y = context.width() / m_aspectRatio; // its height follows from its width
        }
        return request;
    }
    void paint(Painter&, const Style&) const override {}

private:
    SizeRequest m_request;
    float m_aspectRatio;
};

struct Item {
    std::string name;
    SizeRequest request = { .min = { 0.f, 20.f } };
    float aspectRatio = 0.f; ///< Above 0: at least as high as its width divided by this.

    [[nodiscard]] std::unique_ptr<Widget> create() const { return std::make_unique<Block>(request, aspectRatio); }
};

/// A widget that fills what it is given, and needs at least this height.
Item dynamic(std::string name, float height = 20.f, float width = 0.f) {
    return { .name = std::move(name), .request = { .min = { width, height } } };
}

/// A widget that would like `preferred` height, can do with `least`, and grows to `most`.
Item flexible(std::string name, float least, float preferred, float most) {
    return { .name = std::move(name),
             .request = {
                 .min = { 0.f, least }, .preferred = { 0.f, preferred }, .max = sf::Vector2f(100000.f, most) } };
}

/// A widget that is never higher than `height`, and as wide as it gets unless `maxWidth` says less.
Item constant(std::string name, float height = 20.f, float maxWidth = 100000.f) {
    return { .name = std::move(name), .request = { .min = { 0.f, height }, .max = sf::Vector2f(maxWidth, height) } };
}

struct Region {
    static constexpr bool isView = true;
    std::string name;
    [[nodiscard]] std::unique_ptr<Widget> create() const {
        return std::make_unique<Block>(SizeRequest{ .min = { 0.f, 40.f } }, 0.f);
    }
};

/// A floating panel with these widgets, as it is when the UI has been built: cells found.
Store panelOf(std::vector<WidgetSetup> widgets, int columns = 1, int rows = 0, const Layout& layout = {}) {
    UISetup setup;
    setup.panels = { { .name = "Panel", .columns = columns, .rows = rows, .widgets = std::move(widgets) } };
    Store store{ setup };
    layout::prepareWidgets(store, layout);
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

layout::WidgetLayout
lay(Store& store,
    float width = 200.f,
    std::optional<float> available = std::nullopt,
    const layout::PanelRules& rules = {}) {
    return layout::layoutWidgets(store, panel, width, theme, sizes(), nullptr, available, rules);
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

Layout equalCells() {
    Layout layout;
    layout.rows = SizeRule::Equal;
    return layout;
}

} // namespace

// ----- Packed: every widget its own height -----

TEST_CASE("packed widgets are stacked in one column, each as high as it needs", "[ui][layout][widgets]") {
    Store store = panelOf({ dynamic("a", 20.f), dynamic("b", 30.f), dynamic("c", 20.f) });
    const auto result = lay(store);

    // 200 wide with a padding of 10: widgets are 180 wide, with a gap of 5 between them.
    REQUIRE_FALSE(store.panel(panel).grid);
    REQUIRE(rectOf(store, 0) == FloatRect(10.f, 10.f, 180.f, 20.f));
    REQUIRE(rectOf(store, 1) == FloatRect(10.f, 35.f, 180.f, 30.f));
    REQUIRE(rectOf(store, 2) == FloatRect(10.f, 70.f, 180.f, 20.f));
    REQUIRE(result.contentHeight == 100.f); // padding above and below included
    REQUIRE(store.panel(panel).contentHeight == 100.f);
}

TEST_CASE(
    "with several columns, packed widgets fill them one after the other with balanced heights", "[ui][layout][widgets]"
) {
    Store store = panelOf({ dynamic("a"), dynamic("b"), dynamic("c"), dynamic("d") }, 2);
    const auto result = lay(store);

    // Two columns of (200 - 20 - 5) / 2 = 87.5.
    REQUIRE(rectOf(store, 0) == FloatRect(10.f, 10.f, 87.f, 20.f)); // whole pixels inside the column
    REQUIRE(rectOf(store, 1).top() == 35.f);
    REQUIRE(rectOf(store, 2).left() == 103.f); // 102.5, rounded
    REQUIRE(rectOf(store, 2).top() == 10.f);
    REQUIRE(rectOf(store, 3).top() == 35.f);
    REQUIRE(result.contentHeight == 65.f);
}

TEST_CASE("a widget is asked for its size at the width it gets", "[ui][layout][widgets]") {
    // At least half as high as it is wide.
    Store one = panelOf({ Item{ .name = "view", .aspectRatio = 2.f } });
    lay(one);
    REQUIRE(rectOf(one, 0) == FloatRect(10.f, 10.f, 180.f, 90.f));

    lay(one, 400.f); // a wider panel
    REQUIRE(rectOf(one, 0) == FloatRect(10.f, 10.f, 380.f, 190.f));
}

TEST_CASE(
    "a constant widget that is narrower than its column is placed in it by the alignment", "[ui][layout][widgets]"
) {
    Store store = panelOf({ constant("button", 20.f, 100.f) });

    lay(store); // the default: centred
    REQUIRE(rectOf(store, 0) == FloatRect(50.f, 10.f, 100.f, 20.f));

    lay(store, 200.f, std::nullopt, { .widgetAlignment = Alignment::Left });
    REQUIRE(rectOf(store, 0) == FloatRect(10.f, 10.f, 100.f, 20.f));

    lay(store, 200.f, std::nullopt, { .widgetAlignment = Alignment::Right });
    REQUIRE(rectOf(store, 0) == FloatRect(90.f, 10.f, 100.f, 20.f));
}

TEST_CASE("the content is as wide as its columns need to be", "[ui][layout][widgets]") {
    Store one = panelOf({ dynamic("a", 20.f, 120.f), dynamic("b", 20.f, 150.f) });
    REQUIRE(lay(one).contentWidth == 170.f); // the widest minimum and the padding

    Store two = panelOf({ dynamic("a", 20.f, 120.f), dynamic("b", 20.f, 150.f) }, 2);
    REQUIRE(lay(two).contentWidth == 325.f); // two columns of 150, a gap of 5, the padding
}

TEST_CASE("a panel without widgets has no content", "[ui][layout][widgets]") {
    Store store = panelOf({});
    REQUIRE(lay(store).contentHeight == 0.f);
    REQUIRE(lay(store).contentWidth == 0.f);
    REQUIRE(store.panel(panel).contentHeight == 0.f);
}

TEST_CASE("packed: height to spare goes to the dynamic widgets of a column", "[ui][layout][widgets]") {
    Store store = panelOf({ constant("above"), dynamic("view", 40.f), constant("below") });

    // Without height to spare everything has its minimum.
    const auto least = lay(store);
    REQUIRE(least.usesSpareHeight);
    REQUIRE(least.contentHeight == 110.f);
    REQUIRE(rectOf(store, 1) == FloatRect(10.f, 35.f, 180.f, 40.f));

    // The same if the panel is lower than its content: it scrolls instead.
    REQUIRE(lay(store, 200.f, 80.f).contentHeight == 110.f);
    REQUIRE(rectOf(store, 1).height() == 40.f);

    // With 200: the dynamic widget takes the 90 that are left, and what follows moves down.
    const auto result = lay(store, 200.f, 200.f);
    REQUIRE(rectOf(store, 0) == FloatRect(10.f, 10.f, 180.f, 20.f));
    REQUIRE(rectOf(store, 1) == FloatRect(10.f, 35.f, 180.f, 130.f));
    REQUIRE(rectOf(store, 2) == FloatRect(10.f, 170.f, 180.f, 20.f));
    REQUIRE(rectOf(store, 2).bottom() == 190.f); // a padding above the end
    REQUIRE(result.contentHeight == 200.f);
}

TEST_CASE("packed: constant widgets do not take height to spare", "[ui][layout][widgets]") {
    Store store = panelOf({ constant("a"), constant("b") });
    const auto result = lay(store, 200.f, 300.f);

    REQUIRE_FALSE(result.usesSpareHeight);
    REQUIRE(rectOf(store, 0) == FloatRect(10.f, 10.f, 180.f, 20.f));
    REQUIRE(rectOf(store, 1) == FloatRect(10.f, 35.f, 180.f, 20.f));
    REQUIRE(result.contentHeight == 65.f);
}

TEST_CASE("packed widgets have the height they prefer", "[ui][layout][widgets]") {
    Store store = panelOf({ flexible("a", 10.f, 20.f, 40.f), flexible("b", 10.f, 30.f, 40.f) });
    const auto result = lay(store);

    REQUIRE(rectOf(store, 0) == FloatRect(10.f, 10.f, 180.f, 20.f));
    REQUIRE(rectOf(store, 1) == FloatRect(10.f, 35.f, 180.f, 30.f));
    REQUIRE(result.contentHeight == 75.f);

    // Packed widgets are not squeezed: a panel too low for them scrolls.
    REQUIRE(lay(store, 200.f, 50.f).contentHeight == 75.f);
    REQUIRE(rectOf(store, 1).height() == 30.f);
}

TEST_CASE("a widget that sets only a minimum prefers its minimum", "[ui][layout][widgets]") {
    Store store = panelOf({ dynamic("a", 25.f) });
    lay(store);
    REQUIRE(rectOf(store, 0).height() == 25.f);
}

// ----- Grid: equal cells -----

TEST_CASE("in a grid, cells are equal and as high as the largest minimum", "[ui][layout][widgets]") {
    Store store = panelOf({ dynamic("a", 20.f), dynamic("b", 30.f), dynamic("c", 20.f) }, 2, 2);
    const auto result = lay(store);

    // Columns of 87.5, rows of 30, gaps of 5. Dynamic widgets fill their cells.
    REQUIRE(store.panel(panel).grid);
    REQUIRE(rectOf(store, 0) == FloatRect(10.f, 10.f, 87.f, 30.f));
    REQUIRE(rectOf(store, 1) == FloatRect(103.f, 10.f, 87.f, 30.f));
    REQUIRE(rectOf(store, 2) == FloatRect(10.f, 45.f, 87.f, 30.f));
    REQUIRE(result.contentHeight == 85.f); // 10 + 30 + 5 + 30 + 10
    REQUIRE(result.usesSpareHeight);
}

TEST_CASE("a constant widget in a cell that is too high for it is placed by the alignment", "[ui][layout][widgets]") {
    Store store = panelOf({ dynamic("graph", 40.f), constant("slider", 20.f) }, 2, 1);

    lay(store); // centred
    REQUIRE(rectOf(store, 0).height() == 40.f);
    REQUIRE(rectOf(store, 1).top() == 20.f); // 20 high in a row of 40
    REQUIRE(rectOf(store, 1).height() == 20.f);

    lay(store, 200.f, std::nullopt, { .widgetAlignment = Alignment::TopLeft });
    REQUIRE(rectOf(store, 1).top() == 10.f);
    lay(store, 200.f, std::nullopt, { .widgetAlignment = Alignment::Bottom });
    REQUIRE(rectOf(store, 1).top() == 30.f);
}

TEST_CASE("a widget that spans cells counts with its minimum divided over them", "[ui][layout][widgets]") {
    // The graph needs 100 but takes three rows: with two gaps of 5 that is 30 per row, so the
    // sliders' 20 do not decide and the rows are 30, not 100.
    Store store = panelOf({ spanning({ .rows = 3 }, dynamic("graph", 100.f)), constant("a"), constant("b") }, 1, 5);
    const auto result = lay(store);

    REQUIRE(rectOf(store, 0) == FloatRect(10.f, 10.f, 180.f, 100.f)); // three rows of 30 and two gaps
    REQUIRE(rectOf(store, 1) == FloatRect(10.f, 120.f, 180.f, 20.f)); // row 3 starts at 115; centred in 30
    REQUIRE(rectOf(store, 2) == FloatRect(10.f, 155.f, 180.f, 20.f));
    REQUIRE(result.contentHeight == 190.f); // 10 + 5 * 30 + 4 * 5 + 10

    // The same across: a widget of 300 over two columns needs columns of 147.5.
    Store wide =
        panelOf({ spanning({ .columns = 2 }, dynamic("graph", 20.f, 300.f)), dynamic("a", 20.f, 100.f) }, 2, 2);
    REQUIRE(lay(wide).contentWidth == 20.f + 148.f * 2.f + 5.f);
}

TEST_CASE("larger widgets are placed first, and each gets its cells", "[ui][layout][widgets]") {
    // The example of setup.hpp.
    Store store = panelOf(
        { constant("Speed"),
          constant("Size"),
          spanning({ .columns = 2, .rows = 3 }, dynamic("Tick time", 50.f)),
          constant("Reset") },
        2,
        5
    );
    const auto result = lay(store);

    REQUIRE(rectOf(store, 2) == FloatRect(10.f, 10.f, 180.f, 70.f)); // rows 0 to 2: three rows of 20 and two gaps
    REQUIRE(rectOf(store, 0) == FloatRect(10.f, 85.f, 87.f, 20.f));  // 0, 3
    REQUIRE(rectOf(store, 1) == FloatRect(103.f, 85.f, 87.f, 20.f)); // 1, 3
    REQUIRE(rectOf(store, 3) == FloatRect(10.f, 110.f, 87.f, 20.f)); // 0, 4
    REQUIRE(result.contentHeight == 140.f);
}

TEST_CASE("a widget with a position takes its cells", "[ui][layout][widgets]") {
    Store store = panelOf(
        {
            constant("Speed"),
            constant("Size"),
            at({ .row = 1, .columnSpan = 2, .rowSpan = 3 }, dynamic("Tick time", 50.f)),
            constant("Reset"),
        },
        2,
        5
    );
    lay(store);

    REQUIRE(rectOf(store, 0).position() == sf::Vector2f(10.f, 10.f));
    REQUIRE(rectOf(store, 1).position() == sf::Vector2f(103.f, 10.f));
    REQUIRE(rectOf(store, 2) == FloatRect(10.f, 35.f, 180.f, 70.f)); // rows 1 to 3
    REQUIRE(rectOf(store, 3).position() == sf::Vector2f(10.f, 110.f));
}

TEST_CASE("a row nobody uses stays empty, as a separator", "[ui][layout][widgets]") {
    Store store = panelOf({ at({ .row = 0 }, constant("above")), at({ .row = 2 }, constant("below")) });
    const auto result = lay(store);

    REQUIRE(store.panel(panel).rows == 3); // as many as the positions use
    REQUIRE(rectOf(store, 0).top() == 10.f);
    REQUIRE(rectOf(store, 1).top() == 60.f); // one row and two gaps further down
    REQUIRE(result.contentHeight == 90.f);
}

TEST_CASE("the rows of a grid share the height the panel has to spare", "[ui][layout][widgets]") {
    Store store = panelOf({ dynamic("view", 20.f), constant("slider", 25.f) }, 1, 2);
    REQUIRE(lay(store).contentHeight == 75.f); // two rows of 25

    // 295 high: two rows of 135.
    const auto result = lay(store, 200.f, 295.f);
    REQUIRE(result.contentHeight == 295.f);
    REQUIRE(rectOf(store, 0) == FloatRect(10.f, 10.f, 180.f, 135.f)); // dynamic: fills its row
    REQUIRE(rectOf(store, 1) == FloatRect(10.f, 205.f, 180.f, 25.f)); // constant: centred in its row

    // Less height than the rows need changes nothing: the panel scrolls.
    REQUIRE(lay(store, 200.f, 40.f).contentHeight == 75.f);
    REQUIRE(rectOf(store, 0).height() == 25.f);
}

TEST_CASE("a widget that keeps its shape takes the largest such rectangle in its cell", "[ui][layout][widgets]") {
    Item view = dynamic("view", 40.f);
    view.request.widestRatio = 2.f;
    view.request.tallestRatio = 0.5f; // always twice as wide as high
    Store store = panelOf({ view }, 1, 1);

    lay(store, 200.f, 220.f); // a cell of 180 x 200
    REQUIRE(rectOf(store, 0) == FloatRect(10.f, 65.f, 180.f, 90.f));
}

TEST_CASE("a grid sizes its rows by what the widgets prefer", "[ui][layout][widgets]") {
    Store store = panelOf({ flexible("a", 10.f, 20.f, 40.f), flexible("b", 15.f, 30.f, 40.f) }, 1, 2);
    const auto result = lay(store);

    // Rows of 30: the larger preferred height. The first widget gets 30 too: it can use up to 40.
    REQUIRE(rectOf(store, 0) == FloatRect(10.f, 10.f, 180.f, 30.f));
    REQUIRE(rectOf(store, 1) == FloatRect(10.f, 45.f, 180.f, 30.f));
    REQUIRE(result.contentHeight == 85.f);
}

TEST_CASE("a grid that is short of height squeezes its rows down to the widgets' minimum", "[ui][layout][widgets]") {
    // Preferred: rows of 30, content 85. Least: rows of 15, content 55.
    Store store = panelOf({ flexible("a", 10.f, 20.f, 40.f), flexible("b", 15.f, 30.f, 40.f) }, 1, 2);

    // 65 high: rows of (65 - 20 - 5) / 2 = 20. Everything is visible, nothing scrolls.
    auto result = lay(store, 200.f, 65.f);
    REQUIRE(result.contentHeight == 65.f);
    REQUIRE(rectOf(store, 0) == FloatRect(10.f, 10.f, 180.f, 20.f));
    REQUIRE(rectOf(store, 1) == FloatRect(10.f, 35.f, 180.f, 20.f));

    // 40 high: rows would be 7.5, but no widget can go below 15. The rows stay at 15 and the
    // content, 55 high, scrolls.
    result = lay(store, 200.f, 40.f);
    REQUIRE(result.contentHeight == 55.f);
    REQUIRE(rectOf(store, 1) == FloatRect(10.f, 30.f, 180.f, 15.f));
}

TEST_CASE("a grid with room to spare lets constant widgets grow up to their maximum", "[ui][layout][widgets]") {
    Store store = panelOf({ flexible("a", 10.f, 20.f, 40.f) }, 1, 1);

    lay(store, 200.f, 60.f); // a row of 40: exactly the maximum
    REQUIRE(rectOf(store, 0) == FloatRect(10.f, 10.f, 180.f, 40.f));

    lay(store, 200.f, 120.f); // a row of 100: the widget stops at 40, centred
    REQUIRE(rectOf(store, 0) == FloatRect(10.f, 40.f, 180.f, 40.f));
}

// ----- When a panel is a grid -----

TEST_CASE("a panel is packed unless something asks for cells", "[ui][layout][widgets]") {
    REQUIRE_FALSE(panelOf({ dynamic("a"), dynamic("b") }, 2).panel(panel).grid);

    REQUIRE(panelOf({ dynamic("a") }, 1, 3).panel(panel).grid);                    // the panel names rows
    REQUIRE(panelOf({ at({ .row = 1 }, dynamic("a")) }).panel(panel).grid);        // a widget has a position
    REQUIRE(panelOf({ spanning({ .rows = 2 }, dynamic("a")) }).panel(panel).grid); // a widget has a span
    REQUIRE(panelOf({ dynamic("a") }, 1, 0, equalCells()).panel(panel).grid);      // the layout theme wants equal cells
}

TEST_CASE("a panel whose size is given from outside is a grid", "[ui][layout][widgets]") {
    UISetup setup;
    setup.panels = {
        { .name = "filling", .placement = GridCell{}, .widgets = { dynamic("a"), dynamic("b") } },
        { .name = "fitting", .placement = GridCell{}, .layout = { .fit = Fit::Content }, .widgets = { dynamic("a") } },
        { .name = "floating", .placement = Anchor::Top, .widgets = { dynamic("a") } },
    };
    Store store{ setup };
    layout::prepare(store, {});

    REQUIRE(store.panel(PanelId{ 0 }).grid); // top-down: its content adapts to it
    REQUIRE(store.panel(PanelId{ 0 }).rows == 2);
    REQUIRE_FALSE(store.panel(PanelId{ 1 }).grid); // as large as its content: bottom-up
    REQUIRE_FALSE(store.panel(PanelId{ 2 }).grid);
}

TEST_CASE("a grid that names no rows and no positions has as many rows as its widgets need", "[ui][layout][widgets]") {
    Store plain = panelOf({ dynamic("a"), dynamic("b"), dynamic("c") }, 2, 0, equalCells());
    REQUIRE(plain.panel(panel).rows == 2); // three cells in two columns

    // A span is no problem either: five rows for the graph and one for the slider.
    Store spanned = panelOf({ constant("Speed"), spanning({ .rows = 5 }, dynamic("Graph")) });
    REQUIRE(spanned.panel(panel).rows == 6);
    REQUIRE(spanned.widget(WidgetId{ 1 }).cell->row == 0); // the larger one first
    REQUIRE(spanned.widget(WidgetId{ 0 }).cell->row == 5);

    // Shapes that leave holes need more rows than cells: two 2 x 1 and a 1 x 2 in three columns.
    Store awkward = panelOf(
        { spanning({ .columns = 2 }, dynamic("a")),
          spanning({ .columns = 2 }, dynamic("b")),
          spanning({ .rows = 2 }, dynamic("c")) },
        3
    );
    REQUIRE(awkward.panel(panel).rows == 2);
}

TEST_CASE("another layout theme can turn a grid back into a packed panel", "[ui][layout][widgets]") {
    Store store = panelOf({ dynamic("a"), dynamic("b") }, 1, 0, equalCells());
    REQUIRE(store.panel(panel).grid);
    REQUIRE(store.widget(WidgetId{ 1 }).cell.has_value());

    layout::prepareWidgets(store, Layout{}); // every widget its own height again
    REQUIRE_FALSE(store.panel(panel).grid);
    REQUIRE_FALSE(store.widget(WidgetId{ 1 }).cell.has_value());

    layout::prepareWidgets(store, equalCells());
    REQUIRE(store.panel(panel).rows == 2);
}

// ----- What can never be laid out -----

TEST_CASE("widget layouts that can never work are refused", "[ui][layout][widgets]") {
    const auto refuses = [](Store store, const std::string& message) {
        REQUIRE_THROWS_AS(layout::prepareWidgets(store), SetupError);
        REQUIRE_THROWS_WITH(layout::prepareWidgets(store), ContainsSubstring(message));
    };

    SECTION("no columns") {
        refuses(unchecked({ dynamic("a") }, 0), "panel \"Panel\" has 0 columns");
    }
    SECTION("too many columns") {
        refuses(unchecked({ dynamic("a") }, 4), "a panel can have 1 to 3");
    }
    SECTION("a negative number of rows") {
        refuses(unchecked({ dynamic("a") }, 1, -1), "negative number of rows");
    }
    SECTION("a cell outside the panel's columns") {
        refuses(
            unchecked({ at({ .column = 2 }, dynamic("Speed")) }, 2),
            "widget \"Panel/Speed\" reaches outside its panel's grid"
        );
    }
    SECTION("a cell below the panel's rows") {
        refuses(unchecked({ at({ .row = 3 }, dynamic("Speed")) }, 2, 3), "grid of 2 columns and 3 rows");
    }
    SECTION("a span wider than the panel") {
        refuses(unchecked({ spanning({ .columns = 3 }, dynamic("Graph")) }, 2, 4), "reaches outside");
    }
    SECTION("a span of nothing") {
        refuses(unchecked({ at({ .rowSpan = 0 }, dynamic("Speed")) }), "spans less than one cell");
    }
    SECTION("two widgets with a position on the same cell") {
        refuses(
            unchecked(
                { at({ .column = 0, .row = 0, .columnSpan = 2 }, dynamic("Graph")),
                  at({ .column = 1, .row = 0 }, dynamic("Speed")) },
                2
            ),
            "widget \"Panel/Speed\" is placed on a cell another widget already has"
        );
    }
    SECTION("more widgets than the rows the panel names") {
        refuses(unchecked({ dynamic("a"), dynamic("b"), dynamic("c") }, 2, 1), "widget \"Panel/c\" finds no room");
    }
    SECTION("more widgets than the rows the positions use") {
        // One widget says it is in row 0, so the grid has one row; the others do not fit.
        refuses(unchecked({ at({ .row = 0 }, dynamic("a")), dynamic("b") }), "widget \"Panel/b\" finds no room");
        refuses(unchecked({ at({ .row = 0 }, dynamic("a")), dynamic("b") }), "Give the panel more rows");
    }
}

// ----- What a widget may know while it is measured -----

TEST_CASE(
    "a widget being measured can ask for its width, the layout's sizes and the size of text", "[ui][layout][widgets]"
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

    Store packed = panelOf({ dynamic("a"), dynamic("b"), dynamic("c"), dynamic("d") }, 2);
    layout::layoutWidgets(packed, panel, 200.f, theme, wide);
    // Two columns of (200 - 40 - 10) / 2 = 75, 10 apart; rows 5 apart.
    REQUIRE(rectOf(packed, 0) == FloatRect(20.f, 10.f, 75.f, 20.f));
    REQUIRE(rectOf(packed, 1) == FloatRect(20.f, 35.f, 75.f, 20.f));
    REQUIRE(rectOf(packed, 2) == FloatRect(105.f, 10.f, 75.f, 20.f));

    Store grid = panelOf({ dynamic("a"), dynamic("b"), dynamic("c") }, 2, 2);
    const auto result = layout::layoutWidgets(grid, panel, 200.f, theme, wide);
    REQUIRE(rectOf(grid, 1) == FloatRect(105.f, 10.f, 75.f, 20.f));
    REQUIRE(rectOf(grid, 2) == FloatRect(20.f, 35.f, 75.f, 20.f));
    REQUIRE(result.contentHeight == 65.f);
}

// ----- Everything together -----

TEST_CASE("a floating panel is as high as its header and its widgets", "[ui][layout]") {
    UISetup setup;
    setup.panels = {
        { .name = "Controls", .placement = Anchor::TopLeft, .widgets = { dynamic("a", 20.f), dynamic("b", 30.f) } },
        { .name = "Below", .placement = Anchor::TopLeft, .widgets = { dynamic("c", 20.f) } },
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

TEST_CASE("top-down: a panel that fills its cells splits its height among its rows", "[ui][layout]") {
    UISetup setup;
    setup.panels = {
        { .name = "Scene", .placement = GridCell{}, .widgets = { constant("toolbar"), Region{ "world" } } }
    };
    Store store{ setup };
    layout::prepare(store, {});
    layout::arrange(store, { 800.f, 600.f }, {}, theme, sizes());

    // The panel fills the window inside the margins: 780 x 580, of which 550 are content. Two
    // equal rows of (550 - 20 - 5) / 2 = 262.5, on whole pixels.
    REQUIRE(store.panel(panel).rect == FloatRect(10.f, 10.f, 780.f, 580.f));
    REQUIRE(rectOf(store, 0) == FloatRect(10.f, 132.f, 760.f, 20.f));  // constant: centred in its row
    REQUIRE(rectOf(store, 1) == FloatRect(10.f, 278.f, 760.f, 262.f)); // dynamic: fills its row

    // The view is its widget's place in the window: panel at (10, 10), content below the header.
    REQUIRE(store.view(ViewId{ 0 }).rect == FloatRect(20.f, 318.f, 760.f, 262.f));
}

TEST_CASE("top-down: spans decide how the panel's room is shared", "[ui][layout]") {
    // One row for the toolbar, nine for the view.
    UISetup setup;
    setup.panels = {
        { .name = "Scene",
          .placement = GridCell{},
          .widgets = { constant("toolbar"), spanning({ .rows = 9 }, Region{ "world" }) } },
    };
    Store store{ setup };
    layout::prepare(store, {});
    layout::arrange(store, { 800.f, 600.f }, {}, theme, sizes());

    // Ten rows of (550 - 20 - 45) / 10 = 48.5. The view was placed first: rows 0 to 8.
    REQUIRE(store.panel(panel).rows == 10);
    REQUIRE(rectOf(store, 1) == FloatRect(10.f, 10.f, 760.f, 477.f));
    REQUIRE(rectOf(store, 0).top() > rectOf(store, 1).bottom());
    REQUIRE(rectOf(store, 0).height() == 20.f);
}

TEST_CASE("top-down: a panel in a small window squeezes its rows before it scrolls", "[ui][layout]") {
    UISetup setup;
    setup.panels = {
        { .name = "Controls",
          .placement = GridCell{},
          .widgets = { flexible("a", 15.f, 30.f, 40.f),
                       flexible("b", 15.f, 30.f, 40.f),
                       flexible("c", 15.f, 30.f, 40.f) } },
    };
    Store store{ setup };
    layout::prepare(store, {});
    const auto contentFor = [&](float windowHeight) {
        layout::arrange(store, { 400.f, windowHeight }, {}, theme, sizes());
        return store.panel(panel).rect.height() - sizes().headerHeight;
    };

    // Preferred: three rows of 30, content 20 + 90 + 10 = 120. Least: rows of 15, content 75.
    REQUIRE(contentFor(170.f) == 120.f); // room for everything as preferred
    REQUIRE(rectOf(store, 0).height() == 30.f);

    REQUIRE(contentFor(140.f) == 90.f); // squeezed: rows of 20, nothing to scroll
    REQUIRE(rectOf(store, 2).height() == 20.f);
    REQUIRE(layout::contentOverflow(store.panel(panel), sizes()) == 0.f);

    REQUIRE(contentFor(100.f) == 50.f); // too little even for the minimum: rows of 15, the rest scrolls
    REQUIRE(rectOf(store, 2).height() == 15.f);
    REQUIRE(layout::contentOverflow(store.panel(panel), sizes()) == 25.f);
}

TEST_CASE("bottom-up: a panel as large as its content gives spare height to its dynamic widgets", "[ui][layout]") {
    // The same panel, but it fits its content instead of filling its cells: its widgets are
    // packed. (Until panels are sized by their content, WP 3.17, it still has its cells' height.)
    UISetup setup;
    setup.panels = {
        { .name = "Scene",
          .placement = GridCell{},
          .layout = { .fit = Fit::Content },
          .widgets = { constant("toolbar"), Region{ "world" } } },
    };
    Store store{ setup };
    layout::prepare(store, {});
    layout::arrange(store, { 800.f, 600.f }, {}, theme, sizes());

    REQUIRE(rectOf(store, 0) == FloatRect(10.f, 10.f, 760.f, 20.f));
    REQUIRE(rectOf(store, 1) == FloatRect(10.f, 35.f, 760.f, 505.f)); // down to a padding above the end
}

TEST_CASE("a panel takes its widgets' alignment from the layout theme, or says it itself", "[ui][layout]") {
    UISetup setup;
    setup.panels = {
        { .name = "theme's", .placement = Anchor::TopLeft, .widgets = { constant("a", 20.f, 100.f) } },
        { .name = "own",
          .placement = Anchor::TopRight,
          .layout = { .widgetAlignment = Alignment::Right },
          .widgets = { constant("b", 20.f, 100.f) } },
    };
    Store store{ setup };
    Layout layout;
    layout.widgetAlignment = Alignment::Left;
    layout::prepare(store, {}, layout);
    layout::arrange(store, { 800.f, 600.f }, {}, theme, sizes(), nullptr, layout);

    REQUIRE(rectOf(store, 0).left() == 10.f); // at the left of its column
    REQUIRE(rectOf(store, 1).left() == 90.f); // at the right
}

TEST_CASE("a panel that cannot be as high as its content says how far it has to scroll", "[ui][layout]") {
    UISetup setup;
    setup.panels = {
        { .name = "Long", .placement = Anchor::TopLeft, .widgets = { dynamic("a", 300.f), dynamic("b", 300.f) } }
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
    setup.panels = { { .name = "Controls", .placement = Anchor::TopLeft, .widgets = { dynamic("a") } } };
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
