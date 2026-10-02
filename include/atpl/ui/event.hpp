#pragma once

#include "atpl/ui/id.hpp"
#include "atpl/ui/value.hpp"

#include <SFML/System/Vector2.hpp>
#include <SFML/Window/Keyboard.hpp>
#include <SFML/Window/Mouse.hpp>

#include <concepts>
#include <optional>
#include <string_view>
#include <utility>
#include <variant>

namespace atpl {

// Everything the UI tells the application arrives as an `Event`, in one stream, on the main thread:
//
//     ui.handleInput();
//     for (const Event& event : ui.events()) {
//         if (event.isButton("Reset")) { ... }
//         if (const auto* scroll = event.getIf<Scrolled>()) { ... }
//     }
//
// There are two sources of events.
//
// Widget events: the user did something with a widget.
//
// Forwarded input: the user did something the UI has no use for, so the application gets it.
// What the UI keeps for itself:
// - pointer presses, releases, moves and scrolling over a panel
// - keys and text while a widget has the keyboard focus (a text input being edited)
// A press and everything up to its release go to the same place: a drag that starts in a view
// keeps being forwarded when it crosses a panel, and a drag that starts on a widget is never
// forwarded. Window events are always forwarded; the UI also reacts to a resize itself.
//
// The UI only forwards. It does not interpret forwarded input (decision D15).
//
// Names inside events (`name`, `panel`, `viewName`) refer to text owned by the UI. Like the events
// themselves, they are meant to be used while handling the event, not stored.

// ----- Widget events -----

/// A button was pressed.
struct ButtonPressed {
    WidgetId widget;
    std::string_view name;  ///< The widget's name.
    std::string_view panel; ///< The name of its panel.
};

/// The user changed a widget's value. Raised for bound and unbound widgets alike; a bound value
/// has already been written when the event arrives.
struct ValueChanged {
    WidgetId widget;
    std::string_view name;
    std::string_view panel;

    /// The new value.
    Value value;

    /// False while the interaction is still going on (a slider being dragged, text being typed),
    /// true for the change that ends it (mouse released, Enter pressed, a switch flipped).
    /// To react only once per interaction, look at final changes only.
    bool final = true;

    /// The new value as an application type; see `valueAs`.
    template <BindableValue T>
    [[nodiscard]] T as() const {
        return valueAs<T>(value);
    }
};

// ----- Forwarded input -----

/// Where the pointer is.
struct PointerLocation {
    /// Position in window pixels.
    sf::Vector2f window;

    /// The view the pointer is over: a view widget, or the background view where no panel covers
    /// it. Empty if there is no view there.
    std::optional<ViewId> view;
    std::string_view viewName; ///< That view's name, or empty.

    /// Position relative to the top-left corner of that view, in pixels. Equal to `window` if
    /// there is no view.
    sf::Vector2f inView;

    /// Whether the pointer is over the view with this name.
    [[nodiscard]] bool isIn(std::string_view name) const { return view.has_value() && viewName == name; }
};

/// The modifier keys held down.
struct Modifiers {
    bool shift = false;
    bool control = false;
    bool alt = false;
    bool system = false;
};

struct PointerPressed {
    sf::Mouse::Button button;
    PointerLocation pointer;
};

struct PointerReleased {
    sf::Mouse::Button button;
    PointerLocation pointer;
};

struct PointerMoved {
    PointerLocation pointer;
    sf::Vector2f delta; ///< Movement since the last forwarded move, in pixels.
};

struct Scrolled {
    /// Wheel movement in notches. Positive is away from the user (up, or right if horizontal).
    float delta;
    bool horizontal = false;
    PointerLocation pointer;
};

struct KeyPressed {
    sf::Keyboard::Key key;
    Modifiers modifiers;
    PointerLocation pointer; ///< Where the pointer was at that moment.
};

struct KeyReleased {
    sf::Keyboard::Key key;
    Modifiers modifiers;
    PointerLocation pointer;
};

/// A character was typed. Use this for text, and key events for keys as buttons.
struct TextEntered {
    char32_t character;
};

/// The user asked to close the window. The UI does not close it; the application decides.
struct WindowClosed {};

struct WindowResized {
    sf::Vector2u size; ///< The new size in pixels.
};

struct WindowFocusChanged {
    bool focused;
};

/// One event from the UI: exactly one of the types above.
///
/// The list of types is not closed: more can be added later. Code that uses `visit` should end
/// with a catch-all (`const auto&`), so that it keeps compiling when that happens. Code that uses
/// `is` and `getIf` is not affected.
class Event {
public:
    using Data = std::variant<
        ButtonPressed,
        ValueChanged,
        PointerPressed,
        PointerReleased,
        PointerMoved,
        Scrolled,
        KeyPressed,
        KeyReleased,
        TextEntered,
        WindowClosed,
        WindowResized,
        WindowFocusChanged>;

    template <typename T>
        requires std::constructible_from<Data, T>
    Event(T data) :
        m_data(std::move(data)) {}

    /// Whether this is an event of type `T`: `event.is<WindowClosed>()`.
    template <typename T>
    [[nodiscard]] bool is() const {
        return std::holds_alternative<T>(m_data);
    }

    /// The event as a `T`, or null if it is something else:
    /// `if (const auto* scroll = event.getIf<Scrolled>()) { ... }`.
    template <typename T>
    [[nodiscard]] const T* getIf() const {
        return std::get_if<T>(&m_data);
    }

    /// Calls `visitor` with the event as its actual type.
    template <typename Visitor>
    decltype(auto) visit(Visitor&& visitor) const {
        return std::visit(std::forward<Visitor>(visitor), m_data);
    }

    [[nodiscard]] const Data& data() const { return m_data; }

    // Shorthands for the most common questions.

    /// Whether the button with this name was pressed. `name` is "Name" or "Panel/Name"; a plain
    /// name matches the button of that name in any panel.
    [[nodiscard]] bool isButton(std::string_view name) const;
    [[nodiscard]] bool isButton(WidgetId widget) const;

    /// The change, if this event is a change of the widget with this name; otherwise null.
    [[nodiscard]] const ValueChanged* changeOf(std::string_view name) const;
    [[nodiscard]] const ValueChanged* changeOf(WidgetId widget) const;

    /// Whether this key was pressed (and forwarded).
    [[nodiscard]] bool isKey(sf::Keyboard::Key key) const;

private:
    Data m_data;
};

} // namespace atpl
