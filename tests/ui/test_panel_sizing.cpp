#include "ui/layout/arrange.hpp"
#include "ui/layout/widget_layout.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <memory>
#include <optional>
#include <string>
#include <vector>

using namespace atpl;
using atpl::model::Store;

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

class Block final : public Widget {
public:
    explicit Block(SizeRequest request) :
        m_request(request) {}
    [[nodiscard]] SizeRequest measure(const MeasureContext&) const override { return m_request; }
    void paint(Painter&, const Style&) const override {}

private:
    SizeRequest m_request;
};

struct Item {
    std::string name;
    SizeRequest request;
    [[nodiscard]] std::unique_ptr<Widget> create() const { return std::make_unique<Block>(request); }
};

/// A widget that prefers this size and needs at least `least` of it.
Item item(std::string name, sf::Vector2f preferred, sf::Vector2f least = {}) {
    return { .name = std::move(name), .request = { .min = least, .preferred = preferred } };
}

/// Text of a fixed width per character, so tests need no font: half the text size per character.
class FixedWidthText final : public render::TextMeasurer {
public:
    [[nodiscard]] sf::Vector2f measure(std::string_view text, const sf::Font*, float size) const override {
        return { static_cast<float>(text.size()) * size * 0.5f, size };
    }
    [[nodiscard]] float wrappedHeight(std::string_view, const sf::Font*, float size, float) const override {
        return size;
    }
};

const Theme theme;
const sf::Vector2f window(800.f, 600.f);

struct Ui {
    Store store;
    Layout layout;

    Ui(std::vector<PanelSetup> panels, Layout chosen = {}, GridSetup grid = {}) :
        store(setupOf(std::move(panels), chosen)),
        layout(chosen),
        m_grid(grid) {
        layout::prepare(store, m_grid, layout);
    }

    void arrange(sf::Vector2f size = window, const render::TextMeasurer* measurer = nullptr) {
        layout::arrange(store, size, m_grid, theme, sizes(), measurer, layout);
    }

    [[nodiscard]] FloatRect rectOf(std::uint32_t panel) const { return store.panel(PanelId{ panel }).rect; }
    [[nodiscard]] bool shown(std::uint32_t panel) const { return store.panel(PanelId{ panel }).shown; }
    [[nodiscard]] bool drawn(std::uint32_t widget) const { return store.widget(WidgetId{ widget }).visible; }

private:
    static UISetup setupOf(std::vector<PanelSetup> panels, const Layout& layout) {
        UISetup setup;
        setup.layout = layout;
        setup.panels = std::move(panels);
        return setup;
    }
    GridSetup m_grid;
};

PanelSetup floating(std::string name, std::vector<WidgetSetup> widgets, Anchor anchor = Anchor::TopLeft) {
    return { .name = std::move(name), .placement = anchor, .widgets = std::move(widgets) };
}

Layout ownSizes() {
    Layout layout;
    layout.width = SizeRule::Own;
    layout.height = SizeRule::Own;
    return layout;
}

} // namespace

// ----- Floating panels -----

TEST_CASE(
    "a floating panel is as wide as its content prefers, and at least the layout's panel width", "[ui][layout][sizing]"
) {
    Ui ui(
        {
            floating("wide", { item("a", { 300.f, 20.f }) }),
            floating("narrow", { item("b", { 50.f, 20.f }) }, Anchor::TopRight),
        },
        ownSizes()
    );
    ui.arrange();

    REQUIRE(ui.rectOf(0) == FloatRect(10.f, 10.f, 320.f, 70.f)); // 300 and the padding; 30 + 40 high
    REQUIRE(ui.rectOf(1).width() == 200.f);                      // never narrower than the panel width
}

TEST_CASE("a floating panel is at least as wide as its title", "[ui][layout][sizing]") {
    const FixedWidthText measurer;
    PanelSetup panel = floating("Panel", { item("a", { 50.f, 20.f }) });
    panel.title = std::string(40, 'x'); // 40 characters of half the title size each
    Ui ui({ panel }, ownSizes());
    ui.arrange(window, &measurer);

    const float title = 40.f * theme.resolve(Panel::Title).textSize * 0.5f;
    REQUIRE(ui.rectOf(0).width() == title + 20.f);
}

TEST_CASE("floating panels with equal sizes all take the largest", "[ui][layout][sizing]") {
    Layout equal;
    equal.width = SizeRule::Equal;
    equal.height = SizeRule::Equal;
    Ui ui(
        {
            floating("wide", { item("a", { 300.f, 20.f }) }),
            floating("high", { item("b", { 50.f, 100.f }) }, Anchor::TopRight),
            floating("own", { item("c", { 50.f, 20.f }) }, Anchor::BottomLeft),
        },
        equal
    );
    ui.store.panel(PanelId{ 2 }).layout = { .width = SizeRule::Own, .height = SizeRule::Own }; // says otherwise
    ui.arrange();

    REQUIRE(ui.rectOf(0).size() == sf::Vector2f(320.f, 150.f)); // the widest width, the highest height
    REQUIRE(ui.rectOf(1).size() == sf::Vector2f(320.f, 150.f));
    REQUIRE(ui.rectOf(2).size() == sf::Vector2f(200.f, 70.f)); // its own
}

TEST_CASE("the default layout gives floating panels equal widths and their own heights", "[ui][layout][sizing]") {
    Ui ui(
        {
            floating("wide", { item("a", { 300.f, 20.f }) }),
            floating("high", { item("b", { 50.f, 100.f }) }),
        }
    );
    ui.arrange();

    REQUIRE(ui.rectOf(0).size() == sf::Vector2f(320.f, 70.f));
    REQUIRE(ui.rectOf(1).size() == sf::Vector2f(320.f, 150.f));
}

TEST_CASE("a width a panel asks for itself wins over its content", "[ui][layout][sizing]") {
    PanelSetup panel = floating("fixed", { item("a", { 300.f, 20.f }) });
    panel.width = 250.f;
    Ui ui({ panel, floating("other", { item("b", { 400.f, 20.f }) }) });
    ui.arrange({ 1200.f, 600.f }); // wide enough that the limit does not cut it

    REQUIRE(ui.rectOf(0).width() == 250.f);
    REQUIRE(ui.rectOf(1).width() == 420.f); // equal widths are among the panels that do not say
}

TEST_CASE("a floating panel stays within its limit, whatever its content wants", "[ui][layout][sizing]") {
    // The default limit: 480 x 900 pixels, at most half the window's width and nine tenths of
    // its height.
    Ui ui({ floating("big", { item("a", { 1000.f, 2000.f }) }) }, ownSizes());

    ui.arrange({ 800.f, 600.f });
    REQUIRE(ui.rectOf(0).size() == sf::Vector2f(400.f, 540.f)); // half of 800, nine tenths of 600

    ui.arrange({ 2000.f, 1500.f });
    REQUIRE(ui.rectOf(0).size() == sf::Vector2f(480.f, 900.f)); // the fixed size on a large screen

    // What does not fit scrolls.
    REQUIRE(layout::contentOverflow(ui.store.panel(PanelId{ 0 }), sizes()) == 2020.f - 870.f);
}

TEST_CASE("a panel can have a limit of its own", "[ui][layout][sizing]") {
    PanelSetup panel = floating("big", { item("a", { 1000.f, 300.f }) });
    panel.layout.limit = PanelLimit{ .pixels = { 250.f, 900.f }, .windowFraction = { 1.f, 1.f } };
    Ui ui({ panel }, ownSizes());
    ui.arrange();

    REQUIRE(ui.rectOf(0).width() == 250.f);
}

// ----- Grid panels -----

TEST_CASE("a grid panel that fits its content takes only that, at its alignment in its cells", "[ui][layout][sizing]") {
    Layout cards = layouts::cards(); // fit to content, centred
    cards.rows = SizeRule::Own;
    Ui ui(
        { { .name = "card",
            .placement = GridCell{},
            .widgets = { item("a", { 150.f, 20.f }), item("b", { 150.f, 20.f }) } } },
        cards
    );
    ui.arrange();

    // Cell: 780 x 580. Panel: 200 wide (the panel width is wider than its content), 30 + 65 high.
    REQUIRE(ui.rectOf(0) == FloatRect(300.f, 253.f, 200.f, 95.f));

    ui.store.panel(PanelId{ 0 }).layout.alignment = Alignment::BottomRight;
    ui.arrange();
    REQUIRE(ui.rectOf(0) == FloatRect(590.f, 495.f, 200.f, 95.f));
}

TEST_CASE("a grid panel that fills its cells is as large as they are", "[ui][layout][sizing]") {
    Ui ui({ { .name = "pane", .placement = GridCell{}, .widgets = { item("a", { 150.f, 20.f }) } } });
    ui.arrange();
    REQUIRE(ui.rectOf(0) == FloatRect(10.f, 10.f, 780.f, 580.f));
}

TEST_CASE("a collapsed grid panel is its header at the side the layout says", "[ui][layout][sizing]") {
    Ui ui(
        { { .name = "pane", .placement = GridCell{}, .collapsible = true, .widgets = { item("a", { 150.f, 20.f }) } } }
    );
    ui.store.panel(PanelId{ 0 }).collapsed = true;

    ui.arrange();
    REQUIRE(ui.rectOf(0) == FloatRect(10.f, 10.f, 780.f, 30.f)); // the default: towards the top left

    ui.store.panel(PanelId{ 0 }).layout.collapseTowards = Alignment::Bottom;
    ui.arrange();
    REQUIRE(ui.rectOf(0) == FloatRect(10.f, 560.f, 780.f, 30.f));
    REQUIRE_FALSE(ui.drawn(0)); // its widgets are not drawn while it is collapsed
}

TEST_CASE("a grid panel lower than a header is not drawn", "[ui][layout][sizing]") {
    Ui ui({ { .name = "pane", .placement = GridCell{} } });
    ui.arrange({ 800.f, 45.f }); // 25 for the cell
    REQUIRE_FALSE(ui.shown(0));
    ui.arrange({ 800.f, 60.f }); // 40
    REQUIRE(ui.shown(0));
}

TEST_CASE("panels can share the size of their cells, so that equal grids are equal panels", "[ui][layout][sizing]") {
    const GridSetup grid{ .columns = 3, .rows = 1 };
    const auto cardsWith = [&](SizeRule cells) {
        Layout layout = layouts::cards(); // fit to content, centred, equal cells inside each card
        layout.cells = cells;
        return Ui(
            {
                { .name = "small", .widgets = { item("a", { 100.f, 20.f }), item("b", { 100.f, 20.f }) } },
                { .name = "large", .widgets = { item("c", { 300.f, 60.f }), item("d", { 100.f, 20.f }) } },
                { .name = "own", .layout = { .cells = SizeRule::Own }, .widgets = { item("e", { 100.f, 20.f }) } },
            },
            layout,
            grid
        );
    };

    // By default each card's cells are its own: the cards differ.
    Ui own = cardsWith(SizeRule::Own);
    own.arrange({ 1200.f, 600.f });
    REQUIRE(own.rectOf(0).size() == sf::Vector2f(200.f, 95.f));  // two rows of 20
    REQUIRE(own.rectOf(1).size() == sf::Vector2f(320.f, 175.f)); // two rows of 60, 300 wide

    // Shared: every card's cells are as large as the largest, 300 x 60; cards with the same
    // number of cells are the same size. A card that says otherwise keeps its own.
    Ui shared = cardsWith(SizeRule::Equal);
    shared.arrange({ 1200.f, 600.f });
    REQUIRE(shared.rectOf(0).size() == sf::Vector2f(320.f, 175.f));
    REQUIRE(shared.rectOf(1).size() == sf::Vector2f(320.f, 175.f));
    REQUIRE(shared.rectOf(2).size() == sf::Vector2f(200.f, 70.f));
}

TEST_CASE("sharing cells does not touch panels that fill their cells or have no grid", "[ui][layout][sizing]") {
    Layout layout;
    layout.cells = SizeRule::Equal;
    layout.rows = SizeRule::Equal; // floating panels get a grid
    Ui ui(
        {
            { .name = "pane", .placement = GridCell{}, .widgets = { item("a", { 100.f, 20.f }) } },
            floating("floating", { item("b", { 300.f, 100.f }) }),
            floating("small", { item("c", { 50.f, 20.f }) }, Anchor::TopRight),
        },
        layout
    );
    ui.arrange();

    REQUIRE(ui.rectOf(0) == FloatRect(10.f, 10.f, 780.f, 580.f)); // its size comes from the window
    REQUIRE(ui.rectOf(1).height() == 150.f);
    REQUIRE(ui.rectOf(2).height() == 150.f); // shares the floating panel's cell of 300 x 100
}

// ----- What does not fit -----

TEST_CASE("a widget narrower than it needs is not drawn, and its place stays empty", "[ui][layout][sizing]") {
    // Two columns of 87.5 in a panel of 200; the second widget needs at least 120.
    PanelSetup panel = floating(
        "panel", { item("a", { 50.f, 20.f }), item("b", { 50.f, 20.f }, { 120.f, 10.f }), item("c", { 50.f, 20.f }) }
    );
    panel.columns = 2;
    panel.width = 200.f;
    panel.rows = 2;
    Ui ui({ panel });
    ui.arrange();

    REQUIRE(ui.shown(0));
    REQUIRE(ui.drawn(0));
    REQUIRE_FALSE(ui.drawn(1));
    REQUIRE(ui.drawn(2));
    REQUIRE(ui.store.widget(WidgetId{ 2 }).rect.top() > ui.store.widget(WidgetId{ 0 }).rect.top()); // nothing moved up

    ui.store.panel(PanelId{ 0 }).width = 300.f; // wide enough: columns of 137.5
    ui.arrange();
    REQUIRE(ui.drawn(1));
}

TEST_CASE("a panel narrower than its widest widget needs is not drawn", "[ui][layout][sizing]") {
    PanelSetup narrow = floating("narrow", { item("a", { 300.f, 20.f }, { 250.f, 20.f }) });
    narrow.width = 200.f; // 180 for content
    Ui ui({ narrow, floating("below", { item("b", { 50.f, 20.f }) }) });
    ui.arrange();

    REQUIRE_FALSE(ui.shown(0));
    REQUIRE(ui.shown(1));
    REQUIRE(ui.rectOf(1).top() == 10.f); // the stack is placed without it
}

TEST_CASE(
    "a panel lower than its highest widget needs is not drawn, and comes back when there is room",
    "[ui][layout][sizing]"
) {
    // A widget that cannot go below 200; the window's limit leaves the panel less than that.
    Ui ui(
        { floating("tall", { item("a", { 100.f, 300.f }, { 50.f, 200.f }) }),
          floating("small", { item("b", { 50.f, 20.f }) }) },
        ownSizes()
    );

    ui.arrange({ 800.f, 600.f }); // the panel may be 540 high: room enough
    REQUIRE(ui.shown(0));

    ui.arrange({ 800.f, 230.f }); // nine tenths of 230: 207, minus the header leaves 177
    REQUIRE_FALSE(ui.shown(0));
    REQUIRE(ui.shown(1));
    REQUIRE(ui.rectOf(1).top() == 10.f);

    ui.arrange({ 800.f, 600.f }); // and back
    REQUIRE(ui.shown(0));
    REQUIRE(ui.rectOf(1).top() > ui.rectOf(0).bottom());
}

TEST_CASE("a collapsed panel is never too small for its widgets", "[ui][layout][sizing]") {
    PanelSetup narrow = floating("narrow", { item("a", { 300.f, 20.f }, { 250.f, 20.f }) });
    narrow.width = 200.f;
    narrow.collapsed = true;
    Ui ui({ narrow });
    ui.arrange();
    REQUIRE(ui.shown(0)); // its header has room
}

// ----- Folding -----

TEST_CASE("while a panel folds, its content keeps its place and the panels below follow", "[ui][layout][sizing]") {
    Ui ui(
        { floating("folding", { item("a", { 50.f, 20.f }), item("b", { 50.f, 20.f }) }),
          floating("below", { item("c", { 50.f, 20.f }) }) }
    );
    ui.arrange();
    const FloatRect open = ui.rectOf(0);
    const FloatRect widget = ui.store.widget(WidgetId{ 1 }).rect;
    REQUIRE(open.height() == 95.f); // 30 + 10 + 20 + 5 + 20 + 10
    REQUIRE(ui.rectOf(1).top() == 115.f);

    model::Panel& panel = ui.store.panel(PanelId{ 0 });
    panel.collapsed = true;
    panel.opening = 0.5f; // half-way
    ui.arrange();
    REQUIRE(ui.rectOf(0).height() == 63.f); // 30 + 65 / 2, rounded
    REQUIRE(ui.rectOf(1).top() == 83.f);
    REQUIRE(ui.store.widget(WidgetId{ 1 }).rect == widget); // laid out as in the open panel
    REQUIRE(ui.drawn(1));                                   // and still drawn, cut off at the edge

    panel.opening.reset(); // arrived
    ui.arrange();
    REQUIRE(ui.rectOf(0).height() == 30.f);
    REQUIRE_FALSE(ui.drawn(1));
}

TEST_CASE("a panel is never left out while it folds", "[ui][layout][sizing]") {
    // Half-way its content area is lower than its widget needs; that does not count yet.
    Ui ui({ floating("folding", { item("a", { 50.f, 100.f }, { 50.f, 100.f }) }) });
    model::Panel& panel = ui.store.panel(PanelId{ 0 });
    panel.collapsed = true;
    panel.opening = 0.3f;
    ui.arrange();
    REQUIRE(ui.shown(0));
}

// ----- The presets -----

TEST_CASE("each preset arranges the same panels the way it describes", "[ui][layout][sizing]") {
    const GridSetup grid{ .columns = 2, .rows = 1 };
    const auto panels = [] {
        return std::vector<PanelSetup>{
            { .name = "A", .widgets = { item("a", { 100.f, 20.f }) } },
            { .name = "B", .widgets = { item("b", { 100.f, 60.f }) } },
        };
    };

    Ui overlay(panels(), layouts::overlay(), grid); // floating at the top left, equal widths, own heights
    overlay.arrange();
    REQUIRE(overlay.rectOf(0) == FloatRect(10.f, 10.f, 200.f, 70.f));
    REQUIRE(overlay.rectOf(1) == FloatRect(10.f, 90.f, 200.f, 110.f));

    Ui dashboard(panels(), layouts::dashboard(), grid); // filling the grid's cells
    dashboard.arrange();
    REQUIRE(dashboard.rectOf(0) == FloatRect(10.f, 10.f, 385.f, 580.f));
    REQUIRE(dashboard.rectOf(1) == FloatRect(405.f, 10.f, 385.f, 580.f));

    Ui cards(panels(), layouts::cards(), grid); // as large as their content, centred in the cells
    cards.arrange();
    REQUIRE(cards.rectOf(0).size() == sf::Vector2f(200.f, 70.f)); // a grid of one cell each: 30 + 10 + 20 + 10
    REQUIRE(cards.rectOf(1).size() == sf::Vector2f(200.f, 110.f));
    // Centred in their cells, to the pixel.
    const auto centredIn = [](const FloatRect& rect, const FloatRect& cell) {
        const sf::Vector2f offset = rect.center() - cell.center();
        return std::abs(offset.x) <= 0.5f && std::abs(offset.y) <= 0.5f;
    };
    REQUIRE(centredIn(cards.rectOf(0), FloatRect(10.f, 10.f, 385.f, 580.f)));
    REQUIRE(centredIn(cards.rectOf(1), FloatRect(405.f, 10.f, 385.f, 580.f)));
}
