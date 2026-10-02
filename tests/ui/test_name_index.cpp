#include "atpl/ui/error.hpp"

#include "ui/model/name_index.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

using namespace atpl;
using atpl::model::NameIndex;
using Catch::Matchers::ContainsSubstring;

namespace {

/// Two panels that both have a "Speed", and one widget that exists only once.
NameIndex example() {
    NameIndex names;
    names.addPanel("Controls", PanelId{ 0 });
    names.addPanel("Physics", PanelId{ 1 });
    names.addWidget("Controls", "Speed", WidgetId{ 0 });
    names.addWidget("Controls", "Reset", WidgetId{ 1 });
    names.addWidget("Physics", "Speed", WidgetId{ 2 });
    names.addView("world", ViewId{ 0 });
    names.addView("minimap", ViewId{ 1 });
    return names;
}

} // namespace

TEST_CASE("panels and views are found by their name", "[ui][model][names]") {
    const NameIndex names = example();

    REQUIRE(names.panel("Controls") == PanelId{ 0 });
    REQUIRE(names.panel("Physics") == PanelId{ 1 });
    REQUIRE(names.view("world") == ViewId{ 0 });
    REQUIRE(names.view("minimap") == ViewId{ 1 });
}

TEST_CASE("a widget is found by its name alone if only one panel has it", "[ui][model][names]") {
    const NameIndex names = example();

    REQUIRE(names.widget("Reset") == WidgetId{ 1 });
}

TEST_CASE("a widget is always found by panel and name", "[ui][model][names]") {
    const NameIndex names = example();

    REQUIRE(names.widget("Controls/Speed") == WidgetId{ 0 });
    REQUIRE(names.widget("Controls/Reset") == WidgetId{ 1 });
    REQUIRE(names.widget("Physics/Speed") == WidgetId{ 2 });
}

TEST_CASE(
    "a name that fits several widgets is refused, and the message says how to tell them apart", "[ui][model][names]"
) {
    const NameIndex names = example();

    REQUIRE_THROWS_AS(names.widget("Speed"), SetupError);
    REQUIRE_THROWS_WITH(names.widget("Speed"), ContainsSubstring("\"Controls/Speed\" or \"Physics/Speed\""));
}

TEST_CASE("unknown names are refused with a message that names them", "[ui][model][names]") {
    const NameIndex names = example();

    REQUIRE_THROWS_AS(names.panel("Controlls"), SetupError);
    REQUIRE_THROWS_WITH(names.panel("Controlls"), ContainsSubstring("no panel named \"Controlls\""));
    REQUIRE_THROWS_WITH(names.panel("Controlls"), ContainsSubstring("\"Controls\", \"Physics\"")); // what there is

    REQUIRE_THROWS_AS(names.widget("Sped"), SetupError);
    REQUIRE_THROWS_WITH(names.widget("Sped"), ContainsSubstring("no widget named \"Sped\""));

    REQUIRE_THROWS_AS(names.view("map"), SetupError);
    REQUIRE_THROWS_WITH(names.view("map"), ContainsSubstring("no view named \"map\""));
    REQUIRE_THROWS_WITH(names.view("map"), ContainsSubstring("\"minimap\", \"world\""));
}

TEST_CASE("a wrong \"Panel/Name\" says which half is wrong", "[ui][model][names]") {
    const NameIndex names = example();

    REQUIRE_THROWS_WITH(names.widget("Fysics/Speed"), ContainsSubstring("no panel named \"Fysics\""));
    REQUIRE_THROWS_WITH(
        names.widget("Physics/Reset"), ContainsSubstring("panel \"Physics\" has no widget named \"Reset\"")
    );
}

TEST_CASE("a name is never found as something it is not", "[ui][model][names]") {
    const NameIndex names = example();

    REQUIRE_THROWS_AS(names.widget("Controls"), SetupError); // a panel, not a widget
    REQUIRE_THROWS_AS(names.panel("Reset"), SetupError);     // a widget, not a panel
    REQUIRE_THROWS_AS(names.view("Controls"), SetupError);
    REQUIRE_THROWS_AS(names.widget(""), SetupError);
    REQUIRE_THROWS_AS(names.widget("/"), SetupError);
}

TEST_CASE("duplicate names are refused when they are added", "[ui][model][names]") {
    NameIndex names = example();

    REQUIRE_THROWS_AS(names.addPanel("Controls", PanelId{ 2 }), SetupError);
    REQUIRE_THROWS_WITH(
        names.addPanel("Controls", PanelId{ 2 }), ContainsSubstring("two panels are named \"Controls\"")
    );

    REQUIRE_THROWS_AS(names.addWidget("Controls", "Speed", WidgetId{ 3 }), SetupError);
    REQUIRE_THROWS_WITH(
        names.addWidget("Controls", "Speed", WidgetId{ 3 }),
        ContainsSubstring("panel \"Controls\" has two widgets named \"Speed\"")
    );

    REQUIRE_THROWS_AS(names.addView("world", ViewId{ 2 }), SetupError);
    REQUIRE_THROWS_WITH(names.addView("world", ViewId{ 2 }), ContainsSubstring("two views are named \"world\""));

    // What was refused changed nothing.
    REQUIRE(names.panel("Controls") == PanelId{ 0 });
    REQUIRE(names.widget("Controls/Speed") == WidgetId{ 0 });
    REQUIRE(names.widget("Reset") == WidgetId{ 1 });
    REQUIRE(names.view("world") == ViewId{ 0 });
}

TEST_CASE("a name must not contain the separator", "[ui][model][names]") {
    NameIndex names;

    REQUIRE_THROWS_AS(names.addPanel("Left/Right", PanelId{ 0 }), SetupError);
    REQUIRE_THROWS_WITH(names.addPanel("Left/Right", PanelId{ 0 }), ContainsSubstring("\"Left/Right\" contains '/'"));
    names.addPanel("Controls", PanelId{ 0 });
    REQUIRE_THROWS_AS(names.addWidget("Controls", "On/Off", WidgetId{ 0 }), SetupError);
    REQUIRE_THROWS_AS(names.addView("a/b", ViewId{ 0 }), SetupError);
}

TEST_CASE("panels, widgets and views have separate names", "[ui][model][names]") {
    NameIndex names;
    names.addPanel("Map", PanelId{ 0 });
    names.addWidget("Map", "Map", WidgetId{ 0 });
    names.addView("Map", ViewId{ 0 });

    REQUIRE(names.panel("Map") == PanelId{ 0 });
    REQUIRE(names.widget("Map") == WidgetId{ 0 });
    REQUIRE(names.widget("Map/Map") == WidgetId{ 0 });
    REQUIRE(names.view("Map") == ViewId{ 0 });
}
