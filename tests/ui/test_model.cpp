#include "atpl/ui/error.hpp"
#include "atpl/ui/setup.hpp"
#include "atpl/ui/widget.hpp"

#include "ui/model/store.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <memory>
#include <string>
#include <utility>
#include <variant>

using namespace atpl;
using atpl::model::Store;
using Catch::Matchers::ContainsSubstring;

namespace {

// Widgets and descriptors as an application would write its own. The built-in ones are not
// needed to test the model, and do not exist yet.

class PlainWidget final : public Widget {
public:
    explicit PlainWidget(int tag) :
        m_tag(tag) {}
    [[nodiscard]] int tag() const { return m_tag; }

    [[nodiscard]] SizeRequest measure(const MeasureContext&) const override { return {}; }
    void paint(Painter&, const Style&) const override {}

private:
    int m_tag;
};

int widgetsMade = 0;

struct Plain {
    std::string name;
    int tag = 0;
    [[nodiscard]] std::unique_ptr<Widget> create() const {
        ++widgetsMade;
        return std::make_unique<PlainWidget>(tag);
    }
};

/// A region the application draws into, like `View`.
struct Region {
    static constexpr bool isView = true;
    std::string name;
    [[nodiscard]] std::unique_ptr<Widget> create() const { return std::make_unique<PlainWidget>(0); }
};

/// A descriptor with a mistake in it.
struct Broken {
    std::string name;
    [[nodiscard]] std::unique_ptr<Widget> create() const { return nullptr; }
};

/// Two panels that both have a "Speed", a background view and a view widget.
UISetup example() {
    return {
        .background = "world",
        .panels = {
            {
                .name = "Controls",
                .title = "Simulation controls",
                .placement = Anchor::TopRight,
                .columns = 2,
                .width = 320.f,
                .collapsed = true,
                .widgets = { Plain{ "Speed", 1 }, at({.column = 1, .row = 2}, Plain{ "Reset", 2 }) },
            },
            {
                .name = "Physics",
                .placement = GridCell{.column = 1, .row = 0},
                .accent = 0,
                .collapsible = false,
                .widgets = { Plain{ "Speed", 3 }, Region{ "minimap" }, Plain{ "Gravity", 4 } },
            },
            { .name = "Empty" },
        },
    };
}

int tagOf(const model::WidgetSlot& slot) {
    return dynamic_cast<const PlainWidget&>(*slot.widget).tag();
}

} // namespace

TEST_CASE("ids are positions: panels in setup order, widgets panel by panel", "[ui][model]") {
    const Store store{ example() };

    REQUIRE(store.panels().size() == 3);
    REQUIRE(store.panel(PanelId{ 0 }).name == "Controls");
    REQUIRE(store.panel(PanelId{ 1 }).name == "Physics");
    REQUIRE(store.panel(PanelId{ 2 }).name == "Empty");

    REQUIRE(store.widgets().size() == 5);
    REQUIRE(store.widget(WidgetId{ 0 }).name == "Speed");
    REQUIRE(store.widget(WidgetId{ 0 }).panel == PanelId{ 0 });
    REQUIRE(store.widget(WidgetId{ 1 }).name == "Reset");
    REQUIRE(store.widget(WidgetId{ 2 }).name == "Speed");
    REQUIRE(store.widget(WidgetId{ 2 }).panel == PanelId{ 1 });
    REQUIRE(store.widget(WidgetId{ 4 }).name == "Gravity");
    REQUIRE(store.widget(WidgetId{ 4 }).panel == PanelId{ 1 });
}

TEST_CASE("a panel's widgets are a row of slots in the order listed", "[ui][model]") {
    const Store store{ example() };

    const auto controls = store.widgetsOf(PanelId{ 0 });
    REQUIRE(controls.size() == 2);
    REQUIRE(tagOf(controls[0]) == 1);
    REQUIRE(tagOf(controls[1]) == 2);
    REQUIRE(store.firstWidgetOf(PanelId{ 0 }) == WidgetId{ 0 });

    const auto physics = store.widgetsOf(PanelId{ 1 });
    REQUIRE(physics.size() == 3);
    REQUIRE(tagOf(physics[0]) == 3);
    REQUIRE(tagOf(physics[2]) == 4);
    REQUIRE(store.firstWidgetOf(PanelId{ 1 }) == WidgetId{ 2 });
    REQUIRE(&physics[0] == &store.widget(WidgetId{ 2 })); // the same slots, not copies

    REQUIRE(store.widgetsOf(PanelId{ 2 }).empty());
}

TEST_CASE("every descriptor makes its widget exactly once", "[ui][model]") {
    const UISetup setup = example();
    widgetsMade = 0;
    const Store store{ setup };

    REQUIRE(widgetsMade == 4); // the four `Plain` ones
    for (const model::WidgetSlot& slot : store.widgets()) {
        REQUIRE(slot.widget != nullptr);
    }
}

TEST_CASE("a panel takes over what its setup says", "[ui][model]") {
    const Store store{ example() };

    const model::Panel& controls = store.panel(PanelId{ 0 });
    REQUIRE(controls.title == "Simulation controls");
    REQUIRE(std::get<Anchor>(controls.placement) == Anchor::TopRight);
    REQUIRE(controls.columns == 2);
    REQUIRE(controls.width == 320.f);
    REQUIRE(controls.collapsible);
    REQUIRE(controls.collapsed);
    REQUIRE(controls.visible);
    REQUIRE(controls.dirty); // never painted yet

    const model::Panel& physics = store.panel(PanelId{ 1 });
    REQUIRE(physics.title == "Physics"); // no title: the name
    REQUIRE(std::get<GridCell>(physics.placement).column == 1);
    REQUIRE(physics.columns == 1);
    REQUIRE_FALSE(physics.collapsible);
    REQUIRE_FALSE(physics.collapsed);
}

TEST_CASE("a widget slot starts out visible, enabled and untouched", "[ui][model]") {
    const Store store{ example() };

    const model::WidgetSlot& speed = store.widget(WidgetId{ 0 });
    REQUIRE_FALSE(speed.cell.has_value());
    REQUIRE(speed.visible);
    REQUIRE(speed.enabled);
    REQUIRE_FALSE(speed.binding.has_value());
    REQUIRE(model::stateOf(speed) == State::Normal);

    const model::WidgetSlot& reset = store.widget(WidgetId{ 1 });
    REQUIRE(reset.cell.has_value());
    REQUIRE(reset.cell->column == 1);
    REQUIRE(reset.cell->row == 2);
}

TEST_CASE("a slot's state is what input and the application wrote into it", "[ui][model]") {
    Store store{ example() };
    model::WidgetSlot& slot = store.widget(WidgetId{ 0 });

    slot.hovered = true;
    REQUIRE(model::stateOf(slot) == State::Hovered);
    slot.pressed = true;
    slot.focused = true;
    REQUIRE(model::stateOf(slot) == (State::Hovered | State::Pressed | State::Focused));
    slot.hovered = slot.pressed = slot.focused = false;
    slot.enabled = false;
    REQUIRE(model::stateOf(slot) == State::Disabled);
}

TEST_CASE("widgets are found by name, and by panel and name", "[ui][model]") {
    const Store store{ example() };
    const model::NameIndex& names = store.names();

    REQUIRE(names.panel("Physics") == PanelId{ 1 });
    REQUIRE(names.widget("Reset") == WidgetId{ 1 });
    REQUIRE(names.widget("Gravity") == WidgetId{ 4 });
    REQUIRE(names.widget("Controls/Speed") == WidgetId{ 0 });
    REQUIRE(names.widget("Physics/Speed") == WidgetId{ 2 });

    REQUIRE_THROWS_AS(names.widget("Speed"), SetupError); // which one?
    REQUIRE_THROWS_AS(names.widget("Mass"), SetupError);
    REQUIRE_THROWS_AS(names.panel("Graphics"), SetupError);
}

TEST_CASE("the background and every view widget are views", "[ui][model]") {
    const Store store{ example() };

    REQUIRE(store.views().size() == 2);
    REQUIRE(store.backgroundView() == ViewId{ 0 });
    REQUIRE(store.view(ViewId{ 0 }).name == "world");
    REQUIRE_FALSE(store.view(ViewId{ 0 }).widget.has_value()); // not a widget

    REQUIRE(store.names().view("minimap") == ViewId{ 1 });
    const model::View& minimap = store.view(ViewId{ 1 });
    REQUIRE(minimap.widget == WidgetId{ 3 });
    REQUIRE_FALSE(minimap.draw); // nothing to draw until the application says what

    // The view widget is a widget like any other, and knows its view.
    REQUIRE(store.names().widget("minimap") == WidgetId{ 3 });
    REQUIRE(store.widget(WidgetId{ 3 }).view == ViewId{ 1 });
    REQUIRE_FALSE(store.widget(WidgetId{ 2 }).view.has_value());
}

TEST_CASE("without a background view, views are the view widgets only", "[ui][model]") {
    UISetup setup = example();
    setup.background.clear();
    const Store store{ setup };

    REQUIRE_FALSE(store.backgroundView().has_value());
    REQUIRE(store.views().size() == 1);
    REQUIRE(store.names().view("minimap") == ViewId{ 0 });
}

TEST_CASE("an empty setup is a UI without anything", "[ui][model]") {
    const Store store{ UISetup{} };

    REQUIRE(store.panels().empty());
    REQUIRE(store.widgets().empty());
    REQUIRE(store.views().empty());
    REQUIRE_THROWS_WITH(store.names().panel("Controls"), ContainsSubstring("the UI has no panels"));
}

TEST_CASE("duplicate names in the setup are refused", "[ui][model]") {
    SECTION("two panels") {
        UISetup setup = example();
        setup.panels.push_back({ .name = "Controls" });
        REQUIRE_THROWS_AS(Store{ setup }, SetupError);
        REQUIRE_THROWS_WITH(Store{ setup }, ContainsSubstring("two panels are named \"Controls\""));
    }
    SECTION("two widgets in one panel") {
        UISetup setup = example();
        setup.panels[1].widgets.push_back(Plain{ "Gravity" });
        REQUIRE_THROWS_AS(Store{ setup }, SetupError);
        REQUIRE_THROWS_WITH(Store{ setup }, ContainsSubstring("panel \"Physics\" has two widgets named \"Gravity\""));
    }
    SECTION("two view widgets, even in different panels") {
        UISetup setup = example();
        setup.panels[0].widgets.push_back(Region{ "minimap" });
        REQUIRE_THROWS_AS(Store{ setup }, SetupError);
        REQUIRE_THROWS_WITH(Store{ setup }, ContainsSubstring("two views are named \"minimap\""));
    }
    SECTION("a view widget named like the background view") {
        UISetup setup = example();
        setup.panels[0].widgets.push_back(Region{ "world" });
        REQUIRE_THROWS_WITH(Store{ setup }, ContainsSubstring("two views are named \"world\""));
    }
}

TEST_CASE("names that cannot be used are refused", "[ui][model]") {
    SECTION("a panel without a name") {
        UISetup setup = example();
        setup.panels[1].name.clear();
        REQUIRE_THROWS_WITH(Store{ setup }, ContainsSubstring("panel 2 of the setup has no name"));
    }
    SECTION("a widget without a name") {
        UISetup setup = example();
        setup.panels[1].widgets.push_back(Plain{ "" });
        REQUIRE_THROWS_WITH(Store{ setup }, ContainsSubstring("widget 4 of panel \"Physics\" has no name"));
    }
    SECTION("a separator in a panel's name") {
        UISetup setup = example();
        setup.panels[0].name = "Left/Right";
        REQUIRE_THROWS_AS(Store{ setup }, SetupError);
    }
    SECTION("a separator in a widget's name") {
        UISetup setup = example();
        setup.panels[0].widgets.push_back(Plain{ "On/Off" });
        REQUIRE_THROWS_AS(Store{ setup }, SetupError);
    }
    SECTION("a separator in the background view's name") {
        UISetup setup = example();
        setup.background = "main/world";
        REQUIRE_THROWS_AS(Store{ setup }, SetupError);
    }
}

TEST_CASE("a descriptor that makes no widget is refused", "[ui][model]") {
    UISetup setup = example();
    setup.panels[0].widgets.push_back(Broken{ "Oops" });

    REQUIRE_THROWS_AS(Store{ setup }, SetupError);
    REQUIRE_THROWS_WITH(Store{ setup }, ContainsSubstring("\"Controls/Oops\" made no widget"));
}

TEST_CASE("a widget has its panel's colours unless it was given its own", "[ui][model]") {
    UISetup setup;
    setup.theme = themes::colorful(); // four main colours, four accents
    setup.panels = {
        {
            .name = "Playback",
            .main1 = 2,
            .main2 = 3,
            .accent = 1,
            .widgets = {
                Plain{ "Play" },
                colored({.accent = 3}, Plain{ "Delete" }),
                colored({.main1 = 0, .main2 = 1}, Plain{ "Info" }),
                at({.row = 4}, colored({.accent = 2}, Plain{ "Placed" })),
            },
        },
        { .name = "Defaults", .widgets = { Plain{ "Plain" } } },
    };
    const Store store{ setup };

    const PanelColors panel = store.panel(PanelId{ 0 }).colors;
    REQUIRE(panel.main1 == 2);
    REQUIRE(panel.main2 == 3);
    REQUIRE(panel.accent == 1);

    const auto colorsOf = [&](std::uint32_t widget) { return store.widget(WidgetId{ widget }).colors; };
    REQUIRE(colorsOf(0).main1 == 2); // all three from the panel
    REQUIRE(colorsOf(0).main2 == 3);
    REQUIRE(colorsOf(0).accent == 1);

    REQUIRE(colorsOf(1).main1 == 2); // only the accent differs
    REQUIRE(colorsOf(1).main2 == 3);
    REQUIRE(colorsOf(1).accent == 3);

    REQUIRE(colorsOf(2).main1 == 0); // only the main colours differ
    REQUIRE(colorsOf(2).main2 == 1);
    REQUIRE(colorsOf(2).accent == 1);

    REQUIRE(colorsOf(3).accent == 2); // `colored` and `at` together
    REQUIRE(store.widget(WidgetId{ 3 }).cell->row == 4);

    // A panel that names no colours has the theme's first ones.
    REQUIRE(store.panel(PanelId{ 1 }).colors.main1 == 0);
    REQUIRE(store.panel(PanelId{ 1 }).colors.main2 == 1);
    REQUIRE(store.panel(PanelId{ 1 }).colors.accent == 0);
    REQUIRE(colorsOf(4).accent == 0);
}

TEST_CASE("giving a widget colours twice keeps what the second does not name", "[ui][model]") {
    const WidgetSetup widget = colored({ .main1 = 2 }, colored({ .main1 = 1, .accent = 3 }, Plain{ "Twice" }));

    REQUIRE(widget.colors().main1 == 2);
    REQUIRE_FALSE(widget.colors().main2.has_value());
    REQUIRE(widget.colors().accent == 3);
    REQUIRE(widget.name() == "Twice");
    REQUIRE_FALSE(widget.cell().has_value());
    REQUIRE_FALSE(widget.isView());
    REQUIRE(WidgetSetup(Region{ "map" }).isView());
}

TEST_CASE("colours the theme does not have are refused when the UI is built", "[ui][model]") {
    // The default theme has three main colours and one accent.
    SECTION("a panel's accent") {
        UISetup setup = example();
        setup.panels[1].accent = 1;
        REQUIRE_THROWS_AS(Store{ setup }, SetupError);
        REQUIRE_THROWS_WITH(
            Store{ setup }, ContainsSubstring("panel \"Physics\" uses accent colour 1 (accent), but the theme has 1")
        );
    }
    SECTION("a panel's main colour") {
        UISetup setup = example();
        setup.panels[0].main2 = 7;
        REQUIRE_THROWS_WITH(Store{ setup }, ContainsSubstring("panel \"Controls\" uses main colour 7 (main2)"));
        REQUIRE_THROWS_WITH(Store{ setup }, ContainsSubstring("numbered 0 to 2"));
    }
    SECTION("a widget's own colour") {
        UISetup setup = example();
        setup.panels[0].widgets.push_back(colored({ .accent = 3 }, Plain{ "Delete" }));
        REQUIRE_THROWS_AS(Store{ setup }, SetupError);
        REQUIRE_THROWS_WITH(Store{ setup }, ContainsSubstring("widget \"Controls/Delete\" uses accent colour 3"));
    }
}

TEST_CASE("a theme can be checked against a UI before it replaces another", "[ui][model]") {
    UISetup setup;
    setup.theme = themes::colorful();
    setup.panels = {
        { .name = "Playback", .accent = 2, .widgets = { colored({ .accent = 3 }, Plain{ "Delete" }) } },
    };
    const Store store{ setup };

    REQUIRE_NOTHROW(store.requireColors(themes::colorful()));

    // The black and white theme has a single accent.
    REQUIRE_THROWS_AS(store.requireColors(themes::moon()), SetupError);
    REQUIRE_THROWS_WITH(
        store.requireColors(themes::moon()), ContainsSubstring("panel \"Playback\" uses accent colour 2")
    );

    // A theme with enough colours of its own fits.
    Theme wide = themes::moon();
    wide.palette.accents.resize(4, wide.palette.accents.front());
    REQUIRE_NOTHROW(store.requireColors(wide));
}

TEST_CASE("a store can be moved, and its ids and names stay valid", "[ui][model]") {
    Store first{ example() };
    const Store second = std::move(first);

    REQUIRE(second.panels().size() == 3);
    REQUIRE(second.widget(second.names().widget("Gravity")).name == "Gravity");
    REQUIRE(tagOf(second.widget(WidgetId{ 4 })) == 4);
}
