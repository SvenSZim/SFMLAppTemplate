#pragma once

#include <filesystem>
#include <optional>
#include <string_view>

namespace atpl::app {

/// The executable's directory as the operating system reports it, where it offers a way to ask
/// through the file system (Linux: /proc/self/exe). Empty elsewhere.
[[nodiscard]] std::optional<std::filesystem::path> executableDirectoryFromSystem();

/// The executable's directory worked out from `argv0`, resolved against `workingDirectory`.
/// Empty if `argv0` carries no directory (the program was found through PATH) or is empty.
[[nodiscard]] std::optional<std::filesystem::path>
executableDirectoryFromArgv0(std::string_view argv0, const std::filesystem::path& workingDirectory);

} // namespace atpl::app
