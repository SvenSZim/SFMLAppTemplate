#include "atpl/ui/widget.hpp"

#include "ui/render/text_measurer.hpp"

namespace atpl {

// What a widget may know while it says how high it wants to be: the width it gets, the theme's
// sizes, and how much room text takes in the style of one of its parts.

MeasureContext::MeasureContext(
    float width, const Theme& theme, PanelColors colors, const Sizes& sizes, const render::TextMeasurer* measurer
) :
    m_width(width),
    m_theme(&theme),
    m_colors(colors),
    m_sizes(&sizes),
    m_measurer(measurer) {}

float MeasureContext::width() const {
    return m_width;
}

const Sizes& MeasureContext::sizes() const {
    return *m_sizes;
}

sf::Vector2f MeasureContext::textSize(std::string_view text, const Part& part) const {
    if (m_measurer == nullptr) {
        return {};
    }
    const PartStyle style = m_theme->resolve(part, State::Normal, m_colors, m_sizes->text);
    return m_measurer->measure(text, style.font, style.textSize);
}

float MeasureContext::wrappedTextHeight(std::string_view text, const Part& part, float width) const {
    if (m_measurer == nullptr) {
        return 0.f;
    }
    const PartStyle style = m_theme->resolve(part, State::Normal, m_colors, m_sizes->text);
    return m_measurer->wrappedHeight(text, style.font, style.textSize, width);
}

} // namespace atpl
