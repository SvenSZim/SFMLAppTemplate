#include "ui/layout/packing.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <vector>

using atpl::layout::PackId;
using atpl::layout::PackItem;
using atpl::layout::packWidgets;
using atpl::layout::resolveSize;
using Catch::Approx;

TEST_CASE("a single widget fills the single column", "[ui][packing]") {
    const std::vector<PackItem> items = {{1, 30.f}};
    const auto result = packWidgets(items, 200.f, 1, 400.f);

    REQUIRE(result.placements.size() == 1);
    REQUIRE_FALSE(result.overflows);
    REQUIRE(result.placements[0].id == 1);
    REQUIRE(result.placements[0].rect.left() == Approx(4.f));
    REQUIRE(result.placements[0].rect.top() == Approx(4.f));
    REQUIRE(result.placements[0].rect.width() == Approx(192.f));
    REQUIRE(result.placements[0].rect.height() == Approx(30.f));
    REQUIRE(result.contentHeight == Approx(38.f)); // padding + 30 + padding
}

TEST_CASE("widgets in one column are stacked in order", "[ui][packing]") {
    const std::vector<PackItem> items = {{7, 30.f}, {8, 30.f}, {9, 30.f}};
    const auto result = packWidgets(items, 200.f, 1, 400.f);

    REQUIRE(result.placements.size() == 3);
    REQUIRE_FALSE(result.overflows);
    REQUIRE(result.placements[0].id == 7);
    REQUIRE(result.placements[1].id == 8);
    REQUIRE(result.placements[2].id == 9);
    REQUIRE(result.placements[0].rect.top() == Approx(4.f));
    REQUIRE(result.placements[1].rect.top() == Approx(38.f)); // 4 + 30 + spacing
    REQUIRE(result.placements[2].rect.top() == Approx(72.f));
    REQUIRE(result.contentHeight == Approx(106.f));
}

TEST_CASE("equal widgets are split evenly over two columns", "[ui][packing]") {
    const std::vector<PackItem> items = {{1, 40.f}, {2, 40.f}, {3, 40.f}, {4, 40.f}};
    const auto result = packWidgets(items, 200.f, 2, 400.f);

    REQUIRE(result.placements.size() == 4);
    REQUIRE_FALSE(result.overflows);

    // column width: (200 - 2 * 4 padding - 4 spacing) / 2 = 94
    REQUIRE(result.placements[0].rect.width() == Approx(94.f));
    REQUIRE(result.placements[0].rect.left() == Approx(4.f));
    REQUIRE(result.placements[1].rect.left() == Approx(4.f));
    REQUIRE(result.placements[2].rect.left() == Approx(102.f));
    REQUIRE(result.placements[3].rect.left() == Approx(102.f));
    REQUIRE(result.placements[2].rect.top() == Approx(4.f));
    REQUIRE(result.contentHeight == Approx(92.f)); // 4 + 40 + 4 + 40 + 4
}

TEST_CASE("columns grow until a tall widget fits", "[ui][packing]") {
    // An even split would be too low for the first widget, so the column height is raised.
    const std::vector<PackItem> items = {{1, 100.f}, {2, 10.f}, {3, 10.f}, {4, 10.f}};
    const auto result = packWidgets(items, 200.f, 2, 400.f);

    REQUIRE(result.placements.size() == 4);
    REQUIRE_FALSE(result.overflows);
    REQUIRE(result.placements[0].rect.left() == Approx(4.f));
    REQUIRE(result.placements[1].rect.left() == Approx(102.f));
    REQUIRE(result.placements[2].rect.left() == Approx(102.f));
    REQUIRE(result.placements[3].rect.left() == Approx(102.f));
    REQUIRE(result.contentHeight == Approx(108.f)); // the tall widget decides: 4 + 100 + 4
}

TEST_CASE("too many widgets overflow but are all placed", "[ui][packing]") {
    std::vector<PackItem> items;
    for (PackId id = 1; id <= 20; ++id) {
        items.push_back({id, 50.f});
    }
    const auto result = packWidgets(items, 200.f, 1, 100.f);

    REQUIRE(result.overflows);
    REQUIRE(result.placements.size() == 20);
    REQUIRE(result.contentHeight > 100.f);
}

TEST_CASE("overflow fills the earlier columns first", "[ui][packing]") {
    std::vector<PackItem> items;
    for (PackId id = 1; id <= 9; ++id) {
        items.push_back({id, 50.f});
    }
    const auto result = packWidgets(items, 300.f, 3, 120.f);

    REQUIRE(result.overflows);
    REQUIRE(result.placements.size() == 9);
    // two per column fit into 120; the rest runs over in the last column
    REQUIRE(result.placements[1].rect.left() == result.placements[0].rect.left());
    REQUIRE(result.placements[2].rect.left() > result.placements[1].rect.left());
    REQUIRE(result.placements[4].rect.left() > result.placements[3].rect.left());
    REQUIRE(result.placements[8].rect.left() == result.placements[4].rect.left());
}

TEST_CASE("nothing to place gives an empty result", "[ui][packing]") {
    const std::vector<PackItem> none;
    const std::vector<PackItem> one = {{1, 30.f}};

    REQUIRE(packWidgets(none, 200.f, 1, 400.f).placements.empty());
    REQUIRE(packWidgets(one, 200.f, 0, 400.f).placements.empty());
}

TEST_CASE("resolveSize follows the reference dimension within its limits", "[ui][packing]") {
    REQUIRE(resolveSize(20.f, 100.f, 0.1f, 500.f) == Approx(50.f));
    REQUIRE(resolveSize(20.f, 100.f, 0.1f, 50.f) == Approx(20.f));
    REQUIRE(resolveSize(20.f, 100.f, 0.5f, 500.f) == Approx(100.f));
}
