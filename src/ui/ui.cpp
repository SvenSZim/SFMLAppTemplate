#include "atpl/ui/ui.hpp"

#include "ui/frame_loop.hpp"
#include "ui/input/window_events.hpp"
#include "ui/layout/arrange.hpp"
#include "ui/model/store.hpp"
#include "ui/render/font_measurer.hpp"
#include "ui/render/panel_batch.hpp"
#include "ui/render/profiler.hpp"
#include "ui/render/renderer.hpp"
#include "ui/render/text_cache.hpp"
#include "ui/theme/metrics.hpp"
#include "ui/widgets/panel_frame.hpp"

#include <SFML/Graphics/View.hpp>

#include <utility>
#include <vector>

namespace atpl {

// The facade: it owns the modules and calls them in order. What each step does is decided in the
// module that owns the concern, not here.

struct UI::Impl {
    Impl(sf::RenderWindow& targetWindow, UISetup setup) :
        window(targetWindow),
        store(setup), // checks names and colours
        grid(setup.grid),
        theme(std::move(setup.theme)),
        batches(store.panels().size()),
        renderer(&textCache) {
        layout::prepare(store, grid); // finds grid cells; refuses what cannot be laid out

        drawOrder.reserve(batches.size());
        for (render::PanelBatch& batch : batches) {
            drawOrder.push_back(&batch);
        }

        renderer.setProfiler(&profiler);
        profiler.setVisible(setup.profiler);
        themeChanged();
        windowResized();
    }

    // ----- What makes the next frame different -----

    /// The theme's sizes, and everything worked out from them, are out of date.
    void themeChanged() {
        metrics = theme::scaled(theme.metrics);
        profiler.setLook(theme);
        for (model::Panel& panel : store.panels()) {
            panel.dirty = true;
        }
        placementOutdated = true;
        flag.request();
    }

    /// The window has a different size: one unit stays one pixel, and panels find new places.
    void windowResized() {
        windowSize = sf::Vector2f(window.getSize());
        window.setView(sf::View(sf::FloatRect({ 0.f, 0.f }, windowSize)));
        placementOutdated = true;
        flag.request();
    }

    /// A panel was collapsed, expanded, shown or hidden.
    void panelChanged(model::Panel& panel) {
        panel.dirty = true;
        placementOutdated = true;
        flag.request();
    }

    // ----- The steps -----

    /// layout: where every widget, panel and view is.
    void placeIfOutdated() {
        if (!placementOutdated) {
            return;
        }
        placementOutdated = false;
        layout::arrange(store, windowSize, grid, theme, metrics, &textMeasurer);

        // The readout sits in the bottom-right corner (D43).
        const sf::Vector2f readout = profiler.batch().size();
        profiler.setPosition({ windowSize.x - metrics.margin - readout.x, windowSize.y - metrics.margin - readout.y });
    }

    /// model -> render: every batch gets its panel's place, and the panels that look different
    /// are painted again.
    void paintChangedPanels() {
        const std::span<model::Panel> panels = store.panels();
        for (std::size_t i = 0; i < panels.size(); ++i) {
            model::Panel& panel = panels[i];
            render::PanelBatch& batch = batches[i];

            batch.setVisible(panel.shown);
            if (!panel.shown) {
                continue;
            }
            batch.setPosition(panel.rect.position());
            batch.setSize(panel.rect.size());
            if (std::exchange(panel.dirty, false)) {
                batch.markDirty();
            }

            if (batch.isDirty()) {
                const auto build = profiler.measure(render::Profiler::Section::Build);
                const auto layers = batch.rebuild();
                Painter frame(layers.frame, { 0.f, 0.f }, panel.rect.size(), &textMeasurer);
                widgets::paintPanelFrame(frame, panel.title, theme, panel.colors, metrics);

                // Each widget paints itself at the place layout gave it. The content starts
                // below the header.
                const sf::Vector2f content(0.f, metrics.headerHeight);
                for (const model::WidgetSlot& slot : store.widgetsOf(PanelId{ static_cast<std::uint32_t>(i) })) {
                    if (!slot.visible) {
                        continue;
                    }
                    Painter painter(layers.content, slot.rect.position() + content, slot.rect.size(), &textMeasurer);
                    slot.widget->paint(painter, Style(theme, slot.colors, model::stateOf(slot), metrics));
                }
            }
        }
    }

    sf::RenderWindow& window;
    sf::Vector2f windowSize;

    model::Store store;
    GridSetup grid;
    Theme theme;
    Metrics metrics; // the theme's, with the GUI scale applied
    bool placementOutdated = true;

    std::vector<render::PanelBatch> batches;    // one per panel, in id order
    std::vector<render::PanelBatch*> drawOrder; // what the renderer is given
    render::FontMeasurer textMeasurer;
    render::TextCache textCache;
    render::Renderer renderer;
    render::Profiler profiler;
    frame::RedrawFlag flag;

    std::vector<Event> events;
};

UI::UI(sf::RenderWindow& window, UISetup setup) :
    m_impl(std::make_unique<Impl>(window, std::move(setup))) {}

UI::~UI() = default;

// ----- Addressing by name -----

WidgetHandle UI::widget(std::string_view name) {
    return { *this, m_impl->store.names().widget(name) };
}

ViewHandle UI::view(std::string_view name) {
    return { *this, m_impl->store.names().view(name) };
}

PanelHandle UI::panel(std::string_view name) {
    return { *this, m_impl->store.names().panel(name) };
}

// ----- Looks -----

const Theme& UI::theme() const {
    return m_impl->theme;
}

void UI::setTheme(Theme theme) {
    m_impl->store.requireColors(theme); // a theme that does not fit is refused before anything changes
    m_impl->theme = std::move(theme);
    m_impl->themeChanged();
}

// ----- Measuring -----

void UI::setProfilerVisible(bool visible) {
    m_impl->profiler.setVisible(visible);
    m_impl->flag.request();
}

bool UI::isProfilerVisible() const {
    return m_impl->profiler.isVisible();
}

// ----- One frame -----

void UI::handleInput() {
    Impl& impl = *m_impl;
    impl.events.clear();

    // While no frame is asked for, the first call sleeps until something happens, for one
    // display frame at most: this is where an idle application spends its time.
    while (const std::optional<sf::Event> event = frame::nextEvent(impl.window, impl.flag)) {
        if (event->is<sf::Event::Resized>()) {
            impl.windowResized();
        }
        if (const std::optional<Event> forwarded = input::windowEvent(*event)) {
            impl.events.push_back(*forwarded);
        }
    }
}

std::span<const Event> UI::events() const {
    return m_impl->events;
}

void UI::update() {
    m_impl->placeIfOutdated();
}

bool UI::draw() {
    Impl& impl = *m_impl;
    impl.placeIfOutdated(); // the application may have changed a panel since `update`
    impl.paintChangedPanels();
    return impl.renderer.present(impl.window, impl.theme.palette.window, impl.flag, impl.drawOrder).has_value();
}

void UI::requestRedraw() {
    m_impl->flag.request();
}

// ----- Handles -----
//
// A handle is an id and a way back to the UI. What it does is a read or a write in the model,
// plus saying what that makes out of date.

WidgetHandle& WidgetHandle::setEnabled(bool enabled) {
    UI::Impl& impl = *m_ui->m_impl;
    model::WidgetSlot& slot = impl.store.widget(m_id);
    if (slot.enabled != enabled) {
        slot.enabled = enabled;
        impl.store.panel(slot.panel).dirty = true;
        impl.flag.request();
    }
    return *this;
}

bool WidgetHandle::isEnabled() const {
    return m_ui->m_impl->store.widget(m_id).enabled;
}

ViewHandle& ViewHandle::onDraw(DrawFunction draw) {
    m_ui->m_impl->store.view(m_id).draw = std::move(draw);
    m_ui->m_impl->flag.request();
    return *this;
}

FloatRect ViewHandle::rect() const {
    return m_ui->m_impl->store.view(m_id).rect;
}

PanelHandle& PanelHandle::setCollapsed(bool collapsed) {
    model::Panel& panel = m_ui->m_impl->store.panel(m_id);
    if (panel.collapsed != collapsed) {
        panel.collapsed = collapsed;
        m_ui->m_impl->panelChanged(panel);
    }
    return *this;
}

bool PanelHandle::isCollapsed() const {
    return m_ui->m_impl->store.panel(m_id).collapsed;
}

PanelHandle& PanelHandle::setVisible(bool visible) {
    model::Panel& panel = m_ui->m_impl->store.panel(m_id);
    if (panel.visible != visible) {
        panel.visible = visible;
        m_ui->m_impl->panelChanged(panel);
    }
    return *this;
}

bool PanelHandle::isVisible() const {
    return m_ui->m_impl->store.panel(m_id).visible;
}

FloatRect PanelHandle::rect() const {
    return m_ui->m_impl->store.panel(m_id).rect;
}

} // namespace atpl
