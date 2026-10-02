#pragma once

#include "atpl/ui/error.hpp"
#include "atpl/ui/event.hpp"
#include "atpl/ui/handle.hpp"
#include "atpl/ui/setup.hpp"

#include <SFML/Graphics/RenderWindow.hpp>

#include <memory>
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

    // Looks.

    /// The theme in use.
    [[nodiscard]] const Theme& theme() const;

    /// Replaces the theme. Everything is laid out and drawn anew with the next frame.
    void setTheme(Theme theme);

    // Sizes and positions.

    /// The layout theme in use.
    [[nodiscard]] const Layout& layout() const;

    /// Replaces the layout theme. Everything is laid out anew with the next frame.
    /// Throws `SetupError`, and changes nothing, if the panels do not fit the new one: a panel
    /// that takes its place from the layout theme and finds no room in the window's grid.
    void setLayout(Layout layout);

    /// The layout's sizes for the window as it is now, in pixels.
    [[nodiscard]] const Sizes& sizes() const;

    // Measuring.

    /// Shows or hides the profiler readout: a small panel on top of everything that says what
    /// the UI costs. Frames per second, the time spent painting panels and handing them to the
    /// graphics card, draw calls, and how much was painted anew.
    ///
    /// The readout refreshes a few times per second and only when its numbers change, so an idle
    /// application stays idle with it on. There is no key for it: the application decides when
    /// to show it, for example in its `KeyPressed` handler.
    void setProfilerVisible(bool visible);
    [[nodiscard]] bool isProfilerVisible() const;

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

private:
    friend class WidgetHandle;
    friend class ViewHandle;
    friend class PanelHandle;

    struct Impl; // the modules the UI is made of; see src/ui/ui.cpp
    std::unique_ptr<Impl> m_impl;
};

} // namespace atpl
