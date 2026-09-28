#ifndef TEXT_RENDERER
#define TEXT_RENDERER

#include <string>
#include <SFML/Graphics.hpp>

namespace ui::core::renderer {

enum class TextSize : uint8_t {
    Title = 0,
    Label = 1,
    Value = 2,
    Small = 3
};

struct TextConfig {
    std::string fontPath = "resources/fonts/default.ttf";
};

class TextRenderer {
private:
    sf::Font m_font;
    bool m_fontLoaded = false;

    static unsigned int getCharSize(TextSize size) {
        switch (size) {
            case TextSize::Title: return 16;
            case TextSize::Label: return 13;
            case TextSize::Value: return 14;
            case TextSize::Small: return 11;
        }
        return 13;
    }

public:
    TextRenderer() = default;

    bool loadFont(const std::string& path) {
        m_fontLoaded = m_font.openFromFile(path);
        return m_fontLoaded;
    }

    [[nodiscard]]
    bool isFontLoaded() const { return m_fontLoaded; }

    [[nodiscard]]
    const sf::Font& getFont() const { return m_font; }

    [[nodiscard]]
    sf::Text createText(const std::string& content, TextSize size, sf::Color color = sf::Color::White) const {
        sf::Text text(m_font, content, getCharSize(size));
        text.setFillColor(color);
        return text;
    }

    [[nodiscard]]
    static unsigned int getFontSize(TextSize size) {
        return getCharSize(size);
    }
};

}

#endif
