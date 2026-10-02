#pragma once

#include "atpl/ui/binding.hpp"
#include "atpl/ui/id.hpp"
#include "atpl/ui/rect.hpp"

#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/System/Vector2.hpp>

#include <concepts>
#include <functional>
#include <string_view>
#include <type_traits>
#include <utility>

namespace atpl {

class UI;

// Handles are how an application talks to one widget, view or panel. A handle is a small value:
// get one from the UI when needed, use it, drop it. There is no need to keep handles around.
//
// Handles are for the main thread, and are valid as long as the UI they came from.

/// One widget.
class WidgetHandle {
public:
    [[nodiscard]] WidgetId id() const { return m_id; }

    /// Links the widget to a value. From then on the widget shows the value, and writes to it
    /// when the user changes it. Replaces an earlier binding.
    ///
    /// Accepts a `Param<T>`, a `Series`, an implementation of a binding interface, or the result
    /// of `AnyBinding::fromFunctions`. A plain variable does not compile.
    /// Throws `SetupError` if the widget does not work with that kind of value, or if it edits
    /// its value and the binding is read-only.
    WidgetHandle& bind(AnyBinding binding);

    /// Shorthand for `bind(AnyBinding::fromFunctions(getter, setter))`.
    template <ValueGetter Getter, typename Setter>
        requires std::invocable<Setter, std::remove_cvref_t<std::invoke_result_t<Getter>>>
    WidgetHandle& bind(Getter getter, Setter setter) {
        return bind(AnyBinding::fromFunctions(std::move(getter), std::move(setter)));
    }

    /// Shorthand for `bind(AnyBinding::fromFunction(getter))`: a read-only binding.
    template <ValueGetter Getter>
    WidgetHandle& bind(Getter getter) {
        return bind(AnyBinding::fromFunction(std::move(getter)));
    }

    /// Removes the binding. The widget keeps the value it shows.
    WidgetHandle& unbind();

    /// The value the widget currently shows, as a `T`.
    /// Throws `SetupError` if the widget's kind of value cannot be read as a `T`.
    template <BindableValue T>
    [[nodiscard]] T get() const;

    /// Sets the widget's value as if the user had entered it; a bound value is written too.
    /// Throws `SetupError` if the widget's kind of value cannot be set from a `T`.
    template <BindableValue T>
    WidgetHandle& set(const T& value);

    /// Greyed out and not reacting to input while disabled.
    WidgetHandle& setEnabled(bool enabled);
    [[nodiscard]] bool isEnabled() const;

private:
    friend class UI;
    WidgetHandle(UI& ui, WidgetId id) :
        m_ui(&ui),
        m_id(id) {}

    UI* m_ui;
    WidgetId m_id;
};

/// One view: the background view or a view widget.
class ViewHandle {
public:
    /// Draws the view's content. `target` is set up so that (0, 0) is the view's top-left corner,
    /// one unit is one pixel, and nothing outside the view's rectangle is drawn. `size` is the
    /// view's size in pixels.
    using DrawFunction = std::function<void(sf::RenderTarget& target, sf::Vector2f size)>;

    [[nodiscard]] ViewId id() const { return m_id; }

    /// Sets the function that draws the view. It is called on the main thread, once per drawn
    /// frame, at the view's place in the drawing order. Replaces an earlier function.
    ///
    /// The UI only draws a frame when something changed. When the application has something new
    /// to show, it says so with `UI::requestRedraw()`.
    ViewHandle& onDraw(DrawFunction draw);

    /// The view's rectangle in window pixels, as of the last layout.
    [[nodiscard]] FloatRect rect() const;

private:
    friend class UI;
    ViewHandle(UI& ui, ViewId id) :
        m_ui(&ui),
        m_id(id) {}

    UI* m_ui;
    ViewId m_id;
};

/// One panel.
class PanelHandle {
public:
    [[nodiscard]] PanelId id() const { return m_id; }

    /// Folds the panel down to its header, or unfolds it, as if the user had clicked the header.
    PanelHandle& setCollapsed(bool collapsed);
    [[nodiscard]] bool isCollapsed() const;

    /// A hidden panel is neither drawn nor reacts to input, and leaves no gap in a stack.
    PanelHandle& setVisible(bool visible);
    [[nodiscard]] bool isVisible() const;

    /// The panel's rectangle in window pixels, as of the last layout.
    [[nodiscard]] FloatRect rect() const;

private:
    friend class UI;
    PanelHandle(UI& ui, PanelId id) :
        m_ui(&ui),
        m_id(id) {}

    UI* m_ui;
    PanelId m_id;
};

} // namespace atpl
