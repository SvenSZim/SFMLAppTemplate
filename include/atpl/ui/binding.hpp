#pragma once

#include "atpl/core/param.hpp"
#include "atpl/core/revision.hpp"
#include "atpl/core/series.hpp"
#include "atpl/core/text_log.hpp"

#include <concepts>
#include <cstddef>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>

namespace atpl {

// A binding links a widget to application data. A widget does not care about the data's C++ type,
// only about the kind of value it shows or edits. For each kind there is one small interface.
// `Param<T>` and `Series` are the ready-made, thread-safe sources; an application can also
// implement an interface over its own data, or build a binding from two functions.

/// A widget's value, whatever its kind: `bool` for Bool, `double` for Number, `std::size_t` for
/// Index, `std::string` for Text. (Series have no single value.)
using Value = std::variant<bool, double, std::size_t, std::string>;

/// The kinds of value widgets work with.
enum class ValueKind {
    Bool,   ///< on or off: switch
    Number, ///< a number: slider, progress bar
    Index,  ///< a position in a list of options: dropdown
    Text,   ///< text: text input, text display, value display
    Series, ///< a run of samples: graph
    Lines,  ///< a stream of text lines: log
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
///
/// Data comes as samples (the x-axis is time or a count) or as points (the x-axis comes from
/// the data). A source of points says so with `hasPoints` and gives them through `readPoints`;
/// its `read` gives the y values.
class SeriesBinding {
public:
    virtual ~SeriesBinding() = default;

    /// Copies the newest samples into `out`, oldest first, and returns how many were copied.
    virtual std::size_t read(std::span<float> out) const = 0;

    /// Changes whenever the samples may have changed.
    [[nodiscard]] virtual Revision revision() const = 0;

    /// Whether the data are points with an x value of their own.
    [[nodiscard]] virtual bool hasPoints() const { return false; }

    /// Copies the newest points into `out`, oldest first, and returns how many were copied.
    /// Nothing for a source of samples.
    virtual std::size_t readPoints(std::span<Point> /*out*/) const { return 0; }
};

/// The interface between a log and its lines.
class LinesBinding {
public:
    virtual ~LinesBinding() = default;

    /// Replaces the contents of `out` with the newest `newest` lines, oldest first, and returns
    /// how many.
    virtual std::size_t read(std::vector<LogLine>& out, std::size_t newest) const = 0;

    /// How many lines there are now.
    [[nodiscard]] virtual std::size_t size() const = 0;

    /// How many lines were ever added, including those dropped since: tells a reader how many
    /// came since it last looked.
    [[nodiscard]] virtual std::uint64_t pushed() const = 0;

    /// Changes whenever the lines may have changed.
    [[nodiscard]] virtual Revision revision() const = 0;
};

// Which C++ types map to which kind.

template <typename T>
concept BoolValue = std::same_as<T, bool>;

/// Every number type except `bool`. Widgets work with numbers as `double`.
///
/// Reading (application type to `double`) is exact for `float`, for every integer type of up to
/// 32 bits, and for 64-bit integers up to 2^53 (about 9 * 10^15) in size. Larger 64-bit values
/// lose their lowest digits; to show such a value exactly, bind it as text.
///
/// Writing (`double` to application type) goes through `numberTo<T>`: integers are rounded to the
/// nearest value, and anything outside the type's range is clamped to the range.
template <typename T>
concept NumberValue = std::is_arithmetic_v<T> && !std::same_as<T, bool>;

// MSVC warns of "overflow in constant arithmetic" (C4756) where the optimiser folds a call with a
// constant that is far out of range (1e300 to float): the branches below clamp such values before
// any conversion, so nothing overflows. Off for this function only.
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4756)
#endif

/// Converts a number coming from a widget to the application's number type without ever leaving
/// the type's range: integers are rounded to nearest and clamped, `float` is clamped to its
/// finite range. Not-a-number becomes 0 for integers and stays not-a-number for floating point.
template <NumberValue T>
[[nodiscard]] constexpr T numberTo(double value) {
    if (value != value) { // not a number
        return std::floating_point<T> ? static_cast<T>(value) : T{};
    }

    // As doubles, the limits of 64-bit integers round outwards to a power of two. Comparing with
    // >= and <= therefore catches every value that does not fit, including those limits themselves.
    const double lowest = static_cast<double>(std::numeric_limits<T>::lowest());
    const double highest = static_cast<double>(std::numeric_limits<T>::max());

    if constexpr (std::floating_point<T>) {
        if (value <= lowest) {
            return std::numeric_limits<T>::lowest();
        }
        if (value >= highest) {
            return std::numeric_limits<T>::max();
        }
        return static_cast<T>(value);
    } else {
        // Round half away from zero, as std::round does, without leaving constexpr. From 2^52 on
        // every double is a whole number already, and adding 0.5 there would round a second time.
        constexpr double wholeFrom = 4503599627370496.0; // 2^52
        const bool isWhole = value >= wholeFrom || value <= -wholeFrom;
        const double rounded = isWhole ? value : (value < 0.0 ? value - 0.5 : value + 0.5);
        if (rounded <= lowest) {
            return std::numeric_limits<T>::lowest();
        }
        if (rounded >= highest) {
            return std::numeric_limits<T>::max();
        }
        return static_cast<T>(rounded); // truncates towards zero, completing the rounding
    }
}

#if defined(_MSC_VER)
#pragma warning(pop)
#endif

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
    AnyBinding(LinesBinding& binding);

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

    /// Points, for graphs whose x-axis comes from the data.
    AnyBinding(PointSeries& series);

    /// A log of text lines, for log widgets.
    AnyBinding(TextLog& log);

    [[nodiscard]] ValueKind kind() const;
    [[nodiscard]] bool isReadOnly() const;

    // ----- What the UI does with it (an application rarely needs these) -----

    /// The current value. Nothing for a series.
    [[nodiscard]] std::optional<Value> get() const;

    /// Stores a value; it must be of the binding's kind. Does nothing if the binding is read-only.
    void set(const Value& value) const;

    /// Changes whenever the value may have changed.
    [[nodiscard]] Revision revision() const;

    /// The source of a series binding, or null.
    [[nodiscard]] const SeriesBinding* series() const;

    /// The source of a lines binding, or null.
    [[nodiscard]] const LinesBinding* lines() const;

private:
    using Target =
        std::variant<BoolBinding*, NumberBinding*, IndexBinding*, TextBinding*, SeriesBinding*, LinesBinding*>;

    AnyBinding(Target target, std::shared_ptr<void> owned);

    template <typename Interface>
    static AnyBinding owning(std::shared_ptr<Interface> binding) {
        Interface* target = binding.get();
        return AnyBinding(Target(target), std::move(binding));
    }

    Target m_target;
    std::shared_ptr<void> m_owned; ///< Keeps an adapter alive that this binding made; empty otherwise.
};

namespace detail {

/// The type a kind of value travels as: `bool`, `double`, `std::size_t` or `std::string`.
template <BindableValue T>
using KindType = std::conditional_t<
    BoolValue<T>,
    bool,
    std::conditional_t<NumberValue<T>, double, std::conditional_t<IndexValue<T>, std::size_t, std::string>>>;

/// A `Param<T>` seen as the binding of its kind: conversions in both directions, thread-safe
/// because the parameter is.
template <BindableValue T>
class ParamBinding final : public Binding<KindType<T>> {
public:
    explicit ParamBinding(Param<T>& param) :
        m_param(&param) {}

    [[nodiscard]] KindType<T> get() const override {
        if constexpr (NumberValue<T> || IndexValue<T>) {
            return static_cast<KindType<T>>(m_param->get());
        } else {
            return m_param->get();
        }
    }

    void set(const KindType<T>& value) override {
        if constexpr (NumberValue<T>) {
            m_param->set(numberTo<T>(value));
        } else if constexpr (IndexValue<T>) {
            m_param->set(static_cast<T>(value));
        } else {
            m_param->set(value);
        }
    }

    [[nodiscard]] Revision revision() const override { return m_param->revision(); }

private:
    Param<T>* m_param;
};

} // namespace detail

template <BindableValue T>
AnyBinding::AnyBinding(Param<T>& param) :
    AnyBinding(owning(std::make_shared<detail::ParamBinding<T>>(param))) {}

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
            [set = std::move(setter)](double value) { set(numberTo<T>(value)); }
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
