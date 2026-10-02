#include "atpl/ui/error.hpp"

#include "ui/layout/panel_placement.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <cmath>
#include <memory>
#include <string>

using namespace atpl;
using atpl::model::Store;
using Catch::Matchers::ContainsSubstring;

namespace {

// Round numbers, so the expected rectangles can be worked out by hand.
Sizes sizes() {
    Sizes result;
    result.margin = 10.f;
    result.headerHeight = 30.f;
    result.panelWidth = 200.f;
    return result;
}

class PlainWidget final : public Widget {
public:
    [[nodiscard]] SizeRequest measure(const MeasureContext&) const override { return {}; }
    void paint(Painter&, const Style&) const override {}
};

struct Region {
    static constexpr bool isView = true;
    std::string name;
    [[nodiscard]] std::unique_ptr<Widget> create() const { return std::make_unique<PlainWidget>(); }
};

PanelSetup floating(std::string name, Anchor anchor, bool collapsed = false) {
    return { .name = std::move(name), .placement = anchor, .collapsed = collapsed };
}

PanelSetup inGrid(std::string name, GridCell cell) {
    return { .name = std::move(name), .placement = cell };
}

/// A store of these panels, each with content of the given height.
Store storeOf(std::vector<PanelSetup> panels, float contentHeight = 70.f) {
    UISetup setup;
    setup.panels = std::move(panels);
    Store store{ setup };
    for (model::Panel& panel : store.panels()) {
        panel.contentHeight = contentHeight;
    }
    return store;
}

const sf::Vector2f window(800.f, 600.f);

FloatRect rectOf(const Store& store, std::uint32_t panel) {
    return store.panel(PanelId{ panel }).rect;
}

bool shown(const Store& store, std::uint32_t panel) {
    return store.panel(PanelId{ panel }).shown;
}

void place(Store& store, sf::Vector2f size = window, GridSetup grid = {}) {
    layout::placePanels(store, size, grid, sizes());
}

} // namespace

// ----- Floating panels -----

TEST_CASE("a floating panel sits at its anchor, a margin away from the window's edges", "[ui][layout][panels]") {
    // Each panel is 200 wide and 30 + 70 = 100 high.
    Store store = storeOf(
        {
            floating("top left", Anchor::TopLeft),
            floating("top", Anchor::Top),
            floating("top right", Anchor::TopRight),
            floating("left", Anchor::Left),
            floating("right", Anchor::Right),
            floating("bottom left", Anchor::BottomLeft),
            floating("bottom", Anchor::Bottom),
            floating("bottom right", Anchor::BottomRight),
        }
    );
    place(store);

    REQUIRE(rectOf(store, 0) == FloatRect(10.f, 10.f, 200.f, 100.f));
    REQUIRE(rectOf(store, 1) == FloatRect(300.f, 10.f, 200.f, 100.f));
    REQUIRE(rectOf(store, 2) == FloatRect(590.f, 10.f, 200.f, 100.f));
    REQUIRE(rectOf(store, 3) == FloatRect(10.f, 250.f, 200.f, 100.f));
    REQUIRE(rectOf(store, 4) == FloatRect(590.f, 250.f, 200.f, 100.f));
    REQUIRE(rectOf(store, 5) == FloatRect(10.f, 490.f, 200.f, 100.f));
    REQUIRE(rectOf(store, 6) == FloatRect(300.f, 490.f, 200.f, 100.f));
    REQUIRE(rectOf(store, 7) == FloatRect(590.f, 490.f, 200.f, 100.f));
    for (std::uint32_t i = 0; i < 8; ++i) {
        REQUIRE(shown(store, i));
    }
}

TEST_CASE("panels that share a top anchor are stacked downwards in the order listed", "[ui][layout][panels]") {
    Store store = storeOf(
        {
            floating("first", Anchor::TopLeft),
            floating("elsewhere", Anchor::TopRight),
            floating("second", Anchor::TopLeft, true), // collapsed: its header only
            floating("third", Anchor::TopLeft),
        }
    );
    place(store);

    REQUIRE(rectOf(store, 0) == FloatRect(10.f, 10.f, 200.f, 100.f));
    REQUIRE(rectOf(store, 2) == FloatRect(10.f, 120.f, 200.f, 30.f));
    REQUIRE(rectOf(store, 3) == FloatRect(10.f, 160.f, 200.f, 100.f));
    REQUIRE(rectOf(store, 1) == FloatRect(590.f, 10.f, 200.f, 100.f)); // a stack of its own
}

TEST_CASE("panels that share a bottom anchor are stacked upwards", "[ui][layout][panels]") {
    Store store = storeOf({ floating("first", Anchor::BottomRight), floating("second", Anchor::BottomRight) });
    place(store);

    REQUIRE(rectOf(store, 0) == FloatRect(590.f, 490.f, 200.f, 100.f)); // the first is the lowest
    REQUIRE(rectOf(store, 1) == FloatRect(590.f, 380.f, 200.f, 100.f));
}

TEST_CASE("a stack at the side is centred as a whole", "[ui][layout][panels]") {
    Store store = storeOf({ floating("first", Anchor::Left), floating("second", Anchor::Left) });
    place(store);

    // Two panels of 100 and a gap of 10 are 210 high: from 195 to 405 in a window of 600.
    REQUIRE(rectOf(store, 0) == FloatRect(10.f, 195.f, 200.f, 100.f));
    REQUIRE(rectOf(store, 1) == FloatRect(10.f, 305.f, 200.f, 100.f));
}

TEST_CASE("a panel is as wide as it asks to be, or as the theme says", "[ui][layout][panels]") {
    std::vector<PanelSetup> panels = { floating("default", Anchor::TopLeft), floating("wide", Anchor::TopRight) };
    panels[1].width = 320.f;
    Store store = storeOf(std::move(panels));
    place(store);

    REQUIRE(rectOf(store, 0).width() == 200.f);
    REQUIRE(rectOf(store, 1) == FloatRect(470.f, 10.f, 320.f, 100.f));

    // A width a panel asks for scales like the layout's own sizes do.
    Sizes larger = sizes();
    larger.scale = { 1.5f, 1.f };
    layout::placePanels(store, window, {}, larger);
    REQUIRE(rectOf(store, 1).width() == 480.f);
}

TEST_CASE("in a narrow window the margin shrinks first, then the panel", "[ui][layout][panels]") {
    Store store = storeOf({ floating("left", Anchor::TopLeft), floating("right", Anchor::TopRight) });

    place(store, { 210.f, 600.f }); // room for the panel and half the margins
    REQUIRE(rectOf(store, 0) == FloatRect(5.f, 10.f, 200.f, 100.f));
    REQUIRE(rectOf(store, 1) == FloatRect(5.f, 10.f, 200.f, 100.f));

    place(store, { 150.f, 600.f }); // narrower than the panel: clamped to the window
    REQUIRE(rectOf(store, 0) == FloatRect(0.f, 10.f, 150.f, 100.f));
    REQUIRE(rectOf(store, 1) == FloatRect(0.f, 10.f, 150.f, 100.f));
}

TEST_CASE("a stack too high for the window is fitted to it", "[ui][layout][panels]") {
    // Wanted: 30 + 200, a header of 30, 30 + 200; with two gaps that is 510.
    Store store = storeOf(
        { floating("first", Anchor::TopLeft),
          floating("folded", Anchor::TopLeft, true),
          floating("last", Anchor::TopLeft) },
        200.f
    );

    // 400 high: 380 inside the margins, minus 20 of gaps and 90 of headers leaves 270 of content
    // for the two expanded panels: 135 each.
    place(store, { 800.f, 400.f });
    REQUIRE(rectOf(store, 0) == FloatRect(10.f, 10.f, 200.f, 165.f));
    REQUIRE(rectOf(store, 1) == FloatRect(10.f, 185.f, 200.f, 30.f)); // collapsed: keeps its header height
    REQUIRE(rectOf(store, 2) == FloatRect(10.f, 225.f, 200.f, 165.f));
    REQUIRE(rectOf(store, 2).bottom() == 390.f); // ends a margin above the window's edge
}

TEST_CASE("when a stack is fitted, panels that need little keep what they need", "[ui][layout][panels]") {
    Store store = storeOf({ floating("small", Anchor::TopLeft), floating("large", Anchor::TopLeft) });
    store.panel(PanelId{ 0 }).contentHeight = 40.f;
    store.panel(PanelId{ 1 }).contentHeight = 500.f;

    // 400 high: 380 minus one gap and two headers leaves 310 of content. The small panel takes
    // its 40, the large one gets the remaining 270.
    place(store, { 800.f, 400.f });
    REQUIRE(rectOf(store, 0) == FloatRect(10.f, 10.f, 200.f, 70.f));
    REQUIRE(rectOf(store, 1) == FloatRect(10.f, 90.f, 200.f, 300.f));
}

TEST_CASE("if not even the headers fit, the last panels of a stack are not shown", "[ui][layout][panels]") {
    Store store = storeOf(
        {
            floating("first", Anchor::TopLeft),
            floating("second", Anchor::TopLeft),
            floating("third", Anchor::TopLeft),
            floating("other stack", Anchor::TopRight),
        }
    );

    // 100 high: 80 inside the margins. Two headers and a gap are 70; three would be 110.
    place(store, { 800.f, 100.f });
    REQUIRE(shown(store, 0));
    REQUIRE(shown(store, 1));
    REQUIRE_FALSE(shown(store, 2));
    REQUIRE(rectOf(store, 0) == FloatRect(10.f, 10.f, 200.f, 35.f)); // the 10 left over are shared
    REQUIRE(rectOf(store, 1) == FloatRect(10.f, 55.f, 200.f, 35.f));
    REQUIRE(shown(store, 3)); // the other stack has room for its one panel

    // Too low for a single header: nothing is shown.
    place(store, { 800.f, 40.f });
    REQUIRE_FALSE(shown(store, 0));
    REQUIRE_FALSE(shown(store, 3));

    // And everything comes back when there is room again.
    place(store);
    REQUIRE(shown(store, 2));
    REQUIRE(rectOf(store, 2) == FloatRect(10.f, 230.f, 200.f, 100.f));
}

TEST_CASE("a panel the application hides is not shown and leaves no gap", "[ui][layout][panels]") {
    Store store = storeOf(
        {
            floating("first", Anchor::TopLeft),
            floating("hidden", Anchor::TopLeft),
            floating("third", Anchor::TopLeft),
        }
    );
    store.panel(PanelId{ 1 }).visible = false;
    place(store);

    REQUIRE_FALSE(shown(store, 1));
    REQUIRE(rectOf(store, 0) == FloatRect(10.f, 10.f, 200.f, 100.f));
    REQUIRE(rectOf(store, 2) == FloatRect(10.f, 120.f, 200.f, 100.f)); // right below the first

    store.panel(PanelId{ 1 }).visible = true;
    place(store);
    REQUIRE(shown(store, 1));
    REQUIRE(rectOf(store, 2) == FloatRect(10.f, 230.f, 200.f, 100.f));
}

TEST_CASE("collapsing a panel moves the ones stacked after it", "[ui][layout][panels]") {
    Store store = storeOf({ floating("first", Anchor::TopLeft), floating("second", Anchor::TopLeft) });
    place(store);
    REQUIRE(rectOf(store, 1).top() == 120.f);

    store.panel(PanelId{ 0 }).collapsed = true;
    place(store);
    REQUIRE(rectOf(store, 0) == FloatRect(10.f, 10.f, 200.f, 30.f));
    REQUIRE(rectOf(store, 1) == FloatRect(10.f, 50.f, 200.f, 100.f));
}

// ----- Grid panels -----

TEST_CASE("a grid panel fills the cells it spans", "[ui][layout][panels]") {
    // 830 x 430 with a margin of 10 and a 4 x 2 grid: cells of 195 x 200.
    const sf::Vector2f size(830.f, 430.f);
    const GridSetup grid{ .columns = 4, .rows = 2 };
    Store store = storeOf(
        {
            inGrid("one cell", { .column = 0, .row = 0 }),
            inGrid("two columns", { .column = 2, .row = 0, .columnSpan = 2 }),
            inGrid("two rows", { .column = 1, .row = 0, .rowSpan = 2 }),
            inGrid("last cell", { .column = 3, .row = 1 }),
        }
    );
    place(store, size, grid);

    REQUIRE(rectOf(store, 0) == FloatRect(10.f, 10.f, 195.f, 200.f));
    REQUIRE(rectOf(store, 1) == FloatRect(420.f, 10.f, 400.f, 200.f)); // two cells and the margin between
    REQUIRE(rectOf(store, 2) == FloatRect(215.f, 10.f, 195.f, 410.f));
    REQUIRE(rectOf(store, 3) == FloatRect(625.f, 220.f, 195.f, 200.f));
    REQUIRE(rectOf(store, 3).right() == 820.f); // a margin from the window's edge
    REQUIRE(rectOf(store, 3).bottom() == 420.f);
}

TEST_CASE("grid cells shrink and grow with the window", "[ui][layout][panels]") {
    const GridSetup grid{ .columns = 2, .rows = 1 };
    Store store = storeOf({ inGrid("left", { .column = 0 }), inGrid("right", { .column = 1 }) });

    place(store, { 630.f, 220.f }, grid);
    REQUIRE(rectOf(store, 0) == FloatRect(10.f, 10.f, 300.f, 200.f));
    REQUIRE(rectOf(store, 1) == FloatRect(320.f, 10.f, 300.f, 200.f));

    place(store, { 430.f, 120.f }, grid);
    REQUIRE(rectOf(store, 0) == FloatRect(10.f, 10.f, 200.f, 100.f));
    REQUIRE(rectOf(store, 1) == FloatRect(220.f, 10.f, 200.f, 100.f));
}

TEST_CASE("cells that do not divide evenly still line up on whole pixels", "[ui][layout][panels]") {
    const GridSetup grid{ .columns = 3, .rows = 1 };
    Store store = storeOf(
        {
            inGrid("a", { .column = 0 }),
            inGrid("b", { .column = 1 }),
            inGrid("c", { .column = 2 }),
            inGrid("all", { .column = 0, .columnSpan = 3 }),
        }
    );
    place(store, { 801.f, 300.f }, grid);

    for (std::uint32_t i = 0; i < 4; ++i) {
        const FloatRect rect = rectOf(store, i);
        REQUIRE(rect.left() == std::round(rect.left()));
        REQUIRE(rect.width() == std::round(rect.width()));
    }
    // The same margin between neighbours, and the spanning panel covers exactly all three.
    REQUIRE(rectOf(store, 1).left() - rectOf(store, 0).right() == 10.f);
    REQUIRE(rectOf(store, 3).left() == rectOf(store, 0).left());
    REQUIRE(rectOf(store, 3).right() == rectOf(store, 2).right());
}

TEST_CASE("a collapsed grid panel is its header at the top of its cells", "[ui][layout][panels]") {
    std::vector<PanelSetup> panels = { inGrid("folded", {}) };
    panels[0].collapsed = true;
    Store store = storeOf(std::move(panels));
    place(store);

    REQUIRE(rectOf(store, 0) == FloatRect(10.f, 10.f, 780.f, 30.f));
}

TEST_CASE("a grid panel is not shown when its cells have no room left", "[ui][layout][panels]") {
    Store store = storeOf({ inGrid("cell", {}) });

    place(store, { 15.f, 600.f }); // narrower than the two margins
    REQUIRE_FALSE(shown(store, 0));
    place(store);
    REQUIRE(shown(store, 0));
}

TEST_CASE("floating and grid panels are placed independently", "[ui][layout][panels]") {
    Store store = storeOf({ inGrid("main", {}), floating("controls", Anchor::TopRight) });
    place(store);

    REQUIRE(rectOf(store, 0) == FloatRect(10.f, 10.f, 780.f, 580.f));
    REQUIRE(rectOf(store, 1) == FloatRect(590.f, 10.f, 200.f, 100.f)); // on top of the grid
}

// ----- What has to be painted again -----

TEST_CASE(
    "a panel is marked dirty when its size changes or it appears, not when it only moves", "[ui][layout][panels]"
) {
    Store store = storeOf({ floating("first", Anchor::TopLeft), floating("second", Anchor::TopLeft) });
    const auto dirty = [&](std::uint32_t panel) { return store.panel(PanelId{ panel }).dirty; };
    const auto clean = [&] {
        for (model::Panel& panel : store.panels()) {
            panel.dirty = false;
        }
    };

    place(store);
    clean();
    place(store); // the same again: nothing to paint
    REQUIRE_FALSE(dirty(0));
    REQUIRE_FALSE(dirty(1));

    place(store, { 1000.f, 700.f }); // a larger window moves nothing at the top left
    REQUIRE_FALSE(dirty(0));

    store.panel(PanelId{ 0 }).collapsed = true;
    place(store);
    REQUIRE(dirty(0));       // a different size
    REQUIRE_FALSE(dirty(1)); // moved up, same size

    clean();
    store.panel(PanelId{ 1 }).visible = false;
    place(store);
    REQUIRE_FALSE(dirty(1)); // gone: nothing to paint
    store.panel(PanelId{ 1 }).visible = true;
    place(store);
    REQUIRE(dirty(1)); // back again
}

// ----- Views -----

TEST_CASE("the background view is the whole window", "[ui][layout][views]") {
    UISetup setup;
    setup.background = "world";
    Store store{ setup };

    layout::placeViews(store, window, sizes());
    REQUIRE(store.view(ViewId{ 0 }).rect == FloatRect(0.f, 0.f, 800.f, 600.f));

    layout::placeViews(store, { 1024.f, 768.f }, sizes());
    REQUIRE(store.view(ViewId{ 0 }).rect == FloatRect(0.f, 0.f, 1024.f, 768.f));
}

TEST_CASE("a view widget's view is where its widget is in the window", "[ui][layout][views]") {
    UISetup setup;
    setup.panels = { { .name = "Map", .placement = Anchor::TopRight, .widgets = { Region{ "minimap" } } } };
    Store store{ setup };
    store.panel(PanelId{ 0 }).contentHeight = 170.f;
    store.widget(WidgetId{ 0 }).rect = FloatRect(12.f, 12.f, 176.f, 99.f); // as widget layout will set it

    place(store);
    layout::placeViews(store, window, sizes());
    // The panel is at (590, 10); its content starts below the header of 30.
    REQUIRE(store.view(ViewId{ 0 }).rect == FloatRect(602.f, 52.f, 176.f, 99.f));

    // It moves with its panel.
    place(store, { 1000.f, 600.f });
    layout::placeViews(store, { 1000.f, 600.f }, sizes());
    REQUIRE(store.view(ViewId{ 0 }).rect == FloatRect(802.f, 52.f, 176.f, 99.f));
}

TEST_CASE("a view that is not on screen has an empty rectangle", "[ui][layout][views]") {
    UISetup setup;
    setup.panels = { { .name = "Map", .placement = Anchor::TopRight, .widgets = { Region{ "minimap" } } } };
    Store store{ setup };
    store.panel(PanelId{ 0 }).contentHeight = 170.f;
    store.widget(WidgetId{ 0 }).rect = FloatRect(12.f, 12.f, 176.f, 99.f);
    const auto viewRect = [&] {
        place(store);
        layout::placeViews(store, window, sizes());
        return store.view(ViewId{ 0 }).rect;
    };
    REQUIRE(viewRect().width() == 176.f);

    store.panel(PanelId{ 0 }).collapsed = true;
    REQUIRE(viewRect() == FloatRect());
    store.panel(PanelId{ 0 }).collapsed = false;

    store.panel(PanelId{ 0 }).visible = false;
    REQUIRE(viewRect() == FloatRect());
    store.panel(PanelId{ 0 }).visible = true;

    store.widget(WidgetId{ 0 }).visible = false;
    REQUIRE(viewRect() == FloatRect());
}

// ----- Setups that cannot work -----

TEST_CASE("panels that leave their place in the grid open get the first free cells", "[ui][layout][panels]") {
    const GridSetup grid{ .columns = 4, .rows = 2 };
    UISetup setup;
    setup.panels = {
        { .name = "small", .placement = GridSpan{} },
        { .name = "fixed", .placement = GridCell{ .column = 0, .row = 0 } },
        { .name = "wide", .placement = GridSpan{ .columns = 3, .rows = 2 } },
        { .name = "floating", .placement = Anchor::Top },
    };
    Store store{ setup };
    layout::preparePanels(store, grid);

    const auto cellOf = [&](std::uint32_t panel) {
        return std::get<GridCell>(store.panel(PanelId{ panel }).placement);
    };
    REQUIRE(cellOf(1).column == 0); // the one with a position keeps it
    REQUIRE(cellOf(1).row == 0);

    // The large panel is placed before the small ones: columns 1 to 3, both rows.
    REQUIRE(cellOf(2).column == 1);
    REQUIRE(cellOf(2).row == 0);
    REQUIRE(cellOf(2).columnSpan == 3);
    REQUIRE(cellOf(2).rowSpan == 2);

    // Only 0, 1 is left for the small one.
    REQUIRE(cellOf(0).column == 0);
    REQUIRE(cellOf(0).row == 1);

    REQUIRE(std::holds_alternative<Anchor>(store.panel(PanelId{ 3 }).placement)); // floating panels are left alone
}

TEST_CASE("a panel that finds no room in the grid is refused", "[ui][layout][panels]") {
    UISetup setup;
    setup.panels = {
        { .name = "Scene", .placement = GridSpan{ .columns = 2 } },
        { .name = "Inspector", .placement = GridSpan{} },
    };
    Store store{ setup };
    REQUIRE_THROWS_AS(layout::preparePanels(store, { .columns = 2, .rows = 1 }), SetupError);
    REQUIRE_THROWS_WITH(
        layout::preparePanels(store, { .columns = 2, .rows = 1 }),
        ContainsSubstring("panel \"Inspector\" finds no room in the window's grid")
    );
    REQUIRE_NOTHROW(layout::preparePanels(store, { .columns = 3, .rows = 1 }));

    setup.panels = { { .name = "Scene", .placement = GridSpan{ .columns = 5 } } };
    Store tooWide{ setup };
    REQUIRE_THROWS_WITH(
        layout::preparePanels(tooWide, { .columns = 4, .rows = 1 }),
        ContainsSubstring("panel \"Scene\" spans 5 columns")
    );
}

TEST_CASE("panels with a position may share cells", "[ui][layout][panels]") {
    Store store = storeOf({ inGrid("editor", { .column = 0 }), inGrid("preview", { .column = 0 }) });
    REQUIRE_NOTHROW(layout::preparePanels(store, {}));
    place(store);
    REQUIRE(rectOf(store, 0) == rectOf(store, 1));
}

TEST_CASE("placements that can never work are refused", "[ui][layout][panels]") {
    const GridSetup grid{ .columns = 3, .rows = 2 };

    SECTION("everything inside the grid is fine") {
        Store store = storeOf(
            {
                inGrid("corner", { .column = 2, .row = 1 }),
                inGrid("all", { .column = 0, .row = 0, .columnSpan = 3, .rowSpan = 2 }),
                floating("floating", Anchor::Top),
            }
        );
        REQUIRE_NOTHROW(layout::preparePanels(store, grid));
    }
    SECTION("a cell outside the grid") {
        Store store = storeOf({ inGrid("Map", { .column = 3, .row = 0 }) });
        REQUIRE_THROWS_AS(layout::preparePanels(store, grid), SetupError);
        REQUIRE_THROWS_WITH(
            layout::preparePanels(store, grid), ContainsSubstring("panel \"Map\" is placed outside the window's grid")
        );
        REQUIRE_THROWS_WITH(layout::preparePanels(store, grid), ContainsSubstring("3 columns and 2 rows"));
    }
    SECTION("a span that reaches outside the grid") {
        Store store = storeOf({ inGrid("Map", { .column = 1, .row = 1, .columnSpan = 3 }) });
        REQUIRE_THROWS_AS(layout::preparePanels(store, grid), SetupError);
    }
    SECTION("a negative cell") {
        Store store = storeOf({ inGrid("Map", { .column = -1 }) });
        REQUIRE_THROWS_AS(layout::preparePanels(store, grid), SetupError);
    }
    SECTION("a span of nothing") {
        Store store = storeOf({ inGrid("Map", { .rowSpan = 0 }) });
        REQUIRE_THROWS_WITH(layout::preparePanels(store, grid), ContainsSubstring("spans less than one grid cell"));
    }
    SECTION("a grid without columns") {
        Store store = storeOf({ floating("Controls", Anchor::Top) });
        REQUIRE_THROWS_WITH(layout::preparePanels(store, { .columns = 0, .rows = 1 }), ContainsSubstring("at least 1"));
    }
    SECTION("a negative width") {
        std::vector<PanelSetup> panels = { floating("Controls", Anchor::Top) };
        panels[0].width = -5.f;
        Store store = storeOf(std::move(panels));
        REQUIRE_THROWS_WITH(
            layout::preparePanels(store, grid), ContainsSubstring("panel \"Controls\" has a negative width")
        );
    }
}
