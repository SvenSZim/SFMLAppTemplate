#pragma once

#include "atpl/ui/id.hpp"

#include <cstddef>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace atpl::model {

/// Finds panels, widgets and views by name (D13).
///
/// - Panel names are unique in the UI.
/// - Widget names are unique in their panel. A widget is found as "Name" if no other panel has
///   a widget of that name, and always as "Panel/Name".
/// - View names are unique among views.
///
/// Everything that is wrong throws `SetupError` with a message that names the offender: a
/// duplicate, a name with a '/' in it, a name that does not exist, a name that fits several
/// widgets.
///
/// Names are looked up when the application asks for a handle, never per frame.
class NameIndex {
public:
    void addPanel(std::string_view name, PanelId id);
    void addWidget(std::string_view panel, std::string_view name, WidgetId id);
    void addView(std::string_view name, ViewId id);

    [[nodiscard]] PanelId panel(std::string_view name) const;
    [[nodiscard]] WidgetId widget(std::string_view name) const;
    [[nodiscard]] ViewId view(std::string_view name) const;

private:
    // Lets the maps be searched with a string_view, without making a string first.
    struct Hash {
        using is_transparent = void;
        [[nodiscard]] std::size_t operator()(std::string_view text) const noexcept {
            return std::hash<std::string_view>{}(text);
        }
    };
    template <typename T>
    using Map = std::unordered_map<std::string, T, Hash, std::equal_to<>>;

    /// The widgets that share a name, each with its panel.
    struct Named {
        WidgetId id;
        std::string panel;
    };

    Map<PanelId> m_panels;
    Map<WidgetId> m_qualified;         // "Panel/Name"
    Map<std::vector<Named>> m_widgets; // "Name"
    Map<ViewId> m_views;
};

} // namespace atpl::model
