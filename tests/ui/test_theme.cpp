#include "atpl/ui/setup.hpp"
#include "atpl/ui/theme.hpp"
#include "atpl/ui/widgets.hpp"

#include <catch2/catch_test_macros.hpp>

#include <set>

using namespace atpl;

TEST_CASE("a part is identified by its kind and its name", "[ui][theme]") {
    constexpr Kind slider{ "slider" };
    constexpr Kind graph{ "graph" };

    constexpr Part track{ slider, "track", Role::Track };
    constexpr Part sameTrack{ slider, "track", Role::Track };
    constexpr Part knob{ slider, "knob", Role::Handle };
    constexpr Part graphTrack{ graph, "track", Role::Track };

    REQUIRE(track == sameTrack);
    REQUIRE(track.id() == sameTrack.id());

    REQUIRE_FALSE(track == knob);
    REQUIRE(track.id() != knob.id());

    // The same part name under another kind is another part.
    REQUIRE_FALSE(track == graphTrack);
    REQUIRE(track.id() != graphTrack.id());

    static_assert(Part{ Kind{ "a" }, "b", Role::Line }.id() == Part{ Kind{ "a" }, "b", Role::Text }.id());
}

TEST_CASE("kind and part names cannot run into each other", "[ui][theme]") {
    // "ab" + "c" and "a" + "bc" must not be the same part.
    constexpr Part first{ Kind{ "ab" }, "c", Role::Line };
    constexpr Part second{ Kind{ "a" }, "bc", Role::Line };

    REQUIRE(first.id() != second.id());
}

TEST_CASE("a part is shown by default unless it says otherwise", "[ui][theme]") {
    REQUIRE(Slider::Track.shown == Shown::Yes);
    REQUIRE(Slider::Ticks.shown == Shown::No);
    REQUIRE(Graph::Axis.shown == Shown::No);
    REQUIRE(Graph::Grid.shown == Shown::No);
    REQUIRE(View::Frame.shown == Shown::No);
}

TEST_CASE("each piece of text has the role of its text type", "[ui][theme]") {
    REQUIRE(Panel::Title.role == Role::Title);
    REQUIRE(Paragraph::Heading.role == Role::Heading);
    REQUIRE(Paragraph::Body.role == Role::Text);
    REQUIRE(Paragraph::Footer.role == Role::MutedText);
    REQUIRE(Slider::Label.role == Role::MutedText);
    REQUIRE(Slider::ValueText.role == Role::Text);
    REQUIRE(Button::Label.role == Role::Text);

    // A button's face is a neutral area that text sits on, not a bright knob.
    REQUIRE(Button::Face.role == Role::Track);
    REQUIRE(Slider::Knob.role == Role::Handle);
}

TEST_CASE("text types have a size each and share the default font unless they name one", "[ui][theme]") {
    const Typography typography;

    REQUIRE(typography.title.size > typography.text.size);
    REQUIRE(typography.text.size > typography.muted.size);
    REQUIRE(typography.title.font == nullptr);
    REQUIRE(typography.muted.font == nullptr);
}

TEST_CASE("all parts of the built-in widgets and the panel have distinct ids", "[ui][theme]") {
    const Part parts[] = {
        Button::Face,
        Button::Label,
        Switch::Track,
        Switch::Knob,
        Switch::Label,
        Slider::Track,
        Slider::Fill,
        Slider::Knob,
        Slider::Ticks,
        Slider::Label,
        Slider::ValueText,
        ProgressBar::Track,
        ProgressBar::Fill,
        ProgressBar::Label,
        ValueDisplay::Label,
        ValueDisplay::ValueText,
        TextInput::Field,
        TextInput::Content,
        TextInput::Placeholder,
        TextInput::Cursor,
        TextInput::Label,
        Dropdown::Field,
        Dropdown::Selected,
        Dropdown::Arrow,
        Dropdown::List,
        Dropdown::Entry,
        Dropdown::Highlight,
        Dropdown::Label,
        Graph::Background,
        Graph::Curve,
        Graph::Axis,
        Graph::Grid,
        Graph::Label,
        Paragraph::Heading,
        Paragraph::Body,
        Paragraph::Footer,
        View::Frame,
        Panel::Background,
        Panel::Header,
        Panel::Title,
        Panel::Scrollbar,
    };

    std::set<std::uint64_t> ids;
    for (const Part& part : parts) {
        ids.insert(part.id());
    }

    REQUIRE(ids.size() == std::size(parts));
}

TEST_CASE("states combine as flags", "[ui][theme]") {
    const State state = State::Hovered | State::Active;

    REQUIRE(has(state, State::Hovered));
    REQUIRE(has(state, State::Active));
    REQUIRE_FALSE(has(state, State::Pressed));
    REQUIRE_FALSE(has(state, State::Disabled));
    REQUIRE_FALSE(has(State::Normal, State::Hovered));

    static_assert(has(State::Pressed | State::Focused, State::Focused));
}

TEST_CASE("a part override changes nothing until a field is set", "[ui][theme]") {
    PartOverride entry;

    REQUIRE_FALSE(entry.shown.has_value());
    REQUIRE_FALSE(entry.color.has_value());
    REQUIRE_FALSE(entry.radius.has_value());

    entry.shown = true;
    entry.radius = 0.f;

    REQUIRE(entry.shown == true);
    REQUIRE(entry.radius == 0.f);
    REQUIRE_FALSE(entry.thickness.has_value());
}
