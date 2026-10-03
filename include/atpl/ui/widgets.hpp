#pragma once

#include "atpl/core/param.hpp"
#include "atpl/core/series.hpp"
#include "atpl/ui/binding.hpp"
#include "atpl/ui/placement.hpp"
#include "atpl/ui/theme.hpp"
#include "atpl/ui/widget.hpp"

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace atpl {

// The widget pool.
//
// Each widget has one public type, its descriptor: `Button`, `Slider`, `Paragraph`, ... A descriptor says what
// the widget is (name, range, options), and can take what it is bound to straight away:
//
//     .widgets = {
//         Button("Reset"),
//         Slider("Speed", params.speed, {.min = 0, .max = 10}),
//         Switch("Gravity"),                     // bound later: ui.widget("Gravity").bind(...)
//         Graph("Tick time", stats.tickTimes),
//     }
//
// Each descriptor also names its widget's parts (`Slider::Track`, `Slider::Ticks`, ...), which is
// how a theme refers to them: `theme[Slider::Ticks].shown = true;`. Parts marked `Shown::No` are
// optional extras that a theme can switch on.
//
// Every widget has a name, unique within its panel. The name is how the application refers to the
// widget and, unless `label` is set, also the text shown next to it.
//
// A descriptor only accepts sources of the kind its widget works with: `Slider("Speed", flag)`
// with a `Param<bool>` does not compile.

struct ButtonOptions {
    std::string label; ///< Text on the button. Empty: the name.
};

/// A push button. Raises an event when pressed.
struct Button {
    static constexpr Kind kind{ "button" };
    static constexpr Part Face{ kind, "face", Role::Track };
    static constexpr Part Label{ kind, "label", Role::Text };

    std::string name;
    ButtonOptions options;
    std::optional<AnyBinding> binding;

    Button(std::string name, ButtonOptions options = {});
    /// `pressed` is set to true on every press. The reader sets it back to false.
    /// For most uses the button's event is the better tool.
    Button(std::string name, Param<bool>& pressed, ButtonOptions options = {});

    [[nodiscard]] std::unique_ptr<Widget> create() const;
};

struct SwitchOptions {
    std::string label;
    bool initial = false; ///< State while nothing is bound.
};

/// An on/off switch. Kind: Bool.
struct Switch {
    static constexpr Kind kind{ "switch" };
    static constexpr Part Track{ kind, "track", Role::Track };
    static constexpr Part Knob{ kind, "knob", Role::Handle };
    static constexpr Part Label{ kind, "label", Role::MutedText };

    std::string name;
    SwitchOptions options;
    std::optional<AnyBinding> binding;

    Switch(std::string name, SwitchOptions options = {});
    Switch(std::string name, Param<bool>& value, SwitchOptions options = {});
    Switch(std::string name, BoolBinding& value, SwitchOptions options = {});

    [[nodiscard]] std::unique_ptr<Widget> create() const;
};

struct SliderOptions {
    std::string label;
    double min = 0.0;
    double max = 1.0;
    double step = 0.0;           ///< Distance between allowed values. 0: continuous.
    double initial = 0.0;        ///< Value while nothing is bound.
    std::string format = "{:g}"; ///< How the value is shown, in `std::format` syntax.
};

/// A slider over a range of numbers. Kind: Number.
struct Slider {
    static constexpr Kind kind{ "slider" };
    static constexpr Part Track{ kind, "track", Role::Track };
    static constexpr Part Fill{ kind, "fill", Role::Accent };
    static constexpr Part Knob{ kind, "knob", Role::Handle };
    static constexpr Part Ticks{ kind, "ticks", Role::Line, Shown::No };
    static constexpr Part Label{ kind, "label", Role::MutedText };
    static constexpr Part ValueText{ kind, "value", Role::Text };

    std::string name;
    SliderOptions options;
    std::optional<AnyBinding> binding;

    Slider(std::string name, SliderOptions options = {});
    template <NumberValue T>
    Slider(std::string name, Param<T>& value, SliderOptions options = {});
    Slider(std::string name, NumberBinding& value, SliderOptions options = {});

    /// Throws `SetupError` for options that cannot work: `max` not above `min`, a negative
    /// `step`, a `format` that is not valid for a number.
    [[nodiscard]] std::unique_ptr<Widget> create() const;
};

template <NumberValue T>
Slider::Slider(std::string sliderName, Param<T>& value, SliderOptions sliderOptions) :
    name(std::move(sliderName)),
    options(std::move(sliderOptions)),
    binding(AnyBinding(value)) {}

struct ProgressBarOptions {
    std::string label;
    double min = 0.0;
    double max = 1.0;
};

/// A bar that shows how far a number is between `min` and `max`. Read-only. Kind: Number.
struct ProgressBar {
    static constexpr Kind kind{ "progress_bar" };
    static constexpr Part Track{ kind, "track", Role::Track };
    static constexpr Part Fill{ kind, "fill", Role::Accent };
    static constexpr Part Label{ kind, "label", Role::MutedText };

    std::string name;
    ProgressBarOptions options;
    std::optional<AnyBinding> binding;

    ProgressBar(std::string name, ProgressBarOptions options = {});
    template <NumberValue T>
    ProgressBar(std::string name, Param<T>& value, ProgressBarOptions options = {});
    ProgressBar(std::string name, NumberBinding& value, ProgressBarOptions options = {});

    /// Throws `SetupError` if `max` is not above `min`.
    [[nodiscard]] std::unique_ptr<Widget> create() const;
};

template <NumberValue T>
ProgressBar::ProgressBar(std::string barName, Param<T>& value, ProgressBarOptions barOptions) :
    name(std::move(barName)),
    options(std::move(barOptions)),
    binding(AnyBinding(value)) {}

struct TextDisplayOptions {
    std::string label;
    std::string format = "{}"; ///< How a bound number is shown, in `std::format` syntax.
    std::string initial;       ///< Text while nothing is bound.
};

/// A label with a value next to it. Read-only. Kind: Text; numbers, switches and enums are
/// accepted too and shown as text.
struct TextDisplay {
    static constexpr Kind kind{ "text_display" };
    static constexpr Part Label{ kind, "label", Role::MutedText };
    static constexpr Part ValueText{ kind, "value", Role::Text };

    std::string name;
    TextDisplayOptions options;
    std::optional<AnyBinding> binding;

    TextDisplay(std::string name, TextDisplayOptions options = {});
    template <BindableValue T>
    TextDisplay(std::string name, Param<T>& value, TextDisplayOptions options = {});
    TextDisplay(std::string name, TextBinding& value, TextDisplayOptions options = {});
    TextDisplay(std::string name, NumberBinding& value, TextDisplayOptions options = {});

    /// Throws `SetupError` if `format` cannot show a number.
    [[nodiscard]] std::unique_ptr<Widget> create() const;
};

template <BindableValue T>
TextDisplay::TextDisplay(std::string displayName, Param<T>& value, TextDisplayOptions displayOptions) :
    name(std::move(displayName)),
    options(std::move(displayOptions)),
    binding(AnyBinding(value)) {}

struct TextInputOptions {
    std::string label;
    std::string placeholder;     ///< Shown while the input is empty.
    std::size_t maxLength = 256; ///< Longest text that can be entered.
};

/// A single line of editable text. Kind: Text.
///
/// A click puts the cursor there and takes the keyboard focus. While focused: typing, the arrow
/// keys, Home, End, Backspace, Delete, and Ctrl+V to paste. Every change is reported at once
/// (`final` false), as a dragged slider's; Enter, Escape or a click elsewhere end the editing and
/// report it once more with `final` true. Text wider than the field scrolls with the cursor.
// TODO(#43): selection, copying and cutting (Ctrl+A, Ctrl+C, Ctrl+X), and jumping by words.
struct TextInput {
    static constexpr Kind kind{ "text_input" };
    static constexpr Part Field{ kind, "field", Role::Track };
    static constexpr Part Content{ kind, "content", Role::Text };
    static constexpr Part Placeholder{ kind, "placeholder", Role::MutedText };
    static constexpr Part Cursor{ kind, "cursor", Role::Accent };
    static constexpr Part Label{ kind, "label", Role::MutedText };

    std::string name;
    TextInputOptions options;
    std::optional<AnyBinding> binding;

    TextInput(std::string name, TextInputOptions options = {});
    TextInput(std::string name, Param<std::string>& value, TextInputOptions options = {});
    TextInput(std::string name, TextBinding& value, TextInputOptions options = {});

    /// Throws `SetupError` if `maxLength` is zero.
    [[nodiscard]] std::unique_ptr<Widget> create() const;
};

struct DropdownOptions {
    std::string label;
    std::size_t initial = 0;    ///< Selected entry while nothing is bound.
    std::size_t maxVisible = 8; ///< Most entries the open list shows; more scroll. Fewer if the window is small.
};

/// A choice of one entry from a list. Kind: Index, the position of the selected entry.
///
/// Bound to an enum, the enumerators must be numbered like the entries: the first entry is 0.
///
/// A click on the field opens the list above every panel: below the field, or above it if there
/// is no room below. A click on an entry, or pressing on the field, dragging to an entry and
/// releasing, chooses it. While open: Up and Down move the highlight, Enter chooses, Escape
/// closes, the wheel scrolls a long list. A click anywhere else closes it and does nothing else.
struct Dropdown {
    static constexpr Kind kind{ "dropdown" };
    static constexpr Part Field{ kind, "field", Role::Track };
    static constexpr Part Selected{ kind, "selected", Role::Text };
    static constexpr Part Arrow{ kind, "arrow", Role::Line };
    static constexpr Part List{ kind, "list", Role::Surface };
    static constexpr Part Entry{ kind, "entry", Role::Text };
    static constexpr Part Highlight{ kind, "highlight", Role::Accent };
    static constexpr Part Label{ kind, "label", Role::MutedText };
    static constexpr Part Scrollbar{ kind, "scrollbar", Role::Line }; ///< Of a list that scrolls.

    std::string name;
    std::vector<std::string> entries;
    DropdownOptions options;
    std::optional<AnyBinding> binding;

    Dropdown(std::string name, std::vector<std::string> entries, DropdownOptions options = {});
    template <typename T>
        requires IndexValue<T> || std::integral<T>
    Dropdown(std::string name, std::vector<std::string> entries, Param<T>& selected, DropdownOptions options = {});
    Dropdown(std::string name, std::vector<std::string> entries, IndexBinding& selected, DropdownOptions options = {});

    /// Throws `SetupError` if there are no entries, `initial` is not one of them, or `maxVisible`
    /// is zero.
    [[nodiscard]] std::unique_ptr<Widget> create() const;
};

template <typename T>
    requires IndexValue<T> || std::integral<T>
Dropdown::Dropdown(
    std::string dropdownName,
    std::vector<std::string> dropdownEntries,
    Param<T>& selected,
    DropdownOptions dropdownOptions
) :
    name(std::move(dropdownName)),
    entries(std::move(dropdownEntries)),
    options(std::move(dropdownOptions)) {
    if constexpr (IndexValue<T>) {
        binding = AnyBinding(selected);
    } else {
        // An integer parameter would be bound as a number; a dropdown works with positions.
        binding = AnyBinding::ofIndex(
            [&selected] { return static_cast<std::size_t>(std::max(selected.get(), T{ 0 })); },
            [&selected](std::size_t index) { selected = static_cast<T>(index); }
        );
    }
}

/// Where a graph's value axis has its reference line.
enum class GraphBase {
    Zero,    ///< The axis starts at zero (or below, if values are negative): the line is at zero.
    Average, ///< The average of the shown values is in the middle: the line is there.
};

/// What the x-axis of a graph of samples counts.
enum class GraphX {
    Count, ///< Samples: the newest is 0, older ones are negative.
    Time,  ///< Seconds, from `GraphOptions::secondsPerSample`: the newest is 0 s.
};

struct GraphOptions {
    std::string label;
    std::size_t samples = 0;  ///< How many of the newest samples the width shows. 0: as many as there are, up to 1024.
    std::optional<float> min; ///< Lower end of the value axis. Empty: follows the data.
    std::optional<float> max; ///< Upper end of the value axis. Empty: follows the data.
    float height = 0.f;       ///< Preferred height in pixels at the reference window size. 0: about four rows.

    GraphBase base = GraphBase::Zero;
    bool logarithmic = false; ///< Values on a logarithmic scale. Values at or below 0 are drawn at the bottom.

    /// For samples: what the x-axis counts. Points bring their own x values.
    GraphX x = GraphX::Count;
    float secondsPerSample = 1.f; ///< With `GraphX::Time`: the time between two samples.

    std::string format = "{:.1f}"; ///< How values are shown: the current value and the axis labels.
};

/// A line graph of a run of samples, or of points whose x values come from the data.
/// Read-only. Kind: Series.
///
/// What a graph shows beyond its curve is the theme's to decide, through optional parts:
///
///     theme[Graph::Shadow].shown = true;      // the area between curve and reference line, fading
///     theme[Graph::Value].shown = true;       // the newest value at the top right
///     theme[Graph::AxisLabels].shown = true;  // labels on both axes, worked out from the data
///     theme[Graph::Grid].shown = true;        // a background grid
struct Graph {
    static constexpr Kind kind{ "graph" };
    static constexpr Part Background{ kind, "background", Role::Track };
    static constexpr Part Curve{ kind, "curve", Role::Accent };
    static constexpr Part Baseline{ kind, "baseline", Role::Line };
    static constexpr Part Shadow{ kind, "shadow", Role::Accent, Shown::No };
    static constexpr Part Axis{ kind, "axis", Role::Line, Shown::No };
    static constexpr Part AxisLabels{ kind, "axis_labels", Role::MutedText, Shown::No };
    static constexpr Part Grid{ kind, "grid", Role::Line, Shown::No };
    static constexpr Part Label{ kind, "label", Role::MutedText };
    static constexpr Part Value{ kind, "value", Role::Text, Shown::No };

    std::string name;
    GraphOptions options;
    std::optional<AnyBinding> binding;

    Graph(std::string name, GraphOptions options = {});
    Graph(std::string name, Series& samples, GraphOptions options = {});
    Graph(std::string name, PointSeries& points, GraphOptions options = {});
    Graph(std::string name, SeriesBinding& samples, GraphOptions options = {});

    /// Throws `SetupError` for options that cannot work: `max` not above `min`, a logarithmic
    /// axis with a `min` at or below zero, a time axis without time between samples, a `format`
    /// that cannot show a number.
    [[nodiscard]] std::unique_ptr<Widget> create() const;
};

struct ParagraphOptions {
    std::string heading; ///< Shown as a heading. Empty: no heading.
    std::string text;    ///< Shown as body text, wrapped to the panel's width. Empty: no body.
    std::string footer;  ///< Shown below in muted text, wrapped. Empty: no footer.
};

/// Text in a panel: a heading, a body and a footer, each optional. With only a heading it is a
/// section heading; with only a body, a paragraph; with only a footer, a hint.
///
///     Paragraph("Rendering", {.heading = "Rendering"})
///     Paragraph("Help", {.text = "Drag to move the view. Scroll to zoom."})
///     Paragraph("About", {.heading = "Ants", .text = "...", .footer = "v1.5"})
///
/// Each of the three is a part of its own text type, so a theme styles them separately.
/// The body can be bound; it then shows the bound text instead of `text`. Read-only. Kind: Text.
/// Unlike other widgets, the name is not shown anywhere.
struct Paragraph {
    static constexpr Kind kind{ "paragraph" };
    static constexpr Part Heading{ kind, "heading", Role::Heading };
    static constexpr Part Body{ kind, "body", Role::Text };
    static constexpr Part Footer{ kind, "footer", Role::MutedText };

    std::string name;
    ParagraphOptions options;
    std::optional<AnyBinding> binding;

    Paragraph(std::string name, ParagraphOptions options = {});
    Paragraph(std::string name, Param<std::string>& text, ParagraphOptions options = {});
    Paragraph(std::string name, TextBinding& text, ParagraphOptions options = {});

    [[nodiscard]] std::unique_ptr<Widget> create() const;
};

struct ViewOptions {
    /// Height in pixels before GUI scaling. 0: in a panel placed in a grid cell, the view takes
    /// all height the other widgets leave; in a floating panel, `width / aspectRatio`.
    float height = 0.f;
    float aspectRatio = 16.f / 9.f;
};

/// A region the application draws into itself: the main view of a simulation, a minimap, ...
/// Its name is what `UI::view(name)` finds. Takes no binding; it has a draw function instead.
struct View {
    static constexpr Kind kind{ "view" };
    static constexpr Part Frame{ kind, "frame", Role::Line, Shown::No };

    /// Marks the descriptor as one of a view: the UI then keeps a view of this name for it.
    static constexpr bool isView = true;

    std::string name;
    ViewOptions options;

    View(std::string name, ViewOptions options = {});

    [[nodiscard]] std::unique_ptr<Widget> create() const;
};

/// What a type must offer to be used as a widget descriptor: a name, and a way to make the widget.
/// Applications that implement their own widget type write a descriptor for it the same way.
///
/// Two more members are looked at if a descriptor has them:
/// - `binding`, a `std::optional<AnyBinding>`: what the widget is bound to from the start.
/// - `isView`, a `static constexpr bool`: if true, the widget is a region the application draws
///   into, found with `UI::view(name)`.
template <typename D>
concept WidgetDescriptor = requires(const D& descriptor) {
    { descriptor.name } -> std::convertible_to<std::string_view>;
    { descriptor.create() } -> std::same_as<std::unique_ptr<Widget>>;
};

/// One widget of a panel, as stored in `PanelSetup`. Every descriptor converts to it, so
/// descriptors of different widgets can be listed together.
class WidgetSetup {
public:
    template <WidgetDescriptor D>
    WidgetSetup(D descriptor);

    [[nodiscard]] std::string_view name() const;

    /// The widget's position in its panel's grid, if it was given one with `at`.
    [[nodiscard]] std::optional<GridCell> cell() const;

    /// How many cells of its panel's grid the widget takes: what `at` or `spanning` said, or one.
    [[nodiscard]] GridSpan span() const;

    /// The colours the widget was given with `colored`; empty fields mean its panel's.
    [[nodiscard]] ColorOverride colors() const;

    /// What the descriptor was given to bind the widget to, if anything.
    [[nodiscard]] const std::optional<AnyBinding>& binding() const;

    /// Whether the widget is a view: see `View`.
    [[nodiscard]] bool isView() const;

    /// Makes the widget. The UI calls this once for every widget when it is built.
    [[nodiscard]] std::unique_ptr<Widget> create() const;

private:
    friend WidgetSetup at(GridCell cell, WidgetSetup widget);
    friend WidgetSetup spanning(GridSpan span, WidgetSetup widget);
    friend WidgetSetup colored(ColorOverride colors, WidgetSetup widget);

    std::string m_name;
    std::optional<AnyBinding> m_binding;
    bool m_isView = false;
    std::function<std::unique_ptr<Widget>()> m_create;
    std::optional<GridCell> m_cell;
    GridSpan m_span;
    ColorOverride m_colors;
};

namespace detail {

/// The descriptor's `binding` member, if it has one.
template <typename D>
[[nodiscard]] std::optional<AnyBinding> bindingOf(const D& descriptor) {
    if constexpr (requires {
                      { descriptor.binding } -> std::convertible_to<std::optional<AnyBinding>>;
                  }) {
        return descriptor.binding;
    } else {
        return std::nullopt;
    }
}

/// Whether the descriptor type says it describes a view.
template <typename D>
inline constexpr bool describesView = requires { requires D::isView; };

} // namespace detail

// The constructor takes from the descriptor what the UI needs to know before the widget exists,
// and keeps the descriptor to make the widget from. (Members are set in the order they are
// declared in: name and binding are copied before the descriptor is moved.)
template <WidgetDescriptor D>
WidgetSetup::WidgetSetup(D descriptor) :
    m_name(descriptor.name),
    m_binding(detail::bindingOf(descriptor)),
    m_isView(detail::describesView<D>),
    m_create([kept = std::move(descriptor)] { return kept.create(); }) {}

/// Gives a widget a position in its panel's grid:
///
///     at({.column = 1, .row = 0}, Slider("Size", params.size))
///     at({.row = 1, .columnSpan = 2}, Graph("Tick time", stats.tickTimes))
///
/// See `PanelSetup::widgets` for the rules.
[[nodiscard]] WidgetSetup at(GridCell cell, WidgetSetup widget);

/// Gives a widget a size in its panel's grid and leaves its position to the panel:
///
///     spanning({.columns = 2, .rows = 3}, Graph("Tick time", stats.tickTimes))
///
/// See `PanelSetup::widgets` for the rules.
[[nodiscard]] WidgetSetup spanning(GridSpan span, WidgetSetup widget);

/// Gives a single widget colours that differ from its panel's:
///
///     colored({.accent = 3}, Button("Delete"))
///
/// Only the colours that are named differ. Can be combined with `at` and `spanning`.
[[nodiscard]] WidgetSetup colored(ColorOverride colors, WidgetSetup widget);

} // namespace atpl
