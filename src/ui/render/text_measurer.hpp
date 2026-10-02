#pragma once

#include <SFML/Graphics/Font.hpp>
#include <SFML/System/Vector2.hpp>

#include <string_view>

namespace atpl::render {

/// Tells how much room text takes. Painting and measuring ask through this interface, so that
/// everything above it can be tested without a font or a window; tests use a stand-in.
class TextMeasurer {
public:
    virtual ~TextMeasurer() = default;

    /// The size of one line of text in this font at this size.
    [[nodiscard]] virtual sf::Vector2f measure(std::string_view text, const sf::Font* font, float size) const = 0;

    /// The height of text wrapped at word boundaries to `width`.
    [[nodiscard]] virtual float
    wrappedHeight(std::string_view text, const sf::Font* font, float size, float width) const = 0;
};

} // namespace atpl::render
