#include "atpl/app/app.hpp"
#include "atpl/ui/error.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

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
