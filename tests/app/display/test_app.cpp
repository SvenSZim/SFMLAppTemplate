#include "atpl/app/app.hpp"
#include "atpl/ui/error.hpp"

#include <SFML/Window/VideoMode.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <atomic>
#include <stdexcept>
#include <thread>

using namespace atpl;
using namespace std::chrono_literals;

namespace {

AppSetup small() {
    AppSetup setup;
    setup.window = { .title = "atpl test", .size = { 400u, 300u } };
    setup.ui.panels = { { .name = "Panel", .widgets = { Button("Go") } } };
    return setup;
}

} // namespace

TEST_CASE("an app opens its window as set up, with the font from its resources", "[app][display]") {
    App app(small());
    REQUIRE(app.window().isOpen());
    REQUIRE(app.window().getSize() == sf::Vector2u(400u, 300u));
    REQUIRE(app.ui().theme().font != nullptr); // loaded from resources/fonts/default.ttf
    REQUIRE(std::filesystem::exists(app.resources().path("fonts/default.ttf")));
}

TEST_CASE("a GUI scale enlarges the window and the UI's sizes alike", "[app][display]") {
    AppSetup setup = small();
    setup.window.minimumSize = { 200u, 150u };
    setup.scale = 1.5f;
    App app(std::move(setup));
    REQUIRE(app.window().getSize() == sf::Vector2u(600u, 450u)); // the same size on the screen
    REQUIRE(app.ui().scale() == 1.5f);

    AppSetup plain = small();
    App unscaled(std::move(plain));
    REQUIRE(app.ui().sizes().rowHeight > unscaled.ui().sizes().rowHeight);
}

TEST_CASE("a GUI scale chosen from the desktop is one of the automatic steps", "[app][display]") {
    AppSetup setup = small();
    setup.scale = AutoScale{};
    App app(std::move(setup));
    const float scale = app.ui().scale();
    REQUIRE(scale == autoScale(sf::VideoMode::getDesktopMode().size));
}

TEST_CASE("an app loads the fonts its themes name from its resources", "[app][display]") {
    AppSetup setup = small();
    setup.ui.theme = themes::colorful();
    App app(std::move(setup));
    REQUIRE(app.ui().theme().font->getInfo().family == "Roboto");

    app.ui().setTheme(themes::moon()); // the bundled font again
    REQUIRE(app.ui().theme().font->getInfo().family == "Inconsolata");
}

TEST_CASE("run ends with the code given to quit, and calls the update every pass", "[app][display]") {
    App app(small());
    int passes = 0;
    float total = 0.f;
    app.onUpdate([&](float dt) {
        REQUIRE(dt >= 0.f);
        total += dt;
        if (++passes == 5) {
            app.quit(3);
        }
    });
    REQUIRE(app.run() == 3);
    REQUIRE(passes == 5);
    REQUIRE(total < 5.f); // passes follow each other, an idle loop waits a display frame at most
}

TEST_CASE("quit from another thread ends run", "[app][display]") {
    App app(small());
    std::thread other([&app] {
        std::this_thread::sleep_for(50ms);
        app.quit(7);
    });
    const int code = app.run();
    other.join();
    REQUIRE(code == 7);
}

TEST_CASE("an app reports a missing font and a broken UI setup before it starts", "[app][display]") {
    AppSetup noFont = small();
    noFont.font = "fonts/missing.ttf";
    REQUIRE_THROWS_AS(App(noFont), ResourceError);

    AppSetup twice = small();
    twice.ui.panels.push_back({ .name = "Panel" });
    REQUIRE_THROWS_AS(App(twice), SetupError);
}

TEST_CASE("a theme with its own font needs none from the resources", "[app][display]") {
    App first(small());
    AppSetup setup = small();
    setup.font = "fonts/missing.ttf";
    setup.ui.theme.font = first.ui().theme().font;
    REQUIRE_NOTHROW(App(setup));
}

namespace {

/// Counts its ticks, and shows the count.
class Ticking final : public Simulation<int> {
public:
    std::atomic<bool> fail{ false };

private:
    void tick(float /*dt*/) override {
        if (fail) {
            throw std::runtime_error("the simulation broke");
        }
        ++m_ticks;
    }
    void writeState(int& state) const override { state = m_ticks; }

    int m_ticks = 0;
};

} // namespace

TEST_CASE("run with a simulation runs it on its own thread and shows its newest state", "[app][display][simulation]") {
    App app(small());
    Ticking simulation;
    simulation.controls.tickRate = 500.0;
    int seen = 0;
    bool steady = true;
    app.onUpdate([&](float) {
        const int now = simulation.state();
        steady = steady && now >= seen; // never an older state
        seen = now;
        if (seen > 20) {
            app.quit(4);
        }
    });
    REQUIRE(app.run(simulation) == 4);
    REQUIRE(seen > 20);
    REQUIRE(steady);
    REQUIRE(simulation.controls.tickCount.get() >= seen); // the thread has stopped; it got at least that far
}

TEST_CASE("what the simulation throws ends run and is thrown again on the main thread", "[app][display][simulation]") {
    App app(small());
    Ticking simulation;
    app.onUpdate([&](float) { simulation.fail = true; });
    REQUIRE_THROWS_WITH(app.run(simulation), "the simulation broke");
}
