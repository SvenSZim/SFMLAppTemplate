#pragma once

#include "atpl/ui/binding.hpp"
#include "atpl/ui/event.hpp"
#include "atpl/ui/rect.hpp"
#include "atpl/ui/theme.hpp"
#include "atpl/ui/value.hpp"

#include <SFML/System/Vector2.hpp>

#include <optional>
#include <span>
#include <string_view>

namespace atpl {

// The interface every widget type implements, and what a widget is handed to do its work.
//
// A widget owns its behaviour and nothing else:
// - It says how high it wants to be; layout decides where it goes.
// - It says which parts it has and where they are; the theme decides how they look.
// - It emits shapes through the painter; the renderer draws them.
// - It reports what the user did through its context; the UI writes bound values and raises events.
//
// The built-in widgets are written against this interface, and so are an application's own.
// To add a widget type: implement `Widget`, declare its parts, and write a descriptor with a
// `name` and a `create()` (see widgets.hpp).

/// What `Widget::measure` answers.
struct SizeRequest {
    /// The height the widget wants at the width it was offered, in pixels.
    float height = 0.f;

    /// True if the widget takes all height that is left over in its panel instead: a view in a
    /// panel of fixed height. `height` is then the least it needs.
    bool stretch = false;
};

/// What a widget can ask while being measured.
class MeasureContext {
public:
    /// The width the widget will get, in pixels. Widths are decided by layout, never by widgets.
    [[nodiscard]] float width() const;

    /// The theme's sizes, with the GUI scale applied.
    [[nodiscard]] const Metrics& metrics() const;

    /// The size one line of text would have when drawn as `part`: in the size and font of the
    /// part's text type.
    [[nodiscard]] sf::Vector2f textSize(std::string_view text, const Part& part) const;

    /// The height text would have when drawn as `part` and wrapped to `width`.
    [[nodiscard]] float wrappedTextHeight(std::string_view text, const Part& part, float width) const;
};

/// What a widget can do while handling input.
class InputContext {
public:
    /// The widget's size in pixels. The widget's own coordinates run from (0, 0) at its top-left
    /// corner to this.
    [[nodiscard]] sf::Vector2f size() const;

    /// A pointer position in the widget's own coordinates.
    [[nodiscard]] sf::Vector2f local(const PointerLocation& pointer) const;

    /// Hovered, pressed, focused, disabled: kept by the UI, not by the widget.
    [[nodiscard]] State state() const;

    [[nodiscard]] const Metrics& metrics() const;

    /// Says that the widget looks different now. Its panel is redrawn. A change of `state()` is
    /// noticed without this.
    void markDirty();

    /// Sends all pointer input to this widget until `releasePointer()`, wherever the pointer goes:
    /// for dragging. Released automatically when the button is.
    void capturePointer();
    void releasePointer();

    /// Takes or gives up the keyboard focus. While focused, the widget gets keys and text.
    void requestFocus();
    void releaseFocus();

    /// Reports that the user changed the widget's value. The UI writes the bound value, if any,
    /// and raises `ValueChanged`. `final` is false while the interaction is still going on.
    void changeValue(Value value, bool final = true);

    /// Reports that the user pressed the widget as a button. The UI raises `ButtonPressed`.
    void press();
};

/// What a widget can do while time passes.
class UpdateContext {
public:
    [[nodiscard]] State state() const;
    [[nodiscard]] const Metrics& metrics() const;

    /// Says that the widget looks different now. Call it for every step of an animation; while
    /// nothing calls it, nothing is redrawn.
    void markDirty();
};

/// How text is placed inside the rectangle it is given. Vertically it is always centred.
enum class Align { Left, Center, Right };

/// The resolved look of a widget's parts, handed to `Widget::paint`.
class Style {
public:
    /// The style of one of the widget's parts in the widget's current state.
    [[nodiscard]] PartStyle part(const Part& part) const;

    /// The same with further state added for this part only, for example `State::Active` for the
    /// track of a switch that is on.
    [[nodiscard]] PartStyle part(const Part& part, State additional) const;

    /// The widget's current state.
    [[nodiscard]] State state() const;

    [[nodiscard]] const Metrics& metrics() const;
};

/// What a widget draws with. Coordinates are the widget's own: (0, 0) is its top-left corner.
///
/// Every call takes the style of the part being drawn. A part whose style says it is not shown
/// is skipped, so a widget can draw its optional parts without checking.
class Painter {
public:
    /// The widget's size in pixels.
    [[nodiscard]] sf::Vector2f size() const;

    /// A box: fill, outline, corner radius and shadow come from the style.
    void box(const FloatRect& rect, const PartStyle& style);

    /// A straight line in the style's colour and thickness.
    void line(sf::Vector2f from, sf::Vector2f to, const PartStyle& style);

    /// Connected lines through all points, for curves.
    void polyline(std::span<const sf::Vector2f> points, const PartStyle& style);

    /// One line of text inside a rectangle, in the style's colour, size and font. Text that is
    /// too wide ends in an ellipsis.
    void text(const FloatRect& rect, std::string_view text, const PartStyle& style, Align align = Align::Left);

    /// Text wrapped at word boundaries to the rectangle's width, starting at its top.
    void wrappedText(const FloatRect& rect, std::string_view text, const PartStyle& style, Align align = Align::Left);

    /// The size `text` would have in this style.
    [[nodiscard]] sf::Vector2f textSize(std::string_view text, const PartStyle& style) const;
};

/// The interface of a widget type. One object per widget on screen.
class Widget {
public:
    virtual ~Widget() = default;

    // ----- Size -----

    /// How high the widget wants to be at `context.width()`.
    [[nodiscard]] virtual SizeRequest measure(const MeasureContext& context) const = 0;

    // ----- Input -----

    /// Reacts to input meant for this widget: pointer events over it (or captured by it), and
    /// keys and text while it has the focus. The events are the same types the application gets
    /// (event.hpp). Returns true if the event was used.
    ///
    /// The widget changes its own state here and reports through the context: `markDirty`,
    /// `changeValue`, `press`.
    virtual bool handleInput(const Event& /*event*/, InputContext& /*context*/) { return false; }

    // ----- Time -----

    /// Called once per frame with the time since the last frame, in seconds. For animations.
    /// A widget that is not animating does nothing here and causes no redraw.
    virtual void update(float /*dt*/, UpdateContext& /*context*/) {}

    // ----- Drawing -----

    /// Emits the widget's shapes. Called only when the widget's panel is redrawn, not every frame.
    /// `style.part(...)` gives the look of each part; the widget only decides where parts are.
    virtual void paint(Painter& painter, const Style& style) const = 0;

    // ----- Value -----
    //
    // A widget never sees what it is bound to. It keeps its own copy of its value. The UI hands
    // it new values (`setValue`) and is told about the user's changes (`InputContext::changeValue`).

    /// Whether the widget can show or edit a value of this kind. Decides what it can be bound to.
    [[nodiscard]] virtual bool accepts(ValueKind /*kind*/) const { return false; }

    /// Whether the user can change the value through this widget. Such a widget refuses a
    /// read-only binding.
    [[nodiscard]] virtual bool editsValue() const { return false; }

    /// The value the widget currently holds, or nothing if it has none (a button, a graph).
    [[nodiscard]] virtual std::optional<Value> value() const { return std::nullopt; }

    /// Gives the widget a new value: the bound value changed, or the application set one.
    /// The kind is one the widget accepts. Must not report the change back through a context.
    virtual void setValue(const Value& /*value*/) {}

    /// For widgets that accept `ValueKind::Series`: where to read samples from, or null when
    /// nothing is bound. The widget reads when the source's revision changes, in `update`.
    virtual void setSeries(const SeriesBinding* /*source*/) {}
};

} // namespace atpl
