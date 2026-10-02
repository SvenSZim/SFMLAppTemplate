#include "ui/render/font_measurer.hpp"

#include <algorithm>
#include <cmath>

namespace atpl::render {

unsigned int characterSize(float size) {
    return static_cast<unsigned int>(std::max(std::lround(size), 1L));
}

Advance advanceOf(const sf::Font& font, float size) {
    const unsigned int pixels = characterSize(size);
    return [&font, pixels](char32_t previous, char32_t current) {
        const float kerning = previous != 0 ? font.getKerning(previous, current, pixels) : 0.f;
        return kerning + font.getGlyph(current, pixels, false).advance;
    };
}

sf::Vector2f FontMeasurer::measure(std::string_view text, const sf::Font* font, float size) const {
    if (font == nullptr) {
        return {};
    }
    return { lineWidth(decodeUtf8(text), advanceOf(*font, size)), font->getLineSpacing(characterSize(size)) };
}

float FontMeasurer::wrappedHeight(std::string_view text, const sf::Font* font, float size, float width) const {
    if (font == nullptr) {
        return 0.f;
    }
    const std::size_t lines = wrapLines(decodeUtf8(text), width, advanceOf(*font, size)).size();
    return static_cast<float>(lines) * font->getLineSpacing(characterSize(size));
}

} // namespace atpl::render
