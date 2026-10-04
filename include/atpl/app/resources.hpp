#pragma once

#include <SFML/Graphics/Font.hpp>
#include <SFML/Graphics/Texture.hpp>

#include <filesystem>
#include <map>
#include <memory>
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

/// How a texture is sampled.
struct TextureOptions {
    bool smooth = true;    ///< Blends neighbouring pixels when scaled; off for pixel art.
    bool repeated = false; ///< Repeats the image outside its size instead of stretching its edge.
};

/// Access to the files in an application's resource directory.
///
///     const sf::Texture& ant = app.resources().texture("textures/ant.png");  // loaded once
///     const sf::Texture& again = app.resources().texture("textures/ant.png"); // the same one
///
/// `texture` and `font` load a file the first time it is asked for and hand out the same object
/// afterwards; the name is the path relative to the resource directory. What they return lives
/// as long as the `Resources` (copies share what was loaded), so a sprite or a text may keep a
/// reference to it. `loadFont` and `loadTexture` load a fresh copy every time.
///
/// Thread safety: the main thread only. Textures belong to the graphics card's context, and
/// the cache is not locked.
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

    /// Loads a texture given relative to the resource directory.
    /// Throws `ResourceError` if the file does not exist or is not a usable image.
    [[nodiscard]] sf::Texture loadTexture(const std::filesystem::path& relative, TextureOptions options = {}) const;

    /// The font of that name, loaded on first use and shared afterwards.
    /// Throws `ResourceError` like `loadFont` (and tries again on the next call).
    [[nodiscard]] const sf::Font& font(const std::filesystem::path& relative) const;

    /// The texture of that name, loaded on first use and shared afterwards. The options of the
    /// first call are the ones it keeps. Throws `ResourceError` like `loadTexture`.
    [[nodiscard]] const sf::Texture& texture(const std::filesystem::path& relative, TextureOptions options = {}) const;

private:
    /// What was loaded, by name. Behind a pointer so that copies share it and what it holds
    /// never moves.
    struct Cache {
        std::map<std::filesystem::path, std::unique_ptr<sf::Font>> fonts;
        std::map<std::filesystem::path, std::unique_ptr<sf::Texture>> textures;
    };

    std::filesystem::path m_root;
    std::shared_ptr<Cache> m_cache = std::make_shared<Cache>();
};

} // namespace atpl
