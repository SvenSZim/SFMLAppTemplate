#pragma once

#include "atpl/ui/rect.hpp"
#include "atpl/ui/theme.hpp"
#include "atpl/ui/widget.hpp"

#include "ui/render/shapes.hpp"

#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/Font.hpp>

#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace atpl::render {

/// One piece of text to draw: where, what, and how it looks.
struct TextRun {
    FloatRect rect; ///< The room the text has. It is centred vertically in it.
    std::string text;
    sf::Color color;
    float size = 0.f; ///< Height of the text in pixels.
    const sf::Font* font = nullptr;
    Align align = Align::Left;
    bool wrapped = false; ///< Wrapped to the rectangle's width, starting at its top, instead of one line.
};

/// Everything one panel draws: its shapes as triangles, and its text.
///
/// Widgets fill it through a `Painter` when their panel is rebuilt. Shapes are drawn first and
/// text on top, so within a panel text is never covered by a shape.
///
/// The list is reused from rebuild to rebuild: `clear` keeps the memory, including that of the
/// text strings, so refilling a list of similar content allocates nothing.
class DrawList {
public:
    /// The triangles. Append with the functions of shapes.hpp.
    [[nodiscard]] VertexList& shapes() { return m_shapes; }
    [[nodiscard]] const VertexList& shapes() const { return m_shapes; }

    /// Adds text in the style's colour, size and font. Nothing is added if the style is not
    /// shown or fully transparent, or if there is no text or no room.
    void addText(const FloatRect& rect, std::string_view text, const PartStyle& style, Align align, bool wrapped);

    [[nodiscard]] std::span<const TextRun> texts() const { return { m_texts.data(), m_textCount }; }

    /// Empties the list but keeps its memory.
    void clear();

    [[nodiscard]] bool empty() const { return m_shapes.empty() && m_textCount == 0; }

private:
    VertexList m_shapes;
    std::vector<TextRun> m_texts; // entries beyond m_textCount are kept for their memory
    std::size_t m_textCount = 0;
};

} // namespace atpl::render
