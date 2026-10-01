#include "atpl/core/easing.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <algorithm>

using atpl::getRatio;
using atpl::TransitionFunction;
using Catch::Approx;

namespace {

constexpr int steps = 200;

float at(int step) {
    return static_cast<float>(step) / static_cast<float>(steps);
}

} // namespace

TEST_CASE("every easing curve starts at 0 and ends at 1", "[core][easing]") {
    const auto transition = GENERATE(
        TransitionFunction::Linear,
        TransitionFunction::EaseInOutExponential,
        TransitionFunction::EaseInOutQuint,
        TransitionFunction::EaseOutBack,
        TransitionFunction::EaseInBack,
        TransitionFunction::EaseOutElastic
    );

    REQUIRE(getRatio(0.0f, transition) == Approx(0.0f).margin(1e-6));
    REQUIRE(getRatio(1.0f, transition) == Approx(1.0f).margin(1e-6));
}

TEST_CASE("curves without overshoot never move backwards and stay in range", "[core][easing]") {
    const auto transition = GENERATE(
        TransitionFunction::Linear, TransitionFunction::EaseInOutExponential, TransitionFunction::EaseInOutQuint
    );

    float previous = getRatio(0.0f, transition);
    for (int i = 1; i <= steps; ++i) {
        const float current = getRatio(at(i), transition);
        REQUIRE(current >= previous);
        REQUIRE(current >= 0.0f);
        REQUIRE(current <= 1.0f);
        previous = current;
    }
}

TEST_CASE("in-out curves pass through the middle", "[core][easing]") {
    REQUIRE(atpl::linear(0.5f) == Approx(0.5f));
    REQUIRE(atpl::easeInOutExponential(0.5f) == Approx(0.5f));
    REQUIRE(atpl::easeInOutQuint(0.5f) == Approx(0.5f));
}

TEST_CASE("back and elastic curves overshoot", "[core][easing]") {
    float outBackMax = 0.0f;
    float inBackMin = 0.0f;
    float elasticMax = 0.0f;
    for (int i = 0; i <= steps; ++i) {
        outBackMax = std::max(outBackMax, atpl::easeOutBack(at(i)));
        inBackMin = std::min(inBackMin, atpl::easeInBack(at(i)));
        elasticMax = std::max(elasticMax, atpl::easeOutElastic(at(i)));
    }

    REQUIRE(outBackMax > 1.0f);
    REQUIRE(inBackMin < 0.0f);
    REQUIRE(elasticMax > 1.0f);
}

TEST_CASE("the None transition reaches the target at once", "[core][easing]") {
    REQUIRE(getRatio(0.0f, TransitionFunction::None) == 1.0f);
    REQUIRE(getRatio(0.3f, TransitionFunction::None) == 1.0f);
}
