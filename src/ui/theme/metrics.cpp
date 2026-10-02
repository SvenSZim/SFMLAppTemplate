#include "ui/theme/metrics.hpp"

namespace atpl::theme {

Metrics scaled(const Metrics& metrics) {
    const float scale = metrics.scale;

    Metrics result = metrics;
    result.scale = 1.f;
    result.margin *= scale;
    result.padding *= scale;
    result.gap *= scale;
    result.rowHeight *= scale;
    result.panelWidth *= scale;
    result.headerHeight *= scale;
    result.scrollbarWidth *= scale;
    return result;
}

} // namespace atpl::theme
