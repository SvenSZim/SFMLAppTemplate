// These tests open a small window: they carry the CTest label "display".

#include "ui/render/profiler.hpp"
#include "ui/render/renderer.hpp"

#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/System/Clock.hpp>
#include <SFML/System/Sleep.hpp>

#include <catch2/catch_test_macros.hpp>

#include <array>

using namespace atpl;
using atpl::frame::RedrawFlag;
using atpl::render::PanelBatch;
using atpl::render::Profiler;
using atpl::render::Renderer;

namespace {

const sf::Color background(20, 20, 20);

void paint(PanelBatch& batch, sf::Color color) {
    PartStyle style;
    style.color = color;
    render::appendBox(batch.rebuild().frame.shapes(), FloatRect(0.f, 0.f, 60.f, 40.f), style);
}

struct Fixture {
    sf::RenderWindow window{ sf::VideoMode({ 320u, 200u }), "atpl profiler test" };
    Renderer renderer;
    Profiler profiler;
    RedrawFlag flag;
    PanelBatch first;
    PanelBatch second;
    std::array<PanelBatch*, 2> panels{ &first, &second };

    Fixture() {
        paint(first, sf::Color::Red);
        paint(second, sf::Color::Green);
        second.setPosition({ 100.f, 50.f });
        profiler.setLook(
            Theme(), Layout().sizesAt({ 1280.f, 720.f })
        ); // no font: the readout has a background and no glyphs
        renderer.setProfiler(&profiler);
    }

    bool present() { return renderer.present(window, background, flag, panels).has_value(); }

    /// Presents again and again for a while. The window's events are left alone on purpose: on a
    /// desktop the window may gain the focus at any moment, which rightly asks for a frame, and
    /// this is about what the readout asks for.
    void runFor(sf::Time duration) {
        const sf::Clock clock;
        while (clock.getElapsedTime() < duration) {
            present();
            sf::sleep(sf::milliseconds(5));
        }
    }
};

} // namespace

TEST_CASE("the renderer reports every frame to the profiler", "[ui][profiler][display]") {
    Fixture f;
    static_cast<void>(f.profiler.take(Profiler::Clock::now()));

    REQUIRE(f.present()); // both panels were painted for it
    REQUIRE_FALSE(f.present());
    paint(f.second, sf::Color::Blue);
    REQUIRE(f.present()); // one panel was painted for it
    REQUIRE_FALSE(f.present());

    const Profiler::Readout readout = f.profiler.take(Profiler::Clock::now());
    REQUIRE(readout.frames == 2);
    REQUIRE(readout.skipped == 2);
    REQUIRE(readout.panelsRebuilt == 1.5);
    REQUIRE(readout.lastFrame.drawCalls == 2);
    REQUIRE(readout.lastFrame.textCalls == 0);
    REQUIRE(readout.lastFrame.panelsDrawn == 2);
    REQUIRE(readout.submit.average > 0.0);
    REQUIRE(readout.show.average > 0.0);
}

TEST_CASE("the readout does not count itself", "[ui][profiler][display]") {
    Fixture f;
    f.present();
    static_cast<void>(f.profiler.take(Profiler::Clock::now()));

    // Switching the readout on needs a frame, but nothing in the UI changed.
    f.profiler.setVisible(true);
    REQUIRE(f.present());
    REQUIRE_FALSE(f.present());

    const Profiler::Readout own = f.profiler.take(Profiler::Clock::now());
    REQUIRE(own.frames == 0);
    REQUIRE(own.submit.max == 0.0);

    // A frame the UI needs is counted, and without the readout's own draw calls.
    f.flag.request();
    const auto stats = f.renderer.present(f.window, background, f.flag, f.panels);
    REQUIRE(stats.has_value());
    REQUIRE(stats->drawCalls == 2);
    REQUIRE(stats->panelsDrawn == 2);
    REQUIRE(f.profiler.take(Profiler::Clock::now()).frames == 1);
}

TEST_CASE("switching the readout off draws one frame, to remove it", "[ui][profiler][display]") {
    Fixture f;
    f.profiler.setVisible(true);
    f.present();
    REQUIRE_FALSE(f.present());

    f.profiler.setVisible(false);
    REQUIRE(f.present());
    REQUIRE_FALSE(f.present());
}

TEST_CASE("an idle application draws no frames with the readout on", "[ui][profiler][display]") {
    Fixture f;
    f.profiler.setVisible(true);

    // Long enough for the readout to show the first frames and then that nothing is drawn any more.
    f.runFor(sf::milliseconds(700));
    const std::size_t settled = f.renderer.framesDrawn();
    REQUIRE(settled <= 3); // the first frame, the first numbers, and "idle"

    f.runFor(sf::milliseconds(700)); // more than three refresh intervals
    REQUIRE(f.renderer.framesDrawn() == settled);
}
