#include "ui/frame_loop.hpp"
#include "ui/render/panel_batch.hpp"

#include <catch2/catch_test_macros.hpp>

#include <thread>
#include <vector>

using namespace atpl;
using atpl::frame::RedrawFlag;
using atpl::render::PanelBatch;

// ----- The redraw flag -----

TEST_CASE("the first frame is always asked for", "[ui][frame]") {
    RedrawFlag flag;

    REQUIRE(flag.isSet());
    REQUIRE(flag.take());
}

TEST_CASE("taking a request clears it until the next one", "[ui][frame]") {
    RedrawFlag flag;
    REQUIRE(flag.take());

    REQUIRE_FALSE(flag.isSet());
    REQUIRE_FALSE(flag.take());

    flag.request();
    REQUIRE(flag.isSet()); // looking does not clear
    REQUIRE(flag.isSet());
    REQUIRE(flag.take());
    REQUIRE_FALSE(flag.take());
}

TEST_CASE("requests do not add up: one frame serves them all", "[ui][frame]") {
    RedrawFlag flag;
    (void)flag.take();

    flag.request();
    flag.request();
    flag.request();

    REQUIRE(flag.take());
    REQUIRE_FALSE(flag.take());
}

TEST_CASE("a frame can be asked for from other threads", "[ui][frame]") {
    RedrawFlag flag;
    (void)flag.take();

    std::vector<std::thread> threads;
    for (int i = 0; i < 4; ++i) {
        threads.emplace_back([&flag] {
            for (int k = 0; k < 1000; ++k) {
                flag.request();
            }
        });
    }
    for (std::thread& thread : threads) {
        thread.join();
    }

    REQUIRE(flag.take());
    REQUIRE_FALSE(flag.take());
}

// ----- What makes a batch need a frame -----

namespace {

void paint(PanelBatch& batch) {
    PartStyle style;
    style.color = sf::Color::White;
    render::appendBox(batch.rebuild().frame.shapes(), FloatRect(0.f, 0.f, 50.f, 50.f), style);
}

/// A batch that has been painted and presented: nothing about it is new.
PanelBatch settled() {
    PanelBatch batch;
    paint(batch);
    (void)batch.takeChanged();
    return batch;
}

} // namespace

TEST_CASE("a new batch needs a frame, and then no more until something changes", "[ui][frame]") {
    PanelBatch batch;
    paint(batch);

    REQUIRE(batch.takeChanged());
    REQUIRE_FALSE(batch.takeChanged());
    REQUIRE_FALSE(batch.takeChanged());
}

TEST_CASE("repainting, moving, scrolling, clipping, showing and hiding each need a frame", "[ui][frame]") {
    PanelBatch batch = settled();

    paint(batch);
    REQUIRE(batch.takeChanged());

    batch.setPosition({ 10.f, 20.f });
    REQUIRE(batch.takeChanged());

    batch.setScroll(15.f);
    REQUIRE(batch.takeChanged());

    batch.setContentClip(FloatRect(0.f, 0.f, 50.f, 30.f));
    REQUIRE(batch.takeChanged());

    batch.setVisible(false);
    REQUIRE(batch.takeChanged());

    batch.setVisible(true);
    REQUIRE(batch.takeChanged());

    REQUIRE_FALSE(batch.takeChanged());
}

TEST_CASE("setting what is already set needs no frame", "[ui][frame]") {
    PanelBatch batch = settled();
    batch.setPosition({ 10.f, 20.f });
    batch.setScroll(15.f);
    batch.setContentClip(FloatRect(0.f, 0.f, 50.f, 30.f));
    (void)batch.takeChanged();

    batch.setPosition({ 10.f, 20.f });
    batch.setScroll(15.f);
    batch.setContentClip(FloatRect(0.f, 0.f, 50.f, 30.f));
    batch.setVisible(true);

    REQUIRE_FALSE(batch.takeChanged());
}

TEST_CASE("a batch that is only marked dirty needs no frame until it is repainted", "[ui][frame]") {
    PanelBatch batch = settled();

    batch.markDirty();
    REQUIRE_FALSE(batch.takeChanged()); // nothing new to show yet

    paint(batch);
    REQUIRE(batch.takeChanged());
}
