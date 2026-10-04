#pragma once

#include "atpl/ui/layout.hpp"
#include "atpl/ui/theme.hpp"
#include "atpl/ui/widget.hpp"

#include "ui/model/store.hpp"

namespace atpl::widgets {

// Widgets over time: their hover, press and focus fading in and out, and their own animations
// (`Widget::update`). The UI calls this once per update; while it reports movement, frames keep
// coming, and once everything has settled, none are asked for.

/// Moves `value` towards `target` over about `seconds` (`duration` is the time to get 95 % of
/// the way): fast at first, settling softly, and turning back from where it is if the target
/// changes halfway. Within a step of the end it takes the target. A duration of 0 or less takes
/// it at once. Returns whether the value changed.
bool approach(float& value, float target, float seconds, float duration);

/// Lets `seconds` pass for every widget: their state blends move towards their state, and
/// visible widgets get `Widget::update`. The panel of every widget that looks different is
/// marked for repainting. Returns whether anything moved.
bool animateWidgets(model::Store& store, float seconds, const Theme& theme, const Sizes& sizes);

} // namespace atpl::widgets
