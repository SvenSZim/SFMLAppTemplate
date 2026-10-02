#include "atpl/ui/widget.hpp"

namespace atpl {

// A Style is the theme as one widget sees it: with its panel's colours and its own state filled in.

Style::Style(const Theme& theme, PanelColors colors, State state, const Sizes& sizes) :
    m_theme(&theme),
    m_colors(colors),
    m_state(state),
    m_sizes(&sizes) {}

PartStyle Style::part(const Part& part) const {
    return m_theme->resolve(part, m_state, m_colors, m_sizes->text);
}

PartStyle Style::part(const Part& part, State additional) const {
    return m_theme->resolve(part, m_state | additional, m_colors, m_sizes->text);
}

State Style::state() const {
    return m_state;
}

const Sizes& Style::sizes() const {
    return *m_sizes;
}

} // namespace atpl
