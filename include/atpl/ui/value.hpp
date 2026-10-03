#pragma once

#include "atpl/ui/binding.hpp"

#include <cstddef>
#include <string>
#include <variant>

namespace atpl {

/// The kind of the value held.
[[nodiscard]] ValueKind kindOf(const Value& value);

/// Reads a value as an application type, with the same conversions as bindings: numbers through
/// `numberTo<T>`, enums from the index. Throws `SetupError` if the kind cannot be read as a `T`.
template <BindableValue T>
[[nodiscard]] T valueAs(const Value& value);

/// An application value as a widget's value: the reverse of `valueAs`.
template <BindableValue T>
[[nodiscard]] Value valueOf(const T& value) {
    if constexpr (NumberValue<T> || IndexValue<T>) {
        return Value(static_cast<detail::KindType<T>>(value));
    } else {
        return Value(value);
    }
}

namespace detail {

/// Throws `SetupError`: a value of one kind was read as a type of another.
[[noreturn]] void wrongKind(const Value& value, ValueKind wanted);

} // namespace detail

template <BindableValue T>
T valueAs(const Value& value) {
    if constexpr (BoolValue<T>) {
        if (const auto* flag = std::get_if<bool>(&value)) {
            return *flag;
        }
        detail::wrongKind(value, ValueKind::Bool);
    } else if constexpr (NumberValue<T>) {
        if (const auto* number = std::get_if<double>(&value)) {
            return numberTo<T>(*number);
        }
        detail::wrongKind(value, ValueKind::Number);
    } else if constexpr (IndexValue<T>) {
        if (const auto* index = std::get_if<std::size_t>(&value)) {
            return static_cast<T>(*index);
        }
        detail::wrongKind(value, ValueKind::Index);
    } else {
        if (const auto* text = std::get_if<std::string>(&value)) {
            return *text;
        }
        detail::wrongKind(value, ValueKind::Text);
    }
}

} // namespace atpl
