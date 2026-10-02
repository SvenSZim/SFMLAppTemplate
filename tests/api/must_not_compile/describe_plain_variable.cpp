// Must NOT compile: a descriptor does not take a plain variable either (decision D10).
// The same descriptor with a Param compiles; see ui_usage.cpp.
#include "atpl/ui/ui.hpp"

namespace must_not_compile {

atpl::WidgetSetup describePlainVariable() {
    static float rawSpeed = 0.f;
    return atpl::Slider("Speed", rawSpeed);
}

} // namespace must_not_compile
