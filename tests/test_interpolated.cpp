#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <chrono>
#include <thread>

#include "ui/utils/interpolated.hpp"

using ui::utils::anim::Interpolated;
using ui::utils::anim::TransitionFunction;

TEST_CASE("Interpolated values progress to target", "[Interpolated]") {
    // OUTDATED
    Interpolated<float> interpolation(0.0f, TransitionFunction::Linear, 0.01f);
    interpolation.setValue(10.f);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    REQUIRE(interpolation.getValue() == Catch::Approx(10.f));

    Interpolated<int> interpolationInt(0, TransitionFunction::Linear, 0.01f);
    interpolationInt.setValue(5);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    REQUIRE(interpolationInt.getValue() == 5);
}
