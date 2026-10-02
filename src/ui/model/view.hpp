#pragma once

#include "atpl/ui/handle.hpp"
#include "atpl/ui/id.hpp"
#include "atpl/ui/rect.hpp"

#include <optional>
#include <string>

namespace atpl::model {

/// A region the application draws into: the background view, or a view widget.
struct View {
    // ----- From the setup -----

    std::string name;
    std::optional<WidgetId> widget; ///< The view widget, or empty for the background view.

    // ----- Written by layout -----

    FloatRect rect; ///< In window pixels.

    // ----- Written by the application through its handle -----

    ViewHandle::DrawFunction draw; ///< Empty until the application sets one.
};

} // namespace atpl::model
