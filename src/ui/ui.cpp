#include "atpl/ui/ui.hpp"

#include "atpl/ui/widgets.hpp"

#include "ui/binding/sync.hpp"
#include "ui/frame_loop.hpp"
#include "ui/input/input_system.hpp"
#include "ui/input/window_events.hpp"
#include "ui/layout/arrange.hpp"
#include "ui/layout/overlay_placement.hpp"
#include "ui/layout/panel_placement.hpp"
#include "ui/layout/widget_layout.hpp"
#include "ui/model/store.hpp"
#include "ui/render/font_measurer.hpp"
#include "ui/render/panel_batch.hpp"
#include "ui/render/profiler.hpp"
#include "ui/render/renderer.hpp"
#include "ui/render/text_cache.hpp"
#include "ui/widgets/panel_frame.hpp"

#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/Graphics/View.hpp>
#include <SFML/System/Clock.hpp>

#include <algorithm>
#include <cmath>
#include <optional>
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
        layout(std::move(setup.layout)),
        batches(store.panels().size()),
        thumbs(store.panels().size(), 0.f),
        renderer(&textCache) {
        layout::prepare(store, grid, layout); // finds grid cells; refuses what cannot be laid out
        binding::attachAll(store);            // the bindings given in the setup; refuses wrong kinds
        stackingChanged();

        input.setLook(theme, &textMeasurer);
        renderer.setProfiler(&profiler);
        renderer.setViewPainter([this](sf::RenderTarget& target, std::optional<std::size_t> afterPanel) {
            drawViews(target, afterPanel);
        });
        profiler.setVisible(setup.profiler);
        windowResized();
        looksChanged();
    }

    // ----- What makes the next frame different -----

    /// The theme, the layout theme or the sizes changed: everything is painted and placed anew.
    void looksChanged() {
        sizes = layout.sizesAt(windowSize);
        profiler.setLook(theme, sizes);
        for (model::Panel& panel : store.panels()) {
            panel.dirty = true;
        }
        placementOutdated = true;
        flag.request();
    }

    /// The window has a different size: one unit stays one pixel, and panels find new places.
    /// If the sizes follow the window, text and outlines change too.
    void windowResized() {
        windowSize = sf::Vector2f(window.getSize());
        window.setView(sf::View(sf::FloatRect({ 0.f, 0.f }, windowSize)));
        if (layout.sizesAt(windowSize) != sizes) {
            looksChanged();
        }
        placementOutdated = true;
        flag.request();
    }

    /// A panel was collapsed, expanded, shown or hidden.
    void panelChanged(model::Panel& panel) {
        panel.dirty = true;
        placementOutdated = true;
        flag.request();
    }

    /// Panels may have moved between the window's grid and floating: the order they are drawn
    /// in, and found by the pointer in, follows.
    void stackingChanged() {
        baseStacking = store.stackingOrder();
        stacking.clear();
        orderCards();
    }

    /// The order of the panels as it is now: in an overlapped stack, cards cover each other in
    /// their layers, and the one under the pointer comes to the front. Only the order changes;
    /// no panel is painted again.
    void orderCards() {
        std::vector<PanelId> order = layout::cardOrder(store, baseStacking, input.hoveredPanel());
        if (order == stacking) {
            return;
        }
        stacking = std::move(order);
        drawOrder.clear();
        for (const PanelId panel : stacking) {
            drawOrder.push_back(&batches[panel.index]);
        }
        flag.request();
    }

    /// A panel was unfolded: in a stack of cards that would not fit, the others fold.
    void makeRoomFor(PanelId unfolded) {
        for (const PanelId other : layout::cardsToFold(store, unfolded, windowSize, sizes, layout)) {
            panelChanged(store.panel(other));
            widgets::setCollapsed(store.panel(other), true);
        }
    }

    // ----- The steps -----

    /// layout: where every widget, panel and view is.
    void placeIfOutdated() {
        if (!placementOutdated) {
            return;
        }
        placementOutdated = false;
        layout::arrange(store, windowSize, grid, theme, sizes, &textMeasurer, layout);
        input.clampScroll(store, sizes); // a panel may have less to scroll now
        input.forgetHover(store);        // what is under the pointer may have changed; the next move says

        // The readout sits in the bottom-right corner (D43).
        const sf::Vector2f readout = profiler.batch().size();
        profiler.setPosition({ windowSize.x - sizes.margin - readout.x, windowSize.y - sizes.margin - readout.y });
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
            // Widgets are cut off at the edge of the content area: below the header, and before
            // the panel's border at the bottom.
            batch.setContentClip(widgets::contentArea(panel.rect.size(), sizes));
            // Scrolling only moves what is there: the content up, the scrollbar's thumb down.
            const auto bar = widgets::scrollbarOf(panel, sizes, layout::contentOverflow(panel, sizes));
            batch.setScroll(panel.scroll);
            batch.setScrollbarOffset(bar ? bar->thumbTop(panel.scroll) - bar->track.top() : 0.f);
            const float thumb = bar ? bar->thumbLength : 0.f;
            if (std::exchange(panel.dirty, false) || thumb != thumbs[i]) {
                batch.markDirty(); // looks different, or the scrollbar came, went or changed length
            }
            thumbs[i] = thumb;

            if (batch.isDirty()) {
                const auto build = profiler.measure(render::Profiler::Section::Build);
                const auto layers = batch.rebuild();
                Painter frame(layers.frame, { 0.f, 0.f }, panel.rect.size(), &textMeasurer);
                widgets::paintPanelFrame(frame, panel, theme, sizes);
                if (bar.has_value()) {
                    Painter scrollbar(layers.scrollbar, { 0.f, 0.f }, panel.rect.size(), &textMeasurer);
                    widgets::paintScrollbar(scrollbar, panel, *bar, theme, sizes);
                }

                // Each widget paints itself at the place layout gave it. The content starts
                // below the header.
                const sf::Vector2f content(0.f, sizes.headerHeight);
                for (const model::WidgetSlot& slot : store.widgetsOf(PanelId{ static_cast<std::uint32_t>(i) })) {
                    if (!slot.visible) {
                        continue;
                    }
                    Painter painter(layers.content, slot.rect.position() + content, slot.rect.size(), &textMeasurer);
                    slot.widget->paint(painter, Style(theme, slot.colors, model::stateOf(slot), sizes));
                }
            }
        }
    }

    /// The open overlay, if there is one: placed for where its widget is now, and painted
    /// again when it opens, moves or changes size, or when its widget's panel is redrawn. To be
    /// called before the panels are painted, which clears their marks.
    void placeAndPaintOverlay() {
        input.closeOverlayIfGone(store); // its panel may have folded, or the widget been hidden
        const std::optional<WidgetId> open = input.overlay();
        layout::placeOverlays(store, open, windowSize, theme, sizes, &textMeasurer);
        if (!open.has_value()) {
            overlay.setVisible(false);
            overlayOf.reset();
            return;
        }

        const model::WidgetSlot& slot = store.widget(*open);
        const FloatRect rect = slot.overlayRect.value_or(FloatRect());
        overlay.setVisible(true);
        overlay.setPosition(rect.position());
        overlay.setSize(rect.size()); // a new size repaints
        overlay.setContentClip(std::nullopt);
        if (overlayOf != open || store.panel(slot.panel).dirty) {
            overlay.markDirty();
        }
        overlayOf = open;
        if (overlay.isDirty()) {
            const auto build = profiler.measure(render::Profiler::Section::Build);
            const auto layers = overlay.rebuild();
            Painter painter(layers.content, { 0.f, 0.f }, rect.size(), &textMeasurer);
            const FloatRect anchor(slot.overlayAnchor.position() - rect.position(), slot.overlayAnchor.size());
            slot.widget->paintOverlay(painter, Style(theme, slot.colors, model::stateOf(slot), sizes), anchor);
        }
    }

    /// The application's views at one point of the drawing order: the background view, or the
    /// views inside the panel drawn at `afterPanel` in the draw order.
    void drawViews(sf::RenderTarget& target, std::optional<std::size_t> afterPanel) {
        if (!afterPanel.has_value()) {
            if (const std::optional<ViewId> background = store.backgroundView()) {
                drawView(target, *background);
            }
            return;
        }
        const PanelId panel = stacking[*afterPanel];
        for (std::uint32_t i = 0; i < store.views().size(); ++i) {
            const model::View& view = store.views()[i];
            if (view.widget.has_value() && store.widget(*view.widget).panel == panel) {
                drawView(target, ViewId{ i });
            }
        }
    }

    /// One view: its draw function with (0, 0) at the view's top-left corner, one unit a pixel,
    /// and nothing drawn outside the part of it that can be seen.
    void drawView(sf::RenderTarget& target, ViewId id) {
        const model::View& view = store.view(id);
        const layout::ViewPlace place = layout::placeOf(store, id, sizes);
        if (!view.draw || place.visible.width() <= 0.f || place.visible.height() <= 0.f) {
            return;
        }
        const sf::Vector2f targetSize(target.getSize());
        sf::View camera(sf::FloatRect({ 0.f, 0.f }, place.rect.size()));
        camera.setViewport(
            sf::FloatRect(
                { place.rect.left() / targetSize.x, place.rect.top() / targetSize.y },
                { place.rect.width() / targetSize.x, place.rect.height() / targetSize.y }
            )
        );
        camera.setScissor(render::scissorFor(place.visible, target.getSize()));
        const sf::View previous = target.getView();
        target.setView(camera);
        view.draw(target, place.rect.size());

        // A view the user selected says so with its outline, above what was drawn into it.
        if (view.widget.has_value() && store.widget(*view.widget).focused) {
            const model::WidgetSlot& slot = store.widget(*view.widget);
            const float thickness = std::max(std::round(2.f * sizes.text), 1.f);
            sf::RectangleShape outline(place.rect.size() - sf::Vector2f(thickness, thickness) * 2.f);
            outline.setPosition({ thickness, thickness });
            outline.setFillColor(sf::Color::Transparent);
            outline.setOutlineColor(theme.resolve(View::Selection, State::Normal, slot.colors, sizes.text).color);
            outline.setOutlineThickness(thickness);
            target.setView(camera); // the draw function may have changed it
            target.draw(outline);
        }
        target.setView(previous); // whatever the draw function did to it
    }

    sf::RenderWindow& window;
    sf::Vector2f windowSize;

    model::Store store;
    GridSetup grid;
    Theme theme;
    Layout layout;
    Sizes sizes; // the layout's, for the window as it is
    bool placementOutdated = true;

    std::vector<render::PanelBatch> batches;    // one per panel, in id order
    std::vector<float> thumbs;                  // the scrollbar thumb each was painted with, 0 for none
    std::vector<PanelId> baseStacking;          // the panels from the bottom to the top, as set up
    std::vector<PanelId> stacking;              // the same, with cards in the order they are seen
    std::vector<render::PanelBatch*> drawOrder; // the same, as the renderer is given them
    render::PanelBatch overlay;                 // above every panel: an open dropdown list
    std::optional<WidgetId> overlayOf;          // whose overlay it holds
    render::FontMeasurer textMeasurer;
    render::TextCache textCache;
    render::Renderer renderer;
    render::Profiler profiler;
    frame::RedrawFlag flag;

    input::InputSystem input;
    sf::Clock clock;        // the time between updates, for animations
    bool animating = false; // something moves: frames keep coming
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
    m_impl->looksChanged();
}

// ----- Sizes and positions -----

const Layout& UI::layout() const {
    return m_impl->layout;
}

void UI::setLayout(Layout layout) {
    Impl& impl = *m_impl;

    // The panels that take their place from the layout theme get the new one's. If they do not
    // fit, everything goes back to how it was.
    std::vector<Placement> before;
    before.reserve(impl.store.panels().size());
    for (const model::Panel& panel : impl.store.panels()) {
        before.push_back(panel.placement);
    }
    impl.store.applyLayout(layout);
    try {
        layout::prepare(impl.store, impl.grid, layout);
    } catch (...) {
        // Back to how it was: the old placements, and the cells that follow from them.
        impl.store.applyLayout(impl.layout);
        for (std::size_t i = 0; i < before.size(); ++i) {
            impl.store.panels()[i].placement = before[i];
        }
        layout::prepareWidgets(impl.store, impl.layout);
        throw;
    }

    impl.layout = std::move(layout);
    impl.stackingChanged();
    impl.looksChanged();
}

const Sizes& UI::sizes() const {
    return m_impl->sizes;
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

    // While no frame is asked for, the first event is waited for, one display frame at most:
    // this is where an idle application spends its time. The rest are taken as they are,
    // without waiting, so that a stream of pointer moves cannot hold up the frame.
    for (std::optional<sf::Event> event = frame::nextEvent(impl.window, impl.flag); event.has_value();
         event = frame::pendingEvent(impl.window, impl.flag)) {
        if (event->is<sf::Event::Resized>()) {
            impl.windowResized();
        }
        if (const std::optional<Event> forwarded = input::windowEvent(*event)) {
            impl.events.push_back(*forwarded); // the window's own events always reach the application
            continue;
        }
        impl.input.handle(*event, impl.store, impl.stacking, impl.sizes, impl.events);
        if (impl.input.takeFolded()) {
            impl.placementOutdated = true; // a panel began to fold or unfold
            impl.flag.request();
        }
        if (const std::optional<PanelId> unfolded = impl.input.takeUnfolded()) {
            impl.makeRoomFor(*unfolded);
        }
        impl.orderCards(); // the card under the pointer may be another one now
    }
}

std::span<const Event> UI::events() const {
    return m_impl->events;
}

void UI::update() {
    Impl& impl = *m_impl;
    // Panels that fold or unfold move on by the time since the last update; while one does,
    // frames keep coming.
    const float seconds = impl.clock.restart().asSeconds();
    impl.animating = widgets::animatePanels(impl.store, std::min(seconds, 0.1f), impl.layout.foldSeconds);
    if (impl.animating) {
        impl.placementOutdated = true;
        impl.flag.request();
    }
    // Bound values that changed are handed to their widgets.
    binding::sync(impl.store, binding::Clock::now());
    impl.placeIfOutdated();
    impl.orderCards(); // a stack may have begun or stopped overlapping
}

bool UI::draw() {
    Impl& impl = *m_impl;
    impl.placeIfOutdated(); // the application may have changed a panel since `update`
    impl.placeAndPaintOverlay();
    impl.paintChangedPanels();
    const bool drawn =
        impl.renderer.present(impl.window, impl.theme.palette.window, impl.flag, impl.drawOrder, &impl.overlay)
            .has_value();
    if (impl.animating) {
        impl.flag.request(); // something moves: the next pass must not wait for input
    }
    return drawn;
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

WidgetHandle& WidgetHandle::bind(AnyBinding binding) {
    binding::attach(m_ui->m_impl->store, m_id, std::move(binding));
    m_ui->m_impl->flag.request();
    return *this;
}

WidgetHandle& WidgetHandle::unbind() {
    binding::attach(m_ui->m_impl->store, m_id, std::nullopt);
    return *this;
}

Value WidgetHandle::value() const {
    const model::WidgetSlot& slot = m_ui->m_impl->store.widget(m_id);
    if (std::optional<Value> current = slot.widget->value()) {
        return *std::move(current);
    }
    throw SetupError("widget \"" + slot.name + "\" has no value");
}

WidgetHandle& WidgetHandle::setValue(const Value& value) {
    UI::Impl& impl = *m_ui->m_impl;
    model::WidgetSlot& slot = impl.store.widget(m_id);
    if (!slot.widget->accepts(kindOf(value))) {
        throw SetupError("widget \"" + slot.name + "\" does not work with values of this kind");
    }
    slot.widget->setValue(value);
    binding::write(slot, value); // as if the user had entered it
    impl.store.panel(slot.panel).dirty = true;
    impl.flag.request();
    return *this;
}

ViewHandle& ViewHandle::onDraw(DrawFunction draw) {
    m_ui->m_impl->store.view(m_id).draw = std::move(draw);
    m_ui->m_impl->flag.request();
    return *this;
}

FloatRect ViewHandle::rect() const {
    const UI::Impl& impl = *m_ui->m_impl;
    return layout::placeOf(impl.store, m_id, impl.sizes).rect; // scrolled with its panel
}

PanelHandle& PanelHandle::setCollapsed(bool collapsed) {
    model::Panel& panel = m_ui->m_impl->store.panel(m_id);
    if (widgets::setCollapsed(panel, collapsed)) { // folds over a moment, as a click does
        m_ui->m_impl->panelChanged(panel);
        if (!collapsed) {
            m_ui->m_impl->makeRoomFor(m_id);
        }
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
