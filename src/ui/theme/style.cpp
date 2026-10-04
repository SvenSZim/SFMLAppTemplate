#include "atpl/ui/widget.hpp"

#include <array>
#include <cstdint>
#include <utility>

namespace atpl {

// A Style is the theme as one widget sees it: with its panel's colours and its own state filled in.

StateBlend StateBlend::of(State state) {
    return { .hovered = has(state, State::Hovered) ? 1.f : 0.f,
             .pressed = has(state, State::Pressed) ? 1.f : 0.f,
             .focused = has(state, State::Focused) ? 1.f : 0.f };
}

Style::Style(const Theme& theme, PanelColors colors, State state, const Sizes& sizes) :
    Style(theme, colors, state, sizes, StateBlend::of(state)) {}

Style::Style(const Theme& theme, PanelColors colors, State state, const Sizes& sizes, StateBlend blend) :
    m_theme(&theme),
    m_colors(colors),
    m_state(state),
    m_sizes(&sizes),
    m_blend(blend) {}

PartStyle Style::part(const Part& part) const {
    return this->part(part, State::Normal);
}

PartStyle Style::part(const Part& part, State additional) const {
    // The states that ease, in the order their looks are laid over each other; the blend says
    // how far each is in. A state asked for as `additional` is fully in.
    const std::array<std::pair<State, float>, 3> easing{ { { State::Hovered, m_blend.hovered },
                                                           { State::Focused, m_blend.focused },
                                                           { State::Pressed, m_blend.pressed } } };
    State base = m_state | additional;
    for (const auto& [flag, amount] : easing) {
        base = static_cast<State>(static_cast<std::uint8_t>(base) & ~static_cast<std::uint8_t>(flag));
    }

    // At rest every blend is 0 or 1: one state, one look.
    bool atRest = true;
    State settled = base;
    for (const auto& [flag, amount] : easing) {
        const float in = has(additional, flag) ? 1.f : amount;
        if (in >= 1.f) {
            settled = settled | flag;
        } else if (in > 0.f) {
            atRest = false;
        }
    }
    const float scale = m_sizes->text;
    if (atRest) {
        return m_theme->resolve(part, settled, m_colors, scale);
    }

    // Under way: each easing state's look laid over what is there by its blend.
    State reached = base;
    PartStyle style = m_theme->resolve(part, reached, m_colors, scale);
    for (const auto& [flag, amount] : easing) {
        const float in = has(additional, flag) ? 1.f : amount;
        if (in <= 0.f) {
            continue;
        }
        reached = reached | flag;
        style = mix(style, m_theme->resolve(part, reached, m_colors, scale), in);
    }
    return style;
}

PartStyle Style::part(const Part& part, State additional, float amount) const {
    if (amount <= 0.f) {
        return this->part(part);
    }
    if (amount >= 1.f) {
        return this->part(part, additional);
    }
    return mix(this->part(part), this->part(part, additional), amount);
}

State Style::state() const {
    return m_state;
}

const Sizes& Style::sizes() const {
    return *m_sizes;
}

} // namespace atpl
