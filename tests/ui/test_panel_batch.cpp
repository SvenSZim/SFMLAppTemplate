#include "ui/render/panel_batch.hpp"
#include "ui/render/renderer.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace atpl;
using atpl::render::PanelBatch;
using atpl::render::scissorFor;
using Catch::Approx;

namespace {

PartStyle solid(sf::Color color) {
    PartStyle style;
    style.color = color;
    return style;
}

void paint(PanelBatch& batch) {
    const auto layers = batch.rebuild();
    render::appendBox(layers.frame.shapes(), FloatRect(0.f, 0.f, 100.f, 60.f), solid(sf::Color::White));
    render::appendBox(layers.content.shapes(), FloatRect(10.f, 10.f, 20.f, 20.f), solid(sf::Color::Red));
    layers.content.addText(FloatRect(10.f, 40.f, 80.f, 16.f), "label", solid(sf::Color::Black), Align::Left, false);
}

} // namespace

TEST_CASE("a new batch has nothing to draw and says so", "[ui][panel_batch]") {
    const PanelBatch batch;

    REQUIRE(batch.isDirty());
    REQUIRE(batch.rebuildCount() == 0);
    REQUIRE(batch.frame().empty());
    REQUIRE(batch.content().empty());
    REQUIRE(batch.isVisible());
}

TEST_CASE("rebuilding fills both layers and makes the batch clean", "[ui][panel_batch]") {
    PanelBatch batch;
    paint(batch);

    REQUIRE_FALSE(batch.isDirty());
    REQUIRE(batch.rebuildCount() == 1);
    REQUIRE(batch.frame().shapes().size() == 6);
    REQUIRE(batch.content().shapes().size() == 6);
    REQUIRE(batch.content().texts().size() == 1);
}

TEST_CASE("rebuilding again starts from empty layers", "[ui][panel_batch]") {
    PanelBatch batch;
    paint(batch);
    batch.markDirty();
    REQUIRE(batch.isDirty());

    paint(batch);

    REQUIRE(batch.rebuildCount() == 2);
    REQUIRE(batch.frame().shapes().size() == 6); // not doubled
    REQUIRE(batch.content().texts().size() == 1);
}

TEST_CASE("moving a panel does not make its batch dirty", "[ui][panel_batch]") {
    PanelBatch batch;
    paint(batch);

    batch.setPosition({ 300.f, 120.f });

    REQUIRE_FALSE(batch.isDirty());
    REQUIRE(batch.position() == sf::Vector2f(300.f, 120.f));
    REQUIRE(batch.frameTransform().transformPoint({ 0.f, 0.f }) == sf::Vector2f(300.f, 120.f));
    REQUIRE(batch.frameTransform().transformPoint({ 10.f, 5.f }) == sf::Vector2f(310.f, 125.f));
}

TEST_CASE("scrolling moves only the content, and does not make the batch dirty", "[ui][panel_batch]") {
    PanelBatch batch;
    paint(batch);
    batch.setPosition({ 300.f, 120.f });

    batch.setScroll(40.f);

    REQUIRE_FALSE(batch.isDirty());
    REQUIRE(batch.scroll() == 40.f);
    REQUIRE(batch.contentTransform().transformPoint({ 0.f, 0.f }) == sf::Vector2f(300.f, 80.f)); // 40 further up
    REQUIRE(batch.frameTransform().transformPoint({ 0.f, 0.f }) == sf::Vector2f(300.f, 120.f));  // the frame stays
}

TEST_CASE("a new size makes the batch dirty, the same size does not", "[ui][panel_batch]") {
    PanelBatch batch;
    batch.setSize({ 100.f, 60.f });
    paint(batch);

    batch.setSize({ 100.f, 60.f });
    REQUIRE_FALSE(batch.isDirty());

    batch.setSize({ 100.f, 200.f });
    REQUIRE(batch.isDirty());
    REQUIRE(batch.size() == sf::Vector2f(100.f, 200.f));
}

TEST_CASE("the clip area moves with the panel and not with the scrolling", "[ui][panel_batch]") {
    PanelBatch batch;
    REQUIRE_FALSE(batch.clipInWindow().has_value());

    batch.setContentClip(FloatRect(0.f, 30.f, 100.f, 50.f));
    batch.setPosition({ 300.f, 120.f });
    batch.setScroll(25.f);

    REQUIRE(batch.clipInWindow() == FloatRect(300.f, 150.f, 100.f, 50.f));
    REQUIRE(batch.rebuildCount() == 0); // placing and clipping rebuilt nothing

    batch.setContentClip(std::nullopt);
    REQUIRE_FALSE(batch.clipInWindow().has_value());
}

TEST_CASE("hiding a batch keeps its geometry", "[ui][panel_batch]") {
    PanelBatch batch;
    paint(batch);

    batch.setVisible(false);

    REQUIRE_FALSE(batch.isVisible());
    REQUIRE_FALSE(batch.isDirty());
    REQUIRE(batch.frame().shapes().size() == 6);
}

TEST_CASE("a clip rectangle in pixels becomes the share of the target it covers", "[ui][renderer]") {
    const sf::FloatRect scissor = scissorFor(FloatRect(100.f, 50.f, 200.f, 100.f), { 800u, 400u });

    REQUIRE(scissor.position.x == Approx(0.125f));
    REQUIRE(scissor.position.y == Approx(0.125f));
    REQUIRE(scissor.size.x == Approx(0.25f));
    REQUIRE(scissor.size.y == Approx(0.25f));
}

TEST_CASE("a clip rectangle that reaches beyond the target is cut to it", "[ui][renderer]") {
    const sf::FloatRect partly = scissorFor(FloatRect(-50.f, 300.f, 200.f, 400.f), { 800u, 400u });
    REQUIRE(partly.position.x == Approx(0.f));
    REQUIRE(partly.position.y == Approx(0.75f));
    REQUIRE(partly.size.x == Approx(0.1875f)); // 150 of 800
    REQUIRE(partly.size.y == Approx(0.25f));   // down to the edge

    const sf::FloatRect outside = scissorFor(FloatRect(900.f, 0.f, 50.f, 50.f), { 800u, 400u });
    REQUIRE(outside.size.x == 0.f);

    const sf::FloatRect noTarget = scissorFor(FloatRect(0.f, 0.f, 50.f, 50.f), { 0u, 0u });
    REQUIRE(noTarget.size.x == 0.f);
    REQUIRE(noTarget.size.y == 0.f);
}
