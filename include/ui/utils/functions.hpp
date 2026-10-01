#ifndef FUNCTIONS
#define FUNCTIONS

#include <cstdint>
#include <cmath>

namespace ui::utils::anim { 

float linear(float t);
float easeInOutExponential(float t);
float easeOutBack(float t);
float easeInBack(float t);
float easeOutElastic(float t);

enum class TransitionFunction {
    None,
    Linear,
    EaseInOutExponential,
    EaseOutBack,
    EaseInBack,
    EaseOutElastic,
};

float getRatio(float t, TransitionFunction transition);

}

#endif