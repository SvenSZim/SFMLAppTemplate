#include "ui/input/input_system.hpp"
#include "ui/layout/arrange.hpp"

#include <catch2/catch_test_macros.hpp>

#include <functional>
#include <memory>
#include <string>
#include <variant>
#include <vector>

using namespace atpl;
using atpl::input::InputSystem;
using atpl::model::Store;

namespace {

// Round numbers, so that places can be worked out by hand.
Sizes sizes() {
    Sizes result;
    result.margin = 10.f;
    result.padding = { 10.f, 10.f };
    result.gap = { 5.f, 5.f };
    result.headerHeight = 30.f;
    result.rowHeight = 20.f;
    result.panelWidth = 200.f;
    return result;
}

/// What a widget received, and what it does in return.
struct Received {
    std::vector<std::string> received; ///< "pressed 10,5", "moved 30,5", "key", ...
};

using Reaction = std::function<void(const Event&, InputContext&)>;

/// A widget that writes down everything it gets, and reacts as the test says.
class Recorder final : public Widget {
public:
    Recorder(std::shared_ptr<Received> log, Reaction react) :
        m_log(std::move(log)),
        m_react(std::move(react)) {}

    bool reacts = true; ///< What `reactsToPointer` says.

    [[nodiscard]] SizeRequest measure(const MeasureContext&) const override { return { .min = { 0.f, 20.f } }; }
    void paint(Painter&, const Style&) const override {}
    [[nodiscard]] bool reactsToPointer() const override { return reacts; }

    bool handleInput(const Event& event, InputContext& context) override {
        const auto at = [&](const PointerLocation& pointer) {
            const sf::Vector2f local = context.local(pointer);
            return std::to_string(static_cast<int>(local.x)) + "," + std::to_string(static_cast<int>(local.y));
        };
        if (const auto* press = event.getIf<PointerPressed>()) {
            m_log->received.push_back("pressed " + at(press->pointer));
        } else if (const auto* release = event.getIf<PointerReleased>()) {
            m_log->received.push_back("released " + at(release->pointer));
        } else if (const auto* move = event.getIf<PointerMoved>()) {
            m_log->received.push_back("moved " + at(move->pointer));
        } else if (event.is<Scrolled>()) {
            m_log->received.push_back("scrolled");
        } else if (event.is<KeyPressed>()) {
            m_log->received.push_back("key");
        } else if (event.is<TextEntered>()) {
            m_log->received.push_back("text");
        }
        if (m_react) {
            m_react(event, context);
        }
        return true;
    }

private:
    std::shared_ptr<Received> m_log;
    Reaction m_react;
};

struct Recording {
    std::string name;
    std::shared_ptr<Received> log;
    Reaction react;
    [[nodiscard]] std::unique_ptr<Widget> create() const { return std::make_unique<Recorder>(log, react); }
};

struct Region {
    static constexpr bool isView = true;
    std::string name;
    std::shared_ptr<Received> log;
    [[nodiscard]] std::unique_ptr<Widget> create() const { return std::make_unique<Recorder>(log, nullptr); }
};

/// On a press: something a widget does.
Reaction onPress(std::function<void(InputContext&)> action) {
    return [action = std::move(action)](const Event& event, InputContext& context) {
        if (event.is<PointerPressed>()) {
            action(context);
        }
    };
}

/// A window of 800 x 600 with a grid of two columns:
///
///   - the background view "world" behind everything
///   - "Scene" in the right column (405, 10, 385 x 580), whose only widget is the view "minimap"
///   - "Controls" floating at the top left (10, 10, 200 wide), with three widgets of 20 high in a
///     column: "Button" at (20, 50), "Field" at (20, 75), "Off" at (20, 100), each 180 wide
///
/// Everything at x = 300 is outside any panel: there the background view is.
struct Fixture {
    std::shared_ptr<Received> button = std::make_shared<Received>();
    std::shared_ptr<Received> field = std::make_shared<Received>();
    std::shared_ptr<Received> off = std::make_shared<Received>();
    std::shared_ptr<Received> minimap = std::make_shared<Received>();
    Reaction buttonReaction;
    Reaction fieldReaction = onPress([](InputContext& context) { context.requestFocus(); });

    Store store{ setup() };
    std::vector<PanelId> stacking;
    InputSystem input;
    std::vector<Event> events;

    Fixture() {
        layout::prepare(store, { .columns = 2, .rows = 1 });
        layout::arrange(store, { 800.f, 600.f }, { .columns = 2, .rows = 1 }, Theme(), sizes());
        stacking = store.stackingOrder();
        clean();
    }

    UISetup setup() {
        UISetup result;
        result.background = "world";
        result.grid = { .columns = 2, .rows = 1 };
        result.panels = {
            {
                .name = "Controls",
                .placement = Anchor::TopLeft,
                .widgets = {
                    Recording{ "Button", button, [this](const Event& event, InputContext& context) {
                                  if (buttonReaction) {
                                      buttonReaction(event, context);
                                  }
                              } },
                    Recording{ "Field", field, [this](const Event& event, InputContext& context) { fieldReaction(event, context); } },
                    Recording{ "Off", off, nullptr },
                },
            },
            { .name = "Scene", .placement = GridCell{ .column = 1 }, .widgets = { Region{ "minimap", minimap } } },
        };
        return result;
    }

    /// Hands the input system an event and returns what came out of it.
    std::vector<Event> send(const sf::Event& event) {
        events.clear();
        input.handle(event, store, stacking, sizes(), events);
        return events;
    }

    std::vector<Event> move(float x, float y) {
        return send(sf::Event::MouseMoved{ { static_cast<int>(x), static_cast<int>(y) } });
    }
    std::vector<Event> press(float x, float y, sf::Mouse::Button which = sf::Mouse::Button::Left) {
        return send(sf::Event::MouseButtonPressed{ which, { static_cast<int>(x), static_cast<int>(y) } });
    }
    std::vector<Event> release(float x, float y, sf::Mouse::Button which = sf::Mouse::Button::Left) {
        return send(sf::Event::MouseButtonReleased{ which, { static_cast<int>(x), static_cast<int>(y) } });
    }
    std::vector<Event> click(float x, float y) {
        std::vector<Event> all = press(x, y);
        for (Event& event : release(x, y)) {
            all.push_back(std::move(event));
        }
        return all;
    }
    std::vector<Event> wheel(float x, float y, float delta = 1.f) {
        return send(
            sf::Event::MouseWheelScrolled{
                sf::Mouse::Wheel::Vertical, delta, { static_cast<int>(x), static_cast<int>(y) } }
        );
    }
    std::vector<Event> key(sf::Keyboard::Key code = sf::Keyboard::Key::A) {
        return send(
            sf::Event::KeyPressed{ .code = code,
                                   .scancode = sf::Keyboard::Scan::A,
                                   .alt = false,
                                   .control = true,
                                   .shift = false,
                                   .system = false }
        );
    }
    std::vector<Event> text() { return send(sf::Event::TextEntered{ U'a' }); }

    model::WidgetSlot& slot(std::string_view name) { return store.widget(store.names().widget(name)); }
    model::Panel& panel(std::string_view name) { return store.panel(store.names().panel(name)); }

    void clean() {
        for (model::Panel& each : store.panels()) {
            each.dirty = false;
        }
    }
};

} // namespace

// ----- What is kept and what is forwarded -----

TEST_CASE("a click on a panel is not forwarded, a click outside is", "[ui][input]") {
    Fixture f;

    REQUIRE(f.click(100.f, 60.f).empty());  // on the button
    REQUIRE(f.click(100.f, 127.f).empty()); // on the panel, below its widgets
    REQUIRE(f.click(600.f, 300.f).empty()); // on the scene panel

    const auto outside = f.click(300.f, 300.f);
    REQUIRE(outside.size() == 2);
    REQUIRE(outside[0].is<PointerPressed>());
    REQUIRE(outside[1].is<PointerReleased>());
}

TEST_CASE("a widget gets the pointer in its own coordinates", "[ui][input]") {
    Fixture f;
    f.click(30.f, 55.f);
    REQUIRE(f.button->received == std::vector<std::string>{ "pressed 10,5", "released 10,5" });
}

TEST_CASE("moves over a panel go to the widget under the pointer, moves elsewhere are forwarded", "[ui][input]") {
    Fixture f;

    REQUIRE(f.move(30.f, 55.f).empty());
    REQUIRE(f.button->received == std::vector<std::string>{ "moved 10,5" });

    const auto outside = f.move(300.f, 300.f);
    REQUIRE(outside.size() == 1);
    REQUIRE(outside[0].is<PointerMoved>());
}

TEST_CASE("forwarded moves say how far the pointer went since the last forwarded move", "[ui][input]") {
    Fixture f;
    f.move(300.f, 300.f);
    const auto moved = f.move(310.f, 296.f);
    REQUIRE(moved[0].getIf<PointerMoved>()->delta == sf::Vector2f(10.f, -4.f));
}

TEST_CASE("the wheel over a panel is the UI's, elsewhere it is forwarded", "[ui][input]") {
    Fixture f;

    REQUIRE(f.wheel(30.f, 55.f).empty());
    REQUIRE(f.button->received == std::vector<std::string>{ "scrolled" });
    REQUIRE(f.wheel(100.f, 127.f).empty()); // over the panel, but no widget: still kept

    const auto outside = f.wheel(300.f, 300.f, -2.f);
    REQUIRE(outside.size() == 1);
    REQUIRE(outside[0].getIf<Scrolled>()->delta == -2.f);
}

// ----- A press and everything up to its release -----

TEST_CASE(
    "a drag that starts on a widget goes to it wherever the pointer goes, and is never forwarded", "[ui][input]"
) {
    Fixture f;
    f.press(30.f, 55.f);

    REQUIRE(f.move(300.f, 300.f).empty()); // far outside the panel
    REQUIRE(f.move(600.f, 300.f).empty()); // over another panel
    REQUIRE(f.release(300.f, 300.f).empty());

    REQUIRE(
        f.button->received ==
        std::vector<std::string>{ "pressed 10,5", "moved 280,250", "moved 580,250", "released 280,250" }
    );
    REQUIRE(f.minimap->received.empty());

    // Afterwards the pointer outside is forwarded again.
    REQUIRE(f.move(301.f, 300.f).size() == 1);
}

TEST_CASE("a drag that starts outside the panels is forwarded even where it crosses one", "[ui][input]") {
    Fixture f;
    REQUIRE(f.press(300.f, 300.f).size() == 1);

    REQUIRE(f.move(30.f, 55.f).size() == 1); // over the button
    REQUIRE_FALSE(f.slot("Button").hovered); // nothing in the UI takes notice during the drag
    REQUIRE(f.wheel(30.f, 55.f).size() == 1);
    REQUIRE(f.release(30.f, 55.f).size() == 1);
    REQUIRE(f.button->received.empty());
    REQUIRE(f.slot("Button").hovered); // after it, the button is under the pointer

    // Afterwards the panel keeps its input again.
    REQUIRE(f.move(31.f, 55.f).empty());
}

TEST_CASE("a widget that captures the pointer keeps it until the button is released", "[ui][input]") {
    Fixture f;
    f.buttonReaction = onPress([](InputContext& context) { context.capturePointer(); });
    f.press(30.f, 55.f);
    REQUIRE(f.input.captured() == f.store.names().widget("Button"));

    f.move(300.f, 300.f);
    f.release(300.f, 300.f);
    REQUIRE_FALSE(f.input.captured().has_value()); // released with the button
    REQUIRE(f.button->received.back() == "released 280,250");
}

// ----- Hover, press and focus -----

TEST_CASE("hovering and pressing a widget is written to it and marks its panel dirty", "[ui][input]") {
    Fixture f;

    f.move(30.f, 55.f);
    REQUIRE(f.slot("Button").hovered);
    REQUIRE(f.panel("Controls").hovered);
    REQUIRE(f.panel("Controls").dirty);
    REQUIRE(model::stateOf(f.slot("Button")) == State::Hovered);

    f.clean();
    f.move(31.f, 56.f); // still the same widget: nothing changes
    REQUIRE_FALSE(f.panel("Controls").dirty);

    f.press(31.f, 56.f);
    REQUIRE(f.slot("Button").pressed);
    REQUIRE(f.panel("Controls").dirty);

    f.clean();
    f.release(31.f, 56.f);
    REQUIRE_FALSE(f.slot("Button").pressed);
    REQUIRE(f.panel("Controls").dirty);

    f.move(300.f, 300.f);
    REQUIRE_FALSE(f.slot("Button").hovered);
    REQUIRE_FALSE(f.panel("Controls").hovered);
}

TEST_CASE("leaving the window ends the hover", "[ui][input]") {
    Fixture f;
    f.move(30.f, 55.f);
    f.send(sf::Event::MouseLeft{});
    REQUIRE_FALSE(f.slot("Button").hovered);
}

TEST_CASE("a widget with the keyboard focus gets keys and text, which are then not forwarded", "[ui][input]") {
    Fixture f;

    // Without focus, keys and text are the application's.
    const auto key = f.key(sf::Keyboard::Key::Space);
    REQUIRE(key.size() == 1);
    REQUIRE(key[0].isKey(sf::Keyboard::Key::Space));
    REQUIRE(key[0].getIf<KeyPressed>()->modifiers.control);
    REQUIRE(f.text().size() == 1);

    // The field takes the focus when it is pressed.
    f.click(30.f, 80.f);
    REQUIRE(f.input.focused() == f.store.names().widget("Field"));
    REQUIRE(f.slot("Field").focused);
    REQUIRE(f.key().empty());
    REQUIRE(f.text().empty());
    REQUIRE(f.field->received == std::vector<std::string>{ "pressed 10,5", "released 10,5", "key", "text" });

    // A press anywhere else takes it away.
    f.click(300.f, 300.f);
    REQUIRE_FALSE(f.input.focused().has_value());
    REQUIRE_FALSE(f.slot("Field").focused);
    REQUIRE(f.key().size() == 1);
}

TEST_CASE("pressing the focused widget again keeps its focus", "[ui][input]") {
    Fixture f;
    f.click(30.f, 80.f);
    f.click(40.f, 80.f);
    REQUIRE(f.slot("Field").focused);

    f.click(30.f, 55.f); // another widget that does not want the focus
    REQUIRE_FALSE(f.slot("Field").focused);
}

TEST_CASE("a disabled widget gets no input and is not hovered, but its panel still keeps the input", "[ui][input]") {
    Fixture f;
    f.slot("Off").enabled = false;

    REQUIRE(f.click(30.f, 105.f).empty());
    REQUIRE(f.move(31.f, 105.f).empty());
    REQUIRE(f.off->received.empty());
    REQUIRE_FALSE(f.slot("Off").hovered);
}

TEST_CASE("a widget that only shows is passed over by the pointer, as the panel's background is", "[ui][input]") {
    Fixture f;
    static_cast<Recorder&>(*f.slot("Off").widget).reacts = false;

    REQUIRE(f.click(30.f, 105.f).empty()); // the panel's: not forwarded
    REQUIRE(f.move(31.f, 105.f).empty());
    REQUIRE(f.off->received.empty());
    REQUIRE_FALSE(f.slot("Off").hovered);
    REQUIRE_FALSE(f.slot("Off").pressed);
    REQUIRE(f.panel("Controls").hovered);
}

TEST_CASE("a widget that is not drawn gets no input", "[ui][input]") {
    Fixture f;
    f.slot("Button").visible = false;
    REQUIRE(f.click(30.f, 55.f).empty());
    REQUIRE(f.button->received.empty());
}

TEST_CASE("the topmost panel gets the pointer", "[ui][input]") {
    // A panel floating over a grid panel: the grid fills the whole window here.
    auto log = std::make_shared<Received>();
    auto below = std::make_shared<Received>();
    UISetup setup;
    setup.panels = {
        { .name = "Grid", .placement = GridCell{}, .widgets = { Recording{ "Under", below, nullptr } } },
        { .name = "Floating", .placement = Anchor::TopLeft, .widgets = { Recording{ "Over", log, nullptr } } },
    };
    Store store{ setup };
    layout::prepare(store, {});
    layout::arrange(store, { 800.f, 600.f }, {}, Theme(), sizes());
    InputSystem input;
    std::vector<Event> events;
    const std::vector<PanelId> stacking = store.stackingOrder();

    input.handle(
        sf::Event::MouseButtonPressed{ sf::Mouse::Button::Left, { 30, 55 } }, store, stacking, sizes(), events
    );
    REQUIRE(log->received.size() == 1);
    REQUIRE(below->received.empty());
}

// ----- The header -----

TEST_CASE("a click on the header of a collapsible panel folds it, and the next one unfolds it", "[ui][input][panel]") {
    Fixture f;
    REQUIRE(f.click(100.f, 25.f).empty()); // in the header: kept
    REQUIRE(f.panel("Controls").collapsed);
    REQUIRE(f.panel("Controls").opening.has_value()); // it folds over a moment
    REQUIRE(f.input.takeFolded());
    REQUIRE_FALSE(f.input.takeFolded()); // the mark is cleared

    f.click(100.f, 25.f);
    REQUIRE_FALSE(f.panel("Controls").collapsed);
}

TEST_CASE("a press on the header that is released elsewhere folds nothing", "[ui][input][panel]") {
    Fixture f;
    f.press(100.f, 25.f);
    f.release(100.f, 80.f); // on the panel, but not on its header
    REQUIRE_FALSE(f.panel("Controls").collapsed);
    f.press(30.f, 55.f); // and a press on a widget released on the header neither
    f.release(100.f, 25.f);
    REQUIRE_FALSE(f.panel("Controls").collapsed);
    REQUIRE_FALSE(f.input.takeFolded());
}

TEST_CASE("a panel that cannot be folded ignores clicks on its header", "[ui][input][panel]") {
    Fixture f;
    f.panel("Controls").collapsible = false;
    f.click(100.f, 25.f);
    REQUIRE_FALSE(f.panel("Controls").collapsed);
}

TEST_CASE("the header knows when the pointer is over it", "[ui][input][panel]") {
    Fixture f;
    f.move(100.f, 25.f);
    REQUIRE(f.panel("Controls").headerHovered);
    REQUIRE(f.panel("Controls").dirty); // its arrow lights up

    f.clean();
    f.move(30.f, 55.f); // into the content
    REQUIRE_FALSE(f.panel("Controls").headerHovered);
    REQUIRE(f.panel("Controls").dirty);

    f.move(100.f, 25.f);
    f.move(300.f, 300.f); // out of the panel
    REQUIRE_FALSE(f.panel("Controls").headerHovered);
}

TEST_CASE("the header is not part of the content: widgets are found only below it", "[ui][input][panel]") {
    Fixture f;
    f.slot("Button").rect.setPosition({ 10.f, -25.f }); // a widget that reaches up into the header
    f.click(30.f, 25.f);
    REQUIRE(f.button->received.empty());
}

// ----- What widgets report -----

TEST_CASE("a widget that is pressed as a button raises ButtonPressed", "[ui][input]") {
    Fixture f;
    f.buttonReaction = onPress([](InputContext& context) { context.press(); });

    const auto events = f.click(30.f, 55.f);
    REQUIRE(events.size() == 1);
    REQUIRE(events[0].isButton("Button"));
    REQUIRE(events[0].isButton("Controls/Button"));
    REQUIRE(events[0].isButton(f.store.names().widget("Button")));
    REQUIRE_FALSE(events[0].isButton("Scene/Button"));
    REQUIRE_FALSE(events[0].isButton("Field"));
}

TEST_CASE("a widget that changes its value raises ValueChanged", "[ui][input]") {
    Fixture f;
    f.buttonReaction = [](const Event& event, InputContext& context) {
        if (event.is<PointerMoved>()) {
            context.changeValue(0.5, false); // while dragging
        } else if (event.is<PointerReleased>()) {
            context.changeValue(0.75); // done
        }
    };

    f.press(30.f, 55.f);
    const auto during = f.move(40.f, 55.f);
    REQUIRE(during.size() == 1);
    const ValueChanged* change = during[0].changeOf("Button");
    REQUIRE(change != nullptr);
    REQUIRE(std::get<double>(change->value) == 0.5); // valueAs and as<T> come with bindings (WP 3.8)
    REQUIRE_FALSE(change->final);
    REQUIRE(change->panel == "Controls");

    const auto done = f.release(40.f, 55.f);
    REQUIRE(std::get<double>(done[0].changeOf("Controls/Button")->value) == 0.75);
    REQUIRE(done[0].changeOf("Controls/Button")->final);
    REQUIRE(done[0].changeOf("Field") == nullptr);
}

TEST_CASE("a widget that says it looks different marks its panel dirty", "[ui][input]") {
    Fixture f;
    f.buttonReaction = [](const Event& event, InputContext& context) {
        if (event.is<Scrolled>()) {
            context.markDirty();
        }
    };
    f.move(300.f, 300.f);
    f.clean();
    f.wheel(30.f, 55.f); // hovering does not change while the wheel turns: only the widget's word counts
    REQUIRE(f.panel("Controls").dirty);
    REQUIRE_FALSE(f.panel("Scene").dirty);
}

TEST_CASE("a widget sees its size and state in its context", "[ui][input]") {
    Fixture f;
    sf::Vector2f size;
    State state = State::Normal;
    f.buttonReaction = onPress([&](InputContext& context) {
        size = context.size();
        state = context.state();
    });
    f.move(30.f, 55.f);
    f.press(30.f, 55.f);
    REQUIRE(size == sf::Vector2f(180.f, 20.f));
    REQUIRE(state == (State::Hovered | State::Pressed));
}

// ----- Where the pointer is -----

TEST_CASE("forwarded pointer input says which view the pointer is over", "[ui][input]") {
    Fixture f;

    // Outside the panels: the background view, which is the whole window.
    const auto background = f.move(300.f, 250.f);
    const PointerLocation& there = background[0].getIf<PointerMoved>()->pointer;
    REQUIRE(there.view == f.store.backgroundView());
    REQUIRE(there.viewName == "world");
    REQUIRE(there.isIn("world"));
    REQUIRE(there.inView == sf::Vector2f(300.f, 250.f));
    REQUIRE(there.window == sf::Vector2f(300.f, 250.f));
}

TEST_CASE("a view widget is found under the pointer, relative to its own corner", "[ui][input]") {
    Fixture f;

    // A drag that starts outside and crosses the minimap is forwarded with the minimap's view.
    f.press(300.f, 300.f);
    const auto over = f.move(600.f, 300.f);
    const PointerLocation& pointer = over[0].getIf<PointerMoved>()->pointer;
    const model::View& minimap = f.store.view(f.store.names().view("minimap"));
    REQUIRE(pointer.viewName == "minimap");
    REQUIRE(pointer.inView == sf::Vector2f(600.f, 300.f) - minimap.rect.position());

    // Over a panel but not over a view: no view.
    const auto panel = f.move(100.f, 127.f);
    REQUIRE_FALSE(panel[0].getIf<PointerMoved>()->pointer.view.has_value());
}

TEST_CASE("a hover that layout may have made untrue is forgotten", "[ui][input]") {
    Fixture f;
    f.move(30.f, 55.f);
    f.input.forgetHover(f.store);
    REQUIRE_FALSE(f.slot("Button").hovered);
    REQUIRE_FALSE(f.input.hovered().has_value());
}

// ----- The shorthands of Event -----

TEST_CASE("the shorthands of an event answer only for their own kind", "[ui][input]") {
    const Event key(KeyPressed{ sf::Keyboard::Key::Escape, {}, {} });
    REQUIRE(key.isKey(sf::Keyboard::Key::Escape));
    REQUIRE_FALSE(key.isKey(sf::Keyboard::Key::Space));
    REQUIRE_FALSE(key.isButton("Escape"));
    REQUIRE(key.changeOf("Escape") == nullptr);

    const Event released(KeyReleased{ sf::Keyboard::Key::Escape, {}, {} });
    REQUIRE_FALSE(released.isKey(sf::Keyboard::Key::Escape)); // a press is asked for
}
