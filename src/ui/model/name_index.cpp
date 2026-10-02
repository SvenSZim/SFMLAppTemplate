#include "ui/model/name_index.hpp"

#include "atpl/ui/error.hpp"

#include <algorithm>

namespace atpl::model {

namespace {

[[nodiscard]] std::string inQuotes(std::string_view name) {
    return '"' + std::string(name) + '"';
}

/// '/' separates panel and widget in "Panel/Name", so no name may contain it.
void requireNoSlash(std::string_view what, std::string_view name) {
    if (name.find('/') != std::string_view::npos) {
        throw SetupError(
            std::string(what) + " name " + inQuotes(name) + " contains '/', which is reserved for \"Panel/Name\""
        );
    }
}

/// The names in a map, sorted, for messages: `"A", "B"`.
template <typename Map>
[[nodiscard]] std::string listOf(const Map& map) {
    std::vector<std::string_view> names;
    names.reserve(map.size());
    for (const auto& [name, value] : map) {
        names.push_back(name);
    }
    std::sort(names.begin(), names.end());

    std::string list;
    for (const std::string_view name : names) {
        if (!list.empty()) {
            list += ", ";
        }
        list += inQuotes(name);
    }
    return list;
}

} // namespace

void NameIndex::addPanel(std::string_view name, PanelId id) {
    requireNoSlash("panel", name);
    if (!m_panels.emplace(std::string(name), id).second) {
        throw SetupError("two panels are named " + inQuotes(name) + "; panel names must be unique");
    }
}

void NameIndex::addWidget(std::string_view panel, std::string_view name, WidgetId id) {
    requireNoSlash("widget", name);

    std::string qualified;
    qualified.reserve(panel.size() + 1 + name.size());
    qualified.append(panel).append("/").append(name);
    if (!m_qualified.emplace(std::move(qualified), id).second) {
        throw SetupError(
            "panel " + inQuotes(panel) + " has two widgets named " + inQuotes(name) +
            "; widget names must be unique within their panel"
        );
    }

    m_widgets[std::string(name)].push_back({ id, std::string(panel) });
}

void NameIndex::addView(std::string_view name, ViewId id) {
    requireNoSlash("view", name);
    if (!m_views.emplace(std::string(name), id).second) {
        throw SetupError("two views are named " + inQuotes(name) + "; view names must be unique in the whole UI");
    }
}

PanelId NameIndex::panel(std::string_view name) const {
    const auto found = m_panels.find(name);
    if (found == m_panels.end()) {
        throw SetupError(
            "there is no panel named " + inQuotes(name) +
            (m_panels.empty() ? "; the UI has no panels" : "; the panels are " + listOf(m_panels))
        );
    }
    return found->second;
}

WidgetId NameIndex::widget(std::string_view name) const {
    const std::size_t slash = name.find('/');

    // "Panel/Name"
    if (slash != std::string_view::npos) {
        const auto found = m_qualified.find(name);
        if (found != m_qualified.end()) {
            return found->second;
        }
        const std::string_view panel = name.substr(0, slash);
        const std::string_view widget = name.substr(slash + 1);
        if (!m_panels.contains(panel)) {
            throw SetupError(
                "there is no panel named " + inQuotes(panel) + " (looking for widget " + inQuotes(name) + ")"
            );
        }
        throw SetupError("panel " + inQuotes(panel) + " has no widget named " + inQuotes(widget));
    }

    // "Name"
    const auto found = m_widgets.find(name);
    if (found == m_widgets.end()) {
        throw SetupError("there is no widget named " + inQuotes(name));
    }
    const std::vector<Named>& candidates = found->second;
    if (candidates.size() > 1) {
        std::string choices;
        for (const Named& candidate : candidates) {
            if (!choices.empty()) {
                choices += " or ";
            }
            choices += inQuotes(candidate.panel + "/" + std::string(name));
        }
        throw SetupError("several panels have a widget named " + inQuotes(name) + "; say which one: " + choices);
    }
    return candidates.front().id;
}

ViewId NameIndex::view(std::string_view name) const {
    const auto found = m_views.find(name);
    if (found == m_views.end()) {
        throw SetupError(
            "there is no view named " + inQuotes(name) +
            (m_views.empty() ? "; the UI has no views" : "; the views are " + listOf(m_views))
        );
    }
    return found->second;
}

} // namespace atpl::model
