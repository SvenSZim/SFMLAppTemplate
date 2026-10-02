#include "ui/layout/cell.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace atpl;
using atpl::layout::aligned;
using atpl::layout::placeInCell;
using atpl::layout::sizeIn;

namespace {

const FloatRect cell(100.f, 50.f, 200.f, 80.f);

SizeRequest dynamic(sf::Vector2f min = { 40.f, 20.f }) {
    return { .min = min };
}

SizeRequest constant(sf::Vector2f max, sf::Vector2f min = { 40.f, 20.f }) {
    return { .min = min, .max = max };
}

} // namespace

TEST_CASE("a dynamic widget fills its cell", "[ui][layout][cell]") {
    REQUIRE(placeInCell(dynamic(), cell, Alignment::Center) == cell);
    REQUIRE(placeInCell(dynamic(), cell, Alignment::TopLeft) == cell); // nothing to align
}

TEST_CASE("a constant widget is at most as large as its maximum", "[ui][layout][cell]") {
    REQUIRE(sizeIn(constant({ 120.f, 30.f }), { 200.f, 80.f }) == sf::Vector2f(120.f, 30.f));
    REQUIRE(
        sizeIn(constant({ 1000.f, 30.f }), { 200.f, 80.f }) == sf::Vector2f(200.f, 30.f)
    ); // wide enough: the cell's width
    REQUIRE(sizeIn(constant({ 120.f, 300.f }), { 200.f, 80.f }) == sf::Vector2f(120.f, 80.f));

    // A maximum below the minimum or the preferred size makes no sense: the larger counts.
    REQUIRE(sizeIn(constant({ 10.f, 10.f }), { 200.f, 80.f }) == sf::Vector2f(40.f, 20.f));
    SizeRequest preferring = constant({ 10.f, 10.f });
    preferring.preferred = { 60.f, 30.f };
    REQUIRE(sizeIn(preferring, { 200.f, 80.f }) == sf::Vector2f(60.f, 30.f));
}

TEST_CASE("what is smaller than its cell is placed in it at one of nine positions", "[ui][layout][cell]") {
    const SizeRequest slider = constant({ 120.f, 30.f });

    // Across: 100, 140 or 180. Down: 50, 75 or 100.
    REQUIRE(placeInCell(slider, cell, Alignment::TopLeft) == FloatRect(100.f, 50.f, 120.f, 30.f));
    REQUIRE(placeInCell(slider, cell, Alignment::Top) == FloatRect(140.f, 50.f, 120.f, 30.f));
    REQUIRE(placeInCell(slider, cell, Alignment::TopRight) == FloatRect(180.f, 50.f, 120.f, 30.f));
    REQUIRE(placeInCell(slider, cell, Alignment::Left) == FloatRect(100.f, 75.f, 120.f, 30.f));
    REQUIRE(placeInCell(slider, cell, Alignment::Center) == FloatRect(140.f, 75.f, 120.f, 30.f));
    REQUIRE(placeInCell(slider, cell, Alignment::Right) == FloatRect(180.f, 75.f, 120.f, 30.f));
    REQUIRE(placeInCell(slider, cell, Alignment::BottomLeft) == FloatRect(100.f, 100.f, 120.f, 30.f));
    REQUIRE(placeInCell(slider, cell, Alignment::Bottom) == FloatRect(140.f, 100.f, 120.f, 30.f));
    REQUIRE(placeInCell(slider, cell, Alignment::BottomRight) == FloatRect(180.f, 100.f, 120.f, 30.f));
}

TEST_CASE("a widget keeps within the limits on its shape", "[ui][layout][cell]") {
    // At most four times as wide as high: 80 high allows 320, so the cell's 200 are fine.
    SizeRequest wide = dynamic();
    wide.widestRatio = 4.f;
    REQUIRE(sizeIn(wide, { 200.f, 80.f }) == sf::Vector2f(200.f, 80.f));
    REQUIRE(sizeIn(wide, { 200.f, 30.f }) == sf::Vector2f(120.f, 30.f)); // a low cell: narrower

    // At most half as high as wide.
    SizeRequest flat = dynamic();
    flat.tallestRatio = 0.5f;
    REQUIRE(sizeIn(flat, { 100.f, 80.f }) == sf::Vector2f(100.f, 50.f));

    // Both together pin the shape: a view that stays 16:9 takes the largest such rectangle.
    SizeRequest view = dynamic();
    view.widestRatio = 16.f / 9.f;
    view.tallestRatio = 9.f / 16.f;
    REQUIRE(sizeIn(view, { 320.f, 400.f }) == sf::Vector2f(320.f, 180.f)); // the width limits
    REQUIRE(sizeIn(view, { 800.f, 90.f }) == sf::Vector2f(160.f, 90.f));   // the height limits
    REQUIRE(
        placeInCell(view, FloatRect(0.f, 0.f, 320.f, 400.f), Alignment::Center) == FloatRect(0.f, 110.f, 320.f, 180.f)
    );
}

TEST_CASE("a widget is never larger than its cell, even below its minimum", "[ui][layout][cell]") {
    // What happens to a widget that does not fit is decided by the overflow rules; here it only
    // must not stick out.
    REQUIRE(placeInCell(dynamic({ 300.f, 100.f }), cell, Alignment::Center) == cell);
    REQUIRE(placeInCell(constant({ 500.f, 500.f }, { 300.f, 100.f }), cell, Alignment::Center) == cell);
}

TEST_CASE("aligning cuts a size that is larger than the area", "[ui][layout][cell]") {
    REQUIRE(aligned({ 500.f, 20.f }, cell, Alignment::Bottom) == FloatRect(100.f, 110.f, 200.f, 20.f));
    REQUIRE(aligned({ 50.f, 500.f }, cell, Alignment::Right) == FloatRect(250.f, 50.f, 50.f, 80.f));
}

TEST_CASE("placed rectangles are on whole pixels", "[ui][layout][cell]") {
    const FloatRect odd(0.f, 0.f, 101.f, 51.f);
    const FloatRect placed = placeInCell(constant({ 40.f, 20.f }), odd, Alignment::Center);
    REQUIRE(placed == FloatRect(31.f, 16.f, 40.f, 20.f)); // 30.5 and 15.5, rounded
}
