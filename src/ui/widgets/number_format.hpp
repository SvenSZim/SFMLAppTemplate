#pragma once

#include "atpl/ui/error.hpp"

#include <format>
#include <string>

namespace atpl::widgets {

/// A number in an application's format, in `std::format` syntax: "{:.1f}", "{:g} m/s".
[[nodiscard]] inline std::string formatNumber(const std::string& format, double value) {
    return std::vformat(format, std::make_format_args(value));
}

/// Throws `SetupError` if `format` cannot show a number. `who` names the widget in the message.
inline void requireNumberFormat(const std::string& who, const std::string& format) {
    try {
        static_cast<void>(formatNumber(format, 0.0));
    } catch (const std::format_error& error) {
        throw SetupError(who + ": format \"" + format + "\" cannot show a number: " + error.what());
    }
}

} // namespace atpl::widgets
