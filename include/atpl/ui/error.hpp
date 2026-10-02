#pragma once

#include <stdexcept>

namespace atpl {

/// Thrown when the UI is set up or addressed incorrectly: a duplicate or unknown name, a binding
/// of the wrong kind, an invalid placement. The message names the offending panel or widget.
///
/// These are mistakes in the application's code, found the first time the code runs. They are
/// reported at once instead of leaving a UI that silently misbehaves.
class SetupError : public std::logic_error {
public:
    using std::logic_error::logic_error;
};

} // namespace atpl
