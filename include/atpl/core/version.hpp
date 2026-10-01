#pragma once

#include <string_view>

namespace atpl {

/// Version of the template libraries, taken from the CMake project version.
struct Version {
    int major;
    int minor;
    int patch;
};

/// The version the libraries were built as.
[[nodiscard]] Version version();

/// The same version as text, for example "0.1.0".
[[nodiscard]] std::string_view versionString();

} // namespace atpl
