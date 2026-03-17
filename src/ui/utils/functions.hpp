#ifndef FUNCTIONS
#define FUNCTIONS

namespace animutils { 

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