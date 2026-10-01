#pragma once

#include <SFML/Graphics/Font.hpp>

#include <filesystem>
#include <stdexcept>
#include <string_view>

namespace atpl {

/// Thrown when a resource the application needs is missing or cannot be loaded.
/// The message names the full path that was tried.
class ResourceError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

/// The directory that contains the running executable.
///
/// Resources are found relative to this directory, never relative to the working directory, so an
/// application behaves the same wherever it is started from. On Linux the system is asked directly.
/// Elsewhere `argv0` (the first argument of `main`) is used, which is reliable as long as the
/// program was started through a path. If neither works, the working directory is returned.
[[nodiscard]] std::filesystem::path executableDirectory(std::string_view argv0 = {});

/// Access to the files in an application's resource directory.
class Resources {
public:
    /// Uses `<directory of the executable>/resources`, where the build copies the resources.
    [[nodiscard]] static Resources nextToExecutable(std::string_view argv0 = {});

    /// Uses the given directory.
    explicit Resources(std::filesystem::path root);

    [[nodiscard]] const std::filesystem::path& root() const { return m_root; }

    /// Full path of a file given relative to the resource directory.
    /// Throws `ResourceError` if the file does not exist.
    [[nodiscard]] std::filesystem::path path(const std::filesystem::path& relative) const;

    /// Loads a font given relative to the resource directory.
    /// Throws `ResourceError` if the file does not exist or is not a usable font.
    [[nodiscard]] sf::Font loadFont(const std::filesystem::path& relative) const;

private:
    std::filesystem::path m_root;
};

} // namespace atpl
