#include "atpl/app/camera.hpp"
#include "atpl/app/minimap.hpp"

#include <SFML/Graphics/RenderTexture.hpp>
#include <SFML/Graphics/View.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace atpl;
using Catch::Approx;

namespace {

/// The pointer at `inView` in the view called `name`.
PointerLocation in(std::string_view name, sf::Vector2f inView) {
    return { .window = inView, .view = ViewId{ 0 }, .viewName = name, .inView = inView }; // names are literals here
}

/// A target the size of a view, set up as the UI sets it up: one unit a pixel, and a viewport
/// and scissor that the camera has to keep.
struct Target {
    sf::RenderTexture texture{ { 200u, 100u } };

    Target() {
        sf::View view(sf::FloatRect({ 0.f, 0.f }, { 200.f, 100.f }));
        view.setViewport(sf::FloatRect({ 0.25f, 0.f }, { 0.5f, 1.f }));
        view.setScissor(sf::FloatRect({ 0.25f, 0.f }, { 0.5f, 0.5f }));
        texture.setView(view);
    }
};

} // namespace

TEST_CASE(
    "a camera shows the world around its centre, at its zoom, where the UI put the view", "[app][camera][display]"
) {
    Target target;
    Camera camera("world");
    camera.setCenter({ 10.f, 20.f });
    camera.setZoom(2.f);
    camera.apply(target.texture, { 200.f, 100.f });

    const sf::View& view = target.texture.getView();
    REQUIRE(view.getCenter() == sf::Vector2f(10.f, 20.f));
    REQUIRE(view.getSize() == sf::Vector2f(100.f, 50.f));                        // two pixels a unit
    REQUIRE(view.getViewport() == sf::FloatRect({ 0.25f, 0.f }, { 0.5f, 1.f })); // kept
    REQUIRE(view.getScissor() == sf::FloatRect({ 0.25f, 0.f }, { 0.5f, 0.5f }));
    REQUIRE(camera.visibleArea() == FloatRect(-40.f, -5.f, 100.f, 50.f));
    REQUIRE(camera.toWorld({ 100.f, 50.f }) == sf::Vector2f(10.f, 20.f)); // the middle of the view
}

TEST_CASE("a drag that starts in its view moves the camera's world with the pointer", "[app][camera][display]") {
    Camera camera("world");
    camera.setZoom(2.f);

    REQUIRE_FALSE(camera.handle(PointerMoved{ in("world", { 50.f, 50.f }), { 10.f, 0.f } })); // no drag yet
    REQUIRE_FALSE(camera.handle(PointerPressed{ sf::Mouse::Button::Left, in("world", { 50.f, 50.f }) }));
    REQUIRE(camera.handle(PointerMoved{ in("world", { 60.f, 50.f }), { 10.f, 4.f } }));
    REQUIRE(camera.center() == sf::Vector2f(-5.f, -2.f)); // ten pixels are five units at zoom 2

    // Out of the view, the drag goes on until the release.
    REQUIRE(camera.handle(PointerMoved{ in("", { 0.f, 0.f }), { 2.f, 0.f } }));
    camera.handle(PointerReleased{ sf::Mouse::Button::Left, in("", { 0.f, 0.f }) });
    REQUIRE_FALSE(camera.handle(PointerMoved{ in("world", { 70.f, 50.f }), { 10.f, 0.f } }));

    // A drag that starts in another view, or with another button, is not this camera's.
    camera.handle(PointerPressed{ sf::Mouse::Button::Left, in("minimap", { 5.f, 5.f }) });
    REQUIRE_FALSE(camera.handle(PointerMoved{ in("world", { 70.f, 50.f }), { 10.f, 0.f } }));
    camera.handle(PointerReleased{ sf::Mouse::Button::Left, in("world", { 5.f, 5.f }) });
    camera.handle(PointerPressed{ sf::Mouse::Button::Right, in("world", { 5.f, 5.f }) });
    REQUIRE_FALSE(camera.handle(PointerMoved{ in("world", { 70.f, 50.f }), { 10.f, 0.f } }));
}

TEST_CASE("the wheel zooms towards the pointer, within the limits", "[app][camera][display]") {
    Target target;
    Camera camera("world", { .zoomStep = 2.f, .minZoom = 0.5f, .maxZoom = 4.f });
    camera.apply(target.texture, { 200.f, 100.f });

    const sf::Vector2f pointer(150.f, 30.f);
    const sf::Vector2f under = camera.toWorld(pointer);
    REQUIRE(camera.handle(Scrolled{ 1.f, false, in("world", pointer) }));
    REQUIRE(camera.zoom() == 2.f);
    REQUIRE(camera.toWorld(pointer).x == Approx(under.x)); // the same place stays under the pointer
    REQUIRE(camera.toWorld(pointer).y == Approx(under.y));

    camera.handle(Scrolled{ 5.f, false, in("world", pointer) });
    REQUIRE(camera.zoom() == 4.f);                                              // at most
    REQUIRE_FALSE(camera.handle(Scrolled{ 1.f, false, in("world", pointer) })); // nothing changes any more
    camera.handle(Scrolled{ -10.f, false, in("world", pointer) });
    REQUIRE(camera.zoom() == 0.5f); // at least

    REQUIRE_FALSE(camera.handle(Scrolled{ 1.f, false, in("minimap", pointer) })); // another view's
}

TEST_CASE(
    "show fits a part of the world into the view, also before the view's size is known", "[app][camera][display]"
) {
    Target target;
    Camera early("world");
    early.show({ 0.f, 0.f }, { 50.f, 50.f }); // nothing drawn yet: the size is not known
    early.apply(target.texture, { 200.f, 100.f });
    REQUIRE(early.zoom() == 2.f); // the height limits it: 100 pixels for 50 units
    REQUIRE(early.center() == sf::Vector2f(25.f, 25.f));
    REQUIRE(target.texture.getView().getSize() == sf::Vector2f(100.f, 50.f));

    early.show({ -100.f, 0.f }, { 400.f, 10.f }); // now at once
    REQUIRE(early.zoom() == 0.5f);
    REQUIRE(early.visibleArea().contains({ -100.f, 0.f }));
    REQUIRE(early.visibleArea().contains({ 299.f, 9.f }));
}

// ----- Minimap -----

namespace {

/// A main camera seeing 100 by 50 units at zoom 2, and a minimap of a 100 by 100 world drawn
/// 100 pixels square, so that a pixel of the minimap is a unit of the world.
struct Maps {
    Target main;
    sf::RenderTexture small{ { 100u, 100u } };
    Camera camera{ "world" };
    Minimap minimap;

    explicit Maps(MinimapMode mode) :
        minimap("minimap", camera, { { 0.f, 0.f }, { 100.f, 100.f } }, { .mode = mode }) {
        camera.setZoom(2.f);
        camera.setCenter({ 50.f, 50.f });
        camera.apply(main.texture, { 200.f, 100.f });
        minimap.apply(small, { 100.f, 100.f });
    }
};

KeyPressed key(sf::Keyboard::Key code, std::string_view view) {
    return { .key = code, .modifiers = {}, .pointer = {}, .view = ViewId{ 1 }, .viewName = view };
}

} // namespace

TEST_CASE(
    "panning on a minimap: a press centres the main view there, and dragging keeps it there",
    "[app][camera][minimap][display]"
) {
    Maps maps(MinimapMode::Pan);
    REQUIRE(maps.minimap.toWorld({ 20.f, 30.f }) == sf::Vector2f(20.f, 30.f));

    REQUIRE(maps.minimap.handle(PointerPressed{ sf::Mouse::Button::Left, in("minimap", { 20.f, 30.f }) }));
    REQUIRE(maps.camera.center() == sf::Vector2f(20.f, 30.f));
    REQUIRE(maps.minimap.handle(PointerMoved{ in("minimap", { 30.f, 30.f }), { 10.f, 0.f } }));
    REQUIRE(maps.camera.center() == sf::Vector2f(30.f, 30.f));

    // Dragged past the minimap's edge: on, but never out of the world.
    maps.minimap.handle(PointerMoved{ in("world", { 0.f, 0.f }), { 500.f, 0.f } });
    REQUIRE(maps.camera.center() == sf::Vector2f(100.f, 30.f));
    maps.minimap.handle(PointerReleased{ sf::Mouse::Button::Left, in("world", { 0.f, 0.f }) });
    REQUIRE_FALSE(maps.minimap.handle(PointerMoved{ in("minimap", { 40.f, 40.f }), { 1.f, 1.f } })); // over
    REQUIRE_FALSE(
        maps.minimap.handle(PointerPressed{ sf::Mouse::Button::Left, in("world", { 5.f, 5.f }) })
    ); // not its view
}

TEST_CASE(
    "selecting on a minimap: a dragged rectangle becomes what the main view shows", "[app][camera][minimap][display]"
) {
    Maps maps(MinimapMode::Select);
    maps.minimap.handle(PointerPressed{ sf::Mouse::Button::Left, in("minimap", { 60.f, 20.f }) });
    maps.minimap.handle(PointerMoved{ in("minimap", { 40.f, 30.f }), { -20.f, 10.f } });
    REQUIRE(maps.minimap.selection() == FloatRect(40.f, 20.f, 20.f, 10.f)); // from any corner
    REQUIRE(maps.camera.center() == sf::Vector2f(50.f, 50.f));              // nothing moves before the release

    maps.minimap.handle(PointerReleased{ sf::Mouse::Button::Left, in("minimap", { 40.f, 30.f }) });
    REQUIRE_FALSE(maps.minimap.selection().has_value());
    REQUIRE(maps.camera.center() == sf::Vector2f(50.f, 25.f));
    REQUIRE(maps.camera.zoom() == 10.f); // 20 by 10 units fill 200 by 100 pixels

    // A click without dragging centres there.
    maps.minimap.handle(PointerPressed{ sf::Mouse::Button::Left, in("minimap", { 70.f, 70.f }) });
    maps.minimap.handle(PointerReleased{ sf::Mouse::Button::Left, in("minimap", { 70.f, 70.f }) });
    REQUIRE(maps.camera.center() == sf::Vector2f(70.f, 70.f));
    REQUIRE(maps.camera.zoom() == 10.f);
}

TEST_CASE("the arrow keys move the main view while the minimap has the keys", "[app][camera][minimap][display]") {
    Maps maps(MinimapMode::Pan); // the main view shows 100 by 50 units
    REQUIRE(maps.minimap.handle(key(sf::Keyboard::Key::Right, "minimap")));
    REQUIRE(maps.camera.center() == sf::Vector2f(60.f, 50.f)); // a tenth of what it shows
    maps.minimap.handle(key(sf::Keyboard::Key::Up, "minimap"));
    REQUIRE(maps.camera.center() == sf::Vector2f(60.f, 45.f));

    REQUIRE_FALSE(maps.minimap.handle(key(sf::Keyboard::Key::Right, "world"))); // keys for another view
    REQUIRE_FALSE(maps.minimap.handle(key(sf::Keyboard::Key::A, "minimap")));   // not an arrow

    for (int i = 0; i < 20; ++i) {
        maps.minimap.handle(key(sf::Keyboard::Key::Left, "minimap"));
    }
    REQUIRE(maps.camera.center().x == 0.f); // the edge of the world
    REQUIRE_FALSE(maps.minimap.handle(key(sf::Keyboard::Key::Left, "minimap")));
}

TEST_CASE("a minimap's mode can change while it runs", "[app][camera][minimap][display]") {
    Maps maps(MinimapMode::Select);
    maps.minimap.handle(PointerPressed{ sf::Mouse::Button::Left, in("minimap", { 10.f, 10.f }) });
    maps.minimap.handle(PointerMoved{ in("minimap", { 30.f, 30.f }), { 20.f, 20.f } });
    maps.minimap.setMode(MinimapMode::Pan); // the selection is dropped
    REQUIRE_FALSE(maps.minimap.selection().has_value());
    REQUIRE(maps.minimap.mode() == MinimapMode::Pan);
    maps.minimap.handle(PointerMoved{ in("minimap", { 40.f, 40.f }), { 10.f, 10.f } });
    REQUIRE(maps.camera.center() == sf::Vector2f(40.f, 40.f)); // the drag goes on as a pan
}
