#pragma once

#include "atpl/ui/id.hpp"
#include "atpl/ui/setup.hpp"
#include "atpl/ui/theme.hpp"

#include "ui/model/name_index.hpp"
#include "ui/model/panel.hpp"
#include "ui/model/view.hpp"
#include "ui/model/widget_slot.hpp"

#include <optional>
#include <span>
#include <vector>

namespace atpl::model {

/// Everything a UI consists of: its panels, widgets and views. The storage every other module
/// reads, and the only place these things live.
///
/// Built once from the setup; nothing is added or removed afterwards. That is what makes ids
/// simple: an id is the position in its array, so finding something by id is one array access.
///
///   PanelId    panels in the order of the setup
///   WidgetId   widgets panel by panel, each panel's in the order listed
///   ViewId     the background view first, if there is one, then view widgets in widget order
///
/// The store keeps data and does nothing with it: no layout, no input, no drawing.
class Store {
public:
    /// Throws `SetupError` if the setup is wrong in a way the model can see: a missing, duplicate
    /// or invalid name, a descriptor that makes no widget, or colours the theme does not have.
    explicit Store(const UISetup& setup);

    Store(Store&&) noexcept = default;
    Store& operator=(Store&&) noexcept = default;

    // ----- By id -----

    [[nodiscard]] Panel& panel(PanelId id);
    [[nodiscard]] const Panel& panel(PanelId id) const;
    [[nodiscard]] WidgetSlot& widget(WidgetId id);
    [[nodiscard]] const WidgetSlot& widget(WidgetId id) const;
    [[nodiscard]] View& view(ViewId id);
    [[nodiscard]] const View& view(ViewId id) const;

    // ----- All of them, in id order -----

    [[nodiscard]] std::span<Panel> panels() { return m_panels; }
    [[nodiscard]] std::span<const Panel> panels() const { return m_panels; }
    [[nodiscard]] std::span<WidgetSlot> widgets() { return m_widgets; }
    [[nodiscard]] std::span<const WidgetSlot> widgets() const { return m_widgets; }
    [[nodiscard]] std::span<View> views() { return m_views; }
    [[nodiscard]] std::span<const View> views() const { return m_views; }

    /// A panel's widgets, in the order they were listed.
    [[nodiscard]] std::span<WidgetSlot> widgetsOf(PanelId id);
    [[nodiscard]] std::span<const WidgetSlot> widgetsOf(PanelId id) const;

    /// The id of the first of a panel's widgets; the others follow.
    [[nodiscard]] WidgetId firstWidgetOf(PanelId id) const { return { panel(id).firstWidget }; }

    [[nodiscard]] std::optional<ViewId> backgroundView() const { return m_backgroundView; }

    /// The panels from the bottom to the top: those in the window's grid first, then the
    /// floating ones, each in the order of the setup. This is the order they are drawn in, and,
    /// the other way round, the order the pointer finds them in.
    [[nodiscard]] std::vector<PanelId> stackingOrder() const;

    // ----- By name: for handles, never per frame -----

    [[nodiscard]] const NameIndex& names() const { return m_names; }

    // ----- Layout theme -----

    /// Fills in what the panels left to the layout theme: their placement and whether they
    /// collapse. Done when the store is built, and again when the layout theme is replaced;
    /// grid cells have to be found anew afterwards (`layout::prepare`).
    void applyLayout(const Layout& layout);

    // ----- Colours -----

    /// Throws `SetupError` if a panel or a widget uses a main or accent colour the theme does not
    /// have. Checked when the store is built, and to be checked before a theme replaces another.
    void requireColors(const Theme& theme) const;

private:
    std::vector<Panel> m_panels;
    std::vector<WidgetSlot> m_widgets;
    std::vector<View> m_views;
    std::optional<ViewId> m_backgroundView;
    NameIndex m_names;
};

/// A widget's colours: its panel's, with the ones its override names replaced.
[[nodiscard]] PanelColors withOverride(PanelColors panel, const ColorOverride& override);

} // namespace atpl::model
