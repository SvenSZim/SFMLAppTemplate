#pragma once

#include "ui/render/text_layout.hpp"
#include "ui/render/text_measurer.hpp"

#include <SFML/Graphics/Font.hpp>

namespace atpl::render {

/// How far the pen moves from character to character in this font at this size, kerning included.
/// The font must outlive the function. Needs a display, like everything that loads glyphs.
[[nodiscard]] Advance advanceOf(const sf::Font& font, float size);

/// The character size SFML is asked for when text has this height.
[[nodiscard]] unsigned int characterSize(float size);

/// Measures text with the real fonts. A null font measures as nothing.
class FontMeasurer final : public TextMeasurer {
public:
    [[nodiscard]] sf::Vector2f measure(std::string_view text, const sf::Font* font, float size) const override;
    [[nodiscard]] float
    wrappedHeight(std::string_view text, const sf::Font* font, float size, float width) const override;
};

} // namespace atpl::render
