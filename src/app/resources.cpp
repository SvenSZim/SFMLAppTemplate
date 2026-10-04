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

sf::Texture Resources::loadTexture(const std::filesystem::path& relative, TextureOptions options) const {
    const std::filesystem::path full = path(relative);

    sf::Texture texture;
    if (!texture.loadFromFile(full)) {
        throw ResourceError("Not a usable image: " + full.string());
    }
    texture.setSmooth(options.smooth);
    texture.setRepeated(options.repeated);
    return texture;
}

// The name is used as given ("textures/a.png"), made into the platform's form, so that the same
// name with other separators finds the same entry.

const sf::Font& Resources::font(const std::filesystem::path& relative) const {
    std::unique_ptr<sf::Font>& entry = m_cache->fonts[std::filesystem::path(relative).make_preferred()];
    if (!entry) {
        entry = std::make_unique<sf::Font>(loadFont(relative)); // a failure leaves it empty
    }
    return *entry;
}

const sf::Texture& Resources::texture(const std::filesystem::path& relative, TextureOptions options) const {
    std::unique_ptr<sf::Texture>& entry = m_cache->textures[std::filesystem::path(relative).make_preferred()];
    if (!entry) {
        entry = std::make_unique<sf::Texture>(loadTexture(relative, options));
    }
    return *entry;
}

} // namespace atpl
