#pragma once

#include "atpl/ui/theme.hpp"

namespace atpl::theme {

/// The theme's sizes with its GUI scale applied: what layout and widgets work with.
/// The result's own `scale` is 1, so applying it twice changes nothing.
[[nodiscard]] Metrics scaled(const Metrics& metrics);

} // namespace atpl::theme
