#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <chrono>
#include <thread>

#include "ui/core/container/container.hpp"
#include "ui/core/container/container_manager.hpp"

using namespace ui::core::container;
using namespace ui::utils;

TEST_CASE("Container creation and state", "[Container]") {
    ContainerManager manager;

    ContainerSetup setup{
        .title = "Test",
        .rect = FloatRect(10.f, 20.f, 100.f, 30.f)
    };
    ContainerID id = manager.createContainer(std::move(setup));

    REQUIRE(id == 1);
    REQUIRE(manager.getCount() == 1);

    auto& cont = manager.getContainer(id);
    REQUIRE(cont.title == "Test");
    REQUIRE(cont.isExpanded() == false);
    REQUIRE(cont.isDisabled() == false);
}

TEST_CASE("Container expand and collapse", "[Container]") {
    ContainerManager manager;
    ContainerID id = manager.createContainer({
        .title = "Toggle",
        .rect = FloatRect(0.f, 0.f, 100.f, 25.f)
    });

    auto& cont = manager.getContainer(id);
    cont.collapsedRect = FloatRect(0.f, 0.f, 100.f, 25.f);
    cont.expandedRect = FloatRect(0.f, 0.f, 100.f, 200.f);

    REQUIRE(!cont.isExpanded());

    cont.expand();
    REQUIRE(cont.isExpanded());
    REQUIRE(cont.flag.flags.visual == 1);
    REQUIRE(cont.flag.flags.layout == 1);

    cont.flag.flags.visual = 0;
    cont.flag.flags.layout = 0;

    cont.collapse();
    REQUIRE(!cont.isExpanded());
    REQUIRE(cont.flag.flags.visual == 1);
}

TEST_CASE("Container toggle", "[Container]") {
    ContainerManager manager;
    ContainerID id = manager.createContainer({
        .title = "ToggleTest",
        .rect = FloatRect(0.f, 0.f, 50.f, 20.f)
    });

    auto& cont = manager.getContainer(id);
    cont.collapsedRect = FloatRect(0.f, 0.f, 50.f, 20.f);
    cont.expandedRect = FloatRect(0.f, 0.f, 50.f, 150.f);

    REQUIRE(!cont.isExpanded());
    cont.toggle();
    REQUIRE(cont.isExpanded());
    cont.toggle();
    REQUIRE(!cont.isExpanded());
}

TEST_CASE("Disabled container cannot expand", "[Container]") {
    ContainerManager manager;
    ContainerID id = manager.createContainer({
        .title = "Disabled",
        .rect = FloatRect(0.f, 0.f, 50.f, 20.f)
    });

    auto& cont = manager.getContainer(id);
    cont.state.state.disabled = 1;

    cont.expand();
    REQUIRE(!cont.isExpanded());
}

TEST_CASE("Multiple containers have unique IDs", "[Container]") {
    ContainerManager manager;
    ContainerID id1 = manager.createContainer({.title = "A"});
    ContainerID id2 = manager.createContainer({.title = "B"});
    ContainerID id3 = manager.createContainer({.title = "C"});

    REQUIRE(id1 == 1);
    REQUIRE(id2 == 2);
    REQUIRE(id3 == 3);
    REQUIRE(manager.getCount() == 3);
}
