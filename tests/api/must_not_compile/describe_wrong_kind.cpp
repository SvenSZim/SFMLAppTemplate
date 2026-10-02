// Must NOT compile: a slider works with numbers and cannot be described with an on/off value.
// The same descriptor with a Param<float> compiles; see ui_usage.cpp.
#include "atpl/ui/ui.hpp"

namespace must_not_compile {

atpl::WidgetSetup describeWrongKind() {
    static atpl::Param<bool> flag;
    return atpl::Slider("Speed", flag);
}

} // namespace must_not_compile
