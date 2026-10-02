#include "ui/render/profiler.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <chrono>
#include <string>

using namespace atpl;
using atpl::render::FrameStats;
using atpl::render::Profiler;
using Catch::Approx;
using Section = Profiler::Section;
using std::chrono::microseconds;
using std::chrono::milliseconds;

namespace {

/// The default layout's sizes at its reference window size: nothing is scaled.
const Sizes reference = Layout().sizesAt({ 1280.f, 720.f });

/// A point in time, this long after an arbitrary start.
Profiler::Clock::time_point at(milliseconds sinceStart) {
    return Profiler::Clock::time_point(sinceStart);
}

/// One frame that took this long to build and to submit.
void frame(Profiler& profiler, microseconds build, microseconds submit, std::size_t panelsRebuilt = 0) {
    profiler.record(Section::Build, build);
    profiler.record(Section::Submit, submit);
    profiler.frameDrawn(
        FrameStats{ .drawCalls = 7, .textCalls = 3, .triangles = 120, .panelsDrawn = 2 }, panelsRebuilt, 1
    );
}

bool shows(const Profiler& profiler, std::string_view text) {
    const auto runs = profiler.batch().frame().texts();
    return std::any_of(runs.begin(), runs.end(), [&](const render::TextRun& run) {
        return run.text.find(text) != std::string::npos;
    });
}

} // namespace

TEST_CASE("the profiler averages what frames took and keeps the largest", "[ui][profiler]") {
    Profiler profiler;
    static_cast<void>(profiler.take(at(milliseconds(0))));

    frame(profiler, microseconds(100), microseconds(40));
    frame(profiler, microseconds(300), microseconds(60));
    const Profiler::Readout readout = profiler.take(at(milliseconds(500)));

    REQUIRE(readout.frames == 2);
    REQUIRE(readout.seconds == Approx(0.5));
    REQUIRE(readout.framesPerSecond == Approx(4.0));
    REQUIRE(readout.build.average == Approx(0.2));
    REQUIRE(readout.build.max == Approx(0.3));
    REQUIRE(readout.submit.average == Approx(0.05));
    REQUIRE(readout.submit.max == Approx(0.06));
    REQUIRE(readout.lastFrame.drawCalls == 7);
    REQUIRE(readout.lastFrame.triangles == 120);
}

TEST_CASE("time recorded in pieces adds up within a frame", "[ui][profiler]") {
    Profiler profiler;
    static_cast<void>(profiler.take(at(milliseconds(0))));

    // Three panels painted for one frame.
    profiler.record(Section::Build, microseconds(10));
    profiler.record(Section::Build, microseconds(20));
    profiler.record(Section::Build, microseconds(30));
    profiler.frameDrawn({}, 3, 0);

    const Profiler::Readout readout = profiler.take(at(milliseconds(100)));
    REQUIRE(readout.build.average == Approx(0.06));
    REQUIRE(readout.build.max == Approx(0.06));
    REQUIRE(readout.panelsRebuilt == Approx(3.0));
}

TEST_CASE("taking the readout starts a new stretch", "[ui][profiler]") {
    Profiler profiler;
    static_cast<void>(profiler.take(at(milliseconds(0))));
    frame(profiler, microseconds(100), microseconds(100), 2);
    profiler.frameSkipped();
    profiler.frameSkipped();

    const Profiler::Readout first = profiler.take(at(milliseconds(100)));
    REQUIRE(first.frames == 1);
    REQUIRE(first.skipped == 2);
    REQUIRE(first.panelsRebuilt == Approx(2.0));
    REQUIRE(first.textsBuilt == Approx(1.0));

    const Profiler::Readout second = profiler.take(at(milliseconds(200)));
    REQUIRE(second.frames == 0);
    REQUIRE(second.skipped == 0);
    REQUIRE(second.framesPerSecond == 0.0);
    REQUIRE(second.build.average == 0.0);
    REQUIRE(second.build.max == 0.0);
    REQUIRE(second.seconds == Approx(0.1));
}

TEST_CASE("a dropped frame leaves no trace in the numbers", "[ui][profiler]") {
    Profiler profiler;
    static_cast<void>(profiler.take(at(milliseconds(0))));

    profiler.record(Section::Submit, microseconds(900));
    profiler.dropFrame();
    frame(profiler, microseconds(0), microseconds(100));

    const Profiler::Readout readout = profiler.take(at(milliseconds(100)));
    REQUIRE(readout.frames == 1);
    REQUIRE(readout.submit.average == Approx(0.1));
    REQUIRE(readout.submit.max == Approx(0.1));
}

TEST_CASE("a scope times what happens inside it", "[ui][profiler]") {
    Profiler profiler;
    static_cast<void>(profiler.take(at(milliseconds(0))));
    { const auto scope = profiler.measure(Section::Build); }
    profiler.frameDrawn({}, 0, 0);
    const Profiler::Readout readout = profiler.take(at(milliseconds(1)));
    REQUIRE(readout.build.average >= 0.0);
    REQUIRE(readout.build.average < 50.0); // an empty scope takes next to nothing

    // A scope without a profiler does nothing, and must not crash.
    const Profiler::Scope none(nullptr, Section::Build);
}

TEST_CASE("the readout is off by default and then does nothing", "[ui][profiler]") {
    Profiler profiler;
    profiler.setLook(Theme(), reference);

    REQUIRE_FALSE(profiler.isVisible());
    REQUIRE_FALSE(profiler.batch().takeChanged()); // being off is not a change to draw
    REQUIRE_FALSE(profiler.refresh(at(milliseconds(0))));
    REQUIRE_FALSE(profiler.refresh(at(milliseconds(1000))));
    REQUIRE(profiler.batch().frame().empty());
}

TEST_CASE("the readout takes its size and its look from the theme", "[ui][profiler]") {
    Theme theme;
    Profiler profiler;
    profiler.setLook(theme, reference);
    profiler.setVisible(true);
    REQUIRE(profiler.refresh(at(milliseconds(0))));

    const sf::Vector2f size = profiler.batch().size();
    REQUIRE(size.x > 100.f);
    REQUIRE(size.y > 5.f * theme.typography.text.size);

    // One label and one value per line, in the theme's text colours.
    const auto runs = profiler.batch().frame().texts();
    REQUIRE(runs.size() == 2 * Profiler::lineCount);
    REQUIRE(runs[0].text == "frames");
    REQUIRE(runs[0].color == theme.resolve(Profiler::Label).color);
    REQUIRE(runs[1].color == theme.resolve(Profiler::Value).color);
    REQUIRE_FALSE(profiler.batch().frame().shapes().empty()); // the background

    // Larger sizes give a larger readout.
    Layout doubled;
    doubled.metrics.scale = 2.f;
    Profiler large;
    large.setLook(theme, doubled.sizesAt({ 1280.f, 720.f }));
    REQUIRE(large.batch().size().x > size.x * 1.5f);
}

TEST_CASE("the readout is refreshed five times per second, not every frame", "[ui][profiler]") {
    Profiler profiler;
    profiler.setLook(Theme(), reference);
    profiler.setVisible(true);
    REQUIRE(profiler.refresh(at(milliseconds(0)))); // painted when it appears

    frame(profiler, microseconds(100), microseconds(50));
    REQUIRE_FALSE(profiler.refresh(at(milliseconds(16))));
    frame(profiler, microseconds(100), microseconds(50));
    REQUIRE_FALSE(profiler.refresh(at(milliseconds(199))));
    REQUIRE(profiler.batch().rebuildCount() == 1);

    REQUIRE(profiler.refresh(at(milliseconds(200))));
    REQUIRE(profiler.batch().rebuildCount() == 2);
    REQUIRE(shows(profiler, "10 per second")); // two frames in 0.2 s
    REQUIRE(shows(profiler, "0.100 ms"));
    REQUIRE(shows(profiler, "7 calls, 120 triangles"));
}

TEST_CASE("a readout whose text stays the same is not painted again", "[ui][profiler]") {
    Profiler profiler;
    profiler.setLook(Theme(), reference);
    profiler.setVisible(true);
    profiler.refresh(at(milliseconds(0)));

    // The same two frames in every stretch: the same text every time.
    for (int stretch = 1; stretch <= 3; ++stretch) {
        frame(profiler, microseconds(100), microseconds(50));
        frame(profiler, microseconds(100), microseconds(50));
        profiler.refresh(at(milliseconds(200 * stretch)));
    }
    REQUIRE(profiler.batch().rebuildCount() == 2); // when it appeared, and for the first numbers
}

TEST_CASE("an idle application stays idle with the readout on", "[ui][profiler]") {
    Profiler profiler;
    profiler.setLook(Theme(), reference);
    profiler.setVisible(true);
    profiler.refresh(at(milliseconds(0)));
    frame(profiler, microseconds(100), microseconds(50));
    profiler.refresh(at(milliseconds(200)));
    const std::size_t paintedWhileBusy = profiler.batch().rebuildCount();

    // No more frames. The readout says so once ...
    profiler.frameSkipped();
    REQUIRE(profiler.refresh(at(milliseconds(400))));
    REQUIRE(shows(profiler, "idle"));
    REQUIRE(shows(profiler, "0.100 ms")); // what the last frames took stays readable
    REQUIRE(profiler.batch().rebuildCount() == paintedWhileBusy + 1);

    // ... and then asks for nothing more, however long it lasts and however many frames are skipped.
    for (int stretch = 3; stretch <= 50; ++stretch) {
        profiler.frameSkipped();
        REQUIRE_FALSE(profiler.refresh(at(milliseconds(200 * stretch))));
    }
    REQUIRE(profiler.batch().rebuildCount() == paintedWhileBusy + 1);
}

TEST_CASE("switching the readout on starts measuring afresh", "[ui][profiler]") {
    Profiler profiler;
    profiler.setLook(Theme(), reference);

    // Frames from before anybody looked.
    static_cast<void>(profiler.take(at(milliseconds(0))));
    frame(profiler, microseconds(100), microseconds(50));

    profiler.setVisible(true);
    REQUIRE(profiler.batch().takeChanged()); // appearing is a change to draw
    profiler.refresh(at(milliseconds(5000)));
    REQUIRE(profiler.readout().frames == 0);

    profiler.setVisible(false);
    REQUIRE(profiler.batch().takeChanged()); // and so is disappearing
}
