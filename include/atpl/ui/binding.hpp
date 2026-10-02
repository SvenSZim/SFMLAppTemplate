#pragma once

#include "atpl/core/param.hpp"
#include "atpl/core/revision.hpp"
#include "atpl/core/series.hpp"

#include <cmath>
#include <concepts>
#include <cstddef>
#include <functional>
#include <span>
#include <string>
#include <type_traits>
#include <utility>

namespace atpl {

// A binding links a widget to application data. A widget does not care about the data's C++ type,
// only about the kind of value it shows or edits. For each kind there is one small interface.
// `Param<T>` and `Series` are the ready-made, thread-safe sources; an application can also
// implement an interface over its own data, or build a binding from two functions.

/// The kinds of value widgets work with.
enum class ValueKind {
    Bool,   ///< on or off: switch
    Number, ///< a number: slider, progress bar
    Index,  ///< a position in a list of options: dropdown
    Text,   ///< text: text input, text display
    Series, ///< a run of samples: graph
};

/// The interface between a widget and one value of kind `T`.
///
/// `T` is `bool`, `double`, `std::size_t` or `std::string`; use the aliases below.
///
/// Thread safety is the implementer's responsibility. The UI calls all three functions on the
/// main thread; if the data is also touched by another thread, the implementation must make that
/// safe. The bindings the template provides for `Param<T>` are safe.
template <typename T>
class Binding {
public:
    virtual ~Binding() = default;

    /// The current value.
    [[nodiscard]] virtual T get() const = 0;

    /// Stores a value the user entered. Not called if `isReadOnly()` is true.
    virtual void set(const T& value) = 0;

    /// Changes whenever the value may have changed. The UI compares it with the last one it saw
    /// and redraws the widget only if it differs.
    [[nodiscard]] virtual Revision revision() const = 0;

    /// True if the value can only be shown. A widget that edits its value refuses such a binding.
    [[nodiscard]] virtual bool isReadOnly() const { return false; }
};

using BoolBinding = Binding<bool>;
using NumberBinding = Binding<double>;
using IndexBinding = Binding<std::size_t>;
using TextBinding = Binding<std::string>;

/// The interface between a graph and its data.
class SeriesBinding {
public:
    virtual ~SeriesBinding() = default;

    /// Copies the newest samples into `out`, oldest first, and returns how many were copied.
    virtual std::size_t read(std::span<float> out) const = 0;

    /// Changes whenever the samples may have changed.
    [[nodiscard]] virtual Revision revision() const = 0;
};

// Which C++ types map to which kind.

template <typename T>
concept BoolValue = std::same_as<T, bool>;

/// Every number type except `bool`. Converted to and from `double`.
template <typename T>
concept NumberValue = std::is_arithmetic_v<T> && !std::same_as<T, bool>;

/// Enums. The enumerator's value is the index: the first option is 0.
template <typename T>
concept IndexValue = std::is_enum_v<T>;

template <typename T>
concept TextValue = std::same_as<T, std::string>;

/// Every type a widget can be bound to through `Param<T>` or through functions.
template <typename T>
concept BindableValue = BoolValue<T> || NumberValue<T> || IndexValue<T> || TextValue<T>;

/// A function without arguments that returns a bindable value.
template <typename F>
concept ValueGetter = std::invocable<F> && BindableValue<std::remove_cvref_t<std::invoke_result_t<F>>>;

/// Something a widget can be bound to, whatever its kind.
///
/// Applications rarely name this type: it is what `WidgetHandle::bind` and the widget descriptors
/// accept, and the things listed below convert to it. A plain variable does not, on purpose:
///
///     Param<float> speed;       ui.widget("Speed").bind(speed);    // fine
///     float        rawSpeed;    ui.widget("Speed").bind(rawSpeed); // does not compile
///
/// Whatever is bound by reference (`Param`, `Series`, a `Binding`) must outlive the UI.
class AnyBinding {
public:
    /// A parameter. The kind follows from `T`: `bool` is Bool, numbers are Number,
    /// enums are Index, `std::string` is Text.
    template <BindableValue T>
    AnyBinding(Param<T>& param);

    /// A series of samples, for graphs.
    AnyBinding(Series& series);

    /// The application's own implementation of a binding interface.
    AnyBinding(BoolBinding& binding);
    AnyBinding(NumberBinding& binding);
    AnyBinding(IndexBinding& binding);
    AnyBinding(TextBinding& binding);
    AnyBinding(SeriesBinding& binding);

    /// A binding made of two functions, for data of any type and shape:
    ///
    ///     AnyBinding::fromFunctions(
    ///         [&] { return config.speedKmh / 3.6f; },
    ///         [&](float metresPerSecond) { config.speedKmh = metresPerSecond * 3.6f; });
    ///
    /// The kind follows from what `getter` returns. Both functions are called on the main thread;
    /// making that safe is up to the application. The getter is polled to notice changes.
    template <ValueGetter Getter, typename Setter>
        requires std::invocable<Setter, std::remove_cvref_t<std::invoke_result_t<Getter>>>
    [[nodiscard]] static AnyBinding fromFunctions(Getter getter, Setter setter);

    /// A read-only binding made of one function, for widgets that only show a value.
    template <ValueGetter Getter>
    [[nodiscard]] static AnyBinding fromFunction(Getter getter);

    // The same per kind, with the kind named instead of worked out. An empty `set` makes the
    // binding read-only.
    [[nodiscard]] static AnyBinding ofBool(std::function<bool()> get, std::function<void(bool)> set = {});
    [[nodiscard]] static AnyBinding ofNumber(std::function<double()> get, std::function<void(double)> set = {});
    [[nodiscard]] static AnyBinding
    ofIndex(std::function<std::size_t()> get, std::function<void(std::size_t)> set = {});
    [[nodiscard]] static AnyBinding
    ofText(std::function<std::string()> get, std::function<void(const std::string&)> set = {});

    [[nodiscard]] ValueKind kind() const;
    [[nodiscard]] bool isReadOnly() const;
};

// The two templates only work out the kind and convert between the application's type and the
// kind's type. Everything else happens in the functions above.

template <ValueGetter Getter, typename Setter>
    requires std::invocable<Setter, std::remove_cvref_t<std::invoke_result_t<Getter>>>
AnyBinding AnyBinding::fromFunctions(Getter getter, Setter setter) {
    using T = std::remove_cvref_t<std::invoke_result_t<Getter>>;

    if constexpr (BoolValue<T>) {
        return ofBool(std::move(getter), std::move(setter));
    } else if constexpr (NumberValue<T>) {
        return ofNumber(
            [get = std::move(getter)] { return static_cast<double>(get()); },
            [set = std::move(setter)](double value) {
                if constexpr (std::integral<T>) {
                    set(static_cast<T>(std::llround(value)));
                } else {
                    set(static_cast<T>(value));
                }
            }
        );
    } else if constexpr (IndexValue<T>) {
        return ofIndex(
            [get = std::move(getter)] { return static_cast<std::size_t>(get()); },
            [set = std::move(setter)](std::size_t index) { set(static_cast<T>(index)); }
        );
    } else {
        return ofText(std::move(getter), std::move(setter));
    }
}

template <ValueGetter Getter>
AnyBinding AnyBinding::fromFunction(Getter getter) {
    using T = std::remove_cvref_t<std::invoke_result_t<Getter>>;

    if constexpr (BoolValue<T>) {
        return ofBool(std::move(getter));
    } else if constexpr (NumberValue<T>) {
        return ofNumber([get = std::move(getter)] { return static_cast<double>(get()); });
    } else if constexpr (IndexValue<T>) {
        return ofIndex([get = std::move(getter)] { return static_cast<std::size_t>(get()); });
    } else {
        return ofText(std::move(getter));
    }
}

} // namespace atpl
