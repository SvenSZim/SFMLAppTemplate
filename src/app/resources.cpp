#include "atpl/app/resources.hpp"

#include "app/executable_path.hpp"

#include <string>
#include <system_error>
#include <utility>

namespace atpl {

std::filesystem::path executableDirectory(std::string_view argv0) {
    if (const auto fromSystem = app::executableDirectoryFromSystem()) {
        return *fromSystem;
    }

    std::error_code error;
    const std::filesystem::path workingDirectory = std::filesystem::current_path(error);
    if (const auto fromArgv0 = app::executableDirectoryFromArgv0(argv0, workingDirectory)) {
        return *fromArgv0;
    }
    return workingDirectory;
}

Resources Resources::nextToExecutable(std::string_view argv0) {
    return Resources(executableDirectory(argv0) / "resources");
}

Resources::Resources(std::filesystem::path root) :
    m_root(std::move(root)) {}

std::filesystem::path Resources::path(const std::filesystem::path& relative) const {
    std::error_code error;
    if (!std::filesystem::is_directory(m_root, error)) {
        throw ResourceError(
            "Resource directory not found: " + m_root.string() +
            "\nThe build copies it next to the executable. Build again, or check that the executable was not moved"
            " without its resources directory."
        );
    }

    // The platform's own separators throughout, so the path reads the same in messages as elsewhere.
    const std::filesystem::path full = (m_root / relative).make_preferred();
    if (!std::filesystem::is_regular_file(full, error)) {
        throw ResourceError("Resource not found: " + full.string());
    }
    return full;
}

sf::Font Resources::loadFont(const std::filesystem::path& relative) const {
    const std::filesystem::path full = path(relative);

    sf::Font font;
    if (!font.openFromFile(full)) {
        throw ResourceError("Not a usable font: " + full.string());
    }
    return font;
}

} // namespace atpl
