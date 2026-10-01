#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "ui/core/layout/widget_packing.hpp"

using namespace ui::core::layout;

TEST_CASE("Pack single widget in single column", "[WidgetPacking]") {
    std::vector<PackItem> items = {{1, 30.f}};
    auto result = packWidgets(items, 200.f, 1, 400.f);

    REQUIRE(result.placements.size() == 1);
    REQUIRE(!result.overflows);
    REQUIRE(result.placements[0].id == 1);
    REQUIRE(result.placements[0].rect.width() == Catch::Approx(192.f));
}

TEST_CASE("Pack multiple widgets in single column", "[WidgetPacking]") {
    std::vector<PackItem> items = {{1, 30.f}, {2, 30.f}, {3, 30.f}};
    auto result = packWidgets(items, 200.f, 1, 400.f);

    REQUIRE(result.placements.size() == 3);
    REQUIRE(!result.overflows);
    REQUIRE(result.placements[0].rect.top() < result.placements[1].rect.top());
    REQUIRE(result.placements[1].rect.top() < result.placements[2].rect.top());
}

TEST_CASE("Pack widgets across two columns", "[WidgetPacking]") {
    std::vector<PackItem> items = {{1, 40.f}, {2, 40.f}, {3, 40.f}, {4, 40.f}};
    auto result = packWidgets(items, 200.f, 2, 400.f);

    REQUIRE(result.placements.size() == 4);
    REQUIRE(!result.overflows);

    float col1X = result.placements[0].rect.left();
    float col2X = result.placements.back().rect.left();
    bool multiColumn = false;
    for (const auto& p : result.placements) {
        if (p.rect.left() != col1X) {
            multiColumn = true;
            break;
        }
    }
    REQUIRE(multiColumn);
}

TEST_CASE("Overflow when exceeding max height", "[WidgetPacking]") {
    std::vector<PackItem> items;
    for (int i = 0; i < 20; i++) {
        items.push_back({static_cast<WidgetID>(i + 1), 50.f});
    }
    auto result = packWidgets(items, 200.f, 1, 100.f);

    REQUIRE(result.overflows);
    REQUIRE(result.placements.size() == 20);
}

TEST_CASE("resolveSize clamps correctly", "[WidgetPacking]") {
    REQUIRE(resolveSize(20.f, 100.f, 0.1f, 500.f) == Catch::Approx(50.f));
    REQUIRE(resolveSize(20.f, 100.f, 0.1f, 50.f) == Catch::Approx(20.f));
    REQUIRE(resolveSize(20.f, 100.f, 0.5f, 500.f) == Catch::Approx(100.f));
}
