#pragma once

#include <cstdint>

namespace atpl {

// Ids of the things a UI consists of. They are assigned when the UI is built, stay valid for its
// lifetime, and are what events refer to. Names are resolved to ids once, through `UI::widget`,
// `UI::panel` and `UI::view`; everything after that works with ids.

struct PanelId {
    std::uint32_t index = 0;
    [[nodiscard]] friend constexpr bool operator==(PanelId, PanelId) = default;
};

struct WidgetId {
    std::uint32_t index = 0;
    [[nodiscard]] friend constexpr bool operator==(WidgetId, WidgetId) = default;
};

struct ViewId {
    std::uint32_t index = 0;
    [[nodiscard]] friend constexpr bool operator==(ViewId, ViewId) = default;
};

} // namespace atpl
