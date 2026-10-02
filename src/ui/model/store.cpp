#include "ui/model/store.hpp"

#include "atpl/ui/error.hpp"

#include <cassert>
#include <string>

namespace atpl::model {

namespace {

[[nodiscard]] std::string inQuotes(std::string_view name) {
    return '"' + std::string(name) + '"';
}

[[nodiscard]] std::uint32_t idOf(std::size_t position) {
    return static_cast<std::uint32_t>(position);
}

/// Throws if `index` is not one of the `available` colours of that sort.
void requireColor(
    const std::string& user, std::string_view sort, std::string_view field, std::size_t index, std::size_t available
) {
    if (index < available) {
        return;
    }
    throw SetupError(
        user + " uses " + std::string(sort) + " colour " + std::to_string(index) + " (" + std::string(field) +
        "), but the theme has " + std::to_string(available) +
        (available == 0 ? "" : ", numbered 0 to " + std::to_string(available - 1))
    );
}

void requireColors(const std::string& user, PanelColors colors, const Theme& theme) {
    requireColor(user, "main", "main1", colors.main1, theme.palette.mains.size());
    requireColor(user, "main", "main2", colors.main2, theme.palette.mains.size());
    requireColor(user, "accent", "accent", colors.accent, theme.palette.accents.size());
}

} // namespace

PanelColors withOverride(PanelColors panel, const ColorOverride& override) {
    return {
        .main1 = override.main1.value_or(panel.main1),
        .main2 = override.main2.value_or(panel.main2),
        .accent = override.accent.value_or(panel.accent),
    };
}

Store::Store(const UISetup& setup) {
    std::size_t widgetCount = 0;
    for (const PanelSetup& panel : setup.panels) {
        widgetCount += panel.widgets.size();
    }
    m_panels.reserve(setup.panels.size());
    m_widgets.reserve(widgetCount);

    if (!setup.background.empty()) {
        m_backgroundView = ViewId{ 0 };
        m_names.addView(setup.background, *m_backgroundView);
        m_views.push_back({ .name = setup.background, .widget = std::nullopt, .rect = {}, .draw = {} });
    }

    for (const PanelSetup& panelSetup : setup.panels) {
        const PanelId panelId{ idOf(m_panels.size()) };
        if (panelSetup.name.empty()) {
            throw SetupError("panel " + std::to_string(panelId.index + 1) + " of the setup has no name");
        }
        m_names.addPanel(panelSetup.name, panelId);

        Panel& panel = m_panels.emplace_back();
        panel.name = panelSetup.name;
        panel.title = panelSetup.title.empty() ? panelSetup.name : panelSetup.title;
        panel.placement = panelSetup.placement;
        panel.columns = panelSetup.columns;
        panel.rows = panelSetup.rows;
        panel.width = panelSetup.width;
        panel.colors = { .main1 = panelSetup.main1, .main2 = panelSetup.main2, .accent = panelSetup.accent };
        panel.collapsible = panelSetup.collapsible;
        panel.collapsed = panelSetup.collapsed;
        panel.firstWidget = idOf(m_widgets.size());
        panel.widgetCount = idOf(panelSetup.widgets.size());

        for (const WidgetSetup& widgetSetup : panelSetup.widgets) {
            const WidgetId widgetId{ idOf(m_widgets.size()) };
            const std::string_view name = widgetSetup.name();
            if (name.empty()) {
                throw SetupError(
                    "widget " + std::to_string(widgetId.index - panel.firstWidget + 1) + " of panel " +
                    inQuotes(panel.name) + " has no name"
                );
            }
            m_names.addWidget(panel.name, name, widgetId);

            WidgetSlot& slot = m_widgets.emplace_back();
            slot.name = name;
            slot.panel = panelId;
            slot.cell = widgetSetup.cell();
            slot.span = widgetSetup.span();
            slot.colors = withOverride(panel.colors, widgetSetup.colors());
            slot.binding = widgetSetup.binding();
            slot.widget = widgetSetup.create();
            if (slot.widget == nullptr) {
                throw SetupError(
                    "the descriptor of widget " + inQuotes(panel.name + "/" + slot.name) + " made no widget"
                );
            }

            if (widgetSetup.isView()) {
                slot.view = ViewId{ idOf(m_views.size()) };
                m_names.addView(name, *slot.view);
                m_views.push_back({ .name = slot.name, .widget = widgetId, .rect = {}, .draw = {} });
            }
        }
    }

    requireColors(setup.theme);
}

void Store::requireColors(const Theme& theme) const {
    for (const Panel& panel : m_panels) {
        model::requireColors("panel " + inQuotes(panel.name), panel.colors, theme);
    }
    for (const WidgetSlot& slot : m_widgets) {
        // A widget without an override has its panel's colours, which were just checked.
        const Panel& panel = m_panels[slot.panel.index];
        if (slot.colors.main1 != panel.colors.main1 || slot.colors.main2 != panel.colors.main2 ||
            slot.colors.accent != panel.colors.accent) {
            model::requireColors("widget " + inQuotes(panel.name + "/" + slot.name), slot.colors, theme);
        }
    }
}

Panel& Store::panel(PanelId id) {
    assert(id.index < m_panels.size());
    return m_panels[id.index];
}

const Panel& Store::panel(PanelId id) const {
    assert(id.index < m_panels.size());
    return m_panels[id.index];
}

WidgetSlot& Store::widget(WidgetId id) {
    assert(id.index < m_widgets.size());
    return m_widgets[id.index];
}

const WidgetSlot& Store::widget(WidgetId id) const {
    assert(id.index < m_widgets.size());
    return m_widgets[id.index];
}

View& Store::view(ViewId id) {
    assert(id.index < m_views.size());
    return m_views[id.index];
}

const View& Store::view(ViewId id) const {
    assert(id.index < m_views.size());
    return m_views[id.index];
}

std::span<WidgetSlot> Store::widgetsOf(PanelId id) {
    const Panel& owner = panel(id);
    return std::span(m_widgets).subspan(owner.firstWidget, owner.widgetCount);
}

std::span<const WidgetSlot> Store::widgetsOf(PanelId id) const {
    const Panel& owner = panel(id);
    return std::span(m_widgets).subspan(owner.firstWidget, owner.widgetCount);
}

} // namespace atpl::model
