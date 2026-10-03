#pragma once

#include "atpl/ui/binding.hpp"
#include "atpl/ui/event.hpp"
#include "atpl/ui/id.hpp"
#include "atpl/ui/layout.hpp"
#include "atpl/ui/rect.hpp"
#include "atpl/ui/theme.hpp"
#include "atpl/ui/value.hpp"

#include <SFML/System/Vector2.hpp>

#include <chrono>
#include <optional>
#include <span>
#include <string_view>

namespace atpl {

namespace render {
class DrawList;     // what a panel draws; internal
class TextMeasurer; // how much room text takes; internal
} // namespace render

namespace input {
class InputSystem; // what keeps hover, press, capture and focus; internal
} // namespace input

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

/// What `Widget::measure` answers: the size the widget would like, and how far it can go below
/// and above it.
///
///   min         the least it needs to be displayable at all. Below this it is not drawn.
///   preferred   its normal, comfortable size. A panel that is as large as its content is sized
///               from this.
///   max         the most it makes use of. In a grid with room to spare it grows up to here.
///
/// There are two kinds of widgets when it comes to size (docs/LAYOUT.md):
///
///   constant   has a maximum: a slider or a button gains nothing from being huge. Given more
///              room than that, it stays at its maximum and is placed in the room by the layout
///              theme's alignment.
///   dynamic    has no maximum: a graph or a view fills whatever it is given.
///
/// All three are worked out from the layout's sizes (`context.sizes()`), so they grow and shrink
/// with the window like everything else.
///
///     const float row = context.sizes().rowHeight;
///     return { .min = { 80.f, row * 0.75f }, .preferred = { 160.f, row }, .max = sf::Vector2f(100000.f, row * 1.25f)
///     }; return { .min = { 120.f, 60.f }, .preferred = { 240.f, 120.f } };   // dynamic
struct SizeRequest {
    /// The least the widget needs to be displayable, in pixels: from its text sizes and from
    /// how much it has to show.
    sf::Vector2f min;

    /// The size the widget has when nothing presses or stretches it. Where this is smaller than
    /// `min`, `min` counts: a widget that sets only `min` prefers its minimum.
    sf::Vector2f preferred;

    /// The most the widget makes use of. Empty: the widget is dynamic.
    std::optional<sf::Vector2f> max;

    /// Limits on the widget's shape, each 0 for none: its width divided by its height is at most
    /// `widestRatio`, its height divided by its width at most `tallestRatio`. Both together pin
    /// the shape: 16 / 9 and 9 / 16 keep a view at 16:9.
    float widestRatio = 0.f;
    float tallestRatio = 0.f;

    [[nodiscard]] bool isDynamic() const { return !max.has_value(); }
};

/// What a widget can ask while being measured.
class MeasureContext {
public:
    /// Made by the UI for each widget it measures. `theme`, `sizes` and `measurer` must outlive
    /// the context; without a measurer, text measures as nothing.
    MeasureContext(
        float width,
        const Theme& theme,
        PanelColors colors,
        const Sizes& sizes,
        const render::TextMeasurer* measurer = nullptr
    );

    /// The width the widget is offered, in pixels: of its column or its cells. Widths are
    /// decided by layout, never by widgets. A widget whose height depends on its width (text
    /// that wraps) works its minimum height out from this.
    [[nodiscard]] float width() const;

    /// The layout's sizes for the window as it is now, in pixels.
    [[nodiscard]] const Sizes& sizes() const;

    /// The size one line of text would have when drawn as `part`: in the size and font of the
    /// part's text type.
    [[nodiscard]] sf::Vector2f textSize(std::string_view text, const Part& part) const;

    /// The height text would have when drawn as `part` and wrapped to `width`.
    [[nodiscard]] float wrappedTextHeight(std::string_view text, const Part& part, float width) const;

private:
    float m_width;
    const Theme* m_theme;
    PanelColors m_colors;
    const Sizes* m_sizes;
    const render::TextMeasurer* m_measurer;
};

/// What a widget can do while handling input.
class InputContext {
public:
    /// Made by the UI for the widget an event is meant for. `origin` is the widget's top-left
    /// corner in the window.
    InputContext(
        input::InputSystem& input,
        WidgetId widget,
        sf::Vector2f origin,
        sf::Vector2f size,
        State state,
        const Sizes& sizes,
        std::optional<FloatRect> overlay = std::nullopt,
        const Theme* theme = nullptr,
        PanelColors colors = {},
        const render::TextMeasurer* measurer = nullptr
    );

    /// The widget's size in pixels. The widget's own coordinates run from (0, 0) at its top-left
    /// corner to this.
    [[nodiscard]] sf::Vector2f size() const;

    /// A pointer position in the widget's own coordinates.
    [[nodiscard]] sf::Vector2f local(const PointerLocation& pointer) const;

    /// Hovered, pressed, focused, disabled: kept by the UI, not by the widget.
    [[nodiscard]] State state() const;

    [[nodiscard]] const Sizes& sizes() const;

    /// The size one line of text would have when drawn as `part`, as `MeasureContext::textSize`
    /// says: for finding the character under the pointer.
    [[nodiscard]] sf::Vector2f textSize(std::string_view text, const Part& part) const;

    /// Says that the widget looks different now. Its panel is redrawn. A change of `state()` is
    /// noticed without this.
    void markDirty();

    /// Opens or closes the widget's overlay (see `Widget::overlaySize`). Opening it closes any
    /// other.
    void openOverlay();
    void closeOverlay();

    /// Where the open overlay is, in the widget's own coordinates: below it, so from its height
    /// down, or above it, at negative heights. Empty while it is closed, or not placed yet.
    [[nodiscard]] std::optional<FloatRect> overlay() const;

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

    /// Reports that the user pressed the widget as a button. The UI raises `ButtonPressed`, and
    /// sets a bound on/off value to true.
    void press();

private:
    input::InputSystem* m_input;
    WidgetId m_widget;
    sf::Vector2f m_origin;
    sf::Vector2f m_size;
    State m_state;
    const Sizes* m_sizes;
    std::optional<FloatRect> m_overlay;
    const Theme* m_theme;
    PanelColors m_colors;
    const render::TextMeasurer* m_measurer;
};

/// What a widget can do while time passes.
class UpdateContext {
public:
    [[nodiscard]] State state() const;
    [[nodiscard]] const Sizes& sizes() const;

    /// Says that the widget looks different now. Call it for every step of an animation; while
    /// nothing calls it, nothing is redrawn.
    void markDirty();
};

/// How text is placed inside the rectangle it is given. Vertically it is always centred.
enum class Align { Left, Center, Right };

/// The resolved look of a widget's parts, handed to `Widget::paint`.
class Style {
public:
    /// Made by the UI for each widget it paints. `sizes` must outlive the style.
    Style(const Theme& theme, PanelColors colors, State state, const Sizes& sizes);

    /// The style of one of the widget's parts in the widget's current state.
    [[nodiscard]] PartStyle part(const Part& part) const;

    /// The same with further state added for this part only, for example `State::Active` for the
    /// track of a switch that is on.
    [[nodiscard]] PartStyle part(const Part& part, State additional) const;

    /// The widget's current state.
    [[nodiscard]] State state() const;

    /// The layout's sizes for the window as it is now, in pixels.
    [[nodiscard]] const Sizes& sizes() const;

private:
    const Theme* m_theme;
    PanelColors m_colors;
    State m_state;
    const Sizes* m_sizes;
};

/// What a widget draws with. Coordinates are the widget's own: (0, 0) is its top-left corner.
///
/// Every call takes the style of the part being drawn. A part whose style says it is not shown
/// is skipped, so a widget can draw its optional parts without checking.
class Painter {
public:
    /// Made by the UI for each widget it paints: `origin` is the widget's top-left corner in its
    /// panel, `size` its size.
    Painter(
        render::DrawList& list, sf::Vector2f origin, sf::Vector2f size, const render::TextMeasurer* measurer = nullptr
    );

    /// The widget's size in pixels.
    [[nodiscard]] sf::Vector2f size() const;

    /// A box: fill, outline, corner radius and shadow come from the style.
    void box(const FloatRect& rect, const PartStyle& style);

    /// A straight line in the style's colour and thickness.
    void line(sf::Vector2f from, sf::Vector2f to, const PartStyle& style);

    /// Connected lines through all points, for curves.
    void polyline(std::span<const sf::Vector2f> points, const PartStyle& style);

    /// The area between a curve and the horizontal line at `baseline`, in the style's colour.
    /// It is strongest where the curve is furthest from the line and fades to nothing at the
    /// line: the "shadow" of a graph. Parts of the curve above and below the line are both filled.
    void area(std::span<const sf::Vector2f> points, float baseline, const PartStyle& style);

    /// One line of text inside a rectangle, in the style's colour, size and font. Text that is
    /// too wide ends in an ellipsis.
    void text(const FloatRect& rect, std::string_view text, const PartStyle& style, Align align = Align::Left);

    /// Text wrapped at word boundaries to the rectangle's width, starting at its top.
    void wrappedText(const FloatRect& rect, std::string_view text, const PartStyle& style, Align align = Align::Left);

    /// The size `text` would have in this style.
    [[nodiscard]] sf::Vector2f textSize(std::string_view text, const PartStyle& style) const;

    /// The height `text` would have in this style, wrapped to `width`.
    [[nodiscard]] float wrappedTextHeight(std::string_view text, const PartStyle& style, float width) const;

private:
    render::DrawList* m_list;
    sf::Vector2f m_origin;
    sf::Vector2f m_size;
    const render::TextMeasurer* m_measurer;
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

    /// Whether the widget answers to the pointer: is hovered, pressed and handed pointer input.
    /// Widgets that only show something (a text display, a graph) say no; the pointer then
    /// passes over them as over the panel's background, and they never look hovered.
    [[nodiscard]] virtual bool reactsToPointer() const { return true; }

    /// Called when the widget loses the keyboard focus, however that happens: it gave it up, the
    /// user pressed elsewhere, or another widget took it. A text input reports its final value
    /// here.
    virtual void focusLost(InputContext& /*context*/) {}

    // ----- Overlay -----
    //
    // A widget can have one thing that is drawn above every panel and may reach beyond its own
    // rectangle: a dropdown's list. It opens it with `InputContext::openOverlay()`. While it is
    // open, the widget's state has `State::Open`, all pointer input goes to the widget, and a
    // press anywhere but on the widget or its overlay closes it and is used up. Only one overlay
    // is open at a time. The UI also closes it when the widget's panel folds or the widget is
    // hidden or disabled.
    //
    // The UI places the overlay right below the widget's anchor (by default the whole widget),
    // as wide as it and without a gap; right above it if it does not fit below; and inside the
    // window if it fits neither way.

    /// The part of the widget its overlay is attached to, in the widget's coordinates, for a
    /// widget of `size`: a dropdown's field, not its label. By default the whole widget.
    [[nodiscard]] virtual FloatRect overlayAnchor(const MeasureContext& /*context*/, sf::Vector2f size) const {
        return { { 0.f, 0.f }, size };
    }

    /// The size the open overlay would like, at most `maxHeight` high: the room there is. Asked
    /// again with less room if it does not fit; a list then shows fewer entries.
    [[nodiscard]] virtual sf::Vector2f overlaySize(const MeasureContext& /*context*/, float /*maxHeight*/) const {
        return {};
    }

    /// Emits the overlay's shapes, in the overlay's own coordinates: (0, 0) is its top-left
    /// corner, `painter.size()` its size. `anchor` is where the widget's anchor is in the same
    /// coordinates, just above or below. The overlay is drawn above every panel, and may also
    /// draw over its anchor: to join the two into one shape. Called when the overlay opens,
    /// moves, or the widget's panel is redrawn.
    virtual void paintOverlay(Painter& /*painter*/, const Style& /*style*/, const FloatRect& /*anchor*/) const {}

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

    /// How often at most the widget is handed a new value of its binding, or told that its series
    /// changed. A bound value that changes every tick would otherwise repaint the panel every
    /// frame; a change in between is not lost, it arrives with the next hand-over.
    ///
    /// By default: at once for widgets that edit their value or show a series (a slider must
    /// follow, a graph must move smoothly), eight times per second for values that are only
    /// shown. A widget type overrides this if it needs something else.
    [[nodiscard]] virtual std::chrono::milliseconds refreshInterval() const {
        return editsValue() || accepts(ValueKind::Series) ? std::chrono::milliseconds(0)
                                                          : std::chrono::milliseconds(125);
    }
};

} // namespace atpl
