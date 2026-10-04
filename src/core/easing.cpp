#include "atpl/core/easing.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>

namespace atpl {

namespace {

float simplePow(float x, uint32_t p) {
    float res = 1.0f;
    for (uint32_t i(p); i--;) {
        res *= x;
    }
    return res;
}

} // namespace

float linear(float t) {
    return t;
}

float easeInOutExponential(float t) {
    // The curve only approaches 0 and 1; pin the end points so an animation ends exactly on its target.
    if (t <= 0.0f) {
        return 0.0f;
    }
    if (t >= 1.0f) {
        return 1.0f;
    }
    if (t < 0.5f) {
        return std::pow(2.0f, 20.0f * t - 10.0f) * 0.5f;
    }
    return (2.0f - std::pow(2.0f, -20.0f * t + 10.0f)) * 0.5f;
}

float easeInOutQuint(float t) {
    if (t < 0.5f) {
        return 16.0f * simplePow(t, 5);
    }
    return 1.0f - simplePow(-2.0f * t + 2.0f, 5) * 0.5f;
}

float easeOutBack(float t) {
    constexpr float c1 = 1.70158f;
    constexpr float c3 = c1 + 1.0f;
    return 1.0f + c3 * simplePow(t - 1.0f, 3) + c1 * simplePow(t - 1.0f, 2);
}

float easeInBack(float t) {
    constexpr float c1 = 1.70158f;
    constexpr float c3 = c1 + 1.0f;
    return c3 * t * t * t - c1 * t * t;
}

float easeOutElastic(float t) {
    constexpr float c4 = 2.0f * std::numbers::pi_v<float> / 3.0f;
    if (t <= 0.0f) {
        return 0.0f;
    }
    if (t >= 1.0f) {
        return 1.0f;
    }
    return std::pow(2.0f, -10.0f * t) * std::sin((t * 10.0f - 0.75f) * c4) + 1.0f;
}

float getRatio(float t, TransitionFunction transition) {
    switch (transition) {
        case TransitionFunction::None:
            return 1.0f;
        case TransitionFunction::Linear:
            return linear(t);
        case TransitionFunction::EaseInOutExponential:
            return easeInOutExponential(t);
        case TransitionFunction::EaseInOutQuint:
            return easeInOutQuint(t);
        case TransitionFunction::EaseOutBack:
            return easeOutBack(t);
        case TransitionFunction::EaseInBack:
            return easeInBack(t);
        case TransitionFunction::EaseOutElastic:
            return easeOutElastic(t);
    }
    return t;
}

float smoothStep(float t) {
    const float x = std::clamp(t, 0.f, 1.f);
    return x * x * (3.f - 2.f * x);
}

} // namespace atpl
