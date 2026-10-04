#pragma once

namespace atpl {

// Easing curves. Each maps progress t in [0, 1] to an eased ratio with f(0) = 0 and f(1) = 1.
// The "back" and "elastic" curves leave the range [0, 1] in between.

[[nodiscard]] float linear(float t);
[[nodiscard]] float easeInOutExponential(float t);
[[nodiscard]] float easeInOutQuint(float t);
[[nodiscard]] float easeOutBack(float t);
[[nodiscard]] float easeInBack(float t);
[[nodiscard]] float easeOutElastic(float t);
/// Slow at both ends, fast in the middle (3t^2 - 2t^3). The same curve forwards and backwards,
/// so a movement that turns back halfway does not jump.
[[nodiscard]] float smoothStep(float t);

/// How an animated value moves from its start to its target.
enum class TransitionFunction {
    None, ///< No animation: the target is reached at once.
    Linear,
    EaseInOutExponential,
    EaseInOutQuint,
    EaseOutBack,
    EaseInBack,
    EaseOutElastic,
};

/// The eased ratio for progress t in [0, 1] under the given transition.
[[nodiscard]] float getRatio(float t, TransitionFunction transition);

} // namespace atpl
