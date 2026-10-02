// Must NOT compile: a plain variable cannot be bound to a widget (decision D10).
// The same call with a Param compiles; see ui_usage.cpp.
#include "atpl/ui/ui.hpp"

namespace must_not_compile {

void bindPlainVariable(atpl::UI& ui) {
    float rawSpeed = 0.f;
    ui.widget("Speed").bind(rawSpeed);
}

} // namespace must_not_compile
