#include "ui/core/container/container.hpp"

namespace ui::core::container {

void Container::update() {
    if (rect.running()) {
        flag.flags.visual = 1;
    }
}

void Container::expand() {
    if (state.state.disabled) return;
    if (state.state.expanded) return;
    state.state.expanded = 1;
    rect.setValue(expandedRect);
    flag.flags.layout = 1;
    flag.flags.visual = 1;
}

void Container::collapse() {
    if (state.state.disabled) return;
    if (!state.state.expanded) return;
    state.state.expanded = 0;
    rect.setValue(collapsedRect);
    flag.flags.layout = 1;
    flag.flags.visual = 1;
}

void Container::toggle() {
    if (isExpanded()) {
        collapse();
    } else {
        expand();
    }
}

}
