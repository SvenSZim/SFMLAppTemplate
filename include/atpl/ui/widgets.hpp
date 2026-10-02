#pragma once

#include "atpl/core/param.hpp"
#include "atpl/core/series.hpp"
#include "atpl/ui/binding.hpp"

#include <concepts>
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace atpl {

class Widget; // the interface every widget type implements, see widget.hpp

// The widget pool.
//
// Each widget has one public type, its descriptor: `Button`, `Slider`, ... A descriptor says what
// the widget is (name, range, options), and can take what it is bound to straight away:
//
//     .widgets = {
//         Button("Reset"),
//         Slider("Speed", params.speed, {.min = 0, .max = 10}),
//         Switch("Gravity"),                     // bound later: ui.widget("Gravity").bind(...)
//         Graph("Tick time", stats.tickTimes),
//     }
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
    std::string name;
    SliderOptions options;
    std::optional<AnyBinding> binding;

    Slider(std::string name, SliderOptions options = {});
    template <NumberValue T>
    Slider(std::string name, Param<T>& value, SliderOptions options = {});
    Slider(std::string name, NumberBinding& value, SliderOptions options = {});

    [[nodiscard]] std::unique_ptr<Widget> create() const;
};

struct ProgressBarOptions {
    std::string label;
    double min = 0.0;
    double max = 1.0;
};

/// A bar that shows how far a number is between `min` and `max`. Read-only. Kind: Number.
struct ProgressBar {
    std::string name;
    ProgressBarOptions options;
    std::optional<AnyBinding> binding;

    ProgressBar(std::string name, ProgressBarOptions options = {});
    template <NumberValue T>
    ProgressBar(std::string name, Param<T>& value, ProgressBarOptions options = {});
    ProgressBar(std::string name, NumberBinding& value, ProgressBarOptions options = {});

    [[nodiscard]] std::unique_ptr<Widget> create() const;
};

struct TextDisplayOptions {
    std::string label;
    std::string format = "{}"; ///< How a bound number is shown, in `std::format` syntax.
    std::string initial;       ///< Text while nothing is bound.
};

/// A label with a value next to it. Read-only. Kind: Text; numbers, switches and enums are
/// accepted too and shown as text.
struct TextDisplay {
    std::string name;
    TextDisplayOptions options;
    std::optional<AnyBinding> binding;

    TextDisplay(std::string name, TextDisplayOptions options = {});
    template <BindableValue T>
    TextDisplay(std::string name, Param<T>& value, TextDisplayOptions options = {});
    TextDisplay(std::string name, TextBinding& value, TextDisplayOptions options = {});
    TextDisplay(std::string name, NumberBinding& value, TextDisplayOptions options = {});

    [[nodiscard]] std::unique_ptr<Widget> create() const;
};

struct TextInputOptions {
    std::string label;
    std::string placeholder;     ///< Shown while the input is empty.
    std::size_t maxLength = 256; ///< Longest text that can be entered.
};

/// A single line of editable text. Kind: Text.
struct TextInput {
    std::string name;
    TextInputOptions options;
    std::optional<AnyBinding> binding;

    TextInput(std::string name, TextInputOptions options = {});
    TextInput(std::string name, Param<std::string>& value, TextInputOptions options = {});
    TextInput(std::string name, TextBinding& value, TextInputOptions options = {});

    [[nodiscard]] std::unique_ptr<Widget> create() const;
};

struct DropdownOptions {
    std::string label;
    std::size_t initial = 0; ///< Selected entry while nothing is bound.
};

/// A choice of one entry from a list. Kind: Index, the position of the selected entry.
///
/// Bound to an enum, the enumerators must be numbered like the entries: the first entry is 0.
struct Dropdown {
    std::string name;
    std::vector<std::string> entries;
    DropdownOptions options;
    std::optional<AnyBinding> binding;

    Dropdown(std::string name, std::vector<std::string> entries, DropdownOptions options = {});
    template <typename T>
        requires IndexValue<T> || std::integral<T>
    Dropdown(std::string name, std::vector<std::string> entries, Param<T>& selected, DropdownOptions options = {});
    Dropdown(std::string name, std::vector<std::string> entries, IndexBinding& selected, DropdownOptions options = {});

    [[nodiscard]] std::unique_ptr<Widget> create() const;
};

struct GraphOptions {
    std::string label;
    std::size_t samples = 0;   ///< How many of the newest samples are shown. 0: all the source holds.
    std::optional<float> min;  ///< Lower end of the value axis. Empty: follows the data.
    std::optional<float> max;  ///< Upper end of the value axis. Empty: follows the data.
    float height = 0.f;        ///< Height in pixels before GUI scaling. 0: the theme's default.
};

/// A line graph of a run of samples. Read-only. Kind: Series.
struct Graph {
    std::string name;
    GraphOptions options;
    std::optional<AnyBinding> binding;

    Graph(std::string name, GraphOptions options = {});
    Graph(std::string name, Series& samples, GraphOptions options = {});
    Graph(std::string name, SeriesBinding& samples, GraphOptions options = {});

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
    std::string name;
    ViewOptions options;

    View(std::string name, ViewOptions options = {});

    [[nodiscard]] std::unique_ptr<Widget> create() const;
};

/// What a type must offer to be used as a widget descriptor: a name, and a way to make the widget.
/// Applications that implement their own widget type write a descriptor for it the same way.
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
};

} // namespace atpl
