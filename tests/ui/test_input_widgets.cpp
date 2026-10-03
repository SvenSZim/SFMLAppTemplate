#include "atpl/ui/error.hpp"
#include "atpl/ui/setup.hpp"
#include "atpl/ui/widgets.hpp"

#include "ui/binding/sync.hpp"
#include "ui/input/input_system.hpp"
#include "ui/layout/arrange.hpp"
#include "ui/layout/overlay_placement.hpp"
#include "ui/render/draw_list.hpp"
#include "ui/render/text_measurer.hpp"
#include "ui/widgets/utf8.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

using namespace atpl;
using Catch::Matchers::ContainsSubstring;

namespace {

/// Text of a fixed width per character, so tests need no font. Counts bytes: tests type ASCII.
class FixedWidthText final : public render::TextMeasurer {
public:
    [[nodiscard]] sf::Vector2f measure(std::string_view text, const sf::Font*, float size) const override {
        return { static_cast<float>(text.size()) * size * 0.5f, size };
    }
    [[nodiscard]] float wrappedHeight(std::string_view, const sf::Font*, float size, float) const override {
        return size;
    }
};

const FixedWidthText measurer;
const Sizes sizes = Layout().sizesAt({ 1280.f, 720.f }); // nothing scaled

enum class Mode { Normal, Debug, Wireframe };

/// A UI's insides, without a window: one floating panel with the given widgets, laid out, with
/// the input system, the bindings and the overlay in place, as the UI runs them.
struct Harness {
    model::Store store;
    Theme theme;
    std::vector<PanelId> stacking;
    input::InputSystem input;
    std::vector<Event> events;
    sf::Vector2f window;

    explicit Harness(
        std::vector<WidgetSetup> widgets, sf::Vector2f windowSize = { 1280.f, 720.f }, Anchor anchor = Anchor::TopLeft
    ) :
        store(setupOf(std::move(widgets), anchor)),
        window(windowSize) {
        layout::prepare(store, {});
        binding::attachAll(store);
        binding::sync(store, binding::Clock::now());
        layout::arrange(store, window, {}, theme, sizes, &measurer);
        stacking = store.stackingOrder();
        input.setLook(theme, &measurer);
    }

    static UISetup setupOf(std::vector<WidgetSetup> widgets, Anchor anchor) {
        UISetup setup;
        setup.panels = { { .name = "Panel", .placement = anchor, .width = 300.f, .widgets = std::move(widgets) } };
        return setup;
    }

    [[nodiscard]] model::WidgetSlot& slot(std::string_view name) { return store.widget(store.names().widget(name)); }

    /// A widget's rectangle in the window.
    [[nodiscard]] FloatRect rectOf(std::string_view name) {
        const model::WidgetSlot& widget = slot(name);
        const model::Panel& panel = store.panel(widget.panel);
        return { panel.rect.position() + sf::Vector2f(0.f, sizes.headerHeight) + widget.rect.position(),
                 widget.rect.size() };
    }

    /// The middle of a widget's field: the lower part, below its label.
    [[nodiscard]] sf::Vector2f fieldOf(std::string_view name) {
        const FloatRect rect = rectOf(name);
        return { rect.left() + rect.width() * 0.5f, rect.bottom() - 6.f };
    }

    /// Where a widget's open overlay is.
    [[nodiscard]] FloatRect overlayOf(std::string_view name) { return slot(name).overlayRect.value_or(FloatRect()); }

    /// The middle of the entry this many rows down in an open list.
    [[nodiscard]] sf::Vector2f entryOf(std::string_view name, int row) {
        const FloatRect list = overlayOf(name);
        const float entry = std::max(sizes.rowHeight * 0.9f, 8.f);
        const float pad = std::max(std::round(sizes.rowHeight * 0.12f), 2.f);
        return { list.left() + list.width() * 0.5f, list.top() + pad + entry * (static_cast<float>(row) + 0.5f) };
    }

    std::vector<Event> send(const sf::Event& event) {
        events.clear();
        input.handle(event, store, stacking, sizes, events);
        binding::sync(store, binding::Clock::now());
        input.closeOverlayIfGone(store);
        layout::placeOverlays(store, input.overlay(), window, theme, sizes, &measurer);
        return events;
    }
    std::vector<Event> press(sf::Vector2f at) {
        return send(sf::Event::MouseButtonPressed{ sf::Mouse::Button::Left, sf::Vector2i(at) });
    }
    std::vector<Event> moveTo(sf::Vector2f at) { return send(sf::Event::MouseMoved{ sf::Vector2i(at) }); }
    std::vector<Event> release(sf::Vector2f at) {
        return send(sf::Event::MouseButtonReleased{ sf::Mouse::Button::Left, sf::Vector2i(at) });
    }
    std::vector<Event> click(sf::Vector2f at) {
        std::vector<Event> all = press(at);
        for (Event& event : release(at)) {
            all.push_back(std::move(event));
        }
        return all;
    }
    std::vector<Event> wheel(sf::Vector2f at, float delta) {
        return send(sf::Event::MouseWheelScrolled{ sf::Mouse::Wheel::Vertical, delta, sf::Vector2i(at) });
    }
    std::vector<Event> key(sf::Keyboard::Key code, bool control = false) {
        return send(
            sf::Event::KeyPressed{ .code = code,
                                   .scancode = sf::Keyboard::Scan::Unknown,
                                   .alt = false,
                                   .control = control,
                                   .shift = false,
                                   .system = false }
        );
    }
    std::vector<Event> type(std::u32string_view text) {
        std::vector<Event> all;
        for (const char32_t character : text) {
            for (Event& event : send(sf::Event::TextEntered{ character })) {
                all.push_back(std::move(event));
            }
        }
        return all;
    }

    /// Paints a widget, or its overlay, as the UI would, and returns what it drew.
    [[nodiscard]] render::DrawList paint(std::string_view name) {
        const model::WidgetSlot& widget = slot(name);
        render::DrawList list;
        Painter painter(list, { 0.f, 0.f }, widget.rect.size(), &measurer);
        widget.widget->paint(painter, Style(theme, widget.colors, model::stateOf(widget), sizes));
        return list;
    }
    [[nodiscard]] render::DrawList paintOverlay(std::string_view name) {
        const model::WidgetSlot& widget = slot(name);
        render::DrawList list;
        const FloatRect overlay = overlayOf(name);
        Painter painter(list, { 0.f, 0.f }, overlay.size(), &measurer);
        const FloatRect anchor(widget.overlayAnchor.position() - overlay.position(), widget.overlayAnchor.size());
        widget.widget->paintOverlay(painter, Style(theme, widget.colors, model::stateOf(widget), sizes), anchor);
        return list;
    }
};

bool shows(const render::DrawList& list, std::string_view text) {
    const auto runs = list.texts();
    return std::any_of(runs.begin(), runs.end(), [&](const render::TextRun& run) { return run.text == text; });
}

bool hasColor(const render::VertexList& vertices, sf::Color color) {
    return std::any_of(vertices.begin(), vertices.end(), [&](const sf::Vertex& v) { return v.color == color; });
}

/// The values of the ValueChanged events among these, with whether each was final.
std::vector<std::pair<Value, bool>> changes(const std::vector<Event>& events) {
    std::vector<std::pair<Value, bool>> result;
    for (const Event& event : events) {
        if (const auto* changed = event.getIf<ValueChanged>()) {
            result.emplace_back(changed->value, changed->final);
        }
    }
    return result;
}

const std::vector<std::string> modes{ "Normal", "Debug", "Wireframe" };

} // namespace

// ----- Placing an overlay -----

TEST_CASE("an overlay goes below its widget, as wide as it, if it fits there", "[ui][overlay]") {
    const FloatRect anchor(100.f, 100.f, 200.f, 30.f);
    const FloatRect placed =
        layout::placeOverlay(anchor, { 800.f, 600.f }, 10.f, 4.f, [](float) { return sf::Vector2f(0.f, 120.f); });
    REQUIRE(placed == FloatRect(100.f, 134.f, 200.f, 120.f));
}

TEST_CASE("an overlay that does not fit below goes above", "[ui][overlay]") {
    const FloatRect anchor(100.f, 500.f, 200.f, 30.f);
    const FloatRect placed =
        layout::placeOverlay(anchor, { 800.f, 600.f }, 10.f, 4.f, [](float) { return sf::Vector2f(0.f, 120.f); });
    REQUIRE(placed == FloatRect(100.f, 376.f, 200.f, 120.f));
}

TEST_CASE(
    "an overlay that fits neither way is asked again for the larger side, and kept in the window", "[ui][overlay]"
) {
    const FloatRect anchor(100.f, 200.f, 200.f, 30.f); // 186 above, 356 below
    std::vector<float> asked;
    const FloatRect placed = layout::placeOverlay(anchor, { 800.f, 600.f }, 10.f, 4.f, [&](float room) {
        asked.push_back(room);
        return sf::Vector2f(0.f, std::min(room, 1000.f)); // as high as allowed
    });
    REQUIRE(asked == std::vector<float>{ 580.f, 356.f });
    REQUIRE(placed == FloatRect(100.f, 234.f, 200.f, 356.f));

    // An overlay that will not shrink is clamped to the window.
    const FloatRect stubborn =
        layout::placeOverlay(anchor, { 800.f, 600.f }, 10.f, 4.f, [](float) { return sf::Vector2f(0.f, 1000.f); });
    REQUIRE(stubborn.top() >= 10.f);
    REQUIRE(stubborn.bottom() <= 590.f);
}

TEST_CASE("an overlay never reaches beyond the sides of the window", "[ui][overlay]") {
    const FloatRect anchor(700.f, 100.f, 200.f, 30.f);
    const FloatRect placed =
        layout::placeOverlay(anchor, { 800.f, 600.f }, 10.f, 4.f, [](float) { return sf::Vector2f(300.f, 50.f); });
    REQUIRE(placed.right() == 790.f);
    REQUIRE(placed.width() == 300.f);
}

// ----- UTF-8 -----

TEST_CASE("text goes to code points and back", "[ui][widgets][text_input]") {
    const std::string text = "a\xC3\xA4\xE2\x82\xAC\xF0\x9F\x98\x80"; // a, a-umlaut, euro sign, an emoji
    const std::u32string codes = widgets::decodeUtf8(text);
    REQUIRE(codes == std::u32string{ U'a', U'\u00E4', U'\u20AC', U'\U0001F600' });
    REQUIRE(widgets::encodeUtf8(codes) == text);
    REQUIRE(widgets::decodeUtf8("\xFF") == std::u32string{ U'\uFFFD' }); // not UTF-8
    REQUIRE_FALSE(widgets::isPrintable(U'\b'));
    REQUIRE(widgets::isPrintable(U'\u00E4'));
}

// ----- Dropdown -----

TEST_CASE("a click on a dropdown opens its list below it, a click on an entry chooses it", "[ui][widgets][dropdown]") {
    Param<Mode> mode = Mode::Normal;
    Harness ui({ Dropdown("Mode", modes, mode) });

    REQUIRE(ui.click(ui.fieldOf("Mode")).empty());
    REQUIRE(ui.slot("Mode").overlayOpen);
    REQUIRE(has(model::stateOf(ui.slot("Mode")), State::Open));
    REQUIRE(ui.overlayOf("Mode").top() == ui.rectOf("Mode").bottom()); // right at the field, no gap
    REQUIRE(ui.overlayOf("Mode").width() == ui.rectOf("Mode").width());

    const auto events = ui.click(ui.entryOf("Mode", 2));
    REQUIRE(changes(events) == std::vector<std::pair<Value, bool>>{ { Value(std::size_t{ 2 }), true } });
    REQUIRE(mode.get() == Mode::Wireframe);
    REQUIRE_FALSE(ui.slot("Mode").overlayOpen);
    REQUIRE_FALSE(ui.slot("Mode").focused); // keys go to the application again
}

TEST_CASE("a press on a dropdown, dragged to an entry and released, chooses it", "[ui][widgets][dropdown]") {
    Param<Mode> mode = Mode::Normal;
    Harness ui({ Dropdown("Mode", modes, mode) });

    ui.press(ui.fieldOf("Mode"));
    REQUIRE(ui.slot("Mode").overlayOpen);
    ui.moveTo(ui.entryOf("Mode", 1));
    ui.release(ui.entryOf("Mode", 1));
    REQUIRE(mode.get() == Mode::Debug);
    REQUIRE_FALSE(ui.slot("Mode").overlayOpen);
}

TEST_CASE("a click elsewhere closes the list and does nothing else", "[ui][widgets][dropdown]") {
    Param<Mode> mode = Mode::Normal;
    Param<bool> pressed = false;
    Harness ui({ Dropdown("Mode", modes, mode), Button("Go", pressed) });

    ui.click(ui.fieldOf("Mode"));
    REQUIRE(ui.click(ui.rectOf("Go").center()).empty()); // on another widget: it is not pressed
    REQUIRE_FALSE(pressed.get());
    REQUIRE_FALSE(ui.slot("Mode").overlayOpen);

    ui.click(ui.fieldOf("Mode"));
    REQUIRE(ui.click({ 1000.f, 600.f }).empty()); // outside every panel: not forwarded
    REQUIRE_FALSE(ui.slot("Mode").overlayOpen);
    REQUIRE(mode.get() == Mode::Normal);

    ui.click(ui.fieldOf("Mode"));
    ui.click(ui.fieldOf("Mode")); // a second click on the field closes it too
    REQUIRE_FALSE(ui.slot("Mode").overlayOpen);
}

TEST_CASE("an open dropdown answers to the keys, and keeps them from the application", "[ui][widgets][dropdown]") {
    Param<Mode> mode = Mode::Normal;
    Harness ui({ Dropdown("Mode", modes, mode) });

    REQUIRE(ui.key(sf::Keyboard::Key::Down).size() == 1); // closed: forwarded
    ui.click(ui.fieldOf("Mode"));
    REQUIRE(ui.key(sf::Keyboard::Key::Down).empty());
    ui.key(sf::Keyboard::Key::Down);
    ui.key(sf::Keyboard::Key::Down); // stays on the last
    ui.key(sf::Keyboard::Key::Enter);
    REQUIRE(mode.get() == Mode::Wireframe);
    REQUIRE_FALSE(ui.slot("Mode").overlayOpen);

    ui.click(ui.fieldOf("Mode"));
    ui.key(sf::Keyboard::Key::Up);
    REQUIRE(ui.key(sf::Keyboard::Key::Escape).empty()); // closes without choosing
    REQUIRE_FALSE(ui.slot("Mode").overlayOpen);
    REQUIRE(mode.get() == Mode::Wireframe);
    REQUIRE(ui.key(sf::Keyboard::Key::Escape).size() == 1); // the application's again
}

TEST_CASE("a long list shows at most maxVisible entries, and the wheel scrolls it", "[ui][widgets][dropdown]") {
    std::vector<std::string> entries;
    for (int i = 0; i < 10; ++i) {
        entries.push_back("Entry " + std::to_string(i));
    }
    Param<int> chosen = 0;
    Harness ui({ Dropdown("List", entries, chosen, { .maxVisible = 3 }) });

    ui.click(ui.fieldOf("List"));
    REQUIRE(shows(ui.paintOverlay("List"), "Entry 2"));
    REQUIRE_FALSE(shows(ui.paintOverlay("List"), "Entry 3"));

    ui.wheel(ui.entryOf("List", 1), -2.f); // down by two
    REQUIRE(shows(ui.paintOverlay("List"), "Entry 4"));
    REQUIRE_FALSE(shows(ui.paintOverlay("List"), "Entry 1"));
    ui.click(ui.entryOf("List", 0));
    REQUIRE(chosen.get() == 2); // an integer parameter takes the position
}

TEST_CASE(
    "with little room, an open list shows fewer entries, and opens upward near the bottom", "[ui][widgets][dropdown]"
) {
    std::vector<std::string> entries;
    for (int i = 0; i < 8; ++i) {
        entries.push_back("Entry " + std::to_string(i));
    }
    // A low window, and the panel at its bottom: no room below the field.
    Harness ui({ Dropdown("List", entries) }, { 600.f, 200.f }, Anchor::BottomLeft);
    ui.click(ui.fieldOf("List"));

    const FloatRect list = ui.overlayOf("List");
    const FloatRect field = ui.slot("List").overlayAnchor;
    REQUIRE(field.top() > ui.rectOf("List").top()); // attached to the field, not to the label above it
    REQUIRE(list.bottom() == field.top());
    REQUIRE(list.top() >= sizes.margin);
    REQUIRE(shows(ui.paintOverlay("List"), "Entry 0"));
    REQUIRE_FALSE(shows(ui.paintOverlay("List"), "Entry 7")); // the rest scroll
}

TEST_CASE("a list that opens on a later entry finds the entry under the pointer at once", "[ui][widgets][dropdown]") {
    // Opened on "Points", the list still starts at the first entry: all five fit.
    Param<int> chosen = 3;
    Harness ui({ Dropdown("Mode", { "A", "B", "C", "D", "E" }, chosen) });
    ui.click(ui.fieldOf("Mode"));
    ui.moveTo(ui.entryOf("Mode", 1));
    ui.click(ui.entryOf("Mode", 1));
    REQUIRE(chosen.get() == 1);
}

TEST_CASE("a dropdown follows its parameter and shows the chosen entry", "[ui][widgets][dropdown]") {
    Param<Mode> mode = Mode::Debug;
    Harness ui({ Dropdown("Mode", modes, mode), Dropdown("Fixed", modes, { .initial = 2 }) });
    REQUIRE(shows(ui.paint("Mode"), "Debug"));
    REQUIRE(shows(ui.paint("Mode"), "Mode"));
    REQUIRE(shows(ui.paint("Fixed"), "Wireframe"));

    mode = Mode::Normal;
    binding::sync(ui.store, binding::Clock::now());
    REQUIRE(shows(ui.paint("Mode"), "Normal"));
}

TEST_CASE("an open list highlights the entry under the pointer in the accent look", "[ui][widgets][dropdown]") {
    Harness ui({ Dropdown("Mode", modes) });
    ui.click(ui.fieldOf("Mode"));
    ui.moveTo(ui.entryOf("Mode", 1));

    const sf::Color highlight = ui.theme.resolve(Dropdown::Highlight, State::Active).color;
    const render::DrawList list = ui.paintOverlay("Mode");
    REQUIRE(hasColor(list.shapes(), highlight));
    REQUIRE(shows(list, "Debug"));
}

TEST_CASE("an open list is one shape with its field, outlined as the open field is", "[ui][widgets][dropdown]") {
    Harness ui({ Dropdown("Mode", modes) });
    ui.click(ui.fieldOf("Mode"));

    const PartStyle open = ui.theme.resolve(Dropdown::Field, State::Open);
    const render::DrawList list = ui.paintOverlay("Mode");
    REQUIRE(hasColor(list.shapes(), open.border));
    REQUIRE(shows(list, "Normal")); // the field, painted again inside the shape
    // The shape reaches up over the field.
    const auto& vertices = list.shapes();
    const auto highest =
        std::min_element(vertices.begin(), vertices.end(), [](const sf::Vertex& a, const sf::Vertex& b) {
            return a.position.y < b.position.y;
        });
    REQUIRE(highest->position.y <= -ui.slot("Mode").overlayAnchor.height() + 1.f);
}

TEST_CASE(
    "only one list is open at a time, and none while its panel is folded or the widget disabled",
    "[ui][widgets][dropdown]"
) {
    Harness ui({ Dropdown("A", modes), Dropdown("B", modes) });
    ui.click(ui.fieldOf("A"));
    REQUIRE(ui.slot("A").overlayOpen);

    // A press on the other one only closes the first: it is used up.
    ui.click(ui.fieldOf("B"));
    REQUIRE_FALSE(ui.slot("A").overlayOpen);
    REQUIRE_FALSE(ui.slot("B").overlayOpen);
    ui.click(ui.fieldOf("B"));
    REQUIRE(ui.slot("B").overlayOpen);

    ui.slot("B").enabled = false;
    ui.input.closeOverlayIfGone(ui.store);
    REQUIRE_FALSE(ui.slot("B").overlayOpen);

    ui.slot("B").enabled = true;
    ui.click(ui.fieldOf("B"));
    ui.store.panel(ui.slot("B").panel).collapsed = true;
    ui.input.closeOverlayIfGone(ui.store);
    REQUIRE_FALSE(ui.slot("B").overlayOpen);
    REQUIRE_FALSE(ui.input.overlay().has_value());
}

TEST_CASE("dropdown options that cannot work are refused when the UI is built", "[ui][widgets][dropdown]") {
    REQUIRE_THROWS_WITH(Dropdown("D", {}).create(), ContainsSubstring("dropdown \"D\": it needs at least one entry"));
    REQUIRE_THROWS_WITH(Dropdown("D", modes, { .initial = 3 }).create(), ContainsSubstring("initial entry 3"));
    REQUIRE_THROWS_WITH(Dropdown("D", modes, { .maxVisible = 0 }).create(), ContainsSubstring("maxVisible"));
}

// ----- TextInput -----

TEST_CASE("a text input takes the focus on a click and writes every change", "[ui][widgets][text_input]") {
    Param<std::string> name;
    Harness ui({ TextInput("Name", name) });

    ui.click(ui.fieldOf("Name"));
    REQUIRE(ui.slot("Name").focused);
    const auto events = ui.type(U"run");
    REQUIRE(name.get() == "run");
    REQUIRE(changes(events).size() == 3);
    REQUIRE(changes(events).back() == std::pair<Value, bool>{ Value(std::string("run")), false });

    // Enter ends the editing: reported once more as final, and the keys are the application's.
    const auto done = ui.key(sf::Keyboard::Key::Enter);
    REQUIRE(changes(done) == std::vector<std::pair<Value, bool>>{ { Value(std::string("run")), true } });
    REQUIRE_FALSE(ui.slot("Name").focused);
    REQUIRE(ui.key(sf::Keyboard::Key::Enter).size() == 1);
}

TEST_CASE("a click elsewhere ends the editing of a text input", "[ui][widgets][text_input]") {
    Param<std::string> name;
    Harness ui({ TextInput("Name", name) });
    ui.click(ui.fieldOf("Name"));
    ui.type(U"x");
    const auto events = ui.click({ 1000.f, 600.f });
    REQUIRE(changes(events) == std::vector<std::pair<Value, bool>>{ { Value(std::string("x")), true } });
    REQUIRE_FALSE(ui.slot("Name").focused);

    // Without a change, there is nothing to report.
    ui.click(ui.fieldOf("Name"));
    REQUIRE(changes(ui.key(sf::Keyboard::Key::Escape)).empty());
}

TEST_CASE("a text input edits at its cursor", "[ui][widgets][text_input]") {
    Param<std::string> text;
    Harness ui({ TextInput("Text", text) });
    ui.click(ui.fieldOf("Text"));
    ui.type(U"abcd");

    ui.key(sf::Keyboard::Key::Left);
    ui.key(sf::Keyboard::Key::Backspace); // abd
    REQUIRE(text.get() == "abd");
    ui.key(sf::Keyboard::Key::Home);
    ui.key(sf::Keyboard::Key::Delete); // bd
    ui.type(U"X");                     // Xbd
    ui.key(sf::Keyboard::Key::End);
    ui.type(U"!");
    REQUIRE(text.get() == "Xbd!");
    ui.key(sf::Keyboard::Key::Right); // at the end already
    ui.key(sf::Keyboard::Key::Backspace);
    REQUIRE(text.get() == "Xbd");
}

TEST_CASE("a click puts the cursor of a text input between the characters under it", "[ui][widgets][text_input]") {
    Param<std::string> text = std::string("abcd");
    Harness ui({ TextInput("Text", text) });
    const float character = ui.theme.resolve(TextInput::Content).textSize * 0.5f;
    const float inset = std::max(sizes.padding.x * 0.6f, 2.f);

    // Just right of the second character's middle: between "ab" and "cd".
    const FloatRect rect = ui.rectOf("Text");
    ui.click({ rect.left() + inset + character * 1.6f, ui.fieldOf("Text").y });
    ui.type(U"-");
    REQUIRE(text.get() == "ab-cd");
}

TEST_CASE("a text input takes characters, not bytes, and no more than maxLength", "[ui][widgets][text_input]") {
    Param<std::string> text;
    Harness ui({ TextInput("Text", text, { .maxLength = 3 }) });
    ui.click(ui.fieldOf("Text"));
    ui.type(U"\u00E4\u00F6");
    ui.key(sf::Keyboard::Key::Backspace); // the whole character
    REQUIRE(text.get() == "\xC3\xA4");
    ui.type(U"\tbcd"); // a tab is not typed; "d" is one too many
    REQUIRE(
        text.get() == "\xC3\xA4"
                      "bc"
    );
}

TEST_CASE("an empty text input shows its placeholder, and only a focused one its cursor", "[ui][widgets][text_input]") {
    Harness ui({ TextInput("Name", { .placeholder = "untitled" }) });
    const sf::Color cursor = ui.theme.resolve(TextInput::Cursor).color;
    REQUIRE(shows(ui.paint("Name"), "untitled"));
    REQUIRE(shows(ui.paint("Name"), "Name"));
    REQUIRE_FALSE(hasColor(ui.paint("Name").shapes(), cursor));

    ui.click(ui.fieldOf("Name"));
    REQUIRE(hasColor(ui.paint("Name").shapes(), cursor));
    ui.type(U"a");
    REQUIRE_FALSE(shows(ui.paint("Name"), "untitled"));
    REQUIRE(shows(ui.paint("Name"), "a"));
}

TEST_CASE("text wider than the field scrolls with the cursor", "[ui][widgets][text_input]") {
    Param<std::string> text;
    Harness ui({ TextInput("Text", text) });
    ui.click(ui.fieldOf("Text"));
    std::u32string longText;
    for (int i = 0; i < 80; ++i) {
        longText.push_back(static_cast<char32_t>(U'a' + i % 26));
    }
    ui.type(longText);

    const render::DrawList painted = ui.paint("Text");
    const auto runs = painted.texts();
    const auto shown =
        std::find_if(runs.begin(), runs.end(), [](const render::TextRun& run) { return run.text.size() > 10; });
    REQUIRE(shown != runs.end());
    REQUIRE(shown->text.size() < longText.size());
    REQUIRE(shown->text.ends_with("xyzab")); // the end, where the cursor is
}

TEST_CASE(
    "while editing, dots mark text scrolled out at the left; afterwards it is shown from its start",
    "[ui][widgets][text_input]"
) {
    Param<std::string> text;
    Harness ui({ TextInput("Text", text) });
    ui.click(ui.fieldOf("Text"));
    std::u32string longText;
    for (int i = 0; i < 80; ++i) {
        longText.push_back(static_cast<char32_t>(U'a' + i % 26));
    }
    ui.type(longText);

    const auto texts = [&] {
        std::vector<std::string> result;
        const render::DrawList painted = ui.paint("Text");
        for (const render::TextRun& run : painted.texts()) {
            result.emplace_back(run.text);
        }
        return result; // the label first, then what the field shows, left to right
    };
    std::vector<std::string> shown = texts();
    REQUIRE(shown.size() == 3);
    REQUIRE(shown[1] == "...");
    REQUIRE(shown[2].ends_with("xyzab"));

    ui.key(sf::Keyboard::Key::Home); // at the start: the dots move to the right
    shown = texts();
    REQUIRE(shown.size() == 3);
    REQUIRE(shown[1].starts_with("abcde"));
    REQUIRE(shown[2] == "...");

    ui.key(sf::Keyboard::Key::End);
    ui.key(sf::Keyboard::Key::Enter); // done: from the start again, dots at the right
    shown = texts();
    REQUIRE(shown.size() == 3);
    REQUIRE(shown[1].starts_with("abcde"));
    REQUIRE(shown[2] == "...");
}

TEST_CASE("text that fits needs no dots", "[ui][widgets][text_input]") {
    Param<std::string> text = std::string("short");
    Harness ui({ TextInput("Text", text) });
    REQUIRE_FALSE(shows(ui.paint("Text"), "..."));
    REQUIRE(shows(ui.paint("Text"), "short"));
}

TEST_CASE("text input options that cannot work are refused when the UI is built", "[ui][widgets][text_input]") {
    REQUIRE_THROWS_WITH(TextInput("T", { .maxLength = 0 }).create(), ContainsSubstring("text input \"T\": maxLength"));
    const auto input = TextInput("T").create();
    REQUIRE(input->accepts(ValueKind::Text));
    REQUIRE_FALSE(input->accepts(ValueKind::Number));
    REQUIRE(input->editsValue());
}
