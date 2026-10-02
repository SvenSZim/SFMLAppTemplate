#include "atpl/ui/widget.hpp"

#include "ui/render/draw_list.hpp"
#include "ui/render/shapes.hpp"
#include "ui/render/text_measurer.hpp"

namespace atpl {

// The painter turns a widget's own coordinates into its panel's and hands everything on:
// shapes to the tessellation, text to the draw list.

Painter::Painter(render::DrawList& list, sf::Vector2f origin, sf::Vector2f size, const render::TextMeasurer* measurer) :
    m_list(&list),
    m_origin(origin),
    m_size(size),
    m_measurer(measurer) {}

sf::Vector2f Painter::size() const {
    return m_size;
}

void Painter::box(const FloatRect& rect, const PartStyle& style) {
    render::appendBox(m_list->shapes(), FloatRect(rect.position() + m_origin, rect.size()), style);
}

void Painter::line(sf::Vector2f from, sf::Vector2f to, const PartStyle& style) {
    render::appendLine(m_list->shapes(), from + m_origin, to + m_origin, style);
}

void Painter::polyline(std::span<const sf::Vector2f> points, const PartStyle& style) {
    render::appendPolyline(m_list->shapes(), points, style, m_origin);
}

void Painter::area(std::span<const sf::Vector2f> points, float baseline, const PartStyle& style) {
    render::appendArea(m_list->shapes(), points, baseline, style, m_origin);
}

void Painter::text(const FloatRect& rect, std::string_view text, const PartStyle& style, Align align) {
    m_list->addText(FloatRect(rect.position() + m_origin, rect.size()), text, style, align, false);
}

void Painter::wrappedText(const FloatRect& rect, std::string_view text, const PartStyle& style, Align align) {
    m_list->addText(FloatRect(rect.position() + m_origin, rect.size()), text, style, align, true);
}

sf::Vector2f Painter::textSize(std::string_view text, const PartStyle& style) const {
    return m_measurer != nullptr ? m_measurer->measure(text, style.font, style.textSize) : sf::Vector2f{};
}

} // namespace atpl
