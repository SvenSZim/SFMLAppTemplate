#include "ui/widgets/animation.hpp"

#include <algorithm>
#include <cmath>

namespace atpl {

UpdateContext::UpdateContext(State state, const Sizes& sizes, const Motion& motion, bool& dirty) :
    m_state(state),
    m_sizes(&sizes),
    m_motion(&motion),
    m_dirty(&dirty) {}

State UpdateContext::state() const {
    return m_state;
}

const Sizes& UpdateContext::sizes() const {
    return *m_sizes;
}

const Motion& UpdateContext::motion() const {
    return *m_motion;
}

void UpdateContext::markDirty() {
    *m_dirty = true;
}

} // namespace atpl

namespace atpl::widgets {

namespace {

/// Closer than this to its target, a blend takes the target: less than one step of a colour
/// channel (1 / 255), so the last step cannot be seen.
constexpr float settled = 0.004f;

} // namespace

bool approach(float& value, float target, float seconds, float duration) {
    if (value == target) {
        return false;
    }
    if (duration <= 0.f) {
        value = target;
        return true;
    }
    // Exponential: the same share of what is left in every equal stretch of time, so it eases
    // out and turns back without a jump. After `duration`, 95 % (e^-3) of the way is done.
    const float share = 1.f - std::exp(-3.f * std::max(seconds, 0.f) / duration);
    value += (target - value) * share;
    if (std::abs(target - value) < settled) {
        value = target;
    }
    return true;
}

bool animateWidgets(model::Store& store, float seconds, const Theme& theme, const Sizes& sizes) {
    bool moved = false;
    for (model::WidgetSlot& slot : store.widgets()) {
        const StateBlend target = StateBlend::of(model::stateOf(slot));
        if (!slot.visible) {
            // Nobody sees it: it is in its state at once, and its own animations wait.
            slot.blend = target;
            continue;
        }
        bool dirty = false;
        dirty = approach(slot.blend.hovered, target.hovered, seconds, theme.motion.hover) || dirty;
        dirty = approach(slot.blend.pressed, target.pressed, seconds, theme.motion.press) || dirty;
        dirty = approach(slot.blend.focused, target.focused, seconds, theme.motion.focus) || dirty;

        UpdateContext context(model::stateOf(slot), sizes, theme.motion, dirty);
        slot.widget->update(seconds, context);

        if (dirty) {
            store.panel(slot.panel).dirty = true;
            moved = true;
        }
    }
    return moved;
}

} // namespace atpl::widgets
