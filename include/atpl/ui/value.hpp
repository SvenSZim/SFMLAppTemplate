#pragma once

#include "atpl/ui/binding.hpp"

#include <cstddef>
#include <string>
#include <variant>

namespace atpl {

/// A widget's value, whatever its kind: `bool` for Bool, `double` for Number, `std::size_t` for
/// Index, `std::string` for Text. (Series have no single value.)
using Value = std::variant<bool, double, std::size_t, std::string>;

/// The kind of the value held.
[[nodiscard]] ValueKind kindOf(const Value& value);

/// Reads a value as an application type, with the same conversions as bindings: numbers through
/// `numberTo<T>`, enums from the index. Throws `SetupError` if the kind cannot be read as a `T`.
template <BindableValue T>
[[nodiscard]] T valueAs(const Value& value);

} // namespace atpl
