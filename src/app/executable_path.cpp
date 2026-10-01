#include "app/executable_path.hpp"

#include <system_error>

namespace atpl::app {

std::optional<std::filesystem::path> executableDirectoryFromSystem() {
    // Only standard library calls: where /proc/self/exe does not exist this simply reports nothing.
    std::error_code error;
    const std::filesystem::path executable = std::filesystem::read_symlink("/proc/self/exe", error);
    if (error || executable.empty()) {
        return std::nullopt;
    }
    return executable.parent_path();
}

std::optional<std::filesystem::path>
executableDirectoryFromArgv0(std::string_view argv0, const std::filesystem::path& workingDirectory) {
    const std::filesystem::path given(argv0);
    if (!given.has_parent_path()) {
        return std::nullopt;
    }
    const std::filesystem::path full = given.is_absolute() ? given : workingDirectory / given;
    return full.lexically_normal().parent_path();
}

} // namespace atpl::app
