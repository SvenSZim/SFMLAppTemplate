#pragma once

#include "atpl/ui/error.hpp"
#include "atpl/ui/event.hpp"
#include "atpl/ui/handle.hpp"
#include "atpl/ui/setup.hpp"

#include <SFML/Graphics/RenderWindow.hpp>

#include <span>
#include <string_view>

namespace atpl {

/// The user interface of one window.
///
/// Built once from a `UISetup`. After that the application
/// - links widgets to its data:  `ui.widget("Speed").bind(params.speed);`
/// - draws into views:           `ui.view("world").onDraw(...);`
/// - and runs the three steps of a frame, or lets `App` run them.
///
/// The UI uses a window but does not own it: the window must outlive the UI.
/// Everything here is for the main thread, except `requestRedraw()`.
class UI {
public:
    /// Builds the UI. Throws `SetupError` if the setup is invalid: duplicate names, a grid cell
    /// outside the grid, a column count outside 1 to 3.
    UI(sf::RenderWindow& window, UISetup setup);
    ~UI();

    UI(const UI&) = delete;
    UI(UI&&) = delete;
    UI& operator=(const UI&) = delete;
    UI& operator=(UI&&) = delete;

    // Addressing by name. See setup.hpp for the naming rules.
    // Each throws `SetupError` if the name does not exist or is ambiguous.

    /// A widget, by "Name" if that is unique in the UI, otherwise by "Panel/Name".
    [[nodiscard]] WidgetHandle widget(std::string_view name);

    /// A view: the background view or a view widget.
    [[nodiscard]] ViewHandle view(std::string_view name);

    [[nodiscard]] PanelHandle panel(std::string_view name);

    // One frame, in this order.

    /// Reads the window's pending input. Widgets react, bound values are written, and events
    /// for the application are collected.
    void handleInput();

    /// The events collected by the last `handleInput()`, in the order they happened: widget
    /// events and the input the UI had no use for. See event.hpp.
    /// The list is replaced by the next `handleInput()`.
    [[nodiscard]] std::span<const Event> events() const;

    /// Advances animations and picks up bound values that changed.
    void update();

    /// Draws a frame if anything changed since the last one, and returns whether it did.
    /// When it returns false nothing was drawn and the window still shows the previous frame.
    bool draw();

    /// Asks for a frame to be drawn although nothing in the UI changed: the application has
    /// something new for its views. May be called from any thread.
    void requestRedraw();
};

} // namespace atpl
