// These tests open a small window: they carry the CTest label "display".

#include "atpl/ui/ui.hpp"

#include <SFML/Graphics/Image.hpp>
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <SFML/System/Clock.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <variant>

using namespace atpl;
using Catch::Matchers::ContainsSubstring;

namespace {

const sf::Color widgetColor(200, 30, 30);

/// One row high, and painted as a plain red box.
class PlainWidget final : public Widget {
public:
    [[nodiscard]] SizeRequest measure(const MeasureContext& context) const override {
        return { .height = context.sizes().rowHeight };
    }
    void paint(Painter& painter, const Style&) const override {
        PartStyle red;
        red.color = widgetColor;
        painter.box(FloatRect({ 0.f, 0.f }, painter.size()), red);
    }
};

struct Plain {
    std::string name;
    [[nodiscard]] std::unique_ptr<Widget> create() const { return std::make_unique<PlainWidget>(); }
};

struct Region {
    static constexpr bool isView = true;
    std::string name;
    [[nodiscard]] std::unique_ptr<Widget> create() const { return std::make_unique<PlainWidget>(); }
};

/// A grid of two cells with a panel in each, and two panels floating at the top left.
UISetup example() {
    return {
        .background = "world",
        .grid = {.columns = 2, .rows = 1},
        .panels = {
            { .name = "Scene", .placement = GridCell{.column = 0}, .widgets = { Region{ "minimap" } } },
            { .name = "Inspector", .placement = GridCell{.column = 1} },
            { .name = "Controls", .placement = Anchor::TopLeft, .widgets = { Plain{ "Speed" }, Plain{ "Reset" } } },
            { .name = "Statistics", .placement = Anchor::TopLeft, .widgets = { Plain{ "Speed" } } },
        },
    };
}

struct Fixture {
    sf::RenderWindow window{ sf::VideoMode({ 640u, 400u }), "atpl ui test" };

    /// One pass of the main loop. Returns whether a frame was drawn.
    static bool frame(UI& ui) {
        ui.handleInput();
        ui.update();
        return ui.draw();
    }

    /// Passes until the window has settled: its creation may bring events that ask for frames.
    static void settle(UI& ui) {
        const sf::Clock clock;
        while (clock.getElapsedTime() < sf::milliseconds(150)) {
            frame(ui);
        }
    }

    [[nodiscard]] sf::Image picture() {
        sf::Texture capture(window.getSize());
        capture.update(window);
        return capture.copyToImage();
    }
};

} // namespace

TEST_CASE("the UI finds panels, widgets and views by name", "[ui][facade][display]") {
    Fixture f;
    UI ui(f.window, example());

    REQUIRE(ui.panel("Scene").id() == PanelId{ 0 });
    REQUIRE(ui.panel("Statistics").id() == PanelId{ 3 });
    REQUIRE(ui.widget("Reset").id() == WidgetId{ 2 });
    REQUIRE(ui.widget("Controls/Speed").id() == WidgetId{ 1 });
    REQUIRE(ui.widget("Statistics/Speed").id() == WidgetId{ 3 });
    REQUIRE(ui.view("world").id() == ViewId{ 0 });
    REQUIRE(ui.view("minimap").id() == ViewId{ 1 });

    REQUIRE_THROWS_AS(ui.widget("Speed"), SetupError); // which one?
    REQUIRE_THROWS_AS(ui.widget("Mass"), SetupError);
    REQUIRE_THROWS_AS(ui.panel("Graphics"), SetupError);
    REQUIRE_THROWS_AS(ui.view("Scene"), SetupError);
}

TEST_CASE("a setup that cannot work is refused when the UI is built", "[ui][facade][display]") {
    Fixture f;

    SECTION("duplicate names") {
        UISetup setup = example();
        setup.panels.push_back({ .name = "Controls" });
        REQUIRE_THROWS_AS(UI(f.window, std::move(setup)), SetupError);
    }
    SECTION("a panel outside the grid") {
        UISetup setup = example();
        setup.panels[1].placement = GridCell{ .column = 2 };
        REQUIRE_THROWS_WITH(UI(f.window, std::move(setup)), ContainsSubstring("outside the window's grid"));
    }
    SECTION("a colour the theme does not have") {
        UISetup setup = example();
        setup.panels[0].accent = 5;
        REQUIRE_THROWS_WITH(UI(f.window, std::move(setup)), ContainsSubstring("accent colour 5"));
    }
}

TEST_CASE("panels are placed in the window when the UI is updated", "[ui][facade][display]") {
    Fixture f;
    UI ui(f.window, example());
    ui.update();

    // The window is smaller than the layout's reference size, so everything is scaled down.
    const Sizes sizes = ui.sizes();
    REQUIRE(sizes == ui.layout().sizesAt({ 640.f, 400.f }));
    const float margin = sizes.margin;
    const float cellWidth = (640.f - 3.f * margin) / 2.f;

    REQUIRE(ui.panel("Scene").rect() == FloatRect(margin, margin, cellWidth, 400.f - 2.f * margin));
    REQUIRE(ui.panel("Inspector").rect().left() == margin * 2.f + cellWidth);

    // The floating panels are stacked at the top left, each as high as its header and its widgets.
    const float twoRows = sizes.padding.y * 2.f + sizes.rowHeight * 2.f + sizes.gap.y;
    REQUIRE(ui.panel("Controls").rect() == FloatRect(margin, margin, sizes.panelWidth, sizes.headerHeight + twoRows));
    REQUIRE(ui.panel("Statistics").rect().top() == margin * 2.f + sizes.headerHeight + twoRows);

    REQUIRE(ui.view("world").rect() == FloatRect(0.f, 0.f, 640.f, 400.f));
}

TEST_CASE("a frame is drawn only when something changed", "[ui][facade][display]") {
    Fixture f;
    UI ui(f.window, example());

    ui.update();
    REQUIRE(ui.draw()); // the first frame
    Fixture::settle(ui);
    REQUIRE_FALSE(ui.draw());
    REQUIRE_FALSE(ui.draw());

    SECTION("the application asks for one") {
        ui.requestRedraw();
        REQUIRE(ui.draw());
        REQUIRE_FALSE(ui.draw());
    }
    SECTION("a panel is hidden") {
        ui.panel("Statistics").setVisible(false);
        REQUIRE_FALSE(ui.panel("Statistics").isVisible());
        REQUIRE(ui.draw());
        REQUIRE_FALSE(ui.draw());

        ui.panel("Statistics").setVisible(false); // already hidden: nothing to do
        REQUIRE_FALSE(ui.draw());
    }
    SECTION("a panel is collapsed") {
        ui.panel("Inspector").setCollapsed(true);
        REQUIRE(ui.panel("Inspector").isCollapsed());
        REQUIRE(ui.draw());
        REQUIRE(ui.panel("Inspector").rect().height() == ui.sizes().headerHeight);
        REQUIRE_FALSE(ui.draw());
    }
    SECTION("a widget is disabled") {
        ui.widget("Reset").setEnabled(false);
        REQUIRE_FALSE(ui.widget("Reset").isEnabled());
        REQUIRE(ui.draw());
        REQUIRE_FALSE(ui.draw());
    }
    SECTION("the theme is replaced") {
        ui.setTheme(themes::colorful());
        REQUIRE(ui.draw());
        REQUIRE_FALSE(ui.draw());
    }
    SECTION("the profiler readout is switched on and off") {
        REQUIRE_FALSE(ui.isProfilerVisible());
        ui.setProfilerVisible(true);
        REQUIRE(ui.isProfilerVisible());
        REQUIRE(ui.draw());

        ui.setProfilerVisible(false);
        REQUIRE(ui.draw()); // to remove it from the screen
        REQUIRE_FALSE(ui.draw());
    }
}

TEST_CASE("hiding a panel moves the ones stacked after it", "[ui][facade][display]") {
    Fixture f;
    UI ui(f.window, example());
    ui.update();
    const float margin = ui.sizes().margin;
    REQUIRE(ui.panel("Statistics").rect().top() > margin);

    ui.panel("Controls").setVisible(false);
    ui.update();
    REQUIRE(ui.panel("Statistics").rect().top() == margin);
}

TEST_CASE("a theme that does not fit the UI is refused and changes nothing", "[ui][facade][display]") {
    Fixture f;
    UISetup setup = example();
    setup.theme = themes::colorful();
    setup.panels[0].accent = 3; // the colourful theme has four accents
    UI ui(f.window, std::move(setup));
    const sf::Color before = ui.theme().palette.window;

    REQUIRE_THROWS_AS(ui.setTheme(themes::moon()), SetupError); // a single accent
    REQUIRE(ui.theme().palette.window == before);

    Theme wide = themes::moon();
    wide.palette.accents.resize(4, wide.palette.accents.front());
    REQUIRE_NOTHROW(ui.setTheme(wide));
    REQUIRE(ui.theme().palette.window == wide.palette.window);
}

TEST_CASE("a panel that names no place gets the layout theme's", "[ui][facade][display]") {
    Fixture f;
    UISetup setup;
    setup.grid = { .columns = 2, .rows = 1 };
    setup.panels = { { .name = "First" },
                     { .name = "Second" },
                     { .name = "Pinned", .placement = Anchor::BottomRight } };
    UI ui(f.window, std::move(setup)); // the default layout theme: floating at the top left
    ui.update();

    const float margin = ui.sizes().margin;
    REQUIRE(ui.panel("First").rect().position() == sf::Vector2f(margin, margin));
    REQUIRE(ui.panel("Second").rect().left() == margin); // stacked below the first
    REQUIRE(ui.panel("Second").rect().top() > ui.panel("First").rect().bottom());

    // Another layout theme puts the same panels into the window's grid.
    ui.setLayout(layouts::dashboard());
    ui.update();
    REQUIRE(ui.layout().fit == Fit::Fill);
    REQUIRE(ui.panel("First").rect().height() == 400.f - 2.f * ui.sizes().margin); // fills its cell
    REQUIRE(ui.panel("Second").rect().left() > ui.panel("First").rect().right());  // the next cell
    REQUIRE(ui.panel("Second").rect().top() == ui.panel("First").rect().top());

    // A panel that said where it goes stays there.
    REQUIRE(ui.panel("Pinned").rect().right() == 640.f - ui.sizes().margin);
    REQUIRE(ui.draw());
}

TEST_CASE("a layout theme the panels do not fit into is refused and changes nothing", "[ui][facade][display]") {
    Fixture f;
    UISetup setup;
    setup.panels = { { .name = "First" }, { .name = "Second" } }; // a window grid of one cell
    UI ui(f.window, std::move(setup));
    ui.update();
    const FloatRect before = ui.panel("Second").rect();

    REQUIRE_THROWS_AS(ui.setLayout(layouts::dashboard()), SetupError); // two panels, one cell
    REQUIRE(std::holds_alternative<Anchor>(ui.layout().placement));
    ui.update();
    REQUIRE(ui.panel("Second").rect() == before);
}

TEST_CASE("a layout theme with other sizes changes the sizes in use", "[ui][facade][display]") {
    Fixture f;
    UI ui(f.window, example());
    Fixture::settle(ui);
    REQUIRE_FALSE(ui.draw());
    const float rowBefore = ui.sizes().rowHeight;

    ui.setLayout(layouts::compact());
    REQUIRE(ui.sizes().rowHeight < rowBefore);
    REQUIRE(ui.draw()); // everything is laid out and painted anew
    REQUIRE_FALSE(ui.draw());
}

TEST_CASE("the setup can switch the profiler readout on from the start", "[ui][facade][display]") {
    Fixture f;
    UISetup setup = example();
    setup.profiler = true;
    UI ui(f.window, std::move(setup));

    REQUIRE(ui.isProfilerVisible());
}

TEST_CASE("declared panels appear on screen in their theme's colours", "[ui][facade][display]") {
    Fixture f;
    UISetup setup;
    setup.grid = { .columns = 2, .rows = 1 };
    setup.panels = { { .name = "Left", .placement = GridCell{ .column = 0 } } };
    setup.theme.shape.radius = 0.f; // plain corners, so that every pixel inside is the background
    UI ui(f.window, std::move(setup));

    ui.update();
    REQUIRE(ui.draw());
    const sf::Image picture = f.picture();

    const FloatRect panel = ui.panel("Left").rect();
    const sf::Vector2u inside(sf::Vector2f(panel.center()));
    const sf::Vector2u outside(480u, 200u); // the empty right half
    REQUIRE(picture.getPixel(inside) == ui.theme().resolve(Panel::Background).color);
    REQUIRE(picture.getPixel(outside) == ui.theme().palette.window);
}

TEST_CASE("widgets are painted at the place layout gave them", "[ui][facade][display]") {
    Fixture f;
    UISetup setup;
    setup.panels = { { .name = "Controls", .placement = Anchor::TopLeft, .widgets = { Plain{ "Speed" } } } };
    UI ui(f.window, std::move(setup));

    ui.update();
    REQUIRE(ui.draw());
    const sf::Image picture = f.picture();

    // The widget is one row high, a padding below the header and a padding in from the sides.
    const Sizes sizes = ui.sizes();
    const FloatRect panel = ui.panel("Controls").rect();
    const sf::Vector2f widgetCenter(
        panel.center().x, panel.top() + sizes.headerHeight + sizes.padding.y + sizes.rowHeight * 0.5f
    );
    const sf::Vector2f inHeader(panel.right() - sizes.padding.x, panel.top() + sizes.headerHeight * 0.5f);
    REQUIRE(picture.getPixel(sf::Vector2u(widgetCenter)) == widgetColor);
    REQUIRE(picture.getPixel(sf::Vector2u(inHeader)) == ui.theme().resolve(Panel::Background).color);
}

TEST_CASE("resizing the window places the panels anew and tells the application", "[ui][facade][display]") {
    Fixture f;
    UI ui(f.window, example());
    Fixture::settle(ui);
    const float widthBefore = ui.panel("Scene").rect().width();
    const Sizes sizesBefore = ui.sizes();

    f.window.setSize({ 1100u, 650u });

    // The window system reports the new size when it gets to it.
    bool told = false;
    const sf::Clock clock;
    while (!told && clock.getElapsedTime() < sf::seconds(3.f)) {
        ui.handleInput();
        const auto events = ui.events();
        told = std::any_of(events.begin(), events.end(), [](const Event& event) {
            const auto* resized = event.getIf<WindowResized>();
            return resized != nullptr && resized->size == sf::Vector2u(1100u, 650u);
        });
        ui.update();
        ui.draw();
    }
    REQUIRE(told);

    // The sizes follow the window: a larger window has larger margins, within the layout's limits.
    REQUIRE(ui.sizes() == ui.layout().sizesAt({ 1100.f, 650.f }));
    REQUIRE(ui.sizes() != sizesBefore);
    const float margin = ui.sizes().margin;

    REQUIRE(ui.panel("Scene").rect().width() > widthBefore);
    REQUIRE(
        ui.panel("Scene").rect() ==
        FloatRect(margin, margin, std::round((1100.f - 3.f * margin) / 2.f), 650.f - 2.f * margin)
    );
    REQUIRE(ui.view("world").rect() == FloatRect(0.f, 0.f, 1100.f, 650.f));
}

TEST_CASE("a view keeps the draw function the application gives it", "[ui][facade][display]") {
    Fixture f;
    UI ui(f.window, example());
    Fixture::settle(ui);
    REQUIRE_FALSE(ui.draw());

    ui.view("world").onDraw([](sf::RenderTarget&, sf::Vector2f) {});
    REQUIRE(ui.draw()); // there is something new to show
}
