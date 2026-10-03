// These tests open a small window: they carry the CTest label "display".

#include "ui/frame_loop.hpp"
#include "ui/render/renderer.hpp"

#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/System/Clock.hpp>
#include <SFML/System/Sleep.hpp>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <ctime>
#include <thread>

using namespace atpl;
using atpl::frame::RedrawFlag;
using atpl::render::PanelBatch;
using atpl::render::Renderer;

namespace {

const sf::Color background(20, 20, 20);

void paint(PanelBatch& batch, sf::Color color) {
    PartStyle style;
    style.color = color;
    render::appendBox(batch.rebuild().frame.shapes(), FloatRect(0.f, 0.f, 60.f, 40.f), style);
}

struct Fixture {
    sf::RenderWindow window{ sf::VideoMode({ 240u, 160u }), "atpl frame test" };
    Renderer renderer;
    RedrawFlag flag;
    PanelBatch first;
    PanelBatch second;
    std::array<PanelBatch*, 2> panels{ &first, &second };

    Fixture() {
        paint(first, sf::Color::Red);
        paint(second, sf::Color::Green);
        second.setPosition({ 100.f, 50.f });
    }

    bool present() { return renderer.present(window, background, flag, panels).has_value(); }

    /// Runs the main loop as an application would, for a while, and returns the passes it made.
    int runFor(sf::Time duration) {
        const sf::Clock clock;
        int passes = 0;
        while (clock.getElapsedTime() < duration) {
            for (auto event = frame::nextEvent(window, flag); event; event = frame::pendingEvent(window, flag)) {}
            present();
            ++passes;
        }
        return passes;
    }
};

} // namespace

TEST_CASE("the first frame is drawn, the next ones are skipped while nothing changes", "[ui][frame][display]") {
    Fixture f;

    REQUIRE(f.present());
    REQUIRE_FALSE(f.present());
    REQUIRE_FALSE(f.present());
    REQUIRE_FALSE(f.present());

    REQUIRE(f.renderer.framesDrawn() == 1);
    REQUIRE(f.renderer.framesSkipped() == 3);
}

TEST_CASE("a request causes exactly one frame", "[ui][frame][display]") {
    Fixture f;
    f.present();

    f.flag.request();
    REQUIRE(f.present());
    REQUIRE_FALSE(f.present());
    REQUIRE(f.renderer.framesDrawn() == 2);
}

TEST_CASE("a change in any panel causes exactly one frame, without a request", "[ui][frame][display]") {
    Fixture f;
    f.present();

    f.second.setPosition({ 120.f, 60.f }); // moved
    REQUIRE(f.present());
    REQUIRE_FALSE(f.present());

    paint(f.first, sf::Color::Blue); // repainted
    REQUIRE(f.present());
    REQUIRE_FALSE(f.present());

    f.second.setScroll(10.f); // scrolled
    REQUIRE(f.present());
    REQUIRE_FALSE(f.present());

    // Several changes at once still make one frame.
    f.first.setPosition({ 5.f, 5.f });
    f.second.setVisible(false);
    f.flag.request();
    REQUIRE(f.present());
    REQUIRE_FALSE(f.present());
}

TEST_CASE("a drawn frame reports what it took, a skipped one reports nothing", "[ui][frame][display]") {
    Fixture f;

    const auto drawn = f.renderer.present(f.window, background, f.flag, f.panels);
    REQUIRE(drawn.has_value());
    REQUIRE(drawn->panelsDrawn == 2);
    REQUIRE(drawn->drawCalls == 2);

    const auto skipped = f.renderer.present(f.window, background, f.flag, f.panels);
    REQUIRE_FALSE(skipped.has_value());
}

TEST_CASE("an idle application draws no frames and sleeps instead of spinning", "[ui][frame][display]") {
    Fixture f;
    f.runFor(sf::milliseconds(150)); // let the window settle: the events of its creation may ask for frames
    const std::size_t framesBefore = f.renderer.framesDrawn();

    const std::clock_t cpuBefore = std::clock();
    const int passes = f.runFor(sf::milliseconds(400));
    const double cpuSeconds = static_cast<double>(std::clock() - cpuBefore) / CLOCKS_PER_SEC;

    // Nothing asked for a frame: none was drawn.
    REQUIRE(f.renderer.framesDrawn() == framesBefore);

    // The loop slept between passes: about one pass per 16 ms, not thousands. (No lower bound:
    // on a busy machine the system may let one wait run much longer than asked, which is
    // harmless and says nothing about the loop.)
    REQUIRE(passes <= 60);
    REQUIRE(passes >= 1);

    // And used next to no processor time while doing so.
    REQUIRE(cpuSeconds < 0.1);
}

TEST_CASE("a request from another thread is drawn within about one wait", "[ui][frame][display]") {
    Fixture f;
    f.runFor(sf::milliseconds(150));
    const std::size_t framesBefore = f.renderer.framesDrawn();

    std::thread simulation([&f] {
        sf::sleep(sf::milliseconds(60));
        f.flag.request(); // "a new state is ready"
    });
    f.runFor(sf::milliseconds(200));
    simulation.join();

    REQUIRE(f.renderer.framesDrawn() >= framesBefore + 1);
}

TEST_CASE("while a frame is asked for, the loop does not wait for events", "[ui][frame][display]") {
    Fixture f;
    f.runFor(sf::milliseconds(150));

    f.flag.request();
    const sf::Clock clock;
    while (frame::nextEvent(f.window, f.flag, sf::milliseconds(500)).has_value()) {}

    REQUIRE(clock.getElapsedTime() < sf::milliseconds(100)); // far less than the 500 it could have waited
}
